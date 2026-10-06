# v0734h: gap characterisation on the 94-row v0734b table

**Phase 1.3.** A pure disassembly pass on the largest known gaps and the two
container-masked gaps. No `functions.csv` rows are touched — those belong to
Phase 1.2a and are *decisions*, not measurements.

**The plan placed Phase 1.3 behind Phase 1.2a** (completion_plan_v0734.md):
container spans absorb code that has no row of its own, so the gap arithmetic is
a lower bound. The 144 B and 28 B figures are still real unknowns, but every
gap figure in the table is contaminated by the seven >20 KB container rows. The
gaps measured here are **provisional**, and Phase 1.2a must settle before any
of them is quoted as the final word.

## The eight gap runs, characterised

Recomputed on the v0734b table (94 ranged rows, 317,828 B merged union,
273,220 B double-counted overlap, 449,692 B merged span). Full list, every gap:

0x4ec00..0x640f4   87,284 B   <- largest gap; mid-body of a single function
0x658a4..0x6ca64   29,120 B   <- second largest
0x6428c..0x657dc    5,456 B
0x4d2c0..0x4e808    5,448 B   <- from the v0734b split
0x6cb0c..0x6dcb8    4,524 B
0x6cad0..0x6cae0       16 B
0x6ca78..0x6ca84       12 B
0x52c..0x530            4 B

Plus the two that the gap arithmetic does not see at present (still inside a
container span, unmasked until 1.2a):

0x1200..0x1290        144 B   <- real code, masked by `interrupt_return_wait_exit`
0x12bc..0x12d8         28 B   <- real code, masked by `interrupt_return_wait_exit`

## `0x4ec00..0x640f4` (87,284 B) — the largest gap

First 12 instructions decoded from `0x4ec00`:

```text
0004ec00  mov      r14, g0
0004ec04  call     0x0004ec0c
0004ec08  bx       0x00000008(r14)
0004ec0c  mov      g0, r11
0004ec10  ldob     (r11), r8
0004ec14  ldob     0x00000001(r11), r9
0004ec18  ldob     0x00000002(r11), r6
0004ec1c  ldob     0x00000003(r11), r7
0004ec20  addo     4, r6, r12
0004ec24  addo     4, r7, r13
0004ec28  mulo     r12, r13, r5
0004ec2c  lda      0x00000140[r5*2], r4
```

The `0x4ec00..0x4ec08` triple is the **i960 standard epilogue**:
`mov r14, g0; call +4; bx 8(r14)` saves the caller's g0, runs a no-op leaf at
`0x4ec0c`, then jumps via the static-return table at `g0+8`. So `0x4ec00` is the
**tail of a function whose entry is somewhere before 0x4ec00**. The sweep stops at
the epilogue because nothing in this gap is an entry reachable from any known
root. The actual body is `0x4ec0c..0x640cc`: a poly-cluster consumer that walks a
record at `g0`, looks up an entry in a table at `0x140[r5*2]` and continues into
a further function at `0x5015c`.

Last 8 instructions at `0x640d0` **fail to decode** — i.e. `0x640f4` is the
*first* address in the next function (real entry, not tail). The byte at
`0x640f4` itself is a real function entry — `functions.csv` carries
`tile_controller_update` ending at `0x4ec00` (the v0734b repair) and the next
row starts at the gap's end.

Mid sample at `0x56800` (inside the gap):

```text
00056800  cmpibge  25, r4, 0x00056818
00056804  mov      0, r4
00056808  b        0x00056818
0005680c  subi     1, r4, r4
00056810  cmpible  0, r4, 0x00056818
00056814  mov      25, r4
00056818  ldob     0x000001b0(g8), r5
0005681c  cmpobne  r5, r4, 0x00056830
00056820  cmpobg   13, r4, 0x0005682c
00056824  subo     13, r4, r4
00056828  b        0x00056830
0005682c  addo     13, r4, r4
00056830  stob     r4, 0x000001b0(g7)
```

A modular clamp at offset `0x1b0` from g7/g8, with a comparison-and-fold
against `25` and `13`. The shape is consistent with a poly-index / slot-clamp
operation — common across the poly cluster family.

**What this slice knows and what it does not.**

- **Knows**: this gap is one (or two) functions, NOT padding; the sweep
  reaches into the high but does not include its entry; the body is a
  poly-cluster-style consumer with table lookups and modular clamping.
- **Does not know**: the function's *entry* address — necessary for a row.
- **Does not claim**: that the `0x240..0x4ec00` range contains no other
  function. A 6 KB gap from the v0734b split (`0x4d2c0..0x4e808`, 5,448 B) sits
  immediately below this one and may or may not be a continuation.

## `0x658a4..0x6ca64` (29,120 B) — second largest

Not disassembled here. The gap appears to be in the poly cluster family by
address region, but is left for a focused slice.

## `0x6428c..0x657dc` (5,456 B) and `0x6cb0c..0x6dcb8` (4,524 B)

Also not disassembled here. Both are small enough that a focused slice can do
them at the same time as the 29,120 B run.

## `0x4d2c0..0x4e808` (5,448 B) — the v0734b split

This gap **did not exist** in the v0733g-compositioned table: it was a 93,748 B
run from `0x4d2c0..0x640f4`. `tile_controller_update`'s v0734b-measured extent
ending at `0x4ec00` split it. It is mostly inside the largest gap above; the
5,448 B that is genuinely outside `tile_controller_update` is small enough to
be a single function on its own. **Not disassembled here.**

## `0x6cad0..0x6cae0` (16 B), `0x6ca78..0x6ca84` (12 B), `0x52c..0x530` (4 B)

All three are inside other rows' spans (small integers of bytes that fall
between adjacent rows). The 16 B and 12 B runs are between `0x6ca64..0x6cad0`
and `0x6ca84..0x6cae0`; the 4 B is between `0x52c..0x530`. Likely padding /
boundary between adjacent functions.

## Container-masked runs

### `0x1200..0x1290` (144 B) — inside `interrupt_return_wait_exit`'s bogus 66 KB span

First 12 + last 12 instructions:

```text
00001200  ld       0x00500700, r8
00001208  ld       0x00500704, r9
00001210  ld       0x005001dc, r11
00001218  bbs      3, r8, 0x00001228
0000121c  bbs      27, r8, 0x00001228
00001220  bbc      24, r8, 0x00001268
00001224  bbc      25, r8, 0x00001268
00001228  cmpobe   0, r11, 0x00001270
0000122c  lda      0x00f7f700, r3
00001234  and      r3, r9, r9
...
0000125c  setbit   14, r15, r15
00001260  st       r15, 0x00508000
00001268  mov      0, r11
0000126c  b        0x00001278
00001270  lda      0x00001284, r11
00001278  st       r11, 0x005001dc
00001280  ret
```

A 32-instruction function. Loads `0x00500700` / `0x00500704` / `0x005001dc`,
branches on bit flags of `r8`, masks `r9` with `0x00f7f700`, sets bit 14 of the
flag word at `0x00508000` on the failure path, stores `r11` to `0x005001dc`,
returns. The shape is consistent with a search-loop or post-parse validator
that scans the record at `0x005001dc` and updates a status register. **Not a
gap of unknowns**: it is one function, two columns, with no row to attach it to
because `interrupt_return_wait_exit`'s row is the 68 KB thing at
`0x00000d20..0x00010fa4` whose real extent is one bare `ret`.

The byte at `0x1284` does not decode — i.e. `0x1290` is the next real entry.

### `0x12bc..0x12d8` (28 B)

First 7 instructions:

```text
000012bc  mov      2, r15
000012c0  stib     r15, 0x005000fc
000012c8  mov      2, r15
000012cc  stib     r15, 0x005000fd
000012d4  ret
```

Seven instructions: store byte 2 to `0x005000fc`, store byte 2 to `0x005000fd`,
return. A two-byte probe or sentinel writer. **Not a gap**: it is one function
(28 B = 7 instructions), no row to attach it to.

The byte at `0x12d8` is `subo 1, 0, g0` — code, the start of the next
function.

## What this slice is *not*

This is not recovery. Nothing was added to `functions.csv`. Every gap figure
above remains a **lower bound on the real unknown** until Phase 1.2a decides
which container rows shrink to their measured extents.

The 87 KB largest-gap characterisation is the only one that runs to a function
boundary by disassembly. The 29 KB, 5 KB and 4 KB gaps are characterised by
size and region only — disassembly pass for those is left as separate focused
slices.

## Validated

- `vf2i960 disasm` succeeds on `0x4ec00..0x640cc` (87 KB): every instruction
  decoded except the tail byte at `0x640d0` (the first byte of the next
  function), confirming the gap is real code, not padding.
- `vf2i960 disasm` succeeds on `0x1200..0x1280` (144 B): every instruction
  decoded except the tail byte at `0x1284`, same confirmation.
- `vf2i960 disasm` succeeds on `0x12bc..0x12d4` (28 B): every instruction
  decoded.
- No `src/` change. `git diff --stat src` is empty.
- No `functions.csv` change.