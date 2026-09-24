# `fa_coli` `0x22298` measured zero-mask tails

Two existing reference traces identify compact scan-0 exits that are distinct
from the recovered loops:

- with `g7+0x1a4` bit 14, `g8+0x1a4 == 0x100`, and `g7+0x61c == 0`, the
  second dispatch branch writes zero and returns in 14 instructions;
- with `g7+0x1a4 == 0`, `g8+0x1a4 == 0x4100`, and `g7+0x61c == 0`, the
  same scan-0 path writes zero and returns in 15 instructions.

The native helper admits only these exact flag words and zero-scan gates.
Neighboring flag compositions remain explicitly unsupported.
