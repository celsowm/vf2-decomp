# `fa_game_info` positive mask `0x14` through threshold 9

## Scope

This note records the measured extension of the state-8 positive composition
`0x00000014` (bits 2+4) at `0x00018644`. It uses the existing measured
mixed-low corridor. No unmeasured fighter-record distribution is admitted.

## Evidence

The controlled `0x164ac` scenarios use the three measured fighter-record
distributions, both countdown values, both mode-bit-6 values, and thresholds
`0` through `9`. The 120-case matrix matches the reference CPU state,
condition state, local frames, procedure state, counters, and mutable Model 2A
state. The threshold-9 slice is `12/12` exact.

The threshold-10 control remains fail-closed at the original i960 boundary
`0x0001645c` (`0/12` native), so the extension is limited to threshold `9`.

No new hardware or object-field semantics are inferred here.
