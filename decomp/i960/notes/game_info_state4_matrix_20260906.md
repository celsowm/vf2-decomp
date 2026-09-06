# `fa_game_info` state-4 measured matrix — 2026-09-06

This note records the completed clean-room differential recovery of the known state-4 fighter-flag matrix at `fa_game_info` (`0x0001645c`, child `0x00018644`).

## Validation contract

The final functional build is:

- `a6b443a39582cc3bbde3dd1de5fe74d0367a0e30` — `Complete measured state4 game-info matrix`

GitHub CI passed on that exact commit with:

- GCC release;
- Clang release;
- Clang ASan + UBSan;
- Python tooling/frontier regressions.

The ROM-backed differential matrix covers, for each admitted mask:

- fighter0-only, fighter1-only, and bilateral distribution;
- countdown `0/1`;
- mode bit 6 `0/1`;
- threshold `0/1/2`.

The strong contract requires exact final RAM/device snapshot, architectural CPU state including compare state and local frames, and execution/call/return/interrupt counters.

All 15 non-empty combinations of the known state-4 bits `{6, 14, 15, 16}` are now admitted explicitly and pass:

- **15 masks × 36 cases = 540/540 strong-exact**.

No wildcard admission is used.

## Explicit state-4 mask set

- `0x00000040`
- `0x00004000`
- `0x00004040`
- `0x00008000`
- `0x00008040`
- `0x0000c000`
- `0x0000c040`
- `0x00010000`
- `0x00010040`
- `0x00014000`
- `0x00014040`
- `0x00018000`
- `0x00018040`
- `0x0001c000`
- `0x0001c040`

## Recovered families

### Countdown compare family

`0x00000040`, `0x00010000`, and `0x00010040` require no instruction-count correction. Their final compare state is `EQUAL` for countdown `0` and `LESS` for countdown `1`, with the measured frame-3 poststate restored.

### Bit-15-only frame family

`0x00008000` ends in `LESS` and has a distinct measured `frame3.r15 = 0x80004400` on fighter1/bilateral paths.

### Bit-6 + bit-15 accounting family

`0x00008040` removes 3 native-accounted instructions per active fighter, ends in `LESS`, and restores the measured frame-3 poststate. Underflow remains fail-closed.

### Bit-14 + bit-16 family

`0x00014000` adds one instruction per active fighter only when mode bit 6 is set. Compare follows countdown (`EQUAL`/`LESS`).

### Bit-14 + bit-15 + bit-16 family

`0x0001c000` adds 3 instructions per active fighter with mode bit 6 clear and 4 per fighter with it set, ending in `LESS`.

### Bit-14 families

`0x00004000` and `0x00004040` add, per active fighter:

`3 * countdown + mode_bit6`

and compare follows countdown.

`0x0000c000` adds `mode_bit6` per active fighter and ends in `LESS`.

`0x0000c040` adds `5 + mode_bit6` per active fighter and ends in `LESS`.

### Irregular `0x00014040` table

This mask was kept as an explicit distribution/countdown/mode table instead of generalized. Native-to-reference instruction corrections are:

| distribution | cd=0,m=0 | cd=0,m=1 | cd=1,m=0 | cd=1,m=1 |
| --- | ---: | ---: | ---: | ---: |
| fighter0-only | 0 | -1 | -1 | -1 |
| fighter1-only | 0 | +5 | +4 | +5 |
| bilateral | -1 | +3 | +2 | +3 |

Compare follows countdown and the measured frame-3 state is restored.

### Asymmetric `0x0001c040` family

The instruction correction is independent of countdown:

- fighter0-only: `+2`;
- fighter1-only: `+7 + mode_bit6`;
- bilateral: `+9 + mode_bit6`.

The final compare state is `LESS`.

## RAM-semantic branch

The only masks in this 15-mask state-4 space that introduced a new persistent RAM semantic relative to the native path were:

- `0x00018000`;
- `0x00018040`.

For each active fighter, the ROM propagates the child-state bit into `fighter + 0x1a4`:

`state_flags |= 0x00000800`

The native path already removes the legacy `0x8000` state component; the recovered poststate adds the missing child bit. These masks also require `+4` recovered instructions per active fighter, final compare `LESS`, and the measured frame-3 state.

## Cross-regression

After completing state-4, the exact same `a6b443a3` artifact was checked against representative state-8 families:

- `0x00204000`
- `0x00208000`
- `0x00210000`
- `0x00214000`
- `0x00218000`
- `0x06214000`
- `0xc0214000`
- `0xfc214000`

All **8 × 36 = 288/288** cases remained strong-exact.

The proprietary ROM and generated snapshots/traces remain local-only and are not committed.
