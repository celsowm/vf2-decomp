# Positive masks `0x18140` and `0x1c140` through threshold 9

The state-8 positive compositions `0x00018140` and `0x0001c140` were
revalidated through the full dispatcher at thresholds `0..9`. Each matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference for each
mask, including CPU, condition, frame, procedure, counters, and mutable Model
2A state.

The existing high-family predicate already admitted these masks; no runtime
source change was needed. Other unlisted high-bit compositions remain explicit
unsupported boundaries.
