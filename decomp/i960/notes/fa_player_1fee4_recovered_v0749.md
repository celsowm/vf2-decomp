# v0749: 0x1fee4 recovered in C (Phase 3, sub-callee of 0x1ff0c → 0x1fcc0)

## Status

`vf2_hybrid_player_1fee4_execute` is a recovered C implementation
of the trivial init function at `0x0001fee4`, the first sub-callee
of `0x0001ff0c` (`display_profile_mode_constants`), itself a
sub-callee of `0x0001fcc0` (`display_profile_apply`).

The function is **not** yet wired into the dispatcher chain; it
exists as a standalone execute hook like v0741/v0745/v0746/v0747.
Wiring is a separate slice (see `fa_player_29598_wiring_v0744.md`).

## Disassembly (per `vf2i960 disasm`, 36 B, 5 blocks)

```
0001fee4  mov      26, r5               # r5 = 26 (loop counter)
0001fee8  lda      0x0050a0e0, r6       # r6 = write pointer
0001fef0  lda      0x3f800000, r7       # r7 = float 1.0 in IEEE 754
0001fef8  st       r7, (r6)            # *(r6) = 1.0
0001fefc  addo     4, r6, r6            # r6 += 4
0001ff00  cmpdeco  1, r5, r5            # r5 = r5 - 1; if r5 == 0 fall
0001ff04  bl       0x0001fef8          # else branch back to body
0001ff08  ret
```

A 26-iteration loop writing the IEEE 754 float 1.0 (`0x3f800000`)
to 26 consecutive 4-byte locations starting at `0x50a0e0` (covering
`0x50a0e0..0x50a144`).

Note: the v0748 boundary note said "27 iterations" but the
disassembly shows `mov 26, r5` (not 27). The correct count is 26.

## ctest

- New entry: `vf2_player_1fee4` (ctest #42, 0.02 s)
- Wires up `vf2_hybrid_player_1fee4_execute` via
  `CMakeLists.txt:455-459`
- Header decl in `include/vf2/hybrid/player.h:86-93`
- Test fixture:
  - Pre-poisons `0x50a0e0..0x50a14c` with `0xdeadbeef`.
  - Calls the recovered function.
  - Verifies all 26 locations are `0x3f800000` after the call.
  - Verifies `0x50a148` (just past the range) is still `0xdeadbeef`
    (untouched).

## What's NOT in this slice

- **0x1ff0c recovery** (240 B, 5 blocks, 3 paths). Next in the
  sub-callee chain. The 0x1fee4 sub-call can be refused.
- **0x1fcc0 recovery** (548 B, 15 blocks, 5 sub-callees). The full
  function. v0748 boundary note documents the path.
- **Wiring into the dispatcher chain.** Same challenge as
  v0741/v0745/v0746/v0747 (per-step hook infrastructure needed).

## Counter

- `src/recovered/hybrid.c`: +80 lines (the function and execute hook)
- `include/vf2/hybrid/player.h`: +9 lines (declaration)
- `tests/recovered/test_player_1fee4_native.c`: NEW, 100 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_1fee4`, ctest #42)

## Evidence

```
$ ctest --test-dir build -C Debug -R 'vf2_player_1fee4$' --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start 42: vf2_player_1fee4
1/1 Test #42: vf2_player_1fee4 .................   Passed    0.02 sec
100% tests passed, 0 tests failed out of 1
```
