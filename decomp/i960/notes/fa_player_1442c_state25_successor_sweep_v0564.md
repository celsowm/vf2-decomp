# `fa_rob` `0x1442c`: complete state-25 successor-byte sweep (v0564)

## Boundary

This evidence closes the bounded state-byte dimension for the measured
integrated arm:

```text
fighter0 + 0x197 = 25
fighter1 + 0x197 = 1..31
```

The existing fixture keeps the measured `+0x194`, `+0x19b`, `+0x198`,
`+0x654`, `+0x1aa`, `+0x858` and `+0x808` setup unchanged while sweeping
only fighter1 `+0x197`.

## Oracle results

All 31 cases reach `0x1463c` with full live-state equality, 3 calls and
3 returns. Instruction counts are:

```text
state 16: 144
state 24: 105
state 27: 126
state 28: 92
all other states 1..31: 98
```

The final compare is GREATER for states 1..15 and LESS for states 17..31
except the dedicated state-27 path. State 16, 24, 27 and 28 retain their
measured specialized tails.

## Recovery boundary

The native implementation keeps the measured state-25 dispatch and the
dedicated state-16/state-24/state-27/state-28 continuations. The compare
predicate admits the measured LESS family `17..31` excluding state 27; the
ordinary measured states 1..15 retain the GREATER result. No values outside
the swept byte interval, no alternate `+0x194` shapes and no other fighter
fields are newly admitted.

The ROM-backed fixture runs the full sweep and compares CPU registers,
condition state, local frames, procedure counters and touched Model 2A memory
at `0x1463c` for every case.
