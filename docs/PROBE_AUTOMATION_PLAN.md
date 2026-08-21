# Probe automation

This layer accelerates evidence gathering above the existing i960 reference executor and native differential runtime. It does not replace either one.

## Goals

- make controlled state mutation reproducible from snapshots;
- emit structured traces suitable for automated analysis;
- sweep flag/state matrices without one-off validators;
- infer compact rules from measured behavior;
- discover new guest branches with coverage-guided state mutation;
- minimize discovered states back to the smallest measured trigger; and
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

Add `--trace` to emit one JSON record per guest instruction. The final JSON record contains the halt state, instruction/procedure counters and requested memory observations. `--output-snapshot` captures the resulting machine/CPU state for later recovery work.

The probe deliberately uses the existing Model 2A memory model and i960 executor. Unknown behavior remains unknown; the tool does not invent native semantics.

## State sweeps

`tools/python/sweep_state.py` consumes a declarative JSON scenario. Each dimension can mutate a register or a `u8`, `u16` or `u32` memory location. Dimensions either enumerate literal values or generate every subset of selected bit positions. A bit dimension may define `base` to preserve required state bits while sweeping the selected bits.

```sh
python tools/python/check_scenario.py tools/python/scenarios/minimal.json
python tools/python/sweep_state.py \
  tools/python/scenarios/minimal.json \
  --output out/sweep.jsonl
```

Every output line records the exact inputs and structured `vf2probe` result. Snapshot-specific addresses must come from measured evidence; checked-in scenarios are templates rather than recovery claims.

## Rule inference

`tools/python/infer_rules.py` groups sweep cases by stable observable outcome. Explicit binary dimensions and selected bits from integer mask dimensions can be minimized with SymPy when the measured truth table is complete and the selected features uniquely determine the result.

```sh
python tools/python/infer_rules.py out/state8.jsonl \
  --bitfield fighter_flags:1,2,4,6,8
```

The tool refuses to emit a minimized rule if the evidence is incomplete or an omitted variable changes the result. A minimized expression is only a hypothesis. It must still be translated to recovered C and validated against the ROM-backed differential suite across the measured matrix.

## Coverage-guided exploration

`tools/python/explore_state.py` treats exact guest i960 transitions from `vf2probe --trace` as coverage. Starting from the scenario baseline and previously useful inputs, it mutates a small number of dimensions at a time. A state enters the corpus only when it reaches at least one previously unseen `ip_before -> ip_after` edge.

```sh
python tools/python/explore_state.py \
  path/to/measured-scenario.json \
  --corpus out/fa_player-corpus \
  --iterations 10000 \
  --seed 1
```

Accepted cases retain both a JSON evidence record and the resulting `.vf2snap`. The manifest is resumable: subsequent runs reconstruct prior guest coverage and continue the existing corpus rather than starting over.

## Testcase minimization

`tools/python/minimize_case.py` takes one accepted corpus record plus a target edge and repeatedly restores dimensions toward their baseline. For swept bit masks it additionally tries clearing individual optional bits. Every reduction is accepted only if a fresh reference execution still reaches the target edge.

```sh
python tools/python/minimize_case.py \
  path/to/measured-scenario.json \
  out/fa_player-corpus/case-00017.json \
  --edge 0x00018bd4:0x00018c30
```

This turns a noisy coverage discovery into a compact, reproducible state-transition hypothesis before manual semantic recovery begins.

## Optional analysis dependencies

The C build remains dependency-light. Python analysis dependencies live under `tools/python/requirements.txt`:

- `sympy` — boolean minimization;
- `z3-solver` — future bit-vector/path-constraint inference;
- `hypothesis` — future generated-state/property shrinking;
- `duckdb` and `pyarrow` — future large trace corpora; and
- `networkx` — future CFG/frontier ranking.

## Next iterations

1. Model 2A memory-access observers;
2. repeated base+offset analysis to infer candidate fighter/object layouts;
3. frontier ranking from corpus coverage and native/unsupported boundaries; and
4. targeted dynamic taint / Z3 constraints only where measured behavior justifies them.

The ROM-backed executor remains the oracle throughout this process.
