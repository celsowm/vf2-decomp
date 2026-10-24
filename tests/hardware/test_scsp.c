#include "vf2/scsp.h"

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

int main(void)
{
    vf2_scsp scsp;
    uint8_t samples[64];
    uint8_t sample_copy[4];
    uint8_t value = 0u;
    uint16_t register_value = 0u;
    int16_t left[8];
    int16_t right[8];
    int16_t release_left[600];
    int16_t release_right[600];
    size_t index = 0u;

    memset(samples, 0, sizeof(samples));
    vf2_scsp_initialize(&scsp);
    EXPECT_TRUE(vf2_scsp_attach_sample_ram(&scsp, samples, sizeof(samples)) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0000u, UINT16_C(0x1234)) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_read_u16(&scsp, 0x0000u, &register_value) == VF2_OK);
    EXPECT_TRUE(register_value == UINT16_C(0x1234));
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0ffeu, UINT16_C(0x5678)) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_read_u16(&scsp, 0x0ffeu, &register_value) == VF2_OK);
    EXPECT_TRUE(register_value == UINT16_C(0x5678));
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0001u, 0u) == VF2_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(vf2_scsp_write_sample(&scsp, 4u, "VF2!", 4u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_read_sample(&scsp, 4u, sample_copy, sizeof(sample_copy)) == VF2_OK);
    EXPECT_TRUE(memcmp(sample_copy, "VF2!", sizeof(sample_copy)) == 0);
    EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, 0xa5u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_read_u16(&scsp, 0x0404u, &register_value) == VF2_OK);
    EXPECT_TRUE((register_value & 0xffu) == 0xa5u);
    EXPECT_TRUE(vf2_scsp_midi_empty(&scsp));

    for (index = 0u; index < VF2_SCSP_MIDI_FIFO_BYTES - 1u; ++index) {
        EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, (uint8_t)index) == VF2_OK);
    }
    EXPECT_TRUE(vf2_scsp_midi_full(&scsp));
    EXPECT_TRUE(vf2_scsp_midi_receive(&scsp, 0xffu) == VF2_ERROR_OUT_OF_BOUNDS);
    EXPECT_TRUE(vf2_scsp_read_u16(&scsp, 0x0420u, &register_value) == VF2_OK);
    EXPECT_TRUE((register_value & VF2_SCSP_PENDING_MIDI) != 0u);
    for (index = 0u; index < VF2_SCSP_MIDI_FIFO_BYTES - 1u; ++index) {
        EXPECT_TRUE(vf2_scsp_midi_read(&scsp, &value) == VF2_OK);
        EXPECT_TRUE(value == (uint8_t)index);
    }
    EXPECT_TRUE(vf2_scsp_midi_empty(&scsp));
    EXPECT_TRUE(vf2_scsp_read_u16(&scsp, 0x0420u, &register_value) == VF2_OK);
    EXPECT_TRUE((register_value & VF2_SCSP_PENDING_MIDI) == 0u);

    vf2_scsp_reset(&scsp);
    EXPECT_TRUE(vf2_scsp_read_u16(&scsp, 0x0000u, &register_value) == VF2_OK);
    EXPECT_TRUE(register_value == 0u);
    EXPECT_TRUE(vf2_scsp_read_sample(&scsp, 4u, sample_copy, sizeof(sample_copy)) == VF2_OK);
    EXPECT_TRUE(memcmp(sample_copy, "VF2!", sizeof(sample_copy)) == 0);

    /* SCSP official timer formula (FF - TIMA)*2^TACTL.
     * Original VF2 sound ROM writes #0x00CE to SCSP 0x0418
     * and #0x0040 to SCSP 0x0422 on L2 interrupt entry. */
    {
        uint32_t frames = UINT32_C(0xcafe);
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x00ce), &frames) == VF2_OK);
        EXPECT_TRUE(frames == 49u);
        EXPECT_TRUE(UINT32_C(44100) / frames == 900u);
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x00fe), &frames) == VF2_OK);
        EXPECT_TRUE(frames == 1u);
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x0000), &frames) == VF2_OK);
        EXPECT_TRUE(frames == 255u);
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x03ce), &frames) == VF2_OK);
        EXPECT_TRUE(frames == 392u);
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x07fe), &frames) == VF2_OK);
        EXPECT_TRUE(frames == 128u);
        frames = UINT32_C(0xcafe);
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x00ff), &frames) == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(frames == UINT32_C(0xcafe));
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x08ce), &frames) == VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(frames == UINT32_C(0xcafe));
        EXPECT_TRUE(vf2_scsp_timer_a_interval_frames(
            UINT16_C(0x00ce), NULL) == VF2_ERROR_INVALID_ARGUMENT);
    }

    samples[0] = 0x40u;
    samples[1] = 0x60u;
    samples[2] = 0x20u;
    samples[3] = 0xe0u;
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0004u, 0u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0006u, 4u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x000cu, 0u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0010u, 0u) == VF2_OK);
    /* DISDL=7, DIPAN=1f is full LEFT, not full right. */
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0016u, 0xff00u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0000u, 0x0830u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_render(&scsp, left, right, 8u) == VF2_OK);
    EXPECT_TRUE(left[0] > 0);
    EXPECT_TRUE(right[0] == 0);
    EXPECT_TRUE(scsp.slot_active[0] != 0u);
    EXPECT_TRUE(scsp.slot_address[0] < (4u << 12u));

    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0006u, 0u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0000u, 0x0010u) == VF2_OK);
    EXPECT_TRUE(scsp.slot_active[0] != 0u);
    EXPECT_TRUE(vf2_scsp_render(
        &scsp, release_left, release_right,
        sizeof(release_left) / sizeof(release_left[0])
    ) == VF2_OK);
    EXPECT_TRUE(scsp.slot_active[0] == 0u);

    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0000u, 0x0010u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0006u, 2u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&scsp, 0x0000u, 0x0810u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_render(&scsp, left, right, 8u) == VF2_OK);
    EXPECT_TRUE(scsp.slot_active[0] == 0u);

    /* SEGA SCSP 4.2.7 direct mixer: DIPAN 00 is true stereo center,
     * 13 attenuates right by 9dB, 0f mutes left, DISDL 0 mutes both.
     * Use a fresh slot so none of the release-state tests are reused. */
    {
        vf2_scsp mix;
        vf2_scsp_initialize(&mix);
        EXPECT_TRUE(vf2_scsp_attach_sample_ram(
            &mix, samples, sizeof(samples)) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0004u, 0u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0006u, 4u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x000cu, 0u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0010u, 0u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0016u, 0x6000u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0000u, 0x0830u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_render(&mix, left, right, 8u) == VF2_OK);
        EXPECT_TRUE(left[0] > 0 && left[0] == right[0]);

        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0016u, 0x7300u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_render(&mix, left, right, 8u) == VF2_OK);
        EXPECT_TRUE(left[0] > right[0] && right[0] > 0);

        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0016u, 0x6f00u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_render(&mix, left, right, 8u) == VF2_OK);
        EXPECT_TRUE(left[0] == 0 && right[0] > 0);

        EXPECT_TRUE(vf2_scsp_write_u16(&mix, 0x0016u, 0x0000u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_render(&mix, left, right, 8u) == VF2_OK);
        EXPECT_TRUE(left[0] == 0 && right[0] == 0);
    }

    /* Akira AD 10 04: the original sound CPU retunes already-active
     * voices. A stale KEYON-latched slot_step changed their notes'
     * pitch but NOT their playback speed (audible rotor/chopping).
     * Writes to either byte of OCT/FNS must retune immediately,
     * without resetting the playback cursor or ADSR. */
    {
        vf2_scsp voice;
        uint32_t position_before;
        int32_t envelope_before;
        vf2_scsp_initialize(&voice);
        EXPECT_TRUE(vf2_scsp_attach_sample_ram(
            &voice, samples, sizeof(samples)) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&voice, 0x0006u, 32u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&voice, 0x0010u, 0u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&voice, 0x0016u, 0xe000u) == VF2_OK);
        EXPECT_TRUE(vf2_scsp_write_u16(&voice, 0x0000u, 0x0830u) == VF2_OK);
        EXPECT_TRUE(voice.slot_active[0] == 1u);
        EXPECT_TRUE(voice.slot_step[0] == 4096u);
        EXPECT_TRUE(vf2_scsp_render(&voice, left, right, 1u) == VF2_OK);
        position_before = voice.slot_address[0];
        envelope_before = voice.slot_envelope[0];

        EXPECT_TRUE(vf2_scsp_write_u16(&voice, 0x0010u, 0x7800u) == VF2_OK);
        EXPECT_TRUE(voice.slot_step[0] == 2048u);
        EXPECT_TRUE(voice.slot_address[0] == position_before);
        EXPECT_TRUE(voice.slot_envelope[0] == envelope_before);
        EXPECT_TRUE(voice.slot_active[0] == 1u);
        EXPECT_TRUE(vf2_scsp_render(&voice, left, right, 1u) == VF2_OK);
        EXPECT_TRUE(voice.slot_address[0] == position_before + 2048u);

        position_before = voice.slot_address[0];
        EXPECT_TRUE(vf2_scsp_write_u8(&voice, 0x0010u, 0x00u) == VF2_OK);
        EXPECT_TRUE(voice.slot_step[0] == 4096u);
        EXPECT_TRUE(voice.slot_address[0] == position_before);
        EXPECT_TRUE(vf2_scsp_write_u8(&voice, 0x0010u, 0x78u) == VF2_OK);
        EXPECT_TRUE(voice.slot_step[0] == 2048u);

        /* Also handle byte-image writes from the original M68K bus,
         * since this harness can mutate mapped SCSP registers directly. */
        voice.registers[0x0010u] = 0u;
        EXPECT_TRUE(vf2_scsp_render(&voice, left, right, 1u) == VF2_OK);
        EXPECT_TRUE(voice.slot_step[0] == 4096u);
    }

    return failures == 0 ? 0 : 1;
}
