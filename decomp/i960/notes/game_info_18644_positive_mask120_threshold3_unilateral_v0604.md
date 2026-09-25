# Mask `0x120` threshold-3 unilateral slice

The state-8 positive mask `0x00000120` (bits 5+8) is admitted at threshold
`3` only for unilateral records with mode bit 6 clear. The measured dispatcher
correction is −9 instructions at zero countdown and +2 at nonzero countdown.

Using the filtered full-dispatch harness, the four-case matrix (fighter 0 or
fighter 1, countdown 0 or 1) is `4/4` exact against the reference, including
CPU, condition, frame, procedure, counters, and mutable Model 2A state.
Threshold-3 bilateral and mode-bit-6-set cases remain explicit unsupported
controls; no broader family admission is inferred.
