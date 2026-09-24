# `fa_player` `0x1453c/0x14570`: measured type-5 walker misses v0474

## Evidence

The reference executor was driven from `out/park-1442c.vf2snap` at
`0x00014528` with the direct state-27 shape, fighter bases
`g7 = 0x00510980` and `g8 = 0x00512980`, and the type-5 selector in
`g7 + 0x194` varied through the ROM-backed table. The two selected miss
values below both return `g0 = 0` from `0x1ab34` and continue through the
`0x1457c` record loads to the common `0x1463c` return.

| `g7 + 0x194` | reference instructions | calls / returns |
| ---: | ---: | ---: |
| `0x0001` | 62 | 1 / 1 |
| `0x0002` | 69 | 1 / 1 |

The native `0x1453c` body already models the measured zero-record data path;
the recovery now admits only these two measured miss selectors. The existing
`0x0110` and `0x02cf` miss witnesses remain admitted, while every other
unmeasured miss continues to return `VF2_ERROR_UNSUPPORTED`.

## Acceptance

`tests/recovered/test_player_1453c_live.c` compares both new cases against the
reference for exact instruction counts, registers, condition state, frames,
call/return counters and mutable Model 2A memory.
