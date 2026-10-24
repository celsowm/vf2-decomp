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

## v0906 — 0x23B0 terminal/looping page

The owner sound ROM's 0x23B0–0x24BC bytes reveal a distinct
**512-byte page copy with loop-cursor reset**, selected only when
remaining length is <= 0x200 and descriptor flag bit 0 is set.
The already decoded prefix at 0x2242 supplies active flag, signed
16-bit timer and SCSP 0x0409 mode gates. Original 68000 instructions:

- `23B0 BTST #0,(A0)` / `23B6 BEQ 24BE` — flag-clear branch
  stays unsupported.
- `23BA NOT.W D5; 23BC ADDI.W #0x0201,D5; 23C0 ANDI.L #0xFFFF,D5`
  computes `padding = 512 - remaining` for original remaining 0..512.
- `23C6 MOVE.L +0x12(A0),D4; 23CA ANDI.L #0x00FFFFFF,D4;
  23D0 ADD.L D5,D4` sets the next source to masked loop-start
  plus padding.
- `23D2 MOVE.L +0x16(A0),D3; 23D6 ANDI.L #0x00FFFFFF,D3;
  23DC SUB.L D5,D3` sets next remaining bytes to masked loop
  length minus padding (32-bit arithmetic).
- `23DE–23F2` updates mode (+0x10), 16-bit timer, new source,
  remaining; `23F6–24B2` still copies a **full 512 bytes from the
  original source A1**; `24B6` stores the destination end, and the
  common 0x265C SCSP write follows.

Added separate opt-in C helper
`vf2_sound_board_step_stream_short_loop_page()` plus ROM-free
regression checks and inclusion in the owner-ROM per-descriptor
validation. This function is intentionally not called by the normal
sound scheduler. The 0x24BE/0x24D2/0x260C activation/release
branches remain held, as do real-time scheduling and hardware-perfect
mixing. A successful copy does not prove those other semantics.

Owner-ROM copy detection deliberately uses the destination cursor
(+6) equaling the mode-selected pre-step destination plus 512,
not a change in the source cursor (+2). The 0x23B0 loop arm can
reset +2 back to an earlier source, possibly to the exact previous
value; requiring source inequality would silently miss real copies.

Address correction from direct owner-ROM word offsets: the
`BTST #0,0(A0)` occupies 0x23B0..0x23B4, and the word-displacement
`BEQ 0x24BE` starts at 0x23B6. The final 16th MOVEM ends
at 0x24B6, `MOVE.L A2,+6(A0)` starts at 0x24B6,
`BRA 0x265C` starts at 0x24BA. 0x24BC holds that BRA's
extension word; `BTST #3,(A0)` starts at 0x24BE.
This is a PC-label documentation correction only; C branch
conditions were unchanged.
