# v0550: measured `0x19ef8` entry state-flag siblings

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

All 15 nonzero combinations of these four bits were also run.  Their
instruction deltas are additive and all reference/native CPU, condition,
frame, procedure-counter and Model 2A state comparisons are byte-exact.  The
focused fixture runs 18 cases in total, including the three existing
zero-state ROM parks.

The native corridor admits only bits 5, 6, 21 and 23 in the incoming state
word.  Every other nonzero state bit remains `VF2_ERROR_UNSUPPORTED`; the
measured live selector, table stream and downstream scratch/census guards are
unchanged.
