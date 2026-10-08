/* ====================================================================
 * v0755h: 0x2c38 saturation ambiguity test
 *
 * Targets the `subo 1, 0, g1` instruction at 0x2d40 / 0x2d6c /
 * 0x2d98 of 0x2c38 (color_table_rebuild). The disasm-vs-executor
 * ambiguity documented in v0755f:
 *
 *   - vf2i960 disasm convention: `subo 1, 0, g1` -> g1 = 1 - 0 = 1.
 *   - executor convention:       `subo 1, 0, g1` -> g1 = 0 - 1 = 0xFFFFFFFF.
 *
 * The recovery in `hybrid_execute_player_2c38` matches the EXECUTOR
 * (g1 = 0xFFFFFFFF when g1 >= 256 reaches the saturation sub-call).
 * This test triggers that path and verifies the recovery's output
 * at the saturation pixel against BOTH possible outcomes.
 *
 * If executor semantics is correct (g1 = 0xFFFFFFFF):
 *   store = saturate(r3 * g1) >> 7,
 *   where r3 = 0x5000e0 byte = some positive value,
 *   g1 = 0xFFFFFFFF,
 *   r3 * g1 = (r3 << 32) - r3 = some big number,
 *   >> 7 = very large value clipped to uint16 = 0xFFFF.
 *
 * If disasm semantics is correct (g1 = 1):
 *   r3 * 1 = r3,
 *   >> 7 = r3 >> 7.
 *   For r3 = 128: r3 >> 7 = 1.
 *
 * This is a unit-test of the recovery's behavior at a known
 * disassembly position; it is NOT a differential test against
 * the original i960 (no such test exists for 0x2c38). The test
 * PASSES as long as the recovery produces SOMETHING consistent
 * with one of the two interpretations AND that interpretation
 * matches the recovery's documentation.
 *
 * Inputs to force saturation on the FIRST iteration:
 *   0x500235 (red scale byte) = 255   -> r7_in_func = 28*255/18 = 396
 *   0x500234 (red offset byte) = 255  -> r9_in_func = 255
 *   0x5000e0 (red mult byte) = 255
 *   r5 starts at 1 (per the disasm).
 *
 * Trace for outer_iter=0, inner_iter=46 (last iteration, r6
 * reaches 16+31-1=46):
 *   r8 starts at 0
 *   After addo r7=396, r8, r8 (47 times): r8 = 47*396 = 18612
 *   g1 = r8 >> 8 = 72
 *   g1 += r9 = 72 + 255 = 327
 *   327 >= 256 (r3 = 1<<8) -> SATURATION TRIGGERED
 *
 * The recovery writes a 16-bit value to (r4) at this point.
 * Expected per executor:  0xFFFF (saturated to uint16 max)
 * Expected per disasm:   0x0001 (g1=1, 255*1>>7 = 1)
 *
 * This test verifies the EXECUTOR's interpretation (matches
 * the v0755f recovery's documented behavior).
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

/* Output base address where 0x2c38 starts writing color entries.
 * The recovery writes 27*47=1269 entries (each 6 bytes) starting
 * at 0x54612e. The LAST entry is at 0x54612e + 1268*6 = 0x54612e
 * + 7608 = 0x547fa6. The very first entry (outer=0, inner=0) is at
 * 0x54612e. */
#define COLOR_TABLE_FIRST_ENTRY UINT32_C(0x0054612e)
#define LAST_ENTRY_OFFSET_BYTES (1268u * 6u)
#define LAST_ENTRY_ADDR (COLOR_TABLE_FIRST_ENTRY + LAST_ENTRY_OFFSET_BYTES)

/* The first outer iteration's saturation-triggering pixel is
 * the LAST entry in that row (inner = 46). */
#define FIRST_ROW_LAST_INNER (COLOR_TABLE_FIRST_ENTRY + 46u * 6u)

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

static uint16_t read_u16_le(vf2_model2a *m, uint32_t addr) {
    uint16_t v = 0u;
    CHECK(vf2_model2a_read(m, addr, &v, 2) == VF2_OK);
    return v;
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint16_t red_value;
    uint16_t green_value;
    uint16_t blue_value;

    if (vf2_model2a_initialize(&machine) == 0) {
        fprintf(stderr, "FAILED: vf2_model2a_initialize returned 0\n");
        return 1;
    }

    /* Force saturation. Use maximal scale byte (255 -> r7=396) and
     * maximal offset byte (255 -> r9=255) so the inner loop's
     * running sum quickly exceeds the 256 threshold. */
    write_u8(&machine, UINT32_C(0x00500235), 255);  /* red scale */
    write_u8(&machine, UINT32_C(0x00500234), 255);  /* red offset */
    write_u8(&machine, UINT32_C(0x00500237), 255);  /* green scale */
    write_u8(&machine, UINT32_C(0x00500236), 255);  /* green offset */
    write_u8(&machine, UINT32_C(0x00500239), 255);  /* blue scale */
    write_u8(&machine, UINT32_C(0x00500238), 255);  /* blue offset */

    /* Use a mult byte that distinguishes the two interpretations:
     *   disasm (g1=1): store = (255 * 1) >> 7 = 1.
     *   executor (g1=0xFFFFFFFF): store = (255 * 0xFFFFFFFF) >> 7
     *                              = (very large) clipped to 0xFFFF. */
    write_u8(&machine, UINT32_C(0x005000e0), 255);
    write_u8(&machine, UINT32_C(0x005000e1), 255);
    write_u8(&machine, UINT32_C(0x005000e2), 255);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, RET_IP) == VF2_OK);

    vf2_status status = vf2_hybrid_player_2c38_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Read the first row's last inner entry (the first saturation
     * point). All three channels (red/green/blue) should saturate
     * to one of two values. */
    red_value = read_u16_le(&machine, FIRST_ROW_LAST_INNER);
    green_value = read_u16_le(&machine, FIRST_ROW_LAST_INNER + 2u);
    blue_value = read_u16_le(&machine, FIRST_ROW_LAST_INNER + 4u);

    fprintf(stderr, "v0755h saturation test:\n");
    fprintf(stderr, "  first_row_last_inner red   = 0x%04x\n", red_value);
    fprintf(stderr, "  first_row_last_inner green = 0x%04x\n", green_value);
    fprintf(stderr, "  first_row_last_inner blue  = 0x%04x\n", blue_value);

    /* The recovery matches the EXECUTOR (g1=0xFFFFFFFF). With
     * r3 = 255 and g1 = 0xFFFFFFFF:
     *   r3 * g1 = 0xFFFFFF01
     *   >> 7    = 0x01FFFFFE
     *   & 0xFFFF = 0xFFFE.
     * If the disasm interpretation were correct (g1 = 1):
     *   r3 * g1 = 255
     *   >> 7    = 1.
     *   & 0xFFFF = 0x0001. */
    if (red_value == 0xFFFEu) {
        fprintf(stderr, "  matches EXECUTOR interpretation (g1=0xFFFFFFFF)\n");
    } else if (red_value == 0x0001u) {
        fprintf(stderr,
            "  WARNING: matches DISASM interpretation (g1=1) -- "
            "v0755f recovery may be inconsistent with executor\n");
        ++failures;
    } else {
        fprintf(stderr,
            "  UNEXPECTED: 0x%04x != 0xFFFE (executor) or 0x0001 (disasm)\n",
            red_value);
        ++failures;
    }
    CHECK(green_value == 0xFFFEu);
    CHECK(blue_value == 0xFFFEu);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x2c38 saturation ambiguity test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x2c38 saturation test: red/green/blue = 0xFFFE "
           "(matches EXECUTOR semantics, g1=0xFFFFFFFF then 255*g1>>7)\n");
    return 0;
}
