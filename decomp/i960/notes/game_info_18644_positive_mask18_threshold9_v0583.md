# `fa_game_info` positive state-8 mask `0x18` through threshold 9 (v0583)

The positive state-8 composition `0x00000018` (bits 3+4) now uses the
existing measured mixed-low path through threshold `9`. Its separate
bilateral zero-countdown correction remains unchanged. The full-dispatch
threshold-9 slice covers the three measured fighter-record distributions,
both countdown values and both mode-bit-6 values:

```text
3 distributions × 2 countdowns × 2 mode values × threshold 9 = 12 cases
```

All 12 cases match exactly, including CPU registers, condition state, local
frames, procedure state, call/return and instruction counters, and mutable
Model 2A memory. Threshold `10+`, unmeasured distributions and other positive
compositions remain `VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x18 \
  --thresholds 9 --workers 4
```
