# `fa_game_info` positive state-8 mask `0x14` v0518

## Scope

This note closes the measured state-8 mask `0x00000014` (bits 2+4) at
`0x00018644`. Both fighter state bytes are `8`. The complete matrix covers
the three measured fighter-flag distributions, countdown `0` and `1`, mode
bit 6 clear and set, and thresholds `0..3`:

```text
3 distributions × 2 countdowns × 2 mode values × 4 thresholds = 48 cases
```

## Evidence

The existing thresholds `0..2` remained exact. The newly measured threshold-3
cases all followed the same dispatcher join: native accounting is three
instructions high at countdown zero and two instructions low at nonzero
countdown before correction. The reference and recovered runs agree on CPU,
condition state, local frames, procedure state, counters and mutable Model 2A
memory for all 12 new cases.

The full validator reports:

```text
summary: 48/48 exact
```

## Recovery boundary

Native admission is extended only for the exact combined mask `0x14`, measured
fighter distributions and thresholds `0..3`. The adjacent mask `0x18`,
threshold `4` and above, the other v0517 masks at threshold `3`, and
non-distribution flag splits remain `VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x14 \
  --thresholds 0,1,2,3 --base out/game-info-bit102/game_info_1645c.vf2snap \
  --workers 4
```

