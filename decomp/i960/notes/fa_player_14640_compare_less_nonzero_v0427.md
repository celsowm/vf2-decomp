# v0427: recover neutral nonzero less-than tail

## Verdict

The measured neutral bit-4-clear `0x14640` compare-prefix sibling with
nonzero `+0x194` is now native. With `+0x654 != 0` and signed
`s16(+0x1aa) < s16(+0x62a)`, the shared `0x146c8` tail loads the nonzero
`+0x194`, clears `+0x654`, and reaches `0x146d8` in 16 instructions. The
existing zero `+0x194` sibling remains 14 instructions.

The final registers are `r14 = +0x194` and `r15 = 0`, with LESS condition
state. The measured neutral bit-4-set sibling instead clears `+0x194`, leaves
`r14 = +0x62a`, returns at `0x146c4` in 15 instructions and leaves GREATER
condition state. State 13 and all neutral siblings are now covered by the
same compare-less fixture; other less-than compositions remain fail-closed.

## Pin

`vf2_player_14640_compare_less_live` runs neutral zero, state 13, neutral
nonzero and neutral bit-4-set cases and requires exact instruction count,
condition state and full live-state equality.
