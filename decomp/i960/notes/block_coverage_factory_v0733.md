# v0733: `block_coverage.py` — per-function coverage report for the factory chain

The factory chain is missing one navigation aid: an automated way to answer
*"which `functions.csv` entry has the largest unmeasured gap that the next
session should tackle?"* — and that is exactly the shape of question the
v0730 runbook expects the factory chain to answer for `P3-P4`. This commit
adds that tool.

## The gap

- `frontier.py v2` (v0729a, per-edge fighter offsets + per-source
  attribution) ranks **edges** by witness/fighter bonus, but it does not
  know which `functions.csv` entry the edges fall inside.
- `infer_structs.py` (v0729d/v0732i) ranks **fighter offsets** by shared
  access, but it does not surface `functions.csv` ranges at all.
- The result: a hand-eye comparison is required to map
  `frontier.py` ranks back to the recovered/prefix/control-block ranges
  the human is supposed to be filling. That comparison is exactly the
  step the factory should automate.

`block_coverage.py` fills that gap. It reads `decomp/i960/functions.csv`,
streams one or more `out/trace-*.jsonl` files, and emits a per-function
report with:

- `range_words`, `addresses_in_trace`, `coverage_ratio`,
  `total_uncovered_words`, `largest_uncovered_run` (the longest
  contiguous gap in i960 words);
- a `status_bucket` parsed from the CSV column (`fully_recovered`,
  `prefix`, `prefixes`, `control_block`, `first_dispatch`,
  `observed_branch`, `rom_anchor`, `unknown`);
- an `is_wrapper` flag — true only when `status_bucket ==
  "control_block"` AND `byte_size > 0x10000`. Wrappers like
  `main_texture_orchestrator_call` (269 476 B at `0xa030..0x4bcd4`)
  span a large region of code and would otherwise dominate the
  report; `--include-wrappers` opts them back in.

The tool does **not** invent game semantics, decide hardware behaviour,
or modify any code. It is a navigation aid only.

## What it found on the existing corpus

`block_coverage.py --trace out/trace-both.jsonl --trace out/trace-f0.jsonl --include-wrappers --limit 12`
on the existing v0732 era carries:

```text
name                              status                       size     in_trace  long_run   range
main_texture_orchestrator_call    recovered-control-block   269476         499     41012  0xa030..0x4bcd4
interrupt_return_wait_exit        recovered-observed-branch   66180           1     16427  0xd20..0x10fa4
video_register_compose            recovered-observed-branch   48988           0     12247  0x1064..0xcfc0
video_input_latch_write           recovered-observed-branch   48444           0     12111  0x1290..0xcfcc
input_ring_poll                   recovered-observed-branch   48392           0     12098  0x12d8..0xcfe0
input_bit0_sequence_gate          recovered-observed-branch   45420           0     11355  0x1e6c..0xcfd8
input_bit1_sequence_gate          recovered-observed-branch   45312           0     11328  0x1edc..0xcfdc
frame_shadow_verify               recovered-observed-branch   39628           0      9907  0x530..0x9ffc
main_frame_timer_call             recovered-control-block     28508           1      7014  0xa034..0x10f90
task_camera                       recovered-prefixes           7376           0      1844  0x1d320..0x1eff0
texture_status_dispatch_call      recovered-observed-branch    5532           0      1383  0x4bd24..0x4d2c0
texture_active_prepare_call       recovered-observed-branch    5004           0      1251  0x4bde0..0x4d16c
```

Reading the report:

- `main_texture_orchestrator_call` is the wrapper that already dominates
  by construction (a single `control_block` span); its `long_run` of
  41 012 i960 words is the *uncovered* slice between texture calls. Not a
  recovery target on its own — it is a wrapper.
- `interrupt_return_wait_exit` has 1 traced address and 16 427 uncovered
  words behind it. That is the post-timer tail the run-through-exit
  chain crosses; its single hit is the tail `ret`. The remaining 16 427
  words are the *uncharted* body that the runbook's "P3-P4" entry will
  need to look at first when it wants to extend the corridor past
  `0x10fa4`.
- `video_register_compose`, `video_input_latch_write`, `input_ring_poll`,
  the two `input_*_sequence_gate` and `frame_shadow_verify` are
  `recovered-observed-branch` entries whose `long_run` is the full byte
  size. That is the *expected* shape: the recovered branch is the one
  measured access, the rest is the unmeasured sibling. The report makes
  the size of those unmeasured siblings visible at a glance.
- `task_camera` is the one `recovered-prefixes` row in the report; the
  prefix coverage is local, the rest is the unproven sibling set noted in
  `docs/UNCOVERED_BRANCHES.md` (camera viewports, etc).

## Sanity on the smaller player traces

`block_coverage.py --trace out/trace-1442c-s25.jsonl --include-wrappers --limit 5`
is consistent: the wrapper still dominates, and the next entry is
`interrupt_return_wait_exit` (16 545 uncovered words). The smaller
player traces visit no recovered range beyond the wrapper, which is
also correct — `0x1442c` is a measured frontier, not in `functions.csv`
yet.

`block_coverage.py --trace out/trace-bit14.jsonl --include-wrappers --limit 5`
is consistent with the player-corpus reading: 95 traced addresses in
the wrapper, 0 elsewhere.

## Standalone unit suite

`tools/python/test_block_coverage.py` (14 tests) covers:

- `load_functions` filters entry-only / invalid ranges;
- `STATUS_BUCKETS` classifies every known `recovered-*` variant;
- `is_wrapper` only flags large `recovered-control-block` entries;
- `longest_uncovered_run` handles empty/full/mid/edge splits;
- `total_uncovered_words` and `addresses_in_range` count subsets
  correctly;
- `collect_trace_addresses` handles missing files and aggregates
  `step` + `memory` records across multiple JSONL inputs;
- `build_report` flags wrappers distinctly;
- `rank_reports` orders by `largest_uncovered_run` descending with stable
  ties, refuses unknown `--sort` keys;
- `render_text` excludes wrappers unless `--include-wrappers`;
- `FunctionReport.to_dict` is JSON-round-trippable;
- `load_functions` on the real `decomp/i960/functions.csv` returns
  >= 60 entries with `byte_size > 0` (the entry-only filter holds on
  the real table).

ctest entry **#120** `vf2_python_factory_block_coverage` wires the
suite into CMakeLists.txt (TIMEOUT 240 s like its siblings).

## What it does **not** do

- It does not invent fighter offsets, struct fields, or game
  semantics.
- It does not decide hardware behaviour or modify the i960
  executor.
- It does not rank by "most actionable next slice" — only by the
  raw measurements it ingests. The "next slice" decision is still
  the human's, with the runbook as the durable contract.

## How to use it for the next P3-P4 slice

The intended call is:

```sh
python tools/python/block_coverage.py \
  --trace out/<frontier-trace>.jsonl \
  --limit 20
```

By default wrappers are excluded, so the top of the table is the next
non-warner candidate. `--json` produces a machine-readable stream that
can be piped into a future factory-chained step. `--sort` switches
the ordering to `total_uncovered_words`, `coverage_ratio`, or
`byte_size` if the largest-gap metric is not the one the slice wants.

## Files

- `tools/python/block_coverage.py` — new tool (439 lines).
- `tools/python/test_block_coverage.py` — new unit suite (14 tests).
- `CMakeLists.txt` — `vf2_python_factory_block_coverage` ctest entry
  (#120), TIMEOUT 240 s.

`out/` is gitignored and was not modified by this slice.