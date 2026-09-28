# `fa_coli` `0x225cc`: selector cross-products v0473

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, the reference executor was
stopped at `0x00022294` after setting fighter 0 `+0x1a4` to
`0x00400100`, fighter 1 `+0x1a4` to each selector below, and fighter 0
`+0x821` to scan value 4.

| fighter 1 `+0x1a4` | reference instructions |
| --- | ---: |
| `0x00014001` | 108 |
| `0x00014004` | 108 |
| `0x00014010` | 108 |
| `0x00014020` | 108 |
| `0x00014040` | 108 |
| `0x00014080` | 108 |
| `0x00014808` | 110 |
| `0x00014810` | 110 |
| `0x00015008` | 108 |
| `0x00015010` | 108 |
| `0x00015808` | 110 |
| `0x00015810` | 110 |
| `0x0001d008` | 107 |
| `0x0001d010` | 107 |
| `0x0001f008` | 125 |
| `0x0001f010` | 217 |

The bit-14 selectors use the existing direct-tail accounting shape. The
bit-4 words `0x14010`, `0x14810`, `0x15010`, `0x15810` and `0x1d010`
retain the generic two-instruction subtraction and require a measured
five-instruction correction; their bit-3 or bit-11 siblings use the measured
three-instruction correction. No neighboring unmeasured word is admitted.

## Acceptance

`tests/recovered/test_coli_225cc_live.c` compares all sixteen words against
the reference with exact instruction counts, registers, condition state,
frames, call/return counts and mutable Model 2A memory.
