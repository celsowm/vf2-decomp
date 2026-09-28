# v0535: `fa_coli` type-22 selector sweep through 1024

The live `out/coli-225cc-entry.vf2snap` probe was extended from selector 512
through 1024 with `g8+0x19f == 22`. Every selector `g8+0x19c == 1..1024`
reaches `0x10dcc` in the reference with three nested calls and five returns.
The range includes both type-5 walker misses and hits, preserving their
selector-specific walk lengths and hit record pointers.

The ROM-backed `vf2_coli_225cc_live` fixture compares all 1,024 selectors for
exact CPU registers, condition state, procedure state and mutable Model 2A
memory in both normal and sanitizer builds. Selector 0 remains the measured
out-of-bounds control and stays fail-closed. Selectors above 1024 and other
type/flag compositions remain unmeasured.
