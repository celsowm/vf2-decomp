#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/boot.h"
#include "vf2/hash.h"
#include "vf2/hybrid.h"
#include "vf2/rom.h"
#include "vf2/model2a.h"
#include "vf2/platform.h"
#include "vf2/recovered.h"

static int failures = 0;

int vf2_test_i960_decoder(void);
int vf2_test_i960_executor(void);
int vf2_test_i960_snapshot(void);
int vf2_test_i960_interrupts(void);
int vf2_test_i960_cfg(void);
int vf2_test_i960_semantics(void);
int vf2_test_task_catalog(void);

#define EXPECT_TRUE(expression)                                      \
    do {                                                             \
        if (!(expression)) {                                         \
            fprintf(                                                 \
                stderr,                                              \
                "FAILED %s:%d: %s\n",                                \
                __FILE__,                                            \
                __LINE__,                                            \
                #expression                                          \
            );                                                       \
            ++failures;                                              \
        }                                                            \
    } while (0)

static void test_crc32(void)
{
    static const char text[] = "123456789";
    EXPECT_TRUE(
        vf2_crc32(text, sizeof(text) - 1u) == 0xcbf43926u
    );
}

static void test_sha1(void)
{
    static const char text[] = "abc";
    static const char expected[] =
        "a9993e364706816aba3e25717850c26c9cd0d89d";
    uint8_t digest[VF2_SHA1_SIZE];
    char hex[VF2_SHA1_HEX_SIZE];

    vf2_sha1(text, sizeof(text) - 1u, digest);
    vf2_sha1_to_hex(digest, hex);

    EXPECT_TRUE(strcmp(hex, expected) == 0);
}

static void test_load32_word(void)
{
    uint8_t destination[12] = {0u};
    const uint8_t source[] = {0x11u, 0x22u, 0x33u, 0x44u};

    EXPECT_TRUE(
        vf2_apply_rom_load(
            destination,
            sizeof(destination),
            source,
            sizeof(source),
            2u,
            VF2_LOAD32_WORD
        ) == VF2_OK
    );

    EXPECT_TRUE(destination[2] == 0x11u);
    EXPECT_TRUE(destination[3] == 0x22u);
    EXPECT_TRUE(destination[6] == 0x33u);
    EXPECT_TRUE(destination[7] == 0x44u);
}

static void test_load32_byte(void)
{
    uint8_t destination[13] = {0u};
    const uint8_t source[] = {0xaau, 0xbbu, 0xccu};

    EXPECT_TRUE(
        vf2_apply_rom_load(
            destination,
            sizeof(destination),
            source,
            sizeof(source),
            1u,
            VF2_LOAD32_BYTE
        ) == VF2_OK
    );

    EXPECT_TRUE(destination[1] == 0xaau);
    EXPECT_TRUE(destination[5] == 0xbbu);
    EXPECT_TRUE(destination[9] == 0xccu);
}

static void test_load16_word_swap(void)
{
    uint8_t destination[4] = {0u};
    const uint8_t source[] = {0x11u, 0x22u, 0x33u, 0x44u};

    EXPECT_TRUE(
        vf2_apply_rom_load(
            destination,
            sizeof(destination),
            source,
            sizeof(source),
            0u,
            VF2_LOAD16_WORD_SWAP
        ) == VF2_OK
    );

    EXPECT_TRUE(destination[0] == 0x22u);
    EXPECT_TRUE(destination[1] == 0x11u);
    EXPECT_TRUE(destination[2] == 0x44u);
    EXPECT_TRUE(destination[3] == 0x33u);
}

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

typedef struct test_copro_callback_state {
    size_t calls;
    uint32_t address;
    uint32_t value;
    vf2_status result;
} test_copro_callback_state;

static vf2_status test_copro_write_callback(
    void *context,
    uint32_t address,
    const void *source,
    size_t size
)
{
    test_copro_callback_state *state =
        (test_copro_callback_state *)context;
    const uint8_t *bytes = (const uint8_t *)source;

    if (state == NULL || source == NULL || size != sizeof(uint32_t)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    ++state->calls;
    state->address = address;
    state->value = (uint32_t)bytes[0] |
                   ((uint32_t)bytes[1] << 8u) |
                   ((uint32_t)bytes[2] << 16u) |
                   ((uint32_t)bytes[3] << 24u);
    return state->result;
}

static void test_model2a_copro_callbacks(void)
{
    vf2_model2a machine;
    test_copro_callback_state state = {0u, 0u, 0u, VF2_OK};
    uint32_t value = 0u;

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    EXPECT_TRUE(vf2_model2a_set_copro_callbacks(
        &machine, NULL, test_copro_write_callback, &state
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_COPRO_PORT_BASE + 0x24u, UINT32_C(0x12345678)
    ) == VF2_OK);
    EXPECT_TRUE(state.calls == 1u);
    EXPECT_TRUE(state.address == VF2_COPRO_PORT_BASE + 0x24u);
    EXPECT_TRUE(state.value == UINT32_C(0x12345678));
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_COPRO_PORT_BASE + 0x24u, &value
    ) == VF2_OK);
    EXPECT_TRUE(value == 0u);

    state.result = VF2_ERROR_UNSUPPORTED;
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_COPRO_PORT_BASE + 0x28u, UINT32_C(0xaabbccdd)
    ) == VF2_OK);
    EXPECT_TRUE(state.calls == 2u);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_COPRO_PORT_BASE + 0x28u, &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0xaabbccdd));
    vf2_model2a_shutdown(&machine);
}

static void test_model2a_host_input(void)
{
    vf2_model2a machine;
    uint8_t value = 0u;

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 2u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xff));
    EXPECT_TRUE(vf2_model2a_set_input(
        &machine,
        VF2_PLATFORM_BUTTON_COIN | VF2_PLATFORM_BUTTON_START |
        VF2_PLATFORM_BUTTON_PUNCH | VF2_PLATFORM_BUTTON_KICK |
        VF2_PLATFORM_BUTTON_GUARD | VF2_PLATFORM_BUTTON_UP
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 2u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xee));
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 4u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xd8));
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 0x10u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xee));
    EXPECT_TRUE(vf2_model2a_set_input(
        &machine, VF2_PLATFORM_BUTTON_TEST
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 0x10u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xfb));
    EXPECT_TRUE(vf2_model2a_set_input(
        &machine,
        VF2_PLATFORM_BUTTON_COIN | VF2_PLATFORM_BUTTON_START |
        VF2_PLATFORM_BUTTON_PUNCH | VF2_PLATFORM_BUTTON_KICK |
        VF2_PLATFORM_BUTTON_GUARD | VF2_PLATFORM_BUTTON_UP
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 0x12u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xd8));
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 0x14u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xff));
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 6u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0xff));
    EXPECT_TRUE(vf2_model2a_set_input(
        &machine,
        VF2_PLATFORM_BUTTON_P2_PUNCH | VF2_PLATFORM_BUTTON_P2_KICK |
        VF2_PLATFORM_BUTTON_P2_GUARD | VF2_PLATFORM_BUTTON_P2_UP |
        VF2_PLATFORM_BUTTON_P2_LEFT
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 6u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0x58));
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 0x14u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0x58));

    value = UINT8_C(0xfb);
    EXPECT_TRUE(vf2_model2a_write(
        &machine, VF2_IO_CONTROL_BASE + 0x10u, &value, sizeof(value)
    ) == VF2_OK);
    value = UINT8_C(0x55);
    EXPECT_TRUE(vf2_model2a_write(
        &machine, VF2_IO_CONTROL_BASE + 4u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(
        &machine, VF2_IO_CONTROL_BASE + 4u, &value, sizeof(value)
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT8_C(0x55));
    vf2_model2a_shutdown(&machine);
}

static void test_boot_vectors(void)
{
    uint8_t data[32] = {0u};
    vf2_i960_boot_vectors vectors;

    write_le32(data + 0u, 0x00000000u);
    write_le32(data + 4u, 0x00003000u);
    write_le32(data + 8u, 0x00000000u);
    write_le32(data + 12u, 0x000000b0u);

    EXPECT_TRUE(
        vf2_parse_i960_boot_vectors(
            data,
            sizeof(data),
            &vectors
        ) == VF2_OK
    );

    EXPECT_TRUE(vectors.system_address_table == 0x00000000u);
    EXPECT_TRUE(vectors.initial_prcb == 0x00003000u);
    EXPECT_TRUE(vectors.start_ip == 0x000000b0u);
}


static void test_recovered_boot_stage1(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_recovered_boot_stage1_report report;
    uint8_t *rom = NULL;
    size_t index = 0u;

    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    EXPECT_TRUE(rom != NULL);
    if (rom == NULL) {
        return;
    }
    write_le32(rom + 0x3880u, 0x11111111u);
    write_le32(rom + 0x3884u, 0x22222222u);
    write_le32(rom + 0x3888u, 0xffffffffu);
    for (index = 0u; index < 0x410u; ++index) {
        rom[0x3a80u + index] = (uint8_t)index;
    }
    for (index = 0u; index < 0xb0u; ++index) {
        rom[0x3000u + index] = (uint8_t)(0x80u + index);
    }

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL || machine.buffer_ram == NULL) {
        free(rom);
        return;
    }
    EXPECT_TRUE(
        vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK
    );
    memset(machine.work_ram, 0xa5, machine.work_ram_size);
    memset(machine.buffer_ram, 0x5a, machine.buffer_ram_size);
    memset(machine.cpu_control, 0x44, machine.cpu_control_size);
    vf2_i960_cpu_reset(&cpu, 0u, 0x3000u, 0xb0u);

    EXPECT_TRUE(
        vf2_recovered_boot_stage1_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(report.next_instruction == 0x000001b0u);
    EXPECT_TRUE(report.replacement_prcb == 0x005ff410u);
    EXPECT_TRUE(report.cpu_control_bytes_copied == 8u);
    EXPECT_TRUE(report.interrupt_state_bytes_copied == 0x4c0u);
    EXPECT_TRUE(report.work_ram_words_cleared == 0x0003fff8u);
    EXPECT_TRUE(report.buffer_ram_words_cleared == 0x00008000u);
    EXPECT_TRUE(cpu.ip == 0x1b0u);
    EXPECT_TRUE(cpu.prcb == 0x005ff410u);
    EXPECT_TRUE(cpu.reinitialized);
    EXPECT_TRUE(cpu.arithmetic_control == 2u);
    EXPECT_TRUE(machine.cpu_control[0] == 0x11u);
    EXPECT_TRUE(machine.cpu_control[4] == 0x22u);
    EXPECT_TRUE(machine.cpu_control[8] == 0x44u);
    EXPECT_TRUE(machine.work_ram[0] == 0u);
    EXPECT_TRUE(machine.buffer_ram[0] == 0u);
    EXPECT_TRUE(machine.buffer_ram[0x20000u] == 0x5au);
    EXPECT_TRUE(machine.work_ram[0xff000u] == 0u);
    EXPECT_TRUE(machine.work_ram[0xff410u] == 0x80u);
    EXPECT_TRUE(machine.work_ram[0xff424u] == 0x00u);
    EXPECT_TRUE(machine.work_ram[0xff425u] == 0xf0u);
    EXPECT_TRUE(machine.work_ram[0xff426u] == 0x5fu);
    EXPECT_TRUE(machine.work_ram[0xff427u] == 0x00u);

    /* Warm soft-reset entry must preserve registers/control state that the
     * ROM's 0x000000b0 stage never writes. */
    vf2_i960_cpu_reset(&cpu, 0u, 0x005ff410u, 0xb0u);
    cpu.reinitialized = true;
    cpu.process_control = UINT32_C(0x001f0000);
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.interrupt_control = UINT32_C(0x0f0e0d0c);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;
    cpu.registers[0] = UINT32_C(0x005ff5c0);
    cpu.registers[1] = UINT32_C(0x005ff67c);
    cpu.registers[2] = UINT32_C(0x0005f138);
    cpu.registers[9] = UINT32_C(0x0ff7f7ff);
    cpu.registers[23] = UINT32_C(0x00510980);
    cpu.registers[29] = UINT32_C(0x00516480);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff640);
    EXPECT_TRUE(
        vf2_recovered_boot_stage1_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(cpu.ip == UINT32_C(0x000001b0));
    EXPECT_TRUE(cpu.registers[0] == UINT32_C(0x005ff5c0));
    EXPECT_TRUE(cpu.registers[1] == UINT32_C(0x005ff67c));
    EXPECT_TRUE(cpu.registers[2] == UINT32_C(0x0005f138));
    EXPECT_TRUE(cpu.registers[9] == UINT32_C(0x0ff7f7ff));
    EXPECT_TRUE(cpu.registers[23] == UINT32_C(0x00510980));
    EXPECT_TRUE(cpu.registers[29] == UINT32_C(0x00516480));
    EXPECT_TRUE(cpu.registers[VF2_I960_FP_REGISTER] == UINT32_C(0x005ff640));
    EXPECT_TRUE(cpu.arithmetic_control == UINT32_C(0x3f001002));
    EXPECT_TRUE(cpu.interrupt_control == UINT32_C(0x0f0e0d0c));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_recovered_boot_stage2(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_recovered_boot_stage2_report report;
    uint8_t *rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    uint32_t value = 0u;
    uint8_t byte = 0u;
    size_t index = 0u;

    EXPECT_TRUE(rom != NULL);
    if (rom == NULL) {
        return;
    }
    write_le32(rom + 0x3850u, 0x0f0e0d0cu);
    for (index = 0u; index < 16u; ++index) {
        const uint16_t color = (uint16_t)(0x1000u + index);
        rom[0x38bcu + index * 2u] = (uint8_t)color;
        rom[0x38bdu + index * 2u] = (uint8_t)(color >> 8u);
    }

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        free(rom);
        return;
    }
    EXPECT_TRUE(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0x005ff410u, 0x1b0u);
    cpu.reinitialized = true;
    cpu.executed_instructions = 100u;
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.registers[0] = UINT32_C(0x005ff5c0);
    cpu.registers[1] = UINT32_C(0x005ff67c);
    cpu.registers[23] = UINT32_C(0x00510980);
    cpu.registers[29] = UINT32_C(0x00516480);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff640);

    EXPECT_TRUE(vf2_recovered_boot_stage2_execute(&machine, &cpu, &report) == VF2_OK);
    EXPECT_TRUE(cpu.ip == 0x52cu);
    EXPECT_TRUE(cpu.process_control == 0x001f0000u);
    EXPECT_TRUE(cpu.arithmetic_control == 0x3f001000u);
    EXPECT_TRUE(cpu.interrupt_control == 0x0f0e0d0cu);
    EXPECT_TRUE(cpu.registers[0] == UINT32_C(0x005ff5c0));
    EXPECT_TRUE(cpu.registers[1] == UINT32_C(0x005ff67c));
    EXPECT_TRUE(cpu.registers[2] == 0x410u);
    EXPECT_TRUE(cpu.registers[5] == UINT32_C(0x3f001002));
    EXPECT_TRUE(cpu.registers[23] == UINT32_C(0x00510980));
    EXPECT_TRUE(cpu.registers[29] == UINT32_C(0x00516480));
    EXPECT_TRUE(cpu.registers[VF2_I960_FP_REGISTER] == UINT32_C(0x005ff640));
    EXPECT_TRUE(cpu.registers[VF2_I960_G14_REGISTER] == 0x220u);
    EXPECT_TRUE(cpu.executed_instructions == 182614u);
    EXPECT_TRUE(cpu.procedure_calls == UINT64_C(1));
    EXPECT_TRUE(cpu.procedure_returns == UINT64_C(1));
    EXPECT_TRUE(report.palette_entries_written == 12288u);
    EXPECT_TRUE(report.color_entries_written == 128u);
    EXPECT_TRUE(report.tile_halfwords_cleared == 24584u);
    EXPECT_TRUE(report.boot_palette_halfwords_written == 32u);

    EXPECT_TRUE(vf2_model2a_read_u32(&machine, VF2_INTERRUPT_CONTROL_BASE, &value) == VF2_OK);
    EXPECT_TRUE(value == 0u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, VF2_INTERRUPT_CONTROL_BASE + 4u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x21u);
    EXPECT_TRUE(vf2_model2a_read(&machine, VF2_IO_CONTROL_BASE + 0x34u, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == (uint8_t)'S');
    EXPECT_TRUE(vf2_model2a_read(&machine, VF2_IO_CONTROL_BASE + 0x36u, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == (uint8_t)'E');
    EXPECT_TRUE(vf2_model2a_read(&machine, VF2_IO_CONTROL_BASE + 0x38u, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == (uint8_t)'G');
    EXPECT_TRUE(vf2_model2a_read(&machine, VF2_IO_CONTROL_BASE + 0x3au, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == (uint8_t)'A');
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, VF2_COPRO_CONTROL_BASE, &value) == VF2_OK);
    EXPECT_TRUE((value & 0xffffu) == 4u);
    EXPECT_TRUE(machine.palette_ram[0] == 0x00u && machine.palette_ram[1] == 0x10u);
    EXPECT_TRUE(machine.palette_ram[0x2000u] == 0x00u &&
                machine.palette_ram[0x2001u] == 0x10u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}


static void test_recovered_task_registry(void)
{
    vf2_model2a machine;
    vf2_task_descriptor tasks[2];
    vf2_task_catalog catalog;
    vf2_recovered_task_registry_report report;
    uint32_t value = 0u;
    uint8_t byte = 0u;

    memset(tasks, 0, sizeof(tasks));
    memset(&catalog, 0, sizeof(catalog));
    tasks[0].descriptor_address = 0x100u;
    tasks[0].flags = 0x80000000u;
    tasks[0].instance = 2u;
    tasks[0].stack_size = 0x80u;
    tasks[0].entry_point = 0x1000u;
    tasks[0].state_address = VF2_WORK_RAM_BASE + 0x100u;
    tasks[0].scheduler_slot = 3u;
    tasks[1].descriptor_address = 0x140u;
    tasks[1].instance = 1u;
    tasks[1].stack_size = 0x100u;
    tasks[1].entry_point = 0x2000u;
    tasks[1].state_address = VF2_WORK_RAM_BASE + 0x104u;
    catalog.tasks = tasks;
    catalog.count = 2u;
    catalog.capacity = 2u;
    catalog.table_start = 0x100u;
    catalog.table_end = 0x180u;

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    memset(machine.work_ram, 0xa5, machine.work_ram_size);
    EXPECT_TRUE(
        vf2_recovered_task_registry_initialize(&machine, &catalog, &report) == VF2_OK
    );
    EXPECT_TRUE(report.task_count == 2u);
    EXPECT_TRUE(report.registry_start == 0x00510000u);
    EXPECT_TRUE(report.registry_end == 0x00510180u);
    EXPECT_TRUE(report.scratch_start == 0x0050c000u);
    EXPECT_TRUE(report.scratch_end == 0x0050c040u);
    EXPECT_TRUE(report.state_pointers_written == 2u);
    EXPECT_TRUE(report.scratch_bytes_cleared == 64u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x00510000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x80000000u);
    EXPECT_TRUE(vf2_model2a_read(&machine, 0x00510004u, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == 2u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x00510008u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x80u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x0051000cu, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x1000u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x00510038u, &value) == VF2_OK);
    EXPECT_TRUE(value == 81u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, VF2_WORK_RAM_BASE + 0x100u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x00510000u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, VF2_WORK_RAM_BASE + 0x104u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x00510080u);
    EXPECT_TRUE(machine.work_ram[0x0c000u] == 0u);
    EXPECT_TRUE(machine.work_ram[0x0c03fu] == 0u);
    EXPECT_TRUE(machine.work_ram[0x0c040u] == 0xa5u);

    vf2_model2a_shutdown(&machine);
}

static void test_recovered_scheduler_plan(void)
{
    vf2_model2a machine;
    vf2_task_descriptor tasks[3];
    vf2_task_catalog catalog;
    vf2_recovered_scheduler_report report;

    memset(tasks, 0, sizeof(tasks));
    memset(&catalog, 0, sizeof(catalog));
    tasks[0].flags = UINT32_C(0x80000000);
    tasks[0].stack_size = 0x80u;
    tasks[0].entry_point = 0x1000u;
    tasks[0].state_address = VF2_WORK_RAM_BASE + 0x100u;
    tasks[1].stack_size = 0x100u;
    tasks[1].entry_point = 0x2000u;
    tasks[1].state_address = VF2_WORK_RAM_BASE + 0x104u;
    tasks[2].flags = UINT32_C(0x80000000);
    tasks[2].stack_size = 0x60u;
    tasks[2].entry_point = 0x3000u;
    tasks[2].state_address = VF2_WORK_RAM_BASE + 0x108u;
    catalog.tasks = tasks;
    catalog.count = 3u;
    catalog.capacity = 3u;

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    EXPECT_TRUE(vf2_recovered_task_registry_initialize(&machine, &catalog, NULL) == VF2_OK);
    EXPECT_TRUE(vf2_recovered_scheduler_plan(&machine, &catalog, &report) == VF2_OK);
    EXPECT_TRUE(report.descriptor_count == 3u);
    EXPECT_TRUE(report.runnable_count == 2u);
    EXPECT_TRUE(report.runnable_task_indices[0] == 0u);
    EXPECT_TRUE(report.runnable_task_indices[1] == 2u);
    EXPECT_TRUE(report.runnable_registry_addresses[0] == 0x00510000u);
    EXPECT_TRUE(report.runnable_registry_addresses[1] == 0x00510180u);
    EXPECT_TRUE(report.runnable_entry_points[0] == 0x1000u);
    EXPECT_TRUE(report.runnable_entry_points[1] == 0x3000u);
    vf2_model2a_shutdown(&machine);
}


static void test_recovered_task_entries(void)
{
    vf2_model2a machine;
    vf2_recovered_task_report report;
    uint32_t value = 0u;
    size_t index = 0u;
    const uint32_t user_registry = UINT32_C(0x00515880);
    const uint32_t sound_registry = UINT32_C(0x00515d80);

    memset(&machine, 0, sizeof(machine));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    memset(machine.work_ram, 0xa5, machine.work_ram_size);
    EXPECT_TRUE(
        vf2_recovered_task_user_execute(&machine, user_registry, &report) == VF2_OK
    );
    EXPECT_TRUE(report.entry_point == UINT32_C(0x00029748));
    EXPECT_TRUE(report.registry_address == user_registry);
    EXPECT_TRUE(report.bytes_written == 0u);
    EXPECT_TRUE(machine.work_ram[user_registry - VF2_WORK_RAM_BASE] == 0xa5u);

    EXPECT_TRUE(
        vf2_recovered_task_sound_initialize(&machine, sound_registry, &report) == VF2_OK
    );
    EXPECT_TRUE(report.entry_point == UINT32_C(0x000439fc));
    EXPECT_TRUE(report.registry_address == sound_registry);
    EXPECT_TRUE(report.continuation == UINT32_C(0x00043abc));
    EXPECT_TRUE(report.bytes_written == 60u);
    EXPECT_TRUE(report.global_bytes_written == 8u);
    EXPECT_TRUE(
        vf2_model2a_read_u32(&machine, sound_registry + 0x0cu, &value) == VF2_OK
    );
    EXPECT_TRUE(value == UINT32_C(0x00043abc));
    EXPECT_TRUE(
        vf2_model2a_read_u32(&machine, sound_registry + 0x78u, &value) == VF2_OK
    );
    EXPECT_TRUE(value == sound_registry + 0x40u);
    EXPECT_TRUE(
        vf2_model2a_read_u32(&machine, sound_registry + 0x7cu, &value) == VF2_OK
    );
    EXPECT_TRUE(value == sound_registry + 0x40u);
    for (index = 0u; index < 0x30u; ++index) {
        EXPECT_TRUE(machine.work_ram[sound_registry - VF2_WORK_RAM_BASE + 0x40u + index] == 0u);
    }
    for (index = 0u; index < 8u; ++index) {
        EXPECT_TRUE(machine.work_ram[0x4070u + index] == 0u);
    }

    memset(machine.work_ram, 0, machine.work_ram_size);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, 0x00500804u, 0x00502000u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, 0x00500808u, 0x00503000u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, 0x00502000u, 0u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, 0x00503000u, 0u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, 0x00508000u, 1u << 5u) == VF2_OK);
    EXPECT_TRUE(
        vf2_recovered_task_game_info_first_dispatch(
            &machine, 0x00515200u, &report
        ) == VF2_OK
    );
    EXPECT_TRUE(report.entry_point == 0x0001645cu);
    EXPECT_TRUE(report.bytes_written == 0u);

    EXPECT_TRUE(vf2_model2a_write(&machine, 0x00515f04u, "\0", 1u) == VF2_OK);
    EXPECT_TRUE(
        vf2_recovered_task_osage_first_dispatch(
            &machine, 0x00515f00u, &report
        ) == VF2_OK
    );
    EXPECT_TRUE(report.entry_point == 0x000640f4u);
    EXPECT_TRUE(report.bytes_written == 26u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x00515f40u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x00502000u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x00515f48u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x0091e800u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, 0x00515f4cu, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x00007a00u);

    vf2_model2a_shutdown(&machine);
}

static void test_hybrid_scheduler_transition(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_scheduler_transition_report report;
    uint8_t *main_rom = NULL;
    uint8_t name_buffer[12];
    uint8_t tile_name[18];
    uint32_t value = 0u;
    const uint32_t current_registry = UINT32_C(0x00515200);
    const uint32_t next_registry = UINT32_C(0x00515400);
    const uint32_t input_state = UINT32_C(0x00502000);
    const size_t name_offset =
        0x00011dd8u + 17u * 0x40u;
    static const uint8_t expected_name[12] = {
        'f', 'a', '_', 'c', 'a', 'm', 'e', 'r', 'a', 0u, 0u, 0u
    };
    static const uint8_t expected_tile[18] = {
        'c', 0x80u, 'a', 0x80u, 'm', 0x80u, 'e', 0x80u,
        'r', 0x80u, 'a', 0x80u, ' ', 0x80u, ' ', 0x80u,
        0xaau, 0xbbu
    };
    static const uint8_t trailing_tile_sentinel[2] = {0xaau, 0xbbu};

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    memset(name_buffer, 0, sizeof(name_buffer));
    memset(tile_name, 0, sizeof(tile_name));

    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    main_rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    EXPECT_TRUE(main_rom != NULL);
    if (main_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    main_rom[0x00011d94u] = 29u;
    memcpy(main_rom + name_offset, expected_name, sizeof(expected_name));
    EXPECT_TRUE(
        vf2_model2a_attach_main_rom(&machine, main_rom, VF2_MAIN_ROM_SIZE) ==
        VF2_OK
    );

    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00508000), UINT32_C(0x00008800)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00500814), input_state
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, VF2_TIMER_BASE + UINT32_C(4), UINT32_C(0x000fffff)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, VF2_TIMER_BASE + UINT32_C(8), UINT32_C(0x000fffff)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, current_registry + UINT32_C(0x38), 0u
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write(
            &machine, UINT32_C(0x0100045c) + UINT32_C(16),
            trailing_tile_sentinel, sizeof(trailing_tile_sentinel)
        ) == VF2_OK
    );

    cpu.ip = UINT32_C(0x00010dcc);
    cpu.registers[1] = UINT32_C(0x00000180);
    cpu.registers[29] = current_registry;
    cpu.registers[31] = UINT32_C(0x00000100);
    cpu.local_frame_depth = 1u;
    cpu.maximum_local_frame_depth = 1u;

    EXPECT_TRUE(
        vf2_hybrid_first_dispatch_scheduler_advance(
            &machine,
            &cpu,
            13u,
            17u,
            current_registry,
            next_registry,
            UINT32_C(0x0001d320),
            &report
        ) == VF2_OK
    );
    EXPECT_TRUE(report.descriptors_scanned == 4u);
    EXPECT_TRUE(report.recovered_instruction_count == UINT64_C(429));
    EXPECT_TRUE(report.recovered_procedure_calls == UINT64_C(9));
    EXPECT_TRUE(report.recovered_procedure_returns == UINT64_C(8));
    EXPECT_TRUE(cpu.ip == UINT32_C(0x0001d320));
    EXPECT_TRUE(cpu.registers[29] == next_registry);
    EXPECT_TRUE(cpu.local_frame_depth == 2u);
    EXPECT_TRUE(cpu.local_frames[1].registers[15] == UINT32_C(0x000fffff));
    EXPECT_TRUE(cpu.executed_instructions == UINT64_C(429));
    EXPECT_TRUE(cpu.procedure_calls == UINT64_C(9));
    EXPECT_TRUE(cpu.procedure_returns == UINT64_C(8));
    EXPECT_TRUE(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00500038), &value
        ) == VF2_OK
    );
    EXPECT_TRUE(value == 17u);
    EXPECT_TRUE(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0050c000) + 13u * UINT32_C(0x20) + 8u,
            &value
        ) == VF2_OK
    );
    EXPECT_TRUE(value == 1u);
    EXPECT_TRUE(
        vf2_model2a_read(
            &machine, UINT32_C(0x0050e000),
            name_buffer, sizeof(name_buffer)
        ) == VF2_OK
    );
    EXPECT_TRUE(memcmp(name_buffer, expected_name, sizeof(expected_name)) == 0);
    EXPECT_TRUE(
        vf2_model2a_read(
            &machine, UINT32_C(0x0100045c),
            tile_name, sizeof(tile_name)
        ) == VF2_OK
    );
    EXPECT_TRUE(memcmp(tile_name, expected_tile, sizeof(expected_tile)) == 0);

    free(main_rom);
    vf2_model2a_shutdown(&machine);
}


static uint16_t read_le16_from_machine(
    const vf2_model2a *machine,
    uint32_t address
)
{
    uint8_t bytes[2] = {0u, 0u};
    EXPECT_TRUE(vf2_model2a_read(machine, address, bytes, sizeof(bytes)) == VF2_OK);
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static void test_texture_bridge_blocks(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;
    uint8_t byte = 0u;
    uint32_t value = 0u;
    uint8_t *main_rom = NULL;
    static const uint32_t pixels[4] = {
        UINT32_C(0x00112233), UINT32_C(0x00445566),
        UINT32_C(0x00778899), UINT32_C(0x00aabbcc)
    };
    size_t index = 0u;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    main_rom = (uint8_t *)calloc(VF2_MAIN_ROM_SIZE, sizeof(uint8_t));
    EXPECT_TRUE(main_rom != NULL);
    if (machine.work_ram == NULL || machine.texture_ram0 == NULL ||
        main_rom == NULL) {
        free(main_rom);
        vf2_model2a_shutdown(&machine);
        return;
    }
    EXPECT_TRUE(
        vf2_model2a_attach_main_rom(
            &machine, main_rom, VF2_MAIN_ROM_SIZE
        ) == VF2_OK
    );

    cpu.ip = UINT32_C(0x0004c868);
    cpu.registers[8] = UINT32_C(0xab);
    cpu.registers[10] = UINT32_C(0x00500100);
    cpu.registers[17] = UINT32_C(3);
    cpu.executed_instructions = UINT64_C(10);
    EXPECT_TRUE(
        vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_BYTE_RUN);
    EXPECT_TRUE(report.iterations == UINT64_C(3));
    EXPECT_TRUE(report.recovered_instruction_count == UINT64_C(12));
    EXPECT_TRUE(cpu.ip == UINT32_C(0x0004c878));
    EXPECT_TRUE(cpu.registers[10] == UINT32_C(0x005000fa));
    EXPECT_TRUE(cpu.registers[17] == 0u);
    EXPECT_TRUE(cpu.executed_instructions == UINT64_C(22));
    EXPECT_TRUE(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 3u; ++index) {
        EXPECT_TRUE(
            vf2_model2a_read(
                &machine, UINT32_C(0x00500100) - (uint32_t)index * 2u,
                &byte, sizeof(byte)
            ) == VF2_OK
        );
        EXPECT_TRUE(byte == UINT8_C(0xab));
    }

    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    cpu.ip = UINT32_C(0x0004cce8);
    cpu.registers[10] = VF2_TEXTURE_RAM0_BASE;
    cpu.registers[18] = UINT32_C(0x1234);
    cpu.registers[19] = UINT32_C(2);
    EXPECT_TRUE(
        vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_WORD_RUN);
    EXPECT_TRUE(report.iterations == UINT64_C(2));
    EXPECT_TRUE(cpu.ip == UINT32_C(0x0004ccf8));
    EXPECT_TRUE(cpu.registers[10] == VF2_TEXTURE_RAM0_BASE + UINT32_C(8));
    EXPECT_TRUE(read_le16_from_machine(&machine, VF2_TEXTURE_RAM0_BASE) == UINT16_C(0x1234));
    EXPECT_TRUE(read_le16_from_machine(&machine, VF2_TEXTURE_RAM0_BASE + UINT32_C(4)) == UINT16_C(0x1234));

    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    cpu.registers[1] = UINT32_C(0x00501000);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x00500000);
    cpu.registers[25] = UINT32_C(16);
    cpu.registers[26] = UINT32_C(0x00502000);
    cpu.registers[28] = UINT32_C(2);
    cpu.registers[29] = UINT32_C(0xdeadbeef);
    cpu.registers[30] = UINT32_C(8);
    EXPECT_TRUE(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x0004c928), UINT32_C(0x00001234)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND);
    EXPECT_TRUE(report.recovered_instruction_count == UINT64_C(3));
    EXPECT_TRUE(report.recovered_procedure_returns == UINT64_C(1));
    EXPECT_TRUE(cpu.ip == UINT32_C(0x00001234));
    EXPECT_TRUE(cpu.local_frame_depth == 0u);
    EXPECT_TRUE(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00502010), &value
        ) == VF2_OK
    );
    EXPECT_TRUE(value == UINT32_C(0xdeadbeef));

    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    for (index = 0u; index < 4u; ++index) {
        EXPECT_TRUE(
            vf2_model2a_write_u32(
                &machine,
                UINT32_C(0x0055c2ec) - (uint32_t)index * 4u,
                pixels[index]
            ) == VF2_OK
        );
    }
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c2dc), UINT32_C(0)
        ) == VF2_OK
    );
    cpu.registers[1] = UINT32_C(0x00501000);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x00500000);
    cpu.registers[26] = UINT32_C(8);
    cpu.registers[27] = VF2_TEXTURE_RAM0_BASE + UINT32_C(0x100);
    cpu.registers[28] = UINT32_C(2);
    cpu.registers[29] = UINT32_C(2);
    EXPECT_TRUE(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x0004ce88), UINT32_C(0x00005678)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT);
    EXPECT_TRUE(report.rows == UINT64_C(2));
    EXPECT_TRUE(report.iterations == UINT64_C(4));
    EXPECT_TRUE(report.changed_values == UINT64_C(4));
    EXPECT_TRUE(report.recovered_instruction_count == UINT64_C(109));
    EXPECT_TRUE(cpu.ip == UINT32_C(0x00005678));
    EXPECT_TRUE(cpu.local_frame_depth == 0u);
    EXPECT_TRUE(cpu.registers[24] == pixels[3]);
    EXPECT_TRUE(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    EXPECT_TRUE(
        read_le16_from_machine(
            &machine, VF2_TEXTURE_RAM0_BASE + UINT32_C(0x100)
        ) == (uint16_t)(pixels[0] | (pixels[0] >> 12u))
    );
    EXPECT_TRUE(
        read_le16_from_machine(
            &machine, VF2_TEXTURE_RAM0_BASE + UINT32_C(0x104)
        ) == (uint16_t)(pixels[1] | (pixels[1] >> 12u))
    );
    EXPECT_TRUE(
        read_le16_from_machine(
            &machine, VF2_TEXTURE_RAM0_BASE + UINT32_C(0x108)
        ) == (uint16_t)(pixels[2] | (pixels[2] >> 12u))
    );
    EXPECT_TRUE(
        read_le16_from_machine(
            &machine, VF2_TEXTURE_RAM0_BASE + UINT32_C(0x10c)
        ) == (uint16_t)(pixels[3] | (pixels[3] >> 12u))
    );

    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00501004), UINT32_C(0x00000100)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00501008), UINT32_C(0)
        ) == VF2_OK
    );
    byte = 0u;
    EXPECT_TRUE(
        vf2_model2a_write(
            &machine, UINT32_C(0x0050100c), &byte, sizeof(byte)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_model2a_write_u32(
            &machine, VF2_GEOMETRY_BASE + UINT32_C(0x2008),
            UINT32_C(0x00000300)
        ) == VF2_OK
    );
    main_rom[UINT32_C(0x7a04)] = UINT8_C(0x78);
    main_rom[UINT32_C(0x7a05)] = UINT8_C(0x56);
    main_rom[UINT32_C(0x7a06)] = UINT8_C(0x34);
    main_rom[UINT32_C(0x7a07)] = UINT8_C(0x12);
    cpu.registers[1] = UINT32_C(0x00501000);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x00500000);
    cpu.registers[26] = VF2_GEOMETRY_BASE;
    EXPECT_TRUE(
        vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x00002edc), UINT32_C(0x00008888)
        ) == VF2_OK
    );
    EXPECT_TRUE(
        vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK
    );
    EXPECT_TRUE(report.kind == VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT);
    EXPECT_TRUE(cpu.ip == UINT32_C(0x00008888));
    EXPECT_TRUE(
        vf2_model2a_read_u32(
            &machine, VF2_GEOMETRY_BASE + UINT32_C(0x3008), &value
        ) == VF2_OK
    );
    EXPECT_TRUE(value == UINT32_C(0x00000100));
    EXPECT_TRUE(
        vf2_model2a_read_u32(
            &machine, VF2_GEOMETRY_BASE + UINT32_C(0x1008), &value
        ) == VF2_OK
    );
    EXPECT_TRUE(value == UINT32_C(0x12345678));
    EXPECT_TRUE(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00501008), &value
        ) == VF2_OK
    );
    EXPECT_TRUE(value == UINT32_C(0x00000200));

    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    {
        const uint8_t source_bytes[2] = {UINT8_C(0xfe), UINT8_C(0xff)};
        const uint8_t command_class = UINT8_C(3);
        const uint32_t expected_command = UINT32_C(0x81fffe00);
        EXPECT_TRUE(
            vf2_model2a_write(
                &machine, UINT32_C(0x005010de), source_bytes,
                sizeof(source_bytes)
            ) == VF2_OK
        );
        EXPECT_TRUE(
            vf2_model2a_write(
                &machine, UINT32_C(0x005010dc), &command_class,
                sizeof(command_class)
            ) == VF2_OK
        );
        cpu.registers[1] = UINT32_C(0x00501000);
        cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x00500000);
        cpu.registers[26] = VF2_GEOMETRY_BASE;
        cpu.registers[28] = UINT32_C(0x400);
        EXPECT_TRUE(
            vf2_i960_cpu_enter_procedure(
                &cpu, UINT32_C(0x00002f5c), UINT32_C(0x00009999)
            ) == VF2_OK
        );
        EXPECT_TRUE(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        EXPECT_TRUE(
            report.kind == VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP
        );
        EXPECT_TRUE(cpu.ip == UINT32_C(0x00009999));
        EXPECT_TRUE(
            vf2_model2a_read_u32(
                &machine, UINT32_C(0x005010e0), &value
            ) == VF2_OK
        );
        EXPECT_TRUE(value == expected_command);
        EXPECT_TRUE(
            vf2_model2a_read_u32(
                &machine, VF2_GEOMETRY_BASE + UINT32_C(0x400), &value
            ) == VF2_OK
        );
        EXPECT_TRUE(value == expected_command);
    }

    free(main_rom);
    vf2_model2a_shutdown(&machine);
}

static void test_recovered_camera_bit7_mode2(void)
{
    vf2_model2a machine;
    vf2_recovered_camera_update_report report;
    uint8_t *main_rom = NULL;
    uint32_t value = 0u;
    uint8_t byte = 0u;
    const uint32_t camera = UINT32_C(0x00515400);
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    static const uint32_t tracking_expected[10] = {
        UINT32_C(0x00000000), UINT32_C(0x00000000),
        UINT32_C(0x3f666666), UINT32_C(0x00000000),
        UINT32_C(0x00000000), UINT32_C(0x00000000),
        UINT32_C(0x00000000), UINT32_C(0x00000000),
        UINT32_C(0x3f666666), UINT32_C(0x00000000)
    };
    size_t index = 0u;

    memset(&machine, 0, sizeof(machine));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    main_rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    EXPECT_TRUE(main_rom != NULL);
    if (main_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    main_rom[0x0006e2ecu] = 0xc0u;
    main_rom[0x0006e2edu] = 0xf1u;
    main_rom[0x0006e2eeu] = 0x01u;
    main_rom[0x0006e2efu] = 0x00u;
    EXPECT_TRUE(
        vf2_model2a_attach_main_rom(&machine, main_rom, VF2_MAIN_ROM_SIZE) == VF2_OK
    );

#define BIT7_W32(address_, value_)                                            \
    EXPECT_TRUE(vf2_model2a_write_u32(                                        \
        &machine, UINT32_C(address_), UINT32_C(value_)                         \
    ) == VF2_OK)
#define BIT7_CW32(offset_, value_)                                            \
    EXPECT_TRUE(vf2_model2a_write_u32(                                        \
        &machine, camera + UINT32_C(offset_), UINT32_C(value_)                 \
    ) == VF2_OK)

    BIT7_W32(0x00500804, 0x00510980);
    BIT7_W32(0x00500808, 0x00512980);
    BIT7_W32(0x00508000, 0x00008a00);
    BIT7_W32(0x00501084, 0x44160000);
    BIT7_W32(0x00501088, 0x44160000);
    BIT7_W32(0x0050a150, 0x3f800000);
    BIT7_W32(0x0050a00c, 0x41000000);
    BIT7_W32(0x0050a148, 0xbe99999a);
    BIT7_W32(0x0050a0e0, 0x11111111);
    BIT7_W32(0x00510980, 0x04000080);
    BIT7_W32(0x00512980, 0x04000080);

    BIT7_CW32(0x000, 0x80000100);
    BIT7_CW32(0x018, 0x00000000);
    BIT7_CW32(0x01c, 0x3f4f5c29);
    BIT7_CW32(0x020, 0xc0a0a3d7);
    BIT7_CW32(0x044, 0x3f400000);
    BIT7_CW32(0x048, 0x3f733333);
    BIT7_CW32(0x04c, 0x3f800000);
    BIT7_CW32(0x050, 0x3ecccccd);
    BIT7_CW32(0x054, 0xbf4ccccd);
    BIT7_CW32(0x058, 0x3faccccd);
    BIT7_CW32(0x05c, 0x435f3333);
    BIT7_CW32(0x060, 0x432ccccd);
    BIT7_CW32(0x064, 0x3f800000);
    BIT7_CW32(0x1b4, 0x3f800000);
    BIT7_CW32(0x1b8, 0x40333333);
    BIT7_CW32(0x20c, 0x3fb33333);
    EXPECT_TRUE(vf2_model2a_write(&machine, camera + UINT32_C(0x40), "\2", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, camera + UINT32_C(0x2d1), "\74", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1b0), "\0", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1b0), "\0", 1u) == VF2_OK);

    for (index = 0u; index < 10u; ++index) {
        EXPECT_TRUE(vf2_model2a_write_u32(
            &machine, camera + UINT32_C(0x1bc) + (uint32_t)index * UINT32_C(4),
            tracking_expected[index]
        ) == VF2_OK);
        EXPECT_TRUE(vf2_model2a_write_u32(
            &machine, camera + UINT32_C(0x1e4) + (uint32_t)index * UINT32_C(4),
            tracking_expected[index]
        ) == VF2_OK);
    }

    EXPECT_TRUE(
        vf2_recovered_task_camera_first_update(&machine, camera, &report) == VF2_OK
    );
    EXPECT_TRUE(report.mode_handler == UINT32_C(0x0001f1c0));
    EXPECT_TRUE(report.helpers_recovered == 7u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera + UINT32_C(0x1c), &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x7f800000));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera + UINT32_C(0x20), &value) == VF2_OK);
    EXPECT_TRUE(value == 0u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera + UINT32_C(0x190), &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x432ccccd));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera + UINT32_C(0x1bc), &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x80000000));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera + UINT32_C(0x1e4), &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x80000000));
    EXPECT_TRUE(vf2_model2a_read(&machine, camera + UINT32_C(0x2d0), &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == UINT8_C(1));

#undef BIT7_W32
#undef BIT7_CW32

    free(main_rom);
    vf2_model2a_shutdown(&machine);
}

static void test_recovered_camera_and_kill_osage(void)
{
    vf2_model2a machine;
    vf2_recovered_camera_init_report camera_report;
    vf2_recovered_camera_update_report camera_update_report;
    vf2_recovered_camera_gate_report camera_gate_report;
    vf2_recovered_camera_viewport_report viewport_report;
    vf2_recovered_kill_osage_report kill_report;
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    uint32_t value = 0u;
    uint16_t halfword = 0u;
    const uint32_t camera_registry = UINT32_C(0x00515400);
    const uint32_t osage0_registry = UINT32_C(0x00515f00);
    const uint32_t osage1_registry = UINT32_C(0x00516180);
    const uint32_t elapsed_ticks = UINT32_C(100);
    const uint32_t timer3 = UINT32_C(0x000fffff) -
                            (UINT32_C(18) + UINT32_C(25) * elapsed_ticks);

    memset(&machine, 0, sizeof(machine));
    memset(&camera_report, 0, sizeof(camera_report));
    memset(&camera_update_report, 0, sizeof(camera_update_report));
    memset(&camera_gate_report, 0, sizeof(camera_gate_report));
    memset(&viewport_report, 0, sizeof(viewport_report));
    memset(&kill_report, 0, sizeof(kill_report));
    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    main_rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    EXPECT_TRUE(main_rom != NULL);
    if (main_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    main_rom[0x0006e2e8u] = 0x48u;
    main_rom[0x0006e2e9u] = 0xf1u;
    main_rom[0x0006e2eau] = 0x01u;
    main_rom[0x0006e2ebu] = 0x00u;
    main_rom[0x0006eea0u] = 0x06u;
    main_rom[0x0006eea1u] = 0x00u;
    EXPECT_TRUE(
        vf2_model2a_attach_main_rom(&machine, main_rom, VF2_MAIN_ROM_SIZE) == VF2_OK
    );

    main_data = (uint8_t *)calloc(1u, 0x00110000u);
    EXPECT_TRUE(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    main_data[0x00100800u] = 2u;
    main_data[0x00100802u] = 1u;
    main_data[0x00100804u] = 2u;
    main_data[0x00100002u] = 0x1fu;
    main_data[0x00100003u] = 0x00u;
    main_data[0x00100004u] = 0x00u;
    main_data[0x00100005u] = 0x7cu;
    EXPECT_TRUE(
        vf2_model2a_take_main_data(&machine, main_data, 0x00110000u) == VF2_OK
    );
    main_data = NULL;

    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804),
                                      UINT32_C(0x00502000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00502000), 0u) == VF2_OK);
    EXPECT_TRUE(
        vf2_recovered_task_camera_initialize(
            &machine, camera_registry, &camera_report
        ) == VF2_OK
    );
    EXPECT_TRUE(camera_report.entry_point == UINT32_C(0x0001d320));
    EXPECT_TRUE(camera_report.continuation == UINT32_C(0x0001d458));
    EXPECT_TRUE(camera_report.palette_entries_written == 2u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x0cu, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x0001d458));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x1cu, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x3f4f5c29));
    EXPECT_TRUE(vf2_model2a_read(&machine, VF2_PALETTE_RAM_BASE + 0x2002u,
                                 &halfword, sizeof(halfword)) == VF2_OK);
    EXPECT_TRUE(halfword == UINT16_C(0x2117));

    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808),
                                      UINT32_C(0x00503000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00503000), 0u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, camera_registry + 0x40u, "\1", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                      UINT32_C(0x00008800)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a00c),
                                      UINT32_C(0x41000000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a148),
                                      UINT32_C(0xbe99999a)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0e0),
                                      UINT32_C(0x11111111)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0e4),
                                      UINT32_C(0x22222222)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x00502000) + 0x1b0u,
                                  "\0", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x00503000) + 0x1b0u,
                                  "\1", 1u) == VF2_OK);
    EXPECT_TRUE(
        vf2_recovered_task_camera_first_update(
            &machine, camera_registry, &camera_update_report
        ) == VF2_OK
    );
    EXPECT_TRUE(camera_update_report.start_address == UINT32_C(0x0001d458));
    EXPECT_TRUE(camera_update_report.stop_address == UINT32_C(0x0001d660));
    EXPECT_TRUE(camera_update_report.mode_handler == UINT32_C(0x0001f148));
    EXPECT_TRUE(camera_update_report.range_flags == 0u);
    EXPECT_TRUE(camera_update_report.input_flags == UINT32_C(6));
    EXPECT_TRUE(camera_update_report.fighter0_profile == UINT32_C(0x11111111));
    EXPECT_TRUE(camera_update_report.fighter1_profile == UINT32_C(0x22222222));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry, &value) == VF2_OK);
    EXPECT_TRUE((value & UINT32_C(1 << 8)) != 0u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x5cu, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x435f3333));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, UINT32_C(0x0050109c), &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x11111111));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, UINT32_C(0x005010a0), &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x22222222));
    EXPECT_TRUE(vf2_recovered_camera_classify_range(
        UINT32_C(0), UINT32_C(0x3f4f5c29), UINT32_C(0xc0a0a3d7),
        UINT32_C(0x41000000), UINT32_C(0xbe99999a)
    ) == 0u);

    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x0050009c), "\1", 1u) == VF2_OK);
    EXPECT_TRUE(
        vf2_recovered_task_camera_post_update_gate(
            &machine, camera_registry, &camera_gate_report
        ) == VF2_OK
    );
    EXPECT_TRUE(camera_gate_report.start_address == UINT32_C(0x0001d660));
    EXPECT_TRUE(camera_gate_report.stop_address == UINT32_C(0x0001e524));
    EXPECT_TRUE(camera_gate_report.input_flags == UINT32_C(6));
    EXPECT_TRUE(camera_gate_report.control_flags == UINT32_C(1));
    EXPECT_TRUE(camera_gate_report.fast_exit != 0);
    EXPECT_TRUE(camera_gate_report.task_bytes_written == 0u);

    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x0050009c), "\0", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, camera_registry, UINT32_C(0x00000106)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, camera_registry + 0xdeu, "\1", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x0050002b), "\10", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x00500031), "\20", 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write(&machine, camera_registry + 0x2d4u, "\3", 1u) == VF2_OK);
    memset(&camera_gate_report, 0, sizeof(camera_gate_report));
    EXPECT_TRUE(
        vf2_recovered_task_camera_post_update_gate(
            &machine, camera_registry, &camera_gate_report
        ) == VF2_OK
    );
    EXPECT_TRUE(camera_gate_report.stop_address == UINT32_C(0x0001d984));
    EXPECT_TRUE(camera_gate_report.fast_exit == 0);
    EXPECT_TRUE(camera_gate_report.initial_task_flags == UINT32_C(0x00000106));
    EXPECT_TRUE(camera_gate_report.final_task_flags == UINT32_C(0x00000100));
    EXPECT_TRUE(camera_gate_report.task_flag_writes == 3u);
    EXPECT_TRUE(camera_gate_report.task_bytes_written == 12u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x00000100));

    main_rom[0x0006eea0u] = 0x0eu;
    main_rom[0x0006eea1u] = 0x00u;
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a00c),
                                      UINT32_C(0x41000000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a014),
                                      UINT32_C(0xc2c80000)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, camera_registry, 0u) == VF2_OK);
    memset(&viewport_report, 0, sizeof(viewport_report));
    EXPECT_TRUE(
        vf2_recovered_task_camera_viewport_construct(
            &machine, camera_registry, &viewport_report
        ) == VF2_OK
    );
    EXPECT_TRUE(viewport_report.start_address == UINT32_C(0x0001d678));
    EXPECT_TRUE(viewport_report.stop_address == UINT32_C(0x0001d8e8));
    EXPECT_TRUE(viewport_report.first_fixed_table != 0);
    EXPECT_TRUE(viewport_report.second_fixed_table != 0);
    EXPECT_TRUE(viewport_report.first_entries_written == 8u);
    EXPECT_TRUE(viewport_report.second_entries_written == 10u);
    EXPECT_TRUE(viewport_report.helpers_recovered == 3u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x100u, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x0000068e));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x11cu, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x0000068e));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x150u, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x00000765));
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, camera_registry + 0x174u, &value) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x00000765));

    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x0050009c), "\1", 1u) == VF2_OK);
    memset(&camera_gate_report, 0, sizeof(camera_gate_report));
    EXPECT_TRUE(
        vf2_recovered_task_camera_post_update_gate(
            &machine, camera_registry, &camera_gate_report
        ) == VF2_OK
    );
    EXPECT_TRUE(camera_gate_report.viewport_executed != 0);
    EXPECT_TRUE(camera_gate_report.viewport_entries_written == 18u);
    EXPECT_TRUE(camera_gate_report.stop_address == UINT32_C(0x0001e524));

    {
        vf2_hybrid_block_report hybrid_report;
        vf2_i960_cpu hybrid_cpu;
        memset(&hybrid_report, 0, sizeof(hybrid_report));
        memset(&hybrid_cpu, 0, sizeof(hybrid_cpu));
        main_rom[0x0006eea0u] = 0x06u;
        hybrid_cpu.ip = UINT32_C(0x0001d660);
        hybrid_cpu.registers[15] = UINT32_C(0x80000100);
        hybrid_cpu.registers[29] = camera_registry;
        hybrid_cpu.executed_instructions = UINT64_C(100);
        EXPECT_TRUE(
            vf2_hybrid_camera_execute(
                &machine, &hybrid_cpu, camera_registry, &hybrid_report
            ) == VF2_OK
        );
        EXPECT_TRUE(hybrid_report.kind == VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE);
        EXPECT_TRUE(hybrid_report.exit_address == UINT32_C(0x0001e524));
        EXPECT_TRUE(hybrid_report.fast_exit != 0);
        EXPECT_TRUE(hybrid_report.cpu_poststate_applied != 0);
        EXPECT_TRUE(hybrid_report.recovered_instruction_count == UINT64_C(6));
        EXPECT_TRUE(hybrid_cpu.ip == UINT32_C(0x0001e524));
        EXPECT_TRUE(hybrid_cpu.registers[3] == UINT32_C(1));
        EXPECT_TRUE(hybrid_cpu.registers[15] == UINT32_C(6));
        EXPECT_TRUE(hybrid_cpu.executed_instructions == UINT64_C(106));

        EXPECT_TRUE(
            vf2_model2a_read_u32(
                &machine, camera_registry + UINT32_C(0x100), &value
            ) == VF2_OK
        );
        main_rom[0x0006eea0u] = 0x08u;
        hybrid_cpu.ip = UINT32_C(0x0001d660);
        EXPECT_TRUE(
            vf2_hybrid_camera_execute(
                &machine, &hybrid_cpu, camera_registry, NULL
            ) == VF2_ERROR_UNSUPPORTED
        );
        EXPECT_TRUE(hybrid_cpu.ip == UINT32_C(0x0001d660));
        {
            uint32_t after_unsupported = 0u;
            EXPECT_TRUE(
                vf2_model2a_read_u32(
                    &machine, camera_registry + UINT32_C(0x100),
                    &after_unsupported
                ) == VF2_OK
            );
            EXPECT_TRUE(after_unsupported == value);
        }
    }

    main_rom[0x0006eea0u] = 0x06u;

    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00500868),
                                      osage0_registry) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x0050086c),
                                      osage1_registry) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, UINT32_C(0x00500020), 1u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, VF2_TIMER_BASE + 0x0cu,
                                      timer3) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, osage0_registry,
                                      (UINT32_C(1) << 2u) | (UINT32_C(1) << 3u)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, osage0_registry + 0x0cu,
                                      UINT32_C(0x0006428c)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, osage0_registry + 0x128u,
                                      10u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, osage1_registry,
                                      (UINT32_C(1) << 2u)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, osage1_registry + 0x0cu,
                                      UINT32_C(0x0006428c)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(&machine, osage1_registry + 0x128u,
                                      UINT32_C(0x00004200)) == VF2_OK);
    EXPECT_TRUE(
        vf2_recovered_task_kill_osage_execute(
            &machine, UINT32_C(0x00515e80), &kill_report
        ) == VF2_OK
    );
    EXPECT_TRUE(kill_report.records_evaluated == 2u);
    EXPECT_TRUE(kill_report.records_marked_for_kill == 1u);
    EXPECT_TRUE(kill_report.elapsed_ticks == elapsed_ticks + 10u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, osage0_registry, &value) == VF2_OK);
    EXPECT_TRUE((value & (UINT32_C(1) << 3u)) == 0u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, osage1_registry, &value) == VF2_OK);
    EXPECT_TRUE((value & (UINT32_C(1) << 3u)) != 0u);
    EXPECT_TRUE(vf2_model2a_read_u32(&machine, UINT32_C(0x00500164), &value) == VF2_OK);
    EXPECT_TRUE(value == 1u);

    free(main_rom);
    free(main_data);
    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_crc32();
    test_sha1();
    test_load32_word();
    test_load32_byte();
    test_load16_word_swap();
    test_model2a_copro_callbacks();
    test_model2a_host_input();
    test_boot_vectors();
    test_recovered_boot_stage1();
    test_recovered_boot_stage2();
    test_recovered_task_registry();
    test_recovered_scheduler_plan();
    test_recovered_task_entries();
    test_hybrid_scheduler_transition();
    test_texture_bridge_blocks();
    test_recovered_camera_bit7_mode2();
    test_recovered_camera_and_kill_osage();
    EXPECT_TRUE(vf2_test_i960_decoder() == 0);
    EXPECT_TRUE(vf2_test_i960_executor() == 0);
    EXPECT_TRUE(vf2_test_i960_snapshot() == 0);
    EXPECT_TRUE(vf2_test_i960_interrupts() == 0);
    EXPECT_TRUE(vf2_test_i960_cfg() == 0);
    EXPECT_TRUE(vf2_test_i960_semantics() == 0);
    EXPECT_TRUE(vf2_test_task_catalog() == 0);

    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed.\n", failures);
        return EXIT_FAILURE;
    }

    printf("All vf2-decomp tests passed.\n");
    return EXIT_SUCCESS;
}
