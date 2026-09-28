# `fa_game_info` positive state-8 bits 5/7 v0505

The measured full-dispatch masks `0x00000020` and `0x00000080` are now native
for the bounded positive-threshold domain:

- both fighter state bytes are `8`;
- fighter flags use the measured distribution set `{mask,0}`, `{0,mask}` or
  `{mask,mask}`;
- countdown is `0` or `1`;
- mode byte bit 6 is clear or set; and
- the shared threshold is `0`, `1` or `2`.

The combined ROM/native matrix is `72/72` exact. For both masks the child
state and memory already matched; the remaining correction is two dispatcher
instructions. The final condition is EQUAL for countdown `0` and LESS for
countdown `1`. No additional register or memory correction was required.

The corresponding threshold-`3` cases and the neighboring positive mask
`0x00000010` remain explicit controls/boundaries. The `0x10` probe is not
uniform because its mode-bit-6 cases have a different instruction delta.

Reproduce the accepted matrices with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x20 --state 8 \
  --thresholds 0,1,2 --workers 1
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x80 --state 8 \
  --thresholds 0,1,2 --workers 1
```
