# `fa_player` `0x1453c` state-24/state-16 join (v0438)

The `0x14528` entry dispatches by the temporary state registers before the
shared `0x14570` body:

```text
0x14548 cmpobne 16, r7, 0x14560
0x1454c cmpobne 16, r8, 0x14570
0x14564..0x1456c swap g7/g8
```

The existing recovery admitted the measured `r7 == 0`, `r8 == 16` and
`r7 == 25`, `r8 == 16` shapes, but not the same unscaled join with
`r7 == 24`, `r8 == 16`. A controlled ROM run from `out/park-1442c.vf2snap`
uses the valid type-5 index `0x73` at the selected fighter's `+0x194` and
measures:

- `55` instructions to `0x1463c`;
- `+1` call and `+1` return through `0x1ab34`; and
- exact CPU, condition-state, frame, and Work-RAM equality against the
  native helper.

The native join is limited to this measured state-24/state-16 shape. Its
scaling and board/text siblings are covered by the same shared body only when
independently measured; unrecognized state combinations and walker misses
remain fail-closed.

The focused fixture is `vf2_player_1453c_live`; it runs the new short and
board-bit-9-clear text cases alongside the existing matrix.
