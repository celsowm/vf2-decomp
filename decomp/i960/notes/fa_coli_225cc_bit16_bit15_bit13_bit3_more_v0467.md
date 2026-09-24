# `fa_coli` `0x225cc`: bit-3/bit-13 scan-4 compositions v0467

## Evidence

From `out/coli-225cc-entry.vf2snap`, the reference executor was driven with
`g7 + 0x1a4 = 0x00400100`, `g7 + 0x821 = 4`, and the following exact
`g8 + 0x1a4` values:

| flags | reference instructions | measured route |
| --- | ---: | --- |
| `0x0001a108` | 121 | compact bit-8 route at `0x22a28` |
| `0x0001a808` | 127 | `g8+0x804` bit-8 route |
| `0x0001b008` | 125 | `g8+0x804` bit-8 route |
| `0x0001e008` | 125 | `g8+0x804` bit-8 route plus bit-14 direct tail |

Each reference run stopped at `0x00022294` and reached the common direct
`g0=5` tail. The native fixture compares registers, condition state, frames,
instruction count and mutable Model 2A state.

## Recovery boundary

Only these four exact words are admitted. `0x1a108` is admitted to the
existing compact bit-8 implementation and receives its measured nine-step
accounting correction. The other three are admitted to the existing
`+0x804` bit-8 continuation; `0x1e008` receives the measured three-step
bit-14 direct-tail correction. Neighboring bit-3/bit-13 compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Validation

`vf2_coli_225cc_live_tests.exe roms\\vf2` passes all cases, including full
live-state equality for these four witnesses.
