# v0751: 0x1fffc recovered in C (display_color_profile_apply)

## Status

`vf2_hybrid_player_1fffc_execute` is a recovered C implementation
of the display_color_profile_apply at `0x0001fffc`, a sub-callee
of `0x0001fcc0` (display_profile_apply). Both paths are recovered:

- Path A: bit 21 of 0x500068 clear -> use 0x500064 as table index
- Path B: bit 21 of 0x500068 set -> use 3 as table index

The sub-call to `0x2c38` (`color_table_rebuild`, 432 B, 11 blocks,
not yet recovered) is REFUSED.

## Disassembly (per `vf2i960 disasm`, 88 B, 3 blocks)

```
0001fffc  ldob     0x00500064, r12       # r12 = mode byte
00020004  ld       0x00500068, r15
0002000c  bbc      21, r15, 0x00020018  # if bit 21 clear -> use r12
00020010  lda      0x00000003, r12      # else r12 = 3
00020018  shlo     8, r12, r4            # r4 = r12 << 8 (index)
0002001c  ldob     0x0006eeb8(r4), r5    # r5 = table[index + 0]
00020024  ldob     0x0006eeb9(r4), r6    # r6 = table[index + 1]
0002002c  ldob     0x0006eeba(r4), r7    # r7 = table[index + 2]
00020034  stob     r5, 0x005000e0
0002003c  stob     r6, 0x005000e1
00020044  stob     r7, 0x005000e2
0002004c  call     0x00002c38           # REFUSE
00020050  ret
```

## ctest

- New entry: `vf2_player_1fffc` (ctest #44, 0.01 s)
- Wires up `vf2_hybrid_player_1fffc_execute` via
  `CMakeLists.txt:474-478`
- Header decl in `include/vf2/hybrid/player.h:107-117`
- Test fixture:
  - Attaches a small fake main_rom (0x80000 bytes) so the
    table reads at 0x6eeb8 + offset succeed.
  - Path A: 0x500064 = 5; 0x500068 = 0xffdfffff (bit 21 clear).
    Expects bytes 0xa1/0xb2/0xc3 at 0x5000e0/1/2.
  - Path B: 0x500064 = 0; 0x500068 = 0x00200000 (bit 21 set).
    Expects bytes 0x11/0x22/0x33 at 0x5000e0/1/2.

## Notable bug found in test (now fixed)

The original test used `0xffefffff` for "bit 21 clear" but
`0xffefffff` actually has bit 21 SET (because `0xe = 1110`,
so the bit at position 21 is 0 only if the hex digit is `0xd` or
less). The correct "bit 21 clear" value is `0xffdfffff` (or
`0x00000000`).

## Counter

- `src/recovered/hybrid.c`: +90 lines (the function and execute
  hook, after debug print removal)
- `include/vf2/hybrid/player.h`: +10 lines (declaration)
- `tests/recovered/test_player_1fffc_native.c`: NEW, 145 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_1fffc`, ctest #44)

## Evidence

```
$ ctest --test-dir build -C Debug -R 'vf2_player_1fffc$' --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start 44: vf2_player_1fffc
1/1 Test #44: vf2_player_1fffc .................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
```

## What's NOT in this slice

- **0x2c38 (color_table_rebuild) recovery.** 432 B, 11 blocks.
  Sub-callee of 0x1fffc. The v0748 boundary note lists this as
  one of the remaining 0x1fcc0 sub-callees.
- **0x4b410, 0x2eab8, 0x11704 sub-callees.** Other remaining
  0x1fcc0 sub-callees.
- **0x1fcc0 itself.** The full 548 B, 15-block function.
- **Wiring into the dispatcher chain.** Same challenge as
  v0741/v0745/v0746/v0747.
