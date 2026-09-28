# Positive mask `0xc14e` through threshold 9

The state-8 positive composition `0x000c14e` was revalidated through the full
dispatcher at thresholds `0..9`: `120/120` exact across the three measured
fighter-record distributions, both countdown values and both mode-bit-6
values, including CPU, condition, frame, procedure, counters, and mutable
Model 2A state.

The existing high-family predicate already admitted this mask; no runtime
source change was needed. Other unlisted high-bit compositions remain explicit
unsupported boundaries.
