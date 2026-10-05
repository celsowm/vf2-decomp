# v0732i: infer_structs consumes multiple traces; the corpus still does not overlap (v0732h follow-up)

**Tooling slice. No game behaviour change, no tuple admitted, no field
promoted.** This is the concrete next P1 step named in
`p1_dualbase_blocker_v0732h.md`.

## What changed

`tools/python/infer_structs.py` now takes one **or more** traces and rolls
them up into a single per-offset view, so same-offset evidence from
different fighter bases can meet in one candidate.

- `summarize_traces(paths, bases, window)` merges per-offset roll-ups.
- `merge_fields(target, source)` adds counters, unions base sets **and**
  unions per-base trace provenance.
- The `trace` positional became `nargs="+"`; a one-trace invocation behaves
  exactly as before.
- `field_records` gained `base_traces` (base name -> trace labels) and
  `trace_count` (how many distinct runs back the field).
- `--json` gained `traces` and `base_sources`; the human report gained a
  trace list, a per-base source list, and a `provenance:` line on every
  multi-base candidate.

`summarize_trace` is untouched, so anything importing it directly keeps
working.

### Why per-base provenance is not decoration

A cross-trace promotion is a weaker claim than a within-trace one: the two
bases were observed in two *different runs*, so a reader has to be able to
check that. `provenance: fighter0<-out/trace-both.jsonl;
fighter1<-out/p1-f1-valid.jsonl` makes that checkable in one line, and
`base_count == 2` on its own would hide it.

## The unit suite: 8/8

`tools/python/test_infer_structs.py` was 4 cases and is now 8. The four new
ones:

| test | what it pins |
|---|---|
| `test_summarize_traces_single_trace_matches_summarize_trace` | merging one trace is byte-identical to `summarize_trace` - the backward-compatibility pin |
| `test_summarize_traces_promotes_across_traces` | trace A touches only fighter0+`0x1a4`, trace B only fighter1+`0x1a4`; neither promotes alone, together they reach `base_count == 2` with `trace_count == 2` and `base_traces` naming both runs |
| `test_merge_fields_unions_counters_and_provenance` | counters add, sets union, a source-only offset is created rather than dropped, and a plain `dict` target works |
| `test_field_records_tolerates_missing_provenance` | a field built without `base_traces` still serialises, so older record shapes stay readable |

The cross-trace case also asserts the *same-IP* half of the v0704
discipline: both runs use the same instruction pair, so the merged IP counts
**double** (2 each) rather than the IP set growing. That doubling is the
evidence both runs used the same code path.

Two defects were found and fixed while writing these, both mine:

1. `merge_fields` did `target[offset]`, which requires a
   `defaultdict(new_field)`. A plain dict raised `KeyError` on a
   source-only offset. Now uses `setdefault`.
2. The first draft of the cross-trace test asserted merged IP counts of 1
   each. That was wrong - the correct answer is 2 each, because each run
   hits each IP once. The test was wrong, not the code; the assertion was
   rewritten to explain why 2 is the meaningful value.

`test_factory_chain.py` and `test_frontier.py` both still pass, and the 14
`vf2_python_factory_*` / `vf2_phase17_zero` ctest entries pass (9.49 s).

## The honest result on the real corpus: nothing promotes

Running the new roll-up over the two real corpora:

```sh
python tools/python/infer_structs.py \
  out/trace-both.jsonl out/p1-f1-valid.jsonl \
  --base fighter0=0x510000 --base fighter1=0x520000 \
  --window 0x2000 --min-count 2
```

```text
traces: 2
  out/trace-both.jsonl
  out/p1-f1-valid.jsonl
trace accesses: 4977
base sources:
  fighter0: out/p1-f1-valid.jsonl, out/trace-both.jsonl
  fighter1: out/p1-f1-valid.jsonl
unmatched accesses: 3974

+0x0b24  bases=fighter0  R=35 W=0
+0x0980  bases=fighter0  R=30 W=0
+0x01e0  bases=fighter1  R=1  W=2
+0x01e4  bases=fighter1  R=1  W=2
...
```

**No candidate reaches `base_count == 2`,** because the two corpora cover
**disjoint offset regions**: `trace-both.jsonl` touches fighter0 at
`0x0980`/`0x0b24`-ish, while `p1-f1-valid.jsonl` touches fighter1 at
`0x01e0..0x068c`. There is no shared offset for the bases to meet on.

This is the tool behaving correctly on insufficient evidence: it refuses to
promote rather than widening the gate to make a result appear. The
capability is real (the synthetic cross-trace test proves it) and the corpus
is what is missing.

## One more measured negative: the player base *is* selected through `g7`

`p1_dualbase_blocker_v0732h.md` left the selection path unidentified. It is:

```text
--set-reg g7=0x00510980   ->  player = 0x510000  ->  9235 ins, ok,   660 fighter1 accesses
--set-reg g7=0x00520980   ->  player = 0x520000  ->  1400 ins, FAIL at 0x27cc8
```

so `g7` selects the player, and patching the mirrored
`player + 0xbd8` / `player + 0x1a0` is the right idea - but the fighter-1
player **degenerates**: the run collapses back to the documented
`0x27cc8` `cvtri` refusal, which is what happens when the scratch selector
does not resolve. The selector is not read from the same place the scratch
base is, so the mirrored pointers alone are not enough to produce the
fighter-0 mirror of `p1-f1-valid.jsonl`.

That is the next thing to characterise if a genuinely overlapping corpus is
wanted: find where the selector comes from, then reproduce the
`p1-f1-valid.jsonl` shape with `player = 0x520000`. Until then the
overlapping-corpus requirement stands.

## What is still not done

- The `0x1680` block remains single-base. The new corpus covers a different
  region, so it neither confirms nor refutes the v0730 stability finding.
- No field is promoted. Promotion stays an analyst decision recorded in a
  note, and the tool only ever *reports*.
- The `p1_dualbase_blocker_v0732h.md` correction to the v0730 taint note
  still stands and is untouched by this slice.

## Anti-traps

- Do not read `base_count == 2` as a promotion. Check `base_traces` and
  `trace_count` first; if the two bases came from one run the claim is
  stronger, and if they came from two runs it needs the same-IP and
  same-width checks on top.
- Do not tune `--window` upward to manufacture overlap. A wider window finds
  more accesses, not more evidence.
- Do not use the synthetic cross-trace test as evidence about the game. It
  pins the tool's contract; only a real corpus is evidence about VF2.
