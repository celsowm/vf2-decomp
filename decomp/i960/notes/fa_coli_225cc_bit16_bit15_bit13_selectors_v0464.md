# `fa_coli` `0x225cc` bit-13 selector siblings (v0464)

## Verdict

Eight exact scan-4 g8 words are native for the measured g7 word
`0x00400100`: `0x0001a001`, `0x0001a004`, `0x0001a010`, `0x0001a020`,
`0x0001a040`, `0x0001a080`, `0x0001a200` and `0x0001a400`. Each ROM-backed
live fixture matches 217 instructions and full live state.

## Evidence

All witnesses use:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
```

The selector words preserve g8 bits 13, 15 and 16 and take the same measured
bit-13 `0x22744`/`0x22808`/`0x22848` route as v0463. The eight words have the
same measured tail result, including final `g0 = 0x000000ee`; admission stays
an exact-word list and other selector compositions remain fail-closed.
