# Positive masks `0x20` and `0x80` through threshold 9

The isolated state-8 positive compositions `0x00000020` (bit 5) and
`0x00000080` (bit 7) were extended from their measured threshold-3 slices to
thresholds `0..9`. Each complete matrix covers the three measured
fighter-record distributions, both countdown values and both mode-bit-6
values: `120/120` exact against the reference for each mask, including CPU,
condition, frame, procedure, counters, and mutable Model 2A state.

The existing uniform `+2` dispatcher correction remains exact for both masks.
Other unlisted positive compositions remain explicit unsupported boundaries.
