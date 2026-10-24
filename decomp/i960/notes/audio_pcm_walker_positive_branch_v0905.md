# v0905 — 68000 0x2242 positive full-page walker, native C

Date: 2026-10-23. Source of opcode evidence: the owner's
`vf2(20261018-203049).zip` verified sound EPR `epr-17574.30`,
LOAD16_WORD_SWAP, interpreted as big-endian 68000 words. No ROM
bytes/samples/WAV committed.

## Newly resolved original branch control

| PC | Original 68000 behavior | C consequence |
| --- | --- | --- |
| 0x2242–0x2248 | stream[0] bit7 off -> dispatcher 0x07a6 | inactive refused |
| 0x224c–0x225c | word +1c plus word +1a; negative -> file +1c, dispatcher | negative timer refused atomically |
| 0x2260–0x228a | +1 mode zero -> require SCSP 0x0409 bit7 set; take A2 from +0e | alternate target |
| 0x228e–0x22b4 | mode 0x80 -> require SCSP bit7 clear; other modes pass | regular +6 target |
| 0x22b8–0x22c8 | source +2; remaining long +0a; <=0x200 -> 0x23b0 | short path held |
| 0x22ca–0x22e2 | remaining -= 0x200; mode += 0x10; timer &= 0x7fff | count and state |
| 0x22e4–0x23a8 | 16 x MOVEM 32 bytes; publish source +2, destination +6 | exact 512-byte page |
| 0x265c | write descriptor byte +0x16 to SCSP 0x0408 | shared exit-side write |

This proves the **shape** of the positive full-page arm, but neither
full main-loop invocation timing nor other 0x23B0/0x24BE/0x260C
paths. Earlier v0802 prose treated the add as longword; actual
`D068 001A` is `ADD.W (0x1a,A0),D0`; the condition is 16-bit
signed.

## Implemented boundary

`vf2_sound_board_step_stream_full_page(board, slot_index)`
recovers the above **positive branch** for one explicit slot.
It calls the already guarded 512-byte sample-ROM-to-sound-RAM MOVEM
transfer. State changes are committed only once that copy succeeds.
The *normal* audio pump and auto-scheduling remain unchanged.
It refuses unsupported negative timers, gated modes, inactive
records, short/terminal pages and invalid buffers without changing
any RAM, rather than claiming to reproduce their alternate branches.

ROM-free assertions check ordinary +6 target, mode-zero +0e
alternate target and SCSP gate, mode-0x80 inverse gate, wrapping
mode increments, timer and remaining-count fields, page bytes,
and fail-closed boundaries.

## Verification status

The opcode/table addresses were inspected from the owner's ROM.
Synthetic ROM-free behavior is covered by `vf2_sound_board` CI.
**Owner-ROM C-vs-68000 full-record differential and original
scheduler cadence remain separate pending gates**. No sound track
is claimed complete or hardware-accurate.

## Owner-ROM per-descriptor differential instrument

The existing `--probe-akira-walk` / `--verify-akira-walk` harness
now compares all **32 individual original 68000 iterations** to the
new bounded C full-page operation, each with its own pre-step sound-RAM
and SCSP snapshot. An admitted C positive branch must reproduce the
entire 0x20-byte descriptor plus its selected 512-byte destination
page and must not claim a copy when the original skipped it. Any
positive mismatch marks the opt-in owner-ROM strict gate failed.
Unimplemented real-copy branches are explicitly reported as
`AKIRA_NATIVE_FULL_PAGE_UNCOVERED`, not counted as matches.
The hook is committed but **its outcome has not been measured locally
against the owner ROM**; the public CI does not ship those ROM bytes.

The recovered full-page arm now includes the original shared-exit
SCSP write at PC 0x265C. The ROM-free test pins that write on three
admitted mode branches and verifies it is absent on refusals. The
owner-ROM per-descriptor differential additionally compares SCSP
register byte 0x0408, alongside descriptor metadata and PCM bytes.
