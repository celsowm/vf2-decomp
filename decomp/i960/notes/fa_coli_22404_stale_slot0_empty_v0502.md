# `fa_coli` `0x22404` stale slot-0 empty-result family

The stale slot-0 witness was swept over the bounded table-index byte. In
addition to index 0 (35 instructions), indices 2 and 5 produce empty masks
after the ROM mask lookup and registry scan, returning in 41 and 47
instructions respectively with `g0 = 0` and equal final condition state.

The accepted native predicate is the measured stale slot-0 join for table
indices 0, 2 and 5, with computed contact result `g8+0x6d4 == 0`. The native
fixture covers all three selectors. Stale non-empty and unmeasured empty
combinations remain fail-closed until measured.
