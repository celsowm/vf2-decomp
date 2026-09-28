# Positive masks `0x106`, `0x112`, `0x114` and `0x116` through threshold 9

The state-8 positive compositions `0x00000106`, `0x00000112`, `0x00000114`
and `0x00000116` were extended from their measured threshold-3 slices to
thresholds `0..9`. Each complete matrix covers the three measured
fighter-record distributions, both countdown values and both mode-bit-6
values: `120/120` exact against the reference for every mask, including CPU,
condition, frame, procedure, counters, and mutable Model 2A state.

All four use the measured distribution-independent dispatcher accounting:
unilateral joins are `-3` at zero countdown and `+2` otherwise, while bilateral
joins are `-2` at zero countdown and `+3` otherwise. Other unlisted positive
compositions remain explicit unsupported boundaries.
