# `fa_coli` scan-5 `field_0822 = 16` neighbors

The whole-task fixture starts from `out/coli-parked-221e8.vf2snap`, sets
fighter 0 `+0x820 = 0`, `+0x821 = 5`, `+0x822 = 16`, and compares the native
dispatcher with the reference executor through `0x10dcc`.

Two measured orientations are covered:

- fighter 0 bit 8 set: `9391/17/18`;
- fighter 1 bit 8 set: `9385/17/18`.

Both cases match CPU state, condition state, local frames, counters and mutable
Model 2A memory. The extension is a measured matrix entry; other
`field_0822` values and neighboring branches remain fail-closed until probed.
