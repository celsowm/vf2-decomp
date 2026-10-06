# v0740: v0739's "silent-admission hole" claim was WRONG — retracting

**Retraction of v0739.**

## What v0739 said

v0739 promoted the `c3a` fixtures (`f2-r3-c3a-e2.vf2snap`,
`f2-r3-c3a-e4.vf2snap`) to a failing ctest entry, claiming they were
silent-admission holes: the recovered C admitted them at 4422/41
where the v0732c reference said they should be 4636/4637.

## Why v0739 was wrong

The v0739 test confused two different snapshot families:

| snapshot | credits[3] | credits[5] | reference | admitted? |
|----------|-------------|-------------|-----------|-----------|
| `f2-r3-edit` | 3 | 1 | **4637** | yes, MATCH |
| `f2-r3-e2` | 4 | 2 | **4636** | **refused** |
| `f2-r3-c3a-e2` | (different fixture) | (different) | **4422** | (admitted by C) |
| `f2-r3-c3a-e4` | (different fixture) | (different) | **4421** | (admitted by C) |

The c3a fixtures are a **third family**, distinct from both
`f2-r3-edit` (admitted at 4637) and `f2-r3-e2` (refused). The v0739
test was based on a confusion between c3a-e2 (a fixture that produces
4422) and `f2-r3-e2` (a fixture that produces 4636).

## The actual measured behaviour on this build

`f2-r3-c3a-e2.vf2snap`: native **4422 instructions / 41 calls** =
reference **4422 / 41** (delta from 0x9ff8 baseline). MATCH.
`f2-r3-c3a-e4.vf2snap`: native **4421 / 41** = reference **4421 / 41**.
MATCH.

The recovered C correctly admits these fixtures at the right count.
There is **no silent-admission hole**.

## What is still fail-closed (real)

The original v0736 audit's seven refused paths still refuse:

- `f2-r3-e2` (credits[3]=4): REFUSED (reference=4636). The gate
  correctly refuses this shape.
- `f2-r3-e5`, `f2-r3-n8`, `f2-r3-k1`: REFUSED (different
  credits[3] values).
- `nega5-3/4/5`: REFUSED at the `0xa6c0` gate.

The negative control `f2-r3-edit.vf2snap` (credits[3]=3, credits[5]=1)
is correctly admitted at 4637.

## The actual question v0739 left open

The c3a fixtures produce 4422 (not 4636/4637 as v0739 wrongly
assumed). If the user wants c3a fixtures to be REFUSED rather than
admitted at 4422, that is a **new design decision**, not a recovery
hole. Per `f3_punch_kick_release_measured_v0732.md`:

> The current code carries a fixed `edit_delta > 0 ? 4405 : 4402` for
> `a5 == 2 || a5 == 3`, which is 4637 for a +1 edit. That is correct
> for `credits[5] != 2` and wrong by one for `credits[5] == 2`.

The c3a fixtures match the standard a5=2/3 rule (4422 for 0 edit
delta). The user might want the gate to be tightened to also refuse
c3a, but that is a Phase 2.5 follow-up *design* decision, not a
recovery.

## What this slice changes

1. **`tools/python/test_phase_2_5_refused_audit.py`**: reverted to
   the v0736 behaviour. The c3a fixtures are now reported as
   "reference count check" with their actual reference values
   (4422/41 and 4421/41). They PASS, not FAIL. The 7 refused paths
   still refuse.

2. **`ctest #124`**: now passes in 1.21 s. Full `ctest -E
   vf2_player_4505` returns to 121/121 PASS.

3. **The hole is gone.** No `src/` change.

## Validated

- `python tools/python/test_phase_2_5_refused_audit.py`: PASS
- `ctest -R vf2_phase_2_5_refused_audit`: 1/1 PASS in 1.21 s
- `git diff --stat src`: empty
- `git diff --stat decomp/i960/functions.csv`: empty

## What the next slice should pick up

- **If the c3a fixtures should be REFUSED (design decision)**: that
  is a gate-tightening slice. Two-source rule: tighten with a
  measurement that explains WHY c3a is refused (not just because
  v0739 wrongly guessed the reference). Per `AGENTS.md`, this is
  blocked on a second source.
- **If the c3a fixtures should be admitted at 4422 (current
  behaviour)**: nothing to do. This slice is a no-op.
- **If a different fixture is suspected of silent admission**: trace
  it from `0x9ff8` to `0xa010` with `vf2probe --trace`, compare with
  native, and look at the actual reference count.

## The lesson

The v0739 mistake came from pattern-matching "credits[3]=3 in the
name" to "credits[3]=3 in the v0732c table" without checking the
actual reference count of the fixture under test. The trace
divergence investigation (the c3a-e2 vs f2-r3-edit trace compare at
step 4411) was correct, but the **interpretation** was wrong — the
divergence is a real ROM behaviour difference (different credits[5]
or input shape), and the recovered C correctly handles it. Always
run the reference through the same path the native takes before
declaring a count mismatch.

The v0739 retraction note exists on disk because **retract loudly
and in place**: superseded claims stay in history with a banner
pointing at the corrected commit.