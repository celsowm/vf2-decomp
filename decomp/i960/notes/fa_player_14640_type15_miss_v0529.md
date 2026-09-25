# `fa_rob` `0x14640`: extended type-15 miss sweep v0529

The state-27 `0x14640` fixture was extended from the measured type-15
selector range `1..512` to `1..1024`.

Starting from the restored `out/park-1442c.vf2snap` setup, the reference was
run for every selector `+0x194 == 513..1024` with fighter base
`g7 = 0x00510980`, state byte `+0x197 = 27`, and board bit 20 clear,
stopping at `0x000146c4`. Every case reaches the return through one nested
`0x1ab34` call and return. The native body matches each reference instruction
count and the full live-state comparison (registers, condition state,
procedure state and mutable Model 2A memory).

The recovery therefore admits the fully measured type-15 selector interval
`1..1024`. Selectors outside that interval and unmeasured state/flag
compositions remain explicitly fail-closed.
