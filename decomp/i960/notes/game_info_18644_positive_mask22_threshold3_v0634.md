# v0634 — `fa_game_info` positive mask `0x22`, threshold 3

The no-bit-8 state-8 masks `0x00000022` (bits 1+5), `0x00000024` (bits 2+5),
`0x00000026` (bits 1+2+5) and `0x0000002a` (bits 1+3+5) were measured across the three fighter-record
distributions, countdown `0/1` and mode bit 6 clear/set. Before admission, all
12 native runs for each mask differed from the oracle only by a
two-instruction deficit and the compare-result byte. Applying the existing
uniform mixed-low correction produces **12/12 exact** CPU, condition,
frame, procedure-counter and mutable-Model-2A matches.

The admissions are restricted to the measured matrix distribution and
threshold `<= 3`; threshold 4 controls remain explicit
`VF2_ERROR_UNSUPPORTED` cases.
