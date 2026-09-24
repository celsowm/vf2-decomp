# `fa_rob` `0x1453c` type-5 miss sweep v0490

The existing `0x1453c` state-27 live fixture was extended from the measured
`+0x194 == 1..128` type-5 selector range to `129..256`.

Starting from the fixture's `out/park-1442c.vf2snap` restore, each selector was
written to fighter0 `+0x194` with the direct state-27 shape and the reference
was stepped to `0x1463c`. Every selector in `129..256` reached the common
return with one nested `0x1ab34` call and return. The native recovery matches
the reference instruction count for every case and passes the existing full
live-state comparison (registers, condition state, procedure state and
mutable Model 2A memory).

The recovery therefore admits type-5 walker misses through selector `0x100`.
Selectors outside `1..256`, and unmeasured state/scaling compositions, remain
explicitly fail-closed.
