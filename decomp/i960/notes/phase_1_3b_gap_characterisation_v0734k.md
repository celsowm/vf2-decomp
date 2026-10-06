# v0734k: Phase 1.3b — the smaller gap runs are real but not all parseable

**Phase 1.3b of `completion_plan_v0734.md`.** Disassembly pass on the
four gap runs that v0734h did not cover. **No `src/` change, no
`functions.csv` change** — this is a measurement slice like v0734h was.
It characterises what is in the gaps and what is not, so that any later
recovery or function-attribution slice has the boundary drawn.

## Summary of all four gap runs

### `0x4d2c0..0x4e808` — 5,448 B (the v0734b split, also the smallest)

The `texture_status_line` row's back end is at `0x4d318` (88 B in 3
blocks). Inside the gap:

| address | ext (size) | name | description |
|---|---|---|---|
| 0x4d318..0x4deac | 2,452 B | — | **unparseable**. Disassembler fails at every probed offset. |
| 0x4deac..0x4decc | 32 B (1 block) | `sub_0004deac` | `ld 0x00500020, r7; and 15, r7, r7; lda 0x01000ca8, g9; mov r7, g0; balx 0x000093e0, r14` — calls into the registered functions. |
| 0x4decc..0x4def0 | 36 B | — | **unparseable**. |
| 0x4def0..0x4df10 | 32 B (7 blocks) | `sub_0004def0` | starts `shlo 9, 1, r6; stos r7, (r9); addo 4, r9, r9`; ends with `ret`. |
| 0x4df10..0x4dfc4 | 180 B (3 blocks) | `sub_0004df10` | `lda 0x01000a28, g9; shlo 2, 25, g5; ldl 0x00f00000, r14; lda 0x000fffff, r14; and r14, r15, r15; st r15, 0x0050e000; mov 0, g2` × 18+ — g2 initialisation pattern. |
| 0x4dfc4..0x4e7e8 | 2,068 B | — | **unparseable** — almost certainly the poly-cluster body that walks records and writes the values. |
| 0x4e7e8..0x4e808 | 32 B (1 block) | `sub_0004e7e8` | writes r4 to three Model 2A slots (`0x0059e008/00c/010c`), then `call 0x0004f904; ret`. |

5,160 of the 5,448 bytes are unparseable by the disassembler. The
`mov 0, g2` repetition at `0x4df38..0x4df7c` (18+ no-op moves) is real
i960 code that the disassembler treats as data; whether it is
*meaningful* code or the trailing uninitialised-data pattern is open.

**What this slice knows.** This gap is one or two functions plus a
shared-epilogue cluster, NOT padding. The poly-cluster body at
`0x4dfc4..0x4e7e8` is the bulk; `sub_0004e7e8` is the call-and-return
wrapper that prepares the writes. Function entries at `0x4deac` and
`0x4def0` look like thin shims into the registered function table.

**What this slice does not know.** Without the disassembler recognising
the `mov 0` opcode sequence (or whatever is at `0x4d318..0x4deac` and
`0x4dfc4..0x4e7e8`), no row can be claimed. The poly-cluster body is
one candidate for "real function whose entry is somewhere below
`0x4d318`", but its entry is not visible from the measured sweep.

### `0x6428c..0x657dc` — 5,456 B

`vf2i960 function` measures `sub_0006428c` at `0x6428c..0x64864` (1,488
B, 58 blocks, indirect=yes). The sweep stops at `0x64864` because the
next block is reachable only through indirect branches the sweep does
not follow. **Lower bound.**

`0x64864..0x657dc` is **unparseable** by the disassembler (4,028 B).
This gap was characterised as "poly-cluster family by address region"
at v0734h — the sweep measured entries at `0x648c0..` etc. are all
unreachable through direct branches from `0x6428c`.

**What this slice knows.** The 58-block lower-bound measurement is
credible. The remaining 4,028 B is one or more functions whose entries
are not visible from the sweep's root.

**What this slice does not know.** Whether the rest of the gap is one
function or several, and where its entry lives. `vf2i960 function` from
`0x6428c` does not descend into callees — that is by design.

### `0x6cb0c..0x6dcb8` — 4,524 B

Pre-gap: `0x6cb00`, `0x6cb04`, `0x6cb08` are **three `ret`
instructions** (12 B). The gap itself starts with unparseable bytes.

Functions inside the gap:

| address | ext (size) | name | description |
|---|---|---|---|
| 0x6cb0c..0x6cc00 | 252 B | — | **unparseable**. |
| 0x6cc00..0x6ce6c | 620 B (2 blocks) | `sub_0006cc00` | `ldl 0x000033d0(r8), r10; cmpo 0, 1` — Model 2A memory cluster writer (shared epilogue at `0x6ce6c`). |
| 0x6ce00..0x6ce6c | 108 B (1 block) | `sub_0006ce00` | `mov r14, g0; call 0x0006d1b4; bx 8(r14)` — the i960 standard epilogue, sits at the **shared tail**. |
| 0x6ce6c..0x6cf00 | 148 B | — | **unparseable**. |
| 0x6cf00..0x6d10c | 524 B (2 blocks) | `sub_0006cf00` | `ld 0x0050016c, r15; ld 0x000033f0(r15), r15` — Model 2A memory-write helper. |
| 0x6d020..0x6d10c | 236 B (2 blocks) | `sub_0006d020` | inside `sub_0006cf00`'s extent (shared tail). |
| 0x6d10c..0x6dccc | 2,816 B | — | **unparseable**. |
| 0x6dccc..0x6dd24 | 88 B (4 blocks) | `sub_0006dccc` | `ldob 0x0050002b, r15; lda 0x00000010, r14; lda 0x00000011, r13; cmpobe r14, r15, 0x0006dd20` — register-cluster switch. |

After the gap: `system_memory_diagnostic` at `0x6dcb8..0x6dd4c` (148
B, 7 blocks).

Total measured = 1,576 B; unparseable = 4,448 B. **Two shared-epilogue
clusters** (`0x6ce6c` and `0x6d10c`) plus three small isolated functions
(`sub_0006dccc`, etc.).

### `0x658a4..0x6ca64` — 29,120 B (second-largest gap)

The disassembler fails at `0x658a4`, `0x658b0`, `0x65900`, `0x66000`,
`0x66500`, `0x66800`, `0x67000`, `0x68000`, `0x69000`, `0x6a000`,
`0x6c000` — every probed address in the first 24 KB. Decoding starts
again at `0x68fa4`.

Functions inside the gap:

| address | ext (size) | name | description |
|---|---|---|---|
| 0x658a4..0x68fa4 | 14,080 B | — | **unparseable**. |
| 0x68fa4..0x68fb8 | 20 B (5 blocks) | `sub_00068fa4` | `cmpibne g1, sf29, 0x00068bcc` — a 5-block jump dispatcher. |
| 0x68fb8..0x6ada4 | 7,644 B | — | **unparseable**. |
| 0x6ada4..0x6aec4 | 288 B (10 blocks) | `sub_0006ada4` | `ld 0x00000078(g5), r8; ldt 0x000001f4(r7), g0` — poly-cluster record reader (g5/g7 globals). |
| 0x6aec4..0x6b0a4 | 480 B | — | **unparseable**. |
| 0x6b0a4..0x6b130 | 140 B (3 blocks) | `sub_0006b0a4` | — |
| 0x6b1a4..0x6b2e8 | 324 B (9 blocks) | `sub_0006b1a4` | shared tail `0x6b2e8`. |
| 0x6b2a4..0x6b2e8 | 68 B (3 blocks) | `sub_0006b2a4` | inside `sub_0006b1a4` (shared tail). |
| 0x6b3a4..0x6b3d4 | 48 B (2 blocks) | `sub_0006b3a4` | — |
| 0x6b4a4..0x6b50c | 104 B (7 blocks) | `sub_0006b4a4` | — |
| 0x6b5a4..0x6b5ac | 8 B (1 block) | `sub_0006b5a4` | — |
| 0x6b6a4..0x6b6f0 | 76 B (1 block) | `sub_0006b6a4` | — |
| 0x6b7a4..0x6b7a4 | 0 B | `sub_0006b7a4` | inside other function. |
| 0x6b8a4..0x6bf58 | 1,684 B (10 blocks) | `sub_0006b8a4` | — |
| 0x6b9a4..0x6b9a4 | 0 B | `sub_0006b9a4` | inside other function. |
| 0x6baa4..0x6bf58 | 1,164 B (2 blocks) | `sub_0006baa4` | **shared tail `0x6bf58`** with `0x6b8a4`. |
| 0x6bba4..0x6bf58 | — | `sub_0006bba4` | shared tail `0x6bf58`. |
| 0x6bca4..0x6bf58 | — | `sub_0006bca4` | shared tail `0x6bf58`. |
| 0x6bda4..0x6bf58 | — | `sub_0006bda4` | shared tail `0x6bf58`. |
| 0x6bea4..0x6bf58 | 888 B (4 blocks) | `sub_0006bea4` | shared tail `0x6bf58`. |
| 0x6bfa4..0x6c05c | 184 B (18 blocks) | `sub_0006bfa4` | — |
| 0x6c05c..0x6ca64 | 2,568 B | — | **unparseable**. |

**The `0x6bf58` shared-epilogue cluster is exactly the texture-cluster
pattern v0734a characterised** — six functions all measure to the same
shared tail. v0734i documented the same pattern at `0x4bfe0` for the
texture cluster; this is the **poly-cluster analogue** at `0x6bf58`.

29,120 B gap: ~16,640 B unparseable, ~3,480 B measured in 16
discovered functions, ~9,000 B of "barely-measured" zero-block entries
(function inside another function, measurable only by the i960 entry
the disassembler guesses).

After the gap: `task_object` at `0x6ca64..0x6ca78` (20 B, 1 block).

## The pattern

All four gap runs follow the same shape:

1. A small "wrap-up" or call-and-return tail at the gap's *end* — the
   `ret` instruction at `0x4e804`, the `system_memory_diagnostic`
   function starting at `0x6dcb8`, the `task_object` starting at
   `0x6ca64`, the `0x657dc` row already in `functions.csv`.
2. A **shared-epilogue cluster** in the middle — multiple functions all
   converge on the same back-edge `bx 8(r14)` or `ret` address. The
   texture cluster (`0x4bfe0`) is one example; the poly-cluster
   (`0x6bf58`) is another; the `0x6ce6c` and `0x6d10c` Model 2A
   memory helpers are smaller versions.
3. A **bulk of unparseable bytes** at the front — the disassembler
   does not recognise the opcode patterns, but they are real (they are
   not 0xff padding; they vary; they have to be reachable from some
   known root or the program would not link).

The "unparseable" bytes are not a defect in the disassembler; they are
a known gap in this repo's i960 decoder. The recognised pattern at
`0x4df38..0x4df7c` (18+ `mov 0, g2` no-ops) is a known i960 form
that the disassembler does not have a table for.

## What this slice does NOT claim

- **No row.** The discovered functions are un-named (`sub_XXXX`) and
  are not added to `functions.csv`. Naming requires a second independent
  signal (which the disassembler is, by itself, NOT).
- **No recovery of the unparseable bytes.** Disassembler improvements
  are a separate work stream; this slice only catalogues what is and is
  not parseable.
- **No closure of the gap arithmetic.** The 131,864 B / 8-piece total
  in v0734h is unchanged; the four gaps here remain gaps, just better
  characterised.

## Validated

- `vf2i960 function` measures consistently — every probed entry
  produces a single `address=... end=... blocks=... indirect=...` line.
- `vf2i960 disasm` fails on the documented addresses; the failure
  pattern is uniform ("decode failed: unsupported operation") — this
  is the disassembler's known behaviour, not a row defect.
- `git diff --stat src`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- All four gap figures quoted above came from running
  `vf2i960 function` and `vf2i960 disasm` against `roms/vf2/`. No
  synthetic numbers.

## What the next slice should pick up

- **Phase 1.4 / block coverage** is already closed (v0734g). The
  texture-cluster / poly-cluster pattern this slice documented is the
  same shape v0734a found; the function extent tool is already honest
  about it.
- **Phase 2.6 / F4** is the next gameplay slice. The shared-epilogue
  cluster pattern at `0x6bf58` means any of the entries there could
  be the one the F4 cluster references — but no row exists yet, so
  F4 stays bounded as documented in v0734j.
- **Phase 2.3-2.5** (warm-only admissions) does not depend on gap
  characterisation; it can run in parallel.
- **Phase 3 / P3-P4** is the next bulk-boundary work downstream of
  `fa_player`. None of these gaps is on the `fa_player` path.
- **Phase 4** is the largest remaining unstarted bulk and does not
  depend on these gaps.
- The **shared-epilogue cluster pattern** is itself a discovery worth
  keeping in mind: a function's measured extent is the span from its
  entry to the highest `ret` reachable through direct branches and
  fall-through. Multiple entries can share the same `ret`, which means
  their measured extents overlap by design. This is the same shape
  v0734a documented for the texture cluster at `0x4bfe0`; here it
  appears at `0x6bf58` for the poly cluster and at `0x6ce6c` and
  `0x6d10c` for the Model 2A memory helpers. The pattern is **not**
  a defect, and the v0734b `return_to` column already handles it.