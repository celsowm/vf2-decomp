# `fa_game_info` positive state-8 bit-1/bit-6 medium families v0482

The calibrated `0x164ac` boundary was swept through both `0x18644` child
calls and the scheduler return for the four measured masks:

```text
0x0142  state bit 8 + bits 1 and 6
0x4142  state bit 8 + bits 1, 6 and 14
0x8142  state bit 8 + bits 1, 6 and 15
0x10142 state bit 8 + bits 1, 6 and 16
```

For each mask, the distributions `(mask,0)`, `(0,mask)` and `(mask,mask)`,
countdown `0/1`, mode bit 6 clear/set and positive thresholds `0/1/2` were
validated. The existing `0x0142` corridor remained exact; the two new masks
were `36/36` exact each, including complete CPU, Model 2A memory and counter
state (`144/144` overall).

The `0x4142`, `0x8142` and `0x10142` families share the measured positive
medium-bit admission predicate. Their child-call instruction counts require
mask-local corrections at the two dispatcher return sites; no call, return,
condition, memory or device-visible state differences were observed.

Negative controls with bit 2 added (`0x4146`, `0x8146` and `0x10146`) remain
fail-closed at `0x00018644`.
