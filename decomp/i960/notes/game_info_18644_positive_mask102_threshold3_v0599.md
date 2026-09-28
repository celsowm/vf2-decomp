# `fa_game_info` positive mask `0x102` through threshold 3

The state-8 positive composition `0x00000102` (bits 1+8) is admitted through
threshold `3` using its measured joins. Bilateral records are two instructions
short at zero countdown and three long at nonzero countdown; fighter-0-only is
three short only with mode bit 6 plus zero countdown; fighter-1-only is one
long in that case; all other joins are two long.

The controlled `0x164ac` matrix covers the three measured fighter-record
distributions, both countdown values, both mode-bit-6 values, and thresholds
`0` through `3`: `48/48` exact against the reference, including CPU,
condition, frame, procedure, counters, and mutable Model 2A state. The
threshold-4 control remains fail-closed (`0/12` native).

No unmeasured distribution or new hardware/object-field semantics are admitted.
