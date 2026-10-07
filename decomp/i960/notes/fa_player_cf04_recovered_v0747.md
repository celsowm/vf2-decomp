# v0747: 0xcf04 recovered in C (Phase 3, fourth callee of 0x29414) - dead code found

## Status

`vf2_hybrid_player_cf04_execute` is a recovered C implementation
of the post-frame IRQ handler at `0x0000cf04`, the fourth callee
of `0x00029414` (per `fa_player_29414_callees_boundary_v0737.md`).
Both paths are recovered (A: bit 15 set, B: bit 15 clear). The
sub-call to `0x1fcc0` (`display_profile_apply`, 548 B, not yet
recovered) is **REFUSED** at the call site, and the dispatcher's
interpreted fallback runs the call and the trailing clrbit 21.

**Notable finding**: the `addo 1, r3, r3` at `0xcf38` is **dead
code**. The prologue's `setbit 21` at `0x500068` is unconditional,
and the `bbs 21, r15, 0xcf3c` at `0xcf34` reads `0x500068` *after*
the setbit. So the branch is always taken (skip the addo), and
r3 = initial_value % 11 (no increment ever happens). The
disassembly shows both paths, but only one is live.

The function is **not** yet wired into the dispatcher chain; it
exists as a standalone execute hook like v0741/v0745/v0746.

## Disassembly (per `vf2i960 disasm`, 184 B, 6 blocks)

```
0000cf04  ld       0x00500068, r15
0000cf0c  setbit   21, r15, r15         # prologue: set bit 21
0000cf10  st       r15, 0x00500068
0000cf18  ld       0x00500068, r15
0000cf20  bbc      15, r15, 0x0000cf64  # if bit 15 clear -> path B
0000cf24  ldob     0x0050005b, r3
0000cf2c  ld       0x00500068, r15      # re-read AFTER setbit 21
0000cf34  bbs      21, r15, 0x0000cf3c  # always taken (bit 21 set by prologue)
0000cf38  addo     1, r3, r3            # DEAD CODE
0000cf3c  remo     11, r3, r3
0000cf40  stob     r3, 0x0050005b
0000cf48  stob     r3, 0x00500064
0000cf50  ld       0x0050a700, r15
0000cf58  st       r15, 0x0050a00c
0000cf60  b        0x0000cf8c

0000cf64  ldob     0x00500054, r3
0000cf6c  ldob     0x00012508[r3*2], r3
0000cf74  stob     r3, 0x00500064
0000cf7c  ld       0x0050a704, r15
0000cf84  st       r15, 0x0050a00c

0000cf8c  ld       0x00500068, r15
0000cf94  clrbit   15, r15, r15
0000cf98  st       r15, 0x00500068
0000cfa0  call     0x0001fcc0           # REFUSED
0000cfa4  ld       0x00500068, r15
0000cfac  clrbit   21, r15, r15
0000cfb0  st       r15, 0x00500068
0000cfb8  ret
```

## Why the addo 1 is dead code

The prologue at `0xcf04-0xcf10` writes bit 21 to `0x500068`
*unconditionally*. The `bbs` at `0xcf34` re-reads `0x500068` and
tests bit 21 — which is now set. So the `bbs` always branches to
`0xcf3c`, skipping the `addo 1, r3, r3` at `0xcf38`. The `addo`
is unreachable. This is preserved in the C recovery (the `bbs`
check is still there, but the increment never happens).

## Paths recovered

| path | conditions | effect |
|---|---|---|
| A | bit 15 of 0x500068 set | r3 = *(0x50005b) % 11; store r3 to 0x50005b and 0x500064; write *(0x50a700) to 0x50a00c |
| B | bit 15 of 0x500068 clear | r3 = *(0x50054); r3 = *(0x12508 + r3*2); store r3 to 0x500064; write *(0x50a704) to 0x50a00c |
| common | (always) | clrbit 15 of 0x500068; REFUSE call to 0x1fcc0; clrbit 21 NOT applied (interpreted fallback) |

## ctest

- New entry: `vf2_player_cf04` (ctest #41, 0.02 s)
- Wires up `vf2_hybrid_player_cf04_execute` via `CMakeLists.txt:443-447`
- Header decl in `include/vf2/hybrid/player.h:68-83`
- Test fixture:
  - Attaches a small fake main_rom (0x30000 bytes) so path B's read
    of `0x1250e` (a MAIN_ROM address) succeeds.
  - Path A: r3 = 5 (or 10) -> mod 11 = 5 (or 10).
  - Path B: r3 = *(0x50054) = 3 -> r3 = *(0x12508 + 3*2) = 0x42.
  - All paths assert `VF2_ERROR_UNSUPPORTED` (the sub-call refuse).

## What's NOT in this slice

- **0x1fcc0 (`display_profile_apply`) recovery.** This is the
  548-byte function that the v0747 sub-call refuses. It's a
  sizable function on its own and would be a v0748 candidate.
- **Wiring into the dispatcher chain.** Same challenge as
  v0741/v0745/v0746 (per-step hook infrastructure needed).

## Counter

- `src/recovered/hybrid.c`: +165 lines (the function and execute hook)
- `include/vf2/hybrid/player.h`: +16 lines (declaration)
- `tests/recovered/test_player_cf04_native.c`: NEW, 200 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_cf04`, ctest #41)

## Evidence

```
$ ctest --test-dir build -C Debug -R "vf2_player_cf04$" --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start 41: vf2_player_cf04
1/1 Test #41: vf2_player_cf04 ..................   Passed    0.02 sec
100% tests passed, 0 tests failed out of 1
```

All 4 callees of 0x29414 now have ctest entries:
- v0741 `vf2_player_29598` (ctest #38)
- v0745 `vf2_player_439ac` (ctest #39)
- v0746 `vf2_player_43888` (ctest #40)
- v0747 `vf2_player_cf04` (ctest #41)
