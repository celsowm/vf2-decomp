# v0671 — selector-1 `0x1a1e4` setup boundary

From the live `fa_rob` `0x144b0` nonzero park with fighter0 `+0x194 = 1`,
the reference reaches `0x1a044` after 31 instructions. The `call 0x1a1e4`
then returns at `0x1a048` after 167 additional instructions (+1 call and
+1 return).

At the return boundary, fighter0 has:

- `+0x1a4 = 0x00000163`;
- `+0x1a8 = 1`; and
- `+0x1aa = 1`.

The setup is not interchangeable with the accepted selector-`0x505` plan;
the selector-1 continuation remains explicit ROM-backed behavior.
