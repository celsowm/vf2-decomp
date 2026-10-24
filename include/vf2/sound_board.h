#ifndef VF2_SOUND_BOARD_H
#define VF2_SOUND_BOARD_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/scsp.h"
#include "vf2/status.h"

enum {
    VF2_SOUND_RAM_BYTES = 0x080000u,
    /* Effective sound-RAM addresses after the ROM's A6 base (0x1000). */
    VF2_SOUND_COMMAND_RING_BASE = 0x001000u,
    VF2_SOUND_COMMAND_COUNT = 0x002506u,
    VF2_SOUND_COMMAND_WRITE_CURSOR = 0x002504u,
    VF2_SOUND_COMMAND_CURSOR = 0x002508u,
    VF2_SOUND_CURRENT_COMMAND = 0x00250eu,
    VF2_SOUND_LOOKUP_CURRENT = 0x00250fu,
    VF2_SOUND_LOOKUP_MODE = 0x002510u,
    VF2_SOUND_LOOKUP_CONTROL = 0x002511u,
    VF2_SOUND_LOOKUP_WORD_ALT = 0x002512u,
    VF2_SOUND_LOOKUP_BYTE = 0x002515u,
    VF2_SOUND_LOOKUP_WORD = 0x002516u,
    VF2_SOUND_LOOKUP_META = 0x002518u,
    VF2_SOUND_LOOKUP_RELEASE = 0x00250du,
    VF2_SOUND_SHARED_FACTOR_151F = 0x00251fu,
    VF2_SOUND_CHANNEL_TABLE_BASE = 0x004200u,
    VF2_SOUND_CHANNEL_CONTROL_BASE = 0x004220u,
    VF2_SOUND_CHANNEL_CONTROL_ALT_BASE = 0x004240u,
    /* Stream files live at absolute 0x3000/0x3020 (driver a2 bases
       0x2000(a6)/0x2020(a6) with A6 = 0x1000); absolute 0x2000 is the
       separate sample-upload control area and is unmodeled. */
    VF2_SOUND_DESCRIPTOR_BASE = 0x003000u,
    VF2_SOUND_STREAM_ALTERNATE_BASE = 0x003020u,
    VF2_SOUND_STREAM_DESCRIPTOR_BYTES = 0x20u,
    VF2_SOUND_STREAM_DESCRIPTOR_COUNT = 6u,
    VF2_SOUND_STREAM_MAINTENANCE_COUNT = 8u,
    VF2_SOUND_COMMAND_DESCRIPTOR_BASE = 0x004000u,
    VF2_SOUND_COMMAND_DESCRIPTOR_BYTES = 0x10u,
    VF2_SOUND_COMMAND_DESCRIPTOR_COUNT = 20u,
    /* L3 UART receiver ring files ($1500 page, a6-relative on the
     * 68k side): the filed staging bytes at $1500-$1503, the 12-bit
     * write cursor ($1504, shared with emit_command), the shared
     * count ($1506), and the $1526 dance flag. The assembled ring
     * entries land in the 0x1000 command ring the drain consumes
     * ($0000(a6) destination = absolute 0x1000). */
    VF2_SOUND_RING_STAGE_BASE = 0x002500u,
    VF2_SOUND_SHARED_COUNTER_1526 = 0x002526u,
    VF2_SOUND_VOICE_TABLE_BASE = 0x002800u,
    VF2_SOUND_SHARED_COUNTER_151E = 0x00251eu,
    VF2_SOUND_SHARED_COUNTER_152E = 0x00252eu,
    VF2_SOUND_VOICE_BYTES = 0x10u,
    VF2_SOUND_VOICE_COUNT = 32u,
    /* Voice-record layout (v0825 V3b, 16 bytes, stride 0x10 x32).
     * Every name is behaviorally grounded in >=2 committed
     * recoveries; bytes 0x07/0x0d-0x0f are untouched by all of
     * them and stay unnamed (honest gap, not padding claimed). */
    VF2_SOUND_VOICE_SLOT = 0x00u,
    VF2_SOUND_VOICE_FLAGS = 0x02u,
    VF2_SOUND_VOICE_BYTE = 0x03u,
    VF2_SOUND_VOICE_CHANNEL = 0x04u,
    VF2_SOUND_VOICE_AGE_RELEASE = 0x05u,
    VF2_SOUND_VOICE_SELECT = 0x06u,
    VF2_SOUND_VOICE_MODE = 0x08u,
    VF2_SOUND_VOICE_AGE = 0x09u,
    VF2_SOUND_VOICE_META = 0x0au,
    VF2_SOUND_VOICE_RATE = 0x0bu,
    VF2_SOUND_VOICE_SAMPLE = 0x0cu,
    VF2_SOUND_AUDIO_ROM_BASE = 0x600000u,
    VF2_SOUND_SAMPLE_ROM_BASE = 0x800000u,
    VF2_SOUND_SAMPLE_DIRECT_BYTES = 0x200000u
};

typedef struct vf2_sound_voice_maintenance_report {
    size_t voices_scanned;
    size_t voices_expired;
    size_t normal_expirations;
    size_t release_expirations;
    size_t voices_aged;
    uint8_t counter_151e_before;
    uint8_t counter_151e_after;
    uint8_t counter_152e_before;
    uint8_t counter_152e_after;
} vf2_sound_voice_maintenance_report;

typedef struct vf2_sound_stream_maintenance_report {
    /* L2 stream tick (ROM 0x1f16, vector 26) diagnostics, measured
       v0852. descriptors_scanned = worker calls a completed walk made
       (8; 0 when the ring-count gate skipped the body). timers_expired
       = countdowns that reached zero this tick (the first such body is
       unmeasured, so a nonzero value comes with a refusal).
       packets_emitted = prescaler beats filed: 0 while the emit body
       is refused (never half-applied). escape_records /
       pointer_handoffs / control_updates / bytes_consumed are retired
       from the removed per-descriptor model and stay 0. Refusal
       detail: refused_stage 0 = none, 1 = prescaler beat emit
       (0x21c2-0x2206), 2 = worker applier (0x1f92-0x1fba);
       refused_record = record index 0..7 (the prescaler lives in
       record 0's a2), 0xff when none. On a refusal the tick applied
       no mutation. */
    size_t descriptors_scanned;
    size_t timers_expired;
    size_t packets_emitted;
    size_t escape_records;
    size_t pointer_handoffs;
    size_t control_updates;
    size_t bytes_consumed;
    uint8_t refused_stage;
    uint8_t refused_record;
    uint16_t write_cursor_before;
    uint16_t write_cursor_after;
} vf2_sound_stream_maintenance_report;

typedef struct vf2_sound_dispatch_report {
    uint8_t command;
    uint8_t payload;
    uint8_t command_class;
    uint16_t cursor_before;
    uint16_t cursor_after;
    size_t descriptors_scanned;
    size_t descriptors_matched;
    size_t descriptors_updated;
    size_t voices_cleared;
    size_t voices_matched;
    size_t voices_allocated;
    uint8_t channel;
    uint8_t selected_value;
    int16_t signed_value;
    uint32_t lookup_word;
    size_t voices_updated;
    uint32_t handler_address;
    /* 1 when dispatch_next consumed the entry itself (count--,
       ring cursor += 4); the session pump advances only when 0. */
    int handled;
} vf2_sound_dispatch_report;

typedef struct vf2_sound_board {
    uint8_t sound_ram[VF2_SOUND_RAM_BYTES];
    const uint8_t *audio_rom;
    size_t audio_rom_size;
    const uint8_t *sample_rom;
    size_t sample_rom_size;
    uint16_t control;
    vf2_scsp scsp;
    /* Opt-in only: complete the ROM-verified D92->DF8->ED6
     * 512-byte upload and KEYON directly inside dispatch_next
     * when the mapped source, destination and slot pass guards.
     * Default zero keeps unsupported engine legs fail-closed.
     * NOT a claim of naturally triggered i960 gameplay SFX. */
    uint8_t allow_verified_d92_copy;
    /* Explicit opt-in ONLY for the owner-ROM-differentiated, frame-342
     * original 9E 0A 7F dispatch through allocator FAC -> A8C
     * with an idle slot-0 voice and full owner audio/sample bank.
     * Default OFF preserves historical FAC refusal, including
     * allocator arrivals with unverified D4/SCSP addressing. */
    uint8_t allow_verified_90_allocator;
} vf2_sound_board;

vf2_status vf2_sound_board_initialize(
    vf2_sound_board *board,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
);

/* Recovered ORIGINAL 68000 sample upload at 0xCB2..0xD72 only.
 * Exactly 16 MOVEM.L (A0)+,D0-D7 / MOVEM.L D0-D7,(A2) /
 * ADDA.W #32,A2 triplets = 512 bytes, 48 original instructions.
 * Bounds/alignment and attached sample ROM are checked before writing.
 * Inputs/outputs are emulated A0 and A2 bus addresses; their pointers
 * advance by 512 only on success. The following engine body at 0xD72
 * and live playback are NOT recovered or admitted by this API. */
vf2_status vf2_sound_board_copy_engine_page_512(
    vf2_sound_board *board,
    uint32_t *source,
    uint32_t *destination
);

/* Bounded native C sample page/cursor transfer for one 0x80-active
 * 0x2242 walker record. This is NOT the original scheduler/predicate:
 * callers must independently establish when a transfer is due. */
vf2_status vf2_sound_board_copy_stream_page_512(
    vf2_sound_board *board, size_t slot_index
);

/* Original 68000 0x2242..0x225E active stream negative-timer
 * handoff. The CPU performs a 16-bit wrapping sum of descriptor
 * +0x1C and +0x1A. Only if the signed result is negative does
 * it store the updated word at +0x1C before jumping to the
 * 0x07A6 command dispatcher. This bounded helper reproduces
 * exactly the pre-dispatch store, not the downstream dispatcher,
 * its scheduling cadence, or any SCSP write. Positive/idle cases
 * are refused without mutation (handled by other walker arms). */
vf2_status vf2_sound_board_step_stream_timer_handoff(
    vf2_sound_board *board, size_t slot_index
);

/* ROM 0x2242 -> 0x07A6 -> 0x265C -> 0x2666/0x266E ->
 * 0x2242: one complete *empty command ring* main-loop hop.
 * Handles inactive descriptors (no timer change) and active
 * negative-timer descriptors (+1C word store); refuses a live
 * positive timer or any pending command. Applies original SCSP
 * 0x0408 write, and reports the next original A0 address:
 * +0x16 == 0 resets A0 to 0x2000, otherwise A0 += 0x20.
 * No audio timer cadence or nonempty dispatcher is inferred. */
vf2_status vf2_sound_board_step_stream_empty_dispatch(
    vf2_sound_board *board,
    size_t slot_index,
    uint32_t *next_descriptor_address
);

/* Original 68000 0x2242 -> 0x22B8 -> 0x22CA full-page arm.
 * Recovers only the *positive, full 512-byte* branch, including the
 * timer, page-mode, SCSP 0x0409 gating, sample copy and descriptors.
 * Inactive, countdown-underflow, MIDI-gated, and <=512-byte
 * remaining-length arms deliberately refuse without side effects.
 * Successful copies also write desc +0x16 to SCSP 0x0408 (0x265C).
 * Not called by normal scheduling; no claim of cycle-exact timing. */
vf2_status vf2_sound_board_step_stream_full_page(
    vf2_sound_board *board, size_t slot_index
);

/* Original 68000 0x23B0 short/looping PCM page arm:
 * Only selected if stream[0] bit 0 is SET and remaining <= 512.
 * After copying 512 bytes from current source to current destination,
 * records next source (+2) from the masked loop start (+0x12)
 * plus (512 - remaining) and next remaining (+0x0A) from masked
 * loop length (+0x16) minus that offset. 16-bit timer, mode, MIDI
 * gating and SCSP 0x0408 write match the positive 0x2242 entry.
 * Unrecovered 0x24BE flag-clear arm is NOT guessed or scheduled. */
vf2_status vf2_sound_board_step_stream_short_loop_page(
    vf2_sound_board *board, size_t slot_index
);

/* Verified 68000 0x23B0 -> 0x24BE -> 0x24D2/0x251E release arm.
 * A terminal (remaining <=512) active 0x88 voice with descriptor
 * bit3 SET, bit0 CLEAR, and SCSP MIDI bit7 clear, releases its
 * active flag, updates the selected SCSP slot and voice-table
 * countdown, then writes desc+0x16 to SCSP 0x0408.
 * No page is copied. Normal audio scheduling is not changed. */
vf2_status vf2_sound_board_step_stream_short_release(
    vf2_sound_board *board, size_t slot_index
);

/* ROM 0x24BE -> 0x2522..0x260A: terminal non-loop PCM page.
 * A +0x00=0x80 stream and remaining<=512 copies the final 512-byte
 * page WITHOUT advancing +2/+6 or decreasing remaining; +0x1E
 * receives (destination - +0x0E + remaining) low 16 bits.
 * If mode bit7 is clear, mark bit3 active and increment mode by 0x10.
 * If mode bit7 is set, follow the 0x260C release tail instead:
 * update SCSP slot+1/+6, release stream, update voice status/countdown.
 * Both terminate via the observed 0x265C SCSP master byte write.
 * Mode 0x00 with MIDI7 clear or mode 0x80 with MIDI7 set branches
 * directly to 0x265C without copying or changing the descriptor,
 * but still writes SCSP 0x0408. Requires explicit call: scheduler/IRQ cadence remains unrecovered. */
vf2_status vf2_sound_board_step_stream_short_terminal(
    vf2_sound_board *board, size_t slot_index
);

/* Explicitly execute one recovered original 0x2242 -> 0x2242
 * descriptor iteration with empty command ring 0x2506.
 * Chooses timer/idle handoff, MIDI7 skip, full page, short-loop,
 * terminal page or voice release according to the original 68000
 * branch conditions. Returns the exact next A0; does NOT infer
 * the original CPU/SCSP interrupt rate or schedule anything. */
vf2_status vf2_sound_board_step_stream_walk_one(
    vf2_sound_board *board, size_t slot_index,
    uint32_t *next_descriptor_address
);

/* Original 68000 tail at 0xD72..0xD90 (seven instructions).
 * Writes the post-copy A0/A2 pointers to the channel stream,
 * sets SCSP KEYON bits (read/OR/write), and sets stream+0 = 0x80.
 * Only for a previously validated 512-byte sample-page copy.
 * A1 is identified by a valid voice index; A3=0x2000, A5=SCSP base.
 * No dispatcher, RTS, game trigger or audio PCM is implied. */
vf2_status vf2_sound_board_finalize_engine_copy_d72(
    vf2_sound_board *board,
    size_t voice_index,
    uint32_t sample_end,
    uint32_t destination_end
);

/* Alternate original 68000 engine tail 0xEB8..0xED6 (7 instructions).
 * Same pointer/SCSP KEYON writes as 0xD72..0xD90, but stream+0=0x81.
 * The caller must have completed and validated the 512-byte page at
 * 0xDF8..0xEB8. No dispatcher or live-event semantics are implied. */
vf2_status vf2_sound_board_finalize_engine_copy_eb8(
    vf2_sound_board *board,
    size_t voice_index,
    uint32_t sample_end,
    uint32_t destination_end
);

vf2_status vf2_sound_board_read_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t *value
);

vf2_status vf2_sound_board_write_u8(
    vf2_sound_board *board,
    uint32_t address,
    uint8_t value
);

vf2_status vf2_sound_board_read_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t *value
);

vf2_status vf2_sound_board_write_u16(
    vf2_sound_board *board,
    uint32_t address,
    uint16_t value
);

/* Execute the instruction-aligned 0x5fe voice-maintenance handler. */
vf2_status vf2_sound_board_maintain_voices(
    vf2_sound_board *board,
    vf2_sound_voice_maintenance_report *report
);

/* Execute the bounded 0x1f7c stream-interpreter packet paths. */
vf2_status vf2_sound_board_maintain_streams(
    vf2_sound_board *board,
    vf2_sound_stream_maintenance_report *report
);

/* Consume one ROM-format four-byte command-ring entry. */
vf2_status vf2_sound_board_dispatch_next(
    vf2_sound_board *board,
    vf2_sound_dispatch_report *report
);

/* Run the ROM 0xa8c setup-authoring arm on one descriptor/voice
 * pair (S3c): the shared head (+2 seed, slot key, +6 word) then
 * the HIGH (desc[2] >= 0xf0) or LOW authoring branch. Effects
 * applied, UNSUPPORTED at the 0xc1e common-tail door.
 * Indices (not pointers) keep RAM ownership inside the board.
 * Drain-unreachable bodies (LOW branch, ch-0xf seeding) are
 * proven through this entry with synthetic-entry differentials;
 * dispatch_next drives the HIGH/ch!=0xf shape. */
vf2_status vf2_sound_board_setup_author(
    vf2_sound_board *board,
    size_t descriptor_index,
    size_t voice_index,
    vf2_sound_dispatch_report *report
);

/* Emit one four-byte packet through the ROM stream-interpreter ring state. */
vf2_status vf2_sound_board_emit_command(
    vf2_sound_board *board,
    const uint8_t packet[4]
);

/* File one received UART byte the way the ROM L3 receiver ISR
 * (0x526, autovector 26) does: 0xff is the sound-CPU reboot door
 * (refused cleanly -- reboot is out of scope); >= 0xf0 is
 * ignored with no state change; otherwise the $1500 ring file,
 * $1502 age, $1503 file, $1504 cursor, $1506 count and the $1526
 * dance update run, with the $400001 system-port poke deferred
 * (unmodeled port, cf04 precedent: body applied, poke skipped).
 * The byte is the ISR's already-read d0 (the $0405(a5) hardware
 * read stays on silicon). */
vf2_status vf2_sound_board_file_uart_byte(
    vf2_sound_board *board,
    uint8_t byte
);

vf2_status vf2_sound_board_render(
    vf2_sound_board *board,
    int16_t *left,
    int16_t *right,
    size_t frames
);

#endif
