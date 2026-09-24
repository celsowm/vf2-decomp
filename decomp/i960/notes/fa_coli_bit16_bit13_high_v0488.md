# `fa_coli` `0x225cc`: low-bit sweep of `0x0001f000` selectors v0488

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, the reference executor was
stopped at `0x00022294` with fighter 0 `+0x1a4 = 0x00400100` and scan value
4. The following exact fighter-1 selector words were measured:

| selector words | reference instructions |
| --- | ---: |
| `0x0001f002`, `0x0001f003`, `0x0001f006`, `0x0001f007` | 217 |
| `0x0001f00a`, `0x0001f00b`, `0x0001f00e`, `0x0001f00f` | 125 |
| `0x0001f012`, `0x0001f013`, `0x0001f016`, `0x0001f017` | 217 |
| `0x0001f018`, `0x0001f019`, `0x0001f01a`, `0x0001f01b` | 125 |

The 217-step words retain the measured `g0 = 0xee` result propagation. The
bit-3 words use the direct-tail correction; the final four bit-3/bit-4 words
also require a separately measured two-instruction correction.

## Recovery boundary

Only these sixteen exact words are admitted. No general low-bit mask rule is
inferred; neighboring unmeasured compositions remain
`VF2_ERROR_UNSUPPORTED`.

## Validation

`tests/recovered/test_coli_225cc_live.c` compares each reference and native
run for exact instruction count, registers, condition state, procedure state,
call/return counts and mutable Model 2A memory.
