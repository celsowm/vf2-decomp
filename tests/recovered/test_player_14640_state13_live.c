/* ROM-backed differential fixture for the fa_rob state-13 neutral-tail arm
 * 0x14640 (v0411).
 *
 * The 0x14640 arm is the fa_rob fighter-state helper reached when the fighter
 * g7 has +0x198 == 0, +0x654 == 0, +0x197 == 13 (so the `0x14660 cmpobne 27,
 * r3` and the `0x1469c cmpobne 28, r3` are both taken, and the `0x146b8
 * cmpobe 13, r3` IS taken to 0x146c8) and bit 4 of (g7) SET.  The 0x146c8
 * neutral tail then runs with +0x194(g7) != 0 (because +0x197 == 13 forces
 * the high byte of the +0x194 u32), so the `0x146cc cmpobe 0, r14` is not
 * taken and the sibling clears +0x654(g7): `0x146c8 ld +0x194(g7), r14 ;
 * 0x146d0 mov 0, r15 ; 0x146d4 st r15, +0x654(g7)`.  Leaves r3 = +0x197
 * (13), r14 = +0x194 (unchanged), r15 = 0.  No walker.
 *
 * This fixture restores out/park-1442c.vf2snap, jumps to the 0x14640 entry
 * with +0x197 = 13 and bit 4 of (g7) set, then runs the reference interpreter
 * and the native helper to ip == 0x146d8 and asserts exact step/call/ret
 * lockstep plus full live-state equality (registers/CC/AC/frames/Work-RAM).
 *
 * Witness (measured, current build):
 *   vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
 *     --set-ip 0x14640 --set-reg g7=0x510980 --set-reg g8=0x512980 \
 *     --set-u32 0x510980=0x2014 --set-u8 0x510B17=0x0D --until 0x146d8
 *     -> 14 steps, +0 call / +0 return.
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

#define STATE13_ENTRY UINT32_C(0x00014640)
#define STATE13_RETURN UINT32_C(0x000146d8)

#define REF_TOTAL UINT64_C(14)
#define REF_CALLS UINT64_C(0)
#define REF_RETS UINT64_C(0)

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
        vf2_hybrid_player_14640_state13_execute_for_test(NULL, &cpu) != VF2_OK
    );
    CHECK(
        vf2_hybrid_player_14640_state13_execute_for_test(&machine, NULL) !=
        VF2_OK
    );
}

/* ROM-independent fail-closed checks for the state-13 neutral-tail arm.  The
 * arm must refuse any non-matching shape (wrong ip, zero fighter base, no
 * pushed frame) before touching memory. */
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
        vf2_hybrid_player_14640_state13_execute_for_test(&machine, &cpu) !=
        VF2_OK
    );

    /* Correct ip but zero fighter base. */
    cpu.ip = UINT32_C(0x00014640);
    cpu.registers[16u + 7u] = 0u;
    CHECK(
        vf2_hybrid_player_14640_state13_execute_for_test(&machine, &cpu) !=
        VF2_OK
    );
    cpu.registers[16u + 7u] = 0x510980u;

    /* Correct ip and fighter base but no pushed frame. */
    cpu.local_frame_depth = 0u;
    CHECK(
        vf2_hybrid_player_14640_state13_execute_for_test(&machine, &cpu) !=
        VF2_OK
    );
    cpu.local_frame_depth = 1u;
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

static void run_rom_case(
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
    uint32_t fighter0 = 0u;
    uint32_t flags = 0u;
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

    /* Force the measured state-13 shape on both machines: +0x197 = 13 (which
     * also forces +0x194 non-zero via its high byte) and bit 4 of (g7) set.
     * +0x198/+0x654 are already 0. */
    reference_cpu.ip = STATE13_ENTRY;
    native_cpu.ip = STATE13_ENTRY;
    write_u8(&reference_machine, fighter0 + UINT32_C(0x197), 13u);
    write_u8(&native_machine, fighter0 + UINT32_C(0x197), 13u);
    CHECK(vf2_model2a_read(
              &reference_machine, fighter0, (uint8_t *)&flags, 4u) == VF2_OK);
    flags |= (UINT32_C(1) << 4u);
    write_u32(&reference_machine, fighter0, flags);
    write_u32(&native_machine, fighter0, flags);

    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;

    /* Reference: step the 0x14640 state-13 neutral-tail arm to its 0x146d8
     * ret instruction (14 steps / +0 call / +0 return). */
    steps = 0u;
    while (reference_cpu.ip != STATE13_RETURN && steps < 1024u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == STATE13_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == REF_TOTAL);
    CHECK(reference_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(reference_cpu.procedure_returns - snap_returns == REF_RETS);

    /* Native: the 0x14640 state-13 neutral-tail arm. */
    native_status = vf2_hybrid_player_14640_state13_execute_for_test(
        &native_machine, &native_cpu);
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == STATE13_RETURN);
    native_instructions =
        native_cpu.executed_instructions - snap_instructions;
    CHECK(native_instructions == REF_TOTAL);
    CHECK(native_cpu.procedure_calls - snap_calls == REF_CALLS);
    CHECK(native_cpu.procedure_returns - snap_returns == REF_RETS);

    printf(
        "player-14640-state13-live ref=%llu native=%llu calls=%llu/%llu rets=%llu/%llu\n",
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
            "player-14640-state13-live ref=%d native=%d compare=%d "
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

    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);

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
                stderr, "%d player-14640-state13-live unit test(s) failed\n",
                failures);
            return EXIT_FAILURE;
        }
        puts("player-14640-state13-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d player-14640-state13-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("player-14640-state13-live differential tests passed");
    return EXIT_SUCCESS;
}
