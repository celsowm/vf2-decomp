# `fa_coli` `0x225cc` bit-16 scan-4 continuation (v0451)

## Verdict

The measured `g8 + 0x1a4` bit-16 composition with `g7 + 0x821 == 4` is
native through the shared `0x22e24` join. The recovery matches the reference
at the live differential boundary, including architectural state, mutable
memory, call/return state, and the exact instruction count.

## Evidence

The reference was driven from `out/coli-225cc-entry.vf2snap` with:

```text
g8 + 0x1a4 = 0x00010000
g7 + 0x821 = 4
```

It ran 217 instructions and stopped at `0x00022294`. The relevant branch
sequence was:

```text
0x22c88: bbs 16 -> 0x22d8c
0x22d90: bbc 22 -> 0x22e24
```

The measured `g7 + 0x1a4` value was `0x00000100`, so the bit-22-set
continuation remains outside this recovery. The final observed values include
`g8 + 0x198 = 0x0c0004ac` and `g8 + 0x5de = 0xfff2`.

The ROM-backed live fixture compares the complete reference/native state and
proves equality for this witness. The exact count also pins a measured
seven-instruction accounting correction in the shared composition.

Admission is intentionally exact: other bit-16 compositions, including the
bit-22-set continuation, remain fail-closed.
