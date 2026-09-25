/* ROM-backed differential fixture for the live player 0x505 corridor
 * 0x14288 -> 0x1428c (v0389) plus the measured 0x1428c head through
 * 0x142c0 (v0390).
 *
 * Witness (measured, current build):
 *   vf2probe --snapshot out/pre14288.vf2snap \
 *     --until 0x1428c            -> 1622 steps, 4 calls / 4 rets
 *   Identical corridor (1622/4/4) on punch10, boot and
 *   player-14288-natres parks: entry +0x1a4 == 0, final +0x1a4 ==
 *   0x200; finals F0 0x04000800 / 0x800 / 0x84000882.
 *   vf2probe --snapshot out/pre14288.vf2snap \
 *     --until 0x142c0            -> 10869 steps, +10 calls / +10 rets
 *     (1622 corridor + 3-insn setbit-26 head + 9235-step 0x270d4
 *     five-slot wrapper + 1 call + 7-insn 0x1429c->0x142c0 tail).
 *   Identical 10869/+10/+10 span on boot and natres parks.
 *
 * Live profile selector is 0x505 (ROM `ldos (g6), g0` at 0x14280; bit 14
 * clear, so `bbc 14` at 0x19f2c skips the clrbit-6/5/21 prologue).  The
 * forced-g0 0x4505 shape of earlier notes never occurs live and faults
 * at the 0x27048 cvtri; it stays fail-closed.
 *
 * Corridor shape (calls: 0x19ef8, 0x1a1e4, 0x26ef0, 0x27130; rets 4):
 *   0x19ef8 prologue (clrbit 9, +0x1a4 -> r7, cmpobe 0,g0 taken,
 *     setbit 11, bbc 14 taken) -> setup `call 0x1a1e4` (16-entry
 *     zeroing, +0x1a4 -> +0xbd4 snapshot, record store, opcode stream
 *     `8 then 0` via 0x1a408/0x1a39c, flag rules at 0x1a320/0x1a32c,
 *     tail at 0x1a3a4/0x1a3d0) ->
 *   scratch `call 0x26ef0` (mode-0-only 20x3 expansion on the live
 *     shape, tail st 0x780/0x784/0x788 off scratch base in +0xbd8) ->
 *   clear/return `call 0x27130` (early path via 0x271c4 on the live
 *     shape) -> tail `ret 0x1428c` via 0x1a118 -> 0x1a134.
 *
 * Head shape (0x1428c -> 0x142c0):
 *   `ld (g7),r15; setbit 26,r15; st r15,(g7)` (F0 0x800 -> 0x4000800,
 *   no CC write) -> `call 0x270d4` (five measured 0x27b5c slots off
 *   record 0x0201c2fc, selectors 0x0505/0x0039/0x00f1/0x00e7/0x00af,
 *   9235 steps / +5 calls / +5 rets, exits EQUAL via the cvtri/stis
 *   cmpdeco tail) -> 0x1429c `lda/st/lda/st/ld/ldob/ldob` tail
 *   (+0x10 = 0x501500, +0x0c = 0x142f4, g0 = +0x640, g1 = +0x04,
 *   g2 = +0x1b0; 7 steps, no calls, no CC write) -> 0x142c0.
 *
 * The fixture restores the measured base, natres and boot snapshots
 * (entry +0x1a4 == 0, CC=NONE) into both machines and runs the reference
 * interpreter against the native corridor; no synthetic work-RAM seed is
 * constructed.
 * ROM/main_data differences versus the frozen snapshots are served by the
 * attached real images.
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

#define PLAYER_4505_ENTRY UINT32_C(0x00014288)
#define PLAYER_4505_RETURN UINT32_C(0x0001428c)
#define PLAYER_4505_HEAD_END UINT32_C(0x000142c0)

static int failures = 0;
static const char *test_snapshot_path = NULL;
static uint32_t test_initial_state_flags = 0u;
static uint32_t test_selector = UINT32_C(0x00000505);
static int test_corridor_only = 0;
static int test_expect_unsupported = 0;
static int test_quadruple_only = 0;
static uint32_t test_quadruple_mask = UINT32_C(0x0000000f);

static uint64_t expected_corridor_delta(void)
{
    uint64_t delta = 0u;

    if ((test_initial_state_flags & (UINT32_C(1) << 5u)) != 0u) {
        delta += UINT64_C(9);
    }
    if ((test_initial_state_flags & (UINT32_C(1) << 6u)) != 0u) {
        delta += UINT64_C(5);
    }
    if ((test_initial_state_flags & (UINT32_C(1) << 21u)) != 0u) {
        delta += UINT64_C(12);
    }
    if ((test_initial_state_flags & (UINT32_C(1) << 23u)) != 0u) {
        delta += UINT64_C(2);
    }
    return delta;
}

static uint64_t expected_corridor_instructions(void)
{
    return test_selector == UINT32_C(0x00000284)
        ? UINT64_C(1804)
        : UINT64_C(1622) + expected_corridor_delta();
}

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
        vf2_hybrid_first_dispatch_task_execute(NULL, &cpu, 0u, NULL) !=
        VF2_OK
    );
    CHECK(
        vf2_hybrid_first_dispatch_task_execute(&machine, NULL, 0u, NULL) !=
        VF2_OK
    );
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
)
{
    /* Both machines restore the measured live snapshot
     * out/pre14288.vf2snap (parked at 0x14288 via the 0x13f08 prefix
     * drive); ROM images are re-attached after restore exactly like
     * the probe harness and the coli snapshot fixtures.  No synthetic
     * work-RAM seed: the snapshot carries the full live entry state
     * (entry +0x1a4 == 0, CC=NONE). */
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snap;
    vf2_hybrid_task_report report;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    uint64_t reference_instructions = 0u;
    uint64_t native_instructions = 0u;
    uint64_t snap_instructions = 0u;
    uint64_t snap_calls = 0u;
    uint64_t snap_returns = 0u;
    uint32_t registry = 0u;
    uint32_t steps = 0u;
    int ok = 0;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&report, 0, sizeof(report));
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
    ok = test_snapshot_path != NULL
        ? vf2_i960_snapshot_read_file(&snap, test_snapshot_path) == VF2_OK
        : (vf2_i960_snapshot_read_file(
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
    reference_cpu.registers[VF2_I960_G0_REGISTER] = test_selector;
    native_cpu.registers[VF2_I960_G0_REGISTER] = test_selector;
    /* Attach order matches the probe harness (ROM images first); the
     * machine borrows the buffers, freed once at the end. */
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
    CHECK(vf2_model2a_write_u32(
              &reference_machine, UINT32_C(0x00510b24),
              test_initial_state_flags) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &native_machine, UINT32_C(0x00510b24),
              test_initial_state_flags) == VF2_OK);
    CHECK(reference_cpu.ip == PLAYER_4505_ENTRY);
    CHECK(native_cpu.ip == PLAYER_4505_ENTRY);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;
    registry = native_cpu.registers[29];

    /* Reference: step the real corridor to its return. */
    reference_instructions = reference_cpu.executed_instructions;
    while (reference_cpu.ip != PLAYER_4505_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_4505_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == expected_corridor_instructions());
    CHECK(reference_cpu.procedure_calls - snap_calls == UINT64_C(4));
    CHECK(reference_cpu.procedure_returns - snap_returns == UINT64_C(4));

    if (test_expect_unsupported) {
        native_status = vf2_hybrid_player_19ef8_execute_for_test(
            &native_machine, &native_cpu
        );
        CHECK(native_status != VF2_OK);
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        vf2_i960_snapshot_destroy(&snap);
        return;
    }

    /* Native: the frozen 0x1428c head (setbit-26 + 27b5c fanout) is a
     * separate, still-unmeasured boundary, so exercise only the
     * recovered 0x14288 -> 0x19ef8 corridor unit directly.  It ends
     * with ip == 0x1428c after 1622 steps / 4 calls / 4 rets. */
    native_instructions = native_cpu.executed_instructions;
    native_status = vf2_hybrid_player_19ef8_execute_for_test(
        &native_machine, &native_cpu);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_4505_RETURN);

    /* Instruction-count lockstep on the live shape.  Both sides start
     * from the parked snapshot, so deltas against it are the corridor
     * counts (1622/4/4). */
    printf(
        "player-4505-live ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions,
        (unsigned long long)(
            reference_cpu.procedure_calls - snap_calls),
        (unsigned long long)(
            native_cpu.procedure_calls - snap_calls),
        (unsigned long long)(
            reference_cpu.procedure_returns - snap_returns),
        (unsigned long long)(
            native_cpu.procedure_returns - snap_returns));
    CHECK(native_instructions == reference_instructions);
    CHECK(native_instructions == expected_corridor_instructions());
    CHECK(native_cpu.procedure_calls - snap_calls == UINT64_C(4));
    CHECK(native_cpu.procedure_returns - snap_returns == UINT64_C(4));

    /* Full live-state equality (registers, CC/AC, frames, memory). */
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-4505-live flags=0x%08x ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (unsigned)test_initial_state_flags,
            (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    if (test_corridor_only) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        vf2_i960_snapshot_destroy(&snap);
        return;
    }

    /* v0390: extend the same parked machines through the measured
     * 0x1428c head to 0x142c0.  The reference runs the real head
     * (setbit-26 + 0x270d4 wrapper + 0x1429c tail); the native side
     * runs the recovered head unit directly (the shared dispatch has
     * no 0x1428c entry case; the chain reaches it internally from
     * 0x14288).  Both must land on 0x142c0 with 10869 total steps /
     * +10 calls / +10 rets and full live-state equality. */
    steps = 0u;
    while (reference_cpu.ip != PLAYER_4505_HEAD_END && steps < 16384u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_4505_HEAD_END);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == UINT64_C(10869));
    CHECK(reference_cpu.procedure_calls - snap_calls == UINT64_C(10));
    CHECK(reference_cpu.procedure_returns - snap_returns == UINT64_C(10));

    native_status = vf2_hybrid_player_1428c_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_4505_HEAD_END);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    printf(
        "player-1428c-head ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions,
        (unsigned long long)(
            reference_cpu.procedure_calls - snap_calls),
        (unsigned long long)(
            native_cpu.procedure_calls - snap_calls),
        (unsigned long long)(
            reference_cpu.procedure_returns - snap_returns),
        (unsigned long long)(
            native_cpu.procedure_returns - snap_returns));
    CHECK(native_instructions == reference_instructions);
    CHECK(native_instructions == UINT64_C(10869));
    CHECK(native_cpu.procedure_calls - snap_calls == UINT64_C(10));
    CHECK(native_cpu.procedure_returns - snap_returns == UINT64_C(10));

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-1428c-head ref=%d native=%d compare=%d "
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

static void run_rom_differential(const char *rom_directory)
{
    /* NOTE: one ROM buffer pair is shared by both machines (attach
     * borrows; shutdown frees solely machine-owned state). */
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

    if (test_quadruple_only) {
        size_t branch_index = 0u;
        test_snapshot_path = NULL;
        test_selector = UINT32_C(0x00000505);
        test_corridor_only = 1;
        test_expect_unsupported = 0;
        for (branch_index = 0u; branch_index < 16u; ++branch_index) {
            const uint32_t branch_bits =
                ((branch_index & 1u) != 0u ? (UINT32_C(1) << 5u) : 0u) |
                ((branch_index & 2u) != 0u ? (UINT32_C(1) << 6u) : 0u) |
                ((branch_index & 4u) != 0u ? (UINT32_C(1) << 21u) : 0u) |
                ((branch_index & 8u) != 0u ? (UINT32_C(1) << 23u) : 0u);
            test_initial_state_flags = test_quadruple_mask | branch_bits;
            run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
        }
        test_expect_unsupported = 1;
        test_initial_state_flags = test_quadruple_mask |
            ((test_quadruple_mask & (UINT32_C(1) << 8u)) != 0u
                 ? ((test_quadruple_mask & (UINT32_C(1) << 10u)) != 0u
                        ? ((test_quadruple_mask & (UINT32_C(1) << 12u)) != 0u
                               ? ((test_quadruple_mask & (UINT32_C(1) << 14u)) != 0u
                               ? ((test_quadruple_mask & (UINT32_C(1) << 15u)) != 0u
                                      ? ((test_quadruple_mask & (UINT32_C(1) << 16u)) != 0u
                                             ? (UINT32_C(1) << 17u)
                                             : (UINT32_C(1) << 16u))
                                      : (UINT32_C(1) << 15u))
                                      : (UINT32_C(1) << 14u))
                               : (UINT32_C(1) << 12u))
                        : (UINT32_C(1) << 10u))
                 : (UINT32_C(1) << 8u));
        run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
        free(main_rom);
        free(main_data);
        return;
    }

    test_snapshot_path = NULL;
    test_initial_state_flags = 0u;
    test_corridor_only = 0;
    test_selector = UINT32_C(0x00000505);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
    test_snapshot_path = "D:/ia/vf2-decomp/out/pre14288-natres.vf2snap";
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
    test_snapshot_path = "D:/ia/vf2-decomp/out/pre14288-boot.vf2snap";
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
    test_snapshot_path = NULL;
    test_selector = UINT32_C(0x00000284);
    test_corridor_only = 1;
    test_initial_state_flags = 0u;
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
    test_selector = UINT32_C(0x00000505);
    test_corridor_only = 1;
    {
        const uint32_t measured_state_masks[] = {
            UINT32_C(1) << 5u,
            UINT32_C(1) << 6u,
            UINT32_C(1) << 21u,
            UINT32_C(1) << 23u,
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 6u),
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 21u),
            (UINT32_C(1) << 6u) | (UINT32_C(1) << 21u),
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 6u) |
                (UINT32_C(1) << 21u),
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 23u),
            (UINT32_C(1) << 6u) | (UINT32_C(1) << 23u),
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 6u) |
                (UINT32_C(1) << 23u),
            (UINT32_C(1) << 21u) | (UINT32_C(1) << 23u),
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 21u) |
                (UINT32_C(1) << 23u),
            (UINT32_C(1) << 6u) | (UINT32_C(1) << 21u) |
                (UINT32_C(1) << 23u),
            (UINT32_C(1) << 5u) | (UINT32_C(1) << 6u) |
                (UINT32_C(1) << 21u) | (UINT32_C(1) << 23u)
        };
        size_t index = 0u;
        for (index = 0u;
             index < sizeof(measured_state_masks) /
                         sizeof(measured_state_masks[0]);
             ++index) {
            test_initial_state_flags = measured_state_masks[index];
            run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
        }
        for (index = 0u; index < 32u; ++index) {
            if (index == 5u || index == 6u || index == 21u || index == 23u) {
                continue;
            }
            test_initial_state_flags = UINT32_C(1) << index;
            run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
        }
        /* v0559: every measured non-branch pair and triple composes with
         * every subset of the four branch bits; larger combinations remain. */
        for (index = 0u; index < 32u; ++index) {
            size_t branch_index = 0u;
            uint32_t non_branch_bit = UINT32_C(1) << index;
            if (index == 5u || index == 6u || index == 21u || index == 23u) {
                continue;
            }
            for (branch_index = 0u; branch_index < 16u; ++branch_index) {
                const uint32_t branch_bits =
                    ((branch_index & 1u) != 0u ? (UINT32_C(1) << 5u) : 0u) |
                    ((branch_index & 2u) != 0u ? (UINT32_C(1) << 6u) : 0u) |
                    ((branch_index & 4u) != 0u ? (UINT32_C(1) << 21u) : 0u) |
                    ((branch_index & 8u) != 0u ? (UINT32_C(1) << 23u) : 0u);
                test_initial_state_flags = non_branch_bit | branch_bits;
                run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
            }
        }
        for (index = 0u; index < 32u; ++index) {
            size_t second_index = 0u;
            const uint32_t first_bit = UINT32_C(1) << index;
            if (index == 5u || index == 6u || index == 21u || index == 23u) {
                continue;
            }
            for (second_index = index + 1u; second_index < 32u;
                 ++second_index) {
                size_t branch_index = 0u;
                const uint32_t second_bit = UINT32_C(1) << second_index;
                if (second_index == 5u || second_index == 6u ||
                    second_index == 21u || second_index == 23u) {
                    continue;
                }
                for (branch_index = 0u; branch_index < 16u; ++branch_index) {
                    const uint32_t branch_bits =
                        ((branch_index & 1u) != 0u ? (UINT32_C(1) << 5u) : 0u) |
                        ((branch_index & 2u) != 0u ? (UINT32_C(1) << 6u) : 0u) |
                        ((branch_index & 4u) != 0u ? (UINT32_C(1) << 21u) : 0u) |
                        ((branch_index & 8u) != 0u ? (UINT32_C(1) << 23u) : 0u);
                    test_initial_state_flags =
                        first_bit | second_bit | branch_bits;
                    run_rom_case(
                        main_rom, main_rom_size, main_data, main_data_size
                    );
                    test_initial_state_flags |= UINT32_C(1) << 5u;
                    run_rom_case(
                        main_rom, main_rom_size, main_data, main_data_size
                    );
                }
            }
        }
        for (index = 0u; index < 32u; ++index) {
            size_t second_index = 0u;
            const uint32_t first_bit = UINT32_C(1) << index;
            if (index == 5u || index == 6u || index == 21u || index == 23u) {
                continue;
            }
            for (second_index = index + 1u; second_index < 32u;
                 ++second_index) {
                size_t third_index = 0u;
                size_t branch_index = 0u;
                const uint32_t second_bit = UINT32_C(1) << second_index;
                if (second_index == 5u || second_index == 6u ||
                    second_index == 21u || second_index == 23u) {
                    continue;
                }
                for (third_index = second_index + 1u; third_index < 32u;
                     ++third_index) {
                    if (third_index == 5u || third_index == 6u ||
                        third_index == 21u || third_index == 23u) {
                        continue;
                    }
                    for (branch_index = 0u; branch_index < 16u;
                         ++branch_index) {
                        const uint32_t branch_bits =
                            ((branch_index & 1u) != 0u ? (UINT32_C(1) << 5u) : 0u) |
                            ((branch_index & 2u) != 0u ? (UINT32_C(1) << 6u) : 0u) |
                            ((branch_index & 4u) != 0u ? (UINT32_C(1) << 21u) : 0u) |
                            ((branch_index & 8u) != 0u ? (UINT32_C(1) << 23u) : 0u);
                        test_initial_state_flags =
                            first_bit | second_bit |
                            (UINT32_C(1) << third_index) | branch_bits;
                        run_rom_case(
                            main_rom, main_rom_size, main_data, main_data_size
                        );
                    }
                }
            }
        }
        test_expect_unsupported = 0;
        test_initial_state_flags =
            (UINT32_C(1) << 0u) | (UINT32_C(1) << 1u) |
            (UINT32_C(1) << 2u) | (UINT32_C(1) << 3u);
        run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
        test_expect_unsupported = 1;
        test_initial_state_flags =
            (UINT32_C(1) << 0u) | (UINT32_C(1) << 1u) |
            (UINT32_C(1) << 2u) | (UINT32_C(1) << 3u) |
            (UINT32_C(1) << 4u);
        run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
        test_expect_unsupported = 0;
    }
    test_initial_state_flags = 0u;
    test_selector = UINT32_C(0x00000505);
    test_corridor_only = 0;
    test_expect_unsupported = 0;
    test_snapshot_path = NULL;

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();

    if (argc != 2 && argc != 3) {
        if (failures != 0) {
            fprintf(
                stderr, "%d player-4505-live unit test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("player-4505-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    if (argc == 3 && strcmp(argv[2], "--quadruple") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000000f);
    } else if (argc == 3 && strcmp(argv[2], "--quadruple-17") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x00000017);
    } else if (argc == 3 && strcmp(argv[2], "--quadruple-1b") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000001b);
    } else if (argc == 3 && strcmp(argv[2], "--quadruple-1d") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000001d);
    } else if (argc == 3 && strcmp(argv[2], "--quadruple-1e") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000001e);
    } else if (argc == 3 && strcmp(argv[2], "--five-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000001f);
    } else if (argc == 3 && strcmp(argv[2], "--six-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000009f);
    } else if (argc == 3 && strcmp(argv[2], "--seven-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000019f);
    } else if (argc == 3 && strcmp(argv[2], "--eight-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0000059f);
    } else if (argc == 3 && strcmp(argv[2], "--nine-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0001059f);
    } else if (argc == 3 && strcmp(argv[2], "--ten-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0005059f);
    } else if (argc == 3 && strcmp(argv[2], "--eleven-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0008059f);
    } else if (argc == 3 && strcmp(argv[2], "--twelve-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0018059f);
    } else if (argc == 3 && strcmp(argv[2], "--fourteen-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0040059f);
    } else if (argc == 3 && strcmp(argv[2], "--fifteen-low") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0100059f);
    } else if (argc == 3 && strcmp(argv[2], "--mask-0200059f") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0200059f);
    } else if (argc == 3 && strcmp(argv[2], "--mask-0400059f") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0400059f);
    } else if (argc == 3 && strcmp(argv[2], "--mask-0800059f") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x0800059f);
    } else if (argc == 3 && strcmp(argv[2], "--mask-1000059f") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x1000059f);
    } else if (argc == 3 && strcmp(argv[2], "--mask-4000059f") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x4000059f);
    } else if (argc == 3 && strcmp(argv[2], "--mask-8000059f") == 0) {
        test_quadruple_only = 1;
        test_quadruple_mask = UINT32_C(0x8000059f);
    } else if (argc == 3) {
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-4505-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-4505-live differential tests passed");
    return EXIT_SUCCESS;
}
