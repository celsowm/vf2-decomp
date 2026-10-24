#ifndef VF2_SCSP_H
#define VF2_SCSP_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

enum {
    /* 68000 address map recovered from the Model 2 sound board. */
    VF2_SCSP_SOUND_RAM_BASE = 0x000000u,
    VF2_SCSP_CPU_BASE = 0x100000u,
    VF2_SCSP_CONTROL_BASE = 0x400000u,
    VF2_SCSP_AUDIO_ROM_BASE = 0x600000u,
    VF2_SCSP_SAMPLE_ROM_BASE = 0x800000u,
    VF2_SCSP_REGISTER_BYTES = 0x1000u,
    VF2_SCSP_SLOT_COUNT = 32u,
    VF2_SCSP_SLOT_BYTES = 0x20u,
    VF2_SCSP_MASTER_BASE = 0x0400u,
    VF2_SCSP_MASTER_BYTES = 0x0200u,
    VF2_SCSP_RING_BASE = 0x0600u,
    VF2_SCSP_RING_BYTES = 0x0100u,
    /* DSP register surface: COEF (64 words) + MADRS (32 words) + zero
       pad + MPRO (128 steps x 4 words). The boot uploads a real
       effect program here (v0826). */
    VF2_SCSP_DSP_BASE = 0x0700u,
    VF2_SCSP_DSP_BYTES = 0x0500u,
    VF2_SCSP_MIDI_FIFO_BYTES = 32u,
    VF2_SCSP_PENDING_MIDI = 0x0008u
};

typedef struct vf2_scsp {
    uint8_t registers[VF2_SCSP_REGISTER_BYTES];
    uint8_t *sample_ram;
    size_t sample_ram_size;
    const uint8_t *sample_rom;
    size_t sample_rom_size;
    uint8_t midi_input[VF2_SCSP_MIDI_FIFO_BYTES];
    uint8_t midi_read_index;
    uint8_t midi_write_index;
    uint32_t slot_address[VF2_SCSP_SLOT_COUNT];
    uint32_t slot_step[VF2_SCSP_SLOT_COUNT];
    int32_t slot_envelope[VF2_SCSP_SLOT_COUNT];
    uint8_t slot_active[VF2_SCSP_SLOT_COUNT];
    uint8_t slot_envelope_state[VF2_SCSP_SLOT_COUNT];
    uint8_t slot_backward[VF2_SCSP_SLOT_COUNT];
} vf2_scsp;

/* Timer A is an 8-bit UP counter. Its low-byte reload
 * reaches FF in (0xFF - reload) increments, at one increment
 * every 2^TACTL SCSP 44.1kHz sample frames.
 *
 * This PURE formula does not assert/clear IRQ or advance an M68K.
 * Use a real register image + IRQ source/cadence verification before
 * scheduling the driver. FF has no finite countdown. */
vf2_status vf2_scsp_timer_a_interval_frames(
    uint16_t tima_word, uint32_t *interval_frames
);

void vf2_scsp_initialize(vf2_scsp *scsp);
void vf2_scsp_reset(vf2_scsp *scsp);

vf2_status vf2_scsp_attach_sample_ram(
    vf2_scsp *scsp,
    uint8_t *sample_ram,
    size_t sample_ram_size
);

vf2_status vf2_scsp_attach_sample_rom(
    vf2_scsp *scsp,
    const uint8_t *sample_rom,
    size_t sample_rom_size
);

vf2_status vf2_scsp_read_u16(
    vf2_scsp *scsp,
    uint32_t address,
    uint16_t *value
);

vf2_status vf2_scsp_read_u8(
    vf2_scsp *scsp,
    uint32_t address,
    uint8_t *value
);

vf2_status vf2_scsp_write_u16(
    vf2_scsp *scsp,
    uint32_t address,
    uint16_t value
);

vf2_status vf2_scsp_write_u8(
    vf2_scsp *scsp,
    uint32_t address,
    uint8_t value
);

vf2_status vf2_scsp_midi_receive(vf2_scsp *scsp, uint8_t value);
vf2_status vf2_scsp_midi_read(vf2_scsp *scsp, uint8_t *value);
int vf2_scsp_midi_empty(const vf2_scsp *scsp);
int vf2_scsp_midi_full(const vf2_scsp *scsp);

vf2_status vf2_scsp_read_sample(
    const vf2_scsp *scsp,
    uint32_t address,
    void *destination,
    size_t size
);

vf2_status vf2_scsp_write_sample(
    vf2_scsp *scsp,
    uint32_t address,
    const void *source,
    size_t size
);

/* Render the active PCM slots into signed 16-bit stereo output. */
vf2_status vf2_scsp_render(
    vf2_scsp *scsp,
    int16_t *left,
    int16_t *right,
    size_t frames
);

#endif
