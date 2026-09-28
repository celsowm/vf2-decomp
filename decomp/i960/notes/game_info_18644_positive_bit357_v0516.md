# `fa_game_info` positive state-8 bit-3/5/7 family (v0516)

## Scope

This note records the measured native slice for the no-bit-8 positive masks
made from bits 3, 5 and 7:

```text
0x00000028  (bits 3+5)
0x00000088  (bits 3+7)
0x000000a0  (bits 5+7)
0x000000a8  (bits 3+5+7)
```

Both fighter state bytes are `8`. The measured matrix covers the three
fighter-flag distributions, countdown `0` and `1`, mode bit 6 clear and set,
and shared thresholds `0..2`. Each mask is `36/36` exact against the original
i960 execution; the combined result is `144/144` exact.

## Recovered behavior

The child memory path already matches the reference. The dispatcher accounting
requires two additional native instructions for every accepted case. The final
condition is EQUAL when countdown is zero and LESS when countdown is nonzero.
The next local frame retains the measured stale values:

```text
r3 = 0x41000000
r4 = 0x07800f0f
r7 = 0x41000000
```

The admission remains exact to the four masks above, state 8 on both fighters,
the measured distribution, and threshold at most 2. Threshold-3 controls are
`0/12` exact for each mask, so they remain unsupported.

## Reproduction

From the repository root, for example:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0xa8 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 4
```

The same command with `--mask 0x28`, `0x88` and `0xa0` reproduces the other
three exact matrices.
