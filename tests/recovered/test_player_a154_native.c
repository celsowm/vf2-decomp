/* ====================================================================
 * 0xa154 recovery ctest entry (v0759)
 *
 * Exercises the recovered zero_loop_43_dwords
 * vf2_hybrid_player_a154_execute.
 *
 * Pre-poisons 0x501800..0x5018ac with a sentinel, runs the
 * function, and verifies all 43 dwords (172 bytes) are zero.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0000a154)
#define RET_IP UINT32_C(0x0000a174)
#define WRAM_BASE UINT32_C(0x00501800)
/* 43 dwords stepping by 4: last write at 0x501800 + 42*4 = 0x5018a8. */
#define WRAM_END UINT32_C(0x005018a8)
#define BEYOND UINT32_C(0x005018ac)

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

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;
    uint32_t off;

    if (vf2_model2a_initialize(&machine) == 0) return 1;

    /* Pre-poison the 43 dwords with 0xdeadbeef. */
    for (off = WRAM_BASE; off <= WRAM_END; off += 4u) {
        write_u32_le(&machine, off, UINT32_C(0xdeadbeef));
    }
    /* The dword just BEYOND should remain untouched (sentinel). */
    write_u32_le(&machine, BEYOND, UINT32_C(0xcafef00d));

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, RET_IP) == VF2_OK);

    status = vf2_hybrid_player_a154_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify all 43 dwords are zero. */
    for (off = WRAM_BASE; off <= WRAM_END; off += 4u) {
        uint32_t v = read_u32_le(&machine, off);
        if (v != 0u) {
            fprintf(stderr, "0x%x: expected 0, got 0x%x\n", off, v);
            ++failures;
        }
    }
    /* Verify BEYOND is untouched. */
    CHECK(read_u32_le(&machine, BEYOND) == UINT32_C(0xcafef00d));

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0xa154 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0xa154 zero_loop_43_dwords: 43 dwords cleared\n");
    return 0;
}
