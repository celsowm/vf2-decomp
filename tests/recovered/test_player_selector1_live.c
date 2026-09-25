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
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void run_case(const char *rom_directory)
{
    vf2_model2a ref_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    uint8_t *rom = NULL;
    uint8_t *data = NULL;
    size_t rom_size = 0u;
    size_t data_size = 0u;
    char snapshot_path[512];
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    memset(&ref_machine, 0, sizeof(ref_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);
    CHECK(vf2_model2a_initialize(&ref_machine) != 0);
    CHECK(vf2_model2a_initialize(&native_machine) != 0);
    CHECK(vf2_romset_build_region(rom_directory, VF2_REGION_MAINCPU,
                                  &rom, &rom_size) == VF2_OK);
    CHECK(vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA,
                                  &data, &data_size) == VF2_OK);
    (void)snprintf(snapshot_path, sizeof(snapshot_path),
                   "%s/../../out/144b0-nonzero-before1a1e4.vf2snap",
                   rom_directory);
    CHECK(vf2_i960_snapshot_read_file(
        &snap, snapshot_path) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &ref_cpu, &ref_machine) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &native_cpu, &native_machine) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&ref_machine, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_machine, data, data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&native_machine, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&native_machine, data, data_size) == VF2_OK);
    start_instructions = snap.cpu.executed_instructions;
    start_calls = snap.cpu.procedure_calls;
    start_returns = snap.cpu.procedure_returns;
    while (ref_cpu.ip != UINT32_C(0x0001a048)) {
        CHECK(vf2_i960_step(&ref_cpu, &ref_machine, NULL) == VF2_OK);
        if (ref_cpu.executed_instructions - start_instructions > 512u) break;
    }
    CHECK(ref_cpu.ip == UINT32_C(0x0001a048));
    CHECK(ref_cpu.executed_instructions - start_instructions == UINT64_C(167));
    CHECK(ref_cpu.procedure_calls - start_calls == UINT64_C(1));
    CHECK(ref_cpu.procedure_returns - start_returns == UINT64_C(1));
    {
        const vf2_status native_setup =
            vf2_hybrid_player_selector1_setup_execute_for_test(
                &native_machine, &native_cpu);
        CHECK(native_setup == VF2_OK);
    }
    CHECK(native_cpu.ip == UINT32_C(0x0001a048));
    CHECK(native_cpu.executed_instructions - start_instructions == UINT64_C(167));
    CHECK(native_cpu.procedure_calls - start_calls == UINT64_C(1));
    CHECK(native_cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(vf2_i960_compare_live_state(&ref_cpu, &ref_machine,
                                      &native_cpu, &native_machine, &diff) == VF2_OK);
    CHECK(diff.equal);
    if (!diff.equal) {
        fprintf(stderr, "selector1 diff component=%s offset=%zu expected=0x%08x actual=0x%08x bytes=%zu\n",
                diff.component, diff.first_offset, (unsigned)diff.expected_value,
                (unsigned)diff.actual_value, diff.differing_bytes);
    }
    puts("player-selector1-live ref=167 native=167 calls=1/1 rets=1/1");
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&ref_machine);
    vf2_model2a_shutdown(&native_machine);
    free(rom);
    free(data);
}

static void run_return_tail_case(const char *rom_directory)
{
    vf2_model2a ref_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    uint8_t *rom = NULL;
    uint8_t *data = NULL;
    size_t rom_size = 0u;
    size_t data_size = 0u;
    char snapshot_path[512];
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    memset(&ref_machine, 0, sizeof(ref_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);
    CHECK(vf2_model2a_initialize(&ref_machine) != 0);
    CHECK(vf2_model2a_initialize(&native_machine) != 0);
    CHECK(vf2_romset_build_region(rom_directory, VF2_REGION_MAINCPU,
                                  &rom, &rom_size) == VF2_OK);
    CHECK(vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA,
                                  &data, &data_size) == VF2_OK);
    (void)snprintf(snapshot_path, sizeof(snapshot_path),
                   "%s/../../out/selector1-before-144b8.vf2snap",
                   rom_directory);
    CHECK(vf2_i960_snapshot_read_file(&snap, snapshot_path) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &ref_cpu, &ref_machine) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &native_cpu, &native_machine) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&ref_machine, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_machine, data, data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&native_machine, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&native_machine, data, data_size) == VF2_OK);
    start_instructions = snap.cpu.executed_instructions;
    start_calls = snap.cpu.procedure_calls;
    start_returns = snap.cpu.procedure_returns;
    while (ref_cpu.ip != UINT32_C(0x0001463c)) {
        CHECK(vf2_i960_step(&ref_cpu, &ref_machine, NULL) == VF2_OK);
        if (ref_cpu.executed_instructions - start_instructions > 128u) break;
    }
    CHECK(ref_cpu.ip == UINT32_C(0x0001463c));
    CHECK(ref_cpu.executed_instructions - start_instructions == UINT64_C(35));
    CHECK(ref_cpu.procedure_calls - start_calls == 0u);
    CHECK(ref_cpu.procedure_returns - start_returns == 0u);
    {
        const vf2_status tail_status =
            vf2_hybrid_player_selector1_return_tail_execute_for_test(
                &native_machine, &native_cpu);
        if (tail_status != VF2_OK) {
            fprintf(stderr, "selector1-tail status=%d ip=%08x depth=%u g7=%08x g8=%08x\n",
                    (int)tail_status, (unsigned)native_cpu.ip,
                    (unsigned)native_cpu.local_frame_depth,
                    (unsigned)native_cpu.registers[VF2_I960_G0_REGISTER + 7u],
                    (unsigned)native_cpu.registers[VF2_I960_G0_REGISTER + 8u]);
        }
        CHECK(tail_status == VF2_OK);
    }
    CHECK(native_cpu.ip == UINT32_C(0x0001463c));
    CHECK(native_cpu.executed_instructions - start_instructions == UINT64_C(35));
    CHECK(native_cpu.procedure_calls - start_calls == 0u);
    CHECK(native_cpu.procedure_returns - start_returns == 0u);
    CHECK(vf2_i960_compare_live_state(&ref_cpu, &ref_machine,
                                      &native_cpu, &native_machine, &diff) == VF2_OK);
    CHECK(diff.equal);
    if (!diff.equal) {
        fprintf(stderr, "selector1-tail diff component=%s offset=%zu expected=0x%08x actual=0x%08x bytes=%zu\n",
                diff.component, diff.first_offset, (unsigned)diff.expected_value,
                (unsigned)diff.actual_value, diff.differing_bytes);
    }
    puts("player-selector1-tail ref=35 native=35 calls=0/0 rets=0/0");
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&ref_machine);
    vf2_model2a_shutdown(&native_machine);
    free(rom);
    free(data);
}

int main(int argc, char **argv)
{
    if (argc == 1) return 0;
    if (argc == 2) {
        run_case(argv[1]);
        run_return_tail_case(argv[1]);
    }
    if (failures != 0) return 1;
    puts("player-selector1-live differential tests passed");
    return 0;
}
