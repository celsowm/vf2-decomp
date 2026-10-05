# P2: the `0x0d00` block's producer/consumer pair is two guest instructions (v0732n)

**Evidence only. No behaviour change, no tuple admitted, no field promoted.**
First step of P2: pin down *who* writes and reads the promoted block, so the
next decomposition has a named target instead of an address range.

## The block's access geometry, measured per base

`out/trace-both.jsonl`, base `0x510980` / `0x512980`:

```text
ip 0x0002399c   240 accesses, 240 distinct addresses   {write, 4} x240
ip 0x00023a38   240 accesses, 240 distinct addresses   {read,  4} x240
```

Both instructions touch the **identical** address set, and the sets are
**120 slots of 4 bytes per base** — 480 B, contiguous at 4-byte stride from
`0x0d00` to `0x0edc`. The 240 figure is 120 slots x 2 bases; each base gets
**240 writes and 240 reads** in the merged roll-up, i.e. 120 of each per
base. That is the `240/240 per base` already pinned in
`p1_promoted_dualbase_layout_v0732m.md`, and it agrees.

> **A counting trap worth recording.** Measuring "distinct addresses touched
> by IP 0x2399c" across the whole trace gives 240 with a 7716-byte gap in the
> middle, and subtracting `0x510980` from every address makes the tail look
> like it runs to `0x2edc`. Both are artefacts: the gap is the jump from
> fighter0's block to fighter1's, and the tail offsets are *fighter1* offsets
> measured from fighter0's base. Per base the answer is 120 slots / 480 B.
> `frontier.contiguous_fighter_blocks(width=4, min_count=2, min_length=120)`
> reports exactly `len 120 bytes 480 R 240 W 240 base_count 2 ip_overlap 1.0`,
> which is the check to trust.

## The producer

```text
00023980  mov      0, r9
00023984  ldob     0x0002394c[r9], r11      ; per-slot index from a table
0002398c  mulo     12, r9, r10              ; r9 * 12  -> source stride
00023990  ldt      (r8)[r10], r4            ; load a 12-byte triple
00023994  ld       0x0000000c(r3)[r11*16], r7
0002399c  stq      r4, 0x00000d00(g7)[r11*16]   ; <- THE WRITER
000239a4  cmpinco  29, r9, r9               ; loop r9 = 0..29
000239a8  bne      0x00023984
```

So the block is produced by a **30-iteration loop** that gathers a 12-byte
triple per iteration from a source table at `r8`, and stores it into
`g7 + 0x0d00`. `g7` is the fighter base, which is why the same loop produces
the same 120 slots for both fighters - that is the mechanical reason the
block is dual-base, not a coincidence.

## The consumer

```text
00023a38  ldq      0x00000d00(g7)[r3*16], r4    ; <- THE READER
00023a40  mov      r7, r15
00023a44  subr     r15, r5, r7
00023a48  addr     r15, r5, r5
00023a4c  ld       0x000000fc(g13), r15
00023a50  cmpr     r7, r15
00023a54  bg       0x00023a64
00023a58  ld       0x0000010c(g13), r15
00023a5c  setbit   r3, r15, r15                ; accumulate a bitmask
00023a60  st       r15, 0x0000010c(g13)
00023a64  ld       0x00000100(g13), r15
00023a68  cmpr     r5, r15
```

The read feeds a **threshold classification**: each loaded value is compared
against a pair of float constants held in `(g13)+0xfc` and `(g13)+0x100`
(established at `0x239ac..0x239e0` as `0.1`, `-0.3`-ish, and a scaled `0x50a010`
load), and slot index `r3` is folded into a **bitmask at `(g13)+0x10c`** via
`setbit r3`.

So the block is not a pose cache: it is a **per-slot scalar series that gets
thresholded into a bitmask**, one bit per slot, 120 slots wide. That is a much
stronger statement than "scratch array", and it is why the v0730 taint result
(no branch depends on it) is consistent - the comparison result feeds a
bitmask *store*, not a control-flow decision.

**This is an observation about the consumer's shape, not a recovered semantic.**
The register-level mechanics are not yet proven against a differential; the
address geometry and the two guest IPs are.

## What the producer loop actually moves

`mulo 12, r9, r10` with `ldt (r8)[r10]` says the **source stride is 12 bytes**
per iteration, while the **destination stride is 16** (`[r11*16]`). 30
iterations therefore gather 30 x 12 = 360 source bytes into 30 x 16 = 480
destination bytes. **360 vs 480 is a real discrepancy, not a rounding story**,
and it is the first concrete thing to resolve here: either `r11` is not
`0..29`, or the destination stride is not uniform across all 120 slots, or
`ldt` loads more than one destination word per iteration.

> **RESOLVED at v0732o. This is NOT a discrepancy, and all three candidate
> causes above are falsified.** `ldob 0x2394c[r9], r11` reads a 30-byte
> **permutation of 0..29** out of the ROM, and `0xd00 + r11*16` predicts
> **60 of 60** observed destination offsets (30 slots x 2 bases). 12 -> 16 is
> a **3-word to 4-word expansion**: each iteration loads a packed 3-word
> triple and writes it into a padded 4-word slot. The block is
> `uint32_t field_0d00[30][4]`, not a flat 120-word array. The table is read
> 60 times in the trace, which is also the first independent confirmation that
> the producer runs twice on the same code, once per fighter. See
> `p2_slot_permutation_v0732o.md`.

The original concern below is kept for the audit trail only. **Do not** treat
it as an open item.

Do not close that gap by assuming. It is a 12-vs-16 question that a single
controlled run with `r8`/`r3` pinned can settle, and it decides whether the
array is 30 entries of 4 words or 120 entries of 1 word - which changes the
provisional struct in `p1_promoted_dualbase_layout_v0732m.md` from
`uint32_t field_0d00[120]` to something else.

## Next step for P2, in order

1. ~~**Settle 12 vs 16** with a controlled probe (pin `r8`, `r3`, `g7`; capture
   the source and destination address sequences separately). This is a
   measurement, not a recovery.
2. Re-measure the block geometry against the answer and correct
   `p1_promoted_dualbase_layout_v0732m.md` if the array is not 120 words.
3. Only then treat `0x2399c`/`0x23a38` as a recovery target.

Still open from `fa_player_28918_live_v0713.md` and untouched here: the
mode-5 loop at `0x28a04`, exhausted keyframe walks at `0x2896c`, modes above
6 and fighter bit6 set at `0x28af8`, and the `0x281b0`/`0x28208` table heads
that join at `0x2891c`.

## Anti-traps

- Do not read "240 accesses" as 240 slots. It is 120 slots x 2 bases.
- Do not subtract `0x510980` from a fighter1 address. Use the per-base offset.
- Do not name the block `pose`, `state` or `animation`. The consumer
  thresholding it into a bitmask is a stronger claim than "scratch array",
  but a *shape*, not a meaning.
- Do not assume the 12-byte source stride and 16-byte destination stride
  reconcile. They do not, on the current evidence, and that gap is the next
  measurement.
