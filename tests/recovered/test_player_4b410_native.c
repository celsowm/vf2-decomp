/* ====================================================================
 * 0x4b410 recovery ctest entry (v0752)
 *
 * Exercises the recovered video_command_submit
 * vf2_hybrid_player_4b410_execute:
 *
 *   - Writes 1 to 0x550000 (control word).
 *   - Writes 3 to 0x5502e0 (status word).
 *   - Writes g0/g1/g2 to 0x5502e4/0x5502e8/0x5502ec.
 *
 * The test verifies:
 *   1. The 5 writes happened at the right addresses.
 *   2. Return status is VF2_OK and IP lands on 0x4b448.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0004b410)
#define RET_IP UINT32_C(0x0004b448)

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

    /* Set up the CPU with g0/g1/g2 as the test values. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    cpu.registers[VF2_I960_G0_REGISTER + 0u] = UINT32_C(0xdeadbeef);
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0xcafebabe);
    cpu.registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0xfeedface);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, ENTRY_IP, RET_IP) == VF2_OK);

    /* Pre-poison the write addresses with sentinels. */
    CHECK(vf2_model2a_write_u32(
        &machine, UINT32_C(0x00550000), UINT32_C(0xaaaaaaaa)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, UINT32_C(0x005502e0), UINT32_C(0xbbbbbbbb)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, UINT32_C(0x005502e4), UINT32_C(0xcccccccc)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, UINT32_C(0x005502e8), UINT32_C(0xdddddddd)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, UINT32_C(0x005502ec), UINT32_C(0xeeeeeeee)) == VF2_OK);

    /* Call the recovered function. */
    vf2_status status = vf2_hybrid_player_4b410_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify all 5 writes. */
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(
        &machine, UINT32_C(0x00550000), &v) == VF2_OK);
    CHECK(v == UINT32_C(1));
    v = 0u;
    CHECK(vf2_model2a_read_u32(
        &machine, UINT32_C(0x005502e0), &v) == VF2_OK);
    CHECK(v == UINT32_C(3));
    v = 0u;
    CHECK(vf2_model2a_read_u32(
        &machine, UINT32_C(0x005502e4), &v) == VF2_OK);
    CHECK(v == UINT32_C(0xdeadbeef));
    v = 0u;
    CHECK(vf2_model2a_read_u32(
        &machine, UINT32_C(0x005502e8), &v) == VF2_OK);
    CHECK(v == UINT32_C(0xcafebabe));
    v = 0u;
    CHECK(vf2_model2a_read_u32(
        &machine, UINT32_C(0x005502ec), &v) == VF2_OK);
    CHECK(v == UINT32_C(0xfeedface));

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x4b410 video_command_submit test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x4b410 video_command_submit (5 writes) verified\n");
    return 0;
}
