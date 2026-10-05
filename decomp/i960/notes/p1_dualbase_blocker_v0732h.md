# P1: the dual-base blocker is characterised, and a fighter1-inclusive trace now exists (v0732h)

**Evidence only. No behaviour change, no tuple admitted, no field promoted.**
This answers the P1 pickup item from
`p1_player_0x1680_block_stability_v0732.md` / `p1_taint_0x1680_block_v0730.md`,
which both deferred dual-base promotion to "a fighter1-inclusive trace that
does not exist yet". It also corrects one claim in the taint note.

## The premise, verified first

Both existing corpora are **fighter0-only**. Measured directly over the memory
events in the `0x510000..0x512000` and `0x520000..0x522000` windows:

```text
out/trace-both.jsonl    3122 memory events   fighter0: 341   fighter1: 0
out/trace-f0.jsonl      3075 memory events   fighter0: 336   fighter1: 0
out/fa_player_trace.jsonl 50008 events       fighter0:   1   fighter1: 0
```

So `base_count = 1` in the frontier output is correct, and the v0729/v0730
notes were right that promotion could not fire. That part is confirmed.

## Correction: the taint note's fighter1 lines are not from this corpus

`p1_taint_0x1680_block_v0730.md` quotes

```text
branch 0x000222b0 depends on: fighter1 + 0x01a4 bit 14
branch 0x000223c4 depends on: fighter0 + 0x01a4 bit 0
                              fighter1 + 0x05b8 bit 0
```

as surfacing from `out/trace-both.jsonl`. That trace contains **zero**
fighter1 accesses, so those dependencies cannot have been derived from it.
`tools/python/taint.py` takes fighter bases from the *scenario*
(`out/state8-posbit6-v0727.json`), not only from the trace, so a fighter1 tag
there is consistent with a scenario-supplied base rather than a measured
access. **The v0730 taint conclusions should be re-derived on a trace that
actually carries fighter1 accesses before they are relied on.** This note does
not re-derive them; it flags that the input was not what the note assumed.

## Why a naive "longer trace" does not work

Running the player park states forward with a large step budget stops
deterministically:

```text
out/park-player-270d4.vf2snap   ip=0x027cc8  1400 ins  10294 calls  unsupported
out/park-player-270d4b.vf2snap  ip=0x027cc8  1400 ins  10294 calls  unsupported
```

The stop is `cvtri r13, r13` at `0x00027cc8`, inside the 36-iteration
conversion tail at `0x27cc4..0x27cd8`:

```text
00027cc4  906cd000  ld       (g3), r13
00027cc8  6c68100d  cvtri    r13, r13
00027ccc  ca6cd000  stis     r13, (g3)
00027cd0  599cc804  addo     4, g3, g3
00027cd4  5a630b01  cmpdeco  1, r12, r12
00027cd8  14ffffec  bl       0x00027cc4
```

The last memory event before the stop reads address `0x1a8` with bits
`0x6005a115` (about 3.86e19), and `convert_real_to_integer` refuses at
`src/i960/executor.c:306` because the rounded value leaves
`[INT32_MIN, INT32_MAX]`. The preceding element at `0x1a4` held
`0x00003860` and converted to 0 normally.

Three things were checked before calling this a legitimate boundary rather
than a bug:

- **The decode is right.** The register-format opcode
  `((word >> 20) & 0xff0) | ((word >> 7) & 0x0f)` for `0x6c68100d` is
  `0x6c0`, which the project's own table maps to `cvtri` and which is CVTRI
  in the i960 REG opcode space. Operand assignment (`decoder.c` case `-20`,
  with `m1 = m3 = 0`) gives `r13, r13`, self-consistent with the surrounding
  loop that does `ld (g3), r13` immediately before.
- **The refusal is deliberate and already documented.**
  `fa_player_27ce0_gate_v0687.md` records that the `0x27cc8` slice was lifted,
  that it "confirms the existing 36-word `cvtri`/`stis` conversion tail in
  `hybrid_execute_player_27b5c`", and that "non-finite/overflow values remain
  unsupported". `player_27b5c_valid_degenerate_v0357.md` shows *why* these park
  states are degenerate: `F0+0x1a0` and `F0+0xbd8` are zero, so the ROM reads
  a selector from `*(0)` and the tail walks from a null scratch base - which is
  exactly why the loop is reading `0x1a4`/`0x1a8` instead of a fighter.
- **Guessing would violate the rules.** The i960 leaves out-of-range
  floating-point to integer conversion undefined, so any saturating or
  wrapping rule would be invented hardware behaviour (rule 1) replacing a
  fail-closed path (rule 2).

So: **the reference executor cannot produce a fighter1-inclusive trace from
the degenerate park states, and that is correct.** P1 is not blocked on
tooling.

## The unblock: a valid form, one patch away

`player_27b5c_valid_degenerate_v0357.md` documents the valid form, and the
documented witness command no longer reproduces from the on-disk snapshot
(`0x5101a0`, `0x510bd8` and the whole fighter window are zero in
`out/player-1428c-f0-s6.vf2snap` today). Reconstructing those two pointers -
`scratch_base` is read as `player + 0xbd8` per
`player_selector_scratch_locate.inc:43` - gives a run the reference completes:

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/player-1428c-f0-s6.vf2snap \
  --set-u32 0x00510bd8=0x00520000 --set-u32 0x005101a0=0x0201c2fc \
  --set-ip 0x000270d4 --set-reg g7=0x00510980 \
  --until 0x0002712c --max-steps 20000 --memory-trace --trace \
  > out/p1-f1-valid.jsonl
```

Result: `status ok`, `halt_reason "stop address"`, `ip 0x2712c`,
**9235 instructions**, 10302 calls. That is the same shape as the 9235-step
figure already pinned in `tests/recovered/test_player_4505_live.c:37`.

Fighter coverage - this is the half that did not exist:

```text
fighter0   2 accesses    2 distinct offsets   widths {4: 2}     offsets 0x0b20..0x1558
fighter1 660 accesses  300 distinct offsets   widths {2:180, 4:480}  offsets 0x01e0..0x068c
```

`infer_structs.py` attributes the fighter1 traffic to four writer IPs, all in
the already-known `0x27b5c` corridor:

```text
0x00027c40  0x00027c54  0x00027cc4  0x00027ccc
```

so this is the `cvtri`/`stis` conversion tail writing expanded packages into
fighter1's scratch slots, exactly the shape the v0357 note described
("slot `+0x1e0..+0x5a0` em `0x520000` | zeros | floats/pacotes expandidos").
The measured span is wider - `0x01e0..0x068c` - because the run covers the
whole 9235 instructions, not just the tail.

## What is still missing: promotion has not fired

This trace is **single-base** (fighter1). `infer_structs.py` reports
`bases=fighter1` on every candidate, so its dual-base promotion contract -
which requires the same offset from both `fighter0` and `fighter1` - does
**not** fire. A mirror attempt that patched `0x520bd8`/`0x5201a0` instead
produced byte-identical counts (660 fighter1, 1855 events, the same
`executed_instructions` 16288393), so the player base is not selected through
those pointers; the selection path is not yet identified.

Two consequences:

- The **0x1680 block remains single-base.** The new trace covers
  `0x01e0..0x068c`, a different region, so it neither confirms nor refutes
  the v0730 stability finding for `0x1680..0x1860`.
- Getting promotion to fire needs a run that touches **both** bases in one
  trace, or an identified way to re-point the selected player. The
  `0x27b5c` helper takes a single `player` pointer, so two runs at different
  bases give same-offsets-different-bases evidence *across* traces - which
  the current `infer_structs` contract does not consume, since it keys on
  `bases` within one trace's roll-up. Extending that contract to consume
  multiple traces is a real candidate, and it is the concrete next P1 step.

## Final count of what changed

Nothing in `src/`. The only additions are the three analysis scripts in
`out/` (gitignored): `basecov.py` (fighter-base coverage), `ipcov.py`
(per-IP attribution) and `grid.py` / `celldiff.py` from the v0732g slice.
All measurements above are reproducible from the commands shown.

## Anti-traps

- Do not treat the mirror run as evidence that the player base is selectable
  via `player + 0xbd8`; it is not, and the run proved that by being
  byte-identical.
- Do not promote `field_01e0`..`field_068c` to names like `pose` or `scratch`
  without independent evidence. They are the fighter1 side of a conversion
  tail; that is all that is measured.
- Do not re-run `taint.py` against `out/trace-both.jsonl` and read fighter1
  tags as measured - see the correction above.
- Do not add a saturating rule for out-of-range `cvtri` to unblock the
  reference executor. The hardware behaviour is undefined, the current
  refusal is correct, and the C side already recovers the tail.
