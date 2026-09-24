# `fa_coli` `0x22404` stale slot-1 empty tail

The stale slot-1 witness from v0499 was rerun with the fighter table index
`g7+0x820` changed from `1` to `0`. The reference mask is empty, writes zero
to `g8+0x6d4`, sets `g0 = 0`, and returns in 35 instructions. The final
condition state remains equal after the slot-1 stale pending-clear rejoin.

The native recovery admits only stale slot 1 with table index 0 for this empty
tail. Other stale-empty combinations remain explicitly unsupported. The
ROM-backed `vf2_coli_22404_live_differential` fixture now covers slot-0 stale
non-empty, slot-1 stale non-empty, and slot-1 stale-empty cases.
