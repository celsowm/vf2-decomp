# `fa_game_info` `0x18644` differential tools

Reusable tooling for the ROM-backed recovery workflow. These scripts replace the temporary inline Python/heredoc staging code used during early exploration.

## One state pair

```bash
python3 decomp/i960/tools/validate_game_info_pair.py \
  ./build/vf2i960 /path/to/vf2-roms 0x102 0x112 --reverse
```

The validator executes the strong chain for every countdown/mode-bit-6 case:

`native child #1 -> caller ROM -> native child #2 -> ROM tail -> 0x10dcc`

It requires both `vf2i960 compare-snapshots` equality and explicit equality of serialized instruction/call/return/interrupt counters.

## Per-call accounting diagnosis

```bash
python3 decomp/i960/tools/diagnose_game_info_pair.py \
  ./build/vf2i960 /path/to/vf2-roms 0x100 0x102 --reverse
```

This runs the first and second `0x18644` calls independently against the same pure-ROM reference and prints architectural equality plus counter deltas for each call site. Use it before changing instruction accounting so a full-chain delta is not incorrectly treated as one global correction.

## Full tracked matrix

```bash
python3 decomp/i960/tools/validate_game_info_matrix.py \
  ./build/vf2i960 /path/to/vf2-roms --workers 2
```

By default this checks rows marked `recovered_exact=yes` in `decomp/i960/game_info_18644_state_vectors.csv`. Add `--all` to attempt all 64 ordered pairs. `--workers` controls ordered-pair parallelism; keep it conservative because each validator launches ptrace and ROM-backed subprocesses.

## Fixture generation

```bash
python3 decomp/i960/tools/make_game_info_fixture.py \
  ./build/vf2i960 /path/to/vf2-roms /tmp/case.vf2snap \
  --state0 0x102 --state1 0x112 --countdown 1 --mode-bit6 0
```

Use `--base existing-164ac.vf2snap` to avoid rebuilding the calibrated caller boundary.

## State-4 flag matrix

Once a ROM directory is available, validate the full four-bit state-4 matrix
with the chained native/ROM harness:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --all --threshold 0
```

The mask bits map to state flags 6, 14, 15 and 16. Each mask is tested with
fighter 0 only, fighter 1 only and both fighters, for both countdown values
and both mode-bit-6 values. The current ROM-backed result is 192/192 exact.
Negative-threshold coverage can be run separately, for example with
`--threshold -1`.

The same harness accepts the fighter state byte explicitly. The complete
state-8 matrix for flag bits 1, 2 and 4 is:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --all --threshold 0
```

Use `--state 8 --include-bit8 --all` to include flag bit 8 as the fourth
dimension. That runs 192 ROM-backed cases across the three fighter
distributions, both countdown values and both mode-bit-6 values.
Use `--extra-bit 21` (repeatable) to add other `+0x1a4` flag bits as measured
dimensions; for example, `--state 8 --extra-bit 21 --mask 8` probes the
isolated bit-21 case in all 12 countdown/mode/distribution combinations.
For the positive no-bit-8 bit-6 submatrix, use `--state 8 --extra-bit 6
--all --threshold 0`; all 192 fixtures (bits 1/2/4/6) are exact.
The aggregate high-bit fixture can be selected with four repeated dimensions;
masks `248..255` cover bit 8 plus all four high bits with every subset of
flag bits 1/2/4, and each mask is exact across all 12 combinations.

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 21 --extra-bit 26 --extra-bit 29 --extra-bit 30 \
  --extra-bit 31 --mask 511 --threshold 0
```
With `--threshold -1`, the complete state-4 matrix (`--state 4 --all`) is
native and exact across all 192 fixtures for flag bits 6, 14, 15 and 16.
The state-8 matrix with `--include-bit8` and repeated `--extra-bit` options
for bits 21, 26, 29, 30 and 31 is native and exact across all 3,072 fixtures
(256 masks × 12 distributions) at threshold `-1`, and the same 3,072
fixtures are exact at threshold `0`. This covers every
isolated, asymmetric and bilateral distribution, both countdown values, both
mode-bit-6 values and every combination of the nine admitted flags. Any
other state-8 flag bit (apart from bit 6 on the negative threshold) retains
the complete ROM interpreter fallback at the dispatcher boundary. State-8 bit 6 is additionally admitted on the negative
threshold path: its full ten-bit matrix is exact across 12,288 fixtures
(1,024 masks × 12 distributions). On the positive threshold, the child is
also exact for the complete no-bit-8 bits-1/2/4/6 submatrix (192 fixtures),
plus the eight tested masks containing bit 6, bit 8 and all five high bits,
covering every subset of low bits 1/2/4 (96 more fixtures). Positive bit-6
compositions outside the measured slices remain unproven or explicit
boundaries. The measured
positive slices are exact across all 12 distributions/countdown/mode cases:
the measured `0x140` (bit 8 + bit 6), `0x142` (bit 8 + bits 1 + 6), `0x144`
(bit 8 + bits 2 + 6), `0x146` (bit 8 + bits 1 + 2 + 6), `0x150` (bit 8 +
bits 4 + 6), and `0x152` (bit 8 + bits 1 + 4 + 6) fighter-state
compositions. The `0x146` focused matrix can be reproduced with:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --mask 27 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x144` focused matrix is:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --mask 26 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The measured positive `0x150` (bit 8 + bits 4 + 6) composition is also exact
across all 12 cases. The `0x154` and `0x156` compositions are exact as well.
The positive `0x4140` (bit 8 + bits 6 + 14) high-bit composition is exact
across all 12 cases. The adjacent positive `0x8140` (bit 8 + bits 6 + 15)
composition is exact across all 12 cases; other positive high-bit compositions
remain measured frontier work. The adjacent positive `0x10140` (bit 8 + bits
6 + 16) composition is also exact across all 12 cases.

The `0x150` slice can be reproduced with:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --mask 28 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x8140` focused matrix uses `--extra-bit 15 --mask 56`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 15 --mask 56 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x154` focused matrix uses `--mask 30`; the `0x156` matrix uses
`--mask 31`.

The `0x4140` focused matrix uses `--extra-bit 14 --mask 56`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --mask 56 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x10140` focused matrix uses `--extra-bit 16 --mask 56`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 16 --mask 56 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x0c140` focused matrix uses `--extra-bit 14 --extra-bit 15
--mask 120`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 15 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

The `0x14140` focused matrix uses `--extra-bit 14 --extra-bit 16
--mask 120`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 16 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

The `0x18140` focused matrix uses `--extra-bit 15 --extra-bit 16
--mask 120`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 15 --extra-bit 16 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

The `0x1c140` focused matrix uses `--extra-bit 14 --extra-bit 15
--extra-bit 16 --mask 248`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 15 --extra-bit 16 \
  --mask 248 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x204140` focused matrix uses `--extra-bit 14 --extra-bit 21
--mask 120`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 21 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

The `0x208140` focused matrix uses `--extra-bit 15 --extra-bit 21
--mask 120`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 15 --extra-bit 21 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

The `0x210140` focused matrix uses `--extra-bit 16 --extra-bit 21
--mask 120`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 16 --extra-bit 21 --mask 120 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

The `0x20c140` focused matrix uses `--extra-bit 14 --extra-bit 15
--extra-bit 21 --mask 248`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 15 --extra-bit 21 \
  --mask 248 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x214140` focused matrix uses `--extra-bit 14 --extra-bit 16
--extra-bit 21 --mask 248`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 16 --extra-bit 21 \
  --mask 248 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x218140` focused matrix uses `--extra-bit 15 --extra-bit 16
--extra-bit 21 --mask 248`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 15 --extra-bit 16 --extra-bit 21 \
  --mask 248 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The `0x21c140` focused matrix uses `--extra-bit 14 --extra-bit 15
--extra-bit 16 --extra-bit 21 --mask 504`:

```bash
python3 decomp/i960/tools/validate_game_info_state4.py \
  ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
  --extra-bit 6 --extra-bit 14 --extra-bit 15 --extra-bit 16 \
  --extra-bit 21 --mask 504 --threshold 0 \
  --base out/state8-positive.boundary.vf2snap
```

The measured positive bit-6/high-bit masks `0x200140`, `0x4000140`,
`0x20000140`, `0x40000140` and `0x80000140` are each exact across 12 cases.
Run the isolated family with:

```bash
for bit in 21 26 29 30 31; do
  python3 decomp/i960/tools/validate_game_info_state4.py \
    ./build/vf2i960 /path/to/vf2-roms --state 8 --include-bit8 \
    --extra-bit 6 --extra-bit "$bit" --mask 56 --threshold 0 \
    --base out/state8-positive.boundary.vf2snap
done
```

The measured aggregate masks `0x24000140` and `0xe4200140` are exact as
well. Their focused matrices use `--mask 120` with extra bits 6, 26 and 29,
and `--mask 1016` with extra bits 6, 21, 26, 29, 30 and 31, respectively.

## Native child runner

```bash
python3 decomp/i960/tools/run_game_info_child.py \
  ./build/vf2i960 /path/to/vf2-roms input.vf2snap output.vf2snap \
  --return-address 0x164b0
```

The Linux/x86-64 ptrace harness resolves executable symbols through `nm`; it does not contain build-specific function offsets. A snapshot at `0x164ac`/`0x164c0` is automatically converted to the corresponding native return-IP form before invoking the recovered child.

## Counter comparison

```bash
python3 decomp/i960/tools/compare_snapshot_counters.py \
  expected.vf2snap actual.vf2snap --signature
```

This exists because `compare-snapshots` is primarily a CPU/mutable-memory comparison and must not be used as the sole proof for serialized meta counters.

## Positive state-8 bit-6 sweep

`validate_game_info_positive_bit6_sweep.py` wraps `validate_game_info_state4.py`
to enumerate every flag mask that has bit 6 set inside the measured positive
composition space (low bits 1/2/4, bit 8, and high bits 21/26/29/30/31) and emit
one JSONL record per mask. It is resumable and parallelizable so a full
exhaustive proof can be split across shards.

```bash
python3 decomp/i960/tools/validate_game_info_positive_bit6_sweep.py \
  ./build/vf2i960 /path/to/vf2-roms out/posbit6-shard0.jsonl \
  --base out/state8-positive.boundary.vf2snap \
  --shard 0 --shards 8

# or a curated mask list (hex accepted)
python3 decomp/i960/tools/validate_game_info_positive_bit6_sweep.py \
  ./build/vf2i960 /path/to/vf2-roms out/posbit6-curated.jsonl \
  --base out/state8-positive.boundary.vf2snap \
  --masks 16,23,24,31 --shard 0 --shards 1
```

The tool never decides behaviour; it only runs the ROM-backed differential
validator and aggregates the result. Unit test (ROM-independent):

```bash
python3 decomp/i960/tools/test_validate_game_info_positive_bit6_sweep.py
```

