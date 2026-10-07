/* ====================================================================
 * 0xcf04 recovery ctest entry (v0747)
 *
 * Exercises the recovered post-frame IRQ handler
 * vf2_hybrid_player_cf04_execute:
 *
 *   - Sets bit 21 of 0x500068 (prologue).
 *   - Path A (bit 15 set): r3 = *(0x50005b); if bit 21 of 0x500068
 *     clear, r3++; r3 %= 11; store r3 to 0x50005b and 0x500064;
 *     write *(0x50a700) to 0x50a00c.
 *   - Path B (bit 15 clear): r3 = *(0x50054); r3 = *(0x12508 + r3*2);
 *     store r3 to 0x500064; write *(0x50a704) to 0x50a00c.
 *   - Common tail: clrbit 15 of 0x500068.
 *   - REFUSE the sub-call to 0x1fcc0 (display_profile_apply).
 *
 * Two paths are tested:
 *   Path A: bit 15 set
 *   Path B: bit 15 clear
 *
 * Each path verifies:
 *   - The setbit 21 of 0x500068 happened.
 *   - The clrbit 15 of 0x500068 happened.
 *   - The path-specific writes happened.
 *   - The return status is VF2_ERROR_UNSUPPORTED (refused at the
 *     sub-call, as documented).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x0000cf04)
#define REFUSE_IP UINT32_C(0x0000cfb8)  /* ret, after clrbit 21 (interpreted) */

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

/* Build a CPU+frame setup that the 0xcf04 function expects on entry:
 *  - cpu.ip = 0xcf04
 *  - local_frame_depth = 1 (after enter_procedure)
 *  - the saved frame has g2 = 0xcfb8 (the function's `ret`)
 */
static void setup_cpu(vf2_i960_cpu *cpu) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(cpu, ENTRY_IP, REFUSE_IP) == VF2_OK);
}

static void run_path_a(vf2_model2a *machine, vf2_i960_cpu *cpu,
                       uint8_t *fake_rom) {
    /* Path A: bit 15 of 0x500068 set. After the prologue sets
     * bit 21, the bbs at 0xcf34 always sees bit 21 set, so the
     * addo 1 is dead code. r3 = initial_value % 11. */
    setup_cpu(cpu);
    /* 0x500068 with bit 15 set (bit 21 may be 0 or 1, prologue
     * always sets it to 1). */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0x00008000)) == VF2_OK);
    /* 0x50005b = 5 -> 5 % 11 = 5. */
    uint8_t r3_init = 5u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x0050005b), &r3_init, 1) ==
          VF2_OK);
    /* 0x50a700 -> some sentinel value. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050a700),
                                UINT32_C(0xcafebabe)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_cf04_execute(machine, cpu);
    /* REFUSED at the sub-call. */
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->ip == REFUSE_IP);
    /* Verify 0x50005b = 5 and 0x500064 = 5. */
    uint8_t r3_after = 0xffu;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x0050005b), &r3_after, 1) ==
          VF2_OK);
    CHECK(r3_after == 5u);
    r3_after = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00500064), &r3_after, 1) ==
          VF2_OK);
    CHECK(r3_after == 5u);
    /* 0x50a00c = 0xcafebabe. */
    uint32_t a00c = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x0050a00c), &a00c) == VF2_OK);
    CHECK(a00c == UINT32_C(0xcafebabe));
    /* 0x500068: setbit 21 (0x200000) + bit 15 cleared -> 0x00200000. */
    uint32_t reg068 = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &reg068) == VF2_OK);
    CHECK(reg068 == UINT32_C(0x00200000));
    /* Avoid unused-parameter warning. */
    (void)fake_rom;
}

static void run_path_a_no_increment(vf2_model2a *machine,
                                     vf2_i960_cpu *cpu) {
    /* Path A: r3 = 10 -> 10 % 11 = 10 (no increment, per the
     * dead-code analysis above). */
    setup_cpu(cpu);
    /* 0x500068 with bits 15 and 21 already set. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                UINT32_C(0x00208000)) == VF2_OK);
    /* 0x50005b = 10 -> 10 % 11 = 10. */
    uint8_t r3_init = 10u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x0050005b), &r3_init, 1) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050a700),
                                UINT32_C(0xfeedface)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_cf04_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    uint8_t r3_after = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x0050005b), &r3_after, 1) ==
          VF2_OK);
    CHECK(r3_after == 10u);
    /* 0x500064 also = 10. */
    r3_after = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00500064), &r3_after, 1) ==
          VF2_OK);
    CHECK(r3_after == 10u);
}

static void run_path_b(vf2_model2a *machine, vf2_i960_cpu *cpu,
                       uint8_t *fake_rom) {
    /* Path B: bit 15 clear. */
    setup_cpu(cpu);
    /* 0x500068 with bit 15 clear. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    /* 0x50054 = 3. */
    uint8_t r3_init = 3u;
    CHECK(vf2_model2a_write(machine, UINT32_C(0x00500054), &r3_init, 1) ==
          VF2_OK);
    /* 0x12508 + 3*2 = 0x1250e: write a sentinel byte into the
     * fake main_rom buffer (the address is in MAIN_ROM). */
    fake_rom[0x1250e] = UINT8_C(0x42);
    /* 0x50a704 -> some sentinel value. */
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050a704),
                                UINT32_C(0xdeadbeef)) == VF2_OK);
    vf2_status status = vf2_hybrid_player_cf04_execute(machine, cpu);
    CHECK(status == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu->ip == REFUSE_IP);
    /* 0x500064 = 0x42. */
    uint8_t r3_after = 0u;
    CHECK(vf2_model2a_read(machine, UINT32_C(0x00500064), &r3_after, 1) ==
          VF2_OK);
    CHECK(r3_after == UINT8_C(0x42));
    /* 0x50a00c = 0xdeadbeef. */
    uint32_t a00c = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x0050a00c), &a00c) == VF2_OK);
    CHECK(a00c == UINT32_C(0xdeadbeef));
    /* 0x500068: setbit 21 + bit 15 was already clear = 0x00200000. */
    uint32_t reg068 = 0u;
    CHECK(vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &reg068) == VF2_OK);
    CHECK(reg068 == UINT32_C(0x00200000));
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

    /* Attach a small fake main_rom so path B's read of 0x1250e
     * (a MAIN_ROM address) succeeds. The buffer is the same one
     * the function reads from. */
    fake_rom = (uint8_t *)calloc(1u, UINT32_C(0x30000));
    CHECK(fake_rom != NULL);
    if (fake_rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return 1;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, fake_rom, UINT32_C(0x30000)) ==
          VF2_OK);

    run_path_a(&machine, &cpu, fake_rom);
    run_path_a_no_increment(&machine, &cpu);
    run_path_b(&machine, &cpu, fake_rom);

    vf2_model2a_shutdown(&machine);
    free(fake_rom);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0xcf04 IRQ handler test (%d failures)\n",
                failures);
        return 1;
    }
    printf("ok: 0xcf04 paths A (mod 11) + B (table lookup) + sub-call REFUSED\n");
    return 0;
}
