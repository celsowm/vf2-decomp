# v0687 — `fa_player` `0x27ce0` gate

The optional liftkit scaffold (`segamodel2-tools` revision
`94e2b4b8d9425d9b120136b568653353698062b1`) was used only as a static aid.
Its 16-instruction slice at `0x27ce0` identifies the four loads and the two
selector comparisons before the shared `0x27d00` body:

```text
ldos  +0x1aa
cmpibe 1      -> 0x27d00
ldos  +0xc4e
cmpibe 1      -> 0x27d00
ldos  +0xc4c
ldos  +0x1a8
cmpobe equal  -> 0x27cfc
ret           -> caller + 4
```

The existing live `0x505` witness takes the second branch (`+0xc4e != 1`)
and continues to `0x27d00`. A controlled ROM-backed equal-selector witness
sets `+0x1aa = 1`, `+0xc4e = 1` and `+0xc4c = +0x1a8 = 0x505`; the reference
returns to `0x1abf8` after 9 instructions including the call, with the
compare result equal and no memory writes. Native C now admits exactly this
shape and matches the executor's full live state. Other gate shapes continue
through the existing fail-closed `0x27d00` path.

The adjacent `0x27cc8` slice was also lifted. It confirms the existing
36-word `cvtri`/`stis` conversion tail in `hybrid_execute_player_27b5c`; it
does not justify a new generic conversion rule. Non-finite/overflow values
remain unsupported.
