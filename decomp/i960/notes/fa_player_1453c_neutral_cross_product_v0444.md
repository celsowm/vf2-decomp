# `fa_player` bounded neutral `0x14528` cross-product (v0444)

The neutral dispatch path is reached when neither state byte selects the
dedicated state-16 or state-27 arms. Two complete reference rows from
`out/park-1442c.vf2snap` measured every `r7=0..31` value with `r8 == 0` and
with `r8 == 1`.

Both rows produce 9 instructions and no calls/returns for every `r7` except
16 and 27. The exceptions take the already measured direct type-5 or
state-27 arms. This is the three dispatch comparisons followed by the five
instructions at `0x14628..0x14638`; it clears both `+0x198` fields and leaves
the measured GREATER compare result.

Native C now admits the measured bounded cross-product where both state bytes
are in `0..31` and neither is 16 or 27. The live fixture proves the
state-26/state-1 pair with exact CPU, condition-state, frame, and Work-RAM
equality. Values above 31 and dedicated 16/27 combinations remain fail-closed.
