/* ====================================================================
 * 0x323fc recovery ctest entry (v0756)
 *
 * Exercises the recovered post_cf04_combat_state_clear
 * vf2_hybrid_player_323fc_execute:
 *
 *   - Calls 0xcf04 (which has its own hybrid recovery, v0747).
 *   - clrbit 19 of 0x500068 (after 0xcf04's clrbit 21, the
 *     0x500068 state is bit20-clear + bit19-clear).
 *   - Clears bit 8 of *(g13) (g13 = cpu->registers[29]).
 *   - Writes 0x53f to 0x500024.
 *   - Writes 100 to (g13 + 0x40).
 *   - Stores 0 to (g13 + 0x47).
 *   - Dispatches on bit 0 of *(g13):
 *     - bit 0 set (path A): toggle 0x500056 between 0/1,
 *       setbits 1 and 3 of *(g13), store 0x324a0 to (g13 + 0xc),
 *       ret to 0x324a0.
 *     - bit 0 clear (path B/C): ret to 0x3244c.
 *
 * The test verifies the 3 paths and that g13 is consumed
 * as a pointer to work RAM (not a work-RAM address itself).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x000323fc)
#define RET_IP_PATH_A UINT32_C(0x000324a0)
#define RET_IP_PATH_B UINT32_C(0x0003244c)
#define G13_PTR UINT32_C(0x00500814)
#define G13_WORK_BASE UINT32_C(0x00580000)

#define G13_REGISTER_INDEX (16u + 13u)  /* g13 = cpu->registers[29] */

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

    /* Pre-state: 0x500068 has bit19 set + bit21 set (so cf04 clears
     * bit21, then this function clears bit19, leaving 0).
     * 0x500064 = 0 (mode; cf04 will overwrite it; not relevant for
     * this recovery).
     * 0x500000..0x50007f: clear (zero). */
    write_u32_le(&machine, UINT32_C(0x00500068), UINT32_C(0x80000)); /* bit19 */
    write_u32_le(&machine, UINT32_C(0x00500200), UINT32_C(0x200000)); /* bit21 */
    write_u32_le(&machine, UINT32_C(0x00500064), UINT32_C(0));
    write_u8(&machine, UINT32_C(0x00500056), pre_500056);

    /* g13_base set up so it points to G13_WORK_BASE. */
    write_u32_le(&machine, G13_PTR, G13_WORK_BASE);
    /* Pre-set *(g13) to g13_word_initial. */
    write_u32_le(&machine, G13_WORK_BASE, g13_word_initial);
    /* Pre-poison (g13 + 0xc) with a SENTINEL so we can verify
     * the recovery does NOT overwrite it on path B/C (and DOES
     * overwrite it with 0x324a0 on path A). */
    write_u32_le(&machine, G13_WORK_BASE + UINT32_C(0xc),
                 UINT32_C(0xcafef00d));

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    /* Set g13 to G13_WORK_BASE. */
    cpu.registers[G13_REGISTER_INDEX] = G13_WORK_BASE;
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, ret_ip_expected) == VF2_OK);

    status = vf2_hybrid_player_323fc_execute(&machine, &cpu);

    /* Verify post-state. */
    if (status != VF2_OK) {
        fprintf(stderr, "FAILED: 0x323fc returned status=%d\n", (int)status);
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

    /* 0x500068: cf04 does setbit 21 + clrbit 15 (in body), then
     * refuses the trailing clrbit 21 (which would have happened
     * at 0xcfa4 in a wired interpreter fallback). So in this
     * standalone test, 0x500068 ends with bit 21 set + bits 19,
     * 15 cleared (19 from us). */
    CHECK((read_u32_le(&machine, UINT32_C(0x00500068)) &
           (UINT32_C(1) << 19u)) == 0u);
    CHECK((read_u32_le(&machine, UINT32_C(0x00500068)) &
           (UINT32_C(1) << 15u)) == 0u);
    CHECK((read_u32_le(&machine, UINT32_C(0x00500068)) &
           (UINT32_C(1) << 21u)) != 0u);

    /* 0x500024 = 0x53f. */
    CHECK(read_u32_le(&machine, UINT32_C(0x00500024)) == UINT32_C(0x53f));

    /* (g13 + 0x47) = 0. */
    CHECK(read_u8(&machine, G13_WORK_BASE + UINT32_C(0x47)) == UINT8_C(0));

    /* (g13 + 0x40) = 100. */
    CHECK(read_u8(&machine, G13_WORK_BASE + UINT32_C(0x40)) == UINT8_C(100));

    /* Bit 8 of *(g13) clear. */
    CHECK((read_u32_le(&machine, G13_WORK_BASE) &
           (UINT32_C(1) << 8u)) == 0u);

    if (ret_ip_expected == RET_IP_PATH_A) {
        /* Path A: bits 1, 3 set. (g13 + 0xc) = 0x324a0.
         * 0x500056 toggled from pre_500056 to (pre==0 ? 1 : 0). */
        uint32_t g13_word = read_u32_le(&machine, G13_WORK_BASE);
        CHECK((g13_word & (UINT32_C(1) << 1u)) != 0u);
        CHECK((g13_word & (UINT32_C(1) << 3u)) != 0u);
        CHECK(read_u32_le(&machine, G13_WORK_BASE + UINT32_C(0xc))
              == UINT32_C(0x324a0));
        CHECK(read_u8(&machine, UINT32_C(0x00500056))
              == (pre_500056 == 0u ? UINT8_C(1) : UINT8_C(0)));
    } else {
        /* Path B/C: only the common prefix touches *(g13):
         * clrbit 8 + writes to g13+0x47 and g13+0x40. Bits 1
         * and 3 are not touched, so they retain their pre-state.
         * For path B (g13=0, pre-bits-1-3 clear) they're still
         * clear. For path C (g13=8, bit 3 pre-set) bit 3 stays
         * set — verify bit 3 = (initial & bit3). */
        uint32_t g13_word = read_u32_le(&machine, G13_WORK_BASE);
        CHECK((g13_word & (UINT32_C(1) << 1u))
              == (g13_word_initial & (UINT32_C(1) << 1u)));
        CHECK((g13_word & (UINT32_C(1) << 3u))
              == (g13_word_initial & (UINT32_C(1) << 3u)));
        /* (g13 + 0xc) untouched (still sentinel 0xcafef00d). */
        CHECK(read_u32_le(&machine, G13_WORK_BASE + UINT32_C(0xc))
              == UINT32_C(0xcafef00d));
        /* 0x500056 unchanged. */
        CHECK(read_u8(&machine, UINT32_C(0x00500056)) == pre_500056);
    }

    vf2_model2a_shutdown(&machine);
    return 1;
}

int main(void) {
    /* Path A: bit 0 of *(g13) set. Pre-set 0x500056=0 (so
     * toggle -> 1). */
    if (!run_path(
            UINT32_C(0x00000001), /* bit 0 set */
            0u,
            RET_IP_PATH_A))
    {
        return 1;
    }

    /* Path A variant: pre_500056=1 (toggles to 0). */
    if (!run_path(
            UINT32_C(0x00000001),
            1u,
            RET_IP_PATH_A))
    {
        return 1;
    }

    /* Path B: bit 0 clear, bit 3 clear. */
    if (!run_path(
            UINT32_C(0x00000000), /* both clear */
            0u,
            RET_IP_PATH_B))
    {
        return 1;
    }

    /* Path C: bit 0 clear, bit 3 set. */
    if (!run_path(
            UINT32_C(0x00000008), /* bit 3 set */
            0u,
            RET_IP_PATH_B))
    {
        return 1;
    }

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x323fc post_cf04_combat_state_clear "
                "(%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x323fc post_cf04_combat_state_clear: 4 paths "
           "(bit-0-set + 0x500056 toggle, path B, path C)\n");
    return 0;
}
