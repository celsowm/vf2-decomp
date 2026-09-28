# `fa_game_info` positive state-8 bit-1/bit-2/bit-6 medium families v0483

The calibrated `0x164ac` boundary was swept through both `0x18644` child
calls and the scheduler return for the eight measured masks:

```text
0x0142  state bit 8 + bits 1 and 6
0x0146  state bit 8 + bits 1, 2 and 6
0x4142  state bit 8 + bits 1, 6 and 14
0x4146  state bit 8 + bits 1, 2, 6 and 14
0x8142  state bit 8 + bits 1, 6 and 15
0x8146  state bit 8 + bits 1, 2, 6 and 15
0x10142 state bit 8 + bits 1, 6 and 16
0x10146 state bit 8 + bits 1, 2, 6 and 16
```

For each mask, the distributions `(mask,0)`, `(0,mask)` and `(mask,mask)`,
countdown `0/1`, mode bit 6 clear/set and positive thresholds `0/1/2` were
validated. The existing `0x0142` corridor and the newly admitted `0x0146`
family remained exact; the six high-bit variants were `36/36` exact each,
including complete CPU, Model 2A memory and counter state (`288/288` overall).

The eight families share the measured positive medium-bit admission predicate.
The high-bit variants with bit 2 require mask-local second-dispatcher
instruction-count corrections; no call, return, condition, memory or
device-visible state differences were observed.

The measured bit-4 neighbor `0x414e` remains fail-closed at `0x00018644`.
