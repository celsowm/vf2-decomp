# `fa_player` state-16/state-25 type-walker misses (v0450)

Two additional integrated rows complete in the ROM even though the
`0x1ab34` type-5 walk returns `g0 == 0` at a type-8 terminator:

- fighter 0 low-halfword `+0x194 == 0x110`: four chain iterations, then
  `161` instructions to `0x1463c`;
- fighter 0 low-halfword `+0x194 == 0x2cf`: two chain iterations, then
  `147` instructions to `0x1463c`.

Both retain fighter 1 low-halfword `+0x194 == 0`, the swapped zero-selector
`0x19ef8` arm, and four calls/four returns. The native recovery now preserves
the measured zero record through the `0x1457c` tail for these two indices.
Other walker misses remain fail-closed.
