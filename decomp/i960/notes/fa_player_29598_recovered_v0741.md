# 0x29598 recovered in C (v0741) — Phase 3 first decomposition

## Status

`vf2_hybrid_player_29598_execute` is a recovered C implementation of the
4-path dispatch at `0x00029598`, the first callee of `0x00029414` (per
`fa_player_29414_callees_boundary_v0737.md`).  Three of the four paths
are recovered; the fourth (path D, sub-calls) is **explicitly refused**
so the dispatcher chain can fall back to `hybrid_execute_interpreted_task`.

The function is **not** yet wired into the dispatcher chain; it exists
as a standalone execute hook that any future dispatcher code (e.g.
`hybrid_execute_player_post_29414`) can call when `cpu->ip == 0x29598`.
Wiring is a separate slice; this slice delivers the recovery + ctest
entry and proves the path semantics directly.

## Disassembly (per v0737)

```
00029598  shlo     4, 15, r14          # r14 = 0xf0
0002959c  and      r14, g0, g0         # g0 = g0 & 0xf0
000295a0  cmpobe   0, g0, 0x000295e8   # if g0 == 0, ret
000295a4  ldob     0x000295ec(g1), r13 # r13 = *(uint8_t*)(0x295ec + g1)
000295ac  bbc      r13, g0, 0x000295e4 # if bit r13 of g0 clear, jmp
000295b0  cmpobe   15, g1, 0x000295bc  # if g1 == 15, jmp to setbit path
000295b4  addo     1, g1, g1           # g1 += 1
000295b8  b        0x000295e8          # ret
000295bc  ld       0x00500068, r15     # PATH D: 3 sub-calls
000295c4  setbit   20, r15, r15
000295c8  st       r15, 0x00500068
000295d0  call     0x0000cf04
000295d4  lda      0x00ad231f, g0
000295dc  call     0x000439ac
000295e0  call     0x00043888          # VF2_SELECTOR2_QUEUE_ENTRY
000295e4  mov      0, g1
000295e8  ret
```

## Paths recovered

| path | body count | conditions | result | C path |
|---|---|---|---|---|
| A | 3 | g0 bit 4 clear | skip -> ret | `if ((g0 & 0x10) == 0) { ret; }` |
| B | 4 | g0 bit 4 set, bit r13 of g0 clear | mov 0, g1 -> ret | `if ((g0 & (1<<r13)) == 0) { g1=0; ret; }` |
| C | 5 | g0 bit 4 set, bit r13 of g0 set, g1 != 15 | addo 1, g1 -> ret | `g1 += 1; ret;` |
| D | 8 | g0 bit 4 set, bit r13 of g0 set, g1 == 15 | REFUSED (sub-calls) | `return VF2_ERROR_UNSUPPORTED;` |

The sub-calls in path D (0xcf04, 0x439ac, 0x43888) are not yet recovered
in this slice. The function does apply the `setbit 20` at 0x500068 and
the `lda 0xad231f, g0` (those are the side-effects that callers can
rely on) before returning `VF2_ERROR_UNSUPPORTED`, so a future
dispatcher that calls this function and falls back on
`VF2_ERROR_UNSUPPORTED` does not double-apply those side-effects (it
would interpret the path from 0x295bc onward in that case).

## ctest

- New entry: `vf2_player_29598` (ctest #38, 0.02 s)
- Wires up `vf2_hybrid_player_29598_execute` via `CMakeLists.txt:419-428`
- Header decl in `include/vf2/hybrid/player.h:35-44`
- Test fixture:
  - Attaches a small (0x30000-byte) fake `main_rom` with the right
    bytes at 0x295ec, 0x295f1, 0x295fb for paths B/C/D. (MAIN_ROM is
    read-only in production; the fake rom buffer is read-only inside
    the test as well — the function's read does not actually mutate it.)
  - Uses `vf2_i960_cpu_enter_procedure` to push a fake frame with
    return_address = 0x295e8 (the function's `ret`).
  - Sets g0, g1 via direct register writes.
- Path A: bit 4 of g0 clear -> 3 body + 1 ret = 4 instructions.
- Path B: g0 = 0x10, g1 = 0, r13 = 0xff at 0x295ec -> mov 0, g1; ret
  -> 4 body + 1 ret = 5 instructions, g1 = 0.
- Path C: g0 = 0x10, g1 = 5, r13 = 0x04 at 0x295f1 (bit 4 of 0x10 is
  set) -> addo 1, g1; b; ret -> 5 body + 1 ret = 6 instructions,
  g1 = 6.
- Path D: g0 = 0x10, g1 = 15, r13 = 0x04 at 0x295fb -> REFUSED,
  side-effects: 0x500068 bit 20 set, g0 = 0xad231f.

## What's NOT in this slice

- **Wiring into the dispatcher chain.** The dispatcher in
  `hybrid_execute_player_post_29414` (or its successor) currently
  falls through `hybrid_execute_interpreted_until` for the
  `0x28178..0x164c4` range that contains 0x29598. Adding a check
  `if (cpu->ip == 0x29598) { return vf2_hybrid_player_29598_execute(...); }`
  in that function is a separate change. It is also the only way
  the function can produce a measurable FULL MATCH on a real
  reference run — the unit test proves the path semantics but
  cannot run against the reference i960 program.

- **Path D (sub-calls).** Sub-callees 0xcf04 (184 B), 0x439ac (80 B)
  and 0x43888 (200 B, `VF2_SELECTOR2_QUEUE_ENTRY`) need separate
  recovery slices. They are not in scope here.

## Counter

- src/recovered/hybrid.c: +144 lines (the function and execute hook)
- include/vf2/hybrid/player.h: +9 lines (declaration)
- tests/recovered/test_player_29598_native.c: NEW, 175 lines
- CMakeLists.txt: +5 lines (add_executable + add_test)
- ctest: +1 (vf2_player_29598)

## Evidence

```
$ ctest --test-dir build -C Debug -R 'vf2_player_29598$' --output-on-failure
Test project D:/ia/vf2-decomp/build
    Start 38: vf2_player_29598
1/1 Test #38: vf2_player_29598 .................   Passed    0.02 sec
100% tests passed, 0 tests failed out of 1
```

ctest #125 `vf2_phase_2_5_refused_audit` still PASSES (1.32 s).
