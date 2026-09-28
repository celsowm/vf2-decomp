# Positive masks `0x28`, `0x88`, `0xa0` and `0xa8` through threshold 3

The state-8 positive compositions `0x00000028`, `0x00000088`, `0x000000a0`
and `0x000000a8` are admitted through threshold `3` using their uniform
two-instruction dispatcher correction.

The controlled matrices cover the three measured fighter-record distributions,
both countdown values, both mode-bit-6 values, and thresholds `0` through `3`:
`192/192` exact against the reference, including CPU, condition, frame,
procedure, counters, and mutable Model 2A state. Threshold-4 controls remain
fail-closed (`0/12` native for each mask).

No unmeasured distribution or new hardware/object-field semantics are admitted.
