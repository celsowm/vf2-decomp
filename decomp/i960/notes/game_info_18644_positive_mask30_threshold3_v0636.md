# v0636 — `fa_game_info` positive mask `0x30`, threshold 3

The no-bit-8 state-8 mask `0x00000030` (bits 4+5) was measured across the
three fighter-record distributions, countdown `0/1` and mode bit 6 clear/set.
The native dispatcher accounting is distribution-sensitive at zero countdown:
the unilateral cases require subtracting three instructions, bilateral mode
bit-6-clear requires subtracting six, and bilateral mode-bit-6-set requires
subtracting three. All non-zero-countdown cases require adding two. With that
measured correction, all **12/12** cases match CPU state, condition, frame,
procedure counters and mutable Model-2A state.

Admission is restricted to the measured matrix distribution and threshold
`<= 3`; threshold 4 controls remain explicit `VF2_ERROR_UNSUPPORTED` cases.
