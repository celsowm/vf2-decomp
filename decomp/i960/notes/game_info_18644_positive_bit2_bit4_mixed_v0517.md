# `fa_game_info` positive mixed bit-2/bit-4 masks (v0517)

## Scope

This note records the measured native slice for the exact state-8 masks:

```text
0x00000014  (bits 2+4)
0x0000001a  (bits 1+3+4)
0x0000001c  (bits 2+3+4)
0x00000034  (bits 2+4+5)
0x00000094  (bits 2+4+7)
```

Both fighter state bytes are `8`. The measured matrix covers the three
fighter-flag distributions, countdown `0` and `1`, mode bit 6 clear and set,
and shared thresholds `0..2`. Each mask is `36/36` exact against the original
i960 execution; the combined result is `180/180` exact.

## Recovered behavior

The child memory path already matches the reference. The dispatcher accounting
requires subtracting three native instructions when countdown is zero and
adding two when countdown is nonzero. The final condition is EQUAL when
countdown is zero and LESS when countdown is nonzero. The next local frame
retains the measured stale values:

```text
r3 = 0x41000000
r4 = 0x07800f0f
r7 = 0x41000000
```

Admission is limited to the five masks above, state 8 on both fighters, the
measured distributions, and threshold at most 2. Threshold-3 controls are
`0/12` exact for each mask, so they remain unsupported.

## Reproduction

From the repository root, for example:

```text
python decomp/i960/tools/validate_game_info_full_dispatch.py \
  build/Debug/vf2i960.exe roms/vf2 --mask 0x14 \
  --base out/game-info-bit102/game_info_1645c.vf2snap --workers 1
```

The same command with `--mask 0x1a`, `0x1c`, `0x34` and `0x94` reproduces the
other exact matrices.
