# `fa_coli` bilateral scan composition matrix — v0685

The parked `0x221e8` whole-task snapshot was swept with both fighter bit-8
flags set and each fighter's `field_0821` set independently to
`{0,1,4,5}`. The 16 reference cases all reached `0x10dcc`.

| fighter-0 scan | fighter-1 scan | result |
| --- | --- | --- |
| `0/1/4` | `0/1/4` | `9520/18/19` |
| `5` | `0/1/4` | `9526/18/19` |
| `0/1/4` | `5` | `9526/18/19` |
| `5` | `5` | `9532/18/19` |

The native fixture applies both object fields explicitly and compares CPU,
condition state, procedure counters, local state and mutable Model 2A memory.
All 16 cases match exactly. The recovery keeps the ordinary measured F0
correction, adds the measured F1 scan-5 correction, and admits the resulting
`9532/18/19` task cluster at the whole-task gate.

This does not establish other `field_0821` values, other object fields, or
unmeasured flag compositions; those paths remain fail-closed.
