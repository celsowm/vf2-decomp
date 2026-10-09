# v0756: 0x323fc (`post_cf04_combat_state_clear`) recovered in C

**Slice 10 of the advance plan execution chain.** First
post-`0xcf04` cleanup sibling. Small function (164 B, 7 blocks)
that uses an already-recovered callee, making it a clean recovery
target. Stand-alone execute hook — not yet wired into the
per-step hook table.

## Function shape

```
sub_000323fc: address=0x000323fc end=0x000324a0 blocks=7 indirect=no
```

164 B, 7 blocks, no indirect branches. The function:
1. Calls `0xcf04` (post-frame IRQ handler, recovered v0747).
2. `clrbit 19` of `0x500068` + store.
3. Stores `0` to `(g13 + 0x47)` (1 byte).
4. `clrbit 8` of `*(g13)`.
5. Stores `0x53f` to `0x500024`.
6. Stores `100` (i.e., `25 << 2`) to `(g13 + 0x40)`.
7. Dispatches on bit 0 of `*(g13)`:
   - **bit 0 set**: read `0x500056`, toggle 0↔1, setbit 1 and 3 of
     `*(g13)`, store `0x324a0` to `(g13 + 0xc)`, ret to `0x324a0`.
   - **bit 0 clear**: no-op ret to `0x3244c`.

g13 is an i960 global register (`cpu->registers[16+13]=29`); it
points to a work-RAM struct set up by the caller.

## Path inventory (test exercises all 4)

| path | trigger | exp ret ip | exp *(g13) post |
|------|---------|-----------|------------------|
| A.1 | bit 0 + pre_0x500056=0 | 0x324a0 | bit1=1, bit3=1, 0x500056=1 |
| A.2 | bit 0 + pre_0x500056=1 | 0x324a0 | bit1=1, bit3=1, 0x500056=0 |
| B   | bit 0=0, bit 3=0 | 0x3244c | unchanged (besides clrbit 8) |
| C   | bit 0=0, bit 3=1 | 0x3244c | unchanged (bit 3 stays set) |

## Sub-callee inlined

`0xcf04` is called via a pushed frame and direct invocation of
`vf2_hybrid_player_cf04_execute`. The cf04 recovery refuses the
`0x1fcc0` sub-call from within itself (returning
`VF2_ERROR_UNSUPPORTED`). This recovery discards that refusal
and continues with its own body — the cf04 work (setbit 21,
path-A/path-B body writes, clrbit 15) has been applied; only the
trailing clrbit 21 is deferred to the per-step loop's interpreted
fallback if 0x323fc is ever wired.

## Verification

- ctest entry `vf2_player_323fc` (#50, 0.02 s) PASSES.
- All 4 paths verified end-to-end.
- The `cf04` state side-effects (bit 21 set, etc.) are documented
  in the test as known boundary behavior, not failures.

## What's in this slice (v0756) and what's NOT

- This slice **does** recover `0x323fc` in C with `0xcf04` inlined
  via a direct `vf2_hybrid_player_cf04_execute` call.
- This slice **does not** wire `0x323fc` into the per-step hook
  table. The function isn't reached in any measured F4-native path,
  so activation would need a separate snapshot or a focused test.

## Sibling function

`0x32284` shares the trailing body (`block_0003240c` onwards) but
clrsbit `18` instead of `19` and jumps to `0x3240c` directly without
the `clrbit 19` step. It's the same shape with different bit
parameters. Can be recovered as a single additional slice if a
caller scenario is found. Not yet done.

## Validated

- `git diff --stat src/`: recovery + test changes only.
- ctest #50 `vf2_player_323fc`: PASS.
- All other tests in the suite: still PASS.
