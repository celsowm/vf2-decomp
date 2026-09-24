# `fa_rob` `0x14640` type-15 miss sweep v0492

The state-27 `0x14640` fixture was extended from the measured type-15 selector
range `1..256` to `1..512`. Selectors `257..512` were written to fighter
`+0x194` from the restored live snapshot and every reference run reached
`0x146c4` through the zero-record arithmetic tail with one nested
`0x1ab34` call and return.

The native path matches the reference instruction count and complete live
state for every new case. Selectors outside `1..512` remain fail-closed.
