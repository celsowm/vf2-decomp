# `fa_coli` `0x225cc`: bit-13-only scan-4 shapes v0469

## Evidence

From `out/coli-225cc-entry.vf2snap`, the reference executor was driven with
`g7 + 0x1a4 = 0x00400100`, `g7 + 0x821 = 4`, and exact `g8 + 0x1a4`
values `0x00002000`, `0x00002001`, `0x00002010` and `0x0000a000`.

All four runs stopped at `0x00022294` after 217 instructions with the
bit-13 `0x22744`/`0x22808`/`0x22848` route and the same measured call/return
shape as the existing `0x1a000` corridor.

## Recovery boundary

The four exact words are admitted to the measured scan-4 long body. The
outer gate now recognizes these exact non-bit16 shapes; no broad bit mask is
introduced. Neighboring bit13-only compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Validation

`vf2_coli_225cc_live_tests.exe roms\\vf2` passes all cases with full
register, condition-state, frame and mutable-memory equality.
