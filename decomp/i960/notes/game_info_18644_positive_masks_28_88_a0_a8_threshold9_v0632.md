# Positive masks `0x28`, `0x88`, `0xa0` and `0xa8` through threshold 9

The state-8 positive compositions `0x00000028`, `0x00000088`, `0x000000a0`
and `0x000000a8` (nonempty subsets of bits 3, 5 and 7 without bit 8) were
extended from their measured threshold-3 slices to thresholds `0..9`. Each
complete matrix covers the three measured fighter-record distributions, both
countdown values and both mode-bit-6 values: `120/120` exact against the
reference for every mask, including CPU, condition, frame, procedure, counters,
and mutable Model 2A state.

The existing uniform `+2` dispatcher correction remains exact for all four
masks. Other unlisted positive compositions remain explicit unsupported
boundaries.
