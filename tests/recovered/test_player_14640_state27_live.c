/* ROM-backed differential fixture for the fa_rob state-27 arm 0x14640
 * (v0405/v0425/v0432).
 *
 * The 0x14640 arm is the fa_rob fighter-state helper reached when the
 * fighter g7 has +0x197 == 27 and +0x198 == 0. The original shape has
 * +0x654 == 0; the compare-prefix sibling has +0x654 != 0 and unequal
 * signed +0x1aa/+0x62a values. Both fall through to the 0x14664 type-15 walk. It indexes
 * a type-15 record chain via +0x194(g7) (0x1ab34, g1 == 15), stores
 * r4 = s16(+1(record)) - 1 into +0x62a(g7), moves the original full
 * +0x194(g7) u32 into +0x654(g7) and clears +0x194(g7).
 *
 * This fixture restores out/park-1442c.vf2snap, jumps to the 0x14640 entry
 * with the state-27 shape (fighter0 +0x197 = 27, +0x194 = 0x73 indexing a
 * valid type-15 chain, bit 20 of 0x500068 clear), then runs the reference
 * interpreter and the native helper to ip == 0x146c4 and asserts exact
 * step/call/ret lockstep plus full live-state equality
 * (registers/CC/AC/frames/Work-RAM).
 *
 * Witness (measured, current build):
 *   vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
 *     --set-ip 0x14640 --set-reg g7=0x510980 --set-reg g8=0x512980 \
 *     --set-u8 0x510B17=27 --set-u16 0x510B14=0x73 --until 0x146c4
 *     -> 41 steps, +1 call / +1 return (the 0x1ab34 type-15 walker).
 *
 * v0432 also pins the board-bit-20 shift sibling at 45 steps.
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

#define STATE27_ENTRY UINT32_C(0x00014640)
#define STATE27_RETURN UINT32_C(0x000146c4)

#define REF_TOTAL UINT64_C(41)
#define REF_COMPARE_PREFIX_TOTAL UINT64_C(44)
#define REF_SHIFT_TOTAL UINT64_C(45)
#define REF_CALLS UINT64_C(1)
#define REF_RETS UINT64_C(1)

#define TYPE15_INDEX UINT16_C(0x0073)

static int failures = 0;
static uint16_t test_type15_index = TYPE15_INDEX;
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
        vf2_hybrid_player_14640_state27_execute_for_test(NULL, &cpu) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_14640_state27_execute_for_test(&machine, NULL) !=
        VF2_OK
    );
}

/* ROM-independent fail-closed checks for the 0x14640 state-27 arm.  The arm
 * must refuse any non-state-27 shape (wrong ip, +0x198 != 0,
 * +0x197 != 27, zero fighter base, no pushed frame) before touching memory. */
static void test_unit_fail_closed(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    cpu.local_frame_depth = 1u;
    cpu.registers[16u + 7u] = 0x510980u;

    /* Wrong entry address. */
    cpu.ip = UINT32_C(0x0001442c);
    CHECK(
        vf2_hybrid_player_14640_state27_execute_for_test(&machine, &cpu) !=
        VF2_OK
    );

    /* Correct ip but zero fighter base. */
    cpu.ip = UINT32_C(0x00014640);
    cpu.registers[16u + 7u] = 0u;
    CHECK(
        vf2_hybrid_player_14640_state27_execute_for_test(&machine, &cpu) !=
        VF2_OK
    );
    cpu.registers[16u + 7u] = 0x510980u;

    /* Correct ip and fighter base but no pushed frame. */
    cpu.local_frame_depth = 0u;
    CHECK(
        vf2_hybrid_player_14640_state27_execute_for_test(&machine, &cpu) !=
        VF2_OK
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

static void write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    CHECK(vf2_model2a_write(machine, address, &value, 1u) == VF2_OK);
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int compare_shape
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
    uint64_t expected_instructions = 0u;
    uint32_t fighter0 = 0u;
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
    CHECK(fighter0 != 0u);

    /* Force the measured state-27 shape on both machines: +0x197 == 27 and
     * +0x194 indexes a valid type-15 chain (0x73). */
    reference_cpu.ip = STATE27_ENTRY;
    native_cpu.ip = STATE27_ENTRY;
    write_u32(&reference_machine, fighter0 + UINT32_C(0x198), 0u);
    write_u32(&native_machine, fighter0 + UINT32_C(0x198), 0u);
    write_u32(&reference_machine, fighter0 + UINT32_C(0x654),
              compare_shape != 0 ? UINT32_C(1) : UINT32_C(0));
    write_u32(&native_machine, fighter0 + UINT32_C(0x654),
              compare_shape != 0 ? UINT32_C(1) : UINT32_C(0));
    write_u16(&reference_machine, fighter0 + UINT32_C(0x1aa),
              (compare_shape == 1 || compare_shape == 3) ? UINT16_C(1) :
              compare_shape == 2 ? UINT16_C(2) : UINT16_C(0));
    write_u16(&native_machine, fighter0 + UINT32_C(0x1aa),
              (compare_shape == 1 || compare_shape == 3) ? UINT16_C(1) :
              compare_shape == 2 ? UINT16_C(2) : UINT16_C(0));
    write_u16(&reference_machine, fighter0 + UINT32_C(0x62a),
              (compare_shape == 1 || compare_shape == 3) ? UINT16_C(2) :
              compare_shape == 2 ? UINT16_C(1) : UINT16_C(0));
    write_u16(&native_machine, fighter0 + UINT32_C(0x62a),
              (compare_shape == 1 || compare_shape == 3) ? UINT16_C(2) :
              compare_shape == 2 ? UINT16_C(1) : UINT16_C(0));
    write_u8(&reference_machine, fighter0 + UINT32_C(0x197), 27u);
    write_u8(&native_machine, fighter0 + UINT32_C(0x197), 27u);
    write_u16(&reference_machine, fighter0 + UINT32_C(0x194), test_type15_index);
    write_u16(&native_machine, fighter0 + UINT32_C(0x194), test_type15_index);
    if (compare_shape == 3) {
        write_u32(&reference_machine, UINT32_C(0x00500068),
                  UINT32_C(1) << 20u);
        write_u32(&native_machine, UINT32_C(0x00500068),
                  UINT32_C(1) << 20u);
    }

    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;
    expected_instructions = compare_shape == 3 ? REF_SHIFT_TOTAL :
        compare_shape != 0 ? REF_COMPARE_PREFIX_TOTAL : REF_TOTAL;
    if (test_expected_steps_override != 0u) {
        expected_instructions = test_expected_steps_override == UINT64_MAX
            ? 0u : test_expected_steps_override;
    }

    /* Reference: step the 0x14640 state-27 arm to its 0x146c4 ret
     * instruction (41 steps / +1 call / +1 return). */
    steps = 0u;
    while (reference_cpu.ip != STATE27_RETURN && steps < 1024u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == STATE27_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    if (expected_instructions != 0u) {
        CHECK(reference_instructions == expected_instructions);
    }
    CHECK(reference_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == REF_RETS);

    if (test_expect_unsupported) {
        native_status = vf2_hybrid_player_14640_state27_execute_for_test(
            &native_machine, &native_cpu);
        CHECK(native_status != VF2_OK);
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        vf2_i960_snapshot_destroy(&snap);
        return;
    }

    /* Native: the 0x14640 state-27 arm. */
    native_status = vf2_hybrid_player_14640_state27_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == STATE27_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    if (expected_instructions != 0u) {
        CHECK(native_instructions == expected_instructions);
    }
    CHECK(native_instructions == reference_instructions);
    CHECK(native_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == REF_RETS);

    printf(
        "player-14640-state27-live ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
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
            "player-14640-state27-live ref=%d native=%d compare=%d "
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

static void run_rom_case_with_type15_index(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint16_t type15_index,
    uint64_t expected_steps
)
{
    const uint16_t previous_index = test_type15_index;
    const uint64_t previous_steps = test_expected_steps_override;

    test_type15_index = type15_index;
    test_expected_steps_override = expected_steps;
    run_rom_case(
        main_rom, main_rom_size, main_data, main_data_size, 0
    );
    test_type15_index = previous_index;
    test_expected_steps_override = previous_steps;
}

static void run_rom_case_with_unsupported_type15_index(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint16_t type15_index
)
{
    const uint16_t previous_index = test_type15_index;
    const uint64_t previous_steps = test_expected_steps_override;
    const int previous_expectation = test_expect_unsupported;

    test_type15_index = type15_index;
    test_expected_steps_override = UINT64_MAX;
    test_expect_unsupported = 1;
    run_rom_case(
        main_rom, main_rom_size, main_data, main_data_size, 0
    );
    test_type15_index = previous_index;
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

    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 0);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 1);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 2);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 3);

    {
        static const struct {
            uint16_t index;
            uint64_t steps;
        } type15_selector_cases[] = {
            { UINT16_C(1), UINT64_C(44) },
            { UINT16_C(2), UINT64_C(51) },
            { UINT16_C(3), UINT64_C(44) },
            { UINT16_C(4), UINT64_C(44) },
            { UINT16_C(5), UINT64_C(44) },
            { UINT16_C(6), UINT64_C(44) },
            { UINT16_C(7), UINT64_C(43) },
            { UINT16_C(8), UINT64_C(43) },
            { UINT16_C(9), UINT64_C(43) },
            { UINT16_C(10), UINT64_C(43) },
            { UINT16_C(11), UINT64_C(44) },
            { UINT16_C(12), UINT64_C(44) },
            { UINT16_C(13), UINT64_C(51) },
            { UINT16_C(14), UINT64_C(51) },
            { UINT16_C(15), UINT64_C(51) },
            { UINT16_C(16), UINT64_C(37) },
            { UINT16_C(17), UINT64_C(51) },
            { UINT16_C(18), UINT64_C(51) },
            { UINT16_C(19), UINT64_C(51) },
            { UINT16_C(20), UINT64_C(51) },
            { UINT16_C(21), UINT64_C(37) },
            { UINT16_C(22), UINT64_C(44) },
            { UINT16_C(23), UINT64_C(51) },
            { UINT16_C(24), UINT64_C(37) },
            { UINT16_C(25), UINT64_C(44) },
            { UINT16_C(26), UINT64_C(51) },
            { UINT16_C(27), UINT64_C(44) },
            { UINT16_C(28), UINT64_C(37) },
            { UINT16_C(29), UINT64_C(44) },
            { UINT16_C(30), UINT64_C(44) },
            { UINT16_C(31), UINT64_C(44) },
            { UINT16_C(32), UINT64_C(36) },
            { UINT16_C(33), UINT64_C(36) },
            { UINT16_C(34), UINT64_C(36) },
            { UINT16_C(35), UINT64_C(36) },
            { UINT16_C(36), UINT64_C(44) },
            { UINT16_C(37), UINT64_C(44) },
            { UINT16_C(38), UINT64_C(51) },
            { UINT16_C(39), UINT64_C(51) },
            { UINT16_C(40), UINT64_C(37) },
            { UINT16_C(41), UINT64_C(51) },
            { UINT16_C(42), UINT64_C(51) },
            { UINT16_C(43), UINT64_C(44) },
            { UINT16_C(44), UINT64_C(37) },
            { UINT16_C(45), UINT64_C(44) },
            { UINT16_C(46), UINT64_C(51) },
            { UINT16_C(47), UINT64_C(44) },
            { UINT16_C(48), UINT64_C(44) },
            { UINT16_C(49), UINT64_C(44) },
            { UINT16_C(50), UINT64_C(44) },
            { UINT16_C(51), UINT64_C(51) },
            { UINT16_C(52), UINT64_C(51) },
            { UINT16_C(53), UINT64_C(44) },
            { UINT16_C(54), UINT64_C(44) },
            { UINT16_C(55), UINT64_C(51) },
            { UINT16_C(56), UINT64_C(51) },
            { UINT16_C(57), UINT64_C(37) },
            { UINT16_C(58), UINT64_C(58) },
            { UINT16_C(59), UINT64_C(51) },
            { UINT16_C(60), UINT64_C(29) },
            { UINT16_C(61), UINT64_C(29) },
            { UINT16_C(62), UINT64_C(29) },
            { UINT16_C(63), UINT64_C(29) },
            { UINT16_C(64), UINT64_C(29) },
            { UINT16_C(65), UINT64_C(29) },
            { UINT16_C(66), UINT64_C(29) },
            { UINT16_C(67), UINT64_C(36) },
            { UINT16_C(68), UINT64_C(36) },
            { UINT16_C(69), UINT64_C(37) },
            { UINT16_C(70), UINT64_C(36) },
            { UINT16_C(71), UINT64_C(29) },
            { UINT16_C(72), UINT64_C(29) },
            { UINT16_C(73), UINT64_C(29) },
            { UINT16_C(74), UINT64_C(29) },
            { UINT16_C(75), UINT64_C(29) },
            { UINT16_C(76), UINT64_C(44) },
            { UINT16_C(77), UINT64_C(29) },
            { UINT16_C(78), UINT64_C(29) },
            { UINT16_C(79), UINT64_C(51) },
            { UINT16_C(80), UINT64_C(37) },
            { UINT16_C(81), UINT64_C(44) },
            { UINT16_C(82), UINT64_C(44) },
            { UINT16_C(83), UINT64_C(30) },
            { UINT16_C(84), UINT64_C(44) },
            { UINT16_C(85), UINT64_C(51) },
            { UINT16_C(86), UINT64_C(37) },
            { UINT16_C(87), UINT64_C(30) },
            { UINT16_C(88), UINT64_C(37) },
            { UINT16_C(89), UINT64_C(37) },
            { UINT16_C(90), UINT64_C(37) },
            { UINT16_C(91), UINT64_C(37) },
            { UINT16_C(92), UINT64_C(44) },
            { UINT16_C(93), UINT64_C(37) },
            { UINT16_C(94), UINT64_C(51) },
            { UINT16_C(95), UINT64_C(30) },
            { UINT16_C(96), UINT64_C(44) },
            { UINT16_C(97), UINT64_C(51) },
            { UINT16_C(98), UINT64_C(44) },
            { UINT16_C(99), UINT64_C(37) },
            { UINT16_C(100), UINT64_C(44) },
            { UINT16_C(101), UINT64_C(44) },
            { UINT16_C(102), UINT64_C(44) },
            { UINT16_C(103), UINT64_C(36) },
            { UINT16_C(104), UINT64_C(44) },
            { UINT16_C(105), UINT64_C(72) },
            { UINT16_C(106), UINT64_C(44) },
            { UINT16_C(107), UINT64_C(72) },
            { UINT16_C(108), UINT64_C(58) },
            { UINT16_C(109), UINT64_C(44) },
            { UINT16_C(110), UINT64_C(44) },
            { UINT16_C(111), UINT64_C(44) },
            { UINT16_C(112), UINT64_C(51) },
            { UINT16_C(113), UINT64_C(51) },
            { UINT16_C(114), UINT64_C(43) },
            { UINT16_C(115), UINT64_C(41) },
            { UINT16_C(116), UINT64_C(51) },
            { UINT16_C(117), UINT64_C(37) },
            { UINT16_C(118), UINT64_C(44) },
            { UINT16_C(119), UINT64_C(37) },
            { UINT16_C(120), UINT64_C(44) },
            { UINT16_C(121), UINT64_C(44) },
            { UINT16_C(122), UINT64_C(44) },
            { UINT16_C(123), UINT64_C(44) },
            { UINT16_C(124), UINT64_C(51) },
            { UINT16_C(125), UINT64_C(37) },
            { UINT16_C(126), UINT64_C(51) },
            { UINT16_C(127), UINT64_C(51) },
            { UINT16_C(128), UINT64_C(37) }
        };
        size_t case_index = 0u;

        for (case_index = 0u;
             case_index < sizeof(type15_selector_cases) /
                 sizeof(type15_selector_cases[0]);
             ++case_index) {
            run_rom_case_with_type15_index(
                main_rom, main_rom_size, main_data, main_data_size,
                type15_selector_cases[case_index].index,
                type15_selector_cases[case_index].steps
            );
        }
        for (case_index = 129u; case_index <= 1024u; ++case_index) {
            run_rom_case_with_type15_index(
                main_rom, main_rom_size, main_data, main_data_size,
                (uint16_t)case_index, UINT64_MAX
            );
        }
        run_rom_case_with_unsupported_type15_index(
            main_rom, main_rom_size, main_data, main_data_size,
            UINT16_C(0x0401)
        );
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
                stderr, "%d player-14640-state27-live unit test(s) failed\n",
                failures);
            return EXIT_FAILURE;
        }
        puts("player-14640-state27-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-14640-state27-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-14640-state27-live differential tests passed");
    return EXIT_SUCCESS;
}
