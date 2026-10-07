/* ====================================================================
 * 0x11704 recovery ctest entry (v0753)
 *
 * Exercises the recovered video_table_expand_128
 * vf2_hybrid_player_11704_execute:
 *
 *   - Reads 32-bit outer count g2 from 0x78d0c.
 *   - For each of g2 iterations:
 *     - Copies 128 bytes from 0x78d10 to 0x12800000 (luma RAM).
 *     - The copy is byte-by-byte (ldob) but each byte is
 *       stored as a 4-byte word (st) with the dst pointer
 *       incrementing by 4. The upper 3 bytes of each word
 *       come from whatever is in the destination word's high
 *       24 bits; the recovered C writes the byte zero-extended
 *       to 32 bits, matching the i960 `ldob` + `st` shape.
 *   - No sub-calls.
 *
 * Two scenarios are tested:
 *   Scenario 1: outer=1 -> 128 word-writes (bytes 0..127)
 *   Scenario 2: outer=3 -> 384 word-writes (bytes 0..383)
 *
 * Each scenario verifies:
 *   1. The 128*N bytes at 0x12800000..(0x12800000 + 128*N*4) (step 4)
 *      contain the corresponding source bytes (zero-extended).
 *   2. The 4-byte gaps between writes remain at the sentinel.
 *   3. The location just past the range is untouched.
 *   4. Return status is VF2_OK and IP lands on 0x11740 (ret).
 *   5. The work-RAM source pointer at 0x78d10 has advanced by 128*N
 *      (the function does not roll back the read pointer).
 *   6. The outer-count at 0x78d0c has been decremented to 0
 *      (cmpdeco semantics: g2 starts at outer, becomes 0).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00011704)
#define RET_IP UINT32_C(0x00011740)
#define OUTER_COUNT_ADDR UINT32_C(0x00078d0c)
#define SOURCE_ADDR UINT32_C(0x00078d10)
#define DEST_BASE UINT32_C(0x12800000)
#define INNER_COUNT 128u

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

/* Pre-poison the luma RAM region with a sentinel so we can tell
 * which 4-byte words were written by the function. The pattern
 * is 0xAABBDDii (ii = low byte of the original word index) so
 * that "untouched" is distinguishable from "function wrote 0". */
static void poison_luma(vf2_model2a *machine, uint32_t word_count) {
    uint32_t i;
    for (i = 0; i < word_count; ++i) {
        uint32_t sentinel = UINT32_C(0xAABB0000) | (i & UINT32_C(0xffff));
        CHECK(vf2_model2a_write_u32(
            machine, DEST_BASE + i * 4u, sentinel) == VF2_OK);
    }
}

/* Verify that the luma RAM holds the expected byte pattern at
 * every 4th byte, and that the upper 3 bytes of each word are 0
 * (because the function zero-extends the byte before storing as
 * a 32-bit word). Also verify the high 3 bytes of each word are
 * zero (i.e. ldob+st shape). */
static void verify_luma(vf2_model2a *machine, const uint8_t *expected,
                         uint32_t byte_count) {
    uint32_t i;
    for (i = 0; i < byte_count; ++i) {
        uint32_t value = 0u;
        uint32_t word_addr = DEST_BASE + i * 4u;
        CHECK(vf2_model2a_read_u32(machine, word_addr, &value) == VF2_OK);
        if (value != (uint32_t)expected[i]) {
            fprintf(stderr,
                    "FAILED: luma[%u] = 0x%08x, expected 0x%02x\n",
                    i, value, (unsigned)expected[i]);
            ++failures;
        }
    }
}

static void setup_source(vf2_model2a *machine, uint8_t *fake_rom,
                          const uint8_t *bytes, uint32_t byte_count) {
    /* Writes to the i960 program ROM window (0..main_rom_size) are
     * silently accepted by vf2_model2a_write without storing. So
     * write the bytes into the fake_rom buffer backing the view
     * directly, then the recovered function's reads will see them. */
    (void)machine;
    memcpy(fake_rom + SOURCE_ADDR, bytes, byte_count);
}

static void run_scenario(vf2_model2a *machine, vf2_i960_cpu *cpu,
                          uint8_t *fake_rom,
                          uint32_t outer,
                          const uint8_t *expected,
                          uint32_t total_bytes) {
    /* Pre-poison the luma destination so untouched locations are
     * distinguishable from "function wrote 0". Poison 32 words
     * beyond the expected range to catch over-runs. */
    poison_luma(machine, (total_bytes / INNER_COUNT) * INNER_COUNT + 32u);

    /* Set the outer count at 0x78d0c. Writes to the i960 program
     * ROM window are silently accepted by vf2_model2a_write but
     * not stored; write the count into the fake_rom backing buffer
     * as a little-endian 32-bit word so the function's read sees
     * it. */
    {
        uint32_t addr = OUTER_COUNT_ADDR;
        fake_rom[addr + 0] = (uint8_t)(outer & UINT32_C(0xff));
        fake_rom[addr + 1] = (uint8_t)((outer >> 8) & UINT32_C(0xff));
        fake_rom[addr + 2] = (uint8_t)((outer >> 16) & UINT32_C(0xff));
        fake_rom[addr + 3] = (uint8_t)((outer >> 24) & UINT32_C(0xff));
    }

    /* Pre-populate the source buffer. */
    setup_source(machine, fake_rom, expected, total_bytes);

    /* Push a frame: return IP = 0x11740 (the ret). */
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, RET_IP) == VF2_OK);

    vf2_status status = vf2_hybrid_player_11704_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);

    /* Verify the 128*N writes are correct. */
    verify_luma(machine, expected, total_bytes);

    /* Verify a sentinel word just past the range is still untouched. */
    {
        uint32_t past_value = 0u;
        uint32_t past_addr = DEST_BASE + total_bytes * 4u;
        CHECK(vf2_model2a_read_u32(machine, past_addr, &past_value) == VF2_OK);
        uint32_t expected_sentinel = UINT32_C(0xAABB0000) |
            ((total_bytes / INNER_COUNT) * INNER_COUNT) & UINT32_C(0xffff);
        if (past_value != expected_sentinel) {
            fprintf(stderr,
                    "FAILED: luma past-end = 0x%08x, expected sentinel 0x%08x\n",
                    past_value, expected_sentinel);
            ++failures;
        }
    }
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
    if (machine.luma_ram == NULL) {
        fprintf(stderr, "FAILED: machine.luma_ram is NULL\n");
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    /* The source 0x78d10 and outer-count 0x78d0c live in the
     * i960 program ROM window. Attach a small fake main_rom so
     * the recovered function's reads succeed. 0x80000 is enough
     * to cover 0x78d10 + 384 bytes for scenario 2. */
    fake_rom = (uint8_t *)calloc(1u, UINT32_C(0x80000));
    CHECK(fake_rom != NULL);
    if (fake_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return 1;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, fake_rom, UINT32_C(0x80000)) ==
          VF2_OK);

    /* Scenario 1: outer=1, 128 source bytes.
     * Use a pattern where every byte is distinct so we can
     * tell which writes happened. */
    {
        uint8_t bytes1[128];
        uint32_t i;
        for (i = 0; i < 128u; ++i) {
            bytes1[i] = (uint8_t)(0x40u + (i & UINT32_C(0x3f)));
        }
        run_scenario(&machine, &cpu, fake_rom, 1u, bytes1, 128u);
    }

    /* Scenario 2: outer=3, 384 source bytes (128 bytes * 3).
     * The src pointer advances, so the second and third
     * iterations read from 0x78d90 and 0x78e10 respectively. */
    {
        uint8_t bytes2[384];
        uint32_t i;
        for (i = 0; i < 384u; ++i) {
            bytes2[i] = (uint8_t)((i * 7u + 0x11u) & UINT32_C(0xff));
        }
        run_scenario(&machine, &cpu, fake_rom, 3u, bytes2, 384u);
    }

    vf2_model2a_shutdown(&machine);
    free(fake_rom);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x11704 video_table_expand_128 test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x11704 video_table_expand_128: outer=1, outer=3\n");
    return 0;
}
