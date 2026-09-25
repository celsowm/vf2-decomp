# `fa_rob` `0x1442c`: state-25 successor family (v0563)

## Evidence

The ROM-backed `out/park-1442c.vf2snap` fixture was measured with the same
state-25 setup used by v0561/v0562 and swept fighter1 `+0x197` through the
bounded family:

```text
17..23, 25..26, 28..31
```

The reference reaches `0x1463c` for every case with 3 calls and 3 returns.
All cases compare LESS at `0x14560`; instruction counts are 98 for every
case except successor 28, which takes a measured 92-instruction sibling.
The already recovered successors 24 and 27 remain their distinct 105- and
126-instruction arms.

## Recovery

The existing state-25 body already reaches the common neutral tail for this
measured family. Its final compare mapping now admits only the measured
state range 17..31 excluding state 27; state 24 and state 25 are included in
that measured range, while state 27 continues through its dedicated helper.
The state-28 shorter count is recorded in the integrated fixture. No values
outside the measured family are newly admitted by this slice.

## Validation

The focused fixture runs each measured successor and compares CPU registers,
condition state, local frames, procedure counters and touched Model 2A memory
at `0x1463c`. Representative outputs are:

```text
player-1442c-live state=17-25-neutral ref=98 native=98 calls=3/3 rets=3/3
player-1442c-live state=28-25-28 ref=92 native=92 calls=3/3 rets=3/3
player-1442c-live state=31-25-neutral ref=98 native=98 calls=3/3 rets=3/3
```

Unmeasured state values remain `VF2_ERROR_UNSUPPORTED`.
