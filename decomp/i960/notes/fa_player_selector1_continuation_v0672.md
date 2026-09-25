# v0672 — selector-1 continuation endpoint

The selector-1 setup snapshot at `0x1a048` must be resumed toward the
`fa_rob` return at `0x1463c`; stopping at the unrelated player-corridor
endpoint `0x1428c` produces a self-branch and is not a valid contract.

From `out/144b0-nonzero-after1a1e4.vf2snap`, the reference reaches
`0x1463c` after 3,967 instructions, with three procedure calls and four
returns. The full path from the original `0x144b0` park is 4,134 instructions
through the same endpoint. The selector-1 continuation is not yet native and
remains explicitly ROM-backed.
