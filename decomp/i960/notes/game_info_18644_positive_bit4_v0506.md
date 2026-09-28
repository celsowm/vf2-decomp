# `fa_game_info` positive state-8 bit-4 family v0506

The measured full-dispatch mask `0x00000010` is now native for the bounded
positive-threshold domain:

- both fighter state bytes are `8`;
- fighter flags use the measured distribution set `{mask,0}`, `{0,mask}` or
  `{mask,mask}`;
- countdown is `0` or `1`;
- mode byte bit 6 is clear or set; and
- the shared threshold is `0`, `1` or `2`.

The ROM/native matrix is `36/36` exact. The measured dispatcher accounting is
three instructions shorter for mode bit 6 set with a zero countdown, and two
instructions longer for the other accepted combinations. Every accepted case
also uses the stale frame values `r3=0x41000000`, `r4=0x07800f0f` and
`r7=0x41000000`, with EQUAL for countdown `0` and LESS for countdown `1`.

Threshold `3` remains an explicit fail-closed control.

Reproduce the accepted matrix with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x10 --state 8 \
  --thresholds 0,1,2 --workers 1
```
