# v0752: 0x4b410 recovered in C (video_command_submit)

## Status

`vf2_hybrid_player_4b410_execute` is a recovered C implementation
of the video_command_submit at `0x0004b410`, a sub-callee of
`0x0001fcc0` (display_profile_apply). The function is the
smallest in the chain — 60 B, 1 block, 8 instructions + ret.

## Disassembly (per `vf2i960 disasm`, 60 B, 1 block)

```
0004b410  mov      1, r3               # r3 = 1
0004b414  st       r3, 0x00550000      # *0x550000 = 1 (control)
0004b41c  lda      0x005502e0, r3      # r3 = 0x5502e0 (status addr)
0004b424  mov      3, r15             # r15 = 3
0004b428  st       r15, [r3]           # *0x5502e0 = 3 (status)
0004b430  st       g0, 0x00000004[r3]  # 0x5502e4 = g0
0004b438  st       g1, 0x00000008[r3]  # 0x5502e8 = g1
0004b440  st       g2, 0x0000000c[r3]  # 0x5502ec = g2
0004b448  ret
```

## ctest

- New entry: `vf2_player_4b410` (ctest #45, 0.02 s)
- Wires up `vf2_hybrid_player_4b410_execute` via
  `CMakeLists.txt:482-486`
- Header decl in `include/vf2/hybrid/player.h:120-126`
- Test fixture:
  - Sets g0 = 0xdeadbeef, g1 = 0xcafebabe, g2 = 0xfeedface.
  - Pre-poisons the 5 write addresses with sentinels.
  - Verifies all 5 writes match (1, 3, g0, g1, g2).

## Counter

- `src/recovered/hybrid.c`: +75 lines (the function and execute
  hook)
- `include/vf2/hybrid/player.h`: +7 lines (declaration)
- `tests/recovered/test_player_4b410_native.c`: NEW, 90 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_4b410`, ctest #45)

## Evidence

```
$ ctest --test-dir build -C Debug -R 'vf2_player_4b410$' --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start 45: vf2_player_4b410
1/1 Test #45: vf2_player_4b410 .................   Passed    0.02 sec
100% tests passed, 0 tests failed out of 1
```

## What's NOT in this slice

- **0x2c38 (color_table_rebuild) recovery.** 432 B, 11 blocks.
  Sub-callee of 0x1fffc. The v0748 boundary note lists this as
  one of the remaining 0x1fcc0 sub-callees.
- **0x2eab8, 0x11704 sub-callees.** Other remaining 0x1fcc0
  sub-callees.
- **0x1fcc0 itself.** The full 548 B, 15-block function. 4 of 5
  sub-callees are now recovered (0x1fee4, 0x1ff0c, 0x1fffc,
  0x4b410); only 0x2c38, 0x2eab8, 0x11704 remain.
- **Wiring into the dispatcher chain.** Same challenge as
  v0741/v0745/v0746/v0747.
