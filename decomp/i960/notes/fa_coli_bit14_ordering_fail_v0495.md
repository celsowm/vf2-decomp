# `fa_coli` `0x22298` second-loop ordering-fail tail

The measured scan-2 witness takes the second dispatch branch with
`g7+0x1a4 == 0`, `g8+0x1a4` bit 8 set, `g7+0x61c == 0`, and
`g8+0x821 == 2`. Its ordering values are `g7+0x1f8 == 0` and
`g7+0x6e4 == 0`, so the `0x2233c` loop gate fails. The ROM selects the
`0xffff` common tail at `0x223a8`, stores it at `g7+0x6dc`, and returns in
22 instructions.

The native helper now admits this exact ordering-fail shape with body count
21 plus the procedure return. The ordering-success loop and all other
unmeasured scan-2 siblings remain bounded by their existing gates.
