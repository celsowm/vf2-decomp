# `fa_rob` `0x1453c/0x14570`: extended type-5 miss sweep v0528

The direct state-27 `0x1453c/0x14570` fixture was extended from the measured
selector range `1..512` to `1..1024`.

Starting from the restored `out/park-1442c.vf2snap` setup, the reference was
run for every selector `+0x194 == 513..1024` with fighter bases
`g7 = 0x00510980` and `g8 = 0x00512980`, stopping at `0x0001463c`.
Every case reaches the common return through one nested `0x1ab34` call and
return. The native body matches each reference instruction count and the full
live-state comparison (registers, condition state, procedure state and
mutable Model 2A memory).

The recovery therefore admits the fully measured type-5 selector interval
`1..1024`. Selectors outside that interval and unmeasured state/scaling
compositions remain explicitly fail-closed.
