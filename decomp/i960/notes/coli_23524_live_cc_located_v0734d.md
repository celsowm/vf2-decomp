# v0734d: the live-leg condition state is NOT the `+0x650` clamp — the obvious candidate is disproved

**Phase 2.2, measured twice.** The first measurement in this file was wrong and
is corrected below. The correction is the useful part: a plausible fix was built,
measured, and thrown away because the evidence refused it.

**No C is changed. `src/` is byte-identical.**

## What was measured, and how

The measurement was taken **outside** the test harness, so the instrument is
verified rather than assumed. `out/coli-parked-221e8.vf2snap` walked to the shell
entry (`0x23524`, 7 instructions, cc EQUAL, g14 `0x22428`), snapshotted there, and
the live-leg recipe applied at the entry exactly as `side_apply()` does:

```text
F0_FLAGS     0x00510b24 = 0x00000100
F1_FLAGS     0x00511aa4 = 0x00000000
F0_820_BYTE  0x005111a0 = 0x01
REGISTRY_9CC 0x005149cc = 0x0000ffff
```

`vf2probe` then ran to the boundary `0x22210` with `--trace`:

```json
{"type":"final","halt_reason":"stop address","ip":139792,
 "run_instructions":9300,"compare":"greater","local_frame_depth":5}
```

**9300 instructions, cc GREATER, ip `0x22210`** — reproducing the test's leg-B
measurement exactly, from a different driver. `0x22210` = 139792. ✓

## The exit trace tail

The trace's tail, decoded against the ROM:

```text
0002385c  lda      0x3cf5c28f, r14
00023864  subr     r14, r15, r15
00023868  bbc      31, r15, 0x00023870
0002386c  mov      0, r15
00023870  st       r15, 0x00000650(g8)
00023874  bx       (g14)             -> 0x23648
00023648  ret                       -> 0x22210
```

`0x23874` is the `bx (g14)` the v0733d note already names and `0x23648` is the
`ret` that pops the caller's frame — so the tail is confirmed against the ROM,
not inferred from the trace.

## First measurement — and its disproof

The obvious candidate is the `subr` at `0x23864`: it is the last arithmetic
instruction before the tail, it is the second of the `+0x650` clamp pair and so
the last writer, and the native recovers the clamp without publishing any compare
state (`hybrid.c:31149-31182`). Publishing its result looks like the fix.

**Three measurements kill it.**

1. **`bbc` is exonerated.** Stopping the reference either side of it on the live
   leg gives the same state — `GREATER` at `0x23868` (after the `subr`) and
   `GREATER` at `0x2386c` (after the `bbc`). So under `vf2_i960_run` the `bbc`
   does not write cc, and it is not the last writer either. *(This is the B49
   sub-question, now answered for this path — and the answer is no.)*

2. **The operands are identical on both legs.** `g8 + 0x650` is `0x511F50`, and
   reading it with `--read-u32` at `0x2385c` — after the `ld` at `0x23858` has put
   it in `r15`, before the `lda` sets `r14` — gives **`0` on the warm leg and
   `0` on the live leg.** So `subr r14, r15, r15` has the same two inputs on both
   legs.

3. **The reference nevertheless exits differently** — EQUAL on warm, GREATER on
   live. A single `subr` with identical operands cannot produce two different
   compare states. **The `subr` at `0x23864` is therefore not the last writer.**

The fix was built anyway, on the assumption that the subr was it. It published
`LESS` on **both** legs and broke the warm leg:

```text
MISMATCH A warm: compare_result divergence is 1, measured 0 (reference 2, native 1)
MISMATCH A warm: arithmetic_control divergence is 1, measured 0 (reference 0x3f001002, native 0x3f001004)
B live f0: MATCH (... cc 3 vs 1 ...)     <- reference GREATER, native now LESS
```

With `t1 == 0` neither `0 - thr` nor `thr - 0` yields EQUAL, which is what warm
requires. The code change was reverted and `src/recovered/hybrid.c` is
byte-identical to `v0734c`.

## What survives

- The live-leg defect is **real and still open**. It is *not* the `+0x650` clamp.
- `hybrid.c:31149-31182` recovers the clamp correctly and still publishes no
  compare state — that observation stands, it just is not the bug.
- The warm leg agrees with the reference **by accident** (it inherits EQUAL). That
  is still true and still worth stating: it is not evidence the recovery is right.
- **`bbc` does not write cc under `vf2_i960_run`** — B49's sub-question, answered
  for this path.

## The next step, precisely

The last cc writer is **before `0x23864`**, and the point at which warm and live
diverge is not yet known. The measurement that finds it:

> Walk warm and live from the `0x23524` entry, recording `compare_result` after
> **every** instruction, and report the first guest `ip` at which the two cc
> sequences differ.

The existing `--trace` records `ip_before`/`ip_after`/`mnemonic` but **not** the
cc, so this needs either a `compare` field added to the trace record or a small
diagnostic that walks both legs and diffs the cc per step. That is the whole
remaining cost of item 2.2; the answer then lands in the same block.

`g14` (item 2.1) is untouched and remains open — the native never writes it, and
the reference's `0x23648` comes from the `bal 0x23694` at `0x23644`. This slice
does not claim it.