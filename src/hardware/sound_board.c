#include "vf2/sound_board.h"
#include "sound_a0_descriptor.h"
#include "sound_l2_ad.h"

#include <string.h>

/* ROM table addressed by the 0x174c B0 handler's PC-relative lookup at
   audio-CPU address 0x179e. */

static uint16_t read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24u) |
           ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) |
           (uint32_t)data[3];
}

static void write_be16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8u);
    data[1] = (uint8_t)value;
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24u);
    data[1] = (uint8_t)(value >> 16u);
    data[2] = (uint8_t)(value >> 8u);
    data[3] = (uint8_t)value;
}

static vf2_status sound_board_key_off(
    vf2_sound_board *board,
    const uint8_t *voice
)
{
    /* ROM L1-ISR key portion (0x636/0x684): d1 = voice[0], the slot
       word (slot*0x20, v0794-proven, used directly as the SCSP/RAM
       stride with no masking). SCSP[slot+0x0b] = 0x1f (max release
       rate) and sound_ram[0x2000+slot] = 0. No slot-control
       read-modify-write exists on ROM; the earlier control-bit form
       came from a misread of indexed move.b #imm as absolute. */
    const uint16_t slot_word = read_be16(voice + VF2_SOUND_VOICE_SLOT);
    const uint32_t scsp_address = (uint32_t)slot_word + 0x0bu;
    const uint32_t ram_address =
        UINT32_C(0x2000) + (uint32_t)slot_word;
    if (vf2_scsp_write_u8(&board->scsp, scsp_address,
                          UINT8_C(0x1f)) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (ram_address >= VF2_SOUND_RAM_BYTES) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    board->sound_ram[ram_address] = 0u;
    return VF2_OK;
}

static vf2_status read_rom_byte(
    const uint8_t *rom,
    size_t rom_size,
    uint32_t offset,
    uint8_t *value
)
{
    if (rom == NULL || (size_t)offset >= rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = rom[offset];
    return VF2_OK;
}

static vf2_status read_audio_be32(
    const vf2_sound_board *board,
    uint32_t address,
    uint32_t *value
)
{
    const uint32_t offset = address - VF2_SOUND_AUDIO_ROM_BASE;

    if (board == NULL || value == NULL || address < VF2_SOUND_AUDIO_ROM_BASE ||
        (size_t)offset + 4u > board->audio_rom_size ||
        board->audio_rom == NULL) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = ((uint32_t)board->audio_rom[offset] << 24u) |
             ((uint32_t)board->audio_rom[offset + 1u] << 16u) |
             ((uint32_t)board->audio_rom[offset + 2u] << 8u) |
             board->audio_rom[offset + 3u];
    return VF2_OK;
}

/* Forward: table readers defined below dispatch_80/90 (v0825
 * S2c), used by the HIGH row picks above them. */
static vf2_status sound_board_read_table_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
);

static vf2_status sound_board_read_table_word(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
);

static vf2_status read_audio_byte_absolute(
    const vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
);

static vf2_status read_audio_be16(
    const vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t high = 0u;
    uint8_t low = 0u;

    if (value == NULL || read_audio_byte_absolute(board, address, &high) != VF2_OK ||
        read_audio_byte_absolute(board, address + 1u, &low) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = (uint16_t)(((uint16_t)high << 8u) | low);
    return VF2_OK;
}

static vf2_status read_audio_byte_absolute(
    const vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
)
{
    if (address < VF2_SOUND_AUDIO_ROM_BASE) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    return read_rom_byte(
        board->audio_rom, board->audio_rom_size,
        address - VF2_SOUND_AUDIO_ROM_BASE, value
    );
}

/* ROM 0x1ac0 (B0 entry 0x3a) / 0x422 (boot) sample uploader,
   re-recovered v0822. Both entries converge on one body; only the
   prologues differ (B0: SR set + payload 0x7f arm; boot: d0 = 0).
   Silicon shape (byte-verified):
   - first-copy: d7 = word@0x605b26, `dbf` copies d7+1 bytes
     0x605b28+i -> $5000+i (the C's old `d7 bytes` was short one);
   - B0 0x7f arm: `beq` targets the 0x1b46 epilogue (SR restore +
     rts), a clean exit -- the old "mid-dbf trap" was +4 damage;
   - row pick: d1 = max word@row_base, `cmp.w d1,d0; bgt` exits
     clean (B0 never modeled this gate); else row = long at
     row_base+2+d0*4 (the C's direct be32(row_base) read the max
     word AS a pointer -- wrong mechanism and address);
   - records: d7 = row word, `dbf` runs count+1 records;
   - entries at the CONSTANT base 0x605b28 + selector*0x14 (the C's
     `table = be32(0x605b28)` mistook entry 0's source field for
     the base -- they coincide numerically in real ROM only);
   - stored slot word is the FULL `stored - source` long (the C's
     low-byte mask was invented);
   - inner copy runs (length+0x1ff).w+1 bytes; $6FFFF guard halts.
   Row/selector words address through the bus: a RAM-mapped row
   pointer (real boot row 0 IS rom, but the reader must not assume)
   reads RAM; unmapped addresses fail closed like the stepper. */
static vf2_status read_bus_be16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    if (board == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address + 1u < VF2_SOUND_RAM_BYTES) {
        *value = read_be16(board->sound_ram + address);
        return VF2_OK;
    }
    return read_audio_be16(board, address, value);
}

static vf2_status sound_board_sample_upload(
    vf2_sound_board *board,
    uint32_t row_table_base,
    int check_7f,
    uint8_t payload
)
{
    uint16_t first_count = 0u;
    uint16_t record_total = 0u;
    uint16_t record_index = 0u;
    uint16_t max_index = 0u;
    uint16_t d0 = 0u;
    uint32_t row = 0u;
    /* a2 is the fixed $5000 sample-slot table (absolute short). */
    uint32_t slot_base = UINT32_C(0x5000);
    uint32_t target = 0u;
    size_t index = 0u;

    if (read_audio_be16(board, UINT32_C(0x605b26), &first_count) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM dbf: d7+1 iterations (0x1ad0-0x1ad2 / 0x42e-0x430). */
    for (index = 0u; index <= (size_t)first_count; ++index) {
        uint8_t staged = 0u;
        if (read_audio_byte_absolute(
                board, UINT32_C(0x605b28) + (uint32_t)index, &staged
            ) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (UINT32_C(0x5000) + index >= VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[0x5000u + index] = staged;
    }
    if (check_7f && (payload & 0x7fu) == 0x7fu) {
        /* ROM 0x1ada-0x1ade: beq $1b46, the SR-restore/rts epilogue.
           Staged bytes persist; clean OK. */
        return VF2_OK;
    }
    /* ROM 0x1ae2-0x1af5 / 0x438-0x44b: d1 = max word, bgt exits. */
    if (read_audio_be16(board, row_table_base, &max_index) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    d0 = check_7f ? (uint16_t)(payload & 0x7fu) : 0u;
    if (d0 > max_index) {
        return VF2_OK;
    }
    if (read_audio_be32(board,
                        row_table_base + 2u + (uint32_t)d0 * 4u,
                        &row) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_bus_be16(board, row, &record_total) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    row += 2u;
    target = UINT32_C(0x50000);
    /* ROM dbf: count+1 records (0x1b42 / 0x498). */
    for (record_index = 0u; ; ++record_index) {
        uint16_t selector = 0u;
        uint32_t entry = 0u;
        uint32_t source = 0u;
        uint32_t length = 0u;
        uint32_t stored = 0u;
        uint16_t count = 0u;
        if (read_bus_be16(board, row, &selector) != VF2_OK) {
            return VF2_ERROR_UNSUPPORTED;
        }
        row += 2u;
        entry = (uint32_t)selector * UINT32_C(0x14);
        /* Entry structs live at the constant base 0x605b28 (a0);
           the +0/+4/+8 longs are source/length/stored. */
        if (read_audio_be32(board, UINT32_C(0x605b28) + entry,
                            &source) != VF2_OK ||
            read_audio_be32(board, UINT32_C(0x605b28) + entry + 8u,
                            &stored) != VF2_OK ||
            read_audio_be32(board, UINT32_C(0x605b28) + entry + 4u,
                            &length) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        length += UINT32_C(0x1ff);
        /* ROM sub.l d2,d3: full-long difference (no masking). */
        stored = stored - source;
        if (slot_base + 8u + entry + 4u > VF2_SOUND_RAM_BYTES ||
            slot_base + entry + 4u > VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        /* ROM 0x1b24: move.l d3,$08(a2,d1.w) -- entry-indexed, NOT
           a fixed slot (v0822: the old fixed-$6008 form collapsed
           every record's stored word onto one address). */
        write_be32(board->sound_ram + slot_base + 8u + entry, stored);
        write_be32(board->sound_ram + slot_base + entry, target);
        board->sound_ram[slot_base + entry] =
            (uint8_t)(board->sound_ram[slot_base + entry] | 0x80u);
        count = (uint16_t)(length & UINT32_C(0xffff));
        for (index = 0u; ; ++index) {
            uint8_t sample = 0u;
            vf2_status sample_status = VF2_OK;
            if (target >= VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            sample_status = vf2_sound_board_read_u8(
                board, source + (uint32_t)index, &sample);
            if (sample_status != VF2_OK) {
                /* Silicon bus error on unmapped sample reads; the
                   stepper refuses the same way (status-equivalence,
                   as the old P0c up-first contract). */
                return sample_status;
            }
            board->sound_ram[target] = sample;
            ++target;
            if (index >= count) {
                break;
            }
        }
        /* ROM cmpa.l #$6ffff,a3 / bgt halt: fail closed. */
        if (target > UINT32_C(0x6ffff)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (record_index >= record_total) {
            break;
        }
    }
    return VF2_OK;
}

vf2_status vf2_sound_board_initialize(
    vf2_sound_board *board,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
)
{
    static const uint16_t scsp_block[7] = {0u, 0xc8u, 0u, 0x2c8u,
                                           0x88u, 0x48u, 0u};
    uint32_t blob = 0u;
    uint32_t image = 0u;
    uint32_t return_table = 0u;
    size_t index = 0u;
    size_t voice = 0u;

    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(board, 0, sizeof(*board));
    board->audio_rom = audio_rom;
    board->audio_rom_size = audio_rom_size;
    board->sample_rom = sample_rom;
    board->sample_rom_size = sample_rom_size;
    vf2_scsp_initialize(&board->scsp);
    if (sample_rom != NULL && sample_rom_size != 0u) {
        (void)vf2_scsp_attach_sample_rom(&board->scsp,
                                          sample_rom, sample_rom_size);
    }
    /* ROM 0x28e SCSP init. $400001 writes are an unmodeled system
       port outside the audio contract; the four $0405 reads drain the
       MIDI-in buffer (MIBUF) and are skipped here. */
    if (vf2_scsp_write_u8(&board->scsp, UINT32_C(0x401), 0u) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be32(board, UINT32_C(0x604940), &blob) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* DSP effect-program upload (v0826): the 0x500-byte blob is the
       COEF (64 words) + MADRS (32 words) + zero pad + MPRO (128 steps
       x 4 words) image of the 0x700-0xbff DSP register window. The
       hardware starts the program when the burst crosses the MPRO
       word at 0xbf0; the model stores the bytes verbatim. */
    for (index = 0u; index < VF2_SCSP_DSP_BYTES; ++index) {
        uint8_t program_byte = 0u;
        if (read_audio_byte_absolute(board, blob + (uint32_t)index,
                                     &program_byte) !=
            VF2_OK ||
            vf2_scsp_write_u8(&board->scsp,
                              VF2_SCSP_DSP_BASE + (uint32_t)index,
                              program_byte) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    /* DSP delay-ring config: RBL = (v >> 7) & 3 = 2 -> 32K words,
       RBP = v & 0x3f = 0x38 -> ring at delay words 0x38000-0x3ffff. */
    if (vf2_scsp_write_u16(&board->scsp, UINT32_C(0x402), UINT16_C(0x138)) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < VF2_SCSP_SLOT_COUNT; ++index) {
        uint32_t slot = (uint32_t)index * VF2_SCSP_SLOT_BYTES;
        if (vf2_scsp_write_u8(&board->scsp, slot + 0x0bu, UINT8_C(0xff)) !=
            VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, slot + 0x0du, UINT8_C(0xff)) !=
            VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, slot, UINT8_C(0x10)) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    /* ROM 0x2d4-0x2f2 master block tail: lea $041c(a0),a0 then seven
       move.w #imm,(a0)+ stores (0, 0xc8, 0, 0x2c8, 0x88, 0x48, 0). The
       post-increment form is invisible to (d16,a5)-only scans. */
    for (index = 0u; index < 7u; ++index) {
        if (vf2_scsp_write_u16(&board->scsp,
                               UINT32_C(0x41c) + (uint32_t)index * 2u,
                               scsp_block[index]) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    if (vf2_scsp_write_u16(&board->scsp, UINT32_C(0x418), UINT16_C(0xce)) !=
        VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, UINT32_C(0x41a), UINT16_C(0x3c0)) !=
        VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, UINT32_C(0x422), UINT16_C(0x40)) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0x320: the 0x1000+0x3F00 clear is subsumed by the memset
       above; only $1526 = 0xef escapes it. */
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 8u] = 0xefu;
    /* ROM 0x336 command-descriptor images. */
    if (read_audio_be32(board, UINT32_C(0x603066), &image) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < UINT32_C(0xa0); ++index) {
        uint8_t staged = 0u;
        if (read_audio_byte_absolute(board, image + (uint32_t)index, &staged) !=
            VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + index] = staged;
    }
    if (read_audio_be32(board, UINT32_C(0x6030a6), &image) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < UINT32_C(0xa0); ++index) {
        uint8_t staged = 0u;
        if (read_audio_byte_absolute(board, image + (uint32_t)index, &staged) !=
            VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 0xa0u + index] =
            staged;
    }
    board->sound_ram[VF2_SOUND_SHARED_FACTOR_151F] = 0xffu;
    for (index = 0u; index < 20u; ++index) {
        uint8_t *descriptor = board->sound_ram +
            VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            index * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
        uint16_t scaled = (uint16_t)(((uint16_t)descriptor[3u] * 0xffu) >> 8u);
        /* ROM d3 = 0xff either way ($151f was just stored). */
        if ((scaled & 0xffu) != 0u) {
            descriptor[0x0au] = (uint8_t)(scaled + 1u);
        }
    }
    /* ROM $151a(a6): voice-table base pointer for the compactor. */
    write_be32(board->sound_ram + UINT32_C(0x251a), UINT32_C(0x2800));
    /* ROM 0x3a8 voice records: slot word = index*0x20, sample longs. */
    for (voice = 0u; voice < VF2_SOUND_VOICE_COUNT; ++voice) {
        uint8_t *record = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            voice * VF2_SOUND_VOICE_BYTES;
        uint16_t slot = 0u;
        uint32_t sample = 0u;
        if (read_audio_be16(board, UINT32_C(0x6006e6) + (uint32_t)voice * 2u,
                            &slot) != VF2_OK ||
            read_audio_be32(board, UINT32_C(0x600726) + (uint32_t)voice * 4u,
                            &sample) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be16(record + VF2_SOUND_VOICE_SLOT, slot);
        write_be32(record + VF2_SOUND_VOICE_SAMPLE, sample);
    }
    /* ROM 0x3cc stream records. */
    for (voice = 0u; voice < 32u; ++voice) {
        uint8_t *record = board->sound_ram + UINT32_C(0x2000) +
            voice * UINT32_C(0x20);
        write_be32(record + 0x0eu,
                   UINT32_C(0x10000) + (uint32_t)voice * UINT32_C(0x2000));
        record[0x12u] = (uint8_t)((uint8_t)voice * 2u);
        record[0x16u] = (uint8_t)(8u + (uint8_t)voice * 8u);
    }
    /* ROM 0x402: effect-return EFSDL/EFPAN (slot byte 0x17, bits 5-7
       and 0-4) for slots 0-15, armed from the 0x605368 blob0. This is
       the return path the DSP program's four EFREG channels feed. */
    if (read_audio_be32(board, UINT32_C(0x60536a), &return_table) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < 16u; ++index) {
        uint8_t level = 0u;
        if (read_audio_byte_absolute(board, return_table + (uint32_t)index,
                                     &level) != VF2_OK ||
            vf2_scsp_write_u8(&board->scsp,
                              UINT32_C(0x17) + (uint32_t)index *
                                  VF2_SCSP_SLOT_BYTES,
                              level) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
    }
    /* ROM 0x422 boot staging: d0 = 0, same max-gate + indexed pick
       (0x438-0x44b). The old high-word gate matched the skip outcome
       on its fixture by coincidence; the unified gate covers it. */
    return sound_board_sample_upload(board, UINT32_C(0x608df0), 0, 0u);
}

/* ROM 0xCB2..0xD72: 16 unrolled triples (48 instructions total).
 * Per triple: MOVEM.L (A0)+,D0-D7 reads 32 consecutive sample bytes;
 * MOVEM.L D0-D7,(A2) writes the same 32 bytes to sound RAM;
 * ADDA.W #32,A2 advances the destination. The two regions cannot
 * overlap for this proven sample-ROM -> sound-RAM corridor.
 * Preserve the unmodeled post-0xD72 frontier in dispatch_next. */
vf2_status vf2_sound_board_copy_engine_page_512(
    vf2_sound_board *board,
    uint32_t *source,
    uint32_t *destination
)
{
    uint32_t sample_offset;
    size_t i;
    enum { COPY_BYTES = 512u, MOVEM_BLOCK_BYTES = 32u };
    if (board == NULL || source == NULL || destination == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (((*source | *destination) & UINT32_C(1)) != 0u ||
        *source < VF2_SOUND_SAMPLE_ROM_BASE ||
        *destination > VF2_SOUND_RAM_BYTES ||
        board->sample_rom == NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    sample_offset = *source - VF2_SOUND_SAMPLE_ROM_BASE;
    if ((uint64_t)sample_offset + COPY_BYTES > board->sample_rom_size ||
        (uint64_t)*destination + COPY_BYTES > VF2_SOUND_RAM_BYTES) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* The original destination A2 does not postincrement on MOVEM.
     * The explicit ADDA.W #32 supplies every 32-byte increment. */
    for (i = 0u; i < COPY_BYTES; i += MOVEM_BLOCK_BYTES) {
        memcpy(board->sound_ram + *destination + i,
               board->sample_rom + sample_offset + i,
               MOVEM_BLOCK_BYTES);
    }
    *source += COPY_BYTES;
    *destination += COPY_BYTES;
    return VF2_OK;
}

/* The original 0x2242 walker maintains 32 stream records at sound
 * RAM 0x2000 + slot*0x20. In the measured 0x80-active Akira case,
 * +2 and +6 contain the BE32 source/destination page cursors.
 * Only the independently recovered 512-byte page-and-cursor effect
 * is translated here; actual walker selection/timing stays original
 * 68000-only until its predicate is differentially verified. */
vf2_status vf2_sound_board_copy_stream_page_512(
    vf2_sound_board *board, size_t slot_index
)
{
    uint8_t *stream;
    uint32_t source;
    uint32_t destination;
    vf2_status result;
    if (board == NULL || slot_index >= VF2_SOUND_VOICE_COUNT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    stream = board->sound_ram + UINT32_C(0x2000) +
             slot_index * UINT32_C(0x20);
    if (stream[0] != UINT8_C(0x80)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    source = read_be32(stream + 2u);
    destination = read_be32(stream + 6u);
    result = vf2_sound_board_copy_engine_page_512(
        board, &source, &destination);
    if (result != VF2_OK) {
        return result;
    }
    write_be32(stream + 2u, source);
    write_be32(stream + 6u, destination);
    return VF2_OK;
}

/* ROM-anchored positive 0x2242 walker branch (v0905).
 * 2242 move.b (a0),d0 / BMI 224C; otherwise JMP 07A6
 * 224C move.w +1C,d0 / add.w +1A,d0 / BPL 2260
 *      negative stores the timer and JMP 07A6 (not modeled).
 * 2260 move.b +1,d2: 0 -> SCSP 0409 bit7 must be SET,
 *      use +0E as A2; 0x80 -> bit7 must be CLEAR, use +06;
 *      all other bytes -> +06 without this MIDI status test.
 * 22B8 movea.l +2,a1; 22BC move.l +0A,d5
 * 22C0 cmpi.l #512,d5 / BLS 23B0 (short-page arm held).
 * 22CA sub.l #512,d5; 22D0 addi.b #0x10,d2
 * 22D4 andi.w #0x7FFF,d0 -> +1C; 22E0 d5 -> +0A
 * 22E4..23A0 16x MOVEM.L (a1)+,d0-d7 / (a2) / adda #32
 * 23A4 a1 -> +2; 23A8 a2 -> +6.
 * 265C move.b +16(a0),0x0408(a5) on the shared exit.
 * Full-page branch only. The unused original path at 23B0 and
 * the 24BE/260C activation tails remain unsupported.
 * Side effects are transactional on this C entry. */
vf2_status vf2_sound_board_step_stream_full_page(
    vf2_sound_board *board, size_t slot_index
)
{
    uint8_t *record;
    uint16_t timer;
    uint8_t mode;
    uint8_t midi_status;
    uint32_t source;
    uint32_t destination;
    uint32_t remaining;
    vf2_status result;
    if (board == NULL || slot_index >= VF2_SOUND_VOICE_COUNT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    record = board->sound_ram + UINT32_C(0x2000) +
             slot_index * UINT32_C(0x20);
    if ((record[0] & UINT8_C(0x80)) == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    timer = (uint16_t)(read_be16(record + 0x1cu) +
                       read_be16(record + 0x1au));
    if ((timer & UINT16_C(0x8000)) != 0u) {
        /* The original negative timer arm writes +1C and returns to
         * the drain; this partial-C operation refuses atomically. */
        return VF2_ERROR_UNSUPPORTED;
    }
    mode = record[1u];
    result = vf2_scsp_read_u8(
        &board->scsp, UINT32_C(0x0409), &midi_status);
    if (result != VF2_OK) {
        return result;
    }
    if ((mode == 0u && (midi_status & UINT8_C(0x80)) == 0u) ||
        (mode == UINT8_C(0x80) &&
         (midi_status & UINT8_C(0x80)) != 0u)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    remaining = read_be32(record + 0x0au);
    if (remaining <= UINT32_C(512)) {
        /* 0x23B0 has different loop/short-page semantics. */
        return VF2_ERROR_UNSUPPORTED;
    }
    source = read_be32(record + 2u);
    destination = read_be32(record + (mode == 0u ? 0x0eu : 6u));
    result = vf2_sound_board_copy_engine_page_512(
        board, &source, &destination);
    if (result != VF2_OK) {
        return result;
    }
    record[1u] = (uint8_t)(mode + UINT8_C(0x10));
    write_be16(record + 0x1cu, (uint16_t)(timer & UINT16_C(0x7fff)));
    write_be32(record + 0x0au, remaining - UINT32_C(512));
    write_be32(record + 2u, source);
    write_be32(record + 6u, destination);
    /* ROM 0x23AC branches to 0x265C, where the original CPU
     * writes descriptor +0x16 to SCSP master byte 0x0408. */
    return vf2_scsp_write_u8(&board->scsp, UINT32_C(0x0408),
                             record[0x16u]);
}

/* ROM 0x23B0..0x24B8: terminal/looping 512-byte page, +0x01
 * descriptor flag only. Decode directly from owner ROM:
 *   23B0 BTST #0,(A0); BEQ 24BC (unsupported here)
 *   23BA NOT.W D5; 23BC ADDI.W #0x201,D5;
 *   23C0 ANDI.L #0xFFFF,D5        => 512 - remaining
 *   23C6 MOVE.L +0x12(A0),D4; ANDI.L #0x00FFFFFF,D4;
 *   23D0 ADD.L D5,D4               => next source
 *   23D2 MOVE.L +0x16(A0),D3; ANDI.L #0x00FFFFFF,D3;
 *   23DC SUB.L D5,D3               => next remaining
 *   23DE ADDI.B #0x10,D2; 23E2 ANDI.W #0x7FFF,D0
 *   23E6..23F2 write timer,+2,+1,+0xA;
 *   23F6..24B2 16x MOVEM.L 32 bytes from OLD source (A1)
 *   24B4 MOVE.L A2,+6; 24B8 BRA 0x265C shared SCSP exit.
 * The full 512-byte transfer is intentional even if the remaining
 * counter is shorter: it uses the current source A1, not computed D4.
 * Only guarded memory is accessed and writes commit after success.
 * Overflow/underflow in D4/D3 are modulo-32-bit as on 68000. */
vf2_status vf2_sound_board_step_stream_short_loop_page(
    vf2_sound_board *board, size_t slot_index
)
{
    uint8_t *record;
    uint16_t timer;
    uint8_t mode;
    uint8_t midi_status;
    uint32_t source;
    uint32_t destination;
    uint32_t remaining;
    uint32_t padding;
    uint32_t loop_source;
    uint32_t loop_remaining;
    vf2_status result;
    if (board == NULL || slot_index >= VF2_SOUND_VOICE_COUNT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    record = board->sound_ram + UINT32_C(0x2000) +
             slot_index * UINT32_C(0x20);
    if ((record[0] & UINT8_C(0x81)) != UINT8_C(0x81)) {
        /* 0x80-only active records branch to the 0x24BC tail. */
        return VF2_ERROR_UNSUPPORTED;
    }
    timer = (uint16_t)(read_be16(record + 0x1cu) +
                       read_be16(record + 0x1au));
    if ((timer & UINT16_C(0x8000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    mode = record[1u];
    result = vf2_scsp_read_u8(
        &board->scsp, UINT32_C(0x0409), &midi_status);
    if (result != VF2_OK) {
        return result;
    }
    if ((mode == 0u && (midi_status & UINT8_C(0x80)) == 0u) ||
        (mode == UINT8_C(0x80) &&
         (midi_status & UINT8_C(0x80)) != 0u)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    remaining = read_be32(record + 0x0au);
    if (remaining > UINT32_C(512)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    padding = UINT32_C(512) - remaining;
    loop_source = (read_be32(record + 0x12u) &
                   UINT32_C(0x00ffffff)) + padding;
    loop_remaining = (read_be32(record + 0x16u) &
                      UINT32_C(0x00ffffff)) - padding;
    source = read_be32(record + 2u);
    destination = read_be32(record + (mode == 0u ? 0x0eu : 6u));
    result = vf2_sound_board_copy_engine_page_512(
        board, &source, &destination);
    if (result != VF2_OK) {
        return result;
    }
    record[1u] = (uint8_t)(mode + UINT8_C(0x10));
    write_be16(record + 0x1cu, (uint16_t)(timer & UINT16_C(0x7fff)));
    write_be32(record + 2u, loop_source);
    write_be32(record + 0x0au, loop_remaining);
    write_be32(record + 6u, destination);
    return vf2_scsp_write_u8(&board->scsp, UINT32_C(0x0408),
                             record[0x16u]);
}

/* ROM 0xD72..0xD90: 7 instructions, stop BEFORE RTS at 0xD90.
 *   move.w (a1),d4; move.l a0,2(a3,d4.w)
 *   move.l a2,6(a3,d4.w); move.b (a5,d4.w),d5
 *   ori.b #0x18,d5; move.b d5,(a5,d4.w)
 *   move.b #0x80,(a3,d4.w).
 * A3=0x2000, A5=0x100000. Checked inputs represent a preceding
 * fully mapped 512-byte sample-page transfer, not arbitrary
 * newly invented game-state transitions. */
static vf2_status sound_board_finalize_engine_copy_tail(
    vf2_sound_board *board,
    size_t voice_index,
    uint32_t sample_end,
    uint32_t destination_end,
    uint8_t stream_flag
)
{
    const uint32_t stream_base = UINT32_C(0x2000);
    size_t voice;
    uint16_t channel_offset;
    uint32_t stream;
    uint8_t key;
    if (board == NULL || voice_index >= VF2_SOUND_VOICE_COUNT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    voice = VF2_SOUND_VOICE_TABLE_BASE +
            voice_index * VF2_SOUND_VOICE_BYTES;
    channel_offset = (uint16_t)(((uint16_t)board->sound_ram[voice] << 8u) |
                                board->sound_ram[voice + 1u]);
    /* The MC68000 index is a SIGNED D4.W. Only mapped 32 SCSP
     * channel offsets are admitted by this explicitly bounded API. */
    if ((channel_offset % VF2_SCSP_SLOT_BYTES) != 0u ||
        channel_offset / VF2_SCSP_SLOT_BYTES >= VF2_SCSP_SLOT_COUNT ||
        sample_end < VF2_SOUND_SAMPLE_ROM_BASE + UINT32_C(512) ||
        board->sample_rom == NULL ||
        (uint64_t)sample_end - VF2_SOUND_SAMPLE_ROM_BASE >
            board->sample_rom_size ||
        destination_end < UINT32_C(512) ||
        destination_end > VF2_SOUND_RAM_BYTES ||
        ((sample_end | destination_end) & UINT32_C(1)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    stream = stream_base + (uint32_t)channel_offset;
    if (stream > VF2_SOUND_RAM_BYTES - 10u ||
        vf2_scsp_read_u8(&board->scsp, channel_offset, &key) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* 68000 MOVE.L is big-endian; only after all guards pass. */
    write_be32(board->sound_ram + stream + 2u, sample_end);
    write_be32(board->sound_ram + stream + 6u, destination_end);
    key = (uint8_t)(key | UINT8_C(0x18));
    if (vf2_scsp_write_u8(&board->scsp, channel_offset, key) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    board->sound_ram[stream] = stream_flag;
    return VF2_OK;
}

/* Exact original 0xD72..0xD90 seven-instruction tail; stream=0x80. */
vf2_status vf2_sound_board_finalize_engine_copy_d72(
    vf2_sound_board *board,
    size_t voice_index,
    uint32_t sample_end,
    uint32_t destination_end
)
{
    return sound_board_finalize_engine_copy_tail(
        board, voice_index, sample_end, destination_end, UINT8_C(0x80));
}

/* Exact original 0xEB8..0xED6 seven-instruction tail; stream=0x81.
 * The alternate path has a separately recovered 16x MOVEM upload
 * at 0xDF8..0xEB8, sharing copy_engine_page_512 unchanged. */
vf2_status vf2_sound_board_finalize_engine_copy_eb8(
    vf2_sound_board *board,
    size_t voice_index,
    uint32_t sample_end,
    uint32_t destination_end
)
{
    return sound_board_finalize_engine_copy_tail(
        board, voice_index, sample_end, destination_end, UINT8_C(0x81));
}

vf2_status vf2_sound_board_read_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
)
{
    if (board == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address < VF2_SOUND_RAM_BYTES) {
        *value = board->sound_ram[address];
        return VF2_OK;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES) {
        return vf2_scsp_read_u8(&board->scsp,
                                address - VF2_SCSP_CPU_BASE, value);
    }
    if (address == VF2_SCSP_CONTROL_BASE ||
        address == VF2_SCSP_CONTROL_BASE + 1u) {
        *value = (uint8_t)(address == VF2_SCSP_CONTROL_BASE
            ? board->control >> 8u : board->control);
        return VF2_OK;
    }
    if (address >= VF2_SOUND_AUDIO_ROM_BASE &&
        address < VF2_SOUND_AUDIO_ROM_BASE + 0x080000u) {
        return read_rom_byte(board->audio_rom, board->audio_rom_size,
                             address - VF2_SOUND_AUDIO_ROM_BASE, value);
    }
    /* Sample window follows the ATTACHED size (v0822): the stepper
       maps SAMPLE_BASE + offset < sample_rom_size, and real upload
       sources reach ~0x23e226 past the old 2MB DIRECT cap. Capping
       at DIRECT_BYTES made the C refuse what silicon reads. */
    if (address >= VF2_SOUND_SAMPLE_ROM_BASE &&
        address - VF2_SOUND_SAMPLE_ROM_BASE < board->sample_rom_size) {
        return read_rom_byte(board->sample_rom, board->sample_rom_size,
                             address - VF2_SOUND_SAMPLE_ROM_BASE, value);
    }
    return VF2_ERROR_OUT_OF_BOUNDS;
}

vf2_status vf2_sound_board_write_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t value
)
{
    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address < VF2_SOUND_RAM_BYTES) {
        board->sound_ram[address] = value;
        return VF2_OK;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES) {
        return vf2_scsp_write_u8(&board->scsp,
                                 address - VF2_SCSP_CPU_BASE, value);
    }
    if (address == VF2_SCSP_CONTROL_BASE) {
        board->control = (uint16_t)((board->control & 0x00ffu) |
                                    ((uint32_t)value << 8u));
        return VF2_OK;
    }
    if (address == VF2_SCSP_CONTROL_BASE + 1u) {
        board->control = (uint16_t)((board->control & 0xff00u) | value);
        return VF2_OK;
    }
    return VF2_ERROR_OUT_OF_BOUNDS;
}

vf2_status vf2_sound_board_read_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t high = 0u;
    uint8_t low = 0u;
    vf2_status status = VF2_OK;
    if (board == NULL || value == NULL || (address & 1u) != 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES - 1u) {
        return vf2_scsp_read_u16(&board->scsp,
                                 address - VF2_SCSP_CPU_BASE, value);
    }
    status = vf2_sound_board_read_u8(board, address, &high);
    if (status == VF2_OK) {
        status = vf2_sound_board_read_u8(board, address + 1u, &low);
    }
    if (status == VF2_OK) {
        *value = (uint16_t)(((uint16_t)high << 8u) | low);
    }
    return status;
}

vf2_status vf2_sound_board_write_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t value
)
{
    vf2_status status = VF2_OK;
    if (board == NULL || (address & 1u) != 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (address >= VF2_SCSP_CPU_BASE &&
        address < VF2_SCSP_CPU_BASE + VF2_SCSP_REGISTER_BYTES - 1u) {
        return vf2_scsp_write_u16(&board->scsp,
                                  address - VF2_SCSP_CPU_BASE, value);
    }
    status = vf2_sound_board_write_u8(board, address, (uint8_t)(value >> 8u));
    if (status == VF2_OK) {
        status = vf2_sound_board_write_u8(board, address + 1u,
                                          (uint8_t)value);
    }
    return status;
}

vf2_status vf2_sound_board_maintain_voices(
    vf2_sound_board *board,
    vf2_sound_voice_maintenance_report *report
)
{
    vf2_sound_voice_maintenance_report local = {0};
    uint8_t *voices = NULL;
    size_t index = 0u;

    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* The ROM writes the SCSP timer/control words on entry (L1 ISR
       0x602: $041a = 0x3c0, $0422 = 0x80; the L2 driver pokes $0418 /
       $0422 per tick and is otherwise an unmodeled interrupt
       boundary). */
    if (vf2_scsp_write_u16(
            &board->scsp, VF2_SCSP_MASTER_BASE + 0x001au, 0x03c0u
        ) != VF2_OK ||
        vf2_scsp_write_u16(
            &board->scsp, VF2_SCSP_MASTER_BASE + 0x0022u, 0x0080u
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    local.counter_151e_before =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
    local.counter_152e_before =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
    voices = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    /* ROM L1 voice ISR 0x5fe (v0821, +2-base reaudit): a full 32-voice
       scan, stride 0x10, every voice visited exactly once. Per voice:
       skip unless bit0 of +2; decrement +0xb; on nonzero skip; bit3
       of +2 picks the release arm (0x684) or the normal arm (0x636).
       Both arms key off (d1 = voice word 0; SCSP[slot+0x0b] = 0x1f,
       ram[0x1000+slot] = 0), clear +2, snapshot d2 (normal: +9,
       release: +5), clear +9 (release also +5), decrement $151e /
       $152e, then rebalance all 32 voices (normal: +9 bytes != 0xff
       with mem > d2 decrement; release: +5 bytes of voices whose +9
       == 0xff with mem > d2 decrement). No traps: the ISR always
       reaches rte with a balanced stack (stepper 140 steps, a7
       balanced on every L1 leg).
       Byte domain (v0821 correction to the reaudit note): the
       rebalance compares are cmp.B (opcodes 0xB412/0xB42A, opmode
       000 = byte-verified), and d2 is loaded by move.b, so the ISR
       entry residue in d2's upper bytes never participates. The
       MAME d2-variance data stands; its "C must take d2 as input"
       conclusion is void. The C is deterministic over RAM. */
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        int release_path = 0;
        uint8_t age = 0u;
        size_t other = 0u;

        ++local.voices_scanned;
        if ((voice[VF2_SOUND_VOICE_FLAGS] & 1u) == 0u) {
            continue;
        }
        --voice[VF2_SOUND_VOICE_RATE];
        if (voice[VF2_SOUND_VOICE_RATE] != 0u) {
            continue;
        }
        release_path = (voice[VF2_SOUND_VOICE_FLAGS] & 8u) != 0u;
        if (sound_board_key_off(board, voice) != VF2_OK) {
            local.counter_151e_after =
                board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
            local.counter_152e_after =
                board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
            *report = local;
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        voice[VF2_SOUND_VOICE_FLAGS] = 0u;
        ++local.voices_expired;
        if (!release_path) {
            /* Normal arm 0x636-0x680. */
            age = voice[VF2_SOUND_VOICE_AGE];
            voice[VF2_SOUND_VOICE_AGE] = 0u;
            --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
            ++local.normal_expirations;
            for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
                uint8_t *target =
                    voices + other * VF2_SOUND_VOICE_BYTES + VF2_SOUND_VOICE_AGE;
                /* ROM 0x666-0x674: skip 0xff slots; bcc skips when
                   d2 >= mem (unsigned); else decrement. The expired
                   voice itself (now 0) always skips via bcc. */
                if (*target == UINT8_MAX || age >= *target) {
                    continue;
                }
                --(*target);
                ++local.voices_aged;
            }
        } else {
            /* Release arm 0x684-0x6d2. */
            age = voice[VF2_SOUND_VOICE_AGE_RELEASE];
            voice[VF2_SOUND_VOICE_AGE] = 0u;
            voice[VF2_SOUND_VOICE_AGE_RELEASE] = 0u;
            --board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
            ++local.release_expirations;
            for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
                uint8_t *slot9 =
                    voices + other * VF2_SOUND_VOICE_BYTES + VF2_SOUND_VOICE_AGE;
                uint8_t *slot5 =
                    voices + other * VF2_SOUND_VOICE_BYTES + VF2_SOUND_VOICE_AGE_RELEASE;
                /* ROM 0x6b6-0x6c8: only 0xff-aged slots; then the
                   same unsigned d2 >= mem skip; else decrement +5. */
                if (*slot9 != UINT8_MAX || age >= *slot5) {
                    continue;
                }
                --(*slot5);
                ++local.voices_aged;
            }
        }
    }
    local.counter_151e_after =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
    local.counter_152e_after =
        board->sound_ram[VF2_SOUND_SHARED_COUNTER_152E];
    *report = local;
    return VF2_OK;
}

/* L2 stream tick (ROM 0x1f16..0x1f7a, vector 26), measured v0852.
   Head (0x1f16): movem spill (frame 0x7ecc..0x7eff), then two
   unconditional SCSP pokes -- 0x0418 = 0x00ce and 0x0422 = 0x0040,
   the timer/control words the boot master tail 0x2f4 stages -- then
   the ring-count gate: a byte of 0xff at ram[0x2506] takes 0x1f2e's
   bcc past the whole body. Body (0x1f32): a2 = 0x3000 (a6 = 0x1000),
   prescaler jsr 0x21ba, then eight worker jsr 0x1f7c calls over
   records 0x3000 + i*0x20. Measured boot-state verdict (v0823 F10):
   boot clears 0x3000 and no ROM store ever arms it, so the
   prescaler returns at its [0xc] == 0 gate and all eight workers
   bail at their bit-7 gate -- 51 steps to the rte, no RAM delta,
   pokes the only state change. The recovered tick applies the pokes,
   the prescaler countdown ([0xc] != 0 -> [0xd]--, 8-bit wrap exact)
   and the worker countdowns (bit 7 set -> word[+2]--, 16-bit wrap
   exact, 0x1f88) while every countdown stays nonzero. A countdown
   of 1 is the handoff to an unmeasured body: the prescaler files
   either the 3-byte beat (be 10 <[0xe]> + cursor/count, 0x21e6-
   0x2206, [0xe] >= 4) or, with [0xe] < 4, the stream-end (be 50 7e
   00 + bit7 clear + [0xc] = 0, 0x2208-0x223c); the worker applier
   (0x1f92-0x1fba) reads stream bytes + the +1 latch and jumps the
   0x1fbc class table. The tick then refuses BEFORE mutating
   anything and names the record, fail closed. */
vf2_status vf2_sound_board_maintain_streams(
    vf2_sound_board *board,
    vf2_sound_stream_maintenance_report *report
)
{
    uint8_t *records = NULL;
    size_t index = 0u;
    size_t ad_index = SIZE_MAX;
    uint8_t ad_packets[6] = {0u};
    uint8_t ad21_packets[36] = {0u};
    int ad21_mode = 0;
    uint16_t ad_cursor = 0u;

    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *report = (vf2_sound_stream_maintenance_report){0};
    report->refused_record = 0xffu;
    report->write_cursor_before = read_be16(
        board->sound_ram + VF2_SOUND_COMMAND_WRITE_CURSOR
    );
    report->write_cursor_after = report->write_cursor_before;
    records = board->sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
    /* 0x1f26-0x1f2e: byte gate, == 0xff skips the body. */
    if (board->sound_ram[VF2_SOUND_COMMAND_COUNT] != 0xffu) {
        /* Preflight both live-body handoffs before any mutation. */
        if (records[0x0cu] != 0u && records[0x0du] == 1u) {
            report->timers_expired = 1u;
            report->refused_stage = 1u;
            report->refused_record = 0u;
            return VF2_ERROR_UNSUPPORTED;
        }
        for (index = 0u; index < 8u; ++index) {
            uint8_t *record = records + index * 0x20u;
            if ((record[0] & 0x80u) != 0u && read_be16(record + 2u) == 1u) {
                /* Original A0 12-byte streams: B handler at 0x1ffc,
                 * default 90 at 0x2080, FF 2F terminator 0x2192.
                 * Do not admit arbitrary worker bodies or more than
                 * one pending handoff per bounded tick. */
                uint32_t next = read_be32(record + 4u);
                uint8_t candidate[6] = {0u};
                ad_cursor = (uint16_t)(read_be16(board->sound_ram +
                          VF2_SOUND_COMMAND_WRITE_CURSOR) & 0x0fffu);
                if (index >= 1u && index <= 6u && ad_index == SIZE_MAX &&
                    (ad_cursor & 3u) == 0u &&
                    board->sound_ram[VF2_SOUND_COMMAND_COUNT] <= 253u &&
                    vf2_sound_l2_ad_expand(board->audio_rom,
                          board->audio_rom_size, next, candidate)) {
                    memcpy(ad_packets, candidate, sizeof(ad_packets));
                    ad_index = index;
                } else if (index == 1u && ad_index == SIZE_MAX &&
                    (ad_cursor & 3u) == 0u &&
                    board->sound_ram[VF2_SOUND_COMMAND_COUNT] <= 243u &&
                    vf2_sound_l2_ad21_expand(board->audio_rom,
                          board->audio_rom_size, next, ad21_packets)) {
                    /* Original AD21->B/CF record at 0x60A6B3. This
                     * is deliberately NOT the AD23/AD24 8F grammar.
                     * All 12 files must fit the ring and count,
                     * preflighted before mutating either. */
                    ad_index = index;
                    ad21_mode = 1;
                } else {
                    report->timers_expired = 1u;
                    report->refused_stage = 2u;
                    report->refused_record = (uint8_t)index;
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        }
        /* Measured body: ordinary countdowns, plus the bounded
         * 68000 B/90 two-packet AD stream expansion when admitted. */
        if (records[0x0cu] != 0u) {
            records[0x0du] = (uint8_t)(records[0x0du] - 1u);
        }
        for (index = 0u; index < 8u; ++index) {
            uint8_t *record = records + index * 0x20u;
            if ((record[0] & 0x80u) != 0u) {
                if (index == ad_index) {
                    size_t packet = 0u;
                    uint8_t *ring = board->sound_ram + VF2_SOUND_COMMAND_RING_BASE;
                    size_t packet_count = ad21_mode ? 12u : 2u;
                    const uint8_t *packets = ad21_mode ?
                        ad21_packets : ad_packets;
                    for (packet = 0u; packet < packet_count; ++packet) {
                        uint16_t at = (uint16_t)((ad_cursor +
                                                   (uint16_t)(4u * packet)) &
                                                  0x0fffu);
                        /* Original MOVE.B stores three packet bytes.
                         * The fourth sound-ring byte is untouched. */
                        ring[at] = packets[packet * 3u];
                        ring[at + 1u] = packets[packet * 3u + 1u];
                        ring[at + 2u] = packets[packet * 3u + 2u];
                    }
                    write_be16(board->sound_ram +
                               VF2_SOUND_COMMAND_WRITE_CURSOR,
                               (uint16_t)((ad_cursor +
                                    (uint16_t)(4u * packet_count)) & 0x0fffu));
                    board->sound_ram[VF2_SOUND_COMMAND_COUNT] =
                        (uint8_t)(board->sound_ram[VF2_SOUND_COMMAND_COUNT] +
                                  (uint8_t)packet_count);
                    /* 0x1f9c files EVERY negative stream class
                     * byte into descriptor[+1], including 0xff.
                     * The terminating ff 2f therefore leaves 0xff
                     * in +1 after 0x2192 clears status. Measured
                     * against the original 68000 at 0x2198. */
                    record[0] = 0u;
                    record[1] = UINT8_C(0xff);
                    write_be16(record + 2u, 0u);
                    report->timers_expired = 1u;
                    report->packets_emitted = packet_count;
                    /* 8F two-packet path has its original ten-byte
                     * accounting. The CF 12-packet stream count
                     * is deferred until the interpreter's exact
                     * per-byte accounting is independently traced. */
                    report->bytes_consumed = ad21_mode ? 0u : 10u;
                    report->write_cursor_after =
                        (uint16_t)((ad_cursor +
                                    (uint16_t)(4u * packet_count)) & 0x0fffu);
                } else {
                    write_be16(record + 2u,
                               (uint16_t)(read_be16(record + 2u) - 1u));
                }
            }
        }
        report->descriptors_scanned = 8u;
    }
    /* 0x1f1a/0x1f20: applied on every tick that is not refused (the
       gate skip still leaves the head's pokes applied). */
    if (vf2_scsp_write_u16(&board->scsp, UINT32_C(0x418), UINT16_C(0xce)) !=
            VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, UINT32_C(0x422), UINT16_C(0x40)) !=
            VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    return VF2_OK;
}

static vf2_status sound_board_clear_c0_voices(
    vf2_sound_board *board,
    uint8_t channel,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *voices = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    size_t index = 0u;

    /* ROM 0x1de0 head: channel control/table clear. d1 = channel
       ($150e&0xf = packet command&0xf, set by the drain). */
    board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel] = 0u;
    board->sound_ram[VF2_SOUND_CHANNEL_CONTROL_ALT_BASE + channel] = 0u;
    write_be16(board->sound_ram + VF2_SOUND_CHANNEL_TABLE_BASE +
               (size_t)channel * 2u, 0u);
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = voices + index * VF2_SOUND_VOICE_BYTES;
        uint16_t slot_word = 0u;
        uint32_t scsp_base = 0u;
        uint32_t stream_slot = 0u;
        if ((voice[VF2_SOUND_VOICE_FLAGS] & 0x40u) == 0u ||
            /* ROM 0x1e1c is cmp.B (like the 0x1332 scan match): the
               channel test is the single voice[4] byte. */
            voice[VF2_SOUND_VOICE_CHANNEL] != channel) {
            continue;
        }
        /* ROM 0x1e24-0x1e38 (V5 key form): d4 = voice[0], the slot
           word, used directly as the stride. SCSP[slot+0x0b] = 0x1f,
           SCSP[slot+0x00] = 0x10, sound_ram[0x2000+slot] = 0 (a3 =
           0x1000(a6)). The earlier absolute-write form came from the
           same indexed-#imm decoder misread as the key_off fiction.
           The $151e decrement is unconditional; there is no age
           rebalance on this path. */
        slot_word = read_be16(voice + VF2_SOUND_VOICE_SLOT);
        scsp_base = (uint32_t)slot_word;
        stream_slot = UINT32_C(0x2000) + (uint32_t)slot_word;
        if (vf2_scsp_write_u8(&board->scsp, scsp_base + 0x0bu,
                              UINT8_C(0x1f)) != VF2_OK ||
            vf2_scsp_write_u8(&board->scsp, scsp_base,
                              UINT8_C(0x10)) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (stream_slot >= VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[stream_slot] = 0u;
        voice[VF2_SOUND_VOICE_FLAGS] = 0u;
        voice[VF2_SOUND_VOICE_CHANNEL] = 0u;
        --board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E];
        ++report->voices_cleared;
    }
    return VF2_OK;
}

/* ROM 0x11d0 voice subroutine (v0825 E1f-c), called once per
   scan match with a1 = voice, d1.w = channel. Three exits, all
   rts (the caller continues its dbf): defer, key+L1-arm,
   teardown. d1 upper is scan-entry residue but every use reads
   .w/.b only (0x11d4 index, implicit), so the byte channel the
   C passes is exact. d4 = voice word 0 (slot stride, no mask).
   d2/d0 byte snapshots (move.b) feed cmp.b only -- upper
   residue never participates (v0821 byte-domain rule). */
static vf2_status sound_board_voice_sub_11d0(
    vf2_sound_board *board,
    uint8_t *voice,
    uint8_t channel,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint16_t slot = 0u;
    uint32_t stream = 0u;
    uint8_t countdown = 0u;
    int release_path = 0;
    size_t other = 0u;

    /* ROM 0x11d0-0x11e4: channel-table ($3220(a6) = 0x4220) bit7
       defers the voice (bset #1, no SCSP traffic). */
    if ((ram[VF2_SOUND_CHANNEL_CONTROL_BASE + channel] & 0x80u) !=
        0u) {
        voice[VF2_SOUND_VOICE_FLAGS] |= 0x02u;
        return VF2_OK;
    }
    slot = read_be16(voice + VF2_SOUND_VOICE_SLOT);
    countdown = voice[VF2_SOUND_VOICE_RATE];
    if (countdown != 0u) {
        /* ROM 0x11f2-0x11fe: slot key ($10 control, V1 form) +
           L1 service arm (bset #0). No key-on: 0x10 lacks
           KEYONB, same as every setup key. */
        if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                              UINT8_C(0x10)) != VF2_OK) {
            report->handler_address = UINT32_C(0x000011f2);
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        voice[VF2_SOUND_VOICE_FLAGS] |= 0x01u;
        return VF2_OK;
    }
    /* ROM 0x1200-0x120c: key-off pair (SCSP[slot+0x0b] = 0x1f,
       SCSP[slot] = 0x10), then bit3 dispatches on the
       scan-andi'd +2 (bit7 already clear, bit4/1 clear,
       low7 nonzero by the scan gates). */
    if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot + 0x0bu,
                          UINT8_C(0x1f)) != VF2_OK ||
        vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                          UINT8_C(0x10)) != VF2_OK) {
        report->handler_address = UINT32_C(0x00001200);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    release_path =
        (voice[VF2_SOUND_VOICE_FLAGS] & 0x08u) != 0u;
    stream = UINT32_C(0x2000) + (uint32_t)slot;
    if (stream >= VF2_SOUND_RAM_BYTES) {
        report->handler_address = UINT32_C(0x0000121e);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    ram[stream] = 0u;
    voice[VF2_SOUND_VOICE_FLAGS] = 0u;
    if (!release_path) {
        /* Normal teardown 0x1216-0x1260 (L1-normal shape): +9
           snapshot, +9 = 0, $151e--, rebalance +9 (skip 0xff,
           bcc skips d2 >= mem, else decrement). */
        uint8_t age = voice[VF2_SOUND_VOICE_AGE];
        uint8_t *voices = ram + VF2_SOUND_VOICE_TABLE_BASE;
        voice[VF2_SOUND_VOICE_AGE] = 0u;
        --ram[VF2_SOUND_SHARED_COUNTER_151E];
        for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
            uint8_t *target = voices +
                other * VF2_SOUND_VOICE_BYTES + VF2_SOUND_VOICE_AGE;
            if (*target == UINT8_MAX || age >= *target) {
                continue;
            }
            --(*target);
        }
    } else {
        /* Release teardown 0x1262-0x12b2 (L1-release shape): +5
           snapshot, +9 = 0, +5 = 0, $152e--, rebalance +5 over
           0xff-aged voices. */
        uint8_t age = voice[VF2_SOUND_VOICE_AGE_RELEASE];
        uint8_t *voices = ram + VF2_SOUND_VOICE_TABLE_BASE;
        voice[VF2_SOUND_VOICE_AGE] = 0u;
        voice[VF2_SOUND_VOICE_AGE_RELEASE] = 0u;
        --ram[VF2_SOUND_SHARED_COUNTER_152E];
        for (other = 0u; other < VF2_SOUND_VOICE_COUNT; ++other) {
            uint8_t *slot9 = voices + other * VF2_SOUND_VOICE_BYTES +
                VF2_SOUND_VOICE_AGE;
            uint8_t *slot5 = voices + other * VF2_SOUND_VOICE_BYTES +
                VF2_SOUND_VOICE_AGE_RELEASE;
            if (*slot9 != UINT8_MAX || age >= *slot5) {
                continue;
            }
            --(*slot5);
        }
    }
    return VF2_OK;
}

static vf2_status sound_board_dispatch_80(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    vf2_sound_dispatch_report *report
)
{
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    /* ROM 0x10d0 (v0825 S2b primer correction): the switch's beq
       lands ON the primer (`move.b (a3)+,d0` with a3 at ring+1 as
       the drain leaves it), so d2 = PAYLOAD, not command. Base =
       (payload&0x7f) + desc[4]-byte: ROM 0x10da is `add.b`
       (disassembled, same byte-add shape as the 90 arm's 0x860),
       so desc[5] never participates -- the old word-add was a
       print-era assumption. The 0x10d4 andi.l #$7f is LONG, so
       d0's upper is zeroed entry-independently and the low byte
       escapes to $150f clean. Payload/byte2 are never read past
       the head. */
    const uint16_t base =
        (uint16_t)(((uint16_t)(payload & 0x7fu) + descriptor[4u]) &
                   0xffu);

    report->handler_address = UINT32_C(0x000010d0);
    report->channel = channel;
    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)base;
    /* Split at 0x10e2 (`move.b desc[2],d3` / cmpi #$f0 / bcs):
     * the LOW arm (desc[2] < 0xf0) walks a 6-byte-entry row
     * table (0x1148 cmp.b/bls loop, stride 6, no silicon cap)
     * selected by desc[2]&0x7f at 0x6053aa, then files the
     * same row word/byte and falls into the 0x1174 voice scan
     * (E1g: measured sel-3 walk below); the HIGH arm
     * (desc[2] >= 0xf0) files the row word/byte and falls into
     * the same scan (E1f-a). Residue audit: 0x10f2 andi.l is
     * LONG (d3 exact despite the subi.b/drain upper); 0x1138
     * andi.l is LONG (LOW d3 exact); 0x1106 andi.l is LONG
     * (HIGH d0 exact); 0x1162 andi.l is LONG (LOW d2 exact --
     * the move.b/add.b'd drain residue dies there, so the
     * $1515 index is clean); the walk cmp.b/add.b are
     * byte-only (upper never participates). */
    {
        if (descriptor[2u] < 0xf0u) {
            uint16_t row_word = 0u;
            uint8_t row_byte = 0u;
            uint32_t select =
                (uint32_t)((uint32_t)descriptor[2u] & 0x7fu);
            uint32_t table_base = 0u;
            uint32_t cursor = 0u;
            uint8_t threshold = 0u;
            uint8_t addend = 0u;
            uint8_t index = 0u;
            /* ROM 0x1132-0x1140 LOW table pick: table_base =
             * be32(0x6053aa + select*4), cursor = table_base. */
            if (read_audio_be32(board,
                                UINT32_C(0x6053aa) + select * 4u,
                                &table_base) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            cursor = table_base;
            /* ROM 0x1144-0x1156 threshold walk: d2 = $150f
             * (the head-filed base); stop at the first entry
             * with d2 <= threshold (unsigned bls); stride 6.
             * No silicon cap: the C advances the same way and
             * fails closed only where silicon would leave the
             * audio ROM (unmeasurable). */
            for (;;) {
                if (read_audio_byte_absolute(board, cursor,
                                            &threshold) !=
                    VF2_OK) {
                    return VF2_ERROR_OUT_OF_BOUNDS;
                }
                if ((uint32_t)base <= (uint32_t)threshold) {
                    break;
                }
                cursor += 6u;
            }
            /* ROM 0x1158-0x1162: $1516 = word(cursor+4);
             * d2 = (base + byte(cursor+3)) & 0x7f (add.b wraps
             * mod 256, andi.l keeps the low 7). */
            if (sound_board_read_table_word(board, cursor + 4u,
                                           &row_word) !=
                VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            write_be16(board->sound_ram + VF2_SOUND_LOOKUP_WORD,
                       row_word);
            if (read_audio_byte_absolute(board, cursor + 3u,
                                        &addend) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            index = (uint8_t)(((uint32_t)base + (uint32_t)addend) &
                              0x7fu);
            /* ROM 0x1168-0x116e: $1515 = byte(0x60c3e6+index).
             * $1515 has no named slot ($15xx -> 0x25xx pattern). */
            if (sound_board_read_table_u8(
                    board,
                    UINT32_C(0x60c3e6) + (uint32_t)index,
                    &row_byte) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            board->sound_ram[UINT32_C(0x2515)] = row_byte;
        } else {
            uint32_t select =
                (uint32_t)((descriptor[2u] - 0xf0u) & 0x7fu);
            uint32_t table_base = 0u;
            uint32_t entry = 0u;
            uint16_t word = 0u;
            uint8_t row_byte = 0u;

        /* ROM 0x10ee-0x1112 HIGH row pick (90-path shape:
         * table_base = be32(0x6032c6 + select*4), entry =
         * table_base + (payload&0x7f)*6, $1516 = word(entry)).
         * Odd entry fails closed like silicon (address error). */
        if (read_audio_be32(board,
                            UINT32_C(0x6032c6) + select * 4u,
                            &table_base) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        entry = table_base + (uint32_t)(payload & 0x7fu) * 6u;
        if (sound_board_read_table_word(board, entry, &word) !=
            VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be16(board->sound_ram + VF2_SOUND_LOOKUP_WORD, word);
        /* ROM 0x1118-0x1128: $1515 = byte(0x60c3e6 + base&0x7f).
         * $1515 has no named slot ($15xx -> 0x25xx pattern). */
        if (sound_board_read_table_u8(
                board,
                UINT32_C(0x60c3e6) + (uint32_t)(base & 0x7fu),
                &row_byte) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[UINT32_C(0x2515)] = row_byte;
        }
        /* ROM 0x1174-0x11ce voice scan (v0825 E1f-b/E1g: shared by
           the HIGH row and the LOW walk). d1 = $150e & 0xf
           (drain-filed channel). Per voice (stride 0x10 x32):
           +3 == $1515, +4 == d1, +6 word == $1516, +2 bit4/1
           clear; then andi.b #$7f,+2 IN MEMORY (fires on every
           gate-passer, even when the beq below skips the call);
           full pass inlines jsr 0x11d0 (E1f-c: always rts's, so
           the scan continues). Miss-all exhausts the dbf to the
           0x11ce rts, which resumes the drain -- the dispatch
           completes (OK) and the packet consumes. */
        {
            uint8_t scan_channel =
                (uint8_t)(board->sound_ram[VF2_SOUND_CURRENT_COMMAND] &
                          0x0fu);
            uint8_t scan_byte =
                board->sound_ram[UINT32_C(0x2515)];
            uint16_t row_word = read_be16(board->sound_ram +
                                          VF2_SOUND_LOOKUP_WORD);
            size_t vi = 0u;

            for (vi = 0u; vi < VF2_SOUND_VOICE_COUNT; ++vi) {
                uint8_t *scan_voice =
                    board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
                    vi * VF2_SOUND_VOICE_BYTES;
                if (scan_voice[VF2_SOUND_VOICE_BYTE] != scan_byte ||
                    scan_voice[VF2_SOUND_VOICE_CHANNEL] !=
                        scan_channel ||
                    read_be16(scan_voice + VF2_SOUND_VOICE_SELECT) !=
                        row_word ||
                    (scan_voice[VF2_SOUND_VOICE_FLAGS] & 0x10u) != 0u ||
                    (scan_voice[VF2_SOUND_VOICE_FLAGS] & 0x02u) != 0u) {
                    continue;
                }
                scan_voice[VF2_SOUND_VOICE_FLAGS] &= 0x7fu;
                if (scan_voice[VF2_SOUND_VOICE_FLAGS] == 0u) {
                    continue;
                }
                /* Full pass: inline jsr 0x11d0 (E1f-c subroutine;
                 * always rts's, so the scan continues). */
                if (sound_board_voice_sub_11d0(board, scan_voice,
                                               scan_channel,
                                               report) != VF2_OK) {
                    return VF2_ERROR_OUT_OF_BOUNDS;
                }
            }
            /* Miss-all: dbf exhausted to the 0x11ce rts; fall out
             * to the shared completion below. */
        }
    }
    /* Scan completed (0x11ce rts resumes the drain): the dispatch
     * completes and the packet consumes. */
    return VF2_OK;
}

static vf2_status sound_board_read_table_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
)
{
    return vf2_sound_board_read_u8(board, address, value);
}

static vf2_status sound_board_read_table_word(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t high = 0u;
    uint8_t low = 0u;

    /* ROM move.w traps on odd addresses (address error); fail closed
       the same way instead of tearing a word. */
    if ((address & 1u) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (sound_board_read_table_u8(board, address, &high) != VF2_OK ||
        sound_board_read_table_u8(board, address + 1u, &low) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = (uint16_t)(((uint16_t)high << 8u) | low);
    return VF2_OK;
}

/* ROM 0x884 HIGH table path (v0825 S2c). Returns VF2_OK with
   *reached_door set: 0 = the 0x8a6 gate hit (word == 0x00ff ->
   0x8b0 rts, back to the drain scan, head effects only), 1 = the
   full path ran to the Loop-B door at 0x902 ($1511/$1512/$1516/
   $150d filed, $1515 NOT -- that file is Loop-B's own first
   block). Errors propagate; the caller converts the door to a
   held-packet refusal. */
static vf2_status sound_board_dispatch_90_high(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t payload,
    int *reached_door
)
{
    uint8_t select = 0u;
    uint32_t table_base = 0u;
    uint32_t entry = 0u;
    uint16_t gate = 0u;
    uint8_t filed = 0u;
    uint16_t word = 0u;

    /* ROM 0x884-0x894: d3 = desc[2]-0xf0 (byte subi), &0x7f
       (the long-residue note in dispatch_90: the pick reads d3.w
       only, so the mask is the whole index model), a2 =
       be32(0x6032c6 + d3*4) + (payload&0x7f)*6. */
    select = (uint8_t)((uint8_t)(descriptor[2u] - 0xf0u) & 0x7fu);
    if (read_audio_be32(board,
                        UINT32_C(0x6032c6) + (uint32_t)select * 4u,
                        &table_base) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    entry = table_base + (uint32_t)(payload & 0x7fu) * 6u;
    /* ROM 0x8a6: cmpi.w #$ff,(a2) -- the empty-slot word is 0x00ff,
       not 0xffff (stepper gate-hit legs pin the 0x8b0 rts on words
       that read 0x00ff). */
    if (sound_board_read_table_word(board, entry, &gate) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (gate == UINT16_C(0x00ff)) {
        *reached_door = 0;
        return VF2_OK;
    }
    /* ROM 0x8b2-0x8c2: d1 = byte(a2+4), or desc[6] when the table
       byte is the 0x80 sentinel; file $1511. */
    if (sound_board_read_table_u8(board, entry + 4u, &filed) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (filed == 0x80u) {
        filed = descriptor[6u];
    }
    board->sound_ram[VF2_SOUND_LOOKUP_CONTROL] = filed;
    /* ROM 0x8c6/0x8cc: $1512 = word(a2+2), $1516 = word(a2). */
    if (sound_board_read_table_word(board, entry + 2u, &word) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    write_be16(board->sound_ram + VF2_SOUND_LOOKUP_WORD_ALT, word);
    if (sound_board_read_table_word(board, entry, &word) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    write_be16(board->sound_ram + VF2_SOUND_LOOKUP_WORD, word);
    /* ROM 0x8d2-0x8f6: d1 = desc[13]; nonzero files itself and
       clears desc[13] (bra past the desc[12] logic); zero falls to
       d1 = desc[12], kept unless (d1&7) == 0, when d1 =
       byte(a2+5). File $150d. */
    filed = descriptor[0x0du];
    if (filed != 0u) {
        descriptor[0x0du] = 0u;
    } else {
        filed = descriptor[0x0cu];
        if ((filed & 0x07u) == 0u) {
            if (sound_board_read_table_u8(board, entry + 5u, &filed) !=
                VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
        }
    }
    board->sound_ram[VF2_SOUND_LOOKUP_RELEASE] = filed;
    *reached_door = 1;
    return VF2_OK;
}

/* ROM Loop-B 0x902 (v0825 S2d): the sequencer core the 90 HIGH
   path falls into. Files $1515, gates on $150e-channel and $152e,
   scans 32 voices stride 0x10 for the first +9 == 0xff with +5 ==
   0, arms it (0x95a: SCSP slot key with $10 control, stream-slot
   clear, +5 = 5, +5 rebalance over 0xff-aged voices, $152e--).
   All three exits (0xa8c setup arm, 0x9ba setup scan, 0xf12
   compactor) are unrecovered doors: the handler is set to the
   door address and the packet is held (UNSUPPORTED). d4/d7 are
   freshly loaded on every path (0x8fe andi.w indexes d4.w only;
   both dbf loops reload d7) -- no caller residue participates. */
/* ROM 0x9ba setup scan (v0825 S3a): 32-voice teardown sweep,
   stride 0x10 from the 0x2800 table. EVERY voice with +2 != 0,
   +3 == 0xba, +4 == 0x0f is torn down (the loop does not stop at
   the first): +2 = 0, d2 = +9 (byte -- the move.b leaves the
   entry long-residue in place, and the rebalance's cmp.b reads
   the low byte only, so the C models d2 as the +9 byte), +9 = 0,
   SCSP[slot] = 0x10, ram[0x2000+slot] = 0, $151e--, then the
   V1-shaped +9 rebalance over all 32 voices (skip 0xff slots;
   unsigned d2 >= mem skips; else mem--) with the CURRENT d2.
   Deliberately NO key-off: unlike the V1 arms there is no
   SCSP[slot+0x0b] = 0x1f write on this path (stepper-pinned:
   SCSP[0x2b]/[0x4b] keep prior values). d7 is freshly loaded;
   d2 is only read after a teardown sets it. End: bra 0xf12. */
/* ROM 0x1014 voice-picker subroutine (v0825 S3b), called by the
   HIGH setup-authoring arm. Inputs: d4 = slot word, d5 = $1513
   byte, d6 = $1512 byte. Effect: SCSP[slot+0x10] = combined
   picker word; returns the row-table word in *d2_out (discarded
   by the HIGH caller at 0xb22+ -- no d2 read before 0xc1e -- but
   pinned by the differential against the stepper's live d2, so
   the whole subroutine is proven, not just the store).
   Residue audit: d0/d1 uppers are caller history (d0.low =
   $1512 via 0x101c move.b, d1.low = table+4 byte via 0x8b2);
   0x1054 andi.l #$f clears d1 fully and every indexed use reads
   .w/.b only, so all addresses are exact. move.w (a3) odd would
   address-error on ROM -- fail closed. */
static vf2_status sound_board_voice_picker_1014(
    vf2_sound_board *board,
    uint16_t slot,
    uint8_t b1513,
    uint8_t b1512,
    uint16_t *d2_out
)
{
    uint16_t d5 = (uint16_t)(b1513 & 0x0fu);
    uint16_t d6 = b1512;
    uint16_t d0 = b1512;
    uint16_t d2 = 0u;
    uint16_t w = 0u;
    uint32_t row = 0u;
    uint8_t d1 = 0u;
    uint32_t entry = 0u;

    d6 = (uint16_t)(d6 << 7u);
    d2 = d6;
    d6 &= UINT16_C(0x7800);
    d2 = (uint16_t)(d2 >> 3u);
    d2 &= UINT16_C(0x00f0);
    d2 = (uint16_t)(d2 + d5);
    d2 = (uint16_t)(d2 << 1u);
    if (read_audio_be16(board, UINT32_C(0x60c4a6) + d2, &w) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    d6 |= w;
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x10u, d6) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    d0 &= UINT16_C(0x00f0);
    d0 = (uint16_t)(d0 >> 2u);
    if (read_audio_be32(board, UINT32_C(0x60c726) + d0, &row) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0x1048/0x1054/0x105a: d1 = d0.low($1512 byte) & 0xf,
       lsl.b #5 (byte wrap), full-long cleared above. */
    d1 = (uint8_t)((uint8_t)(b1512 & 0x0fu) << 5u);
    entry = row + d1;
    if (sound_board_read_table_word(board, entry, &w) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *d2_out = w;
    return VF2_OK;
}

/* Setup-authoring cluster (v0825 S3b/c/S4): ROM 0xa8c arm. Shared
   head (+2 seed, slot key, +6 word) in sound_board_setup_author_at,
   then the HIGH / LOW branches below, then the 0xc1e sample-tail.
   Refuse at the 0xc68 playback-engine door (S4+), packet held.
   Word/long SCSP stores need an even slot (ROM address-error
   shape) -- fail closed odd. */
/* Shared authoring volume math (v0825 S3b/c, ROM 0xafa/0xba0):
   d6 = ((voice8 * desc10) >> 8); low ~= ; low >>= 1;
   SCSP[slot+0x0d] = low. Pure bytes; the mulu.w/lsr.w word ops
   never let caller residue escape (16-bit product of two bytes;
   only the low byte is stored). */
static vf2_status sound_board_author_volume(
    vf2_sound_board *board,
    uint16_t slot,
    uint8_t voice8,
    uint8_t desc10
)
{
    uint16_t prod = (uint16_t)((uint16_t)voice8 * desc10);
    uint8_t level = (uint8_t)(prod >> 8u);

    level = (uint8_t)~level;
    level >>= 1u;
    if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot + 0x0du,
                          level) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    return VF2_OK;
}

/* ROM 0x1062 voice-picker subroutine (v0825 S3c), called by the
   LOW authoring arm. Inputs: d4 = slot word, d3 = channel (exact:
   the LOW caller leaves d3 = $150e&0xf via 0xb86 andi.l, untouched
   to the jsr). Effect: SCSP[slot+0x10] = combined picker word;
   returns the row-table word in *d2_out (discarded by the LOW
   caller too -- pinned via the report's lookup word against live
   d2 like the 0x1014). Shape: seed = word(RAM 0x4200 + ch*2);
   slide = sign16(seed + sign16(byte(voice10 + desc5))); picker =
   word(be32(0x602676 + (voice3&0xf)*4) + 2*slide) (the e0-tail
   picker half with a channel-table slide source); store =
   (((voice3&0xf0)<<7) + picker) & 0x7bff; d2 =
   word(be32(0x60c726 + ((voice3&0xf0)>>2)) + bytewrap((voice3&0xf)
   <<5) + 2*slide). The 0x1082 lsl.l pollutes d0's upper but the
   table pick reads d0.w only; the 0x1092 (a3)+ bump is dead (a3
   reloaded at 0x10ba). d6 needs no input: the volume level it
   carries has a proven-zero upper (mulu.w full rewrite of two
   bytes) and 0x1092 overwrites its low word. move.w (a3) odd
   would address-error -- fail closed. */
static vf2_status sound_board_voice_picker_1062(
    vf2_sound_board *board,
    uint16_t slot,
    uint8_t channel,
    uint8_t voice3,
    uint8_t voice10,
    uint8_t desc5,
    uint16_t *d2_out
)
{
    uint16_t seed = 0u;
    int32_t slide_sum = 0;
    uint32_t table = 0u;

    /* ROM 0x1062-0x1068: d5 = word(0x4200 + (ch<<1)) (lsl.w + d3.w
       index; d3 = channel exact). */
    seed = read_be16(board->sound_ram + UINT32_C(0x4200) +
                     (uint32_t)channel * 2u);
    /* ROM 0x106c-0x1078: d3 = voice10 + desc5 (byte add),
       ext.w, d5 += d3 (16-bit wrap), ext.l. */
    slide_sum = (int32_t)(int16_t)(uint16_t)(
        (uint16_t)seed +
        (uint16_t)(int16_t)(int8_t)(uint8_t)(voice10 + desc5));
    /* ROM 0x107a-0x10a4: picker = word(be32(0x602676 + (voice3&0xf)
       *4) + 2*slide) via the bus reader (table may aim at RAM or
       ROM); store = (((voice3&0xf0)<<7) + picker) & 0x7bff. */
    if (read_audio_be32(board, UINT32_C(0x602676) +
                        (uint32_t)(voice3 & 0x0fu) * 4u,
                        &table) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    {
        int64_t pick_at = 0;
        uint16_t picked = 0u;
        uint16_t shaped = 0u;
        uint32_t row = 0u;
        uint8_t d1 = 0u;
        uint16_t w = 0u;

        pick_at = (int64_t)table + 2LL * (int64_t)slide_sum;
        if (pick_at < (int64_t)VF2_SOUND_AUDIO_ROM_BASE ||
            (uint64_t)pick_at + 2u > (uint64_t)VF2_SOUND_AUDIO_ROM_BASE +
                board->audio_rom_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (sound_board_read_table_word(board, (uint32_t)pick_at,
                                        &picked) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        shaped = (uint16_t)((((uint16_t)(voice3 & 0xf0u) << 7u) +
                             picked) & UINT16_C(0x7bff));
        if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x10u,
                               shaped) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        /* ROM 0x10a8-0x10cc: row = be32(0x60c726 + ((voice3&0xf0)
           >>2)); d2 = word(row + bytewrap((voice3&0xf)<<5) +
           2*slide). */
        if (read_audio_be32(board, UINT32_C(0x60c726) +
                            ((uint32_t)(voice3 & 0xf0u) >> 2u),
                            &row) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        d1 = (uint8_t)((uint8_t)(voice3 & 0x0fu) << 5u);
        pick_at = (int64_t)row + (int64_t)d1 +
            2LL * (int64_t)slide_sum;
        if (pick_at < (int64_t)VF2_SOUND_AUDIO_ROM_BASE ||
            (uint64_t)pick_at + 2u > (uint64_t)VF2_SOUND_AUDIO_ROM_BASE +
                board->audio_rom_size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (sound_board_read_table_word(board, (uint32_t)pick_at,
                                        &w) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *d2_out = w;
        return VF2_OK;
    }
}

/* ROM 0xa8c-0xb72 HIGH branch (v0825 S3b, desc[2] >= 0xf0; head
   done by the core): +2 |= 0x10; +3 = $1515; +8 = $1510 mode;
   +4 = channel; +9/+5 seeding per the 0xf gate; volume math; the
   0x1014 picker; sample-slot/zero words; channel-table word;
   $150d/$1511 bytes. Ends at the 0xc1e door. */
static vf2_status sound_board_run_setup_author_high(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    uint8_t *voice,
    uint16_t slot,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint16_t word = 0u;
    uint8_t channel = 0u;
    uint16_t d2 = 0u;

    /* ROM 0xab0-0xaca: +2 |= 0x10; +3 = $1515; +8 = $1510 mode;
       +4 = $150e-channel. (Head done by the core.) */
    voice[VF2_SOUND_VOICE_FLAGS] |= 0x10u;
    voice[VF2_SOUND_VOICE_BYTE] = ram[VF2_SOUND_LOOKUP_BYTE];
    voice[VF2_SOUND_VOICE_MODE] = ram[VF2_SOUND_LOOKUP_MODE];
    channel = (uint8_t)(ram[VF2_SOUND_CURRENT_COMMAND] & 0x0fu);
    voice[VF2_SOUND_VOICE_CHANNEL] = channel;
    /* ROM 0xace-0xaf6: ch 0xf seeds +9 = $151e ($151e++); else
       +9 = 0xff, +5 = $152e ($152e++), +2 |= 8. */
    if (channel == 0x0fu) {
        voice[VF2_SOUND_VOICE_AGE] = ram[VF2_SOUND_SHARED_COUNTER_151E];
        ++ram[VF2_SOUND_SHARED_COUNTER_151E];
    } else {
        voice[VF2_SOUND_VOICE_AGE] = 0xffu;
        voice[VF2_SOUND_VOICE_AGE_RELEASE] = ram[VF2_SOUND_SHARED_COUNTER_152E];
        ++ram[VF2_SOUND_SHARED_COUNTER_152E];
        voice[VF2_SOUND_VOICE_FLAGS] |= 0x08u;
    }
    /* ROM 0xafa-0xb12 volume math (shared helper). */
    if (sound_board_author_volume(board, slot, voice[VF2_SOUND_VOICE_MODE],
                                  descriptor[10u]) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xb16-0xb1e: jsr 0x1014 (d5 = $1513, d6 = $1512). */
    if (sound_board_voice_picker_1014(
            board, slot, ram[VF2_SOUND_LOOKUP_WORD_ALT + 1u],
            ram[VF2_SOUND_LOOKUP_WORD_ALT], &d2) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    report->lookup_word = d2;
    /* ROM 0xb22-0xb30: SCSP[slot+4] = 0x00001fff (move.l bytes),
       +0x0c = 0, +0x0e = 0. */
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 4u, 0u) !=
            VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 6u,
                           UINT16_C(0x1fff)) != VF2_OK ||
        vf2_scsp_write_u8(&board->scsp, (uint32_t)slot + 0x0cu, 0u) !=
            VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x0eu, 0u) !=
            VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xb36-0xb52: d5 = desc[8] word | ((ram[0x4220+ch] << 5)
       & 0xe0); SCSP[slot+0x12] = d5. lsl.w/andi.w keep 16 bits;
       exact. */
    word = read_be16(descriptor + 8u);
    {
        uint16_t gain =
            (uint16_t)(((uint16_t)ram[0x4220u + channel] << 5u) &
                       UINT16_C(0x00e0));
        word |= gain;
    }
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x12u,
                           word) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xb56-0xb6e: +0x15 = $150d; +0x16 = ($1511&0x1f)|0x60.
       Falls through to the 0xc1e tail (core runs it). */
    if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot + 0x15u,
                          ram[VF2_SOUND_LOOKUP_RELEASE]) != VF2_OK ||
        vf2_scsp_write_u8(
            &board->scsp, (uint32_t)slot + 0x16u,
            (uint8_t)((ram[VF2_SOUND_LOOKUP_CONTROL] & 0x1fu) |
                      0x60u)) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    return VF2_OK;
}

/* ROM 0xb76-0xc1a LOW branch (v0825 S3c, desc[2] < 0xf0; head done
   by the core): +3 = $1515; +8 = $1510 mode; +4 = channel;
   +9 = $151e ($151e++); +10 = $1518; volume math; the 0x1062
   picker (SCSP[slot+0x10] + d2 to lookup_word); sample-slot/zero
   words; d5 = desc[8] word | ((ram[0x4220+ch]<<5)&0xe0) |
   (ram[0x4240+ch]&7); +0x15 = desc[12]; +0x16 = ($1511&0x1f) |
   0x60. Falls into the 0xc1e door. Unlike HIGH there is no +2
   bset chain, no +5/$152e traffic, and the 0x12 word carries a
   second channel-table row. */
static vf2_status sound_board_run_setup_author_low(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    uint8_t *voice,
    uint16_t slot,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint8_t channel = 0u;
    uint16_t word = 0u;
    uint16_t d2 = 0u;

    voice[VF2_SOUND_VOICE_BYTE] = ram[VF2_SOUND_LOOKUP_BYTE];
    voice[VF2_SOUND_VOICE_MODE] = ram[VF2_SOUND_LOOKUP_MODE];
    channel = (uint8_t)(ram[VF2_SOUND_CURRENT_COMMAND] & 0x0fu);
    voice[VF2_SOUND_VOICE_CHANNEL] = channel;
    voice[VF2_SOUND_VOICE_AGE] = ram[VF2_SOUND_SHARED_COUNTER_151E];
    ++ram[VF2_SOUND_SHARED_COUNTER_151E];
    voice[VF2_SOUND_VOICE_META] = ram[VF2_SOUND_LOOKUP_META];
    if (sound_board_author_volume(board, slot, voice[VF2_SOUND_VOICE_MODE],
                                  descriptor[10u]) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xbbc: jsr 0x1062 (d3 = channel from the 0xb86 andi.l,
       carried in the parameter). */
    if (sound_board_voice_picker_1062(
            board, slot, channel, voice[VF2_SOUND_VOICE_BYTE], voice[VF2_SOUND_VOICE_META],
            descriptor[5u], &d2) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    report->lookup_word = d2;
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 4u, 0u) !=
            VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 6u,
                           UINT16_C(0x1fff)) != VF2_OK ||
        vf2_scsp_write_u8(&board->scsp, (uint32_t)slot + 0x0cu, 0u) !=
            VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x0eu, 0u) !=
            VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xbd4-0xbfe: d5 = desc[8] word | ((ram[0x4220+ch]<<5) &
       0xe0) | (ram[0x4240+ch] & 7); SCSP[slot+0x12] = d5. */
    word = read_be16(descriptor + 8u);
    {
        uint16_t gain =
            (uint16_t)(((uint16_t)ram[0x4220u + channel] << 5u) &
                       UINT16_C(0x00e0));
        uint16_t shade =
            (uint16_t)((uint16_t)ram[0x4240u + channel] &
                       UINT16_C(0x0007));
        word |= (uint16_t)(gain | shade);
    }
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x12u,
                           word) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xc02-0xc1a: +0x15 = desc[12]; +0x16 = ($1511&0x1f) |
       0x60. */
    if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot + 0x15u,
                          descriptor[12u]) != VF2_OK ||
        vf2_scsp_write_u8(
            &board->scsp, (uint32_t)slot + 0x16u,
            (uint8_t)((ram[VF2_SOUND_LOOKUP_CONTROL] & 0x1fu) |
                      0x60u)) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    return VF2_OK;
}

/* ROM 0xa8c setup-authoring core (v0825 S3c): shared head (+2 =
   0x40, d4 = slot, SCSP[slot] = 0x10, +6 = $1516 word) then the
   0xaac split on desc[2]. The odd-slot guard covers the word/long
   SCSP stores on both branches. */
static vf2_status sound_board_run_sample_tail(
    vf2_sound_board *board,
    uint8_t *voice,
    uint16_t slot,
    vf2_sound_dispatch_report *report
);

static vf2_status sound_board_run_engine_key(
    vf2_sound_board *board,
    uint8_t *voice,
    uint16_t slot,
    uint32_t d3,
    vf2_sound_dispatch_report *report
);

/* ROM 0xf12 allocator: defined below (used by the Loop-B
 * gates and the setup scan before its definition site). */
/* ROM 0xf50 reaper: defined below (the allocator's exhausted
 * tail bras to it). Both take the drain descriptor -- the
 * reaper-found path bras to the 0xa8c setup author, which
 * reads desc[2] for its HIGH/LOW split. */
static vf2_status sound_board_run_allocator_0f12(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    vf2_sound_dispatch_report *report
);
static vf2_status sound_board_run_reaper_0f50(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    vf2_sound_dispatch_report *report
);

static vf2_status sound_board_setup_author_at(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    uint8_t *voice,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint16_t slot = read_be16(voice + VF2_SOUND_VOICE_SLOT);

    if ((slot & 1u) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    voice[VF2_SOUND_VOICE_FLAGS] = 0x40u;
    if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                          UINT8_C(0x10)) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    write_be16(voice + VF2_SOUND_VOICE_SELECT, read_be16(ram + VF2_SOUND_LOOKUP_WORD));
    {
        vf2_status branch_status = VF2_OK;

        /* ROM 0xaac split; both branches fall through to the 0xc1e
           tail (S4). */
        if (descriptor[2u] < 0xf0u) {
            branch_status = sound_board_run_setup_author_low(
                board, descriptor, voice, slot, report);
        } else {
            branch_status = sound_board_run_setup_author_high(
                board, descriptor, voice, slot, report);
        }
        if (branch_status != VF2_OK) {
            return branch_status;
        }
        return sound_board_run_sample_tail(board, voice, slot, report);
    }
}

/* ROM 0xc1e sample-tail (v0825 S4, 10 insns): d3 = (voice+6 &
   0x7ff) * 20 (mulu.w, 32-bit exact, always < 0x10000 so the
   d3.w index is whole); SCSP[slot+8] = long($5000-table entry +
   0x10) (the V2 uploader's stored/target words live there);
   voice+0xb = ROM[0x600c48 + (byte(entry+0x13) & 0x1f)] (the
   0x0c48+ rate table sits in the disassembler's data zone --
   read as data, never executed). NOTE (v0825 S4a correction):
   the absolute address is 0x600c48 (0x0c48 ROM offset); a first
   draft wrote 0x60c448 (0xc448, an unrelated ramp zone) and the
   stepper caught it (C=02 vs S=00 on zero tables, C=02 vs S=0x64
   on a staged index). Count hex digits against the print.
   Refuses at the 0xc68 playback engine (S4+ door), packet held.
   d3 arithmetic cannot leave the 512KB RAM (max 0x5000+0xffec+
   0x13), so no OOB arm exists. */
static vf2_status sound_board_run_sample_tail(
    vf2_sound_board *board,
    uint8_t *voice,
    uint16_t slot,
    vf2_sound_dispatch_report *report
)
{
    uint32_t d3 = 0u;
    uint32_t entry = 0u;
    uint8_t rate = 0u;
    uint8_t southern = 0u;

    d3 = (uint32_t)(read_be16(voice + VF2_SOUND_VOICE_SELECT) & UINT16_C(0x07ff)) *
        UINT32_C(20);
    entry = UINT32_C(0x5000) + d3;
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 8u,
                           (uint16_t)(((uint16_t)board->sound_ram[entry +
                                                                 0x10u]
                                       << 8u) |
                                      board->sound_ram[entry + 0x11u])) !=
            VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 0x0au,
                           (uint16_t)(((uint16_t)board->sound_ram[entry +
                                                                 0x12u]
                                       << 8u) |
                                      board->sound_ram[entry + 0x13u])) !=
            VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (vf2_sound_board_read_u8(board, entry + 0x13u, &rate) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    rate &= 0x1fu;
    if (read_audio_byte_absolute(board, UINT32_C(0x600c48) + rate,
                                 &southern) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    ((uint8_t *)voice)[VF2_SOUND_VOICE_RATE] = southern;
    return sound_board_run_engine_key(board, voice, slot, d3, report);
}

/* ROM 0xc68 engine head + slot program (v0825 E1b, 6 + 11 insns).
 * Inputs: voice (+0 slot word, +6 select, +0x0c config word),
 * slot, d3 (sample-tail index), d2 rate (report->lookup_word from
 * the HIGH/LOW picker -- both branches store it before the tail).
 * Gate words from the $5000 table at d3: +0x00 bit 31 set takes
 * the 0xed8 arm (v0825 E1c: key-on, completes); +0x0c long
 * nonzero takes the 0xd92 arm (v0825 E1i: bclr voice, stream
 * RMW pair, same slot program, refuses at the 0xdf8 dead
 * loop). Effects on the fall-through (S4 shape pinned):
 * d5 = long(table+0x04); SCSP[slot+0] long = 0x10300000 long +
 * word(voice+0x0c) (KEYON program); stream[slot+0x0a] long =
 * d5 - 0x200; stream[slot+0x01] = 0x10; stream[slot+0x1a] =
 * d2rate and stream[slot+0x1c] = d2rate + 0xeba (16-bit exact).
 * Refuses at the 0xcb2 copy engine because the full continuation is
 * not recovered. MOVEM.L D0-D7,(A2) is LEGAL on MC68000 (v0880);
 * the first 16 copy triplets at 0xcb2..0xd72 are separately recovered
 * and tested by copy_engine_page_512, but dispatch_next must still
 * fail closed instead of inventing the rest of playback.
 * The bit31-taken branch runs sound_board_run_engine_keyon_ed8
 * (v0825 E1c) instead of refusing. */
static vf2_status sound_board_run_engine_keyon_ed8(
    vf2_sound_board *board,
    uint16_t slot,
    uint32_t d3,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint32_t table = UINT32_C(0x5000) + d3;
    uint32_t stream = UINT32_C(0x2000) + (uint32_t)slot;
    uint32_t t0 = 0u;
    uint32_t t4 = 0u;
    uint32_t t8 = 0u;
    uint32_t key = 0u;
    uint16_t w6 = 0u;
    uint8_t hib = 0u;

    /* ROM 0xed8 key-on arm (v0825 E1c, 15 insns). Taken when the
       table+0 long has BE bit31 set (stepper-pinned: 0xf10
       arrival in 381 steps, rts resumes the drain at 0x7f6 in 1
       step). Effects: key = (t0 & 0xfffff) | 0x10300000 to
       SCSP[slot+0] (u16 pair); SCSP[slot+4] = low word of
       table+8; SCSP[slot+6] = low word of table+4 minus 1
       (subq.w: low-word wrap exact); RMW slot+0 high byte |=
       0x18 through the model so synchronize fires -- KEYONB
       sets and slot_active goes 1 (S slot0 0x1830/0x0000, +4
       0x0000, +6 0x4a61, stream 0x40, active0=1); stream byte
       ram[0x2000+slot] = 0x40. No voice writes, no 151x
       counters, d2 untouched. Returns VF2_OK: the rts resumes
       the drain scan, which the C 0x90 loop models by
       continuing. */
    if (table + 12u > VF2_SOUND_RAM_BYTES ||
        stream >= VF2_SOUND_RAM_BYTES) {
        report->handler_address = UINT32_C(0x00000ed8);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }    t0 = ((uint32_t)ram[table] << 24u) |
         ((uint32_t)ram[table + 1u] << 16u) |
         ((uint32_t)ram[table + 2u] << 8u) | ram[table + 3u];
    t4 = ((uint32_t)ram[table + 4u] << 24u) |
         ((uint32_t)ram[table + 5u] << 16u) |
         ((uint32_t)ram[table + 6u] << 8u) | ram[table + 7u];
    t8 = ((uint32_t)ram[table + 8u] << 24u) |
         ((uint32_t)ram[table + 9u] << 16u) |
         ((uint32_t)ram[table + 10u] << 8u) | ram[table + 11u];
    /* ROM 0xed8-0xee4: key long to the slot. */
    key = (t0 & UINT32_C(0x000fffff)) | UINT32_C(0x10300000);
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot,
                           (uint16_t)(key >> 16u)) != VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 2u,
                           (uint16_t)key) != VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 4u,
                           (uint16_t)t8) != VF2_OK) {
        report->handler_address = UINT32_C(0x00000ee4);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xef0-0xef6: subq.w #1 on the low word, then move.w. */
    w6 = (uint16_t)t4;
    --w6;
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 6u,
                           w6) != VF2_OK) {
        report->handler_address = UINT32_C(0x00000ef0);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xefa-0xf02: high byte |= 0x18 (KEYONB+KEYONEX, rest
       preserved). Read-OR-write through the model so the key-on
       effect fires exactly as the two silicon stores. */
    if (vf2_scsp_read_u8(&board->scsp, (uint32_t)slot,
                         &hib) != VF2_OK) {
        report->handler_address = UINT32_C(0x00000efa);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    hib |= 0x18u;
    if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                          hib) != VF2_OK) {
        report->handler_address = UINT32_C(0x00000efa);
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xf06-0xf0a: stream flag. 0xf10 rts = continue. */
    ram[stream] = 0x40u;
    return VF2_OK;
}

/* ROM 0xd92 second engine arm (v0825 E1i). Taken when the
 * table+0x0c long (d0) is nonzero -- head-loaded, so every
 * input derives: d1 = long(table+8), d5/a0 the table pair,
 * d2 the setup rate, d4 the slot word, a1 the voice.
 * Effects: bclr #4 voice+2; the E1b slot program (d6 =
 * long(voice+0x0c) | 0x10300000 to SCSP[slot+0] as the u16
 * pair; stream[slot+0x0a] long = d5 - 0x200); the stream RMW
 * pair (+0x16 keeps its top byte, low 24 = d0 low 24; +0x12
 * keeps its top byte, low 24 = d1 low 24); the rate pair
 * +0x1a/+0x1c (d2 + 0xeba 16-bit wrap exact);
 * stream[slot+0x01] = 0x10. Refuses at the unrecovered 0xdf8
 * copy loop. The old illegal-EA MOVEM explanation was disproved
 * for 0xcb6; do not infer an illegal instruction here without an
 * independent original-ROM opcode/EA verification. */
static vf2_status sound_board_run_engine_key_d92(
    vf2_sound_board *board,
    uint8_t *voice,
    uint16_t slot,
    uint32_t d3,
    uint32_t d0,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint32_t table = UINT32_C(0x5000) + d3;
    uint32_t stream = UINT32_C(0x2000) + (uint32_t)slot;
    uint32_t d1 = 0u;
    uint32_t d5 = 0u;
    uint32_t d6 = 0u;
    uint32_t e12 = 0u;
    uint32_t e16 = 0u;
    uint16_t rate = 0u;

    /* ROM 0xd92: bclr #4, voice+2. */
    voice[VF2_SOUND_VOICE_FLAGS] &= 0xefu;
    /* ROM 0xd98-0xda8: table longs + the E1b d6 form (32-bit
     * fetch OR'd with the KEYON base -- E1b's word-trap
     * applies unchanged). */
    d1 = (((uint32_t)ram[table + 8u] << 24u) |
          ((uint32_t)ram[table + 9u] << 16u) |
          ((uint32_t)ram[table + 10u] << 8u) | ram[table + 11u]);
    d5 = (((uint32_t)ram[table + 4u] << 24u) |
          ((uint32_t)ram[table + 5u] << 16u) |
          ((uint32_t)ram[table + 6u] << 8u) | ram[table + 7u]);
    d6 = UINT32_C(0x10300000) |
         (((uint32_t)voice[0x0cu] << 24u) |
          ((uint32_t)voice[0x0du] << 16u) |
          ((uint32_t)voice[0x0eu] << 8u) | voice[0x0fu]);
    rate = (uint16_t)(report->lookup_word & UINT32_C(0xffff));
    if (stream + UINT32_C(0x1e) > VF2_SOUND_RAM_BYTES) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot,
                           (uint16_t)(d6 >> 16u)) != VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 2u,
                           (uint16_t)d6) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xdb0-0xdb6: stream sample long. */
    d5 -= UINT32_C(0x200);
    write_be32(ram + stream + 0x0au, d5);
    /* ROM 0xdba-0xde2: the RMW pair (top byte preserved, low
     * 24 replaced -- andi.l/or.l exact). */
    e16 = (((uint32_t)ram[stream + 0x16u] << 24u) |
           ((uint32_t)ram[stream + 0x17u] << 16u) |
           ((uint32_t)ram[stream + 0x18u] << 8u) |
           ram[stream + 0x19u]);
    write_be32(ram + stream + 0x16u,
               (e16 & UINT32_C(0xff000000)) |
               (d0 & UINT32_C(0x00ffffff)));
    e12 = (((uint32_t)ram[stream + 0x12u] << 24u) |
           ((uint32_t)ram[stream + 0x13u] << 16u) |
           ((uint32_t)ram[stream + 0x14u] << 8u) |
           ram[stream + 0x15u]);
    write_be32(ram + stream + 0x12u,
               (e12 & UINT32_C(0xff000000)) |
               (d1 & UINT32_C(0x00ffffff)));
    /* ROM 0xde6-0xdf2: rate pair + key byte (E1b shape). */
    write_be16(ram + stream + 0x1au, rate);
    write_be16(ram + stream + 0x1cu,
               (uint16_t)(rate + UINT16_C(0x0eba)));
    ram[stream + 0x01u] = 0x10u;
    report->handler_address = UINT32_C(0x00000df8);
    /* The original 68000 now executes 16 MOVEM triplets at DF8,
     * followed by the EB8 seven-instruction KEYON tail and RTS at
     * ED6. Both exact fragments passed independent owner-ROM
     * differentials (v0889/v0890). The opt-in path completes
     * ONLY this measured arm; all other engine refusals stay.
     * A0 comes from table+0 (C74), A2 from voice+0x0c (D9C).
     * Preflight ALL addresses, slot and sample attachment before
     * writing the first sample byte. Invalid state holds packet
     * and preserves the old fail-closed 0xDF8 frontier. */
    if (board->allow_verified_d92_copy != 0u) {
        uint32_t source = read_be32(ram + table);
        uint32_t destination = read_be32(voice + 0x0cu);
        uint32_t slot_offset = (uint32_t)read_be16(voice);
        size_t voice_offset = (size_t)(voice - (ram +
                                                   VF2_SOUND_VOICE_TABLE_BASE));
        size_t voice_index = voice_offset / VF2_SOUND_VOICE_BYTES;
        vf2_status status;
        if (board->sample_rom == NULL ||
            board->scsp.sample_rom != board->sample_rom ||
            board->scsp.sample_rom_size < board->sample_rom_size ||
            voice_offset % VF2_SOUND_VOICE_BYTES != 0u ||
            voice_index >= VF2_SOUND_VOICE_COUNT ||
            slot_offset != (uint32_t)slot ||
            slot_offset % VF2_SCSP_SLOT_BYTES != 0u ||
            slot_offset >= VF2_SCSP_SLOT_COUNT * VF2_SCSP_SLOT_BYTES ||
            source < VF2_SOUND_SAMPLE_ROM_BASE ||
            ((source | destination) & UINT32_C(1)) != 0u ||
            (uint64_t)(source - VF2_SOUND_SAMPLE_ROM_BASE) +
                UINT64_C(512) > board->sample_rom_size ||
            destination > VF2_SOUND_RAM_BYTES - UINT32_C(512)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_sound_board_copy_engine_page_512(
            board, &source, &destination);
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_sound_board_finalize_engine_copy_eb8(
            board, voice_index, source, destination);
        if (status != VF2_OK) {
            return status;
        }
        report->handler_address = UINT32_C(0x00000ed6);
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status sound_board_run_engine_key(
    vf2_sound_board *board,
    uint8_t *voice,
    uint16_t slot,
    uint32_t d3,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint32_t table = UINT32_C(0x5000) + d3;
    uint32_t d5 = 0u;
    uint32_t d6 = 0u;
    uint32_t d0 = 0u;
    uint16_t rate = 0u;
    uint32_t stream = 0u;

    /* ROM 0xc68-0xc80 head gates (table longs, BE). d0 is
     * head-loaded (move.l), so the d92 arm derives -- E1b-dig
     * listed it as residue only because no S4 shape set it. */
    if ((ram[table] & 0x80u) != 0u) {
        return sound_board_run_engine_keyon_ed8(board, slot, d3,
                                               report);
    }
    d0 = (((uint32_t)ram[table + 0x0cu] << 24u) |
          ((uint32_t)ram[table + 0x0du] << 16u) |
          ((uint32_t)ram[table + 0x0eu] << 8u) |
          ram[table + 0x0fu]);
    if (d0 != 0u) {
        return sound_board_run_engine_key_d92(board, voice, slot,
                                              d3, d0, report);
    }
    d5 = ((uint32_t)ram[table + 4u] << 24u) |
         ((uint32_t)ram[table + 5u] << 16u) |
         ((uint32_t)ram[table + 6u] << 8u) | ram[table + 7u];
    /* ROM 0xc82: d6 = long(voice+0x0c) (32-bit fetch) OR'd with
     * the KEYON base -- a word read drops the low half
     * (caught: C=0x10300001 vs S=0x10310000). */
    d6 = UINT32_C(0x10300000) |
         (((uint32_t)voice[0x0cu] << 24u) |
          ((uint32_t)voice[0x0du] << 16u) |
          ((uint32_t)voice[0x0eu] << 8u) | voice[0x0fu]);
    rate = (uint16_t)(report->lookup_word & UINT32_C(0xffff));
    stream = UINT32_C(0x2000) + (uint32_t)slot;
    if (stream + UINT32_C(0x1e) > VF2_SOUND_RAM_BYTES) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0xc82-0xcaa slot program. SCSP long store as the u16
     * pair the model exposes; 16-bit rate wrap is exact. */
    if (vf2_scsp_write_u16(&board->scsp, (uint32_t)slot,
                           (uint16_t)(d6 >> 16u)) != VF2_OK ||
        vf2_scsp_write_u16(&board->scsp, (uint32_t)slot + 2u,
                           (uint16_t)d6) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    d5 -= UINT32_C(0x200);
    write_be32(ram + stream + 0x0au, d5);
    ram[stream + 0x01u] = 0x10u;
    write_be16(ram + stream + 0x1au, rate);
    write_be16(ram + stream + 0x1cu,
               (uint16_t)(rate + UINT16_C(0x0eba)));
    report->handler_address = UINT32_C(0x00000cb2);
    /* Frame-342 original ROM: the exact 9E 0A 7F allocator leg
     * reaches legal MOVEM copy 0xCB2..0xD72 after FAC->A8C.
     * Original 0xC68 loads D5 from $5000[D3]+0; 0xC74
     * MOVEA.L D5,A0 preserves that source, then 0xC76
     * reloads D5 from table+4. A2 comes from voice+0x0C
     * at original 0xC7C. Both original subroutines
     * are now recovered as isolated differential C routines.
     * Compose ONLY for that verified command with slot 0;
     * other 90-class allocator cases retain the CB2 refusal.
     * Preflight both 512-byte regions BEFORE any page transfer. */
    if (board->allow_verified_90_allocator != 0u &&
        report->command == UINT8_C(0x9e) &&
        report->payload == UINT8_C(0x0a) &&
        slot == 0u &&
        voice == ram + VF2_SOUND_VOICE_TABLE_BASE &&
        board->sample_rom != NULL &&
        board->scsp.sample_rom == board->sample_rom) {
        uint16_t read_cursor = (uint16_t)(
            read_be16(ram + VF2_SOUND_COMMAND_CURSOR) & UINT16_C(0x0fff));
        if (read_cursor <= UINT16_C(0x0ffd) &&
            ram[VF2_SOUND_COMMAND_RING_BASE + read_cursor] ==
                UINT8_C(0x9e) &&
            ram[VF2_SOUND_COMMAND_RING_BASE + read_cursor + 1u] ==
                UINT8_C(0x0a) &&
            ram[VF2_SOUND_COMMAND_RING_BASE + read_cursor + 2u] ==
                UINT8_C(0x7f)) {
            uint32_t source = read_be32(ram + table);
            uint32_t destination = read_be32(voice + 0x0cu);
            vf2_status status;
            if (source < VF2_SOUND_SAMPLE_ROM_BASE ||
                destination > VF2_SOUND_RAM_BYTES - UINT32_C(512) ||
                ((source | destination) & UINT32_C(1)) != 0u ||
                (uint64_t)(source - VF2_SOUND_SAMPLE_ROM_BASE) +
                    UINT64_C(512) > board->sample_rom_size) {
                return VF2_ERROR_UNSUPPORTED;
            }
            status = vf2_sound_board_copy_engine_page_512(
                board, &source, &destination);
            if (status != VF2_OK) {
                return status;
            }
            status = vf2_sound_board_finalize_engine_copy_d72(
                board, 0u, source, destination);
            if (status != VF2_OK) {
                return status;
            }
            /* The original MC68000 MOVEM uploads the 512-byte page
             * into SCSP-visible sound RAM, not directly to the ROM.
             * Attach the owning board's current RAM before rendering.
             * Out-of-range RAM reads still fall back to the ROM bank.
             * Keep this scoped to the explicit verified-9E opt-in:
             * generic board memcpy users must not inherit an unsafe
             * self-pointer at initialization. */
            status = vf2_scsp_attach_sample_ram(
                &board->scsp, board->sound_ram, VF2_SOUND_RAM_BYTES);
            if (status != VF2_OK) {
                return status;
            }
            report->handler_address = UINT32_C(0x00000d90);
            return VF2_OK;
        }
    }
    return VF2_ERROR_UNSUPPORTED;
}

vf2_status vf2_sound_board_setup_author(
    vf2_sound_board *board,
    size_t descriptor_index,
    size_t voice_index,
    vf2_sound_dispatch_report *report
)
{
    if (board == NULL || report == NULL ||
        descriptor_index >= VF2_SOUND_COMMAND_DESCRIPTOR_COUNT ||
        voice_index >= VF2_SOUND_VOICE_COUNT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *report = (vf2_sound_dispatch_report){0};
    return sound_board_setup_author_at(
        board,
        board->sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            descriptor_index * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES,
        board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            voice_index * VF2_SOUND_VOICE_BYTES,
        report);
}

/* ROM 0x49e $1526 dance (v0825 8g): ror.b #1 the flag; 0x7f forces
 * 0xf7; store back; (low nibble | 0x10) goes to the $400001
 * system port, which is unmodeled -- the poke is deferred
 * (cf04 precedent) while the flag update is applied. */
static void sound_board_run_dance_49e(vf2_sound_board *board)
{
    uint8_t flag = board->sound_ram[VF2_SOUND_SHARED_COUNTER_1526];

    flag = (uint8_t)((flag >> 1u) | ((flag & 1u) << 7u));
    if (flag == 0x7fu) {
        flag = 0xf7u;
    }
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_1526] = flag;
}

/* ROM L3 receiver 0x526 file path (v0825 8g, d0 = byte): ring
 * assembly at $0000(a6) + ($1504 & 0xfff) = absolute 0x1000 +
 * cursor (the command ring the drain consumes, shared with
 * emit_command), $1504 += 4 (& 0xfff), $1506++, then the $49e
 * dance. Two shapes share the tail: the 0x580 arm (d7 = $1502 -
 * 1 != 0) word-copies the $1500/$1501 staging bytes plus the new
 * byte; the 0x5b8 arm (d7 == 0, i.e. $1502 == 1) byte-copies
 * $1500 plus the new byte. */
static void sound_board_run_ring_file(
    vf2_sound_board *board,
    uint8_t byte,
    int word_copy
)
{
    uint8_t *ram = board->sound_ram;
    uint32_t cursor =
        ((uint32_t)(((uint32_t)ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                    ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]) &
         UINT32_C(0x0fff));

    ram[VF2_SOUND_COMMAND_RING_BASE + cursor] =
        ram[VF2_SOUND_RING_STAGE_BASE];
    if (word_copy) {
        ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 1u] =
            ram[VF2_SOUND_RING_STAGE_BASE + 1u];
        ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 2u] = byte;
    } else {
        ram[VF2_SOUND_COMMAND_RING_BASE + cursor + 1u] = byte;
    }
    cursor = (cursor + UINT32_C(4)) & UINT32_C(0x0fff);
    ram[VF2_SOUND_COMMAND_WRITE_CURSOR] =
        (uint8_t)(cursor >> 8u);
    ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = (uint8_t)cursor;
    ++ram[VF2_SOUND_COMMAND_COUNT];
    sound_board_run_dance_49e(board);
}

vf2_status vf2_sound_board_file_uart_byte(
    vf2_sound_board *board,
    uint8_t byte
)
{
    uint8_t *ram = NULL;
    uint8_t d1 = 0u;

    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* ROM 0x52e/0x532: 0xff is the sound-CPU reboot door (sr mask,
     * $401 clear, a7 reset, bra $100) -- refused cleanly. */
    if (byte == 0xffu) {
        return VF2_ERROR_UNSUPPORTED;
    }
    ram = board->sound_ram;
    /* ROM 0x536/0x53a: >= 0xf0 is ignored with no state change
     * (branches over the $1503 file straight to the epilogue). */
    if (byte >= 0xf0u) {
        return VF2_OK;
    }
    if ((byte & 0x80u) != 0u) {
        /* ROM 0x560-0x57c: file $1500; d1 = 2, or 1 when
         * 0xc0 <= byte < 0xe0; file $1502. */
        ram[VF2_SOUND_RING_STAGE_BASE] = byte;
        d1 = 2u;
        if (byte >= 0xc0u && byte < 0xe0u) {
            d1 = 1u;
        }
        ram[VF2_SOUND_RING_STAGE_BASE + 2u] = d1;
    } else {
        /* ROM 0x546-0x55c: d1 = $1503 (or $1502 when $1503 is
         * zero); the ring-file arm is taken when d1 == 1 BEFORE
         * the subq (beq tests d1 - 1 == 0); otherwise the byte is
         * filed at $1501. The $1503 epilogue always files the
         * POST-sub value. */
        uint8_t select = ram[VF2_SOUND_RING_STAGE_BASE + 3u];
        if (select == 0u) {
            select = ram[VF2_SOUND_RING_STAGE_BASE + 2u];
        }
        d1 = (uint8_t)(select - 1u);
        if (select == 1u) {
            uint8_t d7 =
                (uint8_t)(ram[VF2_SOUND_RING_STAGE_BASE + 2u] - 1u);
            sound_board_run_ring_file(board, byte, d7 != 0u);
        } else {
            ram[VF2_SOUND_RING_STAGE_BASE + 1u] = byte;
        }
    }
    /* ROM 0x5e2 (all non-f0 paths): file $1503. */
    ram[VF2_SOUND_RING_STAGE_BASE + 3u] = d1;
    return VF2_OK;
}

static vf2_status sound_board_run_setup_scan(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    size_t i = 0u;

    for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
        uint8_t *voice = ram + VF2_SOUND_VOICE_TABLE_BASE +
            i * VF2_SOUND_VOICE_BYTES;
        if (voice[VF2_SOUND_VOICE_FLAGS] == 0u || voice[VF2_SOUND_VOICE_BYTE] != 0xbau ||
            voice[VF2_SOUND_VOICE_CHANNEL] != 0x0fu) {
            continue;
        }
        {
            uint8_t age = voice[VF2_SOUND_VOICE_AGE];
            uint16_t slot = read_be16(voice + VF2_SOUND_VOICE_SLOT);
            size_t j = 0u;

            voice[VF2_SOUND_VOICE_FLAGS] = 0u;
            voice[VF2_SOUND_VOICE_AGE] = 0u;
            if (vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                                  UINT8_C(0x10)) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            if ((uint32_t)0x2000 + (uint32_t)slot >=
                VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ram[0x2000u + slot] = 0u;
            --ram[VF2_SOUND_SHARED_COUNTER_151E];
            for (j = 0u; j < VF2_SOUND_VOICE_COUNT; ++j) {
                uint8_t *target = ram + VF2_SOUND_VOICE_TABLE_BASE +
                    j * VF2_SOUND_VOICE_BYTES + VF2_SOUND_VOICE_AGE;
                if (*target == UINT8_MAX || age >= *target) {
                    continue;
                }
                --(*target);
            }
        }
    }
    /* ROM 0x9ba-end: bra 0xf12 (into the allocator, E1o). */
    return sound_board_run_allocator_0f12(board, descriptor, report);
}

static vf2_status sound_board_run_loopb(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint8_t index = 0u;
    uint8_t filed = 0u;
    size_t i = 0u;

    /* ROM 0x8fa-0x908: d4 = $150f&0x7f, $1515 =
       ROM[0x60c3e6+d4]. */
    index = (uint8_t)(ram[VF2_SOUND_LOOKUP_CURRENT] & 0x7fu);
    if (read_audio_byte_absolute(board, UINT32_C(0x60c3e6) + index,
                                 &filed) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    ram[VF2_SOUND_LOOKUP_BYTE] = filed;
    /* ROM 0x90e-0x91a: d3 = $150e&0xf; channel 0xf takes the 0x9a2
       setup-gate: $1515 == 0xb6/0xb8 doors to the 0x9ba scan,
       anything else to 0xf12. */
    if ((ram[VF2_SOUND_CURRENT_COMMAND] & 0x0fu) == 0x0fu) {
        if (filed == 0xb6u || filed == 0xb8u) {
            /* Fall-through to the 0x9ba setup scan (S3a). */
            return sound_board_run_setup_scan(board, descriptor, report);
        }
        /* ROM 0x91c: bra 0xf12 (into the allocator, E1o). */
        return sound_board_run_allocator_0f12(board, descriptor, report);
    }
    /* ROM 0x91e-0x926: $152e < 5 doors to 0xf12 (scan skipped,
       $152e untouched; into the allocator, E1o). */
    if (ram[VF2_SOUND_SHARED_COUNTER_152E] < 5u) {
        return sound_board_run_allocator_0f12(board, descriptor, report);
    }
    /* ROM 0x92a-0x94c: 32-voice scan for +9 == 0xff with +5 == 0
       (byte compares, stride 0x10 from the 0x2800 table). */
    for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
        uint8_t *voice = ram + VF2_SOUND_VOICE_TABLE_BASE +
            i * VF2_SOUND_VOICE_BYTES;
        if (voice[VF2_SOUND_VOICE_AGE] == 0xffu && voice[VF2_SOUND_VOICE_AGE_RELEASE] == 0u) {
            uint16_t slot = read_be16(voice + VF2_SOUND_VOICE_SLOT);
            size_t j = 0u;

            /* ROM 0x95a-0x974: d4 = slot word; SCSP[slot+0x0b] =
               0x1f, SCSP[slot] = 0x10, ram[0x2000+slot] = 0,
               voice+5 = 5. */
            if (vf2_scsp_write_u8(&board->scsp,
                                  (uint32_t)slot + 0x0bu,
                                  UINT8_C(0x1f)) != VF2_OK ||
                vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                                  UINT8_C(0x10)) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            if ((uint32_t)0x2000 + (uint32_t)slot >=
                VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ram[0x2000u + slot] = 0u;
            voice[VF2_SOUND_VOICE_AGE_RELEASE] = 5u;
            /* ROM 0x97a-0x996: rebalance -- every 0xff-aged voice's
               +5 decrements, INCLUDING the just-armed one (5->4;
               stepper-pinned). +9 is never cleared on this path. */
            for (j = 0u; j < VF2_SOUND_VOICE_COUNT; ++j) {
                uint8_t *target = ram + VF2_SOUND_VOICE_TABLE_BASE +
                    j * VF2_SOUND_VOICE_BYTES;
                if (target[9u] != 0xffu) {
                    continue;
                }
                --target[5u];
            }
            /* ROM 0x99a: $152e--, bra 0xa8c (setup arm). a1 still
               addresses the armed voice; a0 the drain descriptor. */
            --ram[VF2_SOUND_SHARED_COUNTER_152E];
            return sound_board_setup_author_at(
                board, descriptor, voice, report);
        }
    }
    /* ROM 0x950: scan miss zeroes $152e, bra 0xf12 (into the
       allocator, E1o). */
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 0u;
    return sound_board_run_allocator_0f12(board, descriptor, report);
}

/* ROM 0xf12-0xf4c voice allocator (v0825 E1m). $151a holds
 * the absolute cursor (init 0x2800; voices 0x2800..0x29f0
 * stride 0x10, sentinel 0x2a00): 0x2a00 wraps to 0x2800
 * (write-back), then up to 32 records scanned for +2 == 0
 * (move.b/compare -- the cursor is RAM, not registers, so no
 * residue question exists at all). Found: cursor = found +
 * 0x10 (write-back), refuse at the 0xfac setup authoring (S3
 * territory: voice fill + volume math + engine). Allocator
 * arrivals can never enter it (E1n): d4 still holds the
 * walk's $1515 index (never reloaded), so the slot stores
 * would address-error on odd indices (ROM vector 2 blinks)
 * and mis-program slot-by-index on even ones. None-free
 * (dbf exhausts): bra 0xf50 into the reaper (E1p). Wired into the 90-LOW walk end (E1m) and all
 * three Loop-B exits plus the setup-scan tail (E1o) -- every
 * ROM bra to 0xf12 now enters it. */
static vf2_status sound_board_run_allocator_0f12(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    uint32_t cursor = 0u;
    size_t i = 0u;

    cursor = (((uint32_t)ram[UINT32_C(0x251a)] << 24u) |
              ((uint32_t)ram[UINT32_C(0x251a) + 1u] << 16u) |
              ((uint32_t)ram[UINT32_C(0x251a) + 2u] << 8u) |
              ram[UINT32_C(0x251a) + 3u]);
    if (cursor == UINT32_C(0x2a00)) {
        cursor = UINT32_C(0x2800);
        ram[UINT32_C(0x251a)] = 0u;
        ram[UINT32_C(0x251a) + 1u] = 0u;
        ram[UINT32_C(0x251a) + 2u] = 0x28u;
        ram[UINT32_C(0x251a) + 3u] = 0x00u;
    }
    for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
        uint32_t record = cursor + (uint32_t)i * 0x10u;
        if (record + 2u >= VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (ram[record + 2u] == 0u) {
            uint32_t next = record + 0x10u;
            if (next + 3u >= VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ram[UINT32_C(0x251a)] = (uint8_t)(next >> 24u);
            ram[UINT32_C(0x251a) + 1u] = (uint8_t)(next >> 16u);
            ram[UINT32_C(0x251a) + 2u] = (uint8_t)(next >> 8u);
            ram[UINT32_C(0x251a) + 3u] = (uint8_t)next;
            report->handler_address = UINT32_C(0x00000fac);
            /* Verified natural frame-342 9E 0A 7F:
             * the ORIGINAL 68000 arrives at 0xFAC with D4=0,
             * A1=0x2800 and a free, even slot-0 voice. ROM
             * BRA.W at FAC enters the already recovered A8C
             * setup author, which reaches the SCSP KEYON path.
             * Do NOT generalize to allocator arrivals with
             * unknown D4 residue, odd slots, other commands,
             * or missing full audio/sample bank. Default-off
             * preserves the established fail-closed behavior. */
            if (board->allow_verified_90_allocator != 0u &&
                report->command == UINT8_C(0x9e) &&
                report->payload == UINT8_C(0x0a) &&
                record == VF2_SOUND_VOICE_TABLE_BASE &&
                read_be16(ram + record) == 0u &&
                board->audio_rom != NULL &&
                board->audio_rom_size >= UINT32_C(0x80000) &&
                board->sample_rom != NULL &&
                board->sample_rom_size >= UINT32_C(0x800000) &&
                board->scsp.sample_rom == board->sample_rom &&
                (read_be16(ram + VF2_SOUND_COMMAND_CURSOR) &
                    UINT16_C(0x0fff)) <= UINT16_C(0x0ffd)) {
                uint16_t ring_cursor = (uint16_t)(
                    read_be16(ram + VF2_SOUND_COMMAND_CURSOR) &
                    UINT16_C(0x0fff));
                uint8_t *ring = ram + VF2_SOUND_COMMAND_RING_BASE;
                if (ring[ring_cursor] == UINT8_C(0x9e) &&
                    ring[ring_cursor + 1u] == UINT8_C(0x0a) &&
                    ring[ring_cursor + 2u] == UINT8_C(0x7f)) {
                    return sound_board_setup_author_at(
                        board, descriptor, ram + record, report);
                }
            }
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    /* ROM 0xf3c: dbf exhausts (no +2-free voice), bra 0xf50
       into the reaper (E1p). */
    return sound_board_run_reaper_0f50(board, descriptor, report);
}

/* ROM 0xf50-0xfac reaper (v0825 E1p). 32-voice scan from the
 * 0x2800 table base for +9 == 0 (move.b/compare, stride
 * 0x10 -- base reset, NOT the $151a cursor). Exhausted (dbf,
 * none age-zero): the 0xf68 rts pops the drain's jsr 0x81a
 * return and resumes at 0x7f6 -- the descriptor scan finishes
 * (no writes), the ring cursor advances by 4 at 0x804 and
 * control exits at 0x265c (all stepper-pinned, E1r). The
 * command completes with no authoring: VF2_OK, so
 * dispatch_next's consume block models that drain tail
 * (handler pin stays 0xf50). Found (0xf6a): d4 =
 * voice slot word; SCSP[slot+0x0b] = 0x1f, SCSP[slot] =
 * 0x10, voice+2 = 0, ram[0x2000+slot] = 0; rebalance loop
 * decrements every non-0xff age INCLUDING the just-found
 * voice (0 -> 0xff, stepper-pinned); $151e--; bra 0xa8c
 * into the setup author (the 0xa92 head reloads d4 from the
 * voice slot, so the author sees the same entry state as a
 * core-fill arrival with this voice). The 0xfb0 +9==0x80
 * variant has no inbound edge anywhere in the 512KB audio
 * program image (no absolute/word-absolute/PC-relative
 * branch targets it) and stays unrecovered. */
static vf2_status sound_board_run_reaper_0f50(
    vf2_sound_board *board,
    const uint8_t *descriptor,
    vf2_sound_dispatch_report *report
)
{
    uint8_t *ram = board->sound_ram;
    size_t i = 0u;

    for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
        uint8_t *voice = ram + VF2_SOUND_VOICE_TABLE_BASE +
            i * VF2_SOUND_VOICE_BYTES;
        if (voice[VF2_SOUND_VOICE_AGE] != 0u) {
            continue;
        }
        {
            uint16_t slot = read_be16(voice + VF2_SOUND_VOICE_SLOT);
            size_t j = 0u;

            if (vf2_scsp_write_u8(&board->scsp,
                                  (uint32_t)slot + 0x0bu,
                                  UINT8_C(0x1f)) != VF2_OK ||
                vf2_scsp_write_u8(&board->scsp, (uint32_t)slot,
                                  UINT8_C(0x10)) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            voice[VF2_SOUND_VOICE_FLAGS] = 0u;
            if ((uint32_t)0x2000 + (uint32_t)slot >=
                VF2_SOUND_RAM_BYTES) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            ram[0x2000u + slot] = 0u;
            for (j = 0u; j < VF2_SOUND_VOICE_COUNT; ++j) {
                uint8_t *target = ram + VF2_SOUND_VOICE_TABLE_BASE +
                    j * VF2_SOUND_VOICE_BYTES;
                if (target[VF2_SOUND_VOICE_AGE] == 0xffu) {
                    continue;
                }
                --target[VF2_SOUND_VOICE_AGE];
            }
            --ram[VF2_SOUND_SHARED_COUNTER_151E];
            return sound_board_setup_author_at(
                board, descriptor, voice, report);
        }
    }
    /* Exhausted: the rts at 0xf68 resumes the drain (0x7f6),
       which consumes the command -- the drain tail advances the
       ring cursor by 4 (0x804) and exits at 0x265c with no
       authoring. VF2_OK, not a refusal (E1r: cursor 0 -> 4,
       steps pinned). The pump counts it dispatched, matching
       silicon's silent drop of a 90 command on a fully busy
       voice table. */
    report->handler_address = UINT32_C(0x00000f50);
    return VF2_OK;
}

static vf2_status sound_board_dispatch_90(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    uint8_t ring_byte2,
    vf2_sound_dispatch_report *report,
    int *reached_door
)
{
    /* ROM 0x858 (v0825 S2b primer correction): the switch's beq
       lands ON the primer (`move.b (a3)+,d0` with a3 at ring+1 as
       the drain leaves it -- stepper-proven switch-90 leg enters at
       0x858 in 4 steps), so d2 = PAYLOAD (ring byte 1) and the
       second byte is ring byte 2 (the (a3) peek with a3 at ring+2).
       The old "d2 = command / second = payload" matched the C's own
       head-entry staging, not the drain's reality -- the same trap
       class as the S2a e0 primer fix, caught the same way (a
       drain-entry differential, not a re-read of the print).
       Channel (report only -- the body itself never reads the
       command byte) stays command & 0xf. */
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    const uint8_t head = payload;
    const uint8_t second = ring_byte2;
    uint16_t base_full = 0u;
    uint8_t mode = 0u;

    report->handler_address = UINT32_C(0x00000858);
    report->channel = channel;
    /* ROM 0x85c: base = head + desc[4]-byte (`move.b`/`add.b`; only
       the low byte adds). d0's upper is zero on entry (the drain's
       0x7be/0x7c8 andi.l chain clears it and every move.b preserves
       the zero), so the byte add is exact and the low byte escapes
       to $150f. */
    base_full = (uint16_t)((uint16_t)head + descriptor[4u]);
    board->sound_ram[VF2_SOUND_LOOKUP_CURRENT] = (uint8_t)base_full;
    /* ROM 0x866: second == 0 continues into the 80 body
       mid-instruction (0x10e4): trap on HW. */
    if (second == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* ROM 0x86c mode table: `move.b (a1,d0.l)` with d0.l == second
       exactly (upper zero per the chain above -- no masking exists
       on this path, none is needed). */
    if (read_audio_byte_absolute(
            board,
            UINT32_C(0x60c6a6) + second,
            &mode
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    board->sound_ram[VF2_SOUND_LOOKUP_MODE] = mode;
    report->selected_value = mode;
    /* Split at 0x878 (`move.b desc[2],d3` / cmpi.b #$f0 / bcs):
       - < 0xf0 -> 0xa3e low path (row-table walk, files
         $1516/$1518/$1515/$1511, bra $f12 -- never Loop-B).
         Unrecovered (S2d backlog): refuse with head filed.
       - >= 0xf0 -> HIGH table path (sound_board_dispatch_90_high,
         v0825 S2c): gate-hit returns OK with the door shut (back
         to the drain scan); the door returns OK with files applied
         and the caller converts to a held-packet refusal at 0x902.
       The d3 upper survives subi.b/andi.w as long-residue, but the
       table pick reads d3.w only (0x894 `move.l (a1,d3.w)`), so the
       index is exact. */
    if (descriptor[2u] < 0xf0u) {
        /* ROM 0xa3e-0xa88 LOW row walk (v0825 E1k): the same
         * 0x6053aa table family as the 80-LOW walk (E1g), but
         * the masking discipline differs -- 0xa44 andi.w and
         * 0xa72 andi.w are WORD ops (d3/d4 uppers survive), yet
         * every use reads .w/.b only (0xa4a move.l (a1,d3.w),
         * 0xa52 cmp.b, 0xa6e add.b, 0xa7c move.b (a1,d4.w)), so
         * the residue never participates (v0821 byte-domain
         * rule, same conclusion as E1g's LONG audit by a
         * different route). Files $1516 (word cursor+4), $1518
         * (byte cursor+2), $1515 (byte 0x60c3e6+index, index =
         * (base + byte(cursor+3)) & 0x7f with byte-add wrap),
         * $1511 (desc[6]), then bra 0xf12 (voice allocator,
         * S2/S3 setup territory -- refused). The walk is
         * uncapped with per-read OOB fail-closed (E1g
         * precedent: identical to silicon for every in-ROM
         * walk). */
        uint32_t select = (uint32_t)descriptor[2u] & 0x7fu;
        uint32_t table_base = 0u;
        uint32_t cursor = 0u;
        uint8_t walk_base =
            board->sound_ram[VF2_SOUND_LOOKUP_CURRENT];
        uint8_t threshold = 0u;
        uint8_t addend = 0u;
        uint8_t meta = 0u;
        uint8_t row_byte = 0u;
        uint16_t word = 0u;
        if (read_audio_be32(board,
                            UINT32_C(0x6053aa) + select * 4u,
                            &table_base) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        cursor = table_base;
        for (;;) {
            if (read_audio_byte_absolute(board, cursor,
                                        &threshold) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            if ((uint32_t)walk_base <= (uint32_t)threshold) {
                break;
            }
            cursor += 6u;
        }
        if (sound_board_read_table_word(board, cursor + 4u,
                                        &word) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be16(board->sound_ram + VF2_SOUND_LOOKUP_WORD, word);
        if (read_audio_byte_absolute(board, cursor + 2u,
                                    &meta) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[VF2_SOUND_LOOKUP_META] = meta;
        if (read_audio_byte_absolute(board, cursor + 3u,
                                    &addend) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        if (sound_board_read_table_u8(
                board,
                UINT32_C(0x60c3e6) +
                (uint32_t)(((uint32_t)walk_base +
                             (uint32_t)addend) &
                            0x7fu),
                &row_byte) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        board->sound_ram[UINT32_C(0x2515)] = row_byte;
        board->sound_ram[VF2_SOUND_LOOKUP_CONTROL] =
            descriptor[6u];
        /* Into the 0xf12 allocator (E1m; E1o wires every ROM
         * bra to it, E1p its exhausted tail to the reaper). */
        return sound_board_run_allocator_0f12(board, descriptor, report);
    }
    {
        vf2_status high_status =
            sound_board_dispatch_90_high(board, descriptor, payload,
                                         reached_door);
        if (high_status != VF2_OK) {
            return high_status;
        }
        if (*reached_door) {
            /* Fall-through 0x8fe into Loop-B (S2d/S3). */
            return sound_board_run_loopb(board, descriptor, report);
        }
        return VF2_OK;
    }
}

static vf2_status sound_board_dispatch_a0(
    vf2_sound_board *board,
    uint8_t command,
    uint8_t payload,
    uint8_t ring_byte2,
    vf2_sound_dispatch_report *report
)
{
    uint32_t index = (uint32_t)payload & 0x7fu;
    uint32_t row_base = 0u;
    uint16_t max = 0u;
    uint8_t *ram = board->sound_ram;

    report->handler_address = UINT32_C(0x00001e74);
    report->channel = (uint8_t)(command & 0x0fu);
    /* ROM 0x1e74-0x1e84 row-dir pick: idx = ring byte 1
     * (payload) & 0x7f; row = be32(0x608e12 + idx*4). Both
     * andi.l are LONG (d0 exact despite drain residue); the
     * lsl.w #2 runs on a zeroed upper, so the .w index is
     * whole. */
    if (read_audio_be32(board,
                        UINT32_C(0x608e12) + index * 4u,
                        &row_base) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* ROM 0x1e88-0x1e92 max gate: d0 = ring byte 2 (andi.l
     * #$ff, exact); d2 = max word (move.w loads the low word
     * only and cmp.w compares low words only, so the stale
     * upper never participates -- the old "d2 never loaded /
     * cmp.l residue split" comment misread the print; the ROM
     * bytes are move.w/cmp.w). Odd row fails closed like
     * silicon (move.w address error). */
    if (sound_board_read_table_word(board, row_base, &max) !=
        VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* ROM 0x1e94 bgt (signed word): payload byte > max takes
     * the 0x1eac rts, which resumes the drain -- the packet
     * consumes. The head wrote no RAM (register-only d0/d2/a4
     * traffic), so both exits are side-effect-free here. */
    if ((int16_t)ring_byte2 > (int16_t)max) {
        return VF2_OK;
    }
    /* ROM 0x1e98 join: entry-pointer table into code-as-pointer
     * sled (v0804: rows 64+ read code; T2: the body spins on
     * non-pristine history). Residue-wild by measurement:
     * refuse at the join, packet held. */
    report->handler_address = UINT32_C(0x00001e98);
    /* Original owner-ROM AD23/AD24 record headers have bit7 set.
     * Authentic i960 frame-342 command AD 21 00 selects row
     * 0x6091A8 (max=0x007B), record 0x60A6B2, header=0xCF.
     * All three take the ROM 0x1EA0 BTST #7 -> 0x1ED4
     * six-descriptor vacant search. Recover only the FIRST
     * vacant slot: 0x1F04 zeros 32 bytes, and 0x1EF2..0x1F02
     * installs A2=record+1, word 1 and bit7. The precise AD21
     * third byte 0 is checked here; do not generalize to other
     * AD21 selectors or the unmeasured all-busy seventh address. */
    if (command == UINT8_C(0xad) &&
        (payload == UINT8_C(0x23) || payload == UINT8_C(0x24) ||
         (payload == UINT8_C(0x21) && ring_byte2 == UINT8_C(0)))) {
        uint32_t record = 0u;
        uint8_t header = 0u;
        uint8_t *bank = ram + VF2_SOUND_DESCRIPTOR_BASE +
                        VF2_SOUND_STREAM_DESCRIPTOR_BYTES;
        size_t remaining = VF2_SOUND_RAM_BYTES -
                           (VF2_SOUND_DESCRIPTOR_BASE +
                            VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
        int selected = -1;

        if (read_audio_be32(board, row_base + UINT32_C(2) +
                            (uint32_t)ring_byte2 * UINT32_C(4),
                            &record) != VF2_OK ||
            record < VF2_SOUND_AUDIO_ROM_BASE ||
            record == UINT32_MAX ||
            read_audio_byte_absolute(board, record, &header) != VF2_OK ||
            (header & UINT8_C(0x80)) == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        selected = vf2_sound_a0_descriptor_init(bank, remaining, record + 1u);
        if (selected < 0) {
            /* Still refuse the original unmeasured all-busy path. */
            report->handler_address = UINT32_C(0x00001eee);
            return VF2_ERROR_UNSUPPORTED;
        }
        ++report->descriptors_updated;
        report->handler_address = UINT32_C(0x00001f02);
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

/* ROM 0x1356 per-voice tail (e0, channel < 9 matches). Residue audit
   (master note): d0/d2/d5/d6 lows are deterministic (mem, table, or
   cleared); stale uppers die in andi.w masks or in the discarded d2/d4
   words (`move.w` uses the low word for value and index alike). d4 =
   voice[0] is the slot word; both stores are d4-indexed (the old
   absolute forms, and a later d0-scatter misread of the same ext
   word, came from indexed-dst print bugs): SCSP[d4+0x10] = d0 and
   ram[0x201a+d4] = picked word (stream-slot timer). An odd slot word
   fails closed (address-error shape on ROM). The lsl.b #5 offset
   wraps at the byte. */
static vf2_status sound_board_e0_voice_tail(
    vf2_sound_board *board,
    const uint8_t *voice,
    const uint8_t *descriptor,
    uint16_t shared
)
{
    int16_t addend = 0;
    int32_t slide = 0;
    uint8_t voice_nibble = 0u;
    uint32_t table = 0u;
    int64_t slot_at = 0;
    uint16_t picker = 0u;
    uint16_t shaped = 0u;
    uint8_t row = 0u;
    uint32_t row_table = 0u;
    uint32_t offset = 0u;
    int64_t store_at = 0;
    uint16_t stored = 0u;

    /* d3 = voice[10] + desc[5]-byte, sign-extended (TRUE add.b at
       ROM 0x135e; only the low byte adds, ext.w sign-extends it).
       d5 = $1524-word plus d3, sign-extended (ext.l drops the
       caller-upper). */
    addend = (int16_t)(int8_t)(uint8_t)(voice[VF2_SOUND_VOICE_META] + descriptor[5u]);
    slide = (int32_t)(int16_t)(uint16_t)(shared +
                                         (uint16_t)(uint16_t)addend);
    voice_nibble = (uint8_t)(voice[VF2_SOUND_VOICE_BYTE] & 0x0fu);
    if (read_audio_be32(board, UINT32_C(0x602676) +
                        (uint32_t)voice_nibble * 4u, &table) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    slot_at = (int64_t)table + 2LL * (int64_t)slide;
    if (slot_at < (int64_t)VF2_SOUND_AUDIO_ROM_BASE ||
        (uint64_t)slot_at + 2u >
            (uint64_t)VF2_SOUND_AUDIO_ROM_BASE + board->audio_rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be16(board, (uint32_t)slot_at, &picker) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    /* d0 = (voice[3]&0xf0)<<7; add.l table-word; andi.w #$7bff (upper
       never escapes: only the low word is stored). */
    shaped = (uint16_t)((((uint16_t)(voice[VF2_SOUND_VOICE_BYTE] & 0xf0u) << 7u) + picker) &
                        UINT16_C(0x7bff));
    {
        /* ROM 0x1396: move.w d0,$10(a5,d4.w). Slot+0x10 word = d0. */
        const uint32_t slot_base = (uint32_t)read_be16(voice + VF2_SOUND_VOICE_SLOT);
        vf2_status voice_status = VF2_OK;
        if ((slot_base & 1u) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        voice_status =
            vf2_scsp_write_u16(&board->scsp, slot_base + UINT32_C(0x10),
                               shaped);
        if (voice_status != VF2_OK) {
            return voice_status;
        }
    }
    row = (uint8_t)(((uint8_t)(voice[VF2_SOUND_VOICE_BYTE] & 0xf0u) >> 2u) & 0x3cu);
    if (read_audio_be32(board, UINT32_C(0x60c726) + row, &row_table) !=
        VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    offset = ((uint32_t)(voice[VF2_SOUND_VOICE_BYTE] & 0x0fu) << 5u) & 0xffu;
    store_at = (int64_t)row_table + (int64_t)offset + 2LL * (int64_t)slide;
    if (store_at < (int64_t)VF2_SOUND_AUDIO_ROM_BASE ||
        (uint64_t)store_at + 2u >
            (uint64_t)VF2_SOUND_AUDIO_ROM_BASE + board->audio_rom_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (read_audio_be16(board, (uint32_t)store_at, &stored) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    {
        /* ROM 0x13c4: move.w d2,$1a(a2,d4.w) with a2 = 0x2000 and d4
           = voice[0] (slot word). Stream-slot timer, not ram[0x601a]. */
        const uint32_t timer_at = UINT32_C(0x201a) +
            (uint32_t)read_be16(voice + VF2_SOUND_VOICE_SLOT);
        if (timer_at + 2u > (uint32_t)VF2_SOUND_RAM_BYTES) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_be16(board->sound_ram + timer_at, stored);
    }
    return VF2_OK;
}

static vf2_status sound_board_dispatch_e0(
    vf2_sound_board *board,
    uint8_t *descriptor,
    uint8_t command,
    uint8_t payload,
    uint8_t ring_byte2,
    vf2_sound_dispatch_report *report
)
{
    /* ROM e0 entry 0x12b4 (v0825 S2a primer correction): the primer
       is `move.b (a3)+,d0` / `move.b d0,d2` / `move.b (a3),d0` with
       a3 = ring+cursor+1 as the drain leaves it -- so d2 = PAYLOAD
       (ring byte 1) and d0 = ring byte 2, NOT command/payload. The
       S7b model (d2 = command via a stream staged {cmd,payload})
       matched its own staging, not the drain's reality; the
       drain-e0 differential caught it (shared 0xfff2 vs 0xfff1).
       Combined = (payload&0x7f)|((byte2&0x7f)<<7), >> 5. The
       0x12ba/0x12c0 andi.l #$7f are LONG (byte-verified): both
       uppers are zeroed entry-independently, so NO residue rides
       into the stored word. The stored word is ±selected, negated
       when transformed-bit8 is clear. Channel (voice match +
       channel-table word) stays command & 0xf. */
    uint16_t combined = (uint16_t)((payload & 0x7fu) |
                                   ((uint32_t)(ring_byte2 & 0x7fu) << 7u));
    uint16_t transformed = (uint16_t)(combined >> 5u);
    uint8_t table_index = (uint8_t)transformed;
    uint8_t table_number = (uint8_t)(descriptor[7u] & 0x0fu);
    uint32_t table_pointer = 0u;
    uint8_t selected = 0u;
    uint16_t result = 0u;
    uint8_t channel = (uint8_t)(command & 0x0fu);
    size_t index = 0u;

    /* ROM neg.b/subq.b normalization (0x12d2-0x12dc, btst #8): the
       index is the POST-negation low byte (0xFF when it normalizes
       to zero). Both negs key on transformed-bit8 (0x100), coupled:
       bit8 set skips both, bit8 clear runs both. */
    if ((transformed & 0x100u) == 0u) {
        table_index = (uint8_t)(0u - table_index);
        if (table_index == 0u) {
            table_index = UINT8_MAX;
        }
    }
    if (read_audio_be32(
            board,
            UINT32_C(0x602b26) + (uint32_t)table_number * 4u,
            &table_pointer
        ) != VF2_OK ||
        read_audio_byte_absolute(
            board, table_pointer + table_index, &selected
        ) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    result = (transformed & 0x100u) != 0u
        ? (uint16_t)selected
        : (uint16_t)(0u - (uint16_t)selected);
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 6u] =
        (uint8_t)(result >> 8u);
    board->sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 7u] =
        (uint8_t)result;
    board->sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE +
                     (size_t)channel * 2u] =
        (uint8_t)(result >> 8u);
    board->sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 1u +
                     (size_t)channel * 2u] =
        (uint8_t)result;
    report->channel = channel;
    report->selected_value = selected;
    report->signed_value = (int16_t)result;
    report->handler_address = UINT32_C(0x000012b6);

    /* ROM bcc on channel >= 9 exits via rts at 0x13c8 (stepper leg
       e0-gate @34 steps). Benign; the early return stands on that
       evidence. */
    if (channel >= 9u) {
        return VF2_OK;
    }
    for (index = 0u; index < VF2_SOUND_VOICE_COUNT; ++index) {
        uint8_t *voice = board->sound_ram + VF2_SOUND_VOICE_TABLE_BASE +
            index * VF2_SOUND_VOICE_BYTES;
        /* ROM 0x1332 is cmp.B voice+4 (byte-verified opcode; the old
           cmp.w print hid it): match on the single channel byte, not
           a word. voice[5] is ignored by the scan. */
        if (voice[VF2_SOUND_VOICE_CHANNEL] == channel &&
            (voice[VF2_SOUND_VOICE_FLAGS] & 0x7fu) != 0u) {
            vf2_status status =
                sound_board_e0_voice_tail(board, voice, descriptor, result);
            if (status != VF2_OK) {
                return status;
            }
            ++report->voices_matched;
        }
    }
    return VF2_OK;
}

static vf2_status sound_board_dispatch_b0(
    vf2_sound_board *board,
    uint8_t command,
    uint8_t payload,
    uint8_t argument,
    vf2_sound_dispatch_report *report
)
{
    /* Owner-ROM opcode trace at 0x13cc (frame342 AD21):
     * MOVE.B D0,D2 takes ring byte 1 (payload), MOVE.B (A3),D0
     * takes ring byte 2 (argument); ANDI.W #$7f,D2 and LSL.W
     * #2,D2 select a four-byte PC-relative dispatch table.
     * BA 2A 00 -> table[0x2a] -> 0x194a; BA 07 7f ->
     * table[0x07] -> 0x16ba, neither stages ROM data in the
     * verified owner-ROM snapshot. Only table[0x3a] targets the
     * 0x1ac0 uploader. The prior command&0x7f selector always
     * selected 0x3a for BA and spuriously re-staged 13000 bytes. */
    const uint8_t channel = (uint8_t)(command & 0x0fu);
    const uint8_t entry = (uint8_t)(payload & 0x7fu);

    report->handler_address = UINT32_C(0x000013cc);
    report->channel = channel;
    report->selected_value = entry;
    if (entry != 0x3au) {
        /* Owner-ROM AD21 0x2a/0x07 arms were measured to leave
         * RAM unchanged except the common drain head/retirement.
         * Other B0 table arms still need independent recovery. */
        return VF2_OK;
    }
    /* 0x1ac0 runs only for B0 table selector 0x3a. The third
     * ring byte is now D0 and selects the uploader's row. */
    return sound_board_sample_upload(
        board, UINT32_C(0x608df0),
        1, argument);
}

vf2_status vf2_sound_board_dispatch_next(
    vf2_sound_board *board,
    vf2_sound_dispatch_report *report
)
{
    vf2_sound_dispatch_report local = {0};
    uint8_t *ram = NULL;
    uint16_t cursor = 0u;
    uint8_t count = 0u;
    size_t index = 0u;

    if (board == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    ram = board->sound_ram;
    count = ram[VF2_SOUND_COMMAND_COUNT];
    cursor = (uint16_t)(read_be16(ram + VF2_SOUND_COMMAND_CURSOR) &
                        UINT16_C(0x0fffu));
    local.cursor_before = cursor;
    local.cursor_after = cursor;
    if (count == 0u) {
        local.cursor_after = cursor;
        *report = local;
        return VF2_OK;
    }
    local.command = ram[VF2_SOUND_COMMAND_RING_BASE + cursor];
    local.payload = ram[VF2_SOUND_COMMAND_RING_BASE +
                        ((cursor + 1u) & UINT16_C(0x0fffu))];
    local.command_class = (uint8_t)(local.command & 0xf0u);
    ram[VF2_SOUND_CURRENT_COMMAND] = local.command;
    for (index = 0u; index < VF2_SOUND_COMMAND_DESCRIPTOR_COUNT; ++index) {
        uint8_t *descriptor = ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            index * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
        ++local.descriptors_scanned;
        /* ROM drain scan 0x7dc-0x7ea (v0825 S2a): `btst #7,$0000`
           gate, then `cmp.b $0001(a0),d0` with d0 = command & 0xf
           (0x7d2 `andi.b`) -- a BYTE match on descriptor[1]. The old
           word-match (be16(desc+1) == channel) was a print-era
           assumption with no silicon behind it; it made Loop-B
           unreachable (no boot descriptor both word-matches and
           carries desc[2] >= 0xf0) while byte-match routes img2's
           0x0a-0x0e rows into the 0x90 HIGH path. Proven by the
           drain-e0 differential (desc[1] = ch, desc[2] = 0). */
        if ((descriptor[0] & 0x80u) == 0u ||
            descriptor[1u] != (uint8_t)(local.command & 0x0fu)) {
            continue;
        }
        ++local.descriptors_matched;
        /* Switch mapping (v0825 S2b): the ROM switch reads filed
           $150e and jumps per class ($90->0x858, $80->0x10d0,
           $e0->0x12b4, $b0->0x13ca, $c0->0x1e5a, $a0->0x1e74,
           else rts at 0x856 -- the C's own switch-routing legs pin
           every one). The old default->90 mapping is retired. */
        if (local.command_class == 0x80u) {
            vf2_status status = sound_board_dispatch_80(
                board, descriptor, local.command, local.payload, &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0x90u) {
            int at_door = 0;
            vf2_status status = sound_board_dispatch_90(
                board, descriptor, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))], &local,
                &at_door
            );
            /* Gate-hit (0x8b0 rts): head effects only, back to the
               drain scan -- a later descriptor may still match, so
               continue like the default-ignore arm. Trap / low
               split / bus error (status != OK) holds the packet, as
               does a door the C cannot complete (OK + at_door, the
               E1b 0xcb2 shape: run_loopb's exits all refuse, so a
               completed descent never surfaces OK+door -- except
               E1c's 0xed8 key-on arm, whose rts resumes the drain
               (S: 0x7f6 in 1 step, drain exit 0x265c) and must
               consume, not hold. Hold therefore keys on status
               alone; the OK+door=1 combination is the completed
               path. */
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
            /* Verified original frame-342 9E0A7F completes a real
             * MOVEM+KEYON tail at D90. The original 68000 returns
             * directly to the drain retirement at this terminal
             * point; it does NOT descend into another matching
             * descriptor's FAC allocator. Continuing the generic
             * C loop incorrectly files three more lookup bytes
             * and refuses an already completed sound packet.
             * This stop is narrowly scoped to the explicitly
             * admitted and fully completed D90 path. */
            if (board->allow_verified_90_allocator != 0u &&
                local.command == UINT8_C(0x9e) &&
                local.payload == UINT8_C(0x0a) &&
                local.handler_address == UINT32_C(0x00000d90)) {
                break;
            }
        } else if (local.command_class != 0x80u &&
                   local.command_class != 0xa0u &&
                   local.command_class != 0xb0u &&
                   local.command_class != 0xc0u &&
                   local.command_class != 0xe0u) {
            /* ROM switch rts at 0x856 (v0825 S2b): unlisted classes
               run no body -- the old default->90 mapping contradicted
               the C's own switch-00 leg (filed 0x00 reaches 0x856 in
               14 steps). The drain still consumes the packet and a
               later descriptor may still match a listed class, so
               record the ignore and continue the scan. */
            local.handler_address = UINT32_C(0x00000856);
            local.channel = (uint8_t)(local.command & 0x0fu);
        } else if (local.command_class == 0xa0u) {
            /* Primer-correct ring byte 2 (S2b 0x90-primer
             * precedent: a3 sits at ring+2 when the class arm
             * runs, so the max-gate reads the second payload
             * byte, not the first). */
            vf2_status status = sound_board_dispatch_a0(
                board, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))],
                &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xb0u) {
            vf2_status status = sound_board_dispatch_b0(
                board, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))], &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        } else if (local.command_class == 0xc0u) {
            /* Original ROM 0x1e5c receives the payload through
               the switch's D2 primer; the observed real frame-342
               Cx packets prove descriptor[2] = (payload | 0x80),
               NOT (command | 0x80). Three independent original
               MC68000 cases: CD 74 -> F4, CA 7E -> FE,
               CB 7F -> FF. The old code confused the command
               class selector with D2's preserved payload byte. */
            descriptor[2] = (uint8_t)(local.payload | 0x80u);
            local.handler_address = UINT32_C(0x00001e5c);
            if (sound_board_clear_c0_voices(
                    board, (uint8_t)(local.command & 0x0fu), &local
                ) != VF2_OK) {
                *report = local;
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
        } else {
            /* Primer-correct byte 2 (S2a): the drain leaves a3 at
               ring+cursor+1, so the head's d0 comes from ring byte 2. */
            vf2_status status = sound_board_dispatch_e0(
                board, descriptor, local.command, local.payload,
                ram[VF2_SOUND_COMMAND_RING_BASE +
                    ((cursor + 2u) & UINT16_C(0x0fffu))],
                &local
            );
            if (status != VF2_OK) {
                *report = local;
                return status;
            }
        }
    }
    ram[VF2_SOUND_COMMAND_COUNT] = (uint8_t)(count - 1u);
    cursor = (uint16_t)((cursor + 4u) & UINT16_C(0x0fffu));
    ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
    ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
    local.cursor_after = cursor;
    local.handled = 1;
    *report = local;
    return VF2_OK;
}

vf2_status vf2_sound_board_emit_command(
    vf2_sound_board *board,
    const uint8_t packet[4]
)
{
    uint8_t *ram = NULL;
    uint16_t cursor = 0u;
    size_t index = 0u;

    if (board == NULL || packet == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (board->sound_ram[VF2_SOUND_COMMAND_COUNT] == UINT8_MAX) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    ram = board->sound_ram;
    cursor = (uint16_t)(read_be16(ram + VF2_SOUND_COMMAND_WRITE_CURSOR) &
                        UINT16_C(0x0fffu));
    for (index = 0u; index < 4u; ++index) {
        ram[VF2_SOUND_COMMAND_RING_BASE +
            ((cursor + (uint16_t)index) & UINT16_C(0x0fffu))] = packet[index];
    }
    cursor = (uint16_t)((cursor + 4u) & UINT16_C(0x0fffu));
    ram[VF2_SOUND_COMMAND_WRITE_CURSOR] = (uint8_t)(cursor >> 8u);
    ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = (uint8_t)cursor;
    ++ram[VF2_SOUND_COMMAND_COUNT];
    return VF2_OK;
}

vf2_status vf2_sound_board_render(
    vf2_sound_board *board,
    int16_t *left,
    int16_t *right,
    size_t frames
)
{
    if (board == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_scsp_render(&board->scsp, left, right, frames);
}
