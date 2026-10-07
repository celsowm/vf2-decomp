/* ====================================================================
 * 0x1fee4 recovery ctest entry (v0749)
 *
 * Exercises the recovered trivial init function
 * vf2_hybrid_player_1fee4_execute:
 *
 *   - Writes IEEE 754 float 1.0 (0x3f800000) to 26 consecutive
 *     4-byte locations starting at 0x50a0e0 (covering
 *     0x50a0e0..0x50a144).
 *
 * The test verifies:
 *   1. All 26 locations contain 0x3f800000.
 *   2. Locations outside the range (0x50a148 and beyond) are
 *      untouched.
 *   3. Return status is VF2_OK and IP lands on 0x1ff08.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0001fee4)
#define RET_IP UINT32_C(0x0001ff08)
#define INIT_BASE UINT32_C(0x0050a0e0)
#define INIT_COUNT 26u

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

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) {
        fprintf(stderr, "FAILED: vf2_model2a_initialize returned 0\n");
        return 1;
    }
    if (machine.work_ram == NULL) {
        fprintf(stderr, "FAILED: machine.work_ram is NULL\n");
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    /* Pre-poison the init range with a sentinel so we know the
     * function actually wrote 0x3f800000. */
    {
        uint32_t address = INIT_BASE;
        uint32_t i;
        for (i = 0; i < INIT_COUNT + 2u; ++i) {
            CHECK(vf2_model2a_write_u32(
                &machine, address, UINT32_C(0xdeadbeef)) == VF2_OK);
            address += 4u;
        }
    }

    /* Set up the CPU and call the recovered function. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, ENTRY_IP, RET_IP) == VF2_OK);
    vf2_status status = vf2_hybrid_player_1fee4_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify all 26 locations are 0x3f800000. */
    {
        uint32_t address = INIT_BASE;
        uint32_t i;
        for (i = 0; i < INIT_COUNT; ++i) {
            uint32_t value = 0u;
            CHECK(vf2_model2a_read_u32(
                &machine, address, &value) == VF2_OK);
            CHECK(value == UINT32_C(0x3f800000));
            address += 4u;
        }
    }

    /* Verify the location just past the range (0x50a148) is
     * still the sentinel (untouched by the function). */
    {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(
            &machine, INIT_BASE + INIT_COUNT * 4u, &value) == VF2_OK);
        CHECK(value == UINT32_C(0xdeadbeef));
    }

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x1fee4 trivial init test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x1fee4 init: 26 x float 1.0 at 0x50a0e0..0x50a144\n");
    return 0;
}
