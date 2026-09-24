# v0413: recover the compare-less state-13 tail

The `0x14640` signed-less compare-prefix recovery now also admits the measured
state-13 sibling.  Its gates are `+0x198 == 0`, `+0x654 != 0`, signed
`+0x1aa < +0x62a`, `+0x197 == 13`, bit 4 set and nonzero `+0x194`.

The path takes the state-13 branch through `0x146c8`, clears `+0x654`, and
lands at the unconsumed `0x146d8` return after 17 instructions with no calls
or returns.  The final `cmpobe 0,r14` is not taken, so condition state is
LESS.  The existing compare-less fixture now runs both the v0412 neutral
shape and this v0413 state-13 shape against the reference with full live-state
equality.  Other compare-less state/flag combinations remain fail-closed.
