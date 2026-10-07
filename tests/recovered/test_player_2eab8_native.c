/* ====================================================================
 * 0x2eab8 recovery ctest entry (v0754)
 *
 * Exercises the recovered display_runtime_initialize
 * vf2_hybrid_player_2eab8_execute:
 *
 *   - Reads *0x500814 -> r3 (a work-RAM struct base).
 *   - Reads *0x50084c -> r4 (a work-RAM struct base, used by
 *     the inlined 0x31004 sub-call).
 *   - Performs ~36 writes to (r3 + offset), 3 writes to absolute
 *     addresses 0x50a160..0x168, 1 zero-byte at 0x50a14d, and
 *     6 writes to (r4 + offset) for the inlined sub-call.
 *   - No refused sub-calls; the sub-call to 0x31004 is fully
 *     inlined.
 *
 * The test verifies:
 *   1. *0x500814 is consumed as the r3 base.
 *   2. *0x50084c is consumed as the r4 base.
 *   3. All expected writes land at the expected addresses.
 *   4. Return status is VF2_OK and IP lands on 0x2ec20.
 *
 * r3 and r4 are pointed at work RAM regions: 0x580000 and
 * 0x590000 respectively. These are well within the work RAM
 * range (0x500000..0x5FFFFF) so the reads/writes succeed
 * directly.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0002eab8)
#define RET_IP UINT32_C(0x0002ec20)
#define R3_PTR_ADDR UINT32_C(0x00500814)
#define R4_PTR_ADDR UINT32_C(0x0050084c)
#define R3_BASE UINT32_C(0x00580000)  /* work RAM region for r3 */
#define R4_BASE UINT32_C(0x00590000)  /* work RAM region for r4 */

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

    if (vf2_model2a_initialize(&machine) == 0) {
        fprintf(stderr, "FAILED: vf2_model2a_initialize returned 0\n");
        return 1;
    }
    if (machine.work_ram == NULL) {
        fprintf(stderr, "FAILED: machine.work_ram is NULL\n");
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    /* Pre-poison the byte at (R3_BASE + 0xdf) with bit 0 SET,
     * so we can verify that the clrbit 0 of that byte takes
     * effect. (Without the poison, the byte starts at 0 and the
     * clrbit is a no-op on a clear bit.) */
    write_u8(&machine, R3_BASE + 0xdf, 0xab);  /* bit 0 set */

    /* Set r3 = R3_BASE and r4 = R4_BASE. */
    write_u32_le(&machine, R3_PTR_ADDR, R3_BASE);
    write_u32_le(&machine, R4_PTR_ADDR, R4_BASE);

    /* Push a frame: return IP = 0x2ec20 (the ret). */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, ENTRY_IP, RET_IP) == VF2_OK);

    vf2_status status = vf2_hybrid_player_2eab8_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify the 3 work-RAM stores at 0x50a160..0x168. */
    CHECK(read_u32_le(&machine, UINT32_C(0x0050a160)) == UINT32_C(0xc0900000));
    CHECK(read_u32_le(&machine, UINT32_C(0x0050a164)) == UINT32_C(0x3dcccccd));
    CHECK(read_u32_le(&machine, UINT32_C(0x0050a168)) == UINT32_C(0x3dcccccd));

    /* Verify the zero byte at 0x50a14d. */
    CHECK(read_u8(&machine, UINT32_C(0x0050a14d)) == UINT8_C(0));

    /* Verify the clrbit 0 at (R3_BASE + 0xdf). The pre-poison was
     * 0xab; bit 0 is 1, so clrbit 0 should make it 0xaa. */
    CHECK(read_u8(&machine, R3_BASE + 0xdf) == UINT8_C(0xaa));

    /* Verify the byte at (R3_BASE + 0x27c) is 0. */
    CHECK(read_u8(&machine, R3_BASE + 0x27c) == UINT8_C(0));

    /* Verify the word-stores. */
    CHECK(read_u32_le(&machine, R3_BASE + 0x234) == UINT32_C(0x3c872b02));
    CHECK(read_u32_le(&machine, R3_BASE + 0x238) == UINT32_C(0x3ca3d70a));
    CHECK(read_u32_le(&machine, R3_BASE + 0x264) == UINT32_C(0x409851ec));
    CHECK(read_u32_le(&machine, R3_BASE + 0x268) == UINT32_C(0x40d051ec));
    CHECK(read_u32_le(&machine, R3_BASE + 0x2c4) == UINT32_C(0x3e19999a));
    CHECK(read_u32_le(&machine, R3_BASE + 0x2cc) == UINT32_C(0xbcf5c28f));

    /* Verify the zero words. */
    CHECK(read_u32_le(&machine, R3_BASE + 0x240) == UINT32_C(0));
    CHECK(read_u32_le(&machine, R3_BASE + 0x2bc) == UINT32_C(0));
    CHECK(read_u32_le(&machine, R3_BASE + 0x2c0) == UINT32_C(0));
    CHECK(read_u32_le(&machine, R3_BASE + 0x2c8) == UINT32_C(0));

    /* Verify the short stores. */
    CHECK(read_u16_le(&machine, R3_BASE + 0x23c) == UINT16_C(13));
    CHECK(read_u16_le(&machine, R3_BASE + 0x260) == UINT16_C(0));
    CHECK(read_u16_le(&machine, R3_BASE + 0x26c) == UINT16_C(0));
    CHECK(read_u16_le(&machine, R3_BASE + 0x26e) == UINT16_C(0));
    CHECK(read_u16_le(&machine, R3_BASE + 0x244) == UINT16_C(0));

    /* Verify the byte stores. */
    CHECK(read_u8(&machine, R3_BASE + 0x23e) == UINT8_C(88));
    CHECK(read_u8(&machine, R3_BASE + 0x246) == UINT8_C(0xff));
    CHECK(read_u8(&machine, R3_BASE + 0x23f) == UINT8_C(0));
    CHECK(read_u8(&machine, R3_BASE + 0x27d) == UINT8_C(0));
    CHECK(read_u8(&machine, R3_BASE + 0x27e) == UINT8_C(0));
    CHECK(read_u8(&machine, R3_BASE + 0x27f) == UINT8_C(0));

    /* Verify the 6 zero shorts at 0x2b0..0x2ba. */
    {
        uint32_t off;
        for (off = 0x2b0u; off <= 0x2bau; off += 2u) {
            CHECK(read_u16_le(&machine, R3_BASE + off) == UINT16_C(0));
        }
    }

    /* Verify the inlined 0x31004 sub-call writes to (R4_BASE + ...). */
    CHECK(read_u32_le(&machine, R4_BASE + 0x40) == UINT32_C(0));
    CHECK(read_u32_le(&machine, R4_BASE + 0x54) == UINT32_C(0x40c00000));
    CHECK(read_u32_le(&machine, R4_BASE + 0x58) == UINT32_C(0x40966666));
    CHECK(read_u32_le(&machine, R4_BASE + 0x60) == UINT32_C(0));
    CHECK(read_u32_le(&machine, R4_BASE + 0x64) == UINT32_C(0));
    CHECK(read_u32_le(&machine, R4_BASE + 0x70) == UINT32_C(0));

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x2eab8 display_runtime_initialize test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x2eab8 display_runtime_initialize: 1 block, 0x31004 inlined\n");
    return 0;
}
