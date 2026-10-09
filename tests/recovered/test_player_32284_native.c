/* ====================================================================
 * 0x32284 recovery ctest entry (v0756b)
 *
 * Exercises the recovered post_cf04_combat_state_clear_v18
 * vf2_hybrid_player_32284_execute:
 *
 * Same body as 0x323fc except clrbit 18 of 0x500068 instead of
 * bit 19. Tests the 3 paths (A toggles 0x500056 + setbits;
 * B/C just ret) and verifies bit 18 of 0x500068 is cleared.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00032284)
#define RET_IP_PATH_A UINT32_C(0x000324a0)
#define RET_IP_PATH_B UINT32_C(0x0003244c)
#define G13_PTR UINT32_C(0x00500814)
#define G13_WORK_BASE UINT32_C(0x00580000)

#define G13_REGISTER_INDEX (16u + 13u)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
                fprintf(                                            \
                    stderr,                                         \
                    "FAILED %s:%d: %s\n",                          \
                    __FILE__,                                       \
                    __LINE__,                                       \
                    #expression                                     \
                );                                                  \
                ++failures;                                         \
        }                                                           \
    } while (0)

static void write_u32_le(vf2_model2a *m, uint32_t addr, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, addr, v) == VF2_OK);
}
static uint32_t read_u32_le(vf2_model2a *m, uint32_t addr) {
    uint32_t v = 0;
    CHECK(vf2_model2a_read_u32(m, addr, &v) == VF2_OK);
    return v;
}
static void write_u8(vf2_model2a *m, uint32_t addr, uint8_t v) {
    CHECK(vf2_model2a_write(m, addr, &v, 1) == VF2_OK);
}
static uint8_t read_u8(vf2_model2a *m, uint32_t addr) {
    uint8_t v = 0;
    CHECK(vf2_model2a_read(m, addr, &v, 1) == VF2_OK);
    return v;
}

static int run_path(
    uint32_t g13_word_initial,
    uint8_t pre_500056,
    uint32_t ret_ip_expected
) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;

    if (vf2_model2a_initialize(&machine) == 0) return 0;
    if (machine.work_ram == NULL) {
        vf2_model2a_shutdown(&machine);
        return 0;
    }

    /* Pre-state: 0x500068 has bit18 set + bit21 set. */
    write_u32_le(&machine, UINT32_C(0x00500068),
                 (UINT32_C(1) << 18u) | (UINT32_C(1) << 21u));
    write_u32_le(&machine, UINT32_C(0x00500200), UINT32_C(0x200000));
    write_u32_le(&machine, UINT32_C(0x00500064), UINT32_C(0));
    write_u8(&machine, UINT32_C(0x00500056), pre_500056);

    write_u32_le(&machine, G13_PTR, G13_WORK_BASE);
    write_u32_le(&machine, G13_WORK_BASE, g13_word_initial);
    /* Sentinel at g13+0xc. */
    write_u32_le(&machine, G13_WORK_BASE + UINT32_C(0xc),
                 UINT32_C(0xcafef00d));

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    cpu.registers[G13_REGISTER_INDEX] = G13_WORK_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, ret_ip_expected) == VF2_OK);

    status = vf2_hybrid_player_32284_execute(&machine, &cpu);
    if (status != VF2_OK) {
        fprintf(stderr, "FAILED: 0x32284 returned status=%d\n",
                (int)status);
        ++failures;
        vf2_model2a_shutdown(&machine);
        return 1;
    }
    if (cpu.ip != ret_ip_expected) {
        fprintf(stderr, "FAILED: cpu.ip=0x%x expected 0x%x\n",
                cpu.ip, ret_ip_expected);
        ++failures;
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    /* Bit 18 of 0x500068 must be cleared. */
    CHECK((read_u32_le(&machine, UINT32_C(0x00500068)) &
           (UINT32_C(1) << 18u)) == 0u);

    /* Common-prefix checks. */
    CHECK(read_u32_le(&machine, UINT32_C(0x00500024)) == UINT32_C(0x53f));
    CHECK(read_u8(&machine, G13_WORK_BASE + UINT32_C(0x47)) == UINT8_C(0));
    CHECK(read_u8(&machine, G13_WORK_BASE + UINT32_C(0x40)) == UINT8_C(100));
    CHECK((read_u32_le(&machine, G13_WORK_BASE) &
           (UINT32_C(1) << 8u)) == 0u);

    if (ret_ip_expected == RET_IP_PATH_A) {
        uint32_t g13_word = read_u32_le(&machine, G13_WORK_BASE);
        CHECK((g13_word & (UINT32_C(1) << 1u)) != 0u);
        CHECK((g13_word & (UINT32_C(1) << 3u)) != 0u);
        CHECK(read_u32_le(&machine, G13_WORK_BASE + UINT32_C(0xc))
              == UINT32_C(0x324a0));
        CHECK(read_u8(&machine, UINT32_C(0x00500056))
              == (pre_500056 == 0u ? UINT8_C(1) : UINT8_C(0)));
    } else {
        /* Path B/C: bits 1, 3 unchanged. */
        CHECK(read_u32_le(&machine, G13_WORK_BASE + UINT32_C(0xc))
              == UINT32_C(0xcafef00d));
        CHECK(read_u8(&machine, UINT32_C(0x00500056)) == pre_500056);
    }

    vf2_model2a_shutdown(&machine);
    return 1;
}

int main(void) {
    if (!run_path(UINT32_C(1), 0u, RET_IP_PATH_A)) return 1;
    if (!run_path(UINT32_C(1), 1u, RET_IP_PATH_A)) return 1;
    if (!run_path(UINT32_C(0), 0u, RET_IP_PATH_B)) return 1;
    if (!run_path(UINT32_C(0x8), 0u, RET_IP_PATH_B)) return 1;

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x32284 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x32284 post_cf04_combat_state_clear_v18: 4 paths\n");
    return 0;
}
