/* Differential pin for the measured fa_rob 0x14640 signed-less compare
 * prefix siblings (v0412/v0427). */

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
#define TOTAL UINT64_C(14)
#define NONZERO_TOTAL UINT64_C(16)

static int failures = 0;

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #x); \
    ++failures; } } while (0)

static void write_u32(vf2_model2a *m, uint32_t a, uint32_t v)
{
    const uint8_t b[4] = {
        (uint8_t)v, (uint8_t)(v >> 8u), (uint8_t)(v >> 16u),
        (uint8_t)(v >> 24u)
    };
    CHECK(vf2_model2a_write(m, a, b, sizeof(b)) == VF2_OK);
}

static void write_u16(vf2_model2a *m, uint32_t a, uint16_t v)
{
    const uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8u)};
    CHECK(vf2_model2a_write(m, a, b, sizeof(b)) == VF2_OK);
}

static void write_u8(vf2_model2a *m, uint32_t a, uint8_t v)
{
    CHECK(vf2_model2a_write(m, a, &v, 1u) == VF2_OK);
}

static void test_unit_fail_closed(void)
{
    vf2_model2a m;
    vf2_i960_cpu c;
    memset(&m, 0, sizeof(m));
    memset(&c, 0, sizeof(c));
    c.ip = ENTRY;
    c.local_frame_depth = 1u;
    c.registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(0x510980);
    CHECK(vf2_hybrid_player_14640_compare_less_execute_for_test(NULL, &c) != VF2_OK);
    CHECK(vf2_hybrid_player_14640_compare_less_execute_for_test(&m, NULL) != VF2_OK);
    CHECK(vf2_hybrid_player_14640_compare_less_execute_for_test(&m, &c) != VF2_OK);
}

static void run_case(const uint8_t *rom, size_t rom_size,
                     const uint8_t *data, size_t data_size,
                     int shape)
{
    vf2_model2a rm;
    vf2_model2a nm;
    vf2_i960_cpu rc;
    vf2_i960_cpu nc;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_status status;
    uint32_t fighter;
    uint64_t base_steps;
    uint32_t steps = 0u;
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
    write_u32(&rm, fighter + UINT32_C(0x194),
              shape == 2 ? UINT32_C(1) : UINT32_C(0));
    write_u32(&nm, fighter + UINT32_C(0x194),
              shape == 2 ? UINT32_C(1) : UINT32_C(0));
    write_u32(&rm, fighter, shape == 1 ? UINT32_C(0x10) : 0u);
    write_u32(&nm, fighter, shape == 1 ? UINT32_C(0x10) : 0u);
    write_u8(&rm, fighter + UINT32_C(0x197), shape == 1 ? 13u : 0u);
    write_u8(&nm, fighter + UINT32_C(0x197), shape == 1 ? 13u : 0u);
    write_u16(&rm, fighter + UINT32_C(0x1aa), 1u);
    write_u16(&nm, fighter + UINT32_C(0x1aa), 1u);
    write_u16(&rm, fighter + UINT32_C(0x62a), 2u);
    write_u16(&nm, fighter + UINT32_C(0x62a), 2u);

    base_steps = snap.cpu.executed_instructions;
    while (rc.ip != RETURN_IP && steps < 128u) {
        status = vf2_i960_step(&rc, &rm, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) break;
    }
    CHECK(rc.ip == RETURN_IP);
    CHECK(rc.executed_instructions - base_steps ==
          (shape == 1 ? 17u : shape == 2 ? NONZERO_TOTAL : TOTAL));

    status = vf2_hybrid_player_14640_compare_less_execute_for_test(&nm, &nc);
    CHECK(status == VF2_OK);
    CHECK(nc.ip == RETURN_IP);
    CHECK(nc.executed_instructions - base_steps ==
          (shape == 1 ? 17u : shape == 2 ? NONZERO_TOTAL : TOTAL));
    CHECK(vf2_i960_compare_live_state(&rc, &rm, &nc, &nm, &diff) == VF2_OK);
    CHECK(diff.equal);
    if (!diff.equal) {
        fprintf(stderr, "compare-less mismatch %s at %zu: 0x%08x/0x%08x\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    vf2_model2a_shutdown(&rm);
    vf2_model2a_shutdown(&nm);
    vf2_i960_snapshot_destroy(&snap);
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
            run_case(rom, rom_size, data, data_size, 0);
            run_case(rom, rom_size, data, data_size, 1);
            run_case(rom, rom_size, data, data_size, 2);
        }
        free(rom);
        free(data);
    }
    if (failures != 0) return EXIT_FAILURE;
    puts(argc == 2 ? "player-14640-compare-less-live differential tests passed"
                   : "player-14640-compare-less-live unit tests passed");
    return EXIT_SUCCESS;
}
