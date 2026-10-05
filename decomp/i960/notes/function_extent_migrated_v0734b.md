# v0734b: the 25 inverted rows are migrated, and 5 more rows were losing half their notes

**Phase 1.2b of `completion_plan_v0734.md`.** v0734a found that `functions.csv`'s
`end` column carries two different quantities and that 25 rows were being deleted
from every report. This slice repairs them — with the schema change v0734a
called for — and finds a second malformation in the same file on the way.

## Hand-check of one row, done without trusting the script

`tile_runtime_gate`, migrated from `end=0x0000cfe4` (a continuation) to
`0x0004429c`:

```text
00044268  ld       0x00508000, r9
00044270  bbs      4, r9, 0x00044294
00044274  bbc      2, r9, 0x00044298
00044278  bbc      7, r9, 0x00044290
0004427c  ld       0x0059e014, r14
00044284  cmpobe   0, r14, 0x00044290
00044288  call     0x0004f1ec
0004428c  b        0x00044290
00044290  ret                       <- one exit
00044294  call     0x0004f124
00044298  ret                       <- another exit
0004429c  decode failed: unsupported operation   <- NOT code
```

Three things fall out, none of which the migration script looked at:

1. The function has **two** `ret`s, both inside the migrated extent. A multi-exit
   routine is exactly the case v0733c worried a "first-`ret` walk" would get
   wrong, and the sweep got it right.
2. **The first byte past `0x0004429c` does not decode.** That is an
   independent confirmation the boundary is real and not merely where the sweep
   happened to stop.
3. The row's own `notes` say *"Observed clear tile runtime flags return in four
   instructions"*. With bit 4 clear and bit 2 set the path is
   `ld` → `bbs` → `bbc` → `ret` = **exactly four**. The executed count in the
   note and the static extent now agree, which is the same
   static-vs-executed relationship v0733c accepted for its two repairs.

## The schema

`end` becomes **always a code extent**. The call-return continuation moves to a
new **trailing** `return_to` column:

```text
address,end,name,status,source,notes,return_to
```

Trailing is deliberate: `src/analysis/symbols.c`'s `apply_file` reads
`functions.csv` by column **index** (`name_column = 2`), so inserting a column
would have silently renamed every function. It caps at 8 fields and reads only
indices 0 and 2, so a 7th is ignored — verified, not assumed: the C analyzer
still resolves **53 user-named functions and an identical 263-name set** after
the migration, and `basic_blocks` is unchanged at 3758.

Every Python consumer uses `csv.DictReader` with named access, so a trailing
column is transparent to them.

## Second source, per row — and the one refusal-shaped case

A row was migrated **only** when an independent source confirmed the measured
boundary. The sources are the same ones v0733c used:

- **A** — the instruction at `measured_end - 4` decodes as `ret`, read straight
  from the ROM, independent of the CFG sweep that produced `measured_end`;
- **B** — additionally, `measured_end` equals some *other* row's `address`.

| verdict | count | rows |
|---|---|---|
| A (`ret`) + B (adjacent row) | 9 | `frame_buffer_gate`, `texture_upload_dispatch`, `texture_record_advance`, `texture_body_return`, `texture_post_body_call`, `texture_orchestrator_epilogue`, `game_state_update`, `frame_timer_suffix`, `frame_wait_poll` |
| A (`ret`) only | 15 | |
| **branch terminal — LOWER BOUND** | **1** | `main_post_timer` |
| refused | **0** | |

`main_post_timer` is the interesting one. Its measured extent `0x0000a048` is
correct as a **lower bound** but its highest block does not end in a `ret`:

```text
0000a044  b        0x00009fb0    <- unconditional; the block leaves here
```

so the sweep stops at `0xa048` and never reaches the `b 0x0000a0c0` that sits
there. Its `notes` say so explicitly, in the row itself:

> extent is a LOWER BOUND - the highest block terminates in `b` at 0x0000a044
> rather than a ret, so more code may belong to this row

**A lower bound must not be laundered into an extent**, so it is labelled, not
laundered. Three more are lower bounds for a different reason — `indirect=yes`,
which the sweep cannot follow: `frame_dispatch_tick`, `player_update_gate`,
`tile_controller_update`. All four are flagged in their own `notes`.

**The refusal path is proven, not assumed.** The classifier returns `refuse`
for any boundary that is neither a `ret` nor a branch terminal; fed `0xa030` it
reports `call` and refuses. A gate that can only say yes is not a gate.

## Second defect: five rows were losing half their note

Five rows carried an **unescaped comma inside `notes`**:

```text
0x0004cb64,0x0004cc28,texture_word_prepare,...,dynamic-differential+unit,Computes the timer threshold, executes the recovered timer helper and prepares the word decoder; observed four times
```

`csv.DictReader` splits that into seven fields, so `notes` ended at the first
comma and **everything after it landed in the `None` restkey** — silently, for
every consumer in the repo. `texture_word_prepare`'s note really ends
"...prepares the **word decoder**"; nobody reading that table could see it.

All five are now properly quoted, and the migration rejoins fields 5..n before
writing (the first attempt at this script dropped exactly that text — the audit
below is what caught it).

## What the migration is not

`out/migrate_v0734b.py` is a **scratch tool**; `out/` is gitignored and it is
deliberately not committed. The durable gate is in `block_coverage.py`:

- `--strict-ranges` still fails on any row with `end < address`. It now exits
  **0** on the migrated CSV, having exited 1 before — that transition is the
  fix, and it is reversible in one edit if a regression appears.
- a new `find_continuation_rows()` census keeps the 25 rows **visible** as rows
  that also record a `return_to`. They are ranked by `end` like everything else,
  but a reader is told the second quantity exists.

Both gates are proven able to fail, by planting the defect:

| planted defect | failure message |
|---|---|
| unquote a note containing a comma | `texture_word_prepare has 8 fields, expected 7` |
| restore an inverted `end` on `tile_runtime_gate` | `an inverted row is back` |

## Effect

| | before | after |
|---|---|---|
| bounded rows visible to `block_coverage.py` | **69** | **94** |
| inverted rows | 25 (deleted silently) | **0** |
| rows with truncated notes | 5 | **0** |
| rows naming their continuation | 0 | 25 |

`tile_controller_update` (1016 B) and `game_input_update` (916 B) now appear in
the ranking at all; neither was ever in a report.

**The published number is unchanged**: `decomp_dev_report.py` still reports
`2336/524288 program-ROM bytes (0.4456%), 13/13 fully tracked functions`. That
is exactly what v0734a's audit predicted — none of the 25 carry a status that
report counts — and it is now confirmed by construction rather than by
inference.

## Validated

- `test_block_coverage.py`: **24/24** (was 21/21).
- C analyzer: 53 user-named functions, identical 263-name set, `basic_blocks`
  unchanged at 3758.
- `decomp_dev_report.py`: published percentage unchanged.
- File audit: 102 lines, all exactly 7 fields, LF endings preserved, longest
  line well inside the C reader's 2048-byte buffer.

## Still open

- **Phase 1.2a** — the **13 remaining container rows**, one at a time.
- **Phase 1.3** — the coverage arithmetic is still contaminated. It is *less*
  contaminated now (25 phantom rows are gone), but the 267 628 B of overlap and
  the 93 748 B gap were computed before, so both must be recomputed before any
  of it is quoted.
- Four rows in the table are explicitly lower bounds (`main_post_timer` plus the
  three `indirect=yes` ones). They are labelled in place, not repaired.
- `coverage_ratio` is still `0.00` for every ranked row: the tool orders
  candidates, it does not certify them.