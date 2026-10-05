# P2 step 2: the 0xd00 slot permutation is a ROM table; 12-vs-16 is a 3→4 expansion, not a gap (v0732o)

> **Superseded at v0732q in one clause only.** The permutation table, the
> source/destination strides and the "3 -> 4 expansion" reading are all correct
> and re-confirmed. What is added: the expansion's 4th word is **not** padding
> and **not** a constant — it is a memory read from a **ROM-resident** window
> at `0x020078b4` (the guest loads `r3 = 0x020078a8` at `0x2396c`), holding 30
> small positive floats in live state. The "one source word per slot is NOT
> covered by the triple" flag below is right, and now has its answer. See
> `p2_producer_contract_v0732q.md`.

**Resolves the open question in `p2_block_producer_consumer_v0732n.md`.** That
note flagged a "12 vs 16 stride discrepancy" as the next measurement. It is
measured now, and it is **not** a discrepancy — the two strides are the two
ends of a deliberate expansion.

## The table

The producer loop indexes a 30-byte permutation table inline in the ROM:

```text
00023984  ldob   0x0002394c[r9], r11
```

Read straight out of the memory trace (the `ldob` records carry their byte
values, so this needs no ROM image mapping):

```text
index  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19
value 01 00 02 05 04 07 06 08 0a 09 0c 0b 0d 03 10 0f 0e 13 12 11 15

index 20 21 22 23 24 25 26 27 28 29
value 14 18 17 16 1b 1a 19 1d 1c
```

```text
table length            30
sorted == range(30)     True
is a permutation        True
```

A **permutation of 0..29**, in shuffled order. It is read 60 times in the
trace - once per iteration for each of the two fighters - which is the first
independent confirmation that the producer runs twice on the same code.

## The destination formula predicts every access

`stq r4, 0x00000d00(g7)[r11*16]` with `r11` from the table gives 30 slot
starts, and those are exactly the 30 16-byte-aligned block starts:

```text
slot starts (30)   0xd00 0xd10 0xd20 0xd30 0xd40 0xd50 ...
predicted offsets actually observed   60 of 60
```

The 60 is 30 slots × 2 bases. So the addressing is **not** a mystery: the table
is a slot permutation, and `g7` is the fighter base.

## The 12-vs-16 question, answered

```text
slot bytes / iter     16   total dest bytes: 480
source stride / iter  12   total source bytes: 360
```

30 iterations, a **12-byte source stride** (`mulo 12, r9, r10` feeding
`ldt (r8)[r10]`) and a **16-byte destination slot**. 360 into 480.

**That is the answer, not a mismatch:** each iteration reads a packed 3-word
(12-byte) triple and writes it into a padded 4-word (16-byte) slot. The block
is 30 slots of 4 words, and 3 → 4 is exactly the expansion you would expect
from a packed-source / slot-destination staging loop.

My v0732n note read 360 ≠ 480 as an unresolved contradiction. It was a
description of the transform, and the only missing piece was the table that
explains the destination order. The note's proposed causes — "`r11` is not
`0..29`", "the destination stride is not uniform", "`ldt` writes more than
one destination word" — are all **falsified**: `r11` is a permutation of
`0..29`, the stride is uniformly 16, and one iteration does write a full
4-word slot.

## The block's shape, now settled

```text
30 slots x 16 bytes = 480 bytes per base
each slot          = 4 x 4-byte sub-offsets
union of sub-offsets = 120 offsets, contiguous at 4-byte stride
                     = 0x0d00 .. 0x0edc   (the last 4 bytes, 0x0ed8..0x0edf,
                       are the tail of the 30th slot's 16-byte alignment)
```

This is why the measured block is 120 contiguous 4-byte offsets rather than
30 obviously-segregated slots: adjacent 16-byte slots abut, so at 4-byte
granularity the array looks flat. The slot structure is real but invisible to
a 4-byte-granularity contiguity detector.

## Correction to the promoted layout

`p1_promoted_dualbase_layout_v0732m.md` records

```c
uint32_t field_0d00[120];   /* THE BLOCK */
```

That is **correct in extent and confirmed**, but it hides the slot structure.
The better-expressed form is:

```c
uint32_t field_0d00[30][4];  /* 30 padded 4-word slots, 12-byte-packed source */
```

Both are the same 480 bytes and the same 120 measured 4-byte offsets. The
`[30][4]` form is preferred because it matches the producer's iteration count
and the consumer's `setbit r3` indexing, and because it predicts where the
three loaded words land. The promoted-layout note is updated accordingly.

**No semantic name is added.** The producer is a gather-and-scatter with a
permutation; the consumer thresholds the result into a bitmask. That is a
shape, not a meaning. It does not make `field_0d00` a pose array, an
animation array, or a collision array.

## What is now recoverable, and what is not

The producer loop is **30 iterations of: read a permutation byte, load a
12-byte triple from `(r8)[r9*12]`, store it into slot `perm[r9]`.** That is a
small, fully determined block with a measured addressing rule and a
ROM-resident table. It is a genuine recovery candidate.

**Not yet proven:** no differential exists for `0x23984..0x239a8`, and the
consumer at `0x23a38` still needs its threshold constants and bitmask
accumulation verified register-by-register. Per the recovery lifecycle, the
producer can only be admitted after a controlled fixture matches the reference
on registers, memory and counters.

## Next step for P2, in order

1. Build a controlled fixture for `0x23984..0x239a8`: pin `g7` to one fighter
   base, `r8` to a synthetic 360-byte source, `r3` and the table page, and
   measure the destination image. That proves the gather/scatter and gives the
   recovery its differential.
2. Then the consumer `0x23a38`: the two float constants, the `cmpr` ordering,
   and the `setbit` accumulation into `(g13)+0x10c`.
3. Then the `0x281b0` / `0x28208` table heads that join at `0x2891c`.

## Anti-traps

- Do not read 360 vs 480 as a bug. It is a 3-word to 4-word expansion.
- Do not assume the block is a flat 120-word array. It is 30 padded slots; at
  4-byte granularity the slot structure is invisible.
- Do not hardcode the permutation. It is ROM data at `0x2394c`; the recovered
  block must read it, not embed it.
- Do not add a semantic name to `field_0d00`. Gather/scatter plus a bitmask
  consumer is a shape, not a meaning.
