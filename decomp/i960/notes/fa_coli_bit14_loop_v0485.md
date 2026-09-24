# `fa_coli` `0x22298` bit-14 16-trip loop v0485

At the existing `0x22298` child witness, the reference executor was patched
to the measured loop gates:

```text
g7 + 0x1a4 = 0x00004000
g8 + 0x1a4 = 0x00000100
g7 + 0x61c = 1
g8 + 0x821 = 0
```

The ROM takes `0x222b4..0x2231c`, performs all sixteen table comparisons,
stores the resulting 16-bit mask at `g7 + 0x6dc`, and returns to `0x22214`.
The measured witness executes 138 instructions including `ret` and stores
`0xffff`; the corresponding no-selected-bit control executes 122 including
`ret`. The recovered body accounts for `121 + popcount(result)` instructions
and reproduces the signed comparison mask.

The native runtime test pins the full-mask witness at 138 instructions, one
return and `0xffff` storage. Admission is deliberately limited to
`0x61c == 1` and scan byte `0`; the alternate bit-14 branches and all other
loop gate values remain `VF2_ERROR_UNSUPPORTED`.
