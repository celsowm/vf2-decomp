# `fa_game_info` positive state-8 bit-8 pair families v0511

The measured full-dispatch masks `0x00000104` (bits 2+8) and `0x00000110`
(bits 4+8) are now native for the bounded positive-threshold domain:

- both fighter state bytes are `8`;
- fighter flags use `{mask,0}`, `{0,mask}` or `{mask,mask}`;
- countdown is `0` or `1`;
- mode byte bit 6 is clear or set; and
- the shared threshold is `0`, `1` or `2`.

The combined ROM/native matrix is `72/72` exact. For `0x104`, unilateral
joins are three instructions long at zero countdown and two short at nonzero
countdown; bilateral joins are two long at zero countdown and three short at
nonzero countdown. For `0x110`, the isolated bit-8 distribution-specific
corrections apply: bilateral +3 except mode bit 6 plus zero countdown (+2),
fighter-0-only −3 in that case, fighter-1-only +1 in that case, and +2 for
the remaining accepted joins.

Both masks use stale frame values `r3=0x41000000`, `r4=0x07800f0f`,
`r7=0x41000000`, with EQUAL for countdown `0` and LESS for countdown `1`.
Other positive compositions remain explicit fail-closed boundaries.

Reproduce the accepted matrices with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x104 --state 8 \
  --thresholds 0,1,2 --workers 1
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x110 --state 8 \
  --thresholds 0,1,2 --workers 1
```
