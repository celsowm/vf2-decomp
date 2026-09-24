# `fa_coli` `0x225cc` bit-16/bit-22 selector combinations (v0454)

## Verdict

The scan-4/bit-16 continuation is native for all four measured combinations
of the g7 bit-4 and bit-12 selectors when g7 bit 22 is set. The ROM-backed
fixture proves full live-state equality for the newly measured bit-4 paths.

## Evidence

Starting from `out/coli-225cc-entry.vf2snap`, each case used:

```text
g8 + 0x1a4 = 0x00010000
g7 + 0x821 = 4
```

The newly measured g7 flag words were:

```text
0x00400010  -> 110 instructions  (bit 4 set, bit 12 clear)
0x00401000  -> 108 instructions  (bit 4 clear, bit 12 set)
0x00401010  -> 122 instructions  (bits 4 and 12 set)
```

Together with the v0453 bit-4-clear witnesses, these cover the four selector
combinations. The result remains the bit-22-set g0=5 continuation, with the
same `g0 = 0xeb`, `g8 + 0x198 = 0x0c0100eb`, and `g8 + 0x5de = 0xfff2`
postconditions. Other g7 flag bits are not read on this route; downstream
data-dependent branches remain guarded by their existing fail-closed checks.
