# `fa_game_info` positive state-8 mask `0x1a` through threshold 9 (v0582)

The positive state-8 composition `0x0000001a` (bits 1+3+4) now uses its
existing measured `native_state8_bit1_bit3_bit4_positive_path` through
threshold `9`. The reference/native full-dispatch matrix covers the three
measured fighter-record distributions, countdown `0` and `1`, and mode bit 6
clear and set:

```text
3 distributions × 2 countdowns × 2 mode values × threshold 9 = 12 cases
```

All 12 cases match exactly, including CPU registers, condition state, local
frames, procedure state, call/return and instruction counters, and mutable
Model 2A memory. Reference snapshots at thresholds 8 and 9 have identical
dispatcher accounting and differ only in the threshold word (`8` versus
`9`). Threshold `10+`, unmeasured distributions and other positive
compositions remain `VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x1a \
  --thresholds 9 --workers 4
```
