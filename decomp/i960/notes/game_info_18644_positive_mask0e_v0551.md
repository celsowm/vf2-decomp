# `fa_game_info` positive state-8 mask `0x0e` (v0551)

The measured positive state-8 composition `0x0000000e` (bits 1+2+3) is now
admitted through shared threshold `8`. Both fighter state bytes remain `8`,
and the complete matrix covers the three measured fighter-flag distributions,
countdown `0` and `1`, mode bit 6 clear and set, and thresholds `0..8`:

```text
3 distributions × 2 countdowns × 2 mode values × 9 thresholds = 108 cases
```

All 108 cases match the reference at the full dispatcher boundary, including
CPU registers, condition state, local frames, procedure state, counters and
mutable Model 2A memory. The measured native path needs a two-instruction
positive correction relative to the generic mixed-mask zero-countdown
accounting; the nonzero-countdown path is exact without that correction.

At v0551 the admission was limited to thresholds `0..8`. The later v0581
measurement extends this same mask to threshold `9` without changing the
dispatcher accounting; see
`game_info_18644_positive_mask0e_threshold9_v0581.md`. Threshold `10+`, other
flag distributions and unmeasured positive compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Reproduction

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0xe \
  --thresholds 0,1,2,3,4,5,6,7,8 --workers 4
```
