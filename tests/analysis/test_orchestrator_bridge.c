#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/orchestrator_gates.h"
#include "vf2/analysis/orchestrator_scan.h"
#include "vf2/hybrid.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void enter_parent(vf2_i960_cpu *cpu, uint32_t target)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu, target, UINT32_C(0x00001004)
        ) == VF2_OK
    );
}

static void write_test_u16(
    vf2_model2a *machine, uint32_t address, uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)(value & UINT16_C(0x00ff)),
        (uint8_t)(value >> 8u)
    };
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static uint16_t read_test_u16(
    const vf2_model2a *machine, uint32_t address
)
{
    uint8_t bytes[2] = {0u, 0u};
    CHECK(vf2_model2a_read(machine, address, bytes, sizeof(bytes)) == VF2_OK);
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static void write_rom_u32(uint8_t *rom, uint32_t address, uint32_t value)
{
    rom[address] = (uint8_t)(value & UINT32_C(0xff));
    rom[address + UINT32_C(1)] =
        (uint8_t)((value >> 8u) & UINT32_C(0xff));
    rom[address + UINT32_C(2)] =
        (uint8_t)((value >> 16u) & UINT32_C(0xff));
    rom[address + UINT32_C(3)] =
        (uint8_t)((value >> 24u) & UINT32_C(0xff));
}

static void test_inactive_scan_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint8_t inactive[2] = {0u, 0u};
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < VF2_ORCHESTRATOR_RECORD_COUNT; ++index) {
        CHECK(
            vf2_model2a_write(
                &machine,
                VF2_ORCHESTRATOR_RECORD_START +
                    (uint32_t)index * VF2_ORCHESTRATOR_RECORD_STRIDE +
                    VF2_ORCHESTRATOR_RECORD_ACTIVE_OFFSET,
                inactive,
                sizeof(inactive)
            ) == VF2_OK
        );
    }
    enter_parent(&cpu, VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);
    CHECK(report.exit_address == VF2_ORCHESTRATOR_RECORD_SCAN_EXIT);
    CHECK(report.iterations == UINT64_C(10));
    CHECK(report.recovered_instruction_count == UINT64_C(43));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == VF2_ORCHESTRATOR_RECORD_SCAN_EXIT);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(43));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_child_gate_dispatch(
    uint32_t entry,
    uint32_t target,
    vf2_hybrid_bridge_kind expected_kind
)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_CHILD_STATE, 0u
        ) == VF2_OK
    );
    enter_parent(&cpu, entry);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == expected_kind);
    CHECK(report.entry_address == entry);
    CHECK(report.exit_address == target);
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == target);
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(3));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_loop_gate_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, VF2_ORCHESTRATOR_CHILD_STATE, 0u
        ) == VF2_OK
    );
    enter_parent(&cpu, VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY);
    CHECK(report.exit_address == VF2_ORCHESTRATOR_LOOP_GATE_EXIT);
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.recovered_instruction_count == UINT64_C(2));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == VF2_ORCHESTRATOR_LOOP_GATE_EXIT);
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(2));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));

    vf2_model2a_shutdown(&machine);
}

static void test_header_decode_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t stream[18] = {
        0xffu, 0xffu, 0x09u, 0x04u, 0x00u, 0x82u, 0x00u, 0x00u,
        0x00u, 0x10u, 0x00u, 0x80u, 0xb4u, 0x28u, 0xc4u, 0xa3u,
        0x01u, 0x31u
    };
    uint32_t value = 0u;
    uint8_t dimensions[4] = {0u, 0u, 0u, 0u};
    const uint32_t source = UINT32_C(0x00551000);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write(
            &machine, source, stream, sizeof(stream)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004c180));
    cpu.registers[19] = source;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE);
    CHECK(report.entry_address == UINT32_C(0x0004c180));
    CHECK(report.exit_address == UINT32_C(0x0004c3f0));
    CHECK(report.iterations == UINT64_C(8));
    CHECK(report.changed_values == UINT64_C(10));
    CHECK(report.bytes_written == 36u);
    CHECK(report.recovered_instruction_count == UINT64_C(120));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004c3f0));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(120));
    CHECK(cpu.registers[3] == UINT32_C(0x0055c344));
    CHECK(cpu.registers[4] == UINT32_C(0x0055c344));
    CHECK(cpu.registers[5] == UINT32_C(9));
    CHECK(cpu.registers[7] == UINT32_C(7));
    CHECK(cpu.registers[12] == UINT32_C(0x1ff));
    CHECK(cpu.registers[13] == UINT32_C(0x00028b48));
    CHECK(cpu.registers[14] == UINT32_C(20));
    CHECK(cpu.registers[15] == source + UINT32_C(16));
    CHECK(cpu.registers[16] == UINT32_C(0x00028b40));
    CHECK(cpu.registers[18] == UINT32_C(1));
    CHECK(cpu.registers[19] == UINT32_C(0x100));
    CHECK(cpu.registers[20] == UINT32_C(0x102));
    CHECK(cpu.registers[21] == UINT32_C(0x122));
    CHECK(cpu.registers[22] == UINT32_C(0x142));
    CHECK(cpu.registers[27] == UINT32_C(0xa3c4));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(4))
    );
    CHECK(
        vf2_model2a_read(
            &machine, UINT32_C(0x0055c320), dimensions, sizeof(dimensions)
        ) == VF2_OK
    );
    CHECK(dimensions[0] == 128u && dimensions[1] == 0u);
    CHECK(dimensions[2] == 128u && dimensions[3] == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c324), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x4000));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c328), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(7));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c32c), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(4));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c330), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(9));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c334), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x82));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c338), &value
        ) == VF2_OK
    );
    CHECK(value == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c33c), &value
        ) == VF2_OK
    );
    CHECK(value == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c340), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(1));

    vf2_model2a_shutdown(&machine);
}

static void test_header_decode_context_restore(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t value = 0u;
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    enter_parent(&cpu, UINT32_C(0x0004c180));
    cpu.registers[19] = UINT32_C(0x00551000);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550080), 1u) == VF2_OK);
    for (index = 0u; index < 14u; ++index) {
        CHECK(vf2_model2a_write_u32(
                  &machine, UINT32_C(0x00550084) + (uint32_t)index * 4u,
                  UINT32_C(0x11000000) + (uint32_t)index) == VF2_OK);
    }
    for (index = 0u; index < 13u; ++index) {
        CHECK(vf2_model2a_write_u32(
                  &machine, UINT32_C(0x00550084) + UINT32_C(56) +
                      (uint32_t)index * 4u,
                  UINT32_C(0x22000000) + (uint32_t)index) == VF2_OK);
    }
    CHECK(vf2_model2a_write_u32(
              &machine, UINT32_C(0x00550084) + UINT32_C(108),
              UINT32_C(0x0004c3f0)) == VF2_OK);

    CHECK(vf2_hybrid_post_frame_bridge_execute(
              &machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE);
    CHECK(report.entry_address == UINT32_C(0x0004c180));
    CHECK(report.exit_address == UINT32_C(0x0004c3f0));
    CHECK(report.recovered_instruction_count == UINT64_C(62));
    CHECK(cpu.ip == UINT32_C(0x0004c3f0));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x0004c3f0));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x11000000));
    CHECK(cpu.registers[3] == UINT32_C(0x22000000));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00550080), &value) == VF2_OK);
    CHECK(value == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_system_memory_diagnostic(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t low = 0u;
    uint32_t high = 0u;
    uint8_t enabled = 1u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500171), &enabled, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &mode, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c318), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c31c), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0006dcb8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC);
    CHECK(report.recovered_instruction_count == UINT64_C(75));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(report.bytes_written == 32u);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(75));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059c318), &low) == VF2_OK);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059c31c), &high) == VF2_OK);
    CHECK(low == UINT32_C(1));
    CHECK(high == 0u);

    mode = UINT8_C(16);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &mode, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c318), UINT32_C(0x11223344)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c31c), UINT32_C(0x55667788)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0006dcb8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    memset(&report, 0, sizeof(report));

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC);
    CHECK(report.recovered_instruction_count == UINT64_C(66));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(report.bytes_written == 16u);
    CHECK(cpu.executed_instructions == UINT64_C(66));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059c318), &low) == VF2_OK);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059c31c), &high) == VF2_OK);
    CHECK(low == UINT32_C(0x11223344));
    CHECK(high == UINT32_C(0x55667788));

    /* Mode 17 follows the adjacent return path but executes one additional
     * branch instruction compared with mode 16. */
    mode = UINT8_C(17);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &mode, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0006dcb8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.recovered_instruction_count == UINT64_C(67));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.executed_instructions == UINT64_C(67));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    vf2_model2a_shutdown(&machine);
}


static void test_video_input_sync(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t flags = 0u;
    uint8_t words[4] = {0x34u, 0x12u, 0x78u, 0x56u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(0x00008800)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500710), UINT32_C(8)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500700), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500714), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0100a008), words, sizeof(words)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000110f4));
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC);
    CHECK(report.recovered_instruction_count == UINT64_C(28));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.executed_instructions == UINT64_C(28));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00508000), &flags) == VF2_OK);
    CHECK(flags == UINT32_C(0x00008a00));

    vf2_model2a_shutdown(&machine);
}


static void test_frame_counter_and_phase(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t counter = 0u;
    uint32_t base = VF2_WORK_RAM_BASE + UINT32_C(0x1000);
    uint8_t shift = 9u;
    uint8_t phase = UINT8_C(0x2f);
    uint8_t next = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050d000), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050d006), &shift, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000112f8));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(21));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050d000), &counter) == VF2_OK);
    CHECK(counter == UINT32_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c), UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050d000), UINT32_C(9)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000112f8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(5));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.bytes_written == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050d000), &counter) == VF2_OK);
    CHECK(counter == UINT32_C(9));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c), base) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, base + UINT32_C(0x3001), &phase, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00011c78));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(10));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0059c001), &next, 1u) == VF2_OK);
    CHECK(next == UINT8_C(0x30));

    vf2_model2a_shutdown(&machine);
}


static void test_frame_shadow_and_buffer_gate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t live_addresses[9] = {
        UINT32_C(0x00500234), UINT32_C(0x00500235),
        UINT32_C(0x00500236), UINT32_C(0x00500237),
        UINT32_C(0x00500238), UINT32_C(0x00500239),
        UINT32_C(0x005000e0), UINT32_C(0x005000e1),
        UINT32_C(0x005000e2)
    };
    uint32_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < UINT32_C(9); ++index) {
        uint8_t value = (uint8_t)(index + UINT32_C(1));
        CHECK(vf2_model2a_write(&machine, live_addresses[index], &value, 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x00544600) + index, &value, 1u) == VF2_OK);
    }
    enter_parent(&cpu, UINT32_C(0x00000530));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY);
    CHECK(report.recovered_instruction_count == UINT64_C(28));
    CHECK(report.iterations == UINT64_C(9));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), UINT32_C(0x00510000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), UINT32_C(0x00512000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00510000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512000), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000110b0));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(9));
    CHECK(cpu.registers[23] == UINT32_C(0x00510000));
    CHECK(cpu.registers[24] == UINT32_C(0x00512000));

    vf2_model2a_shutdown(&machine);
}


static void test_frame_dispatch_tick(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    uint8_t selector = 1u;
    uint32_t value = 0u;
    const size_t rom_size = (size_t)UINT32_C(0x00080000);
    const size_t main_data_size = (size_t)UINT32_C(0x00b00000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    main_data = (uint8_t *)calloc(main_data_size, 1u);
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        free(rom);
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0000a6fc)] = UINT8_C(0x74);
    rom[UINT32_C(0x0000a6fd)] = UINT8_C(0xa9);
    rom[UINT32_C(0x0000a73c)] = UINT8_C(0x5c);
    rom[UINT32_C(0x0000a73d)] = UINT8_C(0x0b);
    rom[UINT32_C(0x0000a73e)] = UINT8_C(0x01);
    rom[UINT32_C(0x0000a73f)] = UINT8_C(0x00);
    write_rom_u32(
        rom, UINT32_C(0x0005ff04), UINT32_C(0x00078000)
    );
    write_rom_u32(
        rom, UINT32_C(0x00078000), UINT32_C(0x010014b0)
    );
    memcpy(
        rom + UINT32_C(0x00078004), "EXIT", sizeof("EXIT")
    );
    write_rom_u32(
        rom, UINT32_C(0x0005fefc), UINT32_C(0x00078020)
    );
    write_rom_u32(
        rom, UINT32_C(0x00078020), UINT32_C(0x01001330)
    );
    memcpy(
        rom + UINT32_C(0x00078024), "BACK", sizeof("BACK")
    );
    write_rom_u32(
        rom, UINT32_C(0x0005feac), UINT32_C(0x00078040)
    );
    write_rom_u32(
        rom, UINT32_C(0x00078040), UINT32_C(0x01001230)
    );
    memcpy(
        rom + UINT32_C(0x00078044), "START", sizeof("START")
    );
    write_rom_u32(
        rom, UINT32_C(0x0005ff00), UINT32_C(0x0005ef60)
    );
    write_rom_u32(
        rom, UINT32_C(0x0005ff1c), UINT32_C(0x00078060)
    );
    write_rom_u32(
        rom, UINT32_C(0x00078060), UINT32_C(0x01000bb2)
    );
    memcpy(
        rom + UINT32_C(0x00078064),
        "EXIT TEST MODE", sizeof("EXIT TEST MODE")
    );
    rom[UINT32_C(0x000027ac)] = UINT8_C(0);
    rom[UINT32_C(0x000027ad)] = UINT8_C(1);
    rom[UINT32_C(0x000027e2)] = UINT8_C(0x10);
    rom[UINT32_C(0x000027e6)] = UINT8_C(0x20);
    rom[UINT32_C(0x000028b8)] = UINT8_C(0);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(
        vf2_model2a_attach_main_data(
            &machine, main_data, main_data_size
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &selector, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500024), UINT32_C(640)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
    CHECK(report.recovered_instruction_count == UINT64_C(14));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.executed_instructions == UINT64_C(14));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050002c), &value) == VF2_OK);
    CHECK(value == UINT32_C(2));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(639));

    selector = UINT8_C(17);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &selector, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c), UINT32_C(0x00510000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), UINT32_C(0x00511000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), UINT32_C(0x00512000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), 0u) == VF2_OK);
    {
        uint8_t phase_index = UINT8_C(0x0b);
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t mirrored_selector = 0u;
        uint32_t saved_g4_address = 0u;
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000a4), &phase_index, 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000a6), &phase_state, 1u) == VF2_OK);
        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        cpu.registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0x12345678);
        saved_g4_address = cpu.registers[1] + UINT32_C(64);
        memset(&report, 0, sizeof(report));

        CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
        CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
        CHECK(report.recovered_instruction_count == UINT64_C(36));
        CHECK(report.recovered_procedure_calls == UINT64_C(2));
        CHECK(report.recovered_procedure_returns == UINT64_C(3));
        CHECK(report.bytes_written == 9u);
        CHECK(cpu.executed_instructions == UINT64_C(36));
        CHECK(cpu.procedure_calls == UINT64_C(3));
        CHECK(cpu.procedure_returns == UINT64_C(3));
        CHECK(cpu.maximum_local_frame_depth == 3u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x12345678));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == UINT32_C(0x00511000));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == UINT32_C(0x00512000));
        CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002b), &mirrored_selector, 1u) == VF2_OK);
        CHECK(mirrored_selector == UINT8_C(17));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050002c), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x00020000));
        CHECK(vf2_model2a_read_u32(&machine, saved_g4_address, &value) == VF2_OK);
        CHECK(value == UINT32_C(0x12345678));
    }

    {
        uint8_t phase_index = UINT8_C(0x0b);
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t phase_a5 = UINT8_C(0x7a);
        uint8_t phase_a7 = 0u;
        uint8_t resulting_phase_index = 0u;
        uint8_t resulting_phase_a5 = UINT8_C(0xff);
        uint8_t resulting_phase_a7 = 0u;
        const uint32_t phase_object = UINT32_C(0x00513000);

        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a4), &phase_index, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a5), &phase_a5, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a6), &phase_state, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a7), &phase_a7, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500704), UINT32_C(0x04000104)
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500864), phase_object
            ) == VF2_OK
        );
        write_test_u16(
            &machine, phase_object + UINT32_C(0x80), UINT16_C(0x1234)
        );
        write_test_u16(
            &machine, UINT32_C(0x01000000), UINT16_C(0xffff)
        );
        write_test_u16(
            &machine, UINT32_C(0x010017fe), UINT16_C(0xffff)
        );
        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        memset(&report, 0, sizeof(report));

        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
        CHECK(report.recovered_instruction_count == UINT64_C(12657));
        CHECK(report.recovered_procedure_calls == UINT64_C(5));
        CHECK(report.recovered_procedure_returns == UINT64_C(6));
        CHECK(report.changed_values == UINT64_C(3084));
        CHECK(report.bytes_written == 6166u);
        CHECK(cpu.executed_instructions == UINT64_C(12657));
        CHECK(cpu.procedure_calls == UINT64_C(6));
        CHECK(cpu.procedure_returns == UINT64_C(6));
        CHECK(cpu.maximum_local_frame_depth == 4u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x00078004));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == 0u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x0100013c));
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a4),
                &resulting_phase_index, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_index == UINT8_C(0x8b));
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a5),
                &resulting_phase_a5, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_a5 == 0u);
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a7),
                &resulting_phase_a7, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_a7 == UINT8_C(0xff));
        CHECK(
            read_test_u16(
                &machine, phase_object + UINT32_C(0x80)
            ) == UINT16_C(0)
        );
        CHECK(
            read_test_u16(
                &machine, UINT32_C(0x01000000)
            ) == UINT16_C(32)
        );
        CHECK(
            read_test_u16(
                &machine, UINT32_C(0x010017fe)
            ) == UINT16_C(32)
        );
        CHECK(
            read_test_u16(
                &machine, UINT32_C(0x0100013c)
            ) == UINT16_C(0x8045)
        );
        CHECK(
            read_test_u16(
                &machine, UINT32_C(0x0100013e)
            ) == UINT16_C(0x8058)
        );
        CHECK(
            read_test_u16(
                &machine, UINT32_C(0x01000140)
            ) == UINT16_C(0x8049)
        );
        CHECK(
            read_test_u16(
                &machine, UINT32_C(0x01000142)
            ) == UINT16_C(0x8054)
        );
    }

    {
        uint8_t phase_index = UINT8_C(0x8b);
        uint8_t phase_a5 = 0u;
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t system_flags = UINT8_C(1);
        uint8_t resulting_a5 = 0u;
        uint8_t zero_bytes[8] = {0};

        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a4), &phase_index, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a5), &phase_a5, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a6), &phase_state, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x00500171), &system_flags, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500700), 0u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500704), 0u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00510000 + 0x3320), 0u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x00510000 + 0x3324),
                zero_bytes, sizeof(zero_bytes)
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x00510000 + 0x3350),
                zero_bytes, 1u
            ) == VF2_OK
        );
        {
            const uint8_t first_meter[6] = {1u, 0u, 0u, 0u, 0u, 0u};
            const uint8_t second_meter[6] = {0u, 0u, 0u, 0u, 0u, 0u};
            CHECK(
                vf2_model2a_write(
                    &machine, UINT32_C(0x00510000 + 0x3380),
                    first_meter, sizeof(first_meter)
                ) == VF2_OK
            );
            CHECK(
                vf2_model2a_write(
                    &machine, UINT32_C(0x00510000 + 0x3388),
                    second_meter, sizeof(second_meter)
                ) == VF2_OK
            );
        }

        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        /* The ROM-backed preterminal checkpoint enters this path with g4=0.
         * Keep the synthetic fixture aligned with that observed state; strict
         * differential replay below is the preservation contract. */
        cpu.registers[VF2_I960_G0_REGISTER + 4u] = 0u;
        memset(&report, 0, sizeof(report));
        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
        CHECK(report.recovered_instruction_count == UINT64_C(13286));
        CHECK(report.recovered_procedure_calls == UINT64_C(27));
        CHECK(report.recovered_procedure_returns == UINT64_C(28));
        CHECK(cpu.executed_instructions == UINT64_C(13286));
        CHECK(cpu.procedure_calls == UINT64_C(28));
        CHECK(cpu.procedure_returns == UINT64_C(28));
        CHECK(cpu.maximum_local_frame_depth == 9u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x00078064));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == 0u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(15));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01000bb2));
        CHECK(
            vf2_model2a_read_u32(
                &machine, UINT32_C(0x00500024), &value
            ) == VF2_OK
        );
        CHECK(value == UINT32_C(320));
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a5), &resulting_a5, 1u
            ) == VF2_OK
        );
        CHECK(resulting_a5 == UINT8_C(0xff));
        CHECK(read_test_u16(&machine, UINT32_C(0x01000000)) == UINT16_C(32));
        CHECK(read_test_u16(&machine, UINT32_C(0x010017fe)) == UINT16_C(32));
        CHECK(read_test_u16(&machine, UINT32_C(0x01000bb2)) == UINT16_C(0x8045));
        CHECK(read_test_u16(&machine, UINT32_C(0x01000bb4)) == UINT16_C(0x8058));

        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        memset(&report, 0, sizeof(report));
        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.recovered_instruction_count == UINT64_C(626));
        CHECK(report.recovered_procedure_calls == UINT64_C(25));
        CHECK(report.recovered_procedure_returns == UINT64_C(26));
        CHECK(cpu.executed_instructions == UINT64_C(626));
        CHECK(cpu.procedure_calls == UINT64_C(26));
        CHECK(cpu.procedure_returns == UINT64_C(26));
        CHECK(
            vf2_model2a_read_u32(
                &machine, UINT32_C(0x00500024), &value
            ) == VF2_OK
        );
        CHECK(value == UINT32_C(319));

        /* Terminal countdown: 1 -> 0 does not return through the phase
         * wrappers. It performs the observed soft-reset preamble and branches
         * directly to the boot entry at 0x000000b0. */
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500024), UINT32_C(1)
            ) == VF2_OK
        );
        write_test_u16(
            &machine, UINT32_C(0x0100a00c), UINT16_C(0xffff)
        );
        write_test_u16(
            &machine, UINT32_C(0x0100a00e), UINT16_C(0x9234)
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x0050081c), UINT32_C(0x00513000)
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00513000), UINT32_C(0xffffffff)
            ) == VF2_OK
        );
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500708), UINT32_C(1)) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), UINT32_C(2)) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500700), UINT32_C(3)) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050070c), UINT32_C(4)) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00e80004), UINT32_C(0x12345678)) == VF2_OK);
        {
            const uint8_t one = UINT8_C(1);
            CHECK(
                vf2_model2a_write(
                    &machine, UINT32_C(0x0050009c), &one, sizeof(one)
                ) == VF2_OK
            );
        }
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059cfe0), 0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059cfe4), 0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059cfe8), 0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059cfec), 0u) == VF2_OK);

        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        cpu.registers[VF2_I960_G0_REGISTER + 4u] = 0u;
        memset(&report, 0, sizeof(report));
        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
        CHECK(report.recovered_instruction_count == UINT64_C(13194));
        CHECK(report.recovered_procedure_calls == UINT64_C(27));
        CHECK(report.recovered_procedure_returns == UINT64_C(25));
        CHECK(cpu.ip == UINT32_C(0x000000b0));
        CHECK(cpu.local_frame_depth == 3u);
        CHECK(cpu.executed_instructions == UINT64_C(13194));
        CHECK(cpu.procedure_calls == UINT64_C(28));
        CHECK(cpu.procedure_returns == UINT64_C(25));
        CHECK(cpu.registers[3] == UINT32_C(0x00e80004));
        CHECK(cpu.registers[4] == 0u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == 0u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01001800));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
        CHECK(value == 0u);
        CHECK(read_test_u16(&machine, UINT32_C(0x00500082)) == UINT16_C(0x8000));
        CHECK(read_test_u16(&machine, UINT32_C(0x0100a00c)) == UINT16_C(0x7fff));
        CHECK(read_test_u16(&machine, UINT32_C(0x0100a00e)) == UINT16_C(0x1234));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00513000), &value) == VF2_OK);
        CHECK(value == UINT32_C(0xfffffffc));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500708), &value) == VF2_OK); CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500704), &value) == VF2_OK); CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500700), &value) == VF2_OK); CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050070c), &value) == VF2_OK); CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00e80004), &value) == VF2_OK); CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfe0), &value) == VF2_OK); CHECK(value == UINT32_C(0x52455320));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfe4), &value) == VF2_OK); CHECK(value == UINT32_C(0x4e4c2053));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfe8), &value) == VF2_OK); CHECK(value == UINT32_C(0x4e204544));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfec), &value) == VF2_OK); CHECK(value == UINT32_C(0x20514555));
        CHECK(read_test_u16(&machine, UINT32_C(0x01000000)) == UINT16_C(32));
        CHECK(read_test_u16(&machine, UINT32_C(0x010017fe)) == UINT16_C(32));
    }

    {
        uint8_t phase_index = UINT8_C(0x0b);
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t resulting_phase_index = 0u;

        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a4), &phase_index, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a6), &phase_state, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500704), UINT32_C(0x00002000)
            ) == VF2_OK
        );
        write_test_u16(&machine, UINT32_C(0x010014ac), UINT16_C(0x801c));
        write_test_u16(&machine, UINT32_C(0x0100132c), UINT16_C(0x0020));
        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        cpu.registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0x12345678);
        memset(&report, 0, sizeof(report));

        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
        CHECK(report.recovered_instruction_count == UINT64_C(49));
        CHECK(report.recovered_procedure_calls == UINT64_C(2));
        CHECK(report.recovered_procedure_returns == UINT64_C(3));
        CHECK(report.changed_values == UINT64_C(7));
        CHECK(report.bytes_written == 14u);
        CHECK(cpu.executed_instructions == UINT64_C(49));
        CHECK(cpu.procedure_calls == UINT64_C(3));
        CHECK(cpu.procedure_returns == UINT64_C(3));
        CHECK(cpu.maximum_local_frame_depth == 3u);
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x12345678));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == UINT32_C(0x00511000));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == UINT32_C(0x00512000));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x0100132c));
        CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a4),
                &resulting_phase_index, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_index == UINT8_C(10));
        CHECK(read_test_u16(&machine, UINT32_C(0x010014ac)) == UINT16_C(0x8020));
        CHECK(read_test_u16(&machine, UINT32_C(0x0100132c)) == UINT16_C(0x801c));
    }

    {
        uint8_t phase_index = 0u;
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t resulting_phase_index = 0u;

        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a4), &phase_index, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a6), &phase_state, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500704), UINT32_C(0x00002000)
            ) == VF2_OK
        );
        write_test_u16(&machine, UINT32_C(0x0100122c), UINT16_C(0x801c));
        write_test_u16(&machine, UINT32_C(0x010014ac), UINT16_C(0x0020));
        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        memset(&report, 0, sizeof(report));

        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.recovered_instruction_count == UINT64_C(50));
        CHECK(cpu.executed_instructions == UINT64_C(50));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x010014ac));
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a4),
                &resulting_phase_index, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_index == UINT8_C(11));
        CHECK(read_test_u16(&machine, UINT32_C(0x0100122c)) == UINT16_C(0x8020));
        CHECK(read_test_u16(&machine, UINT32_C(0x010014ac)) == UINT16_C(0x801c));
    }

    {
        uint8_t phase_index = UINT8_C(11);
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t resulting_phase_index = UINT8_C(0xff);

        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a4), &phase_index, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a6), &phase_state, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500704), UINT32_C(0x00000008)
            ) == VF2_OK
        );
        write_test_u16(&machine, UINT32_C(0x010014ac), UINT16_C(0x801c));
        write_test_u16(&machine, UINT32_C(0x0100122c), UINT16_C(0x0020));
        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        memset(&report, 0, sizeof(report));

        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.recovered_instruction_count == UINT64_C(50));
        CHECK(report.changed_values == UINT64_C(7));
        CHECK(report.bytes_written == 14u);
        CHECK(cpu.executed_instructions == UINT64_C(50));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x0100122c));
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a4),
                &resulting_phase_index, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_index == UINT8_C(0));
        CHECK(read_test_u16(&machine, UINT32_C(0x010014ac)) == UINT16_C(0x8020));
        CHECK(read_test_u16(&machine, UINT32_C(0x0100122c)) == UINT16_C(0x801c));
    }

    {
        uint8_t phase_index = UINT8_C(10);
        uint8_t phase_state = UINT8_C(0xff);
        uint8_t resulting_phase_index = 0u;

        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a4), &phase_index, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x005000a6), &phase_state, 1u
            ) == VF2_OK
        );
        CHECK(
            vf2_model2a_write_u32(
                &machine, UINT32_C(0x00500704), UINT32_C(0x08003000)
            ) == VF2_OK
        );
        write_test_u16(&machine, UINT32_C(0x0100132c), UINT16_C(0x801c));
        write_test_u16(&machine, UINT32_C(0x010014ac), UINT16_C(0x0020));
        enter_parent(&cpu, UINT32_C(0x0000a6c0));
        memset(&report, 0, sizeof(report));

        CHECK(
            vf2_hybrid_post_frame_bridge_execute(
                &machine, &cpu, &report
            ) == VF2_OK
        );
        CHECK(report.recovered_instruction_count == UINT64_C(49));
        CHECK(cpu.executed_instructions == UINT64_C(49));
        CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x010014ac));
        CHECK(
            vf2_model2a_read(
                &machine, UINT32_C(0x005000a4),
                &resulting_phase_index, 1u
            ) == VF2_OK
        );
        CHECK(resulting_phase_index == UINT8_C(11));
        CHECK(read_test_u16(&machine, UINT32_C(0x0100132c)) == UINT16_C(0x8020));
        CHECK(read_test_u16(&machine, UINT32_C(0x010014ac)) == UINT16_C(0x801c));
    }

    /* Selector 3, phase-zero alternate profile: 0xae78 classifies the
     * profile as the zero-derived display path, expands empty descriptor
     * streams, draws from 0x02ab2f82 and advances to phase one. */
    write_rom_u32(
        rom, UINT32_C(0x0000a6f8) + UINT32_C(3 * 4),
        UINT32_C(0x0000acf8)
    );
    write_rom_u32(
        rom, UINT32_C(0x0000aac4), UINT32_C(0x0000ae78)
    );
    write_rom_u32(
        main_data, UINT32_C(0x00003320), UINT32_C(0)
    );
    main_data[UINT32_C(0x00003324)] = UINT8_C(25);
    main_data[UINT32_C(0x00003350)] = UINT8_C(1);
    main_data[UINT32_C(0x00ab2f82) + UINT32_C(0)] = UINT8_C(0);
    main_data[UINT32_C(0x00ab2f82) + UINT32_C(1)] = UINT8_C(0);
    write_rom_u32(
        main_data, UINT32_C(0x00ab2f82) + UINT32_C(4), UINT32_C(1)
    );
    write_rom_u32(
        main_data, UINT32_C(0x00ab2f82) + UINT32_C(8), UINT32_C(1)
    );
    main_data[UINT32_C(0x00ab2f82) + UINT32_C(12)] = UINT8_C(7);
    selector = UINT8_C(3);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &selector, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){0}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c), VF2_MAIN_DATA_BASE) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050009c), 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050008f), &(uint8_t){1}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500834), UINT32_C(0x00520000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), UINT32_C(0x00530000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), UINT32_C(0x00531000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500868), UINT32_C(0x00521100)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050086c), UINT32_C(0x00521104)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521100), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521104), 0u) == VF2_OK);
    {
        const uint32_t clear_slots[] = {
            UINT32_C(0x00500828), UINT32_C(0x00500844),
            UINT32_C(0x00500848), UINT32_C(0x0050081c),
            UINT32_C(0x00500858), UINT32_C(0x00500878),
            UINT32_C(0x0050087c), UINT32_C(0x00500880)
        };
        size_t index = 0u;

        for (index = 0u; index < sizeof(clear_slots) / sizeof(clear_slots[0]); ++index) {
            const uint32_t pointer = UINT32_C(0x00521000) + (uint32_t)index * UINT32_C(4);
            CHECK(vf2_model2a_write_u32(&machine, clear_slots[index], pointer) == VF2_OK);
            CHECK(vf2_model2a_write_u32(&machine, pointer,
                                         index == 3u ? UINT32_C(3) : UINT32_C(0x80000000)) == VF2_OK);
        }
    }
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002a), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(4));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(1));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(256));
    CHECK(read_test_u16(&machine, UINT32_C(0x01004000)) == UINT16_C(7));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050009c), &value) == VF2_OK);
    CHECK((value & UINT32_C(1)) == 0u);

    /* Selector 3 phase one: the ROM's mode-25/flag-clear gate takes the
     * countdown path and leaves the phase unchanged after 2 -> 1. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(4), UINT32_C(0x0000afe0)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){1}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500024), UINT32_C(2)) == VF2_OK);
    main_data[UINT32_C(0x00003324)] = UINT8_C(25);
    write_rom_u32(main_data, UINT32_C(0x00003320), UINT32_C(0));
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002a), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(3));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(1));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(1));

    /* The alternate phase-one branch calls 0x2584. With a zero measurement,
     * the observed sum comparison selects phase six. */
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){1}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500024), UINT32_C(10)) == VF2_OK);
    main_data[UINT32_C(0x00003324)] = UINT8_C(24);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500708), UINT32_C(3)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(6));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(10));

    /* Phase three (0xb394) publishes the ready bit, decrements its timer and
     * advances when the terminal gate is clear. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(3 * 4), UINT32_C(0x0000b394)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500024), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550000), UINT32_C(0)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(4));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));

    /* Phase four (0xb3f8) consumes the descriptor tables, clears the display
     * plane and installs the next task resources before advancing. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(4 * 4), UINT32_C(0x0000b3f8)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){4}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500814), UINT32_C(0x00522000)) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x01000000), UINT16_C(0xffff));
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(5));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));
    CHECK(read_test_u16(&machine, UINT32_C(0x01000000)) == UINT16_C(32));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00522040), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(15));

    /* Phase five (0xb4f0) closes the timer, updates both render descriptors
     * and releases the runtime-ready bit before entering phase six. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(5 * 4), UINT32_C(0x0000b4f0)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){5}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500024), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050009c), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00530000), UINT32_C(0xffffffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00531000), UINT32_C(0xffffffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(1) << 16u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(6));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050009c), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00530000), &value) == VF2_OK);
    CHECK(value == ((UINT32_C(0xffffffff) & ~((UINT32_C(1) << 23u) | (UINT32_C(1) << 22u))) |
                    (UINT32_C(1) << 26u)));

    /* Phase six (0xb588) resets the tile/text staging planes and advances to
     * the next worker. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(6 * 4), UINT32_C(0x0000b588)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){6}, 1u) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x01000000), UINT16_C(0xffff));
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(7));
    CHECK(read_test_u16(&machine, UINT32_C(0x01000000)) == UINT16_C(32));

    /* Phase seven (0xb66c) builds the two gameplay task descriptors and
     * publishes the selector-side handoff state. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(7 * 4), UINT32_C(0x0000b66c)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){7}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500168), VF2_MAIN_DATA_BASE) == VF2_OK);
    write_rom_u32(main_data, UINT32_C(4), UINT32_C(0x12345678));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500854), UINT32_C(0x00521300)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050085c), UINT32_C(0x00521304)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500860), UINT32_C(0x00521308)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521300), UINT32_C(0xffffffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521304), UINT32_C(0xffffffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521308), UINT32_C(0xffffffff)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050005b), &(uint8_t){0}, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(8));
    CHECK(read_test_u16(&machine, UINT32_C(0x00530026)) == UINT16_C(0xc000));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050a00c), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x12345678));

    /* Phase eight is the delayed inline-text worker: its timer expires but
     * the opaque 0x9444 callback does not store a new phase here. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(8 * 4), UINT32_C(0x0000b9b8)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){8}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550000), UINT32_C(1)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(8));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00520050), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));

    /* With the countdown already expired and the runtime not ready, phase
     * eight leaves both selector frames live and hands the inline payload at
     * 0xba08 to the shared 0x9444 text thunk. */
    rom[UINT32_C(0x0000ba08)] = UINT8_C(0x20);
    rom[UINT32_C(0x0000ba09)] = UINT8_C(0x20);
    rom[UINT32_C(0x0000ba0a)] = UINT8_C(0);
    rom[UINT32_C(0x0000ba0b)] = UINT8_C(0);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){8}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550000), UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), UINT32_C(0x12345678)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
    CHECK(report.exit_address == UINT32_C(0x00009444));
    CHECK(report.recovered_instruction_count == UINT64_C(26));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(cpu.ip == UINT32_C(0x00009444));
    CHECK(cpu.local_frame_depth == UINT32_C(3));
    CHECK(cpu.maximum_local_frame_depth == UINT32_C(3));
    CHECK(cpu.registers[14] == UINT32_C(0x0000ba08));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01000ef4));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00520050), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500034), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x00000100));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500031), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(8));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500068), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x12355678));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK);
    CHECK(report.exit_address == UINT32_C(0x0000ba0c));
    CHECK(report.recovered_instruction_count == UINT64_C(36));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0000ba0c));
    CHECK(cpu.local_frame_depth == UINT32_C(3));
    CHECK(read_test_u16(&machine, UINT32_C(0x01000ef4)) == UINT16_C(0x8020));
    CHECK(read_test_u16(&machine, UINT32_C(0x01000ef6)) == UINT16_C(0x8020));

    /* Phase nine (0xbaec) performs the delayed timer/state handoff and uses
     * the normal text branch when the profile phase flag is clear. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(9 * 4), UINT32_C(0x0000baec)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){9}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500864), UINT32_C(0x00521400)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521400), UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550000), UINT32_C(0)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(10));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00520050), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));

    /* Phases ten through twelve cover the compact timer chain: task-gate
     * release, pointer countdown and the phase-12 display reset. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(10 * 4), UINT32_C(0x0000bc10)
    );
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(11 * 4), UINT32_C(0x0000bcb0)
    );
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(12 * 4), UINT32_C(0x0000bce4)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){10}, 1u) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x00500028), UINT16_C(1));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00521400), UINT32_C(1) << 29u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(11));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00520050), &value) == VF2_OK);
    CHECK(value == UINT32_C(128));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00521400), &value) == VF2_OK);
    CHECK((value & (UINT32_C(1) << 29u)) == 0u);
    CHECK((value & (UINT32_C(1) << 28u)) != 0u);

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){11}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(1)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(12));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){12}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(13));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500068), &value) == VF2_OK);
    CHECK((value & (UINT32_C(1) << 14u)) != 0u);

    /* Phase thirteen samples the four Model 2A timer words and reuses the
     * gameplay task handoff with its two generated modes. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(13 * 4), UINT32_C(0x0000bdc8)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){13}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500098), UINT32_C(0x28)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500168), VF2_MAIN_DATA_BASE) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(14));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500098), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x32fcccf8));

    /* Phase fourteen exposes only the delayed 0x9444 text handoff; the timer
     * decrements while the phase remains stable. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(14 * 4), UINT32_C(0x0000c0a4)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){14}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(14));

    /* Phase fifteen's task-gated countdown reaches phase sixteen at zero. */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(15 * 4), UINT32_C(0x0000c268)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){15}, 1u) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x00500028), UINT16_C(1));
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(16));

    /* Phase sixteen decrements the task countdown and stays while the
     * result is nonzero (measured ROM corridor: 34 instructions). */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(16 * 4), UINT32_C(0x0000c414)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){16}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(2)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
    CHECK(report.recovered_instruction_count == UINT64_C(34));
    CHECK(report.recovered_procedure_calls == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(4));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(16));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00520050), &value) == VF2_OK);
    CHECK(value == UINT32_C(1));

    /* The zero countdown takes the three-instruction advance epilogue and
     * increments the phase byte to seventeen (37 instructions). */
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){16}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00520050), UINT32_C(1)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
    CHECK(report.recovered_instruction_count == UINT64_C(37));
    CHECK(report.recovered_procedure_calls == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(4));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(17));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00520050), &value) == VF2_OK);
    CHECK(value == UINT32_C(0));

    /* Phase seventeen clears the phase byte, wrapping the selector-3 cycle
     * back to phase zero (31 instructions). */
    write_rom_u32(
        rom, UINT32_C(0x0000aac4) + UINT32_C(17 * 4), UINT32_C(0x0000c448)
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &(uint8_t){3}, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500030), &(uint8_t){17}, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a6c0));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK);
    CHECK(report.recovered_instruction_count == UINT64_C(31));
    CHECK(report.recovered_procedure_calls == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(4));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500030), &selector, 1u) == VF2_OK);
    CHECK(selector == UINT8_C(0));

    vf2_model2a_shutdown(&machine);
    free(main_data);
    free(rom);
}


static void test_repeated_selector_fast_paths_and_atomicity(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_cpu before;
    vf2_hybrid_bridge_report report = {0};
    uint8_t state = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), UINT32_C(1) << 14u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00024534));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), UINT32_C(1) << 31u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c), UINT32_C(0x00010000)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00001abc));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_INPUT_UPDATE);
    CHECK(report.recovered_instruction_count == UINT64_C(23));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));

    enter_parent(&cpu, UINT32_C(0x00001f5c));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE);
    CHECK(report.recovered_instruction_count == UINT64_C(7));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    state = UINT8_C(1);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00503001), &state, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00000c78));
    before = cpu;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_ERROR_UNSUPPORTED);
    CHECK(memcmp(&cpu, &before, sizeof(cpu)) == 0);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_NONE);

    vf2_model2a_shutdown(&machine);
}


static void test_video_layer_rejection_is_write_atomic(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_cpu before;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint8_t index = 0u;
    uint8_t frame_mode = 0u;
    uint8_t auxiliary = 0u;
    const uint32_t source = UINT32_C(0x00510000);
    const size_t rom_size = (size_t)UINT32_C(0x00026694);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x00026690)] = UINT8_C(0x78);
    rom[UINT32_C(0x00026691)] = UINT8_C(0x56);
    rom[UINT32_C(0x00026692)] = UINT8_C(0x34);
    rom[UINT32_C(0x00026693)] = UINT8_C(0x12);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);

    write_test_u16(&machine, UINT32_C(0x00503036), UINT16_C(0x1111));
    write_test_u16(&machine, UINT32_C(0x00503038), UINT16_C(0x2222));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500834), source) == VF2_OK);
    write_test_u16(&machine, source + UINT32_C(0x44), UINT16_C(0x3333));
    write_test_u16(&machine, source + UINT32_C(0x46), UINT16_C(0x4444));
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050d004), &index, 1u) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x00503054), UINT16_C(1));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050304c), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &frame_mode, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500031), &auxiliary, 1u) == VF2_OK);

    write_test_u16(&machine, UINT32_C(0x018000bc), UINT16_C(0xa1a1));
    write_test_u16(&machine, UINT32_C(0x018000be), UINT16_C(0xb2b2));
    write_test_u16(&machine, UINT32_C(0x0180006c), UINT16_C(0xc3c3));
    write_test_u16(&machine, UINT32_C(0x0180008c), UINT16_C(0xd4d4));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x01801058), UINT32_C(0xe5e5e5e5)) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x00023f6c));
    before = cpu;
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_ERROR_UNSUPPORTED);
    CHECK(memcmp(&cpu, &before, sizeof(cpu)) == 0);
    CHECK(read_test_u16(&machine, UINT32_C(0x018000bc)) == UINT16_C(0xa1a1));
    CHECK(read_test_u16(&machine, UINT32_C(0x018000be)) == UINT16_C(0xb2b2));
    CHECK(read_test_u16(&machine, UINT32_C(0x0180006c)) == UINT16_C(0xc3c3));
    CHECK(read_test_u16(&machine, UINT32_C(0x0180008c)) == UINT16_C(0xd4d4));
    {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x01801058), &value) == VF2_OK);
        CHECK(value == UINT32_C(0xe5e5e5e5));
    }

    vf2_model2a_shutdown(&machine);
    free(rom);
}


static void test_frame_interrupt_support_batch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint8_t byte = 0u;
    uint32_t value = 0u;
    const size_t rom_size = (size_t)UINT32_C(0x00050728);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x00026690)] = UINT8_C(0x78);
    rom[UINT32_C(0x00026691)] = UINT8_C(0x56);
    rom[UINT32_C(0x00026692)] = UINT8_C(0x34);
    rom[UINT32_C(0x00026693)] = UINT8_C(0x12);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a748));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(5));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500700), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500710), UINT32_C(0x11223344)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005001dc), 0u) == VF2_OK);
    byte = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500f00), &byte, 1u) == VF2_OK);
    /* The compose helper samples the board's video-control bytes at these
     * offsets; keep the synthetic board input neutral while the host-input
     * mask remains covered by the model2a input tests. */
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x01c00010), &byte, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x01c00012), &byte, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x01c00014), &byte, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x01c0001c), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00001064));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE);
    CHECK(report.recovered_instruction_count == UINT64_C(63));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.executed_instructions == UINT64_C(63));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500700), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x0f000000));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500704), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x0f000000));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500708), &value) == VF2_OK);
    CHECK(value == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500710), &value) == VF2_OK);
    CHECK(value == UINT32_C(8));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500714), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x11223344));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005001dc), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x00001284));

    /* Repeated compose keeps the installed callback when no relevant video
     * bits changed. The original helper takes its two-instruction-longer
     * non-zero callback path. */
    enter_parent(&cpu, UINT32_C(0x00001064));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE);
    CHECK(report.recovered_instruction_count == UINT64_C(65));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.executed_instructions == UINT64_C(65));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500704), &value) == VF2_OK);
    CHECK(value == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005001dc), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x00001284));

    byte = UINT8_C(3);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500718), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00001290));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x01c0001e), &byte, 1u) == VF2_OK);
    CHECK(byte == UINT8_C(3));

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    byte = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00503001), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00024534));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE);

    write_test_u16(&machine, UINT32_C(0x00503036), UINT16_C(0x1111));
    write_test_u16(&machine, UINT32_C(0x00503038), UINT16_C(0x2222));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500834), UINT32_C(0x00510000)) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x00510044), UINT16_C(0x3333));
    write_test_u16(&machine, UINT32_C(0x00510046), UINT16_C(0x4444));
    byte = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050d004), &byte, 1u) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x00503054), 0u);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050304c), 0u) == VF2_OK);
    byte = UINT8_C(1);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &byte, 1u) == VF2_OK);
    byte = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500031), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00023f6c));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT);
    CHECK(report.recovered_instruction_count == UINT64_C(40));
    CHECK(read_test_u16(&machine, UINT32_C(0x018000bc)) == UINT16_C(0x1111));
    CHECK(read_test_u16(&machine, UINT32_C(0x018000be)) == UINT16_C(0x2222));
    CHECK(read_test_u16(&machine, UINT32_C(0x0180006c)) == UINT16_C(0x3333));
    CHECK(read_test_u16(&machine, UINT32_C(0x0180008c)) == UINT16_C(0x4444));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x01801058), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x12345678));
    CHECK(read_test_u16(&machine, UINT32_C(0x01800466)) == UINT16_C(0x82df));
    CHECK(read_test_u16(&machine, UINT32_C(0x0180146a)) == UINT16_C(0x815b));

    byte = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500148), &byte, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050014a), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00001e6c));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(7));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500149), &byte, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050014b), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00001edc));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE);

    byte = UINT8_C(2);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000fc), &byte, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000fd), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000012d8));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_INPUT_RING_POLL);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_MAX);

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00044268));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(4));

    /* Repeated tile controller skips glyph expansion when no controller
     * update flags are pending. The original path executes twelve
     * instructions and preserves the architectural condition state. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(0x00008a00)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059e000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059e008), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004e808));
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    cpu.compare_result = VF2_I960_COMPARE_LESS;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x0004e808));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.iterations == UINT64_C(0));
    CHECK(report.changed_values == UINT64_C(0));
    CHECK(report.bytes_written == 0u);
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(12));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == 0u);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001004));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);

    vf2_model2a_shutdown(&machine);
}


static void test_frame_geometry_gate_busy_paths(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t byte = 0u;
    const size_t rom_size = (size_t)UINT32_C(0x0000a800);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    /* No ROM bytes are read on the busy paths: 0x0000a748 -> 0x0000a800 only
     * touches work RAM at 0x00500704/0x0050002a/0x005000a6. A tiny blank ROM
     * of the maximum reachable address keeps the dispatcher's IP check happy
     * without introducing a region-decode dependency. */
    {
        uint8_t *const rom = (uint8_t *)calloc(rom_size, 1u);
        CHECK(rom != NULL);
        if (rom == NULL) {
            vf2_model2a_shutdown(&machine);
            return;
        }
        CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    }

    /* Third-sweep busy subpath observed with flags=0x0ff7f7ff and the
     * frame-state byte at 0x0050002a still below the retry threshold of 17.
     * The recovered gate must write 16 to 0x0050002a and return through the
     * architectural link. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    byte = UINT8_C(1);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a748));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(8));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(1));
    CHECK(report.bytes_written == 1u);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002a), &byte, 1u) == VF2_OK);
    CHECK(byte == UINT8_C(16));
    CHECK(cpu.ip == UINT32_C(0x00001004));

    /* The reference frame_geometry_gate clears the recovered-C procedural
     * frame on each enter_parent call, so we use a fresh parent frame for
     * the second case. With frame_state == 17 and a non-zero alt byte the
     * gate returns through 0x0000a800 after the cmpobne-taken branch
     * without touching memory. This matches the recovered second-sweep
     * evidence where 0x005000a6 == 255. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    byte = UINT8_C(17);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &byte, 1u) == VF2_OK);
    byte = UINT8_C(255);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000a6), &byte, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a748));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(7));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(0));
    CHECK(report.bytes_written == 0u);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    /* Neither of the busy-frame state bytes changed during this visit. */
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002a), &byte, 1u) == VF2_OK);
    CHECK(byte == UINT8_C(17));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x005000a6), &byte, 1u) == VF2_OK);
    CHECK(byte == UINT8_C(255));

    /* alt_byte == 0 forwards the deep reset sequence at
     * 0x0000a784 (calls to 0x00008ef0 and 0x0006116c followed by an
     * unconditional branch to 0x000000b0). Measured at 12566
     * instructions, 2 calls, 2 returns, 6180 bytes. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), UINT32_C(0x0ff7f7ff)) == VF2_OK);
    byte = UINT8_C(17);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &byte, 1u) == VF2_OK);
    byte = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000a6), &byte, 1u) == VF2_OK);
    // ensure deep reset writes are observable
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500700), UINT32_C(0x11223344)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500708), UINT32_C(0x55667788)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050070c), UINT32_C(0x99aabbcc)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00e80004), UINT32_C(0x12345678)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059cfe0), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0000a748));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE);
    CHECK(report.entry_address == UINT32_C(0x0000a748));
    CHECK(report.exit_address == UINT32_C(0x000000b0));
    CHECK(report.recovered_instruction_count == UINT64_C(12566));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(report.bytes_written == 6180u);
    CHECK(cpu.ip == UINT32_C(0x000000b0));
    // verify zeroed state
    {
        uint32_t v;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500700), &v) == VF2_OK); CHECK(v==0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500704), &v) == VF2_OK); CHECK(v==0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500708), &v) == VF2_OK); CHECK(v==0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050070c), &v) == VF2_OK); CHECK(v==0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00e80004), &v) == VF2_OK); CHECK(v==0u);
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfe0), &v) == VF2_OK); CHECK(v==UINT32_C(0x52455320));
    }

    vf2_model2a_shutdown(&machine);
}


static void test_game_meter_update(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    const size_t rom_size = (size_t)UINT32_C(0x00002a00);
    const uint32_t base = VF2_WORK_RAM_BASE + UINT32_C(0x1000);
    const uint8_t mode = 0u;
    const uint8_t variant = 0u;
    const uint8_t first[6] = {1u, 0u, 0u, 0u, 0u, 0u};
    const uint8_t second[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    uint32_t stack_selector = UINT32_MAX;
    uint32_t stack_structure = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x000027ac)] = 0u;
    rom[UINT32_C(0x000027ad)] = 1u;
    rom[UINT32_C(0x000027e2)] = UINT8_C(0x10);
    rom[UINT32_C(0x000027e6)] = UINT8_C(0x20);
    rom[UINT32_C(0x000028b8)] = 0u;
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c), base) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, base + UINT32_C(0x3320), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, base + UINT32_C(0x3324), &mode, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, base + UINT32_C(0x3350), &variant, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, base + UINT32_C(0x3380), first, sizeof(first)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, base + UINT32_C(0x3388), second, sizeof(second)) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x000020f0));
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = base;
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_METER_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x000020f0));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.iterations == UINT64_C(2));
    CHECK(report.changed_values == UINT64_C(2));
    CHECK(report.bytes_written == 40u);
    CHECK(report.recovered_instruction_count == UINT64_C(389));
    CHECK(report.recovered_procedure_calls == UINT64_C(20));
    CHECK(report.recovered_procedure_returns == UINT64_C(21));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(389));
    CHECK(cpu.procedure_calls == UINT64_C(21));
    CHECK(cpu.procedure_returns == UINT64_C(21));
    CHECK(cpu.maximum_local_frame_depth == 6u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == 1u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(2));
    CHECK(vf2_model2a_read_u32(&machine, VF2_WORK_RAM_BASE + UINT32_C(0x30c0), &stack_selector) == VF2_OK);
    CHECK(vf2_model2a_read_u32(&machine, VF2_WORK_RAM_BASE + UINT32_C(0x30c4), &stack_structure) == VF2_OK);
    CHECK(stack_selector == 1u);
    CHECK(stack_structure == base + UINT32_C(0x3388));

    /* Repeated game-state fast path: with no new video bits, the original
     * branches directly to game_meter_update, restores g9 and returns. */
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00500068), UINT32_C(1) << 31u
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500704), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x00001f5c));
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x12345678);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE);
    CHECK(report.recovered_instruction_count == UINT64_C(406));
    CHECK(report.recovered_procedure_calls == UINT64_C(21));
    CHECK(report.recovered_procedure_returns == UINT64_C(22));
    CHECK(report.bytes_written == 44u);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(406));
    CHECK(cpu.procedure_calls == UINT64_C(22));
    CHECK(cpu.procedure_returns == UINT64_C(22));
    CHECK(cpu.maximum_local_frame_depth == 7u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x12345678));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_game_event_queue_write(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t count = UINT8_C(2);
    uint8_t index = UINT8_C(3);
    uint32_t event = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00504001), &count, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00504003), &index, 1u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000438ec));
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x009e0a7f);

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_EVENT_QUEUE_WRITE);
    CHECK(report.recovered_instruction_count == UINT64_C(19));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.bytes_written == 22u);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(19));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00504001), &count, 1u) == VF2_OK);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00504003), &index, 1u) == VF2_OK);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050402c), &event) == VF2_OK);
    CHECK(count == UINT8_C(3));
    CHECK(index == UINT8_C(4));
    CHECK(event == UINT32_C(0x009e0a7f));

    vf2_model2a_shutdown(&machine);
}


static void test_texture_maintenance(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t first_base = UINT32_C(0x00551000);
    const uint32_t second_base = UINT32_C(0x00552000);
    const uint8_t first_record[2] = {1u, 0u};
    const uint8_t second_record[2] = {2u, 0u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00550188), first_record, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005501a8), second_record, 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050083c), UINT32_C(11)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500840), UINT32_C(12)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), first_base) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), second_base) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, first_base + UINT32_C(0x640), UINT32_C(7)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, second_base + UINT32_C(0x640), UINT32_C(8)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004b8d8));

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_MAINTENANCE);
    CHECK(report.recovered_instruction_count == UINT64_C(17));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(17));
    CHECK(cpu.procedure_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns == UINT64_C(3));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x005501a8));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(12));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == second_base);
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.registers[4] == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_texture_upload_and_entry_gate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *main_data = NULL;
    const uint8_t zero_short[2] = {0u, 0u};
    uint32_t row = 0u;
    uint32_t plane = 0u;
    uint32_t column = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    main_data = (uint8_t *)calloc((size_t)UINT32_C(0x0010b800), 1u);
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    for (row = 0u; row < UINT32_C(7); ++row) {
        for (plane = 0u; plane < UINT32_C(3); ++plane) {
            for (column = 0u; column < UINT32_C(16); ++column) {
                const uint16_t index = (uint16_t)(
                    UINT32_C(1) + row * UINT32_C(7) +
                    plane * UINT32_C(31) + column
                );
                const size_t source_offset =
                    (size_t)UINT32_C(0x0010b3e0) +
                    (size_t)row * 0x60u +
                    (size_t)plane * 0x20u +
                    (size_t)column * 2u;
                main_data[source_offset] = (uint8_t)index;
                main_data[source_offset + 1u] = (uint8_t)(index >> 8u);
                write_test_u16(
                    &machine,
                    UINT32_C(0x00544000) + plane * UINT32_C(0x200) +
                        (uint32_t)index * UINT32_C(2),
                    (uint16_t)(UINT16_C(0x4000) + index)
                );
            }
        }
    }
    CHECK(
        vf2_model2a_attach_main_data(
            &machine, main_data, (size_t)UINT32_C(0x0010b800)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502a8), zero_short, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502b0), zero_short, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502b8), zero_short, 2u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004ba80));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH);
    CHECK(report.recovered_instruction_count == UINT64_C(10));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00002de4));
    CHECK(cpu.local_frame_depth == 2u);

    write_test_u16(&machine, UINT32_C(0x005502a8), 0u);
    write_test_u16(&machine, UINT32_C(0x005502b0), 0u);
    write_test_u16(&machine, UINT32_C(0x005502b8), UINT16_C(1));
    write_test_u16(&machine, UINT32_C(0x005502ba), 0u);
    write_test_u16(&machine, UINT32_C(0x005502bc), UINT16_C(0x000b));
    enter_parent(&cpu, UINT32_C(0x0004ba80));
    cpu.arithmetic_control = UINT32_C(0x3f001002);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH);
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.iterations == UINT64_C(125));
    CHECK(report.changed_values == UINT64_C(337));
    CHECK(report.bytes_written == 674u);
    CHECK(report.recovered_instruction_count == UINT64_C(2035));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(2035));
    CHECK(cpu.procedure_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns == UINT64_C(3));
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x10));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x0b));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(6));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0x02109700));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001002));
    CHECK(read_test_u16(&machine, UINT32_C(0x005502b8)) == 0u);
    for (row = 0u; row < UINT32_C(7); ++row) {
        for (plane = 0u; plane < UINT32_C(3); ++plane) {
            for (column = 0u; column < UINT32_C(16); ++column) {
                const uint16_t index = (uint16_t)(
                    UINT32_C(1) + row * UINT32_C(7) +
                    plane * UINT32_C(31) + column
                );
                CHECK(
                    read_test_u16(
                        &machine,
                        UINT32_C(0x01811c60) +
                            row * UINT32_C(0x200) +
                            plane * UINT32_C(0x4000) +
                            column * UINT32_C(2)
                    ) == (uint16_t)(UINT16_C(0x4000) + index)
                );
            }
        }
    }

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055000c), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004bd00));
    report = (vf2_hybrid_bridge_report){0};
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE);
    CHECK(report.recovered_instruction_count == UINT64_C(2));
    CHECK(cpu.ip == UINT32_C(0x0004bd24));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);

    vf2_model2a_shutdown(&machine);
    free(main_data);
}


static void test_texture_record_status_setup(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    enter_parent(&cpu, UINT32_C(0x0004bd5c));
    cpu.registers[3] = 0u;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP);
    CHECK(report.entry_address == UINT32_C(0x0004bd5c));
    CHECK(report.exit_address == UINT32_C(0x0004bde0));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.bytes_written == 0u);
    CHECK(cpu.ip == UINT32_C(0x0004bde0));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(1));

    vf2_model2a_shutdown(&machine);
}


static void test_texture_stream_header_call(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t pointer_cell = UINT32_C(0x00551000);
    const uint32_t stream = UINT32_C(0x00552000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, pointer_cell, stream) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, stream, 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004be6c));
    cpu.registers[10] = pointer_cell;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL);
    CHECK(report.recovered_instruction_count == UINT64_C(5));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004c180));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == stream + UINT32_C(4));

    vf2_model2a_shutdown(&machine);
}


static void test_texture_stream_expand_and_resume(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t pointer_cell = UINT32_C(0x00551000);
    const uint32_t stream = UINT32_C(0x00560004);
    const uint32_t source = UINT32_C(0x00560010);
    const uint32_t destination = UINT32_C(0x00570000);
    uint8_t header[3] = {UINT8_C(15), UINT8_C(15), UINT8_C(6)};
    uint8_t source_data[128];
    uint8_t raw[16] = {0};
    uint32_t shifted = 0u;
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < sizeof(source_data); ++index) {
        source_data[index] = (uint8_t)(index + 1u);
    }
    CHECK(vf2_model2a_write_u32(&machine, pointer_cell, stream) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, stream, UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, stream + UINT32_C(4), header, sizeof(header)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, source, source_data, sizeof(source_data)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f8), destination) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2fc), destination + UINT32_C(0x4000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550004), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550008), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550080), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f0000c), UINT32_C(0x0007a120)) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004be6c));
    cpu.registers[10] = pointer_cell;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL);
    CHECK(report.entry_address == UINT32_C(0x0004be6c));
    CHECK(report.exit_address == UINT32_C(0x0004be80));
    CHECK(report.iterations == UINT64_C(8));
    CHECK(report.rows == UINT64_C(8));
    CHECK(report.changed_values == UINT64_C(64));
    CHECK(report.bytes_written == 257u);
    CHECK(report.recovered_instruction_count == UINT64_C(226));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.ip == UINT32_C(0x0004be80));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(226));
    CHECK(cpu.procedure_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns == UINT64_C(2));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(vf2_model2a_read(&machine, destination, raw, sizeof(raw)) == VF2_OK);
    CHECK(memcmp(raw, source_data, sizeof(raw)) == 0);
    CHECK(vf2_model2a_read_u32(&machine, destination + UINT32_C(16), &shifted) == VF2_OK);
    CHECK(shifted == UINT32_C(0x00000403));
    CHECK(vf2_model2a_read(&machine, destination + UINT32_C(0x3800), raw, sizeof(raw)) == VF2_OK);
    CHECK(memcmp(raw, source_data + 112u, sizeof(raw)) == 0);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.entry_address == UINT32_C(0x0004be80));
    CHECK(report.exit_address == UINT32_C(0x0004bf60));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x0004bf60));
    CHECK(cpu.executed_instructions == UINT64_C(229));

    vf2_model2a_shutdown(&machine);
}

static void test_status_dispatch_leading_inactive_records(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t active_record =
        VF2_ORCHESTRATOR_RECORD_START +
        UINT32_C(9) * VF2_ORCHESTRATOR_RECORD_STRIDE;
    size_t index = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < VF2_ORCHESTRATOR_RECORD_COUNT; ++index) {
        write_test_u16(
            &machine,
            VF2_ORCHESTRATOR_RECORD_START +
                (uint32_t)index * VF2_ORCHESTRATOR_RECORD_STRIDE +
                VF2_ORCHESTRATOR_RECORD_ACTIVE_OFFSET,
            index == 9u ? UINT16_MAX : 0u
        );
    }
    write_test_u16(&machine, active_record, UINT16_C(0x22));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    enter_parent(&cpu, VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL);
    CHECK(report.entry_address == VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY);
    CHECK(report.exit_address == UINT32_C(0x0004d2c0));
    CHECK(report.iterations == UINT64_C(10));
    CHECK(report.recovered_instruction_count == UINT64_C(44));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004d2c0));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.local_frames[1].registers[5] == active_record);
    CHECK(cpu.local_frames[1].registers[6] == VF2_ORCHESTRATOR_RECORD_END);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x22));

    vf2_model2a_shutdown(&machine);
}

static void test_counter_update_active_countdowns(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t value = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502c0), UINT32_C(3)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502d0), UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502e0), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004bb98));

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
    CHECK(report.recovered_instruction_count == UINT64_C(16));
    CHECK(report.changed_values == UINT64_C(2));
    CHECK(report.bytes_written == 8u);
    CHECK(cpu.ip == UINT32_C(0x0004bc58));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005502c0), &value) == VF2_OK);
    CHECK(value == UINT32_C(2));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005502d0), &value) == VF2_OK);
    CHECK(value == UINT32_C(1));

    vf2_model2a_shutdown(&machine);
}

static void test_counter_update_zero_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t arithmetic_before = UINT32_C(0x3f001004);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502c0), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502d0), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502e0), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004bb98));
    cpu.arithmetic_control = arithmetic_before;
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
    CHECK(report.exit_address == UINT32_C(0x0004bc58));
    CHECK(report.iterations == UINT64_C(3));
    CHECK(report.changed_values == UINT64_C(0));
    CHECK(report.bytes_written == 0u);
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(cpu.ip == UINT32_C(0x0004bc58));
    CHECK(cpu.registers[3] == UINT32_C(0x005502e0));
    CHECK(cpu.registers[4] == UINT32_MAX);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(1));

    vf2_model2a_shutdown(&machine);
}

static void test_game_threshold_evaluate(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    const size_t rom_size = (size_t)UINT32_C(0x00002a00);
    const uint32_t base = VF2_WORK_RAM_BASE + UINT32_C(0x1000);
    const uint8_t mode = 0u;
    const uint8_t variant = 0u;
    const uint8_t numerator0 = UINT8_C(4);
    const uint8_t numerator1 = UINT8_C(6);
    const uint8_t offset0 = UINT8_C(1);
    const uint8_t offset1 = UINT8_C(1);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x000027ac)] = 0u;
    rom[UINT32_C(0x000027ad)] = 1u;
    rom[UINT32_C(0x000027e0)] = 0u;
    rom[UINT32_C(0x000027e1)] = 0u;
    rom[UINT32_C(0x000027e2)] = UINT8_C(0x10);
    rom[UINT32_C(0x000027e3)] = 0u;
    rom[UINT32_C(0x000027e4)] = 0u;
    rom[UINT32_C(0x000027e5)] = 0u;
    rom[UINT32_C(0x000027e6)] = UINT8_C(0x20);
    rom[UINT32_C(0x000027e7)] = 0u;
    rom[UINT32_C(0x000028b8)] = 0u;
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0050016c), base
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, base + UINT32_C(0x3320), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3350), &variant,
            sizeof(variant)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3380), &numerator0,
            sizeof(numerator0)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3388), &numerator1,
            sizeof(numerator1)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x3385), &offset0,
            sizeof(offset0)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, base + UINT32_C(0x338d), &offset1,
            sizeof(offset1)
        ) == VF2_OK
    );

    enter_parent(&cpu, UINT32_C(0x000028d4));
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE);
    CHECK(report.entry_address == UINT32_C(0x000028d4));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(2));
    CHECK(report.bytes_written == 8u);
    CHECK(report.recovered_instruction_count == UINT64_C(125));
    CHECK(report.recovered_procedure_calls == UINT64_C(5));
    CHECK(report.recovered_procedure_returns == UINT64_C(6));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(125));
    CHECK(cpu.procedure_calls == UINT64_C(6));
    CHECK(cpu.procedure_returns == UINT64_C(6));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.registers[17] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    vf2_model2a_shutdown(&machine);
    free(rom);
}


static void test_active_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint32_t active_flags = UINT32_MAX;
    const uint32_t record = UINT32_C(0x00550168);
    const uint32_t stream = UINT32_C(0x00551000);
    const uint32_t stream_word = UINT32_C(0x12340004);
    const uint8_t count[2] = {3u, 0u};
    const size_t rom_size = (size_t)UINT32_C(0x0004c200);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0004c128)] = 10u;
    rom[UINT32_C(0x0004c129)] = 0u;
    rom[UINT32_C(0x0004c12a)] = 0xfdu;
    rom[UINT32_C(0x0004c12b)] = 0xffu;
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x10), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, record + UINT32_C(2), count, sizeof(count)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x14), stream
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, record + UINT32_C(0x18), UINT32_C(0x00552000)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, stream, stream_word) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004bde0));
    cpu.registers[5] = record;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL);
    CHECK(report.entry_address == UINT32_C(0x0004bde0));
    CHECK(report.exit_address == UINT32_C(0x0004d16c));
    CHECK(report.recovered_instruction_count == UINT64_C(22));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.bytes_written == 4u);
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004d16c));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.maximum_local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(22));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.local_frames[1].registers[2] == UINT32_C(0x0004be6c));
    CHECK(cpu.local_frames[1].registers[5] == record);
    CHECK(cpu.local_frames[1].registers[6] == 1u);
    CHECK(cpu.local_frames[1].registers[7] == 0u);
    CHECK(cpu.local_frames[1].registers[8] == 3u);
    CHECK(cpu.local_frames[1].registers[9] == stream);
    CHECK(cpu.local_frames[1].registers[10] == UINT32_C(0x00552000));
    CHECK(cpu.registers[16] == UINT32_C(0x12));
    CHECK(cpu.registers[17] == UINT32_C(0x34));
    CHECK(cpu.registers[18] == stream_word);
    CHECK(cpu.registers[22] == UINT32_C(28));
    CHECK(cpu.registers[23] == UINT32_C(49));
    CHECK(cpu.registers[24] == 0u);
    CHECK(cpu.arithmetic_control == (arithmetic_before & ~UINT32_C(7)));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x0055c2f4), &active_flags
        ) == VF2_OK
    );
    CHECK(active_flags == 0u);

    free(rom);
    vf2_model2a_shutdown(&machine);
}


static void test_record_advance_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t pointer0 = 0u;
    uint32_t pointer1 = 0u;
    uint8_t count_bytes[2] = {3u, 0u};
    uint8_t final_count[2] = {0u, 0u};
    const uint32_t record = UINT32_C(0x00550168);
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write(
            &machine, record + UINT32_C(2), count_bytes, sizeof(count_bytes)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bf60));
    cpu.registers[5] = record;
    cpu.registers[6] = 1u;
    cpu.registers[8] = 3u;
    cpu.registers[9] = UINT32_C(0x00551000);
    cpu.registers[10] = UINT32_C(0x00552000);
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE);
    CHECK(report.entry_address == UINT32_C(0x0004bf60));
    CHECK(report.exit_address == UINT32_C(0x0004bd24));
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(3));
    CHECK(report.bytes_written == 10u);
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bd24));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(12));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(3));
    CHECK(cpu.registers[6] == 0u);
    CHECK(cpu.registers[8] == 2u);
    CHECK(cpu.registers[9] == UINT32_C(0x00551004));
    CHECK(cpu.registers[10] == UINT32_C(0x00552004));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(2))
    );
    CHECK(
        vf2_model2a_read(
            &machine, record + UINT32_C(2), final_count, sizeof(final_count)
        ) == VF2_OK
    );
    CHECK(final_count[0] == 2u && final_count[1] == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, record + UINT32_C(0x14), &pointer0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, record + UINT32_C(0x18), &pointer1
        ) == VF2_OK
    );
    CHECK(pointer0 == UINT32_C(0x00551004));
    CHECK(pointer1 == UINT32_C(0x00552004));

    vf2_model2a_shutdown(&machine);
}


static void test_color_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t zero = 0u;
    uint8_t dimensions[4] = {8u, 0u, 6u, 0u};

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550004), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &zero, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f0000c), UINT32_C(0x000ffffe)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550008), 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550080), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f4), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0055c320), dimensions, sizeof(dimensions)) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004cd18));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE);
    CHECK(report.entry_address == UINT32_C(0x0004cd18));
    CHECK(report.exit_address == UINT32_C(0x0004cdb0));
    CHECK(report.recovered_instruction_count == UINT64_C(33));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0004cdb0));
    CHECK(cpu.executed_instructions == UINT64_C(33));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[11] == UINT32_C(0x0055c2f8));
    CHECK(cpu.registers[12] == UINT32_C(8));
    CHECK(cpu.registers[13] == UINT32_C(6));
    CHECK(cpu.registers[14] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[15] == UINT32_C(0x000ffffe));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.registers[26] == UINT32_C(2048));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);

    vf2_model2a_shutdown(&machine);
}


static void test_word_prepare_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t zero = 0u;
    uint8_t wait = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550004), 0u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &zero, 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f0000c), UINT32_C(0x000fffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550008), 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550080), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f4), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c340), UINT32_C(7)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0055c2f8), UINT32_C(9)) == VF2_OK);

    enter_parent(&cpu, UINT32_C(0x0004cb64));
    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE);
    CHECK(report.entry_address == UINT32_C(0x0004cb64));
    CHECK(report.exit_address == UINT32_C(0x0004cc28));
    CHECK(report.recovered_instruction_count == UINT64_C(34));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.bytes_written == 5u);
    CHECK(cpu.ip == UINT32_C(0x0004cc28));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(34));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[8] == UINT32_C(7));
    CHECK(cpu.registers[9] == UINT32_C(0x005502f0));
    CHECK(cpu.registers[11] == UINT32_C(9));
    CHECK(cpu.registers[13] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[14] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[15] == UINT32_C(0x000fffff));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_NONE);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050008c), &wait, 1u) == VF2_OK);
    CHECK(wait == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_tree_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint8_t *rom = NULL;
    uint32_t value = 0u;
    const size_t rom_size = (size_t)UINT32_C(0x0004ae00);
    const uint32_t stack_start = VF2_WORK_RAM_BASE + UINT32_C(0x3000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(rom_size, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    rom[UINT32_C(0x0004ad98)] = UINT8_C(0x78);
    rom[UINT32_C(0x0004ad99)] = UINT8_C(0x56);
    rom[UINT32_C(0x0004ad9a)] = UINT8_C(0x34);
    rom[UINT32_C(0x0004ad9b)] = UINT8_C(0x12);
    rom[UINT32_C(0x0004ad9c)] = UINT8_C(0xef);
    rom[UINT32_C(0x0004ad9d)] = UINT8_C(0xcd);
    rom[UINT32_C(0x0004ad9e)] = UINT8_C(0xab);
    rom[UINT32_C(0x0004ad9f)] = UINT8_C(0x90);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, rom_size) == VF2_OK);

    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c344), UINT32_C(0x0055c34c)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c348), UINT32_C(0x80000011)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c34c), UINT32_C(0x90000022)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c2f4), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c33c), UINT32_C(3)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x0055c340), UINT32_C(4)
        ) == VF2_OK
    );
    {
        const uint8_t width[2] = {8u, 0u};
        CHECK(
            vf2_model2a_write(
                &machine, UINT32_C(0x0055c320), width, sizeof(width)
            ) == VF2_OK
        );
    }

    enter_parent(&cpu, UINT32_C(0x0004c544));
    CHECK(cpu.registers[1] == stack_start + UINT32_C(64));
    cpu.registers[3] = UINT32_C(0x33333333);
    cpu.registers[4] = UINT32_C(0x44444444);
    cpu.registers[13] = UINT32_C(0xabcdef00);
    cpu.registers[17] = UINT32_C(0x17171717);
    cpu.registers[22] = UINT32_C(0x22222222);
    cpu.registers[27] = UINT32_C(0x27272727);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH);
    CHECK(report.entry_address == UINT32_C(0x0004c544));
    CHECK(report.exit_address == UINT32_C(0x0004c6e0));
    CHECK(report.iterations == UINT64_C(256));
    CHECK(report.bytes_written == 2160u);
    CHECK(report.recovered_instruction_count == UINT64_C(858));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004c6e0));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.registers[1] == stack_start + UINT32_C(64));
    CHECK(cpu.executed_instructions == UINT64_C(858));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[3] == UINT32_C(0x33333333));
    CHECK(cpu.registers[4] == UINT32_C(0x44444444));
    CHECK(cpu.registers[5] == UINT32_C(0x00545000));
    CHECK(cpu.registers[6] == UINT32_C(3));
    CHECK(cpu.registers[7] == 0u);
    CHECK(cpu.registers[8] == 0u);
    CHECK(cpu.registers[9] == UINT32_C(0x0055c2ee));
    CHECK(cpu.registers[10] == UINT32_C(0x0055c2ef));
    CHECK(cpu.registers[11] == UINT32_C(0x005502f0));
    CHECK(cpu.registers[12] == UINT32_C(7));
    CHECK(cpu.registers[13] == UINT32_C(0xabcdef00));
    CHECK(cpu.registers[16] == 0u);
    CHECK(cpu.registers[17] == UINT32_C(0x17171717));
    CHECK(cpu.registers[20] == UINT32_C(0x81000011));
    CHECK(cpu.registers[21] == UINT32_C(0x12345678));
    CHECK(cpu.registers[22] == UINT32_C(0x22222222));
    CHECK(cpu.registers[26] == UINT32_C(4));
    CHECK(cpu.registers[27] == UINT32_C(0x27272727));
    CHECK(cpu.registers[30] == UINT32_C(8));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00545000), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x81000011));
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00545004), &value
        ) == VF2_OK
    );
    CHECK(value == UINT32_C(0x12345678));

    free(rom);
    vf2_model2a_shutdown(&machine);
}


static void test_counter_update_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t counter0 = UINT32_MAX;
    uint32_t counter1 = UINT32_MAX;
    uint32_t counter2 = UINT32_MAX;
    const uint32_t arithmetic_before = UINT32_C(0x3f001002);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502c0), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502d0), 0u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502e0), UINT32_C(3)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bb98));
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x0004bb98));
    CHECK(report.exit_address == UINT32_C(0x0004bc58));
    CHECK(report.iterations == UINT64_C(3));
    CHECK(report.changed_values == UINT64_C(1));
    CHECK(report.bytes_written == 4u);
    CHECK(report.recovered_instruction_count == UINT64_C(14));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(report.cpu_poststate_applied == 1);
    CHECK(cpu.ip == UINT32_C(0x0004bc58));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(14));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[3] == UINT32_C(0x005502e0));
    CHECK(cpu.registers[4] == UINT32_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(
        cpu.arithmetic_control ==
        ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(4))
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502c0), &counter0
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502d0), &counter1
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x005502e0), &counter2
        ) == VF2_OK
    );
    CHECK(counter0 == 0u);
    CHECK(counter1 == 0u);
    CHECK(counter2 == UINT32_C(2));

    vf2_model2a_shutdown(&machine);
}



static void test_counter_update_expiry_dispatch(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t value = 0u;
    const uint32_t arithmetic_before = UINT32_C(0x3f001004);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502c0), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502d0), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502e0), UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502e4), UINT32_C(0x22)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502e8), UINT32_C(0x0b)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502ec), UINT32_C(0x162)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00550288), UINT32_C(0x0000ffff)) == VF2_OK);
    write_test_u16(&machine, UINT32_C(0x0055c2f0), 0u);
    enter_parent(&cpu, UINT32_C(0x0004bb98));
    cpu.arithmetic_control = arithmetic_before;
    cpu.compare_result = VF2_I960_COMPARE_LESS;

    CHECK(vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x0004bb98));
    CHECK(report.exit_address == UINT32_C(0x0004bc58));
    CHECK(report.iterations == UINT64_C(3));
    CHECK(report.changed_values == UINT64_C(12));
    CHECK(report.bytes_written == 38u);
    CHECK(report.recovered_instruction_count == UINT64_C(55));
    CHECK(report.recovered_procedure_calls == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x0004bc58));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.executed_instructions == UINT64_C(55));
    CHECK(cpu.procedure_calls == UINT64_C(4));
    CHECK(cpu.procedure_returns == UINT64_C(3));
    CHECK(cpu.maximum_local_frame_depth == 3u);
    CHECK(cpu.registers[1] == VF2_WORK_RAM_BASE + UINT32_C(0x3040));
    CHECK(cpu.registers[3] == UINT32_C(0x005502e0));
    CHECK(cpu.registers[4] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x0b));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(0x005502b8));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(1));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(cpu.arithmetic_control == ((arithmetic_before & ~UINT32_C(7)) | UINT32_C(2)));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005502e0), &value) == VF2_OK);
    CHECK(value == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00550288), &value) == VF2_OK);
    CHECK(value == UINT32_C(0xffff0022));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005502a4), &value) == VF2_OK);
    CHECK(value == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005502b8), &value) == VF2_OK);
    CHECK(value == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005502bc), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x0b));
    CHECK(vf2_model2a_read_u32(&machine, VF2_WORK_RAM_BASE + UINT32_C(0x3080), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x0b));

    vf2_model2a_shutdown(&machine);
}


static void test_final_status_zero_counter_return(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    uint32_t cleared = UINT32_MAX;
    const uint32_t arithmetic_before = UINT32_C(0x3f001004);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    write_test_u16(&machine, UINT32_C(0x0055c2f0), UINT16_MAX);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502c0), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502d0), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005502e0), 0u) == VF2_OK);
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00508000), UINT32_C(1) << 9u
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x00550000), UINT32_C(0xfeedface)
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x0004bf90));
    cpu.compare_result = VF2_I960_COMPARE_LESS;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL);
    CHECK(report.entry_address == UINT32_C(0x0004bf90));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.iterations == UINT64_C(3));
    CHECK(report.changed_values == UINT64_C(2));
    CHECK(report.bytes_written == 6u);
    CHECK(report.recovered_instruction_count == UINT64_C(13));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.executed_instructions == UINT64_C(13));
    CHECK(cpu.procedure_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_LESS);
    CHECK(cpu.arithmetic_control == arithmetic_before);
    CHECK(read_test_u16(&machine, UINT32_C(0x0055c2f0)) == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &machine, UINT32_C(0x00550000), &cleared
        ) == VF2_OK
    );
    CHECK(cleared == 0u);

    vf2_model2a_shutdown(&machine);
}


static void test_final_status_first_counter_call(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report = {0};
    const uint32_t arithmetic_before = UINT32_C(0x3f001001);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    write_test_u16(&machine, UINT32_C(0x0055c2f0), UINT16_MAX);
    CHECK(
        vf2_model2a_write_u32(
            &machine, UINT32_C(0x005502c0), UINT32_C(5)
        ) == VF2_OK
    );
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0004bf90));
    cpu.compare_result = VF2_I960_COMPARE_GREATER;
    cpu.arithmetic_control = arithmetic_before;

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL);
    CHECK(report.entry_address == UINT32_C(0x0004bf90));
    CHECK(report.exit_address == UINT32_C(0x0004d25c));
    CHECK(report.iterations == UINT64_C(1));
    CHECK(report.changed_values == UINT64_C(1));
    CHECK(report.bytes_written == 2u);
    CHECK(report.recovered_instruction_count == UINT64_C(7));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(cpu.ip == UINT32_C(0x0004d25c));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.executed_instructions == UINT64_C(7));
    CHECK(cpu.procedure_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns == UINT64_C(0));
    CHECK(cpu.local_frames[1].registers[3] == 0u);
    CHECK(cpu.local_frames[1].registers[14] == UINT32_C(5));
    CHECK(cpu.local_frames[1].registers[15] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(cpu.arithmetic_control == arithmetic_before);
    CHECK(read_test_u16(&machine, UINT32_C(0x0055c2f0)) == 0u);

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_system_memory_diagnostic();
    test_video_input_sync();
    test_frame_counter_and_phase();
    test_frame_shadow_and_buffer_gate();
    test_frame_dispatch_tick();
    test_repeated_selector_fast_paths_and_atomicity();
    test_video_layer_rejection_is_write_atomic();
    test_frame_interrupt_support_batch();
    test_frame_geometry_gate_busy_paths();
    test_game_event_queue_write();
    test_texture_maintenance();
    test_texture_upload_and_entry_gate();
    test_texture_record_status_setup();
    test_texture_stream_header_call();
    test_texture_stream_expand_and_resume();
    test_status_dispatch_leading_inactive_records();
    test_game_threshold_evaluate();
    test_game_meter_update();
    test_inactive_scan_dispatch();
    test_child_gate_dispatch(
        VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY,
        VF2_ORCHESTRATOR_CHILD_GATE_A_TARGET,
        VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A
    );
    test_child_gate_dispatch(
        VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY,
        VF2_ORCHESTRATOR_CHILD_GATE_B_TARGET,
        VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B
    );
    test_loop_gate_dispatch();
    test_header_decode_dispatch();
    test_header_decode_context_restore();
    test_color_prepare_dispatch();
    test_word_prepare_dispatch();
    test_tree_dispatch();
    test_active_prepare_dispatch();
    test_record_advance_dispatch();
    test_final_status_zero_counter_return();
    test_final_status_first_counter_call();
    test_counter_update_active_countdowns();
    test_counter_update_dispatch();
    test_counter_update_zero_dispatch();
    test_counter_update_expiry_dispatch();

    if (failures != 0) {
        fprintf(stderr, "%d orchestrator-bridge test(s) failed\n", failures);
        return 1;
    }
    printf("orchestrator-bridge tests passed\n");
    return 0;
}
