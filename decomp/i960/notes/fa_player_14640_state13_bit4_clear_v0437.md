# fa_rob `0x14640` state-13 bit-4-clear tail (v0437)

The measured state-13 helper shape has `+0x198 == 0`, `+0x654 == 0`,
`+0x197 == 13`, and nonzero `+0x194` from the state byte.  With fighter bit 4
clear, `0x146b4 bbc 4` branches directly to `0x146c8`; the tail clears
`+0x654` and returns at `0x146d8`.

The ROM-backed fixture proves exact CPU/condition/frame/memory equality for
both flag shapes: bit 4 clear takes 13 instructions and bit 4 set takes 14,
with no calls or returns.  Other state-byte and flag compositions remain
fail-closed.
