# `fa_coli` `0x225cc` additional bit-15/bit-16 selectors (v0460)

## Verdict

Eight additional exact g8 words are native on the measured scan-4/bit-16
continuation. The live fixture proves full state equality. The bit-3 word
`0x00018008` is deliberately not admitted: it reaches the ROM return but
enters an unmodeled downstream branch. Bit 8 and bit 13 variants remain
deferred.

## Evidence

All admitted witnesses use `g7 + 0x1a4 = 0x00400100` and `g7 + 0x821 = 4`.
The additional native words are:

```text
0x00018002  0x00018020  0x00018040  0x00018080
0x00018200  0x00018400  0x0001c000  0x00038000
```

Each compact sibling matches the reference at 107 instructions. The
`0x0001c000` word requires the measured +3 instruction correction in the
direct g0=5 tail. `0x00018008` is measured at 107 reference instructions
but remains fail-closed in native C; `0x00018100` is a 103-instruction
reference path requiring separate bit-8 recovery, and `0x0001a000` did not
reach the selected return within the probe limit.

