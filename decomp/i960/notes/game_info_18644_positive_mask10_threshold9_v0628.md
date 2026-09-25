# Positive mask `0x10` through threshold 9

The isolated state-8 positive composition `0x00000010` (bit 4) was extended
from its measured threshold-3 slice to thresholds `0..9`. The complete matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.

The existing dispatcher split remains exact: mode bit 6 set with zero
countdown uses `-3` instructions, while all other accepted cases use `+2`.
Other unlisted positive compositions remain explicit unsupported boundaries.
