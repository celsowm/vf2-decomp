# v0733f: B45 root cause — `vf2_i960_run` and `vf2_i960_step` are different machines because of a build-time macro

**v0733e reported the `vf2_i960_run` vs hand-stepped-loop condition-state
divergence and said the mechanism was "not yet isolated". It is isolated, and it
was never a mystery: `vf2_i960_run` does not call the same stepper that everyone
else calls.**

No production C is changed by this note. The fix direction is clear but its blast
radius is the entire oracle, and measuring that is the next slice.

## The cause

`CMakeLists.txt:112-115`:

```cmake
set_source_files_properties(
    src/i960/executor.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_i960_step=vf2_i960_step_legacy"
)
```

Inside `executor.c` only, every textual reference to `vf2_i960_step` is replaced
with `vf2_i960_step_legacy`. So:

- `vf2_i960_run` (`executor.c:1482`) calls **`vf2_i960_step_legacy`**.
- `vf2_i960_step` (`executor_arch.c:219`) is what **every other caller in the
  repo** gets — the fixtures, `native_differential.c`,
  `native_differential_step.c`, `native_runtime.c`, `vf2probe`'s callers.

The arch stepper does three things the legacy one does not: it pre-evaluates the
compare operands, it applies `arch_fix_direct_compare`, and for
`bbs`/`bbc`/`cmpob`/`cmpib` it rewrites the compare state
(`executor_arch.c:150-171`). **`vf2_i960_run` therefore never applies that fix.**

## The experiment that proves it

Instrumented both entry points (reverted afterwards; tree is clean):

```text
=== run-bare ===
[TMP] vf2_i960_run stepped ip=000221f0
[TMP] vf2_i960_run stepped ip=000221f4
...
[TMP] vf2_i960_run stepped ip=00023524
--run-bare: ip=0x00023524 steps=7 cc=equal arith=0x3f001002 depth=6

=== manual-bare (7 direct vf2_i960_step calls) ===
[TMP] vf2_i960_step ip=000221e8
[TMP] vf2_i960_step ip=000221f0
[TMP] step bbs@000221f0 op0 kind=4 lit=5 op1 kind=1 reg=15
[TMP] arch_fix bbs first=5 second=0x00008a00 set=0
...
7 bare vf2_i960_step calls: ip=0x00023524 steps=7 cc=none arith=0x3f001000
```

`vf2_i960_run stepped` fires seven times. **`vf2_i960_step` never fires once.**
That is not a state or serialisation difference; the run path is simply not
entering the arch stepper.

The whole observed divergence follows from the single instruction at `0x221f0`:

```text
000221f0  bbs 5, r15, 0x00022294        ; r15 = [0x00508000] = 0x8a00, bit 5 clear
```

- arch stepper: `arch_fix bbs first=5 second=0x00008a00 set=0` -> `cc = NONE`,
  `arith -> 0x3f001000`
- legacy via `vf2_i960_run`: compare state untouched -> `cc = EQUAL`,
  `arith = 0x3f001002`

Every downstream leg, and the whole v0733d "entry-state dependent" illusion, came
from this one macro.

## Which side is right is NOT settled, and I previously got this wrong

v0733e asserted that the hand-stepped loop was "arch-correct" because bit 5 is
clear and `executor_arch.c` maps clear to `NONE`. That conflated the repo's own
convention with the architecture. It is a convention, not a fact:

- The repo's `arch_fix_direct_compare` encodes **clear -> `NONE`, set ->
  `EQUAL`**.
- Real i960 `BBT` sets `ac0 = <bit value>`, `ac1 = 0`, so the compare word
  `ac1:ac0` is `00` (equal) when the bit is clear and `01` (greater) when set.

On that reading **neither repo path matches the architecture**, and they happen
to disagree in a way that neither side is obviously right about. Meanwhile other
code depends on the existing convention — `executor_arch.c:282-300` re-decides
`bo`/`bno` from the AC low bits and explicitly avoids the scanbit domain, with a
measured justification in its comment.

So this note does **not** claim a side is correct. Deciding the BBT convention
repo-wide is a separate, larger question than the build asymmetry.

## Blast radius, and why nothing was changed

Removing the macro is a one-line change with repo-wide consequences:

- `vf2probe` and `vf2cycles` (both on `vf2_i960_run`) would start reporting the
  arch-stepper compare state for every `bbs`/`bbc`/`cmpob`/`cmpib`.
- Every fixture that steps by hand is already on the arch stepper, so the two
  would finally agree — which is the good outcome.
- But every compare-state constant measured through the probe, and every
  `bo`/`bno` decision that depends on the convention, has to be re-validated.

AGENTS.md requires the oracle's behaviour to stay stable and changes to be the
smallest **proven** one. Removing this macro is neither proven nor small, so it
is not done here. **The measurement is the next step, not the edit.**

## Plan for the fix (Phase 0.1, next)

1. Build a separate variant tree (e.g. `build-archstep`) with the macro removed,
   so the default `build/` is untouched and the result is reversible by
   construction.
2. Run the full suite. Record exactly which tests move and by how much.
3. Classify each move: genuine improvement (fixture and probe finally agree),
   stale pin (update with the new measurement), or `bo`/`bno` convention
   fallout (needs its own decision).
4. Only then decide whether to remove the macro, and add a regression test that
   drives both entry points over a `bbs`/`cmpob` window and asserts they agree —
   the acceptance criterion Phase 0 was written with.

## Retraction

v0733e's statement that the mechanism was "not yet isolated" is **withdrawn**, and
so is its claim that the hand-stepped loop is "the arch-correct one". Both were
wrong. What v0733e got right — that its own fixture was the thing that had to
change, and that the resulting divergences needed re-measuring — stands.

`docs/UNCOVERED_BRANCHES.md` and `AGENTS.md` carry the same correction.

## Validated

- Instrumentation reverted; `git status` clean, `src/i960/executor.c` and
  `src/i960/executor_arch.c` byte-identical to `HEAD`.
- Full rebuild warning-free under `VF2_WARNINGS_AS_ERRORS=ON`.
- Suite: see the commit message.