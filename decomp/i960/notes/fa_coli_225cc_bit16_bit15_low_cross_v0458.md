# `fa_coli` `0x225cc` bit-15/bit-16 low-selector cross-product (v0458)

## Verdict

Eleven additional g8 flag words are native on the measured scan-4/bit-16
continuation. Together with v0456 and v0457, the fixture proves 17 exact
bit-15/bit-16 words with full live-state equality. Unmeasured compositions
remain fail-closed.

## Evidence

All witnesses use:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
g8 + 0x1a4 = measured word
```

The additional words are:

```text
0x00018004  0x00018005
0x00018801  0x00018804  0x00018805
0x00019001  0x00019004  0x00019005
0x00019801  0x00019804  0x00019805
```

Reference instruction counts are 107 for the `0x180xx`/`0x190xx` words and
109 for the `0x188xx`/`0x198xx` words. The values were first enumerated by
ROM probe and then checked by the live reference/native comparator, including
registers, condition state, frames, counters and mutable memory.

