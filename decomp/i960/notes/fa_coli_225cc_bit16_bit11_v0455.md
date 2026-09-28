# `fa_coli` `0x225cc` bit-16/bit-11 continuation (v0455)

## Verdict

The scan-4/bit-16 path now admits the measured g8 selector family using bits
11 and 12. The ROM-backed fixture proves full live-state equality for the
new r3=42 continuation and the bit-12 sibling.

## Evidence

Each probe used:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
```

Measured g8 flag words were:

```text
0x00010800  -> 110 instructions  (bit 11 set)
0x00011000  -> 108 instructions  (bit 12 set)
0x00011800  -> 110 instructions  (bits 11 and 12 set)
```

The existing `0x00010000` witness supplies the all-clear selector case. Bit
11 selects the g0=5 r3=42 table index; bit 12 leaves the measured upstream
branch shape unchanged. All cases match the existing postconditions
`g0 = 0xeb`, `g8 + 0x198 = 0x0c0100eb`, and `g8 + 0x5de = 0xfff2`, including
full registers, CC/AC, frames, counters, and mutable memory. Other g8 bits
remain fail-closed.
