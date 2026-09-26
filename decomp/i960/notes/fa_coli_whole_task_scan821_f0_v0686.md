# `fa_coli` fighter-0 scan-2 neighbors — v0686

The parked `0x221e8` whole-task snapshot was run with fighter-0 bit 8 and
`field_0821 = 2`, first with fighter 1's bit-8 flag clear and then with both
bit-8 flags set. Both reference runs reached `0x10dcc`:

| flags | result |
| --- | --- |
| fighter 0 only | `9392/17/18` |
| both fighters | `9527/18/19` |

The native recovery removes two measured accounting instructions in these
shapes and matches CPU registers, condition state, procedure counters, local
state and mutable Model 2A memory exactly. The whole-task gate admits only
these measured call/return shapes.

The neighboring scan-6 mutation was also measured, but it enters a different
midbody branch at `0x22210` (`9389/17/18` single and `9524/18/19` bilateral)
that is not recovered here and remains `VF2_ERROR_UNSUPPORTED`.
