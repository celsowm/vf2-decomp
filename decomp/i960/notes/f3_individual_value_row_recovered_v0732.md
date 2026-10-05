# v0732g: the INDIVIDUAL value-row render was already recovered; only the gate and the call split were missing

**Slice: F3 remainder. The INDIVIDUAL post-edit release on value row 2 is now
admitted and proven. Rows 1, 3, 4 and 5 of that leg are all resolved as
either measured or fail-closed.**

The v0732f note closed with "the render is NOT recovered" and a 510-vs-416
write-count discrepancy. That framing was wrong. The render was already
recovered; the comparison was against the wrong baseline.

## The 510-vs-416 discrepancy was a baseline error

v0732f compared `out/indk2-rel.jsonl` against `out/f2-r4-rel.jsonl` - an
INDIVIDUAL row-2 release against a **COMMON row-4** release. Two variables
moved at once, so the reported row-6, row-11 and row-13 deltas were the
row-4/row-2 chute difference wearing a mode difference's clothes.

Re-run against the correct control - the COMMON **row-2** KICK release,
`out/f2r2k-rel.jsonl`, same row and same input, mode the only variable - the
INDIVIDUAL render differs on exactly **12 rows**, and every one of them was
already mode-aware in `execute_frame_phase17_bit7_index5`:

```text
COMMON: 403 cells / 417 writes      INDIVIDUAL: 498 cells / 510 writes

row  5  "COIN CHUTE TYPE  COMMON"    vs  "COIN CHUTE TYPE  INDIVIDUAL"
row  7  1P CONTINUE 1 CREDIT         vs  1P CONTINUE 2 CREDITS
row 24  COIN CHUTE #2  COINS CREDITS vs  (all blank, cols 16-28)
rows 25-33 (blank 0x8020 / absent)   vs  cols 30-47 = 0x0020
```

- **Row 5** is `runs[]`'s mode-scoped label, already `{5,35,"    COMMON",0}`
  and `{5,35,"INDIVIDUAL",1}`. Both are 10 cells at column 35, so the longer
  word needs no trailing erase.
- **Row 7** is *not* a mode difference. The two snapshots have different
  credit state (`credits[2]` = 1 in `f2r2k`, 2 in `indk2`), and the existing
  `credit_cells` render already draws that digit plus the plural 'S' at
  column 48.
- **Row 24 and rows 25-33** are the INDIVIDUAL erase that has been in the
  function since the v0727 corridor: 13 space glyphs at row 24 column 16, then
  `write_phase17_tile_run(..., 0x0020, 18)` over rows 24-33 columns 30-47.

The 162 "extra" writes v0732f counted are simply those 10 rows x 18 cells
being written in INDIVIDUAL and left alone in COMMON. The cell encoding is
`0x8000 | ASCII`, so the text blank is `0x8020` and the erase blank is the
**flag-clear** `0x0020` - a different value, which is why the two modes look
unlike in a value diff even where both write blanks.

**Conclusion: no render change was needed or made.** `git diff` for this slice
touches the latch table, the instruction count and the call count only.

## The 38 was the TOTAL, not the recovered body

v0732f recorded "4062 - singulars / 38 calls". The 38 is what the reference
reports for the whole `0x9ff8 -> 0xa010` frame. The recovered block does not
cover the whole frame.

This block leaves a fixed **232-instruction native tail**, in both modes:

```text
COMMON      4422 total - 4190 recovered = 232
INDIVIDUAL  4294 total - 4062 recovered = 232
```

The tail's *call* count is what differs by mode: 0 under COMMON, 6 under
INDIVIDUAL. So the recovered body carries 32 calls in both, and

```text
 38 reference total  =  32 recovered  +  6 native tail
```

Pinning the body's 38 as v0732f proposed would have made the recovered block
claim six calls the original never makes at this boundary. The first
differential run caught it exactly: `native ins=4294 calls=44`.

The same arithmetic recovers row 1, which the pre-v0732g code had pinned
correctly at 4060/32:

```text
 4292 total - 232 tail = 4060 instructions
   38 total -   6 tail =   32 calls
```

## What changed

`src/recovered/texture_bridge_match.c`, `execute_frame_phase17_bit7_index5`:

1. **One latch added** - the INDIVIDUAL-only post-edit release on value row 2
   (`coin_mode = 1`). The latch shape is the one COMMON already uses: the
   KICK/PUNCH direction lives in the preceding EDIT frame, not in the
   release, so a single tuple covers both directions.

2. **The old INDIVIDUAL arm was restored but keyed to the row.** It was
   `released_flags == 4 && phase_a5 >= 1 && INDIVIDUAL -> 4060/32`. That was
   right for row 1 and wrong for row 2, and it was dead code - no latch
   admitted an INDIVIDUAL release. It is now
   `phase_a5 == 1 -> 4060/32`.

3. **A new counted rule for row 2** -
   `phase_a5 == 2 && INDIVIDUAL -> 4062 - singulars / 32`.

4. **The residual INDIVIDUAL case fails closed** instead of falling through
   to COMMON's `4190 - singulars`.

5. The steady INDIVIDUAL idle pin and its `g1 = 0` pin were narrowed from
   `phase_a5 >= 1` to `phase_a5 == 1` to match.

### A fail-open the row-2 differential alone would have hidden

The first version of this change keyed the new rule on `coin_flags` alone.
The COMMON row-1 post-edit latch is mode-agnostic (`coin_mode = both`), so an
INDIVIDUAL **row-1** release arrives on the same shape and the new arm claimed
it as `4062/32`. Result: `native 4294` against `reference 4292` - wrong on the
count by 2, with registers and memory still matching.

That is the dangerous shape of bug: the poststate was right, so a
state-only differential would have passed. It is also precisely the trap
`f3_individual_count_completed_v0732.md` warned about ("any future patch must
key on the row"). The comment I first wrote claimed the rule was row-keyed
while the condition was not. The negative control is what found it.

**Keep running negative controls on every row of an admitted leg.** A
differential on the rows being added proves those rows; only a differential on
the *neighbouring* rows proves the gate is not too wide.

## The differential contract, per frame

Reference leg = `vf2probe` (the i960 reference executor running the ROM).
Native leg = `vf2i960 native-resume`. Start counters taken with
`--until 0x9ff8`, deltas to zero. Full contract = instruction delta, call
delta, `compare-snapshots registers`, `compare-snapshots ranges`.

### Admitted, all three FULL MATCH

| leg | `credits[0..3]` | rendered `{c1,c2,c4,c5}` | singulars | reference | native |
|---|---|---|---|---|---|
| `indk2-c` | `[2,2,2,2]` | `{2,2,2,2}` | 0 | 4294 / 38 | **4294 / 38** |
| `indk-c` | `[1,2,1,2]` | `{2,1,2,2}` | 1 | 4293 / 38 | **4293 / 38** |
| `indp-c` | `[3,3,1,2]` | `{3,1,2,2}` | 1 | 4293 / 38 | **4293 / 38** |

`indk2` is a KICK release, `indp` a PUNCH, both built from existing idles with
no patched state. Registers and memory match on all three.

### Negative controls

| leg | how built | reference | native | verdict |
|---|---|---|---|---|
| `nega5-1` | `indk2-c` with `0x005000a5` patched 2 -> 1 | 4292 / 38 | 4292 / 38 | **FULL MATCH** |
| `nega5-3` | same, 2 -> 3 | - | refused at `0xa6c0` | fail-closed |
| `nega5-4` | same, 2 -> 4 | - | refused at `0xa6c0` | fail-closed |
| `nega5-5` | same, 2 -> 5 | - | refused at `0xa6c0` | fail-closed |

`nega5-1` is a **patched-state probe, not a natural-walk recovery claim**. It
does not prove row 1 is reachable in INDIVIDUAL - per
`f3_f4_individual_mode_measured_v0732.md` it is not - but it does prove the
4060/32 body is right for row 1 through the *post-edit release* entry as well
as the steady idle, which is what that pin claims.

Rows 3, 4 and 5 of the INDIVIDUAL post-edit release remain unmeasured and
unadmitted.

### COMMON regression, all six legs unchanged

`f2-r2-relcl` 4422/41, `f2-r3-relcl` 4421/41, `f2-r4-relcl` 4418/41,
`f2-r3-edit` 4637/44, `f2-r4-edit` 4635/43, `f2-r4-cl` 4425/41 - all FULL
MATCH.

## Final count rules on this leg

```text
COMMON      value rows 2,3   body = 4190 - singulars,  41 calls
COMMON      value row  4    body = 4193 (preset 0) / 4186 (preset >= 1), 41 calls
INDIVIDUAL  value row  1    body = 4060,               32 calls
INDIVIDUAL  value row  2    body = 4062 - singulars,   32 calls
INDIVIDUAL  value rows 3,4,5                        refused
```

where `singulars` counts the rendered credit cells
`{credits[1], credits[2], credits[4], credits[5]}` equal to 1, and "body"
excludes the 232-instruction native tail this block leaves behind.

## Corrections to the record

- v0732f's "the render is NOT recovered" was **wrong**. It compared against
  the wrong control row and attributed a row difference to the mode.
- v0732f's "4062 - singulars / **38** calls" was **wrong on the calls**. 38 is
  the frame total; the body carries 32.
- v0732f's note that "the 4060/32 was measured on row 1 ... admitting a5 = 2
  needs the existing 4060/32 branch narrowed to a5 == 1" was **right**, and is
  what this slice does - the branch was deleted outright at first and had to
  be restored, row-keyed, because row 1 is genuinely measured.

## Standing caveat

The 7-instruction `a5 = 4` delta between `preset == 0` and `preset >= 1` is
still unexplained, and still pinned as measured. Nothing in this slice
touches it.
