# Python recovery tools

These scripts sit above the validated C/i960 runtime. They do not replace the reference executor; they automate controlled experiments against it.

## Setup

```sh
python -m venv .venv
. .venv/bin/activate
pip install -r tools/python/requirements.txt
```

Only `sympy` is required by the initial rule-minimization path. The remaining packages are analysis-only and do not affect the C build.

## `sweep_state.py`

A scenario is JSON so runs remain easy to diff and archive. Each dimension can mutate a register or a `u8`/`u16`/`u32` memory location. A dimension may enumerate literal values or generate every subset of a list of bit positions.

Validate a scenario before a large run:

```sh
python tools/python/check_scenario.py tools/python/scenarios/minimal.json
```

Then sweep it:

```sh
python tools/python/sweep_state.py \
  tools/python/scenarios/minimal.json \
  --output out/sweep.jsonl
```

For a bit-mask dimension, `base` preserves required state bits while the selected bits vary. Signed values such as `-1` are accepted for 32-bit register/`u32` mutations and are passed as their two's-complement bit pattern.

## `infer_rules.py`

The default report groups cases by observable final signature. Explicit binary dimensions and selected bits from integer masks can be minimized with SymPy when the measured truth table is complete and the selected features uniquely determine the result.

```sh
python tools/python/infer_rules.py out/state8.jsonl \
  --bitfield fighter0_flags:1,2,4,6,8 \
  --bitfield fighter1_flags:1,2,4,6,8
```

The minimizer refuses to produce a rule if the selected features do not uniquely determine the outcome or if the measured truth table is incomplete. A minimized expression is still only a hypothesis and must be proved by the ROM-backed differential suite.

## `explore_state.py`

This is the coverage-guided layer. It mutates previously useful inputs and treats exact guest i960 `ip_before -> ip_after` transitions as coverage. A candidate enters the corpus only when it adds an unseen guest edge.

```sh
python tools/python/explore_state.py \
  path/to/measured-scenario.json \
  --corpus out/fa_player-corpus \
  --iterations 10000 \
  --seed 1 \
  --max-mutations 3
```

The corpus is resumable and retains the exact input, new edges and resulting `.vf2snap` for every accepted case.

## `minimize_case.py`

The minimizer starts from an accepted coverage case and repeatedly restores dimensions toward the scenario baseline. For bit-mask dimensions it also clears individual swept bits. Every reduction is accepted only if a fresh reference execution still reaches the requested guest edge.

```sh
python tools/python/minimize_case.py \
  path/to/measured-scenario.json \
  out/fa_player-corpus/case-00017.json \
  --edge 0x00018bd4:0x00018c30 \
  --output out/minimized-18bd4-18c30.json
```

## `trace_case.py`

Run one exact scenario input with guest-instruction and Model 2A bus tracing. Output is streamed directly to JSONL, so large traces do not accumulate in Python memory.

```sh
python tools/python/trace_case.py \
  out/state8-positive.json \
  --output out/state8-case.jsonl \
  --set fighter0_flags=0x40 \
  --set fighter1_flags=0x0 \
  --set countdown=0 \
  --set threshold=0
```

`vf2probe --memory-trace` emits successful bus accesses with an absolute instruction `step`; the normal step event immediately following the guest instruction carries its exact `ip_before`. Analysis tools correlate the two by step number instead of guessing the instruction address from the machine's already-advanced IP.

The observer is enabled only for the reference run itself. Scenario mutations, final reads and output-snapshot capture are deliberately excluded from the memory trace.

## `infer_structs.py`

Correlate a memory trace with measured object bases and rank repeated `base + offset` fields. A generated game-info scenario already records the live `fighter0` and `fighter1` bases in its metadata.

```sh
python tools/python/infer_structs.py \
  out/state8-case.jsonl \
  --scenario out/state8-positive.json \
  --json out/state8-fields.json
```

The report groups each candidate offset by fighter base, read/write count, access width and the guest instruction addresses touching it. Offsets observed relative to both fighters rank ahead of one-off accesses, which makes repeated object-layout fields much easier to identify. Aggregation is streaming; memory use depends mainly on distinct offsets/IPs rather than trace length.

Additional object bases can be supplied explicitly:

```sh
python tools/python/infer_structs.py trace.jsonl \
  --base object0=0x00512000 \
  --base object1=0x00513000
```

## `frontier.py`

Rank the recovery frontier from any mix of corpus manifests, sweeps and traces. The unit is a guest address/edge; ranking uses measured witnesses, snapshot reproducibility, unsupported finals and recovered-range attribution from `decomp/i960/functions.csv`.

```sh
python tools/python/frontier.py \
  out/state8-corpus/manifest.jsonl \
  out/state8-sweep.jsonl \
  out/state8-case.jsonl \
  --limit 25 --exclude-recovered

# machine-readable variant
python tools/python/frontier.py out/state8-corpus/manifest.jsonl --json
```

`--exclude-recovered` leaves the actual working frontier by hiding edges whose endpoints are both inside recovered ranges. The report ranks candidates; it never turns them into recoveries without differential proof.

Unit tests:

```sh
python tools/python/test_frontier.py
```

## `extract_original_symbols.py`

Recover the original Sega i960 symbol table from a Model 2 port DLL. The table is located by shape rather than by address, so the script is not keyed to one build; a DLL with no such run is reported instead of guessed at. The DLL is not part of this repository.

```sh
# report every candidate run and emit nothing
python tools/python/extract_original_symbols.py --dll <port.dll> --tables

# write the analyzer overlay
python tools/python/extract_original_symbols.py --dll <port.dll>   --out decomp/i960/original_symbols.csv

# verify the committed overlay against a DLL
python tools/python/extract_original_symbols.py --dll <port.dll>   --check decomp/i960/original_symbols.csv
```

`--check` exits non-zero on any disagreement and is how the committed file stays honest without the DLL being in the tree. This script has no dependencies. See `docs/ORIGINAL_SYMBOLS.md`.

## Next layer

The next high-value steps beyond the shipped frontier ranker are targeted dynamic taint and Z3 bit-vector constraints for branches whose measured inputs still resist a compact semantic rule.
