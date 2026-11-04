#include "vf2/m68k_step.h"
#include "vf2/rom.h"
#include "vf2/scsp.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real L2 entry PC 0x1F16 -> 0x1F7A, distinct from the already
 * proven record-0 worker 0x1F7C -> 0x2158. Owner-ROM only on the
 * differential side. No ROM data is included or committed here. */
static vf2_sound_board board;
static vf2_sound_board before;
static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp original_scsp;

static int assert_true(int yes, const char *reason)
{
    if (!yes) fprintf(stderr, "AKIRA_L2_FAIL %s\n", reason);
    return yes;
}

static void fixture(const uint8_t *rom, size_t size)
{
    uint8_t *desc;
    size_t i;
    memset(&board, 0, sizeof(board));
    board.audio_rom = rom;
    board.audio_rom_size = size;
    vf2_scsp_initialize(&board.scsp);
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
    desc[0] = UINT8_C(0x88);
    desc[3] = UINT8_C(1);
    desc[5] = UINT8_C(0x61);
    desc[6] = UINT8_C(0x57);
    desc[7] = UINT8_C(0x73);
    desc[9] = UINT8_C(0x60);
    desc[10] = UINT8_C(0x9c);
    desc[11] = UINT8_C(0x3e);
    for (i = 0u; i < 32u; ++i) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + i * 4u + 3u] =
            (uint8_t)(0x40u + i);
    }
}

static int rom_free(void)
{
    static uint8_t empty_rom[0x16000u];
    vf2_sound_stream_maintenance_report report;
    fixture(empty_rom, sizeof(empty_rom));
    memcpy(&before, &board, sizeof(before));
    if (!assert_true(vf2_sound_board_maintain_streams(
        &board, &report) == VF2_ERROR_UNSUPPORTED &&
        memcmp(&board, &before, sizeof(board)) == 0 &&
        report.refused_stage == 2u &&
        report.refused_record == 0u,
        "unverified worker refuses without partial L2 writes")) return 0;

    /* An expiring second stream makes the combined body unmeasured:
     * no first-worker side effects may leak. */
    fixture(empty_rom, sizeof(empty_rom));
    board.sound_ram[VF2_SOUND_STREAM_ALTERNATE_BASE] = UINT8_C(0x80);
    board.sound_ram[VF2_SOUND_STREAM_ALTERNATE_BASE + 3u] = 1u;
    memcpy(&before, &board, sizeof(before));
    if (!assert_true(vf2_sound_board_maintain_streams(
        &board, &report) == VF2_ERROR_UNSUPPORTED &&
        memcmp(&board, &before, sizeof(board)) == 0,
        "competing worker refuses atomically")) return 0;

    /* Unarmed descriptor is still the existing normal L2 idle path. */
    fixture(empty_rom, sizeof(empty_rom));
    board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE] = 0u;
    if (!assert_true(vf2_sound_board_maintain_streams(
        &board, &report) == VF2_OK &&
        report.packets_emitted == 0u &&
        report.descriptors_scanned == 8u &&
        board.scsp.registers[0x418u] == 0u &&
        board.scsp.registers[0x419u] == UINT8_C(0xce),
        "idle L2 still reaches original SCSP head")) return 0;
    fprintf(stderr, "AKIRA_L2_ROM_FREE PASS\n");
    return 1;
}

static int actual_rom(const uint8_t *rom, size_t size)
{
    vf2_m68k_bus bus = {0};
    vf2_m68k_regs regs;
    vf2_sound_stream_maintenance_report report;
    vf2_status status;
    uint32_t steps = 0u;
    uint32_t sr_events = 0u;
    int reached = 0;
    size_t at;
    fixture(rom, size);
    memcpy(original_ram, board.sound_ram, sizeof(original_ram));
    original_scsp = board.scsp;
    bus.ram = original_ram;
    bus.ram_size = sizeof(original_ram);
    bus.rom = rom;
    bus.rom_size = size;
    bus.scsp = &original_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x1f16);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    status = vf2_sound_board_maintain_streams(&board, &report);
    if (!assert_true(status == VF2_OK &&
        report.descriptors_scanned == 8u &&
        report.timers_expired == 1u &&
        report.packets_emitted == 32u &&
        report.refused_stage == 0u &&
        report.write_cursor_after == UINT16_C(128),
        "native L2 integrated fixture")) return 0;

    for (steps = 0u; steps < 16000u; ++steps) {
        uint32_t done = 0u;
        int suspended = 0;
        if (regs.pc == UINT32_C(0x1f7a)) {
            reached = 1;
            break;
        }
        if (bus.event_count >= 60u) {
            sr_events += (uint32_t)bus.event_count;
            bus.event_count = 0u;
        }
        status = vf2_m68k_run_slice(
            &bus, &regs, UINT32_MAX, 0, 1u, &done, &suspended);
        if (status != VF2_OK || done != 1u || !suspended) {
            fprintf(stderr,
                "AKIRA_L2_ORIGINAL_REFUSED pc=%05x status=%d "
                "step=%u done=%u suspended=%d\n",
                (unsigned)regs.pc,(int)status,(unsigned)steps,
                (unsigned)done,suspended);
            return 0;
        }
    }
    sr_events += (uint32_t)bus.event_count;
    if (!assert_true(reached && regs.a[7] == UINT32_C(0x7f00),
        "real 68000 L2 tick reached epilogue with balanced stack")) return 0;

    /* Original 68000 spills temporary registers on the supervisor
     * stack; the C implementation is stackless. Compare every other
     * byte, both SCSP states and the complete 32-entry FIFO. */
    for (at = 0u; at < VF2_SOUND_RAM_BYTES; ++at) {
        if (at >= 0x7e00u && at < 0x7f00u) continue;
        if (original_ram[at] != board.sound_ram[at]) {
            fprintf(stderr,
                "AKIRA_L2_RAM_MISMATCH at=%05x original=%02x native=%02x "
                "steps=%u\n",
                (unsigned)at,(unsigned)original_ram[at],
                (unsigned)board.sound_ram[at],(unsigned)steps);
            return 0;
        }
    }
    if (!assert_true(memcmp(&original_scsp, &board.scsp,
                            sizeof(original_scsp)) == 0,
        "complete modeled SCSP matches original L2")) return 0;
    fprintf(stderr,
        "AKIRA_L2_ORIGINAL_DIFF PASS stop=%05x steps=%u count=%u "
        "cursor=%u duration=%02x%02x stream=%02x%02x%02x%02x "
        "sr_events=%u ram=1 scsp=1\n",
        (unsigned)regs.pc,(unsigned)steps,
        (unsigned)original_ram[VF2_SOUND_COMMAND_COUNT],
        (unsigned)((original_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                   original_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]),
        (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE + 2u],
        (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE + 3u],
        (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE + 4u],
        (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE + 5u],
        (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE + 6u],
        (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE + 7u],
        (unsigned)sr_events);

    /* The first worker installed the original 0x01B2 countdown.
     * Continue three more real L2 ticks on the same RAM and SCSP;
     * the verified worker must NOT re-run or append FIFO entries.
     * Only the CPU scratch registers are reset at each IRQ entry. */
    {
        unsigned tick;
        for (tick = 1u; tick <= 3u; ++tick) {
            const uint16_t expected =
                (uint16_t)(UINT16_C(0x01b2) - (uint16_t)tick);
            uint32_t next_steps = 0u;
            int second_reached = 0;
            status = vf2_sound_board_maintain_streams(&board, &report);
            if (!assert_true(status == VF2_OK &&
                report.packets_emitted == 0u &&
                report.timers_expired == 0u &&
                report.descriptors_scanned == 8u &&
                report.write_cursor_after == UINT16_C(128),
                "native follow-up L2 does not replay worker")) return 0;
            vf2_m68k_regs_reset(&regs);
            regs.pc = UINT32_C(0x1f16);
            regs.a[5] = UINT32_C(0x100000);
            regs.a[6] = UINT32_C(0x1000);
            regs.a[7] = UINT32_C(0x7f00);
            bus.event_count = 0u;
            for (next_steps = 0u; next_steps < 800u; ++next_steps) {
                uint32_t done = 0u;
                int suspended = 0;
                if (regs.pc == UINT32_C(0x1f7a)) {
                    second_reached = 1;
                    break;
                }
                status = vf2_m68k_run_slice(
                    &bus, &regs, UINT32_MAX, 0, 1u,
                    &done, &suspended);
                if (status != VF2_OK || done != 1u || !suspended) {
                    fprintf(stderr,
                        "AKIRA_L2_FOLLOWUP_CPU_REFUSED tick=%u pc=%05x "
                        "status=%d steps=%u\n",
                        tick,(unsigned)regs.pc,(int)status,
                        (unsigned)next_steps);
                    return 0;
                }
            }
            if (!assert_true(second_reached &&
                regs.a[7] == UINT32_C(0x7f00) &&
                original_ram[VF2_SOUND_COMMAND_COUNT] == 32u &&
                original_ram[VF2_SOUND_DESCRIPTOR_BASE + 2u] ==
                    (uint8_t)(expected >> 8u) &&
                original_ram[VF2_SOUND_DESCRIPTOR_BASE + 3u] ==
                    (uint8_t)expected,
                "original L2 follow-up countdown and queue")) return 0;
            for (at = 0u; at < VF2_SOUND_RAM_BYTES; ++at) {
                if (at >= 0x7e00u && at < 0x7f00u) continue;
                if (original_ram[at] != board.sound_ram[at]) {
                    fprintf(stderr,
                        "AKIRA_L2_FOLLOWUP_RAM_MISMATCH tick=%u "
                        "at=%05x original=%02x native=%02x\n",
                        tick,(unsigned)at,(unsigned)original_ram[at],
                        (unsigned)board.sound_ram[at]);
                    return 0;
                }
            }
            if (!assert_true(memcmp(&original_scsp, &board.scsp,
                                    sizeof(original_scsp)) == 0,
                "original/native SCSP equality on repeated L2 tick")) {
                return 0;
            }
            fprintf(stderr,
                "AKIRA_L2_FOLLOWUP_DIFF PASS tick=%u steps=%u "
                "duration=%04x queue=32 ram=1 scsp=1\n",
                tick,(unsigned)next_steps,(unsigned)expected);
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    uint8_t *rom = NULL;
    size_t size = 0u;
    int ok;
    if (argc == 2 && strcmp(argv[1], "--rom-free") == 0) {
        return rom_free() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: %s --rom-free | <owner-rom-directory>\n",
                argv[0]);
        return 2;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                               &rom, &size) != VF2_OK || rom == NULL) {
        fprintf(stderr, "AKIRA_L2_FAIL unable to load private ROM\n");
        return 2;
    }
    ok = actual_rom(rom, size);
    free(rom);
    return ok ? 0 : 1;
}
