# v0750: 0x1ff0c recovered in C (display_profile_mode_constants)

## Status

`vf2_hybrid_player_1ff0c_execute` is a recovered C implementation
of the display_profile_mode_constants at `0x0001ff0c`, a
sub-callee of `0x0001fcc0` (display_profile_apply). All three
paths are recovered:

- Path A: 0x500064 == 10 -> write 2 floats
- Path B: 0x500064 == 6  -> write 12 floats
- Path C: default         -> ret (no writes)

The function delegates the init sub-call to `0x1fee4` (the v0749
recovery). The frame-handling design ensures the caller's frame
is preserved.

## Disassembly (per `vf2i960 disasm`, 240 B, 5 blocks)

```
0001ff0c  call     0x0001fee4            # init 26 x float 1.0
0001ff10  ldob     0x00500064, r15
0001ff18  cmpobe   10, r15, 0x0001ff24   # path A
0001ff1c  cmpobe   6, r15, 0x0001ff48    # path B
0001ff20  ret                            # path C

# path A: 2 floats at 0x50a124, 0x50a128
# path B: 12 floats at 0x50a0e4, 0x50a0e8, 0x50a0f0, 0x50a0f8,
#                  0x50a100, 0x50a118, 0x50a11c, 0x50a124, 0x50a128,
#                  0x50a12c, 0x50a134
#                  (10 contiguous pairs + 2 standalone, per the v0750
#                  test's enumeration)
```

## Frame-handling design

`vf2_hybrid_player_1ff0c_execute` pushes a fresh frame for the
`0x1fee4` call (so that `0x1fee4`'s `vf2_i960_cpu_return_procedure`
in its public execute function only pops the inner frame), then
continues with the path dispatch and pops its own frame at the
end. The test fixture pushes the outer frame; the recovery
manages the inner one.

## ctest

- New entry: `vf2_player_1ff0c` (ctest #43, 0.02 s)
- Wires up `vf2_hybrid_player_1ff0c_execute` via
  `CMakeLists.txt:467-471`
- Header decl in `include/vf2/hybrid/player.h:96-104`
- Test fixture:
  - Pre-poisons 12 write addresses with `0xdeadbeef`.
  - Sets `0x500064` to the path-specific value.
  - Verifies that the 0x1fee4 init ran (all addresses have
    `0x3f800000` after the call) and that the path-specific
    writes also happened.

## Counter

- `src/recovered/hybrid.c`: +150 lines (the function and execute
  hook). Also a small refactor of `vf2_hybrid_player_1fee4_execute`
  to make the frame-pop explicit (the inner function no longer
  pops the frame; the public function does).
- `include/vf2/hybrid/player.h`: +9 lines (declaration)
- `tests/recovered/test_player_1ff0c_native.c`: NEW, 175 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_1ff0c`, ctest #43)

## Evidence

```
$ ctest --test-dir build -C Debug \
    -R "vf2_player_(29598|439ac|43888|cf04|1fee4|1ff0c)$|vf2_phase_2_5_refused_audit$" \
    --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start  38: vf2_player_29598
1/7 Test  #38: vf2_player_29598 .................   Passed    0.02 sec
    Start  39: vf2_player_439ac
2/7 Test  #39: vf2_player_439ac .................   Passed    0.01 sec
    Start  40: vf2_player_43888
3/7 Test  #40: vf2_player_43888 .................   Passed    0.02 sec
    Start  41: vf2_player_cf04
4/7 Test  #41: vf2_player_cf04 ..................   Passed    0.02 sec
    Start  42: vf2_player_1fee4
5/7 Test  #42: vf2_player_1fee4 .................   Passed    0.02 sec
    Start  43: vf2_player_1ff0c
6/7 Test  #43: vf2_player_1ff0c .................   Passed    0.02 sec
    Start 130: vf2_phase_2_5_refused_audit
7/7 Test #130: vf2_phase_2_5_refused_audit ......   Passed    1.13 sec
100% tests passed, 0 tests failed out of 7
```

## What's NOT in this slice

- **0x1fffc, 0x4b410, 0x2eab8, 0x11704 sub-callees.** These are
  the remaining 4 sub-callees of 0x1fcc0 (per v0748 boundary
  note). Each is a separate slice.
- **0x1fcc0 itself** (548 B, 15 blocks, 5 sub-callees). The full
  function. v0748 boundary note documents the path.
- **Wiring into the dispatcher chain.** Same challenge as
  v0741/v0745/v0746/v0747 (per-step hook infrastructure
  needed).
