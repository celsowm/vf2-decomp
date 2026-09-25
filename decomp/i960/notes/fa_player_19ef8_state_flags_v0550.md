# v0556: measured `0x19ef8` entry state-flag compositions

The live `0x505` player corridor was extended from the zero-state entry to
the bounded nonzero `fighter + 0x1a4` matrix.  The witness starts from
`out/pre14288.vf2snap` at `0x14288`, with `g7 = 0x00510980`, and mutates only
the entry state word before the reference run.

Measured singleton deltas to the existing `1622`-instruction corridor:

| entry `+0x1a4` bit | total instructions | observed effect |
| --- | ---: | --- |
| 5 | 1631 | final `+0x1a4` is `0x280` (`0x200 | bit 7`) |
| 6 | 1627 | no additional final memory difference on this table shape |
| 21 | 1634 | final player word toggles bit 6 (`0x800` → `0x840`) |
| 23 | 1624 | global `0x0050a010` is copied to player `+0x1c` |

All 15 nonzero combinations of these four bits were also run.  In addition,
each of the other 28 individual state bits was measured as a singleton, and
each was composed with every one of the 16 subsets of the four branch bits.
The resulting 448 mixed cases all reached `0x1428c` and matched full state.
The new two-bit family also reaches `0x1428c` in all 6048 cases with full
state equality. The branch-free triple family adds 3276 more exact cases; the
focused fixture now runs 9818 accepted cases in total, plus a four-bit
negative control, including the three existing zero-state ROM parks.

The native corridor admits every measured singleton, any combination made
solely from bits 5, 6, 21 and 23, and every measured combination containing
one or two additional non-branch bits. The new two-bit family contains 378
pairs × 16 branch subsets = 6048 full live-state comparisons. The branch-free
three-bit family adds 3276 exact comparisons. Words with four or more
non-branch bits, and triples combined with branch bits, remain
`VF2_ERROR_UNSUPPORTED`. The measured live selector, table stream and
downstream scratch/census guards are unchanged.
