# `fa_coli` `0x225cc` bit-13 scan-4 sibling (v0463)

## Verdict

The exact scan-4 g8 word `0x0001a000` is native for the measured g7 word
`0x00400100`. The ROM-backed live fixture matches 217 instructions and full
live state.

## Evidence

The witness uses:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
g8 + 0x1a4 = 0x0001a000
```

The scan-4 path takes g8 bit 13 with bits 15 and 16 set, then follows the
measured bit-13 branch through `0x22744`/`0x22808` and the `0x22848` join.
The existing long-body recovery matches the memory effects, calls, returns,
condition state and instruction count; the measured `0x230d4`/`0x23238`
result remains visible as final `g0 = 0x000000ee`. The exact word is admitted
only in the scan-4 gate; other bit-13 compositions remain fail-closed.
