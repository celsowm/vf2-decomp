# `fa_game_info` positive mask `0x100` through threshold 3

The isolated state-8 positive composition `0x00000100` (bit 8) is admitted
through threshold `3` using its measured distribution-specific dispatcher
join: bilateral records are three instructions short except for mode bit 6
with zero countdown, where they are two short; fighter-0-only is three long
only in that case; fighter-1-only is one short in that case; all other joins
are two short.

The controlled `0x164ac` matrix covers the three measured fighter-record
distributions, both countdown values, both mode-bit-6 values, and thresholds
`0` through `3`: `48/48` exact against the reference, including CPU,
condition, frame, procedure, counters, and mutable Model 2A state. The
threshold-4 control remains fail-closed (`0/12` native).

No unmeasured distribution or new hardware/object-field semantics are admitted.
