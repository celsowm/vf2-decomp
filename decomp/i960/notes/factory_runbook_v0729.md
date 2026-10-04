# Factory runbook: chained use of frontier v2 + taint + infer_structs (v0729)

This note is the playbook for how the v0729 factory tooling chains
together to attack the next F-slice (TEST MENU) or P-slice
(`fa_player`) recovery. Each tool has a unit-test gate; the runbook
tells you the order to use them and the deliverable each step
produces.

## Step 1 — surface the next frontier edge

`tools/python/frontier.py v2` ranks guest-i960 edges from probe
corpora, sweeps and traces. With fighter bases configured, every
ranked edge carries the exact fighter offsets it touches.

```sh
python tools/python/frontier.py \
  out/trace-both.jsonl out/trace-f0.jsonl \
  --fighter-base 0x510000 --fighter-base 0x520000 \
  --fighter-window 0x2000 --limit 6 --json \
  --output out/frontier-next.jsonl
```

Read the top edge where:
- `from_function` is the recovered function just exited (boundary
  hit);
- `fighter_access_count > 0` (the edge actually depends on a fighter
  field, so a recovery is meaningful);
- `boundary_distance` is small (close to the current native frontier);
- `unsupported_finals > 0` (corpus reproduced an unsupported exit, so
  the edge is the right target).

The JSON gives you `fighter_read_offsets` and `fighter_write_offsets`
in hex32 form. The first few ranks above the
`--exclude-recovered` filter are the next slice's entry edges.

## Step 2 — promote the candidate fighter fields

If the frontier edge touches an offset that is currently single-base
or absent from the provisional `field_xxxx` notation, run
`tools/python/infer_structs.py` on the same trace to upgrade it to
multi-corridor provenance:

```sh
python tools/python/infer_structs.py \
  out/trace-both.jsonl \
  --base fighter0=0x510000 --base fighter1=0x520000 \
  --window 0x2000 --limit 30 --json out/infer-structs-next.json
```

A field appearing under both `fighter0` and `fighter1` (i.e. `bases`
contains both names in the JSON) is promotable. The v0706 / v0704
dual-base provenance rule applies: same offset from both bases, same
access width, repeated access from the same guest IPs, consistent
read/write role across state transitions. Promotion only — no
semantic rename yet.

### Step 2a — automatic struct-block detection (v0729h)

When the per-offset roll-up shows many same-width fields at adjacent
offsets touched by the same guest IPs, run `frontier.py v2`'s
`contiguous_fighter_blocks` to aggregate them into one struct-like
block:

```python
from frontier import Frontier
f = Frontier()
f.set_fighter_bases([0x510000, 0x520000], window=0x2000)
for trace in ['out/trace-both.jsonl', 'out/trace-f0.jsonl']:
    f.ingest_trace(Path(trace), trace)
for b in f.contiguous_fighter_blocks(width=4, min_length=3):
    print(b['offset'], b['length'], b['byte_size'], b['ip_overlap'])
```

This is exactly how the 0x1680 block (length 120, 480B, 1.0 IP
overlap) was first surfaced — the union of two traces showed every
offset 0x1680..0x1860 touched by the same pair of guest IPs
(0x2399c + 0x23a38). The hand-enumeration step that previous
playbook runs needed is now a single API call.

## Step 3 — characterise the branch dependency

For the edge picked in step 1, the dependent branch IP is the
`from` of the edge. Run `tools/python/taint.py` against the trace
with the same fighter bases to produce the AGENTS.md next-work #3
contract output:

```sh
python tools/python/taint.py \
  --rom-dir roms/vf2 \
  --vf2i960 build/Debug/vf2i960.exe \
  --scenario out/state8-posbit6-v0727.json \
  --trace out/trace-bit14.jsonl \
  --until 0x00023a94 \
  --json out/taint-next.json
```

Expected output shape:

```text
branch 0x00023a94 depends on:
  fighter0 + 0x01a4 bit 6
  fighter0 + 0x05b6
```

If the output is empty or has the wrong shape, the contract is not
yet met — do not promote the inference to recovered semantics. Keep
the path fail-closed per AGENTS.md rule 2.

## Step 4 — gate every promotion with the test suite

Six test suites guard the factory:

| suite | coverage |
| --- | --- |
| `tools/python/test_frontier.py` | 18/18 — v2 factory contract |
| `tools/python/test_taint.py` | 7/7 — taint unit algorithm |
| `tools/python/test_taint_e2e.py` | 1/1 — taint on real corpus |
| `tools/python/test_infer_structs.py` | 4/4 — dual-base promotion |
| `tools/python/test_infer_rules.py` | 10/10 — conservative refusal contract |
| `tools/python/test_check_scenario.py` | 9/9 — scenario validation |
| `tools/python/test_sweep_state.py` | 9/9 — sweep driver helpers |
| `tools/python/test_explore_state.py` | 9/9 — coverage-guided explorer |
| `tools/python/test_minimize_case.py` | 7/7 — testcase minimizer |
| `tools/python/test_trace_case.py` | 4/4 — trace_case helpers |
| `tools/python/test_z3_branch.py` | 5/5 — Z3 branch helper (skips if z3 missing) |
| `tools/python/test_factory_chain.py` | 3/3 — Step 1 -> 2 -> 2a integration |

All twelve are wired into ctest:

```sh
ctest --test-dir build -C Debug -R vf2_python_factory --output-on-failure
```

Standalone run:

```sh
python tools/python/test_frontier.py \
&& python tools/python/test_taint.py \
&& python tools/python/test_taint_e2e.py \
&& python tools/python/test_infer_structs.py \
&& python tools/python/test_infer_rules.py \
&& python tools/python/test_check_scenario.py \
&& python tools/python/test_sweep_state.py \
&& python tools/python/test_explore_state.py \
&& python tools/python/test_minimize_case.py \
&& python tools/python/test_trace_case.py \
&& python tools/python/test_z3_branch.py \
&& python tools/python/test_factory_chain.py
```

Total: 86 Python test cases. Total ctest time: ~7 s.

When the frontier edge in step 1 is discovered inside a sweep
scenario (rather than a hand-rolled trace), `tools/python/infer_rules.py`
adds a fifth tool to the chain.  It infers a minimized DNF rule
from the sweep outcomes and the documented contract is conservative:
if the selected features do not fully determine the outcome, or the
truth table is incomplete, it refuses to produce a rule.  This is
exactly the discipline that retired the 15-entry triple/quad table in
v0728.  Do not weaken `infer_rules.py`'s refusal logic.

Before any sweep is launched, the scenario JSONL must pass
`tools/python/check_scenario.py` validation.  This is the upstream
gate the factory enforces: invalid scenarios never reach the
probe or the inference tools.

## Step 5 — write the per-slice note

Per AGENTS.md, every new recovery commits a compact evidence note
under `decomp/i960/notes/`. Use the existing
`playable_coin_assignment_natural_v0727.md`,
`fa_player_28184_head_siblings_v0707.md`,
`frontier_v2_player_corpus_v0729.md` as templates. Required sections:

- **Result** — slice scope and what was recovered.
- **Measured oracle behavior** — branch dependencies, address
  ranges, poststate rule.
- **Validation** — chain of differentials from a known-good
  snapshot; CTest suites green.
- **Still open** — sibling fail-closed pins, future slice edges.

## Step 6 — strict build and ROM-backed differential

```sh
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

Then the specific ROM-backed differential for the slice family
(e.g. `vf2i960 native-seventh-dispatch` once F5 lands, or the
`fa_player` live tests).

## When NOT to use this runbook

- The plan's Layer 3 work (TGP single-mesh packet decode, SCSP FM/DSP,
  attract mode phase ≥16, combat, CPU AI, ring-out, full round FSM).
  Those have no measurable fighter-window surface and need their own
  measurement probes before factory tooling helps.
- Any time the frontier edge's `from_function` is not recovered.
  Factory tooling will surface the edge but recovering it requires
  the surrounding corridor to be native first.