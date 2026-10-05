# v0734a: `functions.csv`'s `end` column is overloaded; 25 rows were being deleted silently

> **v0734b repaired all 25.** `end` is now always a code extent and the
> call-return continuation moved to a trailing `return_to` column. Bounded rows
> visible to `block_coverage.py` went **69 -> 94**, inverted rows **25 -> 0**,
> and `--strict-ranges` now exits 0 where it exited 1. Four rows are labelled
> **lower bounds** in their own `notes` and deliberately not repaired
> (`main_post_timer` plus three `indirect=yes`). See
> `function_extent_migrated_v0734b.md`.
>
> Everything below is left as the measurement record it was.

**Phase 1.1 of `completion_plan_v0734.md` ("replace or document `vf2i960
function`") is closed with a different answer than the plan assumed.** The
instrument was not the broken part. The *table* is.

## What the plan expected

Phase 1.1 asked whether `vf2i960 function` could be turned into a real extent
oracle to repair `functions.csv`. The first thing measured was whether the tool
is a linear first-`ret` walk, as v0733c recorded.

## It is not linear — and that is not the defect either

`vf2i960 function` builds real basic blocks and follows **direct branch
targets**, including backward ones. `main_texture_orchestrator_call` at
`0x0000a030` ends with

```text
0000a030  call     0x0004bb18
...
0000a044  b        0x00009fb0      <- backward branch into a shared block run
0000a048  b        0x0000a0c0
```

and owns **17 blocks**: 11 at `0x9fb0..0xa01c` reached through that backward
branch, plus its own 6. A first-`ret` walk cannot produce blocks *below* its
entry.

The texture rows are stronger evidence. Six different entry points in v0733c's
own table measured the same `end=0x0004bfe0`, and four of them were re-run here:

| entry | name | blocks | end |
|---|---|---|---|
| `0x0004bd24` | `texture_status_scan_end` | 50 | `0x0004bfe0` |
| `0x0004bde0` | `texture_active_prepare_call` | 50 | `0x0004bfe0` |
| `0x0004bebc` | `texture_child_zero_gate_a` | 50 | `0x0004bfe0` |
| `0x0004bf90` | `texture_final_status_call` | 7 | `0x0004bfe0` |

All four share their last three blocks (`0x4bfcc`, `0x4bfd8`, `0x4bfdc`) — a
shared epilogue:

```text
0004bfcc  ld       0x00508000, r15
0004bfd4  bbs      9, r15, 0x0004bfdc
0004bfd8  call     0x0004d25c
0004bfdc  ret                        <- end = 0x0004bfe0, one past this ret
```

**Measured definition.** `end` is one past the highest `ret` reachable from the
entry through direct branches and fall-through. It does **not** descend into
callees and does **not** follow indirect branches, so it is a sound **lower
bound** — exactly as v0733c said. The `0x23524` case is the demonstration:

```text
00023644  bal      0x00023694    <- the 179-insn shell body is a callee
00023648  ret                     <- linear path stops here, end = 0x0002364c
0002364c  ldt     0x000001f4(g7), r4   <- a callee, coincidentally here
```

v0733c and v0733d are **upheld**. Nothing in this slice retracts them. The
mechanism is now stated precisely instead of approximated.

## The real defect: `end` means two different things in one column

`decomp/i960/functions.csv` overloads its `end` column. Most rows carry a **code
extent**. **25 of the 94 bounded rows instead carry the call-return
continuation** — the address control resumes at in the caller. Their own
`notes` say so:

```text
frame_timer_suffix   0x00010fa4-0x0000a038  "Completes the changed-frame-byte
                     timer path and returns to the main loop"
game_input_update    0x00001abc-0x00000c90  "Composes both observed input
                     sequence gates and returns to the interrupt dispatcher"
main_post_timer      0x0000a038-0x00009fb0  "Composes memory diagnostic frame
                     counters and the branch into the main clear tail"
```

A continuation may sit anywhere in the ROM, **including below the function's own
entry**, so every one of these rows has `end < address`. `byte_size` is
therefore negative, and `block_coverage.py` dropped all 25 at
`load_functions()`'s `if end <= start: continue`.

**94 bounded rows in, 69 rows out, 25 gone, no warning printed.** That is the
v0733b defect class one level up: a report that silently shrank, in a tool
built specifically to tell you which rows have unmeasured gaps.

### It was never visible because the arithmetic was never checked

`byte_size` returns `end - start`. Sixteen of the 25 render as **negative
sizes** in any consumer that does not guard — `tile_runtime_gate
0x00044268..0x0000cfe4` is `-225924`. The "267 628 B of overlap" and the
"93 748 B gap" figures carried through v0733b-v0733g are computed over a table
in which a quarter of the rows are not ranges.

## Measured true extents, all 25

ROM-backed, `vf2i960 function roms/vf2 <address>`. `csv_end` is the value
currently in the file.

| name | csv_end | measured_end | bytes | blocks | indirect |
|---|---|---|---|---|---|
| `task_coli` | `0x00010dcc` | `0x00022298` | 176 | 16 | no |
| `system_memory_diagnostic` | `0x0000a03c` | `0x0006dd4c` | 148 | 7 | no |
| `video_input_sync` | `0x00000ce0` | `0x0001128c` | 408 | 33 | no |
| `frame_counter_advance` | `0x0000a040` | `0x000113f4` | 252 | 12 | no |
| `frame_phase_advance` | `0x0000a044` | `0x00011cb4` | 60 | 1 | no |
| `frame_buffer_gate` | `0x0000a004` | `0x000110f4` | 68 | 5 | no |
| `frame_dispatch_tick` | `0x0000a010` | `0x0000a6f8` | 56 | 3 | **yes** |
| `game_event_queue_write` | `0x00002020` | `0x00043950` | 100 | 3 | no |
| `texture_upload_dispatch` | `0x00002de4` | `0x0004bb18` | 152 | 10 | no |
| `texture_record_advance` | `0x0004bd24` | `0x0004bfe0` | 128 | 45 | no |
| `texture_body_return` | `0x0004bb94` | `0x0004bfe0` | 4 | 1 | no |
| `texture_post_body_call` | `0x0004b8d8` | `0x0004bcd4` | 320 | 10 | no |
| `texture_orchestrator_epilogue` | `0x0000a034` | `0x0004bcd4` | 124 | 1 | no |
| `game_meter_update` | `0x00001f98` | `0x000023f0` | 768 | 29 | no |
| `frame_geometry_gate` | `0x0000a014` | `0x0000a804` | 188 | 19 | no |
| `player_update_gate` | `0x0000cfd0` | `0x00024680` | 332 | 10 | **yes** |
| `video_layer_commit` | `0x0000cfd4` | `0x00024158` | 492 | 21 | no |
| `tile_runtime_gate` | `0x0000cfe4` | `0x0004429c` | 52 | 8 | no |
| `game_input_update` | `0x00000c90` | `0x00001e50` | 916 | 27 | no |
| `game_state_update` | `0x00000cd4` | `0x000020f0` | 404 | 21 | no |
| `tile_controller_update` | `0x00000cdc` | `0x0004ec00` | 1016 | 70 | **yes** |
| `interrupt_ack` | `0x00000040` | `0x00000d44` | 20 | 1 | no |
| `frame_timer_suffix` | `0x0000a038` | `0x000110b0` | 268 | 6 | no |
| `main_post_timer` | `0x00009fb0` | `0x0000a048` | 16 | 4 | no |
| `frame_wait_poll` | `0x00000bc0` | `0x000110b0` | 288 | 8 | no |

Several measured ends land exactly on another CSV row's start
(`texture_upload_dispatch` -> `0x4bb18` = `texture_orchestrator_save_call`;
`frame_buffer_gate` -> `0x110f4` = `video_input_sync`; both
`texture_post_body_call` and `texture_orchestrator_epilogue` -> `0x4bcd4` =
`texture_frame_gate_call`), and `task_coli` -> `0x00022298` is the shell
boundary already pinned by the v0732 whole-task differential. That is a second
independent source for those rows and the strongest kind.

**The three `indirect=yes` rows are lower bounds only** — the sweep cannot
follow `j`/`jm`, so `frame_dispatch_tick`, `player_update_gate` and
`tile_controller_update` need the same second-source treatment as `0x23524`
before their measured value is treated as an extent.

## No row is repaired here

v0733c's standing rule — *a bound is repaired only when a second source agrees*
— still governs, and this slice supplies evidence, not amendments. The
second-source check is per row and is Phase 1.2's work. What changes now is that
the work is well-defined: the method is the measured extent plus a
disassembly-visible boundary, and the 25 rows are enumerated with their measured
values instead of being invisible.

## The tool change

`block_coverage.py` now **names every row it cannot rank**, on stderr and in the
text report:

```text
warning: 25 functions.csv rows have end < address and are EXCLUDED from this
report. Their 'end' column holds a call-return continuation, not a code extent:
  task_coli                          0x000221e8..0x00010dcc
  ...
```

plus `--strict-ranges`, which turns the warning into a non-zero exit for anyone
who wants it enforced.

The census deliberately does **not** change `load_functions`'s contract: the
coverage table keeps the semantics its 21 unit tests pin, and the inventory is a
separate, independently tested scan. Guessing an extent for an inverted row would
be inventing a measurement.

## Gate integrity

`test_strict_ranges_gate_fails_on_a_planted_inverted_row` plants an inverted row
in a temp CSV, runs the real CLI, and **requires a non-zero exit and the row
named in stderr** — then requires the default run to stay a warning. The gate
is proven able to fail rather than assumed to.

`test_real_csv_inverted_census_is_pinned` asserts the census is exactly **25**
and that every inventoried row is genuinely absent from the loaded set. If a
future slice repairs rows, this number must be updated deliberately.

## Validated

- `test_block_coverage.py`: **21/21** (was 17/17).
- The real CSV loads 69 rows; the census reports 25 inverted; 69 + 25 = 94.

## `decomp_dev_report.py` audited: latent, not active

`decomp_dev_report.py:101` carries the **same** `if end <= start: continue`
guard, and it computes a *published* progress percentage from the remainder, so
it was checked rather than assumed.

**Result: the published number is not currently affected.** It counts only
`recovered`, `recovered-rom-anchor` and `candidate` rows, and **0 of the 25**
inverted rows carry one of those statuses — they are all
`recovered-observed-branch` / `recovered-control-block` /
`recovered-first-dispatch`. The guard therefore drops nothing the report was
going to use today. Current output is
`2336/524288 program-ROM bytes (0.4456%), 13/13 fully tracked functions`.

It stays a **latent trap**: the day a continuation-carrying row is promoted to
`recovered`, it will be deleted from a public number with no warning, exactly as
it was deleted from `block_coverage.py`. No change is made here — a guard that
is currently correct should not be "fixed" into a different shape on a hunch.

## Still open

- **Phase 1.2** — repair the 25 inverted rows and the 13 remaining container
  rows, per row, two sources each. For the inverted rows the right fix is a
  **schema change** (a separate `return_to` column), not a rewritten `end`.
- The coverage arithmetic in v0733b-v0733g (the 267 628 B overlap, the
  93 748 B gap) is contaminated and must be recomputed once the inverted rows
  are classified.
- `coverage_ratio` is still `0.00` for every ranked row: the tool orders
  candidates, it does not certify them.