# `fa_coli` `0x225cc`: additional `0x0001fxxx` selectors v0487

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, the reference executor was
stopped at `0x00022294` with fighter 0 `+0x1a4 = 0x00400100` and scan value
4. The following exact fighter-1 selector words were measured:

| fighter 1 `+0x1a4` | reference instructions |
| --- | ---: |
| `0x0001f001` | 217 |
| `0x0001f005` | 217 |
| `0x0001f009` | 125 |
| `0x0001f00c` | 125 |
| `0x0001f011` | 217 |
| `0x0001f014` | 217 |
| `0x0001f015` | 217 |
| `0x0001f020` | 217 |

The six 217-step words retain the measured `g0 = 0xee` result propagation.
The two bit-3 words use the existing direct-tail route with its measured
three-instruction correction.

## Recovery boundary

Only these eight exact words are admitted. No general `0x1fxxx` mask rule is
inferred; neighboring unmeasured compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Validation

`tests/recovered/test_coli_225cc_live.c` compares each reference and native
run for exact instruction count, registers, condition state, procedure state,
call/return counts and mutable Model 2A memory.
