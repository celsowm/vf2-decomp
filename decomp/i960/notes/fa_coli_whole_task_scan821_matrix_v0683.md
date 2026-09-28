# `fa_coli` whole-task scan-`0x821` matrix

The parked `0x221e8` whole-task fixture was swept over the measured neutral
domains:

- fighter flags: `0` or bit 8 for each fighter;
- fighter 0 `field_0804`: `0` or bit 15;
- fighter 0 `field_0821`: `0`, `1`, `4` or `5`; and
- fighter 0 `field_0822`: `0`, `1`, `16` or `256`.

All 128 reference cases reached `0x10dcc`. The observed accounting clusters
were `9214/18/19`, `9385/17/18`, `9391/17/18`, `9520/18/19` and
`9526/18/19`. Within this measured domain `field_0822` and `field_0804` did
not change the result. `field_0821 = 5` selects the longer scan-5 result for
the single-live and bilateral shapes; the other measured values use the
shorter accounting shape.

The native whole-task fixture now replays all 128 cases and compares complete
CPU, condition, frame, counter and mutable Model 2A state against the
reference. The recovery adds only the measured accounting correction for
single-live scan values `0/1/4/5` and bilateral values `0/1/4`; the bilateral
scan-5 exception remains separately guarded. Unmeasured flags, fighter-1
field variants and neighboring scan values remain fail-closed.
