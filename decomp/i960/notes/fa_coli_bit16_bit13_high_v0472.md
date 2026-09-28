# `fa_coli` `0x225cc`: additional bit16 scan-4 selectors v0472

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, the reference executor was
stopped at `0x00022294` after setting fighter 0 `+0x1a4` to
`0x00400100`, fighter 1 `+0x1a4` to the selector below, and fighter 0
`+0x821` to scan value 4.

| fighter 1 `+0x1a4` | reference instructions |
| --- | ---: |
| `0x00014000` | 108 |
| `0x00014008` | 108 |
| `0x00014800` | 110 |
| `0x00015000` | 108 |
| `0x00015800` | 110 |
| `0x00016800` | 218 |
| `0x00017000` | 218 |
| `0x00017800` | 218 |
| `0x0001d000` | 107 |
| `0x0001f000` | 217 |

The native implementation initially under-counted the first five words and
`0x1d000` by three instructions. The six-word set
`0x14000/0x14008/0x14800/0x15000/0x15800/0x1d000` therefore receives the
measured direct-tail `+3` correction. The reference leaves `g0 = 0xee` for
`0x16800`, `0x17000` and `0x17800`; those words use the existing measured
bit-13 result propagation.

## Acceptance

`tests/recovered/test_coli_225cc_live.c` compares all ten words against the
reference with exact instruction counts, registers, condition state, frames,
call/return counts and mutable Model 2A memory.
