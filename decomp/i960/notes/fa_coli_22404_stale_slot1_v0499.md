# `fa_coli` `0x22404` stale slot-1 scan

The live first-contact entry was repeated with the fighter slot byte changed
from `0` to `1` and the slot-1 registry snapshot at `g13+0x8e` set to
`0xffff`, while the current snapshot, pending mask, thresholds, table index,
and FIFO inputs stayed unchanged. The reference takes the same stale-slot
pending-clear rejoin and the existing slot-1 15-trip scan, returning in 139
instructions with `g0 = 1` and equal final condition state.

The native stale-slot guard is removed for this measured non-empty shape. The
ROM-backed `vf2_coli_22404_live_differential` fixture now runs both the slot-0
and slot-1 stale cases and compares complete live state. Stale+empty cases
remain fail-closed.
