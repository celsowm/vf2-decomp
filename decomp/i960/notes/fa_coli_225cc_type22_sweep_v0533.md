# v0533: `fa_coli` type-22 selector sweep through 256

The live `out/coli-225cc-entry.vf2snap` probe was extended from selector 64
through 256 with `g8+0x19f == 22`. Every selector `g8+0x19c == 1..256`
reaches `0x10dcc` in the reference with three nested calls and five returns.
The range contains both type-5 walker misses and hits; their selector-
dependent walk lengths and hit record pointers are preserved by the existing
`0x1ab34` body and the type-22 `g0` threading.

The ROM-backed `vf2_coli_225cc_live` fixture now compares all 256 selectors
for exact CPU registers, condition state, procedure state and mutable Model 2A
memory. Selector 0 remains the measured out-of-bounds control and stays
fail-closed. Selectors above 256 and other type/flag compositions remain
unmeasured.
