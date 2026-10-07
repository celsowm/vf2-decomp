# v0743: Phase 2.4 — the 7-instruction a5=4 delta IS a gated stos

**Slice 5 attempt of `advance_plan_v0735.md`.** This is a
**measurement slice**, not a recovery. The 7-instruction delta
that v0732g pinned as a measured constant (`preset == 0 ? 4193 : 4186`)
is now mechanically explained.

**No `src/` change. No `functions.csv` change.** The recovered
`phase17_bit7_index5` (in `src/recovered/texture_bridge_match.c:9269-9294`)
keeps the measured-constant rule; this note documents the
mechanical cause but does not yet pin it in C (the next session
should pick that up).

## Measurement

Two `vf2probe --trace` runs from `out/sixth-fresh.vf2snap`-family
snapshots, stopping at the proven `0x164c4` boundary:

| snapshot | preset | total steps | body |
|---|---|---|---|
| `out/f2r4-k-rel.vf2snap` | 0 | 4671 | 4193 |
| `out/f2r4-a-c.vf2snap` | 1 | 4664 | 4186 |

The 7-step delta matches the v0732g measurement.

## First IP-level divergence

Both traces are identical through step 2653 (`ip_before=0x60dbc`).
At step 2654 they diverge:

```
preset=0  ip_before=0x60dc0  ip_after=0x60dc4   (walk through)
preset=1  ip_before=0x60ddc  ip_after=0x60de0   (jump over)
```

The conditional is at `0x60dbc`:

```text
00060db8  and      15, r6, r9          # r9 = r6 & 0xf
00060dbc  cmpibne  1, r9, 0x00060ddc   # if r9 != 1, jmp 0x60ddc
00060dc0  addo     31, 16, r7          # <-- 7-instruction body
00060dc4  shlo     7, r8, r15
00060dc8  addo     r7, r7, r14
00060dcc  addo     r15, r14, r15
00060dd0  shlo     24, 1, r14
00060dd4  addo     r15, r14, g9
00060dd8  stos     r10, (g9)           # the gated write
00060ddc  addo     r12, r9, r9         # continuation
```

So the 7-instruction delta is **exactly the `stos r10, (g9)` body
that gets skipped** when the dispatch code's lower nibble is not 1.

## The gate: where r6 comes from

Just before the conditional:

```text
00060d20  and      15, r3, r4          # r4 = r3 & 0xf
00060d24  ld       0x000614c4[r4*4], r5  # r5 = 4-byte table lookup
00060d2c  mov      24, r8
00060d30  ldob     (r5), r6            # r6 = first byte of pointed-to address
```

So the dispatch code is:

```c
r6 = *(uint8_t*)(*(uint32_t*)(0x614c4 + (r3 & 0xf) * 4));
gate = (r6 & 0xf) == 1;
```

At preset=0: the indirect byte has lower nibble 1, so the body runs.
At preset=1: the indirect byte has lower nibble != 1, so the body is skipped.

## The body (7 instructions)

```text
00060dc0  addo     31, 16, r7        # r7 = 47
00060dc4  shlo     7, r8, r15        # r15 = r8 << 7
00060dc8  addo     r7, r7, r14       # r14 = 2*r7 = 94
00060dcc  addo     r15, r14, r15     # r15 = (r8 << 7) + 94
00060dd0  shlo     24, 1, r14        # r14 = 0x01000000
00060dd4  addo     r15, r14, g9      # g9 = (r8 << 7) + 94 + 0x01000000
00060dd8  stos     r10, (g9)         # *(uint16_t*)g9 = (uint16_t)r10
```

In C:

```c
if ((*(uint8_t*)(*(uint32_t*)(0x614c4 + (r3 & 0xf) * 4)) & 0xf) == 1) {
    uint32_t g9 = (r8u << 7) + 94u + 0x01000000u;
    *(uint16_t *)g9 = (uint16_t)r10;
}
```

## What the body IS

`g9 = (r8 << 7) + 94 + 0x01000000` is a write into video RAM at a
specific tile position (the 0x01000000 is the Model 2A video RAM
base; the (r8 << 7) + 94 is a row×stride + column calculation).
The body stores a 16-bit value `r10` at that tile position. This
is **the chute-slot write** that v0732c corrected from slot 2 to
`0x61550 + 2*preset`: at preset=0 the write happens (one extra
tile), at preset>=1 the write is skipped (no extra tile), which
matches the v0732c measurement that row 24's count is 34 vs 33
and the total is 417 vs 416.

## What this slice does NOT cover

- **Why preset affects the dispatch code.** The 16-entry table at
  `0x614c4` is populated by some prior code. Why preset=0 yields
  a byte with lower nibble 1, while preset=1 yields a different
  byte, is a separate question. The next session should trace
  the table population (probably in the row-4 edit cycle).
- **The full row-4 body** (the 4193 vs 4186 measurement). The
  7-instruction delta explains 7 of the difference, but the
  remaining 4186 instructions are not analysed here.
- **Native C recovery.** The recovered `phase17_bit7_index5` keeps
  the measured-constant rule. Implementing the C-level gated
  `stos` is the natural next step but requires recovering the
  table-population code (above bullet).

## What's still left after this slice

- The next session can implement the C-level gated `stos` once
  the table population is understood. The signature is:
  `if ((*(uint8_t*)(*(uint32_t*)(0x614c4 + (r3 & 0xf) * 4)) & 0xf) == 1) { *(uint16_t *)((r8u << 7) + 94u + 0x01000000u) = (uint16_t)r10; }`
- Or, more conservatively, keep the measured-constant rule and
  add a comment pointing to this note as the mechanical cause.

## Validated

- `git diff --stat src/`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- The recovered code at `texture_bridge_match.c:9269-9294` is
  unchanged.
- The pinned 4193/4186 measurement is unchanged.
- ctest #125 `vf2_phase_2_5_refused_audit`: PASSES.
- ctest #38 `vf2_player_29598`: PASSES (v0741).
