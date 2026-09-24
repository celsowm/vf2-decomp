/* Differential pins for the measured fa_rob 0x14640 compare-greater tails
 * admitted in v0414. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

#define ENTRY UINT32_C(0x00014640)
#define RETURN_IP UINT32_C(0x000146d8)

static int failures = 0;

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #x); \
    ++failures; } } while (0)

static void write_bytes(vf2_model2a *m, uint32_t address,
                        const uint8_t *bytes, size_t size)
{
    CHECK(vf2_model2a_write(m, address, bytes, size) == VF2_OK);
}

static void write_u32(vf2_model2a *m, uint32_t address, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)value, (uint8_t)(value >> 8u),
        (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)
    };
    write_bytes(m, address, bytes, sizeof(bytes));
}

static void write_u16(vf2_model2a *m, uint32_t address, uint16_t value)
{
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    write_bytes(m, address, bytes, sizeof(bytes));
}

static void write_u8(vf2_model2a *m, uint32_t address, uint8_t value)
{
    write_bytes(m, address, &value, 1u);
}

static void run_case(const uint8_t *rom, size_t rom_size,
                     const uint8_t *data, size_t data_size,
                     uint32_t flags, uint32_t r194, uint8_t state,
                     uint64_t expected_steps)
{
    vf2_model2a rm;
    vf2_model2a nm;
    vf2_i960_cpu rc;
    vf2_i960_cpu nc;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_status status;
    uint32_t fighter;
    uint32_t steps = 0u;
    uint64_t base_steps;
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
    if (!ok) {
        vf2_model2a_shutdown(&rm);
        vf2_model2a_shutdown(&nm);
        vf2_i960_snapshot_destroy(&snap);
        return;
    }
    CHECK(vf2_i960_snapshot_restore(&snap, &rc, &rm) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &nc, &nm) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&rm, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&rm, data, data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nm, rom, rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nm, data, data_size) == VF2_OK);

    fighter = rc.registers[VF2_I960_G0_REGISTER + 7u];
    CHECK(fighter != 0u);
    rc.ip = ENTRY;
    nc.ip = ENTRY;
    write_u32(&rm, fighter + UINT32_C(0x198), 0u);
    write_u32(&nm, fighter + UINT32_C(0x198), 0u);
    write_u32(&rm, fighter + UINT32_C(0x654), 1u);
    write_u32(&nm, fighter + UINT32_C(0x654), 1u);
    write_u32(&rm, fighter, flags);
    write_u32(&nm, fighter, flags);
    write_u32(&rm, fighter + UINT32_C(0x194), r194);
    write_u32(&nm, fighter + UINT32_C(0x194), r194);
    write_u8(&rm, fighter + UINT32_C(0x197), state);
    write_u8(&nm, fighter + UINT32_C(0x197), state);
    write_u16(&rm, fighter + UINT32_C(0x1aa), 2u);
    write_u16(&nm, fighter + UINT32_C(0x1aa), 2u);
    write_u16(&rm, fighter + UINT32_C(0x62a), 1u);
    write_u16(&nm, fighter + UINT32_C(0x62a), 1u);

    base_steps = snap.cpu.executed_instructions;
    while (rc.ip != RETURN_IP && steps < 128u) {
        status = vf2_i960_step(&rc, &rm, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) break;
    }
    CHECK(rc.ip == RETURN_IP);
    CHECK(rc.executed_instructions - base_steps == expected_steps);
    status = vf2_hybrid_player_14640_compare_execute_for_test(&nm, &nc);
    CHECK(status == VF2_OK);
    CHECK(nc.ip == RETURN_IP);
    CHECK(nc.executed_instructions - base_steps == expected_steps);
    CHECK(vf2_i960_compare_live_state(&rc, &rm, &nc, &nm, &diff) == VF2_OK);
    CHECK(diff.equal);
    if (!diff.equal) {
        fprintf(stderr, "compare-tail mismatch %s at %zu: 0x%08x/0x%08x\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
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
    CHECK(vf2_hybrid_player_14640_compare_execute_for_test(NULL, &cpu) != VF2_OK);
    CHECK(vf2_hybrid_player_14640_compare_execute_for_test(&machine, NULL) != VF2_OK);
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
            /* v0414 neutral bit-4-clear: +0x194 == 0, 14 steps, EQUAL. */
            run_case(rom, rom_size, data, data_size, 0u, 0u, 0u, 14u);
            /* v0414 neutral bit-4-clear with nonzero +0x194: 16 steps. */
            run_case(rom, rom_size, data, data_size, 0u,
                     UINT32_C(0x1234), 0u, 16u);
            /* v0414 state-13 bit-4-clear: the state byte makes +0x194 nonzero. */
            run_case(rom, rom_size, data, data_size, 0u, 0u, 13u, 16u);
            /* v0414 state-13 bit-4-set: 0x146b8 takes to 0x146c8. */
            run_case(rom, rom_size, data, data_size, UINT32_C(0x10),
                     0u, 13u, 17u);
        }
        free(rom);
        free(data);
    }
    if (failures != 0) return EXIT_FAILURE;
    puts(argc == 2 ? "player-14640-compare-tail-live differential tests passed"
                   : "player-14640-compare-tail-live unit tests passed");
    return EXIT_SUCCESS;
}
