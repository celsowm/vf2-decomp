# v0737: Phase 3 next-target boundary — the 4 callees of `0x29414`

**Slice 2 of `advance_plan_v0735.md`.** The `fa_player` recovered
corridor reaches `0x28918` and `0x29414` (both native since v0297 and
v0713 respectively). Both functions are recovered in C; this slice
characterises **what they call**, so a future recovery slice can pick
the smallest callee and write the diff.

**No `src/` change, no `functions.csv` change** — this is a
measurement slice like v0734k was. `block_coverage.py` is unaffected.

## What `0x29414` calls

| callee | end | blocks | size | shape | recovered? |
|---|---|---|---|---|---|
| `0x29598` | `0x295ec` | 7 | 84 B | bit-test dispatch continuation; calls 0xcf04, 0x439ac, 0x43888 | NO (sub_00029598) |
| `0xcf04`  | `0xcfbc`  | 6 | 184 B | (small intermediate) | NO |
| `0x439ac` | `0x439fc` | 6 | 80 B | (small intermediate) | NO |
| `0x43888` | `0x43950` | 10 | 200 B | `selector2_queue_entry` (named constant in `src/recovered/native_runtime_condition.c:9`) | NO |

`0x29598` is the smallest, called **twice** from `0x29414` (at
`0x29570` and `0x29588`). It is the natural first decomposition
target — it sets bit 20 of `0x500068` (per v0297's documented bit-19
family) and then calls the three sub-callees in sequence.

`0x43888` is the largest and is referenced by name in the recovered
code (`VF2_SELECTOR2_QUEUE_ENTRY = 0x00043888`) — it is queued by the
selector dispatcher and is a real function with semantic identity,
not just an unnamed stub.

`0x439ac` is a 80 B cluster adjacent to `0x43888` (offset `0x0124` in
the same 0x800-byte region). Both are in the SELECTOR2 family.

`0xcf04` is a 184 B cluster in the timer/clock region (the same
region as `interrupt_restore_prefix` measured at v0734h).

## The 0x29598 disassembly

```text
block_00029598:                                # entry
  00029598  shlo     4, 15, r14
  0002959c  and      r14, g0, g0                # r14 = g0 bit 4
  000295a0  cmpobe   0, g0, 0x000295e8          # if bit 4 clear, skip
block_000295a4:
  000295a4  ldob     0x000295ec(g1), r13        # load byte from 0x295ec+g1
  000295ac  bbc      r13, g0, 0x000295e4        # branch on bit
block_000295b0:
  000295b0  cmpobe   15, g1, 0x000295bc         # if g1 == 15
block_000295b4:
  000295b4  addo     1, g1, g1                  # g1 += 1
  000295b8  b        0x000295e8                 # exit
block_000295bc:                                # the g1 == 15 branch
  000295bc  ld       0x00500068, r15
  000295c4  setbit   20, r15, r15               # set bit 20
  000295c8  st       r15, 0x00500068
  000295d0  call     0x0000cf04                 # → 0xcf04
  000295d4  lda      0x00ad231f, g0
  000295dc  call     0x000439ac                 # → 0x439ac
  000295e0  call     0x00043888                 # → 0x43888
block_000295e4:
  000295e4  mov      0, g1                      # g1 = 0
```

A standard "test bit + loop iteration + dispatch" pattern. The
`setbit 20, 0x500068` is the documented v0297 bit-19-set signal (bit
20 of the same word — they're adjacent in the dispatcher's mask
family). The three calls then execute the routine.

## What a focused `0x29598` recovery slice would do

1. **Build a `vf2probe` scenario.** The natural entry is
   `out/player-14288-rt.vf2snap` (the v0297 fixture) with `0x500068`
   patched to set bit 4 of `g0` and `g1 = 15`. Take a
   `vf2probe --trace --memory-trace` to `0x295ec` (the function end).

2. **Pick the call site.** Both call sites in `0x29414` end with the
   same instruction count (the function is idempotent in `g1`'s value).
   The first call site (`0x29570`) is the easier one because the
   selector has not yet been consumed.

3. **Translate to C.** The body is:
   - `if (g0 & (1 << 4))` — skip
   - `r13 = *(uint8_t*)(0x295ec + g1)`
   - `if (!(g0 & (1 << r13)))` — branch
   - `if (g1 == 15)` — increment g1
   - else: setbit 20 in 0x500068; call 0xcf04; g0 = 0x00ad231f; call
     0x439ac; call 0x43888
   - return

4. **Native differential.** Run `vf2i960 native-resume` on the same
   scenario, compare register + memory state at `0x295ec`. Expect
   instruction count: ~14 if the bit-4-skip path, ~30 if the
   bit-20-set path.

5. **Wire as ctest entry** `vf2_player_29498` (or similar). Two
   scenarios: bit-4-clear (skip) and bit-4-set (full path).

**Estimated scope:** ~50-100 LOC of C in `src/recovered/hybrid.c`
plus a Python ctest entry. 1 commit, ~half-day wall time once the
scenario is on disk.

## What this slice does NOT cover

- **The 3 callees of `0x29598`** (`0xcf04`, `0x439ac`, `0x43888`)
  are not recovered in this slice. They each remain as original-i960
  continuations; the `0x29598` recovery still calls back into them
  through the native-resume fallback.
- **`0x28918` callees** (the curve evaluator) are not in this note —
  `0x28918` is already native per v0713, and its callees are the
  float subroutines documented in
  `fa_player_28918_live_v0713.md`. Those are outside this slice's
  scope.
- **Phase 4 simulation systems** (hitboxes/hurtboxes, collision,
  damage/combos, ring-out, fighter physics, CPU logic) are not in
  this slice.

## What the next slice should pick up

- **Recover `0x29598`** as the first `fa_player` callee decomposition.
  This is the natural next step and is the smallest body of the four.
- **Recover `0x43888`** (selector2_queue_entry) next. It is named in
  the recovered code (`VF2_SELECTOR2_QUEUE_ENTRY`) and is a known
  semantic identity, not just an unnamed stub.
- **Recover `0xcf04` and `0x439ac`** after — they are smaller but
  context-dependent.
- **The four `0x28918` callees** (float lerp subroutines) per the v0713
  note's "Gaps closed / rules found" section — separate focused
  slice.
- **Phase 4 simulation systems** after Phase 3 is closed.

## Validated

- All four callee extents measured by `vf2i960 function`.
- `0x29598` fully disassembled (the smallest callee, called twice).
- `git diff --stat src`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- The published percentage unchanged at 0.4456%.