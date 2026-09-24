# `fa_game_info` positive state-8 `0x4142` family v0480

The positive threshold frontier was measured from the calibrated `0x164ac`
boundary with both fighters in state 8 and fighter state flags `0x4142`
(state bit 8 plus bits 1, 6 and 14). The three distributions were tested:

```text
fighter0=0x4142, fighter1=0
fighter0=0,      fighter1=0x4142
fighter0=0x4142, fighter1=0x4142
```

For each distribution, countdown `0/1`, mode bit 6 clear/set and positive
thresholds `0/1/2` were run through both `0x18644` calls and the scheduler
return. All 36 cases matched the reference in complete CPU and Model 2A
state, including counters and final memory.

The native admission adds only the measured bit-1+bit-6+bit-14 combination in
the existing state-8 bit-1 gates. Neighboring unmeasured low-bit and bit-15/
bit-16 compositions remain rejected at `0x18644`.

The Windows-native `vf2i960 game-info-child` command provides the direct child
invocation used by `validate_game_info_state4.py`; the Linux ptrace helper is
still retained for Linux analysis environments.
