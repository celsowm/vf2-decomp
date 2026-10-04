# Frontier v2 player-corpus smoke (v0729)

`tools/python/frontier.py` v2 was run on the existing dual-base player
corpus (`out/trace-both.jsonl` and `out/trace-f0.jsonl`) with both
fighter bases configured (`0x510000`, `0x520000`, window `0x2000`).
The run is reproducible:

```sh
python tools/python/frontier.py \
  out/trace-both.jsonl out/trace-f0.jsonl \
  --fighter-base 0x510000 --fighter-base 0x520000 \
  --fighter-window 0x2000 --limit 6 --json \
  --output out/frontier-player-v0729.jsonl
```

## What surfaced

- **Per-source attribution**: every edge in the top-6 has
  `sources: ["trace-both.jsonl", "trace-f0.jsonl"]` (both traces
  witnessed the edge), so `src` count = 2 for all rows in the text
  table.  This is the v2 proof that multi-input ingestion is
  attributable per edge.
- **Fighter offsets in the player corridor** (from the JSON trailer
  roll-up):
  - `fighter0 + 0x0b24` read 70 times at IPs `0x23a9c`, `0x233e0`,
    `0x238ac`, `0x2229c`, width 4 — corroborates the v0706
    `field_0b24` candidate (`player_270d4` byte-substructure was
    v0706-tier).
  - `fighter0 + 0x0980` read 60 times at IP `0x23a94`, width 4 —
    surfaces as a v2-only candidate (no prior `field_0980` notation
    in the v0702-v0707 notes; needs follow-up `infer_structs.py`
    pass to attribute).
  - `fighter0 + 0x11a0` read 5 times at `0x238c0`, `0x22450`,
    `0x23410`, width 1 — byte-substructure candidate.
  - `fighter0 + 0x0984` read 4 times at `0x23974`, `0x22404`,
    width 1 — sibling byte field to `0x0980`.
- The two traces together cover 9528+9393 = 18,921 measured
  instructions and 3122+3075 = 6197 fighter-window accesses inside
  the recovered `main_texture_orchestrator_call` block.

## Next evidence pass

- The next P1 slice (Layer 1, P1 in the v0729 plan) should run
  `infer_structs.py` against the same `trace-both.jsonl` to confirm
  `0x0980` and `0x0984` as dual-base stable fields, then run
  `taint_branch.py` on `0x23a94` (the dominant IP touching
  `0x0980`) to characterise the branch it depends on. This is the
  planned evidence path into the first non-collider reached from
  `0x29414` natively.

## Untouched

- No new recovery claim is made from this run. Frontier v2 only
  surfaces measured signals; the next slice writes the C recovery
  after a separate `vf2cmp`-proven differential pass.
- The frontier output JSONL lives in `out/` and is gitignored per
  `AGENTS.md` repository hygiene rules.