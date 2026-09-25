# Masks `0x1a0` and `0x1a8` threshold-3 unilateral slices

The state-8 positive masks `0x000001a0` (bits 5+7+8) and `0x000001a8`
(bits 3+5+7+8) are admitted at threshold `3` only for unilateral records with
mode bit 6 clear. The measured dispatcher correction is −9 instructions at
zero countdown and +2 at nonzero countdown.

The filtered full-dispatch harness measures four unilateral countdown cases
for each mask: `4/4` exact against the reference, including CPU, condition,
frame, procedure, counters, and mutable Model 2A state. Bilateral and
mode-bit-6-set threshold-3 cases remain explicit unsupported controls; no
broader family admission is inferred.
