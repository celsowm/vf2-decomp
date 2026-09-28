# `fa_coli` `0x225cc`: measured `0x0001f004` selector v0486

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, the reference executor was
stopped at `0x00022294` after setting fighter 0 `+0x1a4` to
`0x00400100`, fighter 1 `+0x1a4` to `0x0001f004`, and fighter 0 `+0x821`
to scan value 4.

The run returned in **217 instructions**. It follows the measured
bit16+bit13 high-selector route and has the same complete return state as the
existing `0x0001f000` and `0x0001f010` witnesses.

## Recovery boundary

Only the exact word `0x0001f004` is added to the existing scan-4 admissions.
No general bit-mask rule is inferred; neighboring unmeasured compositions
remain `VF2_ERROR_UNSUPPORTED`.

## Validation

`tests/recovered/test_coli_225cc_live.c` compares the reference and native
runs for exact instruction count, registers, condition state, procedure
state, call/return counts and mutable Model 2A memory.
