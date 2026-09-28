# `fa_game_info` positive state-8 mask `0x0e` through threshold 9 (v0581)

The existing native corridor for state-8 composition `0x0000000e` (bits
1+2+3) was extended by one measured threshold. The reference executor was
run for all three measured fighter-record distributions, countdown `0` and
`1`, and mode bit 6 clear and set:

```text
3 distributions × 2 countdowns × 2 mode values × threshold 9 = 12 cases
```

All 12 cases match the native full dispatcher exactly, including CPU
registers, condition state, local frames, procedure state, call/return and
instruction counters, and mutable Model 2A memory. Relative to threshold 8,
the reference changes only the measured threshold word from `8` to `9`; the
instruction and dispatcher accounting remain identical.

The extension is intentionally specific to mask `0x0e`. The sibling mixed
masks remain capped at threshold 8, and threshold 10 for `0x0e`, unmeasured
fighter-flag distributions, and other positive compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0xe \
  --thresholds 9 --workers 4
```
