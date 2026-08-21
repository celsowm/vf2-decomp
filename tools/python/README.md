# Python recovery tools

These scripts sit above the validated C/i960 runtime. They do not replace the reference executor; they automate controlled experiments against it.

## Setup

```sh
python -m venv .venv
. .venv/bin/activate
pip install -r tools/python/requirements.txt
```

Only `sympy` is used by the initial rule-minimization path. The remaining packages are staged for the next exploration iterations and remain optional to the C build.

## `sweep_state.py`

A scenario is JSON so runs remain easy to diff and archive. Each dimension can mutate a register or a `u8`/`u16`/`u32` memory location. A dimension may enumerate literal values or generate every subset of a list of bit positions.

```sh
python tools/python/sweep_state.py \
  tools/python/scenarios/state8_example.json \
  --output out/state8.jsonl
```

Every line in the output contains the exact input tuple, the probe return code and the final structured probe record.

The checked-in scenario is intentionally an address placeholder/template: replace its fighter address with the measured address from the snapshot being investigated rather than treating the example as recovery evidence.

## `infer_rules.py`

```sh
python tools/python/infer_rules.py out/state8.jsonl
```

The default report groups cases by observable final signature (status, halt reason, IP, procedure counters and requested `u32` reads).

For dimensions encoded explicitly as `0/1`, SymPy can minimize a binary outcome truth table:

```sh
python tools/python/infer_rules.py out/example.jsonl --boolean bit1 bit2 bit4 bit6
```

A minimized expression is only a hypothesis. It must be implemented in recovered C and proved with the normal ROM-backed differential suite before becoming an accepted native path.
