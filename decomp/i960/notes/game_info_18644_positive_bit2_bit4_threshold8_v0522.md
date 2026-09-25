# `fa_game_info` positive mixed masks through threshold 8 (v0522)

The five measured positive state-8 masks `0x00000014`, `0x0000001a`,
`0x0000001c`, `0x00000034` and `0x00000094` are now admitted through shared
threshold `8`. Both fighter state bytes remain `8`, and the complete matrix
covers the three measured fighter-flag distributions, countdown `0` and `1`,
and mode bit 6 clear and set:

```text
5 masks × 3 distributions × 2 countdowns × 2 mode values × 8 thresholds = 240 cases
```

All 240 cases match the reference at the full dispatcher boundary, including
CPU registers, condition state, local frames, procedure state, counters and
mutable Model 2A memory. The mixed-mask accounting remains exact: the native
path subtracts three instructions at countdown zero and adds two at a nonzero
countdown.

Admission remains limited to the five measured masks, the three measured flag
distributions, state `8` on both fighters and thresholds `0..8`. Threshold
`9+`, the adjacent `0x18`, other flag distributions and unmeasured positive
compositions remain `VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x14 --thresholds 5,6,7,8 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 4
```

The same command with `--mask 0x1a`, `0x1c`, `0x34` and `0x94` reproduces the
other exact 48-case slices.

