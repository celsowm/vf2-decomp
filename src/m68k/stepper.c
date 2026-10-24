#include "vf2/m68k_step.h"

#include "vf2/m68k.h"

#include <stdlib.h>
#include <string.h>

#define VF2_STEP_SCSP_BASE UINT32_C(0x100000)
#define VF2_STEP_ROM_MIRROR_BASE UINT32_C(0x600000)
#define VF2_STEP_SAMPLE_BASE UINT32_C(0x800000)

void vf2_m68k_regs_reset(vf2_m68k_regs *regs)
{
    size_t index = 0u;
    if (regs == NULL) {
        return;
    }
    for (index = 0u; index < 8u; ++index) {
        regs->d[index] = 0u;
        regs->a[index] = 0u;
    }
    regs->pc = 0u;
    regs->x = 0u;
    regs->n = 0u;
    regs->z = 0u;
    regs->v = 0u;
    regs->c = 0u;
    regs->sr_high = UINT16_C(0x2000); /* supervisor, IPL 0 */
    regs->usp = 0u;
    regs->ssp = 0u;
    regs->irq_active = 0u;
}

/* Sign-extend a 16-bit extension word to 32 bits using unsigned arithmetic only
 * (keeps -Wsign-conversion quiet on gcc 13 without changing the value). */
static uint32_t sext16(uint32_t v)
{
    return ((v & UINT32_C(0xffff)) ^ UINT32_C(0x8000)) - UINT32_C(0x8000);
}

static vf2_status rom_u16(
    const vf2_m68k_bus *bus,
    uint32_t address,
    uint16_t *value
)
{
    if (bus == NULL || value == NULL ||
        (uint64_t)address + 2u > (uint64_t)bus->rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = (uint16_t)(((uint16_t)bus->rom[address] << 8u) |
                        bus->rom[address + 1u]);
    return VF2_OK;
}

static vf2_status rom_u32(
    const vf2_m68k_bus *bus,
    uint32_t address,
    uint32_t *value
)
{
    uint16_t high = 0u;
    uint16_t low = 0u;
    vf2_status status = rom_u16(bus, address, &high);
    if (status != VF2_OK) {
        return status;
    }
    status = rom_u16(bus, address + 2u, &low);
    if (status != VF2_OK) {
        return status;
    }
    *value = ((uint32_t)high << 16u) | low;
    return VF2_OK;
}

static vf2_status log_event(
    vf2_m68k_bus *bus,
    uint32_t address,
    uint32_t value,
    uint8_t size,
    uint8_t is_write
)
{
    vf2_m68k_bus_event *slot = NULL;
    if (bus->event_count >= VF2_M68K_STEP_MAX_EVENTS) {
        return VF2_ERROR_UNSUPPORTED;
    }
    slot = &bus->events[bus->event_count];
    slot->address = address;
    slot->value = value;
    slot->size = size;
    slot->is_write = is_write;
    ++bus->event_count;
    return VF2_OK;
}

/* Data-space access with the sound_board access-kind split. size:
 * 0 = byte, 1 = word, 2 = long. */
static vf2_status bus_read(
    vf2_m68k_bus *bus,
    uint32_t address,
    unsigned size,
    uint32_t *value
)
{
    if (bus == NULL || value == NULL || size > 2u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((size == 1u && (address & 1u) != 0u) ||
        (size == 2u && (address & 1u) != 0u)) {
        /* Address-error shape on ROM. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (address >= VF2_STEP_SCSP_BASE &&
        (uint64_t)address + (size == 2u ? 4u : (size == 1u ? 2u : 1u)) <=
            (uint64_t)VF2_STEP_SCSP_BASE + VF2_SCSP_REGISTER_BYTES) {
        uint32_t slot = address - VF2_STEP_SCSP_BASE;
        vf2_status status = VF2_OK;
        if (bus->scsp == NULL) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 0u) {
            uint8_t byte = 0u;
            status = vf2_scsp_read_u8(bus->scsp, slot, &byte);
            *value = byte;
            return status;
        }
        if (size == 1u) {
            uint16_t word = 0u;
            status = vf2_scsp_read_u16(bus->scsp, slot, &word);
            *value = word;
            return status;
        }
        {
            uint16_t high = 0u;
            uint16_t low = 0u;
            status = vf2_scsp_read_u16(bus->scsp, slot, &high);
            if (status != VF2_OK) {
                return status;
            }
            status = vf2_scsp_read_u16(bus->scsp, slot + 2u, &low);
            if (status != VF2_OK) {
                return status;
            }
            *value = ((uint32_t)high << 16u) | low;
            return VF2_OK;
        }
    }
    if (address >= VF2_STEP_ROM_MIRROR_BASE &&
        (uint64_t)address - VF2_STEP_ROM_MIRROR_BASE + 1u <= bus->rom_size) {
        uint32_t offset = address - VF2_STEP_ROM_MIRROR_BASE;
        if (size == 0u) {
            *value = bus->rom[offset];
            return VF2_OK;
        }
        if (size == 1u) {
            if ((uint64_t)offset + 2u > (uint64_t)bus->rom_size) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            *value = (uint32_t)(((uint32_t)bus->rom[offset] << 8u) |
                                bus->rom[offset + 1u]);
            return VF2_OK;
        }
        if ((uint64_t)offset + 4u > (uint64_t)bus->rom_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *value = ((uint32_t)bus->rom[offset] << 24u) |
                 ((uint32_t)bus->rom[offset + 1u] << 16u) |
                 ((uint32_t)bus->rom[offset + 2u] << 8u) |
                 bus->rom[offset + 3u];
        return VF2_OK;
    }
    if (address >= VF2_STEP_SAMPLE_BASE &&
        (uint64_t)address - VF2_STEP_SAMPLE_BASE < bus->sample_rom_size) {
        uint32_t offset = address - VF2_STEP_SAMPLE_BASE;
        if (bus->sample_rom == NULL) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 0u) {
            *value = bus->sample_rom[offset];
            return VF2_OK;
        }
        if (size == 1u) {
            if ((uint64_t)offset + 2u > (uint64_t)bus->sample_rom_size) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            *value = (uint32_t)(((uint32_t)bus->sample_rom[offset] << 8u) |
                                bus->sample_rom[offset + 1u]);
            return VF2_OK;
        }
        if ((uint64_t)offset + 4u > (uint64_t)bus->sample_rom_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *value = ((uint32_t)bus->sample_rom[offset] << 24u) |
                 ((uint32_t)bus->sample_rom[offset + 1u] << 16u) |
                 ((uint32_t)bus->sample_rom[offset + 2u] << 8u) |
                 bus->sample_rom[offset + 3u];
        return VF2_OK;
    }
    if ((uint64_t)address +
            (size == 2u ? 4u : (size == 1u ? 2u : 1u)) <= bus->ram_size) {
        if (bus->ram == NULL) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 0u) {
            *value = bus->ram[address];
            return VF2_OK;
        }
        if (size == 1u) {
            *value = (uint32_t)(((uint32_t)bus->ram[address] << 8u) |
                                bus->ram[address + 1u]);
            return VF2_OK;
        }
        *value = ((uint32_t)bus->ram[address] << 24u) |
                 ((uint32_t)bus->ram[address + 1u] << 16u) |
                 ((uint32_t)bus->ram[address + 2u] << 8u) |
                 bus->ram[address + 3u];
        return VF2_OK;
    }
    if (address == UINT32_C(0x400000) || address == UINT32_C(0x400001)) {
        /* The $400000 system port is mapped but its behavior is
           unmodeled: record and continue. Every other unmapped
           address traps on silicon: refuse (fail closed). */
        *value = 0u;
        return log_event(bus, address, 0u, (uint8_t)size, 0u);
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status bus_write(
    vf2_m68k_bus *bus,
    uint32_t address,
    unsigned size,
    uint32_t value
)
{
    if (bus == NULL || size > 2u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((size == 1u && (address & 1u) != 0u) ||
        (size == 2u && (address & 1u) != 0u)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (address >= VF2_STEP_SCSP_BASE &&
        (uint64_t)address + (size == 2u ? 4u : (size == 1u ? 2u : 1u)) <=
            (uint64_t)VF2_STEP_SCSP_BASE + VF2_SCSP_REGISTER_BYTES) {
        uint32_t slot = address - VF2_STEP_SCSP_BASE;
        vf2_status status = VF2_OK;
        if (bus->scsp == NULL) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 0u) {
            return vf2_scsp_write_u8(bus->scsp, slot, (uint8_t)value);
        }
        if (size == 1u) {
            return vf2_scsp_write_u16(bus->scsp, slot, (uint16_t)value);
        }
        status = vf2_scsp_write_u16(
            bus->scsp, slot, (uint16_t)(value >> 16u));
        if (status != VF2_OK) {
            return status;
        }
        return vf2_scsp_write_u16(bus->scsp, slot + 2u, (uint16_t)value);
    }
    if ((address >= VF2_STEP_ROM_MIRROR_BASE &&
         (uint64_t)address - VF2_STEP_ROM_MIRROR_BASE < bus->rom_size) ||
        (address >= VF2_STEP_SAMPLE_BASE &&
         (uint64_t)address - VF2_STEP_SAMPLE_BASE < bus->sample_rom_size)) {
        /* Writes to ROM/sample windows refuse. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if ((uint64_t)address +
            (size == 2u ? 4u : (size == 1u ? 2u : 1u)) <= bus->ram_size) {
        if (bus->ram == NULL) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 0u) {
            bus->ram[address] = (uint8_t)value;
            return VF2_OK;
        }
        if (size == 1u) {
            bus->ram[address] = (uint8_t)(value >> 8u);
            bus->ram[address + 1u] = (uint8_t)value;
            return VF2_OK;
        }
        bus->ram[address] = (uint8_t)(value >> 24u);
        bus->ram[address + 1u] = (uint8_t)(value >> 16u);
        bus->ram[address + 2u] = (uint8_t)(value >> 8u);
        bus->ram[address + 3u] = (uint8_t)value;
        return VF2_OK;
    }
    if (address == UINT32_C(0x400000) || address == UINT32_C(0x400001)) {
        return log_event(bus, address, value, (uint8_t)size, 1u);
    }
    return VF2_ERROR_UNSUPPORTED;
}

/* Program-space read for fetch and PC-relative data. */
static vf2_status prog_read(
    vf2_m68k_bus *bus,
    uint32_t address,
    unsigned size,
    uint32_t *value
)
{
    if (size == 0u) {
        uint16_t word = 0u;
        vf2_status status = rom_u16(bus, address & ~1u, &word);
        if (status != VF2_OK) {
            return status;
        }
        /* Byte immediates and PC-relative bytes use the low byte of
           the containing word, matching the decoder's mask rule. */
        *value = (address & 1u) != 0u ? (word & 0xffu) : (word >> 8u);
        return VF2_OK;
    }
    if (size == 1u) {
        uint16_t word = 0u;
        vf2_status status = rom_u16(bus, address, &word);
        if (status != VF2_OK) {
            return status;
        }
        *value = word;
        return VF2_OK;
    }
    return rom_u32(bus, address, value);
}

static uint32_t size_mask(unsigned size)
{
    if (size == 0u) {
        return UINT32_C(0x000000ff);
    }
    if (size == 1u) {
        return UINT32_C(0x0000ffff);
    }
    return UINT32_C(0xffffffff);
}

static uint32_t size_msb(unsigned size)
{
    if (size == 0u) {
        return UINT32_C(0x00000080);
    }
    if (size == 1u) {
        return UINT32_C(0x00008000);
    }
    return UINT32_C(0x80000000);
}

static void flags_logic(vf2_m68k_regs *regs, unsigned size, uint32_t result)
{
    regs->n = (result & size_msb(size)) != 0u ? 1u : 0u;
    regs->z = (result & size_mask(size)) == 0u ? 1u : 0u;
    regs->v = 0u;
    regs->c = 0u;
}

static void flags_add(
    vf2_m68k_regs *regs,
    unsigned size,
    uint32_t a,
    uint32_t b,
    uint32_t result
)
{
    uint32_t mask = size_mask(size);
    uint32_t msb = size_msb(size);
    uint64_t full = (uint64_t)(a & mask) + (uint64_t)(b & mask);
    regs->n = (result & msb) != 0u ? 1u : 0u;
    regs->z = (result & mask) == 0u ? 1u : 0u;
    regs->v = ((((~(a ^ b)) & (a ^ result)) & msb) != 0u) ? 1u : 0u;
    regs->c = (((full >> (size == 0u ? 8u : (size == 1u ? 16u : 32u))) &
                1u) != 0u) ? 1u : 0u;
    regs->x = regs->c;
}

static void flags_sub(
    vf2_m68k_regs *regs,
    unsigned size,
    uint32_t a,
    uint32_t b,
    uint32_t result,
    int set_x
)
{
    uint32_t mask = size_mask(size);
    uint32_t msb = size_msb(size);
    regs->n = (result & msb) != 0u ? 1u : 0u;
    regs->z = (result & mask) == 0u ? 1u : 0u;
    regs->v = ((((a ^ b) & (a ^ result)) & msb) != 0u) ? 1u : 0u;
    regs->c = ((a & mask) < (b & mask)) ? 1u : 0u;
    if (set_x) {
        regs->x = regs->c;
    }
}

static uint32_t an_stride(unsigned areg, unsigned size)
{
    if (size == 2u) {
        return 4u;
    }
    if (size == 1u) {
        return 2u;
    }
    return areg == 7u ? 2u : 1u;
}

/* Indexed/indirect address for modes 5, 6, 7.0-7.3. Reads extension
 * words from the program at pc + *off and advances *off. pc_next is
 * the address just past the extension word (PC-relative base). */
static vf2_status ea_param_addr(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    unsigned mode,
    unsigned reg,
    uint32_t pc,
    size_t *off,
    int from_program,
    uint32_t *address
)
{
    uint16_t ext = 0u;
    vf2_status status = VF2_OK;
    (void)from_program;
    if (mode == 5u || mode == 6u || mode == 7u) {
        status = prog_read(bus, pc + (uint32_t)*off, 1u, address);
        if (status != VF2_OK) {
            return status;
        }
        ext = (uint16_t)*address;
        *off += 2u;
    }
    if (mode == 5u) {
        *address = regs->a[reg] + sext16(ext);
        return VF2_OK;
    }
    if (mode == 6u) {
        uint32_t base = regs->a[reg];
        uint32_t index = 0u;
        int32_t disp = (int32_t)(int8_t)(ext & 0xffu);
        unsigned xn = (unsigned)((ext >> 12u) & 7u);
        if (((ext >> 15u) & 1u) != 0u) {
            index = regs->a[xn];
        } else if (((ext >> 11u) & 1u) != 0u) {
            index = regs->d[xn];
        } else {
            index = (uint32_t)(int32_t)(int16_t)regs->d[xn];
        }
        *address = base + (uint32_t)disp + index;
        return VF2_OK;
    }
    if (mode == 7u && reg == 0u) {
        *address = sext16(ext);
        return VF2_OK;
    }
    if (mode == 7u && reg == 1u) {
        uint32_t high = 0u;
        uint32_t low = 0u;
        high = ((uint32_t)ext) << 16u;
        status = prog_read(bus, pc + (uint32_t)*off, 1u, &low);
        if (status != VF2_OK) {
            return status;
        }
        *off += 2u;
        *address = high | (low & 0xffffu);
        return VF2_OK;
    }
    if (mode == 7u && reg == 2u) {
        /* PC + displacement: base is the address of the ext word. */
        uint32_t base = pc + (uint32_t)*off - 2u;
        *address = base + sext16(ext);
        return VF2_OK;
    }
    if (mode == 7u && reg == 3u) {
        uint32_t base = pc + (uint32_t)*off - 2u;
        uint32_t index = 0u;
        int32_t disp = (int32_t)(int8_t)(ext & 0xffu);
        unsigned xn = (unsigned)((ext >> 12u) & 7u);
        if (((ext >> 15u) & 1u) != 0u) {
            index = regs->a[xn];
        } else if (((ext >> 11u) & 1u) != 0u) {
            index = regs->d[xn];
        } else {
            index = (uint32_t)(int32_t)(int16_t)regs->d[xn];
        }
        *address = base + (uint32_t)disp + index;
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

/* PC-relative data addresses resolve into the program image. */
static int ea_is_program(unsigned mode, unsigned reg)
{
    return mode == 7u && (reg == 2u || reg == 3u);
}

static vf2_status ea_read(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    unsigned mode,
    unsigned reg,
    unsigned size,
    uint32_t pc,
    size_t *off,
    uint32_t *value
)
{
    vf2_status status = VF2_OK;
    if (mode == 0u) {
        *value = regs->d[reg] & size_mask(size);
        return VF2_OK;
    }
    if (mode == 1u) {
        if (size == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 1u) {
            *value = regs->a[reg] & UINT32_C(0x0000ffff);
            return VF2_OK;
        }
        *value = regs->a[reg];
        return VF2_OK;
    }
    if (mode == 3u) {
        uint32_t address = regs->a[reg];
        status = bus_read(bus, address, size, value);
        if (status != VF2_OK) {
            return status;
        }
        regs->a[reg] = address + an_stride(reg, size);
        return VF2_OK;
    }
    if (mode == 4u) {
        uint32_t address = regs->a[reg] - an_stride(reg, size);
        status = bus_read(bus, address, size, value);
        if (status != VF2_OK) {
            return status;
        }
        regs->a[reg] = address;
        return VF2_OK;
    }
    if (mode == 2u || mode == 5u || mode == 6u || mode == 7u) {
        uint32_t address = 0u;
        if (mode == 2u) {
            address = regs->a[reg];
        } else {
            status = ea_param_addr(bus, regs, mode, reg, pc, off, 0u,
                                   &address);
            if (status != VF2_OK) {
                return status;
            }
        }
        if (ea_is_program(mode, reg)) {
            return prog_read(bus, address, size, value);
        }
        return bus_read(bus, address, size, value);
    }
    return VF2_ERROR_UNSUPPORTED;
}

/* Writes to Dn preserve the untouched upper bits for byte/word.
 * An-direct and #imm destinations are refused (callers route MOVEA
 * and address operations separately). */
static vf2_status ea_write(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    unsigned mode,
    unsigned reg,
    unsigned size,
    uint32_t pc,
    size_t *off,
    uint32_t value
)
{
    vf2_status status = VF2_OK;
    if (mode == 0u) {
        if (size == 2u) {
            regs->d[reg] = value;
        } else {
            regs->d[reg] =
                (regs->d[reg] & ~size_mask(size)) | (value & size_mask(size));
        }
        return VF2_OK;
    }
    if (mode == 1u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (mode == 3u) {
        uint32_t address = regs->a[reg];
        status = bus_write(bus, address, size, value);
        if (status != VF2_OK) {
            return status;
        }
        regs->a[reg] = address + an_stride(reg, size);
        return VF2_OK;
    }
    if (mode == 4u) {
        uint32_t address = regs->a[reg] - an_stride(reg, size);
        status = bus_write(bus, address, size, value);
        if (status != VF2_OK) {
            return status;
        }
        regs->a[reg] = address;
        return VF2_OK;
    }
    if (mode == 2u || mode == 5u || mode == 6u || mode == 7u) {
        uint32_t address = 0u;
        if (mode == 2u) {
            address = regs->a[reg];
        } else {
            status = ea_param_addr(bus, regs, mode, reg, pc, off, 0u,
                                   &address);
            if (status != VF2_OK) {
                return status;
            }
        }
        if (ea_is_program(mode, reg)) {
            /* Stores into the program image refuse. */
            return VF2_ERROR_UNSUPPORTED;
        }
        return bus_write(bus, address, size, value);
    }
    return VF2_ERROR_UNSUPPORTED;
}

/* Resolve a read-modify-write address ONCE. Extension words are
 * consumed here only; the caller reads, computes, and writes back
 * through the same address, post-incrementing (An)+ at the end.
 * Using ea_read + ea_write for one operand would consume the
 * extension twice and update An twice. Modes 0/1/7.2-7.4 refuse. */
static vf2_status ea_rmw_addr(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    unsigned mode,
    unsigned reg,
    unsigned size,
    uint32_t pc,
    size_t *off,
    uint32_t *address
)
{
    if (mode == 2u) {
        *address = regs->a[reg];
        return VF2_OK;
    }
    if (mode == 1u) {
        /* (An) indirect: same address as (An) dereference, no stride
           change. Admitted for the walk-sled ori.b (a2); the decoder
           gates which ops may use it. */
        *address = regs->a[reg];
        return VF2_OK;
    }
    if (mode == 3u) {
        *address = regs->a[reg];
        return VF2_OK;
    }
    if (mode == 4u) {
        regs->a[reg] -= an_stride(reg, size);
        *address = regs->a[reg];
        return VF2_OK;
    }
    if (mode == 5u || mode == 6u || (mode == 7u && reg <= 1u)) {
        return ea_param_addr(bus, regs, mode, reg, pc, off, 0u, address);
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status fetch_imm(    vf2_m68k_bus *bus,
    uint32_t pc,
    size_t *off,
    unsigned size,
    uint32_t *value
)
{
    vf2_status status = VF2_OK;
    if (size == 2u) {
        status = prog_read(bus, pc + (uint32_t)*off, 2u, value);
        if (status != VF2_OK) {
            return status;
        }
        *off += 4u;
        return VF2_OK;
    }
    status = prog_read(bus, pc + (uint32_t)*off, 1u, value);
    if (status != VF2_OK) {
        return status;
    }
    *off += 2u;
    if (size == 0u) {
        *value &= 0xffu;
    }
    return VF2_OK;
}

static int bcc_holds(const vf2_m68k_regs *regs, unsigned cond)
{
    switch (cond) {
    case 0u:
        return 1;
    case 1u:
        return 0;
    case 2u:
        return regs->c == 0u && regs->z == 0u;
    case 3u:
        return regs->c != 0u || regs->z != 0u;
    case 4u:
        return regs->c == 0u;
    case 5u:
        return regs->c != 0u;
    case 6u:
        return regs->z == 0u;
    case 7u:
        return regs->z != 0u;
    case 8u:
        return regs->v == 0u;
    case 9u:
        return regs->v != 0u;
    case 10u:
        return regs->n == 0u;
    case 11u:
        return regs->n != 0u;
    case 12u:
        return regs->n == regs->v;
    case 13u:
        return regs->n != regs->v;
    case 14u:
        return regs->z == 0u && regs->n == regs->v;
    default:
        return regs->z != 0u || regs->n != regs->v;
    }
}

/* True 68k sizes for the general ADD/SUB/CMP/OR forms. Both the
 * stepper and the disassembler (decoder.c:1155-1156, fixed v0803)
 * now use the Motorola size mapping (opmode 0/4=.B, 1/5=.W, 2/6=.L).
 * Earlier v0xxx versions had the decoder printing a rotated mapping
 * (0->w, 1->l, 2->b); the stepper always used the Motorola mapping,
 * so decoder prints diverged from stepper execution until v0803. */
static unsigned true_alu_size(unsigned opmode)
{
    if (opmode == 0u || opmode == 4u) {
        return 0u;
    }
    if (opmode == 1u || opmode == 5u) {
        return 1u;
    }
    return 2u;
}

static vf2_status step_move(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    unsigned size
)
{
    unsigned smode = (unsigned)((opcode >> 3u) & 7u);
    unsigned sreg = (unsigned)(opcode & 7u);
    unsigned dmode = (unsigned)((opcode >> 6u) & 7u);
    unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
    uint32_t src = 0u;
    vf2_status status = VF2_OK;
    if (smode == 1u && size == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (smode == 7u && sreg == 4u) {
        status = fetch_imm(bus, pc, off, size, &src);
        if (status != VF2_OK) {
            return status;
        }
    } else if (smode == 0u || smode == 1u || smode == 2u || smode == 3u ||
               smode == 4u || smode == 5u || smode == 6u ||
               (smode == 7u && sreg <= 3u)) {
        status = ea_read(bus, regs, smode, sreg, size, pc, off, &src);
        if (status != VF2_OK) {
            return status;
        }
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (dmode == 1u) {
        /* MOVEA shape: word sign-extends, no flags. */
        if (size == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (size == 1u) {
            regs->a[dreg] = (uint32_t)(int32_t)(int16_t)src;
        } else {
            regs->a[dreg] = src;
        }
        return VF2_OK;
    }
    if (dmode == 0u) {
        if (size == 2u) {
            regs->d[dreg] = src;
        } else {
            regs->d[dreg] = (regs->d[dreg] & ~size_mask(size)) |
                            (src & size_mask(size));
        }
        flags_logic(regs, size, src);
        return VF2_OK;
    }
    if (dmode == 2u || dmode == 3u || dmode == 4u || dmode == 5u ||
        dmode == 6u || (dmode == 7u && (dreg == 0u || dreg == 1u))) {
        status = ea_write(bus, regs, dmode, dreg, size, pc, off, src);
        if (status != VF2_OK) {
            return status;
        }
        flags_logic(regs, size, src);
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status step_imm_op(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    unsigned size,
    unsigned kind
)
{
    /* kind: 0 ori, 1 andi, 2 subi, 3 addi, 4 cmpi, 5 eori. */
    unsigned mode = (unsigned)((opcode >> 3u) & 7u);
    unsigned reg = (unsigned)(opcode & 7u);
    uint32_t imm = 0u;
    uint32_t dst = 0u;
    uint32_t result = 0u;
    vf2_status status = fetch_imm(bus, pc, off, size, &imm);
    if (status != VF2_OK) {
        return status;
    }
    if (mode == 7u && reg == 4u) {
        /* CCR/SR forms: only ori.b #,ccr is on recovered paths. */
        if (kind != 0u || size != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        {
            uint8_t ccr = (uint8_t)((regs->x << 4u) | (regs->n << 3u) |
                                    (regs->z << 2u) | (regs->v << 1u) |
                                    regs->c);
            ccr |= (uint8_t)imm;
            regs->x = (uint8_t)((ccr >> 4u) & 1u);
            regs->n = (uint8_t)((ccr >> 3u) & 1u);
            regs->z = (uint8_t)((ccr >> 2u) & 1u);
            regs->v = (uint8_t)((ccr >> 1u) & 1u);
            regs->c = (uint8_t)(ccr & 1u);
        }
        return VF2_OK;
    }
    /* Mode 1 ((An)) admitted for the walk-sled ori.b (a2); the decoder
       gates which imm-ops may use it (ori/eori only). */
    if (mode != 0u && mode != 1u && mode != 2u && mode != 3u && mode != 4u &&
        mode != 5u && mode != 6u &&
        !(mode == 7u && (reg == 0u || reg == 1u))) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (mode == 0u) {
        status = ea_read(bus, regs, mode, reg, size, pc, off, &dst);
        if (status != VF2_OK) {
            return status;
        }
    } else {
        uint32_t address = 0u;
        status = ea_rmw_addr(bus, regs, mode, reg, size, pc, off,
                             &address);
        if (status != VF2_OK) {
            return status;
        }
        status = bus_read(bus, address, size, &dst);
        if (status != VF2_OK) {
            return status;
        }
        if (mode == 3u) {
            regs->a[reg] = address + an_stride(reg, size);
        }
        if (kind == 4u) {
            flags_sub(regs, size, dst, imm, (dst - imm) & size_mask(size),
                      0);
            return VF2_OK;
        }
        if (kind == 0u || kind == 5u) {
            result = kind == 0u ? ((dst | imm) & size_mask(size))
                                : ((dst ^ imm) & size_mask(size));
        } else if (kind == 1u) {
            result = (dst & imm) & size_mask(size);
        } else if (kind == 3u) {
            result = (dst + imm) & size_mask(size);
        } else {
            result = (dst - imm) & size_mask(size);
        }
        status = bus_write(bus, address, size, result);
        if (status != VF2_OK) {
            return status;
        }
        if (kind == 3u) {
            flags_add(regs, size, dst, imm, result);
        } else if (kind == 2u) {
            flags_sub(regs, size, dst, imm, result, 1);
        } else {
            flags_logic(regs, size, result);
        }
        return VF2_OK;
    }
    if (kind == 0u || kind == 5u) {
        result = kind == 0u ? ((dst | imm) & size_mask(size))
                            : ((dst ^ imm) & size_mask(size));
    } else if (kind == 1u) {
        result = (dst & imm) & size_mask(size);
    } else if (kind == 3u) {
        result = (dst + imm) & size_mask(size);
    } else {
        result = (dst - imm) & size_mask(size);
    }
    if (kind == 4u) {
        flags_sub(regs, size, dst, imm, result, 0);
        return VF2_OK;
    }
    status = ea_write(bus, regs, mode, reg, size, pc, off, result);
    if (status != VF2_OK) {
        return status;
    }
    if (kind == 3u) {
        flags_add(regs, size, dst, imm, result);
    } else if (kind == 2u) {
        flags_sub(regs, size, dst, imm, result, 1);
    } else {
        flags_logic(regs, size, result);
    }
    return VF2_OK;
}

static vf2_status step_alu(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    unsigned opmode,
    unsigned kind
)
{
    /* kind: 0 add, 1 sub, 2 or, 3 cmp. True sizes (see note above). */
    unsigned smode = (unsigned)((opcode >> 3u) & 7u);
    unsigned sreg = (unsigned)(opcode & 7u);
    unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
    unsigned size = true_alu_size(opmode);
    uint32_t src = 0u;
    uint32_t dst = 0u;
    uint32_t result = 0u;
    vf2_status status = VF2_OK;
    if (opmode == 3u || opmode == 7u) {
        /* ADDA/SUBA/CMPA: destination is An. */
        if (kind != 0u && kind != 1u && kind != 3u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (smode != 0u && smode != 1u && smode != 2u && smode != 3u &&
            smode != 4u && smode != 5u && smode != 6u &&
            !(smode == 7u &&
              (sreg == 0u || sreg == 1u || sreg == 4u))) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (smode == 1u && size == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (smode == 7u && sreg == 4u) {
            unsigned isz = opmode == 7u ? 2u : 1u;
            status = fetch_imm(bus, pc, off, isz, &src);
            if (status != VF2_OK) {
                return status;
            }
            if (isz == 1u) {
                src = (uint32_t)(int32_t)(int16_t)src;
            }
        } else {
            unsigned rsz = size;
            if (opmode != 7u) {
                rsz = 1u;
            }
            if (smode == 1u && rsz == 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            status = ea_read(bus, regs, smode, sreg, rsz, pc, off, &src);
            if (status != VF2_OK) {
                return status;
            }
            if (rsz == 1u) {
                src = (uint32_t)(int32_t)(int16_t)(src & 0xffffu);
            }
        }
        if (kind == 3u) {
            uint32_t aext = regs->a[dreg];
            result = (aext - src) & size_mask(2u);
            flags_sub(regs, 2u, aext, src, result, 0);
            return VF2_OK;
        }
        if (kind == 0u) {
            regs->a[dreg] = regs->a[dreg] + src;
        } else {
            regs->a[dreg] = regs->a[dreg] - src;
        }
        return VF2_OK;
    }
    if (opmode <= 2u) {
        if (smode != 0u && smode != 2u && smode != 3u && smode != 5u) {
            /* Mirrors the decoder's admitted surface; anything else
               (An-direct, indexed, absolute, #imm, MxM shapes) refuses. */
            return VF2_ERROR_UNSUPPORTED;
        }
        status = ea_read(bus, regs, smode, sreg, size, pc, off, &src);
        if (status != VF2_OK) {
            return status;
        }
        dst = regs->d[dreg] & size_mask(size);
        if (kind == 2u) {
            result = (dst | src) & size_mask(size);
            regs->d[dreg] = (regs->d[dreg] & ~size_mask(size)) | result;
            flags_logic(regs, size, result);
            return VF2_OK;
        }
        if (kind == 0u) {
            result = (dst + src) & size_mask(size);
            regs->d[dreg] = (regs->d[dreg] & ~size_mask(size)) | result;
            flags_add(regs, size, dst, src, result);
            return VF2_OK;
        }
        if (kind == 1u) {
            result = (dst - src) & size_mask(size);
            regs->d[dreg] = (regs->d[dreg] & ~size_mask(size)) | result;
            flags_sub(regs, size, dst, src, result, 1);
            return VF2_OK;
        }
        result = (dst - src) & size_mask(size);
        flags_sub(regs, size, dst, src, result, 0);
        return VF2_OK;
    }
    if (opmode >= 4u && opmode <= 6u) {
        /* Dn,ea forms (ADD/SUB/OR only; the decoder admits src Dn
           with ea modes 0/2/3/5 for ADD and narrower for SUB). */
        if (kind == 3u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (smode != 0u && smode != 2u && smode != 3u && smode != 5u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        src = regs->d[dreg] & size_mask(size);
        if (smode == 0u) {
            status = ea_read(bus, regs, smode, sreg, size, pc, off, &dst);
            if (status != VF2_OK) {
                return status;
            }
        } else {
            uint32_t address = 0u;
            status = ea_rmw_addr(bus, regs, smode, sreg, size, pc, off,
                                 &address);
            if (status != VF2_OK) {
                return status;
            }
            status = bus_read(bus, address, size, &dst);
            if (status != VF2_OK) {
                return status;
            }
            if (smode == 3u) {
                regs->a[sreg] = address + an_stride(sreg, size);
            }
            if (kind == 2u) {
                result = (dst | src) & size_mask(size);
            } else if (kind == 0u) {
                result = (dst + src) & size_mask(size);
            } else {
                result = (dst - src) & size_mask(size);
            }
            status = bus_write(bus, address, size, result);
            if (status != VF2_OK) {
                return status;
            }
            if (kind == 2u) {
                flags_logic(regs, size, result);
            } else if (kind == 0u) {
                flags_add(regs, size, dst, src, result);
            } else {
                flags_sub(regs, size, dst, src, result, 1);
            }
            return VF2_OK;
        }
        if (kind == 2u) {
            result = (dst | src) & size_mask(size);
        } else if (kind == 0u) {
            result = (dst + src) & size_mask(size);
        } else {
            result = (dst - src) & size_mask(size);
        }
        status = ea_write(bus, regs, smode, sreg, size, pc, off, result);
        if (status != VF2_OK) {
            return status;
        }
        if (kind == 2u) {
            flags_logic(regs, size, result);
        } else if (kind == 0u) {
            flags_add(regs, size, dst, src, result);
        } else {
            flags_sub(regs, size, dst, src, result, 1);
        }
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status step_quick(
    vf2_m68k_regs *regs,
    vf2_m68k_bus *bus,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    int is_sub
)
{
    unsigned mode = (unsigned)((opcode >> 3u) & 7u);
    unsigned reg = (unsigned)(opcode & 7u);
    unsigned count = (unsigned)((opcode >> 9u) & 7u);
    unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
    unsigned size = size_code == 0u ? 0u : (size_code == 1u ? 1u : 2u);
    uint32_t amount = count == 0u ? 8u : count;
    uint32_t dst = 0u;
    uint32_t result = 0u;
    vf2_status status = VF2_OK;
    if (size_code == 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (mode == 1u) {
        /* Quick to An: word/long only, full 32-bit add, no flags. */
        if (size == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (is_sub != 0) {
            regs->a[reg] = regs->a[reg] - amount;
        } else {
            regs->a[reg] = regs->a[reg] + amount;
        }
        return VF2_OK;
    }
    if (mode != 0u && mode != 2u && mode != 3u && mode != 4u &&
        mode != 5u && mode != 6u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* No size gate for memory modes: silicon ADDQ/SUBQ take B/W/L
       to (An)/(An)+/-(An)/d16(An)/d8(An,Xn). (The old code refused
       W/L here; 0x804 addq.w #4,$1508(a6) on the live drain exit
       needs the word form.) */
    if (mode == 0u) {
        status = ea_read(bus, regs, mode, reg, size, pc, off, &dst);
        if (status != VF2_OK) {
            return status;
        }
        if (is_sub != 0) {
            result = (dst - amount) & size_mask(size);
        } else {
            result = (dst + amount) & size_mask(size);
        }
        status = ea_write(bus, regs, mode, reg, size, pc, off, result);
        if (status != VF2_OK) {
            return status;
        }
    } else {
        /* Memory RMW: resolve once, read, postinc, write back. */
        uint32_t address = 0u;
        status = ea_rmw_addr(bus, regs, mode, reg, size, pc, off,
                             &address);
        if (status != VF2_OK) {
            return status;
        }
        status = bus_read(bus, address, size, &dst);
        if (status != VF2_OK) {
            return status;
        }
        if (mode == 3u) {
            regs->a[reg] = address + an_stride(reg, size);
        }
        if (is_sub != 0) {
            result = (dst - amount) & size_mask(size);
        } else {
            result = (dst + amount) & size_mask(size);
        }
        status = bus_write(bus, address, size, result);
        if (status != VF2_OK) {
            return status;
        }
    }
    if (is_sub != 0) {
        flags_sub(regs, size, dst, amount, result, 1);
    } else {
        flags_add(regs, size, dst, amount, result);
    }
    return VF2_OK;
}

static vf2_status step_mulu(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    int is_signed
)
{
    unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
    unsigned smode = (unsigned)((opcode >> 3u) & 7u);
    unsigned sreg = (unsigned)(opcode & 7u);
    uint32_t src = 0u;
    uint32_t result = 0u;
    vf2_status status = VF2_OK;
    /* The decoder admits #imm and Dn sources only. */
    if (smode == 7u && sreg == 4u) {
        status = fetch_imm(bus, pc, off, 1u, &src);
        if (status != VF2_OK) {
            return status;
        }
    } else if (smode == 0u) {
        status = ea_read(bus, regs, smode, sreg, 1u, pc, off, &src);
        if (status != VF2_OK) {
            return status;
        }
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (is_signed != 0) {
        int32_t product = (int32_t)(int16_t)(regs->d[dreg] & 0xffffu) *
                          (int32_t)(int16_t)(src & 0xffffu);
        result = (uint32_t)product;
    } else {
        result = (regs->d[dreg] & 0xffffu) * (src & 0xffffu);
    }
    regs->d[dreg] = result;
    flags_logic(regs, 2u, result);
    return VF2_OK;
}

/* ROR Dn, immediate count only (ROXR stays refused). The walk-sled
 * ror.l #8,d1 chain needs exactly this form: rotate right, C takes
 * each bit shifted out of bit 0, X untouched, V cleared. */
static vf2_status step_ror_reg(vf2_m68k_regs *regs, uint16_t opcode)
{
    unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
    unsigned count = (unsigned)((opcode >> 9u) & 7u);
    unsigned reg = (unsigned)(opcode & 7u);
    unsigned width = size_code == 0u ? 8u : (size_code == 1u ? 16u : 32u);
    uint32_t mask =
        width == 8u ? UINT32_C(0xff) :
        (width == 16u ? UINT32_C(0xffff) : UINT32_C(0xffffffff));
    uint32_t value = regs->d[reg] & mask;
    unsigned i = 0u;
    if (count == 0u) {
        count = 8u;
    }
    for (i = 0u; i < count; ++i) {
        uint32_t bit = value & 1u;
        value = (value >> 1u) | (bit << (width - 1u));
        regs->c = (uint8_t)bit;
    }
    value &= mask;
    regs->d[reg] = (regs->d[reg] & ~mask) | value;
    regs->n = (value & (UINT32_C(1) << (width - 1u))) != 0u ? 1u : 0u;
    regs->z = value == 0u ? 1u : 0u;
    regs->v = 0u;
    return VF2_OK;
}

static vf2_status step_shift(
    vf2_m68k_regs *regs,
    uint16_t opcode
)
{
    unsigned type = (unsigned)((opcode >> 3u) & 3u);
    unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
    unsigned count = (unsigned)((opcode >> 9u) & 7u);
    unsigned dreg = (unsigned)(opcode & 7u);
    unsigned size = size_code == 0u ? 0u : (size_code == 1u ? 1u : 2u);
    unsigned times = count == 0u ? 8u : count;
    unsigned step = 0u;
    uint32_t value = regs->d[dreg] & size_mask(size);
    uint32_t msb = size_msb(size);
    uint32_t out = 0u;
    int left = (opcode & UINT16_C(0x0100)) != 0;
    uint8_t overflow = 0u;
    if (type > 1u) {
        /* Rotate forms are not on recovered paths. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (size_code == 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (step = 0u; step < times; ++step) {
        if (type == 0u && left == 0) {
            /* Arithmetic right: sign replicates. */
            out = (value & 1u);
            value = (value >> 1u) | (value & msb);
        } else if (left == 0) {
            out = (value & 1u);
            value = (value >> 1u) & size_mask(size);
        } else {
            out = (value & msb) != 0u ? 1u : 0u;
            if (type == 0u && ((value & msb) !=
                               (((value << 1u) & size_mask(size)) & msb))) {
                overflow = 1u;
            }
            value = (value << 1u) & size_mask(size);
        }
    }
    regs->x = (uint8_t)out;
    regs->c = (uint8_t)out;
    regs->v = (type == 0u && left != 0) ? overflow : 0u;
    regs->n = (value & msb) != 0u ? 1u : 0u;
    regs->z = value == 0u ? 1u : 0u;
    if (size == 2u) {
        regs->d[dreg] = value;
    } else {
        regs->d[dreg] =
            (regs->d[dreg] & ~size_mask(size)) | (value & size_mask(size));
    }
    return VF2_OK;
}

static vf2_status step_bit(
    vf2_m68k_regs *regs,
    vf2_m68k_bus *bus,
    uint16_t opcode,
    uint32_t pc,
    size_t *off
)
{
    unsigned type = (unsigned)((opcode >> 6u) & 3u);
    unsigned mode = (unsigned)((opcode >> 3u) & 7u);
    unsigned reg = (unsigned)(opcode & 7u);
    uint16_t bitword = 0u;
    unsigned bit = 0u;
    uint32_t dst = 0u;
    vf2_status status = prog_read(bus, pc + (uint32_t)*off, 1u, &dst);
    if (status != VF2_OK) {
        return status;
    }
    bitword = (uint16_t)dst;
    *off += 2u;
    if (mode != 0u && mode != 2u && mode != 3u && mode != 4u &&
        mode != 5u && mode != 6u &&
        !(mode == 7u && (reg == 0u || reg == 1u))) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (mode == 0u) {
        bit = bitword & 31u;
        dst = regs->d[reg];
        regs->z = ((dst >> bit) & 1u) == 0u ? 1u : 0u;
        if (type == 1u) {
            regs->d[reg] = dst ^ (UINT32_C(1) << bit);
        } else if (type == 2u) {
            regs->d[reg] = dst & ~(UINT32_C(1) << bit);
        } else if (type == 3u) {
            regs->d[reg] = dst | (UINT32_C(1) << bit);
        }
        return VF2_OK;
    }
    bit = bitword & 7u;
    {
        /* Memory bitop: resolve once (see ea_rmw_addr). A pure test
           still post-increments (An)+ on the access. */
        uint32_t address = 0u;
        status = ea_rmw_addr(bus, regs, mode, reg, 0u, pc, off, &address);
        if (status != VF2_OK) {
            return status;
        }
        status = bus_read(bus, address, 0u, &dst);
        if (status != VF2_OK) {
            return status;
        }
        if (mode == 3u) {
            regs->a[reg] = address + an_stride(reg, 0u);
        }
        regs->z = ((dst >> bit) & 1u) == 0u ? 1u : 0u;
        if (type == 1u) {
            dst ^= (uint32_t)(1u << bit);
        } else if (type == 2u) {
            dst &= (uint32_t)~(1u << bit);
        } else if (type == 3u) {
            dst |= (uint32_t)(1u << bit);
        }
        if (type != 0u) {
            status = bus_write(bus, address, 0u, dst);
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    return VF2_OK;
}

static vf2_status step_movem(
    vf2_m68k_regs *regs,
    vf2_m68k_bus *bus,
    uint16_t opcode,
    uint32_t pc,
    size_t *off
)
{
    unsigned size_long = (opcode & UINT16_C(0x0040)) != 0u;
    unsigned to_reg = (opcode & UINT16_C(0xffc0)) != UINT16_C(0x48c0);
    unsigned mode = (unsigned)((opcode >> 3u) & 7u);
    unsigned reg = (unsigned)(opcode & 7u);
    uint16_t mask = 0u;
    uint32_t value = 0u;
    uint32_t address = 0u;
    int bit = 0;
    vf2_status status = prog_read(bus, pc + (uint32_t)*off, 1u, &value);
    if (status != VF2_OK) {
        return status;
    }
    mask = (uint16_t)value;
    *off += 2u;
    if (size_long == 0u) {
        /* Word movem is not on recovered paths. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (to_reg != 0u) {
        if (mode != 3u && mode != 5u && mode != 6u && mode != 2u &&
            !(mode == 7u && (reg == 0u || reg == 1u || reg == 2u ||
                             reg == 3u))) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (mode == 3u) {
            address = regs->a[reg];
            for (bit = 0; bit < 16; ++bit) {
                if ((mask & (uint16_t)((uint16_t)1u << (unsigned)bit)) ==
                    0u) {
                    continue;
                }
                status = bus_read(bus, address, 2u, &value);
                if (status != VF2_OK) {
                    return status;
                }
                if (bit < 8) {
                    regs->d[bit] = value;
                } else {
                    regs->a[bit - 8] = value;
                }
                address += 4u;
            }
            regs->a[reg] = address;
            return VF2_OK;
        }
        status = ea_param_addr(bus, regs, mode, reg, pc, off, 0u, &address);
        if (status != VF2_OK) {
            return status;
        }
        if (ea_is_program(mode, reg)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (bit = 0; bit < 16; ++bit) {
            if ((mask & (uint16_t)((uint16_t)1u << (unsigned)bit)) == 0u) {
                continue;
            }
            status = bus_read(bus, address, 2u, &value);
            if (status != VF2_OK) {
                return status;
            }
            if (bit < 8) {
                regs->d[bit] = value;
            } else {
                regs->a[bit - 8] = value;
            }
            address += 4u;
        }
        return VF2_OK;
    }
    /* Motorola M68000 PRM, MOVEM register->memory: (An) is a
     * legal CONTROL ALTERABLE destination. Postincrement remains
     * prohibited. The old exclusion of mode 2 falsely declared
     * original ROM 0x48D2 to be an illegal instruction. */
    if (mode != 2u && mode != 4u && mode != 5u && mode != 6u &&
        !(mode == 7u && (reg == 0u || reg == 1u))) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (mode == 4u) {
        /* MC68000 MOVEM reg -> -(An) has a REVERSED register
         * mask: bit0=A7, ..., bit7=A0, bit8=D7, ..., bit15=D0.
         * Transfers follow descending address order. The
         * original VF2.1 L2 ISR has mask 0xFFF8 and matching
         * MOVEM.L (A7)+ normal mask 0x1FFF. */
        address = regs->a[reg];
        for (bit = 0; bit < 16; ++bit) {
            if ((mask & (uint16_t)((uint16_t)1u << (unsigned)bit)) == 0u) {
                continue;
            }
            address -= 4u;
            value = bit < 8 ?
                regs->a[7 - bit] : regs->d[15 - bit];
            status = bus_write(bus, address, 2u, value);
            if (status != VF2_OK) {
                return status;
            }
        }
        regs->a[reg] = address;
        return VF2_OK;
    }
    if (mode == 2u) {
        /* (An) is a base address, without an extension word or
         * implicit update of An. ea_param_addr intentionally handles
         * extension-bearing modes only. */
        address = regs->a[reg];
    } else {
        status = ea_param_addr(bus, regs, mode, reg, pc, off, 0u, &address);
        if (status != VF2_OK) {
            return status;
        }
    }
    if (ea_is_program(mode, reg)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* Control modes store D0..D7, then A0..A7, in ascending
     * address order. Reverse order applies ONLY to -(An). */
    for (bit = 0; bit < 16; ++bit) {
        if ((mask & (uint16_t)((uint16_t)1u << (unsigned)bit)) == 0u) {
            continue;
        }
        value = bit < 8 ? regs->d[bit] : regs->a[bit - 8];
        status = bus_write(bus, address, 2u, value);
        if (status != VF2_OK) {
            return status;
        }
        address += 4u;
    }
    return VF2_OK;
}

static vf2_status step_bcc(
    vf2_m68k_regs *regs,
    vf2_m68k_bus *bus,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    uint32_t *new_pc
)
{
    unsigned cond = (unsigned)((opcode >> 8u) & 15u);
    unsigned low = (unsigned)(opcode & 0xffu);
    int32_t disp = 0;
    uint32_t base = 0u;
    uint32_t value = 0u;
    vf2_status status = VF2_OK;
    if (low == 0xffu) {
        /* Long branches are not on recovered paths. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (low == 0u) {
        status = prog_read(bus, pc + (uint32_t)*off, 1u, &value);
        if (status != VF2_OK) {
            return status;
        }
        *off += 2u;
        disp = (int32_t)(int16_t)value;
    } else {
        disp = (int32_t)(int8_t)low;
    }
    /* The displacement is relative to the address of the extension
     * word, i.e. opcode address + 2, for both forms. */
    base = pc + (uint32_t)*off;
    if (bcc_holds(regs, cond)) {
        *new_pc = pc + 2u + (uint32_t)disp;
    } else {
        *new_pc = base;
    }
    return VF2_OK;
}

static vf2_status step_dbc(
    vf2_m68k_regs *regs,
    vf2_m68k_bus *bus,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    uint32_t *new_pc
)
{
    unsigned cond = (unsigned)((opcode >> 8u) & 15u);
    unsigned reg = (unsigned)(opcode & 7u);
    uint32_t value = 0u;
    uint32_t low = 0u;
    int32_t disp = 0;
    vf2_status status = prog_read(bus, pc + (uint32_t)*off, 1u, &value);
    if (status != VF2_OK) {
        return status;
    }
    *off += 2u;
    disp = (int32_t)(int16_t)value;
    if (bcc_holds(regs, cond)) {
        *new_pc = pc + (uint32_t)*off;
        return VF2_OK;
    }
    low = (regs->d[reg] - 1u) & 0xffffu;
    regs->d[reg] = (regs->d[reg] & UINT32_C(0xffff0000)) | low;
    if (low == 0xffffu) {
        *new_pc = pc + (uint32_t)*off;
    } else {
        *new_pc = pc + 2u + (uint32_t)disp;
    }
    return VF2_OK;
}

static vf2_status step_jsr_jmp(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint16_t opcode,
    uint32_t pc,
    size_t *off,
    uint32_t *new_pc,
    int is_jsr,
    uint32_t ret_pc
)
{
    uint32_t target = 0u;
    vf2_status status = VF2_OK;
    if (opcode == UINT16_C(0x4eb9) || opcode == UINT16_C(0x4ef9)) {
        status = prog_read(bus, pc + (uint32_t)*off, 2u, &target);
        if (status != VF2_OK) {
            return status;
        }
        *off += 4u;
    } else if (opcode == UINT16_C(0x4eba) || opcode == UINT16_C(0x4efa)) {
        uint32_t ext = 0u;
        status = prog_read(bus, pc + (uint32_t)*off, 1u, &ext);
        if (status != VF2_OK) {
            return status;
        }
        *off += 2u;
        /* Motorola base: the address of the ext word (opcode + 2). */
        target = pc + 2u + sext16(ext);
    } else if ((opcode & UINT16_C(0xfff8)) == UINT16_C(0x4e90) ||
               (opcode & UINT16_C(0xfff8)) == UINT16_C(0x4ed0)) {
        target = regs->a[opcode & 7u];
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (is_jsr != 0) {
        regs->a[7] -= 4u;
        status = bus_write(bus, regs->a[7], 2u, ret_pc);
        if (status != VF2_OK) {
            return status;
        }
    }
    *new_pc = target;
    return VF2_OK;
}

/* Build a real MC68000 short-format exception frame:
 *     new SSP +0..1: interrupted SR
 *     new SSP +2..5: interrupted PC (big endian)
 * The vector is read from ROM vector 24+level (autovector).
 * The executable sound-ROM offsets are normalized to the
 * existing stepper's zero-based program memory.
 * Only one active IRQ is supported until RTE: refusing nested
 * levels makes the current limited semantics explicit. */
vf2_status vf2_m68k_irq_autovector_enter(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs, unsigned level
)
{
    uint32_t vector_pc = 0u;
    uint32_t supervisor_stack;
    uint32_t new_stack;
    uint16_t old_sr;
    vf2_status status;
    if (bus == NULL || regs == NULL || bus->rom == NULL ||
        bus->ram == NULL || level < 1u || level > 7u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (regs->irq_active != 0u ||
        level <= (unsigned)((regs->sr_high >> 8u) & UINT16_C(7))) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = rom_u32(bus, (uint32_t)(24u + level) * UINT32_C(4),
                     &vector_pc);
    if (status != VF2_OK) return status;
    if (vector_pc >= VF2_STEP_ROM_MIRROR_BASE) {
        vector_pc -= VF2_STEP_ROM_MIRROR_BASE;
    }
    /* The native stepper always fetches through zero-based ROM.
     * Disallow unmapped or odd vector entries before stack writes. */
    if ((vector_pc & UINT32_C(1)) != 0u ||
        (uint64_t)vector_pc + 2u > bus->rom_size) {
        return VF2_ERROR_UNSUPPORTED;
    }
    supervisor_stack =
        (regs->sr_high & UINT16_C(0x2000)) != 0u ?
        regs->a[7] : regs->ssp;
    if ((supervisor_stack & UINT32_C(1)) != 0u ||
        supervisor_stack < UINT32_C(6) ||
        supervisor_stack > bus->ram_size) {
        return VF2_ERROR_UNSUPPORTED;
    }
    new_stack = supervisor_stack - UINT32_C(6);
    old_sr = (uint16_t)(
        (regs->sr_high & UINT16_C(0xffe0)) |
        ((uint16_t)(regs->x & 1u) << 4u) |
        ((uint16_t)(regs->n & 1u) << 3u) |
        ((uint16_t)(regs->z & 1u) << 2u) |
        ((uint16_t)(regs->v & 1u) << 1u) |
        (uint16_t)(regs->c & 1u));
    status = bus_write(bus, new_stack + 2u, 2u, regs->pc);
    if (status != VF2_OK) return status;
    status = bus_write(bus, new_stack, 1u, (uint32_t)old_sr);
    if (status != VF2_OK) return status;
    if ((regs->sr_high & UINT16_C(0x2000)) == 0u) {
        regs->usp = regs->a[7];
    }
    regs->ssp = new_stack;
    regs->a[7] = new_stack;
    regs->sr_high = (uint16_t)(
        (old_sr & UINT16_C(0xf8e0)) |
        UINT16_C(0x2000) |
        (uint16_t)(level << 8u));
    regs->pc = vector_pc;
    regs->irq_active = 1u;
    return VF2_OK;
}

vf2_status vf2_m68k_irq_autovector_enter_timed(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    unsigned level, uint32_t *entry_cycles
)
{
    vf2_status status;
    if (entry_cycles == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    *entry_cycles = 0u;
    status = vf2_m68k_irq_autovector_enter(bus, regs, level);
    if (status != VF2_OK) return status;
    /* Official MC68000 exception-processing table: 44 clocks
     * for an autovector interrupt (uncontended zero-wait bus).
     * Preserve all state transitions from the proven helper. */
    *entry_cycles = UINT32_C(44);
    return VF2_OK;
}

vf2_status vf2_m68k_run(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint32_t stop_pc,
    int stop_on_return,
    uint32_t max_steps,
    uint32_t *steps_done
)
{
    uint32_t done = 0u;
    if (bus == NULL || regs == NULL || steps_done == NULL ||
        bus->rom == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *steps_done = 0u;
    while (done < max_steps) {
        uint32_t pc = regs->pc;
        uint16_t opcode = 0u;
        uint16_t first = 0u;
        vf2_m68k_instruction instruction;
        size_t off = 2u;
        uint32_t new_pc = 0u;
        vf2_status status = VF2_OK;
        unsigned cls = 0u;
        if (pc == stop_pc) {
            *steps_done = done;
            return VF2_OK;
        }
        status = rom_u16(bus, pc, &first);
        if (status != VF2_OK) {
            return status;
        }
        opcode = first;
        status = vf2_m68k_decode(bus->rom, bus->rom_size, pc, &instruction);
        if (status != VF2_OK || instruction.supported == 0) {
            *steps_done = done;
            return VF2_ERROR_UNSUPPORTED;
        }
        new_pc = pc + (uint32_t)instruction.length;
        cls = (unsigned)((opcode & 0xf000u) >> 12u);
        if (cls >= 1u && cls <= 3u) {
            unsigned size =
                cls == 1u ? 0u : (cls == 3u ? 1u : 2u);
            status = step_move(bus, regs, opcode, pc, &off, size);
        } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0)) {
            unsigned mode = (unsigned)((opcode >> 3u) & 7u);
            unsigned reg = (unsigned)(opcode & 7u);
            unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
            uint32_t address = 0u;
            if (mode != 2u && mode != 5u && mode != 6u &&
                !(mode == 7u && (reg == 0u || reg == 1u || reg == 2u ||
                                 reg == 3u))) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                status = ea_param_addr(bus, regs, mode, reg, pc, &off, 0u,
                                       &address);
                if (status == VF2_OK) {
                    regs->a[dreg] = mode == 2u ? regs->a[reg] : address;
                }
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0000) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0040) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0080)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size = size_code == 0u ? 0u
                    : (size_code == 1u ? 1u : 2u);
                status = step_imm_op(bus, regs, opcode, pc, &off, size, 0u);
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0a00) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0a40) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0a80)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size = size_code == 0u ? 0u
                    : (size_code == 1u ? 1u : 2u);
                status = step_imm_op(bus, regs, opcode, pc, &off, size, 5u);
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0200) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0240) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0280)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size = size_code == 0u ? 0u
                    : (size_code == 1u ? 1u : 2u);
                status = step_imm_op(bus, regs, opcode, pc, &off, size, 1u);
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0400) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0440) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0480)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size = size_code == 0u ? 0u
                    : (size_code == 1u ? 1u : 2u);
                status = step_imm_op(bus, regs, opcode, pc, &off, size, 2u);
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0600) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0640) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0680)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size = size_code == 0u ? 0u
                    : (size_code == 1u ? 1u : 2u);
                status = step_imm_op(bus, regs, opcode, pc, &off, size, 3u);
            }
        } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x0c00)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size = size_code == 0u ? 0u
                    : (size_code == 1u ? 1u : 2u);
                status = step_imm_op(bus, regs, opcode, pc, &off, size, 4u);
            }
        } else if ((opcode & UINT16_C(0xf1ff)) == UINT16_C(0xd0fc) ||
                   (opcode & UINT16_C(0xf1ff)) == UINT16_C(0xd1fc) ||
                   (opcode & UINT16_C(0xf1ff)) == UINT16_C(0x90fc) ||
                   (opcode & UINT16_C(0xf1ff)) == UINT16_C(0x91fc)) {
            /* ADDA/SUBA #imm: word sign-extends, long is full, no
               flags. Placed before SUB/ADD: 0x90FC would otherwise
               die in the SUB narrow-mode guard. */
            unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
            int is_long = (opcode & UINT16_C(0x0100)) != 0;
            int is_sub = (opcode & UINT16_C(0xf000)) == UINT16_C(0x9000);
            uint32_t imm = 0u;
            status = fetch_imm(bus, pc, &off, is_long ? 2u : 1u, &imm);
            if (status == VF2_OK) {
                uint32_t delta =
                    is_long ? imm : (uint32_t)(int32_t)(int16_t)imm;
                if (is_sub != 0) {
                    regs->a[dreg] = regs->a[dreg] - delta;
                } else {
                    regs->a[dreg] = regs->a[dreg] + delta;
                }
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x8000)) {
            /* OR: decoder admits opmode 0-2 with src 0/2/3/5/6. */
            unsigned opmode = (unsigned)((opcode >> 6u) & 7u);
            unsigned smode = (unsigned)((opcode >> 3u) & 7u);
            if (opmode > 2u || (smode != 0u && smode != 2u && smode != 3u &&
                                smode != 5u && smode != 6u)) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                status = step_alu(bus, regs, opcode, pc, &off, opmode, 2u);
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x9000)) {
            /* SUB: decoder admits opmode 0-2 with src 0/2/3/5. */
            unsigned opmode = (unsigned)((opcode >> 6u) & 7u);
            unsigned smode = (unsigned)((opcode >> 3u) & 7u);
            if (opmode > 2u || (smode != 0u && smode != 2u && smode != 3u &&
                                smode != 5u)) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                status = step_alu(bus, regs, opcode, pc, &off, opmode, 1u);
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0xd000)) {
            unsigned opmode = (unsigned)((opcode >> 6u) & 7u);
            unsigned smode = (unsigned)((opcode >> 3u) & 7u);
            if (opmode <= 2u) {
                if (smode != 0u && smode != 2u && smode != 3u &&
                    smode != 5u) {
                    status = VF2_ERROR_UNSUPPORTED;
                } else {
                    status =
                        step_alu(bus, regs, opcode, pc, &off, opmode, 0u);
                }
            } else if (opmode == 3u || opmode == 7u) {
                if (smode != 0u && smode != 2u && smode != 3u &&
                    smode != 5u) {
                    status = VF2_ERROR_UNSUPPORTED;
                } else {
                    status =
                        step_alu(bus, regs, opcode, pc, &off, opmode, 0u);
                }
            } else if (opmode >= 4u && opmode <= 6u) {
                /* Dn,ea: admitted dst modes mirror the src list. */
                if (smode != 0u && smode != 2u && smode != 3u &&
                    smode != 5u) {
                    status = VF2_ERROR_UNSUPPORTED;
                } else {
                    status =
                        step_alu(bus, regs, opcode, pc, &off, opmode, 0u);
                }
            } else {
                status = VF2_ERROR_UNSUPPORTED;
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0xb000) &&
                   (((opcode >> 3u) & 7u) == 0u ||
                    ((opcode >> 3u) & 7u) == 2u ||
                    ((opcode >> 3u) & 7u) == 3u ||
                    ((opcode >> 3u) & 7u) == 5u)) {
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            unsigned opmode = size_code == 0u ? 0u
                : (size_code == 1u ? 1u : 2u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                /* CMP uses true sizes here (see note on step_alu). */
                status = step_alu(bus, regs, opcode, pc, &off, opmode, 3u);
            }
        } else if (((opcode & UINT16_C(0xf1ff)) == UINT16_C(0xb0fc) ||
                    (opcode & UINT16_C(0xf1ff)) == UINT16_C(0xb1fc))) {
            unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
            int is_long = (opcode & UINT16_C(0x0100)) != 0;
            uint32_t imm = 0u;
            uint32_t result = 0u;
            status = fetch_imm(bus, pc, &off, is_long ? 2u : 1u, &imm);
            if (status == VF2_OK) {
                if (is_long == 0) {
                    imm = (uint32_t)(int32_t)(int16_t)imm;
                }
                result = regs->a[dreg] - imm;
                flags_sub(regs, 2u, regs->a[dreg], imm, result, 0);
            }
        } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0xc0c0) ||
                   (opcode & UINT16_C(0xf1c0)) == UINT16_C(0xc1c0)) {
            int is_signed = (opcode & UINT16_C(0x0100)) != 0;
            if (((opcode & UINT16_C(0x003f)) != UINT16_C(0x003c)) &&
                ((opcode & UINT16_C(0x0038)) != 0u)) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                status = step_mulu(bus, regs, opcode, pc, &off, is_signed);
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0xe000) &&
                   ((opcode >> 6u) & 3u) != 3u &&
                   (opcode & UINT16_C(0x0020)) == 0u &&
                   ((((opcode >> 3u) & 3u) <= 1u) ||
                    (((opcode >> 3u) & 3u) == 3u))) {
            /* Immediate-count arithmetic/logical shifts + ROR only
               (ROXR stays refused). */
            if (((opcode >> 3u) & 3u) == 3u) {
                status = step_ror_reg(regs, opcode);
            } else {
                status = step_shift(regs, opcode);
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x6000)) {
            status = step_bcc(regs, bus, opcode, pc, &off, &new_pc);
        } else if ((opcode & UINT16_C(0xf0f8)) == UINT16_C(0x50c8)) {
            status = step_dbc(regs, bus, opcode, pc, &off, &new_pc);
        } else if (opcode == UINT16_C(0x4eb9) ||
                   opcode == UINT16_C(0x4ef9) ||
                   opcode == UINT16_C(0x4eba) ||
                   opcode == UINT16_C(0x4efa) ||
                   (opcode & UINT16_C(0xfff8)) == UINT16_C(0x4e90) ||
                   (opcode & UINT16_C(0xfff8)) == UINT16_C(0x4ed0)) {
            int is_jsr = opcode == UINT16_C(0x4eb9) ||
                         opcode == UINT16_C(0x4eba) ||
                         (opcode & UINT16_C(0xfff8)) == UINT16_C(0x4e90);
                status = step_jsr_jmp(bus, regs, opcode, pc, &off, &new_pc,
                                  is_jsr, pc + (uint32_t)instruction.length);
        } else if (opcode == UINT16_C(0x4e75)) {
            if (stop_on_return != 0) {
                *steps_done = done + 1u;
                return VF2_OK;
            }
            {
                uint32_t ret = 0u;
                status = bus_read(bus, regs->a[7], 2u, &ret);
                if (status == VF2_OK) {
                    regs->a[7] += 4u;
                    new_pc = ret;
                }
            }
        } else if (opcode == UINT16_C(0x4e73)) {
            /* Legacy isolated original-ROM fixtures start at ISR
             * entry with no stacked frame; keep their old stop
             * behavior. For the opt-in IRQ API, restore actual
             * 68000 SR + PC from a six-byte supervisor stack
             * frame and resume the interrupted instruction. */
            if (regs->irq_active == 0u) {
                *steps_done = done + 1u;
                return VF2_OK;
            }
            {
                uint32_t stacked_sr = 0u, stacked_pc = 0u;
                const uint32_t old_sp = regs->a[7];
                if ((regs->sr_high & UINT16_C(0x2000)) == 0u) {
                    *steps_done = done;
                    return VF2_ERROR_UNSUPPORTED;
                }
                status = bus_read(bus, old_sp, 1u, &stacked_sr);
                if (status == VF2_OK) {
                    status = bus_read(bus, old_sp + 2u, 2u,
                                      &stacked_pc);
                }
                if (status != VF2_OK) {
                    *steps_done = done;
                    return status;
                }
                if ((stacked_pc & UINT32_C(1)) != 0u ||
                    stacked_pc >= bus->rom_size) {
                    *steps_done = done;
                    return VF2_ERROR_UNSUPPORTED;
                }
                regs->ssp = old_sp + 6u;
                regs->sr_high = (uint16_t)(stacked_sr &
                                           UINT32_C(0xffe0));
                regs->x = (uint8_t)((stacked_sr >> 4u) & 1u);
                regs->n = (uint8_t)((stacked_sr >> 3u) & 1u);
                regs->z = (uint8_t)((stacked_sr >> 2u) & 1u);
                regs->v = (uint8_t)((stacked_sr >> 1u) & 1u);
                regs->c = (uint8_t)(stacked_sr & 1u);
                if ((regs->sr_high & UINT16_C(0x2000)) == 0u) {
                    regs->a[7] = regs->usp;
                } else {
                    regs->a[7] = regs->ssp;
                }
                regs->irq_active = 0u;
                new_pc = stacked_pc;
                if (stop_on_return != 0) {
                    regs->pc = new_pc;
                    *steps_done = done + 1u;
                    return VF2_OK;
                }
            }
        } else if (opcode == UINT16_C(0x4e71)) {
            status = VF2_OK;
        } else if (opcode == UINT16_C(0x46fc)) {
            /* MOVE #imm,SR: keep historic event logging, now also
               preserve upper SR bits needed by the opt-in IRQ mask
               and RTE frame tests. No privilege fault emulation. */
            uint32_t imm = 0u;
            status = fetch_imm(bus, pc, &off, 1u, &imm);
            if (status == VF2_OK) {
                regs->x = (uint8_t)((imm >> 4u) & 1u);
                regs->n = (uint8_t)((imm >> 3u) & 1u);
                regs->z = (uint8_t)((imm >> 2u) & 1u);
                regs->v = (uint8_t)((imm >> 1u) & 1u);
                regs->c = (uint8_t)(imm & 1u);
                regs->sr_high = (uint16_t)(
                    imm & UINT32_C(0xffe0));
                status = log_event(bus, pc, imm, 1u, 1u);
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x7000) &&
                   (opcode & UINT16_C(0x0100)) == 0u) {
            /* MOVEQ: bits 11-9 are Dn, bit 8 is fixed 0. */
            unsigned dreg = (unsigned)((opcode >> 9u) & 7u);
            uint32_t value =
                (uint32_t)(int32_t)(int8_t)(opcode & 0xffu);
            regs->d[dreg] = value;
            flags_logic(regs, 2u, value);
        } else if ((opcode & UINT16_C(0xfff8)) == UINT16_C(0x4880) ||
                   (opcode & UINT16_C(0xfff8)) == UINT16_C(0x48c0)) {
            unsigned dreg = (unsigned)(opcode & 7u);
            if ((opcode & UINT16_C(0xfff8)) == UINT16_C(0x4880)) {
                regs->d[dreg] = (regs->d[dreg] & UINT32_C(0xffff0000)) |
                                (uint32_t)(int32_t)(int16_t)
                                    (regs->d[dreg] & 0xffu);
                flags_logic(regs, 1u, regs->d[dreg]);
            } else {
                regs->d[dreg] =
                    (uint32_t)(int32_t)(int16_t)(regs->d[dreg] & 0xffffu);
                flags_logic(regs, 2u, regs->d[dreg]);
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x4400) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4440) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4480)) {
            /* NEG: Dn only (decoder admits Dn only). */
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            unsigned dreg = (unsigned)(opcode & 7u);
            unsigned size = size_code == 0u ? 0u
                : (size_code == 1u ? 1u : 2u);
            uint32_t value = 0u;
            uint32_t result = 0u;
            if (size_code > 2u ||
                ((opcode >> 3u) & 7u) != 0u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                value = regs->d[dreg] & size_mask(size);
                result = (0u - value) & size_mask(size);
                if (size == 2u) {
                    regs->d[dreg] = result;
                } else {
                    regs->d[dreg] = (regs->d[dreg] & ~size_mask(size)) |
                                    result;
                }
                flags_sub(regs, size, 0u, value, result, 1);
            }
        } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x4a00)) {
            /* TST: decoder admits Dn and d16(An) only. */
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            unsigned mode = (unsigned)((opcode >> 3u) & 7u);
            unsigned reg = (unsigned)(opcode & 7u);
            unsigned size = size_code == 0u ? 0u
                : (size_code == 1u ? 1u : 2u);
            uint32_t value = 0u;
            if (size_code > 2u || (mode != 0u && mode != 5u)) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                status = ea_read(bus, regs, mode, reg, size, pc, &off,
                                 &value);
                if (status == VF2_OK) {
                    flags_logic(regs, size, value);
                }
            }
        } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x4600) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4640) ||
                   (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4680)) {
            /* NOT: decoder admits Dn only. */
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            unsigned dreg = (unsigned)(opcode & 7u);
            unsigned size = size_code == 0u ? 0u
                : (size_code == 1u ? 1u : 2u);
            uint32_t result = 0u;
            if (size_code > 2u ||
                ((opcode >> 3u) & 7u) != 0u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                result = (~regs->d[dreg]) & size_mask(size);
                if (size == 2u) {
                    regs->d[dreg] = result;
                } else {
                    regs->d[dreg] = (regs->d[dreg] & ~size_mask(size)) |
                                    result;
                }
                flags_logic(regs, size, result);
            }
        } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x5000)) {
            /* True quick shapes: class 5, bit 8 selects add/sub.
               (The decoder's SUBQ side additionally misses counts
               4-7; the decode gate refuses those first, so the
               stepper never executes them. P6 tracks the print.) */
            unsigned mode = (unsigned)((opcode >> 3u) & 7u);
            if (mode != 0u && mode != 1u && mode != 2u && mode != 3u &&
                mode != 5u && mode != 6u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                int is_sub =
                    (opcode & UINT16_C(0x0100)) != 0u;
                status = step_quick(regs, bus, opcode, pc, &off, is_sub);
            }
        } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x0800)) {
            /* Static bit operations only; dynamic (0x01xx) refuses. */
            status = step_bit(regs, bus, opcode, pc, &off);
        } else if ((opcode & UINT16_C(0xff38)) == UINT16_C(0x4200) &&
                   ((opcode >> 3u) & 7u) == 0u) {
            /* CLR Dn only (byte/word/long). Other CLR eas stay
               refused: not on any recovered path. N=0,Z=1,V=C=0
               via flags_logic; X is untouched (it never writes X).
               Needed by the L3 0xFF-reset path into boot (0x10c+). */
            unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
            unsigned dreg = (unsigned)(opcode & 7u);
            if (size_code > 2u) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                unsigned size =
                    size_code == 0u ? 0u : (size_code == 1u ? 1u : 2u);
                if (size == 2u) {
                    regs->d[dreg] = 0u;
                } else {
                    regs->d[dreg] &= ~size_mask(size);
                }
                flags_logic(regs, size, 0u);
            }
        } else if (((opcode & UINT16_C(0xffc0)) == UINT16_C(0x48c0) &&
                    (((opcode >> 3u) & 7u) == 2u ||
                     ((opcode >> 3u) & 7u) == 4u)) ||
                   ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x4cc0) &&
                    ((opcode >> 3u) & 7u) == 3u)) {
            /* MC68000 MOVEM reg->(An) is legal, not just -(An).
             * Preserve tight dispatch: 0x48C0 mode0 is EXT.L. */
            status = step_movem(regs, bus, opcode, pc, &off);
        } else {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status != VF2_OK) {
            *steps_done = done;
            return status;
        }
        if (off != instruction.length) {
            *steps_done = done;
            /* Stepper/decoder length divergence tripwire. */
            return VF2_ERROR_UNSUPPORTED;
        }
        regs->pc = new_pc;
        ++done;
    }
    *steps_done = done;
    return VF2_ERROR_UNSUPPORTED;
}

/* MC68000 original integer timing reference:
 * NXP MC68000UM Tables 8-8,8-9,8-10 and 8-11.
 * This is a closed timing subset, not a generalized cycle
 * emulator. Return UNSUPPORTED for every absent timing rather
 * than treating a four-cycle instruction-count floor as actual
 * execution time. Branch timings depend on current CCR and
 * DBcc low-word count, NOT only on the opcode.
 */
static vf2_status vf2_m68k_cycle_cost(
    const vf2_m68k_bus *bus, const vf2_m68k_regs *regs,
    uint32_t *cost
)
{
    uint16_t op = 0u;
    vf2_status status;
    uint32_t pc;
    if (bus == NULL || regs == NULL || cost == NULL)
        return VF2_ERROR_INVALID_ARGUMENT;
    pc = regs->pc;
    status = rom_u16(bus, pc, &op);
    if (status != VF2_OK) return status;
    if (op == UINT16_C(0x4e71) ||
        ((op & UINT16_C(0xf100)) == UINT16_C(0x7000))) {
        *cost = 4u;
        return VF2_OK;
    }
    if (op == UINT16_C(0x4e75)) {
        *cost = 16u;
        return VF2_OK;
    }
    if (op == UINT16_C(0x4e73) && regs->irq_active != 0u) {
        *cost = 20u;
        return VF2_OK;
    }
    if (op == UINT16_C(0x48e7) ||
        op == UINT16_C(0x4cdf)) {
        uint16_t mask = 0u;
        unsigned n = 0u;
        status = rom_u16(bus, pc + 2u, &mask);
        if (status != VF2_OK) return status;
        while (mask != 0u) {
            n += (unsigned)(mask & UINT16_C(1));
            mask >>= 1u;
        }
        /* MOVEM.L R->M -(An) 8+8*n; M->R (An)+ 12+8*n.
         * These are the actual original VF2 L2 ISR opcodes. */
        *cost = (op == UINT16_C(0x48e7) ? 8u : 12u) + 8u * n;
        return VF2_OK;
    }
    if ((op & UINT16_C(0xf0f8)) == UINT16_C(0x50c8)) {
        const unsigned condition = (unsigned)((op >> 8u) & 15u);
        const unsigned reg = (unsigned)(op & 7u);
        if (bcc_holds(regs, condition)) {
            *cost = 12u;
        } else if ((regs->d[reg] & UINT32_C(0xffff)) == 0u) {
            *cost = 14u;
        } else {
            *cost = 10u;
        }
        return VF2_OK;
    }
    if ((op & UINT16_C(0xf000)) == UINT16_C(0x6000)) {
        const unsigned condition = (unsigned)((op >> 8u) & 15u);
        const unsigned short_disp = (unsigned)(op & UINT16_C(0xff));
        if (short_disp == 0xffu) {
            return VF2_ERROR_UNSUPPORTED; /* Bcc.L is not 68000 */
        }
        if (condition == 0u) {
            *cost = 10u; /* BRA, byte or word */
        } else if (condition == 1u) {
            /* Existing step_bcc does NOT model BSR's saved return
             * PC on the 68000 stack. Never admit a timing whose
             * instruction semantics cannot be trusted. */
            return VF2_ERROR_UNSUPPORTED;
        } else {
            *cost = bcc_holds(regs, condition) ? 10u :
                (short_disp == 0u ? 12u : 8u);
        }
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

vf2_status vf2_m68k_run_cycle_slice(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    uint32_t stop_pc, uint32_t cycle_budget,
    uint32_t *cycles_done, uint32_t *steps_done,
    int *suspended
)
{
    if (bus == NULL || regs == NULL || cycles_done == NULL ||
        steps_done == NULL || suspended == NULL || cycle_budget == 0u)
        return VF2_ERROR_INVALID_ARGUMENT;
    *cycles_done = 0u;
    *steps_done = 0u;
    *suspended = 0;
    while (regs->pc != stop_pc) {
        uint32_t cost = 0u;
        uint32_t steps = 0u;
        int partial = 0;
        vf2_status result = vf2_m68k_cycle_cost(bus, regs, &cost);
        if (result != VF2_OK) return result;
        if (cost > cycle_budget - *cycles_done) {
            *suspended = 1;
            return VF2_OK;
        }
        result = vf2_m68k_run_slice(
            bus, regs, stop_pc, 0, 1u, &steps, &partial);
        if (result != VF2_OK) return result;
        if (steps != 1u) return VF2_ERROR_UNSUPPORTED;
        *cycles_done += cost;
        ++*steps_done;
        if (*cycles_done == cycle_budget && regs->pc != stop_pc) {
            *suspended = 1;
            return VF2_OK;
        }
    }
    return VF2_OK;
}

/* Reentrant instruction-quanta shim around the legacy stepper.
 * max_steps reaching VF2_ERROR_UNSUPPORTED was ambiguous with
 * an opcode refusal. The legacy run contract guarantees the
 * former executes exactly max_steps; the latter executes fewer.
 * Never report a genuine error as a completed/suspendable step.
 * PC and RAM remain unmodified by this shim itself. */
vf2_status vf2_m68k_run_slice(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    uint32_t stop_pc, int stop_on_return,
    uint32_t max_instructions, uint32_t *steps_done,
    int *suspended
)
{
    vf2_status status;
    if (bus == NULL || regs == NULL || steps_done == NULL ||
        suspended == NULL || max_instructions == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *suspended = 0;
    status = vf2_m68k_run(
        bus, regs, stop_pc, stop_on_return,
        max_instructions, steps_done);
    if (status == VF2_ERROR_UNSUPPORTED &&
        *steps_done == max_instructions) {
        /* The last legal instruction may have landed exactly
         * on stop_pc: caller shouldn't have to run an extra
         * zero-step quantum to learn it is at the boundary. */
        *suspended = regs->pc != stop_pc;
        return VF2_OK;
    }
    return status;
}

void vf2_m68k_snapshot_init(vf2_m68k_snapshot *snap)
{
    if (snap != NULL) {
        memset(snap, 0, sizeof(*snap));
    }
}

vf2_status vf2_m68k_snapshot_capture(
    vf2_m68k_snapshot *snap,
    const vf2_m68k_bus *bus,
    const vf2_m68k_regs *regs
)
{
    uint8_t *image = NULL;
    if (snap == NULL || bus == NULL || regs == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (bus->ram_size != 0u && bus->ram == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (bus->scsp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (bus->event_count > VF2_M68K_STEP_MAX_EVENTS) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (bus->ram_size != 0u) {
        image = (uint8_t *)calloc(bus->ram_size, 1u);
        if (image == NULL) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        memcpy(image, bus->ram, bus->ram_size);
    }
    free(snap->ram);
    snap->ram = image;
    snap->ram_size = bus->ram_size;
    snap->regs = *regs;
    snap->scsp = *bus->scsp;
    snap->event_count = bus->event_count;
    memcpy(snap->events, bus->events, sizeof(snap->events));
    return VF2_OK;
}

vf2_status vf2_m68k_snapshot_restore(
    const vf2_m68k_snapshot *snap,
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs
)
{
    if (snap == NULL || bus == NULL || regs == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (snap->ram_size != bus->ram_size) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (snap->ram_size != 0u &&
        (snap->ram == NULL || bus->ram == NULL)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (bus->scsp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (snap->event_count > VF2_M68K_STEP_MAX_EVENTS) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *regs = snap->regs;
    if (snap->ram_size != 0u) {
        memcpy(bus->ram, snap->ram, snap->ram_size);
    }
    *bus->scsp = snap->scsp;
    bus->event_count = snap->event_count;
    memcpy(bus->events, snap->events, sizeof(bus->events));
    return VF2_OK;
}

static int scsp_state_equal(const vf2_scsp *a, const vf2_scsp *b)
{
    size_t index = 0u;
    if (memcmp(a->registers, b->registers, sizeof(a->registers)) != 0) {
        return 0;
    }
    if (a->sample_ram != b->sample_ram ||
        a->sample_rom != b->sample_rom ||
        a->sample_ram_size != b->sample_ram_size ||
        a->sample_rom_size != b->sample_rom_size) {
        return 0;
    }
    if (memcmp(a->midi_input, b->midi_input, sizeof(a->midi_input)) != 0 ||
        a->midi_read_index != b->midi_read_index ||
        a->midi_write_index != b->midi_write_index) {
        return 0;
    }
    for (index = 0u; index < VF2_SCSP_SLOT_COUNT; ++index) {
        if (a->slot_address[index] != b->slot_address[index] ||
            a->slot_step[index] != b->slot_step[index] ||
            a->slot_envelope[index] != b->slot_envelope[index] ||
            a->slot_active[index] != b->slot_active[index] ||
            a->slot_envelope_state[index] !=
                b->slot_envelope_state[index] ||
            a->slot_backward[index] != b->slot_backward[index]) {
            return 0;
        }
    }
    return 1;
}

static int regs_equal(const vf2_m68k_regs *a, const vf2_m68k_regs *b)
{
    size_t index = 0u;
    for (index = 0u; index < 8u; ++index) {
        if (a->d[index] != b->d[index] || a->a[index] != b->a[index]) {
            return 0;
        }
    }
    return a->pc == b->pc && a->x == b->x && a->n == b->n &&
        a->z == b->z && a->v == b->v && a->c == b->c &&
        a->sr_high == b->sr_high && a->usp == b->usp &&
        a->ssp == b->ssp && a->irq_active == b->irq_active;
}

static int events_equal(const vf2_m68k_snapshot *a, const vf2_m68k_snapshot *b)
{
    size_t index = 0u;
    if (a->event_count != b->event_count) {
        return 0;
    }
    for (index = 0u; index < a->event_count; ++index) {
        if (a->events[index].address != b->events[index].address ||
            a->events[index].value != b->events[index].value ||
            a->events[index].size != b->events[index].size ||
            a->events[index].is_write != b->events[index].is_write) {
            return 0;
        }
    }
    return 1;
}

int vf2_m68k_snapshot_equal(
    const vf2_m68k_snapshot *a,
    const vf2_m68k_snapshot *b
)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    if (!regs_equal(&a->regs, &b->regs)) {
        return 0;
    }
    if (a->ram_size != b->ram_size) {
        return 0;
    }
    if (a->ram_size != 0u &&
        (a->ram == NULL || b->ram == NULL ||
         memcmp(a->ram, b->ram, a->ram_size) != 0)) {
        return 0;
    }
    if (!scsp_state_equal(&a->scsp, &b->scsp)) {
        return 0;
    }
    if (!events_equal(a, b)) {
        return 0;
    }
    return 1;
}

void vf2_m68k_snapshot_destroy(vf2_m68k_snapshot *snap)
{
    if (snap != NULL) {
        free(snap->ram);
        vf2_m68k_snapshot_init(snap);
    }
}
