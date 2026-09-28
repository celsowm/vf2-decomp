# `fa_coli` `0x225cc` bit-3 scan-4 sibling (v0462)

## Verdict

The exact scan-4 g8 word `0x00018008` is native for the measured g7 word
`0x00400100`. The ROM-backed live fixture matches 107 instructions and full
live state.

## Evidence

The witness uses:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
g8 + 0x1a4 = 0x00018008
```

The bit-3 outer gate now admits this one scan-4 word into the existing long
body. At `0x22b10`, g8 bit 16 skips the `bbc 14`/mask branch and reaches the
common `0x22bc0` path; the measured word also has a set `g8+0x804` bit 8,
which is not read on that reference edge. The native path therefore keeps
that field-bit exception exact and leaves other bit-3 compositions
fail-closed. The reference returns at `0x22294` in 107 instructions with
registers, condition state, frames, counters and mutable memory equal.
