# `fa_coli` `0x225cc` bit-13 selector siblings (v0466)

## Verdict

Four exact scan-4 g8 words are native for the measured g7 word
`0x00400100`: `0x0001a100`, `0x0001a800`, `0x0001b000` and `0x0001e000`.
Each ROM-backed live fixture matches 217 instructions and full live state.

## Evidence

All witnesses use:

```text
g7 + 0x1a4 = 0x00400100
g7 + 0x821 = 4
```

The words preserve the v0463 bit-13 scan-4 route and add one measured
selector at a time: bit 8, bit 11, bit 12 or bit 14. They share the
`0x22744`/`0x22808`/`0x22848` path and final `g0 = 0x000000ee`. Admission is
kept as an exact-word list; other selector combinations remain fail-closed.
