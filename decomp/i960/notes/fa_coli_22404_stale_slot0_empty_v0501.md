# `fa_coli` `0x22404` stale slot-0 empty tail

The live stale entry was kept at slot 0 with `g13+0x8c == 0xffff` and the
fighter table index `g7+0x820` changed to `0`. The reference mask is empty,
writes zero to `g8+0x6d4`, sets `g0 = 0`, and returns in 35 instructions with
equal final condition state.

The native recovery admits only stale slot 0 with table index 0 for this
empty tail. The ROM-backed live differential fixture now covers both stale
slot-0 and stale slot-1 empty tails; other stale-empty combinations remain
explicitly unsupported.
