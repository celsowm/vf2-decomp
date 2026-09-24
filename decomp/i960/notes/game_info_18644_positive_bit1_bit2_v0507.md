# `fa_game_info` positive state-8 bits 1/2 v0507

The measured full-dispatch masks `0x00000002` and `0x00000004` are now native
for the bounded positive-threshold domain:

- both fighter state bytes are `8`;
- fighter flags use the measured distribution set `{mask,0}`, `{0,mask}` or
  `{mask,mask}`;
- countdown is `0` or `1`;
- mode byte bit 6 is clear or set; and
- the shared threshold is `0`, `1` or `2`.

The combined ROM/native matrix is `72/72` exact. Both masks use the measured
two-instruction dispatcher correction, stale frame values
`r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`, and EQUAL for countdown `0`
or LESS for countdown `1`.

Threshold `3` and other positive compositions remain explicit fail-closed
boundaries.

Reproduce the accepted matrices with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x2 --state 8 \
  --thresholds 0,1,2 --workers 1
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x4 --state 8 \
  --thresholds 0,1,2 --workers 1
```
