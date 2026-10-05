# v0733b: the coverage report was ranking container ranges, not functions

**The v0733 report answered "which gap is biggest" with the wrong question.**
Every one of its top twelve entries was a *container* range — a `functions.csv`
row whose `end` is a region bound rather than the procedure's own extent — so
the ranking measured the size of a region that contains other rows, not the
size of any unmeasured function.

The tool exists to answer *"which `functions.csv` entry has the largest
unmeasured gap?"* for P3–P4. As shipped, following its advice would have sent
the next session to a 66 KB "function".

## The measurement

`functions.csv` mixes leaves and containers. Of the 69 rows with a real
`(start, end)` range:

```text
rows with a real range: 69
rows involved in any overlap: 57
strict containers (contain another row): 20
leaves: 49
```

Overlap alone proves nothing — a prefix/leaf decomposition has containers by
construction. The real tell is that the `end` value is not the function's own
extent. The CSV says so itself, in the same rows:

| row | `notes` claim | `end - address` |
|---|---|---|
| `video_register_compose` | "in **sixty-three instructions**" | 48 988 B |
| `video_input_latch_write` | — | 48 444 B |
| `input_ring_poll` | — | 48 392 B |
| `interrupt_return_wait_exit` | — | 66 180 B |

Four rows, all spanning ~`0x0d00..0x0d000`, nesting one another, each 600 %+
overlapped by its siblings. And the one row whose `notes` states an instruction
count contradicts its own range by three orders of magnitude.

The v0733 top twelve, with the container flag added by this slice:

```text
name                             long_run  container
main_texture_orchestrator_call    41012  True
interrupt_return_wait_exit        16427  True
video_register_compose            12247  True
video_input_latch_write           12111  True
input_ring_poll                   12098  True
input_bit0_sequence_gate          11355  True
input_bit1_sequence_gate          11328  True
frame_shadow_verify                9907  True
main_frame_timer_call              7014  True
task_camera                        1844  True
texture_status_dispatch_call       1383  True
texture_active_prepare_call        1251  True
```

**Twelve of twelve.** So 21 of 69 rows (20 containers + 1 wrapper) were
excluded, and the entire published ranking was inside that set.

## Two bugs, not one

### 1. No container detection

`mark_containers()` flags a row when it **strictly contains** another row's
range. Strict containment is the test rather than mere overlap, so an ordinary
prefix/leaf decomposition where siblings only abut is left alone — covered by
`test_mark_containers_leaves_abutting_siblings_alone`.

`is_container` travels in the JSON (`is_container` / `is_wrapper` /
`is_excluded_by_default`), so a consumer of the machine-readable output can tell
a container from a leaf without re-reading the CSV. `--include-wrappers` now
opts both back in, and the help text says why.

**No `end` value was changed.** Inventing extents would be exactly the kind of
guess this repo forbids; the tool now *detects and reports* the problem instead
of silently ranking by it.

### 2. The limit was applied before the exclusion

Worse than the missing flag: adding the exclusion alone made the report render
**completely empty**, because `main()` did `rows = rows[: args.limit]` and
*then* filtered. When the top-N are all excluded — which is the case here, twelve
of twelve — the table comes out empty, and **an empty table is
indistinguishable from "the corpus covered everything."**

`main()` now excludes, then limits, and applies the same filter to the JSON
output so the two agree (JSON previously still emitted the container rows that
the text report hid). `test_exclusion_before_limit_keeps_a_full_report` pins
the order by asserting that the old order returns `[]`.

That one is worth dwelling on. A gate that reports "nothing to do" when it
actually failed to look is worse than no gate — the same shape as the v0732s
sanitizer no-op, and the same lesson: **a report that comes back empty needs to
be distinguishable from a report that is genuinely empty.**

## The corrected ranking

```text
name                             status                        size  in_trace  cov  uncovered  long_run  range
boot_stage_2                     recovered                      892         0  0.00        223       223  0x000001b0..0x0000052c
texture_maintenance              recovered-observed-branch      704         0  0.00        176       176  0x0004b8d8..0x0004bb98
camera_viewport_construct        recovered-control-block        624         0  0.00        156       156  0x0001d678..0x0001d8e8
texture_header_decode            recovered-observed-branch      624         0  0.00        156       156  0x0004c180..0x0004c3f0
color_table_rebuild              recovered-rom-anchor           428         0  0.00        107       107  0x00002c38..0x00002de4
texture_tree_dispatch            recovered-observed-branch      412         0  0.00        103       103  0x0004c544..0x0004c6e0
task_osage                       recovered-first-dispatch       408         0  0.00        102       102  0x000640f4..0x0006428c
texture_default_limits_select    recovered-observed-branch      320         0  0.00         80        80  0x0004bfe0..0x0004c120
game_threshold_evaluate          recovered-observed-branch      316         0  0.00         79        79  0x000028d4..0x00002a10
camera_derived_state_update      recovered-observed-branch      264         0  0.00         66        66  0x00020558..0x00020660
boot_entry                       recovered                      256         0  0.00         64        64  0x000000b0..0x000001b0
texture_word_prepare             recovered-observed-branch      196         0  0.00         49        49  0x0004cb64..0x0004cc28
```

192–892 bytes, 48–223 uncovered words. That is the right order of magnitude for
procedures, and it is a ranking a human can act on.

## Validated

- `test_block_coverage.py`: **17/17**, including three new cases
  (`mark_containers` strict-containment, abutting siblings untouched, and the
  exclude-before-limit regression).
- `ctest -R vf2_python_factory`: **14/14** in 7.57 s.
- Full suite: **117/117** non-dominator entries in 257.78 s. The three `4505`
  entries are the ~23-minute dominator and are run separately; they were passing
  on this tree at the v0732r and v0732s validations, and this slice touches no C.

## What is still unresolved

- **The container rows' true extents are unknown.** This slice detects the
  problem and stops. Fixing `functions.csv` needs a measured bound per row
  (disassembly of the `ret`), which is per-function work.
- `coverage_ratio` is `0.00` for every listed row: the v0729–v0732 corpus
  (`trace-both` / `trace-f0`) does not reach any of these addresses. So the
  *ranking* is now sound but the *coverage* is unmeasured for them — this tool
  orders candidates, it does not certify them.
