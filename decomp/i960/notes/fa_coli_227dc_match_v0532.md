# v0532: `fa_coli` `0x227dc` type-5 match selector sweep

The measured bit-13/bit-3, scan-1 and `g7+0x844` bit-30 shape was rerun with
the main-data entries for selectors `g7+0x848 == 1..64` all pointing to the
same measured record `0x02014d6d`.  Only that record's type byte was changed
from type 2 to type 5, so `0x1ab34` returns the measured record
`0x02014d75` for every selector.

The reference takes the nine-instruction match tail for all 64 selectors and
returns to `0x22240` after 61 instructions.  Every case leaves
`g0 = 0x02014d75`, `g1 = 5`, `g7+0x198 = 0x1100000f` and the measured record
byte copied to `g7+0x822`; the native/reference snapshots compare equal for
CPU, condition state, procedure state and writable Model 2A memory.

The native gate now admits a nonzero selector only when the resolver returns
this measured record.  Selector 0, other records and other flag/scan
compositions remain fail-closed.
