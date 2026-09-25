# `fa_rob` `0x1442c`: state-25/state-25 successor (v0561)

## Boundary

This slice extends the measured `fa_rob` fighter-exchange body at `0x1442c`
for the state pair:

```text
fighter0 + 0x197 = 25
fighter1 + 0x197 = 25
```

The existing `0x144b0` recovery already modeled the body for the ordinary
neutral successor. The new witness proves the same collision/state-exchange
body and neutral `0x14528..0x1463c` tail for the state-25 successor.

## Evidence

The ROM-backed fixture restores `out/park-1442c.vf2snap` and applies the
measured state-25 setup: fighter1 `+0x194 = 0x0073`, both `+0x19b` values
zero, both `+0x198` and `+0x654` values zero, fighter1 `+0x1aa = 0`,
fighter0 `+0x858 = 0`, and fighter1 `+0x808 = 2`. The reference reaches
`0x1463c` in 98 instructions with 3 calls and 3 returns.

The final `0x14560 cmpobne 16,r8` compare is LESS for `r8 = 25`. The native
path therefore admits state 25 only when the measured `r13 < r3` precondition
selects the neutral tail, and rejects the equal/greater `r13` shapes.

## Recovery and validation

The `0x1442c` state-25 dispatch no longer rejects `fighter1 + 0x197 = 25`.
The `0x144b0` gate remains fail-closed for nonzero `+0x194`, unsupported
state pairs, and state-25 cases where `r13 >= r3`. The final compare maps
state-25 (like the measured state-24 successor) to LESS; ordinary neutral
state remains GREATER.

The focused fixture compares registers, condition state, local frames,
procedure counters and touched Model 2A memory at `0x1463c`:

```text
player-1442c-live-25-25 ref=98 native=98 calls=3/3 rets=3/3
```

Unmeasured state compositions remain `VF2_ERROR_UNSUPPORTED`.
