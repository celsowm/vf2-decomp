#include "vf2/sound_board.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

/* ROM-measured boot image (v0794a): the structural tables consumed by
   boot inits 0x28e/0x320/0x336/0x3a8/0x3cc/0x402/0x422. All payload
   regions are zero; the nonzero bytes are init-structural (voice slot
   words, stream records, table pointers, the d1 < 0 upload skip). */
static void test_boot_rom(uint8_t *rom)
{
    size_t voice = 0u;

    memset(rom, 0, 0x10010u);
    /* Historical fixture bytes (vector area, init doesn't read them). */
    rom[0x0000u] = 0x12u; rom[0x0001u] = 0x34u;
    /* 0x28e wave-blob source -> 0x607000 (0x5a probe + zero tail). */
    rom[0x4940u] = 0x00u; rom[0x4941u] = 0x60u;
    rom[0x4942u] = 0x70u; rom[0x4943u] = 0x00u;
    rom[0x7000u] = 0x5au;
    /* 0x336 descriptor images (zero) at 0x607100/0x607200. */
    rom[0x3066u] = 0x00u; rom[0x3067u] = 0x60u;
    rom[0x3068u] = 0x71u; rom[0x3069u] = 0x00u;
    rom[0x30a6u] = 0x00u; rom[0x30a7u] = 0x60u;
    rom[0x30a8u] = 0x72u; rom[0x30a9u] = 0x00u;
    /* 0x3a8 voice slot words (index*0x20, big-endian). */
    for (voice = 0u; voice < 32u; ++voice) {
        rom[0x6e6u + voice * 2u] = (uint8_t)(((uint16_t)voice * 0x20u) >> 8u);
        rom[0x6e6u + voice * 2u + 1u] =
            (uint8_t)(((uint16_t)voice * 0x20u) & 0xffu);
    }
    /* 0x402 volume table pointer -> 0x607300 (16 zero levels). */
    rom[0x536au] = 0x00u; rom[0x536bu] = 0x60u;
    rom[0x536cu] = 0x73u; rom[0x536du] = 0x00u;
    rom[0x7300u] = 0x5au;
    /* Uploader fixture (v0822 true semantics): first_count 0 (stages
       1 zero byte), max 0, row 0 -> 0x608e00 (count 0 -> 1 record,
       selector 0 -> entry 0), entry 0 = {source 0x800000 (the
       2-byte sample_rom), length for a 2-byte copy, stored 0}.
       Init then completes end-to-end (OK). Legs override deltas. */
    rom[0x5b26u] = 0x00u; rom[0x5b27u] = 0x00u;
    rom[0x5b28u] = 0x00u; rom[0x5b29u] = 0x80u;
    rom[0x5b2au] = 0x00u; rom[0x5b2bu] = 0x00u;
    rom[0x5b2cu] = 0x00u; rom[0x5b2du] = 0x00u;
    rom[0x5b2eu] = 0xfeu; rom[0x5b2fu] = 0x02u;
    rom[0x5b30u] = 0x00u; rom[0x5b31u] = 0x00u;
    rom[0x5b32u] = 0x00u; rom[0x5b33u] = 0x00u;
    rom[0x8df0u] = 0x00u; rom[0x8df1u] = 0x00u;
    rom[0x8df2u] = 0x00u; rom[0x8df3u] = 0x60u;
    rom[0x8df4u] = 0x8eu; rom[0x8df5u] = 0x00u;
    rom[0x8e00u] = 0x00u; rom[0x8e01u] = 0x00u;
    rom[0x8e02u] = 0x00u; rom[0x8e03u] = 0x00u;
}

/* V3c first-tone probe (synthetic, ROM-free): one 8-bit square-wave
 * voice (64-sample forward loop, 1 sample/frame ~= 689 Hz at
 * 44.1 kHz), attack 31, level bypass, near-center pan. Renders 512
 * frames through vf2_sound_board_render and pins audibility
 * (peak/RMS/attack-ramp/stereo/active-slot) in-test; a WAV copy
 * goes to out/first-tone.wav when writable (gitignored bonus
 * artifact for human ears -- NOT the gate, the asserts are). */
static void test_render_first_tone(void)
{
    static vf2_sound_board board;
    static uint8_t sample_rom[256u];
    static int16_t left[512u];
    static int16_t right[512u];
    size_t i = 0u;
    int32_t peak_left = 0;
    int32_t peak_right = 0;
    int64_t sum_sq = 0;
    int32_t late_peak = 0;

    for (i = 0u; i < 64u; ++i) {
        sample_rom[i] = (uint8_t)(i < 32u ? 100 : -100);
    }
    memset(&board, 0, sizeof(board));
    vf2_scsp_initialize(&board.scsp);
    EXPECT_TRUE(vf2_scsp_attach_sample_rom(&board.scsp, sample_rom,
                                          sizeof(sample_rom)) == VF2_OK);
    /* Slot 0: KEYONB|PCM8B|forward-loop, start 0, loop 0..64,
       attack 31, level bypass, pitch 1 sample/frame,
       direct output=0dB (DISDL7), centered (DIPAN00). */
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0000u, 0x0830u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0002u, 0x0000u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0004u, 0x0000u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0006u, 64u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0008u, 0x001fu) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x000au, 0x0000u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x000cu, 0x0100u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0010u, 0x0000u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0016u, 0xe000u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right, 512u) ==
                VF2_OK);
    for (i = 0u; i < 512u; ++i) {
        int32_t l = left[i];
        int32_t r = right[i];
        int32_t al = l < 0 ? -l : l;
        int32_t ar = r < 0 ? -r : r;
        if (al > peak_left) {
            peak_left = al;
        }
        if (ar > peak_right) {
            peak_right = ar;
        }
        sum_sq += (int64_t)l * l + (int64_t)r * r;
        if (i >= 16u && (al > late_peak || ar > late_peak)) {
            late_peak = al > ar ? al : ar;
        }
    }
    fprintf(stderr, "first-tone: peakL=%d peakR=%d late=%d meansq=%lld\n",
            (int)peak_left, (int)peak_right, (int)late_peak,
            (long long)(sum_sq / 1024));
    /* Attack ramp (order-robust: envelope starts at 0 and grows,
       so frame 0 is quieter than frame 4 regardless of whether a
       given frame advances the envelope before or after scaling;
       the first attempt pinned frame-0 ~= 0, which bakes in one
       ordering -- the ramp is the silicon-agnostic fact). */
    {
        int32_t l0 = left[0] < 0 ? -left[0] : left[0];
        int32_t l4 = left[4] < 0 ? -left[4] : left[4];
        int32_t r0 = right[0] < 0 ? -right[0] : right[0];
        int32_t r4 = right[4] < 0 ? -right[4] : right[4];
        EXPECT_TRUE(l0 < l4);
        EXPECT_TRUE(r0 < r4);
    }
    /* ...then the square wave arrives on both channels. */
    EXPECT_TRUE(late_peak > 8000);
    EXPECT_TRUE(peak_left > 8000);
    EXPECT_TRUE(peak_right > 8000);
    EXPECT_TRUE(sum_sq / 1024 > 9000000);
    EXPECT_TRUE(board.scsp.slot_active[0u] == 1u);
    {
        /* 44-byte PCM16 stereo WAV, best-effort (out/ is gitignored;
           a missing dir only skips the artifact, never the gate). */
        static const uint8_t hdr[44] = {
            0x52u, 0x49u, 0x46u, 0x46u, 0x24u, 0x10u, 0x00u, 0x00u,
            0x57u, 0x41u, 0x56u, 0x45u, 0x66u, 0x6du, 0x74u, 0x20u,
            0x10u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x02u, 0x00u,
            0x44u, 0xacu, 0x00u, 0x00u, 0x10u, 0xb1u, 0x02u, 0x00u,
            0x04u, 0x00u, 0x10u, 0x00u, 0x64u, 0x61u, 0x74u, 0x61u,
            0x00u, 0x10u, 0x00u, 0x00u
        };
        FILE *wav = fopen("out/first-tone.wav", "wb");
        if (wav != NULL) {
            uint8_t frame[4];
            size_t f = 0u;
            (void)fwrite(hdr, 1u, sizeof(hdr), wav);
            for (f = 0u; f < 512u; ++f) {
                frame[0] = (uint8_t)left[f];
                frame[1] = (uint8_t)((uint16_t)left[f] >> 8u);
                frame[2] = (uint8_t)right[f];
                frame[3] = (uint8_t)((uint16_t)right[f] >> 8u);
                (void)fwrite(frame, 1u, sizeof(frame), wav);
            }
            (void)fclose(wav);
            fprintf(stderr, "first-tone: wrote out/first-tone.wav\n");
        } else {
            fprintf(stderr, "first-tone: out/ missing, WAV skipped\n");
        }
    }
}

/* Frame-342 AD21 L2's B9/BB..BE packets take the B0-class
 * default table door. BA instead selects the sample uploader, which
 * needs an independent original 68000 differential before asserting
 * a whole-stream drain. This is a ROM-free C routing regression. */
static void test_ad21_b_class_drain(void)
{
    static vf2_sound_board board;
    static uint8_t rom[0x10010u];
    static const uint8_t sample[2] = {0u, 0u};
    unsigned i;
    test_boot_rom(rom);
    EXPECT_TRUE(vf2_sound_board_initialize(&board, rom, sizeof(rom),
                                           sample, sizeof(sample)) == VF2_OK);
    for (i = 0u; i < 12u; ++i) {
        uint8_t *desc = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        vf2_sound_dispatch_report report;
        const uint8_t packet[4] = {
            (uint8_t)(0xb9u + i % 6u),
            (uint8_t)(i < 6u ? 0x2au : 7u),
            (uint8_t)(i < 6u ? 0u : 0x7fu), 0u
        };
        /* BA maps to uploader 0x1ac0; do not assert it is a no-op. */
        if (packet[0] == 0xbau) continue;
        /* One matching active channel descriptor; other rows remain
         * as initialized. These B0 default doors consume the ring. */
        memset(board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE, 0,
               VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
               VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
        desc[0] = 0x80u;
        desc[1] = (uint8_t)(packet[0] & 0x0fu);
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) == VF2_OK);
        EXPECT_TRUE(report.command == packet[0]);
        EXPECT_TRUE(report.payload == packet[1]);
        EXPECT_TRUE(report.command_class == 0xb0u);
        EXPECT_TRUE(report.descriptors_matched == 1u);
        EXPECT_TRUE(report.handler_address == UINT32_C(0x13cc));
        EXPECT_TRUE(report.selected_value == (uint8_t)(packet[1] & 0x7fu));
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    }
}

/* Original-ROM BA 2A 00 and BA 07 7F do NOT target the 0x1ac0
 * uploader. The B0 dispatch selects table entries by ring byte 1,
 * not the BA command byte: the staging area must remain untouched. */
static void test_ad21_ba_does_not_stage(void)
{
    static vf2_sound_board board;
    static uint8_t rom[0x10010u];
    static const uint8_t sample[2] = {0x35u, 0xacu};
    static const uint8_t selectors[2] = {0x2au, 0x07u};
    unsigned i;
    for (i = 0u; i < 2u; ++i) {
        uint8_t *descriptor;
        vf2_sound_dispatch_report report;
        uint8_t before_sample[16u];
        uint8_t before_scsp[sizeof(board.scsp.registers)];
        const uint8_t packet[4] = {
            0xbau, selectors[i], (uint8_t)(i == 0u ? 0u : 0x7fu), 0u
        };
        test_boot_rom(rom);
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rom, sizeof(rom),
                                               sample, sizeof(sample)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        memset(descriptor, 0, VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
               VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
        descriptor[0] = 0x80u;
        descriptor[1] = 0x0au;
        memset(board.sound_ram + 0x5000u, 0x55, 16u);
        memcpy(before_sample, board.sound_ram + 0x50000u,
               sizeof(before_sample));
        memcpy(before_scsp, board.scsp.registers, sizeof(before_scsp));
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) == VF2_OK);
        EXPECT_TRUE(report.command == 0xbau);
        EXPECT_TRUE(report.payload == selectors[i]);
        EXPECT_TRUE(report.command_class == 0xb0u);
        EXPECT_TRUE(report.handler_address == UINT32_C(0x13cc));
        EXPECT_TRUE(report.selected_value == selectors[i]);
        EXPECT_TRUE(report.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[0x5000u] == 0x55u);
        EXPECT_TRUE(board.sound_ram[0x5001u] == 0x55u);
        EXPECT_TRUE(memcmp(before_sample, board.sound_ram + 0x50000u,
                           sizeof(before_sample)) == 0);
        EXPECT_TRUE(memcmp(before_scsp, board.scsp.registers,
                           sizeof(before_scsp)) == 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    }
}

/* Explicit B0 table selector 0x3a reaches the 0x1ac0 uploader.
 * It consumes ring byte 2 (0x07) as the sample-row index. Changing
 * ring byte 3 must not change the uploaded bytes. */
static void test_ad21_ba07_sample_copy(void)
{
    static vf2_sound_board board;
    static uint8_t rom[0x10010u];
    static const uint8_t sample[2] = {0x35u, 0xacu};
    vf2_sound_dispatch_report report;
    uint8_t *descriptor;
    uint8_t sample_first[16u] = {0u};
    uint8_t slot_first[16u] = {0u};
    uint8_t scsp_first[sizeof(board.scsp.registers)] = {0u};
    unsigned variant;
    uint8_t packet[4] = {0xbau, 0x3au, 0x07u, 0u};
    test_boot_rom(rom);
    /* Row 7 points at a one-record selection of entry 0. */
    rom[0x8df0u] = 0u; rom[0x8df1u] = 7u;
    rom[0x8e0eu] = 0u; rom[0x8e0fu] = 0x60u;
    rom[0x8e10u] = 0x78u; rom[0x8e11u] = 0u;
    rom[0x7800u] = 0u; rom[0x7801u] = 0u;
    rom[0x7802u] = 0u; rom[0x7803u] = 0u;
    /* Entry-0 source: two bytes in the synthetic audio ROM.
     * (length + 0x1ff) & 0xffff = 1. */
    rom[0x5b28u] = 0u; rom[0x5b29u] = 0x60u;
    rom[0x5b2au] = 0x74u; rom[0x5b2bu] = 0u;
    rom[0x5b2cu] = 0u; rom[0x5b2du] = 0u;
    rom[0x5b2eu] = 0xfeu; rom[0x5b2fu] = 2u;
    rom[0x7400u] = 0xd3u; rom[0x7401u] = 0x4au;
    for (variant = 0u; variant < 2u; ++variant) {
    packet[3] = variant == 0u ? 0x7fu : 0u;
    EXPECT_TRUE(vf2_sound_board_initialize(&board, rom, sizeof(rom),
                                           sample, sizeof(sample)) == VF2_OK);
    descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    memset(descriptor, 0, VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
           VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
    descriptor[0] = 0x80u;
    descriptor[1] = 0x0au;
    board.sound_ram[0x50000u] = 0u;
    board.sound_ram[0x50001u] = 0u;
    EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) == VF2_OK);
    EXPECT_TRUE(report.command == 0xbau);
    EXPECT_TRUE(report.payload == 0x3au);
    EXPECT_TRUE(report.selected_value == 0x3au);
    EXPECT_TRUE(report.descriptors_matched == 1u);
    EXPECT_TRUE(board.sound_ram[0x50000u] == 0xd3u);
    EXPECT_TRUE(board.sound_ram[0x50001u] == 0x4au);
    EXPECT_TRUE(board.sound_ram[0x50000u + 2u] == 0u);
    EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    if (variant == 0u) {
        memcpy(sample_first, board.sound_ram + 0x50000u, sizeof(sample_first));
        memcpy(slot_first, board.sound_ram + 0x5000u, sizeof(slot_first));
        memcpy(scsp_first, board.scsp.registers, sizeof(scsp_first));
    } else {
        EXPECT_TRUE(memcmp(sample_first, board.sound_ram + 0x50000u,
                           sizeof(sample_first)) == 0);
        EXPECT_TRUE(memcmp(slot_first, board.sound_ram + 0x5000u,
                           sizeof(slot_first)) == 0);
        EXPECT_TRUE(memcmp(scsp_first, board.scsp.registers,
                           sizeof(scsp_first)) == 0);
    }
    }
}

/* Explicit B0 selector 0x3a with argument byte 0x2a selects row 42.
 * This is NOT the original AD21 BA 2A 00 packet, which selects table
 * entry 0x2a instead of the uploader. */
static void test_ad21_ba2a_sample_copy(void)
{
    static vf2_sound_board board;
    static uint8_t rom[0x10010u];
    static const uint8_t sample[2] = {0x35u, 0xacu};
    vf2_sound_dispatch_report report;
    uint8_t *descriptor;
    uint8_t packet[4] = {0xbau, 0x3au, 0x2au, 0u};
    uint8_t first_sample[16u] = {0u};
    uint8_t first_slots[16u] = {0u};
    uint8_t first_scsp[sizeof(board.scsp.registers)] = {0u};
    unsigned variant;
    test_boot_rom(rom);
    rom[0x8df0u] = 0u; rom[0x8df1u] = 0x2au;
    /* Directory entry at 0x608df2 + 0x2a * 4 = 0x608e9a. */
    rom[0x8e9au] = 0u; rom[0x8e9bu] = 0x60u;
    rom[0x8e9cu] = 0x78u; rom[0x8e9du] = 0u;
    rom[0x7800u] = 0u; rom[0x7801u] = 0u;
    rom[0x7802u] = 0u; rom[0x7803u] = 0u;
    rom[0x5b28u] = 0u; rom[0x5b29u] = 0x60u;
    rom[0x5b2au] = 0x74u; rom[0x5b2bu] = 0u;
    rom[0x5b2cu] = 0u; rom[0x5b2du] = 0u;
    rom[0x5b2eu] = 0xfeu; rom[0x5b2fu] = 2u;
    rom[0x7400u] = 0x2au; rom[0x7401u] = 0xb9u;
    for (variant = 0u; variant < 2u; ++variant) {
    packet[3] = variant == 0u ? 0u : 0x7fu;
    EXPECT_TRUE(vf2_sound_board_initialize(&board, rom, sizeof(rom),
                                           sample, sizeof(sample)) == VF2_OK);
    descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    memset(descriptor, 0, VF2_SOUND_COMMAND_DESCRIPTOR_COUNT *
           VF2_SOUND_COMMAND_DESCRIPTOR_BYTES);
    descriptor[0] = 0x80u;
    descriptor[1] = 0x0au;
    board.sound_ram[0x50000u] = 0u;
    board.sound_ram[0x50001u] = 0u;
    EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) == VF2_OK);
    EXPECT_TRUE(report.command == 0xbau);
    EXPECT_TRUE(report.payload == 0x3au);
    EXPECT_TRUE(report.selected_value == 0x3au);
    EXPECT_TRUE(report.descriptors_matched == 1u);
    EXPECT_TRUE(board.sound_ram[0x50000u] == 0x2au);
    EXPECT_TRUE(board.sound_ram[0x50001u] == 0xb9u);
    EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    if (variant == 0u) {
        memcpy(first_sample, board.sound_ram + 0x50000u, sizeof(first_sample));
        memcpy(first_slots, board.sound_ram + 0x5000u, sizeof(first_slots));
        memcpy(first_scsp, board.scsp.registers, sizeof(first_scsp));
    } else {
        EXPECT_TRUE(memcmp(first_sample, board.sound_ram + 0x50000u,
                           sizeof(first_sample)) == 0);
        EXPECT_TRUE(memcmp(first_slots, board.sound_ram + 0x5000u,
                           sizeof(first_slots)) == 0);
        EXPECT_TRUE(memcmp(first_scsp, board.scsp.registers,
                           sizeof(first_scsp)) == 0);
    }
    }
}

/* ROM-free pin for the native 0x2242 page/cursor effect.
 * Does not simulate the ROM's predicate or arcade IRQ cadence. */
static void store_pcm_cursor(uint8_t *p, uint32_t x)
{
    p[0] = (uint8_t)(x >> 24u);
    p[1] = (uint8_t)(x >> 16u);
    p[2] = (uint8_t)(x >> 8u);
    p[3] = (uint8_t)x;
}

static void test_pcm_stream_page_native(void)
{
    static vf2_sound_board b;
    static uint8_t samples[0x800u];
    uint8_t *slot0 = b.sound_ram + 0x2000u;
    uint8_t *slot7 = b.sound_ram + 0x2000u + 7u * 0x20u;
    uint8_t old_record[10u];
    size_t i;

    memset(&b, 0, sizeof(b));
    for (i = 0u; i < sizeof(samples); ++i) {
        samples[i] = (uint8_t)(i * 13u + 7u);
    }
    b.sample_rom = samples;
    b.sample_rom_size = sizeof(samples);
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(NULL, 0u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 32u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 0u) ==
                VF2_ERROR_UNSUPPORTED);

    slot0[0] = 0x80u;
    store_pcm_cursor(slot0 + 2u, UINT32_C(0x800200));
    store_pcm_cursor(slot0 + 6u, UINT32_C(0x10200));
    memset(b.sound_ram + 0x10200u, 0xa5, 1024u);
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 0u) == VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x10200u,
                       samples + 0x200u, 512u) == 0);
    EXPECT_TRUE(slot0[0] == 0x80u && slot0[4] == 0x04u &&
                slot0[8] == 0x04u);
    EXPECT_TRUE(b.sound_ram[0x10400u] == 0xa5u);
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 0u) == VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x10400u,
                       samples + 0x400u, 512u) == 0);
    EXPECT_TRUE(slot0[4] == 0x06u && slot0[8] == 0x06u);

    /* The observed first full original walker also touched slot 7.
     * Its independent record must not alias the slot-0 cursors. */
    slot7[0] = 0x80u;
    store_pcm_cursor(slot7 + 2u, UINT32_C(0x800000));
    store_pcm_cursor(slot7 + 6u, UINT32_C(0x12000));
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 7u) == VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x12000u, samples, 512u) == 0);
    EXPECT_TRUE(slot7[4] == 0x02u && slot7[8] == 0x22u);
    EXPECT_TRUE(slot0[4] == 0x06u && slot0[8] == 0x06u);

    /* Alternate/unknown flags stay held and cursors unmodified. */
    slot7[0] = 0x81u;
    memcpy(old_record, slot7, sizeof(old_record));
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 7u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(old_record, slot7, sizeof(old_record)) == 0);
    slot7[0] = 0x80u;
    memset(b.sound_ram + 0x12200u, 0x5a, 512u);

    /* Invalid alignment, past-ROM and past-RAM must not partially copy
     * and must preserve both 32-bit cursor words exactly. */
    store_pcm_cursor(slot7 + 2u, UINT32_C(0x800001));
    store_pcm_cursor(slot7 + 6u, UINT32_C(0x12200));
    memcpy(old_record, slot7, sizeof(old_record));
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 7u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(old_record, slot7, sizeof(old_record)) == 0);
    EXPECT_TRUE(b.sound_ram[0x12200u] == 0x5au);

    store_pcm_cursor(slot7 + 2u, UINT32_C(0x800700));
    memcpy(old_record, slot7, sizeof(old_record));
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 7u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(old_record, slot7, sizeof(old_record)) == 0);
    EXPECT_TRUE(b.sound_ram[0x12200u] == 0x5au);

    store_pcm_cursor(slot7 + 2u, UINT32_C(0x800000));
    store_pcm_cursor(slot7 + 6u, UINT32_C(0x7ff00));
    memcpy(old_record, slot7, sizeof(old_record));
    EXPECT_TRUE(vf2_sound_board_copy_stream_page_512(&b, 7u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(old_record, slot7, sizeof(old_record)) == 0);
    EXPECT_TRUE(b.sound_ram[0x7ff00u] == 0u);
}

/* Independent ROM-free gates for the positive original 0x2242
 * full-page walker arm; no synthetic scheduling/cadence is assumed. */
static void test_pcm_walker_full_page(void)
{
    static vf2_sound_board b;
    static uint8_t samples[0x1000u];
    uint8_t *record = b.sound_ram + 0x2000u + 3u * 0x20u;
    uint8_t before[0x20u];
    size_t i;
    memset(&b, 0, sizeof(b));
    for (i = 0u; i < sizeof(samples); ++i) {
        samples[i] = (uint8_t)(i * 7u + 13u);
    }
    b.sample_rom = samples;
    b.sample_rom_size = sizeof(samples);
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(NULL, 0u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 32u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    record[0] = 0x80u;
    record[1] = 0x10u; /* neither zero nor 0x80: no status gate */
    store_pcm_cursor(record + 2u, UINT32_C(0x800000));
    store_pcm_cursor(record + 6u, UINT32_C(0x10200));
    store_pcm_cursor(record + 0x0au, UINT32_C(0x900));
    record[0x16u] = 0x5au;
    record[0x1au] = 0x00u;
    record[0x1bu] = 0x03u;
    record[0x1cu] = 0x00u;
    record[0x1du] = 0x04u;
    memset(b.sound_ram + 0x10200u, 0x55, 1024u);
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) == VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x10200u, samples, 512u) == 0);
    EXPECT_TRUE(record[1] == 0x20u);
    EXPECT_TRUE(record[4] == 0x02u && record[8] == 0x04u);
    EXPECT_TRUE(record[0x0du] == 0x00u && record[0x0cu] == 0x07u);
    EXPECT_TRUE(record[0x1cu] == 0x00u && record[0x1du] == 0x07u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x5au);
    EXPECT_TRUE(b.sound_ram[0x10400u] == 0x55u);

    /* Mode zero selects the original alternate destination at +0E.
     * Unlike +06, this address is read only, never updated. */
    record[1] = 0u;
    record[0x16u] = 0x33u;
    store_pcm_cursor(record + 2u, UINT32_C(0x800200));
    store_pcm_cursor(record + 6u, UINT32_C(0x12200));
    store_pcm_cursor(record + 0x0eu, UINT32_C(0x14200));
    store_pcm_cursor(record + 0x0au, UINT32_C(0x700));
    b.scsp.registers[0x409u] = 0u;
    memcpy(before, record, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, record, sizeof(before)) == 0);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x5au);
    b.scsp.registers[0x409u] = 0x80u;
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) == VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x14200u, samples + 512u, 512u) == 0);
    EXPECT_TRUE(record[1] == 0x10u);
    EXPECT_TRUE(record[8] == 0x44u && record[9] == 0u);
    EXPECT_TRUE(record[0x0eu] == 0x00u && record[0x0fu] == 0x01u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x33u);

    /* Mode 0x80 uses +06, with the inverse MIDI7 gate. */
    record[1] = 0x80u;
    record[0x16u] = 0x77u;
    store_pcm_cursor(record + 2u, UINT32_C(0x800400));
    store_pcm_cursor(record + 6u, UINT32_C(0x16200));
    store_pcm_cursor(record + 0x0au, UINT32_C(0x600));
    memcpy(before, record, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, record, sizeof(before)) == 0);
    b.scsp.registers[0x409u] = 0u;
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) == VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x16200u, samples + 1024u, 512u) == 0);
    EXPECT_TRUE(record[1] == 0x90u && record[8] == 0x64u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x77u);

    /* Remaining <=512 goes to the 0x23B0 alternative, NOT MOVEM
     * full-page path. Timer sign underflow also refuses atomically. */
    record[1] = 0x10u;
    store_pcm_cursor(record + 0x0au, UINT32_C(512));
    memcpy(before, record, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, record, sizeof(before)) == 0);
    store_pcm_cursor(record + 0x0au, UINT32_C(1024));
    record[0x1au] = 0xffu; record[0x1bu] = 0xf0u;
    record[0x1cu] = 0x00u; record[0x1du] = 0x01u;
    memcpy(before, record, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, record, sizeof(before)) == 0);
    record[0] = 0x00u;
    EXPECT_TRUE(vf2_sound_board_step_stream_full_page(&b, 3u) ==
                VF2_ERROR_UNSUPPORTED);
}

/* Exact 0x23B0 looping/short-page shape: page copy uses the
 * old source; the NEXT source/remaining come from 24-bit loop fields.
 * ROM-independent guards must not fabricate the 0x24BE branch. */
static void test_pcm_walker_short_loop_page(void)
{
    static vf2_sound_board b;
    static uint8_t samples[0x800u];
    uint8_t *r = b.sound_ram + 0x2000u + 4u * 0x20u;
    uint8_t before[0x20u];
    size_t i;
    memset(&b, 0, sizeof(b));
    for (i = 0u; i < sizeof(samples); ++i) {
        samples[i] = (uint8_t)(i * 11u + 3u);
    }
    b.sample_rom = samples;
    b.sample_rom_size = sizeof(samples);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(NULL, 4u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 32u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    r[0] = 0x81u;
    r[1] = 0x10u;
    store_pcm_cursor(r + 2u, UINT32_C(0x800000));
    store_pcm_cursor(r + 6u, UINT32_C(0x18200));
    store_pcm_cursor(r + 0x0au, UINT32_C(0x180));
    store_pcm_cursor(r + 0x12u, UINT32_C(0x8f2000));
    store_pcm_cursor(r + 0x16u, UINT32_C(0xa5012400));
    r[0x1au] = 0u;
    r[0x1bu] = 2u;
    r[0x1cu] = 0u;
    r[0x1du] = 5u;
    memset(b.sound_ram + 0x18200u, 0x55, 512u);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 4u) ==
                VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x18200u, samples, 512u) == 0);
    EXPECT_TRUE(r[1] == 0x20u);
    EXPECT_TRUE(r[2] == 0u && r[3] == 0x8fu &&
                r[4] == 0x20u && r[5] == 0x80u);
    EXPECT_TRUE(r[6] == 0u && r[7] == 0x01u &&
                r[8] == 0x84u && r[9] == 0u);
    EXPECT_TRUE(r[0x0au] == 0u && r[0x0bu] == 0x01u &&
                r[0x0cu] == 0x23u && r[0x0du] == 0x80u);
    EXPECT_TRUE(r[0x1cu] == 0u && r[0x1du] == 7u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0xa5u);

    /* At exactly 512 bytes, padding=0, source becomes masked loop
     * pointer; a 0x80-only flag must NOT use this branch. */
    r[0] = 0x80u;
    store_pcm_cursor(r + 0x0au, UINT32_C(512));
    memcpy(before, r, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 4u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, r, sizeof(before)) == 0);
    r[0] = 0x81u;
    r[1] = 0u;
    b.scsp.registers[0x409u] = 0u;
    memcpy(before, r, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 4u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, r, sizeof(before)) == 0);
    b.scsp.registers[0x409u] = 0x80u;
    store_pcm_cursor(r + 2u, UINT32_C(0x800200));
    store_pcm_cursor(r + 0x0eu, UINT32_C(0x1a200));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 4u) ==
                VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x1a200u,
                       samples + 0x200u, 512u) == 0);
    EXPECT_TRUE(r[1] == 0x10u && r[8] == 0xa4u);
    EXPECT_TRUE(r[0x0eu] == 0u && r[0x0fu] == 0x01u);

    /* Unsupported count and out-of-sample-range refuse atomically. */
    r[1] = 0x10u;
    store_pcm_cursor(r + 0x0au, UINT32_C(513));
    memcpy(before, r, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 4u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, r, sizeof(before)) == 0);
    store_pcm_cursor(r + 0x0au, UINT32_C(128));
    store_pcm_cursor(r + 2u, UINT32_C(0x800700));
    memcpy(before, r, sizeof(before));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_loop_page(&b, 4u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(before, r, sizeof(before)) == 0);
}

/* Terminal no-copy voice-release arm (ROM 0x24BE -> 0x24D2).
 * It must not perform any synthetic 512-byte PCM upload. */
static void test_pcm_walker_short_release(void)
{
    static vf2_sound_board b;
    uint8_t *r = b.sound_ram + 0x2000u + 2u * 0x20u;
    uint8_t *voice = b.sound_ram + VF2_SOUND_VOICE_TABLE_BASE + 3u * 0x10u;
    uint8_t snapshot[0x20u];
    memset(&b, 0, sizeof(b));
    r[0] = 0x88u;
    r[1] = 0x10u;
    r[0x12u] = 6u; /* SCSP byte offset=0x60, voice entry index 3 */
    r[0x16u] = 0x42u;
    r[0x1eu] = 0x12u;
    r[0x1fu] = 0x34u;
    store_pcm_cursor(r + 0x0au, 128u);
    voice[2u] = 8u; /* fast +0x50 countdown arm */
    voice[0x0bu] = 4u;
    b.scsp.registers[0x61u] = 0xffu;
    b.scsp.registers[0x62u] = 0xabu;
    b.sound_ram[0x10200u] = 0x5au;
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(NULL, 2u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 32u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 2u) ==
                VF2_OK);
    EXPECT_TRUE(r[0u] == 0u && r[1u] == 0x10u);
    EXPECT_TRUE(voice[2u] == 9u && voice[0x0bu] == 0x54u);
    EXPECT_TRUE(b.scsp.registers[0x61u] == 0x9fu);
    EXPECT_TRUE(b.scsp.registers[0x66u] == 0x12u &&
                b.scsp.registers[0x67u] == 0x34u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x42u);
    EXPECT_TRUE(b.sound_ram[0x10200u] == 0x5au);
    /* Slow +3 arm, byte overflow, and strict gate refusals. */
    r[0] = 0x88u;
    voice[2u] = 0u;
    voice[0x0bu] = 0xffu;
    b.scsp.registers[0x61u] = 0x7fu;
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 2u) ==
                VF2_OK);
    EXPECT_TRUE(voice[2u] == 1u && voice[0x0bu] == 2u);
    EXPECT_TRUE(b.scsp.registers[0x61u] == 0x1fu);
    r[0] = 0x88u;
    r[1] = 0x10u;
    memcpy(snapshot, r, sizeof(snapshot));
    b.scsp.registers[0x409u] = 0x80u;
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 2u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(snapshot, r, sizeof(snapshot)) == 0);
    b.scsp.registers[0x409u] = 0u;
    r[0x12u] = 7u; /* unaligned channel index refused */
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 2u) ==
                VF2_ERROR_UNSUPPORTED);
    r[0x12u] = 6u;
    r[0] = 0x89u; /* looping sample must use different arm */
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 2u) ==
                VF2_ERROR_UNSUPPORTED);
    r[0] = 0x88u;
    store_pcm_cursor(r + 0x0au, 513u);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_release(&b, 2u) ==
                VF2_ERROR_UNSUPPORTED);
}

/* 0x2522 terminal 512B MOVEM: unlike normal refills, +2 and +6
 * stay frozen, and 0x1E is the low-word delta plus remaining. */
static void test_pcm_walker_short_terminal(void)
{
    static vf2_sound_board b;
    static uint8_t samples[0x1000u];
    uint8_t *r = b.sound_ram + 0x2000u + 5u * 0x20u;
    uint8_t *voice = b.sound_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x30u;
    uint8_t saved[0x20u];
    size_t i;
    memset(&b, 0, sizeof(b));
    for (i = 0u; i < sizeof(samples); ++i) {
        samples[i] = (uint8_t)(i * 17u + 1u);
    }
    b.sample_rom = samples;
    b.sample_rom_size = sizeof(samples);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(NULL, 5u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 32u) ==
                VF2_ERROR_INVALID_ARGUMENT);
    r[0u] = 0x80u;
    r[1u] = 0x10u;
    store_pcm_cursor(r + 2u, UINT32_C(0x800200));
    store_pcm_cursor(r + 6u, UINT32_C(0x10200));
    store_pcm_cursor(r + 0x0au, UINT32_C(0x180));
    store_pcm_cursor(r + 0x0eu, UINT32_C(0x10100));
    r[0x12u] = 6u;
    r[0x16u] = 0x42u;
    r[0x1au] = 0u; r[0x1bu] = 2u;
    r[0x1cu] = 0u; r[0x1du] = 5u;
    memset(b.sound_ram + 0x10200u, 0x55, 512u);
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 5u) ==
                VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x10200u,
                       samples + 0x200u, 512u) == 0);
    EXPECT_TRUE(r[0u] == 0x88u && r[1u] == 0x20u);
    EXPECT_TRUE(r[0x1eu] == 0x02u && r[0x1fu] == 0x80u);
    EXPECT_TRUE(r[4u] == 0x02u && r[8u] == 0x02u);
    EXPECT_TRUE(r[0x0cu] == 0x01u && r[0x0du] == 0x80u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x42u);

    /* 0x25F6 mode bit7 -> 0x260C live release tail, even if the
     * bit3 of the incoming descriptor was CLEAR. */
    r[0u] = 0x80u;
    r[1u] = 0x90u;
    store_pcm_cursor(r + 2u, UINT32_C(0x800400));
    store_pcm_cursor(r + 6u, UINT32_C(0x10400));
    r[0x16u] = 0x6bu;
    b.scsp.registers[0x61u] = 0x7fu;
    voice[2u] = 0x08u;
    voice[0x0bu] = 4u;
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 5u) ==
                VF2_OK);
    EXPECT_TRUE(memcmp(b.sound_ram + 0x10400u,
                       samples + 0x400u, 512u) == 0);
    EXPECT_TRUE(r[0u] == 0u && r[1u] == 0x90u);
    EXPECT_TRUE(r[0x1eu] == 0x04u && r[0x1fu] == 0x80u);
    EXPECT_TRUE(b.scsp.registers[0x61u] == 0x1fu);
    EXPECT_TRUE(b.scsp.registers[0x66u] == 0x04u &&
                b.scsp.registers[0x67u] == 0x80u);
    EXPECT_TRUE(voice[2u] == 9u && voice[0x0bu] == 0x54u);
    EXPECT_TRUE(b.scsp.registers[0x408u] == 0x6bu);
    EXPECT_TRUE(r[4u] == 0x04u && r[8u] == 0x04u);

    /* Fail-closed for looping/busy entries, invalid sample page,
     * unsupported odd channel and unsigned countdown >512. */
    r[0u] = 0x81u;
    memcpy(saved, r, sizeof(saved));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 5u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(saved, r, sizeof(saved)) == 0);
    r[0u] = 0x80u;
    r[0x12u] = 7u;
    memcpy(saved, r, sizeof(saved));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 5u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(saved, r, sizeof(saved)) == 0);
    r[0x12u] = 6u;
    store_pcm_cursor(r + 0x0au, UINT32_C(513));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 5u) ==
                VF2_ERROR_UNSUPPORTED);
    store_pcm_cursor(r + 0x0au, UINT32_C(0x180));
    store_pcm_cursor(r + 2u, UINT32_C(0x800f00));
    memcpy(saved, r, sizeof(saved));
    EXPECT_TRUE(vf2_sound_board_step_stream_short_terminal(&b, 5u) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(memcmp(saved, r, sizeof(saved)) == 0);
}

int main(void)
{
    static uint8_t audio_rom[0x10010u];
    static const uint8_t sample_rom[] = {0xa5u, 0x5au};
    vf2_sound_board board;
    int16_t left[1] = {0};
    int16_t right[1] = {0};
    uint16_t value = 0u;
    uint8_t byte = 0u;
    vf2_sound_voice_maintenance_report maintenance;
    vf2_sound_stream_maintenance_report stream_maintenance;
    uint8_t *voices;

    test_boot_rom(audio_rom);
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                               sample_rom, sizeof(sample_rom)) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_write_u16(&board, 0x0000u, 0x1234u) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x0000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x1234u);
    EXPECT_TRUE(vf2_sound_board_write_u8(&board, 0x10040bu, 0x1fu) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_read_u8(&board, 0x10040bu, &byte) == VF2_OK);
    EXPECT_TRUE(byte == 0x1fu);
    EXPECT_TRUE(vf2_sound_board_write_u16(&board, 0x400000u, 0x0020u) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x400000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x0020u);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x600000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x1234u);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x800000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0xa55au);
    EXPECT_TRUE(vf2_scsp_read_sample(&board.scsp, 0u, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == 0xa5u);
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right, 1u) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_write_u8(&board, 0x600000u, 0u) ==
                VF2_ERROR_OUT_OF_BOUNDS);

    voices = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    /* Normal expiration: age field +9 and shared counter 0x151e. */
    voices[0] = 0u;
    voices[1] = 3u;
    voices[2] = 1u;
    voices[9] = 3u;
    voices[0x0b] = 1u;
    voices[VF2_SOUND_VOICE_BYTES + 9u] = 8u;
    /* Release expiration: alternate age field +5 and 0x152e. */
    voices[VF2_SOUND_VOICE_BYTES] = 0u;
    voices[VF2_SOUND_VOICE_BYTES + 1u] = 4u;
    voices[VF2_SOUND_VOICE_BYTES + 2u] = 9u;
    voices[VF2_SOUND_VOICE_BYTES + 5u] = 2u;
    voices[VF2_SOUND_VOICE_BYTES + 0x0bu] = 1u;
    voices[2u * VF2_SOUND_VOICE_BYTES + 5u] = 8u;
    voices[2u * VF2_SOUND_VOICE_BYTES + 9u] = 0xffu;
    board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] = 2u;
    board.sound_ram[VF2_SOUND_SHARED_COUNTER_152E] = 3u;
    /* Slot sentinels: both key-offs fire under the true scan, so
       SCSP[0x000f] (preset 0x5a) and ram[0x2004] (preset 0xaa) both
       fall. Pinned below. */
    board.sound_ram[0x2004u] = 0xaau;
    EXPECT_TRUE(vf2_scsp_write_u8(&board.scsp, 0x000fu, 0x5au) == VF2_OK);
    /* P0c voice-maintenance contract (v0821: true 32-voice scan; the
       frozen-pointer/bodge pins were +4 artefacts, retired). One call
       services every voice: voice[0] takes the normal arm (timer
       1 -> 0, key-off slot 0x0003, $151e--, +9 rebalance with
       d2 = 3), voice[1] takes the release arm (timer 1 -> 0, key-off
       slot 0x0004, $152e--, +5 rebalance with d2 = 2). The call
       returns OK -- the ISR always reaches rte (triangle L1:
       stepper 140/346/285 steps, a7 balanced on every leg). The
       triangle differential gates the silicon-correct shape; this
       unit test pins the same contract on the C alone. */
    EXPECT_TRUE(vf2_sound_board_maintain_voices(&board, &maintenance) ==
                VF2_OK);
    EXPECT_TRUE(maintenance.voices_scanned == 32u);
    EXPECT_TRUE(maintenance.voices_expired == 2u);
    EXPECT_TRUE(maintenance.normal_expirations == 1u);
    EXPECT_TRUE(maintenance.release_expirations == 1u);
    /* Normal rebalance (d2 = 3): voice1 +9 = 8 -> 7 (3 < 8);
       voice2 +9 = 0xff skipped. Release rebalance (d2 = 2): voice2
       (+9 == 0xff) +5 = 8 -> 7 (2 < 8). */
    EXPECT_TRUE(maintenance.voices_aged == 2u);
    /* Counter 151e (normal expiration): decremented exactly once. */
    EXPECT_TRUE(maintenance.counter_151e_before == 2u);
    EXPECT_TRUE(maintenance.counter_151e_after == 1u);
    /* Counter 152e (release expiration): decremented exactly once. */
    EXPECT_TRUE(maintenance.counter_152e_before == 3u);
    EXPECT_TRUE(maintenance.counter_152e_after == 2u);
    /* Voice 0 expired via the normal arm. */
    EXPECT_TRUE(voices[2u] == 0u);
    EXPECT_TRUE(voices[9u] == 0u);
    /* Voice 1 expired via the release arm (reached on the same call:
       the scan visits every voice). Its +9 passed through 7 (normal
       rebalance) before the release arm cleared it; +5 cleared. */
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 2u] == 0u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 9u] == 0u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 5u] == 0u);
    /* Voice 2 is the release-rebalance witness (+9 == 0xff gates it
       in; +5 = 8 -> 7). */
    EXPECT_TRUE(voices[2u * VF2_SOUND_VOICE_BYTES + 9u] == 0xffu);
    EXPECT_TRUE(voices[2u * VF2_SOUND_VOICE_BYTES + 5u] == 7u);
    /* ROM V1 key: SCSP[slotword+0x0b] = 0x1f, ram[0x2000+slotword] = 0
       (slot words 0x0003/0x0004 above). No control-bit RMW exists.
       Both key-offs fire, so both slot sentinels fall. */
    EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x000eu, &byte) == VF2_OK);
    EXPECT_TRUE(byte == 0x1fu);
    EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x000fu, &byte) == VF2_OK);
    EXPECT_TRUE(byte == 0x1fu);
    EXPECT_TRUE(board.sound_ram[0x2003u] == 0u);
    EXPECT_TRUE(board.sound_ram[0x2004u] == 0u);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x041au, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x03c0u);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x0422u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x0080u);

    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                               sample_rom, sizeof(sample_rom)) == VF2_OK);
    {
        uint8_t *descriptor = board.sound_ram +
            VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        uint8_t *voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
        vf2_sound_dispatch_report dispatch;

        descriptor[0] = 0x80u;
        descriptor[1] = 1u;
        descriptor[2] = 0x01u;
        voice[0] = 0u;
        voice[1] = 3u;
        voice[2] = 0x40u;
        /* Byte match (v0819): ROM 0x1e1c is cmp.B on voice[4]. */
        voice[4] = 1u;
        voice[5] = 1u;
        board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] = 1u;
        board.sound_ram[0x2000u] = 0xaau;
        EXPECT_TRUE(vf2_sound_board_write_u8(
            &board, VF2_SCSP_CPU_BASE + 0u, 0xffu
        ) == VF2_OK);
        {
            const uint8_t packet[4] = {0xc1u, 0x70u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.command == 0xc1u);
        EXPECT_TRUE(dispatch.payload == 0x70u);
        EXPECT_TRUE(dispatch.descriptors_scanned == 20u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.voices_cleared == 1u);
        EXPECT_TRUE(dispatch.handler_address == 0x1e5cu);
        EXPECT_TRUE(dispatch.handled != 0);
        /* C0 original D2 is the packet PAYLOAD: 0x70 | 0x80 = F0.
         * Real frame-342 independent 68000 observations likewise pin
         * CD74->F4, CA7E->FE and CB7F->FF. */
        EXPECT_TRUE(descriptor[2] == 0xf0u);
        EXPECT_TRUE(voice[2] == 0u && voice[4] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] == 4u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0xff00u);
        /* ROM V5 key (d4 = voice[0] = 0x0003): SCSP[0x000e] = 0x1f,
           SCSP[0x0003] = 0x10, ram[0x2003] = 0. ram[0x2000] keeps its
           0xaa (untouched); SCSP[0] keeps the pre-write 0xff. */
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x0eu, &byte) == VF2_OK);
        EXPECT_TRUE(byte == 0x1fu);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x0003u, &byte) == VF2_OK);
        EXPECT_TRUE(byte == 0x10u);
        EXPECT_TRUE(board.sound_ram[0x2003u] == 0u);
        EXPECT_TRUE(board.sound_ram[0x2000u] == 0xaau);

        /* 90-class with a zero second byte (S2b primer truth: head
           = payload 0, second = ring byte 2 = 0): base 0x10 stored
           (0x00 + 0x10), then the 0x866 beq-trap into the 80 body
           mid-instruction. Mode unread, packet held. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u] = 0x77u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0x90u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x858u);
        EXPECT_TRUE(dispatch.channel == 0u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x10u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* 90-class prefix (S2b primer truth: head = payload 0x05,
           second = ring byte 2 = 0x02). Base 0x15 (0x05 + 0x10),
           mode = ROM[0x60c6a6+2]. desc[2] = 3 is LOW (E1k):
           sel-3 synthetic row (th 0x7f, word 0x00ff), walk
           stops at e0, files $1516/$1518/$1515/$1511, runs the
           E1m allocator (voice0 free), refuses at 0xfac,
           packet held. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 2u] = 0x44u;
        audio_rom[0x53b6u] = 0x00u; audio_rom[0x53b7u] = 0x60u;
        audio_rom[0x53b8u] = 0x74u; audio_rom[0x53b9u] = 0x00u;
        audio_rom[0x7400u] = 0x7fu; audio_rom[0x7402u] = 0x00u;
        audio_rom[0x7403u] = 0x00u; audio_rom[0x7404u] = 0x00u;
        audio_rom[0x7405u] = 0xffu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 3u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0x93u, 0x05u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0xfacu);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x44u);
        EXPECT_TRUE(dispatch.selected_value == 0x44u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD + 1u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_META] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x2515u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CONTROL] == 0x00u);
        /* Allocator (E1m): init-staged cursor 0x2800, all voices
           free -> voice0, cursor 0x2810, refuse at 0xfac. */
        EXPECT_TRUE(board.sound_ram[0x251au] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x251au + 1u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x251au + 2u] == 0x28u);
        EXPECT_TRUE(board.sound_ram[0x251au + 3u] == 0x10u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* 90-class mode with a nonzero base high byte (S2b: base =
           0x05 + 0x80 = 0x85, mid 0, so the marker lives at
           0x60c6a6+2 -- the old +0x10000 file read a mode the new
           head never addresses). desc[2] = 15 is LOW (E1k):
           sel-15 synthetic row needs th >= 0x85 (0x7f would be
           skipped), so th 0xff; walk stops at e0, files, runs
           the allocator, refuses at 0xfac, held. */
        {
            static uint8_t big_rom[0x20000u];
            memset(big_rom, 0, sizeof(big_rom));
            test_boot_rom(big_rom);
            big_rom[0xc6a6u + 2u] = 0x2au;
            big_rom[0x53e6u] = 0x00u; big_rom[0x53e7u] = 0x60u;
            big_rom[0x53e8u] = 0x74u; big_rom[0x53e9u] = 0x00u;
            big_rom[0x7400u] = 0xffu; big_rom[0x7402u] = 0x00u;
            big_rom[0x7403u] = 0x00u; big_rom[0x7404u] = 0x00u;
            big_rom[0x7405u] = 0xffu;
            EXPECT_TRUE(vf2_sound_board_initialize(&board, big_rom, sizeof(big_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 15u;
            descriptor[2] = 15u;
            descriptor[4] = 0x80u;
            {
                const uint8_t packet[4] = {0x9fu, 0x05u, 0x02u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                        VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(dispatch.handler_address == 0xfacu);
            EXPECT_TRUE(dispatch.channel == 15u);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x85u);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x2au);
            EXPECT_TRUE(dispatch.selected_value == 0x2au);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD + 1u] == 0xffu);
            EXPECT_TRUE(board.sound_ram[0x251au] == 0x00u);
            EXPECT_TRUE(board.sound_ram[0x251au + 1u] == 0x00u);
            EXPECT_TRUE(board.sound_ram[0x251au + 2u] == 0x28u);
            EXPECT_TRUE(board.sound_ram[0x251au + 3u] == 0x10u);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        }

        /* 90 HIGH door, synthetic table (S2c): desc[2] = 0xf0 ->
           select 0, pointer at 0x6032c6 -> 0x605000; payload 0x02
           -> stride 12 -> entry 0x60500c {0x1234, 0x5678, 0x41,
           0x9a}. Gate word != 0x00ff (passes); byte4 != 0x80 (no
           sentinel); desc[13] = 0 + desc[12] = 0x08 (&7 == 0) ->
           $150d = byte(a2+5) = 0x9a. Refuse at the Loop-B door,
           packet held. (The drain-entry differentials prove the
           same path on real ROM; this leg pins the exact table
           arithmetic.) */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 2u] = 0x44u;
        audio_rom[0x32c6u] = 0x00u; audio_rom[0x32c7u] = 0x60u;
        audio_rom[0x32c8u] = 0x50u; audio_rom[0x32c9u] = 0x00u;
        audio_rom[0x500cu] = 0x12u; audio_rom[0x500du] = 0x34u;
        audio_rom[0x500eu] = 0x56u; audio_rom[0x500fu] = 0x78u;
        audio_rom[0x5010u] = 0x41u; audio_rom[0x5011u] = 0x9au;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 0xf0u;
        descriptor[4] = 0x10u;
        descriptor[6] = 0x55u;
        descriptor[12] = 0x08u;
        descriptor[13] = 0x00u;
        {
            const uint8_t packet[4] = {0x93u, 0x02u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        /* S2d: the HIGH door falls through to Loop-B, which doors
           0xf12 here ($150e ch 3, $152e 0) into the E1o
           allocator (voice0 free: cursor 0x2810, refuse at
           0xfac). $1515 reads 0x00 from this synthetic image. */
        EXPECT_TRUE(dispatch.handler_address == 0xfacu);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x12u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x44u);
        EXPECT_TRUE(board.sound_ram[0x2511u] == 0x41u);
        EXPECT_TRUE(board.sound_ram[0x2512u] == 0x56u);
        EXPECT_TRUE(board.sound_ram[0x2513u] == 0x78u);
        EXPECT_TRUE(board.sound_ram[0x2516u] == 0x12u);
        EXPECT_TRUE(board.sound_ram[0x2517u] == 0x34u);
        EXPECT_TRUE(board.sound_ram[0x250du] == 0x9au);
        EXPECT_TRUE(board.sound_ram[0x2515u] == 0u);
        EXPECT_TRUE(board.sound_ram[0x251au] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x251au + 1u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x251au + 2u] == 0x28u);
        EXPECT_TRUE(board.sound_ram[0x251au + 3u] == 0x10u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* 90 HIGH 0x80 sentinel (S2c): same table with byte4 =
           0x80 -> $1511 = desc[6] = 0x55; desc[13] = 0x07 (nonzero)
           files itself and clears. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 2u] = 0x44u;
        audio_rom[0x32c6u] = 0x00u; audio_rom[0x32c7u] = 0x60u;
        audio_rom[0x32c8u] = 0x50u; audio_rom[0x32c9u] = 0x00u;
        audio_rom[0x500cu] = 0x12u; audio_rom[0x500du] = 0x34u;
        audio_rom[0x500eu] = 0x56u; audio_rom[0x500fu] = 0x78u;
        audio_rom[0x5010u] = 0x80u; audio_rom[0x5011u] = 0x9au;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 0xf0u;
        descriptor[4] = 0x10u;
        descriptor[6] = 0x55u;
        descriptor[12] = 0x08u;
        descriptor[13] = 0x07u;
        {
            const uint8_t packet[4] = {0x93u, 0x02u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        /* S2d: the HIGH door falls through to Loop-B, which doors
           0xf12 here ($150e ch 3, $152e 0) into the allocator
           (cursor 0x2810, refuse at 0xfac). $1515 reads 0x00
           from this synthetic image. */
        EXPECT_TRUE(dispatch.handler_address == 0xfacu);
        EXPECT_TRUE(board.sound_ram[0x2511u] == 0x55u);
        EXPECT_TRUE(board.sound_ram[0x250du] == 0x07u);
        EXPECT_TRUE(descriptor[13] == 0u);
        EXPECT_TRUE(board.sound_ram[0x251au] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x251au + 1u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x251au + 2u] == 0x28u);
        EXPECT_TRUE(board.sound_ram[0x251au + 3u] == 0x10u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);

        /* 90 HIGH gate-hit (S2c): entry word 0x00ff -> 0x8b0 rts,
           head effects only, scan continues, packet consumed. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 2u] = 0x44u;
        audio_rom[0x32c6u] = 0x00u; audio_rom[0x32c7u] = 0x60u;
        audio_rom[0x32c8u] = 0x50u; audio_rom[0x32c9u] = 0x00u;
        audio_rom[0x500cu] = 0x00u; audio_rom[0x500du] = 0xffu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 0xf0u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0x93u, 0x02u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x858u);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x12u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x44u);
        EXPECT_TRUE(board.sound_ram[0x2511u] == 0u);
        EXPECT_TRUE(board.sound_ram[0x2512u] == 0u);
        EXPECT_TRUE(board.sound_ram[0x2516u] == 0u);
        EXPECT_TRUE(board.sound_ram[0x250du] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* Setup-author entry guards (S3c public boundary). */
        EXPECT_TRUE(vf2_sound_board_setup_author(
            NULL, 0u, 0u, &dispatch) == VF2_ERROR_INVALID_ARGUMENT);
        EXPECT_TRUE(vf2_sound_board_setup_author(
            &board, 0u, 0u, NULL) == VF2_ERROR_INVALID_ARGUMENT);
        EXPECT_TRUE(vf2_sound_board_setup_author(
            &board, 20u, 0u,
            &dispatch) == VF2_ERROR_INVALID_ARGUMENT);
        EXPECT_TRUE(vf2_sound_board_setup_author(
            &board, 0u, 32u,
            &dispatch) == VF2_ERROR_INVALID_ARGUMENT);

        /* v0850 pins-audit: the setup-author odd-slot guard (ROM
           move.w address-error shape; E1j "odd/wild rows fail
           closed"). Slot word 0x0001 is odd: refuse before any
           authoring -- the flags byte stays clear. The stepper has no
           address-error model, so only this C-level leg pins the
           guard. */
        {
            uint8_t *odd_voice =
                board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            odd_voice[0] = 0x00u;
            odd_voice[1] = 0x01u;
            odd_voice[VF2_SOUND_VOICE_FLAGS] = 0x00u;
            EXPECT_TRUE(vf2_sound_board_setup_author(
                &board, 0u, 0u, &dispatch) == VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(odd_voice[VF2_SOUND_VOICE_FLAGS] == 0x00u);
        }

        /* Default class (S2b): ROM rts at 0x856 -- no body, no
           $150f/$1510 writes. The drain still consumes the packet
           (count 0, cursor 4, handled). */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 9u] = 0x33u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 3u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0xd3u, 0x07u, 0x09u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x856u);
        EXPECT_TRUE(dispatch.command_class == 0xd0u);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* Default class with a zero ring byte 2: identical ignore
           (the old zero-second trap belonged to the 90 body the
           default arm no longer enters). */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u] = 0x77u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 3u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0xd3u, 0x07u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x856u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* A0 max-gate (E1j): idx = ring byte 1 & 0x7f, max =
           row word, bgt (signed) over max exits via the 0x1eac
           rts (consume); else the 0x1e98 join (residue-wild,
           held). Packet {0xa0, 0x01, 0x01} rows max 0x0002:
           0x01 > 0x02 false -> join, UNSUPPORTED @0x1e98. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8e16u] = 0x00u; audio_rom[0x8e17u] = 0x60u;
        audio_rom[0x8e18u] = 0x74u; audio_rom[0x8e19u] = 0x00u;
        audio_rom[0x7400u] = 0x00u; audio_rom[0x7401u] = 0x02u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0u;
        {
            const uint8_t packet[4] = {0xa0u, 0x01u, 0x01u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x1e98u);
        EXPECT_TRUE(dispatch.channel == 0u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* Packet {0xa5, 0x02, 0x03} rows max 0x0002: 0x03 >
           0x02 -> rts, OK + consume (handler stays the 0x1e74
           head, OK-path precedent). */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8e1au] = 0x00u; audio_rom[0x8e1bu] = 0x60u;
        audio_rom[0x8e1cu] = 0x74u; audio_rom[0x8e1du] = 0x00u;
        audio_rom[0x7400u] = 0x00u; audio_rom[0x7401u] = 0x02u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 5u;
        descriptor[2] = 5u;
        {
            const uint8_t packet[4] = {0xa5u, 0x02u, 0x03u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x1e74u);
        EXPECT_TRUE(dispatch.channel == 5u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* v0850 pins-audit: the a0 row max word gates through move.w;
           an odd row pointer (0x00607401) is the ROM address-error
           shape -- refuse at the 0x1e74 head with the packet held.
           The stepper has no address-error model, so only this C-level
           leg pins the guard. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8e16u] = 0x00u; audio_rom[0x8e17u] = 0x60u;
        audio_rom[0x8e18u] = 0x74u; audio_rom[0x8e19u] = 0x01u; /* odd */
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0u;
        {
            const uint8_t packet[4] = {0xa0u, 0x01u, 0x01u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x1e74u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* L2 stream tick (v0852, ROM 0x1f16): the head applies two
           unconditional SCSP pokes (0x0418 = 0x00ce, 0x0422 = 0x0040),
           the ring-count gate reads 0x2506, and the body walks the
           eight 0x3000 records. On the boot state every countdown is
           clear: decrement-only, no RAM delta (the 51-step F10
           verdict). Poke the two registers away from their boot values
           first so the head is load-bearing here. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        board.scsp.registers[0x418u] = 0x11u;
        board.scsp.registers[0x419u] = 0x22u;
        board.scsp.registers[0x422u] = 0x33u;
        board.scsp.registers[0x423u] = 0x44u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.descriptors_scanned == 8u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 0u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.refused_stage == 0u);
        EXPECT_TRUE(stream_maintenance.refused_record == 0xffu);
        EXPECT_TRUE(stream_maintenance.escape_records == 0u);
        EXPECT_TRUE(stream_maintenance.pointer_handoffs == 0u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 0u);
        EXPECT_TRUE(board.scsp.registers[0x418u] == 0x00u &&
                    board.scsp.registers[0x419u] == 0xceu);
        EXPECT_TRUE(board.scsp.registers[0x422u] == 0x00u &&
                    board.scsp.registers[0x423u] == 0x40u);
        EXPECT_TRUE(stream_maintenance.write_cursor_before ==
                    stream_maintenance.write_cursor_after);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* Measured body: countdowns decay. The prescaler byte sits in
           record 0 ([0xc] != 0 -> [0xd]--); workers decay the word at
           +2 when bit 7 is set, 16-bit wrap exact (0 -> 0xffff). */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0x0cu] = 8u;
        descriptor[0x0du] = 5u;
        descriptor = board.sound_ram + VF2_SOUND_STREAM_ALTERNATE_BASE;
        descriptor[0] = 0x80u;
        descriptor[2] = 0u;
        descriptor[3] = 5u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE + 0x40u;
        descriptor[0] = 0x80u;
        descriptor[2] = 0u;
        descriptor[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.descriptors_scanned == 8u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x0du] == 4u);
        descriptor = board.sound_ram + VF2_SOUND_STREAM_ALTERNATE_BASE;
        EXPECT_TRUE(descriptor[0] == 0x80u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 4u);
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE + 0x40u;
        EXPECT_TRUE(descriptor[2] == 0xffu && descriptor[3] == 0xffu);

        /* An armed record whose countdown reaches zero hands off to the
           unmeasured applier: refuse before mutating anything and name
           the record (worker stage). */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0xc0u;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x81u;
        descriptor[7] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(stream_maintenance.refused_stage == 2u);
        EXPECT_TRUE(stream_maintenance.refused_record == 0u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(descriptor[0] == 0x80u);
        EXPECT_TRUE(descriptor[1] == 0xc0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 1u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x81u && descriptor[7] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(stream_maintenance.write_cursor_before ==
                    stream_maintenance.write_cursor_after);

        /* The prescaler's beat emit is the other unmeasured body:
           record 0 [0xc] != 0 with [0xd] == 1 refuses at the prescaler
           stage, and nothing is half-applied. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0x0cu] = 8u;
        descriptor[0x0du] = 1u;
        descriptor[0x0eu] = 8u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(stream_maintenance.refused_stage == 1u);
        EXPECT_TRUE(stream_maintenance.refused_record == 0u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 1u);
        EXPECT_TRUE(descriptor[0x0du] == 1u && descriptor[0x0eu] == 8u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* The ring-count gate: 0xff skips the body (0x1f2e bcc) even
           for an armed record; the head pokes still apply. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        board.sound_ram[VF2_SOUND_COMMAND_COUNT] = 0xffu;
        board.scsp.registers[0x418u] = 0x11u;
        descriptor = board.sound_ram + VF2_SOUND_STREAM_ALTERNATE_BASE;
        descriptor[0] = 0x80u;
        descriptor[2] = 0u;
        descriptor[3] = 5u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.descriptors_scanned == 0u);
        EXPECT_TRUE(stream_maintenance.refused_stage == 0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 5u);
        EXPECT_TRUE(board.scsp.registers[0x418u] == 0x00u &&
                    board.scsp.registers[0x419u] == 0xceu);

        /* The instruction-aligned e0 handler completes its table lookup and
           channel result write when no live voice needs the wider SCSP tail. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u;
        audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x10u;
        audio_rom[0x2b29u] = 0x00u;
        /* Primer-corrected head (S2a): payload 0 + byte2 0 ->
           transformed 0, bit8 clear, negated via the zero->0xFF
           rule. The marker lives at the NEGATED index 0xFF. */
        audio_rom[0x1000u + 0xffu] = 0x5au;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0x09u;
        descriptor[2] = 0x09u;
        descriptor[7] = 0u;
        {
            /* A matching channel-9 voice must NOT trigger the tail: the
               ROM exits via rts at 0x13c8 (e0 gate) instead. */
            uint8_t *nine_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            nine_voice[2] = 0x01u;
            nine_voice[4] = 0u;
            nine_voice[5] = 9u;
        }
        {
            const uint8_t packet[4] = {0xe9u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.channel == 9u);
        EXPECT_TRUE(dispatch.voices_matched == 0u);
        EXPECT_TRUE(dispatch.selected_value == 0x5au);
        EXPECT_TRUE(dispatch.signed_value == (int16_t)-0x5a);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 18u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 19u] == 0xa6u);

        /* Primer-corrected head (S2a): payload 0x42 + byte2 0 ->
           transformed 2, bit8 clear, negated (2 -> 0xfe): selected
           0x05 stored as -0x0005. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        audio_rom[0x4000u + 0xfeu] = 0x05u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 2u;
        descriptor[2] = 2u;
        descriptor[7] = 0u;
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.channel == 2u);
        EXPECT_TRUE(dispatch.selected_value == 0x05u);
        EXPECT_TRUE(dispatch.signed_value == (int16_t)-0x05);
        EXPECT_TRUE(dispatch.voices_matched == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 6u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 7u] == 0xfbu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 4u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 5u] == 0xfbu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* e0 voice tail (0x1356): per-match SCSP+0x10 and ram[0x601a]. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        /* Primer-corrected index is 0xFE (payload 0x42, byte2 0):
           the 0x05 marker lives there (shared = -5). */
        audio_rom[0x4000u + 0xfeu] = 0x05u;
        audio_rom[0x2682u] = 0x00u; audio_rom[0x2683u] = 0x60u;
        audio_rom[0x2684u] = 0x50u; audio_rom[0x2685u] = 0x00u;
        /* Slide is 0x001b under the primer-corrected result (-5
           shared + 0x20 addend), so the picker/timer bytes live at
           the +0x36/+0x96 offsets. Values unchanged (same picker ->
           same shaped 0x2234); only addresses move. */
        audio_rom[0x5036u] = 0x12u; audio_rom[0x5037u] = 0x34u;
        audio_rom[0xc72eu] = 0x00u; audio_rom[0xc72fu] = 0x60u;
        audio_rom[0xc730u] = 0x60u; audio_rom[0xc731u] = 0x00u;
        audio_rom[0x6096u] = 0xabu; audio_rom[0x6097u] = 0xcdu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 2u;
        descriptor[2] = 2u;
        /* P6 fixture shift (here and at the small-shaped e0 tail
           below): ROM 0x135e is `add.b`, so only descriptor[5] (the
           low byte) contributes to slide. voice[10] = 0x20 +
           desc[5] = 0 gives addend 0x20; with the corrected shared
           result (+5) slide = 0x0025, so the picker/timer marker
           bytes are staged at the +0x4a offsets. The rest of the e0
           fixture -- SCSP[0x30] expected 0x2234 here and 0x0020 in
           the small-shaped leg, ram[0x203a..0x203b] = 0xab,0xcd --
           continues to test the SAME indexed ROM byte. */
        descriptor[5] = 0u;
        descriptor[6] = 0u;
        descriptor[7] = 0u;
        {
            uint8_t *tail_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            /* Slot word 0x0020 (voice[0..1]) proves the stores are
               slot-indexed: SCSP[0x0030], ram[0x203a]. */
            tail_voice[0] = 0u;
            tail_voice[1] = 0x20u;
            tail_voice[2] = 0x01u;
            tail_voice[3] = 0x23u;
            /* Byte match (v0819): ROM 0x1332 is cmp.B on voice[4]. */
            tail_voice[4] = 2u;
            tail_voice[5] = 2u;
            tail_voice[10] = 0x20u;
        }
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.voices_matched == 1u);
        /* ROM 0x1396: SCSP[d4+0x10] = shaped with d4 = 0x0020, so
           SCSP[0x0030] = 0x2234. A d4 of 0 would coincide with the
           old absolute-write misreading; the nonzero slot word
           proves the indexing. */
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x30u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0x2234u);
        /* ROM 0x13c4: ram[0x201a+d4] = ram[0x203a], the stream-slot
           timer word, not ram[0x601a]. */
        EXPECT_TRUE(board.sound_ram[0x203au] == 0xabu);
        EXPECT_TRUE(board.sound_ram[0x203bu] == 0xcdu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* e0 voice tail, small shaped value: SCSP[d4+0x10] with
           d4 = 0x0020. voice[3] = 0x03 keeps nibble 3 (same tables)
           with high 0x00; picker 0x0020 -> shaped 0x0020 ->
           SCSP[0x0030] = 0x0020. Row base repatched for row 0. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        /* Primer-corrected index is 0xFE (see above). */
        audio_rom[0x4000u + 0xfeu] = 0x05u;
        audio_rom[0x2682u] = 0x00u; audio_rom[0x2683u] = 0x60u;
        audio_rom[0x2684u] = 0x50u; audio_rom[0x2685u] = 0x00u;
        /* Slide 0x001b (see above): picker at +0x36, timer at +0x96.
           Values unchanged (shaped stays picker = 0x0020). */
        audio_rom[0x5036u] = 0x00u; audio_rom[0x5037u] = 0x20u;
        audio_rom[0xc726u] = 0x00u; audio_rom[0xc727u] = 0x60u;
        audio_rom[0xc728u] = 0x60u; audio_rom[0xc729u] = 0x00u;
        audio_rom[0x6096u] = 0xabu; audio_rom[0x6097u] = 0xcdu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 2u;
        descriptor[2] = 2u;
        /* P6 fixture shift: see the matching note in the first e0
           fixture above. desc[6] = 0 here too; slide is 0x0025. */
        descriptor[5] = 0u;
        descriptor[6] = 0u;
        descriptor[7] = 0u;
        {
            uint8_t *tail_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            tail_voice[0] = 0u;
            tail_voice[1] = 0x20u;
            tail_voice[2] = 0x01u;
            tail_voice[3] = 0x03u;
            /* Byte match (v0819): ROM 0x1332 is cmp.B on voice[4]. */
            tail_voice[4] = 2u;
            tail_voice[5] = 2u;
            /* P6 fixture shift: ROM 0x135e is `add.b`, so only
               descriptor[5] (the low byte) adds; slide is 0x0025
               (see above). */
            tail_voice[10] = 0x20u;
        }
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.voices_matched == 1u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x0030u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0x0020u);
        EXPECT_TRUE(board.sound_ram[0x203au] == 0xabu);
        EXPECT_TRUE(board.sound_ram[0x203bu] == 0xcdu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* v0850 pins-audit: the e0 voice-tail odd-slot guard. Same
           fixture as the small-shaped leg above, but the slot word is
           0x0021 (odd): refuse (ROM move.w address-error shape) before
           the SCSP write -- slot+0x10 (0x0030) stays clear and the
           packet is held. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        audio_rom[0x4000u + 0xfeu] = 0x05u;
        audio_rom[0x2682u] = 0x00u; audio_rom[0x2683u] = 0x60u;
        audio_rom[0x2684u] = 0x50u; audio_rom[0x2685u] = 0x00u;
        audio_rom[0x5036u] = 0x00u; audio_rom[0x5037u] = 0x20u;
        audio_rom[0xc726u] = 0x00u; audio_rom[0xc727u] = 0x60u;
        audio_rom[0xc728u] = 0x60u; audio_rom[0xc729u] = 0x00u;
        audio_rom[0x6096u] = 0xabu; audio_rom[0x6097u] = 0xcdu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 2u;
        descriptor[2] = 2u;
        descriptor[5] = 0u;
        descriptor[6] = 0u;
        descriptor[7] = 0u;
        {
            uint8_t *tail_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            tail_voice[0] = 0u;
            tail_voice[1] = 0x21u; /* slot 0x0021: odd */
            tail_voice[2] = 0x01u;
            tail_voice[3] = 0x03u;
            tail_voice[4] = 2u;
            tail_voice[5] = 2u;
            tail_voice[10] = 0x20u;
        }
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.voices_matched == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x0030u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0u);

        /* B0 non-uploader: entry is ring byte 1 & 0x7f, not command & 0x7f. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 1u;
        descriptor[2] = 1u;
        {
            const uint8_t packet[4] = {0xb1u, 0x01u, 0x50u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.channel == 1u);
        EXPECT_TRUE(dispatch.selected_value == 0x01u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* BF 01 selects the B0 table entry 01, not command-derived 3f. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 15u;
        descriptor[2] = 15u;
        {
            const uint8_t packet[4] = {0xbfu, 0x01u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.channel == 15u);
        EXPECT_TRUE(dispatch.selected_value == 0x01u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* B0 entry 0x3a with third byte&0x7f == 0x7f exits via the 0x1b46
           epilogue (clean OK, verified beq target -- the old
           mid-dbf-trap/UNSUPPORTED pin was +4 damage): staged bytes
           persist (first_count 2 -> 3 bytes, dbf+1), packet consumed. */
        {
            static uint8_t rich_rom[0x10010u];
            memset(rich_rom, 0, sizeof(rich_rom));
            test_boot_rom(rich_rom);
            rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x02u;
            EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 10u;
            descriptor[2] = 10u;
            {
                const uint8_t packet[4] = {0xbau, 0x3au, 0x7fu, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                        VF2_OK);
            EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
            EXPECT_TRUE(dispatch.channel == 10u);
            EXPECT_TRUE(dispatch.selected_value == 0x3au);
            EXPECT_TRUE(board.sound_ram[0x5000u] == 0x00u);
            EXPECT_TRUE(board.sound_ram[0x5001u] == 0x80u);
            EXPECT_TRUE(board.sound_ram[0x5002u] == 0x00u);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
            EXPECT_TRUE(dispatch.cursor_after == 4u);
        }

        /* v0850 pins-audit: the sample stager's row/selector word reads
           address through the bus; an unmapped pointer is the bus-error
           class and fails closed (UNSUPPORTED, packet held) -- the same
           shape the stepper refuses with. The boot upload inside
           initialize() must still run with test_boot_rom's valid row,
           so each fixture poisons the row AFTER init.
           Row 0x005FFFFE fails the first (record-total) read while its
           +2 selector read would land on the audio ROM base, so the
           first fixture pins exactly the record-total guard; row
           0x0007FFFE (RAM top) passes the record-total read and pins
           exactly the selector guard. */
        {
            static uint8_t bus_rom[0x10010u];
            memset(bus_rom, 0, sizeof(bus_rom));
            test_boot_rom(bus_rom);
            EXPECT_TRUE(vf2_sound_board_initialize(&board, bus_rom, sizeof(bus_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            bus_rom[0x8df2u] = 0x00u; bus_rom[0x8df3u] = 0x5fu;
            bus_rom[0x8df4u] = 0xffu; bus_rom[0x8df5u] = 0xfeu;
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 10u;
            descriptor[2] = 10u;
            {
                const uint8_t packet[4] = {0xbau, 0x3au, 0u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                        VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
            EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

            memset(bus_rom, 0, sizeof(bus_rom));
            test_boot_rom(bus_rom);
            EXPECT_TRUE(vf2_sound_board_initialize(&board, bus_rom, sizeof(bus_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            bus_rom[0x8df2u] = 0x00u; bus_rom[0x8df3u] = 0x07u;
            bus_rom[0x8df4u] = 0xffu; bus_rom[0x8df5u] = 0xfeu;
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 10u;
            descriptor[2] = 10u;
            {
                const uint8_t packet[4] = {0xbau, 0x3au, 0u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                        VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
            EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);
        }

        /* B0 entry 0x3a upload (payload 0x05): max-gate + indexed row
           pick, two records (count 1 -> dbf+1), entry-indexed stored
           words, full-long differences, marker copies, consumed.
           (v0822: the old direct-be32 row read and fixed-$6008 stored
           word are retired; $6008 staying zero discriminates.) */
        {
            static uint8_t rich_rom[0x10010u];
            memset(rich_rom, 0, sizeof(rich_rom));
            test_boot_rom(rich_rom);
            /* max 5 at the directory base; row 5 -> 0x607800. */
            rich_rom[0x8df0u] = 0x00u; rich_rom[0x8df1u] = 0x05u;
            rich_rom[0x8e06u] = 0x00u; rich_rom[0x8e07u] = 0x60u;
            rich_rom[0x8e08u] = 0x78u; rich_rom[0x8e09u] = 0x00u;
            /* entry 0: source 0x607400 (deadbeef), 4-byte copy
               ((L+0x1ff)&0xffff == 3 -> L = 0xfe04),
               stored 0x11223344. */
            rich_rom[0x5b28u] = 0x00u; rich_rom[0x5b29u] = 0x60u;
            rich_rom[0x5b2au] = 0x74u; rich_rom[0x5b2bu] = 0x00u;
            rich_rom[0x5b2cu] = 0x00u; rich_rom[0x5b2du] = 0x00u;
            rich_rom[0x5b2eu] = 0xfeu; rich_rom[0x5b2fu] = 0x04u;
            rich_rom[0x5b30u] = 0x11u; rich_rom[0x5b31u] = 0x22u;
            rich_rom[0x5b32u] = 0x33u; rich_rom[0x5b33u] = 0x44u;
            /* row 5: count 1, both selectors entry 0. */
            rich_rom[0x7800u] = 0x00u; rich_rom[0x7801u] = 0x01u;
            rich_rom[0x7802u] = 0x00u; rich_rom[0x7803u] = 0x00u;
            rich_rom[0x7804u] = 0x00u; rich_rom[0x7805u] = 0x00u;
            rich_rom[0x7400u] = 0xdeu; rich_rom[0x7401u] = 0xadu;
            rich_rom[0x7402u] = 0xbeu; rich_rom[0x7403u] = 0xefu;
            EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 10u;
            descriptor[2] = 10u;
            {
                const uint8_t packet[4] = {0xbau, 0x3au, 0x05u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
            EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
            EXPECT_TRUE(dispatch.channel == 10u);
            EXPECT_TRUE(dispatch.selected_value == 0x3au);
            EXPECT_TRUE(dispatch.handled != 0);
            /* stored = 0x11223344 - 0x00607400 = 0x10c1bf44 at the
               indexed $5008 (the old fixed $6008 stays zero). */
            EXPECT_TRUE(board.sound_ram[0x5008u] == 0x10u);
            EXPECT_TRUE(board.sound_ram[0x5009u] == 0xc1u);
            EXPECT_TRUE(board.sound_ram[0x500au] == 0xbfu);
            EXPECT_TRUE(board.sound_ram[0x500bu] == 0x44u);
            EXPECT_TRUE(board.sound_ram[0x6008u] == 0x00u);
            /* record 1 overwrote the slot target last: 0x50004|bit7
               = 80 05 00 04 (verified by execution -- never hand-hex
               a target word again). */
            EXPECT_TRUE(board.sound_ram[0x5000u] == 0x80u);
            EXPECT_TRUE(board.sound_ram[0x5001u] == 0x05u);
            EXPECT_TRUE(board.sound_ram[0x5002u] == 0x00u);
            EXPECT_TRUE(board.sound_ram[0x5003u] == 0x04u);
            EXPECT_TRUE(board.sound_ram[0x50000u] == 0xdeu);
            EXPECT_TRUE(board.sound_ram[0x50001u] == 0xadu);
            EXPECT_TRUE(board.sound_ram[0x50002u] == 0xbeu);
            EXPECT_TRUE(board.sound_ram[0x50003u] == 0xefu);
            EXPECT_TRUE(board.sound_ram[0x50004u] == 0xdeu);
            EXPECT_TRUE(board.sound_ram[0x50005u] == 0xadu);
            EXPECT_TRUE(board.sound_ram[0x50006u] == 0xbeu);
            EXPECT_TRUE(board.sound_ram[0x50007u] == 0xefu);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
            EXPECT_TRUE(dispatch.cursor_after == 4u);
        }
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u] == 0u);
        /* 80 head (S2b primer truth: d2 = payload, base =
           (payload&0x7f) + desc[4]-byte -- 0x10da is add.b, desc[5]
           never participates). Packet {0x81, 0, ..} -> base 0x00.
           desc[2] = 1 takes the LOW arm (E1g): sel 1 staged to a
           synthetic row (th 0x7f, add 0, word 0x00ff), so the
           walk stops at e0, $1516 = 0x00ff, $1515 = 0x00; zeroed
           voices miss-all on the channel (1 vs 0), the dispatch
           completes and consumes. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x53aeu] = 0x00u; audio_rom[0x53afu] = 0x60u;
        audio_rom[0x53b0u] = 0x74u; audio_rom[0x53b1u] = 0x00u;
        audio_rom[0x7400u] = 0x7fu; audio_rom[0x7403u] = 0x00u;
        audio_rom[0x7404u] = 0x00u; audio_rom[0x7405u] = 0xffu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 1u;
        descriptor[2] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x20u;
        {
            const uint8_t packet[4] = {0x81u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x10d0u);
        EXPECT_TRUE(dispatch.channel == 1u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD + 1u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[0x2515u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* 80 head with desc[4] = 1 (S2b: base 0x01 -- the old
           word-add pin 0x0f read desc[5] = 0 into the base).
           desc[2] = 15 takes the LOW arm (E1g): sel 15 staged
           to the same synthetic row, walk stops at e0,
           $1516 = 0x00ff, $1515 = 0x00; miss-all on the channel
           (15 vs 0), completes and consumes. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x53e6u] = 0x00u; audio_rom[0x53e7u] = 0x60u;
        audio_rom[0x53e8u] = 0x74u; audio_rom[0x53e9u] = 0x00u;
        audio_rom[0x7400u] = 0x7fu; audio_rom[0x7403u] = 0x00u;
        audio_rom[0x7404u] = 0x00u; audio_rom[0x7405u] = 0xffu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 15u;
        descriptor[2] = 15u;
        descriptor[4] = 1u;
        descriptor[5] = 0u;
        {
            const uint8_t packet[4] = {0x8fu, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x10d0u);
        EXPECT_TRUE(dispatch.channel == 15u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x01u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_WORD + 1u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[0x2515u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* B0 entries 0x7d/0x7e are table-default (dead 0x1c0a): consume
           with no voice effects. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 0x03u;
        {
            const uint8_t packet[4] = {0xb3u, 0x7du, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.selected_value == 0x7du);
        EXPECT_TRUE(dispatch.voices_cleared == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 0x03u;
        {
            const uint8_t packet[4] = {0xb3u, 0x7eu, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.selected_value == 0x7eu);
        EXPECT_TRUE(dispatch.voices_cleared == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* Unlisted class ignore (S2b): a 0xd3 command takes the
           switch rts at 0x856 -- no 90-body entry, no trap, no
           $150f/$1510 writes. Consumed, handled. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 3u;
        descriptor[2] = 0x03u;
        {
            const uint8_t packet[4] = {0xd3u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.command_class == 0xd0u);
        EXPECT_TRUE(dispatch.handler_address == 0x856u);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);
    }
    {
        /* Boot mirror (v0794a): voice slot words, stream records,
           shared slots, SCSP constants. */
        size_t voice = 0u;
        uint8_t level = 0u;
        uint16_t word = 0u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        for (voice = 0u; voice < 32u; ++voice) {
            uint8_t *record = board.sound_ram +
                VF2_SOUND_VOICE_TABLE_BASE +
                voice * VF2_SOUND_VOICE_BYTES;
            EXPECT_TRUE(record[0u] == (uint8_t)(((uint16_t)voice * 0x20u) >> 8u));
            EXPECT_TRUE(record[1u] ==
                        (uint8_t)(((uint16_t)voice * 0x20u) & 0xffu));
            EXPECT_TRUE(record[0x0cu] == 0u && record[0x0du] == 0u &&
                        record[0x0eu] == 0u && record[0x0fu] == 0u);
        }
        for (voice = 0u; voice < 32u; ++voice) {
            uint8_t *record = board.sound_ram + 0x2000u + voice * 0x20u;
            EXPECT_TRUE(record[0xeu] == 0u);
            EXPECT_TRUE(record[0xfu] == (uint8_t)(1u + voice / 8u));
            EXPECT_TRUE(record[0x10u] ==
                        (uint8_t)(((uint8_t)voice * 0x20u) & 0xffu));
            EXPECT_TRUE(record[0x11u] == 0u);
            EXPECT_TRUE(record[0x12u] == (uint8_t)((uint8_t)voice * 2u));
            EXPECT_TRUE(record[0x16u] ==
                        (uint8_t)(8u + (uint8_t)voice * 8u));
        }
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_FACTOR_151F] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_FACTOR_151F] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 8u] ==
                    0xefu);
        word = (uint16_t)((uint16_t)board.sound_ram[0x251au] << 8u) |
            board.sound_ram[0x251bu];
        EXPECT_TRUE(word == 0x0000u);
        EXPECT_TRUE(board.sound_ram[0x251cu] == 0x28u);
        EXPECT_TRUE(board.sound_ram[0x251du] == 0x00u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x402u, &word) == VF2_OK);
        EXPECT_TRUE(word == 0x138u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x418u, &word) == VF2_OK);
        EXPECT_TRUE(word == 0xceu);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x41au, &word) == VF2_OK);
        EXPECT_TRUE(word == 0x3c0u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x422u, &word) == VF2_OK);
        EXPECT_TRUE(word == 0x40u);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x00u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0x10u);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x3edu, &level) == VF2_OK);
        EXPECT_TRUE(level == 0xffu);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x17u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0x5au);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x700u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0x5au);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x701u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0u);
    }
    {
        /* Fail-closed init: a short ROM refuses instead of booting. */
        static const uint8_t tiny_rom[0x100u] = {0u};
        EXPECT_TRUE(vf2_sound_board_initialize(&board, tiny_rom, sizeof(tiny_rom),
                                   sample_rom, sizeof(sample_rom)) ==
                    VF2_ERROR_OUT_OF_BOUNDS);
    }
    {
        /* Boot descriptor precompute (v0794a): desc[10] from desc[3]. */
        uint8_t *descriptor = NULL;
        audio_rom[0x7103u] = 0x80u;
        audio_rom[0x710au] = 0x55u;
        audio_rom[0x7113u] = 0x00u;
        audio_rom[0x711au] = 0x77u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        EXPECT_TRUE(descriptor[3u] == 0x80u);
        EXPECT_TRUE(descriptor[0x0au] == 0x80u);
        EXPECT_TRUE(descriptor[0x13u] == 0x00u);
        EXPECT_TRUE(descriptor[0x1au] == 0x77u);
        audio_rom[0x7103u] = 0x00u;
        audio_rom[0x710au] = 0x00u;
        audio_rom[0x7113u] = 0x00u;
        audio_rom[0x711au] = 0x00u;
    }
    {
        /* Sample uploader (v0794a, re-pinned v0822): staging, record
           copy, guard. Init always runs the upload (boot d0 = 0);
           there is no skip-by-negative-max under true semantics. */
        /* test_boot_rom clears 0x10010 bytes; the board is initialised with a 0x10000-byte ROM. */
        static uint8_t rich_rom_storage[0x10010u];
        uint8_t *const rich_rom = rich_rom_storage;
        uint8_t *slot_table = NULL;
        test_boot_rom(rich_rom);
        /* Five staged bytes (first_count 4 -> dbf+1); the 5th
           (rom[0x5b2c] = 0xee, doubling as entry 0's length high
           byte) survives at $5004 -- the record target clobbers
           $5000..3. Entry 0 stays sample-mapped so init completes. */
        rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x04u;
        rich_rom[0x5b28u] = 0x00u; rich_rom[0x5b29u] = 0x80u;
        rich_rom[0x5b2au] = 0x00u; rich_rom[0x5b2bu] = 0x00u;
        rich_rom[0x5b2cu] = 0xeeu; rich_rom[0x5b2du] = 0x00u;
        rich_rom[0x5b2eu] = 0xfeu; rich_rom[0x5b2fu] = 0x02u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, UINT32_C(0x10000),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        EXPECT_TRUE(board.sound_ram[0x5000u] == 0x80u);
        EXPECT_TRUE(board.sound_ram[0x5001u] == 0x05u);
        EXPECT_TRUE(board.sound_ram[0x5002u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x5003u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x5004u] == 0xeeu);
        EXPECT_TRUE(board.sound_ram[0x5008u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[0x5009u] == 0x80u);
        EXPECT_TRUE(board.sound_ram[0x500au] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x500bu] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x50000u] == 0xa5u);
        EXPECT_TRUE(board.sound_ram[0x50001u] == 0x5au);
        /* Two-record upload (row 0x607800, count 1, selectors 0,0;
           entry 0 = {source 0x607400 deadbeef, 4-byte copy, stored
           0x11223344}). */
        rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x00u;
        rich_rom[0x5b28u] = 0x00u; rich_rom[0x5b29u] = 0x60u;
        rich_rom[0x5b2au] = 0x74u; rich_rom[0x5b2bu] = 0x00u;
        rich_rom[0x5b2cu] = 0x00u; rich_rom[0x5b2du] = 0x00u;
        rich_rom[0x5b2eu] = 0xfeu; rich_rom[0x5b2fu] = 0x04u;
        rich_rom[0x5b30u] = 0x11u; rich_rom[0x5b31u] = 0x22u;
        rich_rom[0x5b32u] = 0x33u; rich_rom[0x5b33u] = 0x44u;
        rich_rom[0x8df0u] = 0x00u; rich_rom[0x8df1u] = 0x00u;
        rich_rom[0x8df2u] = 0x00u; rich_rom[0x8df3u] = 0x60u;
        rich_rom[0x8df4u] = 0x78u; rich_rom[0x8df5u] = 0x00u;
        rich_rom[0x7800u] = 0x00u; rich_rom[0x7801u] = 0x01u;
        rich_rom[0x7802u] = 0x00u; rich_rom[0x7803u] = 0x00u;
        rich_rom[0x7804u] = 0x00u; rich_rom[0x7805u] = 0x00u;
        rich_rom[0x7400u] = 0xdeu; rich_rom[0x7401u] = 0xadu;
        rich_rom[0x7402u] = 0xbeu; rich_rom[0x7403u] = 0xefu;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, UINT32_C(0x10000),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        EXPECT_TRUE(board.sound_ram[0x5008u] == 0x10u);
        EXPECT_TRUE(board.sound_ram[0x5009u] == 0xc1u);
        EXPECT_TRUE(board.sound_ram[0x500au] == 0xbfu);
        EXPECT_TRUE(board.sound_ram[0x500bu] == 0x44u);
        EXPECT_TRUE(board.sound_ram[0x5000u] == 0x80u);
        slot_table = board.sound_ram + 0x5000u;
        EXPECT_TRUE(slot_table[1u] == 0x05u && slot_table[2u] == 0x00u &&
                    slot_table[3u] == 0x04u);
        EXPECT_TRUE(board.sound_ram[0x50000u] == 0xdeu);
        EXPECT_TRUE(board.sound_ram[0x50001u] == 0xadu);
        EXPECT_TRUE(board.sound_ram[0x50002u] == 0xbeu);
        EXPECT_TRUE(board.sound_ram[0x50003u] == 0xefu);
        EXPECT_TRUE(board.sound_ram[0x50004u] == 0xdeu);
        EXPECT_TRUE(board.sound_ram[0x50005u] == 0xadu);
        EXPECT_TRUE(board.sound_ram[0x50006u] == 0xbeu);
        EXPECT_TRUE(board.sound_ram[0x50007u] == 0xefu);
        /* Guard trip (5 x 0x8000-byte records cross $6FFFF). Entry 0
           keeps phase 2's source; only the length grows. */
        rich_rom[0x7800u] = 0x00u; rich_rom[0x7801u] = 0x04u;
        rich_rom[0x7802u] = 0x00u; rich_rom[0x7803u] = 0x00u;
        rich_rom[0x7804u] = 0x00u; rich_rom[0x7805u] = 0x00u;
        rich_rom[0x7806u] = 0x00u; rich_rom[0x7807u] = 0x00u;
        rich_rom[0x7808u] = 0x00u; rich_rom[0x7809u] = 0x00u;
        rich_rom[0x5b2cu] = 0x00u; rich_rom[0x5b2du] = 0x00u;
        rich_rom[0x5b2eu] = 0x7eu; rich_rom[0x5b2fu] = 0x00u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, UINT32_C(0x10000),
                                   sample_rom, sizeof(sample_rom)) ==
                    VF2_ERROR_UNSUPPORTED);
        /* Record 0's copy (deadbeef from entry 0's 0x607400 source)
           landed before the 5th record tripped the guard. */
        EXPECT_TRUE(board.sound_ram[0x50000u] == 0xdeu);
    }
    test_pcm_walker_full_page();
    test_pcm_walker_short_loop_page();
    test_pcm_walker_short_release();
    test_pcm_walker_short_terminal();
    test_pcm_stream_page_native();
    test_render_first_tone();
    test_ad21_b_class_drain();
    test_ad21_ba_does_not_stage();
    test_ad21_ba07_sample_copy();
    test_ad21_ba2a_sample_copy();
    return failures == 0 ? 0 : 1;
}
