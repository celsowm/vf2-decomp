# `fa_coli` `0x225cc` bit-16/bit-22-set continuation (v0452)

## Verdict

The measured scan-4 witness with `g8 + 0x1a4` bit 16 set and `g7 + 0x1a4`
equal to `0x00400100` is native through the bit-22-set `0x22d8c` g0=5
continuation. The ROM-backed live comparator proves complete state equality.

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, the reference mutation was:

```text
g8 + 0x1a4 = 0x00010000
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
```

The branch sequence is `0x22c88 bbs 16 -> 0x22d8c`, followed by the
bit-22-set continuation into the g0=5 `0x230d4` body. At `0x23144`, g8 bit
11 is clear, so the r3=40 branch reaches the shared tail without the r3=42
increment.

The reference runs 108 instructions to `0x22294`. The live result includes
`g0 = 0xeb`, `g8 + 0x198 = 0x0c0100eb`, and `g8 + 0x5de = 0xfff2`; the
native fixture matches registers, CC/AC, frames, counters, and mutable memory.
The direct entry requires the measured g7 flag word exactly and remains
fail-closed for other compositions.
