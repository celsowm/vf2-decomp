# `fa_coli` `0x225cc` bit-15/bit-16 bit-4 cross-product (v0459)

## Verdict

Fifteen additional g8 flag words with bit 4 set are native on the measured
scan-4/bit-16 continuation. Along with v0456-v0458, the live fixture now
proves 32 exact words with full CPU, condition-state, frame, counter and
mutable-memory equality. Other compositions remain fail-closed.

## Evidence

All witnesses use `g7 + 0x1a4 = 0x00400100`, `g7 + 0x821 = 4`. The new
g8 words are:

```text
0x00018011  0x00018014  0x00018015
0x00018810  0x00018811  0x00018814  0x00018815
0x00019010  0x00019011  0x00019014  0x00019015
0x00019810  0x00019811  0x00019814  0x00019815
```

They match the ROM at 107 instructions for the `0x180xx`/`0x190xx` words
and 109 for the `0x188xx`/`0x198xx` words. The bit-4 selector uses the
measured two-instruction correction in the direct g0=5 tail.

