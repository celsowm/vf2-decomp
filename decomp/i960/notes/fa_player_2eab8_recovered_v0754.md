# 0x2eab8 recovered in C (v0754) — display_runtime_initialize

## Summary

`0x2eab8` is recovered in C and unit-tested. The function performs
a long struct initialization at `*0x500814` (offset `0x234..0x2cc`),
plus 3 work-RAM stores at `0x50a160..0x168` and a zero byte at
`0x50a14d`. The single sub-call to `0x31004` is **fully inlined**
(its 6 writes to `*0x50084c + 0x40, 0x54, 0x58, 0x60, 0x64, 0x70`
are replicated directly), so the function has no refused sub-calls.

This is a stand-alone execute hook (not yet wired into the
dispatcher chain). It is the 6th recovered sub-callee of
`0x1fcc0` (display_profile_apply); only `0x2c38`
(color_table_rebuild, 432 B, 11 blocks, complex — deferred)
remains.

## Function shape

| metric | value | source |
|---|---|---|
| extent | `0x2eab8..0x2ec24` (364 B) | `vf2i960 function` |
| blocks | 1 | `vf2i960 function` |
| sub-calls | 1 (to `0x31004`, inlined) | `vf2i960 disasm` |
| indirect branches | 0 | `vf2i960 function` |

## Body instructions

72 instructions in `0x2eab8..0x2ec1c` (inclusive of the call) +
10 instructions in the inlined `0x31004` body (excluding its own
`ret`) = **82 body instructions**, plus the `ret` at `0x2ec20`.

## Test

`tests/recovered/test_player_2eab8_native.c` exercises one
scenario with:

- `*0x500814 = 0x580000` (r3 base in work RAM)
- `*0x50084c = 0x590000` (r4 base in work RAM)
- pre-poisoned byte at `0x5800df = 0xab` (bit 0 set, so the
  `clrbit 0` is observable)

The test verifies:
- The 3 work-RAM stores at `0x50a160..0x168`.
- The zero byte at `0x50a14d`.
- The clrbit at `0x5800df` (0xab → 0xaa).
- All 36 writes to `(0x580000 + offset)` (0x27c, 0x234, 0x238,
  0x23c, 0x23e, 0x23f, 0x240, 0x244, 0x246, 0x260, 0x264, 0x268,
  0x26c, 0x26e, 0x27d, 0x27e, 0x27f, 0x2b0..0x2ba, 0x2bc, 0x2c0,
  0x2c4, 0x2c8, 0x2cc).
- All 6 writes from the inlined `0x31004` sub-call at
  `(0x590000 + offset)` (0x40, 0x54, 0x58, 0x60, 0x64, 0x70).
- Return status is `VF2_OK` and `cpu->ip == 0x2ec20`.

```
$ ctest -R vf2_player_2eab8
1/1 Test #47: vf2_player_2eab8 ........ Passed  0.02 sec
```

## Why inline `0x31004`?

`0x31004` (display_transform_defaults) is a 60 B, 1-block, no-indirect
function whose entire body is a small sequence of constant writes
to `*0x50084c + offset`. Inlining its actions into the caller's
recovery makes the caller's path self-contained and lets the test
verify the full side-effect set in one ctest entry, without needing
the per-step hook infrastructure (v0744 candidate A) to dispatch
to `0x31004` separately. The 6 inlined writes are at distinct
addresses from the outer function's writes (r3 vs r4 pointers, and
non-overlapping offset ranges), so there is no risk of double-write.

If a future slice needs `0x31004` to be callable independently
(for example, from a different caller), the C function can be
extracted and the inlining undone.

## Status

- All paths through the function are recovered (no paths REFUSED).
- No refused sub-calls.
- Function code: `src/recovered/hybrid.c:33719-34056` (approximate
  range; the function is ~340 lines including comments).
- Header declaration: `include/vf2/hybrid/player.h:140-150`.
- ctest entry: #47 (vf2_player_2eab8).
- `src/` byte-different from `e393a8cc` (the v0754 work is
  appended to `hybrid.c` and `player.h`).

## Sub-callees of 0x1fcc0 — final state

After v0754, **5 of 6 sub-callees of `0x1fcc0` are recovered**:

| sub-callee | recovered? | slice |
|---|---|---|
| `0x1fee4` | ✓ | v0749 |
| `0x1ff0c` | ✓ | v0750 |
| `0x1fffc` | ✓ | v0751 |
| `0x4b410` | ✓ | v0752 |
| `0x2eab8` | ✓ | v0754 |
| `0x2c38` (color_table_rebuild) | ✗ | deferred — 432 B, 11 blocks, complex |

`0x1fcc0` itself (548 B, 15 blocks, 6 sub-callees) remains
unrecovered because `0x2c38` is not yet recovered. With the
other 5 sub-callees unit-tested, the next step is either to
recover `0x2c38` (a substantial slice — 11 blocks, multiple
loops, non-standard `cmpinco` instructions) or to wire the
existing 5 sub-callees into `0x1fcc0`'s dispatcher chain via
the per-step hook (v0744 candidate A).

## Wiring boundary (unchanged from v0744)

The dispatcher chain in `hybrid_execute_player_post_29414` and
similar would call this via `hybrid_execute_interpreted_until`,
which uses `vf2_i960_run` with only `stop_address` / `max_steps`
— not a per-step callback. Wiring is a separate slice that needs
the per-step hook table.
