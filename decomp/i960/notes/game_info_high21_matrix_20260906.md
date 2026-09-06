# fa_game_info high-21 matrix recovery — 2026-09-06

ROM-backed full-dispatch validation was run from the calibrated `0x0001645c`
`fa_game_info` task entry through the scheduler return at `0x00010dcc`.

Each admitted mask was checked over the complete measured matrix:

- fighter-0-only, fighter-1-only, and bilateral distributions;
- countdown byte 0 and 1;
- mode bit 6 clear and set;
- shared threshold 0, 1, and 2.

That is 36 ROM/native comparisons per mask. Validation compares the complete
snapshot plus instruction, call, return, interrupt-entry, and interrupt-return
counters. No ROM, snapshot, or trace artifact is committed.

## Exact masks

The current recovered runtime is exact for all 180 measured cases:

| Combined state-8 mask | Result |
| --- | ---: |
| `0x00204000` | 36/36 exact |
| `0x00208000` | 36/36 exact |
| `0x00210000` | 36/36 exact |
| `0x00214000` | 36/36 exact |
| `0x00218000` | 36/36 exact |

The confirming build is commit `6d03c905f2449d7f0bede46674b0983ea97bbfc1`.
Its normal CI gate completed successfully under GCC, Clang, ASan/UBSan, and the
Python tooling checks.

## Recovered poststate details

### `0x00204000`

Instruction and RAM state were already exact. The missing architectural
condition state is:

- fighter-0-only with countdown 0: `EQUAL`;
- any distribution with countdown 1: `LESS`.

### `0x00208000`

The measured dispatcher accounting is distribution/countdown dependent:

- countdown 0: subtract one native instruction for every distribution;
- countdown 1 unilateral: add three native instructions;
- countdown 1 bilateral: add seven native instructions.

Condition state is `EQUAL` for fighter-0-only/countdown 0 and `LESS` for every
countdown-1 case.

### `0x00210000` and `0x00218000`

The child-side state transition has two independent measured effects for each
fighter carrying the mask:

```text
fighter + 0x1a4 : state_flags |= 0x00000800
fighter + 0xb24 : field &= ~0x00008000
```

The second operation removes a stale legacy write produced by the previous
recovered child path. Instruction counts already match the ROM. Condition state
uses the same measured pattern: fighter-0-only/countdown 0 is `EQUAL`; every
countdown-1 case is `LESS`.

### `0x00214000`

RAM already matches. Only dispatcher accounting and condition state required
recovery. The instruction correction table is independent of threshold:

| Distribution | cd=0 mode6=0 | cd=0 mode6=1 | cd=1 mode6=0 | cd=1 mode6=1 |
| --- | ---: | ---: | ---: | ---: |
| fighter 0 only | +3 | +2 | +2 | +2 |
| fighter 1 only | +3 | +8 | +7 | +8 |
| bilateral | +4 | +8 | +7 | +8 |

Condition state is `EQUAL` for fighter-0-only/countdown 0 and `LESS` for every
countdown-1 case.

## Next frontier

Nearby probes on the same confirming artifact show:

- `0x20214000`: 36/36 exact already;
- `0x0021c000`: 12/36 exact;
- `0x04214000`: 12/36 exact.

Those partially exact masks are better next targets than extending baseline
frame endurance, because the baseline native corridor has already remained exact
for thousands of repeated dispatches.
