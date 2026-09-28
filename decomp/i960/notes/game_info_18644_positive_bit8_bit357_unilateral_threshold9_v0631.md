# Bit-8 plus bit-3/5/7 unilateral slices through threshold 9

The measured unilateral, mode-bit-6-clear slices for masks `0x00000108`,
`0x00000120`, `0x00000128`, `0x00000180`, `0x00000188`, `0x000001a0` and
`0x000001a8` were extended from threshold 3 to thresholds `0..9`. Each mask
passed `40/40` exact cases: both unilateral fighter distributions, both
countdown values and all ten thresholds, including CPU, condition, frame,
procedure, counters, and mutable Model 2A state.

The correction remains `-9` at zero countdown and `+2` otherwise. Bilateral,
mode-bit-6-set, and unlisted distributions remain explicit unsupported
boundaries.
