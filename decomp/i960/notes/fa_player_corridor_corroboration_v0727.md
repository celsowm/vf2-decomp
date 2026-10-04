# `fa_player` corridor corroboration and 270d4 slot-walk fields (v0727)

## Fresh fighter-metadata scenario

`make_game_info_probe_scenario.py` was re-run from the live `0x164ac`
boundary to regenerate a fighter-metadata scenario
(`out/state8-posbit6-v0727.json`, fighters `0x00510980/0x00512980`,
mode `0x0059c351`, state 8, bits 1,2,4,6,8,21,26,29,30,31) plus a fresh
`trace_case.py` case. `infer_structs.py` over that trace revalidates
exactly the committed game-info window fields with identical
widths/roles and dual-base attribution (no new offsets, no role
changes): `+0x1f4/+0x1fc/+0x0/+0x5b4/+0x5b8/+0x1a4/+0x844/+0x5f4
/+0x19f/+0x1200`.

## 270d4 slot-walk trace

`vf2probe --snapshot out/park-player-270d4.vf2snap --until 0x10dcc
--trace --memory-trace` (1,678 records, 277 successful accesses;
275 non-fighter) shows the five-slot record walk is fighter0-scoped
in this park: two single-base reads only:

- `fighter0 + 0x01a0` 4B R at ip `0x270d4` (the record-chain pointer
  the slot walk iterates; sits in the header's current
  `_pad_01a0` padding);
- `fighter0 + 0x0bd8` 4B R at ip `0x270d8` (4B neighbor of the
  committed `field_0bdc` census byte).

Both fail the header's dual-base bar (the walk is single-subject by
construction) and stay note-only candidates — no struct change.
Promoting the v0701 single-corridor fields (`001c`/`0804`/`0bdc`) to
dual-base still needs a bilateral-live corridor trace (unchanged
from the v0701 follow-up).

## Taint wiring check

`taint.py` runs end-to-end on the fresh scenario + trace
(`out/state8-taint-case.jsonl`), producing sideband
branch-dependency evidence in the AGENTS.md shape, e.g.
`branch 0x000186c0 depends on: fighter1 + 0x01a4 bit 4,
fighter1 + 0x0844 bit 4` and the `0x18698` bit-6 example target.
No behavior change; tooling only.
