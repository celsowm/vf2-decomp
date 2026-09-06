# fa_game_info high-21 matrix recovery — 2026-09-06

ROM-backed full-dispatch validation is run from the calibrated `0x0001645c`
`fa_game_info` task entry through the scheduler return at `0x00010dcc`.

Each admitted mask is checked over the complete measured matrix:

- fighter-0-only, fighter-1-only, and bilateral distributions;
- countdown byte 0 and 1;
- mode bit 6 clear and set;
- shared threshold 0, 1, and 2.

That is 36 ROM/native comparisons per mask. Validation compares the complete
snapshot plus instruction, call, return, interrupt-entry, and interrupt-return
counters. No ROM, snapshot, or trace artifact is committed.

## Exact admitted masks

The recovered runtime is exact for all 684 measured cases across 19 explicitly
admitted masks:

| Combined state-8 mask | Result |
| --- | ---: |
| `0x00204000` | 36/36 exact |
| `0x00208000` | 36/36 exact |
| `0x00210000` | 36/36 exact |
| `0x00214000` | 36/36 exact |
| `0x00218000` | 36/36 exact |
| `0x0021c000` | 36/36 exact |
| `0x04214000` | 36/36 exact |
| `0x06214000` | 36/36 exact |
| `0x08214000` | 36/36 exact |
| `0x0a214000` | 36/36 exact |
| `0x0c214000` | 36/36 exact |
| `0x10214000` | 36/36 exact |
| `0x12214000` | 36/36 exact |
| `0x14214000` | 36/36 exact |
| `0x16214000` | 36/36 exact |
| `0x18214000` | 36/36 exact |
| `0x1c214000` | 36/36 exact |
| `0x24214000` | 36/36 exact |
| `0x84214000` | 36/36 exact |

The confirming build is commit `80345fd572b295b82fb3133fdb200c3353541971`.
Its CI gate completed successfully under GCC, Clang, ASan/UBSan, and Python
tooling checks. The ROM-backed matrix was run against the `vf2i960-linux`
artifact produced by that exact commit.

## Recovered poststate classes

### `0x00204000`

Instruction and RAM state were already exact. The missing architectural
condition state is fighter-0-only/countdown 0 = `EQUAL`, and every countdown-1
case = `LESS`.

### `0x00208000`

Dispatcher accounting is distribution/countdown dependent:

- countdown 0: subtract one native instruction for every distribution;
- countdown 1 unilateral: add three native instructions;
- countdown 1 bilateral: add seven native instructions.

Condition state is fighter-0-only/countdown 0 = `EQUAL`, and every countdown-1
case = `LESS`.

### `0x00210000` and `0x00218000`

For each fighter carrying the mask, the measured child-side poststate is:

```text
fighter + 0x1a4 : state_flags |= 0x00000800
fighter + 0xb24 : field &= ~0x00008000
```

Instruction counts already match. Condition state follows the same measured
`EQUAL` / `LESS` pattern.

### `0x00214000`

RAM already matches. The instruction correction table is independent of
threshold:

| Distribution | cd=0 mode6=0 | cd=0 mode6=1 | cd=1 mode6=0 | cd=1 mode6=1 |
| --- | ---: | ---: | ---: | ---: |
| fighter 0 only | +3 | +2 | +2 | +2 |
| fighter 1 only | +3 | +8 | +7 | +8 |
| bilateral | +4 | +8 | +7 | +8 |

Condition state follows the same measured `EQUAL` / `LESS` pattern.

### Condition-only family

These masks already had exact RAM and instruction accounting; only architectural
condition state was missing:

- `0x0021c000`;
- `0x04214000`;
- `0x24214000`;
- `0x84214000`.

The correction is fighter-0-only/countdown 0 = `EQUAL`, and every countdown-1
case = `LESS`.

### Measured +2/+3 accounting family

These masks have exact RAM and share the same measured instruction deficit:

- `0x06214000`;
- `0x08214000`;
- `0x0a214000`;
- `0x0c214000`;
- `0x10214000`;
- `0x12214000`;
- `0x14214000`;
- `0x16214000`;
- `0x18214000`;
- `0x1c214000`.

Unilateral cases require `+2` native instructions and bilateral cases require
`+3`. Condition state follows the same measured `EQUAL` / `LESS` pattern. The
runtime enumerates every admitted mask explicitly; this is not a wildcard high-
bit rule.

## Nearby base-exact masks

Several related masks require no interposer correction and remain on the base
recovered path. Confirmed examples include:

- `0x20214000`: 36/36 exact;
- `0x40214000`: 36/36 exact;
- `0x44214000`: 36/36 exact;
- `0x80214000`: 36/36 exact.

## Next frontier

Continue the same controlled high-bit sweep from the `0x00214000` family,
prioritizing masks not yet admitted and classifying them into full-state exact,
condition-only, measured accounting, or genuine RAM-semantic divergences before
changing recovery code.
