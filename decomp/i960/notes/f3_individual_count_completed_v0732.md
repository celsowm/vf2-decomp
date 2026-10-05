# v0732f: the INDIVIDUAL release count is 4062 minus singulars; the render is NOT recovered

**Evidence only. No behaviour change, no tuple admitted.** Extends
`f3_f4_individual_mode_measured_v0732.md` with two more INDIVIDUAL release
samples. The count rule is now measured across the singular domain; the
render is characterised but not recovered.

## The count completes across the singular domain

Three INDIVIDUAL value-row releases, all built without patching state
(alternate KICK / PUNCH presses at the row-2 INDIVIDUAL idle, then settle to
`0x10fa0`, cross the frame IRQ to `0x9ff8`, patch the release latch):

| snapshot | `credits[0..3]` | rendered `{c1,c2,c4,c5}` | singulars | reference | body |
|---|---|---|---|---|---|
| `indk2-rel` | `[2,2,2,2]` | `{2,2,2,2}` | 0 | **4294** / 38 | 4062 |
| `indk-rel` | `[1,2,1,2]` | `{2,1,2,2}` | 1 | **4293** / 38 | 4061 |
| `indp-rel` | `[3,3,1,2]` | `{3,1,2,2}` | 1 | **4293** / 38 | 4061 |

So the same counted mechanism as COMMON, with a different base and call count:

```text
COMMON     value-row release body = 4190 - singulars,  41 calls
INDIVIDUAL value-row release body = 4062 - singulars,  38 calls
```

Two points at 1 singular and one at 0, with the 0-singular case **higher** by
exactly one - the same direction as COMMON, where more singulars take the
shorter handler. Direction does not matter: `indk-rel` is a KICK and `indp-rel`
a PUNCH, and both give 4293/38.

Note the base is **4062**, not the 4060 that the existing INDIVIDUAL branch
carries. That 4060/32 was measured on **row 1** after the COMMON->INDIVIDUAL
toggle, a different row with a different render and 6 fewer calls. Admitting
`a5 = 2` in INDIVIDUAL therefore needs the existing 4060/32 branch narrowed to
`a5 == 1`, or the two rows will collide.

## The render is a different shape, not a filtered COMMON one

Row-write histograms from the reference, `out/indk2-rel.jsonl` against the
COMMON `out/f2-r4-rel.jsonl`:

```text
COMMON     416 writes
  5:25  6:26  7:19  8:26  9:19 11:36 13:34 15:17 17:17 19:17 21:17
  24:33 26:17 28:17 30:17 32:17 35:14 38:4 44:24 45:20

INDIVIDUAL 510 writes
  5:25  6:27  7:19  8:26  9:19 11:35 13:36 15:17 17:17 19:17 21:17
  24:31 25:18 26:18 27:18 28:18 29:18 30:18 31:18 32:18 33:18
  35:14 38:4 44:24 45:20
```

Differences beyond the count:

- **Rows 25-33 are erased**, 18 cells each - 162 extra writes. The recovered
  code already has an INDIVIDUAL erase block for rows 24-33 at column 30,
  length 18, so this is consistent with that block running over a wider span
  than the COMMON path uses.
- **Row 24 drops from 33 to 31** - the erase reaches into it.
- **Row 13 gains 2** (34 -> 36), **row 6 gains 1** (26 -> 27), **row 11 loses
  1** (36 -> 35).

So INDIVIDUAL is not "COMMON minus the chute section". Rows 6, 11 and 13 all
change, and the only way to pin the values is a value-level comparison of the
reference trace against the current render - which is a full slice, not a
follow-up edit.

## What would be needed to admit it

1. A value-level diff of `indk2-rel` / `indk-rel` / `indp-rel` against the
   current render, to map the row 6, 11, 13 and 24 cell-by-cell.
2. Narrow the existing INDIVIDUAL 4060/32 branch to `a5 == 1` so it cannot
   swallow the value rows.
3. `4062 - singulars` / 38 calls for the INDIVIDUAL value rows, with the gate
   admitting COMMON-only... no - admitting INDIVIDUAL `a5 == 2` explicitly,
   leaving `a5 >= 3` refused (they are not reachable by walking, per
   `f3_f4_individual_mode_measured_v0732.md`).
4. A ROM-backed differential on all three snapshots plus the 117/117 ctest run.

None of that is done. The native still refuses all three, which is correct.

## Standing caveat

The existing `4060` / `32` figure and the new `4062` / `38` were measured on
different rows under different renders. They are not two values of one rule -
they are two different frames. Any future patch must key on the row, not
replace one constant with the other.
