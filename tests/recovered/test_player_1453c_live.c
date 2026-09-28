/* ROM-backed differential fixture for the fa_rob state-27/state-16 arms
 * 0x1453c/0x14570 (v0404/v0415/v0416/v0430).
 *
 * The 0x1453c arm is the fa_rob fighter-state exchange reached at 0x14528
 * when the state-27 fighter's +0x197 == 27.  The second-fighter shape first
 * swaps g7/g8 at 0x14530..0x14538. Both paths set that fighter's +0x197
 * to 16, walks a type-5 record chain via +0x194(g7) (0x1ab34, g1 == 5),
 * stores the computed +0x194(g8) and +0x822(g8), clears bit 21 of
 * +0x1a4(g8), flips bit 6 of (g8), then rejoins the 0x14628 common exit.
 *
 * The v0403 blocker was that the live park's +0x194 low half is 0, which
 * indexes an unmapped chain and faults in the walker.  This fixture restores
 * out/park-1442c.vf2snap, jumps to the 0x14528 entry with the state-27 shape
 * and a measured +0x194 that indexes a valid type-5 chain (0x73), then runs
 * the reference interpreter and the native helper to ip == 0x1463c and
 * asserts exact step/call/ret lockstep plus full live-state equality
 * (registers/CC/AC/frames/Work-RAM). The measured cases are 52/56 steps for
 * state 27, 52/55 steps for the asymmetric state-16 joins, 54/58 steps for
 * the board-controlled both-state-16 joins, and 55 steps for the measured
 * direct first-scaling state-16 variant. The state-24/state-16 swapped join
 * also measures 55 steps. The board-bit-9-clear text tail is
 * rerun for the 34 non-baseline state/scaling shapes and must add 74 steps
 * plus one call/return with full live-state equality.
 *
 * Witness (measured, current build):
 *   vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
 *     --set-ip 0x14528 --set-reg g7=0x510980 --set-reg g8=0x512980 \
 *     --set-reg r7=27 --set-reg r8=0 --set-reg r10=0x510980 \
 *     --set-reg r11=0x512980 --set-u16 0x510B14=0x73 --until 0x1463c
 *     -> 52 steps, +1 call / +1 return (the 0x1ab34 walker).
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

#define STATE27_ENTRY UINT32_C(0x00014528)
#define STATE27_RETURN UINT32_C(0x0001463c)

#define REF_TOTAL UINT64_C(52)
#define REF_CALLS UINT64_C(1)
#define REF_RETS UINT64_C(1)

#define TYPE5_INDEX UINT16_C(0x0073)
#define CASE_STATE27_DIRECT 0
#define CASE_STATE27_SWAPPED 1
#define CASE_STATE16_DIRECT 2
#define CASE_STATE16_SWAPPED 3
#define CASE_STATE16_BOTH_DIRECT 4
#define CASE_STATE16_BOTH_SWAPPED 5
#define CASE_STATE16_DIRECT_SCALE 6
#define CASE_STATE16_DIRECT_SECOND_GATE 7
#define CASE_STATE16_DIRECT_LATER_SCALE 8
#define CASE_STATE16_DIRECT_TEXT 9
#define CASE_STATE27_DIRECT_SCALE 10
#define CASE_STATE27_SWAPPED_SCALE 11
#define CASE_STATE16_SWAPPED_SCALE 12
#define CASE_STATE16_BOTH_DIRECT_SCALE 13
#define CASE_STATE16_BOTH_SWAPPED_SCALE 14
#define CASE_STATE27_DIRECT_SECOND_GATE 15
#define CASE_STATE27_SWAPPED_SECOND_GATE 16
#define CASE_STATE16_SWAPPED_SECOND_GATE 17
#define CASE_STATE16_BOTH_DIRECT_SECOND_GATE 18
#define CASE_STATE16_BOTH_SWAPPED_SECOND_GATE 19
#define CASE_STATE27_DIRECT_LATER_SCALE 20
#define CASE_STATE27_SWAPPED_LATER_SCALE 21
#define CASE_STATE16_SWAPPED_LATER_SCALE 22
#define CASE_STATE16_BOTH_DIRECT_LATER_SCALE 23
#define CASE_STATE16_BOTH_SWAPPED_LATER_SCALE 24
#define CASE_STATE27_DIRECT_MIXED_SHORT 25
#define CASE_STATE27_DIRECT_MIXED_LATER 26
#define CASE_STATE27_SWAPPED_MIXED_SHORT 27
#define CASE_STATE27_SWAPPED_MIXED_LATER 28
#define CASE_STATE16_SWAPPED_MIXED_SHORT 29
#define CASE_STATE16_SWAPPED_MIXED_LATER 30
#define CASE_STATE16_BOTH_DIRECT_MIXED_SHORT 31
#define CASE_STATE16_BOTH_DIRECT_MIXED_LATER 32
#define CASE_STATE16_BOTH_SWAPPED_MIXED_SHORT 33
#define CASE_STATE16_BOTH_SWAPPED_MIXED_LATER 34
#define CASE_STATE24_SWAPPED 35
#define CASE_STATE28_SWAPPED 36
#define CASE_STATE26_SWAPPED 37
#define CASE_STATE26_NEUTRAL 38
#define CASE_STATE16_DIRECT_FAMILY 39

static int failures = 0;
static uint16_t test_type5_index = TYPE5_INDEX;
static uint64_t test_expected_steps_override = 0u;
static int test_expect_unsupported = 0;

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
        vf2_hybrid_player_1453c_execute_for_test(NULL, &cpu) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_1453c_execute_for_test(&machine, NULL) != VF2_OK
    );
}

/* ROM-independent fail-closed checks for the 0x1453c/0x14570 arm. The arm
 * must refuse unknown state shapes (wrong ip, no pushed frame, zero fighter
 * base) before touching memory. */
static void test_unit_fail_closed(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    cpu.local_frame_depth = 1u;

    /* Wrong entry address. */
    cpu.ip = UINT32_C(0x0001442c);
    cpu.registers[7] = 27u;
    cpu.registers[16u + 7u] = 0x510980u;
    cpu.registers[16u + 8u] = 0x512980u;
    cpu.registers[10] = 0x510980u;
    cpu.registers[11] = 0x512980u;
    CHECK(
        vf2_hybrid_player_1453c_execute_for_test(&machine, &cpu) != VF2_OK
    );

    /* Correct ip but r7 != 27. */
    cpu.ip = UINT32_C(0x00014528);
    cpu.registers[7] = 0u;
    CHECK(
        vf2_hybrid_player_1453c_execute_for_test(&machine, &cpu) != VF2_OK
    );

    /* Correct ip and r7 == 27 but no pushed frame. */
    cpu.ip = UINT32_C(0x00014528);
    cpu.registers[7] = 27u;
    cpu.local_frame_depth = 0u;
    CHECK(
        vf2_hybrid_player_1453c_execute_for_test(&machine, &cpu) != VF2_OK
    );
    cpu.local_frame_depth = 1u;
}

static void write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static void write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    CHECK(vf2_model2a_write(machine, address, &value, 1u) == VF2_OK);
}

static void write_u32(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t value
)
{
    const uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u),
        (uint8_t)(value >> 16u),
        (uint8_t)(value >> 24u)
    };
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static void run_rom_case_with_text(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int shape,
    int text_case
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
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint8_t b19b_f1 = 0u;
    uint8_t b197_f1 = 0u;
    uint32_t steps = 0u;
    uint32_t type5_fighter = 0u;
    uint64_t expected_steps = 0u;
    const char *label = "unknown";
    int swapped = 0;
    int both16 = 0;
    int scale_first = 0;
    int second_gate = 0;
    int later_scale = 0;
    int text_branch = 0;
    uint32_t scale_fighter = 0u;
    uint64_t expected_calls = REF_CALLS;
    uint64_t expected_returns = REF_RETS;
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
              "D:/ia/vf2-decomp/out/park-1442c.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/park-1442c.vf2snap") == VF2_OK);
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
    CHECK(vf2_model2a_attach_main_rom(&reference_machine, main_rom,
                                      main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&reference_machine, main_data,
                                        main_data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&native_machine, main_rom,
                                      main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&native_machine, main_data,
                                        main_data_size) == VF2_OK);

    fighter0 = reference_cpu.registers[16u + 7u];
    fighter1 = reference_cpu.registers[16u + 8u];
    CHECK(fighter0 != 0u);
    CHECK(fighter1 != 0u);
    CHECK(vf2_model2a_read(&reference_machine, fighter1 + UINT32_C(0x19b),
                           &b19b_f1, 1u) == VF2_OK);
    CHECK(vf2_model2a_read(&reference_machine, fighter1 + UINT32_C(0x197),
                           &b197_f1, 1u) == VF2_OK);

    switch (shape) {
    case CASE_STATE27_DIRECT:
        reference_cpu.registers[7] = 27u;
        native_cpu.registers[7] = 27u;
        reference_cpu.registers[8] = (uint32_t)b197_f1;
        native_cpu.registers[8] = (uint32_t)b197_f1;
        expected_steps = REF_TOTAL;
        label = "state27-direct";
        break;
    case CASE_STATE27_SWAPPED:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 27u;
        native_cpu.registers[8] = 27u;
        expected_steps = UINT64_C(56);
        swapped = 1;
        label = "state27-swapped";
        break;
    case CASE_STATE16_DIRECT:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 0u;
        native_cpu.registers[8] = 0u;
        expected_steps = UINT64_C(52);
        label = "state16-direct";
        break;
    case CASE_STATE16_SWAPPED:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(55);
        swapped = 1;
        label = "state16-swapped";
        break;
    case CASE_STATE16_BOTH_DIRECT:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(54);
        both16 = 1;
        label = "state16-both-direct";
        break;
    case CASE_STATE16_BOTH_SWAPPED:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(58);
        swapped = 1;
        both16 = 1;
        label = "state16-both-swapped";
        break;
    case CASE_STATE16_DIRECT_SCALE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 0u;
        native_cpu.registers[8] = 0u;
        expected_steps = UINT64_C(55);
        scale_first = 1;
        label = "state16-direct-scale";
        break;
    case CASE_STATE16_DIRECT_SECOND_GATE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 0u;
        native_cpu.registers[8] = 0u;
        expected_steps = UINT64_C(54);
        second_gate = 1;
        label = "state16-direct-second-gate";
        break;
    case CASE_STATE16_DIRECT_LATER_SCALE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 0u;
        native_cpu.registers[8] = 0u;
        expected_steps = UINT64_C(57);
        later_scale = 1;
        label = "state16-direct-later-scale";
        break;
    case CASE_STATE16_DIRECT_TEXT:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 0u;
        native_cpu.registers[8] = 0u;
        expected_steps = UINT64_C(126);
        text_branch = 1;
        expected_calls = UINT64_C(2);
        expected_returns = UINT64_C(2);
        label = "state16-direct-text";
        break;
    case CASE_STATE27_DIRECT_SCALE:
        reference_cpu.registers[7] = 27u;
        native_cpu.registers[7] = 27u;
        reference_cpu.registers[8] = (uint32_t)b197_f1;
        native_cpu.registers[8] = (uint32_t)b197_f1;
        expected_steps = UINT64_C(55);
        scale_first = 1;
        label = "state27-direct-scale";
        break;
    case CASE_STATE27_SWAPPED_SCALE:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 27u;
        native_cpu.registers[8] = 27u;
        expected_steps = UINT64_C(59);
        swapped = 1;
        scale_first = 1;
        label = "state27-swapped-scale";
        break;
    case CASE_STATE16_SWAPPED_SCALE:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(58);
        swapped = 1;
        scale_first = 1;
        label = "state16-swapped-scale";
        break;
    case CASE_STATE16_BOTH_DIRECT_SCALE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(57);
        both16 = 1;
        scale_first = 1;
        label = "state16-both-direct-scale";
        break;
    case CASE_STATE16_BOTH_SWAPPED_SCALE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(61);
        swapped = 1;
        both16 = 1;
        scale_first = 1;
        label = "state16-both-swapped-scale";
        break;
    case CASE_STATE27_DIRECT_SECOND_GATE:
        reference_cpu.registers[7] = 27u;
        native_cpu.registers[7] = 27u;
        reference_cpu.registers[8] = (uint32_t)b197_f1;
        native_cpu.registers[8] = (uint32_t)b197_f1;
        expected_steps = UINT64_C(54);
        second_gate = 1;
        label = "state27-direct-second-gate";
        break;
    case CASE_STATE27_SWAPPED_SECOND_GATE:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 27u;
        native_cpu.registers[8] = 27u;
        expected_steps = UINT64_C(58);
        swapped = 1;
        second_gate = 1;
        label = "state27-swapped-second-gate";
        break;
    case CASE_STATE16_SWAPPED_SECOND_GATE:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(57);
        swapped = 1;
        second_gate = 1;
        label = "state16-swapped-second-gate";
        break;
    case CASE_STATE16_BOTH_DIRECT_SECOND_GATE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(56);
        both16 = 1;
        second_gate = 1;
        label = "state16-both-direct-second-gate";
        break;
    case CASE_STATE16_BOTH_SWAPPED_SECOND_GATE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(60);
        swapped = 1;
        both16 = 1;
        second_gate = 1;
        label = "state16-both-swapped-second-gate";
        break;
    case CASE_STATE27_DIRECT_LATER_SCALE:
        reference_cpu.registers[7] = 27u;
        native_cpu.registers[7] = 27u;
        reference_cpu.registers[8] = (uint32_t)b197_f1;
        native_cpu.registers[8] = (uint32_t)b197_f1;
        expected_steps = UINT64_C(57);
        later_scale = 1;
        label = "state27-direct-later-scale";
        break;
    case CASE_STATE27_SWAPPED_LATER_SCALE:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 27u;
        native_cpu.registers[8] = 27u;
        expected_steps = UINT64_C(61);
        swapped = 1;
        later_scale = 1;
        label = "state27-swapped-later-scale";
        break;
    case CASE_STATE16_SWAPPED_LATER_SCALE:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(60);
        swapped = 1;
        later_scale = 1;
        label = "state16-swapped-later-scale";
        break;
    case CASE_STATE16_BOTH_DIRECT_LATER_SCALE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(59);
        both16 = 1;
        later_scale = 1;
        label = "state16-both-direct-later-scale";
        break;
    case CASE_STATE16_BOTH_SWAPPED_LATER_SCALE:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(63);
        swapped = 1;
        both16 = 1;
        later_scale = 1;
        label = "state16-both-swapped-later-scale";
        break;
    case CASE_STATE27_DIRECT_MIXED_SHORT:
        reference_cpu.registers[7] = 27u;
        native_cpu.registers[7] = 27u;
        reference_cpu.registers[8] = (uint32_t)b197_f1;
        native_cpu.registers[8] = (uint32_t)b197_f1;
        expected_steps = UINT64_C(57);
        scale_first = 1;
        second_gate = 1;
        label = "state27-direct-mixed-short";
        break;
    case CASE_STATE27_DIRECT_MIXED_LATER:
        reference_cpu.registers[7] = 27u;
        native_cpu.registers[7] = 27u;
        reference_cpu.registers[8] = (uint32_t)b197_f1;
        native_cpu.registers[8] = (uint32_t)b197_f1;
        expected_steps = UINT64_C(60);
        scale_first = 1;
        later_scale = 1;
        label = "state27-direct-mixed-later";
        break;
    case CASE_STATE27_SWAPPED_MIXED_SHORT:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 27u;
        native_cpu.registers[8] = 27u;
        expected_steps = UINT64_C(61);
        swapped = 1;
        scale_first = 1;
        second_gate = 1;
        label = "state27-swapped-mixed-short";
        break;
    case CASE_STATE27_SWAPPED_MIXED_LATER:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 27u;
        native_cpu.registers[8] = 27u;
        expected_steps = UINT64_C(64);
        swapped = 1;
        scale_first = 1;
        later_scale = 1;
        label = "state27-swapped-mixed-later";
        break;
    case CASE_STATE16_SWAPPED_MIXED_SHORT:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(60);
        swapped = 1;
        scale_first = 1;
        second_gate = 1;
        label = "state16-swapped-mixed-short";
        break;
    case CASE_STATE16_SWAPPED_MIXED_LATER:
        reference_cpu.registers[7] = 0u;
        native_cpu.registers[7] = 0u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(63);
        swapped = 1;
        scale_first = 1;
        later_scale = 1;
        label = "state16-swapped-mixed-later";
        break;
    case CASE_STATE16_BOTH_DIRECT_MIXED_SHORT:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(59);
        both16 = 1;
        scale_first = 1;
        second_gate = 1;
        label = "state16-both-direct-mixed-short";
        break;
    case CASE_STATE16_BOTH_DIRECT_MIXED_LATER:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(62);
        both16 = 1;
        scale_first = 1;
        later_scale = 1;
        label = "state16-both-direct-mixed-later";
        break;
    case CASE_STATE16_BOTH_SWAPPED_MIXED_SHORT:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(63);
        swapped = 1;
        both16 = 1;
        scale_first = 1;
        second_gate = 1;
        label = "state16-both-swapped-mixed-short";
        break;
    case CASE_STATE16_BOTH_SWAPPED_MIXED_LATER:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(66);
        swapped = 1;
        both16 = 1;
        scale_first = 1;
        later_scale = 1;
        label = "state16-both-swapped-mixed-later";
        break;
    case CASE_STATE24_SWAPPED:
        reference_cpu.registers[7] = 24u;
        native_cpu.registers[7] = 24u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(55);
        swapped = 1;
        label = "state24-swapped";
        break;
    case CASE_STATE28_SWAPPED:
        reference_cpu.registers[7] = 28u;
        native_cpu.registers[7] = 28u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(55);
        swapped = 1;
        label = "state28-swapped";
        break;
    case CASE_STATE26_SWAPPED:
        reference_cpu.registers[7] = 26u;
        native_cpu.registers[7] = 26u;
        reference_cpu.registers[8] = 16u;
        native_cpu.registers[8] = 16u;
        expected_steps = UINT64_C(55);
        swapped = 1;
        label = "state26-swapped";
        break;
    case CASE_STATE26_NEUTRAL:
        reference_cpu.registers[7] = 26u;
        native_cpu.registers[7] = 26u;
        reference_cpu.registers[8] = 1u;
        native_cpu.registers[8] = 1u;
        expected_steps = UINT64_C(9);
        expected_calls = UINT64_C(0);
        expected_returns = UINT64_C(0);
        label = "state26-neutral";
        break;
    case CASE_STATE16_DIRECT_FAMILY:
        reference_cpu.registers[7] = 16u;
        native_cpu.registers[7] = 16u;
        reference_cpu.registers[8] = 26u;
        native_cpu.registers[8] = 26u;
        expected_steps = UINT64_C(52);
        label = "state16-direct-family";
        break;
    default:
        CHECK(0);
        goto cleanup;
    }

    if (text_case) {
        text_branch = 1;
        expected_steps += UINT64_C(74);
        expected_calls = UINT64_C(2);
        expected_returns = UINT64_C(2);
    }
    if (test_expected_steps_override != 0u) {
        expected_steps = test_expected_steps_override == UINT64_MAX
            ? 0u : test_expected_steps_override;
    }

    /* Force the measured direct or swapped state shape on both machines. */
    reference_cpu.ip = STATE27_ENTRY;
    native_cpu.ip = STATE27_ENTRY;
    reference_cpu.registers[10] = fighter0;
    native_cpu.registers[10] = fighter0;
    reference_cpu.registers[11] = fighter1;
    native_cpu.registers[11] = fighter1;
    reference_cpu.registers[14u] = (uint32_t)b19b_f1;
    native_cpu.registers[14u] = (uint32_t)b19b_f1;
    type5_fighter = swapped ? fighter1 : fighter0;
    write_u16(&reference_machine,
              type5_fighter + UINT32_C(0x194), test_type5_index);
    write_u16(&native_machine,
              type5_fighter + UINT32_C(0x194), test_type5_index);
    if (scale_first) {
        scale_fighter = swapped ? fighter0 : fighter1;
        write_u32(&reference_machine, scale_fighter + UINT32_C(0x1a4), 1u);
        write_u32(&native_machine, scale_fighter + UINT32_C(0x1a4), 1u);
    }
    if (second_gate) {
        write_u8(&reference_machine, UINT32_C(0x0059c351), 0x40u);
        write_u8(&native_machine, UINT32_C(0x0059c351), 0x40u);
    }
    if (later_scale) {
        scale_fighter = swapped ? fighter0 : fighter1;
        write_u8(&reference_machine, UINT32_C(0x0059c351), 0x40u);
        write_u8(&native_machine, UINT32_C(0x0059c351), 0x40u);
        write_u32(&reference_machine, scale_fighter,
                  UINT32_C(0x20000000));
        write_u32(&native_machine, scale_fighter,
                  UINT32_C(0x20000000));
    }
    if (text_branch) {
        write_u32(&reference_machine, UINT32_C(0x00508000), 0u);
        write_u32(&native_machine, UINT32_C(0x00508000), 0u);
    }
    if (both16) {
        const uint32_t board28 = swapped ? UINT32_C(1) : UINT32_C(0);
        write_u32(&reference_machine, UINT32_C(0x00500028), board28);
        write_u32(&native_machine, UINT32_C(0x00500028), board28);
    }

    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: step the 0x1453c arm to its 0x1463c ret instruction. */
    steps = 0u;
    while (reference_cpu.ip != STATE27_RETURN && steps < 1024u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        if (!test_expect_unsupported) {
            CHECK(reference_status == VF2_OK);
        }
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    if (test_expect_unsupported) {
        CHECK(reference_cpu.ip != STATE27_RETURN);
        native_status = vf2_hybrid_player_1453c_execute_for_test(
            &native_machine, &native_cpu);
        CHECK(native_status != VF2_OK);
        goto cleanup;
    }
    CHECK(reference_cpu.ip == STATE27_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    if (expected_steps != 0u) {
        CHECK(reference_instructions == expected_steps);
    }
    CHECK(reference_cpu.procedure_calls - snap_calls == expected_calls);
    CHECK(reference_cpu.procedure_returns - snap_returns == expected_returns);

    /* Native: the 0x1453c state-27 arm. */
    native_status = vf2_hybrid_player_1453c_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == STATE27_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    if (expected_steps != 0u) {
        CHECK(native_instructions == expected_steps);
    }
    CHECK(native_instructions == reference_instructions);
    CHECK(native_cpu.procedure_calls - snap_calls == expected_calls);
    CHECK(native_cpu.procedure_returns - snap_returns == expected_returns);

    printf(
        "player-1453c-live %s ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
        label,
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions,
        (unsigned long long)(reference_cpu.procedure_calls - snap_calls),
        (unsigned long long)(native_cpu.procedure_calls - snap_calls),
        (unsigned long long)(reference_cpu.procedure_returns - snap_returns),
        (unsigned long long)(native_cpu.procedure_returns - snap_returns));

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-1453c-live ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

cleanup:
    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
    vf2_i960_snapshot_destroy(&snap);
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int shape
)
{
    run_rom_case_with_text(
        main_rom, main_rom_size, main_data, main_data_size, shape, 0
    );
}

static void run_rom_case_with_type5_index(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int shape,
    uint16_t type5_index,
    uint64_t expected_steps
)
{
    const uint16_t previous_index = test_type5_index;
    const uint64_t previous_steps = test_expected_steps_override;

    test_type5_index = type5_index;
    test_expected_steps_override = expected_steps;
    run_rom_case_with_text(
        main_rom, main_rom_size, main_data, main_data_size, shape, 0
    );
    test_type5_index = previous_index;
    test_expected_steps_override = previous_steps;
}

static void run_rom_case_with_unsupported_type5_index(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint16_t type5_index
)
{
    const uint16_t previous_index = test_type5_index;
    const uint64_t previous_steps = test_expected_steps_override;
    const int previous_expectation = test_expect_unsupported;

    test_type5_index = type5_index;
    test_expected_steps_override = UINT64_MAX;
    test_expect_unsupported = 1;
    run_rom_case_with_text(
        main_rom, main_rom_size, main_data, main_data_size,
        CASE_STATE27_DIRECT, 0
    );
    test_type5_index = previous_index;
    test_expected_steps_override = previous_steps;
    test_expect_unsupported = previous_expectation;
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
                 CASE_STATE27_DIRECT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_SWAPPED);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_DIRECT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_SWAPPED);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_DIRECT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_SWAPPED);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_DIRECT_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_DIRECT_SECOND_GATE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_DIRECT_LATER_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_DIRECT_TEXT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_DIRECT_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_SWAPPED_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_SWAPPED_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_DIRECT_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_SWAPPED_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_DIRECT_SECOND_GATE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_SWAPPED_SECOND_GATE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_SWAPPED_SECOND_GATE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_DIRECT_SECOND_GATE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_SWAPPED_SECOND_GATE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_DIRECT_LATER_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_SWAPPED_LATER_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_SWAPPED_LATER_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_DIRECT_LATER_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_SWAPPED_LATER_SCALE);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_DIRECT_MIXED_SHORT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_DIRECT_MIXED_LATER);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_SWAPPED_MIXED_SHORT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE27_SWAPPED_MIXED_LATER);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_SWAPPED_MIXED_SHORT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_SWAPPED_MIXED_LATER);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_DIRECT_MIXED_SHORT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_DIRECT_MIXED_LATER);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_SWAPPED_MIXED_SHORT);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_BOTH_SWAPPED_MIXED_LATER);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE24_SWAPPED);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE28_SWAPPED);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE26_SWAPPED);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE26_NEUTRAL);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                 CASE_STATE16_DIRECT_FAMILY);
    {
        static const struct {
            uint16_t index;
            uint64_t steps;
        } type5_selector_cases[] = {
            { UINT16_C(1), UINT64_C(62) },
            { UINT16_C(2), UINT64_C(69) },
            { UINT16_C(3), UINT64_C(62) },
            { UINT16_C(4), UINT64_C(62) },
            { UINT16_C(5), UINT64_C(62) },
            { UINT16_C(6), UINT64_C(62) },
            { UINT16_C(7), UINT64_C(61) },
            { UINT16_C(8), UINT64_C(61) },
            { UINT16_C(9), UINT64_C(61) },
            { UINT16_C(10), UINT64_C(61) },
            { UINT16_C(11), UINT64_C(62) },
            { UINT16_C(12), UINT64_C(62) },
            { UINT16_C(13), UINT64_C(52) },
            { UINT16_C(14), UINT64_C(52) },
            { UINT16_C(15), UINT64_C(52) },
            { UINT16_C(16), UINT64_C(55) },
            { UINT16_C(17), UINT64_C(69) },
            { UINT16_C(18), UINT64_C(69) },
            { UINT16_C(19), UINT64_C(69) },
            { UINT16_C(20), UINT64_C(69) },
            { UINT16_C(21), UINT64_C(55) },
            { UINT16_C(22), UINT64_C(52) },
            { UINT16_C(23), UINT64_C(69) },
            { UINT16_C(24), UINT64_C(55) },
            { UINT16_C(25), UINT64_C(52) },
            { UINT16_C(26), UINT64_C(69) },
            { UINT16_C(27), UINT64_C(52) },
            { UINT16_C(28), UINT64_C(55) },
            { UINT16_C(29), UINT64_C(62) },
            { UINT16_C(30), UINT64_C(52) },
            { UINT16_C(31), UINT64_C(62) },
            { UINT16_C(32), UINT64_C(54) },
            { UINT16_C(33), UINT64_C(54) },
            { UINT16_C(34), UINT64_C(54) },
            { UINT16_C(35), UINT64_C(54) },
            { UINT16_C(36), UINT64_C(52) },
            { UINT16_C(37), UINT64_C(62) },
            { UINT16_C(38), UINT64_C(69) },
            { UINT16_C(39), UINT64_C(69) },
            { UINT16_C(40), UINT64_C(55) },
            { UINT16_C(41), UINT64_C(69) },
            { UINT16_C(42), UINT64_C(69) },
            { UINT16_C(43), UINT64_C(52) },
            { UINT16_C(44), UINT64_C(55) },
            { UINT16_C(45), UINT64_C(62) },
            { UINT16_C(46), UINT64_C(69) },
            { UINT16_C(47), UINT64_C(62) },
            { UINT16_C(48), UINT64_C(62) },
            { UINT16_C(49), UINT64_C(62) },
            { UINT16_C(50), UINT64_C(62) },
            { UINT16_C(51), UINT64_C(69) },
            { UINT16_C(52), UINT64_C(69) },
            { UINT16_C(53), UINT64_C(62) },
            { UINT16_C(54), UINT64_C(62) },
            { UINT16_C(55), UINT64_C(69) },
            { UINT16_C(56), UINT64_C(69) },
            { UINT16_C(57), UINT64_C(55) },
            { UINT16_C(58), UINT64_C(76) },
            { UINT16_C(59), UINT64_C(69) },
            { UINT16_C(60), UINT64_C(47) },
            { UINT16_C(61), UINT64_C(47) },
            { UINT16_C(62), UINT64_C(47) },
            { UINT16_C(63), UINT64_C(47) },
            { UINT16_C(64), UINT64_C(47) },
            { UINT16_C(65), UINT64_C(47) },
            { UINT16_C(66), UINT64_C(47) },
            { UINT16_C(67), UINT64_C(54) },
            { UINT16_C(68), UINT64_C(54) },
            { UINT16_C(69), UINT64_C(55) },
            { UINT16_C(70), UINT64_C(54) },
            { UINT16_C(71), UINT64_C(47) },
            { UINT16_C(72), UINT64_C(47) },
            { UINT16_C(73), UINT64_C(47) },
            { UINT16_C(74), UINT64_C(47) },
            { UINT16_C(75), UINT64_C(47) },
            { UINT16_C(76), UINT64_C(62) },
            { UINT16_C(77), UINT64_C(47) },
            { UINT16_C(78), UINT64_C(47) },
            { UINT16_C(79), UINT64_C(52) },
            { UINT16_C(80), UINT64_C(55) },
            { UINT16_C(81), UINT64_C(52) },
            { UINT16_C(82), UINT64_C(62) },
            { UINT16_C(83), UINT64_C(48) },
            { UINT16_C(84), UINT64_C(52) },
            { UINT16_C(85), UINT64_C(69) },
            { UINT16_C(86), UINT64_C(55) },
            { UINT16_C(87), UINT64_C(48) },
            { UINT16_C(88), UINT64_C(55) },
            { UINT16_C(89), UINT64_C(55) },
            { UINT16_C(90), UINT64_C(55) },
            { UINT16_C(91), UINT64_C(55) },
            { UINT16_C(92), UINT64_C(52) },
            { UINT16_C(93), UINT64_C(55) },
            { UINT16_C(94), UINT64_C(69) },
            { UINT16_C(95), UINT64_C(48) },
            { UINT16_C(96), UINT64_C(52) },
            { UINT16_C(97), UINT64_C(69) },
            { UINT16_C(98), UINT64_C(52) },
            { UINT16_C(99), UINT64_C(55) },
            { UINT16_C(100), UINT64_C(62) },
            { UINT16_C(101), UINT64_C(62) },
            { UINT16_C(102), UINT64_C(62) },
            { UINT16_C(103), UINT64_C(54) },
            { UINT16_C(104), UINT64_C(62) },
            { UINT16_C(105), UINT64_C(90) },
            { UINT16_C(106), UINT64_C(62) },
            { UINT16_C(107), UINT64_C(90) },
            { UINT16_C(108), UINT64_C(76) },
            { UINT16_C(109), UINT64_C(52) },
            { UINT16_C(110), UINT64_C(62) },
            { UINT16_C(111), UINT64_C(52) },
            { UINT16_C(112), UINT64_C(69) },
            { UINT16_C(113), UINT64_C(69) },
            { UINT16_C(114), UINT64_C(61) },
            { UINT16_C(115), UINT64_C(52) },
            { UINT16_C(116), UINT64_C(52) },
            { UINT16_C(117), UINT64_C(55) },
            { UINT16_C(118), UINT64_C(52) },
            { UINT16_C(119), UINT64_C(55) },
            { UINT16_C(120), UINT64_C(62) },
            { UINT16_C(121), UINT64_C(62) },
            { UINT16_C(122), UINT64_C(62) },
            { UINT16_C(123), UINT64_C(62) },
            { UINT16_C(124), UINT64_C(52) },
            { UINT16_C(125), UINT64_C(55) },
            { UINT16_C(126), UINT64_C(69) },
            { UINT16_C(127), UINT64_C(69) },
            { UINT16_C(128), UINT64_C(55) }
        };
        size_t case_index = 0u;

        for (case_index = 0u;
             case_index < sizeof(type5_selector_cases) /
                 sizeof(type5_selector_cases[0]);
             ++case_index) {
            run_rom_case_with_type5_index(
                main_rom, main_rom_size, main_data, main_data_size,
                CASE_STATE27_DIRECT,
                type5_selector_cases[case_index].index,
                type5_selector_cases[case_index].steps
            );
        }
        for (case_index = 129u; case_index <= 1359u; ++case_index) {
            run_rom_case_with_type5_index(
                main_rom, main_rom_size, main_data, main_data_size,
                CASE_STATE27_DIRECT,
                (uint16_t)case_index,
                UINT64_MAX
            );
        }
        run_rom_case_with_unsupported_type5_index(
            main_rom, main_rom_size, main_data, main_data_size,
            UINT16_C(0x0550)
        );
    }

    {
        static const int text_shapes[] = {
            CASE_STATE27_DIRECT,
            CASE_STATE27_SWAPPED,
            CASE_STATE16_DIRECT,
            CASE_STATE16_SWAPPED,
            CASE_STATE16_BOTH_DIRECT,
            CASE_STATE16_BOTH_SWAPPED,
            CASE_STATE16_DIRECT_SCALE,
            CASE_STATE16_DIRECT_SECOND_GATE,
            CASE_STATE16_DIRECT_LATER_SCALE,
            CASE_STATE27_DIRECT_SCALE,
            CASE_STATE27_SWAPPED_SCALE,
            CASE_STATE16_SWAPPED_SCALE,
            CASE_STATE16_BOTH_DIRECT_SCALE,
            CASE_STATE16_BOTH_SWAPPED_SCALE,
            CASE_STATE27_DIRECT_SECOND_GATE,
            CASE_STATE27_SWAPPED_SECOND_GATE,
            CASE_STATE16_SWAPPED_SECOND_GATE,
            CASE_STATE16_BOTH_DIRECT_SECOND_GATE,
            CASE_STATE16_BOTH_SWAPPED_SECOND_GATE,
            CASE_STATE27_DIRECT_LATER_SCALE,
            CASE_STATE27_SWAPPED_LATER_SCALE,
            CASE_STATE16_SWAPPED_LATER_SCALE,
            CASE_STATE16_BOTH_DIRECT_LATER_SCALE,
            CASE_STATE16_BOTH_SWAPPED_LATER_SCALE,
            CASE_STATE27_DIRECT_MIXED_SHORT,
            CASE_STATE27_DIRECT_MIXED_LATER,
            CASE_STATE27_SWAPPED_MIXED_SHORT,
            CASE_STATE27_SWAPPED_MIXED_LATER,
            CASE_STATE16_SWAPPED_MIXED_SHORT,
            CASE_STATE16_SWAPPED_MIXED_LATER,
            CASE_STATE16_BOTH_DIRECT_MIXED_SHORT,
            CASE_STATE16_BOTH_DIRECT_MIXED_LATER,
            CASE_STATE16_BOTH_SWAPPED_MIXED_SHORT,
            CASE_STATE16_BOTH_SWAPPED_MIXED_LATER,
            CASE_STATE24_SWAPPED,
            CASE_STATE28_SWAPPED,
            CASE_STATE26_SWAPPED,
            CASE_STATE16_DIRECT_FAMILY
        };
        size_t i;

        for (i = 0u; i < sizeof(text_shapes) / sizeof(text_shapes[0]); ++i) {
            run_rom_case_with_text(
                main_rom, main_rom_size, main_data, main_data_size,
                text_shapes[i], 1
            );
        }
    }

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();
    test_unit_fail_closed();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(
                stderr, "%d player-1453c-live unit test(s) failed\n",
                failures);
            return EXIT_FAILURE;
        }
        puts("player-1453c-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-1453c-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-1453c-live differential tests passed");
    return EXIT_SUCCESS;
}
