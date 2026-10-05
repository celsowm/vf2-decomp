# v0732j: NO trace on disk has a shared fighter offset - the dual-base promotion input has never existed

**Evidence only. No behaviour change, no tuple admitted, no field promoted.**
This completes the correction promised in `p1_dualbase_blocker_v0732h.md` and
answers the question v0732i left open. It is a **negative** result, so it is
stated with the counts that back it.

## The exhaustive scan

`tools/python` cannot produce the promotion input because the input is not
there. Every `.jsonl` trace in `out/` under 6 MB was scanned for offsets
touched from **both** `0x510000 + n` and `0x520000 + n` (window `0x2000`):

```text
traces scanned                                 264
traces with any fighter-window access            92
traces with fighter1 access at all                3
traces with a single SHARED offset                0
```

Not one trace in the corpus has any offset touched from both bases. The
closest case is `out/player-19ef8-baseline.jsonl`, which is genuinely
dual-base and still disjoint:

```text
fighter0 32 offsets, fighter1 63 offsets, SHARED 0
```

The three traces with any fighter1 traffic at all:

| trace | fighter0 | fighter1 | shared |
|---|---|---|---|
| `out/p1-f1-valid.jsonl` (v0732h) | 2 | 300 | **0** |
| `out/p1-f0-mirror2.jsonl` (degenerate) | 0 | 2 | **0** |
| `out/player-19ef8-baseline.jsonl` | 32 | 63 | **0** |

So `infer_structs`' `base_count == 2` contract has **never had its input**,
in any corpus, at any point in this project. That is consistent with every
earlier report of `base_count = 1`, and it means the missing input is a
**game-state** problem, not a tooling or corpus-collection problem.

## What this means for the three open P1 items

| item | status after this scan |
|---|---|
| capture a fighter1-inclusive trace | **done** at v0732h, but it is single-base and disjoint |
| merge multiple traces | **done** at v0732i, but disjoint regions cannot merge into an overlap |
| produce a *shared-offset* trace | **the real blocker**, and it is not a tooling gap |

`0x27b5c` takes a single `player` pointer, so on its own it can only ever
produce one base. Producing a shared offset needs a state where the game
itself processes both fighters through the *same* offset - for example a
2-player VS state where the per-fighter loop runs to completion for both.
The v0732h measurement that `g7` selects the player (and that pointing it at
fighter1 degenerates) is consistent with that: no single-`player` helper can
give the overlap.

**This makes P1 a state-reconstruction problem.** It is not blocked on
`infer_structs`, on `frontier.py`, on taint, or on a longer reference run -
all of those are now demonstrably not the limiting factor.

## Re-derived taint numbers (the v0732h correction, closed out)

`p1_taint_0x1680_block_v0730.md` quoted

```text
branch 0x000222b0 depends on: fighter1 + 0x01a4 bit 14
branch 0x000223c4 depends on: fighter0 + 0x01a4 bit 0
                              fighter1 + 0x05b8 bit 0
```

as surfacing from `out/trace-both.jsonl`, which has zero fighter1 accesses.
Running `taint.py` against the fighter1 trace that *does* have them:

```sh
python tools/python/taint.py --rom-dir roms/vf2 \
  --vf2i960 build/Debug/vf2i960.exe \
  --scenario out/state8-posbit6-v0727.json \
  --trace out/p1-f1-valid.jsonl
```

```text
branches reported                  17
branches with fighter taint         0
```

All 17 branches in the `0x27b98..0x27c24` window report
`no fighter taint - condition clear or immediate`, even though this trace has
660 fighter1 accesses. That is the expected result: the fighter1 traffic is
the `cvtri`/`stis` conversion tail at `0x27cc4`/`0x27ccc`, which writes slots
and is not a control-flow decision.

**So the v0730 *conclusion* survives re-derivation** - the block/conversion
tail does not feed fighter-flag branches - even though the specific
`fighter1 + 0x01a4` lines in that note are not reproducible from any on-disk
trace with these bases. The note stays flagged; its headline finding is
independently confirmed on a trace that actually carries fighter1 accesses.

The 70-branch run quoted in v0730 came from a corpus that is not
`out/trace-both.jsonl`, and none of the 264 traces here reproduces it. That
part of the note remains unexplained and is left as such rather than
attributed to a file I cannot identify.

## Tools used

`out/sharedoff.py` (new, gitignored) does the shared-offset scan: it buckets
memory events by base and offset and prints the intersection, with per-side
read/write counts, access widths and guest IPs, plus the IP overlap. It exists
because answering "does any corpus have this?" by hand across 264 files is
exactly the kind of sample a human should not do. The v0732h note's
`basecov.py` gives the per-base roll-up and is still the right tool for
"which base does this trace touch".

Neither script is committed - they live in `out/` and are reproducible. If
the shared-offset scan turns out to be needed repeatedly it belongs in
`tools/python/` next to `infer_structs.py`, which already computes the same
intersection internally via `base_count`.

## Anti-traps

- Do not widen `--window` to manufacture a shared offset. The two fighter
  windows do not overlap, and a wider window finds more accesses, not more
  evidence.
- Do not read "0 of 264" as "the game never shares offsets between fighters".
  It says only that **no state we have captured** shows it. VF2 in a 2-player
  VS almost certainly does; the corpus just has not been captured there.
- Do not re-run `taint.py` against `out/trace-both.jsonl` and read fighter1
  tags as measured. Still wrong, still flagged.
- Do not treat the v0730 70-branch count as a property of the player chain.
  It is not reproducible from any trace on disk.

---

> **RETRACTED at v0732k.** Every dual-base claim below used the WRONG fighter
> bases (`0x510000`/`0x520000` instead of the measured
> `0x510980`/`0x512980`), so all fighter1 traffic was invisible. The
> conclusions here are wrong. `out/trace-both.jsonl` is genuinely dual-base
> (144 shared offsets, 341 events per base) and the 120-offset block is
> promoted with `base_count == 2`. The `0x27cc8` `cvtri` analysis in this note
> is unaffected and still valid - that boundary is real. See
> `p1_fighter_bases_retraction_v0732k.md`.
