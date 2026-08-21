# Python recovery tools

These scripts sit above the validated C/i960 runtime. They do not replace the reference executor; they automate controlled experiments against it.

## Setup

```sh
python -m venv .venv
. .venv/bin/activate
pip install -r tools/python/requirements.txt
```

Only `sympy` is required by the initial rule-minimization path. The remaining packages are staged for the next exploration iterations and remain optional to the C build.

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

For a bit-mask dimension, `base` preserves required state bits while the selected bits vary. For example, state 8 plus every subset of bits 1/2/4/6 is represented as:

```json
{
  "name": "fighter_flags",
  "kind": "u32",
  "address": "0x00501234",
  "base": "0x100",
  "bits": [1, 2, 4, 6]
}
```

The address above is illustrative only. Snapshot-specific addresses must come from measured recovery evidence.

Every JSONL line contains the exact input tuple, probe return code and final structured probe record. Signed values such as `-1` are accepted for 32-bit register/`u32` mutations and are passed as their two's-complement bit pattern.

## `infer_rules.py`

The default report groups cases by observable final signature: status, halt reason, IP, instruction/procedure counters and requested `u32` reads.

```sh
python tools/python/infer_rules.py out/sweep.jsonl
```

For dimensions explicitly encoded as `0/1`, SymPy can minimize a complete measured truth table:

```sh
python tools/python/infer_rules.py out/example.jsonl --boolean countdown mode
```

Integer mask dimensions can be expanded directly into boolean features:

```sh
python tools/python/infer_rules.py out/state8.jsonl \
  --bitfield fighter_flags:1,2,4,6,8
```

The minimizer refuses to produce a rule if the selected features do not uniquely determine the observed outcome or if the measured truth table is incomplete. A minimized expression is still only a hypothesis: implement it in recovered C and prove it with the normal ROM-backed differential suite before accepting the path.

## Next layer

The next automation step is guest-edge coverage/corpus retention (`vf2explore`), followed by memory-access tracing and testcase minimization. `z3-solver`, `hypothesis`, `duckdb`, `pyarrow` and `networkx` are already isolated as analysis-only dependencies for those iterations.
