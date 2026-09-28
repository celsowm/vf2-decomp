# `fa_coli` `0x225cc`: bit16+bit13 high selectors v0471

## Evidence

The reference executor was driven from `out/coli-225cc-entry.vf2snap` with
`g7 + 0x1a4 = 0x00400100` and `g7 + 0x821 = 4`.

| `g8 + 0x1a4` | instructions |
| --- | ---: |
| `0x00012800` | 218 |
| `0x00013000` | 218 |
| `0x00016000` | 218 |
| `0x00012808` | 128 |
| `0x00013008` | 126 |
| `0x00016008` | 126 |
| `0x00012810` | 218 |
| `0x00013010` | 218 |
| `0x00016010` | 218 |

Every run stopped at `0x00022294`. The native fixture compares complete CPU
state, condition state, procedure state and mutable Model 2A memory.

## Recovery boundary

Only these nine exact words are admitted. The six standard selectors reuse the
measured bit16+bit13 body. The three bit3 siblings reuse its exact bit3 gate;
`0x16008` carries a separately measured three-instruction accounting
correction. Neighboring compositions remain `VF2_ERROR_UNSUPPORTED`.

## Validation

`vf2_coli_225cc_live_tests.exe roms\\vf2` passes all cases with full
live-state equality.
