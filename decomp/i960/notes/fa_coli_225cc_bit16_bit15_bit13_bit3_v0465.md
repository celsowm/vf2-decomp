# `fa_coli` `0x225cc` bit-3/bit-13 scan-4 sibling (v0465)

## Verdict

The exact scan-4 g8 word `0x0001a008` is native for the measured g7 word
`0x00400100`. The ROM-backed live fixture matches 125 instructions and full
live state.

## Evidence

The witness uses:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
g8 + 0x1a4 = 0x0001a008
```

The g8 bit-3 outer route reaches the measured bit-13 `0x22744` branch,
continues through `0x22778`/`0x22914`, and then follows the existing common
scan-4 cascade and direct g0=5 tail. The exact word is admitted alongside
the v0462 bit-3 sibling; other bit-3/bit-13 compositions remain fail-closed.
