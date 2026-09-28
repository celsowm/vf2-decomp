# v0670 — `fa_rob` `0x144b0` nonzero-selector prefix boundary

Starting from `out/park-1442c-s25.vf2snap`, the reference was patched with
fighter0 `+0x194 = 1` and run from the live `0x144b0` entry. It reaches
`0x0001a044`, immediately before the `call 0x1a1e4`, after 31 instructions.

The measured writes are:

- fighter0 `+0x5cc = 0`;
- fighter0 `+0x60c = 0`;
- fighter0 flags retain the measured word and set bit 11;
- fighter0 `+0x1a8 = 1`; and
- fighter0 `+0x1aa = 1`.

The subsequent selector-1 setup/continuation is not yet recovered. It remains
an explicit ROM-backed boundary; no native acceptance is added by this note.

## Measured selector setup

Executing only the `call 0x1a1e4` from the `0x1a044` boundary returns at
`0x1a048` after 167 instructions. The resulting fighter fields are
`+0x1a4 = 0x163`, `+0x1a8 = 1` and `+0x1aa = 1`.
