# `fa_coli` `0x22404` stale slot-0 empty-result family

The stale slot-0 witness was swept over the bounded table-index byte. In
addition to index 0 (35 instructions), indices 2 and 5 produce empty masks
after the ROM mask lookup and registry scan, returning in 41 and 47
instructions respectively with `g0 = 0` and equal final condition state.

The accepted native predicate is therefore the measured join condition:
stale slot 0 (`g13+0x8c != g7+0x1a8`) and computed contact result
`g8+0x6d4 == 0`. The native fixture covers indices 0, 2 and 5. Stale
non-empty and unmeasured empty combinations remain fail-closed until measured.
