# Mask `0x188` threshold-3 unilateral slice

The state-8 positive mask `0x00000188` (bits 3+7+8) is admitted at threshold
`3` only for unilateral records with mode bit 6 clear. The measured dispatcher
correction is −9 instructions at zero countdown and +2 at nonzero countdown.

The filtered full-dispatch harness measures the four unilateral countdown
cases (`4/4` exact), including CPU, condition, frame, procedure, counters, and
mutable Model 2A state. Bilateral and mode-bit-6-set threshold-3 cases remain
explicit unsupported controls; no broader family admission is inferred.
