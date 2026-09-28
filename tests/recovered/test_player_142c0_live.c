/* ROM-backed differential fixture for the live player 0x142c0
 * geometry-expansion body (v0392).
 *
 * This closes the open evidence gap recorded in
 * decomp/i960/notes/player_142c0_v0391.md: the accepted
 * hybrid_execute_player_142c0 body previously had no focused ROM-backed
 * differential fixture.  It is native-chained in the first dispatch and
 * passes native-sixth-dispatch, but the 56-step / +3-call / +3-ret
 * 0x142c0 -> 0x14310 span and the exact command/flag poststate were not
 * pinned by a standalone fixture.
 *
 * Witness (measured, current build):
 *   vf2probe --snapshot out/pre14288.vf2snap \
 *     --until 0x142c0            -> 10869 steps, +10 calls / +10 rets
 *     (1622 corridor + setbit-26 head + 0x270d4 five-slot wrapper +
 *     0x1429c tail).
 *   vf2probe --snapshot at142c0.vf2snap \
 *     --until 0x14310            -> 56 steps, +3 calls / +3 rets.
 *
 *   The head (v0390) leaves g1 == 0 at 0x142c0 on this path (the native
 *   body adds 56 insns, the g1 == 0 case).  The body writes command
 *   0x550000 = 1, the command family at 0x005502c0 + g1*0x10 =
 *   (3, g0, g1, g2), sets bit 21 of player_flags
 *   (F0 0x04000800 -> 0x04002800), runs the 0x4b5d0 table lookup, and
 *   exits ip == 0x14310 with the measured 56-insn / +3-call / +3-ret
 *   span.  At the boundary the ROM leaves r15 == game phase (byte
 *   0x0050002b) and r14 == frame phase (byte 0x00530005, 0 here).
 *
 * The fixture restores the measured live snapshot out/pre14288.vf2snap
 * into both machines and runs the reference interpreter against the
 * native wrappers through 0x14310.  No synthetic work-RAM seed is
 * constructed; ROM/main_data differences versus the frozen snapshot are
 * served by the attached real images.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define PLAYER_142C0_ENTRY UINT32_C(0x00014288)
#define PLAYER_142C0_CORRIDOR_END UINT32_C(0x0001428c)
#define PLAYER_142C0_BODY UINT32_C(0x000142c0)
#define PLAYER_142C0_RETURN UINT32_C(0x00014310)

#define REF_TOTAL_TO_BODY UINT64_C(10869)
#define REF_TOTAL_TO_RETURN UINT64_C(10925)
#define REF_BODY_STEPS UINT64_C(56)
#define REF_BODY_CALLS UINT64_C(3)
#define REF_BODY_RETS UINT64_C(3)

static int failures = 0;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            fprintf(                                                          \
                stderr,                                                       \
                "FAILED %s:%d: %s\n",                                         \
                __FILE__, __LINE__, #expression                               \
            );                                                                \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(
        vf2_hybrid_player_142c0_execute_for_test(NULL, &cpu) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_142c0_execute_for_test(&machine, NULL) != VF2_OK
    );
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int use_g1_one
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    uint64_t reference_instructions = 0u;
    uint64_t native_instructions = 0u;
    uint64_t snap_instructions = 0u;
    uint64_t snap_calls = 0u;
    uint64_t snap_returns = 0u;
    uint32_t steps = 0u;
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
              "D:/ia/vf2-decomp/out/pre14288.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/pre14288.vf2snap") == VF2_OK);
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
    CHECK(reference_cpu.ip == PLAYER_142C0_ENTRY);
    CHECK(native_cpu.ip == PLAYER_142C0_ENTRY);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: step the full corridor through the head to the 0x142c0
     * body entry (10869 steps / +10 calls / +10 rets). */
    steps = 0u;
    while (reference_cpu.ip != PLAYER_142C0_BODY && steps < 32768u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_142C0_BODY);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == REF_TOTAL_TO_BODY);
    CHECK(reference_cpu.procedure_calls - snap_calls == UINT64_C(10));
    CHECK(reference_cpu.procedure_returns - snap_returns == UINT64_C(10));

    /* The measured sibling enters the same body with g1 == 1. */
    if (use_g1_one) {
        reference_cpu.registers[VF2_I960_G0_REGISTER + 1u] = 1u;
    }

    /* Native: corridor (19ef8 -> 0x1428c) + head (1428c -> 0x142c0) to
     * the same body entry. */
    native_status = vf2_hybrid_player_19ef8_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_142C0_CORRIDOR_END);
    if (native_status == VF2_OK &&
        native_cpu.ip == PLAYER_142C0_CORRIDOR_END) {
        native_status = vf2_hybrid_player_1428c_execute_for_test(
            &native_machine, &native_cpu);
        CHECK(native_status == VF2_OK);
        CHECK(native_cpu.ip == PLAYER_142C0_BODY);
    }
    if (use_g1_one) {
        native_cpu.registers[VF2_I960_G0_REGISTER + 1u] = 1u;
    }
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == REF_TOTAL_TO_BODY);
    CHECK(native_cpu.procedure_calls - snap_calls == UINT64_C(10));
    CHECK(native_cpu.procedure_returns - snap_returns == UINT64_C(10));

    /* Full live-state equality at the body entry (both machines reach
     * 0x142c0 from the same parked snapshot). */
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-142c0-live@body ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    /* Reference: step the 0x142c0 body to its 0x14310 return
     * (56 steps / +3 calls / +3 rets). */
    steps = 0u;
    while (reference_cpu.ip != PLAYER_142C0_RETURN && steps < 2048u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_142C0_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions ==
          REF_TOTAL_TO_RETURN - (use_g1_one ? UINT64_C(1) : UINT64_C(0)));
    CHECK(reference_cpu.procedure_calls - snap_calls == UINT64_C(13));
    CHECK(reference_cpu.procedure_returns - snap_returns == UINT64_C(13));

    /* Native: the 0x142c0 body. */
    native_status = vf2_hybrid_player_142c0_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_142C0_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions ==
          REF_TOTAL_TO_RETURN - (use_g1_one ? UINT64_C(1) : UINT64_C(0)));
    CHECK(native_cpu.procedure_calls - snap_calls == UINT64_C(13));
    CHECK(native_cpu.procedure_returns - snap_returns == UINT64_C(13));

    printf(
        "player-142c0-live ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions,
        (unsigned long long)(reference_cpu.procedure_calls - snap_calls),
        (unsigned long long)(native_cpu.procedure_calls - snap_calls),
        (unsigned long long)(reference_cpu.procedure_returns - snap_returns),
        (unsigned long long)(native_cpu.procedure_returns - snap_returns));

    /* Full live-state equality at the body return, covering registers,
     * CC/AC, frames and memory (command 0x550000 family + float triple
     * + bit-21 flag poststate). */
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-142c0-live@return ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
    vf2_i960_snapshot_destroy(&snap);
}

static void run_rom_differential(const char *rom_directory, int use_g1_one)
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
                 use_g1_one);

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();

    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--g1-1") != 0)) {
        if (failures != 0) {
            fprintf(
                stderr, "%d player-142c0-live unit test(s) failed\n",
                failures);
            return EXIT_FAILURE;
        }
        puts("player-142c0-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1], argc == 3);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-142c0-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-142c0-live differential tests passed");
    return EXIT_SUCCESS;
}
