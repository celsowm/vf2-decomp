# v0755h: 0x2c38 saturation ambiguity - first observation

**Slice 9 of the advance plan execution chain.** This is a focused
unit test that TRIGGERS the saturation path inside `0x2c38` and
verifies the recovery's behavior at the saturation pixel. It does
NOT yet resolve the disasm-vs-executor ambiguity definitively.

## The ambiguity

The `subo 1, 0, g1` instruction at `0x2d40` (and its twins at
`0x2d6c`, `0x2d98`) is reached when the inner-loop accumulator
overflows the 8-bit saturation threshold:

```
block_00002d2c:
  00002d2c  addo     r7, r8, r8
  00002d30  shro     8, r8, g1
  00002d34  addo     r9, g1, g1
  00002d38  shlo     8, 1, r3            ; r3 = 256
  00002d3c  cmpobl   g1, r3, 0x00002d44   ; if g1 < 256 -> skip
  00002d40  subo     1, 0, g1             ; saturation
```

Two interpretations:
- **i960 disasm convention**: `g1 = 1 - 0 = 1`. Subsequent
  multiplication by 255, then `>> 7`, would yield `0x0001`.
- **executor convention** (`src/i960/executor.c:752-753`):
  `address = operand[1] - operand[0]` (operand order reversed).
  `g1 = 0 - 1 = 0xFFFFFFFF`. Subsequent multiplication by 255,
  then `>> 7`, yields `0x01FFFFFE` & `0xFFFF` = `0xFFFE`.

These are vastly different: `0x0001` vs `0xFFFE`.

## What v0755h does

Forces the saturation path on the FIRST outer iteration by setting
all six input bytes to 255 (max scale + max offset). Runs the
recovery once and inspects the first row's last inner entry (the
saturation point at inner_iter=46).

The test PASSES if the recovery produces the EXECUTOR's value
(`0xFFFE`). It FAILs with a clear message if the disasm value
(`0x0001`) or any other value appears. This documents the
current state: the recovery matches the executor, and this test
acts as a regression guard against any silent change.

## What v0755h does NOT do

This test does NOT resolve the ambiguity. Both interpretations
remain plausible. Resolution requires either:
- A live ROM-backed differential path that exercises `0x2c38`
  and compares recovered state against the original i960. The
  v0755c note flagged this as the "next step that requires a
  real snapshot that activates the per-step hook and verifies
  the F4 differential still FULL MATCHES". To my knowledge no
  such snapshot exists in the current corpus.
- An authoritative i960 reference (e.g., the i960 programmer's
  guide) or disassembly output from a known-correct disassembler
  applied to `subo 1, 0, g1`.

## Inputs chosen for the test

To force saturation immediately (outer_iter=0, inner_iter=46):
- `0x500235` (red scale byte) = 255
  → r7 = (28 * 1 * 255) / 18 = 396 (i960 `mulo r5=1, r7_byte, r7; mulo 28, r7, r7; divo 18, r7, r7`)
- `0x500234` (red offset byte) = 255
- `0x5000e0` (red mult byte) = 255

After 47 iterations of `addo r7, r8, r8`, r8 = 47 * 396 = 18612.
g1 = 18612 >> 8 = 72.
g1 += r9 = 72 + 255 = 327.
327 >= 256 → saturation triggers at the `cmpobl g1, r3, ...` branch.

The test reads the saturation pixel at `0x54612e + 46 * 6 =
0x54612e + 276 = 0x546242`. (The first row's last inner entry is
inner_iter = 46 = 47 - 1 since the loop initializes r6 = 47 and
decrements to 1.)

## Validated

- `git diff --stat src/`: only the new test + CMakeLists.txt +
  AGENTS.md (from earlier commits).
- ctest #new `vf2_subo_saturation_2d40`: PASS.
- ctest #existing `vf2_player_2c38`: still PASS.

## Future slice: resolution

A real ROM-backed differential that fires `0x2c38` would require:
1. A snapshot where the post-29414 task chain reaches `0x1fcc0`,
2. With `mode == 10` so the inline reaches the `0x2c38` call,
3. With inputs that force saturation on at least one pixel.

The current native-resume diff (F4, vt_individual_release) does
not exercise this path; the per-step hook's loop has yet to fire
in any measured scenario (DORMANT per v0755c).
