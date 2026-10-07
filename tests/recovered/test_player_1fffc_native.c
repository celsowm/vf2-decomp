/* ====================================================================
 * 0x1fffc recovery ctest entry (v0751)
 *
 * Exercises the recovered display_color_profile_apply
 * vf2_hybrid_player_1fffc_execute:
 *
 *   - Reads 0x500064 (mode byte) and 0x500068 (control word).
 *   - If bit 21 of 0x500068 clear, uses 0x500064 as the index.
 *     If bit 21 set, uses 3 as the index.
 *   - Index is shifted left by 8 to get a table offset.
 *   - Reads 3 bytes from the ROM table at 0x6eeb8 + offset.
 *   - Stores the 3 bytes to 0x5000e0 / 0x5000e1 / 0x5000e2.
 *   - REFUSES the sub-call to 0x2c38 (color_table_rebuild).
 *
 * Two paths are tested:
 *   Path A: bit 21 of 0x500068 clear -> uses 0x500064 as index
 *   Path B: bit 21 of 0x500068 set   -> uses 3 as index
 *
 * Each path verifies:
 *   1. The 3 stores to 0x5000e0..0x5000e2 match the table bytes.
 *   2. The return status is VF2_ERROR_UNSUPPORTED (refused).
 *   3. The cpu->ip lands on 0x20050 (the ret, after the call).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0001fffc)
#define REFUSE_IP UINT32_C(0x00020050)  /* ret, after the sub-call (interpreted) */

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

static void setup_cpu(vf2_i960_cpu *cpu) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, REFUSE_IP) == VF2_OK);
}

static void run_path_a(vf2_model2a *machine, vf2_i960_cpu *cpu,
                        uint8_t *fake_rom) {
    /* Path A: bit 21 of 0x500068 clear. Index = 0x500064 = 5.
     * Offset = 5 << 8 = 0x500. */
    setup_cpu(cpu);
    /* 0x500064 = 5 */
    uint8_t mode = 5u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1) ==
          VF2_OK);
    /* 0x500068 with bit 21 clear. Use 0xffdfffff (bit 21 = 0). */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0xffdfffff)) == VF2_OK);
    /* Pre-populate the fake rom with sentinel values at the
     * expected table offsets: 0x6eeb8 + 0x500 = 0x6f3b8, 0x6f3b9, 0x6f3ba. */
    fake_rom[0x6f3b8] = UINT8_C(0xa1);
    fake_rom[0x6f3b9] = UINT8_C(0xb2);
    fake_rom[0x6f3ba] = UINT8_C(0xc3);
    vf2_status status = vf2_hybrid_player_1fffc_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->ip == REFUSE_IP);
    /* Verify 0x5000e0..0x5000e2. */
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x005000e0), &v, 1) == VF2_OK);
    CHECK(v == UINT8_C(0xa1));
    v = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x005000e1), &v, 1) == VF2_OK);
    CHECK(v == UINT8_C(0xb2));
    v = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x005000e2), &v, 1) == VF2_OK);
    CHECK(v == UINT8_C(0xc3));
}

static void run_path_b(vf2_model2a *machine, vf2_i960_cpu *cpu,
                        uint8_t *fake_rom) {
    /* Path B: bit 21 of 0x500068 set. Index = 3.
     * Offset = 3 << 8 = 0x300. */
    setup_cpu(cpu);
    /* 0x500064 = 0 (doesn't matter for path B, but set anyway). */
    uint8_t mode = 0u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1) ==
          VF2_OK);
    /* 0x500068 with bit 21 set. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0x00200000)) == VF2_OK);
    /* Pre-populate the fake rom at 0x6eeb8 + 0x300 = 0x6f1b8, etc. */
    fake_rom[0x6f1b8] = UINT8_C(0x11);
    fake_rom[0x6f1b9] = UINT8_C(0x22);
    fake_rom[0x6f1ba] = UINT8_C(0x33);
    vf2_status status = vf2_hybrid_player_1fffc_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->ip == REFUSE_IP);
    /* Verify 0x5000e0..0x5000e2. */
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x005000e0), &v, 1) == VF2_OK);
    CHECK(v == UINT8_C(0x11));
    v = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x005000e1), &v, 1) == VF2_OK);
    CHECK(v == UINT8_C(0x22));
    v = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x005000e2), &v, 1) == VF2_OK);
    CHECK(v == UINT8_C(0x33));
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *fake_rom = NULL;

    if (vf2_model2a_initialize(&machine) == 0) {
        fprintf(stderr, "FAILED: vf2_model2a_initialize returned 0\n");
        return 1;
    }
    if (machine.work_ram == NULL) {
        fprintf(stderr, "FAILED: machine.work_ram is NULL\n");
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    /* Attach a small fake main_rom so the table reads at
     * 0x6eeb8 + offset succeed. */
    fake_rom = (uint8_t *)calloc(1u, UINT32_C(0x80000));
    CHECK(fake_rom != NULL);
    if (fake_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return 1;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, fake_rom, UINT32_C(0x80000)) ==
          VF2_OK);

    run_path_a(&machine, &cpu, fake_rom);
    run_path_b(&machine, &cpu, fake_rom);

    vf2_model2a_shutdown(&machine);
    free(fake_rom);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x1fffc color profile test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x1fffc paths A (mode 5) / B (mode 3) + sub-call REFUSED\n");
    return 0;
}
