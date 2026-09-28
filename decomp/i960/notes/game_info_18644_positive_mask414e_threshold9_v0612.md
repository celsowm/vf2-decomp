# Positive mask `0x414e` through threshold 9

The state-8 positive composition `0x0000414e` was revalidated through the
full dispatcher at thresholds `0..9`. The matrix covers the three measured
fighter-record distributions, both countdown values and both mode-bit-6
values: `120/120` exact against the reference, including CPU, condition,
frame, procedure, counters, and mutable Model 2A state.

The native high-family predicate already admitted this mask; no runtime source
change was needed. The prior documentation describing `0x414e` as an
unsupported neighbor was stale and is corrected here. Other unlisted high-bit
compositions remain explicit unsupported boundaries.
