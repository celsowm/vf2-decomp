/* ====================================================================
 * 0x1ff0c recovery ctest entry (v0750)
 *
 * Exercises the recovered display_profile_mode_constants
 * vf2_hybrid_player_1ff0c_execute:
 *
 *   - Calls 0x1fee4 (26-iter init) — delegated to the recovered
 *     0x1fee4 function.
 *   - Reads byte at 0x500064.
 *   - Path A: byte == 10 -> write 0x3f0f5c29 to 0x50a124,
 *                       write 0x3ef0a3d7 to 0x50a128
 *   - Path B: byte == 6  -> write 12 floats to 0x50a0e4..0x50a134
 *                       (10 contiguous pairs + 2 standalone)
 *   - Path C: default   -> ret (no writes)
 *
 * Each path is verified by:
 *   1. Initialising a Model 2A + i960 CPU.
 *   2. Pre-poisoning the write addresses with sentinels.
 *   3. Setting 0x500064 to the path-specific value.
 *   4. Using vf2_i960_cpu_enter_procedure to push a fake frame.
 *   5. Calling vf2_hybrid_player_1ff0c_execute.
 *   6. Asserting the return status, IP, and the writes.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0001ff0c)
#define RET_A_IP UINT32_C(0x0001ff44)
#define RET_B_IP UINT32_C(0x0001fff8)
#define RET_C_IP UINT32_C(0x0001ff20)

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

static void setup_cpu(vf2_i960_cpu *cpu, uint32_t ret_ip) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, ret_ip) == VF2_OK);
}

static void poison_writes(vf2_model2a *machine) {
    /* Pre-poison the write addresses with 0xdeadbeef so we can
     * verify the function actually wrote the float constants. */
    uint32_t addresses[12] = {
        0x0050a0e4, 0x0050a0e8, 0x0050a0f0, 0x0050a0f8,
        0x0050a100, 0x0050a118, 0x0050a11c, 0x0050a124,
        0x0050a128, 0x0050a12c, 0x0050a134, 0x0050a0e0
    };
    size_t i;
    for (i = 0; i < 12u; ++i) {
        CHECK(vf2_model2a_write_u32(
            machine, addresses[i], UINT32_C(0xdeadbeef)) == VF2_OK);
    }
}

static void run_path_a(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path A: 0x500064 == 10 -> 2 floats (after 0x1fee4 init). */
    setup_cpu(cpu, RET_A_IP);
    poison_writes(machine);
    /* 0x500064 = 10. */
    uint8_t mode = 10u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1) ==
          VF2_OK);
    vf2_status status = vf2_hybrid_player_1ff0c_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_A_IP);
    /* 0x50a124 = 0x3f0f5c29, 0x50a128 = 0x3ef0a3d7. */
    uint32_t v = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x0050a124), &v) == VF2_OK);
    CHECK(v == UINT32_C(0x3f0f5c29));
    v = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x0050a128), &v) == VF2_OK);
    CHECK(v == UINT32_C(0x3ef0a3d7));
    /* 0x50a0e4 should be 0x3f800000 (from 0x1fee4 init, not from
     * path A which doesn't touch it). */
    v = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x0050a0e4), &v) == VF2_OK);
    CHECK(v == UINT32_C(0x3f800000));
}

static void run_path_b(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path B: 0x500064 == 6 -> 12 floats (after 0x1fee4 init). */
    setup_cpu(cpu, RET_B_IP);
    poison_writes(machine);
    uint8_t mode = 6u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1) ==
          VF2_OK);
    vf2_status status = vf2_hybrid_player_1ff0c_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_B_IP);
    /* Verify all 12 path-B writes. Note: 0x50a0e0 is touched by
     * 0x1fee4 (init) but not by path B, so it stays 0x3f800000. */
    uint32_t expected[12] = {
        0x3f0a3d71, 0x3f0a3d71, 0x3f6b851f, 0x3f5eb852,
        0x3f028f5c, 0x3f0a3d71, 0x3f0a3d71, 0x3f07ae14,
        0x3f28f5c3, 0x3f11eb85, 0x3f028f5c, 0x3f800000
    };
    uint32_t addresses[12] = {
        0x0050a0e4, 0x0050a0e8, 0x0050a0f0, 0x0050a0f8,
        0x0050a100, 0x0050a118, 0x0050a11c, 0x0050a124,
        0x0050a128, 0x0050a12c, 0x0050a134, 0x0050a0e0
    };
    size_t i;
    for (i = 0; i < 12u; ++i) {
        uint32_t v = 0u;
        CHECK(vf2_model2a_read_u32(machine, addresses[i], &v) == VF2_OK);
        CHECK(v == expected[i]);
    }
}

static void run_path_c(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path C: 0x500064 == 0 (default) -> no path-specific writes.
     * 0x1fee4 init still ran, so all addresses have 0x3f800000. */
    setup_cpu(cpu, RET_C_IP);
    poison_writes(machine);
    uint8_t mode = 0u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1) ==
          VF2_OK);
    vf2_status status = vf2_hybrid_player_1ff0c_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_C_IP);
    /* All 12 locations should be 0x3f800000 (from 0x1fee4 init). */
    uint32_t addresses[12] = {
        0x0050a0e4, 0x0050a0e8, 0x0050a0f0, 0x0050a0f8,
        0x0050a100, 0x0050a118, 0x0050a11c, 0x0050a124,
        0x0050a128, 0x0050a12c, 0x0050a134, 0x0050a0e0
    };
    size_t i;
    for (i = 0; i < 12u; ++i) {
        uint32_t v = 0u;
        CHECK(vf2_model2a_read_u32(machine, addresses[i], &v) == VF2_OK);
        CHECK(v == UINT32_C(0x3f800000));
    }
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

    run_path_a(&machine, &cpu);
    run_path_b(&machine, &cpu);
    run_path_c(&machine, &cpu);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x1ff0c mode constants test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x1ff0c paths A (10) / B (6) / C (default) all match\n");
    return 0;
}
