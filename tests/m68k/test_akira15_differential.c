#include "vf2/m68k_step.h"
#include "vf2/rom.h"
#include "vf2/scsp.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native 15th Akira packet plus private-ROM 68000 differential.
 * An original-ROM comparison is opt-in, never represented by fake data. */
static vf2_sound_board native_board;
static vf2_sound_board snapshot;
static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp original_scsp;

static int check(int ok, const char *what)
{
    if (!ok) fprintf(stderr, "AKIRA_15_FAIL %s\n", what);
    return ok;
}

static void prepare(vf2_sound_board *board,
                    const uint8_t *rom, size_t rom_size,
                    uint16_t start)
{
    uint8_t *d;
    size_t i;
    memset(board, 0, sizeof(*board));
    board->audio_rom = rom;
    board->audio_rom_size = rom_size;
    vf2_scsp_initialize(&board->scsp);
    d = board->sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
    d[0] = UINT8_C(0x88);
    d[3] = 1u;
    d[5] = UINT8_C(0x61);
    d[6] = UINT8_C(0x57);
    d[7] = UINT8_C(0x73);
    d[9] = UINT8_C(0x60);
    d[10] = UINT8_C(0x9c);
    d[11] = UINT8_C(0x3e);
    board->sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] =
        (uint8_t)(start >> 8u);
    board->sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] =
        (uint8_t)start;
    for (i = 0u; i < 15u; ++i) {
        const uint16_t cursor = (uint16_t)(
            (start + 4u * (uint16_t)i) & UINT16_C(0x0fff));
        board->sound_ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 3u] =
            (uint8_t)(UINT8_C(0x60) + i);
    }
}

static int native_case(const uint8_t *rom, size_t rom_size, uint16_t start)
{
    const size_t at = UINT32_C(0x615773) - VF2_SOUND_AUDIO_ROM_BASE;
    vf2_sound_music_prefix14_report first;
    vf2_sound_music_fifteenth_report final;
    uint16_t cursor;
    if (rom_size - at < 40u) return 0;
    prepare(&native_board, rom, rom_size, start);
    if (!check(vf2_sound_board_probe_akira_l2_prefix14(
        &native_board, &first) == VF2_OK, "native prefix14")) return 0;
    memcpy(&snapshot, &native_board, sizeof(snapshot));
    if (!check(vf2_sound_board_probe_akira_l2_fifteenth_b(
        &native_board, &final) == VF2_OK, "native packet15")) return 0;
    cursor = (uint16_t)((start + 56u) & UINT16_C(0x0fff));
    if (!check(final.a3_after == UINT32_C(0x61579b) &&
        final.cursor_before == cursor &&
        final.cursor_after ==
            (uint16_t)((start + 60u) & UINT16_C(0x0fff)) &&
        final.packet[0] == UINT8_C(0xb6) &&
        final.packet[1] == UINT8_C(0x0a) &&
        final.packet[2] == UINT8_C(0x2c) &&
        native_board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 15u &&
        native_board.sound_ram[
            VF2_SOUND_COMMAND_RING_BASE + cursor + 3u] ==
            UINT8_C(0x6e), "native packet15 byte/cursor/status")) return 0;
    if (!check(vf2_sound_board_probe_akira_l2_fifteenth_b(
        &native_board, &final) == VF2_ERROR_UNSUPPORTED,
        "native refuses replay")) return 0;

    /* Late failure: source or previous slot mismatch cannot change
     * any RAM/SCSP data or return a partially populated report. */
    memcpy(&native_board, &snapshot, sizeof(native_board));
    native_board.sound_ram[VF2_SOUND_COMMAND_RING_BASE +
        (uint16_t)((start + 52u) & UINT16_C(0x0fff))] ^= UINT8_C(1);
    memcpy(&snapshot, &native_board, sizeof(snapshot));
    memset(&final, 0x5a, sizeof(final));
    if (!check(vf2_sound_board_probe_akira_l2_fifteenth_b(
        &native_board, &final) == VF2_ERROR_UNSUPPORTED &&
        memcmp(&native_board, &snapshot, sizeof(native_board)) == 0 &&
        final.packet[0] == UINT8_C(0x5a),
        "native refuses corrupted history without mutation")) return 0;
    return 1;
}

static int owner_differential(const uint8_t *rom, size_t rom_size)
{
    vf2_m68k_bus bus = {0};
    vf2_m68k_regs regs;
    vf2_sound_music_prefix14_report first;
    vf2_sound_music_fifteenth_report last;
    vf2_status status = VF2_OK;
    size_t i;
    int reached = 0;
    uint32_t instructions = 0u;

    prepare(&native_board, rom, rom_size, 0u);
    memcpy(original_ram, native_board.sound_ram, sizeof(original_ram));
    original_scsp = native_board.scsp;
    bus.ram = original_ram;
    bus.ram_size = sizeof(original_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.scsp = &original_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x1f7c);
    regs.a[2] = VF2_SOUND_DESCRIPTOR_BASE;
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    for (i = 0u; i < 12000u; ++i) {
        uint32_t done = 0u;
        int suspended = 0;
        if (regs.pc == UINT32_C(0x212c) &&
            original_ram[VF2_SOUND_COMMAND_COUNT] == 15u) {
            reached = 1;
            break;
        }
        status = vf2_m68k_run_slice(
            &bus, &regs, UINT32_MAX, 0, 1u, &done, &suspended);
        if (status != VF2_OK || done != 1u || !suspended) break;
        ++instructions;
    }
    if (!check(reached && status == VF2_OK &&
        instructions == 472u && regs.a[3] == UINT32_C(0x61579b) &&
        bus.event_count == 29u, "original 68000 15th boundary")) return 0;
    if (!check(vf2_sound_board_probe_akira_l2_prefix14(
        &native_board, &first) == VF2_OK, "C first14")) return 0;
    if (!check(vf2_sound_board_probe_akira_l2_fifteenth_b(
        &native_board, &last) == VF2_OK, "C fifteenth")) return 0;
    if (!check(last.a3_after == regs.a[3] &&
        memcmp(original_ram, native_board.sound_ram,
               sizeof(original_ram)) == 0 &&
        memcmp(&original_scsp, &native_board.scsp,
               sizeof(original_scsp)) == 0,
        "full 512KiB RAM and SCSP parity")) return 0;
    fprintf(stderr,
        "AKIRA_15_ORIGINAL_DIFF PASS pc=%05x a3=%06x steps=%u "
        "packets=%u fifo_cursor=%u sr_events=%u ram=1 scsp=1\n",
        (unsigned)regs.pc, (unsigned)regs.a[3],
        (unsigned)instructions,
        (unsigned)original_ram[VF2_SOUND_COMMAND_COUNT],
        (unsigned)last.cursor_after, (unsigned)bus.event_count);
    return 1;
}

int main(int argc, char **argv)
{
    uint8_t *rom = NULL;
    size_t rom_size = 0u;
    int ok;
    if (argc == 2 && strcmp(argv[1], "--rom-free") == 0) {
        static uint8_t fake_rom[0x16000u];
        static const uint8_t source[40] = {
            0xbeu,0x50u,0xfeu,0xbeu,0x20u,0x80u,
            0xbeu,0x10u,0x72u,0x00u,
            0x9fu,0x48u,0xf7u,0x9fu,0x2eu,0xc0u,
            0x9fu,0x33u,0xffu,0xc1u,0x81u,0x9fu,0x2bu,0xeeu,
            0xc3u,0x82u,0xc8u,0x8du,0xb0u,0x0au,0xc0u,
            0x07u,0xffu,0xc0u,0x80u,0xb6u,0x07u,0xf8u,
            0x0au,0xacu
        };
        memcpy(fake_rom + UINT32_C(0x615773) -
               VF2_SOUND_AUDIO_ROM_BASE, source, sizeof(source));
        /* The C probes only inspect the bounded stream. They don't
         * require a fabricated emulated ROM instruction body. */
        ok = native_case(fake_rom, sizeof(fake_rom), 0u) &&
             native_case(fake_rom, sizeof(fake_rom),
                         UINT16_C(0x0ffc));
        if (ok) puts("AKIRA_15_NATIVE_ROM_FREE PASS");
        return ok ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: %s <owner-ROM-dir> | --rom-free\n", argv[0]);
        return 2;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                               &rom, &rom_size) != VF2_OK || rom == NULL) {
        fprintf(stderr, "AKIRA_15_FAIL ROM validation\n");
        return 2;
    }
    ok = owner_differential(rom, rom_size);
    free(rom);
    return ok ? 0 : 1;
}
