# `fa_rob` `0x14640` non-equal compare-prefix variants (v0428)

The compare prefix at `0x14650..0x14658` only branches on equality of the
signed halfword values loaded from fighter `+0x1aa` and `+0x62a`. Measured
greater witnesses therefore follow the same tails already recovered for the
less-than witnesses:

- state 27, `+0x197 == 27`: the type-15 walk reaches `0x146c4` in 44
  instructions with one call/return;
- state 28, `+0x197 == 28`: the arithmetic tail reaches `0x146c4` in 16
  instructions with no call/return; and
- neutral and state-13 tails retain their existing 14/16/15/17-step shapes
  according to bit 4 and `+0x194`.

Equal compare values remain routed to the dedicated `0x146dc` escape arm.
Board bit 20, walker misses and other unmeasured compositions remain
unsupported.

## Pin

The state-27 and state-28 live fixtures each run original and both unequal
compare directions with exact instruction/call/return counts and full
register, condition, frame and Work-RAM equality. The compare-less fixture
also runs the greater counterparts of neutral zero/nonzero, bit-4-set and
state-13 shapes.
