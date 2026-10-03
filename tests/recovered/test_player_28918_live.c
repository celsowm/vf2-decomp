/* Live differential for the 0x28918 curve/keyframe evaluator (v0713).
 *
 * Witness (measured, current build):
 *   vf2probe --rom-dir roms/vf2 --snapshot out/player-14288-natres.vf2snap \
 *     --set-ip 0x27D00 --set-reg r1=0x5FF500 --set-reg fp=0x5FF400 \
 *     --set-reg r23=<fighter> --set-reg r27=0x1008000 --set-reg r28=0 \
 *     --set-reg r0=0 ... (all other regs 0) --set-u16 <fighter>+0x1AA=<ctr> \
 *     --until 0x28274
 *     F0 counter 2  -> 1088 steps, ip == 0x28274
 *     F1 counter 2  -> 1920 steps, ip == 0x28274
 *     F0 counter 0  -> 1058 steps, ip == 0x28274
 *     F0 counter 30 -> 1060 steps, ip == 0x28274
 *
 * The fixture restores the measured live snapshot
 * out/player-14288-natres.vf2snap (fighter curve tables at 0x520000 /
 * 0x5207c8; if the file is absent, regenerate it via the 0x14288
 * corridor recipe in fa_player_downstream_dual_base_v0706.md and the
 * 0x14288 live tests), applies the counter mutation, zeroes registers
 * except sp/fp/g7/g11/g12, runs the reference 0x27d00 -> 0x28274
 * window, then proves the native
 * vf2_hybrid_player_28918_execute_for_test chain step/call/ret-locked
 * with full live-state equality. The (g11)[g12] staging word is
 * loop-invariant and write-before-read, so the VRAM scratch is
 * faithful (probe-verified at two addresses).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/hybrid/test_entries.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

static int failures = 0;

#define CHECK(condition)                                   \
    do {                                                   \
        if (!(condition)) {                                \
            fprintf(                                       \
                stderr, "CHECK failed %s:%d: %s\n",         \
                __FILE__, __LINE__, #condition);           \
            ++failures;                                    \
            return;                                        \
        }                                                  \
    } while (0)

#define PLAYER_28918_ENTRY UINT32_C(0x00027d00)
#define PLAYER_28918_RETURN UINT32_C(0x00028274)
#define PLAYER_28918_STACK UINT32_C(0x005ff500)
#define PLAYER_28918_SCRATCH UINT32_C(0x001008000)

static void write_u16(vf2_model2a *machine, uint32_t address, uint16_t value)
{
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));

    if (vf2_hybrid_player_28918_execute_for_test(NULL, &cpu) == VF2_OK) {
        fprintf(stderr, "28918 null machine accepted\n");
        ++failures;
    }
    if (vf2_hybrid_player_28918_execute_for_test(&machine, NULL) == VF2_OK) {
        fprintf(stderr, "28918 null cpu accepted\n");
        ++failures;
    }
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint32_t fighter,
    uint16_t counter,
    uint64_t ref_steps
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    int ok = 0;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);

    CHECK(vf2_model2a_initialize(&reference_machine));
    CHECK(vf2_model2a_initialize(&native_machine));
    if (reference_machine.work_ram == NULL ||
        native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    ok = (vf2_i960_snapshot_read_file(
              &snap,
              "D:/ia/vf2-decomp/out/player-14288-natres.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/player-14288-natres.vf2snap") == VF2_OK);
    CHECK(ok);
    if (!ok) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    CHECK(vf2_i960_snapshot_restore(
              &snap, &reference_cpu, &reference_machine) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(
              &snap, &native_cpu, &native_machine) == VF2_OK);
    CHECK(
        vf2_model2a_attach_main_rom(&reference_machine, main_rom,
                                    main_rom_size) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_data(&reference_machine, main_data,
                                     main_data_size) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_rom(&native_machine, main_rom,
                                    main_rom_size) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_data(&native_machine, main_data,
                                     main_data_size) == VF2_OK
    );
    /* Routing mutation only; the table chain, buffers and keyframes
     * stay live. Live routing fields already match the S0 bit20-clear
     * curve==0 shape (mode 0x80004400, sense 0, F0 edge 0x78). */
    write_u16(&reference_machine, fighter + UINT32_C(0x1aa), counter);
    write_u16(&native_machine, fighter + UINT32_C(0x1aa), counter);

    vf2_i960_cpu_reset(&reference_cpu, 0u, 0u, PLAYER_28918_ENTRY);
    vf2_i960_cpu_reset(&native_cpu, 0u, 0u, PLAYER_28918_ENTRY);
    reference_cpu.registers[1] = PLAYER_28918_STACK;
    native_cpu.registers[1] = PLAYER_28918_STACK;
    reference_cpu.registers[VF2_I960_FP_REGISTER] = PLAYER_28918_STACK -
        UINT32_C(0x100);
    native_cpu.registers[VF2_I960_FP_REGISTER] = PLAYER_28918_STACK -
        UINT32_C(0x100);
    reference_cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter;
    native_cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter;
    reference_cpu.registers[VF2_I960_G0_REGISTER + 11u] =
        PLAYER_28918_SCRATCH;
    native_cpu.registers[VF2_I960_G0_REGISTER + 11u] = PLAYER_28918_SCRATCH;

    memset(&options, 0, sizeof(options));
    options.stop_address = PLAYER_28918_RETURN;
    options.max_steps = 4096u;
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    reference_status =
        vf2_i960_run(&reference_cpu, &reference_machine, &options, &result);
    /* CPUs were reset, so counters start at zero and finals are deltas. */
    if (reference_status != VF2_OK ||
        reference_cpu.ip != PLAYER_28918_RETURN ||
        result.executed_instructions != ref_steps ||
        reference_cpu.procedure_calls != UINT64_C(2) ||
        reference_cpu.procedure_returns != UINT64_C(1)) {
        fprintf(
            stderr,
            "player-28918-live fighter=%08x counter=%u ref status=%d "
            "ip=%08x steps=%llu calls=%llu rets=%llu want 0x28274/%llu/2/1\n",
            (unsigned)fighter, (unsigned)counter, (int)reference_status,
            (unsigned)reference_cpu.ip,
            (unsigned long long)result.executed_instructions,
            (unsigned long long)reference_cpu.procedure_calls,
            (unsigned long long)reference_cpu.procedure_returns,
            (unsigned long long)ref_steps);
        ++failures;
    }

    native_status = vf2_hybrid_player_28918_execute_for_test(
        &native_machine, &native_cpu);
    if (native_status != VF2_OK || native_cpu.ip != PLAYER_28918_RETURN) {
        fprintf(
            stderr,
            "player-28918-live fighter=%08x counter=%u native status=%d "
            "ip=%08x\n",
            (unsigned)fighter, (unsigned)counter, (int)native_status,
            (unsigned)native_cpu.ip);
        ++failures;
    }
    if (native_status == VF2_OK &&
        (native_cpu.executed_instructions != result.executed_instructions ||
         native_cpu.procedure_calls != reference_cpu.procedure_calls ||
         native_cpu.procedure_returns != reference_cpu.procedure_returns)) {
        fprintf(
            stderr,
            "player-28918-live fighter=%08x counter=%u counter-lockstep "
            "native steps=%llu calls=%llu rets=%llu ref steps=%llu "
            "calls=%llu rets=%llu\n",
            (unsigned)fighter, (unsigned)counter,
            (unsigned long long)native_cpu.executed_instructions,
            (unsigned long long)native_cpu.procedure_calls,
            (unsigned long long)native_cpu.procedure_returns,
            (unsigned long long)result.executed_instructions,
            (unsigned long long)reference_cpu.procedure_calls,
            (unsigned long long)reference_cpu.procedure_returns);
        ++failures;
    }
    printf(
        "player-28918-live fighter=%08x counter=%u ref=%llu native=%llu\n",
        (unsigned)fighter, (unsigned)counter,
        (unsigned long long)result.executed_instructions,
        (unsigned long long)native_cpu.executed_instructions);

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-28918-live fighter=%08x counter=%u ref=%d native=%d "
            "compare=%d component=%s offset=%zu expected=0x%08x "
            "actual=0x%08x\n",
            (unsigned)fighter, (unsigned)counter, (int)reference_status,
            (int)native_status, (int)compare_status, diff.component,
            diff.first_offset, (unsigned)diff.expected_value,
            (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
    vf2_i960_snapshot_destroy(&snap);
}

static void run_rom_differential(const char *rom_directory)
{
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;

    CHECK(
        vf2_romset_build_region(
            rom_directory, VF2_REGION_MAINCPU, &main_rom, &main_rom_size
        ) == VF2_OK
    );
    CHECK(
        vf2_romset_build_region(
            rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size
        ) == VF2_OK
    );
    if (main_rom == NULL || main_data == NULL) {
        free(main_rom);
        free(main_data);
        ++failures;
        return;
    }

    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 UINT32_C(0x00510980), 2u, UINT64_C(1088));
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 UINT32_C(0x00512980), 2u, UINT64_C(1920));
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 UINT32_C(0x00510980), 0u, UINT64_C(1058));
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 UINT32_C(0x00510980), 30u, UINT64_C(1060));

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(
                stderr, "%d player-28918-live unit test(s) failed\n",
                failures);
            return EXIT_FAILURE;
        }
        puts("player-28918-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-28918-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-28918-live differential tests passed");
    return EXIT_SUCCESS;
}
