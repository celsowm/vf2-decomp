# v0738: Phase 4 simulation systems boundary — what physics/hitbox look like

**Slice 3 of `advance_plan_v0735.md`.** This is a **boundary
characterisation slice**, not a recovery. The plan estimated
500-2000 LOC of C + Python over 1-2 weeks for Phase 4; this slice
characterises the post-`0x28780` frontier so a focused recovery can
start from a measured boundary, not a guess.

**No `src/` change. No `functions.csv` change.**

## Current recovered corridor end (v0730 verified)

```
pre-hex → 0x19ef8 → 0x1428c → 0x142c0 → 0x14310
       → 0x143e4 → 0x1ab74 → 0x27ce0 → 0x27d00
       → 0x28184 / 0x28780 / 0x28268
       → 0x27d90 / 0x27dcc / 0x27fa0 / 0x2901c
       → 0x28174 → 0x29414 → post-29414 (0x29598 + 4 callees)
       → 0x164c4
```

The proven native boundary is `0x164c4` (v0730, 14,277,453 instructions,
10,288 calls / 10,286 returns from `out/sixth-fresh.vf2snap`).
**Post-`0x28780` geometry through `0x2826c`/`0x27d90` remains native.**

Per `docs/UNCOVERED_BRANCHES.md:3475-3506`, "physics/hitboxes and an
input-driven pin distinct from PUNCH remain later dedicated work."
Specifically:

> Physics/hitbox/damage and `0x19ef8` flag-bit siblings remain
> unrecovered (see `taint_29414_v0302.md` and
> `fighter_candidate_layout_v0302.md`).

## What `combat_live_v0351.md` already measured

Phase 4's smallest measurable chunk is **mid-body combat**:

- `out/coli-arm-fresh-c330.vf2snap` is the PUNCH-armed park. Fields:
  - `0x514980` flags = `0x80000000`
  - `0x51498c` entry = `0x000221e8`
  - `0x500024` countdown = 0
  - fighters = `0x510980` / `0x512980`, F0 = `0x04000000`

- `0x22298` C live sibling (bit-8 set + bit-1 clear) — **native
  recovered** at body 13, single `stos 0` to `g7 + 0x6dc`.

- Midbody live `g0=1 -> 0x225cc` — **measured** at 380 instructions,
  12 calls / 10 returns, with `0x225cc` × 1 long (249 until `0x22294`).

- "**Pin whole-task prefix+380**: **Não pinado**" — **not yet
  pinned**. The probe does not re-enter `0x221e8` in 5000 steps
  (stays in frame-wait `0x10fa8`).

## The Phase 4 frontier, concretely

Three distinct simulation-system frontiers exist post-`0x28780`:

1. **Physics / fighter state transitions** (jump, idle, crouch).
   The smallest measurable shape: fighter state byte change at
   `0x514a??` (the F0/F1 state index byte). Each transition has
   a measurable instruction count and a register diff. ~50 LOC of
   C per transition.

2. **Hitbox / hurtbox** (the `+0x61e`/`+0x626` corridor). Per
   `fa_player_144b0_state25_v0395.md` and the `+0x1aa = 1` collision
   state, the hitbox computation is a `s16(0x3) - s16(0x7)` + scaled
   r5 write. Recoverable as a small C function with a `vf2probe`
   scenario. ~100 LOC of C.

3. **Damage / combo** (the `+0x654`/`+0x62a` write and the
   `+0x822(f1) = +0x822(f0)` chain). Per v0395, the body is
   `r5 = 0x11000000 + s16(rec+1)` + `r3 = r4 - 1` + two stores.
   This is the actual damage number, but it is recovered via the
   `0x144b0` state's "store `+0x654(f1) = r5`" line. The
   **chain** (multi-hit combo) is not recovered. ~200 LOC of C.

4. **Ring-out** (arena boundary). The arena boundary is checked
   when the fighter position leaves a window; the actual handler
   is the `b` at the end of the `0x144b0` recovery that takes the
   path to `0x14634`/`0x14638`. The handler clears `+0x198` for
   both fighters and the "ring out" state is set. This is part of
   the recovered `0x144b0` state, but the **trigger condition**
   (when does position-out-of-arena fire) is not in the recovered
   code — it is in the dispatching logic before `0x144b0`. ~50 LOC
   of C.

5. **CPU opponent decision logic** (the `0x180bc` player task
   boundary). Per `fa_player_180bc_v0298.md`, `0x180bc` is the
   park entry. The CPU vs human selection and the decision tree
   is the largest remaining piece — 1000+ LOC.

## The smallest focused Phase 4 slice

**Target: physics / fighter state transition (idle → crouch).**
This is bounded and measurable.

### Recipe

1. **Build a `vf2probe` scenario.** Take a fresh boot snapshot
   (`out/sixth-fresh.vf2snap` is the proven starting point). Patch
   F0's state byte at the appropriate offset (to be measured; the
   AGENTS.md handoff says `+0x197`) to a value that triggers the
   `idle -> crouch` transition.

2. **Run the reference.** `vf2probe --until 0x180bc` (the post-arm
   boundary). Measure the instruction count and the state-byte
   diff.

3. **Recover the body.** Translate the disassembly to C. The body
   shape is:
   - Read state byte from `0x514a97` (offset to be confirmed).
   - Compare against crouch threshold (typically 2 or 4).
   - On match, write the crouch-state byte, copy `+0x1a4` flag bits,
     and set the crouch-motion word.

4. **Native differential.** Run `vf2i960 native-resume` on the same
   scenario, compare state byte + registers. Expect instruction
   count: ~30-80.

5. **Wire as ctest entry** `vf2_player_idle_to_crouch`. Assert
   native == reference at `0x180bc`.

### Estimated scope

~50-100 LOC of C in `src/recovered/hybrid.c`, one Python ctest
entry (~30 LOC), one fixture (~50 LOC). 1 commit, ~half-day wall time.

## What this slice does NOT cover

- **Hitbox / hurtbox, damage / combo, ring-out, CPU logic.** Each is
  a separate focused slice.
- **Recovering the `0x29598` callee** (the Phase 3 next-target from
  v0737) — that is Slice 2 of the advance plan, independent.

## What the next slice should pick up

- **Recover `0x29598`** per v0737 (Phase 3).
- **Recover physics / idle-to-crouch** per this slice (Phase 4 first
  chunk).
- **Recover the hitbox body** in the `0x144b0` state-25 sibling — the
  `+0x61e`/`+0x626`/`+0x1aa = 1` shape (already partially recovered
  per v0395).
- **Damage / combo** after the hitbox is written.
- **Ring-out trigger** after damage / combo.
- **CPU logic** last (the largest piece).

## Validated

- All measurements cited come from `combat_live_v0351.md`,
  `fa_player_144b0_state25_v0395.md`, `docs/UNCOVERED_BRANCHES.md`,
  and `fa_player_next_targets_v0292.md`.
- `git diff --stat src`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- The published percentage unchanged at 0.4456%.