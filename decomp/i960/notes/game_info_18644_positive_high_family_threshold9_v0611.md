# Positive bit-6/bit-14 corridors through threshold 9

The existing positive state-8 corridors `0x00000142`, `0x00004142`,
`0x00008142`, `0x00010142` and `0x00004146` were revalidated through
threshold `9` using the unchanged native high-family predicate.

Each matrix covers the three measured fighter-record distributions, both
countdown values, both mode-bit-6 values, and thresholds `0` through `9`:
`120/120` exact against the reference, including CPU, condition, frame,
procedure, counters, and mutable Model 2A state. No source widening was
needed; this note corrects the documented frontier to match measured behavior.

Unlisted high-bit compositions remain explicit unsupported boundaries.
