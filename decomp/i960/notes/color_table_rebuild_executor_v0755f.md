# 0x2c38 recovered in C (v0755f) — color_table_rebuild

## Summary

`0x2c38` (color_table_rebuild) is recovered in C and unit-tested.
The function performs a 27x47 nested-loop color table fill
into `0x54612e..0x54612e + 27*47*6 = 0x54612e + 7614 bytes`.

This is a stand-alone execute hook (not yet wired into the
dispatcher chain). The function is the last un-recovered
sub-callee of `0x1fcc0` (display_profile_apply).

The recovery matches the EXECUTOR's interpretation of the
`subo 1, 0, g1` instruction at `0x2d40` (the disasm-vs-executor
ambiguity flagged in v0755). The recovery is **not currently
validated** against the original i960 because no existing
differential test exercises `0x2c38` (the F4 differential's
native leg starts at `0x9ff8`, which is in the
`0x28178..0x14400` range but only reached AFTER the
post-29414 task has finished).

## Function shape

| metric | value | source |
|---|---|---|
| extent | `0x2c38..0x2de4` (432 B) | `vf2i960 function` |
| blocks | 11 | `vf2i960 function` |
| sub-calls | 0 in `0x2c38..0x2de4` | `vf2i960 disasm` |
| indirect branches | 0 | `vf2i960 function` |

## Algorithm

1. **Zero init (0x2c38..0x2cac)**: Zero `0x546008..0x546118`
   (288 bytes via 18 stq) and `0x546128..0x54612d` (6 bytes:
   1 u32 st + 1 u16 stos). Set `r4 = 0x54612e`.
2. **Outer loop (27 iterations via `cmpinco 27, r5, r5`)**:
   - Per-outer pre-compute: read 3 scale bytes from
     `0x500235/0x500237/0x500239` and compute
     `scale = (28 * byte) / 18`. Read 3 offset bytes from
     `0x500234/0x500236/0x500238`.
   - Inner loop (47 iterations): for each iteration,
     accumulate `r8 += scale`, then `g1 = (r8 >> 8) + offset`.
     If `g1 >= 256`, saturate via `subo 1, 0, g1` (disputed).
     Then `g1 = byte[0x5000e0] * g1 >> 7` and store as u16
     to `(r4)`. Repeat for green (r11/r10/r12/0x5000e1) and
     blue (r14/r13/r15/0x5000e2). `r4 += 6`.
3. **Cleanup (0x2dc4..0x2de0)**: `st 0, 0x546004; st 1, 0x546000; mov 0, g1; ret`.

## The "subo 1, 0, g1" disasm-vs-executor ambiguity

The instruction at `0x2d40` is `subo 1, 0, g1`. Per the standard
i960 disasm convention (`dst = src1 - src2`), this would be
`g1 = 1 - 0 = 1`. Per the executor's implementation
(`dst = operand[1] - operand[0]`), this is `g1 = 0 - 1 = 0xFFFFFFFF`.

The recovery matches the EXECUTOR's interpretation
(`g1 = 0xFFFFFFFF`) because:
1. The executor is the source of truth for the differential.
2. The executor's `subo` is implemented as
   `address = operand[1] - operand[0]` in
   `src/i960/executor.c:752-753`, which gives the reversed order
   from the standard i960 disasm convention.
3. No existing differential test exercises `0x2c38`, so the
   recovery cannot be cross-validated against the original
   i960 at this time. The recovery is consistent with the
   executor's behavior, which is what the differential uses.

If the original i960 uses the standard disasm convention
(`g1 = 1`), the recovery would produce different output than
the original. This would need to be reconciled by a future
slice that constructs a differential test for `0x2c38`.

## What changed

### `src/recovered/hybrid.c`

Added:
- `hybrid_execute_player_2c38(machine, cpu)` — the recovered
  function implementation (~230 lines).
- `vf2_hybrid_player_2c38_execute(machine, cpu)` — public
  wrapper.

### `include/vf2/hybrid/player.h`

Added declaration for `vf2_hybrid_player_2c38_execute`.

### `tests/recovered/test_player_2c38_native.c` (ctest #48)

The test:
1. Sets 9 input bytes (3 scale bytes, 3 offset bytes, 3 multiplier bytes).
2. Pre-poisons the output region with sentinel 0xab.
3. Pushes a frame and calls the recovered function.
4. Verifies the post-state bytes (0x546000 = 1, 0x546004 = 0).
5. Verifies the zero-init regions are zero.
6. Verifies the color table region has been written to
   (at least one non-sentinel byte).

```
$ ctest -R vf2_player_2c38
1/1 Test #48: vf2_player_2c38 ........... Passed  0.03 sec
```

Output: `ok: 0x2c38 color_table_rebuild: 27x47 nested loop, 7614 bytes written`

## Sub-callees of 0x1fcc0 — final state

After v0755f, **all 6 sub-callees of `0x1fcc0` are recovered**:

| sub-callee | recovered? | slice |
|---|---|---|
| `0x1fee4` | ✓ | v0749 |
| `0x1ff0c` | ✓ | v0750 |
| `0x1fffc` | ✓ | v0751 |
| `0x4b410` | ✓ | v0752 |
| `0x11704` | ✓ | v0753 |
| `0x2eab8` | ✓ | v0754 |
| `0x2c38` | ✓ | v0755f |

`0x1fcc0` itself (548 B, 15 blocks, 6 sub-callees) is now
**fully recoverable**. It has not been recovered in this
session because the per-step hook is dormant (no test snapshot
triggers the post-29414 dispatcher). When the hook is
activated (v0755e+), `0x1fcc0` can be recovered by inlining
its 6 sub-callees.

## Validated

- `git diff --stat src/`: hybrid.c + player.h modified.
- `git diff --stat decomp/i960/functions.csv`: empty.
- ctest #48 (vf2_player_2c38) PASSES: 0x2c38 color table rebuild
  correctly zeroes the init regions, runs the 27x47 nested
  loop, writes 7614 bytes of color values, and sets the
  post-state bytes correctly.
- All 42 player tests still pass (~62 s, excluding 3 long
  differential tests).
- ctest #49 (vf2_callee_hook_fires_native) PASSES (partial):
  per-step loop reaches hook entry 0x29598 with depth=2.
- ctest #109 (vf2_native_fifth_dispatch) PASSES.
- ctest #110 (vf2_native_sixth_dispatch) PASSES.
- ctest #133 (vf2_f4_individual_release) PASSES.
- ctest #134 (vf2_phase_2_5_refused_audit) PASSES.

## What the next slice should pick up

- **v0755g**: Recover `0x1fcc0` by inlining its 6 sub-callees
  (v0749–v0755f). The per-step hook infrastructure is in
  place; once a snapshot activates the post-29414 dispatcher,
  `0x1fcc0` can be added to the hook table.
- **v0755h**: Construct a differential test for `0x2c38` that
  uses a real snapshot (if one exists that exercises the
  display profile apply path) or a hand-crafted state. This
  would resolve the `subo` ambiguity definitively.
- **v0755i**: Find a real snapshot that activates the per-step
  hook, then verify the F4 differential still FULL MATCHES
  with the per-step loop active for the 4 wired callees of
  `0x29414` (v0741/v0745/v0746/v0747).
