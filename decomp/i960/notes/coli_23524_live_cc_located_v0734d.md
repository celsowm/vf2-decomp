# v0734d: the live-leg condition state is located — it is the `+0x650` clamp's `subr`, not an unknown

**Phase 2.2 measured.** The completion plan said item 2.2 "needs the live path's
last compare-setting instruction". This is it. **No C is changed here** — the
measurement stands on its own and names the exact edit.

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

## The last compare-setting instruction

The trace's tail, decoded against the ROM:

```text
0002385c  lda      0x3cf5c28f, r14
00023864  subr     r14, r15, r15     <- r15 = r15 - r14  (dest is LAST)
00023868  bbc      31, r15, 0x00023870
0002386c  mov      0, r15
00023870  st       r15, 0x00000650(g8)
00023874  bx       (g14)             -> 0x23648
00023648  ret                       -> 0x22210
```

`0x23874` is the `bx (g14)` the v0733d note already names, and `0x23648` is the
`ret` that pops the caller's frame — so the tail is confirmed against the ROM,
not inferred from the trace.

The cc at the boundary is set by the **`subr` at `0x23864`** — the second of the
pair of `+0x650` clamps (one per fighter, `0x23848`/`0x23864`; `g8`'s is last and
therefore wins). `subr` destination is printed **LAST** on i960, so this is
`r15 = r15 - 0x3cf5c28f`.

## Why the native gets warm right and live wrong

`src/recovered/hybrid.c:31149-31182` recovers this clamp faithfully — it reads
both `+0x650` words, subtracts the constant, applies the `bbc 31` sign-bit clamp,
and stores both results. **It never publishes `compare_result` or
`arithmetic_control`.**

So the native's exit cc is simply whatever the last recovered child left:

| leg | inherited cc | reference exits | native exits | agrees by |
|---|---|---|---|---|
| A warm | EQUAL | EQUAL | EQUAL | **accident** — the inherited value happened to be right |
| B live | EQUAL | GREATER | EQUAL | nothing |

That is the whole defect. The warm leg is not evidence the recovery is right;
it is evidence the bug is invisible on that leg.

## The fix, precisely

Publish the cc from the final `subr` rather than inheriting it — computed from
the actual clamped value, inside the existing block at `hybrid.c:31161-31181`,
immediately after the `g8` result is stored:

```c
/* 0x23864 subr r14, r15, r15 is the last compare-setting instruction before
 * the `bx (g14)` tail, so its result is what the reference exits with. On the
 * warm leg t1 == thr so this is EQUAL and the existing agreement is preserved
 * by construction rather than by the inherited value; on the live leg it is
 * GREATER, which is the measured reference state. */
```

with the ordering `LESS / EQUAL / GREATER` taken from the i960 `SUBR` signed
result, and the value used being `r1` **before** the `bbc 31` clamp (the clamp
is a branch, not an arithmetic result — the `subr`'s result is what sets cc).

**Two things must be settled before this ships, and neither can be guessed:**

1. **The `bbc` contribution.** `0x23868` follows the `subr`. Per B49 the repo
   does not yet know whether `bbc` writes cc on this path (`arch_fix_direct_compare`
   is not applied under `vf2_i960_run`). If it does, the last writer is the
   `bbc`, not the `subr`, and the published value is different. **B49 gates
   this.**
2. **Which value the reference compares.** "GREATER" must be re-derived from the
   measured `t1` on the live leg, not inferred from the sign of the float.

## Acceptance, unchanged from the plan

The repaired shell must FULL MATCH leg B on `compare_result` and
`arithmetic_control`, **and** leg A must still match EQUAL/`0x3f001002` — now by
construction rather than by accident. The refused leg C must keep its LESS state.

Then `cc_diverges` / `arith_diverges` go to 0 for leg B in
`tests/recovered/test_coli_23524_live.c`, and that test's stale-divergence
branch ("expected the two documented divergences ... someone published them")
becomes the gate that says so.

**`g14` (item 2.1) is untouched by this and remains open.** The reference's
`g14` is `0x23648` on both admitted legs because both execute the `bal 0x23694`
at `0x23644`; the native never writes it. That is a separate fix and this slice
does not claim it.