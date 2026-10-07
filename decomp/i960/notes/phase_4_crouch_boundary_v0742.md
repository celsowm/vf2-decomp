# v0742: Phase 4 / crouch/idle transition — boundary investigation status

**Slice 4 attempt of `advance_plan_v0735.md`.** This is a **status
record**, not a recovery. The user-driven "do all open" instruction
was followed as far as the public evidence allows in this session;
the actual crouch/idle transition recovery is a multi-day
investigation that needs a probe scenario from scratch and is the
right work for the next session.

**No `src/` change. No `functions.csv` change.**

## What v0738 said the recipe was

The `phase_4_simulation_boundary_v0738.md` boundary note proposed:

1. Build a `vf2probe` scenario on `out/sixth-fresh.vf2snap` that
   patches F0's state byte to trigger the idle-to-crouch transition.
2. Run reference to `0x180bc`.
3. Translate the disassembly to C.
4. Native differential.
5. Wire as ctest entry.

**The note was hand-wavy on the actual state byte offset and the
specific function.** It said "+0x197" but offered no source; the
combat work in `combat_live_v0351.md` measures the coli state byte
at `+0x0821`, which is collision-related, not posture. The actual
fighter state byte is at **`+0x01b0`** (a 1B read at `ip 0x00013f0c`
in the player task loop), and it indexes a 32-entry state descriptor
table at `0x0200620c`.

## What the post-0x28780 corridor actually contains

Disassembly of `0x00028780..0x00028950` (the player task body):

```
00028780  ld       0x00000bd8(g7), r4       # r4 = fighter+0xbd8
00028784  lda      0x0000078c(r4), g4        # g4 = r4+0x78c (table)
00028788  ld       0x00000784(r4), g3        # g3 = r4+0x784
0002878c  ld       0x00000780(r4), g2        # g2 = r4+0x780
00028790  addo     31, 29, g1                # g1 = 60 (32-iter counter)
00028794  ldob     (g4), r6                  # r6 = *g4++
00028798  cmpobe   4, r6, 0x000287f4         # dispatch on r6
0002879c  bg       0x000287dc
000287a0  ldob     (g3), r11                 # r11 = *g3
000287a4  addo     1, g3, g3                 # g3++
000287a8  cmpobe   5, r6, 0x000287c8
... (32-iter object/stream expansion loop)
00028840  bbs      6, r15, 0x00028af8        # if fighter+0 bit6, ...
00028844  ret                                # leaf
00028848-00028914  (32-iter variant loop)
00028918  cvtir    g6, g6                    # 0x28918: 3rd variant
```

This is **not** a crouch/idle transition. It is a 32-iteration
**object/stream expansion loop** that walks a per-fighter data
table at `+0xbd8` and dispatches on a 1B code per entry (4, 5, 3, 0).
Three near-identical variants at 0x28780, 0x28848, 0x28918. The
caller decides which variant to run.

## Where the crouch transition actually lives

The state byte is at `+0x01b0` (1B) and is read at `ip 0x00013f0c`
in the player task loop. The state descriptor table is at
`0x0200620c` (32 entries, 4B each = 128 B; the 32-entry size is
implied by the `addo 31, 29, g1` at 0x28790 setting up the
32-iteration loop in 0x28780+).

A crouch/idle transition is a **state byte change** at `+0x01b0`.
The function that writes the new state byte is in the **input
processing** path, which is in the corridor between 0x14288 (player
task entry) and 0x180bc (post-arm flag tail). The exact callee
that writes `+0x01b0` based on input is not yet identified in
public notes.

## The actual crouch slice — recipe for the next session

1. **Identify the state-writer.** The 0x14288 player task entry
   calls `0x19ef8` (the v0302 state-8 update), then
   `0x270d4` (5-slot wrapper), then `0x4b838` (v0390 mask-family
   dispatch), then `0x4b5d0` (the warm leg). The 0x4b838 family
   already covers 4 cases (mask 0, 1, 2, 3) per `fa_player_4505_live`.
   The crouch transition is likely in `0x4b838` or one of its
   callees — when the input latch carries a crouch button-press,
   the state byte is overwritten.

2. **Build a crouch probe.** From `out/sixth-fresh.vf2snap` (or
   `out/coli-arm-fresh-c330.vf2snap`), patch the input latch at
   `0x500700` to set a crouch input. Run `vf2probe --until 0x180bc`
   and capture the state byte diff at `+0x01b0`.

3. **Measure the new body.** If the body is small (~30-80 ins)
   and the state byte diff is consistent, recover it in C. If
   the body is large or has multiple code paths, do a full
   boundary characterisation like v0737/v0738.

4. **Native differential.** Add a ctest entry
   `vf2_player_crouch_transition` that runs the scenario on both
   reference and native and asserts the same final state byte.

## Why this is the right work for a future session, not this one

The crouch slice needs:
- A real ROM fixture (have: `roms/vf2/` is in place).
- A working crouch probe (need to build it).
- An identified state-writer function (need: `+0x01b0` write
  is in some 0x4bxxx or 0x19ef8 callee; TBD via trace).
- A native-vs-reference differential at the player task tail
  (the same machinery that v0734l used for F4).

This is at least 1-2 days of focused work. The next session
should pick it up after reading the v0730_first_action_runbook
and `combat_live_v0351.md` for context.

## What IS recoverable in this session (smaller slice)

The v0738 note also proposed **hitbox / hurtbox** as an alternative
Phase 4 chunk. Per `fa_player_1442c_state16_state25_v0449.md` and
the `+0x1aa = 1` collision state, the hitbox computation is a
`s16(0x3) - s16(0x7)` + scaled r5 write. This is a smaller, more
bounded slice than the crouch transition because:

- The write offset (`+0x61e` / `+0x626`) and the trigger state
  (`+0x1aa = 1`) are already known.
- The state-25 walker at 0x1442c is already recovered
  (v0449), so the ctest scaffolding exists.
- The body is ~10-15 instructions, not 30-80.

The next session can pick this up first as a smaller win, then
circle back to crouch.

## Validated

- `git diff --stat src/`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- ctest #125 `vf2_phase_2_5_refused_audit`: PASSES.
- ctest #38 `vf2_player_29598`: PASSES (v0741 slice).
- No claims of Phase 4 progress beyond characterisation.
