# `fa_coli` `0x225cc`: bit16+bit13 scan-4 selectors v0470

## Evidence

The reference executor was driven from `out/coli-225cc-entry.vf2snap` with
`g7 + 0x1a4 = 0x00400100` and `g7 + 0x821 = 4`.

| `g8 + 0x1a4` | instructions | route |
| --- | ---: | --- |
| `0x00012000` | 218 | bit16+bit13 long body |
| `0x00012001` | 218 | bit16+bit13 long body |
| `0x00012004` | 218 | bit16+bit13 long body |
| `0x00012008` | 126 | bit3+bit16+bit13 sibling |
| `0x00012010` | 218 | bit16+bit13 long body |
| `0x00012020` | 218 | bit16+bit13 long body |
| `0x00012040` | 218 | bit16+bit13 long body |
| `0x00012080` | 218 | bit16+bit13 long body |

Every reference run stopped at `0x00022294`; the fixture compares full live
CPU state and mutable Model 2A memory.

## Recovery boundary

Only the eight measured words are admitted. The seven standard selectors use
the existing bit16+bit13 body; `0x12008` uses the exact bit3 sibling gate.
Unmeasured bit16+bit13 compositions remain `VF2_ERROR_UNSUPPORTED`.

## Validation

`vf2_coli_225cc_live_tests.exe roms\\vf2` passes all cases with full
live-state equality.
