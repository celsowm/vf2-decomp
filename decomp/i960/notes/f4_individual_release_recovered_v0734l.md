# v0734l: F4 is FULL MATCH — three INDIVIDUAL releases, native vs reference

**Closes Phase 2.6 / F4 of `completion_plan_v0734.md`.**

v0734j recorded F4 as "bounded and ready for a focused slice" — but the
v0732g work had already produced the three released fixtures
(`indk-c`, `indp-c`, `indk2-c`) and proven them FULL MATCH manually.
This slice (a) re-measures all three on the current build, (b) wires
them into a ctest entry, and (c) closes the phase.

## What F4 actually is

Per `f3_f4_individual_mode_measured_v0732.md`: INDIVIDUAL walks at
rows 1-2 only (rows 3-5 are unreachable by walking in INDIVIDUAL mode
— row 2's down-neighbour is row 0, and the step also exits INDIVIDUAL
mode). The proven set is therefore three frames:

| snapshot | description | measured at v0732g |
|---|---|---|
| `out/indk-c.vf2snap` | row-1 INDIVIDUAL KICK release | 4293 ins / 38 calls |
| `out/indp-c.vf2snap` | row-1 INDIVIDUAL PUNCH release | 4293 ins / 38 calls |
| `out/indk2-c.vf2snap` | row-2 INDIVIDUAL KICK release | 4294 ins / 38 calls |

All three fixtures were built from existing idles with **no patched
state**, per v0732d's recipe (`ind-1-c` -> `ind-b-c` -> `ind-c-c`
toggle, then a single nav-down to row 2). The 38 call delta is the
whole `0x9ff8 -> 0xa010` frame: 32 calls in the recovered
`phase17_bit7_index5` body + 6 calls in the 232-instruction native tail.

## What this slice measures

For each of the three fixtures, both legs run from the same .vf2snap
and exit at `0xa010`. The differential contract is instruction count +
call count.

**Reference leg** = `vf2probe --until 0x9ff8` to get the baseline,
then `vf2probe --until 0xa010` to get the end counters. Delta is
compared.

**Native leg** = `vf2i960 native-resume` to `0xa010`. The tool already
prints the frame count.

Both legs must match each other AND match the published `4293/38`,
`4293/38`, `4294/38`.

### Re-measured on this build

```text
-- row-1 INDIVIDUAL KICK release (indk-c.vf2snap)
   reference: instructions=4293 calls=38
   native   : instructions=4293 calls=38
   ok: FULL MATCH (4293/38)
-- row-1 INDIVIDUAL PUNCH release (indp-c.vf2snap)
   reference: instructions=4293 calls=38
   native   : instructions=4293 calls=38
   ok: FULL MATCH (4293/38)
-- row-2 INDIVIDUAL KICK release (indk2-c.vf2snap)
   reference: instructions=4294 calls=38
   native   : instructions=4294 calls=38
   ok: FULL MATCH (4294/38)

3/3 INDIVIDUAL releases FULL MATCH
```

The total wall time was 15.26 s — each `vf2probe` run is ~2-3 s of
booting the i960 reference from the snapshot.

## What this slice does NOT claim

- **It does not claim rows 3, 4 or 5.** Those rows are unreachable
  by walking in INDIVIDUAL mode (v0732d); the `nega5-3/4/5` probes
  are fail-closed at the `0xa6c0` gate.
- **It does not claim the `a5 = 4` INDIVIDUAL release.** That row is
  fail-closed by the chute-slot gate (v0732c).
- **It does not claim the `a7 = 5` MANUAL SETTING nested editor.**
  Deferred.

## What this slice changes

1. New test entry: `vf2_f4_individual_release` (ctest #123).
   Three rows, runs `vf2probe` (reference) and `vf2i960 native-resume`
   (native) on each, asserts equality on instruction + call count and
   against the published numbers.

2. New note: `decomp/i960/notes/f4_individual_release_recovered_v0734l.md`
   (this file).

3. `CMakeLists.txt`: a new `add_test(NAME vf2_f4_individual_release ...)`
   call and a 900-s TIMEOUT group with the existing factory chain tests.

4. New script: `tools/python/test_f4_individual_release.py`.

## Validated

- `ctest -C Debug -R vf2_f4_individual_release`: **PASS in 15.26 s**.
- Both legs verified at the published `4293/38`, `4293/38`, `4294/38`.
- Three negative controls:
  - Plant a wrong `want_ins` of 4295 (off by 1): test FAILS on row-2.
  - Plant a wrong `want_calls` of 39: test FAILS on all three rows.
  - Plant a comparison swap (`ref["instructions"]` vs `ref["calls"]`):
    test FAILS on all three rows.
- The test entry does **not** depend on `vf2probe --trace` output —
  just the run-counter JSON. That is the same mechanism vf2probe uses
  for its own differential runs.
- `git diff --stat src`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- The full `ctest -E vf2_player_4505` suite is unchanged (still
  119/119 PASS) — the new test entry is added on top.

## What the next slice should pick up

- **Phase 2.3-2.5** (warm-only admissions). The fence here is
  similar: a one-conditional filter (`coin_flags & 1`) for INDIVIDUAL
  and a 7-instruction `a5=4` delta for the row-3+ rows. The
  recovered `phase17_bit7_index5` function already handles both
  (v0732g wired them in), so this is primarily a measurement pass.
- **Phase 1.2a** is closed (v0734i); the texture cluster overlap is
  legitimate, `interrupt_return_wait_exit` is shrunk, and the
  identity-open rows are annotated.
- **Phase 3 / P3-P4** is the next bulk-boundary work downstream of
  `fa_player`. The F4 slice does not depend on it.
- **Phase 4** is the largest remaining bulk (hitboxes/hurtboxes,
  collision, damage/combos, ring-out, fighter physics, CPU logic).
  Fully unstarted.
- **Phase 5** is semantic naming, evidence-gated.