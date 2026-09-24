# `fa_rob` `0x1453c` type-5 miss sweep v0491

The direct state-27 `0x1453c/0x14570` fixture was extended from selector
`1..256` to `1..512` for fighter `+0x194`. Selectors `257..512` were measured
against the restored ROM snapshot and all reached `0x1463c` through one nested
`0x1ab34` call and return. The native path matches each reference instruction
count and the complete live-state comparison.

Only the measured `1..512` selector interval is admitted. Larger selectors and
unmeasured state/scaling compositions remain fail-closed.
