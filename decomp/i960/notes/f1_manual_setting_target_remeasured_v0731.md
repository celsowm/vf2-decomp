# F1 target re-measured: MANUAL SETTING is not selector-17 index 4 (v0731)

This note **supersedes the starting-state claims** in
`decomp/i960/notes/f1_manual_setting_entry_boundary_v0730.md`.
Nothing was implemented this session: the goal was to re-measure the F1
target before writing C against it, per AGENTS.md rule 1 (evidence before
implementation) and rule 10 (when uncertain, preserve the boundary).

Result: the v0730 note's central premise does not hold. The real F1 gap is
narrower and differently located. Details and reproductions below.

## 1. Oracle baseline is intact (re-verified)

`out/ca-test2.vf2snap` is the v0727 "TEST MENU -> coin entry" starting
point. Restored state, read with `--max-steps 1` (important: `--max-steps 0`
is **not** a no-op — it runs the 2,000,000-instruction default, which
silently advances the state; this tripped an earlier reading in this
session):

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 --snapshot out/ca-test2.vf2snap \
  --max-steps 1 --read-u32 0x005000a4 --read-u32 0x00500700 --read-u32 0x00500704
# a4 word 0xFFFF0085 -> phase_index=0x85, phase_a5=0x00, a6=0xff, a7=0xff
# 0x500700 = 0x0f000004 (TEST held), 0x500704 = 0 (no nav)
```

So `ca-*` snapshots are **inside** selector-17 index 5 (`phase_index` 0x85,
bit 7 set), not on the bit-7-clear parent. `out/ca-idle-r1.vf2snap` is the
same screen at `phase_a5 = 1` with the settled idle latch `0x0f000000`.

One frame from `ca-test2`, stopped at the frame return `0x0000a010`:

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 --snapshot out/ca-test2.vf2snap \
  --max-steps 200000 --until 0x0000a010 --read-u32 0x005000a4
# run_instructions 4420, halt_reason "stop address", ip 0xa010
```

4420 = the v0727-documented `232 cluster prefix + 4188/35 body`. The v0727
COIN ASSIGNMENT corridor is therefore still reproducible on this checkout;
this session changed no C.

## 2. Two documented index-5 bodies reproduce exactly

Both measured from `out/ca-idle-r1.vf2snap` (`0x85`, `phase_a5 = 1`,
settled idle latches), one frame each to `0x0000a010`:

| leg | latch patch | `run_instructions` | v0727 documented body |
| --- | --- | --- | --- |
| SERVICE/DOWN tap | `0x500700=0x0f001000`, `0x500704=0x1000`, `0x500708=0`, `0x50070c=0x0f000000` | 4426 | 232 + `4194/34` nav body |
| TEST edge | `0x500700=0x0f000004`, `0x500704=0x4`, `0x500708=0`, `0x50070c=0x0f000000` | 4637 | 232 + `4405/38` row-1 COMMON->INDIVIDUAL edit |

The row-4 value-edit body also reproduces, measured from `ca-test2` with
`phase_a5` forced to 4 (`--set-u8 0x005000a5=4`) plus the TEST edge above:
`run_instructions 4635` = 232 + `4403`, the documented row-4 TEST edit body.
This is the key negative result for the v0730 note: **a TEST press with
`phase_a5 = 4` inside index 5 is a value-row edit, not a submenu entry.**

## 3. Selector-17 index 4 is GAME ASSIGNMENT, not MANUAL SETTING

`src/recovered/texture_bridge_match.c:4902` documents the index-4 entry in
its own recovered comment:

```
/* TEST-held GAME ASSIGNMENT entry (measured from the 0x9ff8 failure
 * state): input/previous latch 0x0f000004 while TEST is held, ...
```

`execute_frame_phase17_bit7_index4` is entered from
`execute_frame_phase17` for `phase_index == 0x84`
(`texture_bridge_match.c:15455`). The v0727 note's "TEST MENU -> GAME
ASSIGNMENT re-entry ... proven index-4 machinery re-enters cleanly" is
consistent with this. The v0730 F1 note instead identifies index 4 as
MANUAL SETTING.

## 4. MANUAL SETTING is a label *inside* the COIN ASSIGNMENT menu

`execute_frame_phase17_bit7_index5` renders the parent menu from a
measured text-run table (`texture_bridge_match.c:7926`-`7961`):

```c
{5u,  18u, "COIN CHUTE TYPE",                 2u},
{6u,  18u, "CREDIT TO 1P START",              2u},
{7u,  28u, "1P CONTINUE",                     2u},
{8u,  18u, "CREDIT TO VS START",              2u},
{9u,  28u, "VS CONTINUE",                     2u},
...
{35u, 18u, "MANUAL SETTING",                  2u},
{38u, 18u, "EXIT",                            2u},
{44u, 20u, "SELECT BY SERVICE BUTTON",        2u},
{45u, 22u, "AND PUSH TEST BUTTON",            2u}
```

and again in the INDIVIDUAL-layout branch at `texture_bridge_match.c:8536`
(`6 * 0x80`, column 25).

So the COIN ASSIGNMENT menu shows a **MANUAL SETTING** entry at screen row
35 and **EXIT** at row 38, while the SERVICE cursor ring is **six
positions**: `phase_a5 = 0..5`, proven by `nav_cc_forward[6]` /
`nav_cc_back[6]` (`texture_bridge_match.c:9051`-`9052`) and by the four
distinct nav bodies `4194` (forward, non-wrap), `4195` (forward wrap at
`a5=5`), `4191` (back, non-wrap), `4192` (back wrap at `a5=0`).

Consequence: **the MANUAL SETTING row is not one of the six
SERVICE-selectable ring positions.** It is rendered, and the menu footer
says "SELECT BY SERVICE BUTTON / AND PUSH TEST BUTTON", but the recovered
cursor ring provably stops at `a5 = 5`. How row 35 is actually selected is
**not measured** and is now the first open question for F1.

The ROM text records agree on the naming. From
`out/analysis/strings.csv` (regenerated this session with
`vf2i960 analyze roms/vf2 out/analysis`):

```text
0x00078128  "COIN/CREDIT SETTING     #  "
0x00078148  "MANUAL SETTING"
0x0007815c  "    COMMON"
0x000781e8  "    MANUAL SETTING"
0x00078200  "    MANUAL SETTING"
0x00078224  "COIN TO CREDIT"
0x00078350  "1 COIN COUNTS AS   COINS"
0x00078370  "MANUAL SETTING"
0x0004ab54  "SETTING NUM"
0x0004ab84  "MANUAL SET"
```

The two indented `"    MANUAL SETTING"` records and the
`"SETTING NUM"` / `"MANUAL SET"` pair are the likely MANUAL SETTING page
labels, i.e. the "nested a7 editor" the v0727 note refers to.

## 5. The `0x59f34` dispatch is not on the index-5 corridor

The v0730 note's disassembly of `0x00059f34` is **accurate** — reproduced
verbatim, including the `0x59f58[r3*4]` jump-table load and `bx`:

```text
00059f34  ld       0x00500704, r8
00059f3c  ld       0x00500700, r9
00059f44  ldob     0x005000a5, r3
00059f4c  ld       0x00059f58[r3*4], r3
00059f54  bx       (r3)
```

But it is not reached by the live index-5 frames. A full instruction trace
of one index-5 frame (`--trace`, 4420 `step` records, `ip_before` is
**decimal**, not hex) contains only 10 instructions in `0x59000`-`0x5b000`:

```text
0x58fe0 0x58fe8 0x58ff0 0x58ff8 0x59000 0x59008 0x59010
0x59154 0x59158 0x59160
```

Neither `0x59f34` nor `0x5a0a4` appears. The frame runs
`0x58fe0`..`0x59160` and then hands off at `0x59160`; `0x00059164` is the
indirect target that `execute_frame_phase17_bit7_index0` requires, i.e. the
next function in the same region, well before `0x59f34`. So the `0x59f34`
row dispatch and its `0x5a0a4` row-4 handler belong to a screen this
session did not visit. Their identification as the TEST MENU cursor and the
MANUAL SETTING row is **unproven**.

Trace artifact: `out/f1-screen05-frame.jsonl` (gitignored, regenerable).

## 6. Corrected F1 definition

The open slice is **not** "enter selector-17 index 4". It is:

1. **Which input path selects the COIN ASSIGNMENT `MANUAL SETTING` entry at
   screen row 35?** The six-position `a5` ring does not reach it. Candidate
   paths to measure, in order of cheapness:
   - a cursor step past `a5 = 5` (does the ROM ring actually have a 7th
     position that the recovered `next > 5 -> 0` clamp hides?);
   - a LEFT/RIGHT axis rather than SERVICE up/down;
   - a different `phase_a5` range entirely (the index-6 handler admits
     `a5` up to 41, so wider cursor spaces exist in this family);
   - the `"    MANUAL SETTING"` indented text records at `0x000781e8` /
     `0x00078200`, which may belong to a different page reached from row 35.
2. **The entry frame's poststate** — instruction/call counts, CC, globals
   and the `phase_index` write (whether row 35 sets bit 7 and to which
   lower index), measured at the `0x0000a010` frame boundary.
3. **The nested a7 editor** after that, per the v0727 note.

Everything stays fail-closed until each step is measured and proven by a
chained strict differential from `out/sixth-fresh.vf2snap`.

## 7. Tooling cautions for the next session

- `vf2probe --max-steps 0` is **not** a no-op; it runs the 2,000,000-step
  default and returns a post-execution state. Use `--max-steps 1` when the
  intent is "restore and read".
- `vf2probe --trace` emits `ip_before`/`ip_after` as **decimal** integers.
  Grepping for `"ip":"0x..."` silently matches nothing and yields a false
  negative — that mistake briefly produced a wrong conclusion in this
  session before the format was checked against the raw record.
- `out/analysis/*` (from `vf2i960 analyze`) covers the **default bank
  only**. `function-splits.csv` has zero entries in `0x00059xxx`, and
  `xrefs.csv` has no entry for `0x00059f34`, so xref-driven navigation will
  not find bank-1 code. Use targeted `disasm` plus live traces instead.
- The v0727 walk driver was not committed; the `ca-*` snapshots remain in
  `out/` (gitignored). Reconstruct legs with `vf2probe --set-u32` on the
  four latch words `0x500700` / `0x500704` / `0x500708` / `0x50070c` and
  stop at `--until 0x0000a010`.
- **Frame chaining does not work the obvious way.** `--until 0x0000a010`
  is checked *before* each step, and a frame *ends* at `0x0000a010`. So a
  snapshot captured with `--until 0x0000a010` resumes already sitting on
  the stop address, and the next leg immediately halts with
  `run_instructions: 0`, `halt_reason: "stop address"`. Only snapshots
  captured at the frame **entry** are usable chain points; the v0727
  `ca-*` snapshots sit at `ip = 0x00000530`, which is the step right after
  the `0x00009ff8` diagnostic call and yields the documented 4420.
- **Do not try to re-arm with `--until 0x00009ff8`.** From a `0x0000a010`
  snapshot that ran the full 200,000-step limit and ended at
  `ip = 0x00010fe8` without ever reaching `0x00009ff8` — the bridge frame
  is not the next task dispatched, so there is no cheap re-arm. Chaining
  additional legs therefore needs a pre-built entry snapshot, not a
  stop-address loop. This is the concrete reason the v0727 session needed
  its own `ca-*` captures.

## 8. Validation

This slice changed no C, test or CMake file, so the gate result is a
regression check rather than a proof of new behavior.

- `cmake --build build --config Debug --parallel` — clean.
- `ctest --test-dir build -C Debug` — **117/117 passed, 0 failed**,
  1776.99 s total. Two entries dominate the wall time:
  `vf2_player_4505_live_tests` (~25 min) and
  `vf2_player_14640_state27_live_tests`; the 13
  `vf2_python_factory*` entries finish in ~12 s combined.

## 9. Anti-traps

- Do not implement an index-4 MANUAL SETTING entry; index 4 is
  GAME ASSIGNMENT and already has substantial recovery.
- Do not treat the `0x59f34` / `0x59f58` / `0x5a0a4` disassembly as
  "measured F1 starting state". It decodes, but no measured frame reaches
  it, and the MANUAL SETTING attribution is unproven.
- Do not add a row-35 a5 position by widening the recovered
  `next > 5 -> 0` clamp. Measure first; a 7th ring position is a
  hypothesis, not a finding.
- Do not rename index-4/index-5 semantics in code comments from this note
  alone. The GAME ASSIGNMENT naming is the recovered code's own comment
  (`texture_bridge_match.c:4902`) and the MANUAL SETTING row-35 label is a
  measured render (`texture_bridge_match.c:7957`); both are recorded here
  as evidence with citations, not as a rename.
- Do not commit `out/` artifacts, snapshots or traces.
