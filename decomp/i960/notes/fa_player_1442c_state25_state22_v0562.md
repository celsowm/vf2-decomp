# `fa_rob` `0x1442c`: state-25/state-22 successor (v0562)

## Boundary

This slice records the measured successor:

```text
fighter0 + 0x197 = 25
fighter1 + 0x197 = 22
```

It uses the already recovered state-25 body and neutral `0x14528..0x1463c`
tail. This is a single measured state composition; it does not generalize
the remaining state-25 successor family.

## Evidence

The ROM-backed `out/park-1442c.vf2snap` fixture uses the same measured setup
as the state-25 witness: fighter1 `+0x194 = 0x0073`, both `+0x19b` values
zero, both `+0x198` and `+0x654` values zero, fighter1 `+0x1aa = 0`,
fighter0 `+0x858 = 0`, and fighter1 `+0x808 = 2`. Only fighter1 `+0x197`
changes to 22. The reference reaches `0x1463c` in 98 instructions with
3 calls and 3 returns.

The final `0x14560 cmpobne 16,r8` compare is LESS for `r8 = 22`. The native
tail admits this measured compare outcome alongside the already proven state
24 and state 25 successors; other state compositions remain fail-closed.

## Validation

The focused fixture compares registers, condition state, local frames,
procedure counters and touched Model 2A memory at `0x1463c`:

```text
player-1442c-live-25-22 ref=98 native=98 calls=3/3 rets=3/3
```
