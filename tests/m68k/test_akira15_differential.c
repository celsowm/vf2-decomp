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
static vf2_sound_board full_worker_board;
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

static int owner_differential(const uint8_t *rom, size_t rom_size,
                              int trace_continuation)
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
    if (trace_continuation) {
        unsigned last_count = 15u;
        uint32_t more_steps = 0u;
        uint32_t observed_prior_events = 0u;
        /* A private ROM is required. This is the SAME 68000 regs/RAM/
         * SCSP object that already matched native at packet 15. */
        for (more_steps = 0u; more_steps < 15000u; ++more_steps) {
            uint32_t done = 0u;
            int suspended = 0;
            const unsigned count = original_ram[VF2_SOUND_COMMAND_COUNT];
            if (regs.pc == UINT32_C(0x212c) && count > last_count) {
                const uint16_t cur =
                    (uint16_t)(((uint16_t)original_ram[
                        VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                        original_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]);
                const uint16_t prev =
                    (uint16_t)((cur + UINT16_C(0x0ffc)) & UINT16_C(0x0fff));
                const uint8_t *packet =
                    original_ram + VF2_SOUND_COMMAND_RING_BASE + prev;
                if (count != last_count + 1u ||
                    cur != (uint16_t)(count * 4u)) {
                    fprintf(stderr,
                        "AKIRA_FOLLOWUP_REFUSED discontinuous count=%u "
                        "previous=%u cursor=%u\n",
                        count,last_count,(unsigned)cur);
                    return 0;
                }
                fprintf(stderr,
                    "AKIRA_ORIGINAL_FOLLOWUP packet=%u command=%02x%02x%02x "
                    "a3=%06x status=%02x duration=%02x%02x "
                    "stream=%02x%02x%02x%02x steps=%u events=%u\n",
                    count,(unsigned)packet[0],(unsigned)packet[1],
                    (unsigned)packet[2],(unsigned)regs.a[3],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE+2u],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE+3u],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE+4u],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE+5u],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE+6u],
                    (unsigned)original_ram[VF2_SOUND_DESCRIPTOR_BASE+7u],
                    (unsigned)(instructions+more_steps),
                    (unsigned)(observed_prior_events + bus.event_count));
                last_count = count;
                if (count == 32u) {
                    fprintf(stderr,
                        "AKIRA_FOLLOWUP_BOUNDARY count=32 reached=1 "
                        "worker_return_unobserved=1\n");
                }
            }
            if (regs.pc == UINT32_C(0x2158) &&
                rom_size > UINT32_C(0x2159) &&
                rom[0x2158] == 0x4eu && rom[0x2159] == 0x75u) {
                fprintf(stderr,
                    "AKIRA_FOLLOWUP_WORKER_RTS packet_count=%u "
                    "a3=%06x steps=%u\n",
                    (unsigned)original_ram[VF2_SOUND_COMMAND_COUNT],
                    (unsigned)regs.a[3],
                    (unsigned)(instructions+more_steps));
                {
                    vf2_sound_music_worker32_report worker;
                    size_t at;
                    prepare(&full_worker_board, rom, rom_size, 0u);
                    if (!check(vf2_sound_board_probe_akira_l2_first_worker32(
                        &full_worker_board, &worker) == VF2_OK,
                        "native 32-packet descriptor worker")) return 0;
                    if (memcmp(original_ram, full_worker_board.sound_ram,
                               sizeof(original_ram)) != 0) {
                        for (at = 0u; at < sizeof(original_ram); ++at) {
                            if (original_ram[at] !=
                                full_worker_board.sound_ram[at]) break;
                        }
                        fprintf(stderr,
                            "AKIRA_WORKER32_RAM_MISMATCH offset=%05x "
                            "original=%02x native=%02x\n",
                            (unsigned)at,(unsigned)original_ram[at],
                            (unsigned)full_worker_board.sound_ram[at]);
                        return 0;
                    }
                    if (!check(
                        memcmp(&original_scsp, &full_worker_board.scsp,
                               sizeof(original_scsp)) == 0 &&
                        worker.packets_emitted == 32u &&
                        worker.pending_after == 32u &&
                        worker.ephemeral_a3_after == regs.a[3] &&
                        worker.countdown_after == UINT16_C(0x01b2) &&
                        worker.stream_after == UINT32_C(0x6157cc),
                        "full original/native worker32 SCSP/commit")) return 0;
                    fprintf(stderr,
                        "AKIRA_WORKER32_ORIGINAL_DIFF PASS "
                        "pc=%05x steps=%u packets=%u a3=%06x "
                        "duration=%04x stream=%06x ram=1 scsp=1\n",
                        (unsigned)regs.pc,
                        (unsigned)(instructions+more_steps),
                        (unsigned)worker.pending_after,
                        (unsigned)regs.a[3],
                        (unsigned)worker.countdown_after,
                        (unsigned)worker.stream_after);
                }
                return 1;
            }
            if (bus.event_count >= 60u) {
                observed_prior_events += (uint32_t)bus.event_count;
                bus.event_count = 0u; /* passive telemetry only */
            }
            status = vf2_m68k_run_slice(
                &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
            if (status != VF2_OK || done != 1u || !suspended) {
                fprintf(stderr,
                    "AKIRA_FOLLOWUP_REFUSED status=%d pc=%05x "
                    "steps=%u count=%u\n",
                    (int)status,(unsigned)regs.pc,
                    (unsigned)(instructions+more_steps),
                    (unsigned)original_ram[VF2_SOUND_COMMAND_COUNT]);
                return 0;
            }
        }
        fprintf(stderr,
            "AKIRA_FOLLOWUP_REFUSED instruction budget exhausted "
            "count=%u pc=%05x\n",
            (unsigned)original_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)regs.pc);
        return 0;
    }
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
        memcpy(fake_rom + (UINT32_C(0x615773) -
               VF2_SOUND_AUDIO_ROM_BASE), source, sizeof(source));
        /* The C probes only inspect the bounded stream. They don't
         * require a fabricated emulated ROM instruction body. */
        ok = native_case(fake_rom, sizeof(fake_rom), 0u) &&
             native_case(fake_rom, sizeof(fake_rom),
                         UINT16_C(0x0ffc));
        if (ok) {
            vf2_sound_music_worker32_report refused;
            prepare(&native_board, fake_rom, sizeof(fake_rom), 0u);
            memcpy(&snapshot, &native_board, sizeof(snapshot));
            memset(&refused, 0x5au, sizeof(refused));
            ok = check(
                vf2_sound_board_probe_akira_l2_first_worker32(
                    &native_board, &refused) == VF2_ERROR_UNSUPPORTED &&
                memcmp(&native_board, &snapshot, sizeof(snapshot)) == 0 &&
                refused.packets_emitted == UINT8_C(0x5a),
                "worker32 rejects unverified source without mutation");
        }
        if (ok) puts("AKIRA_15_NATIVE_ROM_FREE PASS");
        return ok ? 0 : 1;
    }
    if (argc != 2 &&
        !(argc == 3 && strcmp(argv[2], "--trace-continuation") == 0)) {
        fprintf(stderr, "usage: %s <owner-ROM-dir> "
                        "[--trace-continuation] | --rom-free\n", argv[0]);
        return 2;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                               &rom, &rom_size) != VF2_OK || rom == NULL) {
        fprintf(stderr, "AKIRA_15_FAIL ROM validation\n");
        return 2;
    }
    ok = owner_differential(
        rom, rom_size,
        argc == 3 && strcmp(argv[2], "--trace-continuation") == 0);
    free(rom);
    return ok ? 0 : 1;
}
