# Positive mask `0x110` through threshold 9

The state-8 positive pair composition `0x00000110` (bits 4+8) was extended
from its measured threshold-3 slice to thresholds `0..9`. The complete matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.

The existing distribution-specific bit-8 dispatcher correction remains exact,
including the bilateral mode-bit-6/countdown case and the unilateral `-3/+1`
split. Other unlisted positive compositions remain explicit unsupported
boundaries.
