#include "vf2/scsp.h"

#include <string.h>

#define VF2_SCSP_FIXED_SHIFT 12u
#define VF2_SCSP_FIXED_ONE (UINT32_C(1) << VF2_SCSP_FIXED_SHIFT)
#define VF2_SCSP_KEYONB UINT16_C(0x0800)
#define VF2_SCSP_PCM8B UINT16_C(0x0010)
#define VF2_SCSP_SLOT_ENVELOPE_MAX 32767

enum {
    VF2_SCSP_ENVELOPE_ATTACK = 0,
    VF2_SCSP_ENVELOPE_DECAY1,
    VF2_SCSP_ENVELOPE_DECAY2,
    VF2_SCSP_ENVELOPE_RELEASE
};

static uint16_t read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static void write_be16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8u);
    data[1] = (uint8_t)value;
}

static uint16_t slot_word(const vf2_scsp *scsp, unsigned slot, unsigned offset)
{
    return read_be16(scsp->registers + slot * VF2_SCSP_SLOT_BYTES + offset);
}

static uint32_t slot_start(const vf2_scsp *scsp, unsigned slot)
{
    const uint16_t control = slot_word(scsp, slot, 0u);
    return ((uint32_t)(control & 0x000fu) << 16u) |
           slot_word(scsp, slot, 2u);
}

static uint32_t slot_step(const vf2_scsp *scsp, unsigned slot)
{
    const uint16_t pitch = slot_word(scsp, slot, 0x10u);
    const unsigned fns = pitch & 0x03ffu;
    int octave = (int)((pitch >> 11u) & 0x0fu);
    uint32_t step = fns + 0x0400u;

    octave = (octave ^ 8) - 8 + 2;
    if (octave >= 0) {
        step <<= (unsigned)octave;
    } else {
        step >>= (unsigned)(-octave);
    }
    return step == 0u ? 1u : step;
}

static void synchronize_slot(vf2_scsp *scsp, unsigned slot)
{
    const uint16_t control = slot_word(scsp, slot, 0u);
    const int key_on = (control & VF2_SCSP_KEYONB) != 0u;

    if (key_on && !scsp->slot_active[slot]) {
        scsp->slot_active[slot] = 1u;
        scsp->slot_backward[slot] = 0u;
        scsp->slot_address[slot] = 0u;
        scsp->slot_step[slot] = slot_step(scsp, slot);
        scsp->slot_envelope[slot] = 0;
        scsp->slot_envelope_state[slot] = VF2_SCSP_ENVELOPE_ATTACK;
    } else if (key_on && scsp->slot_active[slot] &&
               scsp->slot_envelope_state[slot] == VF2_SCSP_ENVELOPE_RELEASE) {
        scsp->slot_address[slot] = 0u;
        scsp->slot_envelope[slot] = 0;
        scsp->slot_envelope_state[slot] = VF2_SCSP_ENVELOPE_ATTACK;
    } else if (!key_on && scsp->slot_active[slot]) {
        scsp->slot_envelope_state[slot] = VF2_SCSP_ENVELOPE_RELEASE;
    }

    /* PITCH is live, not a one-time KEYON latch. The original 68000
     * rewrites OCT/FNS while a slot is active (Akira music, AD 10 04).
     * Recalculate the playback increment without resetting sample
     * position, envelope or key-on state. Also covers 68000 bus paths
     * that update the register byte image directly. */
    if (scsp->slot_active[slot]) {
        scsp->slot_step[slot] = slot_step(scsp, slot);
    }
}

static void advance_envelope(vf2_scsp *scsp, unsigned slot)
{
    const uint16_t attack_decay = slot_word(scsp, slot, 8u);
    const uint16_t sustain_release = slot_word(scsp, slot, 10u);
    const unsigned attack = attack_decay & 0x001fu;
    const unsigned decay1 = (attack_decay >> 6u) & 0x001fu;
    const unsigned decay2 = (attack_decay >> 11u) & 0x001fu;
    const unsigned release = sustain_release & 0x001fu;
    const unsigned decay_level = (sustain_release >> 5u) & 0x001fu;
    const int32_t sustain = (int32_t)(31u - decay_level) * 1024;
    int32_t step = 0;

    switch (scsp->slot_envelope_state[slot]) {
    case VF2_SCSP_ENVELOPE_ATTACK:
        step = (int32_t)(attack + 1u) * 256;
        scsp->slot_envelope[slot] += step;
        if (scsp->slot_envelope[slot] >= VF2_SCSP_SLOT_ENVELOPE_MAX) {
            scsp->slot_envelope[slot] = VF2_SCSP_SLOT_ENVELOPE_MAX;
            scsp->slot_envelope_state[slot] = VF2_SCSP_ENVELOPE_DECAY1;
        }
        break;
    case VF2_SCSP_ENVELOPE_DECAY1:
        step = (int32_t)(decay1 + 1u) * 64;
        scsp->slot_envelope[slot] -= step;
        if (scsp->slot_envelope[slot] <= sustain) {
            scsp->slot_envelope[slot] = sustain;
            scsp->slot_envelope_state[slot] = VF2_SCSP_ENVELOPE_DECAY2;
        }
        break;
    case VF2_SCSP_ENVELOPE_DECAY2:
        step = (int32_t)(decay2 + 1u) * 32;
        scsp->slot_envelope[slot] -= step;
        if (scsp->slot_envelope[slot] <= sustain) {
            scsp->slot_envelope[slot] = sustain;
        }
        break;
    case VF2_SCSP_ENVELOPE_RELEASE:
    default:
        step = (int32_t)(release + 1u) * 64;
        scsp->slot_envelope[slot] -= step;
        if (scsp->slot_envelope[slot] <= 0) {
            scsp->slot_envelope[slot] = 0;
            scsp->slot_active[slot] = 0u;
        }
        break;
    }
}

static vf2_status read_sample_byte(
    const vf2_scsp *scsp,
    uint32_t address,
    uint8_t *value
)
{
    if (scsp->sample_ram != NULL && (size_t)address < scsp->sample_ram_size) {
        *value = scsp->sample_ram[address];
        return VF2_OK;
    }
    if (scsp->sample_rom != NULL && (size_t)address < scsp->sample_rom_size) {
        *value = scsp->sample_rom[address];
        return VF2_OK;
    }
    return VF2_ERROR_OUT_OF_BOUNDS;
}

static int16_t read_sample_pcm16(const vf2_scsp *scsp, uint32_t address)
{
    uint8_t high = 0u;
    uint8_t low = 0u;
    if (read_sample_byte(scsp, address, &high) != VF2_OK ||
        read_sample_byte(scsp, address + 1u, &low) != VF2_OK) {
        return 0;
    }
    return (int16_t)(((uint16_t)high << 8u) | low);
}

static int16_t slot_sample_at(
    const vf2_scsp *scsp,
    unsigned slot,
    uint32_t address
)
{
    const uint16_t control = slot_word(scsp, slot, 0u);
    const uint32_t start = slot_start(scsp, slot);
    uint8_t value = 0u;

    if ((control & VF2_SCSP_PCM8B) != 0u) {
        if (read_sample_byte(scsp, start + address, &value) != VF2_OK) {
            return 0;
        }
        return (int16_t)((int32_t)(int8_t)value * INT32_C(256));
    }
    return read_sample_pcm16(scsp, start + (address & ~1u));
}

static int16_t slot_interpolated_sample(
    const vf2_scsp *scsp,
    unsigned slot
)
{
    const uint16_t control = slot_word(scsp, slot, 0u);
    const uint32_t address = scsp->slot_address[slot];
    const uint32_t sample_address = (control & VF2_SCSP_PCM8B) != 0u
        ? address >> VF2_SCSP_FIXED_SHIFT
        : (address >> (VF2_SCSP_FIXED_SHIFT - 1u)) & ~1u;
    const uint32_t fraction = address & (VF2_SCSP_FIXED_ONE - 1u);
    const int32_t first = slot_sample_at(scsp, slot, sample_address);
    const int32_t second = slot_sample_at(
        scsp,
        slot,
        sample_address + ((control & VF2_SCSP_PCM8B) != 0u ? 1u : 2u)
    );
    const int64_t mixed = (int64_t)first * (VF2_SCSP_FIXED_ONE - fraction) +
                          (int64_t)second * fraction;
    return (int16_t)(mixed >> VF2_SCSP_FIXED_SHIFT);
}

static void advance_slot(vf2_scsp *scsp, unsigned slot)
{
    const uint16_t control = slot_word(scsp, slot, 0u);
    const unsigned loop = (control >> 5u) & 3u;
    const uint32_t loop_start = slot_word(scsp, slot, 4u);
    const uint32_t loop_end = slot_word(scsp, slot, 6u);
    uint32_t position;

    if (scsp->slot_backward[slot]) {
        scsp->slot_address[slot] -= scsp->slot_step[slot];
    } else {
        scsp->slot_address[slot] += scsp->slot_step[slot];
    }
    position = scsp->slot_address[slot] >> VF2_SCSP_FIXED_SHIFT;

    if (loop == 0u) {
        if (loop_end != 0u && position >= loop_end) {
            scsp->slot_active[slot] = 0u;
        }
    } else if (loop == 1u) {
        if (loop_end != 0u && position >= loop_end) {
            scsp->slot_address[slot] =
                (loop_start + (position - loop_end)) << VF2_SCSP_FIXED_SHIFT;
        }
    } else {
        if (!scsp->slot_backward[slot] && loop_end != 0u &&
            position >= loop_end) {
            scsp->slot_backward[slot] = 1u;
        } else if (scsp->slot_backward[slot] && position <= loop_start) {
            scsp->slot_backward[slot] = 0u;
        }
    }
}

/* SCSP MIXER (SEGA User's Manual 4.2.7):
 * DISDL bits 15:13 are 0=mute, 1=-36dB ... 7=0dB.
 * DIPAN bits 12:8 are NOT linear 0=left, 31=right:
 * 00/10=center; bit4 selects the attenuated channel and
 * bits 3:0 give 3dB attenuation steps, 0xF=mute.
 * Q15 coefficients are rounded from 10^(-3*n/20), n=0..14.
 * This models only the direct (dry) route, not DSP/wet output.
 */
static const int32_t scsp_pan_atten_q15[16] = {
    32768, 23198, 16423, 11627, 8231, 5827, 4125, 2920,
    2068, 1464, 1036, 734, 519, 368, 260, 0
};

static int32_t clamp_sample(int32_t value)
{
    if (value < -32768) {
        return -32768;
    }
    if (value > 32767) {
        return 32767;
    }
    return value;
}

static void update_midi_pending(vf2_scsp *scsp)
{
    uint16_t pending = read_be16(scsp->registers + 0x0420u);
    if (vf2_scsp_midi_empty(scsp)) {
        pending = (uint16_t)(pending & (uint16_t)~VF2_SCSP_PENDING_MIDI);
    } else {
        pending = (uint16_t)(pending | VF2_SCSP_PENDING_MIDI);
    }
    write_be16(scsp->registers + 0x0420u, pending);
}

/* SCSP User's Manual §4.2.11: for 44.1kHz sampled time,
 * TACTL=0..7 increments every 1,2,4,..128 PCM frames.
 * A timer loaded with FE expires after ONE increment:
 * FF (0xFF) - FE (0xFE), NOT 256 - FE.
 * This does not synthesize pending IRQs or CPU instruction cycles. */
vf2_status vf2_scsp_timer_a_interval_frames(
    uint16_t tima_word, uint32_t *interval_frames
)
{
    uint32_t increment_count;
    uint32_t prescale;
    if (interval_frames == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((tima_word & UINT16_C(0xf800)) != 0u ||
        (tima_word & UINT16_C(0x00ff)) == UINT16_C(0x00ff)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    increment_count = UINT32_C(255) - (tima_word & UINT16_C(255));
    prescale = UINT32_C(1) << ((tima_word >> 8u) & UINT16_C(7));
    *interval_frames = increment_count * prescale;
    return VF2_OK;
}

void vf2_scsp_initialize(vf2_scsp *scsp)
{
    if (scsp != NULL) {
        memset(scsp, 0, sizeof(*scsp));
    }
}

void vf2_scsp_reset(vf2_scsp *scsp)
{
    uint8_t *sample_ram = NULL;
    size_t sample_ram_size = 0u;
    const uint8_t *sample_rom = NULL;
    size_t sample_rom_size = 0u;
    if (scsp == NULL) {
        return;
    }
    sample_ram = scsp->sample_ram;
    sample_ram_size = scsp->sample_ram_size;
    sample_rom = scsp->sample_rom;
    sample_rom_size = scsp->sample_rom_size;
    memset(scsp, 0, sizeof(*scsp));
    scsp->sample_ram = sample_ram;
    scsp->sample_ram_size = sample_ram_size;
    scsp->sample_rom = sample_rom;
    scsp->sample_rom_size = sample_rom_size;
}

vf2_status vf2_scsp_attach_sample_ram(
    vf2_scsp *scsp,
    uint8_t *sample_ram,
    size_t sample_ram_size
)
{
    if (scsp == NULL || sample_ram == NULL || sample_ram_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    scsp->sample_ram = sample_ram;
    scsp->sample_ram_size = sample_ram_size;
    return VF2_OK;
}

vf2_status vf2_scsp_attach_sample_rom(
    vf2_scsp *scsp,
    const uint8_t *sample_rom,
    size_t sample_rom_size
)
{
    if (scsp == NULL || sample_rom == NULL || sample_rom_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    scsp->sample_rom = sample_rom;
    scsp->sample_rom_size = sample_rom_size;
    return VF2_OK;
}

vf2_status vf2_scsp_read_u16(
    vf2_scsp *scsp,
    uint32_t address,
    uint16_t *value
)
{
    if (scsp == NULL || value == NULL || (address & 1u) != 0u ||
        address > VF2_SCSP_REGISTER_BYTES - 2u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address == 0x0404u && !vf2_scsp_midi_empty(scsp)) {
        uint8_t midi_value = 0u;
        vf2_status status = vf2_scsp_midi_read(scsp, &midi_value);
        if (status != VF2_OK) {
            return status;
        }
        *value = (uint16_t)((read_be16(scsp->registers + address) & 0xff00u) |
                            midi_value);
    } else {
        *value = read_be16(scsp->registers + address);
    }
    return VF2_OK;
}

vf2_status vf2_scsp_write_u16(
    vf2_scsp *scsp,
    uint32_t address,
    uint16_t value
)
{
    if (scsp == NULL || (address & 1u) != 0u ||
        address > VF2_SCSP_REGISTER_BYTES - 2u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* SCIPD and MCIPD are status registers. Only the CPU interrupt bit is
     * writable in the recovered host boundary. */
    if (address == 0x0420u || address == 0x042eu) {
        uint16_t current = read_be16(scsp->registers + address);
        write_be16(scsp->registers + address,
                   (uint16_t)(current | (value & 0x0020u)));
    } else {
        write_be16(scsp->registers + address, value);
    }
    if (address < VF2_SCSP_MASTER_BASE &&
        (address / VF2_SCSP_SLOT_BYTES) < VF2_SCSP_SLOT_COUNT &&
        ((address % VF2_SCSP_SLOT_BYTES) <= 1u ||
         (address % VF2_SCSP_SLOT_BYTES) == 0x10u)) {
        synchronize_slot(scsp, address / VF2_SCSP_SLOT_BYTES);
    }
    return VF2_OK;
}

vf2_status vf2_scsp_read_u8(
    vf2_scsp *scsp,
    uint32_t address,
    uint8_t *value
)
{
    if (scsp == NULL || value == NULL || address >= VF2_SCSP_REGISTER_BYTES) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address == 0x0405u && !vf2_scsp_midi_empty(scsp)) {
        return vf2_scsp_midi_read(scsp, value);
    }
    *value = scsp->registers[address];
    return VF2_OK;
}

vf2_status vf2_scsp_write_u8(
    vf2_scsp *scsp,
    uint32_t address,
    uint8_t value
)
{
    if (scsp == NULL || address >= VF2_SCSP_REGISTER_BYTES) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address == 0x0420u || address == 0x042eu) {
        scsp->registers[address] = (uint8_t)(scsp->registers[address] |
                                             (value & 0x20u));
    } else {
        scsp->registers[address] = value;
    }
    if (address < VF2_SCSP_MASTER_BASE &&
        (address / VF2_SCSP_SLOT_BYTES) < VF2_SCSP_SLOT_COUNT &&
        ((address % VF2_SCSP_SLOT_BYTES) <= 1u ||
         (address % VF2_SCSP_SLOT_BYTES) == 0x10u ||
         (address % VF2_SCSP_SLOT_BYTES) == 0x11u)) {
        synchronize_slot(scsp, address / VF2_SCSP_SLOT_BYTES);
    }
    return VF2_OK;
}

vf2_status vf2_scsp_midi_receive(vf2_scsp *scsp, uint8_t value)
{
    if (scsp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (vf2_scsp_midi_full(scsp)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    scsp->midi_input[scsp->midi_write_index] = value;
    scsp->midi_write_index = (uint8_t)((scsp->midi_write_index + 1u) &
                                       (VF2_SCSP_MIDI_FIFO_BYTES - 1u));
    update_midi_pending(scsp);
    return VF2_OK;
}

vf2_status vf2_scsp_midi_read(vf2_scsp *scsp, uint8_t *value)
{
    if (scsp == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (vf2_scsp_midi_empty(scsp)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = scsp->midi_input[scsp->midi_read_index];
    scsp->midi_read_index = (uint8_t)((scsp->midi_read_index + 1u) &
                                      (VF2_SCSP_MIDI_FIFO_BYTES - 1u));
    update_midi_pending(scsp);
    return VF2_OK;
}

int vf2_scsp_midi_empty(const vf2_scsp *scsp)
{
    return scsp == NULL || scsp->midi_read_index == scsp->midi_write_index;
}

int vf2_scsp_midi_full(const vf2_scsp *scsp)
{
    return scsp != NULL &&
           (uint8_t)((scsp->midi_write_index + 1u) &
                     (VF2_SCSP_MIDI_FIFO_BYTES - 1u)) == scsp->midi_read_index;
}

vf2_status vf2_scsp_read_sample(
    const vf2_scsp *scsp,
    uint32_t address,
    void *destination,
    size_t size
)
{
    const size_t available = scsp == NULL ? 0u :
        (scsp->sample_ram != NULL ? scsp->sample_ram_size : scsp->sample_rom_size);
    if (scsp == NULL || destination == NULL ||
        (size_t)address > available || size > available - (size_t)address) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (size != 0u && scsp->sample_ram == NULL && scsp->sample_rom == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(destination,
           scsp->sample_ram != NULL ? scsp->sample_ram + address :
                                       scsp->sample_rom + address,
           size);
    return VF2_OK;
}

vf2_status vf2_scsp_write_sample(
    vf2_scsp *scsp,
    uint32_t address,
    const void *source,
    size_t size
)
{
    if (scsp == NULL || source == NULL || scsp->sample_ram == NULL ||
        (size_t)address > scsp->sample_ram_size ||
        size > scsp->sample_ram_size - (size_t)address) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    memcpy(scsp->sample_ram + address, source, size);
    return VF2_OK;
}

vf2_status vf2_scsp_render(
    vf2_scsp *scsp,
    int16_t *left,
    int16_t *right,
    size_t frames
)
{
    size_t frame;

    if (scsp == NULL || (frames != 0u && (left == NULL || right == NULL))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (frame = 0u; frame < frames; ++frame) {
        int32_t mix_left = 0;
        int32_t mix_right = 0;
        unsigned slot;

        for (slot = 0u; slot < VF2_SCSP_SLOT_COUNT; ++slot) {
            const uint16_t level = slot_word(scsp, slot, 0x0cu);
            const uint16_t pan_word = slot_word(scsp, slot, 0x16u);
            const unsigned pan = (pan_word >> 8u) & 0x1fu;
            const unsigned direct_level = (pan_word >> 13u) & 7u;
            const int32_t pan_atten = scsp_pan_atten_q15[pan & 0x0fu];
            int32_t sample;
            int32_t dry;
            int32_t left_gain;
            int32_t right_gain;

            synchronize_slot(scsp, slot);
            if (!scsp->slot_active[slot]) {
                continue;
            }
            sample = slot_interpolated_sample(scsp, slot);
            advance_envelope(scsp, slot);
            if ((level & 0x0100u) == 0u) {
                sample = (sample * (VF2_SCSP_SLOT_ENVELOPE_MAX -
                                    (int32_t)(level & 0xffu) * 128)) /
                         VF2_SCSP_SLOT_ENVELOPE_MAX;
            }
            sample = (sample * scsp->slot_envelope[slot]) /
                     VF2_SCSP_SLOT_ENVELOPE_MAX;
            /* Apply the owner's slot-level dry attenuation BEFORE
             * summing multiple simultaneous voices. Reducing only
             * the final clipped WAV cannot restore lost peaks. */
            dry = direct_level == 0u ? 0 :
                  sample / (int32_t)(1u << (7u - direct_level));
            left_gain = (pan & 0x10u) != 0u ? 32768 : pan_atten;
            right_gain = (pan & 0x10u) != 0u ? pan_atten : 32768;
            mix_left += (dry * left_gain) / 32768;
            mix_right += (dry * right_gain) / 32768;
            advance_slot(scsp, slot);
        }
        left[frame] = (int16_t)clamp_sample(mix_left);
        right[frame] = (int16_t)clamp_sample(mix_right);
    }
    return VF2_OK;
}
