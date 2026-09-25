# `fa_rob` `0x14640` bit-4-clear state-13 greater tail — v0540

The second measured greater witness for the compare-prefix uses the state-13
tail:

- `g7 = 0x00510980`;
- `+0x198 = 0`, `+0x654 = 1`;
- `+0x197 = 13`;
- fighter flags have bit 4 clear;
- signed `+0x1aa = 2`, `+0x62a = 1`.

The reference reaches `0x146d8` in 16 instructions. The nonzero
`+0x194` value implied by the state-13 byte is loaded by the `0x146c8` tail,
`+0x654` is cleared, and the final compare is LESS. Consuming the return
through the parked caller takes 17 instructions.

The generic native dispatcher admits only this exact state-13 greater witness
in addition to the v0539 neutral witness. All other greater compositions stay
`VF2_ERROR_UNSUPPORTED`.
