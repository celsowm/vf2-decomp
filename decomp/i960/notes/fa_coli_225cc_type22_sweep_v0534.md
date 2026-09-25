# v0534: `fa_coli` type-22 selector sweep through 512

The live `out/coli-225cc-entry.vf2snap` probe was extended from selector 256
through 512 with `g8+0x19f == 22`. Every selector `g8+0x19c == 1..512`
reaches `0x10dcc` in the reference with three nested calls and five returns.
The range includes both type-5 walker misses and hits, with selector-specific
walk lengths and hit record pointers preserved by the native resolver path.

The ROM-backed `vf2_coli_225cc_live` fixture now compares all 512 selectors
for exact CPU registers, condition state, procedure state and mutable Model 2A
memory, in both the normal and sanitizer builds. Selector 0 remains the
measured out-of-bounds control and stays fail-closed. Selectors above 512 and
other type/flag compositions remain unmeasured.
