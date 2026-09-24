# v0412: recover the fa_rob compare-prefix less-than neutral tail

## Verdict

The measured `0x14640` compare-prefix sibling with signed
`s16(+0x1aa) < s16(+0x62a)` is now native for one complete neutral shape.
The accepted gates are:

- `+0x198 == 0` and `+0x654 != 0`;
- `+0x197` is not 27, 28 or 13;
- bit 4 of `(g7)` is clear; and
- `+0x194 == 0`.

The reference path falls through the shared tail, branches at `0x146b4` to
`0x146c8`, then takes `cmpobe 0,r14` at `0x146cc` to the `0x146d8` return.
It is 14 instructions, has no calls or returns, leaves `+0x654` unchanged,
and finishes with EQUAL condition state.  The native helper preserves those
register, condition and memory effects and leaves the return unconsumed for
the caller.

## Pins

`vf2_player_14640_compare_less_live` restores `out/park-1442c.vf2snap`,
sets `+0x654 = 1`, `+0x1aa = 1`, `+0x62a = 2`, clears the neutral fighter
fields and runs the reference and native helpers to `0x146d8`.  The fixture
requires 14/14 instructions and full live-state equality.  Other less-than
compositions, state-27/28/13 tails, bit-4-set paths and nonzero `+0x194`
remain fail-closed.
