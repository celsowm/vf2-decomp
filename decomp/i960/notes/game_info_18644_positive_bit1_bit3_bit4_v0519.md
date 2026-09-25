# `fa_game_info` positive state-8 mask `0x1a` v0519

The measured positive state-8 mask `0x0000001a` (bits 1+3+4) is now
admitted through threshold `3`. Both fighter state bytes remain `8`, and the
complete matrix covers the three measured fighter-flag distributions,
countdown `0` and `1`, mode bit 6 clear and set, and threshold `3`:

```text
3 distributions × 2 countdowns × 2 mode values × 1 threshold = 12 cases
```

All 12 cases match the reference at the full dispatcher boundary, including
CPU registers, condition state, local frames, procedure state, counters and
mutable Model 2A memory. The existing v0517 accounting remains exact: the
native path subtracts three instructions at countdown zero and adds two at a
nonzero countdown.

The native admission is limited to the exact mask, the measured three
distributions and thresholds `0..3`. The adjacent `0x18`, thresholds `4+`,
other flag distributions and unmeasured positive compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x1a \
  --thresholds 3 --base out/game-info-bit102/game_info_1645c.vf2snap \
  --workers 4
```

