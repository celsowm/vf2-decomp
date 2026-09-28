# v0637 — `fa_game_info` positive mask `0x38`, threshold 3

The no-bit-8 state-8 mask `0x00000038` (bits 3+4+5) was measured across the
three fighter-record distributions, countdown `0/1` and mode bit 6 clear/set.
It has the same measured dispatcher accounting as mask `0x30`: unilateral
zero-countdown cases subtract three instructions, bilateral mode-bit-6-clear
cases subtract six, bilateral mode-bit-6-set cases subtract three, and every
non-zero-countdown case adds two. The complete 12-case matrix matches CPU,
condition, frame, procedure counters and mutable Model-2A state exactly.

Admission is restricted to the measured matrix distribution and threshold
`<= 3`; threshold 4 controls remain explicit `VF2_ERROR_UNSUPPORTED` cases.
