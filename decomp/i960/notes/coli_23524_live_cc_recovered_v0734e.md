# v0734e: the live-leg condition state is recovered — the last writer is `cmpobne` at 0x236c8

**Closes Phase 2.2 of `completion_plan_v0734.md`.** The defect v0733e found is
fixed, differentially, and the gate is proven able to fail.

`g14` (item 2.1) is untouched and remains the one open divergence at this
boundary.

## What was wrong

On the live leg the native published `cc EQUAL` / `arithmetic_control
0x3f001002` where the reference publishes `GREATER` / `0x3f001001`. The warm leg
matched — but only because the native **inherited** EQUAL from whatever ran
before, so the warm agreement was an accident, not a recovery.

## The measurement that found it

v0734d named the missing piece: the `--trace` record carried
`ip_before`/`ip_after`/`mnemonic` but **not** the compare state, so the last
compare-setting instruction could not be identified.

`tools/vf2probe/main.c` now emits `compare` and `arith` on every step record,
and the callback already received the `vf2_i960_cpu` — so this was adding two
fields, not building a new instrument.

With that, walking warm and live from the `0x23524` entry and reporting the last
transition in `compare`:

| leg | last cc writer | cc it leaves | reference at `0x22210` |
|---|---|---|---|
| warm | `cmpinco` at **0x23930** | EQUAL (2) | EQUAL, `0x3f001002` |
| live | `cmpobne` at **0x236c8** | GREATER (3) | GREATER, `0x3f001001` |

Both writers sit in the `bal 0x23694` body. The live one is the first
instruction of the `cmpobl` target:

```text
000236ac  ld       0x00000148(g13), r3
000236b0  cmpobl   0, r3, 0x000236c4     <- sets LESS on the way in
000236b4  call     0x0002364c
000236b8  movt     0, r8
000236bc  movt     0, r12
000236c0  b        0x000237b0
000236c4  ld       ...                   <- taken target; does not touch cc
000236c8  cmpobne  ...                   <- leaves GREATER. LAST WRITER.
```

The trace record's `compare` is the state **after** `ip_before` executes. That
matters: reading it as pre-execution puts the writer one instruction early, which
is what made `subr 0x23864` look like the culprit in v0734d.

## Two rejected explanations, kept because they are easy to fall back into

- **The `subr` at `0x23864`** (the `+0x650` clamp). `g8 + 0x650` is `0` on **both**
  legs, so one `subr` with identical operands cannot yield EQUAL on warm and
  GREATER on live. The whole `0x23840..0x23874` tail holds cc unchanged.
- **`bbc` at `0x23868`.** Stopping the reference either side of it gives the
  same state, so it does not write cc under `vf2_i960_run`. This answers B49's
  sub-question for this path.

Both were built, measured, and reverted before this slice. They are recorded
because "the last arithmetic instruction before the return" is the intuitive
answer and it is wrong.

## The fix

One line, in the live branch of the shell's `cmpobl 0, r3` split
(`hybrid.c`, the `r3 == 0xFFFFDFFC` arm):

```c
hybrid_set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
```

The warm branch is deliberately **not** touched: its reference cc is EQUAL and
the native already reaches EQUAL, so adding a publish there would change nothing
while adding a second place to keep in sync. The refused leg C never reaches this
code, so its LESS state and its load-bearing refusal are unaffected.

## Gate integrity

`tests/recovered/test_coli_23524_live.c` recorded `cc_diverges = 1` /
`arith_diverges = 1` for leg B — the divergence itself was asserted, which is why
the fix first showed up as a *failure*:

```text
MISMATCH B live f0: compare_result divergence is 0, measured 1 (reference 3, native 3)
```

Both flags are now `0`, so a regression re-opens the divergence and the test
fails again. Negative control, run clean on this exact tree — publishing EQUAL
instead of GREATER:

```text
MISMATCH B live f0: compare_result divergence is 1, measured 0 (reference 3, native 2)
MISMATCH B live f0: arithmetic_control divergence is 1, measured 0 (reference 0x3f001001, native 0x3f001002)
```

It pins both fields on both the cc and the packed `arithmetic_control`.

An earlier negative control in this slice was run through a scripted whole-file
replace that silently hit **ten** unrelated `hybrid_set_compare_result` call
sites; the diff review caught it and `hybrid.c` was restored from git before the
real control was run. Recorded because it is the reason every control here is
diff-reviewed rather than trusted.

## Validated

- `vf2_coli_23524_live` and `vf2_coli_23524_live_differential`: both pass.
- Leg A (warm) still EQUAL / `0x3f001002`; leg C still refuses with LESS and is
  still load-bearing.
- `hybrid.c` diff is a **pure insertion** — 0 lines removed.
- Full suite: see the commit message.

## Still open at this boundary

- **`g14` (item 2.1).** The reference's `g14` is `0x23648` on both admitted legs
  because both execute `bal 0x23694` at `0x23644`; the refused leg keeps the
  untouched entry value `0x22428`. The native never writes it. That is now the
  *only* divergence at `0x22210`.