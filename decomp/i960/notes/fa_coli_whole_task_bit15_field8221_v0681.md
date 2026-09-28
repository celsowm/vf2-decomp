# `fa_coli` bit-15/`field_0822 = 1` neighbors

The whole-task fixture starts from the parked `0x221e8` checkpoint and
compares the native dispatcher with the reference executor through `0x10dcc`.
The measured matrix entries are:

- fighter 1 bit 8, fighter 0 `+0x804` bit 15, `+0x822 = 1`: `9385/17/18`;
- both fighters bit 8, fighter 0 `+0x804` bit 15, `+0x822 = 1`: `9520/18/19`;
- both fighters bit 8, fighter 0 `+0x804` bit 15, `+0x821 = 5`,
  `+0x822 = 1`: `9526/18/19`.

All three cases match CPU state, condition state, local frames, counters and
mutable Model 2A memory. These are measured matrix expansions only; other
field combinations remain `VF2_ERROR_UNSUPPORTED`.
