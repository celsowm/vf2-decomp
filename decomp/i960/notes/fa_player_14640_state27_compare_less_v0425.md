# v0425: recover state-27 compare-prefix less-than

## Verdict

The measured `0x14640` state-27 sibling with `+0x654 != 0` and signed
`s16(+0x1aa) < s16(+0x62a)` is now native. The three compare-prefix
instructions execute before the existing type-15 walk, which reaches
`0x146c4` in 44 instructions with one call/return. The original
`+0x654 == 0` state-27 path remains 41 instructions.

The accepted gates are:

- `+0x198 == 0`;
- `+0x654 != 0`;
- `+0x197 == 27`;
- signed `+0x1aa < +0x62a`;
- a valid type-15 record at `+0x194`; and
- board `0x500068` bit 20 clear.

The walk stores the original `+0x194` into `+0x654`, writes the derived
halfword to `+0x62a`, and clears `+0x194`, matching the original arm.
Other compare-prefix state-28/bit-4-clear and non-less relations remain
fail-closed.

## Pin

`vf2_player_14640_state27_live` runs both the original and compare-prefix
shapes and requires exact instruction count, call/return counters and full
live-state equality.
