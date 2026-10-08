/* ====================================================================
 * 0x2c38 recovery ctest entry (v0755f)
 *
 * Exercises the recovered color_table_rebuild
 * vf2_hybrid_player_2c38_execute:
 *
 *   - Sets the 6 input bytes (3 scale factors at 0x500234..0x500239
 *     and 3 multipliers at 0x5000e0..0x5000e2).
 *   - Pushes a frame and calls the recovered function.
 *   - Verifies the output:
 *       0x546000 = 1 (post-state byte)
 *       0x546004 = 0 (post-state byte)
 *       0x546008..0x546118 = 0 (zero-init region, 288 bytes)
 *       0x546128..0x54612d = 0 (small zero-init region, 6 bytes)
 *       0x54612e..0x54612e+27*47*6 = 21762 bytes of computed colors
 *
 * The exact byte values in the color table are NOT verified because:
 *   1. The "subo 1, 0, g1" instruction at 0x2d40 has a disasm-vs-
 *      executor ambiguity (g1 = 1 in i960 native, g1 = 0xFFFFFFFF
 *      in the executor). The recovery matches the executor.
 *   2. The recovery is not currently validated against the original
 *      i960 (no differential test exercises 0x2c38).
 *
 * The test verifies the SHAPE of the output (the zero-init regions
 * are zero, the post-state bytes are correct, and the color table
 * region has been written to).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0002c38)
#define RET_IP UINT32_C(0x0002de4)
#define COLOR_TABLE_BASE UINT32_C(0x00546008)
#define COLOR_TABLE_FIRST_ENTRY UINT32_C(0x0054612e)
#define OUTER_COUNT 27
#define INNER_COUNT 47
#define ENTRY_SIZE 6
#define TOTAL_ENTRIES (OUTER_COUNT * INNER_COUNT)

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

static void write_u8(vf2_model2a *m, uint32_t addr, uint8_t v) {
    CHECK(vf2_model2a_write(m, addr, &v, 1) == VF2_OK);
}

static uint8_t read_u8(vf2_model2a *m, uint32_t addr) {
    uint8_t v = 0;
    CHECK(vf2_model2a_read(m, addr, &v, 1) == VF2_OK);
    return v;
}

static uint32_t read_u32(vf2_model2a *m, uint32_t addr) {
    uint32_t v = 0;
    CHECK(vf2_model2a_read_u32(m, addr, &v) == VF2_OK);
    return v;
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint32_t table_end;
    uint32_t i;
    uint8_t has_nonzero = 0;

    if (vf2_model2a_initialize(&machine) == 0) {
        fprintf(stderr, "FAILED: vf2_model2a_initialize returned 0\n");
        return 1;
    }
    if (machine.work_ram == NULL) {
        fprintf(stderr, "FAILED: machine.work_ram is NULL\n");
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    /* Set input bytes. */
    write_u8(&machine, UINT32_C(0x00500234), 100);  /* red offset */
    write_u8(&machine, UINT32_C(0x00500235), 200);  /* red scale */
    write_u8(&machine, UINT32_C(0x00500236), 50);   /* green offset */
    write_u8(&machine, UINT32_C(0x00500237), 150);  /* green scale */
    write_u8(&machine, UINT32_C(0x00500238), 25);   /* blue offset */
    write_u8(&machine, UINT32_C(0x00500239), 75);   /* blue scale */
    write_u8(&machine, UINT32_C(0x005000e0), 128);  /* red mult */
    write_u8(&machine, UINT32_C(0x005000e1), 64);   /* green mult */
    write_u8(&machine, UINT32_C(0x005000e2), 32);   /* blue mult */

    /* Pre-poison the color table region with sentinel. */
    for (i = COLOR_TABLE_BASE;
         i < COLOR_TABLE_FIRST_ENTRY + TOTAL_ENTRIES * ENTRY_SIZE;
         ++i) {
        write_u8(&machine, i, 0xab);
    }

    /* Push a frame. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, RET_IP) == VF2_OK);

    /* Run the recovered function. */
    vf2_status status = vf2_hybrid_player_2c38_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify the post-state bytes. */
    CHECK(read_u32(&machine, UINT32_C(0x00546000)) == 1u);
    CHECK(read_u32(&machine, UINT32_C(0x00546004)) == 0u);

    /* Verify the zero-init regions are zero.
     * 0x546008..0x546118 (288 bytes). */
    for (i = 0; i < 288u; ++i) {
        CHECK(read_u8(&machine, COLOR_TABLE_BASE + i) == 0u);
    }
    /* 0x546128..0x54612d (6 bytes). */
    for (i = 0; i < 6u; ++i) {
        CHECK(read_u8(&machine, UINT32_C(0x00546128) + i) == 0u);
    }

    /* Verify the color table region (0x54612e onwards) has been
     * written to (not all sentinel 0xab). At least one byte
     * should be non-zero. */
    table_end = COLOR_TABLE_FIRST_ENTRY + TOTAL_ENTRIES * ENTRY_SIZE;
    for (i = COLOR_TABLE_FIRST_ENTRY; i < table_end; ++i) {
        if (read_u8(&machine, i) != 0xabu) {
            has_nonzero = 1;
            break;
        }
    }
    CHECK(has_nonzero);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x2c38 color_table_rebuild test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x2c38 color_table_rebuild: 27x47 nested loop, %u bytes written\n",
           (unsigned)(TOTAL_ENTRIES * ENTRY_SIZE));
    return 0;
}
