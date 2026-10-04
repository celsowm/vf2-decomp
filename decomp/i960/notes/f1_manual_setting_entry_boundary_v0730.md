# F1 MANUAL SETTING natural entry — measured starting state (v0730)

> **SUPERSEDED at v0731 by
> `f1_manual_setting_target_remeasured_v0731.md`. Do not implement against
> this note.** Its two load-bearing claims were re-measured and did not
> hold: (1) selector-17 index 4 is **GAME ASSIGNMENT**, not MANUAL SETTING
> (see this file's own reference to
> `texture_bridge_match.c:4902`, "TEST-held GAME ASSIGNMENT entry"), and
> MANUAL SETTING is a label rendered *inside* the COIN ASSIGNMENT menu at
> screen row 35 (`texture_bridge_match.c:7957`); (2) the `0x59f34` /
> `0x59f58` / `0x5a0a4` row dispatch decodes exactly as recorded here but
> is **not reached** by any measured index-5 frame. The disassembly below
> is accurate; its identification as the TEST MENU cursor dispatch and as
> the MANUAL SETTING row handler is unproven. The v0731 note gives the
> corrected F1 definition and the reproductions.

This note captures the **measured F1 starting state** for the next
session. The full F1 slice (natural entry + nested a7 editor) is
multi-hour ROM-backed work per the v0727 playbook and the v0730
first-action runbook; this commit documents the boundary disassembly
and the open handlers so the slice has a clean pickup.

## Entry state (captured, on disk)

- **Snapshot**: `out/sixth-fresh.vf2snap` (v0726-v0728 reference,
  03/10/2026 19:58) — TEST MENU parked state with cursor on row 0.
- **Reference state read at restore**:
  - byte[0x005000a4] = phase_index = 0x0B (read as low byte of u32
    0x00FF000B at 0x5000a4). Phase_index bit-7 (0x80) is clear;
    phase_index is in 0x00..0x0B.
  - byte[0x005000a5] = phase_a5 = 0xA0 (read as low byte of u32
    0x010100A0 at 0x5000a5). Parked-state byte that the recovered
    code's `phase_a5 <= 1` gate rejects in `index0` (selector 17
    index 0).

The probe byte lane does not place 0x5000a4 and 0x5000a5 in the same
u32 read, so byte reads at these addresses do not overlap; treat
phase_index and phase_a5 as **independent** fields and confirm via
the proven read pattern in `execute_frame_phase17_bit7_indexN`.

## Boundary disassembly: cursor dispatch

The TEST MENU cursor row render path is at `0x00059f34` (5
instructions, 20 bytes):

```
0x00059f34  90403000     ld     0x00500704, r8          ; r8 = navigation_flags
0x00059f3c  90483000     ld     0x00500700, r9          ; r9 = input_flags
0x00059f44  80183000     ldob   0x005000a5, r3          ; r3 = phase_a5 (cursor)
0x00059f4c  90183903     ld     0x00059f58[r3*4], r3    ; r3 = table[r3]
0x00059f54  8400d000     bx     (r3)                    ; jump to row handler
```

The jump table at `0x59f58` (8 entries, 32 bytes) reads as:

```
row0 (phase_a5=0): 0x00059f88
row1 (phase_a5=1): 0x00059f9c
row2 (phase_a5=2): 0x0005a000
row3 (phase_a5=3): 0x0005a03c
row4 (phase_a5=4): 0x0005a0a4     ; MANUAL SETTING (TEST MENU row 4)
row5 (phase_a5=5): 0x0005a0e0
row6 (phase_a5=6): 0x0005a148
row7 (phase_a5=7): 0x0005a184
```

Decoded from `epr-18387.14`/`epr-18388.15` (i960 maincpu bank 1,
LOAD32_WORD interleave).

## Row-4 handler (MANUAL SETTING entry render)

`0x0005a0a4` (8 instructions, 60 bytes):

```
0x0005a0a4  lda    0x0100152c, g9       ; menu block ptr
0x0005a0ac  mov    1, g0                ; render id
0x0005a0b4  mov    4, g1
0x0005a0b8  call   0x00008ef0           ; menu render frame
0x0005a0bc  ldib   0x005000a5, r15      ; r15 = phase_a5
0x0005a0c4  lda    0x00000001(r15), r15 ; r15 += 1
0x0005a0cc  stib   r15, 0x005000a5      ; phase_a5 += 1
0x0005a0d4  lda    0x010015ac, g9       ; sound/state ptr
0x0005a0dc  mov    28, g0
0x0005a0e0  call   0x00008440           ; play/setup
0x0005a0e4  ret
```

The row handler **renders** the menu and **increments** phase_a5; it
does NOT write phase_index. The TEST-press entry transition
(phase_index 0x80 → 0x84) is therefore in a different code path
than the cursor dispatch at `0x59f34`.

## Phase_index write convention

`phase_index` is a 1-byte field at `0x005000a4`. The recovered code
writes it from at least 9 sites (`texture_bridge_match.c` lines 1369,
1675, 2078, 2512, 3506, 6340, 8302, 10146, 10539, 10592). The
selector-17 dispatch in `execute_frame_phase17` (line 15384) reads
it back as `(phase_index & 0x80) | lower_index` to select among
`execute_frame_phase17_bit7_indexN`. For the F1 slice, the entry
transition must write `phase_index = 0x84` (bit 7 set, lower 4 = 4).

## What F1 still needs (per-slice pickup)

The v0727 note's "Still open" line for F1 is:

> MANUAL SETTING natural entry and the whole nested a7 editor.

That decomposes into at least three sub-tasks:

1. **Natural entry path** — TEST-press on TEST MENU row 4 transitions
   `phase_index` from 0x80 to 0x84. The TEST MENU handler in
   `execute_frame_phase17_bit7_index0` currently gates
   `phase_a5 <= UINT8_C(1)` (line 1648), which rejects cursor row
   ≥ 2. Extending the gate to admit `phase_a5 == 4` with
   `navigation_flags == UINT32_C(4)` (TEST press) and writing
   `phase_index = 0x84` is the smallest semantic change.
2. **Nested a7 editor** — once `phase_index == 0x84` the
   `execute_frame_phase17_bit7_index4` machinery takes over. That
   function already has substantial recovery (test_held_entry,
   test_nav_entry through test_nav9_entry, test_up1_entry through
   test_up12_entry, render_descriptors, observed_match_count,
   difficulty, packed_flag, special_assignment, initialize,
   exit_control, exit_taken — see
   `texture_bridge_match.c:3796..4660`). The nested a7 editor work
   is the remaining cells in the cursor matrix inside
   `phase_a5 ∈ {0..N}` inside index 4 that the v0727 walk did not
   measure.
3. **Differential proof** — chained `vf2cmp|native-*` strict
   differential from `out/sixth-fresh.vf2snap` through TEST MENU
   cursor nav 4x down to row 4, TEST-press on row 4 (the entry
   transition), then the nested a7 editor's measured state machine.

## Factory chain composition for F1 (per the v0730 runbook)

The factory tooling layer (this session's Layer 2 delivery) provides:

- `tools/python/frontier.py v2` — per-edge fighter offsets, per-source
  attribution, fighter-aware rank, contiguous-fighter-blocks detector
  (used to surface repeated menu-render addresses if F1 reaches
  fighter state).
- `tools/python/taint.py` — characterise which fighter offsets the
  cursor nav (read of `phase_a5`) actually depends on. For F1 the
  cursor is in `0x5000a5` (non-fighter), so taint is light but the
  shape (a single non-fighter byte) validates the cursor dispatch.
- `tools/python/infer_structs.py` — dual-base promotion for any fighter
  field read inside `phase_a5 == 4` renders.
- `tools/python/infer_rules.py` — conservative refusal (won't propose
  a rule from a sparse sample); for F1 the rules are LATCH-TUPLE
  rules, not bitfield rules, so this is not the primary tool.
- `tools/python/check_scenario.py` — validates the F1 entry scenario
  JSON before any sweep.

The factory chain integration test (`test_factory_chain.py` 4/4) and the
runnable exemplar (`factory_chain_demo.py`) confirm the same public
Python API surface F1 would consume is wired and regression-protected.

## Reference reading order (for the next session)

1. `decomp/i960/notes/v0730_first_action_runbook.md` — single
   entry-point for the next agent.
2. `decomp/i960/notes/factory_runbook_v0729.md` — 5-step playbook.
3. `decomp/i960/notes/playable_coin_assignment_natural_v0727.md` —
   the v0727 walk through COIN ASSIGNMENT (selector 17 index 5),
   which has the same shape as MANUAL SETTING (index 4). The natural
   entry body lengths, render body lengths, and poststate rule in the
   v0727 note are the closest analog for F1's measured oracle
   behaviour.
5. This note — F1 starting state.

## Anti-traps

- Do not hand-write the row-4 render in C without first measuring
  the call targets `0x00008ef0` and `0x00008440` (the menu render
  and the post-render setup). The v0727 work shows the v0726 hand
  render for COIN ASSIGNMENT row 0 was retired in favour of measured
  per-derivation data blocks.
- Do not weaken `infer_rules.py`'s refusal contract.
- Do not promote `field_xxxx` to a semantic name without independent
  evidence.
- Do not commit `.vf2snap`, large JSONL, or derived proprietary
  artifacts. `out/` is gitignored.