# fa_rob `0x1442c` state-25/state-24 `0x14474` sibling (v0435)

The measured integrated state-25/state-24 cases take the swapped `0x14474`
body when fighter0 `+0x19f` is 25 or 22.  After the two live `0x14640`
helpers, the branch swaps g7/g8, writes `0x01000000` to the original
fighter0 `+0x194`, clears bit 0 of its `+0x1a4`, and rejoins the `0x14628`
common exit.

The ROM-backed fixture measures and proves exact live-state equality for both
values: 59 instructions with two calls/returns for `+0x19f == 25`, and 60
instructions with two calls/returns for `+0x19f == 22`.  Other state-24
compositions remain fail-closed.
