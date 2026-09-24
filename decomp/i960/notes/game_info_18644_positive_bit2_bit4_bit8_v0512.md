# `fa_game_info` positive state-8 bits 2+4+8 v0512

## Evidence

The committed full-dispatch validator was run from the calibrated
`0x0001645c` entry snapshot with combined state-8 field mask `0x00000114`
(bits 2, 4 and 8). It covered the three measured fighter-record
distributions, countdown `0/1`, mode bit 6 clear/set and thresholds `0..2`:

```text
summary: 36/36 exact
```

Every case matched the reference snapshot, architecture signature, condition
state and instruction/call/return counters.

The pre-recovery counter deltas were stable across mode values:

* unilateral records: native `+3` at countdown `0`, native `-2` at countdown
  `1`;
* bilateral records: native `+2` at countdown `0`, native `-3` at countdown
  `1`.

The accepted correction therefore subtracts `3` for unilateral countdown-zero
cases, adds `2` for other unilateral cases, subtracts `2` for bilateral
countdown-zero cases and adds `3` for other bilateral cases. The final
condition is EQUAL for countdown zero and LESS for countdown one; the measured
stale frame values are preserved.

The threshold-3 control was also run across all 12 distribution/countdown/mode
cases and remained `0/12 exact`, so admission is limited to thresholds `0..2`.

## Recovery boundary

Only the exact combined mask `0x00000114`, both fighter state bytes equal to 8,
the three measured distributions and threshold `0..2` are admitted. Neighboring
positive compositions remain `VF2_ERROR_UNSUPPORTED` until independently
measured.

## Validation

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x114 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 4
```

Result: `36/36 exact`.
