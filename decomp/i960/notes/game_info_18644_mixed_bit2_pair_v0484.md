# `fa_game_info` mixed positive bit-2 pair v0484

The calibrated `0x164ac` boundary was measured with the two fighter fields at
`+0x1a4` set to the ordered pair `0x0146` and `0x0142`, then repeated in the
reverse order. The native child chain was compared with a pure-ROM reference
for countdown `0/1` and mode bit 6 clear/set:

```text
0x0146 -> 0x0142: 4/4 exact
0x0142 -> 0x0146: 4/4 exact
```

All eight cases matched the architectural CPU state, condition state, mutable
Model 2A memory, device-visible state, instruction counters and call/return
counters at the scheduler return. The child-local instruction corrections are
`+9/+11` for return `0x164b0` (countdown clear/set) and `+5/+6` for return
`0x164c4` with countdown clear (mode bit 6 clear/set), or `+10` with countdown
set.

This is a narrow measured admission. Other mixed bit-2/bit-1/bit-6
compositions remain fail-closed until independently measured.
