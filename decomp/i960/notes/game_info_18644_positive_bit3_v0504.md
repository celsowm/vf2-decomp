# `fa_game_info` positive state-8 bit-3 family v0504

The measured full-dispatch mask `0x00000008` is now native for the bounded
positive-threshold domain:

- both fighter state bytes are `8`;
- fighter flags use the measured distribution set `{mask,0}`, `{0,mask}` or
  `{mask,mask}`;
- countdown is `0` or `1`;
- mode byte bit 6 is clear or set; and
- the shared threshold is `0`, `1` or `2`.

The ROM/native matrix is `36/36` exact through the complete dispatcher. The
child path already matched memory and state; the remaining correction is two
dispatcher instructions. The final condition is EQUAL for countdown `0` and
LESS for countdown `1`, with the measured stale-frame values
`r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`.

The adjacent `0x00000018` composition remains a `0/12` control, and threshold
`3` for `0x00000008` remains a `0/12` control. Other positive state-8 bit
compositions remain outside this admission.

Reproduce the accepted matrix with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x8 --state 8 \
  --thresholds 0,1,2 --workers 1
```
