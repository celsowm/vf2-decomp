/* ====================================================================
 * 0x439ac recovery ctest entry (v0745)
 *
 * Exercises the recovered queue dedup-append function
 * vf2_hybrid_player_439ac_execute:
 *
 *   Block 0x439ac: read count at 0x50406a
 *   Block 0x439b8: search read slots 0x504074[count+1..1] for g0
 *   Block 0x439d4: not found -> write g0 to 0x504078[count], count++
 *   Block 0x439f8: ret
 *
 * Three scenarios are tested:
 *   Path A: count == 4 -> return immediately (queue full)
 *   Path B: count == 0, slot has g0 -> return (idempotent dedup)
 *   Path C: count == 0, slot does not have g0 -> write + count++
 *   Path D: count == 2, g0 is in slot 1 -> return (mid-loop match)
 *
 * Each path is verified by:
 *   1. Initialising a Model 2A + i960 CPU.
 *   2. Setting g0 to the test key, then setting up the count and
 *      read slots in work RAM.
 *   3. Using vf2_i960_cpu_enter_procedure to push a fake frame with
 *      return_address = 0x439f8 (the function's `ret`).
 *   4. Calling vf2_hybrid_player_439ac_execute.
 *   5. Asserting the return status, IP landing on 0x439f8, and the
 *      final state of count and the write slots.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x000439ac)
#define RET_IP UINT32_C(0x000439f8)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

/* Build a CPU+frame setup that the 0x439ac function expects on entry:
 *  - cpu.ip = 0x439ac
 *  - local_frame_depth = 1 (after enter_procedure)
 *  - the saved frame has g2 = 0x439f8 (return IP)
 *  - g0 carries the search key
 */
static void setup_cpu(vf2_i960_cpu *cpu, uint32_t g0) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = g0;
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, RET_IP) == VF2_OK);
}

static void run_path_a_full(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path A: count == 4 -> queue full -> return immediately. */
    setup_cpu(cpu, UINT32_C(0xdeadbeef));
    /* Prime count = 4. */
    uint8_t count = 4u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x0050406a), &count, 1) == VF2_OK);
    uint64_t ins_before = cpu->executed_instructions;
    vf2_status status = vf2_hybrid_player_439ac_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* Body: 1 (ldob) + 1 (cmpoble taken) + 1 (ret) = 3 */
    CHECK(cpu->executed_instructions - ins_before == 3u);
    /* Count unchanged. */
    uint8_t count_after = 0xffu;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x0050406a), &count_after, 1) == VF2_OK);
    CHECK(count_after == 4u);
}

static void run_path_b_match_at_top(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path B: count == 0; slot[1] has g0 -> return (idempotent dedup
     * at the very first iteration). */
    setup_cpu(cpu, UINT32_C(0x12345678));
    uint8_t count = 0u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x0050406a), &count, 1) == VF2_OK);
    /* Slot[1] at 0x504074 + 1*4 = 0x504078 has the key. */
    uint32_t slot1 = UINT32_C(0x12345678);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00504078), slot1) == VF2_OK);
    /* Other slots empty. */
    uint32_t zero = 0u;
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00504074), zero) == VF2_OK);
    vf2_status status = vf2_hybrid_player_439ac_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* Count unchanged. */
    uint8_t count_after = 0xffu;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x0050406a), &count_after, 1) == VF2_OK);
    CHECK(count_after == 0u);
}

static void run_path_c_append(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path C: count == 0, no match in slots 1 -> write at write_slot[0]. */
    setup_cpu(cpu, UINT32_C(0xc0ffee00));
    uint8_t count = 0u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x0050406a), &count, 1) == VF2_OK);
    /* Slots empty. */
    uint32_t zero = 0u;
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00504074), zero) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00504078), zero) == VF2_OK);
    vf2_status status = vf2_hybrid_player_439ac_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* Count incremented. */
    uint8_t count_after = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x0050406a), &count_after, 1) == VF2_OK);
    CHECK(count_after == 1u);
    /* write_slot[0] has the key. */
    uint32_t written = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00504078), &written) == VF2_OK);
    CHECK(written == UINT32_C(0xc0ffee00));
}

static void run_path_d_mid_match(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path D: count == 2, key in slot[1] (not slot[3] which is
     * outside the search range starting from count+1=3). */
    setup_cpu(cpu, UINT32_C(0xfeedface));
    uint8_t count = 2u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x0050406a), &count, 1) == VF2_OK);
    /* slot[3] at 0x504074 + 3*4 = 0x504080 */
    uint32_t slot3 = UINT32_C(0x11111111);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00504080), slot3) == VF2_OK);
    /* slot[2] at 0x504074 + 2*4 = 0x50407c */
    uint32_t slot2 = UINT32_C(0x22222222);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050407c), slot2) == VF2_OK);
    /* slot[1] at 0x504074 + 1*4 = 0x504078 -- contains the key. */
    uint32_t slot1 = UINT32_C(0xfeedface);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00504078), slot1) == VF2_OK);
    vf2_status status = vf2_hybrid_player_439ac_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* Count unchanged. */
    uint8_t count_after = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x0050406a), &count_after, 1) == VF2_OK);
    CHECK(count_after == 2u);
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

    run_path_a_full(&machine, &cpu);
    run_path_b_match_at_top(&machine, &cpu);
    run_path_c_append(&machine, &cpu);
    run_path_d_mid_match(&machine, &cpu);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x439ac dedup-append test (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x439ac paths A/B/C/D recovered (queue full / match / append / mid-match)\n");
    return 0;
}
