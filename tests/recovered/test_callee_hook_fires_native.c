/* ====================================================================
 * Per-step hook fire verification ctest entry (v0755c)
 *
 * Drives the per-step loop directly with a hand-constructed state
 * and verifies the per-hook fire counters are non-zero after the
 * loop hits a registered hook entry. This is the direct evidence
 * that the per-step hook infrastructure actually fires (vs. the
 * interpretation fallback being used).
 *
 * The test:
 *   1. Sets up a model2a with a fake main_rom that contains a
 *      minimal program at 0x28178..0x28190 that does:
 *        lda 0x00029598, g0   (load the hook entry address)
 *        call 0x00029598      (call the hooked function)
 *        mov 0, g0
 *        b   0x00014400       (jump to the stop address)
 *   2. Sets up a CPU with cpu->ip = 0x28178, frame pushed.
 *   3. Resets the per-hook counters.
 *   4. Calls vf2_hybrid_run_interpreted_until(0x28178, 0x14400).
 *   5. Verifies the result is VF2_OK.
 *   6. Verifies the per-hook counter for 0x29598 is at least 1.
 *   7. Verifies the cpu->ip ends at 0x14400.
 *
 * The fake main_rom is 0x30000 bytes (large enough to cover the
 * 0x28178..0x28190 program plus 0x295ec..0x295fb reads done by
 * 0x29598's path A/B/C).
 *
 * The test exercises the FULL per-step loop with hook support
 * (v0755b's per-step variant), proving that the dispatcher:
 *   - Steps one instruction at a time.
 *   - Detects cpu->ip == 0x29598 (the hook entry).
 *   - Calls vf2_hybrid_player_29598_execute.
 *   - Increments the per-hook counter.
 *   - Continues interpretation from the post-function IP.
 *   - Reaches 0x14400 and returns VF2_OK.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00028178)
#define HOOK_ENTRY UINT32_C(0x00029598)
#define STOP_IP UINT32_C(0x00014400)
#define FAKE_ROM_SIZE UINT32_C(0x30000)
#define HOOK_COUNT 4u

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

/* Build a fake main_rom with the simplest possible program at
 * 0x28178 that calls 0x29598 and reaches 0x14400.
 *
 *   At 0x28178 (5 instructions = 20 bytes):
 *     b 0x28180           -> 4 bytes (skip over the call to 0x28180)
 *                            so the b immediately jumps to 0x28180.
 *     call 0x00029598     -> 4 bytes (0x28180..0x28183)
 *     mov  0, g0          -> 4 bytes (0x28184..0x28187)
 *     b    0x00014400     -> 4 bytes (0x28188..0x2818b)
 *     ret                 -> 4 bytes (0x2818c..0x2818f)
 *
 * The first b jumps to 0x28180 (which is the call instruction).
 * After the call returns, execution continues at 0x28184 (mov),
 * then 0x28188 (b to 0x14400).
 *
 * Encoding notes (i960 CTRL format uses displacement = target - PC):
 *   B 0x28180 from 0x28178: displacement = 0x28180 - 0x28178 = 0x8.
 *     24-bit: 0x000008. Bytes: 10 00 00 08.
 *   CALL 0x29598 from 0x28180: displacement = 0x29598 - 0x28180 = 0x1418.
 *     Bytes: 09 00 14 18.
 *   MOV 0, g0: bytes 5c 80 1e 00.
 *   B 0x14400 from 0x28188: displacement = 0x14400 - 0x28188 = -0x13d88.
 *     24-bit signed: 0xfec278. Bytes: 10 fe c2 78.
 *   RET: bytes 0a 00 00 00.
 */
static void build_fake_rom(uint8_t *rom) {
    /* B 0x28180 from 0x28178 at the start. */
    rom[0x28178 + 0] = 0x10; rom[0x28178 + 1] = 0x00;
    rom[0x28178 + 2] = 0x00; rom[0x28178 + 3] = 0x08;
    /* CALL 0x00029598 from 0x28180. */
    rom[0x28180 + 0] = 0x09; rom[0x28180 + 1] = 0x00;
    rom[0x28180 + 2] = 0x14; rom[0x28180 + 3] = 0x18;
    /* MOV 0, g0 at 0x28184. */
    rom[0x28184 + 0] = 0x5c; rom[0x28184 + 1] = 0x80;
    rom[0x28184 + 2] = 0x1e; rom[0x28184 + 3] = 0x00;
    /* B 0x00014400 from 0x28188. */
    rom[0x28188 + 0] = 0x10; rom[0x28188 + 1] = 0xfe;
    rom[0x28188 + 2] = 0xc2; rom[0x28188 + 3] = 0x78;
    /* RET at 0x2818c (defensive). */
    rom[0x2818c + 0] = 0x0a; rom[0x2818c + 1] = 0x00;
    rom[0x2818c + 2] = 0x00; rom[0x2818c + 3] = 0x00;
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *fake_rom = NULL;
    uint64_t hook_counts[HOOK_COUNT] = {0};
    uint64_t hook_total = 0;

    if (vf2_model2a_initialize(&machine) == 0) {
        fprintf(stderr, "FAILED: vf2_model2a_initialize returned 0\n");
        return 1;
    }
    if (machine.work_ram == NULL) {
        fprintf(stderr, "FAILED: machine.work_ram is NULL\n");
        vf2_model2a_shutdown(&machine);
        return 1;
    }

    fake_rom = (uint8_t *)calloc(1u, FAKE_ROM_SIZE);
    CHECK(fake_rom != NULL);
    if (fake_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return 1;
    }
    build_fake_rom(fake_rom);
    CHECK(vf2_model2a_attach_main_rom(&machine, fake_rom, FAKE_ROM_SIZE) ==
          VF2_OK);

    /* Reset counters so the test scopes its measurement. */
    vf2_hybrid_reset_callee_hook_counts();

    /* Set up the CPU with cpu->ip = 0x28178, frame pushed, g0
     * such that 0x29598 takes path A (bit 4 of g0 clear). The
     * fake program at 0x28178 will LDA 0x29598 into g0 first, but
     * the LDA runs BEFORE the call, so when the call target is
     * reached, g0 is the value LDA'd. We want g0 = 0x29598 (since
     * LDA loads the address into g0). 0x29598 has bit 4 clear, so
     * path A is taken. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    /* Push a frame so the function can pop it on return. */
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, UINT32_C(0x00028180)) == VF2_OK);
    /* Reset IP to the entry (the frame push leaves IP at the
     * entry, but be explicit). */
    cpu.ip = ENTRY_IP;

    /* Run the per-step loop. */
    vf2_status status = vf2_hybrid_run_interpreted_until(
        &machine, &cpu, ENTRY_IP, STOP_IP);

    /* Read the per-hook counters. */
    vf2_hybrid_get_callee_hook_counts(hook_counts, &hook_total);

    if (failures != 0) {
        fprintf(stderr,
                "FAILED setup: per-step hook test (%d failures)\n",
                failures);
        vf2_model2a_shutdown(&machine);
        free(fake_rom);
        return 1;
    }
    if (status != VF2_OK) {
        fprintf(stderr,
                "FAILED: per-step loop returned %d (expected VF2_OK); "
                "ip=0x%08x hook_total=%llu hook_counts=[%llu, %llu, "
                "%llu, %llu]\n",
                (int)status, (unsigned)cpu.ip,
                (unsigned long long)hook_total,
                (unsigned long long)hook_counts[0],
                (unsigned long long)hook_counts[1],
                (unsigned long long)hook_counts[2],
                (unsigned long long)hook_counts[3]);
        vf2_model2a_shutdown(&machine);
        free(fake_rom);
        return 1;
    }
    CHECK(cpu.ip == STOP_IP);
    CHECK(hook_counts[0] >= 1u);
    CHECK(hook_total >= 1u);

    vf2_model2a_shutdown(&machine);
    free(fake_rom);

    if (failures != 0) {
        fprintf(stderr, "FAILED: per-step hook test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: per-step hook fired for 0x29598 (count=%llu total=%llu)\n",
           (unsigned long long)hook_counts[0],
           (unsigned long long)hook_total);
    return 0;
}
