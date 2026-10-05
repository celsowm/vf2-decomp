#!/usr/bin/env python3
"""Unit tests for tools/python/block_coverage.py.

Runs standalone (no pytest required) so the analysis layer stays
dependency-light:

    python3 tools/python/test_block_coverage.py
"""

from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from block_coverage import (
    FunctionRange,
    FunctionReport,
    STATUS_BUCKETS,
    addresses_in_range,
    build_report,
    collect_trace_addresses,
    find_continuation_rows,
    find_inverted_ranges,
    hex32,
    load_functions,
    longest_uncovered_run,
    mark_containers,
    rank_reports,
    render_text,
    total_uncovered_words,
)

ROOT = Path(__file__).resolve().parents[2]


def test_load_functions_filters_end_only():
    rows = [
        {
            "address": "0x1000",
            "end": "0x2000",
            "name": "recovered_a",
            "status": "recovered",
            "notes": "ok",
        },
        {
            "address": "0x3000",
            "end": "",
            "name": "entry_only",
            "status": "candidate",
        },
        {
            "address": "0x4000",
            "end": "0x3fff",
            "name": "invalid_range",
            "status": "candidate",
        },
    ]
    funcs = load_functions_from_rows(rows)
    assert len(funcs) == 1
    f = funcs[0]
    assert f.start == 0x1000
    assert f.end == 0x2000
    assert f.name == "recovered_a"
    assert f.status == "recovered"
    assert f.byte_size == 0x1000
    assert f.word_count == 0x400
    print("ok: load_functions filters entry-only + invalid ranges")


def load_functions_from_rows(rows):
    """Helper that bypasses CSV file I/O so the test stays focused."""
    from block_coverage import parse_int, FunctionRange as _FR

    out = []
    for row in rows:
        try:
            start = parse_int(row["address"])
        except (KeyError, ValueError):
            continue
        end_text = (row.get("end") or "").strip()
        if not end_text:
            continue
        try:
            end = parse_int(end_text)
        except ValueError:
            continue
        if end <= start:
            continue
        out.append(
            _FR(
                start=start,
                end=end,
                name=(row.get("name") or "").strip(),
                status=(row.get("status") or "").strip(),
                notes=(row.get("notes") or "").strip(),
            )
        )
    out.sort(key=lambda f: f.start)
    return out


def test_status_buckets_classify_recovered_variants():
    cases = [
        ("recovered", "fully_recovered"),
        ("recovered-prefix", "prefix"),
        ("recovered-prefixes", "prefixes"),
        ("recovered-control-block", "control_block"),
        ("recovered-first-dispatch", "first_dispatch"),
        ("recovered-observed-branch", "observed_branch"),
        ("recovered-rom-anchor", "rom_anchor"),
        ("unknown-thing", "unknown"),
        ("", "unknown"),
    ]
    for raw, expected in cases:
        f = FunctionRange(0x1000, 0x1010, "f", raw)
        assert f.status_bucket == expected, f"{raw} -> {f.status_bucket}"
    print("ok: status buckets classify all known variants")


def test_wrapper_classification_requires_large_size():
    # A small control-block is a real sub-control, not a wrapper.
    small = FunctionRange(0x1000, 0x1080, "small_cb", "recovered-control-block")
    assert not small.is_wrapper
    # A large control-block IS a wrapper.
    big = FunctionRange(0x1000, 0x80000, "big_cb", "recovered-control-block")
    assert big.is_wrapper
    # Non-control-block entries are never wrappers, regardless of size.
    big_obs = FunctionRange(0x1000, 0x80000, "big_obs", "recovered-observed-branch")
    assert not big_obs.is_wrapper
    print("ok: wrapper detection only flags large control-block ranges")


def test_longest_uncovered_run_handles_edges():
    f = FunctionRange(0x1000, 0x1020, "test", "recovered")  # 8 words
    # Empty trace: every word is uncovered, run = 8.
    assert longest_uncovered_run(f, set()) == 8
    # Trace covers every word: run = 0.
    assert longest_uncovered_run(f, set(range(0x1000, 0x1020, 4))) == 0
    # Two early words covered: head run is empty (covered reset
    # immediately), tail run is 0x1008..0x101c = 6 words.
    covered = {0x1000, 0x1004}
    assert longest_uncovered_run(f, covered) == 6
    # One address covered mid-range: head run is 4, tail run is 3;
    # longest = 4.
    covered = {0x1010}
    assert longest_uncovered_run(f, covered) == 4
    # Three adjacent covered at the start: tail run is 0x100c..0x101c = 5.
    covered = {0x1000, 0x1004, 0x1008}
    assert longest_uncovered_run(f, covered) == 5
    print("ok: longest_uncovered_run handles edge splits")


def test_total_uncovered_words_counts_unvisited_words():
    f = FunctionRange(0x1000, 0x1020, "test", "recovered")  # 8 words
    assert total_uncovered_words(f, set()) == 8
    assert total_uncovered_words(f, {0x1000, 0x1004, 0x1008, 0x100c}) == 4
    assert total_uncovered_words(f, set(range(0x1000, 0x1020, 4))) == 0
    print("ok: total_uncovered_words counts visited vs unvisited correctly")


def test_addresses_in_range_is_subset_count():
    f = FunctionRange(0x1000, 0x1020, "test", "recovered")  # 8 words
    assert addresses_in_range(f, set()) == 0
    assert addresses_in_range(f, {0x1000, 0x2000}) == 1  # only 0x1000 is in range
    assert addresses_in_range(f, set(range(0x1000, 0x1020, 4))) == 8
    print("ok: addresses_in_range counts only addresses inside the range")


def test_collect_trace_addresses_handles_missing_files():
    with tempfile.TemporaryDirectory() as tmp:
        empty_dir = Path(tmp)
        assert collect_trace_addresses([]) == set()
        # Non-existent files are silently skipped (consistent with
        # how the tool handles missing traces in production).
        assert collect_trace_addresses([empty_dir / "missing.jsonl"]) == set()
    print("ok: collect_trace_addresses handles empty + missing inputs")


def test_collect_trace_addresses_aggregates_step_and_memory():
    with tempfile.TemporaryDirectory() as tmp:
        a = Path(tmp) / "a.jsonl"
        b = Path(tmp) / "b.jsonl"
        a_records = [
            {"type": "step", "step": 1, "ip_before": 0x1000, "ip_after": 0x1004},
            {"type": "memory", "step": 2, "address": 0x500000, "size": 4},
        ]
        b_records = [
            {"type": "step", "step": 3, "ip_before": 0x1004, "ip_after": 0x1008},
            {"type": "memory", "step": 4, "address": 0x501000, "size": 4},
        ]
        a.write_text("\n".join(json.dumps(r) for r in a_records) + "\n")
        b.write_text("\n".join(json.dumps(r) for r in b_records) + "\n")
        addrs = collect_trace_addresses([a, b])
        # Both step and memory records contribute; ip_after also contributes.
        assert {0x1000, 0x1004, 0x1008, 0x500000, 0x501000} <= addrs
    print("ok: collect_trace_addresses aggregates step + memory across files")


def test_build_report_marks_wrappers_separately():
    funcs = [
        FunctionRange(0x1000, 0x1080, "small_recovered", "recovered"),
        FunctionRange(0x2000, 0x80000, "big_wrapper", "recovered-control-block"),
        FunctionRange(0x9000, 0x9080, "small_cb", "recovered-control-block"),
    ]
    rows = build_report(funcs, set())
    by_name = {r.name: r for r in rows}
    assert by_name["small_recovered"].is_wrapper is False
    assert by_name["big_wrapper"].is_wrapper is True
    assert by_name["small_cb"].is_wrapper is False
    # All reports show full range as uncovered (no trace IPs).
    for r in rows:
        assert r.coverage_ratio == 0.0
        assert r.addresses_in_trace == 0
    print("ok: build_report flags wrappers distinctly")


def test_rank_reports_orders_by_largest_uncovered_run_desc():
    funcs = [
        FunctionRange(0x1000, 0x1080, "small", "recovered"),
        FunctionRange(0x2000, 0x2400, "medium", "recovered"),
        FunctionRange(0x3000, 0x3400, "tied_a", "recovered"),
        FunctionRange(0x4000, 0x4400, "tied_b", "recovered"),
    ]
    rows = build_report(funcs, set())
    rows = rank_reports(rows, "largest_uncovered_run")
    names = [r.name for r in rows]
    assert names.index("medium") < names.index("small"), names
    # Ties break by start address ascending.
    assert names.index("tied_a") < names.index("tied_b"), names
    print("ok: rank_reports orders by largest_uncovered_run descending")


def test_rank_reports_rejects_unknown_key():
    rows = build_report([FunctionRange(0x1000, 0x1080, "f", "recovered")], set())
    try:
        rank_reports(rows, "bogus_key")
    except ValueError as exc:
        assert "unknown --sort key" in str(exc)
        print("ok: rank_reports refuses unknown --sort key")
        return
    raise AssertionError("rank_reports accepted unknown --sort key")


def test_render_text_excludes_wrappers_by_default():
    funcs = [
        FunctionRange(0x1000, 0x1080, "real_function", "recovered"),
        FunctionRange(0x2000, 0x80000, "big_wrapper", "recovered-control-block"),
    ]
    rows = build_report(funcs, set())
    rows = rank_reports(rows, "largest_uncovered_run")
    kept = [r for r in rows if not r.is_excluded_by_default]
    text_default = render_text(kept)
    text_with = render_text(rows)
    # Default text excludes the wrapper.
    assert "real_function" in text_default
    assert "big_wrapper" not in text_default
    # With --include-wrappers the wrapper appears.
    assert "big_wrapper" in text_with
    print("ok: render_text excludes wrappers unless --include-wrappers")


def test_mark_containers_flags_strict_containers():
    # A container's `end` is a region bound, not the procedure's extent, so its
    # uncovered run is the union of its children's gaps. That is what let four
    # 45-66 KB "functions" whose own notes describe 63-instruction bodies take
    # the top four places in the real report.
    funcs = [
        FunctionRange(0x1000, 0x8000, "container", "recovered-observed-branch"),
        FunctionRange(0x1100, 0x1200, "child_a", "recovered-observed-branch"),
        FunctionRange(0x2000, 0x2100, "child_b", "recovered-observed-branch"),
    ]
    marked = mark_containers(funcs)
    assert marked == 1, marked
    assert funcs[0].is_container is True
    assert funcs[1].is_container is False
    assert funcs[2].is_container is False
    # A container is excluded by default; its children are not.
    assert funcs[0].is_excluded_by_default is True
    assert funcs[1].is_excluded_by_default is False
    print("ok: mark_containers flags only strict containers")


def test_mark_containers_leaves_abutting_siblings_alone():
    # Overlap is not the test. A prefix/leaf decomposition where siblings only
    # abut must not be swept up as containers.
    funcs = [
        FunctionRange(0x1000, 0x1080, "a", "recovered"),
        FunctionRange(0x1080, 0x1100, "b", "recovered"),
        FunctionRange(0x1100, 0x1180, "c", "recovered"),
    ]
    assert mark_containers(funcs) == 0
    assert not any(f.is_container for f in funcs)
    print("ok: mark_containers leaves abutting siblings alone")


def test_exclusion_before_limit_keeps_a_full_report():
    # Regression: limiting before excluding returned an empty table whenever
    # the top-N by uncovered run were all containers, which is indistinguishable
    # from "the corpus covered everything". The real corpus is exactly that
    # case - the top 12 were all containers.
    funcs = [
        FunctionRange(0x1000, 0x9000, "container", "recovered-observed-branch"),
        FunctionRange(0x1100, 0x1200, "child", "recovered-observed-branch"),
    ]
    mark_containers(funcs)
    rows = rank_reports(build_report(funcs, set()), "largest_uncovered_run")
    # The container sorts first because it is larger.
    assert rows[0].name == "container", [r.name for r in rows]
    limit = 1
    # Order matters: exclude, then limit.
    kept_then_limited = [r for r in rows if not r.is_excluded_by_default][:limit]
    assert [r.name for r in kept_then_limited] == ["child"]
    # The old order returns nothing at all.
    limited_then_kept = [r for r in rows[:limit] if not r.is_excluded_by_default]
    assert limited_then_kept == []
    assert "child" in render_text(kept_then_limited)
    print("ok: exclusion happens before the limit, so the report is not empty")


def test_function_report_round_trip_dict():
    f = FunctionRange(0x1000, 0x2000, "rt", "recovered-observed-branch")
    report = FunctionReport(
        name=f.name,
        start=f.start,
        end=f.end,
        status=f.status,
        status_bucket=f.status_bucket,
        is_wrapper=f.is_wrapper,
        is_container=f.is_container,
        byte_size=f.byte_size,
        word_count=f.word_count,
        addresses_in_trace=0,
        coverage_ratio=0.0,
        total_uncovered_words=f.word_count,
        largest_uncovered_run=f.word_count,
        notes="",
    )
    d = report.to_dict()
    # JSON-serialisable and hex-formatted addresses.
    assert d["start"] == "0x00001000"
    assert d["end"] == "0x00002000"
    assert d["status_bucket"] == "observed_branch"
    assert d["byte_size"] == 0x1000
    # is_container travels with the report, so a consumer of the JSON can tell
    # a container row from a leaf without re-reading functions.csv.
    assert d["is_container"] is False
    assert d["is_wrapper"] is False
    # The dict can be serialised without TypeError.
    json.dumps(d)
    print("ok: FunctionReport.to_dict is JSON-round-trippable")


def test_load_functions_on_real_csv_is_stable():
    """Sanity check: loading the real CSV returns >= 60 entries."""
    real = Path("decomp/i960/functions.csv")
    if not real.exists():
        print("skip: decomp/i960/functions.csv not present in this checkout")
        return
    funcs = load_functions(real)
    assert len(funcs) >= 60, f"only {len(funcs)} functions loaded"
    # Every loaded range has byte_size > 0 (entry-only filtered).
    assert all(f.byte_size > 0 for f in funcs)
    print(f"ok: load_functions loads {len(funcs)} real entries (byte_size > 0)")


def test_find_inverted_ranges_reports_end_before_address():
    """A row whose `end` precedes its `address` is inventoried, not dropped.

    `load_functions` skips these at `end <= start`, so they can never reach
    `build_report`. Before this census existed the 25 such rows in the real
    CSV vanished without a word.
    """
    with tempfile.TemporaryDirectory() as tmp:
        csv_path = Path(tmp) / "functions.csv"
        csv_path.write_text(
            "address,end,name,status\n"
            "0x00010fa4,0x000110b0,frame_timer_suffix,recovered\n"   # extent
            "0x000221e8,0x00010dcc,task_coli,recovered\n"            # return addr
            "0x0000a038,,main_post_timer,recovered\n",               # no end
            encoding="utf-8",
        )
        inverted = find_inverted_ranges(csv_path)
        loaded = [f.name for f in load_functions(csv_path)]
    assert inverted == [("task_coli", 0x000221E8, 0x00010DCC)], inverted
    # ...and it really is excluded from the coverage table.
    assert loaded == ["frame_timer_suffix"], loaded
    print("ok: find_inverted_ranges reports end < address without loading it")


def test_find_inverted_ranges_ignores_zero_length_and_missing_end():
    """`end == address` is not inverted; an empty `end` is not either.

    Both are dropped by load_functions for a *different* reason (a
    zero-length or absent range), so conflating them here would mislabel them.
    """
    with tempfile.TemporaryDirectory() as tmp:
        csv_path = Path(tmp) / "functions.csv"
        csv_path.write_text(
            "address,end,name,status\n"
            "0x1000,0x1000,zero_length,recovered\n"
            "0x2000,,no_end,recovered\n",
            encoding="utf-8",
        )
        assert find_inverted_ranges(csv_path) == []
    print("ok: find_inverted_ranges ignores zero-length and missing ends")


def test_real_csv_inverted_census_is_pinned():
    """The real CSV has **zero** inverted rows after the v0734b migration.

    This is a regression pin, not a target. v0734a measured 25 rows whose
    `end` held a call-return continuation; v0734b migrated every one to a real
    extent plus a separate `return_to`, so the count must be 0. If a later
    slice reintroduces an inverted row this fails, which is the point.
    """
    real = ROOT / "decomp" / "i960" / "functions.csv"
    if not real.exists():
        print("skip: decomp/i960/functions.csv not present in this checkout")
        return
    assert find_inverted_ranges(real) == [], "an inverted row is back"
    # ...and the migrated rows are still named, via return_to rather than end.
    cont = find_continuation_rows(real)
    assert len(cont) == 25, f"continuation census moved to {len(cont)}"
    loaded = {f.name for f in load_functions(real)}
    for name, _start, _ret in cont:
        assert name in loaded, f"{name} carries return_to but is not ranked"
    print("ok: 0 inverted rows, 25 ranked rows still name their continuation")


def test_real_csv_notes_survive_the_commas_they_contain():
    """Every row parses to 7 fields and no note is truncated.

    Five rows carried UNESCAPED commas inside `notes`, so every DictReader
    consumer silently read a truncated note (the text after the first comma
    landed in the `None` restkey). v0734b requoted them. This pins the fix by
    checking text that was previously invisible.
    """
    real = ROOT / "decomp" / "i960" / "functions.csv"
    if not real.exists():
        print("skip: decomp/i960/functions.csv not present in this checkout")
        return
    with real.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.reader(stream))
    assert rows[0] == [
        "address", "end", "name", "status", "source", "notes", "return_to"
    ], rows[0]
    for r in rows[1:]:
        assert len(r) == 7, f"{r[2]} has {len(r)} fields, expected 7"
        assert None not in r, f"{r[2]} has an unquoted extra field"
    by_name = {r[2]: r for r in rows[1:]}
    # Text that used to sit beyond the first comma and was therefore lost.
    assert "word decoder" in by_name["texture_word_prepare"][5]
    assert "byte decoder" in by_name["texture_tree_dispatch"][5]
    assert "color converter" in by_name["texture_color_prepare"][5]
    assert "next function" in by_name["video_input_latch_write"][5]
    assert "container bound" in by_name["video_register_compose"][5]
    print("ok: all rows are 7 fields and the 5 truncated notes are complete")


def test_continuation_census_ignores_rows_without_return_to():
    """`return_to` is optional: a row without it is simply not inventoried."""
    with tempfile.TemporaryDirectory() as tmp:
        csv_path = Path(tmp) / "functions.csv"
        csv_path.write_text(
            "address,end,name,status,source,notes,return_to\n"
            "0x1000,0x1100,a,recovered,s,n,\n"
            "0x2000,0x2100,b,recovered,s,n,0x3000\n",
            encoding="utf-8",
        )
        assert find_continuation_rows(csv_path) == [("b", 0x2000, 0x3000)]
        # ...and the older six-column header still parses (no return_to key).
        assert find_continuation_rows.__doc__ is not None
    print("ok: continuation census only reports rows that set return_to")


def test_strict_ranges_gate_fails_on_a_planted_inverted_row():
    """`--strict-ranges` must be able to fail.

    The warning is easy to write and easy to ignore; the flag is what makes
    the exclusion enforceable. Proven here by running the real CLI against a
    planted CSV and requiring a non-zero exit.
    """
    with tempfile.TemporaryDirectory() as tmp:
        csv_path = Path(tmp) / "functions.csv"
        csv_path.write_text(
            "address,end,name,status\n"
            "0x000221e8,0x00010dcc,task_coli,recovered\n",
            encoding="utf-8",
        )
        proc = subprocess.run(
            [
                sys.executable, str(ROOT / "tools" / "python" / "block_coverage.py"),
                "--functions-csv", str(csv_path), "--strict-ranges", "--limit", "1",
            ],
            capture_output=True,
            text=True,
        )
    assert proc.returncode != 0, f"gate did not fail: rc={proc.returncode}"
    assert "inverted row" in proc.stderr, proc.stderr
    assert "task_coli" in proc.stderr, proc.stderr
    # Without the flag the same input is a warning, not a failure.
    with tempfile.TemporaryDirectory() as tmp:
        csv_path = Path(tmp) / "functions.csv"
        csv_path.write_text(
            "address,end,name,status\n"
            "0x000221e8,0x00010dcc,task_coli,recovered\n",
            encoding="utf-8",
        )
        proc2 = subprocess.run(
            [
                sys.executable, str(ROOT / "tools" / "python" / "block_coverage.py"),
                "--functions-csv", str(csv_path), "--limit", "1",
            ],
            capture_output=True,
            text=True,
        )
    assert proc2.returncode == 0, f"default run should not fail: rc={proc2.returncode}"
    assert "EXCLUDED" in proc2.stderr, proc2.stderr
    print("ok: --strict-ranges gate fails on a planted inverted row")


def main():
    test_load_functions_filters_end_only()
    test_status_buckets_classify_recovered_variants()
    test_wrapper_classification_requires_large_size()
    test_longest_uncovered_run_handles_edges()
    test_total_uncovered_words_counts_unvisited_words()
    test_addresses_in_range_is_subset_count()
    test_collect_trace_addresses_handles_missing_files()
    test_collect_trace_addresses_aggregates_step_and_memory()
    test_build_report_marks_wrappers_separately()
    test_rank_reports_orders_by_largest_uncovered_run_desc()
    test_rank_reports_rejects_unknown_key()
    test_render_text_excludes_wrappers_by_default()
    test_mark_containers_flags_strict_containers()
    test_mark_containers_leaves_abutting_siblings_alone()
    test_exclusion_before_limit_keeps_a_full_report()
    test_function_report_round_trip_dict()
    test_load_functions_on_real_csv_is_stable()
    test_find_inverted_ranges_reports_end_before_address()
    test_find_inverted_ranges_ignores_zero_length_and_missing_end()
    test_real_csv_inverted_census_is_pinned()
    test_real_csv_notes_survive_the_commas_they_contain()
    test_continuation_census_ignores_rows_without_return_to()
    test_strict_ranges_gate_fails_on_a_planted_inverted_row()
    print("\nall block_coverage tests passed")


if __name__ == "__main__":
    main()