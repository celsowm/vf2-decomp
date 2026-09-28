# Positive mask `0x06` through threshold 9

The state-8 positive pair composition `0x00000006` (bits 1+2) was extended
from its measured threshold-3 slice to thresholds `0..9`. The complete matrix
covers the three measured fighter-record distributions, both countdown values
and both mode-bit-6 values: `120/120` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.

The reference remains uniformly two instructions longer than the recovered
child on this path; the existing native `+2` dispatcher correction therefore
applies unchanged. No new distribution or hardware/object-field semantics were
introduced. Other unlisted positive compositions remain explicit unsupported
boundaries.
