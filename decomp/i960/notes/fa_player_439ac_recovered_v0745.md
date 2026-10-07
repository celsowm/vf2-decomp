# v0745: 0x439ac recovered in C (Phase 3, second callee of 0x29414)

## Status

`vf2_hybrid_player_439ac_execute` is a recovered C implementation
of the queue dedup-append function at `0x000439ac`, the second
callee of `0x00029414` (per `fa_player_29414_callees_boundary_v0737.md`).
All four paths are recovered: queue-full early return, dedup match
at slot[1], dedup match at deeper slot, and append.

The function is **not** yet wired into the dispatcher chain; it
exists as a standalone execute hook like the v0741 0x29598. Wiring
is a separate slice (see `fa_player_29598_wiring_v0744.md`).

## Disassembly (per `vf2i960 disasm`)

```
000439ac  ldob     0x0050406a, r3            # r3 = count
000439b4  cmpoble  4, r3, 0x000439f8         # if r3 >= 4, ret
000439b8  mov      g0, r4                    # r4 = search key
000439bc  addi     1, r3, r3                 # r3 = count + 1
000439c0  ld       0x00504074[r3*4], r5      # r5 = read_slot[r3]
000439c8  cmpobe   r4, r5, 0x000439f8        # if r5 == r4, ret (idempotent)
000439cc  cmpdeco  1, r3, r3                 # r3--; if r3 == 0, fall
000439d0  bl       0x000439c0               # else loop
000439d4  ldob     0x0050406a, r3            # reload count
000439dc  st       r4, 0x00504078[r3*4]      # write_slot[count] = r4
000439e4  ldib     0x0050406a, r15
000439ec  lda      0x00000001(r15), r15
000439f0  stib     r15, 0x0050406a           # count++
000439f8  ret
```

## Data layout

Two tables, 4 bytes apart, both 4-byte aligned, indexed by `r3*4`:

| table | base | role |
|---|---|---|
| read slots | `0x00504074` | walked during search |
| write slots | `0x00504078` | appended to on miss |

`r3` runs from `(count + 1)` down to `1` in the search; the write
goes to `write_slot[count]`. Both tables share the count at
`0x0050406a` (1B). The 4-byte gap between the read and write
tables means the "search read slot[1]" hits the same address as
"write slot[0]" — a 4-byte ring of "current read head" and
"current write tail".

## Paths recovered

| path | body count | conditions | result | C path |
|---|---|---|---|---|
| A | 3 | count >= 4 | early return | `if (r3 >= 4) { ret; }` |
| B | variable | match at slot[1] | idempotent return | early return inside loop |
| C | variable | match at slot[k] | idempotent return | early return inside loop |
| D | variable | no match | append + count++ | `write_slot[count] = r4; count++` |

The test exercises A, B (match at top), C (append from empty), and
D (match at slot[1] with count=2 walking slots 3, 2, 1).

## ctest

- New entry: `vf2_player_439ac` (ctest #39, 0.01 s)
- Wires up `vf2_hybrid_player_439ac_execute` via `CMakeLists.txt:430-434`
- Header decl in `include/vf2/hybrid/player.h:46-55`
- Test fixture:
  - Uses `vf2_i960_cpu_enter_procedure` to push a fake frame with
    return_address = `0x439f8` (the function's `ret`).
  - Sets count and read/write slots via direct `vf2_model2a_write`.
  - Sets g0 via direct register write.
- Path A: count = 4 -> 1 (ldob) + 1 (cmpoble taken) + 1 (ret) = 3 ins.
- Path B: count = 0, slot[1] = g0 -> return at first cmpobe.
- Path C: count = 0, no match -> write_slot[0] = g0; count becomes 1.
- Path D: count = 2, key at slot[1] -> return at second cmpobe (slot_index=1).

## What's NOT in this slice

- **Wiring into the dispatcher chain.** The same wiring challenge
  as v0741: the function lives in the `0x28178..0x14400` range
  that the post-29414 dispatcher interprets as a single segment.
  The per-IP hook infrastructure (v0745 candidate A from
  v0744) is needed.
- **0xcf04 and 0x43888 callees.** Per v0737, these are the other
  two callees of 0x29414 path D. 0xcf04 (184 B) has a sub-call
  to 0x1fcc0 (548 B, `display_profile_apply`) that would need
  recovery or refuse-handling. 0x43888 (200 B) is self-contained
  and could be a v0746 candidate.

## Counter

- `src/recovered/hybrid.c`: +180 lines (the function and execute hook)
- `include/vf2/hybrid/player.h`: +10 lines (declaration)
- `tests/recovered/test_player_439ac_native.c`: NEW, 174 lines
- `CMakeLists.txt`: +5 lines (add_executable + add_test)
- ctest: +1 (`vf2_player_439ac`, ctest #39)

## Evidence

```
$ ctest --test-dir build -C Debug -R 'vf2_player_439ac$' --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start 39: vf2_player_439ac
1/1 Test #39: vf2_player_439ac .................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
```

ctest #38 `vf2_player_29598` (v0741) and ctest #125
`vf2_phase_2_5_refused_audit` (v0736) still PASS.
