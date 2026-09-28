# `fa_game_info` positive state-8 mask `0x0c` (v0527)

The measured positive state-8 composition `0x0000000c` (bits 2+3) is now
admitted through shared threshold `8`. Both fighter state bytes remain `8`,
and the complete matrix covers the three measured fighter-flag distributions,
countdown `0` and `1`, mode bit 6 clear and set, and thresholds `0..8`:

```text
3 distributions × 2 countdowns × 2 mode values × 9 thresholds = 108 cases
```

All 108 cases match the reference at the full dispatcher boundary, including
CPU registers, condition state, local frames, procedure state, counters and
mutable Model 2A memory. The measured native path requires a two-instruction
correction at zero countdown; the nonzero-countdown path is already exact.

Admission remains limited to the ten measured low masks, the three measured
flag distributions, state `8` on both fighters and thresholds `0..8`.
Threshold `9+`, other flag distributions and unmeasured positive
compositions remain `VF2_ERROR_UNSUPPORTED`. Neighboring mask `0x0e` was
measured as a different path and remains fail-closed.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0xc --thresholds 0,1,2,3,4,5,6,7,8 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 4
```
