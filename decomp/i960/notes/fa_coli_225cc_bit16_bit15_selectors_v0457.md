# `fa_coli` `0x225cc` bit-15/bit-16 selector siblings (v0457)

## Verdict

Five additional g8 flag words are native on the measured scan-4/bit-16
continuation. The ROM-backed live fixture proves exact full live-state
equality for each word. Other bit-15 compositions remain fail-closed.

## Evidence

All witnesses use:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
g8 + 0x1a4 = measured word
```

Measured g8 words and reference instruction counts:

| g8 flags | instructions | observed selector |
| ---: | ---: | --- |
| `0x00018800` | 109 | bit 11 |
| `0x00019000` | 107 | bit 12 |
| `0x00019800` | 109 | bits 11+12 |
| `0x00018001` | 107 | low bit |
| `0x00018010` | 107 | bit 4 |

The bit-15 edge skips the `bbc 16` instruction before the scan-4 compare.
The bit-4 word also takes the measured direct g0=5 tail, whose native
accounting requires a two-instruction correction. Registers, condition state,
frames, counters and mutable memory match the reference for all five cases.

