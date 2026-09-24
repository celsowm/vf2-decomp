# v0418: recover the second `0x14570` gate

## Verdict

The measured direct state-16 path with the `0x50016c+0x3351` byte bit 6 set
and `g8` bit 29 clear is now native. The ROM takes the not-taken `0x145c0`
branch, loads `(g8)` at `0x145c4`, then takes the bit-29 branch at `0x145c8`
to the common `0x145dc` store. This adds two instructions over the short
tail and reaches `0x1463c` in 54 instructions with one type-5 call/return.

The recovery admits only the measured direct `(r7,r8)=(16,0)` composition;
second-gate variants with bit 29 set, swaps, and later scaling/text paths
remain unsupported.

## Pin

`vf2_player_1453c_live` adds the direct state-16 second-gate case to its
matrix and requires exact instruction count, call/return counters and full
live-state equality.
