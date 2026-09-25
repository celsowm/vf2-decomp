# `fa_game_info` positive bit-2/bit-4 masks through threshold 3 (v0520)

The measured positive state-8 masks `0x0000001c` (bits 2+3+4),
`0x00000034` (bits 2+4+5) and `0x00000094` (bits 2+4+7) are now admitted
through shared threshold `3`. Both fighter state bytes remain `8`, and the
matrix covers the three measured fighter-flag distributions, countdown `0`
and `1`, and mode bit 6 clear and set:

```text
3 masks × 3 distributions × 2 countdowns × 2 mode values = 36 cases
```

All 36 cases match the reference at the full dispatcher boundary, including
CPU registers, condition state, local frames, procedure state, counters and
mutable Model 2A memory. The existing mixed-mask accounting remains exact:
the native path subtracts three instructions at countdown zero and adds two
at a nonzero countdown.

Admission remains limited to the five measured mixed masks, the three measured
flag distributions, state `8` on both fighters and thresholds `0..3`.
Threshold `4+`, the adjacent `0x18`, other flag distributions and unmeasured
positive compositions remain `VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x1c --thresholds 3 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 1
```

The same command with `--mask 0x34` and `--mask 0x94` reproduces the other
exact 12-case slices.

