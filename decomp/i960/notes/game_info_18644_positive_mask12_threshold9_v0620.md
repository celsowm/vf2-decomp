# Positive mask `0x12` through threshold 9

The state-8 positive pair composition `0x00000012` (bits 1+4) was extended
from its measured threshold-3 slice to thresholds `0..9`. The complete matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.

The measured dispatcher correction remains `-3` instructions at zero countdown
and `+2` otherwise. No new distribution or hardware/object-field semantics
were introduced. Other unlisted positive compositions remain explicit
unsupported boundaries.
