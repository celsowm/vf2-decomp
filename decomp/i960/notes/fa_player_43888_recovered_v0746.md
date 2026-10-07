# v0746: 0x43888 recovered in C (Phase 3, third callee of 0x29414)

## Status

`vf2_hybrid_player_43888_execute` is a recovered C implementation
of the selector2 queue entry at `0x00043888`, the third callee
of `0x00029414` (per `fa_player_29414_callees_boundary_v0737.md`).
All six paths are recovered: special-case accept (g0 == 0x00ae101f),
gate-A accept ((0x50002c & 0xc) == 0), gate-A reject (byte bit 0
set), gate-B accept (0x500068 bit 20 clear), gate-B accept with
no g0 mutation (path E), and gate-B accept with g0 -= 0x20000
(path F). Plus a count-full negative control.

The function is **not** yet wired into the dispatcher chain; it
exists as a standalone execute hook like v0741 and v0745. Wiring
is a separate slice (see `fa_player_29598_wiring_v0744.md`).

## Disassembly (per `vf2i960 disasm`, 200 B, 10 blocks)

```
00043888  lda      0x00ae101f, r13
00043890  cmpobe   r13, g0, 0x000438ec    # path A: g0 == magic -> common
00043894  lda      0x0000000c, r3
0004389c  ld       0x0050002c, r13
000438a4  and      r13, r3, r3             # r3 = *(0x50002c) & 0xc
000438a8  cmpobe   0, r3, 0x000438c0      # gate A
000438ac  ld       0x0050016c, r15
000438b4  ldob     0x00003351(r15), r15    # r15 = *(0x50016c + 0x3351)
000438bc  bbs      0, r15, 0x0004394c     # path C: bit 0 set -> RET
000438c0  ld       0x00500068, r15
000438c8  bbc      20, r15, 0x000438ec    # gate B
000438cc  lda      0x00ff0000, r13
000438d4  and      r13, g0, r3             # r3 = g0 & 0x00ff0000
000438d8  lda      0x009e0000, r13
000438e0  cmpobne  r13, r3, 0x000438ec    # path E: not 0x9e -> common
000438e4  shlo     17, 1, r3              # r3 = 0x20000
000438e8  subo     r3, g0, g0             # path F: g0 -= 0x20000

# common (0x438ec..0x4394c):
000438ec  lda      0x00e80004, r6
000438f4  addo     31, 2, r3              # r3 = 33
000438f8  st       r3, (r6)               # poke 33 to 0xe80004 (twice)
000438fc  st       r3, (r6)
00043900  mov      16, r3
00043904  ldob     0x00504001, r5         # r5 = count
0004390c  cmpobge  r5, r3, 0x00043940     # if count >= 16, skip queue write
00043910  addo     1, r5, r5              # count++
00043914  stob     r5, 0x00504001
0004391c  ldob     0x00504003, r3         # r3 = ring index
00043924  st       g0, 0x00504020[r3*4]   # queue[index] = g0
0004392c  mov      15, r4
00043930  addo     1, r3, r3              # r3++
00043934  and      r4, r3, r3             # r3 = r3 & 0xf (ring wrap)
00043938  stob     r3, 0x00504003
00043940  lda      0x00000421, r3
00043944  st       r3, (r6)               # poke 0x421 to 0xe80004 (twice)
00043948  st       r3, (r6)
0004394c  ret
```

## Paths recovered

| path | body count | conditions | result |
|---|---|---|---|
| A | ~14 + ret | g0 == 0x00ae101f | accepted, queue write |
| B | ~14 + ret | (0x50002c & 0xc) == 0 | accepted, queue write |
| C | 7 + ret | gate A fails, byte bit 0 set | RET (no queue write) |
| D | ~14 + ret | gate A fails, byte bit 0 clear, 0x500068 bit 20 clear | accepted, queue write |
| E | ~14 + ret | gate A fails, byte bit 0 clear, 0x500068 bit 20 set, (g0 & 0xff0000) != 0x9e0000 | accepted, queue write, g0 unchanged |
| F | ~14 + ret | gate A fails, byte bit 0 clear, 0x500068 bit 20 set, (g0 & 0xff0000) == 0x9e0000 | accepted, queue write, g0 -= 0x20000 |
| count-full | ~10 + ret | (any accepted) count == 16 | no queue write, count unchanged |

## ctest

- New entry: `vf2_player_43888` (ctest #40, 0.02 s)
- Wires up `vf2_hybrid_player_43888_execute` via `CMakeLists.txt:436-440`
- Header decl in `include/vf2/hybrid/player.h:57-66`
- Test fixture:
  - Uses `vf2_i960_cpu_enter_procedure` to push a fake frame with
    return_address = `0x4394c` (the function's `ret`).
  - Sets g0, 0x50002c, 0x500068, 0x50016c, 0x504001, 0x504003 via direct
    `vf2_model2a_write`. The byte that path B/C/D/E/F tests against is at
    `0x50016c + 0x3351`; the test sets `0x50016c = 0x504000` (work RAM
    base) and writes the byte at `0x507351`.
  - Verifies the queue state: count, ring index, and queue[0] = g0 (or
    post-decrement g0 in path F).

## What's NOT in this slice

- **Wiring into the dispatcher chain.** Same challenge as v0741/v0745.
- **0xcf04 callee.** Per v0737, this is the fourth callee of 0x29414.
  It has a sub-call to 0x1fcc0 (548 B `display_profile_apply`) that
  needs to be refused, OR 0x1fcc0 needs to be recovered too. v0747
  candidate.

## Counter

- `src/recovered/hybrid.c`: +200 lines (the function and execute hook)
- `include/vf2/hybrid/player.h`: +10 lines (declaration)
- `tests/recovered/test_player_43888_native.c`: NEW, 240 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_43888`, ctest #40)

## Evidence

```
$ ctest --test-dir build -C Debug \
    -R "vf2_player_43888$|vf2_player_439ac$|vf2_player_29598$|vf2_phase_2_5_refused_audit$" \
    --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start  38: vf2_player_29598
1/4 Test  #38: vf2_player_29598 .................   Passed    0.02 sec
    Start  39: vf2_player_439ac
2/4 Test  #39: vf2_player_439ac .................   Passed    0.02 sec
    Start  40: vf2_player_43888
3/4 Test  #40: vf2_player_43888 .................   Passed    0.02 sec
    Start 127: vf2_phase_2_5_refused_audit
4/4 Test #127: vf2_phase_2_5_refused_audit ......   Passed    1.17 sec
100% tests passed, 0 tests failed out of 4
```
