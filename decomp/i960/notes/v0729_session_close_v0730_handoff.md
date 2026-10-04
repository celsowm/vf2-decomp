# v0729 session close-out + v0730 handoff (the current state)

## What landed this session (master, v0729a-g)

A factory tooling layer that turns the v0729 plan into a callable
playbook for the next F/P1 slice. Every component is wired into the
canonical ctest gate (`ctest -C Debug -R vf2_python_factory
--output-on-failure`, 6/6 green).

| commit | slice | scope |
| --- | --- | --- |
| `0bf288d6` | v0729a | `frontier.py v2` (per-edge fighter offsets + per-source + fighter-aware rank) |
| `90c354f6` | v0729b | `test_taint.py` (7 unit tests) |
| `f02da40f` | v0729c | `test_taint_e2e.py` (21 branch blocks / 24 fighter deps on real corpus) |
| `8c998337` | v0729d | `test_infer_structs.py` (4 unit tests) |
| `104510da` | v0729e | `test_infer_rules.py` (10 unit tests) |
| `1c3bed53` | v0729g | `test_check_scenario.py` (9 unit tests) |
| `bbc715ae` | v0729f | All 5 + check_scenario wired into ctest |

Plus six documentation commits that update CHANGELOG,
UNCOVERED_BRANCHES, AGENTS.md and the factory runbook.

## Cumulative v0729 factory gate (45 total)

```text
vf2_python_factory_frontier     14/14
vf2_python_factory_taint        7/7
vf2_python_factory_taint_e2e    1/1   (21 branch blocks validated)
vf2_python_factory_infer_structs 4/4
vf2_python_factory_infer_rules  10/10
vf2_python_factory_check_scenario 9/9
total                            45   6.59s ctest time
```

## The factory chain (use this for the next F/P1 slice)

The factory runbook (`factory_runbook_v0729.md`) is the playbook:

1. surface the next frontier edge with frontier.py v2;
2. promote candidate fighter fields with infer_structs.py;
3. characterise the branch dependency with taint.py;
4. gate every promotion with the six ctest suites above;
5. write a per-slice `playable_*` or `fa_player_*` note.

When the discovery starts from a sweep scenario rather than a hand-rolled
trace, `tools/python/infer_rules.py` adds a fifth tool (refusal mode
when the truth table is incomplete / features do not determine the
outcome) and `tools/python/check_scenario.py` is the upstream gate.

## Evidence surfaced this session that the next slice should consume

### TEST MENU corridor (Layer 0)

- `decomp/i960/notes/factory_runbook_v0729.md` — five-step playbook
  for the next F slice.
- The v0727 "Still open" list (MANUAL SETTING natural entry, rows 2-4
  post-edit release frames, PUNCH/KICK releases at value rows, KICK
  −1 from INDIVIDUAL, INDIVIDUAL walks at rows 2-4) is the un-done
  frontier. AGENTS.md "Recommended next work" has been re-ordered to
  reflect that.

### fa_player corridor (Layer 1)

- `decomp/i960/notes/frontier_v2_player_corpus_v0729.md` — player
  corpus smoke: `0x0b24`, `0x0980`, `0x11a0`, `0x0984`,
  `0x1680-1688` candidates from frontier v2.
- `decomp/i960/notes/fa_player_struct_1680_corpus_v0729.md` — **new**:
  29+ contiguous 4-byte R/W fields at fighter offsets 0x1680-0x16f4,
  stable across `trace-both.jsonl` and `trace-f0.jsonl`, both
  accessed by the same pair of guest IPs (`0x2399c` + `0x23a38`).
  Provisional struct promotion with neutral `field_xxxx` names; no
  semantic rename, no recovery claim. **This is the documented P1
  entry evidence the v0729 plan called for.**

### Proven taint output (Layer 2 evidence)

Real branches and their measured dependencies on the dual-base player
trace (output captured this session):

```text
branch 0x000223c4 depends on:
  fighter0 + 0x01a4 bit 0
  fighter1 + 0x05b8 bit 0
branch 0x00022420 depends on:
  fighter0 + 0x01a4 bit 8
  fighter1 + 0x01a4 bit 8
branch 0x000233fc depends on:
  fighter0 + 0x01a4 bit 18
  fighter1 + 0x01a4 bit 18
branch 0x0002384c depends on:
  fighter0 + 0x0650 bit 31
branch 0x00023868 depends on:
  fighter1 + 0x0650 bit 31
branch 0x00023a98 depends on:
  fighter1 + 0x0000 bit 2
branch 0x00023aa0 depends on:
  fighter1 + 0x01a4 bit 23
```

`branch 0x000223c4` is the v0704 dual-base example candidate — both
bases are present in the same dependency tuple, which is exactly the
shape the v0704 discipline requires for promotion.

## Untouched on purpose

- The TEST MENU recovery slices (F1-F5) — multi-hour ROM-backed work
  per the v0727 playbook. The factory is now in place to attack these
  one at a time.
- The `fa_player` corridor past `0x28918` / `0x29414` (P1-P4) — the
  contiguous `0x1680` block is the documented P1 entry evidence;
  full P1 recovery requires a fighter1-inclusive trace plus the
  strict build/diff gate.
- The frame-dispatch selector 3 phase ≥8 (`0x9444` handoff, S1) —
  Layer 3 long-pole work that the runbook explicitly says NOT to
  use the v2 factory on.

## How to pick up this session

1. Read `factory_runbook_v0729.md` first.
2. For TEST MENU completion: start at v0727 "Still open" #89
   (MANUAL SETTING natural entry). Each slice is a single commit per
   the v0727 playbook.
3. For `fa_player` continuation: read
   `fa_player_struct_1680_corpus_v0729.md` first — the contiguous
   `0x1680` block is the measured entry. Run taint `--until 0x2399c`
   on a fighter1-inclusive trace to confirm dual-base provenance
   (the open evidence gap), then commit a per-field note + the C
   recovery.

The factory runbook is the durable contract for what to do next;
the ctest gate is the durable contract for what "still working" looks
like.