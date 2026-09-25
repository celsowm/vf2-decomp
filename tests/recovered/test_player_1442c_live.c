/* ROM-backed differential fixture for the live fa_rob fighter-exchange
 * body 0x1442c (v0393).
 *
 * This is the first native recovery of the fa_rob collision/state-exchange
 * function that immediately follows the recovered player corridor.  The
 * 0x1442c function is called at 0x14388 when the instance byte +0x04(g7)
 * == 0 and runs the fighter-vs-fighter geometry exchange (two 0x14640
 * no-op helper calls with swapped g7/g8, then the neutral-state fast path
 * to the 0x14628 common exit, clearing both fighters' +0x198).
 *
 * Witness (measured, current build):
 *   vf2probe --rom-dir roms/vf2 --snapshot out/pre14288.vf2snap \
 *     --until 0x1442c            -> 10949 steps, ip == 0x1442c
 *   vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
 *     --until 0x1463c            -> 51 steps, +2 calls / +2 rets, ip ==
 *     0x1463c (the two 0x14640 rets; the 0x1463c ret itself is not
 *     consumed because --until stops at the address).
 *
 * The fixture restores the measured live snapshot out/park-1442c.vf2snap
 * into both machines and runs the reference interpreter against the native
 * wrapper to ip == 0x1463c, then asserts exact step/call/ret lockstep and
 * full live-state (registers/CC/AC/frames/Work-RAM) equality.  No
 * synthetic work-RAM seed is constructed; ROM/main_data differences versus
 * the frozen snapshot are served by the attached real images.
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

#define PLAYER_1442C_ENTRY UINT32_C(0x0001442c)
#define PLAYER_1442C_RETURN UINT32_C(0x0001463c)

#define REF_TOTAL UINT64_C(51)
#define REF_CALLS UINT64_C(2)
#define REF_RETS UINT64_C(2)

/* 0x14640 +0x194 != 0 sibling (v0394): restores out/park-14640-sib.vf2snap
 * (parked at 0x14640, g7 = fighter0, +0x194 = 1) and proves the native
 * helper byte-exact to the 0x1438c return: 14 steps / +1 return. */
#define SIB_ENTRY UINT32_C(0x00014640)
#define SIB_RETURN UINT32_C(0x0001438c)
#define SIB_TOTAL UINT64_C(14)
#define SIB_CALLS UINT64_C(0)
#define SIB_RETS UINT64_C(1)

/* 0x144b0 state-25 arm (v0395): restores out/park-1442c-s25.vf2snap
 * (parked at 0x144b0, fighter0 +0x197 == 25, +0x194 == 0) and proves the
 * native arm byte-exact to the 0x1463c boundary: 53 steps / +1 call /
 * +1 return (the 0x19ef8 call). */
#define S25_ENTRY UINT32_C(0x000144b0)
#define S25_RETURN UINT32_C(0x0001463c)
#define S25_TOTAL UINT64_C(53)
#define S25_CALLS UINT64_C(1)
#define S25_RETS UINT64_C(1)

/* 0x144b0 state-25 cmpobl-not-taken sibling (v0397): restores
 * out/park-1442c-s25-nt.vf2snap (parked at 0x144b0, same state-25 shape
 * but fighter1 +0x808 == 1 and +0x1aa == 100, so r13=100 > r3=0 and the
 * 0x1450c cmpobl does not take).  The native arm stores r5 to +0x194(f1)
 * and rejoins the 0x14628 exit: 47 steps / +1 call / +1 return. */
#define S25_NT_ENTRY UINT32_C(0x000144b0)
#define S25_NT_RETURN UINT32_C(0x0001463c)
#define S25_NT_TOTAL UINT64_C(47)
#define S25_NT_CALLS UINT64_C(1)
#define S25_NT_RETS UINT64_C(1)

/* v0433: state-25 -> state-16 reaches the shared swapped 0x14570 body. */
#define S25_16_TOTAL UINT64_C(99)
#define S25_16_CALLS UINT64_C(2)
#define S25_16_RETS UINT64_C(2)

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

static void write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    CHECK(vf2_model2a_write(machine, address, &value, 1u) == VF2_OK);
}

static void write_u16(vf2_model2a *machine, uint32_t address, uint16_t value)
{
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static void write_u32(vf2_model2a *machine, uint32_t address, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)value, (uint8_t)(value >> 8u),
        (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)
    };
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(
        vf2_hybrid_player_1442c_execute_for_test(NULL, &cpu) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_1442c_execute_for_test(&machine, NULL) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_14640_execute_for_test(NULL, &cpu) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_14640_execute_for_test(&machine, NULL) != VF2_OK
    );
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int state25_successor,
    uint8_t state25_f0_19f
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
    if (state25_successor != 0) {
        const uint32_t fighter0 = reference_cpu.registers[16u + 7u];
        const uint32_t fighter1 = reference_cpu.registers[16u + 8u];
        CHECK(fighter0 != 0u);
        CHECK(fighter1 != 0u);
        write_u16(&reference_machine, fighter1 + UINT32_C(0x194), 0x0073u);
        write_u16(&native_machine, fighter1 + UINT32_C(0x194), 0x0073u);
        write_u8(&reference_machine, fighter0 + UINT32_C(0x197), 25u);
        write_u8(&native_machine, fighter0 + UINT32_C(0x197), 25u);
        write_u8(&reference_machine, fighter0 + UINT32_C(0x19f),
                 state25_f0_19f);
        write_u8(&native_machine, fighter0 + UINT32_C(0x19f),
                 state25_f0_19f);
        write_u8(&reference_machine, fighter1 + UINT32_C(0x197),
                 (uint8_t)state25_successor);
        write_u8(&native_machine, fighter1 + UINT32_C(0x197),
                 (uint8_t)state25_successor);
        write_u8(&reference_machine, fighter0 + UINT32_C(0x19b), 0u);
        write_u8(&native_machine, fighter0 + UINT32_C(0x19b), 0u);
        write_u8(&reference_machine, fighter1 + UINT32_C(0x19b), 0u);
        write_u8(&native_machine, fighter1 + UINT32_C(0x19b), 0u);
        write_u32(&reference_machine, fighter0 + UINT32_C(0x198), 0u);
        write_u32(&native_machine, fighter0 + UINT32_C(0x198), 0u);
        write_u32(&reference_machine, fighter1 + UINT32_C(0x198), 0u);
        write_u32(&native_machine, fighter1 + UINT32_C(0x198), 0u);
        write_u32(&reference_machine, fighter0 + UINT32_C(0x654), 0u);
        write_u32(&native_machine, fighter0 + UINT32_C(0x654), 0u);
        write_u32(&reference_machine, fighter1 + UINT32_C(0x654), 0u);
        write_u32(&native_machine, fighter1 + UINT32_C(0x654), 0u);
        write_u16(&reference_machine, fighter1 + UINT32_C(0x1aa), 0u);
        write_u16(&native_machine, fighter1 + UINT32_C(0x1aa), 0u);
        write_u16(&reference_machine, fighter0 + UINT32_C(0x858), 0u);
        write_u16(&native_machine, fighter0 + UINT32_C(0x858), 0u);
        write_u16(&reference_machine, fighter1 + UINT32_C(0x808), 2u);
        write_u16(&native_machine, fighter1 + UINT32_C(0x808), 2u);
    }
    CHECK(reference_cpu.ip == PLAYER_1442C_ENTRY);
    CHECK(native_cpu.ip == PLAYER_1442C_ENTRY);
    CHECK(reference_cpu.local_frame_depth > 0u);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: step the 0x1442c body to its 0x1463c ret instruction
     * (51 steps / +2 calls / +2 rets). */
    steps = 0u;
    while (reference_cpu.ip != PLAYER_1442C_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_1442C_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions ==
          (state25_successor == 16 ? UINT64_C(144) :
           state25_successor == 28 ? UINT64_C(92) :
           (state25_successor >= 17 && state25_successor <= 31 &&
            state25_successor != 24 && state25_successor != 27) ? UINT64_C(98) :
           state25_successor == 24 && state25_f0_19f == 25u ? UINT64_C(59) :
           state25_successor == 24 && state25_f0_19f == 22u ? UINT64_C(60) :
           state25_successor == 24 ? UINT64_C(105) :
           state25_successor == 27 ? UINT64_C(126) : REF_TOTAL));
    CHECK(reference_cpu.procedure_calls - snap_calls ==
          (state25_successor == 16 ? UINT64_C(4) :
           state25_successor == 28 ? UINT64_C(3) :
           (state25_successor >= 17 && state25_successor <= 31 &&
            state25_successor != 24 && state25_successor != 27) ? UINT64_C(3) :
           state25_successor == 24 && state25_f0_19f != 0u ? UINT64_C(2) :
           state25_successor == 24 ? UINT64_C(3) :
           state25_successor == 27 ? UINT64_C(4) : REF_CALLS));
    CHECK(reference_cpu.procedure_returns - snap_returns ==
          (state25_successor == 16 ? UINT64_C(4) :
           state25_successor == 28 ? UINT64_C(3) :
           (state25_successor >= 17 && state25_successor <= 31 &&
            state25_successor != 24 && state25_successor != 27) ? UINT64_C(3) :
           state25_successor == 24 && state25_f0_19f != 0u ? UINT64_C(2) :
           state25_successor == 24 ? UINT64_C(3) :
           state25_successor == 27 ? UINT64_C(4) : REF_RETS));

    /* Native: the 0x1442c body (including the two 0x14640 helper calls). */
    native_status = vf2_hybrid_player_1442c_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_1442C_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions ==
          (state25_successor == 16 ? UINT64_C(144) :
           state25_successor == 28 ? UINT64_C(92) :
           (state25_successor >= 17 && state25_successor <= 31 &&
            state25_successor != 24 && state25_successor != 27) ? UINT64_C(98) :
           state25_successor == 24 && state25_f0_19f == 25u ? UINT64_C(59) :
           state25_successor == 24 && state25_f0_19f == 22u ? UINT64_C(60) :
           state25_successor == 24 ? UINT64_C(105) :
           state25_successor == 27 ? UINT64_C(126) : REF_TOTAL));
    CHECK(native_cpu.procedure_calls - snap_calls ==
          (state25_successor == 16 ? UINT64_C(4) :
           state25_successor == 28 ? UINT64_C(3) :
           (state25_successor >= 17 && state25_successor <= 31 &&
            state25_successor != 24 && state25_successor != 27) ? UINT64_C(3) :
           state25_successor == 24 && state25_f0_19f != 0u ? UINT64_C(2) :
           state25_successor == 24 ? UINT64_C(3) :
           state25_successor == 27 ? UINT64_C(4) : REF_CALLS));
    CHECK(native_cpu.procedure_returns - snap_returns ==
          (state25_successor == 16 ? UINT64_C(4) :
           state25_successor == 28 ? UINT64_C(3) :
           (state25_successor >= 17 && state25_successor <= 31 &&
            state25_successor != 24 && state25_successor != 27) ? UINT64_C(3) :
           state25_successor == 24 && state25_f0_19f != 0u ? UINT64_C(2) :
           state25_successor == 24 ? UINT64_C(3) :
           state25_successor == 27 ? UINT64_C(4) : REF_RETS));

    printf(
        "player-1442c-live state=%d%s ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
        state25_successor,
        state25_successor == 16 ? "-25-16" :
        state25_successor == 22 ? "-25-22" :
        state25_successor == 25 ? "-25-25" :
        state25_successor == 28 ? "-25-28" :
        (state25_successor >= 17 && state25_successor <= 31 &&
         state25_successor != 24 && state25_successor != 27) ? "-25-neutral" :
        state25_successor == 24 && state25_f0_19f == 25u ? "-25-24-19f25" :
        state25_successor == 24 && state25_f0_19f == 22u ? "-25-24-19f22" :
        state25_successor == 24 ? "-25-24" :
        state25_successor == 27 ? "-25-27" : "",
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions,
        (unsigned long long)(reference_cpu.procedure_calls - snap_calls),
        (unsigned long long)(native_cpu.procedure_calls - snap_calls),
        (unsigned long long)(reference_cpu.procedure_returns - snap_returns),
        (unsigned long long)(native_cpu.procedure_returns - snap_returns));

    /* Full live-state equality at the 0x1463c boundary, covering
     * registers, CC/AC, frames and Work-RAM (both fighters' +0x198
     * cleared and the measured r3/r7/r8/r14/r15/CC poststate). */
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-1442c-live ref=%d native=%d compare=%d "
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

static void run_sibling_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
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
              "D:/ia/vf2-decomp/out/park-14640-sib.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/park-14640-sib.vf2snap") == VF2_OK);
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
    CHECK(reference_cpu.ip == SIB_ENTRY);
    CHECK(native_cpu.ip == SIB_ENTRY);
    CHECK(reference_cpu.local_frame_depth > 0u);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: the +0x194 != 0 sibling through its 0x1438c return. */
    steps = 0u;
    while (reference_cpu.ip != SIB_RETURN && steps < 1024u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == SIB_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == SIB_TOTAL);
    CHECK(reference_cpu.procedure_calls - snap_calls == SIB_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == SIB_RETS);

    /* Native: the 0x14640 helper sibling. */
    native_status = vf2_hybrid_player_14640_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == SIB_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == SIB_TOTAL);
    CHECK(native_cpu.procedure_calls - snap_calls == SIB_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == SIB_RETS);

    printf(
        "player-14640-sibling ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
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
            "player-14640-sibling ref=%d native=%d compare=%d "
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

static void run_s25_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int successor_state
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
              "D:/ia/vf2-decomp/out/park-1442c-s25.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/park-1442c-s25.vf2snap") == VF2_OK);
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
    CHECK(reference_cpu.ip == S25_ENTRY);
    CHECK(native_cpu.ip == S25_ENTRY);
    CHECK(reference_cpu.local_frame_depth > 0u);
    if (successor_state != 0) {
        const uint32_t fighter0 = reference_cpu.registers[16u + 7u];
        const uint32_t fighter1 = reference_cpu.registers[16u + 8u];
        CHECK(fighter0 != 0u);
        CHECK(fighter1 != 0u);
        write_u16(&reference_machine, fighter1 + UINT32_C(0x194), 0x0073u);
        write_u16(&native_machine, fighter1 + UINT32_C(0x194), 0x0073u);
        write_u8(&reference_machine, fighter0 + UINT32_C(0x197), 25u);
        write_u8(&native_machine, fighter0 + UINT32_C(0x197), 25u);
        write_u8(&reference_machine, fighter1 + UINT32_C(0x197),
                 (uint8_t)successor_state);
        write_u8(&native_machine, fighter1 + UINT32_C(0x197),
                 (uint8_t)successor_state);
        write_u16(&reference_machine, fighter0 + UINT32_C(0x858), 0u);
        write_u16(&native_machine, fighter0 + UINT32_C(0x858), 0u);
        write_u16(&reference_machine, fighter1 + UINT32_C(0x808), 2u);
        write_u16(&native_machine, fighter1 + UINT32_C(0x808), 2u);
        write_u16(&reference_machine, fighter1 + UINT32_C(0x1aa), 0u);
        write_u16(&native_machine, fighter1 + UINT32_C(0x1aa), 0u);
        reference_cpu.registers[7] = 25u;
        native_cpu.registers[7] = 25u;
        reference_cpu.registers[8] = (uint32_t)successor_state;
        native_cpu.registers[8] = (uint32_t)successor_state;
    }
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: step the 0x144b0 state-25 arm to its 0x1463c boundary
     * (53 steps / +1 call / +1 return). */
    steps = 0u;
    while (reference_cpu.ip != S25_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == S25_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions ==
          successor_state == 27 ? UINT64_C(100) :
          successor_state == 16 ? S25_16_TOTAL : S25_TOTAL);
    CHECK(reference_cpu.procedure_calls - snap_calls ==
          successor_state == 27 ? UINT64_C(2) :
          successor_state == 16 ? S25_16_CALLS : S25_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns ==
          successor_state == 27 ? UINT64_C(2) :
          successor_state == 16 ? S25_16_RETS : S25_RETS);

    /* Native: the 0x144b0 state-25 arm. */
    native_status = vf2_hybrid_player_144b0_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == S25_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions ==
          successor_state == 27 ? UINT64_C(100) :
          successor_state == 16 ? S25_16_TOTAL : S25_TOTAL);
    CHECK(native_cpu.procedure_calls - snap_calls ==
          successor_state == 27 ? UINT64_C(2) :
          successor_state == 16 ? S25_16_CALLS : S25_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns ==
          successor_state == 27 ? UINT64_C(2) :
          successor_state == 16 ? S25_16_RETS : S25_RETS);

    printf(
        "player-144b0-s25%s ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
        successor_state == 16 ? "-16" :
        successor_state == 24 ? "-24" :
        successor_state == 27 ? "-27" : "",
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
            "player-144b0-s25 ref=%d native=%d compare=%d "
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

static void run_s25_nt_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
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
              "D:/ia/vf2-decomp/out/park-1442c-s25-nt.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/park-1442c-s25-nt.vf2snap") == VF2_OK);
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
    CHECK(reference_cpu.ip == S25_NT_ENTRY);
    CHECK(native_cpu.ip == S25_NT_ENTRY);
    CHECK(reference_cpu.local_frame_depth > 0u);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: step the 0x144b0 cmpobl-not-taken sibling to its 0x1463c
     * boundary (47 steps / +1 call / +1 return). */
    steps = 0u;
    while (reference_cpu.ip != S25_NT_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == S25_NT_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == S25_NT_TOTAL);
    CHECK(reference_cpu.procedure_calls - snap_calls == S25_NT_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == S25_NT_RETS);

    /* Native: the 0x144b0 state-25 arm sibling. */
    native_status = vf2_hybrid_player_144b0_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == S25_NT_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == S25_NT_TOTAL);
    CHECK(native_cpu.procedure_calls - snap_calls == S25_NT_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == S25_NT_RETS);

    printf(
        "player-144b0-s25-nt ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
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
            "player-144b0-s25-nt ref=%d native=%d compare=%d "
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

static void run_s25_eq_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
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
    uint32_t fighter1 = 0u;
    uint16_t zero = 0u;
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
              "D:/ia/vf2-decomp/out/park-1442c-s25-nt.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/park-1442c-s25-nt.vf2snap") == VF2_OK);
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
    CHECK(reference_cpu.ip == S25_NT_ENTRY);
    /* cmpobl-equal point: r13 == r3 == 0 (s25-nt has r3 == 0, so clear
     * fighter1 +0x1aa from 100 to 0). */
    fighter1 = reference_cpu.registers[16u + 8u];
    CHECK(fighter1 != 0u);
    CHECK(vf2_model2a_write(&reference_machine, fighter1 + UINT32_C(0x1aa),
                            &zero, 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter1 + UINT32_C(0x1aa),
                            &zero, 2u) == VF2_OK);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    steps = 0u;
    while (reference_cpu.ip != S25_NT_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == S25_NT_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == S25_NT_TOTAL);
    CHECK(reference_cpu.procedure_calls - snap_calls == S25_NT_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == S25_NT_RETS);

    native_status = vf2_hybrid_player_144b0_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == S25_NT_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == S25_NT_TOTAL);
    CHECK(native_cpu.procedure_calls - snap_calls == S25_NT_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == S25_NT_RETS);

    printf(
        "player-144b0-s25-eq ref=%llu native=%llu\n",
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions);

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-144b0-s25-eq ref=%d native=%d compare=%d "
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

static void run_1474_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint8_t b19f_value,
    uint64_t expected_total
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
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint8_t v24 = 24u;
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
    CHECK(reference_cpu.ip == PLAYER_1442C_ENTRY);
    fighter0 = reference_cpu.registers[16u + 7u];
    fighter1 = reference_cpu.registers[16u + 8u];
    CHECK(fighter0 != 0u);
    CHECK(fighter1 != 0u);
    CHECK(vf2_model2a_write(&reference_machine, fighter0 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter0 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&reference_machine, fighter1 + UINT32_C(0x19f),
                            &b19f_value, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter1 + UINT32_C(0x19f),
                            &b19f_value, 1u) == VF2_OK);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    steps = 0u;
    while (reference_cpu.ip != PLAYER_1442C_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_1442C_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == expected_total);
    CHECK(reference_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == REF_RETS);

    native_status = vf2_hybrid_player_1442c_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_1442C_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == expected_total);
    CHECK(native_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == REF_RETS);

    printf(
        "player-1442c-1474-%u ref=%llu native=%llu\n",
        (unsigned)b19f_value,
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions);

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-1442c-1474-%u ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (unsigned)b19f_value,
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

static void run_1474_swapped_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint8_t b19f_value,
    uint64_t expected_total
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
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint8_t v24 = 24u;
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
    CHECK(reference_cpu.ip == PLAYER_1442C_ENTRY);
    fighter0 = reference_cpu.registers[16u + 7u];
    fighter1 = reference_cpu.registers[16u + 8u];
    CHECK(fighter0 != 0u);
    CHECK(fighter1 != 0u);
    CHECK(vf2_model2a_write(&reference_machine, fighter1 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter1 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&reference_machine, fighter0 + UINT32_C(0x19f),
                            &b19f_value, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter0 + UINT32_C(0x19f),
                            &b19f_value, 1u) == VF2_OK);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    steps = 0u;
    while (reference_cpu.ip != PLAYER_1442C_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_1442C_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == expected_total);
    CHECK(reference_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == REF_RETS);

    native_status = vf2_hybrid_player_1442c_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_1442C_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == expected_total);
    CHECK(native_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == REF_RETS);

    printf(
        "player-1442c-1474-swapped-%u ref=%llu native=%llu\n",
        (unsigned)b19f_value,
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions);

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-1442c-1474-swapped-%u ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (unsigned)b19f_value,
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

static void run_1474_both24_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint8_t b19f_value,
    uint64_t expected_total
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
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint8_t v24 = 24u;
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
    CHECK(reference_cpu.ip == PLAYER_1442C_ENTRY);
    fighter0 = reference_cpu.registers[16u + 7u];
    fighter1 = reference_cpu.registers[16u + 8u];
    CHECK(fighter0 != 0u);
    CHECK(fighter1 != 0u);
    CHECK(vf2_model2a_write(&reference_machine, fighter0 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter0 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&reference_machine, fighter1 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter1 + UINT32_C(0x197),
                            &v24, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&reference_machine, fighter1 + UINT32_C(0x19f),
                            &b19f_value, 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&native_machine, fighter1 + UINT32_C(0x19f),
                            &b19f_value, 1u) == VF2_OK);
    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    steps = 0u;
    while (reference_cpu.ip != PLAYER_1442C_RETURN && steps < 4096u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == PLAYER_1442C_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == expected_total);
    CHECK(reference_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == REF_RETS);

    native_status = vf2_hybrid_player_1442c_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == PLAYER_1442C_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == expected_total);
    CHECK(native_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == REF_RETS);

    printf(
        "player-1442c-1474-both24-%u ref=%llu native=%llu\n",
        (unsigned)b19f_value,
        (unsigned long long)reference_instructions,
        (unsigned long long)native_instructions);

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "player-1442c-1474-both24-%u ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (unsigned)b19f_value,
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

    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 0, 0u);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 16, 0u);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 24, 0u);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 24, 25u);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 24, 22u);
    for (int state25_successor = 17; state25_successor <= 31;
         ++state25_successor) {
        if (state25_successor != 24 && state25_successor != 27) {
            run_rom_case(main_rom, main_rom_size, main_data, main_data_size,
                         state25_successor, 0u);
        }
    }
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 27, 0u);
    run_sibling_case(main_rom, main_rom_size, main_data, main_data_size);
    run_s25_case(main_rom, main_rom_size, main_data, main_data_size, 0);
    run_s25_case(main_rom, main_rom_size, main_data, main_data_size, 16);
    run_s25_case(main_rom, main_rom_size, main_data, main_data_size, 24);
    run_s25_case(main_rom, main_rom_size, main_data, main_data_size, 27);
    run_s25_nt_case(main_rom, main_rom_size, main_data, main_data_size);
    run_s25_eq_case(main_rom, main_rom_size, main_data, main_data_size);
    run_1474_case(main_rom, main_rom_size, main_data, main_data_size, 25u, 54u);
    run_1474_case(main_rom, main_rom_size, main_data, main_data_size, 22u, 55u);
    run_1474_swapped_case(main_rom, main_rom_size, main_data, main_data_size, 25u, 57u);
    run_1474_swapped_case(main_rom, main_rom_size, main_data, main_data_size, 22u, 58u);
    run_1474_both24_case(main_rom, main_rom_size, main_data, main_data_size, 25u, 56u);
    run_1474_both24_case(main_rom, main_rom_size, main_data, main_data_size, 22u, 57u);
    run_1474_case(main_rom, main_rom_size, main_data, main_data_size, 0u, 57u);
    run_1474_swapped_case(main_rom, main_rom_size, main_data, main_data_size, 0u, 60u);
    run_1474_both24_case(main_rom, main_rom_size, main_data, main_data_size, 0u, 59u);

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(
                stderr, "%d player-1442c-live unit test(s) failed\n",
                failures);
            return EXIT_FAILURE;
        }
        puts("player-1442c-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-1442c-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-1442c-live differential tests passed");
    return EXIT_SUCCESS;
}
