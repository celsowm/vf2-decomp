# v0634 — `fa_game_info` positive mask `0x22`, threshold 3

The no-bit-8 state-8 mask `0x00000022` (bits 1+5) was measured across the
three fighter-record distributions, countdown `0/1` and mode bit 6 clear/set.
Before admission, all 12 native runs differed from the oracle only by a
two-instruction deficit and the compare-result byte. Applying the existing
uniform mixed-low correction produces **12/12 exact** CPU, condition,
frame, procedure-counter and mutable-Model-2A matches.

The admission is restricted to the measured matrix distribution and threshold
`<= 3`; threshold 4 controls remain explicit `VF2_ERROR_UNSUPPORTED` cases.
