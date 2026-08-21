# Probe automation

This layer accelerates evidence gathering above the existing i960 reference executor and native differential runtime. It does not replace either one.

## Goals

- make controlled state mutation reproducible from snapshots;
- emit structured traces suitable for automated analysis;
- sweep flag/state matrices without one-off validators;
- infer compact rules from measured behavior; and
- preserve the existing fail-closed differential-validation contract.

## `vf2probe`

`vf2probe` restores a `.vf2snap`, applies repeated register or memory mutations and runs the reference i960 executor until a selected address or instruction limit.

Examples:

```sh
build/vf2probe \
  --rom-dir roms/vf2 \
  --snapshot checkpoint.vf2snap \
  --set-reg g0=2 \
  --set-u32 0x00501234=0x100 \
  --until 0x000164c4 \
  --read-u32 0x00501234
```

Add `--trace` to emit one JSON record per guest instruction. The final JSON record contains the halt state, instruction/procedure counters and requested memory observations.

The probe deliberately uses the existing Model 2A memory model and i960 executor. Unknown behavior remains unknown; the tool does not invent native semantics.

## State sweeps

`tools/python/sweep_state.py` consumes a declarative JSON scenario. Each dimension can mutate a register or a `u8`, `u16` or `u32` memory location. Dimensions either enumerate literal values or generate every subset of selected bit positions.

```sh
python tools/python/check_scenario.py tools/python/scenarios/minimal.json
python tools/python/sweep_state.py \
  tools/python/scenarios/minimal.json \
  --output out/sweep.jsonl
```

Every output line records the exact inputs and structured `vf2probe` result. Snapshot-specific addresses must come from measured evidence; checked-in scenarios are templates rather than recovery claims.

## Rule inference

`tools/python/infer_rules.py` groups sweep cases by stable observable outcome. For explicitly binary dimensions, SymPy can minimize a measured truth table:

```sh
python tools/python/infer_rules.py out/sweep.jsonl --boolean bit1 bit2 bit4 bit6
```

A minimized expression is only a hypothesis. It must still be translated to recovered C and validated against the ROM-backed differential suite across the measured matrix.

## Optional analysis dependencies

The C build remains dependency-light. Python analysis dependencies live under `tools/python/requirements.txt`:

- `sympy` — boolean minimization;
- `z3-solver` — future bit-vector/path-constraint inference;
- `hypothesis` — future generated-state shrinking;
- `duckdb` and `pyarrow` — future large trace corpora; and
- `networkx` — future CFG/frontier ranking.

## Next iterations

1. guest-edge coverage and corpus retention (`vf2explore`);
2. Model 2A memory-access observers;
3. testcase minimization for newly reached branches;
4. repeated base+offset analysis to infer candidate fighter/object layouts; and
5. targeted dynamic taint / Z3 constraints only where measured behavior justifies them.

The ROM-backed executor remains the oracle throughout this process.
