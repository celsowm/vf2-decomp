/* ====================================================================
 * Per-step hook counter ctest entry (v0755c)
 *
 * Verifies that the per-hook fire counters exposed by
 * vf2_hybrid_get_callee_hook_counts are:
 *   1. Readable (the getter doesn't crash and returns sensible
 *      values).
 *   2. Zero on a fresh process (no hook has fired yet).
 *   3. Reset to zero by vf2_hybrid_reset_callee_hook_counts.
 *
 * This is a minimal sanity check on the instrumentation. The
 * strong evidence that the hook actually fires in a real
 * dispatcher chain is ctest #133 (vf2_f4_individual_release,
 * v0734l) and ctests #109/#110 (vf2_native_(fifth|sixth)_dispatch):
 * all PASS with the per-step hook infrastructure in place, and
 * the 0x28178..0x14400 range contains the 4 hook entries.
 *
 * The full integration test (verify each specific hook fires in
 * a specific F4 leg) is the next slice (v0755c+).
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid.h"

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

int main(void) {
    uint64_t counts[HOOK_COUNT] = {0};
    uint64_t total = UINT64_C(0xdeadbeef);
    size_t i;

    /* Step 1: read counters on a fresh process. They should be 0
     * for all 4 hooks. The function must not crash. */
    memset(counts, 0xff, sizeof(counts));
    vf2_hybrid_get_callee_hook_counts(counts, &total);
    for (i = 0; i < HOOK_COUNT; ++i) {
        CHECK(counts[i] == 0u);
    }
    CHECK(total == 0u);

    /* Step 2: simulate 3 fires of hook 0 and 1 fire of hook 2 by
     * direct memory poke is not possible (the counters are static
     * inside hybrid.c). We can only verify the getter and resetter
     * are functional. Manually advancing counters would require
     * running the per-step loop with a hook entry, which requires
     * a real i960 program in ROM (out of scope for this unit test).
     *
     * Instead, we verify that vf2_hybrid_reset_callee_hook_counts
     * is callable and the getter still returns 0 after. */

    /* Step 3: reset the counters and verify they are still 0. */
    vf2_hybrid_reset_callee_hook_counts();
    memset(counts, 0xff, sizeof(counts));
    total = UINT64_C(0xdeadbeef);
    vf2_hybrid_get_callee_hook_counts(counts, &total);
    for (i = 0; i < HOOK_COUNT; ++i) {
        CHECK(counts[i] == 0u);
    }
    CHECK(total == 0u);

    /* Step 4: verify out_total may be NULL. */
    vf2_hybrid_get_callee_hook_counts(counts, NULL);
    /* If we got here, NULL out_total is accepted. */

    if (failures != 0) {
        fprintf(stderr,
                "FAILED: per-step hook counter test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: per-step hook counters: getter + resetter + NULL out_total all work\n");
    return 0;
}
