# `fa_coli` `0x225cc` bit-8 sibling (v0461)

## Verdict

The exact scan-4 g8 word `0x00018100` is native. The ROM-backed live fixture
matches 103 instructions and full live state.

## Evidence

The witness uses:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
g8 + 0x1a4 = 0x00018100
```

At `0x22a28`, g8 bit 8 takes the compact route to `0x22bc0`: it scales the
current `r11` value by `3/2`, selects `r8 = 3`, writes
`g7 + 0x194 = 0x14000004`, and joins the common board-bit-9 path. The native
accounting uses the measured +9 correction for the direct g0=5 tail. The
reference returns at `0x22294` in 103 instructions with registers, condition
state, frames, counters and mutable memory equal.
