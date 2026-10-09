/* ====================================================================
 * 0x4ad40 recovery ctest entry (v0760)
 *
 * Exercises the recovered zero_workram_helpers
 * vf2_hybrid_player_4ad40_execute.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0004ad40)
#define RET_IP UINT32_C(0x0004ad74)

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
static void write_u16_le(vf2_model2a *m, uint32_t addr, uint16_t v) {
    CHECK(vf2_model2a_write(m, addr, &v, 2) == VF2_OK);
}
static uint16_t read_u16_le(vf2_model2a *m, uint32_t addr) {
    uint16_t v = 0;
    CHECK(vf2_model2a_read(m, addr, &v, 2) == VF2_OK);
    return v;
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;

    if (vf2_model2a_initialize(&machine) == 0) return 1;

    /* Pre-poison the four targets. */
    write_u16_le(&machine, UINT32_C(0x005502a8), 0xaaaa);
    write_u16_le(&machine, UINT32_C(0x005502b0), 0xbbbb);
    write_u16_le(&machine, UINT32_C(0x005502b8), 0xcccc);
    write_u32_le(&machine, UINT32_C(0x00546000), 0xdeadbeef);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, RET_IP) == VF2_OK);

    status = vf2_hybrid_player_4ad40_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify the four targets are zero. */
    CHECK(read_u16_le(&machine, UINT32_C(0x005502a8)) == 0u);
    CHECK(read_u16_le(&machine, UINT32_C(0x005502b0)) == 0u);
    CHECK(read_u16_le(&machine, UINT32_C(0x005502b8)) == 0u);
    CHECK(read_u32_le(&machine, UINT32_C(0x00546000)) == 0u);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x4ad40 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x4ad40 zero_workram_helpers: 4 work-RAM fields cleared\n");
    return 0;
}
