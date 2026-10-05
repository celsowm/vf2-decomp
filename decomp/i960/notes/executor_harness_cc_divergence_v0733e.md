# v0733e: the "unpinnable compare_result" was my own fixture — retracted

**This note retracts two of the three divergences reported in
`coli_shell_contract_v0733d.md`, and replaces them with one real, bounded gap.**

The retraction is in place at the top of that note. What follows is the evidence
for withdrawing the claim, because a retraction without its provenance is just
another assertion.

## What v0733d claimed

That `compare_result` and `arithmetic_control` bit 1 were unrecovered at the
`0x23524` shell's return on **every** admitted leg, and that they were
"unpinnable" because the exit compare state was "measurably entry-state
dependent". It also left `hybrid.c` unchanged on that basis.

## What is actually true

Driven the way `vf2probe` and the differential tooling drive the reference — via
`vf2_i960_run` — the warm leg is **exact on condition state**:

```text
A warm: MATCH (9151 insn, 13 calls, 14 returns, g3=0x00000000 g4=0x00000000);
        g14 0x00023648 vs 0x00022428, cc 2 vs 2, arithmetic_control 0x3f001002 vs 0x3f001002
B live f0: MATCH (9300 insn, 12 calls, 13 returns, g3=0x00000000 g4=0xffffdffc);
        g14 0x00023648 vs 0x00022428, cc 3 vs 2, arithmetic_control 0x3f001001 vs 0x3f001002
```

The native already publishes EQUAL and `0x3f001002` on the warm leg. So
`compare_result` and `arithmetic_control` are **not** divergences there, and the
"unpinnable" conclusion described a defect that did not exist.

## The cause: the fixture, not the recovery

v0733d's fixture stepped the reference with a hand-rolled `vf2_i960_step` loop.
`vf2probe` calls `vf2_i960_run`, which is the entry point
`native_differential.c`, `native_differential_step.c` and `native_runtime.c`
all use.

Over the **same 7 instructions, from the same snapshot, in the same binary**:

```text
restored: ip=0x000221e8 cc=equal arith=0x3f001002 depth=5 exec=29294484 calls=44717

# hand-rolled vf2_i960_step loop
step 0: 0x000221e8 -> 0x000221f0  ld     cc equal -> equal
step 1: 0x000221f0 -> 0x000221f4  bbs    cc equal -> none      <-- clears cc
step 2: 0x000221f4 -> 0x000221fc  ld     cc none  -> none
...
reached the shell entry after 7 steps: cc=none arith=0x3f001000

# vf2_i960_run with stop_address = 0x23524
via vf2_i960_run: ip=0x00023524 run=7 cc=equal arith=0x3f001002
```

Same trace, same addresses, same `executed_instructions`, same registers — and
a different `compare_result` and `arithmetic_control` at the entry. That
divergence then propagated to the exit on all three legs, which is what made
`compare_result` look entry-dependent when in fact the entry state itself was an
artifact.

**Mechanism: not yet isolated, and it matters.** The two paths call the same
`vf2_i960_step` (`executor_arch.c:219`); `vf2_i960_run` (`executor.c:1482`) only
adds a stop check and a callback. So the difference is not in the step function.
What is established is that it is reproducible, it affects only condition state,
and it is not serialisation — the snapshot format does persist both fields
(`snapshot.c:264/266/343/344/357`).

This is the same shape as the v0733d `vf2i960 function` episode: **the
instrument was the defective part, and it produced a confident, specific,
well-formatted wrong answer.** Two of them in one slice.

## The real gap, now bounded

| field | leg A warm | leg B live | leg C refused |
|---|---|---|---|
| `g14` | `0x23648` vs `0x22428` — **diverges** | same — **diverges** | n/a (refused) |
| `compare_result` | EQUAL vs EQUAL — exact | GREATER vs EQUAL — **diverges** | n/a |
| `arithmetic_control` | `0x3f001002` — exact | `0x3f001001` vs `0x3f001002` — **diverges** | n/a |

So the shell's exit contract has **two** outstanding items, not three:

1. **`g14` is never published**, and is path-dependent (the `bal 0x23694` at
   `0x23644` on the admitted paths, skipped on the refused one). No constant is
   correct for all three legs.
2. **On the live single-fighter leg only**, the native publishes the warm leg's
   condition state — EQUAL and `0x3f001002` — where the reference has GREATER
   and `0x3f001001`. This is a genuine defect on an admitted leg, and **no
   existing test caught it**: `vf2_coli_whole_task_live` compares through
   `0x10dcc`, by which point the compare state has been recomputed downstream.

Item 2 is the more valuable find. It is bounded to one leg and two fields, and
recovering it needs the live path's last compare-setting instruction rather than
another sample.

## Standing rules this adds

- **Measure the instrument before measuring the thing.** Two fixtures in one
  slice invented defects: one by trusting `vf2i960 function`, one by trusting
  its own step loop. In both cases the surrounding prose was confident and the
  numbers were self-consistent.
- **A retraction must name the specific claim, not the section.** v0733d's
  g3/refusal findings are unaffected and are retained; only the two condition
  fields are withdrawn.
- **A fixture must drive the oracle the way the tools drive it.** "Both call
  `vf2i960_step`" is not the same as "both behave identically", and the
  difference is exactly the kind a differential exists to expose.

## Validated

- `vf2_coli_23524_live` and `vf2_coli_23524_live_differential` pass on the
  corrected driver.
- The fixture now asserts the divergence *set* per leg, so "warm diverges" and
  "live diverges" are both visible; asserting "always diverges" would have
  hidden the warm-leg match, and "never diverges" would have hidden the live gap.
- Full non-dominator suite: see the commit message.

## Still open

- Isolate why `vf2_i960_run` and a `vf2_i960_step` loop disagree on condition
  state. Until then, any fixture that steps the reference by hand should be
  treated as measuring a different machine.
- Recover `g14` and the live-leg condition state at the shell's return.
- The live-leg `cc`/`arithmetic_control` gap is a real admitted-leg defect; the
  `0x23524` differential is what found it and is what should keep it visible.
