# `fa_game_info` positive state-8 bit-3+bit-8 unilateral slice v0514

The ROM-backed full-dispatch probe for combined state-8 field mask
`0x00000108` found one bounded native slice:

- both fighter state bytes are `8`;
- exactly one fighter carries `0x00000108` in `+0x1a4`, and the other is
  clear;
- the mode byte's bit 6 is clear;
- countdown is `0` or `1`; and
- the shared threshold is `0`, `1` or `2`.

This is `12/12` exact for each unilateral distribution (`24/24` together),
including architectural state, touched memory, condition state and counters.
The dispatcher correction is `-9` instructions at countdown zero and `+2`
otherwise. The final condition is EQUAL for countdown zero and LESS for
countdown one, with the measured stale-frame values
`r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`.

The bilateral distribution and mode-bit-6 cases remain explicit unsupported
boundaries. Threshold `3` remains a `0/12` control. No neighboring mask is
admitted by this slice.

Reproduce the accepted slice with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x108 --state 8 \
  --thresholds 0,1,2 --workers 4
```
