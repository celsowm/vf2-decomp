# `fa_game_info` positive mask `0x12` through threshold 3

The state-8 positive pair composition `0x00000012` (bits 1+4) is admitted
through threshold `3`. The measured dispatcher split is −3 instructions at
zero countdown and +2 otherwise.

The controlled `0x164ac` matrix covers the three measured fighter-record
distributions, both countdown values, both mode-bit-6 values, and thresholds
`0` through `3`: `48/48` exact against the reference, including CPU,
condition, frame, procedure, counters, and mutable Model 2A state. The
threshold-4 control remains fail-closed (`0/12` native).

No unmeasured distribution or new hardware/object-field semantics are admitted.
