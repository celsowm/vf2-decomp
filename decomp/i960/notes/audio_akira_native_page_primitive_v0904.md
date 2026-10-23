# v0904 — Native C bounded PCM page/cursor transfer

Date: 2026-10-23. This is a **partial** recovery, not a native 68000
scheduler or complete music playback.

## Proven original-ROM observation (v0903)

At original 68000 PC 0x2242 the sample walker processes 32 records,
each 0x20 bytes wide at RAM 0x2000 + slot*0x20. On the owner's
original-ROM first Akira music tick, active slot 0 copied a 512-byte
sample-ROM page and updated the record's BE32 source +2 and destination
+6 cursors:

- source 0x8f96d4 -> 0x8f98d4
- destination 0x10200 -> 0x10400
- original 32-descriptor walk also touched slots 3, 4 and 7.

This does not determine the exact original walker predicate, IRQ
frequency, alternate flags or when a page refill should be invoked.

## New C building block and ROM-free guards

`vf2_sound_board_copy_stream_page_512(board, slot_index)` addresses the
actual 0x2000 sample stream (not the 0x3000 music stream). It accepts
only an explicit active flag **0x80**, reads the two BE32 cursors,
reuses the previously recovered 16 x MOVEM 512-byte sample-ROM upload,
then advances both cursors only after a complete guarded copy.

The normal sound dispatcher and audio timer are unchanged.
Nothing performs prefill or invents a sample-page trigger.

`test_sound_board` now pins two sequential 512-byte transfers, slot-7
isolation, inactive/unknown flags, unaligned source, truncated sample
ROM and crossing the sound-RAM limit. Failure leaves cursors and
destination unchanged.

## Remaining original-versus-native proof

Use the owner's ROM to differential-test this transfer under identical
RAM/SCSP state, then recover the actual 0x2242 predicate and scheduler.
The 10-second Akira WAV from v0903 still uses the **original 68000
walker**, not this native C helper. Do not claim that this change has
recovered the full song, DSP, hardware-correct timing or native stream
progression. No game ROM, original PCM, WAV or snapshot is committed.
