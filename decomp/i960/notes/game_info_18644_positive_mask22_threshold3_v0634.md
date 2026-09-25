# v0634/v0635 — `fa_game_info` positive masks `0x22`/`0x32`, threshold 3

The no-bit-8 state-8 masks `0x00000022` (bits 1+5), `0x00000024` (bits 2+5),
`0x00000026` (bits 1+2+5), `0x0000002a` (bits 1+3+5), `0x0000002c`
(bits 2+3+5), `0x0000002e` (bits 1+2+3+5) and `0x00000032`
(bits 1+4+5) were measured across the three fighter-record distributions,
countdown `0/1` and mode bit 6 clear/set. The first six masks use the uniform
two-instruction correction; `0x32` uses the measured mixed-low correction
(subtract three instructions at zero countdown, add two at non-zero countdown).
Each admitted mask produces **12/12 exact** CPU, condition, frame,
procedure-counter and mutable-Model-2A matches.

The admissions are restricted to the measured matrix distribution and
threshold `<= 3`; threshold 4 controls remain explicit
`VF2_ERROR_UNSUPPORTED` cases.
