# v0673 — native selector-1 setup boundary

The measured nonzero `fa_rob` path now has a native implementation for the
selector-1 setup call at `0x0001a1e4`. The helper admits only the controlled
entry shape parked at `0x0001a044`, with `g0 == 1` and `g7` equal to the live
fighter base.

The ROM boundary is `0x1a044 -> 0x1a048`: 167 instructions, one procedure
call, and one procedure return. The generic selector bytecode plan resolves
the live data pointer `0x02014d6d`, scratch table `0x02c00000`, and three
opcodes. The opcode-2 handler sets state bit 1, so the final measured
`fighter + 0x1a4` value is `0x00000163` rather than the setup base
`0x00000161`.

The native boundary also preserves the measured i960 poststate: comparison
state `NONE` and arithmetic-control low condition bits cleared. A ROM-backed
fixture compares complete CPU, procedure, condition, counter and mutable
Model 2A state against the reference executor. Unmeasured selector shapes and
the continuation after `0x1a048` remain unsupported.

No ROM bytes or snapshots are committed; the fixture uses the existing ignored
`out/144b0-nonzero-before1a1e4.vf2snap` checkpoint.
