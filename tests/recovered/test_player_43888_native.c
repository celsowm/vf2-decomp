/* ====================================================================
 * 0x43888 recovery ctest entry (v0746)
 *
 * Exercises the recovered selector2 queue entry function
 * vf2_hybrid_player_43888_execute:
 *
 *   Path A: g0 == 0x00ae101f -> accepted (no gates)
 *   Path B: (0x50002c & 0xc) == 0 -> accepted
 *   Path C: gate A fails, byte at (0x50016c+0x3351) bit 0 set -> RET
 *   Path D: gate A fails, byte bit 0 clear, 0x500068 bit 20 clear
 *           -> accepted
 *   Path E: gate A fails, byte bit 0 clear, 0x500068 bit 20 set,
 *           (g0 & 0x00ff0000) != 0x009e0000 -> accepted (g0 unchanged)
 *   Path F: gate A fails, byte bit 0 clear, 0x500068 bit 20 set,
 *           (g0 & 0x00ff0000) == 0x009e0000 -> g0 -= 0x20000, accepted
 *
 * Each path is verified by:
 *   1. Initialising a Model 2A + i960 CPU.
 *   2. Setting g0, 0x50002c, 0x500068, 0x50016c+0x3351, 0x504001, 0x504003.
 *   3. Using vf2_i960_cpu_enter_procedure to push a fake frame with
 *      return_address = 0x4394c (the function's `ret`).
 *   4. Calling vf2_hybrid_player_43888_execute.
 *   5. Asserting the return status, IP, g0 mutation, and queue state.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00043888)
#define RET_IP UINT32_C(0x0004394c)

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

/* Build a CPU+frame setup that the 0x43888 function expects on entry:
 *  - cpu.ip = 0x43888
 *  - local_frame_depth = 1 (after enter_procedure)
 *  - the saved frame has g2 = 0x4394c (return IP)
 *  - g0 carries the input value
 */
static void setup_cpu(vf2_i960_cpu *cpu, uint32_t g0) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = g0;
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, RET_IP) == VF2_OK);
}

static void prime_queues(vf2_model2a *machine, uint8_t count,
                          uint8_t ring_idx) {
    /* 0x504001: count (1B)
     * 0x504003: ring index (1B) */
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00504001), &count, 1) ==
          VF2_OK);
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00504003), &ring_idx, 1) ==
          VF2_OK);
}

static void run_path_a(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path A: g0 == 0x00ae101f -> accepted, no gate checks. */
    setup_cpu(cpu, UINT32_C(0x00ae101f));
    prime_queues(machine, 0, 0);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* g0 unchanged. */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == UINT32_C(0x00ae101f));
    /* count became 1, ring index became 1. */
    uint8_t count = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00504001), &count, 1) == VF2_OK);
    CHECK(count == 1u);
    uint8_t ring = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00504003), &ring, 1) == VF2_OK);
    CHECK(ring == 1u);
    /* queue[0] = g0 = 0x00ae101f */
    uint32_t q0 = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00504020), &q0) == VF2_OK);
    CHECK(q0 == UINT32_C(0x00ae101f));
}

static void run_path_b(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path B: (0x50002c & 0xc) == 0 -> accepted, no other gates. */
    setup_cpu(cpu, UINT32_C(0xdeadbeef));
    prime_queues(machine, 0, 0);
    /* 0x50002c with bits 2-3 clear. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                UINT32_C(0xfffffff0)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == UINT32_C(0xdeadbeef));
    /* queue[0] = 0xdeadbeef */
    uint32_t q0 = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00504020), &q0) == VF2_OK);
    CHECK(q0 == UINT32_C(0xdeadbeef));
}

static void run_path_c(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path C: gate A fails, byte bit 0 set -> RET, no queue write. */
    setup_cpu(cpu, UINT32_C(0xfeedface));
    prime_queues(machine, 0, 0);
    /* 0x50002c with bits 2-3 set. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                UINT32_C(0xffffffff)) == VF2_OK);
    /* 0x50016c -> byte at offset 0x3351 = 0x01 (bit 0 set).
     * t1_base must be in work RAM (0x500000..0x5fccaf) so that
     * t1_base + 0x3351 is also in work RAM. */
    uint32_t t1_base = UINT32_C(0x00504000);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050016c),
                                t1_base) == VF2_OK);
    uint8_t t1_byte = UINT8_C(0x01);
    CHECK(vf2_model2a_write(machine, t1_base + UINT32_C(0x3351), &t1_byte,
                             1) == VF2_OK);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* count unchanged (still 0). */
    uint8_t count = 0xffu;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00504001), &count, 1) == VF2_OK);
    CHECK(count == 0u);
    /* ring index unchanged (still 0). */
    uint8_t ring = 0xffu;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00504003), &ring, 1) == VF2_OK);
    CHECK(ring == 0u);
}

static void run_path_d(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path D: gate A fails, byte bit 0 clear, 0x500068 bit 20 clear
     * -> accepted. */
    setup_cpu(cpu, UINT32_C(0x12345678));
    prime_queues(machine, 0, 0);
    /* 0x50002c with bits 2-3 set. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                UINT32_C(0xffffffff)) == VF2_OK);
    /* 0x50016c -> byte at 0x3351 = 0x00 (bit 0 clear).
     * t1_base must be in work RAM so t1_base + 0x3351 is also
     * in work RAM. */
    uint32_t t1_base = UINT32_C(0x00504000);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050016c),
                                t1_base) == VF2_OK);
    uint8_t t1_byte = UINT8_C(0x00);
    CHECK(vf2_model2a_write(machine, t1_base + UINT32_C(0x3351), &t1_byte,
                             1) == VF2_OK);
    /* 0x500068 with bit 20 clear. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0xffefffff)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* g0 unchanged. */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == UINT32_C(0x12345678));
    /* queue[0] = 0x12345678 */
    uint32_t q0 = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00504020), &q0) == VF2_OK);
    CHECK(q0 == UINT32_C(0x12345678));
}

static void run_path_e(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path E: gate A fails, byte bit 0 clear, 0x500068 bit 20 set,
     * (g0 & 0x00ff0000) != 0x009e0000 -> accepted, g0 unchanged. */
    setup_cpu(cpu, UINT32_C(0xabcdef00));  /* bits 16-23 = 0xab, not 0x9e */
    prime_queues(machine, 0, 0);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                UINT32_C(0xffffffff)) == VF2_OK);
    uint32_t t1_base = UINT32_C(0x00504000);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050016c),
                                t1_base) == VF2_OK);
    uint8_t t1_byte = UINT8_C(0x00);
    CHECK(vf2_model2a_write(machine, t1_base + UINT32_C(0x3351), &t1_byte,
                             1) == VF2_OK);
    /* 0x500068 with bit 20 set. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0x00100000)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* g0 unchanged. */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == UINT32_C(0xabcdef00));
}

static void run_path_f(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Path F: gate A fails, byte bit 0 clear, 0x500068 bit 20 set,
     * (g0 & 0x00ff0000) == 0x009e0000 -> g0 -= 0x20000, accepted. */
    setup_cpu(cpu, UINT32_C(0x009e1234));
    prime_queues(machine, 0, 0);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                UINT32_C(0xffffffff)) == VF2_OK);
    uint32_t t1_base = UINT32_C(0x00504000);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050016c),
                                t1_base) == VF2_OK);
    uint8_t t1_byte = UINT8_C(0x00);
    CHECK(vf2_model2a_write(machine, t1_base + UINT32_C(0x3351), &t1_byte,
                             1) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0x00100000)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == RET_IP);
    /* g0 -= 0x20000 -> 0x009e1234 - 0x20000 = 0x009c1234 */
    CHECK(cpu->registers[VF2_I960_G0_REGISTER + 0u] == UINT32_C(0x009c1234));
    /* queue[0] = 0x009c1234 */
    uint32_t q0 = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00504020), &q0) == VF2_OK);
    CHECK(q0 == UINT32_C(0x009c1234));
}

static void run_path_count_full(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    /* Negative control: count = 16 (full). The common tail should
     * NOT increment count or write to the queue. */
    setup_cpu(cpu, UINT32_C(0x00ae101f));  /* path A */
    prime_queues(machine, 16, 0);
    vf2_status status = vf2_hybrid_player_43888_execute(machine, cpu);
    CHECK(status == VF2_OK);
    /* count still 16. */
    uint8_t count = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00504001), &count, 1) == VF2_OK);
    CHECK(count == 16u);
    /* ring index still 0. */
    uint8_t ring = 0xffu;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00504003), &ring, 1) == VF2_OK);
    CHECK(ring == 0u);
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
    run_path_d(&machine, &cpu);
    run_path_e(&machine, &cpu);
    run_path_f(&machine, &cpu);
    run_path_count_full(&machine, &cpu);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x43888 selector2 entry test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0x43888 paths A/B/C/D/E/F + count-full negative control\n");
    return 0;
}
