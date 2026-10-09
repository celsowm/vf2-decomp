/* ====================================================================
 * 0x7ef0 recovery ctest entry (v0758)
 *
 * Exercises the recovered rom_to_wram_triple_copy
 * vf2_hybrid_player_7ef0_execute:
 *
 *   - Reads 12 bytes from ROM at 0x7f64, writes to WRAM 0x501400.
 *   - Reads 12 bytes from ROM at 0x7f70, writes to WRAM 0x50140c.
 *
 * The test attaches a fake main_rom covering the source reads.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00007ef0)
#define RET_IP UINT32_C(0x00007f14)
#define ROM_BASE UINT32_C(0x00007f64)
#define ROM_SIZE 0x80000u  /* cover the 0x7f64 source reads */
#define WRAM_BASE UINT32_C(0x00501400)

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

static void write_u32_le(vf2_model2a *m, uint32_t addr, uint32_t v) {
    CHECK(vf2_model2a_write_u32(m, addr, v) == VF2_OK);
}
static uint32_t read_u32_le(vf2_model2a *m, uint32_t addr) {
    uint32_t v = 0;
    CHECK(vf2_model2a_read_u32(m, addr, &v) == VF2_OK);
    return v;
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_status status;

    static uint8_t fake_rom[ROM_SIZE];

    /* Pre-fill ROM with deterministic data. */
    memset(fake_rom, 0, ROM_SIZE);
    /* Put identifiable bytes in the source regions at 0x7f64.. */
    fake_rom[0x7f64] = 0xaa; /* 0x7f64 byte 0 */
    fake_rom[0x7f68] = 0xbb; /* offset +4 */
    fake_rom[0x7f6c] = 0xcc; /* offset +8 */
    fake_rom[0x7f70] = 0xdd; /* offset +12 */
    fake_rom[0x7f74] = 0xee; /* offset +16 */
    fake_rom[0x7f78] = 0xff; /* offset +20 */

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    CHECK(vf2_model2a_attach_main_rom(
        &machine, fake_rom, sizeof(fake_rom)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);
    CHECK(vf2_i960_cpu_enter_procedure(
        &cpu, ENTRY_IP, RET_IP) == VF2_OK);

    status = vf2_hybrid_player_7ef0_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);

    /* Verify the 6 written dwords. */
    CHECK(read_u32_le(&machine, WRAM_BASE + 0) == 0x000000aau);
    CHECK(read_u32_le(&machine, WRAM_BASE + 4) == 0x000000bbu);
    CHECK(read_u32_le(&machine, WRAM_BASE + 8) == 0x000000ccu);
    CHECK(read_u32_le(&machine, WRAM_BASE + 12) == 0x000000ddu);
    CHECK(read_u32_le(&machine, WRAM_BASE + 16) == 0x000000eeu);
    CHECK(read_u32_le(&machine, WRAM_BASE + 20) == 0x000000ffu);

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x7ef0 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x7ef0 rom_to_wram_triple_copy: 24 bytes copied (2 ldt/stt)\n");
    return 0;
}
