# v0414: recover the compare-prefix greater tails

## Verdict

The measured signed-greater compare-prefix arm at `0x14640` now admits four
additional tails.  All require `+0x198 == 0`, `+0x654 != 0` and signed
`s16(+0x1aa) > s16(+0x62a)`:

- neutral state, bit 4 clear, `+0x194 == 0`: 14 instructions to `0x146d8`,
  leaves `+0x654` unchanged and finishes EQUAL;
- neutral state, bit 4 clear, nonzero `+0x194`: 16 instructions to `0x146d8`,
  clears `+0x654` and finishes LESS;
- state 13, bit 4 clear: 16 instructions to `0x146d8`, clears `+0x654` and
  finishes LESS;
- state 13, bit 4 set: 17 instructions to `0x146d8`, clears `+0x654` and
  finishes LESS.

The existing neutral bit-4-set state remains the separate 15-instruction
greater path to `0x146c4`, which clears `+0x194` and finishes GREATER.
Unmeasured state/flag compositions remain unsupported.

## Pins

`vf2_player_14640_compare_tail_live` restores `out/park-1442c.vf2snap`,
constructs the four measured shapes, and runs the reference interpreter and
native compare helper to `0x146d8`.  It requires exact instruction counts and
full live-state equality, including registers, condition state, frames and
Work RAM.
