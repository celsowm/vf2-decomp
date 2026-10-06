# v0734m: Phase 2.3-2.5 status record — measured, gated, not yet recovered

Phase 2.3-2.5 of `completion_plan_v0734.md` are bounded by the v0732
corridor work. None of them is admitted beyond what v0732c/v0732g
already pinned; the slice below is **a status record**, not a fix.

`src/` is unchanged. `functions.csv` is unchanged. The recovered
`phase17_bit7_index5` (in `src/recovered/texture_bridge_match.c:9269-
9294`) already implements the per-route rule with the 7-instruction
`a5=4` delta as a measured constant (`preset == 0 ? 4193 : 4186`),
documented in-code as "pinned as measured rather than derived."

## Phase 2.3 — `+0x110` / `+0x114` setbit sides (warm leg only)

**What it is.** Per `fa_coli_2396c_v0285.md` and `fa_coli_2396c_measure_v0283.md`,
the recovered `0x2396c` poly-cluster body has two `setbit` instructions
that the **warm leg never takes**:

- `0x23a70`: `setbit g13+0x110` — never fires
- `0x23a88`: `setbit g13+0x114` — never fires

The warm leg always setbits `g13+0x10c` and `g13+0x118` instead
(`0x23a54` / `0x23aac`, 30 trips × 2 fighters = 60 hits).

**The fence.** The completion plan entry says "warm leg only — widen
with the neighbouring leg, never from one sample." A live leg that
fires `+0x110` / `+0x114` setbits has never been measured. Without
that measurement, the gate stays narrow.

**What a focused 2.3 slice does:**

1. Build a live leg snapshot from `out/coli-f0-live.vf2snap` (or
   equivalent) where the `bg` at `0x23a54` is **not** taken and
   `+0x110` setbit fires.
2. Verify the reference `vf2probe --memory-trace` records a `st` to
   `g13+0x110`.
3. Run the native differential against this snapshot.
4. If the native **fails to setbit**, widen the gate by adding the
   `setbit g13+0x110` instruction and re-run.

**This is not started.** The live leg snapshot exists in
`out/coli-f0-live.vf2snap` (or close to it) but no measure traces
through `0x2396c` under live conditions have been recorded.

## Phase 2.4 — 7-instruction `a5 = 4` delta (measured, uncharacterised)

**What it is.** Per `f3_chute_slot_and_count_correction_v0732.md`,
the `a5 = 4` post-edit release body measures as:

| preset | instruction count |
|---|---|
| 0 | 4193 (body 4425 total) |
| 1 | 4186 (body 4418 total) |
| 2 | 4186 (body 4418 total) |

The render difference between `preset = 0` and `preset >= 1` is
**exactly one tile write** (the second blank at `(24,47)`), which is
not seven instructions. So the cause is elsewhere in the body.

**The fence.** The `bg` at `0x23a54` and `0x23aac` are taken on the
warm fixture (the 0x23a54 one always), but the `a5 = 4` body is a
different code path. The 7-instruction delta must be in a comparison
chain specific to `a5 = 4`. Without disassembly trace on both
`preset == 0` and `preset >= 1` fixtures through the entire
`0xa010 -> 0x164c4` corridor, the cause cannot be pinned.

**What a focused 2.4 slice does:**

1. Take two `vf2probe --trace` runs at `out/f2r4-k-rel.vf2snap`
   (`preset == 0`, body 4193) and `out/f2r4-rel.vf2snap`
   (`preset == 1`, body 4186).
2. Compare guest IP sequences instruction-by-instruction.
3. Find the first IP where the two traces diverge.
4. Disassemble that block, identify the conditional that flips
   between the two presets.
5. If the conditional is local (a `bg` / `bno` on a register that
   varies with preset), the rule is mechanical. If it requires
   memory lookup, the rule needs a separate measurement.

**This is not started.** The two snapshots exist; the trace
comparison is the next step.

## Phase 2.5 — edit paths 4505/4506/4509, 4625, 4634, 4636

**What it is.** Per `f3_chute_slot_and_count_correction_v0732.md`,
the edit paths' non-default frames are:

| input frame | path | status |
|---|---|---|
| 4505 | KICK + PUNCH + MENU simultaneously | refuses at `0xa6c0` |
| 4506 | KICK + PUNCH simultaneously | refuses at `0xa6c0` |
| 4509 | KICK + PUNCH + MENU simultaneously (variant) | refuses at `0xa6c0` |
| 4625 | menu-only edit (no KICK, no PUNCH) | refuses at `0xa6c0` |
| 4634 | sub-edit combination | refuses at `0xa6c0` |
| 4636 | sub-edit combination | refuses at `0xa6c0` |

**The fence.** None of these input combinations is a natural walk.
They are simultaneous-press states that the model can't author with the
single-button edit path. The fence is **honest refusal** — the
recovered gate is narrow enough to not claim them as silent successes.

**What a focused 2.5 slice does:** **nothing more.** All six remain
fail-closed, by design. The completion plan calls them "all
currently refuse"; the slice is the proof that they continue to refuse
on the current build, and that the gate is **not too wide** (the
neighbouring refused shapes are still refused with their measured
reference counts).

## Why this is a status record and not a recovery

The recovered `phase17_bit7_index5` already implements what is known:

- F1-F3 admit-set per `f3_individual_value_row_recovered_v0732.md`
  (v0732g).
- The 7-instruction `a5 = 4` delta as a measured constant
  (`preset == 0 ? 4193 : 4186`).
- Refusal at the `0xa6c0` gate for the six refused edit paths.

Phases 2.3, 2.4 each require a measurement the repo does not yet
have on disk. Phase 2.5 needs only an audit that the refuse is real,
which is a five-minute ctest entry rather than a recovery slice.

## Validated

- `git diff --stat src`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- The published percentage unchanged at 0.4456%.
- The `ctest -E vf2_player_4505` suite is unchanged (now 120/120 PASS
  with the new `vf2_f4_individual_release` entry wired in at v0734l).
- The recovered code at `texture_bridge_match.c:9269-9294` already
  has the pinned `a5 = 4` rule with the documented "pinned as measured
  rather than derived" comment.

## What the next slice should pick up

- **Phase 2.5 first** — it is the smallest. A ctest entry that
  patches each of the six refused inputs into the input latch and
  asserts `VF2_ERROR_UNSUPPORTED` at the gate. Negative control:
  patch one to a known-admitted latch and assert the gate DOES admit.
- **Phase 2.4 second** — disassembly comparison of two traces. The
  snapshots exist on disk; the comparison is a script.
- **Phase 2.3 third** — it needs a live leg fixture that fires
  `+0x110` / `+0x114`, which is the most expensive of the three.
- **Phase 3 / P3-P4** is the next bulk-boundary work and does not
  depend on Phases 2.3-2.5.
- **Phase 4** (simulation systems) is the largest remaining bulk.
- **Phase 5** is semantic naming, evidence-gated.