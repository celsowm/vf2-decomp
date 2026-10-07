/* ====================================================================
 * 0x29598 recovery ctest entry (v0741)
 *
 * Exercises the three new recovered paths in
 * vf2_hybrid_player_29598_execute:
 *
 *   Path A: bit 4 of g0 clear -> skip -> ret (3 instructions)
 *   Path B: bbc taken (g0 & (1<<r13) == 0) -> mov 0, g1; ret (4 ins)
 *   Path C: bbc not taken, g1 != 15 -> addo 1, g1; b -> ret (5 ins)
 *   Path D: bbc not taken, g1 == 15 -> REFUSED (sub-calls not recovered)
 *
 * The function reads *(uint8_t*)(0x295ec + g1) for the bbc bit index.
 * That address sits in the MAIN_ROM window; the test attaches a
 * small fake MAIN_ROM with the right bytes at 0x295ec, 0x295f1
 * (path C, g1=5) and 0x295fb (path D, g1=15). The MAIN_ROM region
 * is read-only in production; writes are silently accepted by
 * vf2_model2a_write (see model2a.c:770-775) but not actually stored.
 * The test therefore sets the bytes BEFORE attaching the rom, since
 * the buffer is the same one the rom is attached from.
 *
 * Each path is verified by:
 *   1. Initialising a Model 2A + i960 CPU.
 *   2. Setting g0, g1 to the path-specific inputs.
 *   3. Using vf2_i960_cpu_enter_procedure to push a fake frame with
 *      return_address = 0x295e8 (the function's `ret`).
 *   4. Calling vf2_hybrid_player_29598_execute.
 *   5. Asserting the return status, the IP landing on 0x295e8, and the
 *      executed_instructions count.
 *
 * Path A's body has no further procedure calls; on return, g2 is restored
 * from the saved local frame and the IP is set to 0x295e8. Paths B and C
 * do the same. Path D is REFUSED.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00029598)
#define RET_IP UINT32_C(0x000295e8)

/* Minimum size to cover the test reads (0x295ec..0x295fb). */
#define FAKE_ROM_SIZE UINT32_C(0x30000)

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

/* Build a CPU+frame setup that the 0x29598 function expects on entry:
 *  - cpu.ip = 0x29598
 *  - local_frame_depth = 1 (after enter_procedure)
 *  - the saved frame has g2 = 0x295e8 (return IP)
 *  - g0/g1 carry the path-specific inputs
 */
static void setup_cpu(vf2_i960_cpu *cpu, uint32_t g0, uint32_t g1) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = g0;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = g1;
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, RET_IP) == VF2_OK);
}

static void run_path_a(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path A: g0 bit 4 clear (e.g. g0 = 0x0ff7f700), g1 = 0.
     * Expected: 3 body + 1 ret = 4 instructions executed, VF2_OK,
     * IP returns to 0x295e8. No memory access. */
    setup_cpu(cpu, UINT32_C(0x0ff7f700), 0u);
    uint64_t ins_before = cpu->executed_instructions;
    vf2_status status = vf2_hybrid_player_29598_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    CHECK(cpu->executed_instructions - ins_before == 4u);  /* 3 + ret */
}

static void run_path_b(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path B: g0 bit 4 set; r13 = byte at 0x295ec + g1; bit r13 of g0 clear.
     * Set g0 = 0x10 (bit 4 set), g1 = 0; r13 = byte at 0x295ec. The fake
     * ROM has 0xff at 0x295ec so bit-0x10+ of g0 (= 0x10) is clear. */
    setup_cpu(cpu, UINT32_C(0x10), 0u);
    uint64_t ins_before = cpu->executed_instructions;
    vf2_status status = vf2_hybrid_player_29598_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 1u] == 0u); /* g1 = 0 */
    CHECK(cpu->executed_instructions - ins_before == 5u);  /* 4 + ret */
}

static void run_path_c(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path C: g0 bit 4 set; r13 = byte at 0x295ec+g1; bit r13 of g0 SET;
     * g1 != 15 -> addo 1, g1; b -> ret.
     * g0 = 0x10, g1 = 5, r13 = byte at 0x295ec + 5 = 0x295f1. Fake ROM
     * has 0x01 at 0x295f1 so r13 = 1, bit 1 of g0 (= 0x10) is set. */
    setup_cpu(cpu, UINT32_C(0x10), 5u);
    uint64_t ins_before = cpu->executed_instructions;
    vf2_status status = vf2_hybrid_player_29598_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 1u] == 6u); /* g1 = 5+1 */
    CHECK(cpu->executed_instructions - ins_before == 6u);  /* 5 + ret */
}

static void run_path_d_refused(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path D: g0 bit 4 set; bit r13 of g0 set; g1 == 15 -> REFUSED.
     * Negative control: the recovered function MUST refuse this path
     * because the sub-calls (0xcf04, 0x439ac, 0x43888) are not yet
     * recovered in this slice.
     * The setbit at 0x500068 writes 0x500068 in work RAM (NOT in the
     * fake ROM region), so the test can read it back.
     */
    setup_cpu(cpu, UINT32_C(0x10), 15u);
    /* Prime 0x500068 so we can detect the setbit wrote. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    vf2_status status = vf2_hybrid_player_29598_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    /* setbit at 0x500068 should have run before the refuse:
     * the function applies the setbit, writes g0 = 0xad231f, then refuses. */
    uint32_t r15_after = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &r15_after) == VF2_OK);
    CHECK((r15_after & (UINT32_C(1) << 20u)) != 0u);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == UINT32_C(0x00ad231f));
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

    /* Build a fake main_rom with the right bytes for paths B/C/D. */
    fake_rom = (uint8_t *)calloc(1u, FAKE_ROM_SIZE);
    CHECK(fake_rom != NULL);
    if (fake_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return 1;
    }
    /* path B: r13 at 0x295ec must have bit 4+ clear in g0 = 0x10. */
    fake_rom[0x295ec] = 0xffu;
    /* path C: r13 at 0x295f1 (= 0x295ec + g1=5) must have bit 0 of
     * g0 = 0x10 set. r13 = 1, bit 1 of 0x10 (= 0x10) is 0... so we
     * pick r13 = 4 instead so bit 4 of 0x10 is set. */
    fake_rom[0x295f1] = 0x04u;
    /* path D: r13 at 0x295fb (= 0x295ec + g1=15) must have some set
     * bit of g0 = 0x10 set. r13 = 4 again. */
    fake_rom[0x295fb] = 0x04u;
    CHECK(vf2_model2a_attach_main_rom(&machine, fake_rom, FAKE_ROM_SIZE) ==
          VF2_OK);

    run_path_a(&machine, &cpu);
    run_path_b(&machine, &cpu);
    run_path_c(&machine, &cpu);
    run_path_d_refused(&machine, &cpu);

    vf2_model2a_shutdown(&machine);
    free(fake_rom);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x29598 path test (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x29598 paths A/B/C recovered; path D REFUSED\n");
    return 0;
}
