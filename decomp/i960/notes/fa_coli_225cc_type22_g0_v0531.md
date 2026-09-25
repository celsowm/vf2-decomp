# v0531: `fa_coli` `0x225cc` type-22 resolver `g0`

The bounded selector probe from `out/coli-225cc-entry.vf2snap` was extended
from the 55 walker misses to every selector `g8+0x19c == 1..64`, with
`g8+0x19f == 22`. The reference reaches `0x10dcc` for all 64 cases with
three nested calls and five returns.

The nine type-5 hits expose the record pointer returned by `0x1ab34` in the
architectural `g0`; the measured values are:

```text
13 -> 0x02014676    14 -> 0x020146bb    15 -> 0x020145b0
22 -> 0x02014e28    25 -> 0x02014ea8    27 -> 0x02014f28
30 -> 0x02014776    36 -> 0x02014738    43 -> 0x02014c85
```

The native `0x18bd4` body now threads the nonzero resolver result through
`0x225cc` to the wrapper. Walker misses leave the caller's `g0` unchanged,
matching the measured zero-record tail. The ROM-backed fixture compares all
64 selectors for exact CPU registers, condition state, procedure state and
mutable Model 2A memory.

Selector 0 remains outside the admitted range: the reference faults during
the table walk and the native path remains fail-closed.
