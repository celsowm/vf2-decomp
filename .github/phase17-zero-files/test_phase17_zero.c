#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expression);               \
            ++failures;                                             \
        }                                                           \
    } while (0)

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status initialize_phase17_zero_state(
    vf2_model2a *machine,
    uint32_t runtime_flags
)
{
    const uint32_t player0 = UINT32_C(0x00510000);
    const uint32_t player1 = UINT32_C(0x00512000);
    const uint32_t control = UINT32_C(0x00514000);
    const uint32_t descriptor = UINT32_C(0x00516000);
    uint16_t fighter_control = UINT16_C(0x1234);
    vf2_status status = VF2_OK;

    status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000), runtime_flags);
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x0050002a), UINT8_C(17));
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x005000a6), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500804), player0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500808), player1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500814), control);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050081c), descriptor);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, descriptor + UINT32_C(0x0c), UINT32_C(0x0001b9ac));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500700), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500704), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050070c), 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x00508008), 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x00500085), UINT8_C(0x40));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a0), &fighter_control,
            sizeof(fighter_control));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player0, UINT32_MAX);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player1, UINT32_MAX);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, player0 + UINT32_C(0x1200), UINT8_C(0x5a));
    }
    if (status == VF2_OK) {
        status = write_u8(machine, player1 + UINT32_C(0x1200), UINT8_C(0xa5));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500860), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050083c), UINT32_C(0x00518000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00518000), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500840), UINT32_C(0x00518100));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00518100), 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x0050a0b8), 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, UINT32_C(0x0050a0b9), 0u);
    }
    return status;
}

static vf2_status enter_frame_dispatch(vf2_i960_cpu *cpu)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = UINT32_C(0x00503000);
    return vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0000a6c0), UINT32_C(0x00001004));
}

static void run_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    uint32_t runtime_flags,
    uint64_t expected_instructions
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result run_result;
    vf2_hybrid_bridge_report bridge_report;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&options, 0, sizeof(options));
    memset(&run_result, 0, sizeof(run_result));
    memset(&bridge_report, 0, sizeof(bridge_report));
    memset(&diff, 0, sizeof(diff));

    CHECK(vf2_model2a_initialize(&reference_machine));
    CHECK(vf2_model2a_initialize(&native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(
              &reference_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(
              &native_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(initialize_phase17_zero_state(
              &reference_machine, runtime_flags) == VF2_OK);
    CHECK(initialize_phase17_zero_state(
              &native_machine, runtime_flags) == VF2_OK);
    CHECK(enter_frame_dispatch(&reference_cpu) == VF2_OK);
    CHECK(enter_frame_dispatch(&native_cpu) == VF2_OK);

    options.stop_address = UINT32_C(0x00001004);
    options.max_steps = UINT64_C(200000);
    reference_status = vf2_i960_run(
        &reference_cpu, &reference_machine, &options, &run_result);
    native_status = vf2_hybrid_post_frame_bridge_execute(
        &native_machine, &native_cpu, &bridge_report);
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);

    if (reference_status != VF2_OK || native_status != VF2_OK ||
        compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "phase17-zero flags=0x%08x ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (unsigned)runtime_flags, (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(reference_status == VF2_OK);
    CHECK(run_result.halt_reason == VF2_I960_HALT_STOP_ADDRESS);
    CHECK(reference_cpu.executed_instructions == expected_instructions);
    CHECK(native_status == VF2_OK);
    CHECK(bridge_report.recovered_instruction_count == expected_instructions);
    CHECK(bridge_report.recovered_procedure_calls == UINT64_C(6));
    CHECK(bridge_report.recovered_procedure_returns == UINT64_C(7));
    CHECK(native_cpu.maximum_local_frame_depth == UINT32_C(5));
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

int main(int argc, char **argv)
{
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;

    if (argc != 2) {
        fprintf(stderr, "usage: %s ROM_DIR\n", argv[0]);
        return EXIT_FAILURE;
    }
    CHECK(vf2_romset_build_region(
              argv[1], VF2_REGION_MAINCPU,
              &main_rom, &main_rom_size) == VF2_OK);
    if (main_rom == NULL) {
        return EXIT_FAILURE;
    }

    run_case(main_rom, main_rom_size, 0u, UINT64_C(267));
    run_case(
        main_rom, main_rom_size, UINT32_C(1) << 9u, UINT64_C(266));
    free(main_rom);

    if (failures != 0) {
        fprintf(stderr, "%d phase17-zero differential test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("phase17-zero differential tests passed");
    return EXIT_SUCCESS;
}
