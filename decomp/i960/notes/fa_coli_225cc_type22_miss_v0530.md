# v0530: `fa_coli` `0x225cc` type-22 walker-miss sweep

The parked `out/coli-225cc-entry.vf2snap` was probed with the fighter-1 type
byte at `g8+0x19f` set to `22` and selectors `g8+0x19c == 1..64`.  The
reference reaches the parent boundary `0x10dcc` for every selector in that
interval.  Nine selectors are type-5 hits and remain a separate register
return frontier; the other 55 selectors are type-5 walker misses:

```text
1..12, 16..21, 23..24, 26, 28..29, 31..35, 37..64
```

Every measured miss has the same zero-record arithmetic tail as selector 1:
the resolver returns `g0 == 0`, the fields read at offsets `1` and `3` are
zero, and the shortcut completes with three nested calls and five returns.
The reference instruction counts vary with the measured type-8 walk shape;
the native body obtains that count from the existing `0x1ab34` recovery rather
than hard-coding a selector table.

The native recovery now admits any measured `walk == 0` result.  The index-0
control still fails before this branch on the reference walk, so it remains
unsupported.  The ROM-backed `vf2_coli_225cc_live` fixture runs all 55 miss
selectors and compares complete CPU state, condition state, procedure state
and mutable Model 2A memory.

The nine hit selectors in the same bounded probe (`13, 14, 15, 22, 25, 27,
30, 36, 43`) still expose an unmodeled `g0` resolver return and remain
fail-closed in the native wrapper until that register contract is recovered.
