# Positive mask `0x100` through threshold 9

The isolated state-8 positive composition `0x00000100` (bit 8) was extended
from its measured threshold-3 slice to thresholds `0..9`. The complete matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.

The existing distribution-specific dispatcher accounting remains exact:
bilateral joins use `-2` at zero countdown and `+3` otherwise; the affected
unilateral mode-bit-6 split uses `-3`/`+1`, and other joins use `+2`. Other
unlisted positive compositions remain explicit unsupported boundaries.
