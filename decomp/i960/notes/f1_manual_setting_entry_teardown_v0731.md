# v0731 F1: the MANUAL SETTING entry is a COIN ASSIGNMENT screen teardown

Supersedes the "page render" reading in
`f1_manual_setting_poststate_bug_v0731.md`. That note recorded a 549-byte
tile-ram gap on the selector-17 index-5 MANUAL SETTING entry and attributed
it to a missing page render. **That attribution was wrong.** The gap is a
teardown, and it is now recovered and proven.

## TL;DR

- The 549 differing bytes are **364 tile cells**, all of which the reference
  writes as `0x0020` (a space glyph with no attribute bit).
- The blanked cell set is **exactly the inverse of the `runs[]` render table
  already in `texture_bridge_match.c`**, under the current mode filter, minus
  the two hint rows this branch redraws, plus the six `cursor_addresses[]`
  cells: 30 admitted runs (358 cells) + 6 cursor cells = 364 cells.
  The rule is derived, not hand-tabulated.
- `r14` is the caller-frame value, not the parked pin `1`.
- With both applied the strict differential prints **`Snapshots match.`**
- **INDIVIDUAL mode is a different body and stays fail-closed.**

## Tooling added first

`compare-snapshots` gained a `ranges` mode, because the one-line summary
("549 differences") made the defect unactionable. It prints every maximal
differing run per region plus word-aligned `u16` pairs:

```sh
build/Debug/vf2i960.exe compare-snapshots A.vf2snap B.vf2snap ranges registers
```

Backed by two new public functions in `src/i960/snapshot.c`:

- `vf2_i960_snapshot_diff_runs()` — enumerate maximal differing byte runs
  across the memory regions, region-ordered. Counting-only is supported
  (`runs == NULL`); a truncated capacity still reports the true run count
  and byte total so a caller can detect that it did not get every run.
- `vf2_i960_snapshot_region_data()` — look up a captured region by name.

Both are covered by new cases in `tests/i960/test_snapshot.c`
(return codes 9-16).

## What the difference actually is

From `out/a5-ranges.txt` (200 runs, 549 bytes, all in `tile-ram`):

- **every** differing word has reference value `0x0020`;
- **zero** words where the reference draws text the recovered side lacks.

So the reference was never missing a page — it was *erasing* the coin menu.
Decoding the 364 cells by `(offset / 0x80, (offset % 0x80) / 2)` gives the
whole COIN ASSIGNMENT screen:

| row | col | n | text |
|---|---|---|---|
| 5 | 16 | 1 | *(cursor cell)* |
| 5 | 18 | 15 | `COIN CHUTE TYPE` |
| 5 | 35 | 10 | `    COMMON` |
| 6 | 16 | 1 | *(cursor cell)* |
| 6 | 18 | 18 | `CREDIT TO 1P START` |
| 6 | 40 | 1 | `2` |
| 6 | 42 | 7 | `CREDITS` |
| 7 | 28 | 11 | `1P CONTINUE` |
| 7 | 40 | 1 | `2` |
| 7 | 42 | 7 | `CREDITS` |
| 8 | 16 | 1 | *(cursor cell)* |
| 8 | 18 | 18 | `CREDIT TO VS START` |
| 8 | 40 | 1 | `2` |
| 8 | 42 | 7 | `CREDITS` |
| 9 | 28 | 11 | `VS CONTINUE` |
| 9 | 40 | 1 | `2` |
| 9 | 42 | 7 | `CREDITS` |
| 11 | 16 | 1 | *(cursor cell)* |
| 11 | 18 | 28 | `COIN/CREDIT SETTING     #  1` |
| 11 | 47 | 1 | ` ` |
| 13 | 16 | 13 | `COIN CHUTE #1` |
| 13 | 31 | 17 | `1 COIN  1 CREDIT ` |
| 15/17/19/21 | 31 | 17 each | *(17 filled blanks)* |
| 24 | 16 | 13 | `COIN CHUTE #2` |
| 24 | 31 | 17 | `1 COIN  1 CREDIT ` |
| 26/28/30/32 | 31 | 17 each | *(17 filled blanks)* |
| 35 | 16 | 1 | *(cursor cell)* |
| 35 | 18 | 14 | `MANUAL SETTING` |
| 38 | 16 | 1 | *(cursor cell)* |
| 38 | 18 | 4 | `EXIT` |

Note the teardown erases the `MANUAL SETTING` label and `EXIT` too — the
whole screen goes, and the branch's two existing hint lines
(`SELECT BY SERVICE BUTTON` / `AND PUSH TEST BUTTON`, rows 44/45) are the
only thing left drawn.

## The recovered rule

`clear_phase17_index0_text()` writes `0x0020` over a run of cells. The
teardown is then literally "the render set, inverted":

```c
for (run_index = 0; run_index < sizeof(runs)/sizeof(runs[0]); ++run_index) {
    if (runs[run_index].row >= 44u) continue;          /* hints redrawn below */
    if (runs[run_index].mode == 0u && !common_mode) continue;
    if (runs[run_index].mode == 1u &&  common_mode) continue;
    status = clear_phase17_index0_text(machine,
        runs[run_index].row * 0x80, runs[run_index].column,
        (uint32_t)strlen(runs[run_index].text));
}
/* plus one 0x0020 at each of the six cursor_addresses[] */
```

This is why the count lands exactly on 364 with no fitted constants, and why
it is a rule rather than a table.

## Differential evidence

Recipe unchanged from `f1_manual_setting_poststate_bug_v0731.md`; the entry
snapshot is rebuilt from `out/f1-a5-cl-0x4.vf2snap` with the TEST latch
patched **at `0x9ff8`**.

Two-step proof that isolates each change:

| step | change | result |
|---|---|---|
| 1 | teardown only, `r14` still `1` | `Snapshots differ in registers at offset 0xe: expected=0x1 actual=0x1d (1 differences)` + **`No memory-region differences.`** |
| 2 | `r14 = test_held_r14` | **`Snapshots match.`** |

Final state on the corrected tree:

```text
# a5 = 5, TEST press (COMMON)
native-resume: blocks=1 instructions=14295 entry=0x00009ff8 exit=0x0000a010
  calls=42 returns=42
compare-snapshots -> Snapshots match.
                   -> No memory-region differences.

# a5 = 4 TEST value edit (negative control, must NOT blank the screen)
native-resume: blocks=1 instructions=4635 entry=0x00009ff8 exit=0x0000a010
  calls=43 returns=43
compare-snapshots -> Snapshots match.
```

Accounting reconciles exactly: chain 14295 = 232 prefix + **14063** body
(+1), and 42 calls = 5 prefix + **36** body (+1).

## INDIVIDUAL mode is a different body — left fail-closed

Seeding `coin_flags = 1` and running the **reference** executor on the
`tests/recovered/test_phase17_zero.c` seeded state (both sides enter
`0xa6c0` directly, so the frame-depth boundary is shared) gives the
following final CPU counters. The call figures include the harness's own
`vf2_i960_cpu_enter_procedure` (+1), so the body itself is 36 / 34:

| mode | instructions | calls / returns | recovered body | verdict |
|---|---|---|---|---|
| COMMON | 14063 | 37 / 37 | 14063 / 36 | exact |
| INDIVIDUAL | **13935** | **34 / 34** | 14063 / 36 | **different body** |

In COMMON the reference and the recovered path agree on the final counters
exactly (`ref=14063/37/37 native=14063/37/37`). In INDIVIDUAL the original
program takes a *shorter, different* body even though the net poststate is
the same. `coin_mode` therefore stays `0u` (COMMON only) on the admitted
tuple. Widening it to `2u` would admit INDIVIDUAL with COMMON's counters,
which is unsound.

### A retracted measurement

An earlier attempt to prove INDIVIDUAL through the real chain was **invalid
and is retracted**. It patched `0x0059d6f0` (= `base + 0x3320` with
`base = 0x59a3d0` read from the snapshot) and reported `Snapshots match.`.

A temporary gate diagnostic showed why that was a mirage:

```text
GATE ... coin_flags=0x00000000 base=0x00599000 a5=5 a6=0xff a7=0xff entry=1
```

At the `0xa6c0` dispatch-tick boundary `base` is **`0x599000`**, not
`0x59a3d0` — the chain's earlier recovered blocks (`0x530`, `0x110b0`,
`0x2f5c`, `0xa154`) rewrite `0x50016c` before the tick runs. The patched
word was one the native path never reads, so the "INDIVIDUAL" run was
actually COMMON, and the single differing byte was a word the native path
never writes. Both facts cancelled into a false pass.

The `compare-snapshots` verdict is unaffected: it never covered counters, and
that is exactly why the seeded-state measurement above is the load-bearing
evidence for the mode split.

## Two harness limits found (why there is no ctest for this leg)

`tests/recovered/test_phase17_zero.c` cannot host this case:

1. `vf2_hybrid_post_frame_bridge_execute` enters at `0xa6c0` with
   `local_frame_depth == 1`, while the real chain reaches the tick at depth
   6. The branch inherits its caller frame (`local_frames[depth-1].registers[14/15]`)
   rather than synthesizing it, so ac / cc / depth diverge. Every existing
   case in that harness has ac/cc/depth matching, so this is a boundary the
   harness cannot express, not a wrapper artifact.
2. `base` differs for the same reason, so the index-5 gate words cannot be
   seeded to the values the real boundary carries.

Rather than ship a case that asserts less than the real gate (AGENTS.md
rule 5), the leg keeps the ROM-backed chained differential as its gate and
this note as its pin. A `native-index5-entry` sub-command that boots, walks
TEST MENU -> selector 17 -> COIN ASSIGNMENT -> row 5 -> TEST and runs the
differential itself would give it a ctest; that belongs with B5
(`native-seventh-dispatch`).

## State of the frontier after this change

- MANUAL SETTING **entry** (a5 = 5, COMMON, TEST press): native, proven.
- MANUAL SETTING entry in **INDIVIDUAL**: different body, fail-closed.
- The nested `a7` editor (`phase_a5 == 5 && phase_a7 != 0xff`): untouched,
  still fail-closed.
- PUNCH / KICK at row 5: unmeasured.
