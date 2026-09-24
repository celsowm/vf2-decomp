# `fa_coli` `0x22298` bit-14 scan-5 early return

Starting from the measured `0x22298` bit-14 loop entry, the reference was
patched only at `g8+0x821` from scan `0` to scan `5`. The other loop gates
remain unchanged: `g7+0x1a4` bit 14 is set, `g7+0x61c == 1`, and the
bit-8/bit-1 dispatch selects the bit-14 corridor.

The reference takes the second dispatch branch at `0x22320`, writes zero to
`g7+0x6dc`, and returns in 15 instructions. The native helper now admits
this exact shape with body count 14 plus the procedure return. The adjacent
scan-4 sibling remains explicitly unsupported.
