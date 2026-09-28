# `fa_game_info` positive state-8 bit-8 plus bit-3/5/7 unilateral family v0515

Follow-up ROM-backed full-dispatch probes expanded the v0514 unilateral slice
to these exact combined state-8 masks:

```text
0x00000108  (bits 3+8)
0x00000120  (bits 5+8)
0x00000128  (bits 3+5+8)
0x00000180  (bits 7+8)
0x00000188  (bits 3+7+8)
0x000001a0  (bits 5+7+8)
0x000001a8  (bits 3+5+7+8)
```

For every mask, both fighter state bytes are `8`, exactly one fighter carries
the mask in `+0x1a4`, mode bit 6 is clear, countdown is `0` or `1`, and the
shared threshold is `0`, `1` or `2`. Each mask is `12/12` exact for the two
unilateral distributions (`84/84` exact across the seven-mask family),
including architectural state, touched memory, condition state and counters.

The shared dispatcher correction is `-9` instructions at countdown zero and
`+2` otherwise. The final condition is EQUAL for countdown zero and LESS for
countdown one, with stale-frame values `r3=0x41000000`,
`r4=0x07800f0f`, `r7=0x41000000`.

Bilateral and mode-bit-6 cases remain explicit unsupported boundaries. The
threshold-3 controls for the six newly added masks are all `0/12`; the
previous `0x108` threshold-3 control is also `0/12`.

Reproduce one accepted matrix with:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x1a8 --state 8 \
  --thresholds 0,1,2 --workers 4
```
