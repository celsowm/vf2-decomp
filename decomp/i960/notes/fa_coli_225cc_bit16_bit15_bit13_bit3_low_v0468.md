# `fa_coli` `0x225cc`: bit-3/bit-13 low selectors v0468

## Evidence

The reference executor was driven from `out/coli-225cc-entry.vf2snap` with
`g7 + 0x1a4 = 0x00400100`, `g7 + 0x821 = 4`, and these exact `g8 + 0x1a4`
values:

`0x0001a009`, `0x0001a00c`, `0x0001a018`, `0x0001a028`,
`0x0001a048`, `0x0001a088`, `0x0001a208`, `0x0001a408`.

Every reference run stopped at `0x00022294` after 125 instructions with the
same call/return shape as the existing bit-3/bit-13 direct-tail corridor.

## Recovery boundary

These eight exact words are admitted to the already recovered bit-3/bit-13
long body. No new downstream semantic branch was required: the measured
selectors share the 125-instruction route. Neighboring compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Validation

`vf2_coli_225cc_live_tests.exe roms\\vf2` passes all cases, including full
live-state equality for these eight witnesses.
