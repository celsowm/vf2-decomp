# Positive mask `0x104` through threshold 9

The state-8 positive pair composition `0x00000104` (bits 2+8) was extended
from its measured threshold-3 slice to thresholds `0..9`. The complete matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.

The existing distribution-independent dispatcher correction remains exact:
unilateral joins use `-3` at zero countdown and `+2` otherwise, while bilateral
joins use `-2` at zero countdown and `+3` otherwise. Other unlisted positive
compositions remain explicit unsupported boundaries.
