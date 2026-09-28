# v0638 — `fa_game_info` positive mask `0x3c`, threshold 3

The no-bit-8 state-8 mask `0x0000003c` (bits 2+3+4+5) was measured across
the three fighter-record distributions, countdown `0/1` and mode bit 6
clear/set. Every distribution uses the uniform mixed-low dispatcher split:
subtract three instructions at zero countdown and add two at non-zero
countdown. The complete 12-case matrix matches CPU, condition, frame,
procedure counters and mutable Model-2A state exactly.

Admission is restricted to the measured matrix distribution and threshold
`<= 3`; threshold 4 controls remain explicit `VF2_ERROR_UNSUPPORTED` cases.
