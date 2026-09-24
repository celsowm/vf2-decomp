/* ROM-backed differential fixture for the measured fa_rob state-16 body path
 * through 0x1442c -> 0x14528 -> 0x14570 (v0416).
 *
 * Both fighters enter the body with +0x197 == 16 and a valid type-5 index
 * 0x73 in +0x194. The two 0x14640 neutral helpers leave the state bytes
 * intact, then the 0x14528 branch reaches the direct or 0x500028-gated swap
 * state-16 tail. The reference and native paths are compared through the
 * unconsumed 0x1463c return.
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

#define ENTRY UINT32_C(0x0001442c)
#define RETURN_IP UINT32_C(0x0001463c)
#define TYPE5_INDEX UINT32_C(0x00000073)

static int failures = 0;

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #x); \
    ++failures; } } while (0)

static void write_u8(vf2_model2a *m, uint32_t a, uint8_t v)
{
    CHECK(vf2_model2a_write(m, a, &v, 1u) == VF2_OK);
}

static void write_u32(vf2_model2a *m, uint32_t a, uint32_t v)
{
    const uint8_t b[4] = {
        (uint8_t)v, (uint8_t)(v >> 8u),
        (uint8_t)(v >> 16u), (uint8_t)(v >> 24u)
    };
    CHECK(vf2_model2a_write(m, a, b, sizeof(b)) == VF2_OK);
}

static void run_case(const uint8_t *rom, size_t rom_size,
                     const uint8_t *data, size_t data_size,
                     uint32_t board28, uint8_t state0_19f, uint8_t state1,
                     uint32_t state1_194,
                     uint64_t expected_steps, uint64_t expected_calls)
{
    vf2_model2a rm;
    vf2_model2a nm;
    vf2_i960_cpu rc;
    vf2_i960_cpu nc;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_status status;
    uint32_t f0;
    uint32_t f1;
    uint32_t steps = 0u;
    uint64_t base_steps;
    uint64_t base_calls;
    uint64_t base_returns;
    int ok;

    memset(&rm, 0, sizeof(rm));
    memset(&nm, 0, sizeof(nm));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);
    CHECK(vf2_model2a_initialize(&rm) != 0);
    CHECK(vf2_model2a_initialize(&nm) != 0);
    ok = (vf2_i960_snapshot_read_file(
              &snap, "D:/ia/vf2-decomp/out/park-1442c.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/park-1442c.vf2snap") == VF2_OK);
    CHECK(ok);
    if (!ok) goto cleanup;
    CHECK(vf2_i960_snapshot_restore(&snap, &rc, &rm) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &nc, &nm) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&rm, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&rm, data, data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nm, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nm, data, data_size) == VF2_OK);

    f0 = rc.registers[VF2_I960_G0_REGISTER + 7u];
    f1 = rc.registers[VF2_I960_G0_REGISTER + 8u];
    CHECK(f0 != 0u);
    CHECK(f1 != 0u);
    rc.ip = ENTRY;
    nc.ip = ENTRY;

    write_u32(&rm, f0, 0u);
    write_u32(&nm, f0, 0u);
    write_u32(&rm, f1, 0u);
    write_u32(&nm, f1, 0u);
    write_u32(&rm, f0 + UINT32_C(0x194), TYPE5_INDEX);
    write_u32(&nm, f0 + UINT32_C(0x194), TYPE5_INDEX);
    write_u32(&rm, f1 + UINT32_C(0x194), state1_194);
    write_u32(&nm, f1 + UINT32_C(0x194), state1_194);
    write_u8(&rm, f0 + UINT32_C(0x197), 16u);
    write_u8(&nm, f0 + UINT32_C(0x197), 16u);
    write_u8(&rm, f0 + UINT32_C(0x19f), state0_19f);
    write_u8(&nm, f0 + UINT32_C(0x19f), state0_19f);
    write_u8(&rm, f1 + UINT32_C(0x197), state1);
    write_u8(&nm, f1 + UINT32_C(0x197), state1);
    write_u32(&rm, f0 + UINT32_C(0x198), 0u);
    write_u32(&nm, f0 + UINT32_C(0x198), 0u);
    write_u32(&rm, f1 + UINT32_C(0x198), 0u);
    write_u32(&nm, f1 + UINT32_C(0x198), 0u);
    write_u32(&rm, f0 + UINT32_C(0x654), 0u);
    write_u32(&nm, f0 + UINT32_C(0x654), 0u);
    write_u32(&rm, f1 + UINT32_C(0x654), 0u);
    write_u32(&nm, f1 + UINT32_C(0x654), 0u);
    write_u32(&rm, UINT32_C(0x00500028), board28);
    write_u32(&nm, UINT32_C(0x00500028), board28);

    base_steps = snap.cpu.executed_instructions;
    base_calls = snap.cpu.procedure_calls;
    base_returns = snap.cpu.procedure_returns;
    while (rc.ip != RETURN_IP && steps < 512u) {
        status = vf2_i960_step(&rc, &rm, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) break;
    }
    CHECK(rc.ip == RETURN_IP);
    CHECK(rc.executed_instructions - base_steps == expected_steps);
    CHECK(rc.procedure_calls - base_calls == expected_calls);
    CHECK(rc.procedure_returns - base_returns == expected_calls);

    status = vf2_hybrid_player_1442c_execute_for_test(&nm, &nc);
    CHECK(status == VF2_OK);
    CHECK(nc.ip == RETURN_IP);
    if (nc.executed_instructions - base_steps != expected_steps) {
        fprintf(stderr, "state16 native steps=%llu expected=%llu board=0x%08x\n",
                (unsigned long long)(nc.executed_instructions - base_steps),
                (unsigned long long)expected_steps, (unsigned)board28);
        ++failures;
    }
    CHECK(nc.procedure_calls - base_calls == expected_calls);
    CHECK(nc.procedure_returns - base_returns == expected_calls);
    CHECK(vf2_i960_compare_live_state(&rc, &rm, &nc, &nm, &diff) == VF2_OK);
    CHECK(diff.equal);
    if (!diff.equal) {
        fprintf(stderr, "state16 body mismatch %s at %zu: 0x%08x/0x%08x\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }

cleanup:
    vf2_model2a_shutdown(&rm);
    vf2_model2a_shutdown(&nm);
    vf2_i960_snapshot_destroy(&snap);
}

static void test_unit_fail_closed(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    cpu.ip = ENTRY;
    cpu.local_frame_depth = 1u;
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(0x510980);
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x512980);
    CHECK(vf2_hybrid_player_1442c_execute_for_test(NULL, &cpu) != VF2_OK);
    CHECK(vf2_hybrid_player_1442c_execute_for_test(&machine, NULL) != VF2_OK);
}

int main(int argc, char **argv)
{
    test_unit_fail_closed();
    if (argc == 2) {
        uint8_t *rom = NULL;
        uint8_t *data = NULL;
        size_t rom_size = 0u;
        size_t data_size = 0u;
        CHECK(vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                      &rom, &rom_size) == VF2_OK);
        CHECK(vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                      &data, &data_size) == VF2_OK);
        if (rom != NULL && data != NULL) {
            run_case(rom, rom_size, data, data_size,
                     0u, 0u, 16u, TYPE5_INDEX, UINT64_C(100), UINT64_C(3));
            run_case(rom, rom_size, data, data_size,
                     1u, 0u, 16u, TYPE5_INDEX, UINT64_C(104), UINT64_C(3));
            run_case(rom, rom_size, data, data_size,
                     0u, 0u, 26u, TYPE5_INDEX, UINT64_C(98), UINT64_C(3));
            run_case(rom, rom_size, data, data_size,
                     0u, 0u, 27u, TYPE5_INDEX, UINT64_C(126), UINT64_C(4));
            run_case(rom, rom_size, data, data_size,
                     0u, 0u, 24u, TYPE5_INDEX, UINT64_C(105), UINT64_C(3));
            run_case(rom, rom_size, data, data_size,
                     0u, 25u, 24u, TYPE5_INDEX, UINT64_C(59), UINT64_C(2));
            run_case(rom, rom_size, data, data_size,
                     0u, 22u, 24u, TYPE5_INDEX, UINT64_C(60), UINT64_C(2));
            run_case(rom, rom_size, data, data_size,
                     0u, 0u, 25u, 0u, UINT64_C(144), UINT64_C(4));
        }
        free(rom);
        free(data);
    }
    if (failures != 0) return EXIT_FAILURE;
    puts(argc == 2 ? "player-1442c-state16-live differential tests passed"
                   : "player-1442c-state16-live unit tests passed");
    return EXIT_SUCCESS;
}
