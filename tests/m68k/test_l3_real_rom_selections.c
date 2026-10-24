#include "vf2/m68k_step.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real ROM-backed selections, not claimed gameplay trigger events.
 * Compare the recovered C L3 UART receiver against the 68000 stepper
 * after EACH byte. The independent pre-state is copied before either runs.
 * No command is injected into the i960 gameplay producer.
 */
enum { L3_STEP_LIMIT = 20000u };
static vf2_sound_board board;
static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
static vf2_scsp step_scsp;
static const uint8_t *sample_bank;
static size_t sample_bank_size;
static int akira_wave_prefill_experiment;
static int akira_walk_probe;
static int akira_original_walk_preview;
static int akira_native_walk_preview;
static int akira_tima_49_preview;
static int akira_verify_composite_ticks;
static int verify_akira_original_walk;
static int akira_walk_strict_failed;

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static int table_command(const uint8_t *data, size_t size,
                         uint32_t profile, uint32_t column,
                         uint32_t *command)
{
    const size_t table = 0x1e44cu;
    uint32_t row_pointer = 0u;
    size_t row = 0u;

    if (data == NULL || command == NULL || profile == 0u ||
        profile > 18u || column >= 11u ||
        size < table + 19u * 4u ||
        le32(data + table) != 0u) {
        return 0;
    }
    row_pointer = le32(data + table + (size_t)profile * 4u);
    if (row_pointer < VF2_MAIN_DATA_BASE ||
        (row_pointer & UINT32_C(3)) != 0u) {
        return 0;
    }
    row = (size_t)(row_pointer - VF2_MAIN_DATA_BASE) +
          (size_t)column * 4u;
    if (row > size || size - row < 4u) {
        return 0;
    }
    *command = le32(data + row);
    return *command != 0u &&
           (*command >> 8u == UINT32_C(0x00ad23) ||
            *command >> 8u == UINT32_C(0x00ad24));
}

static int compare_ram(const uint8_t *a, const uint8_t *b, size_t byte_index)
{
    size_t i = 0u;
    for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
        /* The 68000 JSR to 0x49e spills its return address here;
         * the C routine intentionally does not simulate stack traffic. */
        if (i >= 0x7efcu && i < 0x7f00u) {
            continue;
        }
        if (a[i] != b[i]) {
            fprintf(stderr,
                    "L3 byte %u: first RAM mismatch @0x%05x C=%02x stepper=%02x\n",
                    (unsigned)byte_index, (unsigned)i,
                    (unsigned)a[i], (unsigned)b[i]);
            return 0;
        }
    }
    return 1;
}

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24u) |
           ((uint32_t)source[1] << 16u) |
           ((uint32_t)source[2] << 8u) | (uint32_t)source[3];
}

/* A0 is not recovered past 0x1e98. Pin precisely that frontier on
 * two authentic ROM command selectors, with one controlled descriptor
 * matching channel 0xd. The interpreter stops BEFORE the unknown body.
 */
/* Immediately after A0 initialized one real-ROM sound descriptor,
 * L2 is the next blocker: count +2 is 1, which the recovered C
 * stream worker intentionally refuses BEFORE mutation. Independently
 * step the original 68000 countdown instructions to PC 0x1f92,
 * where the still-unrecovered worker consumes the record stream.
 */
static int check_l2_handoff(uint32_t command, const uint8_t *rom,
                            size_t rom_size)
{
    vf2_sound_stream_maintenance_report tick;
    vf2_status cstatus, sstatus;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t steps = 0u;
    const uint32_t active = VF2_SOUND_DESCRIPTOR_BASE +
                            UINT32_C(3) * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    uint8_t *ram = board.sound_ram;
    const uint8_t *packets = command == UINT32_C(0x00ad2342) ?
                            (const uint8_t *)"\xbb\x21\x06\x9b\x42\x78" :
                            (const uint8_t *)"\xba\x21\x07\x9a\x01\x7f";
    size_t i;
    /* The three incoming L3 bytes already advanced this sound
     * board's write cursor by four. Never assume ring offset zero. */
    const uint16_t before = (uint16_t)(
        ((uint16_t)ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
        ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]);
    const uint16_t after = (uint16_t)((before + 8u) & 0x0fffu);

    memcpy(step_ram, ram, sizeof(step_ram));
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&tick, 0, sizeof(tick));
    cstatus = vf2_sound_board_maintain_streams(&board, &tick);
    if (cstatus != VF2_OK || tick.packets_emitted != 2u ||
        tick.bytes_consumed != 10u || tick.refused_stage != 0u ||
        ram[VF2_SOUND_COMMAND_COUNT] != 2u ||
        ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != (uint8_t)(after >> 8u) ||
        ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != (uint8_t)after ||
        ram[active] != 0u || ram[active + 1u] != 0xffu ||
        ram[active + 2u] != 0u || ram[active + 3u] != 0u) {
        fprintf(stderr, "L2 %08x: native B/90 expansion did not complete\n",
                (unsigned)command);
        return 0;
    }
    for (i = 0u; i < 6u; ++i) {
        size_t at = VF2_SOUND_COMMAND_RING_BASE +
                    (((size_t)before + (i / 3u) * 4u +
                      (i % 3u)) & (size_t)0x0fffu);
        if (ram[at] != packets[i]) {
            fprintf(stderr, "L2 %08x: packet byte[%u] got %02x expected %02x\n",
                    (unsigned)command, (unsigned)i,
                    (unsigned)ram[at], (unsigned)packets[i]);
            return 0;
        }
    }

    /* Independent reference prestate: the ROM worker starts at 0x1f7c.
     * Stop BEFORE RTS at 0x2198 after FF 2F termination. No C-only
     * SCSP L2 timer pokes are falsely compared to this worker subset. */
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x1f7c);
    regs.a[2] = active;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    sstatus = vf2_m68k_run(&bus, &regs, UINT32_C(0x2198),
                           0, 300u, &steps);
    fprintf(stderr,
            "L2 %08x detailed: count C/S %u/%u cursor C/S %04x/%04x "
            "flags C/S %02x/%02x latch C/S %02x/%02x "
            "countdown C/S %02x%02x/%02x%02x\n",
            (unsigned)command,
            (unsigned)ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)after,
            (unsigned)(((uint16_t)step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                       step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]),
            (unsigned)ram[active], (unsigned)step_ram[active],
            (unsigned)ram[active+1u], (unsigned)step_ram[active+1u],
            (unsigned)ram[active+2u], (unsigned)ram[active+3u],
            (unsigned)step_ram[active+2u], (unsigned)step_ram[active+3u]);
    {
        size_t ix;
        for (ix = 0u; ix < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++ix) {
            if (ram[active+ix] != step_ram[active+ix]) {
                fprintf(stderr,"L2 first descriptor diff +%02x C=%02x S=%02x\n",
                        (unsigned)ix,
                        (unsigned)ram[active+ix],
                        (unsigned)step_ram[active+ix]);
                break;
            }
        }
        for (ix = 0u; ix < 0x1000u; ++ix) {
            if (ram[VF2_SOUND_COMMAND_RING_BASE+ix] !=
                step_ram[VF2_SOUND_COMMAND_RING_BASE+ix]) {
                fprintf(stderr,"L2 first ring diff +%03x C=%02x S=%02x\n",
                        (unsigned)ix,
                        (unsigned)ram[VF2_SOUND_COMMAND_RING_BASE+ix],
                        (unsigned)step_ram[VF2_SOUND_COMMAND_RING_BASE+ix]);
                break;
            }
        }
    }
    if (sstatus != VF2_OK || regs.pc != UINT32_C(0x2198) ||
        step_ram[VF2_SOUND_COMMAND_COUNT] != 2u ||
        step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != (uint8_t)(after >> 8u) ||
        step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != (uint8_t)after ||
        memcmp(ram + active, step_ram + active,
               VF2_SOUND_STREAM_DESCRIPTOR_BYTES) != 0 ||
        memcmp(ram + VF2_SOUND_COMMAND_RING_BASE,
               step_ram + VF2_SOUND_COMMAND_RING_BASE, 0x1000u) != 0) {
        fprintf(stderr,
                "L2 %08x: ROM differential NOT proven: S=%d PC=%05x steps=%u\n",
                (unsigned)command, (int)sstatus, (unsigned)regs.pc,
                (unsigned)steps);
        return 0;
    }
    fprintf(stderr, "MATCH AD L2 %08x: B and 90 command packets; "
                    "68000 0x2198 and C ring/descriptor agree\n",
            (unsigned)command);
    return 1;
}

/* Real-ROM A0 record allocation: C runs the complete SOUND dispatch,
 * while the original 68000 stepper stops BEFORE the 0x1f02 RTS.
 * Compare only descriptor state at this common semantic boundary;
 * ring counters differ because C has already completed its drain.
 */
static int check_a0_join(uint32_t command, const uint8_t *rom,
                         size_t rom_size)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    vf2_status cstatus, sstatus;
    uint32_t steps = 0u;
    uint32_t row, record;
    uint32_t expected_row = command == UINT32_C(0x00ad2342)
                            ? UINT32_C(0x60959c) : UINT32_C(0x60979e);
    uint32_t expected_record = command == UINT32_C(0x00ad2342)
                               ? UINT32_C(0x60b4fe) : UINT32_C(0x60b7f2);
    uint32_t row_index = (command >> 8u) & UINT32_C(0x7f);
    uint32_t selector = command & UINT32_C(0xff);
    uint8_t *ram = board.sound_ram;
    uint8_t *target = ram + VF2_SOUND_DESCRIPTOR_BASE +
                      3u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
    uint32_t expected_postinc;
    size_t i;

    if ((command != UINT32_C(0x00ad2342) &&
         command != UINT32_C(0x00ad2401)) ||
        0x8e12u + (size_t)row_index * 4u + 4u > rom_size) {
        return 0;
    }
    row = read_be32(rom + 0x8e12u + (size_t)row_index * 4u);
    if (row < VF2_SOUND_AUDIO_ROM_BASE ||
        (size_t)(row - VF2_SOUND_AUDIO_ROM_BASE) + 2u +
        4u * ((size_t)selector + 1u) > rom_size) {
        return 0;
    }
    record = read_be32(rom + (size_t)(row - VF2_SOUND_AUDIO_ROM_BASE) +
                       2u + (size_t)selector * 4u);
    if (row != expected_row || record != expected_record ||
        rom[(size_t)(record - VF2_SOUND_AUDIO_ROM_BASE)] != 0x8fu) {
        fprintf(stderr, "A0 ROM row or selected record invalid\n");
        return 0;
    }
    expected_postinc = record + 1u;

    /* Pristine copy; only slot 2 is free among six candidates,
     * with deliberate nonzero contents to verify the 0x1f04 clear. */
    memset(ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE, 0,
           VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
           VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
    ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE] = 0x80u;
    ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 1u] = 0x0du;
    memset(ram + VF2_SOUND_DESCRIPTOR_BASE, 0,
           8u * VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
    for (i = 0u; i < 6u; ++i) {
        ram[VF2_SOUND_DESCRIPTOR_BASE +
            (i+1u) * VF2_SOUND_STREAM_DESCRIPTOR_BYTES] = 0x80u;
    }
    memset(target, 0x5a, VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
    target[0] = 0u;

    memcpy(step_ram, ram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    cstatus = vf2_sound_board_dispatch_next(&board, &report);

    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    sstatus = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f02), 0,
                           L3_STEP_LIMIT, &steps);

    {
        const size_t off = (size_t)(target - ram);
        size_t pos;
        int found = 0;
        for (pos = 0u; pos < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++pos) {
            if (target[pos] != step_ram[off + pos]) {
                fprintf(stderr,
                        "A0 %08x first descriptor mismatch +0x%02x C=%02x S=%02x\n",
                        (unsigned)command, (unsigned)pos,
                        (unsigned)target[pos], (unsigned)step_ram[off + pos]);
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr,
                    "A0 %08x descriptor matches; other predicates: "
                    "C=%d handled=%d ring(C/S)=%u/%u SCSP=%d "
                    "voice=%02x ptr=%08x\n",
                    (unsigned)command, (int)cstatus, report.handled,
                    (unsigned)ram[VF2_SOUND_COMMAND_COUNT],
                    (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                    memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)),
                    (unsigned)target[0], (unsigned)read_be32(target + 4u));
        }
    }
    if (cstatus != VF2_OK || report.handler_address != UINT32_C(0x1f02) ||
        report.handled != 1 || ram[VF2_SOUND_COMMAND_COUNT] != 0u ||
        sstatus != VF2_OK || regs.pc != UINT32_C(0x1f02) ||
        step_ram[VF2_SOUND_COMMAND_COUNT] != 0u ||
        memcmp(target, step_ram + (size_t)(target-ram),
               VF2_SOUND_STREAM_DESCRIPTOR_BYTES) != 0 ||
        memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)) != 0 ||
        target[0] != 0x80u || target[2] != 0u || target[3] != 1u ||
        read_be32(target + 4u) != expected_postinc) {
        fprintf(stderr,
                "A0 %08x: C=%d handler=%05x handled=%d; "
                "stepper=%d pc=%05x steps=%u; slot2=%02x ptr=%08x\n",
                (unsigned)command, (int)cstatus,
                (unsigned)report.handler_address, report.handled,
                (int)sstatus, (unsigned)regs.pc, (unsigned)steps,
                (unsigned)target[0], (unsigned)read_be32(target+4u));
        return 0;
    }
    fprintf(stderr,
            "MATCH A0 %08x: stepper 0x1f02, slot2 initialized "
            "ptr=%08x / clear32, C consumed ring\n",
            (unsigned)command, (unsigned)expected_postinc);
    return check_l2_handoff(command, rom, rom_size);
}

static int check_one(uint32_t command, const uint8_t *rom, size_t rom_size,
                     int l3_only)
{
    const uint8_t bytes[3] = {
        (uint8_t)(command >> 16u),
        (uint8_t)(command >> 8u),
        (uint8_t)command
    };
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t steps = 0u;
    size_t i = 0u;
    /* The original UART receiver treats C0..DF as TWO-byte
     * commands: byte 2 files the ring; an F0..FE byte 3 is
     * ignored. AD/9E commands file only after byte 3. */
    const int short_form = bytes[0] >= 0xc0u &&
        bytes[0] < 0xe0u && bytes[1] < 0x80u &&
        bytes[2] >= 0xf0u && bytes[2] < 0xffu;
    const size_t file_index = short_form ? 1u : 2u;

    if (vf2_sound_board_initialize(&board, rom, rom_size,
                                    sample_bank, sample_bank_size) !=
        VF2_OK) {
        fprintf(stderr, "L3 initialization failed\n");
        return 0;
    }
    memcpy(step_ram, board.sound_ram, sizeof(step_ram));
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;

    for (i = 0u; i < 3u; ++i) {
        uint8_t *ram = board.sound_ram;
        vf2_status c_status = vf2_sound_board_file_uart_byte(&board, bytes[i]);
        vf2_status s_status = VF2_OK;

        vf2_m68k_regs_reset(&regs);
        regs.d[0] = bytes[i];
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x52e);
        bus.event_count = 0u;
        s_status = vf2_m68k_run(&bus, &regs, UINT32_C(0x5ea),
                                0, L3_STEP_LIMIT, &steps);
        if (c_status != VF2_OK || s_status != VF2_OK ||
            regs.pc != UINT32_C(0x5ea) ||
            !compare_ram(ram, step_ram, i) ||
            memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)) != 0) {
            fprintf(stderr,
                    "L3 command %08x byte[%u]=%02x: C=%d S=%d pc=%05x steps=%u\n",
                    (unsigned)command, (unsigned)i, (unsigned)bytes[i],
                    (int)c_status, (int)s_status, (unsigned)regs.pc,
                    (unsigned)steps);
            return 0;
        }
        /* Original 68000 files CD74 / CA7E / CB7F at byte 2;
         * F8 is an ignored trailer, not a third payload byte. */
        if (ram[VF2_SOUND_COMMAND_COUNT] !=
                (uint8_t)(i >= file_index) ||
            bus.event_count != (size_t)(i == file_index)) {
            fprintf(stderr, "L3 %08x byte %u: early/late ring file or poke\n",
                    (unsigned)command, (unsigned)i);
            return 0;
        }
        if (i == file_index &&
            (bus.events[0].address != UINT32_C(0x400001) ||
             bus.events[0].is_write == 0u)) {
            fprintf(stderr, "L3 %08x: missing final hardware poke\n",
                    (unsigned)command);
            return 0;
        }
    }
    if (board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] != bytes[0] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] != bytes[1] ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 2u] !=
        (short_form ? 0u : bytes[2]) ||
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] != 0u ||
        board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] != 0u ||
        board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] != 4u ||
        board.sound_ram[VF2_SOUND_COMMAND_COUNT] != 1u) {
        fprintf(stderr, "L3 %08x: resulting four-byte command ring differs\n",
                (unsigned)command);
        return 0;
    }
    if (!l3_only && !check_a0_join(command, rom, rom_size)) {
        return 0;
    }
    fprintf(stderr, "MATCH C/68000 L3%s %08x: %02x %02x %02x, ring count 1\n",
            l3_only ? " (isolated packet fixture)" : "",
            (unsigned)command, (unsigned)bytes[0],
            (unsigned)bytes[1], (unsigned)bytes[2]);
    return 1;
}

/* Replay an EXACT UART byte stream without invented fixed three-byte
 * packet boundaries. The ORIGINAL 68000 L3 distinguishes 2-byte Cx
 * commands, ignored F0-FE bytes and 3-byte AD/9E commands. Compare
 * C-vs-original memory/SCSP state and actual hardware-poke count at
 * every individual byte. This remains an isolated 68000 L3 fixture;
 * source-of-frame and hardware interrupt timing are separate oracles. */
/* Local preview from owner-ROM-68000-authored SCSP state.
 * The waveform uses the project's software SCSP model, NOT a
 * board-captured analog output or a C-vs-68000 sequencer differential.
 * Only write on explicit opt-in, after a nonzero eight-voice KEYON. */
static void akira_le16(uint8_t *p, uint16_t x)
{
    p[0] = (uint8_t)x;
    p[1] = (uint8_t)(x >> 8u);
}

static void akira_le32(uint8_t *p, uint32_t x)
{
    p[0] = (uint8_t)x;
    p[1] = (uint8_t)(x >> 8u);
    p[2] = (uint8_t)(x >> 16u);
    p[3] = (uint8_t)(x >> 24u);
}

static int write_akira_stereo_pcm(const char *directory,
                                   const char *filename,
                                   const int16_t *left, const int16_t *right,
                                   size_t frames, int sequenced)
{
    char path[1024];
    uint8_t header[44] = {0u};
    FILE *out;
    size_t i, nonzero = 0u;
    unsigned peak = 0u;
    int length;
    if (directory == NULL || filename == NULL ||
        left == NULL || right == NULL || frames == 0u ||
        frames > (size_t)(UINT32_MAX / 4u)) return 0;
    for (i = 0u; i < frames; ++i) {
        const int32_t a = (int32_t)left[i], b = (int32_t)right[i];
        const unsigned pa = (unsigned)(a < 0 ? -a : a);
        const unsigned pb = (unsigned)(b < 0 ? -b : b);
        nonzero += (size_t)(a != 0 || b != 0);
        if (peak < pa) peak = pa;
        if (peak < pb) peak = pb;
    }
    printf("AKIRA_MUSIC_PCM frames=%u nonzero=%u peak=%u "
           "original_driver=1 model_only=1 sequenced=%u full_song=0\n",
           (unsigned)frames, (unsigned)nonzero, peak, (unsigned)sequenced);
    if (nonzero == 0u) return 0;
    length = snprintf(path, sizeof(path), "%s/%s", directory, filename);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    out = fopen(path, "wb");
    if (out == NULL) return 0;
    memcpy(header, "RIFF", 4u);
    akira_le32(header + 4u, UINT32_C(36) + (uint32_t)frames * 4u);
    memcpy(header + 8u, "WAVEfmt ", 8u);
    akira_le32(header + 16u, 16u);
    akira_le16(header + 20u, 1u);
    akira_le16(header + 22u, 2u);
    akira_le32(header + 24u, 44100u);
    akira_le32(header + 28u, 44100u * 4u);
    akira_le16(header + 32u, 4u);
    akira_le16(header + 34u, 16u);
    memcpy(header + 36u, "data", 4u);
    akira_le32(header + 40u, (uint32_t)frames * 4u);
    if (fwrite(header, 1u, sizeof(header), out) != sizeof(header)) {
        (void)fclose(out);
        return 0;
    }
    for (i = 0u; i < frames; ++i) {
        uint8_t samples[4];
        akira_le16(samples, (uint16_t)left[i]);
        akira_le16(samples + 2u, (uint16_t)right[i]);
        if (fwrite(samples, 1u, sizeof(samples), out) != sizeof(samples)) {
            (void)fclose(out);
            return 0;
        }
    }
    if (fclose(out) != 0) return 0;
    printf("AKIRA_MUSIC_WAV path=%s frames=%u "
           "original_ROM_68000=1 sequenced=%u approximate_pacing=%u\n",
           path, (unsigned)frames, (unsigned)sequenced,
           (unsigned)sequenced);
    return 1;
}

/* EXPERIMENTAL listening-only buffer completeness test.
 * This is NOT original 68000 sequencing and MUST NOT be wired into
 * normal emulation. The original sound driver staged only the first
 * 512 bytes of each 8192-byte PCM8 wave window during initial KEYON.
 * Stream records at RAM 0x2000 + slot*0x20 carry source/destination
 * pointers to the following pages. Prefilling the remaining pages
 * establishes whether the isolated preview's rotor-like pulsing is
 * explained by empty (not yet DMA-serviced) pages.
 *
 * A correct implementation will recover original 68000 DMA timing,
 * respect its rolling RAM cursor and compare against a reference
 * SCSP emulator. This opt-in test cannot prove that.
 */
static size_t akira_prefill_wave_pages(
    uint8_t *ram, const uint8_t *samples, size_t samples_bytes,
    uint32_t previous_source[VF2_SCSP_SLOT_COUNT],
    uint32_t previous_destination[VF2_SCSP_SLOT_COUNT])
{
    size_t count = 0u;
    unsigned slot;
    if (ram == NULL || samples == NULL) return 0u;
    for (slot = 0u; slot < VF2_SCSP_SLOT_COUNT; ++slot) {
        const size_t record = 0x2000u + (size_t)slot * 0x20u;
        const uint32_t source = read_be32(ram + record + 2u);
        const uint32_t destination = read_be32(ram + record + 6u);
        const uint32_t window = destination & ~UINT32_C(0x1fff);
        const uint32_t wave_end = window + UINT32_C(0x2000);
        size_t size;
        if ((ram[record] & UINT8_C(0x80)) == 0u ||
            source < UINT32_C(0x800000) ||
            (uint64_t)(source - UINT32_C(0x800000)) >= samples_bytes ||
            destination < UINT32_C(0x10000) ||
            destination > VF2_SOUND_RAM_BYTES ||
            wave_end > VF2_SOUND_RAM_BYTES ||
            destination >= wave_end ||
            (destination & UINT32_C(0x1ff)) != 0u ||
            (previous_source[slot] == source &&
             previous_destination[slot] == destination)) {
            continue;
        }
        size = (size_t)(wave_end - destination);
        if (size > samples_bytes -
                   (size_t)(source - UINT32_C(0x800000))) {
            continue;
        }
        memcpy(ram + destination,
               samples + (size_t)(source - UINT32_C(0x800000)), size);
        previous_source[slot] = source;
        previous_destination[slot] = destination;
        count += size / 512u;
    }
    return count;
}

static int write_akira_model_preview(const char *directory, vf2_scsp *scsp)
{
    enum { FRAMES = 88200u }; /* 2 seconds at 44.1 kHz */
    static int16_t left[FRAMES], right[FRAMES];
    if (directory == NULL || scsp == NULL) return 0;
    if (vf2_scsp_render(scsp, left, right, FRAMES) != VF2_OK) {
        fprintf(stderr, "AKIRA_WAV unsupported SCSP renderer\n");
        return 0;
    }
    return write_akira_stereo_pcm(
        directory, "vf2_akira_original_8voices_scsp_model_preview.wav",
        left, right, FRAMES, 0);
}

/* Compare one ORIGINAL 0x2242 iteration with the bounded native
 * full-page arm from the identical pre-step state. Every descriptor is
 * inspected, including skips: admitting a copy when the original did
 * NOT copy is a strict failure. A real copy whose branch is still
 * unsupported by C is logged as uncovered (not fabricated as parity).
 * No ROM data or generated PCM is written outside this local test. */
static unsigned akira_native_full_page_vs_original(
    const uint8_t *pre, const vf2_scsp *pre_scsp,
    const uint8_t *post, const vf2_scsp *post_scsp,
    unsigned slot, int strict
)
{
    static vf2_sound_board candidate;
    const uint32_t rec = UINT32_C(0x2000) +
                         (uint32_t)slot * UINT32_C(0x20);
    const uint32_t prior_source = read_be32(pre + rec + 2u);
    const uint32_t prior_dest = read_be32(pre + rec + 6u);
    const uint32_t actual_source = read_be32(post + rec + 2u);
    const uint32_t actual_dest = read_be32(post + rec + 6u);
    const uint8_t mode = pre[rec + 1u];
    const uint32_t copy_dest = read_be32(
        pre + rec + (mode == 0u ? 0x0eu : 6u));
    /* A 0x23B0 short-loop page may reset source +2 to its loop
     * start, possibly back to exactly the previous source value.
     * A MOVEM transfer is instead witnessed by A2 advancing
     * 512 bytes to +6 from the mode-selected PRE-pass destination. */
    const int original_copied =
        actual_dest != prior_dest &&
        actual_dest == (uint32_t)(copy_dest + UINT32_C(512));
    vf2_status native_status;
    int short_branch = 0;
    int parity = 0;
    memcpy(candidate.sound_ram, pre, VF2_SOUND_RAM_BYTES);
    memcpy(&candidate.scsp, pre_scsp, sizeof(candidate.scsp));
    candidate.sample_rom = sample_bank;
    candidate.sample_rom_size = sample_bank_size;
    native_status =
        vf2_sound_board_step_stream_full_page(&candidate, slot);
    if (native_status == VF2_ERROR_UNSUPPORTED) {
        native_status = vf2_sound_board_step_stream_short_loop_page(
            &candidate, slot);
        short_branch = native_status == VF2_OK;
    }
    if (native_status == VF2_OK) {
        parity = original_copied &&
            copy_dest <= VF2_SOUND_RAM_BYTES - 512u &&
            memcmp(candidate.sound_ram + rec, post + rec, 0x20u) == 0 &&
            memcmp(candidate.sound_ram + copy_dest,
                   post + copy_dest, 512u) == 0 &&
            candidate.scsp.registers[0x408u] ==
                post_scsp->registers[0x408u];
        printf("AKIRA_NATIVE_PAGE slot=%u short_loop=%u status=%d "
               "original_copy=%u record_pcm_parity=%u "
               "source_before=%08x source_after=%08x "
               "destination_before=%08x destination_after=%08x\n",
               slot, (unsigned)short_branch, (int)native_status,
               (unsigned)original_copied, (unsigned)parity,
               (unsigned)prior_source,
               (unsigned)actual_source, (unsigned)prior_dest,
               (unsigned)actual_dest);
        if (strict && !parity) {
            akira_walk_strict_failed = 1;
        }
        return 1u;
    }
    if (original_copied) {
        printf("AKIRA_NATIVE_PAGE_UNCOVERED slot=%u "
               "original_copy=1 C_status=%d mode=%02x "
               "remaining=%08x\n",
               slot, (int)native_status, (unsigned)mode,
               (unsigned)read_be32(pre + rec + 0x0au));
    }
    return 0u;
}

/* Owner-ROM differential for the 0x23B0 *short/loop* arm.
 * The real Akira first-tick snapshot copied four ordinary full pages,
 * so it never exercised this path. Here the verified owner's same
 * 68000 ROM and sample ROM run on an independent synthetic stream
 * record, and the resulting 0x20-byte descriptor, 512-byte copied
 * window and SCSP 0x0408 register are compared with the C helper.
 * The independent fixture intentionally starts from 0x2242, not
 * from an injected 0x23B0 PC: all prefix gates must be traversed. */
static int akira_verify_short_loop_fixture(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp original_scsp;
    static vf2_sound_board native;
    vf2_m68k_bus bus;
    vf2_m68k_regs r;
    uint8_t *desc;
    uint32_t one = 0u, rest = 0u;
    vf2_status first, original_status, native_status;
    const uint32_t dest = UINT32_C(0x10200);
    int pass;
    if (rom == NULL || sample_bank == NULL ||
        sample_bank_size < UINT32_C(0x600)) {
        printf("AKIRA_SHORT_LOOP_SETUP sample_or_rom_missing=1\n");
        return 0;
    }
    memset(original_ram, 0, sizeof(original_ram));
    memset(&original_scsp, 0, sizeof(original_scsp));
    memset(&native, 0, sizeof(native));
    desc = original_ram + UINT32_C(0x2000);
    desc[0u] = UINT8_C(0x81);  /* active, loop enabled */
    desc[1u] = UINT8_C(0x10); /* bypass SCSP 0x0409 MIDI gate */
    /* 0x800200 old source, 0x10200 old target, 0x180 remaining.
     * Next src = (0x800400 & 0xffffff) + (512-384) = 0x800480.
     * Next len = (0xa5001000 & 0xffffff) - 128 = 0x0f80.
     * High byte A5 of loop length is also mirrored into SCSP 0408. */
    desc[2u] = 0x00u; desc[3u] = 0x80u;
    desc[4u] = 0x02u; desc[5u] = 0x00u;
    desc[6u] = 0x00u; desc[7u] = 0x01u;
    desc[8u] = 0x02u; desc[9u] = 0x00u;
    desc[0x0au] = 0x00u; desc[0x0bu] = 0x00u;
    desc[0x0cu] = 0x01u; desc[0x0du] = 0x80u;
    desc[0x12u] = 0x00u; desc[0x13u] = 0x80u;
    desc[0x14u] = 0x04u; desc[0x15u] = 0x00u;
    desc[0x16u] = 0xa5u; desc[0x17u] = 0x00u;
    desc[0x18u] = 0x10u; desc[0x19u] = 0x00u;
    desc[0x1au] = 0x00u; desc[0x1bu] = 0x02u;
    desc[0x1cu] = 0x00u; desc[0x1du] = 0x05u;
    memcpy(native.sound_ram, original_ram, sizeof(original_ram));
    native.sample_rom = sample_bank;
    native.sample_rom_size = sample_bank_size;
    if (vf2_scsp_attach_sample_ram(
            &original_scsp, original_ram, sizeof(original_ram)) != VF2_OK) {
        return 0;
    }
    memset(&bus, 0, sizeof(bus));
    bus.ram = original_ram;
    bus.ram_size = sizeof(original_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &original_scsp;
    vf2_m68k_regs_reset(&r);
    r.pc = UINT32_C(0x2242);
    r.a[0] = UINT32_C(0x2000);
    r.a[4] = UINT32_C(0x2506);
    r.a[5] = VF2_SCSP_CPU_BASE;
    r.a[6] = UINT32_C(0x1000);
    r.a[7] = UINT32_C(0x5000);
    native_status = vf2_sound_board_step_stream_short_loop_page(
        &native, 0u);
    first = vf2_m68k_run(&bus, &r, 0u, 0, 1u, &one);
    original_status = vf2_m68k_run(
        &bus, &r, UINT32_C(0x2242), 0, 100000u, &rest);
    pass = native_status == VF2_OK &&
        first == VF2_ERROR_UNSUPPORTED && one == 1u &&
        original_status == VF2_OK &&
        r.pc == UINT32_C(0x2242) &&
        r.a[0] == UINT32_C(0x2020) &&
        memcmp(native.sound_ram + 0x2000u,
               original_ram + 0x2000u, 0x20u) == 0 &&
        memcmp(native.sound_ram + dest,
               original_ram + dest, 512u) == 0 &&
        native.scsp.registers[0x408u] ==
            original_scsp.registers[0x408u] &&
        original_ram[0x2002u] == 0x00u &&
        original_ram[0x2003u] == 0x80u &&
        original_ram[0x2004u] == 0x04u &&
        original_ram[0x2005u] == 0x80u &&
        original_ram[0x200cu] == 0x0fu &&
        original_ram[0x200du] == 0x80u &&
        original_scsp.registers[0x408u] == 0xa5u;
    printf("AKIRA_SHORT_LOOP_ORIGINAL_DIFF matched=%u "
           "original_status=%d native_status=%d "
           "steps=%u original_next_src=%08x "
           "original_next_remaining=%08x scsp0408=%02x\n",
           (unsigned)pass, (int)original_status, (int)native_status,
           (unsigned)(one + rest),
           (unsigned)read_be32(original_ram + 0x2002u),
           (unsigned)read_be32(original_ram + 0x200au),
           (unsigned)original_scsp.registers[0x408u]);
    return pass;
}

/* Owner-ROM differential for the held-until-now 0x24BE/0x24D2
 * terminal release, seeded at 0x2242 to exercise all ROM gates. */
static int akira_verify_short_release_fixture(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp original_scsp;
    static vf2_sound_board native;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint8_t *desc;
    uint8_t *voice;
    uint32_t one = 0u, rest = 0u;
    vf2_status first, original_status, native_status;
    int pass;
    memset(original_ram, 0, sizeof(original_ram));
    memset(&original_scsp, 0, sizeof(original_scsp));
    memset(&native, 0, sizeof(native));
    desc = original_ram + UINT32_C(0x2000);
    voice = original_ram + VF2_SOUND_VOICE_TABLE_BASE + UINT32_C(0x30);
    desc[0] = 0x88u;  /* active, release bit3, no loop bit0 */
    desc[1] = 0x10u;  /* bypass mode0/MIDI7 gate */
    desc[0x0cu] = 0x00u; desc[0x0du] = 0x80u;
    desc[0x12u] = 6u; /* hardware SCSP offset 0x60 */
    desc[0x16u] = 0x42u;
    desc[0x1eu] = 0x12u; desc[0x1fu] = 0x34u;
    voice[2u] = 0x08u;
    voice[0x0bu] = 4u;
    original_scsp.registers[0x61u] = 0x7fu;
    memcpy(native.sound_ram, original_ram, sizeof(original_ram));
    memcpy(native.scsp.registers, original_scsp.registers,
           sizeof(native.scsp.registers));
    if (vf2_scsp_attach_sample_ram(
            &original_scsp, original_ram, sizeof(original_ram)) != VF2_OK) {
        return 0;
    }
    memset(&bus, 0, sizeof(bus));
    bus.ram = original_ram;
    bus.ram_size = sizeof(original_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &original_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x2242);
    regs.a[0] = UINT32_C(0x2000);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x5000);
    native_status = vf2_sound_board_step_stream_short_release(
        &native, 0u);
    first = vf2_m68k_run(&bus, &regs, 0u, 0, 1u, &one);
    original_status = vf2_m68k_run(
        &bus, &regs, UINT32_C(0x2242), 0, 100000u, &rest);
    pass = native_status == VF2_OK &&
        first == VF2_ERROR_UNSUPPORTED && one == 1u &&
        original_status == VF2_OK && regs.pc == UINT32_C(0x2242) &&
        regs.a[0] == UINT32_C(0x2020) &&
        memcmp(native.sound_ram + 0x2000u,
               original_ram + 0x2000u, 0x20u) == 0 &&
        memcmp(native.sound_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x30u,
               original_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x30u,
               0x10u) == 0 &&
        native.scsp.registers[0x61u] == original_scsp.registers[0x61u] &&
        native.scsp.registers[0x66u] == original_scsp.registers[0x66u] &&
        native.scsp.registers[0x67u] == original_scsp.registers[0x67u] &&
        native.scsp.registers[0x408u] == original_scsp.registers[0x408u] &&
        original_ram[0x2000u] == 0u;
    printf("AKIRA_SHORT_RELEASE_ORIGINAL_DIFF matched=%u "
           "original_status=%d native_status=%d steps=%u "
           "stop_pc=%05x voice_status=%02x voice_count=%02x "
           "scsp_key=%02x scsp0408=%02x\n",
           (unsigned)pass, (int)original_status, (int)native_status,
           (unsigned)(one + rest), (unsigned)regs.pc,
           original_ram[VF2_SOUND_VOICE_TABLE_BASE + 0x32u],
           original_ram[VF2_SOUND_VOICE_TABLE_BASE + 0x3bu],
           original_scsp.registers[0x61u],
           original_scsp.registers[0x408u]);
    return pass;
}

/* Original-ROM stepper parity for the two 0x2522 terminal paths.
 * The fixture drives 68000 from 0x2242, through the original
 * bit0-clear/bit3-clear controls, once without immediate release
 * (mode 0x10) and once through its 0x25F6 -> 0x260C branch
 * (mode 0x90). Never exports proprietary sample bytes. */
static int akira_verify_short_terminal_fixture(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp original_scsp;
    static vf2_sound_board native;
    vf2_m68k_bus bus;
    vf2_m68k_regs r;
    unsigned variant;
    int all_match = 1;
    for (variant = 0u; variant < 6u; ++variant) {
        uint8_t *desc;
        uint8_t *voice;
        uint32_t one = 0u, rest = 0u;
        vf2_status first, original_status, native_status;
        const uint32_t dest = UINT32_C(0x10200);
        const uint32_t sample_source = UINT32_C(0x800200);
        const uint32_t voice_offset = VF2_SOUND_VOICE_TABLE_BASE + 0x30u;
        int parity;
        memset(original_ram, 0, sizeof(original_ram));
        memset(&original_scsp, 0, sizeof(original_scsp));
        memset(&native, 0, sizeof(native));
        desc = original_ram + UINT32_C(0x2000);
        voice = original_ram + voice_offset;
        desc[0u] = UINT8_C(0x80);
        /* Cover normal/bit7 modes and SCSP-MIDI7-gated
         * exceptional modes 0x00 (MIDI7 set), 0x80 (MIDI7 clear). */
        desc[1u] = (variant == 0u ? UINT8_C(0x10) :
                    variant == 1u ? UINT8_C(0x90) :
                    (variant == 2u || variant == 4u) ?
                        UINT8_C(0x00) : UINT8_C(0x80));
        desc[2u] = 0x00u; desc[3u] = 0x80u;
        desc[4u] = 0x02u; desc[5u] = 0x00u;
        desc[6u] = 0x00u; desc[7u] = 0x01u;
        desc[8u] = 0x02u; desc[9u] = 0x00u;
        desc[0x0au] = 0u; desc[0x0bu] = 0u;
        desc[0x0cu] = 0x01u; desc[0x0du] = 0x80u;
        desc[0x0eu] = 0x00u; desc[0x0fu] = 0x01u;
        desc[0x10u] =
            (variant == 2u || variant == 4u) ? 0x02u : 0x01u;
        desc[0x11u] = 0x00u;
        desc[0x12u] = 6u;
        desc[0x16u] = 0x42u;
        desc[0x1au] = 0u; desc[0x1bu] = 2u;
        desc[0x1cu] = 0u; desc[0x1du] = 5u;
        voice[2u] = 0x08u; voice[0x0bu] = 4u;
        original_scsp.registers[0x61u] = 0x7fu;
        original_scsp.registers[0x408u] = 0x11u;
        original_scsp.registers[0x409u] =
            (variant == 2u || variant == 5u) ? 0x80u : 0x00u;
        memcpy(native.sound_ram, original_ram, sizeof(original_ram));
        native.sample_rom = sample_bank;
        native.sample_rom_size = sample_bank_size;
        memcpy(native.scsp.registers, original_scsp.registers,
               sizeof(native.scsp.registers));
        if (vf2_scsp_attach_sample_ram(
                &original_scsp, original_ram,
                sizeof(original_ram)) != VF2_OK) {
            return 0;
        }
        memset(&bus, 0, sizeof(bus));
        bus.ram = original_ram;
        bus.ram_size = sizeof(original_ram);
        bus.rom = rom;
        bus.rom_size = rom_size;
        bus.sample_rom = sample_bank;
        bus.sample_rom_size = sample_bank_size;
        bus.scsp = &original_scsp;
        vf2_m68k_regs_reset(&r);
        r.pc = UINT32_C(0x2242);
        r.a[0] = UINT32_C(0x2000);
        r.a[4] = UINT32_C(0x2506);
        r.a[5] = VF2_SCSP_CPU_BASE;
        r.a[6] = UINT32_C(0x1000);
        r.a[7] = UINT32_C(0x5000);
        native_status = vf2_sound_board_step_stream_short_terminal(
            &native, 0u);
        first = vf2_m68k_run(&bus, &r, 0u, 0, 1u, &one);
        original_status = vf2_m68k_run(
            &bus, &r, UINT32_C(0x2242), 0, 100000u, &rest);
        parity = native_status == VF2_OK &&
            first == VF2_ERROR_UNSUPPORTED && one == 1u &&
            original_status == VF2_OK &&
            r.pc == UINT32_C(0x2242) &&
            r.a[0] == UINT32_C(0x2020) &&
            read_be32(original_ram + 0x2002u) == sample_source &&
            read_be32(original_ram + 0x2006u) == dest &&
            memcmp(native.sound_ram + 0x2000u,
                   original_ram + 0x2000u, 0x20u) == 0 &&
            memcmp(native.sound_ram + dest,
                   original_ram + dest, 512u) == 0 &&
            memcmp(native.sound_ram + voice_offset,
                   original_ram + voice_offset, 0x10u) == 0 &&
            memcmp(native.scsp.registers, original_scsp.registers,
                   VF2_SCSP_REGISTER_BYTES) == 0;
        printf("AKIRA_SHORT_TERMINAL_ORIGINAL_DIFF variant=%u "
               "matched=%u native_status=%d m68k_status=%d "
               "steps=%u flag=%02x mode=%02x delta=%04x "
               "key=%02x voice_status=%02x\n",
               variant, (unsigned)parity, (int)native_status,
               (int)original_status, (unsigned)(one + rest),
               (unsigned)original_ram[0x2000u],
               (unsigned)original_ram[0x2001u],
               (unsigned)(
                   ((unsigned)original_ram[0x201eu] << 8u) |
                   original_ram[0x201fu]),
               (unsigned)original_scsp.registers[0x61u],
               (unsigned)original_ram[voice_offset + 2u]);
        if (!parity) all_match = 0;
    }
    return all_match;
}

/* Verified-ROM 0x2242 negative-timer -> 0x07A6 handoff.
 * This stops BEFORE executing the command dispatcher: its own
 * ring/descriptor work is a separate boundary. The C helper
 * must reproduce the entire pre-dispatch RAM+SCSP state. */
static int akira_verify_negative_timer_handoff(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp original_scsp;
    static vf2_sound_board native;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    unsigned variant;
    int all_match = 1;
    for (variant = 0u; variant < 3u; ++variant) {
        uint8_t *desc;
        uint16_t initial, increment;
        uint32_t one = 0u, rest = 0u;
        vf2_status first, original_status, native_status;
        int parity;
        memset(original_ram, 0, sizeof(original_ram));
        memset(&original_scsp, 0, sizeof(original_scsp));
        memset(&native, 0, sizeof(native));
        desc = original_ram + UINT32_C(0x2000);
        desc[0u] = 0x80u;
        desc[1u] = 0x10u;
        desc[0x16u] = 0x65u;
        original_scsp.registers[0x408u] = 0xaau;
        initial = variant == 0u ? UINT16_C(0x7fff) :
                  variant == 1u ? UINT16_C(0xffff) :
                                  UINT16_C(0x8000);
        increment = variant == 0u ? UINT16_C(1) :
                    variant == 1u ? UINT16_C(0) :
                                    UINT16_C(0x7fff);
        desc[0x1au] = (uint8_t)(increment >> 8u);
        desc[0x1bu] = (uint8_t)increment;
        desc[0x1cu] = (uint8_t)(initial >> 8u);
        desc[0x1du] = (uint8_t)initial;
        memcpy(native.sound_ram, original_ram, sizeof(original_ram));
        memcpy(native.scsp.registers, original_scsp.registers,
               sizeof(native.scsp.registers));
        if (vf2_scsp_attach_sample_ram(
                &original_scsp, original_ram,
                sizeof(original_ram)) != VF2_OK) {
            return 0;
        }
        memset(&bus, 0, sizeof(bus));
        bus.ram = original_ram;
        bus.ram_size = sizeof(original_ram);
        bus.rom = rom;
        bus.rom_size = rom_size;
        bus.sample_rom = sample_bank;
        bus.sample_rom_size = sample_bank_size;
        bus.scsp = &original_scsp;
        vf2_m68k_regs_reset(&regs);
        regs.pc = UINT32_C(0x2242);
        regs.a[0] = UINT32_C(0x2000);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = VF2_SCSP_CPU_BASE;
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x5000);
        native_status = vf2_sound_board_step_stream_timer_handoff(
            &native, 0u);
        first = vf2_m68k_run(&bus, &regs, 0u, 0, 1u, &one);
        original_status = vf2_m68k_run(
            &bus, &regs, UINT32_C(0x07a6), 0, 100u, &rest);
        parity = native_status == VF2_OK &&
            first == VF2_ERROR_UNSUPPORTED && one == 1u &&
            original_status == VF2_OK &&
            regs.pc == UINT32_C(0x07a6) &&
            memcmp(original_ram, native.sound_ram,
                   VF2_SOUND_RAM_BYTES) == 0 &&
            original_scsp.registers[0x408u] == 0xaau &&
            memcmp(original_scsp.registers, native.scsp.registers,
                   VF2_SCSP_REGISTER_BYTES) == 0;
        printf("AKIRA_NEGATIVE_TIMER_HANDOFF variant=%u "
               "matched=%u orig=%d C=%d steps=%u "
               "timer=%04x dispatcher_pc=%05x scsp408=%02x\n",
               variant, (unsigned)parity,
               (int)original_status, (int)native_status,
               (unsigned)(one + rest),
               (unsigned)(((unsigned)desc[0x1cu] << 8u) |
                            desc[0x1du]),
               (unsigned)regs.pc,
               (unsigned)original_scsp.registers[0x408u]);
        if (!parity) all_match = 0;
    }
    return all_match;
}

/* Original-ROM empty-queue handoff across 0x07A6 and shared
 * 0x265C exit, until the NEXT occurrence of 0x2242. This
 * proves one full main-loop iteration, not elapsed CPU time
 * or IRQ frequency. Uses distinct nonzero/zero exit bytes. */
static int akira_verify_empty_dispatch_cycle(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp original_scsp;
    static vf2_sound_board native;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    unsigned variant;
    int all_match = 1;
    for (variant = 0u; variant < 4u; ++variant) {
        uint8_t *desc;
        uint32_t one = 0u, rest = 0u;
        uint32_t native_next = UINT32_C(0xdeadbeef);
        const uint32_t expected_next =
            (variant & 1u) != 0u ?
            UINT32_C(0x2000) : UINT32_C(0x2020);
        vf2_status first, original_status, native_status;
        int parity;
        memset(original_ram, 0, sizeof(original_ram));
        memset(&original_scsp, 0, sizeof(original_scsp));
        memset(&native, 0, sizeof(native));
        desc = original_ram + UINT32_C(0x2000);
        /* first two inactive, last two negative timer */
        desc[0u] = variant >= 2u ? 0x80u : 0u;
        desc[0x16u] = (variant & 1u) != 0u ? 0u : 0x65u;
        desc[0x1au] = 0u; desc[0x1bu] = 1u;
        desc[0x1cu] = 0x7fu; desc[0x1du] = 0xffu;
        original_scsp.registers[0x408u] = 0xaau;
        memcpy(native.sound_ram, original_ram,
               sizeof(original_ram));
        memcpy(native.scsp.registers,
               original_scsp.registers,
               sizeof(native.scsp.registers));
        if (vf2_scsp_attach_sample_ram(
                &original_scsp, original_ram,
                sizeof(original_ram)) != VF2_OK) {
            return 0;
        }
        memset(&bus, 0, sizeof(bus));
        bus.ram = original_ram;
        bus.ram_size = sizeof(original_ram);
        bus.rom = rom;
        bus.rom_size = rom_size;
        bus.sample_rom = sample_bank;
        bus.sample_rom_size = sample_bank_size;
        bus.scsp = &original_scsp;
        vf2_m68k_regs_reset(&regs);
        regs.pc = UINT32_C(0x2242);
        regs.a[0] = UINT32_C(0x2000);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = VF2_SCSP_CPU_BASE;
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x5000);
        native_status = vf2_sound_board_step_stream_empty_dispatch(
            &native, 0u, &native_next);
        first = vf2_m68k_run(&bus, &regs, 0u, 0, 1u, &one);
        original_status = vf2_m68k_run(
            &bus, &regs, UINT32_C(0x2242), 0, 100u, &rest);
        parity = native_status == VF2_OK &&
            first == VF2_ERROR_UNSUPPORTED && one == 1u &&
            original_status == VF2_OK &&
            regs.pc == UINT32_C(0x2242) &&
            regs.a[0] == expected_next &&
            native_next == expected_next &&
            memcmp(original_ram, native.sound_ram,
                   VF2_SOUND_RAM_BYTES) == 0 &&
            memcmp(original_scsp.registers, native.scsp.registers,
                   VF2_SCSP_REGISTER_BYTES) == 0;
        printf("AKIRA_EMPTY_DISPATCH_CYCLE variant=%u "
               "matched=%u steps=%u status=%d "
               "next_original_a0=%05x next_native_a0=%05x "
               "timer=%04x scsp408=%02x\n",
               variant, (unsigned)parity,
               (unsigned)(one + rest), (int)original_status,
               (unsigned)regs.a[0], (unsigned)native_next,
               (unsigned)(((unsigned)desc[0x1cu] << 8u) |
                            desc[0x1du]),
               (unsigned)original_scsp.registers[0x408u]);
        if (!parity) all_match = 0;
    }
    return all_match;
}

/* Full first-tick Akira native-vs-68000 32-descriptor scan.
 * Independent snapshots, checked after EVERY original 0x2242
 * recurrence. No invented SCSP interrupt cadence. */
static int akira_verify_composite_native_walk(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t original_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp original_scsp;
    static vf2_sound_board native;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t native_a0 = UINT32_C(0x2000);
    unsigned pass;
    int parity = 1;
    memcpy(original_ram, step_ram, sizeof(original_ram));
    memcpy(&original_scsp, &step_scsp, sizeof(original_scsp));
    memset(&native, 0, sizeof(native));
    memcpy(native.sound_ram, step_ram, sizeof(step_ram));
    memcpy(&native.scsp, &step_scsp, sizeof(native.scsp));
    native.sample_rom = sample_bank;
    native.sample_rom_size = sample_bank_size;
    if (vf2_scsp_attach_sample_ram(
            &original_scsp, original_ram,
            sizeof(original_ram)) != VF2_OK ||
        vf2_scsp_attach_sample_ram(
            &native.scsp, native.sound_ram,
            sizeof(native.sound_ram)) != VF2_OK) {
        return 0;
    }
    memset(&bus, 0, sizeof(bus));
    bus.ram = original_ram;
    bus.ram_size = sizeof(original_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &original_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x2242);
    regs.a[0] = UINT32_C(0x2000);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x5000);
    for (pass = 0u; pass < VF2_SCSP_SLOT_COUNT; ++pass) {
        uint32_t first_steps = 0u, extra_steps = 0u;
        vf2_status c_status, first, original;
        unsigned slot;
        int matched;
        if (native_a0 < UINT32_C(0x2000) ||
            native_a0 >= UINT32_C(0x2400) ||
            ((native_a0 - UINT32_C(0x2000)) &
             UINT32_C(0x1f)) != 0u) {
            parity = 0;
            break;
        }
        slot = (unsigned)((native_a0 - UINT32_C(0x2000)) /
                          VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
        c_status = vf2_sound_board_step_stream_walk_one(
            &native, slot, &native_a0);
        first = vf2_m68k_run(&bus, &regs, 0u, 0, 1u, &first_steps);
        original = vf2_m68k_run(
            &bus, &regs, UINT32_C(0x2242), 0,
            100000u, &extra_steps);
        matched = c_status == VF2_OK &&
            first == VF2_ERROR_UNSUPPORTED &&
            first_steps == 1u && original == VF2_OK &&
            regs.pc == UINT32_C(0x2242) &&
            native_a0 == regs.a[0] &&
            memcmp(native.sound_ram, original_ram,
                   VF2_SOUND_RAM_BYTES) == 0 &&
            memcmp(native.scsp.registers,
                   original_scsp.registers,
                   VF2_SCSP_REGISTER_BYTES) == 0;
        if (!matched) {
            printf("AKIRA_COMPOSITE_WALK_MISMATCH pass=%u slot=%u "
                   "native_status=%d original_status=%d "
                   "orig_a0=%05x native_a0=%05x "
                   "steps=%u flag=%02x mode=%02x\n",
                   pass, slot, (int)c_status, (int)original,
                   (unsigned)regs.a[0], (unsigned)native_a0,
                   (unsigned)(first_steps + extra_steps),
                   original_ram[0x2000u + slot * 0x20u],
                   original_ram[0x2001u + slot * 0x20u]);
            parity = 0;
            break;
        }
    }
    printf("AKIRA_COMPOSITE_WALK_32 parity=%u passes=%u "
           "next_a0=%05x queue=%u\n",
           (unsigned)(parity && pass == VF2_SCSP_SLOT_COUNT &&
                       native_a0 == UINT32_C(0x2000)),
           pass, (unsigned)native_a0,
           original_ram[VF2_SOUND_COMMAND_COUNT]);
    return parity && pass == VF2_SCSP_SLOT_COUNT &&
           native_a0 == UINT32_C(0x2000);
}

/* Independent ROM-backed resume test: run the authentic first Akira
 * 32-record walk either uninterrupted or as 7-instruction quantums.
 * A real IRQ does NOT occur here, so this tests only CPU-state
 * preservation and correct unsupported-opcode vs budget signaling.
 * The first instruction must be stepped separately: 0x2242 is both
 * the start and STOP PC of every descriptor iteration. */
static int akira_verify_walker_instruction_slices(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t full_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t sliced_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp full_scsp, sliced_scsp;
    vf2_m68k_bus bus_full, bus_sliced;
    vf2_m68k_regs regs_full, regs_sliced;
    unsigned pass, reg;
    uint32_t full_total = 0u, sliced_total = 0u, quantum_count = 0u;
    memcpy(full_ram, step_ram, sizeof(full_ram));
    memcpy(sliced_ram, step_ram, sizeof(sliced_ram));
    memcpy(&full_scsp, &step_scsp, sizeof(full_scsp));
    memcpy(&sliced_scsp, &step_scsp, sizeof(sliced_scsp));
    if (vf2_scsp_attach_sample_ram(
            &full_scsp, full_ram, sizeof(full_ram)) != VF2_OK ||
        vf2_scsp_attach_sample_ram(
            &sliced_scsp, sliced_ram, sizeof(sliced_ram)) != VF2_OK) {
        return 0;
    }
    memset(&bus_full, 0, sizeof(bus_full));
    memset(&bus_sliced, 0, sizeof(bus_sliced));
    bus_full.ram = full_ram;
    bus_full.ram_size = sizeof(full_ram);
    bus_full.rom = rom;
    bus_full.rom_size = rom_size;
    bus_full.sample_rom = sample_bank;
    bus_full.sample_rom_size = sample_bank_size;
    bus_full.scsp = &full_scsp;
    bus_sliced.ram = sliced_ram;
    bus_sliced.ram_size = sizeof(sliced_ram);
    bus_sliced.rom = rom;
    bus_sliced.rom_size = rom_size;
    bus_sliced.sample_rom = sample_bank;
    bus_sliced.sample_rom_size = sample_bank_size;
    bus_sliced.scsp = &sliced_scsp;
    vf2_m68k_regs_reset(&regs_full);
    regs_full.pc = UINT32_C(0x2242);
    regs_full.a[0] = UINT32_C(0x2000);
    regs_full.a[4] = UINT32_C(0x2506);
    regs_full.a[5] = VF2_SCSP_CPU_BASE;
    regs_full.a[6] = UINT32_C(0x1000);
    regs_full.a[7] = UINT32_C(0x5000);
    regs_sliced = regs_full;

    for (pass = 0u; pass < VF2_SCSP_SLOT_COUNT; ++pass) {
        uint32_t steps_full_first = 0u, steps_full_rest = 0u;
        uint32_t steps_sliced_first = 0u;
        vf2_status a, b, c;
        int suspended = 0;
        unsigned quantum;
        a = vf2_m68k_run(
            &bus_full, &regs_full, 0u, 0,
            1u, &steps_full_first);
        b = vf2_m68k_run(
            &bus_full, &regs_full, UINT32_C(0x2242),
            0, 100000u, &steps_full_rest);
        full_total += steps_full_first + steps_full_rest;
        c = vf2_m68k_run_slice(
            &bus_sliced, &regs_sliced, 0u, 0,
            1u, &steps_sliced_first, &suspended);
        if (a != VF2_ERROR_UNSUPPORTED ||
            steps_full_first != 1u || b != VF2_OK ||
            c != VF2_OK || !suspended ||
            steps_sliced_first != 1u) {
            printf("AKIRA_M68K_SLICES_PREP_FAIL pass=%u a=%d b=%d c=%d\n",
                   pass, (int)a, (int)b, (int)c);
            return 0;
        }
        sliced_total += steps_sliced_first;
        for (quantum = 0u; quantum < 10000u; ++quantum) {
            uint32_t done = 0u;
            c = vf2_m68k_run_slice(
                &bus_sliced, &regs_sliced,
                UINT32_C(0x2242), 0, 7u,
                &done, &suspended);
            ++quantum_count;
            sliced_total += done;
            if (c != VF2_OK) {
                printf("AKIRA_M68K_SLICES_FAIL pass=%u "
                       "quantum=%u pc=%05x status=%d\n",
                       pass, quantum,
                       (unsigned)regs_sliced.pc, (int)c);
                return 0;
            }
            if (!suspended) break;
        }
        if (quantum == 10000u ||
            full_total != sliced_total ||
            regs_full.pc != regs_sliced.pc ||
            memcmp(full_ram, sliced_ram, sizeof(full_ram)) != 0 ||
            memcmp(full_scsp.registers, sliced_scsp.registers,
                   VF2_SCSP_REGISTER_BYTES) != 0) {
            printf("AKIRA_M68K_SLICES_STATE_FAIL pass=%u "
                   "full=%u sliced=%u a0_full=%05x a0_sliced=%05x\n",
                   pass, (unsigned)full_total,
                   (unsigned)sliced_total,
                   (unsigned)regs_full.a[0],
                   (unsigned)regs_sliced.a[0]);
            return 0;
        }
        for (reg = 0u; reg < 8u; ++reg) {
            if (regs_full.d[reg] != regs_sliced.d[reg] ||
                regs_full.a[reg] != regs_sliced.a[reg]) return 0;
        }
        if (regs_full.x != regs_sliced.x ||
            regs_full.n != regs_sliced.n ||
            regs_full.z != regs_sliced.z ||
            regs_full.v != regs_sliced.v ||
            regs_full.c != regs_sliced.c) return 0;
    }
    printf("AKIRA_M68K_SLICES_ORIGINAL_DIFF parity=1 "
           "descriptors=%u steps_full=%u steps_sliced=%u "
           "quanta=%u quantum_max=7 next_a0=%05x "
           "irq_and_cpu_cycles_not_modeled=1\n",
           (unsigned)VF2_SCSP_SLOT_COUNT,
           (unsigned)full_total, (unsigned)sliced_total,
           quantum_count, (unsigned)regs_sliced.a[0]);
    return full_total == sliced_total &&
           regs_full.a[0] == UINT32_C(0x2000);
}

/* Sound CPU's actual ROM vector 26 / 0x601F16 ISR, now entered
 * through the architectural stack frame instead of synthetic direct
 * PC injection. Compare authentic ROM code on matching independent
 * memories until the RTE opcode at 0x1F7A; the architectural run
 * then performs RTE to the interrupted main-loop PC.
 *
 * This compares body semantics but NOT a timed IRQ clock, and
 * excludes stack scratch space from RAM parity because the legacy
 * direct-entry path had no interrupt frame. */
static int akira_verify_l2_autovector_frame(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t legacy_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t vector_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp legacy_scsp, vector_scsp;
    vf2_m68k_bus legacy_bus, vector_bus;
    vf2_m68k_regs legacy_regs, vector_regs;
    uint32_t legacy_steps = 0u, vector_steps = 0u, rte_steps = 0u;
    uint32_t timed_cycles = 0u, timed_steps = 0u, middle_steps = 0u;
    uint32_t irq_entry_cycles = 0u;
    int timed_suspended = 0;
    vf2_status legacy_status, begin_status, body_status, rte_status;
    vf2_status preflight_status = VF2_ERROR_UNSUPPORTED;
    vf2_status prologue_status = VF2_ERROR_UNSUPPORTED;
    vf2_status mid_status = VF2_ERROR_UNSUPPORTED;
    vf2_status epilogue_preflight = VF2_ERROR_UNSUPPORTED;
    vf2_status epilogue_status = VF2_ERROR_UNSUPPORTED;
    int match;
    if (rom_size < UINT32_C(0x1f7c)) return 0;
    memcpy(legacy_ram, step_ram, sizeof(legacy_ram));
    memcpy(vector_ram, step_ram, sizeof(vector_ram));
    memcpy(&legacy_scsp, &step_scsp, sizeof(legacy_scsp));
    memcpy(&vector_scsp, &step_scsp, sizeof(vector_scsp));
    if (vf2_scsp_attach_sample_ram(
            &legacy_scsp, legacy_ram, sizeof(legacy_ram)) != VF2_OK ||
        vf2_scsp_attach_sample_ram(
            &vector_scsp, vector_ram, sizeof(vector_ram)) != VF2_OK) {
        return 0;
    }
    memset(&legacy_bus, 0, sizeof(legacy_bus));
    memset(&vector_bus, 0, sizeof(vector_bus));
    legacy_bus.ram = legacy_ram;
    legacy_bus.ram_size = sizeof(legacy_ram);
    legacy_bus.rom = rom;
    legacy_bus.rom_size = rom_size;
    legacy_bus.sample_rom = sample_bank;
    legacy_bus.sample_rom_size = sample_bank_size;
    legacy_bus.scsp = &legacy_scsp;
    vector_bus.ram = vector_ram;
    vector_bus.ram_size = sizeof(vector_ram);
    vector_bus.rom = rom;
    vector_bus.rom_size = rom_size;
    vector_bus.sample_rom = sample_bank;
    vector_bus.sample_rom_size = sample_bank_size;
    vector_bus.scsp = &vector_scsp;
    vf2_m68k_regs_reset(&legacy_regs);
    legacy_regs.pc = UINT32_C(0x1f16);
    legacy_regs.a[4] = UINT32_C(0x2506);
    legacy_regs.a[5] = VF2_SCSP_CPU_BASE;
    legacy_regs.a[6] = UINT32_C(0x1000);
    legacy_regs.a[7] = UINT32_C(0x5000);
    vector_regs = legacy_regs;
    vector_regs.pc = UINT32_C(0x2242);
    begin_status = vf2_m68k_irq_autovector_enter_timed(
        &vector_bus, &vector_regs, 2u, &irq_entry_cycles);
    legacy_status = vf2_m68k_run(
        &legacy_bus, &legacy_regs, UINT32_C(0x1f7a),
        0, 100000u, &legacy_steps);
    body_status = VF2_ERROR_UNSUPPORTED;
    rte_status = VF2_ERROR_UNSUPPORTED;
    if (begin_status == VF2_OK) {
        /* Real VF2 ISR 0x1F16 MOVEM.L 0xFFF8,-(A7).
         * The official 68000 cost is 8+8*13=112 cycles.
         * A 111-cycle slice must not mutate even a byte. */
        preflight_status = vf2_m68k_run_cycle_slice(
            &vector_bus, &vector_regs, UINT32_C(0x1f1a),
            111u, &timed_cycles, &timed_steps, &timed_suspended);
        if (preflight_status == VF2_OK && timed_suspended &&
            timed_steps == 0u && timed_cycles == 0u &&
            vector_regs.pc == UINT32_C(0x1f16)) {
            prologue_status = vf2_m68k_run_cycle_slice(
                &vector_bus, &vector_regs, UINT32_C(0x1f1a),
                112u, &timed_cycles, &timed_steps,
                &timed_suspended);
        }
        if (prologue_status == VF2_OK && !timed_suspended &&
            timed_steps == 1u && timed_cycles == 112u &&
            vector_regs.pc == UINT32_C(0x1f1a)) {
            vector_steps = 1u;
            /* Original driver body; other opcode cycle timings
             * are not yet implemented, so no timing claim here. */
            mid_status = vf2_m68k_run(
                &vector_bus, &vector_regs, UINT32_C(0x1f76),
                0, 100000u, &middle_steps);
        }
        if (mid_status == VF2_OK) {
            vector_steps += middle_steps;
            /* MOVEM.L (A7)+,0x1FFF costs 12+8*13=116. */
            epilogue_preflight = vf2_m68k_run_cycle_slice(
                &vector_bus, &vector_regs, UINT32_C(0x1f7a),
                115u, &timed_cycles, &timed_steps, &timed_suspended);
            if (epilogue_preflight == VF2_OK && timed_suspended &&
                timed_steps == 0u && timed_cycles == 0u &&
                vector_regs.pc == UINT32_C(0x1f76)) {
                epilogue_status = vf2_m68k_run_cycle_slice(
                    &vector_bus, &vector_regs, UINT32_C(0x1f7a),
                    116u, &timed_cycles, &timed_steps,
                    &timed_suspended);
            }
        }
        if (epilogue_status == VF2_OK && !timed_suspended &&
            timed_steps == 1u && timed_cycles == 116u &&
            vector_regs.pc == UINT32_C(0x1f7a)) {
            ++vector_steps;
            body_status = VF2_OK;
        }
    }
    if (body_status == VF2_OK) {
        rte_status = vf2_m68k_run(
            &vector_bus, &vector_regs, UINT32_C(0x2242),
            0, 2u, &rte_steps);
    }
    match = begin_status == VF2_OK &&
        irq_entry_cycles == 44u &&
        preflight_status == VF2_OK &&
        prologue_status == VF2_OK &&
        epilogue_preflight == VF2_OK &&
        epilogue_status == VF2_OK &&
        legacy_status == VF2_OK &&
        body_status == VF2_OK &&
        rte_status == VF2_OK &&
        legacy_steps == vector_steps &&
        rte_steps == 1u &&
        vector_regs.pc == UINT32_C(0x2242) &&
        vector_regs.irq_active == 0u &&
        vector_regs.sr_high == UINT16_C(0x2000) &&
        vector_regs.a[7] == UINT32_C(0x5000) &&
        memcmp(legacy_ram, vector_ram, UINT32_C(0x4f00)) == 0 &&
        memcmp(legacy_ram + UINT32_C(0x5100),
               vector_ram + UINT32_C(0x5100),
               VF2_SOUND_RAM_BYTES - UINT32_C(0x5100)) == 0 &&
        memcmp(legacy_scsp.registers, vector_scsp.registers,
               VF2_SCSP_REGISTER_BYTES) == 0 &&
        memcmp(legacy_scsp.slot_active, vector_scsp.slot_active,
               sizeof(legacy_scsp.slot_active)) == 0 &&
        memcmp(legacy_scsp.slot_address, vector_scsp.slot_address,
               sizeof(legacy_scsp.slot_address)) == 0 &&
        memcmp(legacy_scsp.slot_step, vector_scsp.slot_step,
               sizeof(legacy_scsp.slot_step)) == 0 &&
        memcmp(legacy_scsp.slot_envelope, vector_scsp.slot_envelope,
               sizeof(legacy_scsp.slot_envelope)) == 0 &&
        memcmp(legacy_scsp.slot_envelope_state,
               vector_scsp.slot_envelope_state,
               sizeof(legacy_scsp.slot_envelope_state)) == 0 &&
        memcmp(legacy_scsp.slot_backward, vector_scsp.slot_backward,
               sizeof(legacy_scsp.slot_backward)) == 0 &&
        memcmp(legacy_scsp.midi_input, vector_scsp.midi_input,
               sizeof(legacy_scsp.midi_input)) == 0 &&
        legacy_scsp.midi_read_index == vector_scsp.midi_read_index &&
        legacy_scsp.midi_write_index == vector_scsp.midi_write_index;
    {
        unsigned index;
        for (index = 0u; index < 8u; ++index) {
            if (legacy_regs.d[index] != vector_regs.d[index] ||
                (index != 7u &&
                 legacy_regs.a[index] != vector_regs.a[index])) {
                printf("AKIRA_L2_AUTOVECTOR_REG_DIFF index=%u "
                       "direct_d=%08x vector_d=%08x "
                       "direct_a=%08x vector_a=%08x\n",
                       index, (unsigned)legacy_regs.d[index],
                       (unsigned)vector_regs.d[index],
                       (unsigned)legacy_regs.a[index],
                       (unsigned)vector_regs.a[index]);
                match = 0;
            }
        }
        if (memcmp(legacy_scsp.slot_active, vector_scsp.slot_active,
                   sizeof(legacy_scsp.slot_active)) != 0 ||
            memcmp(legacy_scsp.slot_address, vector_scsp.slot_address,
                   sizeof(legacy_scsp.slot_address)) != 0 ||
            memcmp(legacy_scsp.slot_step, vector_scsp.slot_step,
                   sizeof(legacy_scsp.slot_step)) != 0 ||
            memcmp(legacy_scsp.slot_envelope, vector_scsp.slot_envelope,
                   sizeof(legacy_scsp.slot_envelope)) != 0 ||
            memcmp(legacy_scsp.slot_envelope_state,
                   vector_scsp.slot_envelope_state,
                   sizeof(legacy_scsp.slot_envelope_state)) != 0 ||
            memcmp(legacy_scsp.slot_backward, vector_scsp.slot_backward,
                   sizeof(legacy_scsp.slot_backward)) != 0 ||
            memcmp(legacy_scsp.midi_input, vector_scsp.midi_input,
                   sizeof(legacy_scsp.midi_input)) != 0 ||
            legacy_scsp.midi_read_index != vector_scsp.midi_read_index ||
            legacy_scsp.midi_write_index != vector_scsp.midi_write_index) {
            printf("AKIRA_L2_AUTOVECTOR_SCSP_INTERNAL_DIFF "
                   "active=%u address=%u step=%u envelope=%u "
                   "state=%u backwards=%u midi=%u read=%u write=%u\n",
                   (unsigned)(memcmp(
                       legacy_scsp.slot_active, vector_scsp.slot_active,
                       sizeof(legacy_scsp.slot_active)) != 0),
                   (unsigned)(memcmp(
                       legacy_scsp.slot_address, vector_scsp.slot_address,
                       sizeof(legacy_scsp.slot_address)) != 0),
                   (unsigned)(memcmp(
                       legacy_scsp.slot_step, vector_scsp.slot_step,
                       sizeof(legacy_scsp.slot_step)) != 0),
                   (unsigned)(memcmp(
                       legacy_scsp.slot_envelope, vector_scsp.slot_envelope,
                       sizeof(legacy_scsp.slot_envelope)) != 0),
                   (unsigned)(memcmp(
                       legacy_scsp.slot_envelope_state,
                       vector_scsp.slot_envelope_state,
                       sizeof(legacy_scsp.slot_envelope_state)) != 0),
                   (unsigned)(memcmp(
                       legacy_scsp.slot_backward, vector_scsp.slot_backward,
                       sizeof(legacy_scsp.slot_backward)) != 0),
                   (unsigned)(memcmp(
                       legacy_scsp.midi_input, vector_scsp.midi_input,
                       sizeof(legacy_scsp.midi_input)) != 0),
                   (unsigned)(legacy_scsp.midi_read_index !=
                              vector_scsp.midi_read_index),
                   (unsigned)(legacy_scsp.midi_write_index !=
                              vector_scsp.midi_write_index));
        }
    }
    printf("AKIRA_L2_AUTOVECTOR_FRAME matched=%u "
           "enter=%d direct=%d body=%d rte=%d "
           "body_steps=%u legacy_steps=%u rte_steps=%u "
           "restored_pc=%05x stack=%05x irq_active=%u "
           "irq_entry_cycles=%u "
           "prologue_cycles=112 epilogue_cycles=116 "
           "full_isr_cycle_total_not_modeled=1\n",
           (unsigned)match, (int)begin_status,
           (int)legacy_status, (int)body_status, (int)rte_status,
           (unsigned)vector_steps, (unsigned)legacy_steps,
           (unsigned)rte_steps, (unsigned)vector_regs.pc,
           (unsigned)vector_regs.a[7],
           (unsigned)vector_regs.irq_active,
           (unsigned)irq_entry_cycles);
    return match;
}

/* Genuine ROM 0x1F32 LEA 0x2000(A6),A2 followed by
 * 0x1F36 JSR (d16,PC) to the note service. Check exact
 * MOTOROLA cycles 8+18, stack PC and full semantic equivalence
 * against an uninterrupted original-ROM execution through RTS.
 * This is only two timed opcodes, NOT total ISR timing. */
static int akira_verify_l2_jsr_cycle_prefix(
    const uint8_t *rom, size_t rom_size
)
{
    static uint8_t full_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t timed_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp full_scsp, timed_scsp;
    vf2_m68k_bus full_bus, timed_bus;
    vf2_m68k_regs full_regs, timed_regs;
    uint32_t full_steps = 0u, timed_body = 0u;
    uint32_t cycles = 0u, steps = 0u;
    vf2_status full_status, status;
    int suspended = 0, parity;
    unsigned reg;
    if (rom_size < UINT32_C(0x2200)) return 0;
    memcpy(full_ram, step_ram, sizeof(full_ram));
    memcpy(timed_ram, step_ram, sizeof(timed_ram));
    memcpy(&full_scsp, &step_scsp, sizeof(full_scsp));
    memcpy(&timed_scsp, &step_scsp, sizeof(timed_scsp));
    if (vf2_scsp_attach_sample_ram(
            &full_scsp, full_ram, sizeof(full_ram)) != VF2_OK ||
        vf2_scsp_attach_sample_ram(
            &timed_scsp, timed_ram, sizeof(timed_ram)) != VF2_OK)
        return 0;
    memset(&full_bus, 0, sizeof(full_bus));
    memset(&timed_bus, 0, sizeof(timed_bus));
    full_bus.ram = full_ram;
    full_bus.ram_size = sizeof(full_ram);
    full_bus.rom = rom;
    full_bus.rom_size = rom_size;
    full_bus.sample_rom = sample_bank;
    full_bus.sample_rom_size = sample_bank_size;
    full_bus.scsp = &full_scsp;
    timed_bus.ram = timed_ram;
    timed_bus.ram_size = sizeof(timed_ram);
    timed_bus.rom = rom;
    timed_bus.rom_size = rom_size;
    timed_bus.sample_rom = sample_bank;
    timed_bus.sample_rom_size = sample_bank_size;
    timed_bus.scsp = &timed_scsp;
    vf2_m68k_regs_reset(&full_regs);
    full_regs.pc = UINT32_C(0x1f32);
    full_regs.a[4] = UINT32_C(0x2506);
    full_regs.a[5] = VF2_SCSP_CPU_BASE;
    full_regs.a[6] = UINT32_C(0x1000);
    full_regs.a[7] = UINT32_C(0x5000);
    timed_regs = full_regs;
    full_status = vf2_m68k_run(
        &full_bus, &full_regs, UINT32_C(0x1f3a),
        0, 100000u, &full_steps);
    status = vf2_m68k_run_cycle_slice(
        &timed_bus, &timed_regs, UINT32_C(0x1f36),
        7u, &cycles, &steps, &suspended);
    if (status != VF2_OK || !suspended || cycles != 0u ||
        steps != 0u || timed_regs.pc != UINT32_C(0x1f32))
        return 0;
    status = vf2_m68k_run_cycle_slice(
        &timed_bus, &timed_regs, UINT32_C(0x1f36),
        8u, &cycles, &steps, &suspended);
    if (status != VF2_OK || suspended || cycles != 8u ||
        steps != 1u || timed_regs.pc != UINT32_C(0x1f36) ||
        timed_regs.a[2] != UINT32_C(0x3000)) return 0;
    status = vf2_m68k_run_cycle_slice(
        &timed_bus, &timed_regs, UINT32_C(0x21ba),
        17u, &cycles, &steps, &suspended);
    if (status != VF2_OK || !suspended || cycles != 0u ||
        steps != 0u || timed_regs.pc != UINT32_C(0x1f36))
        return 0;
    status = vf2_m68k_run_cycle_slice(
        &timed_bus, &timed_regs, UINT32_C(0x21ba),
        18u, &cycles, &steps, &suspended);
    if (status != VF2_OK || suspended || cycles != 18u ||
        steps != 1u || timed_regs.pc != UINT32_C(0x21ba) ||
        timed_regs.a[7] != UINT32_C(0x4ffc) ||
        timed_ram[0x4ffcu] != 0u ||
        timed_ram[0x4ffdu] != 0u ||
        timed_ram[0x4ffeu] != 0x1fu ||
        timed_ram[0x4fffu] != 0x3au) return 0;
    status = vf2_m68k_run(
        &timed_bus, &timed_regs, UINT32_C(0x1f3a),
        0, 100000u, &timed_body);
    parity = full_status == VF2_OK && status == VF2_OK &&
        full_steps == timed_body + 2u &&
        timed_regs.pc == UINT32_C(0x1f3a) &&
        timed_regs.a[7] == UINT32_C(0x5000) &&
        memcmp(full_ram, timed_ram, sizeof(full_ram)) == 0 &&
        memcmp(full_scsp.registers, timed_scsp.registers,
               VF2_SCSP_REGISTER_BYTES) == 0;
    for (reg = 0u; reg < 8u; ++reg) {
        if (timed_regs.d[reg] != full_regs.d[reg] ||
            timed_regs.a[reg] != full_regs.a[reg]) parity = 0;
    }
    if (timed_regs.x != full_regs.x ||
        timed_regs.n != full_regs.n ||
        timed_regs.z != full_regs.z ||
        timed_regs.v != full_regs.v ||
        timed_regs.c != full_regs.c) parity = 0;
    printf("AKIRA_L2_JSR_CYCLES matched=%u "
           "lea_pc=01f32 lea_cycles=8 "
           "jsr_pc=01f36 jsr_cycles=18 "
           "callee_pc=021ba return_pc=01f3a "
           "full_steps=%u split_steps=%u "
           "full_isr_timing_not_covered=1\n",
           (unsigned)parity,
           (unsigned)full_steps, (unsigned)(timed_body + 2u));
    return parity;
}

/* Original-ROM candidate page-refill service: 0x2242 walker.
 * Run one iteration on an entirely separate snapshot to avoid
 * altering the ordinary music preview or original M68K bus.
 * 0x2242 is the recurrent main-loop head; the first instruction
 * is stepped separately so stop_pc=0x2242 catches the *next*
 * recurrence, not the unexecuted entry. Two SCSP 0x0409 cases
 * test the hardware-presented flag as a controlled hypothesis.
 * This is strictly diagnostic, never a fictitious C-side copy. */
static void akira_probe_original_walk(const uint8_t *rom, size_t rom_size,
                                      unsigned tick)
{
    static uint8_t walk_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp walk_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs r;
    unsigned variant;
    const uint32_t record = UINT32_C(0x2000);
    const uint32_t start = UINT32_C(0x10200);
    if (tick == 0u) {
        if (!akira_verify_short_loop_fixture(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_short_release_fixture(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_short_terminal_fixture(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_negative_timer_handoff(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_empty_dispatch_cycle(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_composite_native_walk(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_walker_instruction_slices(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_l2_autovector_frame(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
        if (!akira_verify_l2_jsr_cycle_prefix(rom, rom_size) &&
            verify_akira_original_walk) {
            akira_walk_strict_failed = 1;
        }
    }
    for (variant = 0u; variant < 2u; ++variant) {
        uint32_t stepped = 0u, walked = 0u;
        size_t changed = 0u, pcm_changed = 0u, i;
        uint32_t original_source = read_be32(step_ram + record + 2u);
        uint32_t original_target = read_be32(step_ram + record + 6u);
        uint32_t final_source, final_target;
        vf2_status first, result;
        memcpy(walk_ram, step_ram, sizeof(walk_ram));
        memcpy(&walk_scsp, &step_scsp, sizeof(walk_scsp));
        if (vf2_scsp_attach_sample_ram(&walk_scsp, walk_ram,
                                        sizeof(walk_ram)) != VF2_OK) {
            printf("AKIRA_WALK_PROBE_SETUP_FAIL tick=%u\n", tick);
            return;
        }
        /* The CPU reads SCSP MIDI/status 0x0409: try genuine observed
         * baseline and independently force only bit 7 in the
         * snapshot, to test whether this signal gates page service. */
        if (variant == 1u) {
            walk_scsp.registers[0x0409u] |= UINT8_C(0x80);
        }
        memset(&bus, 0, sizeof(bus));
        bus.rom = rom;
        bus.rom_size = rom_size;
        bus.ram = walk_ram;
        bus.ram_size = sizeof(walk_ram);
        bus.sample_rom = sample_bank;
        bus.sample_rom_size = sample_bank_size;
        bus.scsp = &walk_scsp;
        vf2_m68k_regs_reset(&r);
        r.pc = UINT32_C(0x2242);
        r.a[0] = UINT32_C(0x2000);
        r.a[4] = UINT32_C(0x2506);
        r.a[5] = VF2_SCSP_CPU_BASE;
        r.a[6] = UINT32_C(0x1000);
        r.a[7] = UINT32_C(0x5000);
        first = vf2_m68k_run(&bus, &r, 0u, 0, 1u, &stepped);
        result = vf2_m68k_run(&bus, &r, UINT32_C(0x2242), 0,
                              100000u, &walked);
        final_source = read_be32(walk_ram + record + 2u);
        final_target = read_be32(walk_ram + record + 6u);
        for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
            if (walk_ram[i] != step_ram[i]) {
                ++changed;
                if (i >= start && i < start + UINT32_C(0x1e00)) {
                    ++pcm_changed;
                }
            }
        }
        printf("AKIRA_WALK_PROBE tick=%u midi7=%u first_status=%d "
               "first_steps=%u status=%d steps=%u stop_pc=%05x "
               "a0=%08x flags=%02x src_before=%08x src_after=%08x "
               "dest_before=%08x dest_after=%08x ram_diff=%u "
               "pcm_next_pages_diff=%u sample_rom_attached=%u\n",
               tick, variant, (int)first, (unsigned)stepped,
               (int)result, (unsigned)walked, (unsigned)r.pc,
               (unsigned)r.a[0], walk_ram[record],
               (unsigned)original_source, (unsigned)final_source,
               (unsigned)original_target, (unsigned)final_target,
               (unsigned)changed, (unsigned)pcm_changed,
               (unsigned)(sample_bank != NULL));
        if (tick == 0u && variant == 0u &&
            result == VF2_OK && r.pc == UINT32_C(0x2242)) {
            /* Exact FIRST-PAGE native C vs original-MC68000 check.
             * This uses the identical PRE-WALK sound-RAM snapshot and
             * the owner's attached verified sample bank. It checks
             * only the recovered copy+cursors, not the predicate,
             * 32-slot traversal, IRQ timing, CPU residue or SCSP DSP.
             * Neither step_ram nor walk_ram is mutated by the C check. */
            static vf2_sound_board native_page;
            const uint8_t *const original_page = walk_ram + start;
            vf2_status c_page_status;
            int c_page_equal;
            memset(&native_page, 0, sizeof(native_page));
            memcpy(native_page.sound_ram, step_ram, sizeof(step_ram));
            native_page.sample_rom = sample_bank;
            native_page.sample_rom_size = sample_bank_size;
            c_page_status =
                vf2_sound_board_copy_stream_page_512(&native_page, 0u);
            c_page_equal = c_page_status == VF2_OK &&
                original_source == UINT32_C(0x8f96d4) &&
                original_target == UINT32_C(0x10200) &&
                final_source == UINT32_C(0x8f98d4) &&
                final_target == UINT32_C(0x10400) &&
                memcmp(native_page.sound_ram + start,
                       original_page, 512u) == 0 &&
                memcmp(native_page.sound_ram + record + 2u,
                       walk_ram + record + 2u, 8u) == 0;
            printf("AKIRA_NATIVE_PAGE_C68000_PARITY matched=%u "
                   "C_status=%d sample_bytes=512 source_after=%08x "
                   "destination_after=%08x original_steps=%u\n",
                   (unsigned)c_page_equal, (int)c_page_status,
                   (unsigned)final_source, (unsigned)final_target,
                   (unsigned)(stepped + walked));
            if (verify_akira_original_walk && !c_page_equal) {
                akira_walk_strict_failed = 1;
            }
            unsigned pass;
            unsigned admitted_pages = akira_native_full_page_vs_original(
                step_ram, &step_scsp, walk_ram, &walk_scsp, 0u,
                verify_akira_original_walk);
            unsigned successful_passes = 1u;
            unsigned advancing_slots = 1u; /* slot 0 above */
            uint32_t all_steps = stepped + walked;
            size_t wave_changed = 0u;
            uint32_t final_a0 = r.a[0];
            for (pass = 1u; pass < VF2_SCSP_SLOT_COUNT; ++pass) {
                uint32_t once = 0u, again = 0u;
                vf2_status one_status, walk_status;
                const uint32_t rec = UINT32_C(0x2000) +
                    (uint32_t)pass * UINT32_C(0x20);
                const uint32_t source_before = read_be32(walk_ram + rec + 2u);
                const uint32_t destination_before =
                    read_be32(walk_ram + rec + 6u);
                /* Preserve the original PRE-pass RAM/SCSP before
                 * allowing the 68000 to mutate its stream record.
                 * Static storage avoids 512KiB of stack. */
                static uint8_t native_before_ram[VF2_SOUND_RAM_BYTES];
                vf2_scsp native_before_scsp;
                memcpy(native_before_ram, walk_ram, sizeof(native_before_ram));
                memcpy(&native_before_scsp, &walk_scsp,
                       sizeof(native_before_scsp));
                bus.event_count = 0u;
                one_status = vf2_m68k_run(&bus, &r, 0u, 0, 1u, &once);
                walk_status = vf2_m68k_run(&bus, &r, UINT32_C(0x2242),
                                           0, 100000u, &again);
                all_steps += once + again;
                if (one_status != VF2_ERROR_UNSUPPORTED ||
                    once != 1u || walk_status != VF2_OK ||
                    r.pc != UINT32_C(0x2242)) {
                    printf("AKIRA_WALK_FULL_STOP pass=%u "
                           "single_status=%d status=%d pc=%05x "
                           "steps=%u a0=%05x\n",
                           pass, (int)one_status, (int)walk_status,
                           (unsigned)r.pc, (unsigned)again,
                           (unsigned)r.a[0]);
                    break;
                }
                ++successful_passes;
                admitted_pages += akira_native_full_page_vs_original(
                    native_before_ram, &native_before_scsp,
                    walk_ram, &walk_scsp, pass,
                    verify_akira_original_walk);
                final_a0 = r.a[0];
                if (source_before != read_be32(walk_ram + rec + 2u) ||
                    destination_before != read_be32(walk_ram + rec + 6u)) {
                    ++advancing_slots;
                    printf("AKIRA_WALK_FULL_SLOT slot=%u "
                           "source=%08x->%08x dest=%08x->%08x "
                           "steps=%u flag=%02x\n",
                           pass, (unsigned)source_before,
                           (unsigned)read_be32(walk_ram + rec + 2u),
                           (unsigned)destination_before,
                           (unsigned)read_be32(walk_ram + rec + 6u),
                           (unsigned)(once + again), walk_ram[rec]);
                }
            }
            for (i = UINT32_C(0x10000); i < UINT32_C(0x50000); ++i) {
                if (walk_ram[i] != step_ram[i]) ++wave_changed;
            }
            printf("AKIRA_NATIVE_PAGE_SUMMARY admitted=%u "
                   "original_copy_slots=%u strict_errors=%u\n",
                   admitted_pages, advancing_slots,
                   (unsigned)akira_walk_strict_failed);
            printf("AKIRA_WALK_FULL_SCAN passes=%u "
                   "advancing_slots=%u total_steps=%u final_a0=%08x "
                   "wave_ram_modified=%u queue=%u\n",
                   successful_passes, advancing_slots, (unsigned)all_steps,
                   (unsigned)final_a0, (unsigned)wave_changed,
                   (unsigned)walk_ram[VF2_SOUND_COMMAND_COUNT]);
            if (verify_akira_original_walk) {
                /* A skipped C branch must not silently pass simply
                 * because the original 68000 completed its scan. */
                const int passed = successful_passes == 32u &&
                    advancing_slots == 4u &&
                    admitted_pages == advancing_slots &&
                    all_steps == 588u &&
                    final_a0 == UINT32_C(0x2000) &&
                    wave_changed == 2039u &&
                    read_be32(walk_ram + 0x2002u) == UINT32_C(0x8f98d4) &&
                    read_be32(walk_ram + 0x2006u) == UINT32_C(0x10400) &&
                    walk_ram[VF2_SOUND_COMMAND_COUNT] == 0u;
                printf("AKIRA_ORIGINAL_WALK_STRICT parity=%u "
                       "passes=%u pages=%u native_admitted=%u "
                       "steps=%u pcm_bytes_changed=%u\n",
                       (unsigned)passed, successful_passes, advancing_slots,
                       admitted_pages, (unsigned)all_steps,
                       (unsigned)wave_changed);
                if (!passed) akira_walk_strict_failed = 1;
            }
        }
    }
}

/* Owner-ROM stream-service preview. Execute 32 authentic 68000
 * descriptor-walk iterations via 0x2242, using the current/live SCSP
 * and RAM. Each iteration's first instruction is stepped separately
 * so the recurrence PC is a real STOP, not an immediate no-op.
 * Only the cadence (one full scan per experimental 50-frame tick)
 * is a test hypothesis, NOT measured original arcade scheduling.
 * This is never used by public CI or production sound-board code. */
static int akira_execute_original_walk_frame(
    vf2_m68k_bus *bus, unsigned tick,
    uint32_t *total_steps, uint32_t *page_copies)
{
    vf2_m68k_regs r;
    unsigned pass;
    vf2_m68k_regs_reset(&r);
    r.pc = UINT32_C(0x2242);
    r.a[0] = UINT32_C(0x2000);
    r.a[4] = UINT32_C(0x2506);
    r.a[5] = VF2_SCSP_CPU_BASE;
    r.a[6] = UINT32_C(0x1000);
    r.a[7] = UINT32_C(0x5000);
    for (pass = 0u; pass < VF2_SCSP_SLOT_COUNT; ++pass) {
        const uint32_t rec = UINT32_C(0x2000) +
                             (uint32_t)pass * UINT32_C(0x20);
        const uint32_t source = read_be32(bus->ram + rec + 2u);
        const uint32_t dest = read_be32(bus->ram + rec + 6u);
        uint32_t one = 0u, rest = 0u;
        vf2_status a, b;
        bus->event_count = 0u;
        a = vf2_m68k_run(bus, &r, 0u, 0, 1u, &one);
        b = vf2_m68k_run(bus, &r, UINT32_C(0x2242), 0,
                         100000u, &rest);
        *total_steps += one + rest;
        if (a != VF2_ERROR_UNSUPPORTED || one != 1u ||
            b != VF2_OK || r.pc != UINT32_C(0x2242)) {
            printf("AKIRA_ORIGINAL_WALK_FAIL tick=%u pass=%u "
                   "single=%d walk=%d steps=%u pc=%05x\n",
                   tick, pass, (int)a, (int)b,
                   (unsigned)(one + rest), (unsigned)r.pc);
            return 0;
        }
        if (source != read_be32(bus->ram + rec + 2u) &&
            dest != read_be32(bus->ram + rec + 6u)) {
            ++*page_copies;
        }
    }
    if (r.a[0] != UINT32_C(0x2000)) {
        printf("AKIRA_ORIGINAL_WALK_BAD_WRAP tick=%u a0=%08x\n",
               tick, (unsigned)r.a[0]);
        return 0;
    }
    if (tick == 0u || tick == 882u || tick == 4410u ||
        tick == 8819u) {
        printf("AKIRA_ORIGINAL_WALK_PROGRESS tick=%u "
               "steps_total=%u pages_total=%u "
               "slot0_source=%08x slot0_dest=%08x\n",
               tick, (unsigned)*total_steps,
               (unsigned)*page_copies,
               (unsigned)read_be32(bus->ram + 0x2002u),
               (unsigned)read_be32(bus->ram + 0x2006u));
    }
    return 1;
}

/* C/original 68000 32-descriptor differential for ONE of the
 * existing 50-frame experimental ticks. The authentic original RAM
 * remains authoritative for subsequent L2/ring operations. */
static int akira_compare_composite_long_tick(
    vf2_m68k_bus *bus, unsigned tick,
    uint32_t *total_steps, uint32_t *page_copies
)
{
    static vf2_sound_board native;
    uint32_t next_a0 = UINT32_C(0x2000);
    unsigned pass;
    memcpy(native.sound_ram, bus->ram, VF2_SOUND_RAM_BYTES);
    memcpy(&native.scsp, bus->scsp, sizeof(native.scsp));
    native.sample_rom = sample_bank;
    native.sample_rom_size = sample_bank_size;
    if (vf2_scsp_attach_sample_ram(
            &native.scsp, native.sound_ram,
            sizeof(native.sound_ram)) != VF2_OK) return 0;
    for (pass = 0u; pass < VF2_SCSP_SLOT_COUNT; ++pass) {
        vf2_status status;
        unsigned slot;
        if (next_a0 < UINT32_C(0x2000) ||
            next_a0 >= UINT32_C(0x2400) ||
            ((next_a0 - UINT32_C(0x2000)) & UINT32_C(31)) != 0u) {
            printf("AKIRA_COMPOSITE_10S_BAD_A0 tick=%u pass=%u a0=%05x\n",
                   tick, pass, (unsigned)next_a0);
            return 0;
        }
        slot = (unsigned)((next_a0 - UINT32_C(0x2000)) / 32u);
        status = vf2_sound_board_step_stream_walk_one(
            &native, slot, &next_a0);
        if (status != VF2_OK) {
            const uint32_t at = UINT32_C(0x2000) + slot * 32u;
            printf("AKIRA_COMPOSITE_10S_UNCOVERED tick=%u pass=%u "
                   "slot=%u status=%d flag=%02x mode=%02x "
                   "remaining=%08x\n",
                   tick, pass, slot, (int)status,
                   native.sound_ram[at], native.sound_ram[at + 1u],
                   (unsigned)read_be32(native.sound_ram + at + 0x0au));
            return 0;
        }
    }
    if (next_a0 != UINT32_C(0x2000) ||
        !akira_execute_original_walk_frame(
            bus, tick, total_steps, page_copies)) {
        printf("AKIRA_COMPOSITE_10S_ORIGINAL_FAIL tick=%u next_a0=%05x\n",
               tick, (unsigned)next_a0);
        return 0;
    }
    if (memcmp(native.sound_ram, bus->ram, VF2_SOUND_RAM_BYTES) != 0 ||
        memcmp(native.scsp.registers, bus->scsp->registers,
               VF2_SCSP_REGISTER_BYTES) != 0 ||
        memcmp(native.scsp.slot_active, bus->scsp->slot_active,
               sizeof(native.scsp.slot_active)) != 0 ||
        memcmp(native.scsp.slot_address, bus->scsp->slot_address,
               sizeof(native.scsp.slot_address)) != 0 ||
        memcmp(native.scsp.slot_step, bus->scsp->slot_step,
               sizeof(native.scsp.slot_step)) != 0 ||
        memcmp(native.scsp.slot_envelope, bus->scsp->slot_envelope,
               sizeof(native.scsp.slot_envelope)) != 0 ||
        memcmp(native.scsp.slot_envelope_state,
               bus->scsp->slot_envelope_state,
               sizeof(native.scsp.slot_envelope_state)) != 0 ||
        memcmp(native.scsp.slot_backward, bus->scsp->slot_backward,
               sizeof(native.scsp.slot_backward)) != 0 ||
        memcmp(native.scsp.midi_input, bus->scsp->midi_input,
               sizeof(native.scsp.midi_input)) != 0 ||
        native.scsp.midi_read_index != bus->scsp->midi_read_index ||
        native.scsp.midi_write_index != bus->scsp->midi_write_index) {
        size_t i;
        for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
            if (native.sound_ram[i] != bus->ram[i]) break;
        }
        printf("AKIRA_COMPOSITE_10S_DIVERGED tick=%u first_ram=%08x "
               "C=%02x original=%02x\n",
               tick, (unsigned)i,
               i < VF2_SOUND_RAM_BYTES ? native.sound_ram[i] : 0u,
               i < VF2_SOUND_RAM_BYTES ? bus->ram[i] : 0u);
        return 0;
    }
    if (tick == 0u || tick == 882u || tick == 4410u ||
        tick == 8819u) {
        printf("AKIRA_COMPOSITE_10S_PROGRESS tick=%u "
               "passes=32 parity=1 pages=%u original_steps=%u\n",
               tick, (unsigned)*page_copies, (unsigned)*total_steps);
    }
    return 1;
}

/* Opt-in native C walker preview: the original ROM still runs the
 * L2 ISR and 0x07A6 command drain. ONLY the 32-descriptor
 * 0x2242 refill walker is replaced by already C/68000-verified
 * code. It writes the C result back to the authoritative state.
 * The 50-sample tick is experimental, not arcade cycle timing. */
static int akira_execute_native_walk_frame(
    vf2_m68k_bus *bus, unsigned tick, uint32_t *page_copies
)
{
    static vf2_sound_board native;
    uint32_t next_a0 = UINT32_C(0x2000);
    unsigned pass;
    memcpy(native.sound_ram, bus->ram, VF2_SOUND_RAM_BYTES);
    memcpy(&native.scsp, bus->scsp, sizeof(native.scsp));
    native.sample_rom = sample_bank;
    native.sample_rom_size = sample_bank_size;
    if (vf2_scsp_attach_sample_ram(
            &native.scsp, native.sound_ram,
            sizeof(native.sound_ram)) != VF2_OK) return 0;
    for (pass = 0u; pass < VF2_SCSP_SLOT_COUNT; ++pass) {
        unsigned slot;
        uint32_t before_source, before_dest;
        const uint32_t desc_base = UINT32_C(0x2000);
        vf2_status status;
        if (next_a0 < desc_base ||
            next_a0 >= UINT32_C(0x2400) ||
            ((next_a0 - desc_base) & UINT32_C(31)) != 0u) {
            printf("AKIRA_NATIVE_WALK_BAD_A0 tick=%u pass=%u a0=%05x\n",
                   tick, pass, (unsigned)next_a0);
            return 0;
        }
        slot = (unsigned)((next_a0 - desc_base) / 32u);
        before_source = read_be32(native.sound_ram + next_a0 + 2u);
        before_dest = read_be32(native.sound_ram + next_a0 + 6u);
        status = vf2_sound_board_step_stream_walk_one(
            &native, slot, &next_a0);
        if (status != VF2_OK) {
            printf("AKIRA_NATIVE_WALK_UNCOVERED tick=%u slot=%u "
                   "status=%d\n", tick, slot, (int)status);
            return 0;
        }
        if (before_source != read_be32(
                native.sound_ram + desc_base + slot * 32u + 2u) &&
            before_dest != read_be32(
                native.sound_ram + desc_base + slot * 32u + 6u)) {
            ++*page_copies;
        }
    }
    if (next_a0 != UINT32_C(0x2000)) {
        printf("AKIRA_NATIVE_WALK_BAD_WRAP tick=%u a0=%05x\n",
               tick, (unsigned)next_a0);
        return 0;
    }
    memcpy(bus->ram, native.sound_ram, VF2_SOUND_RAM_BYTES);
    memcpy(bus->scsp, &native.scsp, sizeof(native.scsp));
    if (vf2_scsp_attach_sample_ram(
            bus->scsp, bus->ram, VF2_SOUND_RAM_BYTES) != VF2_OK) {
        return 0;
    }
    if (tick == 0u || tick == 882u || tick == 4410u ||
        tick == 8819u) {
        printf("AKIRA_NATIVE_WALK_PROGRESS tick=%u pages=%u "
               "source=C_2242 path=explicit_assumed_timing\n",
               tick, (unsigned)*page_copies);
    }
    return 1;
}

/* Owner-ROM SOUND TEST #11: original i960 command 00 AD 10 04.
 * Preserve the exact original BOOT channel descriptor table, not the
 * synthetic one-channel fixture used by the AD23/24 unit tests.
 * Music emits 9x/Bx/Cx packets to channels A..F: clearing boot channels
 * before the L2 tick produced a convincing but FALSE silent result.
 * Controlled original 68000 driver probe; not real-time gameplay. */
static int probe_akira_music(const uint8_t *rom, size_t rom_size,
                             const char *wav_dir, int strict, int progress,
                             const char *sequenced_dir)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    vf2_status c_status, to_record, tail_status;
    uint32_t steps = 0u, tail_steps = 0u, row, record;
    size_t i;
    unsigned active = 0u;
    size_t first_tick_count = 0u, first_drain = 0u;
    size_t total_drain = 0u, active_ticks = 0u;
    size_t last_tick_with_packets = 0u;
    unsigned peak_voices = 0u;
    int full_drain = 1;
    size_t rendered_ticks = 0u;
    enum { SAMPLE_FRAMES = 441000u, LEGACY_FRAMES_PER_TICK = 50u,
           SEQUENCE_TICKS = SAMPLE_FRAMES / LEGACY_FRAMES_PER_TICK };
    size_t frames_per_tick = LEGACY_FRAMES_PER_TICK;
    size_t sequence_ticks = SEQUENCE_TICKS;
    static int16_t music_left[SAMPLE_FRAMES];
    static int16_t music_right[SAMPLE_FRAMES];
    uint32_t last_source[VF2_SCSP_SLOT_COUNT];
    uint32_t last_destination[VF2_SCSP_SLOT_COUNT];
    size_t preview_pages = 0u;
    uint32_t real_walker_steps = 0u;
    uint32_t real_walker_pages = 0u;
    size_t composite_verified_ticks = 0u;
    size_t budget_checked_ticks = 0u;
    size_t budget_overloaded_ticks = 0u;
    uint64_t budget_instruction_sum = 0u;
    uint32_t budget_first_l2 = 0u;
    uint32_t budget_first_dispatch = 0u;
    uint32_t budget_first_walker = 0u;
    uint32_t budget_first_total = 0u;
    uint32_t first_fifo_steps[64u] = {0u};
    size_t first_fifo_count = 0u;
    size_t first_fifo_ceiling = 0u;
    uint32_t budget_peak_total = 0u;
    size_t budget_peak_tick = 0u;
    uint8_t *desc = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    memset(last_source, 0, sizeof(last_source));
    memset(last_destination, 0, sizeof(last_destination));
    if (!check_one(UINT32_C(0x00ad1004), rom, rom_size, 1)) {
        return 0;
    }
    if (rom_size < 0x9c60u) {
        fprintf(stderr, "AKIRA_ROM invalid truncated program\n");
        return 0;
    }
    if (akira_tima_49_preview) {
        /* Owner-ROM opt-in only: validate boot and L2 Timer-A
         * opcodes/targets before using the manual's FF - TIMA.
         * This proves the programmed timer, not actual IRQ
         * preemption, instruction cycles or board-level pacing. */
        const uint16_t boot_opcode = (uint16_t)(
            ((uint16_t)rom[0x2f4u] << 8u) | rom[0x2f5u]);
        const uint16_t irq_opcode = (uint16_t)(
            ((uint16_t)rom[0x1f1au] << 8u) | rom[0x1f1bu]);
        const uint16_t isr_tima = (uint16_t)(
            ((uint16_t)rom[0x1f1cu] << 8u) | rom[0x1f1du]);
        const uint16_t timer_register = (uint16_t)(
            ((uint16_t)rom[0x1f1eu] << 8u) | rom[0x1f1fu]);
        const uint16_t irq_enable = (uint16_t)(
            ((uint16_t)rom[0x1f22u] << 8u) | rom[0x1f23u]);
        const uint16_t irq_register = (uint16_t)(
            ((uint16_t)rom[0x1f24u] << 8u) | rom[0x1f25u]);
        uint16_t boot_tima = 0u;
        uint32_t interval = 0u;
        if (boot_opcode != UINT16_C(0x3b7c) ||
            irq_opcode != UINT16_C(0x3b7c) ||
            timer_register != UINT16_C(0x0418) ||
            irq_enable != UINT16_C(0x0040) ||
            irq_register != UINT16_C(0x0422) ||
            vf2_scsp_timer_a_interval_frames(
                isr_tima, &interval) != VF2_OK ||
            interval == 0u || SAMPLE_FRAMES % interval != 0u ||
            vf2_scsp_read_u16(
                &step_scsp, UINT32_C(0x0418), &boot_tima) != VF2_OK ||
            boot_tima != isr_tima) {
            printf("AKIRA_TIMA_ROM_REJECT boot=%04x irq=%04x "
                   "reload=%04x live=%04x enable=%04x/%04x\n",
                   boot_opcode, irq_opcode, isr_tima,
                   boot_tima, irq_enable, irq_register);
            return 0;
        }
        frames_per_tick = interval;
        sequence_ticks = SAMPLE_FRAMES / interval;
        printf("AKIRA_TIMA_ROM_PERIOD reload=%04x samples_per_tick=%u "
               "ten_second_ticks=%u theoretical_hz=%u "
               "actual_irq_delivery_verified=0\n",
               isr_tima, (unsigned)frames_per_tick,
               (unsigned)sequence_ticks,
               (unsigned)(44100u / frames_per_tick));
    }
    row = read_be32(rom + 0x8e12u + 0x10u * 4u);
    record = read_be32(rom + (size_t)(row - VF2_SOUND_AUDIO_ROM_BASE) +
                       2u + 4u * 4u);
    if (row != UINT32_C(0x608f24) ||
        record != UINT32_C(0x609c36) ||
        read_be32(rom + (size_t)(record - VF2_SOUND_AUDIO_ROM_BASE)) !=
            UINT32_C(0x10101010)) {
        fprintf(stderr, "AKIRA_ROM unexpected AD10/04 music record\n");
        return 0;
    }
    /* Original image 1 row 3 is active channel D. Preserve
     * ALL other boot descriptors, especially A/B/C/E/F channels. */
    if (desc[13u * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES] != UINT8_C(0x81) ||
        desc[13u * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES + 1u] !=
            UINT8_C(0x0d) ||
        memcmp(desc, step_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE,
               VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
               VF2_SOUND_COMMAND_DESCRIPTOR_BYTES) != 0) {
        fprintf(stderr, "AKIRA_ROM boot channel descriptors mismatch\n");
        return 0;
    }
    printf("AKIRA_ROM_BOOT_CHANNELS original_20_descriptors=1 "
           "original_channel_D=1 injected_descriptors=0\n");
    if (vf2_scsp_attach_sample_ram(&board.scsp, board.sound_ram,
                                   sizeof(board.sound_ram)) != VF2_OK ||
        vf2_scsp_attach_sample_ram(&step_scsp, step_ram,
                                   sizeof(step_ram)) != VF2_OK) {
        return 0;
    }
    memset(&report, 0, sizeof(report));
    c_status = vf2_sound_board_dispatch_next(&board, &report);
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;
    vf2_m68k_regs_reset(&regs);
    regs.pc = UINT32_C(0x7a6);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = VF2_SCSP_CPU_BASE;
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x5000);
    to_record = vf2_m68k_run(&bus, &regs, UINT32_C(0x1e98),
                             0, 30000u, &steps);
    printf("AKIRA_A0_FRONTIER command=00ad1004 row=%06x record=%06x "
           "C_status=%d C_handler=%05x M_status=%d M_pc=%05x "
           "M_steps=%u descriptor_matches=%u\n",
           (unsigned)row, (unsigned)record,
           (int)c_status, (unsigned)report.handler_address,
           (int)to_record, (unsigned)regs.pc, (unsigned)steps,
           (unsigned)report.descriptors_matched);
    if (c_status != VF2_ERROR_UNSUPPORTED ||
        report.handler_address != UINT32_C(0x1e98) ||
        to_record != VF2_OK || regs.pc != UINT32_C(0x1e98)) {
        fprintf(stderr, "AKIRA_A0_FRONTIER failed: no original ROM join\n");
        return 0;
    }
    /* Beyond here is a DIAGNOSTIC of the previously unmodeled
     * low-header A0 body. Original success/unsupported are reported,
     * never laundered into native-C/SCSP music parity. */
    tail_status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                               0, 100000u, &tail_steps);
    for (i = 0u; i < VF2_SCSP_SLOT_COUNT; ++i) {
        active += (unsigned)(step_scsp.slot_active[i] != 0u);
    }
    printf("AKIRA_A0_TAIL original_status=%d pc=%05x steps=%u "
           "queue=%u active_slots=%u C_unrecovered=1 "
           "music_pcm_not_proven=1\n",
           (int)tail_status, (unsigned)regs.pc,
           (unsigned)tail_steps,
           (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT], active);
    if (tail_status == VF2_OK && regs.pc == UINT32_C(0x265c)) {
        size_t slot, tick;
        /* Show exact original descriptor pointers after AD10/04,
         * without dumping copyright-protected stream content. */
        for (slot = 0u; slot < 8u; ++slot) {
            const size_t at = VF2_SOUND_DESCRIPTOR_BASE +
                              slot * VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
            const uint32_t next = read_be32(step_ram + at + 4u);
            printf("AKIRA_A0_STREAM slot=%u flags=%02x%02x%02x%02x "
                   "next=%08x countdown=%02x%02x\n",
                   (unsigned)slot,
                   (unsigned)step_ram[at], (unsigned)step_ram[at + 1u],
                   (unsigned)step_ram[at + 2u],
                   (unsigned)step_ram[at + 3u], (unsigned)next,
                   (unsigned)step_ram[at + 12u],
                   (unsigned)step_ram[at + 13u]);
        }
        /* Original L2 IRQ probe: independent CPU invocation for a
         * finite number of ticks; never label these as real-time
         * scheduling or synthesize the recovered-C sequence. */
        for (tick = 0u;
             tick < ((sequenced_dir != NULL || akira_verify_composite_ticks) ?
                     sequence_ticks : (progress ? 1024u : 16u)); ++tick) {
            uint32_t original_steps = 0u;
            uint32_t drain_tick_steps = 0u;
            uint32_t walker_steps_before = real_walker_steps;
            vf2_status status;
            unsigned keyed = 0u;
            size_t voice;
            vf2_m68k_regs_reset(&regs);
            regs.pc = UINT32_C(0x1f16);
            regs.a[4] = UINT32_C(0x2506);
            regs.a[5] = VF2_SCSP_CPU_BASE;
            regs.a[6] = UINT32_C(0x1000);
            regs.a[7] = UINT32_C(0x5000); /* original reset SSP */
            /* Bus events are per invocation, not a persistent FIFO.
             * Reusing >64 diagnostic writes across many ticks caused
             * a false UNSUPPORTED at 0x2080 on tick 434. Reset ONLY
             * this bounded instrumentation log; never RAM or SCSP. */
            bus.event_count = 0u;
            status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a),
                                  0, 100000u, &original_steps);
            for (voice = 0u; voice < VF2_SCSP_SLOT_COUNT; ++voice) {
                keyed += (unsigned)(step_scsp.slot_active[voice] != 0u);
            }
            if (tick == 0u) first_tick_count =
                (size_t)step_ram[VF2_SOUND_COMMAND_COUNT];
            if (keyed > peak_voices) peak_voices = keyed;
            if (step_ram[VF2_SOUND_COMMAND_COUNT] > 0u) {
                ++active_ticks;
                last_tick_with_packets = tick;
            }
            if (!progress || tick < 16u ||
                step_ram[VF2_SOUND_COMMAND_COUNT] != 0u ||
                (tick % 64u) == 0u || status != VF2_OK) {
                printf("AKIRA_MUSIC_L2 tick=%u status=%d pc=%05x "
                       "steps=%u active_slots=%u queue=%u events=%u "
                       "no_playback_claim=1\n",
                       (unsigned)tick, (int)status, (unsigned)regs.pc,
                       (unsigned)original_steps, keyed,
                       (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                       (unsigned)bus.event_count);
            }
            if (status != VF2_OK || regs.pc != UINT32_C(0x1f7a)) {
                break; /* preserve original failure frontier */
            }
            {
                size_t dispatched = 0u;
                unsigned first_failed = 0u;
                while (step_ram[VF2_SOUND_COMMAND_COUNT] > 0u &&
                       dispatched < 64u) {
                    const uint8_t queued = step_ram[VF2_SOUND_COMMAND_COUNT];
                    const uint16_t cursor =
                        (uint16_t)(((uint16_t)step_ram[VF2_SOUND_COMMAND_CURSOR]
                          << 8u) | step_ram[VF2_SOUND_COMMAND_CURSOR + 1u]);
                    const uint16_t at = (uint16_t)(cursor & UINT16_C(0x0fff));
                    uint8_t bytes[3];
                    uint32_t dispatch_steps = 0u;
                    vf2_status dispatch_status;
                    size_t v;
                    unsigned now_active = 0u;
                    bytes[0] = step_ram[VF2_SOUND_COMMAND_RING_BASE + at];
                    bytes[1] = step_ram[VF2_SOUND_COMMAND_RING_BASE +
                                        ((at + 1u) & UINT16_C(0x0fff))];
                    bytes[2] = step_ram[VF2_SOUND_COMMAND_RING_BASE +
                                        ((at + 2u) & UINT16_C(0x0fff))];
                    vf2_m68k_regs_reset(&regs);
                    regs.pc = UINT32_C(0x7a6);
                    regs.a[4] = UINT32_C(0x2506);
                    regs.a[5] = VF2_SCSP_CPU_BASE;
                    regs.a[6] = UINT32_C(0x1000);
                    regs.a[7] = UINT32_C(0x5000);
                    bus.event_count = 0u;
                    dispatch_status = vf2_m68k_run(
                        &bus, &regs, UINT32_C(0x265c),
                        0, 100000u, &dispatch_steps);
                    drain_tick_steps += dispatch_steps;
                    if (tick == 0u &&
                        first_fifo_count <
                            sizeof(first_fifo_steps) /
                            sizeof(first_fifo_steps[0])) {
                        first_fifo_steps[first_fifo_count++] =
                            dispatch_steps;
                    }
                    for (v = 0u; v < VF2_SCSP_SLOT_COUNT; ++v) {
                        now_active += (unsigned)
                            (step_scsp.slot_active[v] != 0u);
                    }
                    if (dispatched < 12u ||
                        dispatch_status != VF2_OK || now_active > 0u) {
                        printf("AKIRA_MUSIC_DRAIN tick=%u index=%u "
                               "bytes=%02x%02x%02x status=%d pc=%05x "
                               "steps=%u count_before=%u count_after=%u "
                               "active_slots=%u\n",
                               (unsigned)tick, (unsigned)dispatched,
                               (unsigned)bytes[0], (unsigned)bytes[1],
                               (unsigned)bytes[2], (int)dispatch_status,
                               (unsigned)regs.pc, (unsigned)dispatch_steps,
                               (unsigned)queued,
                               (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                               now_active);
                    }
                    ++dispatched;
                    if (dispatch_status != VF2_OK ||
                        regs.pc != UINT32_C(0x265c) ||
                        step_ram[VF2_SOUND_COMMAND_COUNT] >= queued) {
                        first_failed = 1u;
                        break;
                    }
                }
                if (tick == 0u) first_drain = dispatched;
                if (first_failed != 0u) full_drain = 0;
                total_drain += dispatched;
                if (dispatched != 0u || !progress || tick < 16u ||
                    (tick % 64u) == 0u || first_failed != 0u) {
                    printf("AKIRA_MUSIC_DRAIN_SUMMARY tick=%u drained=%u "
                           "queue=%u failed=%u original_only=1\n",
                           (unsigned)tick, (unsigned)dispatched,
                           (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
                           first_failed);
                }
                if (first_failed != 0u) break;
            }
            if (akira_original_walk_preview &&
                !(akira_verify_composite_ticks ?
                  akira_compare_composite_long_tick(
                      &bus, (unsigned)tick, &real_walker_steps,
                      &real_walker_pages) :
                  akira_execute_original_walk_frame(
                      &bus, (unsigned)tick, &real_walker_steps,
                      &real_walker_pages))) {
                full_drain = 0;
                break;
            }
            if (akira_native_walk_preview &&
                !akira_execute_native_walk_frame(
                    &bus, (unsigned)tick, &real_walker_pages)) {
                full_drain = 0;
                break;
            }
            if (akira_tima_49_preview &&
                akira_verify_composite_ticks) {
                /* Real MAME Model 2A sound-clock configuration:
                 * 45.1584MHz / 4 = 11.2896MHz M68000.
                 * At 44.1kHz / 49 = 900 Timer-A expiries/sec,
                 * the ideal unpreempted capacity is 12544 CPU
                 * clocks per interval. Every base MC68000 opcode
                 * costs >=4 cycles. Thus instruction_count*4 is
                 * a CONSERVATIVE lower bound, not cycle timing.
                 *
                 * The current harness drains ALL queued commands
                 * synchronously once per modeled tick. Detect
                 * intervals where this can't fit even at the
                 * physically impossible 4-cycles-per-opcode rate.
                 * Never silently treat those as accurate playback. */
                enum { M68K_CYCLES_PER_TIMER_A = 12544u };
                const uint32_t walker_tick_steps =
                    real_walker_steps - walker_steps_before;
                const uint32_t total_tick_steps = original_steps +
                    drain_tick_steps + walker_tick_steps;
                if (tick == 0u) {
                    budget_first_l2 = original_steps;
                    budget_first_dispatch = drain_tick_steps;
                    budget_first_walker = walker_tick_steps;
                    budget_first_total = total_tick_steps;
                    /* An upper bound on the number of FIFO
                     * commands that could complete inside this
                     * IRQ, given an unrealistically optimistic
                     * 4 CPU cycles for every instruction.
                     * Preserve original FIFO order and reserve
                     * one full L2 + stream walk. This is an
                     * exploratory bound, NOT a scheduler. */
                    const uint32_t optimistic_instr_capacity =
                        M68K_CYCLES_PER_TIMER_A / 4u;
                    uint32_t remaining_instr =
                        optimistic_instr_capacity >
                            original_steps + walker_tick_steps ?
                        optimistic_instr_capacity -
                            original_steps - walker_tick_steps : 0u;
                    size_t entry;
                    for (entry = 0u; entry < first_fifo_count;
                         ++entry) {
                        if (first_fifo_steps[entry] >
                            remaining_instr) {
                            break;
                        }
                        remaining_instr -= first_fifo_steps[entry];
                        ++first_fifo_ceiling;
                    }
                }
                budget_instruction_sum += total_tick_steps;
                ++budget_checked_ticks;
                if (total_tick_steps > budget_peak_total) {
                    budget_peak_total = total_tick_steps;
                    budget_peak_tick = tick;
                }
                if ((uint64_t)total_tick_steps * UINT64_C(4) >
                    (uint64_t)M68K_CYCLES_PER_TIMER_A) {
                    ++budget_overloaded_ticks;
                    if (budget_overloaded_ticks <= 4u) {
                        printf("AKIRA_TIMA_CPU_OVERRUN tick=%u "
                               "L2_steps=%u drain_steps=%u "
                               "walker_steps=%u minimum_cycles=%u "
                               "budget_cycles=%u\n",
                               (unsigned)tick, (unsigned)original_steps,
                               (unsigned)drain_tick_steps,
                               (unsigned)walker_tick_steps,
                               (unsigned)(total_tick_steps * 4u),
                               (unsigned)M68K_CYCLES_PER_TIMER_A);
                    }
                }
            }
            if (akira_verify_composite_ticks) {
                int16_t left[LEGACY_FRAMES_PER_TICK];
                int16_t right[LEGACY_FRAMES_PER_TICK];
                vf2_status rendered;
                ++composite_verified_ticks;
                rendered = vf2_scsp_render(
                    &step_scsp, left, right, frames_per_tick);
                if (rendered != VF2_OK) {
                    printf("AKIRA_COMPOSITE_10S_RENDER_FAIL tick=%u status=%d\n",
                           (unsigned)tick, (int)rendered);
                    full_drain = 0;
                    break;
                }
            }
            if (akira_walk_probe && (tick == 0u || tick == 8u ||
                                     tick == 15u)) {
                akira_probe_original_walk(rom, rom_size, (unsigned)tick);
            }
            /* Original-ROM mixer register snapshot: record the *actual*
             * direct send level and pan of active voices. Our current
             * software renderer interprets DIPAN as linear 0..31,
             * whereas the SEGA SCSP manual specifies 00/10=center,
             * low nibble=3 dB increments and bit4 selects side.
             * This diagnostic is owner-ROM-only, no chip data exported. */
            if (sequenced_dir != NULL &&
                (tick == 0u || tick == 882u || tick == 4410u)) {
                size_t mix_slot;
                unsigned active_voices = 0u;
                unsigned with_direct = 0u;
                unsigned pan_center = 0u;
                unsigned pan_left = 0u;
                unsigned pan_right = 0u;
                for (mix_slot = 0u; mix_slot < VF2_SCSP_SLOT_COUNT; ++mix_slot) {
                    if (step_scsp.slot_active[mix_slot] != 0u) {
                        const size_t at = mix_slot * VF2_SCSP_SLOT_BYTES + 0x16u;
                        const uint16_t mixer = (uint16_t)(
                            ((uint16_t)step_scsp.registers[at] << 8u) |
                            step_scsp.registers[at + 1u]);
                        const unsigned disdl = ((unsigned)mixer >> 13u) & 7u;
                        const unsigned dipan = ((unsigned)mixer >> 8u) & 31u;
                        ++active_voices;
                        with_direct += (unsigned)(disdl != 0u);
                        pan_center += (unsigned)((dipan & 15u) == 0u);
                        pan_left += (unsigned)((dipan & 16u) != 0u &&
                                                 (dipan & 15u) != 0u);
                        pan_right += (unsigned)((dipan & 16u) == 0u &&
                                                  (dipan & 15u) != 0u);
                        {
                            const size_t base = mix_slot * VF2_SCSP_SLOT_BYTES;
                            const uint16_t voice_control = (uint16_t)(
                                ((uint16_t)step_scsp.registers[base] << 8u) |
                                step_scsp.registers[base + 1u]);
                            const uint16_t start_low = (uint16_t)(
                                ((uint16_t)step_scsp.registers[base + 2u] << 8u) |
                                step_scsp.registers[base + 3u]);
                            const uint16_t loop_start = (uint16_t)(
                                ((uint16_t)step_scsp.registers[base + 4u] << 8u) |
                                step_scsp.registers[base + 5u]);
                            const uint16_t loop_end = (uint16_t)(
                                ((uint16_t)step_scsp.registers[base + 6u] << 8u) |
                                step_scsp.registers[base + 7u]);
                            const uint16_t pitch = (uint16_t)(
                                ((uint16_t)step_scsp.registers[base + 0x10u] << 8u) |
                                step_scsp.registers[base + 0x11u]);
                            const uint32_t start = ((uint32_t)(
                                voice_control & 0x000fu) << 16u) | start_low;
                            {
                                const uint32_t rec = UINT32_C(0x2000) +
                                    (uint32_t)mix_slot * UINT32_C(0x20);
                                const uint32_t source = read_be32(
                                    step_ram + rec + 2u);
                                const uint32_t dest = read_be32(
                                    step_ram + rec + 6u);
                                printf("AKIRA_ORIGINAL_PAGE_RECORD tick=%u "
                                       "slot=%u flags=%02x src=%08x "
                                       "dest=%08x sample_range=%u\n",
                                       (unsigned)tick, (unsigned)mix_slot,
                                       (unsigned)step_ram[rec],
                                       (unsigned)source, (unsigned)dest,
                                       (unsigned)(source >= UINT32_C(0x800000) &&
                                        source < UINT32_C(0x1000000)));
                            }
                            printf("AKIRA_ORIGINAL_SCSP_VOICE tick=%u slot=%u "
                                   "ctl=%04x start=%05x loop_start=%u "
                                   "loop_end=%u pitch=%04x step=%u "
                                   "address=%u envelope=%d env_state=%u "
                                   "mix=%04x disdl=%u dipan=%02x\n",
                                   (unsigned)tick, (unsigned)mix_slot,
                                   (unsigned)voice_control, (unsigned)start,
                                   (unsigned)loop_start, (unsigned)loop_end,
                                   (unsigned)pitch,
                                   (unsigned)step_scsp.slot_step[mix_slot],
                                   (unsigned)step_scsp.slot_address[mix_slot],
                                   (int)step_scsp.slot_envelope[mix_slot],
                                   (unsigned)step_scsp.slot_envelope_state[mix_slot],
                                   (unsigned)mixer, disdl, dipan);
                            /* PCM address statistics, not private bytes.
                             * Determines whether the apparent loop is a
                             * real staged wave or uninitialized RAM. */
                            if (tick == 0u || tick == 882u) {
                                const uint32_t until =
                                    loop_end == 0u ? 1024u : (uint32_t)loop_end;
                                const uint32_t limit = until > 8192u ? 8192u : until;
                                uint32_t sample_index;
                                size_t sample_nonzero = 0u;
                                size_t sample_changes = 0u;
                                uint32_t abs_sum = 0u;
                                uint8_t previous = 0u;
                                for (sample_index = 0u;
                                     sample_index < limit; ++sample_index) {
                                    const uint32_t addr = start + sample_index;
                                    uint8_t value = 0u;
                                    if (addr < VF2_SOUND_RAM_BYTES) {
                                        value = step_ram[addr];
                                    }
                                    if (value != 0u) ++sample_nonzero;
                                    {
                                        const int32_t signed_sample =
                                            (int32_t)(int8_t)value;
                                        abs_sum += signed_sample < 0 ?
                                            (uint32_t)(-signed_sample) :
                                            (uint32_t)signed_sample;
                                    }
                                    if (sample_index != 0u &&
                                        value != previous) ++sample_changes;
                                    previous = value;
                                }
                                printf("AKIRA_ORIGINAL_WAVE_STATS tick=%u "
                                       "slot=%u start=%05x size=%u "
                                       "bytes_nonzero=%u transitions=%u "
                                       "mean_abs=%u source=SCSP_RAM\n",
                                       (unsigned)tick, (unsigned)mix_slot,
                                       (unsigned)start, (unsigned)limit,
                                       (unsigned)sample_nonzero,
                                       (unsigned)sample_changes,
                                       limit != 0u ?
                                           (unsigned)(abs_sum / limit) : 0u);
                            }
                        }
                    }
                }
                printf("AKIRA_ORIGINAL_SCSP_MIX_SUMMARY tick=%u active=%u "
                       "direct=%u pan_center=%u pan_left=%u pan_right=%u\n",
                       (unsigned)tick, active_voices, with_direct,
                       pan_center, pan_left, pan_right);
            }
            if (sequenced_dir != NULL) {
                const size_t pos = tick * frames_per_tick;
                if (akira_wave_prefill_experiment) {
                    preview_pages += akira_prefill_wave_pages(
                        step_ram, sample_bank, sample_bank_size,
                        last_source, last_destination);
                }
                vf2_status render = vf2_scsp_render(
                    &step_scsp, music_left + pos, music_right + pos,
                    frames_per_tick);
                if (render != VF2_OK) {
                    printf("AKIRA_MUSIC_SEQUENCE_FAIL tick=%u render=%d\n",
                           (unsigned)tick, (int)render);
                    break;
                }
                ++rendered_ticks;
            }
        }
    }
    if (akira_tima_49_preview &&
        akira_verify_composite_ticks) {
        /* The first 900Hz tick must be reported accurately:
         * this synthetic full-drain schedule has a demonstrable
         * CPU-cycle lower-bound overrun. Do not claim that all
         * commands physically executed before one IRQ. */
        printf("AKIRA_TIMA_CPU_BUDGET checked=%u overloaded=%u "
               "first_l2=%u first_drain=%u first_walker=%u "
               "first_total=%u first_minimum_cycles=%u "
               "budget_cycles=12544 peak_tick=%u peak_steps=%u "
               "first_fifo_commands=%u "
               "first_fifo_optimistic_completed_ceiling=%u "
               "first_fifo_backlog_floor=%u "
               "average_steps=%u lower_bound_only=1 "
               "one_scan_per_irq_proven=0\n",
               (unsigned)budget_checked_ticks,
               (unsigned)budget_overloaded_ticks,
               (unsigned)budget_first_l2,
               (unsigned)budget_first_dispatch,
               (unsigned)budget_first_walker,
               (unsigned)budget_first_total,
               (unsigned)(budget_first_total * 4u),
               (unsigned)budget_peak_tick,
               (unsigned)budget_peak_total,
               (unsigned)first_fifo_count,
               (unsigned)first_fifo_ceiling,
               (unsigned)(first_fifo_count - first_fifo_ceiling),
               (unsigned)(budget_checked_ticks == 0u ? 0u :
                   budget_instruction_sum / budget_checked_ticks));
        if (budget_checked_ticks != sequence_ticks ||
            budget_overloaded_ticks == 0u ||
            budget_first_total <= 3136u ||
            first_fifo_count != 32u ||
            first_fifo_ceiling != 1u) {
            fprintf(stderr,
                    "AKIRA_TIMA_CPU_BUDGET expected overrun not measured\n");
            return 0;
        }
    }
    if (akira_verify_composite_ticks) {
        printf("AKIRA_COMPOSITE_10S_STRICT parity=%u ticks=%u "
               "passes=%u original_pages=%u original_steps=%u "
               "modeled_cadence=%uHz realtime_verified=0\n",
               (unsigned)(full_drain &&
                          composite_verified_ticks == sequence_ticks),
               (unsigned)composite_verified_ticks,
               (unsigned)(composite_verified_ticks *
                          VF2_SCSP_SLOT_COUNT),
               (unsigned)real_walker_pages,
               (unsigned)real_walker_steps,
               (unsigned)(44100u / frames_per_tick));
        if (!full_drain ||
            composite_verified_ticks != sequence_ticks) return 0;
    }
    if (progress) {
        printf("AKIRA_MUSIC_PROGRESS ticks=%u total_drained=%u "
               "active_ticks=%u last_packet_tick=%u peak_voices=%u "
               "final_queue=%u original_only=1 real_time_pacing=0\n",
               (unsigned)(sequenced_dir != NULL ||
                          akira_verify_composite_ticks ?
                          sequence_ticks : 1024u),
               (unsigned)total_drain, (unsigned)active_ticks,
               (unsigned)last_tick_with_packets, peak_voices,
               (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT]);
    }
    if (sequenced_dir != NULL) {
        const size_t total_frames = SAMPLE_FRAMES;
        if (akira_native_walk_preview) {
            printf("AKIRA_MUSIC_NATIVE_WALK_PREVIEW "
                   "pages=%u C_native=1 original_L2_and_drain=1 "
                   "approximate_scheduler=1\n",
                   (unsigned)real_walker_pages);
        }
        if (akira_original_walk_preview) {
            printf("AKIRA_MUSIC_ORIGINAL_WALK_PREVIEW "
                   "pages=%u steps=%u original_copy=1 "
                   "approximate_scheduler=1\n",
                   (unsigned)real_walker_pages,
                   (unsigned)real_walker_steps);
        }
        if (akira_wave_prefill_experiment) {
            printf("AKIRA_MUSIC_PREFILL_EXPERIMENT pages=%u "
                   "original_refill_executed=0 unverified_timing=1\n",
                   (unsigned)preview_pages);
        }
        printf("AKIRA_MUSIC_SEQUENCE_FINISH rendered_ticks=%u "
               "total_drained=%u last_packet_tick=%u voices_peak=%u "
               "timer_hz_modeled=%u source=SCSP_TimerA_CE "
               "original_driver=1 realtime_verified=0\n",
               (unsigned)rendered_ticks, (unsigned)total_drain,
               (unsigned)last_tick_with_packets, peak_voices,
               (unsigned)(44100u / frames_per_tick));
        if (rendered_ticks != sequence_ticks || !full_drain ||
            first_tick_count != 32u || total_drain <= 43u) return 0;
        return write_akira_stereo_pcm(
            sequenced_dir,
            akira_tima_49_preview ?
            (akira_native_walk_preview ?
             "vf2_akira_10s_NATIVE_C_WALK_TIMA49_scsp_model.wav" :
             "vf2_akira_10s_ORIGINAL_WALK_TIMA49_scsp_model.wav") :
            (akira_native_walk_preview ?
             "vf2_akira_10s_NATIVE_C_WALK_APPROX_TIMER_scsp_model.wav" :
             (akira_original_walk_preview ?
              "vf2_akira_10s_ORIGINAL_WALK_APPROX_TIMER_scsp_model.wav" :
              (akira_wave_prefill_experiment ?
               "vf2_akira_10s_PREFILL_UNVERIFIED_scsp_model.wav" :
               "vf2_akira_ride_the_tiger_10s_sequenced_scsp_model.wav"))),
            music_left, music_right, total_frames, 1);
    }
    if (strict) {
        static int16_t left[4096u], right[4096u];
        size_t frame, audible = 0u;
        unsigned keyed = 0u;
        vf2_status render_status;
        for (i = 0u; i < VF2_SCSP_SLOT_COUNT; ++i) {
            keyed += (unsigned)(step_scsp.slot_active[i] != 0u);
        }
        render_status = vf2_scsp_render(&step_scsp, left, right, 4096u);
        if (render_status == VF2_OK) {
            for (frame = 0u; frame < 4096u; ++frame) {
                audible += (size_t)(left[frame] != 0 || right[frame] != 0);
            }
        }
        printf("AKIRA_MUSIC_STRICT first_tick_queue=%u first_drain=%u "
               "queue_after=%u active_voices=%u audible_frames=%u "
               "original_only=1 C_music_parity=0\n",
               (unsigned)first_tick_count, (unsigned)first_drain,
               (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
               keyed, (unsigned)audible);
        if (first_tick_count != 32u || first_drain != 32u ||
            !full_drain || step_ram[VF2_SOUND_COMMAND_COUNT] != 0u ||
            keyed != 8u || render_status != VF2_OK || audible == 0u) {
            fprintf(stderr, "AKIRA_MUSIC_STRICT owner-ROM gate failed\n");
            return 0;
        }
    }
    if (wav_dir != NULL) {
        unsigned active_count = 0u;
        size_t v;
        for (v = 0u; v < VF2_SCSP_SLOT_COUNT; ++v) {
            active_count += (unsigned)(step_scsp.slot_active[v] != 0u);
        }
        if (active_count < 8u ||
            !write_akira_model_preview(wav_dir, &step_scsp)) {
            fprintf(stderr, "AKIRA_MUSIC_WAV refused: no verified voice state\n");
            return 0;
        }
    }
    if (verify_akira_original_walk && akira_walk_strict_failed) {
        fprintf(stderr,
                "AKIRA_ORIGINAL_WALK_STRICT owner-ROM gate failed\n");
        return 0;
    }
    return 1;
}

static int check_uart_stream(
    const uint8_t *bytes,
    size_t length,
    const uint8_t *rom,
    size_t rom_size
)
{
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t i;
    uint32_t steps = 0u;
    if (bytes == NULL || length == 0u ||
        vf2_sound_board_initialize(&board, rom, rom_size,
                                    sample_bank, sample_bank_size) !=
            VF2_OK) {
        fprintf(stderr, "L3 UART stream initialization failed\n");
        return 0;
    }
    memcpy(step_ram, board.sound_ram, sizeof(step_ram));
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&bus, 0, sizeof(bus));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = rom;
    bus.rom_size = rom_size;
    bus.sample_rom = sample_bank;
    bus.sample_rom_size = sample_bank_size;
    bus.scsp = &step_scsp;

    for (i = 0u; i < length; ++i) {
        vf2_status c_status, s_status;
        uint8_t before = board.sound_ram[VF2_SOUND_COMMAND_COUNT];
        uint8_t after;
        uint8_t newly_filed;
        size_t event_index;
        c_status = vf2_sound_board_file_uart_byte(&board, bytes[i]);
        vf2_m68k_regs_reset(&regs);
        regs.d[0] = bytes[i];
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x52e);
        bus.event_count = 0u;
        s_status = vf2_m68k_run(&bus, &regs, UINT32_C(0x5ea),
                                0, L3_STEP_LIMIT, &steps);
        after = board.sound_ram[VF2_SOUND_COMMAND_COUNT];
        newly_filed = (uint8_t)(after - before);
        if (c_status != VF2_OK || s_status != VF2_OK ||
            regs.pc != UINT32_C(0x5ea) ||
            !compare_ram(board.sound_ram, step_ram, i) ||
            memcmp(&board.scsp, &step_scsp, sizeof(step_scsp)) != 0 ||
            bus.event_count != (size_t)newly_filed) {
            fprintf(stderr,
                    "L3 UART stream byte %u/%u = %02x differs:"
                    " C=%d S=%d pc=%05x count=%u->%u events=%u\n",
                    (unsigned)i, (unsigned)length, (unsigned)bytes[i],
                    (int)c_status, (int)s_status, (unsigned)regs.pc,
                    (unsigned)before, (unsigned)after,
                    (unsigned)bus.event_count);
            return 0;
        }
        for (event_index = 0u; event_index < bus.event_count; ++event_index) {
            if (bus.events[event_index].address != UINT32_C(0x400001) ||
                bus.events[event_index].is_write == 0u) {
                fprintf(stderr, "L3 UART stream unexpected hardware poke"
                        " at byte %u\n", (unsigned)i);
                return 0;
            }
        }
    }
    fprintf(stderr,
            "MATCH C/68000 original UART stream bytes=%u ring_count=%u"
            " write_cursor=%02x%02x\n",
            (unsigned)length,
            (unsigned)board.sound_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR],
            (unsigned)board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]);
    return 1;
}

int main(int argc, char **argv)
{
    static const struct {
        uint32_t profile, column, expected;
    } legs[] = {
        { 1u, 5u, UINT32_C(0x00ad2342) },
        { 9u, 0u, UINT32_C(0x00ad2401) }
    };
    uint8_t *audio = NULL;
    uint8_t *data = NULL;
    uint8_t *samples = NULL;
    size_t audio_size = 0u, data_size = 0u, samples_size = 0u;
    size_t i = 0u;
    int valid = 1;
    int l3_only = 0;
    int l3_matrix = 0;
    int l3_stream = 0;
    int akira_music_probe = 0;
    int akira_music_strict = 0;
    int akira_music_progress = 0;
    const char *akira_wav_dir = NULL;
    const char *akira_sequenced_dir = NULL;
    uint8_t stream_bytes[64] = {0u};
    size_t stream_length = 0u;
    uint32_t l3_packet = 0u;

    /*
     * Opt-in 24-bit UART packet replay. Caller-supplied bytes can come
     * from a verified local i960 trace, but this test only proves the
     * isolated 68000 L3 receiver agrees with the recovered C receiver.
     * It does NOT prove a natural event, timing, dispatcher or KEYON.
     * Keep the two original A0/L2 ROM-table tests unchanged by default.
     */
    if (argc != 2 && !(argc == 4 &&
                       (strcmp(argv[2], "--l3-packet") == 0 ||
                        strcmp(argv[2], "--uart-stream") == 0 ||
                        strcmp(argv[2], "--akira-wav-dir") == 0 ||
                        strcmp(argv[2], "--akira-sequenced-wav-dir") == 0 ||
                        strcmp(argv[2], "--akira-prefill-wav-dir") == 0 ||
                        strcmp(argv[2], "--akira-original-walk-wav-dir") == 0 ||
                        strcmp(argv[2], "--akira-native-walk-wav-dir") == 0 ||
                        strcmp(argv[2], "--akira-tima-native-wav-dir") == 0 ||
                        strcmp(argv[2], "--akira-tima-original-wav-dir") == 0)) &&
        !(argc == 3 &&
          (strcmp(argv[2], "--matrix-l3") == 0 ||
           strcmp(argv[2], "--probe-akira-music") == 0 ||
           strcmp(argv[2], "--verify-akira-music") == 0 ||
           strcmp(argv[2], "--probe-akira-progress") == 0 ||
           strcmp(argv[2], "--probe-akira-walk") == 0 ||
           strcmp(argv[2], "--verify-akira-walk") == 0 ||
           strcmp(argv[2], "--verify-akira-composite-10s") == 0 ||
           strcmp(argv[2], "--verify-akira-tima-10s") == 0))) {
        fprintf(stderr,
                "usage: %s <vf2-rom-directory> [--l3-packet <six-hex-digits> "
                "| --uart-stream <even-hex-digits> | --matrix-l3 "
                "| --probe-akira-music | --verify-akira-music "
                "| --probe-akira-progress | --probe-akira-walk "
                "| --verify-akira-walk | --verify-akira-composite-10s "
                "| --verify-akira-tima-10s "
                "| --akira-wav-dir DIRECTORY | --akira-sequenced-wav-dir DIRECTORY "
                "| --akira-prefill-wav-dir DIRECTORY "
                "| --akira-original-walk-wav-dir DIRECTORY "
                "| --akira-native-walk-wav-dir DIRECTORY "
                "| --akira-tima-native-wav-dir DIRECTORY "
                "| --akira-tima-original-wav-dir DIRECTORY]\n",
                argv[0]);
        return 2;
    }
    l3_matrix = argc == 3 && strcmp(argv[2], "--matrix-l3") == 0;
    akira_music_probe = argc == 3 &&
                        (strcmp(argv[2], "--probe-akira-music") == 0 ||
                         strcmp(argv[2], "--verify-akira-music") == 0 ||
                         strcmp(argv[2], "--probe-akira-progress") == 0 ||
                         (strcmp(argv[2], "--probe-akira-walk") == 0 ||
                        strcmp(argv[2], "--verify-akira-walk") == 0 ||
                         strcmp(argv[2], "--verify-akira-composite-10s") == 0 ||
                         strcmp(argv[2], "--verify-akira-tima-10s") == 0));
    akira_music_progress = argc == 3 &&
                           strcmp(argv[2], "--probe-akira-progress") == 0;
    akira_walk_probe = argc == 3 &&
                       (strcmp(argv[2], "--probe-akira-walk") == 0 ||
                        strcmp(argv[2], "--verify-akira-walk") == 0);
    verify_akira_original_walk = argc == 3 &&
        strcmp(argv[2], "--verify-akira-walk") == 0;
    akira_music_strict = argc == 3 &&
                         strcmp(argv[2], "--verify-akira-music") == 0;
    akira_verify_composite_ticks = argc == 3 &&
        (strcmp(argv[2], "--verify-akira-composite-10s") == 0 ||
         strcmp(argv[2], "--verify-akira-tima-10s") == 0);
    if (akira_verify_composite_ticks) {
        akira_original_walk_preview = 1;
        akira_music_progress = 1;
        if (strcmp(argv[2], "--verify-akira-tima-10s") == 0) {
            akira_tima_49_preview = 1;
        }
    }
    l3_stream = argc == 4 &&
        strcmp(argv[2], "--uart-stream") == 0;
    if (argc == 4 && strcmp(argv[2], "--akira-wav-dir") == 0) {
        akira_music_probe = 1;
        akira_wav_dir = argv[3];
    }
    if (argc == 4 && strcmp(argv[2], "--akira-sequenced-wav-dir") == 0) {
        akira_music_probe = 1;
        akira_music_progress = 1;
        akira_sequenced_dir = argv[3];
    }
    if (argc == 4 && strcmp(argv[2], "--akira-prefill-wav-dir") == 0) {
        akira_music_probe = 1;
        akira_music_progress = 1;
        akira_wave_prefill_experiment = 1;
        akira_sequenced_dir = argv[3];
    }
    if (argc == 4 &&
        strcmp(argv[2], "--akira-original-walk-wav-dir") == 0) {
        akira_music_probe = 1;
        akira_music_progress = 1;
        akira_original_walk_preview = 1;
        akira_sequenced_dir = argv[3];
    }
    if (argc == 4 &&
        strcmp(argv[2], "--akira-native-walk-wav-dir") == 0) {
        akira_music_probe = 1;
        akira_music_progress = 1;
        akira_native_walk_preview = 1;
        akira_sequenced_dir = argv[3];
    }
    if (argc == 4 &&
        (strcmp(argv[2], "--akira-tima-native-wav-dir") == 0 ||
         strcmp(argv[2], "--akira-tima-original-wav-dir") == 0)) {
        akira_music_probe = 1;
        akira_music_progress = 1;
        akira_tima_49_preview = 1;
        akira_native_walk_preview =
            strcmp(argv[2], "--akira-tima-native-wav-dir") == 0;
        akira_original_walk_preview = !akira_native_walk_preview;
        akira_sequenced_dir = argv[3];
    }
    if (argc == 4 && akira_wav_dir == NULL &&
        akira_sequenced_dir == NULL) {
        const char *hex = argv[3];
        size_t j, length = hex != NULL ? strlen(hex) : 0u;
        l3_only = !l3_stream;
        if (length == 0u || (length & 1u) != 0u ||
            length > sizeof(stream_bytes) * 2u ||
            (!l3_stream && length != 6u)) {
            fprintf(stderr, "UART input requires an even 2..128 hex digits "
                            "(--l3-packet exactly six)\n");
            return 2;
        }
        for (j = 0u; j < length; ++j) {
            unsigned digit;
            if (hex[j] >= '0' && hex[j] <= '9') {
                digit = (unsigned)(hex[j] - '0');
            } else if (hex[j] >= 'a' && hex[j] <= 'f') {
                digit = 10u + (unsigned)(hex[j] - 'a');
            } else if (hex[j] >= 'A' && hex[j] <= 'F') {
                digit = 10u + (unsigned)(hex[j] - 'A');
            } else {
                fprintf(stderr, "UART input has a non-hex digit\n");
                return 2;
            }
            if (l3_stream) {
                if ((j & 1u) == 0u) {
                    stream_bytes[j / 2u] = (uint8_t)(digit << 4u);
                } else {
                    stream_bytes[j / 2u] |= (uint8_t)digit;
                }
            } else {
                l3_packet = (l3_packet << 4u) | (uint32_t)digit;
            }
        }
        stream_length = length / 2u;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU,
                                &audio, &audio_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                &data, &data_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_SAMPLES,
                                &samples, &samples_size) != VF2_OK ||
        audio == NULL || data == NULL || samples == NULL) {
        fprintf(stderr, "cannot load verified sound/main-data regions\n");
        free(audio);
        free(data);
        free(samples);
        return 2;
    }
    sample_bank = samples;
    sample_bank_size = samples_size;
    if (akira_music_probe) {
        valid = probe_akira_music(audio, audio_size, akira_wav_dir, akira_music_strict,
                                  akira_music_progress, akira_sequenced_dir);
        free(audio);
        free(data);
        free(samples);
        return valid ? 0 : 1;
    }
    if (l3_matrix) {
        /*
         * The 18x11 original ROM table contains 107 occupied cells and
         * 98 distinct AD23/AD24 commands in the supported VF2.1 set.
         * A ROM-table cell is not evidence of a naturally triggered SFX.
         * Replay all distinct commands through isolated L3 C/68000 only.
         */
        uint32_t seen[18u * 11u] = {0u};
        size_t unique_count = 0u;
        size_t occupied_count = 0u;
        size_t passed_count = 0u;
        uint32_t profile;
        for (profile = 1u; profile <= 18u; ++profile) {
            uint32_t column;
            for (column = 0u; column < 11u; ++column) {
                uint32_t command = 0u;
                size_t entry;
                if (!table_command(data, data_size, profile,
                                   column, &command)) {
                    /* Valid empty cell is harmless. A nonempty foreign
                     * command family must never be skipped silently. */
                    if (command != 0u) {
                        fprintf(stderr,
                                "matrix: unsupported ROM command %08x at %u,%u\n",
                                (unsigned)command,
                                (unsigned)profile, (unsigned)column);
                        valid = 0;
                    }
                    continue;
                }
                ++occupied_count;
                for (entry = 0u; entry < unique_count; ++entry) {
                    if (seen[entry] == command) {
                        break;
                    }
                }
                if (entry != unique_count) {
                    continue;
                }
                seen[unique_count++] = command;
                if (check_one(command, audio, audio_size, 1)) {
                    ++passed_count;
                } else {
                    valid = 0;
                    fprintf(stderr, "matrix: L3 mismatch command %08x from %u,%u\n",
                            (unsigned)command,
                            (unsigned)profile, (unsigned)column);
                }
            }
        }
        printf("ROM sound-matrix L3: %u occupied, %u unique, %u passed\n",
               (unsigned)occupied_count, (unsigned)unique_count,
               (unsigned)passed_count);
        if (occupied_count != 107u || unique_count != 98u ||
            passed_count != unique_count) {
            fprintf(stderr,
                    "matrix: expected 107 occupied/98 unique with all L3 matches\n");
            valid = 0;
        }
    } else if (l3_only) {
        valid = check_one(l3_packet, audio, audio_size, 1);
    } else if (l3_stream) {
        valid = check_uart_stream(
            stream_bytes, stream_length, audio, audio_size);
    } else {
        for (i = 0u; i < sizeof(legs) / sizeof(legs[0]); ++i) {
            uint32_t command = 0u;
            if (!table_command(data, data_size, legs[i].profile,
                               legs[i].column, &command) ||
                command != legs[i].expected ||
                !check_one(command, audio, audio_size, 0)) {
                fprintf(stderr, "FAIL: profile %u column %u, got %08x\n",
                        (unsigned)legs[i].profile,
                        (unsigned)legs[i].column, (unsigned)command);
                valid = 0;
            }
        }
    }
    free(audio);
    free(data);
    free(samples);
    return valid ? 0 : 1;
}
