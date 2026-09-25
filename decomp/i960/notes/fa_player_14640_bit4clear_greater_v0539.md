# `fa_rob` `0x14640` bit-4-clear greater tail — v0539

The measured neutral compare-prefix sibling is now routed through the generic
`0x14640` dispatcher.

## Witness

Starting from `out/park-1442c.vf2snap` with `IP=0x14640`, the controlled
state is:

- fighter `g7 = 0x00510980`;
- `+0x198 = 0`, `+0x654 = 1`, `+0x194 = 0`;
- `+0x197 = 0`;
- fighter flags `0x04200000` (bit 4 clear);
- signed `+0x1aa = 2`, `+0x62a = 1`.

The reference takes `0x14640..0x14660`, then the bit-4-clear tail at
`0x146b0..0x146d8`: 14 instructions, no nested call or return, with
`+0x194` unchanged at zero and the final compare result from
`cmpobe 0,+0x194` equal. Consuming the `0x146d8` ret through the parked
caller returns to `0x1438c` at 15 total instructions.

The native test exercises the generic dispatcher for this witness and keeps
the existing direct compare-less variants. Full live-state comparison passes.
The dispatch gate admits only this measured greater relation `(2,1)` when
bit 4 is clear; other greater relations and unmeasured shapes remain
`VF2_ERROR_UNSUPPORTED`.
