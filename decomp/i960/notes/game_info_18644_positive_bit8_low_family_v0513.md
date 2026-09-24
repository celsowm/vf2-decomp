# `fa_game_info` positive state-8 bit-8 low-bit family v0513

## Evidence

The committed full-dispatch validator measured the exact combined masks
`0x00000106` (bits 1+2+8), `0x00000112` (bits 1+4+8) and `0x00000116`
(bits 1+2+4+8). Each mask covered three fighter-record distributions,
countdown `0/1`, mode bit 6 clear/set and thresholds `0..2`:

```text
0x106: summary: 36/36 exact
0x112: summary: 36/36 exact
0x116: summary: 36/36 exact
```

All cases matched the reference snapshot, architecture signature, condition
state and instruction/call/return counters.

Before recovery, every mask had the same measured dispatcher deltas as v0512:
unilateral records were native `+3` at countdown `0` and `-2` at countdown
`1`; bilateral records were native `+2` at countdown `0` and `-3` at countdown
`1`. The recovery applies the corresponding distribution-independent join,
the countdown-derived EQUAL/LESS condition and the measured stale frame.

Threshold-3 controls were run for all three masks across all 12
distribution/countdown/mode cases; each was `0/12 exact`.

## Recovery boundary

Only the three exact masks listed above, both fighter state bytes equal to 8,
the three measured distributions and thresholds `0..2` are admitted. Other
positive compositions remain `VF2_ERROR_UNSUPPORTED`.

## Validation

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x106 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 4
```

The same command was run for masks `0x112` and `0x116`; all three returned
`36/36 exact`.
