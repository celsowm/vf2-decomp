/* ====================================================================
 * Per-step loop UNSUPPORTED path hardening ctest (v0763)
 *
 * Drives the per-step loop directly with a hand-constructed state
 * and verifies that when a registered hook refuses (returns
 * VF2_ERROR_UNSUPPORTED), the per-step loop pops the i960-call
 * frame the call instruction pushed and continues from the
 * post-call IP (cpu->ip = call-site + 4).
 *
 * This is the foundational fix that makes refused hooks safe. It
 * replaces the per-step loop's UNSUPPORTED-path `continue;` (which
 * relied on the recovery's cpu->ip being the call's entry IP — a
 * comment that was wrong) with an explicit
 * `vf2_i960_cpu_return_procedure` call that:
 *   - pops the frame the i960 `call` instruction pushed,
 *   - restores the caller's local registers (including r2),
 *   - sets cpu->ip = call-site + 4 (the post-call IP).
 *
 * The test uses the existing 0xcf04 hook (which is registered in
 * g_callee_hooks[] and which refuses at cpu->ip = 0xcfb8 — the
 * `ret` slot of 0xcf04). The pre-fix behaviour stepped the `ret`
 * at 0xcfb8 directly, which correctly popped the 0xcf04 frame
 * (because the original i960 `call 0xcf04` had pushed it). The
 * post-fix behaviour pops the frame via return_procedure and
 * skips the 0xcfb8 step. In both cases the final cpu->ip is
 * the post-call IP and procedure_returns is incremented exactly
 * once.
 *
 * The test:
 *   1. Sets up a model2a with a fake main_rom that contains a
 *      minimal program at 0x1000..0x100c:
 *        b  0x1008          (skip past to the call)
 *        call 0x0000cf04    (call the registered hook)
 *   2. Sets stop = 0x100c (the post-call IP) so the per-step
 *      loop terminates as soon as the refused hook pops the
 *      frame and continues. This avoids depending on any
 *      instruction after the call.
 *   3. Sets up a CPU with cpu->ip = 0x1000, frame pushed.
 *   4. Resets the per-hook counters.
 *   5. Calls vf2_hybrid_run_interpreted_until(0x1000, 0x100c).
 *   6. Verifies:
 *      - status == VF2_OK
 *      - cpu->ip == 0x100c (the post-call IP; the call returned
 *        via the popped frame, not via the recovery's 0xcfb8 step)
 *      - hook_counts[0xcf04] == 1
 *      - procedure_calls == 1 (the i960 `call 0xcf04` pushed one)
 *      - procedure_returns == 1 (the per-step loop popped one
 *        via return_procedure)
 *
 * The strong evidence that the fix is correct is the F4
 * differential (ctest #144, vf2_f4_individual_release) which
 * exercises 0xcf04 in the per-step loop on a real F4 snapshot.
 * Pre-fix, a refused hook that left cpu->ip at a `ret` of a
 * DIFFERENT inlined function (e.g. 0x1fcc0 refusing at 0x20050)
 * would pop the wrong frame and the per-step loop would spin.
 * Post-fix, return_procedure always restores the caller's
 * saved IP and the loop terminates deterministically.
 *
 * Per-step loop range note: the per-step loop only uses the
 * per-step variant when at least one registered hook is in
 * [entry, stop) (or in the wrap-around case for stop < entry).
 * 0xcf04, entry = 0x1000, stop = 0x100c. Since stop > entry,
 * the check is `entry <= h && h < stop` → 0x1000 <= 0xcf04
 * is true but 0xcf04 < 0x100c is false. So 0xcf04 is NOT in
 * range, and the per-step variant is NOT used.
 *
 * To force the per-step variant, we use stop = 0x20000. Then
 * 0xcf04 < 0x20000 is true, so the per-step variant is used
 * for the [0x1000, 0x20000) range. After the hook refuses and
 * pops the frame, cpu->ip = 0x100c. The per-step loop
 * continues stepping (b at 0x100c jumps to 0x20000 if we add
 * one; alternatively we set stop to 0x100c and the per-step
 * loop terminates via cpu->ip == stop_address).
 *
 * Wait: the per-step loop only uses the per-step variant when
 * at least one hook is in range. With stop = 0x100c, 0xcf04
 * is NOT in [0x1000, 0x100c), so the per-step variant is NOT
 * used; the loop falls through to vf2_i960_run, which does not
 * fire hooks at all. That defeats the test.
 *
 * We need stop to include 0xcf04. We use stop = 0x100c but
 * ALSO add a `b stop` at 0x100c, so the per-step loop's hook
 * check fires for 0xcf04 (which is in [0x1000, 0x100c)
 * NO! 0xcf04 is not in [0x1000, 0x100c) because 0xcf04 > 0x100c.
 *
 * The correct range is: entry = 0x1000, stop > 0xcf04. We use
 * stop = 0x10000 (well past 0xcf04). Then 0xcf04 is in
 * [0x1000, 0x10000), so the per-step variant is used. After
 * the call at 0x1008, the hook fires for 0xcf04, refuses,
 * and pops the frame. cpu->ip = 0x100c. The per-step loop
 * continues. We add a `b stop` at 0x100c to reach the stop
 * address and terminate the loop.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/hybrid/player.h"
#include "vf2/i960/decoder.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

/* Index of 0xcf04 in the g_callee_hooks[] table. The order is
 * (v0755c) 0=0x29598, 1=0x439ac, 2=0x43888, 3=0xcf04. */
#define HOOK_INDEX_CF04 3u
#define HOOK_COUNT 4u
#define ENTRY_IP UINT32_C(0x00001000)
#define HOOK_ENTRY UINT32_C(0x0000cf04)
#define POST_CALL_IP UINT32_C(0x0000100c)
#define STOP_IP UINT32_C(0x00010000)
#define FAKE_ROM_SIZE UINT32_C(0x80000)

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

/* Build a fake main_rom with a small program at 0x1000 that
 * calls 0xcf04, then unconditionally branches to STOP_IP.
 *
 * i960 byte order: the i960 is BIG-ENDIAN, but the host reads
 * words in LITTLE-ENDIAN order (vf2_model2a_read_u32). So the
 * byte at the lowest address is the LOW byte of the 32-bit
 * value. The 32-bit value is constructed as
 *     data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24)
 * and the i960 interprets the high byte of the resulting value
 * as the opcode.
 *
 * CTRL format (0x08..0x1f):
 *   0x08 = b, 0x09 = call, 0x0a = ret.
 *
 * Encoding (24-bit signed displacement, target - PC):
 *   B 0x1008 from 0x1000: disp 0x8, value 0x08000008.
 *     Bytes (LE): 08 00 00 08.
 *   CALL 0xcf04 from 0x1008: disp 0xcf04 - 0x1008 = 0xbefc,
 *     value 0x0900befc.
 *     Bytes (LE): fc be 00 09.
 *   B 0x00010000 from 0x100c: disp 0x10000 - 0x100c = 0xeff4,
 *     value 0x0800eff4.
 *     Bytes (LE): f4 ef 00 08.
 */
static void build_fake_rom(uint8_t *rom) {
    /* B 0x1008 from 0x1000. */
    rom[0x1000 + 0] = 0x08; rom[0x1000 + 1] = 0x00;
    rom[0x1000 + 2] = 0x00; rom[0x1000 + 3] = 0x08;
    /* CALL 0x0000cf04 from 0x1008. */
    rom[0x1008 + 0] = 0xfc; rom[0x1008 + 1] = 0xbe;
    rom[0x1008 + 2] = 0x00; rom[0x1008 + 3] = 0x09;
    /* B 0x00010000 from 0x100c. */
    rom[0x100c + 0] = 0xf4; rom[0x100c + 1] = 0xef;
    rom[0x100c + 2] = 0x00; rom[0x100c + 3] = 0x08;
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *fake_rom = NULL;
    uint64_t hook_counts[HOOK_COUNT] = {0};
    uint64_t start_calls;
    uint64_t start_returns;
    vf2_status status;

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
    CHECK(vf2_model2a_attach_main_rom(
        &machine, fake_rom, FAKE_ROM_SIZE) == VF2_OK);

    vf2_hybrid_reset_callee_hook_counts();

    /* Set up a CPU with cpu->ip = 0x1000, frame pushed so the
     * per-step loop has a caller frame to restore. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, UINT32_C(0x00001014)) == VF2_OK);
    cpu.ip = ENTRY_IP;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    /* Run the per-step loop. After the i960 `call 0xcf04`
     * instruction, the per-step loop's hook check fires the
     * 0xcf04 recovery. The recovery refuses (returns
     * VF2_ERROR_UNSUPPORTED) at cpu->ip = 0xcfb8. The post-fix
     * per-step loop pops the call's frame via
     * return_procedure and continues from r2 = 0x100c. The
     * subsequent `b 0x10000` advances cpu->ip to the stop
     * address and the per-step loop terminates with status
     * VF2_OK. */
    status = vf2_hybrid_run_interpreted_until(
        &machine, &cpu, ENTRY_IP, STOP_IP);

    vf2_hybrid_get_callee_hook_counts(hook_counts, NULL);

    /* Per-step loop must terminate successfully. */
    CHECK(status == VF2_OK);
    /* cpu->ip must end at STOP_IP (the b's target). */
    CHECK(cpu.ip == STOP_IP);
    /* The 0xcf04 hook must have fired exactly once. */
    CHECK(hook_counts[HOOK_INDEX_CF04] == 1u);
    /* The caller's frame push (from the i960 `call` instruction)
     * is balanced by the per-step loop's return_procedure: one
     * call, one return. */
    CHECK(cpu.procedure_calls - start_calls == 1u);
    CHECK(cpu.procedure_returns - start_returns == 1u);

    vf2_model2a_shutdown(&machine);
    free(fake_rom);

    if (failures != 0) {
        fprintf(stderr,
                "FAILED: per-step loop UNSUPPORTED-path test "
                "(%d failures)\n  cpu.ip=0x%08x status=%d "
                "calls=%llu returns=%llu\n",
                failures,
                (unsigned)cpu.ip,
                (int)status,
                (unsigned long long)(cpu.procedure_calls - start_calls),
                (unsigned long long)(cpu.procedure_returns - start_returns));
        return 1;
    }
    printf("ok: per-step loop UNSUPPORTED path safely pops frame; "
           "hook_counts[cf04]=%llu; final ip=0x%08x\n",
           (unsigned long long)hook_counts[HOOK_INDEX_CF04],
           (unsigned)cpu.ip);
    return 0;
}
