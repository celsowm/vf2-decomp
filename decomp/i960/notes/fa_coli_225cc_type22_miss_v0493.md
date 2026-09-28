# `fa_coli` `0x225cc` type-22 index-1 walker miss

The parked live `0x225cc` snapshot was probed with the fighter-1 type byte at
`g8+0x19f` set to `22` and the selector at `g8+0x19c` set to `1`.  The
type-5 call from `0x18bd4` reaches the measured `0x1ab34` miss shape: the
record walk returns zero after the type-8 chain, so the `0x18bd4` tail reads
the zero-record fields at offsets `1` and `3` as zero.

The reference reaches the parent boundary `0x10dcc` in 71 instructions, with
three nested calls and five returns from the parked live frame.  The native
shortcut now admits exactly this `index == 1` miss, preserves the measured
zero fields, writes `g1 = 5`, and applies the final `chkbit 10` condition
state.  The ROM-backed `vf2_coli_225cc_live_differential` fixture compares the
complete CPU state, condition state, procedure state and Model 2A memory.

This is a bounded recovery.  Other type-5 walker misses remain explicitly
unsupported until separately measured.
