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

## `explore_state.py`

This is the coverage-guided layer. It uses the same declarative scenario but chooses mutations from previously useful inputs instead of enumerating the full Cartesian product. `vf2probe --trace` provides exact guest i960 edges (`ip_before -> ip_after`). A candidate enters the corpus only when it adds at least one edge that has never been seen before.

```sh
python tools/python/explore_state.py \
  path/to/measured-scenario.json \
  --corpus out/fa_player-corpus \
  --iterations 10000 \
  --seed 1 \
  --max-mutations 3
```

Accepted cases produce:

```text
out/fa_player-corpus/
  manifest.jsonl
  case-00000.json
  case-00000.vf2snap
  case-00001.json
  case-00001.vf2snap
  ...
```

The corpus is resumable. Running the command again reconstructs previously discovered edges from `manifest.jsonl`, retains existing input seeds and continues case numbering. The random seed makes mutation choice reproducible for a given corpus state.

The saved `.vf2snap` is the post-execution checkpoint for the accepted input. The accompanying JSON records the exact starting mutations and the new guest edges that justified retaining it.

## `minimize_case.py`

A coverage discovery often contains more mutations than the branch actually needs. The minimizer starts from an accepted corpus JSON record and repeatedly restores dimensions toward the scenario baseline. For bit-mask dimensions it then attempts to clear individual swept bits while re-running the reference executor each time.

```sh
python tools/python/minimize_case.py \
  path/to/measured-scenario.json \
  out/fa_player-corpus/case-00017.json \
  --edge 0x00018bd4:0x00018c30 \
  --output out/minimized-18bd4-18c30.json
```

The result is a smaller measured input that still reaches the requested guest edge. It is evidence for analysis, not automatically accepted recovered semantics.

## Next layer

The next high-value addition is memory-access tracing around the Model 2A bus so reached branches can be correlated with repeated `base + offset` accesses. That will feed candidate fighter/object structure inference. `z3-solver`, `hypothesis`, `duckdb`, `pyarrow` and `networkx` remain isolated as analysis-only dependencies for those iterations.
