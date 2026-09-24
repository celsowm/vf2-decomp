# `fa_coli` `0x22298` scan-6 second loop

The measured scan-6 witness keeps the scan-2 second-loop gates: zero
`g7+0x1a4`, bit 8 set in `g8+0x1a4`, `g7+0x61c == 0`,
`g7+0x1f8 < g7+0x6e4`, and `g8+0x821 == 6`. The ROM branches from
`0x2232c` to the same `0x2233c..0x223a4` 16-trip loop and returns in 142
instructions for the measured mask.

The native helper now admits scan values 2 and 6 for this already recovered
loop body. Other scan values and gate compositions remain fail-closed.
