# `fa_game_info` positive state-8 bit-8 family v0508

The measured full-dispatch mask `0x00000100` is now native for the bounded
positive-threshold domain:

- both fighter state bytes are `8`;
- fighter flags use `{mask,0}`, `{0,mask}` or `{mask,mask}`;
- countdown is `0` or `1`;
- mode byte bit 6 is clear or set; and
- the shared threshold is `0`, `1` or `2`.

The ROM/native matrix is `36/36` exact. The dispatcher correction is
distribution-specific:

- bilateral: +3 except for mode bit 6 plus zero countdown, where it is +2;
- fighter-0-only: −3 for mode bit 6 plus zero countdown, +2 otherwise; and
- fighter-1-only: +1 for mode bit 6 plus zero countdown, +2 otherwise.

All accepted cases use stale frame values `r3=0x41000000`,
`r4=0x07800f0f`, `r7=0x41000000`, with EQUAL for countdown `0` and LESS for
countdown `1`. Other positive compositions remain explicit fail-closed
boundaries.

Reproduce the accepted matrix with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x100 --state 8 \
  --thresholds 0,1,2 --workers 1
```
