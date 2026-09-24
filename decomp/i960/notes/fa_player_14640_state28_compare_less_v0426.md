# v0426: recover state-28 compare-prefix less-than

## Verdict

The measured `0x14640` state-28 sibling with `+0x654 != 0` and signed
`s16(+0x1aa) < s16(+0x62a)` is now native. The three compare-prefix
instructions precede the existing state-28 arithmetic tail, which reaches
`0x146c4` in 16 instructions with no nested calls or returns. The original
`+0x654 == 0` path remains 13 instructions.

The accepted gates are:

- `+0x198 == 0`;
- `+0x654 != 0`;
- `+0x197 == 28`; and
- signed `+0x1aa < +0x62a`.

The tail adds 3 to `+0x1aa`, clears `+0x194`, and leaves the final compare
state from the state-28 branch, matching the reference. Other compare-prefix
relations remain fail-closed.

## Pin

`vf2_player_14640_state28_live` runs both the original and compare-prefix
shapes and requires exact instruction count, call/return counters and full
live-state equality.
