# v0734g: `block_coverage.py` no longer renders unmeasured coverage as `0.00`

**Phase 1.4.** A small honesty fix to `tools/python/block_coverage.py`. The
text report cannot now be read as "every row is at 0%" when in fact no trace was
supplied.

## The defect

The report has always rendered the `cov` field as a float. When no trace was
supplied, `addresses_in_trace` was zero for every row and `coverage_ratio` was
`0.00`. The stderr banner `warning: no traces supplied` was easy to miss when
the user redirected output to a file or piped the tool into another script —
and the table itself looked like a measurement that said "we tried, nothing was
covered".

The note explicitly says `coverage_ratio 0.00` is **carried as zero, not as
zero**. With no trace every row carries zero; that is not the same as saying
every row is uncovered. Phase 1.4 closes this by changing the rendering, not
the measurement: when `addresses_measured` is false, the table renders the
column as `n/a` and prints a banner naming the absence of measurement.

## The change

```diff
-def render_text(rows: List[FunctionReport]) -> str:
+def render_text(
+    rows: List[FunctionReport],
+    addresses_measured: bool = True,
+) -> str:
```

`main()` now passes `addresses_measured=bool(addresses)` — exactly the source
of truth the original code was guarding with the same condition in the v0
branch that prints the stderr banner.

The rendering, when `addresses_measured` is false:

```text
note: no trace was supplied; every row's coverage and uncovered counts are
shown as 'n/a' / 0 because no instrument observed them, NOT because every row
is at 0%. The byte_size, largest_uncovered_run and range columns are still
meaningful.

name                   size  in_trace   cov  uncovered  long_run  range
tile_controller_update  1016         0   n/a      254       254  0x4e808..0x4ec00
...
```

`byte_size`, `in_trace`, `largest_uncovered_run` and `range` are still shown
verbatim — they are useful for ranking the next slice to attempt and they are
not affected by the absence of a trace. Only `cov` (and the implied `uncovered`
reading) is replaced with the honest `n/a`.

## Gate integrity

`test_render_text_marks_unmeasured_coverage_loudly` was added in
`tools/python/test_block_coverage.py`. It checks three things:

1. With `addresses_measured=False` the rendered text contains the banner,
   contains `n/a`, and does **not** contain `0.00`.
2. With `addresses_measured=True` the rendered text does contain `0.00`,
   does **not** contain `n/a`, and does not contain the banner.
3. With a real (partial) trace but `addresses_measured=False` the rendering
   still says `n/a` — the *flag*, not the observed addresses, drives the
   labelling, which is the only honest reading.

**Negative control** — revert the `n/a` rendering to the old `r.coverage_ratio:>5.2f`:

```text
AssertionError: note: no trace was supplied; every row's coverage and uncovered
counts are shown as 'n/a' / 0 because no instrument observed them, NOT because
every row is at 0%. The byte_size, largest_uncovered_run and range columns are
still meaningful.
```

The gate fails on the regression with the banner copy quoted verbatim.

`test_block_coverage.py` is now **25/25**. The factory chain (`vf2_python_factory_*`)
which uses this tool via `block_coverage` re-runs against it and stays green
(test entry `vf2_python_factory_block_coverage`).

## What this slice does NOT change

- The JSON output (one line per row) carries `coverage_ratio: 0.0` and
  `addresses_in_trace: 0` for an unmeasured row, because the existing
  per-row schema is the audit trail and changing it would invalidate the
  `test_function_report_round_trip_dict` contract. The `is_measured = False`
  banner belongs on the *report*, not on the *row*; the JSON consumer can
  decide its own unmeasured semantics.
- No CSV column was added. This is a presentation fix only.
- No `src/` change. `git diff --stat src` is empty.