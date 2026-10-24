#include "vf2/m68k_step.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void make_bus(
    vf2_m68k_bus *bus,
    vf2_scsp *scsp,
    uint8_t *ram,
    const uint8_t *rom,
    size_t rom_size
)
{
    size_t index = 0u;
    bus->ram = ram;
    bus->ram_size = 32768u;
    bus->rom = rom;
    bus->rom_size = rom_size;
    bus->sample_rom = NULL;
    bus->sample_rom_size = 0u;
    bus->scsp = scsp;
    bus->event_count = 0u;
    for (index = 0u; index < 32768u; ++index) {
        ram[index] = 0u;
    }
    vf2_scsp_initialize(scsp);
}

int main(void)
{
    static const uint8_t program[] = {
        0x70, 0x0a,             /* 0x00 moveq #10,d0 */
        0x56, 0x00,             /* 0x02 addq.b #3,d0 -> 13 */
        0x0c, 0x00, 0x00, 0x0d, /* 0x04 cmpi.b #$0d,d0 (Z=1) */
        0x66, 0x04,             /* 0x08 bne +4 (not taken) */
        0x72, 0x01,             /* 0x0a moveq #1,d1 */
        0x60, 0x02,             /* 0x0c bra +2 -> 0x10 */
        0x72, 0x02,             /* 0x0e moveq #2,d1 (skipped) */
        0x74, 0x03,             /* 0x10 moveq #3,d2 */
        0x53, 0x02,             /* 0x12 subq.b #1,d2 */
        0x51, 0xca, 0xff, 0xfc, /* 0x14 dbf d2,-4 -> 0x14+2-4 = 0x12 (4 iterations) */
        0x41, 0xf8, 0x20, 0x00, /* 0x18 lea $2000,a0 */
        0x28, 0x3c, 0x12, 0x34, 0x56, 0x78, /* 0x1c move.l #$12345678,d4 */
        0x20, 0x84,             /* 0x22 move.l d4,(a0) */
        0x4e, 0xba, 0x00, 0x04, /* 0x24 jsr +4 -> 0x24+2+4 = 0x2a */
        0x60, 0x06,             /* 0x28 bra +6 -> 0x30 (stop) */
        0x76, 0x07,             /* 0x2a moveq #7,d3 */
        0x4e, 0x75              /* 0x2c rts (halts with stop_on_return) */
    };
    static const uint8_t bad_program[] = { 0x4a, 0xfc };
    static uint8_t ram[32768];
    vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    make_bus(&bus, &scsp, ram, program, sizeof(program));
    vf2_m68k_regs_reset(&regs);
    regs.a[7] = 32512u;
    status = vf2_m68k_run(&bus, &regs, 0x30u, 1, 100u, &done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.d[0] == 13u);
    EXPECT_TRUE(regs.d[1] == 1u);
    EXPECT_TRUE(regs.d[2] == 65535u);
    EXPECT_TRUE(regs.d[3] == 7u);
    EXPECT_TRUE(regs.d[4] == UINT32_C(0x12345678));
    EXPECT_TRUE(regs.a[0] == 0x2000u);
    EXPECT_TRUE(regs.z == 0u);
    EXPECT_TRUE(ram[0x2000u] == 0x12u);
    EXPECT_TRUE(ram[0x2001u] == 0x34u);
    EXPECT_TRUE(ram[0x2002u] == 0x56u);
    EXPECT_TRUE(ram[0x2003u] == 0x78u);
    EXPECT_TRUE(regs.a[7] == 32508u);
    EXPECT_TRUE(bus.event_count == 0u);

    /* MOVEM.L D0-D7,(A2) is LEGAL on MC68000. Regression for
     * original sound CPU ROM 0xCB6/0x23FA instruction 48 D2 00 FF.
     * Check full 8-longword order, unchanged A2, and fail-closed
     * out-of-bounds control destination. */
    {
        static const uint8_t movem_direct[] = { 0x48, 0xd2, 0x00, 0xff };
        size_t k;
        uint32_t val;
        make_bus(&bus, &scsp, ram, movem_direct, sizeof(movem_direct));
        vf2_m68k_regs_reset(&regs);
        regs.a[2] = UINT32_C(0x2000);
        for (k = 0u; k < 8u; ++k) {
            regs.d[k] = UINT32_C(0x12340000) | (uint32_t)(k * 0x101u);
        }
        status = vf2_m68k_run(&bus, &regs, 4u, 0, 2u, &done);
        fprintf(stderr,
                "MOVEM diag: status=%d pc=%05x steps=%u a2=%05x "
                "D0=%08x dest=%02x%02x%02x%02x\n",
                (int)status, (unsigned)regs.pc, (unsigned)done,
                (unsigned)regs.a[2], (unsigned)regs.d[0],
                (unsigned)ram[0x2000], (unsigned)ram[0x2001],
                (unsigned)ram[0x2002], (unsigned)ram[0x2003]);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == 4u);
        EXPECT_TRUE(regs.a[2] == UINT32_C(0x2000));
        EXPECT_TRUE(done == 1u);
        for (k = 0u; k < 8u; ++k) {
            size_t offset = 0x2000u + k * 4u;
            val = regs.d[k];
            EXPECT_TRUE(ram[offset] == (uint8_t)(val >> 24u));
            EXPECT_TRUE(ram[offset + 1u] == (uint8_t)(val >> 16u));
            EXPECT_TRUE(ram[offset + 2u] == (uint8_t)(val >> 8u));
            EXPECT_TRUE(ram[offset + 3u] == (uint8_t)val);
        }
        regs.pc = 0u;
        regs.a[2] = (UINT32_C(32768) - UINT32_C(4));
        status = vf2_m68k_run(&bus, &regs, 4u, 0, 1u, &done);
        EXPECT_TRUE(status != VF2_OK);
    }
    /* Fail closed on an unimplemented opcode. */
    make_bus(&bus, &scsp, ram, bad_program, sizeof(bad_program));
    vf2_m68k_regs_reset(&regs);
    status = vf2_m68k_run(&bus, &regs, 0x10u, 0, 100u, &done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);

    /* Fail closed on step exhaustion. */
    make_bus(&bus, &scsp, ram, program, sizeof(program));
    vf2_m68k_regs_reset(&regs);
    regs.a[7] = 32512u;
    status = vf2_m68k_run(&bus, &regs, 0x30u, 1, 2u, &done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(done == 2u);

    /* The CPU state and effects must survive instruction-quanta.
     * This is NOT an IRQ or cycle-budget test, only a correct
     * non-resetting execution primitive for future scheduling. */
    {
        static const uint8_t resumable_program[] = {
            0x70, 0x0a,             /* moveq #10,d0 */
            0x56, 0x00,             /* addq.b #3,d0 */
            0x0c, 0x00, 0x00, 0x0d, /* cmpi.b #0x0d,d0 */
            0x66, 0x04,             /* bne (not taken) */
            0x72, 0x01,             /* moveq #1,d1 */
            0x60, 0x02,             /* bra to 0x10 */
            0x72, 0x02,             /* skipped */
            0x74, 0x03,             /* moveq #3,d2 */
            0x53, 0x02,             /* subq.b #1,d2 */
            0x51, 0xca, 0xff, 0xfc, /* dbf d2,-4 */
            0x41, 0xf8, 0x20, 0x00, /* lea 0x2000,a0 */
            0x28, 0x3c, 0x12, 0x34, 0x56, 0x78, /* move.l */
            0x20, 0x84,             /* move.l d4,(a0) */
            0x4e, 0xba, 0x00, 0x04, /* jsr to 0x2a */
            0x60, 0x06,             /* bra to 0x30 */
            0x76, 0x07,             /* moveq #7,d3 */
            0x4e, 0x75              /* rts */
        };
        static uint8_t reference_ram[32768u];
        vf2_m68k_regs reference_regs;
        uint32_t reference_steps = 0u;
        uint32_t sliced_steps = 0u;
        size_t iteration;
        int suspended = 0;
        make_bus(&bus, &scsp, ram,
                 resumable_program, sizeof(resumable_program));
        vf2_m68k_regs_reset(&regs);
        regs.a[7] = 32512u;
        status = vf2_m68k_run(
            &bus, &regs, 0x30u, 0, 100u, &reference_steps);
        EXPECT_TRUE(status == VF2_OK);
        memcpy(reference_ram, ram, sizeof(reference_ram));
        reference_regs = regs;

        make_bus(&bus, &scsp, ram,
                 resumable_program, sizeof(resumable_program));
        vf2_m68k_regs_reset(&regs);
        regs.a[7] = 32512u;
        for (iteration = 0u; iteration < 100u; ++iteration) {
            uint32_t executed = 0u;
            status = vf2_m68k_run_slice(
                &bus, &regs, 0x30u, 0, 3u,
                &executed, &suspended);
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(executed <= 3u);
            sliced_steps += executed;
            if (status != VF2_OK || !suspended) break;
        }
        EXPECT_TRUE(iteration > 1u && iteration < 100u);
        EXPECT_TRUE(!suspended);
        EXPECT_TRUE(sliced_steps == reference_steps);
        EXPECT_TRUE(regs.pc == reference_regs.pc);
        for (iteration = 0u; iteration < 8u; ++iteration) {
            EXPECT_TRUE(regs.d[iteration] ==
                        reference_regs.d[iteration]);
            EXPECT_TRUE(regs.a[iteration] ==
                        reference_regs.a[iteration]);
        }
        EXPECT_TRUE(regs.x == reference_regs.x &&
                    regs.n == reference_regs.n &&
                    regs.z == reference_regs.z &&
                    regs.v == reference_regs.v &&
                    regs.c == reference_regs.c);
        EXPECT_TRUE(memcmp(ram, reference_ram,
                           sizeof(reference_ram)) == 0);
        status = vf2_m68k_run_slice(
            &bus, &regs, 0x30u, 0, 3u, &done, &suspended);
        EXPECT_TRUE(status == VF2_OK && !suspended && done == 0u);

        make_bus(&bus, &scsp, ram, bad_program,
                 sizeof(bad_program));
        vf2_m68k_regs_reset(&regs);
        suspended = 1;
        status = vf2_m68k_run_slice(
            &bus, &regs, 0x10u, 0, 3u, &done, &suspended);
        EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(done == 0u && !suspended);
        EXPECT_TRUE(vf2_m68k_run_slice(
            &bus, &regs, 0x10u, 0, 0u, &done, &suspended) ==
            VF2_ERROR_INVALID_ARGUMENT);
        EXPECT_TRUE(vf2_m68k_run_slice(
            &bus, &regs, 0x10u, 0, 3u, &done, NULL) ==
            VF2_ERROR_INVALID_ARGUMENT);
    }

    /* Real six-byte 68000 autovector frame + RTE restoration,
     * without assuming accurate instruction timing or IRQ phase. */
    {
        static uint8_t irq_rom[512u];
        const uint32_t stack_top = UINT32_C(0x7000);
        const uint32_t user_sp = UINT32_C(0x6000);
        uint32_t steps = 0u;
        int suspended = 0;
        memset(irq_rom, 0, sizeof(irq_rom));
        /* Vector 26, level-2 autovector -> ROM offset 0x20. */
        irq_rom[0x68u] = 0u; irq_rom[0x69u] = 0u;
        irq_rom[0x6au] = 0u; irq_rom[0x6bu] = 0x20u;
        irq_rom[0x20u] = 0x72u; irq_rom[0x21u] = 0x11u;
        irq_rom[0x22u] = 0x4eu; irq_rom[0x23u] = 0x73u;
        irq_rom[0x100u] = 0x70u; irq_rom[0x101u] = 0x05u;
        irq_rom[0x102u] = 0x56u; irq_rom[0x103u] = 0x00u;
        irq_rom[0x104u] = 0x4eu; irq_rom[0x105u] = 0x71u;

        make_bus(&bus, &scsp, ram, irq_rom, sizeof(irq_rom));
        vf2_m68k_regs_reset(&regs);
        regs.pc = UINT32_C(0x100);
        regs.a[7] = stack_top;
        regs.sr_high = UINT16_C(0x2100); /* supervisor mask 1 */
        regs.x = 1u; regs.z = 1u;
        status = vf2_m68k_run_slice(
            &bus, &regs, 0x104u, 0, 1u,
            &steps, &suspended);
        EXPECT_TRUE(status == VF2_OK && suspended && steps == 1u);
        EXPECT_TRUE(regs.pc == UINT32_C(0x102));
        status = vf2_m68k_irq_autovector_enter(&bus, &regs, 2u);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.irq_active == 1u);
        EXPECT_TRUE(regs.pc == UINT32_C(0x20));
        EXPECT_TRUE(regs.a[7] == stack_top - 6u);
        EXPECT_TRUE(regs.sr_high == UINT16_C(0x2200));
        EXPECT_TRUE(ram[stack_top - 6u] == 0x21u);
        EXPECT_TRUE(ram[stack_top - 5u] == 0x10u);
        EXPECT_TRUE(ram[stack_top - 4u] == 0u &&
                    ram[stack_top - 3u] == 0u);
        EXPECT_TRUE(ram[stack_top - 2u] == 0x01u &&
                    ram[stack_top - 1u] == 0x02u);
        EXPECT_TRUE(vf2_m68k_irq_autovector_enter(
            &bus, &regs, 3u) == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(vf2_m68k_irq_autovector_enter(
            &bus, &regs, 2u) == VF2_ERROR_UNSUPPORTED);
        status = vf2_m68k_run_slice(
            &bus, &regs, UINT32_C(0x102), 0,
            1u, &steps, &suspended);
        EXPECT_TRUE(status == VF2_OK && suspended && steps == 1u);
        EXPECT_TRUE(regs.d[1] == 0x11u && regs.pc == 0x22u);
        status = vf2_m68k_run_slice(
            &bus, &regs, UINT32_C(0x102), 0,
            1u, &steps, &suspended);
        EXPECT_TRUE(status == VF2_OK && !suspended && steps == 1u);
        EXPECT_TRUE(regs.pc == UINT32_C(0x102));
        EXPECT_TRUE(regs.irq_active == 0u);
        EXPECT_TRUE(regs.sr_high == UINT16_C(0x2100));
        EXPECT_TRUE(regs.a[7] == stack_top && regs.ssp == stack_top);
        EXPECT_TRUE(regs.x == 1u && regs.z == 1u);
        status = vf2_m68k_run_slice(
            &bus, &regs, UINT32_C(0x104), 0,
            1u, &steps, &suspended);
        EXPECT_TRUE(status == VF2_OK && !suspended && steps == 1u);
        EXPECT_TRUE(regs.d[0] == 8u);

        /* User-mode entry switches USP to SSP, RTE restores it. */
        make_bus(&bus, &scsp, ram, irq_rom, sizeof(irq_rom));
        vf2_m68k_regs_reset(&regs);
        regs.pc = UINT32_C(0x100);
        regs.sr_high = 0u;
        regs.a[7] = user_sp;
        regs.ssp = stack_top;
        status = vf2_m68k_irq_autovector_enter(&bus, &regs, 2u);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.usp == user_sp);
        EXPECT_TRUE(regs.a[7] == stack_top - 6u);
        EXPECT_TRUE(ram[stack_top - 6u] == 0u);
        status = vf2_m68k_run(&bus, &regs, 0x100u, 0, 5u, &steps);
        EXPECT_TRUE(status == VF2_OK && steps == 2u);
        EXPECT_TRUE(regs.pc == 0x100u);
        EXPECT_TRUE(regs.irq_active == 0u);
        EXPECT_TRUE(regs.a[7] == user_sp);
        EXPECT_TRUE(regs.ssp == stack_top);
        EXPECT_TRUE((regs.sr_high & UINT16_C(0x2000)) == 0u);

        /* Masked and unaligned stacks must refuse BEFORE mutation. */
        vf2_m68k_regs_reset(&regs);
        regs.pc = UINT32_C(0x100);
        regs.a[7] = stack_top;
        regs.sr_high = UINT16_C(0x2300);
        status = vf2_m68k_irq_autovector_enter(&bus, &regs, 2u);
        EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(regs.pc == 0x100u && regs.a[7] == stack_top);
        regs.sr_high = UINT16_C(0x2000);
        regs.a[7] = UINT32_C(5);
        status = vf2_m68k_irq_autovector_enter(&bus, &regs, 2u);
        EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(regs.pc == 0x100u && regs.a[7] == 5u);
    }

    return failures == 0 ? 0 : 1;
}
