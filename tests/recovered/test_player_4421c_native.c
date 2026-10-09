/* ====================================================================
 * 0x4421c recovery ctest entry (v0757)
 *
 * Exercises the recovered post_init_floats_helper
 * vf2_hybrid_player_4421c_execute.
 *
 * The function calls 0x1fee4 (v0749) which writes 1.0 floats to
 * 0x50a0e0..0x50a144, then writes 4 fixed constants and one byte.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0004421c)
#define PTR_ADDR UINT32_C(0x00500814)
#define R4_WORK_BASE UINT32_C(0x00580000)

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

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;
    uint32_t off;

    if (vf2_model2a_initialize(&machine) == 0) return 1;

    /* Set r4 base pointer. */
    write_u32_le(&machine, PTR_ADDR, R4_WORK_BASE);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, UINT32_C(0x44268)) == VF2_OK);

    status = vf2_hybrid_player_4421c_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x44268));

    /* Verify init floats 0x50a0e0..0x50a144 are 1.0. */
    for (off = UINT32_C(0x50a0e0); off <= UINT32_C(0x50a144); off += 4u) {
        CHECK(read_u32_le(&machine, off) == UINT32_C(0x3f800000));
    }

    /* Verify the 5 fixed writes. */
    CHECK(read_u32_le(&machine, UINT32_C(0x0050a148))
          == UINT32_C(0xbe99999a));
    CHECK(read_u32_le(&machine, R4_WORK_BASE + UINT32_C(0x20c))
          == UINT32_C(0x3fb33333));
    CHECK(read_u32_le(&machine, R4_WORK_BASE + UINT32_C(0x290))
          == UINT32_C(0x3eb33333));
    CHECK(read_u8(&machine, R4_WORK_BASE + UINT32_C(0x2d1)) == UINT8_C(60));
    CHECK(read_u32_le(&machine, R4_WORK_BASE + UINT32_C(0x4c))
          == UINT32_C(0x3f800000));

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x4421c (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x4421c post_init_floats_helper: 5 writes + 26 init floats\n");
    return 0;
}
