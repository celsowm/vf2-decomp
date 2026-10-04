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

Three test suites guard the factory:

| suite | coverage |
| --- | --- |
| `tools/python/test_frontier.py` | 14/14 — v2 factory contract |
| `tools/python/test_taint.py` | 7/7 — taint unit algorithm |
| `tools/python/test_taint_e2e.py` | 1/1 — taint on real corpus |
| `tools/python/test_infer_structs.py` | 4/4 — dual-base promotion |

Run all four before any promotion:

```sh
python tools/python/test_frontier.py \
&& python tools/python/test_taint.py \
&& python tools/python/test_taint_e2e.py \
&& python tools/python/test_infer_structs.py
```

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