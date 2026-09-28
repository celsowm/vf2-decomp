# `fa_coli` `0x225cc` bit-16/bit-22 flag family (v0453)

## Verdict

The scan-4 witness with `g8 + 0x1a4` bit 16 set is native for the measured
`g7 + 0x1a4` mask family where bit 22 is set and bit 4 is clear. The ROM
fixture proves complete live-state equality for the existing `0x00400100`
case and four additional flag compositions.

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, each probe used:

```text
g8 + 0x1a4 = 0x00010000
g7 + 0x821 = 4
```

Measured `g7 + 0x1a4` values were:

```text
0x00400000  -> 108 instructions
0x00400001  -> 108 instructions
0x00400100  -> 108 instructions
0x00410100  -> 108 instructions
0x00c00100  -> 108 instructions
```

All five cases reach `0x22294` with the same observed result as the v0452
witness: `g0 = 0xeb`, `g8 + 0x198 = 0x0c0100eb`, and `g8 + 0x5de = 0xfff2`.
The long body only tests g7 bit 4 before the shared bit-22 join tests bit 22,
so the recovery admits the measured mask predicate rather than enumerating the
individual words. Bit-4-set and other unmeasured compositions remain
fail-closed.
