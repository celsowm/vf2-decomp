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

## v0907 — 0x24BE terminal release and voice-table update

A separate bounded C function now models the branch reached
from 0x23B0 when the stream bit0 is CLEAR, bit3 is SET,
remaining <=512 and MIDI7 is CLEAR. 0x24D2 uses the descriptor
byte +0x12 * 16 as its SCSP index; the recovered code masks
SCSP[index+1] by 0x9F and writes +0x1E to SCSP[index+6].
It consumes descriptor flag +0, halves the register index,
then updates sound-RAM 0x2800+index/2 voice status byte +2
(bit0 SET) and counter byte +0x0B (+0x50 when status bit3 set,
else +3), and finally writes +0x16 to SCSP 0x0408.

Only even byte indices corresponding to physical 32-voice
SCSP slots are admitted by the bounded helper. Other branches
such as 0x2524, bit7 gating, odd/out-of-domain indices and
unresolved scheduling are explicitly excluded. The owner-ROM
opt-in fixture now executes original 68000 from entry 0x2242
on a synthetic release descriptor and compares the resulting
voice, descriptor and SCSP effects byte-by-byte with native C.
Its result is a strict gate in `--verify-akira-walk` but is not
part of public CI because no private ROM is shipped.

## v0908 — owner-ROM executed differential evidence (2026-10-24)

The original owner ROM was recovered from the user's saved archive
`vf2(20261018-203049).zip`, rebuilt in a private local ROM folder,
and run against the release Linux binary produced by CI for commit
`537180d771964b865d49472906b02ec9e63ea41c`. Command:

```sh
vf2_m68k_l3_selection_tests /path/to/private/vf2-rom --verify-akira-walk
```

**The program exited 0 and printed**:

```text
AKIRA_SHORT_LOOP_ORIGINAL_DIFF matched=1 original_status=0 native_status=0 steps=85 original_next_src=00800480 original_next_remaining=00000f80 scsp0408=a5
AKIRA_SHORT_RELEASE_ORIGINAL_DIFF matched=1 original_status=0 native_status=0 steps=39 stop_pc=02242 voice_status=09 voice_count=54 scsp_key=1f scsp0408=42
AKIRA_NATIVE_PAGE_SUMMARY admitted=4 original_copy_slots=4 strict_errors=0
AKIRA_ORIGINAL_WALK_STRICT parity=1 passes=32 pages=4 steps=588 pcm_bytes_changed=2039
```

This is **actual C-vs-MC68000 differential execution** for the
ordinary four first-tick copies and two independent synthetic
entry-to-exit 68000 descriptors (short loop and release). It is
not an arcade timing claim. No proprietary ROM contents or
sample wave bytes were uploaded to GitHub. The owner-only strict
gate now also requires the number of *C-admitted original pages*
to equal the four 68000-observed pages; previously a regression
that refused all positive C paths could falsely pass this gate.

Remaining unsupported: the bit3-clear short-page 0x2524 branch,
odd/out-of-domain activation indices, some 0x260C paths and
actual SCSP/IRQ scheduler pacing. No automatic production
integration of the walker has been performed.

## v0909 — recovered 0x2522 / 0x260C real target (2026-10-24)

The original owner's ROM bytes make the 0x2522–0x260C arm
reachable when a short page (<=512 bytes) has both stream
flag bits 0 and 3 clear. Prior historical notes described 0x260C
as an unmapped entry: **incorrect**. The 0x25F6 BNE.W +0x14
explicitly targets 0x260C if the original mode byte bit7 is set.

The recovered routine is `vf2_sound_board_step_stream_short_terminal`:

- 0x2522–0x252C computes a BE16 terminal length as the low
  word of (destination - descriptor +0x0E + remaining), with
  ADD.W semantics (not a full 32-bit addition).
- 0x252E–0x25EE performs 16x 32-byte MOVEM sample copies from
  OLD source to OLD destination (512 bytes). It does **not**
  change the descriptor's source +2, destination +6 or remaining +A.
- When mode bit7 is clear, 0x25FA sets descriptor bit3,
  0x2600 adds 0x10 to mode and 0x2608 jumps to 0x265C.
- When mode bit7 is set, 0x25F6 branches to **0x260C**, which
  runs the SCSP slot-clear, slot +6 length write, descriptor
  release and voice status/countdown updates before 0x265C.

Added ROM-free tests and **owner-ROM original-68000 differential
fixtures** for no-release (mode 0x10) and immediate release
(mode 0x90). The owner-ROM strict gate will refuse parity failure.
Normal scheduling remains unchanged. The synthetic fixtures prove
field effects, not arcade timing.

## v0910 — terminal mode-gate differential extension

The owner-ROM short-terminal fixture now covers all four relevant
original mode-entry shapes: 0x10 (ordinary, defer release),
0x90 (bit7 set, immediate 0x260C release), 0x00
(requires SCSP 0x0409 bit7 set, and uses descriptor +0x0E
as the destination) and 0x80 (requires SCSP 0x0409 bit7
clear and immediately releases). Every variant starts at 0x2242
in the actual original 68000 stepper and is compared byte-for-byte
to the bounded C helper. This isolates exceptional hardware-flag
behavior from soundtrack cadence assumptions.

## v0911 — hardware-gated no-copy exits (0x2282 / 0x22B0)

The 68000's MIDI7 gates for special mode 0x00 (MIDI7 clear)
and mode 0x80 (MIDI7 set) branch straight to the shared
0x265C exit. They do not copy a page or mutate the stream,
but they **do write descriptor +0x16 to SCSP 0x0408**.
The bounded terminal native function now reproduces that side
effect rather than refusing with no changes. Two additional
owner-ROM fixture variants (six total) assert all descriptor,
sample-RAM, SCSP and voice-register comparisons. The ROM-free
test checks the absence of sample edits in both no-copy variants.
No scheduling claim is made.

## v0912 — owner-ROM measured terminal coverage

On 2026-10-24 the Linux `vf2_m68k_l3_selection_tests` binary from
the `81761ac` CI artifact was executed locally using the owner's
saved VF2 ROM (never committed, redistributed or embedded in CI):

```sh
vf2_m68k_l3_selection_tests /private/vf2-rom --verify-akira-walk
```

**Exit status: 0.** Exact original-stepper vs recovered-C comparisons
for six synthetic 0x2242 terminal variants:

| Variant | Mode / MIDI7 | 68000 instructions | Descriptor/PCM/SCSP/voice parity |
| --- | --- | ---: | --- |
| 0 | 0x10 / 0 | 81 | matched |
| 1 | 0x90 / 0 | 92 | matched |
| 2 | 0x00 / 1 | 92 | matched |
| 3 | 0x80 / 0 | 104 | matched |
| 4 | 0x00 / 0 (no-copy) | 23 | matched |
| 5 | 0x80 / 1 (no-copy) | 25 | matched |

The two original earlier synthetic fixtures still passed
(0x23B0 loop copy: 85 instructions; 0x24D2 no-copy release:
39 instructions). The original Akira first-tick 32-slot scan
also passed: `admitted=4`, `original_copy_slots=4`,
`strict_errors=0`, `steps=588`, `pcm_bytes_changed=2039`.

This proves only these branches on the supplied original ROM and
explicitly initialized test states. The real L2/SCSP IRQ scheduling
cadence, live 0x2242 preemption state, full song, DSP and effect
timing are not yet emulated cycle-accurately. No runtime scheduling
integration has been performed in this checkpoint.

## v0913 — 0x2258 negative timer, measured dispatcher handoff

The original 68000's 0x2242 entry begins with stream active-bit
test and a **16-bit** timer update at 0x224C–0x2254. A negative
signed sum writes the updated word at descriptor +0x1C
(0x2258) and takes the **0x225C jump to 0x07A6**, without
reaching the common 0x265C SCSP-master write.

`vf2_sound_board_step_stream_timer_handoff(board, slot)` now
models **only this bounded pre-dispatch handoff**. It refuses
inactive and positive-timer cases without mutation. The
dispatcher from 0x07A6, its IRQ interrupt state and realtime
timing remain separate. ROM-free regressions cover signed
16-bit wrap, unchanged SCSP byte, normal-timer refusal and
inactive descriptor refusal.

An additional strict owner-ROM fixture executes the original
MC68000 from 0x2242 **to the stop PC 0x07A6 (before fetch)**,
for three timer inputs. It compares the full 512KiB sound RAM,
SCSP registers, program counter and test status against the
native C operation. No sample-ROM bytes are committed.
This is a strict branch-level parity test, **not evidence
that the real scheduler calls the handoff at a given rate**.

## v0914 — full original empty-ring 0x2242 loop recurrence

Recovered the contiguous original main-loop path:
`0x2242 -> 0x2248/0x225C -> 0x07A6 -> 0x07AC ->
0x265C -> 0x2666 or 0x266E -> 0x2242`,
**only when the sound-command ring count at 0x2506 is zero**.
The original 0x07A6 `MOVE.B (A4),D0` reads exactly that count
(A4=0x2506). A nonzero count takes an unrecovered dispatcher,
so the new API explicitly refuses it.

`vf2_sound_board_step_stream_empty_dispatch(board, slot,
&next_a0)` implements the branch from an inactive descriptor,
or from an active negative-timer descriptor as recovered in
v0913. On the common `0x265C` exit it writes stream byte
+0x16 into SCSP 0x0408. Critically, `0x2662 BEQ` checks
the moved byte's Z flag: **byte zero resets A0 to 0x2000**,
while nonzero advances the current A0 by 0x20. This is NOT
an arbitrary 32-descriptor modulo wrap. A live positive
timer still belongs to the previously validated PCM branches.
The function is deliberately not called by host scheduling.

The owner-ROM strict suite adds four 68000 entry-to-next-entry
differentials: inactive vs negative-timer active and zero vs
nonzero exit byte. It compares **all sound RAM, SCSP registers
and next 68000 A0** against native C, not only the displayed
fields. An authenticated original 68000 loop transition is
now proven for the empty queue; real IRQ/CPU cycle cadence,
nonempty command ring, and preempted A0 remain open.
