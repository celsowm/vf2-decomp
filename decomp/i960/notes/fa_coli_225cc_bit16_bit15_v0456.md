# `fa_coli` `0x225cc` scan-4 bit-15/bit-16 sibling (v0456)

## Verdict

The measured g8 flag word `0x00018000` is native on the scan-4/bit-16
continuation. The ROM-backed fixture proves exact full live-state equality.

## Evidence

The witness used:

```text
g7 + 0x1a4 = 0x00400100
g8 + 0x1a4 = 0x00018000
g7 + 0x821 = 4
```

At the initial scan gate, g8 bit 15 takes the `bbs 15` edge and skips the
`bbc 16` instruction; the scan-4 compare then continues into the same long
body. The reference reaches `0x22294` in 107 instructions, one fewer than
the bit-15-clear 108-instruction sibling. The postconditions remain
`g0 = 0xeb`, `g8 + 0x198 = 0x0c0100eb`, and `g8 + 0x5de = 0xfff2`, with
registers, CC/AC, frames, counters, and mutable memory equal.

Only this measured bit-15 flag word is admitted; other bit-15 compositions
remain fail-closed.
