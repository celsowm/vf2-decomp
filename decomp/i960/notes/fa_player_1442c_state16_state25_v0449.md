# `fa_player` integrated state-16/state-25 zero-selector join (v0449)

The measured `0x1442c` row with fighter 0 `+0x197 == 16` and fighter 1
`+0x197 == 25` takes the `0x1446c` swap into `0x144b0`. In the accepted
shape, fighter 1 has low halfword `+0x194 == 0` and fighter 0 has the valid
type-5 index `+0x194 == 0x73`.

The swapped `0x19ef8` call therefore uses the already recovered zero-selector
path. After the collision body restores the original fighter bases, the
`0x14528` state checks select the direct state-16 `0x14570` type-5 tail.
The ROM-backed fixture reaches `0x1463c` in **144 instructions** with
**four calls/four returns**, and the native/reference live state is equal.

The native guard checks the measured low-halfword composition and leaves the
nonzero `0x19ef8` selector, other type indices, and sibling state/scaling
compositions fail-closed.
