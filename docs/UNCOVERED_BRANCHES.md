# Mapping of Uncovered and Unobserved Branches (v0.1.3)

This document catalogs major unobserved execution paths and unrecovered
subsystems in Virtua Fighter 2 Version 2.1. The accepted clean-room corridor now
runs through the eleventh-dispatch validation corridor, but it remains one evidence-backed
sequence rather than a complete game implementation. Unsupported paths return
`VF2_ERROR_UNSUPPORTED` instead of falling back to i960 interpretation.

The v0168 boundary audit removed the two remaining single-instruction oracle
handoffs in `native-second-dispatch`: the `ret` stubs at `0x0004bab4` and
`0x000020ec` are now recovered bridges. The strict post-scheduler corridor is
therefore 1,270,824 recovered instructions with zero interpreted instructions,
and the modular return boundaries remain exact through the repeated third to
sixth dispatch validations plus the eleventh-dispatch continuation (the strict
sixth-dispatch base now ends at the tenth `fa_game_info` entry with `8`
repeated scheduler entries, so dispatches 7-10 are covered per-block inside
the sixth command and `native-nth-dispatch 11` proves one further 37-block /
2,166-instruction cycle exact). v0347 measured MATCH through dispatch 40
and pins dispatch 12 in CTest; see `decomp/i960/notes/tracks_bcd_v0347.md`.

## 0. Display / first screen landmark (v0353)

The natural cold-boot corridor now has measured first display states:

- post-boot diagnostic tile plane at the runtime checkpoint `0x0004aff8`
  (`BACKUP RAM IS BROKEN.` at tile `0x010008aa`, `I/O Initialize ...` at
  `0x01000c28`) — recovered C plus unit pins;
- settled sixth-dispatch park `0x0001645c` with frame selector `0x11`
  (TEST MENU family) under `native-sixth-dispatch` MATCH.

Still open for a literal Sega attract screen: no ASCII `SEGA` tile appears on
this trajectory; polygon/TGP attract content and the EXIT TEST MODE →
warm-boot → attract continuation remain explicit boundaries. Diagnostic
selector-17 workers require the frame-dispatch boundary (`0x0000a6c0` /
main-final-cluster), not a `fa_game_info` IP.

See `decomp/i960/notes/display_landmark_v0353.md`.

### v0360 SEGA warning screen (measured + native unit pin)

Frame **selector 0** at `0xa804` draws the Model 2 legal signature when
COUNTRY==0 and the latch at `0x59cfe0` does **not** match
`{0x52455320,0x4e4c2053,0x4e204544,0x20514555}`. ROM strings include
**`SEGA ENTERPRISES,LTD.`** at `0x0000aaad`. Natural witness:
`park-after-irq` → one frame **15853** instructions, selector **1**,
countdown **640**, styled glyphs at `0x01000332..0x01001650`. Warm parks
holding the latch take the **34-insn** path to selector 2 and skip SEGA
(explains v0353–v0356; `0x80xx`-only decoders also miss `0x89xx` glyphs).
Game-attract 3D logo after warm sel 2/3→`0x11` TEST MENU remains open.
See `decomp/i960/notes/sega_warning_screen_v0360.md`.

### v0361 attract sel2/3 → TEST MENU (measured fail-closed)

Natural chain after the SEGA warning: countdown sel1 → sel2 (`0xab0c`) →
sel3 (`0xacf8`, phase table 0–17) → handoff phases 12–15 to selector 16
(`0x10a0c`, recovered `++sel` + `a4=0x0b`) → **TEST MENU 0x11**. Same
terminal state when forcing selectors 4–15 on that park. Geometry/light
streams still identical to TEST baseline; texture-ram hash changes without
a unique SEGA mesh witness. Game-title attract remains closed until a
machine state is measured that does not take the 0x10/0x11 handoff.
See `decomp/i960/notes/attract_sel3_to_testmenu_v0361.md`.

### v0362 attract nav-gate `0xa748` + `teste` frontier (measured)

Scheduler helper `0xa748` forces **frame selector 16** when work `0x500704`
has **bit 26 or bit 2** set (oracle `stib` at `0xa76c` from sel 2). Natural
parks often show `0x500704=0x0f000000` (bit 26), which explains the
sel2→0x10→0x11 TEST chain independent of sel3 phases. Clearing `0x500704`
each `resume-trace` frame from `sega-after-cd` measures attract **sel 3 /
phase 3** (geometry/buffer hashes change; tiles clear; not TEST). The
reference executor then fails closed at **COBR `teste` @ `0x00019024`**
(fighter/object attract body). Game-title 3D logo remains unwitnessed.
See `decomp/i960/notes/attract_nav_gate_teste_v0362.md`.

### v0364 COBR `teste` admitted + attract TGP FIFO (measured)

Reference executor now implements architectural COBR `test*` (0x20–0x27)
as `dest = cc ? 0xffffffff : 0`. Attract with `0x500704` cleared advances
past `0x19024` (sel 3 / phase 3, countdown 256→253); long park shows
texture-ram first-64k non-zero (**4360**) versus TEST baseline **0**.
`--memory-trace` measures **1157** copro FIFO writes at `0x00884000`
during that attract body (function codes + IEEE-like words). Game-title
3D logo mesh still unwitnessed. See `decomp/i960/notes/teste_attract_tgp_v0364.md`.

### v0365 attract phases 3–8 + FIFO differential (measured fail-closed logo)

With `0x500704` held clear, the reference oracle drains selector-3
phase-3 countdown (~1 per frame, 256→0), then reaches phases **4, 5,
6, 7, 8** without entering TEST MENU. Phase 8 remains in an object/task
loop at `0x4c7xx`. Texture-ram first-64k non-zero rises **0 → 4360 →
~10154** on attract parks versus **0** on `sixth-fresh` TEST. Copro
FIFO writes at `0x00884000`: **1157–2016** (attract) vs **9** (TEST).
No named SEGA/title 3D mesh witness (tiles empty; FIFO not correlated
to a unique object). Logo 3D recovery stays fail-closed.
See `decomp/i960/notes/attract_long_fifo_v0365.md`.

### v0366 attract phases 3–14 + video ready latch (measured)

Selector-3 attract phases **3 through 14** are reachable in the
reference oracle when `0x500704` gate bits stay clear and measured
counters are stepped. Phase workers 8/9/14 implement ROM: nonzero
`[0x500834]+0x50` decrements; when zero, `0x00550000==1` **returns**
(video-ready wait; natural post-phase-7 is 1 per v0026); else
`balx 0x00009444` text thunk. Static writes to `0x550000` sit at
`0x4b414` / `0x4b83c` / `0x4ba14` (video command submit). Phase 14
stalls after re-arming ready=1; coli object spin at `0x224xx` stops
frame-dispatch visits. Texture attract ~10154 nz vs TEST 0; still no
named 3D logo mesh. See
`decomp/i960/notes/attract_phases_ready_gate_v0366.md`.

### v0367 video-ready clear `0x4bfc4` + FIFO/texture correlation

ROM stores `0x00550000=1` at `0x4b414`/`0x4b83c`/`0x4ba14` and
clears it at **`0x4bfc4`** only when `r14==0` and texture counter2
(`0x005502e0`) is zero. Recovered `execute_texture_final_status_call`
(`0x4bf90`) implements the same clear when counters
`0x5502c0/d0/e0` are all zero. Portable `model2a`/`vf2_tgp` do **not**
auto-clear the latch on video completion. Attract phase-14 parks show
`ready=1` with counters already 0 (clear path not taken in the object
spin); selector-3 phase14 ROM/C worker does **not** store `phase+1`.
Measured FIFO/texture delta: attract **1157–2016** copro FIFO writes
and texture-64k nz **~10154** versus TEST **9** / **0**. Named 3D
logo mesh remains unwitnessed. See
`decomp/i960/notes/ready_clear_fifo_corr_v0367.md`.

### v0368 oracle pin texture final-status `0x4bf90`

Controlled probe from an attract phase-14 park sets IP to
`0x0004bf90` and stops at `0x0004bfdc`. Measured: all texture counters
zero and `0x550000=1` → **12** instructions, ready **cleared to 0**;
nonzero counter0 or counter2 → ready **stays 1** (6 / 10 steps).
Board `0x508000` bit 9 set skips `call 0x4d25c`. Recovered C unit
already pins the same clear at **13** counts including `ret`.
After a measured clear, selector-3 phase14 still does not store
`phase+1`; named 3D logo remains unwitnessed.
See `decomp/i960/notes/final_status_pin_v0368.md`.

### v0369 sel3 tail oracle + phase14/15 fail-closed (measured)

From the attract phase-14 park with `0x550000` cleared, the oracle
measures phase-14 not-ready at **11** instructions to thunk
`0x00009444` and does **not** store `phase+1` (400k-step resume stays
phase **0x0e** in the `0x4c7xx` object spin). Forced workers measure
phase **15→16** (u16 mask 1, task countdown armed **128**), **16→17**,
and **17→0**. ROM phase-15 special clusters fire when the u16 mask is
**0x700 / 0x540 / 0x380 / 0x1c0** and blit `0x8f1c` descriptors from
main_data (e.g. `0x02a69cd2` 7×26 → tile glyphs **0x88xx** at
`0x01000124`); that payload is sequential glyph indices, **not** a
named SEGA 3D mesh. Recovered C now returns `VF2_ERROR_UNSUPPORTED`
for those special masks and for the phase-14 not-ready thunk path.
Named 3D logo remains unwitnessed. See
`decomp/i960/notes/attract_tail_phase15_masks_v0369.md`.

### v0370 phase store map + final-status `0x4d25c` bit9 clear (measured)

ROM maincpu contains **91** absolute `ldib`/`stib` sites for
`0x00500030`. Selector-3 advance uses the measured `phase+1` epilogue
in workers 3–13/15/16; phase14 (`0xc0a4`) has **no** phase store.
Oracle memory-trace of 80k attract instructions with ready=0 records
**zero** writes to `0x500030` (wrapper only snapshots `0x500031/34`).
Text thunk `0x7fc0` blits C-strings as tile glyphs `0x80xx`. With
texture counters zero and board `0x508000` bit 9 **clear**, oracle
final-status takes **13** instructions to status-tail **`0x4d25c`**
(mode byte `0x50002b=0x03` → thunk dest `0x010000e2`); the probe park
entered the tail with ready already 1 (not a tail re-arm). Dispatch C
recovers status-tail modes `0x0c`/`0x0d` (dual thunk); the non-dispatch
helper still fail-closes those modes. Named 3D logo remains
unwitnessed. See `decomp/i960/notes/phase_stores_status_tail_v0370.md`.

### v0371 status-tail oracle (real ROM) + attract thunk (measured)

From the attract park, oracle `0x4d25c → 0x4d2bc` measures mode
**0x03** at **156** instructions (tile writes only at `0x010000e2`),
mode **0x0c** at **305** and mode **0x0d** at **307** (writes at
special `0x010040e2` **and** common `0x010000e2`; the **+2** delta
matches the recovered unit pin). ROM payloads at `0x4d28c`/`0x4d2ac`
are fifteen `0x20` bytes plus NUL, blitted as glyphs **`0x8020`**.
Phase-14 not-ready thunk: **11** steps to `0x9444` and **15** to
`0x9468` with no tile writes on this park. Unit
`run_common_only_spaces` pins the mode-3 common-only path. Named 3D
logo remains unwitnessed. See
`decomp/i960/notes/status_tail_oracle_v0371.md`.

### v0372 attract submits polygon-ROM objects (measured, logo name open)

Oracle attract phase-5 memory-trace **reads** the TGP object table at
`0x020e0004` (ids **0x148**, **0x088**, **0x145**, family `0x144–0x152`)
and **writes** their word0 values to geo/FIFO `0x800010`/`0x804000`
(e.g. `0x000b026a` = id **0x148**). Helper **`0x7c60`** and task
**`fa_pol_test` `0x21a00`** produce the same FIFO words (`0x800101`,
`0x1800303`, `0x3000606`, `0x1a003434`) on the oracle. Table `w2` with
bit `0x00800000` addresses **polygons.bin** as a **word index**
(`vf2_tgp_read_polygon_word`); id 0x148 → word `0x40430`, dimensions
`w3=0x014a0164`. No `SEGA`/`LOGO`/`TITLE` ASCII in main_data or
polygons.bin. Host-side triangle decode of measured offsets produces
3D fragments (e.g. 384 tris at id `0x1cb`) but **no named SEGA logo
mesh**. Named 3D logo remains unwitnessed; do not invent a mesh name.
See `decomp/i960/notes/attract_poly_objects_v0372.md`.

Helper **`0x00007c60`** is now recovered in C as
`vf2_recovered_polygon_object_submit` (`src/recovered/polygon_object_submit.c`).
Covered: gate `0x50101c > 0x501018 → ret`; FIFO preamble `0x1a003434` via
`(g11)[g12]`; table `ldq` at `0x020e0004[g0*16]`; `st w0 → g10+0x10`;
`stq` via `(g10)[g12]` with r11 forced to `-1`; optional `g1` path
(`r9 += (w3>>16)*4`, `0x5010d0 += w3_low`); `0x501010++` and
`0x50101c += w3_low`. Fail-closed when the table read is unavailable.
Unit + ROM-backed differential pin `0x148→0x000b026a` and
`0x88→0x0008e6de`. Nearby helpers `0x7d14`/`0x7d6c`/`0x7e50`
and inlined sibling `0x1962c` remain **unsupported**. FIFO color
immediates and `display_command_emit` ids are **not** part of this body.

### v0375 host mesh pipeline + TGP transforms absent (measured, fail-closed logo)

Host analysis tools only (no recovered-C semantics change): multi-view
raster `tools/python/render_mesh_host.py` and object-table rank
`tools/python/rank_poly_objects.py` over **4096** poly-ROM ids at
`0x020e0004`. Dense meshes are overwhelmingly **skip3**-coherent
(`tgp.c` `geometry_mode&3<2`; ex. `0x5c7=515` tris, `0x1cb/0x33e=384`);
attract family `0x88/0x140–0x157` stays low-density under host float-link
(best both_agree: `0x148` 10/9). Streaming of attract/boot FIFO+geo
traces measures **0** accepted TGP class-**0x09/0x0b/0x0c** (focus/matrix/
translate) commands after protocol false-class filtering — class bits on
FIFO word0 collide with color immediates (`0x14802929`, …). Measured
host-usable Work-RAM only: display triple `(6.0f, 4.7f, 18.5f)` at
`*(u32*)0x50084c+0x54`, camera-scale globals `0x501084/0x501088=600.0f`.
`vf2probe --max-steps 0` is **not** a park freeze. Named 3D logo mesh
remains **unwitnessed**. See
`decomp/i960/notes/render_mesh_host_v0375.md`,
`object_rank_v0375.md`, `fifo_transforms_v0375.md`.

### v0376 camera store IPs + host views (measured, matrix still absent)

Live store IPs for Work-RAM camera/display state (probe BEFORE/AFTER):
display triple `(6.0f, 4.7f, 18.5f)` written at **`0x31024`** in
`display_transform_defaults@0x31004`; camera scale `600.0f` at
**`0x1d34c`/`0x1d35c`** in `fa_camera@0x1d320`; aperture cursor
`0x5001e4` stored at **`0x31094`** (`display_command_emit@0x31040`
serializes the triple into aperture `0x0090e000[cursor]` — not geo 3x4).
FIFO word **`0x0b001616`** is a **copro arithmetic tag** (class bits
`0x16`, not TGP `0x0b`) paired with `600.0f` → camera task+0x5c/60
(`223.2f`/`172.8f`) — **do not promote to matrix opcode**. TGP
class-09/0b/0c stream commands remain **absent** (reconfirmed on
geo-port 928 writes + snap geometry windows). Host re-render
`render_mesh_host.py` gains analysis views composing measured
Work-RAM only under auto-fit (silhouette-identical to iso-xy until a
non-uniform matrix exists) and optional `--view tgp` JSON hook
(`confidence=absent`). Contact-sheet column-count bug fixed. Named
3D logo still **unwitnessed**. See
`decomp/i960/notes/tgp_camera_state_v0376.md`,
`host_render_views_v0376.md`.

### v0377 pol_test packet boundary + matrix ports + phase5 scene

**P1 (packet):** `fa_pol_test@0x21a00` path A gold FIFO measured
`0x800101,0x1800303,0x3000606` + floats + helper `0x1a003434` +
close `0x1000202`; helper `0x7f24` palette-like geo words from
`0x501400`. Per-id submit writes only object-table **w0/w1/w2** and
`r11=-1` — **polygon-ROM vertex floats never appear on i960 geo/FIFO
writes**. TGP consumes poly ROM via `w2` address outside the guest
oracle. Host `skip3_float_link` remains an **unproven** hypothesis
(hex-calibrated on id `0x97d` attr `0xe1001601`); vertex-stream
decode **ABSENT/UNPROVEN** — fail-closed for recovered C.
See `decomp/i960/notes/packet_format_p1_v0377.md`.

**P2 (matrix ports):** Eleven ports checked (aperture `0x0090e000`,
display object, camera task, work-RAM `0x500000–0x520000`, TGP
function `0x00880000`, upload `0x00980000`, geo control/program,
copro FIFO): **no measured TGP 3x4 matrix / focus**. Aperture holds
display triple only; camera `+0x5c/60` are FIFO arith scratch.
`out/attr-transform/measured_view.json` stays
`confidence=absent`. See `matrix_ports_p2_v0377.md`.

**P3 (scene):** Streaming `fifo-phase5.jsonl` yields **336** object
events / **112** unique ids; host multi-object layout labeled
`no_scene_matrix`; FIFO `0xAABBCCDD` family treated as
`protocol_tag` (not game palette). See `scene_phase5_p3_v0377.md`.
Named 3D logo remains **unwitnessed**.

### v0378 `fa_pol_test` path A + helper `0x7f24` recovered in C

`vf2_recovered_pol_test_path_a` (`src/recovered/pol_test_path_a.c`)
covers the measured path-A body: gate `mode 0x00530150 >= 2`
(`cmpoble 2, r14`; mode < 2 stays `VF2_ERROR_UNSUPPORTED`), palette
pack `0x7f24` (six words `(sext(hi)<<16)+(0x17f-sext(lo))+*(u32*)0x5013f0`
to `(g10)[g12]`), FIFO prelude/interstitial/close immediates, loop
submits via reused `vf2_recovered_polygon_object_submit`
(`0x986` unless mode==3, final always `0x985`, `g1=0`), instruction
pins 163/162/127 for mode=2/3-count=1 and mode=2-count=0. Oracle
re-measured: task traces stop `0x21b00` at 162/161 steps and the
15-word gold FIFO appears verbatim on the reference memory-trace.
Path B (`0x97f/0x97e/0x97d` ids, `0x3f428f5c…` immediates), palette
semantics, vertex stream and scheduler dispatch of `fa_pol_test`
remain unsupported. New guest-edge inventory (`explore_geo_edges.py`)
reclassifies exactly one multi-word geo-stream push outside `0x7c60`:
`0x19684` (`st`/`stq` w0/w1/w2/`-1` shape); TGP class-`0x07/09/0b/0c`
writes stay at **0**, and object-table `w2` consumption stays outside
the guest oracle. See `decomp/i960/notes/pol_test_path_a_v0378.md`,
`geo_edge_coverage_v0378.md`, `tgp_w2_consumption_v0378.md`.

### v0354 EXIT TEST MODE → warm-boot attract (measured)

From the sixth-dispatch park at frame-dispatch `0x0000a6c0`, forcing phase
`0x8b` measures the recovered EXIT TEST MODE first visit at **13,286**
instructions (tile `EXIT TEST MODE`, countdown 320, `a5=0xff`) and the
terminal path at **13,194** instructions into boot entry `0x000000b0`.
Warm boot then takes the **valid-backup** CRC path (not `BACKUP RAM IS
BROKEN`), advances frame selector **0 → 2 in 34 instructions**, passes
selector `0x10` (tile clear `0x8ef0` + redraw `0x7fc0`), and **returns to
TEST MENU** (selector `0x11`) while arming `fa_coli` `0x000221e8`.
No ASCII `SEGA` tile appears on this trajectory; polygon/TGP attract with a
different game/config state remains the open Sega-logo frontier.
Unit pin: `test_frame_dispatch_selector0_signature_fast_path`.

### v0355 input-driven display (measured, negative for Sega tile)

Strict `vf2cycles` from the sixth MATCH park: COIN+START keeps TEST MENU;
PUNCH naturally enters EXIT TEST MODE, runs the countdown to zero under
native lockstep (**332 cycles MATCH**), and redraws TEST MENU. Geometry
FIFO and buffer-ram payloads are identical across those parks (color ramp,
not a unique logo mesh). No ASCII `SEGA` tile on this input-driven path.

### v0356 attract + player `0x27cc8` corridor (measured)

Factory-like backup does not unlock attract/Sega tiles. The standard
`0x4505` player drive from sixth reaches `0x270d4` in 1,749 instructions and
`cvtri` at `0x00027cc8` in 1,709 more (3,458 total, matching the v0352
trace). Wrapper `0x270d4` issues five measured `0x27b5c` calls; the C helper
already covers that body on the `0x1428c` path. Admitting `0x270d4` remains
open pending a live five-slot final-state pin.
See `decomp/i960/notes/attract_player_27cc8_v0356.md`.

### v0357 `0x27b5c` valid vs degenerate (measured + fail-closed)

Zero record/scratch (sixth/punch) is the cvtri-fault shape; C now refuses
it. Parks `player-1428c-*` with record `0x0201c2fc` run `0x270d4 → 0x2712c`
in **9,378** instructions filling five scratch slots. Byte-exact slot pin
against the C expander remains open.

### v0358 five-slot pin blocked on COBR condition codes

Record selectors at main_data `0x0201c2fc` are `{0x0505,0x0039,0x00f1,
0x00e7,0x00af}`. The `0x27b5c` second loop dispatches with `cmpobl`+`be`.
Making COBR `cmpo*` update `compare_result` (hardware-faithful) measures
`0x270d4→0x2712c` at **9,235** instructions and makes recovered C match
oracle on all five slots — but breaks corridor MATCH on `fa_kill_osage`
and phase17/bridge differentials that calibrated stale CC. Executor keeps
legacy COBR (no CC write); `0x270d4` stays `VF2_ERROR_UNSUPPORTED`.
Reopen after a dedicated CC recalibration campaign.
See `decomp/i960/notes/player_270d4_slot_pin_v0358.md`.

### v0359 COBR CC recalibration + `0x270d4` admitted

Executor COBR `cmpo*`/`cmpi*` now set `compare_result` and AC condition
bits together. Recovered exits pin measured last-cmpo CC for `fa_kill_osage`,
`fa_osage0/1`, and first-sweep scheduler finish (GREATER at `0xa014`).
`native-first-dispatch` and `native-sixth-dispatch` MATCH. Player wrapper
`0x270d4` is native-admitted: five-slot ROM pin at **9235** insns,
cursors `g3=0x520630 g5=0x50ea98 g6=0x50e2d0`. Degenerate
record/scratch remains fail-closed.

### v0380 `phase17_zero` per-path CC pinned (202/202 green)

The `phase17_zero` differential failed on final `compare_result` only
(native EQUAL/NONE vs oracle LESS/GREATER/EQUAL per path). A 682k-step
reference trace grouped all cases into 12 tail shapes sharing one
compare-free common tail (`ret @ 0xa6f4 → 0x1004`); each native exit
now replicates its measured last compare with AC bits in lockstep:
word-scan exit (unsigned order vs `0xffffff`), fa_control0 tail as a
function of live mode byte `*(0x50002b)`, preamble
(`cmpobe 0, *0x5000a6`), rect countdown exit, index8 tail on the
pre-update `player0+0x158` field, and per-next transition tails.
Along the way: in the `vf2_i960_run` path `executor.c` compiles with
`vf2_i960_step=vf2_i960_step_legacy`, so legacy COBR semantics apply
(`cmpo/cmpi` incl. branch variants write CC; `bbs/bbc` never do —
3312 executions, zero CC changes). Full `ctest` 62/62 with no corridor
regressions. See
`decomp/i960/notes/phase17_cc_pins_v0380.md`.

## 1. Scheduler and task execution

Recovered scanning uses live registry strides, skips inactive descriptors and
handles the observed changing final active task. Still uncovered:

- dynamic task creation and deletion;
- abnormal task exits and error paths;
- corrupt or structurally different task registries;
- alternate priorities, preemption and interrupt-driven scheduling orders; and
- scheduler/task combinations not reached by the accepted repeated corridor.

## 2. Gameplay (`fa_game_info` and related gates)

The accepted corridor includes active input/state selector fast paths,
the player-update bit-14 exit and their observed sequence gates. Still missing:

The reference i960 executor now covers the conditional range comparisons and
single-precision integer/real conversions encountered when the fighter-state
bit-31 path is forced. The `fa_game_info` dispatcher and its post-call tail are
recovered in C. Both observed `0x18144` invocations now recover the
118-instruction prefix through `0x18538` plus the observed `0x17b68`
`ld`/`bbc`/`ret` helper, both observed `0x18d44` floating-port paths, and the
`0x18c64`/post-call suffix through `0x18640`, and both observed `0x18644`
shared-fighter corridors returning at `0x164b0` and `0x164c4`; unobserved
branches remain explicit ROM-backed boundaries and remain unrecovered as
native C. The observed state-4/bit-15 and non-state-4 bit-15 prefixes are now native. Controlled bit-14/15/16/6 probes now cover the `0x181c0` through
`0x184ec` conditional body; the observed bit-4/6/8/14/15/16 dependent `0x18644`
flag-accumulation paths are native. The ROM-backed state-4 fixture matrix now
matches all 192 cases for flag bits 6, 14, 15 and 16 at both non-negative and
negative thresholds; the complete negative matrix is native, removing the
dispatcher fallback for this four-bit state-4 corridor. The ROM-backed state-8
fixture matrix now
matches all 96 cases for flag bits 1, 2 and 4, and all 192 cases when flag bit
8 is included as the fourth dimension, covering isolated, asymmetric and
bilateral distributions with both countdown and mode-bit-6 settings. With
high bits 21, 26, 29, 30 and 31 appended, both threshold endpoints (`-1` and
`0`) match the complete 3,072-case matrix (256 masks × 12 distributions).
The
low-result branch now applies the same `+0x5b6` update rule as the
high-result branch when `r9 > threshold`. The state-8 negative matrix is also
native and exact for all 192 combinations of bits 1, 2, 4 and 8. State-8
negative cases composed of bits 1, 2, 4, 8, 21, 26, 29, 30 and 31 are now
admitted to the native path and match the complete 3,072-case matrix
(256 masks × 12 distributions) exactly. This includes every isolated,
asymmetric and bilateral distribution, both countdown values, both mode-bit-6
values and all high-bit/low-bit combinations. At non-negative thresholds, the
same high-bit fixture family is exact across the complete matrix as well,
including masks 248..255.
State-8 bit 6 is now admitted for the negative threshold: with the same
nine-bit set, its full ten-bit matrix matches 12,288 fixtures (1,024 masks ×
12 distributions) exactly. On the positive threshold, the recovered child
matches the complete no-bit-8 submatrix for bits 1/2/4/6 (192 fixtures), plus
the eight tested masks containing bit 6, bit 8 and all five high bits,
covering every subset of low bits 1/2/4 (96 more fixtures). Positive bit-6
compositions outside the measured slices remain unproven or explicit
boundaries. The measured
positive `0x140` (state bit 8 + bit 6) and `0x142` (state bit 8 + bits 1 + 6)
fighter-state compositions are additionally exact across all three physical
distributions, both countdown values and both mode-bit-6 settings (24 more
fixtures total). The measured positive `0x144` composition (state bit 8 + bits
2 + 6) is also exact across its 12 distributions/countdown/mode cases. The
measured positive `0x150` composition (state bit 8 + bits 4 + 6) is also exact
across its 12 distributions/countdown/mode cases. The measured positive
`0x146` composition (state bit 8 + bits 1 + 2 + 6) is now exact across its
12-case matrix, and the neighboring `0x152`, `0x154` and `0x156` compositions
are exact as well. The measured positive high-bit composition `0x4140` (state
bit 8 + bits 6 + 14) is also exact across its 12-case matrix, including its
bilateral order-dependent joins. The adjacent positive `0x8140` composition
(state bit 8 + bits 6 + 15) is now exact across its 12-case matrix as well.
The adjacent positive `0x10140` composition (state bit 8 + bits 6 + 16) is
also exact across its 12-case matrix. Other positive high-bit compositions
remain unproven or explicit boundaries. The measured triple `0x0c140` (state
bit 8 + bits 6 + 14 + 15) is now exact across its 12-case matrix as well.
The measured triple `0x14140` (state bit 8 + bits 6 + 14 + 16) is now exact
across its 12-case matrix as well.
The measured triple `0x18140` (state bit 8 + bits 6 + 15 + 16) is now exact
across its 12-case matrix as well.
The measured all-high composition `0x1c140` (state bit 8 + bits 6 + 14 + 15 +
16) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x204140` (state bit 8 + bits 6, 14
and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x208140` (state bit 8 + bits 6, 15
and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x210140` (state bit 8 + bits 6, 16
and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x20c140` (state bit 8 + bits 6, 14,
15 and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x214140` (state bit 8 + bits 6, 14,
16 and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x218140` (state bit 8 + bits 6, 15,
16 and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x21c140` (state bit 8 + bits 6, 14,
15, 16 and 21) is now exact across its 12-case matrix as well.
The measured bit-14 cross-family extensions `0x4004140` (high bit 26),
`0x20004140` (high bit 29), `0x40004140` (high bit 30) and `0x80004140`
(high bit 31) are now native and exact across their 12-case matrices. The
admission is limited to one of these four single high bits; their measured
first-order, second-order and bilateral countdown/mode joins share the
validated accounting rule, while multi-high extensions remain unsupported.
The measured bit-15 extensions `0x4008140` (high bit 26), `0x20008140`
(high bit 29), `0x40008140` (high bit 30) and `0x80008140` (high bit 31),
and the corresponding bit-16 extensions `0x4010140`, `0x20010140`,
`0x40010140` and `0x80010140`, are now native and exact across their
12-case matrices. Each admission is limited to one of the four measured
single high bits; bit-15 uses the validated first-order, second-order and
bilateral accounting rule, while bit-16's measured correction is limited to
the bilateral countdown/mode joins. Multi-high extensions remain unsupported.
The corresponding bit-14 + bit-15, bit-14 + bit-16, bit-15 + bit-16 and
bit-14 + bit-15 + bit-16 cross-family extensions with each of high bits 26,
29, 30 and 31 are now native and exact across their 12-case matrices as
well. Each admission is limited to one measured single high bit; the
countdown/mode corrections are separately measured for each of the four
fighter-bit compositions. Multi-high extensions remain unsupported.
For the bit-14 + bit-16 and bit-15 + bit-16 compositions, all six pairwise
masks from high bits 26, 29, 30 and 31 are now native and exact across their
12-case matrices as well. These twelve pair admissions are limited to the
measured pairs and reuse the corresponding single-high accounting rules;
larger multi-high combinations remain unsupported.
The four measured high-bit triples for each of the bit-14 + bit-16 and
bit-15 + bit-16 compositions are now native and exact across their 12-case
matrices as well: `0x64014140`, `0xa4014140`, `0xc4014140`, `0xe0014140`,
`0x64018140`, `0xa4018140`, `0xc4018140` and `0xe0018140`. Admission is
limited to these eight measured triples and reuses the corresponding pair
accounting rules; larger high-bit combinations remain unsupported.
The two measured all-four-high extensions `0xe4014140` (bit-14 + bit-16)
and `0xe4018140` (bit-15 + bit-16) are also native and exact across their
12-case matrices. Admission is limited to these two masks and reuses the
corresponding triple accounting rules; other larger or high-bit-21
combinations remain unsupported.
The measured bit-14 + bit-16 high-bit-21 pair extensions `0x04214140`,
`0x20214140`, `0x40214140` and `0x80214140`, and the corresponding bit-15
+ bit-16 extensions `0x04218140`, `0x20218140`, `0x40218140` and
`0x80218140`, are now native and exact across their 12-case matrices. Their
admission is limited to one measured pair of bit 21 with 26, 29, 30 or 31;
larger high-bit-21 combinations remain unsupported.
All six bit-15 + bit-16 high-bit-21 triples are also measured exact across
their 12-case matrices: `21+26+29`, `21+26+30`, `21+26+31`, `21+29+30`,
`21+29+31` and `21+30+31` (masks `0x24218140`, `0x44218140`, `0x84218140`,
`0x60218140`, `0xa0218140` and `0xc0218140`). This is evidence for the
existing corridor only; other positive compositions remain unsupported.
For the bit-14 + bit-15 composition specifically, all six pairwise masks from
high bits 26, 29, 30 and 31 (`26+29`, `26+30`, `26+31`, `29+30`, `29+31` and
`30+31`) are now native and exact across their 12-case matrices as well. The
pair admission is limited to those six measured masks and reuses the measured
bit-14 + bit-15 accounting rule; pairs involving high bit 21 and larger
multi-high combinations remain unsupported.
The four measured bit-14 + bit-15 pairs combining high bit 21 with one of
high bits 26, 29, 30 or 31 are now native and exact across their 12-case
matrices as well. Their admission is limited to those four masks and uses the
same measured accounting rule; larger multi-high combinations remain
unsupported.
All six bit-14 + bit-15 high-bit-21 triples are now native and exact across
their 12-case matrices: `21+26+29` (`0x2420c140`), `21+26+30`
(`0x4420c140`), `21+26+31` (`0x8420c140`), `21+29+30` (`0x6020c140`),
`21+29+31` (`0xa020c140`) and `21+30+31` (`0xc020c140`). Each uses an
isolated, measured `0x164c4` accounting rule; other positive compositions
remain explicit unsupported boundaries.
The existing shared corridor has also been measured for the bit-14 + bit-15
 + bit-16 composition with high-bit combinations `21+26`, `21+29`, `21+30`,
 `21+31`, `21+26+29` and `21+26+29+30+31` (masks `0x421c140`, `0x2021c140`,
 `0x4021c140`, `0x8021c140`, `0x2421c140` and `0xe421c140`). Each completed
 its 12-case positive-threshold matrix exactly, including both countdown and
 mode-bit-6 variants. This records measured coverage of the common corridor;
 other unmeasured high-bit compositions remain explicit boundaries.
The complete `0x1645c` dispatcher now also has strict full-task coverage for
state 8 with isolated bit 14: all three fighter-record distributions and both
countdown/mode-bit-6 settings match the reference. The bilateral distribution
uses one measured dispatcher-instruction accounting correction; unilateral
records retain the ordinary count. Other positive dispatcher compositions
remain explicit boundaries until their full-task accounting is measured.
The same dispatcher now admits the six measured state-8 field masks
`0x0421c000`, `0x2021c000`, `0x4021c000`, `0x8021c000`, `0x2421c000` and
`0xe421c000` (bit-14 + bit-15 + bit-16 with the measured high-bit-21
compositions). Each mask matches all three fighter-record distributions and
both countdown/mode-bit-6 settings at threshold `0`; the bilateral controls
also match thresholds `1` and `2`. The bilateral cases use the measured
two-instruction accounting correction.
Other positive dispatcher compositions remain explicit boundaries.
The complete dispatcher also now admits state 8 with bits 6+14+21, field mask
`0x00204000`. Its three fighter-record distributions match both countdown/mode
settings at threshold `0`, and all three distributions match both mode values
at thresholds `1` and `2`. The measured accounting relation includes a fixed
bilateral join plus distribution-specific countdown/mode terms; other positive
bit-6 compositions remain explicit boundaries.
The adjacent state-8 bit-6+bit-15+bit-21 field mask `0x00208000` is also
admitted. Its three distributions match both countdown/mode settings at
threshold `0`, and all three distributions match both mode values at
thresholds `1` and `2`; its accounting uses the separately measured
unilateral and bilateral countdown corrections.
The corresponding state-8 bit-6+bit-16+bit-21 field mask `0x00210000` is
also admitted. It matches the same three distributions, countdown/mode
settings and thresholds `0..2`; only the bilateral distribution requires the
measured one-instruction join correction.
The compound state-8 bit-6+bit-14+bit-15+bit-21 field mask `0x0020c000`
is also admitted. All three distributions match both countdown/mode settings
at threshold `0`, and both mode values at thresholds `1` and `2`; its native
child requires the measured distribution-specific accounting terms and the
bilateral base join.
The sibling state-8 bit-6+bit-14+bit-16+bit-21 field mask `0x00214000` is now
admitted as well. All three distributions match both countdown/mode settings
at thresholds `0`, `1` and `2`; its native child uses the measured
distribution-specific mode/countdown joins, while thresholds outside the
measured nonnegative range remain explicit boundaries.
The sibling state-8 bit-6+bit-15+bit-16+bit-21 field mask `0x00218000` is now
admitted too. All three distributions match both countdown/mode settings at
thresholds `0`, `1` and `2`; only the bilateral distribution requires the
measured two-instruction dispatcher join.
The all-three state-8 bit-6+bit-14+bit-15+bit-16+bit-21 field mask
`0x0021c000` is now admitted. All three distributions match both
countdown/mode settings at thresholds `0`, `1` and `2`; like its adjacent
bit-14/bit-16 and bit-15/bit-16 siblings, it uses a measured bilateral
two-instruction join.
The measured state-8 bit-14+bit-16+high-bit-26 field mask `0x04214000` is
also admitted. All three distributions match both countdown/mode settings at
thresholds `0`, `1` and `2`; its native child and dispatcher use the measured
bit-14/bit-16 high-bit accounting joins, while other high-bit compositions
remain explicit boundaries.
The adjacent state-8 bit-14+bit-16+high-bit-29 field mask `0x20214000` is
also admitted. All 36 combinations across the three fighter-record
distributions, countdown `0/1`, mode bit 6 clear/set and thresholds `0..2`
match the reference at `0x10dcc`, including the full CPU/memory snapshot and
instruction/call/return counters. Its dispatcher uses the measured
bit-14/bit-16 accounting joins and the measured condition-state postcondition;
other high-bit compositions remain explicit boundaries.
The next adjacent state-8 bit-14+bit-16+high-bit-30 field mask `0x40214000`
is admitted under the same measured threshold range. Its 36-case matrix also
matches the full reference snapshot and instruction/call/return counters;
the native child uses the shared bit-14/bit-16 accounting joins and the same
condition-state postcondition. Other high-bit compositions remain explicit
boundaries.
The corresponding state-8 bit-14+bit-16+high-bit-31 field mask `0x80214000`
is now admitted under the same measured threshold range. Its 36-case matrix
also matches the full reference snapshot and instruction/call/return counters;
the native child uses the shared bit-14/bit-16 accounting joins and the same
condition-state postcondition. Other high-bit compositions remain explicit
boundaries.
The first measured high-bit pair, bit-14+bit-16 with high bits 26 and 29,
uses field mask `0x64014000` and is now admitted as well. Its 36-case matrix
matches the full reference snapshot and instruction/call/return counters; it
uses the shared distribution accounting, while all three distributions leave
the measured `NONE` condition-state postcondition. Other high-bit pairs remain
explicit boundaries.
All six full-dispatch state-8 bit-14+bit-16 compositions admitted so far
(`0x04214000`, `0x20214000`, `0x40214000`, `0x80214000`, `0x64014000` and the
new bit-21 triple `0x44214000`) have been re-measured and re-admitted under
the committed `validate_game_info_full_dispatch.py` calibrated task-entry
fixture, which is now the single measurement harness for this corridor. The
earlier dispatcher-accounting constants and NONE/EQUAL condition splits for
the first five masks came from an earlier ad-hoc fixture lineage and did not
reproduce; every one of the six 36-case matrices (three fighter-record
distributions, countdown `0/1`, mode bit 6 clear/set, thresholds `0..2`) now
matches the full reference snapshot, architecture signature and all counters
exactly under one shared measured rule: unilateral accounting `+3 + mode6`
and bilateral accounting `+5 + 2*mode6` instructions, an EQUAL/LESS final
condition state keyed on the countdown byte (left naturally by the isolated
high-26 mask), and measured stale-historical-frame postconditions in the
frame slot beyond the final call depth. Unadmitted neighbours such as
`0x60214000` and out-of-range threshold `3` still fail closed.
Seven further no-bit-21 pair/triple compositions (`0x24014000`,
`0x44014000`, `0x84014000`, `0x60014000`, `0xA0014000`, `0xC0014000` and
`0xA4014000`) are now admitted through the same unified measured rule, each
with its complete 36-case matrix exact, followed by every remaining
bit-14+bit-16 state-8 composition: the isolated high bits without bit 21
(`0x04014000`, `0x20014000`, `0x40014000`, `0x80014000`), the remaining
bit-21 pairs and triples (`0x24214000`, `0x84214000`, `0x60214000`,
`0xA0214000`, `0xC0214000`), the quads (`0xE4014000`, `0x64214000`,
`0xA4214000`, `0xC4214000`, `0xE0214000`), the full quint `0xE4214000` and
the base-only mask `0x00014000`. The whole family — all 32 high-bit subsets
over bits 21/26/29/30/31 on the bit-14+bit-16 base — now matches the
reference exactly across its 36-case matrices under one shared accounting
rule, natural condition-state tail and shared stale-frame postconditions;
the fallback ROM-child path had hidden this join behind a different raw
baseline (see
`decomp/i960/notes/game_info_1645c_full_state8_family_completion_v0123.md`).
Outside-family compositions such as `{14,15}` or `{15,16}` bases and
thresholds above 2 still fail closed.
The bit-14+bit-15 state-8 base family is now admitted as well: all 28
compositions over the `0x...C000` base — the base-only mask, every high-bit
subset over bits 21/26/29/30/31, and the previously stale `0x0020C000` —
match their complete 36-case matrices exactly under one measured per-record
rule (`nrecords * (3 - 4*countdown + mode6)` dispatcher instructions),
natural condition-state tail and shared stale-frame postconditions.
A full audit of every remaining legacy full-dispatch admission against the
committed harness found eleven masks whose predicates still carried
superseded fixture-lineage constants: the two-bit bit-21 compounds
`0x00204000`, `0x00208000`, `0x00210000`, `0x00218000` and all seven
measured bit-14+bit-15+bit-16 compositions (`0x0021C000`, `0x0421C000`,
`0x2021C000`, `0x4021C000`, `0x8021C000`, `0x2421C000`, `0xE421C000`).
Their re-measured accounting is genuinely distribution-asymmetric (for
example `0x0421C000` f0-only versus f1-only differ by 5 instructions in the
mode-bit-6 cells) and is not yet recovered, so those admissions are retired
and the masks fail closed until properly re-measured. Every state-8
full-dispatch admission now remaining in the tree is proven directly under
the committed validator (see
`decomp/i960/notes/game_info_1645c_full_state8_bit14_15_family_and_audit_v0124.md`).
All eleven masks have now been re-admitted from measured data. The seven
bit-14+bit-15+bit-16 base compositions (`0x0021C000`, `0x0421C000`,
`0x2021C000`, `0x4021C000`, `0x8021C000`, `0x2421C000`, `0xE421C000`)
retain the v0125 measured per-fighter accounting (fighter-0 side `3`, or `2`
when countdown/mode6 is set; fighter-1 side
`3 + 5*mode6 + 4*cd − 4*cd*mode6`; bilateral the exact sum except the
isolated 21-only mask, whose bilateral table is `{4, 8, 7, 8}`). The four
bit-21 compounds are independently measured: `0x00204000` and `0x00208000`
retain their v0125 tables (including the measured stale `r15=0` postcondition
for `0x00208000`), while v0126 closes the last architectural gap for
`0x00210000` and `0x00218000`. The recovered `0x18644` child now performs the
reference read/modify/write that sets bit 15 of the 16-bit field at
`fighter + 0xb24` for those exact active masks, preserving all other bits.
Their measured dispatcher deficits are `+4` unilateral / `+7` bilateral for
`0x00210000` and `+4` unilateral / `+8` bilateral for `0x00218000`. See
`decomp/i960/notes/game_info_1645c_full_state8_bit14_15_16_recalibration_v0125.md`
and `decomp/i960/notes/game_info_1645c_full_state8_bit16_compounds_v0126.md`.
The bit-14 + bit-16 triple-high extension `0x24214140` (21+26+29) is now
native and exact across its 12-case matrix. Its correction is isolated to the
measured `0x164c4` return corridor; the neighboring 21+26+30 composition still
measures only 3/12 exact and remains unsupported.
The corresponding bit-14 + bit-16 extension `0x44214140` (21+26+30) is now
native and exact across its 12-case matrix as well, with the same isolated
return-corridor accounting. The neighboring 21+29+30 composition remains a
3/12 control and is still unsupported.
The third measured extension `0x84214140` (21+26+31) is now native and exact
across its 12-case matrix; the other high-bit triples were still unmeasured at
that point.
The measured extension `0x60214140` (21+29+30) is now native and exact across
its 12-case matrix. Its neighboring 21+29+31 control remains 3/12, preserving
the explicit boundary for the unmeasured triple.
The final bit-14 + bit-16 high-bit-21 triples `0xc0214140` (21+30+31) and
`0xa0214140` (21+29+31) are now native and exact across their 12-case
matrices. This completes all six combinations of bit21 with two of high bits
26, 29, 30 and 31 for this family; other positive compositions remain
unsupported.
The measured bit-14 + bit-15 triple-high extensions `0x6400c140`,
`0xa400c140`, `0xc400c140` and `0xe000c140` (high-bit triples 26+29+30,
26+29+31, 26+30+31 and 29+30+31) are now native and exact across their
12-case matrices. Admission is limited to these four measured triples and
reuses the same accounting rule; other triple-high and larger combinations
remain unsupported.
The measured positive isolated bit-6/high-bit masks `0x200140` (bit 21),
`0x4000140` (bit 26), `0x20000140` (bit 29), `0x40000140` (bit 30) and
`0x80000140` (bit 31) are each exact across their 12-case matrices. The
aggregate masks `0x24000140` (bits 26+29) and `0xe4200140` (bits
21+26+29+30+31) are exact across their 12-case matrices as well.
All ten pairwise combinations of high bits 21, 26, 29, 30 and 31 with state
bit 8 and fighter bit 6 are exact across their 12-case matrices too: masks
`0x4200140`, `0x20200140`, `0x40200140`, `0x80200140`, `0x24000140`,
`0x44000140`, `0x84000140`, `0x60000140`, `0xa0000140` and
`0xc0000140`. These ten pairwise matrices contain 120 measured fixtures;
the 26+29 matrix was already counted in the aggregate evidence above, so the
nine newly added matrices extend the measured positive family to 360 unique
cases. Other positive high-bit compositions remain explicit boundaries.
All ten high-bit triples and all five high-bit quads over bits 21, 26, 29,
30 and 31 with state bit 8 and fighter bit 6 are now ROM-measured through the
full `0x1645c` dispatcher as well. Each mask is exact across 36 fixtures
covering the three fighter distributions, countdown 0/1, mode bit 6 clear/set
and thresholds 0..2 (540 fixtures total). Together with the existing singles,
pairs and all-five-high mask, every non-empty high-bit subset is now admitted
for the no-low-bits positive bit-6+bit-8 family; unmeasured low-bit mixes remain
fail-closed.
The controlled probes
now cover `g0 == 0`, `g0 == 1`, `g0 == 2` and `g0 == 3`; the shared
`0x18e08`/`0x18e00` command-port helper body and a controlled low-result
`0x18644` threshold outcome are also covered. The `0x18978..0x189a4` high-state flag tail is native as well: bits 26..29 use the measured progress/limit gate and bits 30..31 are accumulated unconditionally. The following shared `0x189a8..0x189bc` CHKBIT/ALTERBIT tail is native too, including its observable condition-code result and bit-3 accumulation. The type-22 tail now covers both the progress mismatch and the coherent equal-progress `0x18bd4` call path, including the generic `0x1ab34` type-record resolver and the measured bit-2-clear `0x18b58` branch. Unobserved downstream comparisons and other conditional branches remain ROM-backed or unsupported.

The same bridge now covers the following `fa_player` task entry at
`0x00013f08`. Its observed 842-instruction bootstrap through the first nested
call at `0x00014288`, followed by the accepted 1,652-instruction `0x19ef8`
corridor through `0x0001428c` and the downstream geometry expansion through
`0x000142c0`, followed by the small setup corridor through `0x00014310`, the
observed preamble through `0x000143e4` and its state-neutral prefix through
`0x000143fc`, the observed `0x0001ab74` entry prefix through `0x0001abf4`,
the `0x00027ce0` entry prefix through `0x00027d00`, the immediate calls to
`0x00028184` and `0x00028780`, the observed prefix through `0x00028268` and
the accepted `0x00028780` geometry body and the measured
`0x00027d90`/`0x00027dcc`/`0x00027fa0 -> 0x0002901c` and
`0x00028174 -> 0x00029414` calls and the measured `0x00014400 -> 0x00017710`,
`0x00014404 -> 0x0001791c`, `0x00014408 -> 0x0004b640`,
`0x00014414 -> 0x00016504` and `0x00014418 -> 0x000180bc` chain, is native C and matches the ROM
endpoint exactly; later player branches remain explicit original-i960
continuations. A real sixth-entry snapshot therefore
advances through both fighter task records and back to the main loop while
retaining clearly marked ROM-backed boundaries.

- character and arena selection;
- the complete match state machine, timeout and game-over transitions;
- fighter physics, hitboxes, hurtboxes, collision, damage and combos;
- ring-out and arena-boundary handling;
- CPU opponent decision logic; and
- evidence-backed portable fighter/object structures above raw addresses.

The accepted `0x19ef8` corridor now runs the recovered `0x1a1e4`
selector-setup interpreter (`player_selector_execute_setup`) instead of
the previous manual record stores; the `0x505`/`0x284` caller guard and
the post-interpreter `+0x1a8`/`+0x1aa` stores remain (v0294). Measured
next player targets after the coli closure are recorded in
`decomp/i960/notes/fa_player_next_targets_v0292.md`.

Status (v0389): the live `0x505` selector-`0x19ef8` corridor
`0x14288 → 0x1428c` is pinned ROM-backed at 1622 steps / 4 calls /
4 rets from the parked `out/pre14288.vf2snap` entry (`+0x1a4 == 0`,
CC=NONE, `g0 == 0x505`): opcode-1 `0x1a408` copies + post-addo cursor
`0x200e8b3`, mode-dispatched 20x3 expansion (5/52/3 histogram),
`g2 = table + 27` census advance, census-outer-17 float arm
(`0x29724[5] = 0x16e`, triple `0, 0, 0x46800013 → 0, 0, 0x4000`,
`+0xbdc = 0x20`). The forced-g0 `0x4505` shape never occurs live and
stays fail-closed. The shared opcode-8 fix (no fighter stores; the
`0x1a408` copies belong to opcode 1) keeps planar rotation tests
green. See `decomp/i960/notes/player_19ef8_live_v0389.md`. The frozen
`0x1428c` head (setbit-26 + `27b5c` fanout) is the next boundary.

Status (v0390): the measured `0x1428c → 0x142c0` head is native at
9247 steps / +6 calls / +6 rets (10869 total with the corridor, +10/+10
on base/boot/natres parks): setbit-26 store (F0 `0x800 → 0x4000800`,
no CC write), `call 0x270d4` five-slot wrapper off record `0x0201c2fc`
(selectors `0x0505/0x0039/0x00f1/0x00e7/0x00af`, 9235/+5/+5, exits
EQUAL via the cmpdeco tail), and the 7-insn `0x1429c → 0x142c0` tail
(`+0x10 = 0x501500`, `+0x0c = 0x142f4`, `g0 = +0x640`, `g1 = +0x04`,
`g2 = +0x1b0`). Entry gated on record `0x0201c2fc` + entry F0
`0x800`; zero record/scratch and unmeasured selectors stay
fail-closed. See `decomp/i960/notes/player_1428c_head_v0390.md`. The
next boundary is the `0x142c0` geometry-expansion body (`call 0x4b838`);
`hybrid_execute_player_142c0` is native-chained (passes
`native-sixth-dispatch`) and the partial `resume-trace` witness from
`at142c0-a.vf2snap` (56 steps, +3 calls, +3 rets) is recorded; a
focused byte-exact fixture (five-slot payload + float triple vs
`player_270d4_slot_pin_v0358.md` style) remains open. See
`decomp/i960/notes/player_142c0_v0391.md`.

Status (v0392): the `0x142c0` body now has a focused ROM-backed
differential fixture (`vf2_player_142c0_live_differential`) restoring
`out/pre14288.vf2snap`, stepping the reference through the full
corridor + head + body to `0x14310` (10925 / +13 / +13) and asserting
full live-state equality against the native wrappers. The focused
comparison exposed and fixed a register-modeling error the coarser
`native-sixth-dispatch` did not catch: at the `0x14310` boundary the ROM
leaves `r15 == phase` (byte `0x0050002b`, loaded at `0x142c4`) and
`r14 == frame_phase` (byte `0x00530005`, loaded at `0x142f4`); the
recovery previously wrote `registers[15]` from `g0`/`table_last_value`.
See `decomp/i960/notes/player_142c0_v0392.md`.

Status (v0393): the fa_rob fighter-exchange body `0x1442c` (called at
`0x14388` when `+0x04(g7) == 0`) is native for the measured live fast
path, together with its two `0x14640` no-op helper calls. Restoring
`out/park-1442c.vf2snap` (driven from `pre14288`), the reference reaches
`0x1463c` in 51 steps / +2 calls / +2 rets; the native wrapper matches
full live state byte-exact and clears both fighters' `+0x198`. This is
the first native block of the collision/state-exchange function that
follows the recovered player corridor. The heavy collision arms
(`0x144b0`/`0x14518`/`0x14570`), non-no-op `0x14640` branches, and the
`+0x04(g7) != 0` shape remain fail-closed (see
`decomp/i960/notes/fa_player_1442c_live_v0393.md`).

Status (v0394): the fa_rob `0x14640` collision/state helper now has a
second measured sibling native: `+0x194 != 0` (`mov 0,r15;
st r15,+0x654(g7)`, CC = LESS, 14 steps / +1 return), alongside the v0393
no-op (`+0x194 == 0`, CC = EQUAL, 12 steps). The shared gates
(`+0x198 == 0`, `+0x654 == 0`, `+0x197` not 27/28, `(g7)` bit 4 clear)
dispatch to both measured exits. `vf2_player_1442c_live_differential`
now runs both ROM-backed cases byte-exact (51 / +2 / +2 and
14 / +0 / +1). The other `0x14640` gates remain fail-closed (see
`decomp/i960/notes/fa_player_14640_sibling_v0394.md`).

Status (v0395): the fa_rob `0x144b0` state-25 collision arm is native
(entered at `0x144b0` when fighter0 `+0x197 == 25`). On the measured live
shape (fighter0 `+0x197 == 25`, fighter1 `+0x197 == 0`, fighter0
`+0x194 == 0`) it runs the `0x19ef8` g0==0 zero-path, the
collision/state-exchange body, and the `0x14628` common exit; span 53
instructions to `0x1463c` with +1 call / +1 return. `vf2_player_1442c_live`
now runs all three ROM-backed cases byte-exact (fast path 51 / +2 / +2,
sibling 14 / +0 / +1, state-25 53 / +1 / +1). The `0x14640` state-25 helper
path (needed for full-`0x1442c` integration) and the other `0x1442c` heavy
arms (`0x14510`/`0x1453c`/`0x14570`) remain fail-closed (see
`decomp/i960/notes/fa_player_144b0_state25_v0395.md`).

Status (v0397): the `0x144b0` cmpobl-not-taken sibling is native. When
`0x1450c cmpobl r13, r3` does not take (`r13 > r3` unsigned, measured with
fighter1 `+0x808 == 1` / `+0x1aa == 100`), the arm stores `r5` to
`+0x194(f1)` at `0x14510`, branches to the `0x14628` common exit and clears
both `+0x198` (47 steps / +1 / +1, CC = GREATER, `+0x654`/`+0x62a` and the
state chain skipped). The slice also corrects two latent modeling errors
that cancel on the all-zero v0395 pin: ROM `ldos` zero-extends and
`subi r13, r14, r4` computes `+0x808(f1) - +0x858(f0)`. The cmpobl-equal
point is now native too (v0401: `r13 == r3 == 0` via fighter1 `+0x1aa == 0`,
same 47-step fall-through with CC = EQUAL).
The other `0x1442c` heavy arms (`0x1453c`/`0x14570`)
remain fail-closed; the `0x14474` arm is now native for all three
measured entries:
direct (fighter0 `+0x197 == 24`, fighter1 `+0x19f` in {25, 22},
54/55 steps) and swapped (fighter1 `+0x197 == 24`, fighter0 `+0x19f`
in {25, 22}, 57/58 steps via the `0x1446c` swap). Both leave CC =
EQUAL, store `0x01000000` to `+0x194` of the `g8` fighter and clear
bit 0 of its `+0x1a4` (the `0x14640` call on the `+0x197 == 24` side
takes the `+0x194 != 0` sibling since `+0x197` is its high byte).
The both-`== 24` shape is now native too on the f0-priority direct
path (56/57 steps, prefix both siblings 15+15).
Every other `+0x19f` value on all three `0x14474` entries is now
native as well (v0402): the `0x14498` escape restores `g7/g8` and runs
the neutral `0x144a0 -> 0x14528 -> 0x14548 -> 0x14560 -> 0x14628` exit
with no `+0x194`/`+0x1a4` stores (57 direct / 60 swapped / 59 both-24
steps; last compare GREATER for neutral f1, LESS for `r8 == 24`).
Non-neutral downstream states stay fail-closed
(see `decomp/i960/notes/fa_player_144b0_sibling_v0397.md`,
`decomp/i960/notes/fa_player_144b0_equal_v0401.md`,
`decomp/i960/notes/fa_player_14474_arm_v0398.md`,
`decomp/i960/notes/fa_player_14474_swapped_v0399.md`,
`decomp/i960/notes/fa_player_14474_both24_v0400.md` and
`decomp/i960/notes/fa_player_14498_escape_v0402.md`).
Status (v0403, measurement only): the `0x1453c` arm head
(`+0x197(g7) = 16`, 3 steps to `0x14570`) is scoped but not recovered:
the tail faults in `0x1ab34` at `0x1ab4c` for the parked `+0x194` low
half 0 and needs a type-5 record index not yet measured; the f1 == 27
swap and `0x14548..0x1456c` tail joins stay fail-closed (see
`decomp/i960/notes/fa_player_1453c_tail_blocked_v0403.md`).
Status (v0404): the `0x1453c` state-27 arm is now recovered as the
standalone `hybrid_execute_player_1453c` (type-5 walk via `+0x194(g7)`,
52 steps / +1 call / +1 return to `0x1463c`).  The v0403 blocker is
resolved by mining the `0x0200d34c` type-5 chains for a valid index
(`0x73`).  The walker is modelled through `vf2_hybrid_coli_1ab34_execute`,
so frame linkage and call/return counters match the reference exactly; a
type-5 walk miss and the unmeasured scaling/text branches stay fail-closed.
Pinned by `vf2_player_1453c_live_differential`.  The arm is validated at
the `0x14528` synthetic entry and is not yet wired into the `0x1442c`
body dispatch (see reachability note); the separate `0x14640` state-27
type-15 walk and the f1 == 27 swap path remain explicit boundaries (see
`decomp/i960/notes/fa_player_1453c_state27_v0404.md`).
Status (v0415): the measured f1 == 27 swap path at `0x14530..0x14538` is
now native too. From the `0x14528` head with `r7 != 27`, `r8 == 27`, and a
valid type-5 index in the second fighter's `+0x194`, the recovery performs
the three-register swap and matches the ROM through `0x1463c` in 56
instructions / +1 call / +1 return. The expanded
`vf2_player_1453c_live` fixture proves both direct (52-step) and swapped
shapes with full live-state equality. Other `0x1453c` scaling/text/walker
variants and the broader `0x1442c` heavy-arm dispatch remain explicit
boundaries. See
`decomp/i960/notes/fa_player_1453c_swap_v0415.md`.
Status (v0416): the measured `0x14570` state-16 joins are native as well:
`(r7,r8)=(16,0)` reaches `0x1463c` in 52 instructions, `(0,16)` in 55, and
`(16,16)` takes the measured `0x500028` bit-0 gate for 54 instructions
without the swap or 58 with the swap. The six-case
`vf2_player_1453c_live` fixture proves these joins together with the two
state-27 cases, including full live-state equality, and
`vf2_player_1442c_state16_live` proves both-state-16 integration through the
real body (100/104 instructions). Other state combinations, scaling/text/walker
variants and the remaining `0x1442c` heavy-arm dispatch remain explicit
boundaries. See
`decomp/i960/notes/fa_player_1453c_state16_v0416.md`.
Status (v0417): the measured direct state-16 first-scaling arm is native when
`+0x1a4(g8)` bit 0 is set. It scales the type-5 record byte, selects the
`0x1b979` table and reaches `0x1463c` in 55 instructions. The expanded
`vf2_player_1453c_live` fixture proves this shape with full live-state
equality; scaled swapped/state-27 variants and the later scaling/text arms
remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_scaling_v0417.md`.
Status (v0418): the direct state-16 `0x145c0` second gate is native for the
measured `+0x3351` bit-6-set and `g8` bit-29-clear composition. It reaches
`0x1463c` in 54 instructions, two more than the short tail, and preserves the
same record-byte result. The eight-case `vf2_player_1453c_live` fixture proves
this alongside the prior state-27/state-16 shapes; other second-gate and
later scaling/text compositions remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_second_gate_v0418.md`.
Status (v0419): the direct state-16 later scaling arm is native for the
measured `+0x3351` bit-6-set and `g8` bit-29-set composition. The ROM takes
the `0x145cc..0x145d4` `shro`/`addo`/table-select sequence and reaches
`0x1463c` in 57 instructions. The nine-case `vf2_player_1453c_live` fixture
proves this with full live-state equality; swapped/state-27 scaling and the
text branch remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_later_scaling_v0419.md`.
Status (v0420): the direct state-16 board-bit-9-clear text tail is native for
the measured short composition. The ROM calls `0x7fc0` with source
`0x1b970` and destination `0x010006e8`, writes eight glyph shorts, and reaches
`0x1463c` in 126 instructions with two calls/returns. The ten-case
`vf2_player_1453c_live` fixture proves this with full live-state equality;
scaled/swapped text variants remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_text_v0420.md`.
Status (v0421): the first-scaling bit-0 arm is native across the measured
state-27/state-16 matrix: direct/swapped state 27 takes 55/59 instructions,
swapped state 16 takes 58, and both-state-16 direct/swapped takes 57/61.
The fifteen-case `vf2_player_1453c_live` fixture proves these with full
live-state equality and one call/return each. Other scaling compositions and
scaled text variants remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_scaling_matrix_v0421.md`.
Status (v0422): the `0x145c0` bit-6 second gate is native across the measured
state-27/state-16 matrix when g8 bit 29 is clear: direct/swapped state 27
takes 54/58 instructions, swapped state 16 takes 57, and both-state-16
direct/swapped takes 56/60. The twenty-case `vf2_player_1453c_live` fixture
proves these with full live-state equality and one call/return each;
bit-29-set and mixed scaling combinations remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_second_gate_matrix_v0422.md`.
Status (v0423): the bit-29 later-scaling arm is native across the measured
state-27/state-16 matrix with `+0x3351` bit 6 set: direct/swapped state 27
takes 57/61 instructions, swapped state 16 takes 60, and both-state-16
direct/swapped takes 59/63. The 25-case `vf2_player_1453c_live` fixture
proves these with full live-state equality and one call/return each; mixed
first/later scaling and scaled text remain explicit boundaries. See
`decomp/i960/notes/fa_player_1453c_later_scaling_matrix_v0423.md`.
Status (v0424): the mixed first/later scaling matrix is native. With first
scaling enabled and the second gate taken, direct/swapped state 27 reaches
57/60 (bit 29 clear) and 60/64 (bit 29 set), swapped state 16 reaches 60/63,
and both-state-16 direct/swapped reaches 59/62 and 63/66. The 35-case
`vf2_player_1453c_live` fixture proves these with full live-state equality;
mixed scaled text remains an explicit boundary. See
`decomp/i960/notes/fa_player_1453c_mixed_scaling_v0424.md`.
Status (v0429): the board-bit-9-clear text tail now covers the measured
unscaled state-27/state-16 direct, swapped and both-state-16 joins, plus
direct state-16 first-scaling, second-gate and later-scaling variants. The
expanded fixture proves 126/130/129/128/132/129/128/131-step shapes with two
calls/returns and full live-state equality. Other text compositions remained
explicit boundaries at that earlier checkpoint. See
decomp/i960/notes/fa_player_1453c_text_matrix_v0429.md.
Status (v0430): the board-bit-9-clear text tail now covers all 35 accepted
0x1453c/0x14570 state/scaling shapes, including swapped, both-state-16 and
mixed first/later-scaling paths. Every text variant adds 74 instructions and
one call/return to its corresponding short path; the expanded fixture proves
full live-state equality. Other 0x1453c walker misses and unrecognized state
shapes remain explicit boundaries.
Status (v0433): the `0x144b0` state-25 arm now admits the measured
state-16 successor. When the `0x1450c cmpobl` takes (`r13 < r3`), the
`0x14560` fall-through swaps into the recovered `0x14570` type-5 body;
the direct arm matches 99 instructions with two calls/returns, and the
integrated `0x1442c` path matches 144 instructions with four calls/returns.
The focused live fixture proves exact CPU/machine equality for both forms.
Other state-25 successors and unmeasured `0x144b0` compositions remain
explicit boundaries (see
`decomp/i960/notes/fa_player_144b0_state16_v0433.md`).
Status (v0434): the same state-25 arm now admits the measured state-24
successor for the neutral `0x19f` miss. The `0x14474`/`0x14498` prefix
reaches `0x144b0`, and the direct/integrated fixtures prove 53/105
instructions with one/three calls and returns plus exact live-state equality.
Other state-24 `0x14474` compositions remain explicit boundaries (see
`decomp/i960/notes/fa_player_144b0_state24_v0434.md`).
Status (v0435): the integrated state-25/state-24 arm also admits the measured
`0x14474` writes for fighter0 `+0x19f == 25` and `22`. The two witnesses prove
59/60 instructions with two calls/returns and exact live-state equality;
other state-24 compositions remain explicit boundaries (see
`decomp/i960/notes/fa_player_1442c_state25_state24_1474_v0435.md`).
Status (v0436): the integrated state-25/state-27 setup is native through the
measured state-27 helper, which clears fighter1 `+0x194` before the state byte
is re-read. The neutral continuation now proves 126 instructions with four
calls/returns and exact live-state equality. Direct state-27 entry at
`0x144b0` is now also native for the measured direct state-25/state-27
successor: 100 instructions with two calls/returns and exact live-state
equality. Other direct state-25 compositions remain explicit boundaries (see
`decomp/i960/notes/fa_player_144b0_state27_v0440.md`).
Status (v0405): the `0x14640` state-27 arm is now native as the standalone
`hybrid_execute_player_14640_state27` (type-15 walk via `+0x194(g7)`,
41 steps / +1 call / +1 return to the `0x146c4` ret).  It requires
`+0x198 == 0`, `+0x654 == 0` and `+0x197 == 27` on fighter g7, stores
`r4 = s16(+1(rec)) - 1` into `+0x62a(g7)`, moves the original full
`+0x194(g7)` u32 into `+0x654(g7)` and clears `+0x194(g7)`; bit 20 of
`0x500068` must be clear and the type-15 walk must hit.  The arm is
dispatched from `hybrid_execute_player_14640` when `+0x197 == 27` and is
then wired through the `0x146c4` ret, so the full `0x1442c` f0 == 27 flow
is natively reachable.  Final reference `compare_result` is NONE.  Pinned
by `vf2_player_14640_state27_live_differential`.  The state-28
(`0x1469c`) sibling remains an explicit boundary (see
`decomp/i960/notes/fa_player_14640_state27_v0405.md`).
Status (v0406): the `0x14640` state-28 arm is now native as the standalone
`hybrid_execute_player_14640_state28` (no walker, 13 steps / +0 call / +0
return to the `0x146c4` ret).  It requires `+0x198 == 0`, `+0x654 == 0`
and `+0x197 == 28` on fighter g7, adds 3 to `s16(+0x1aa(g7))` and stores
the u16 back, clears `+0x194(g7)`, leaves `r15 = 0` and `r3` the new
`+0x1aa` value.  The arm is dispatched from `hybrid_execute_player_14640`
when `+0x197 == 28` and is then wired through the `0x146c4` ret, so both
the state-27 and state-28 f0 flows are natively reachable.  Final
reference `compare_result` is EQUAL.  Pinned by
`vf2_player_14640_state28_live_differential`.  The `+0x654 != 0`
`+0x1aa`/`+0x62a` compare arm and the `+0x197` not 27/28 neutral tail
(`0x146b0`/`0x146c8`/`0x146dc`) remain explicit boundaries (see
`decomp/i960/notes/fa_player_14640_state28_v0406.md`).
Status (v0407): the `0x14640` bit-4-set neutral arm is now native as the
standalone `hybrid_execute_player_14640_bit4set` (no walker, 12 steps /
+0 call / +0 return to the `0x146c4` ret).  It requires `+0x198 == 0`,
`+0x654 == 0`, `+0x197` not 27/28/13 and bit 4 of `(g7)` SET; it clears
`+0x194(g7)` and leaves `r15 = 0`, `r3 = +0x197`.  The arm is dispatched
from `hybrid_execute_player_14640` when bit 4 of `(g7)` is set and
`+0x197` not 27/28/13, and is then wired through the `0x146c4` ret.  Final
reference `compare_result` is GREATER.  Pinned by
`vf2_player_14640_bit4set_live_differential`.  The `r3 == 13` sibling
(which takes the `0x146c8` tail), the `+0x654 != 0` `+0x1aa`/`+0x62a`
compare arm and the `r198 != 0` escape remain explicit boundaries (see
`decomp/i960/notes/fa_player_14640_bit4set_v0407.md`).
Status (v0408): the `0x14640` escape arm is now native as the standalone
`hybrid_execute_player_14640_escape` (no walker, 5 steps / +0 call / +0
return to the `0x146e8` ret).  It requires `+0x198 != 0` on fighter g7
(the `0x14644 cmpobne 0, r3` jumps directly to `0x146dc`), stores r3
(= the `+0x198` value) to `+0x194(g7)`, clears `+0x654(g7)` and leaves
`r15 = 0`, `r3 = +0x198`.  The arm is dispatched from
`hybrid_execute_player_14640` when `+0x198 != 0` and is then wired through
the `0x146e8` ret.  Final reference `compare_result` is LESS.  Pinned by
`vf2_player_14640_escape_live_differential`.  The `+0x654 != 0`
`+0x1aa`/`+0x62a` compare arm (which can also jump to this same
`0x146dc` escape) remains an explicit boundary (see
`decomp/i960/notes/fa_player_14640_escape_v0408.md`).
Status (v0409): the `0x14640` compare-prefix arm is now native as the
standalone `hybrid_execute_player_14640_compare` (no walker, 15 steps /
+0 call / +0 return to the `0x146c4` ret).  It requires `+0x198 == 0`,
`+0x654 != 0` (the `0x1464c cmpobe 0, r3` not taken) and
`s16(+0x1aa) > s16(+0x62a)` (the `0x14658 cmpobe r13, r14` not taken) on
fighter g7; on the measured `+0x197` not 27/28/13, bit-4-set shape it
clears `+0x194(g7)` and leaves `r15 = 0`, `r3 = +0x197`,
`r13 = s16(+0x1aa)`, `r14 = s16(+0x62a)`.  The arm is dispatched from
`hybrid_execute_player_14640` when `+0x654 != 0` and is then wired through
the `0x146c4` ret.  Final reference `compare_result` is GREATER.  Pinned
by `vf2_player_14640_compare_live_differential`.  The
`s16(+0x1aa) <= s16(+0x62a)` escape jump and the compare-prefix
state-27/state-28/bit-4-clear siblings remain explicit boundaries (see
`decomp/i960/notes/fa_player_14640_compare_v0409.md`).
Status (v0410): the `0x14640` compare-prefix escape arm is now native as
the standalone `hybrid_execute_player_14640_compare_escape` (no walker,
10 steps / +0 call / +0 return to the `0x146e8` ret).  It requires
`+0x198 == 0`, `+0x654 != 0` (the `0x1464c cmpobe 0, r3` not taken) and
`s16(+0x1aa) == s16(+0x62a)` (the `0x14658 cmpobe r13, r14` taken to
`0x146dc`) on fighter g7; it stores r3 (= the `+0x654` value, distinct
from the v0408 `+0x198` escape) to `+0x194(g7)` and clears `+0x654(g7)`,
leaving `r15 = 0`, `r3 = +0x654`, `r13 = s16(+0x1aa)`,
`r14 = s16(+0x62a)`.  The arm is dispatched from `hybrid_execute_player_14640`
when `+0x654 != 0` and `s16(+0x1aa) == s16(+0x62a)` and is then wired
through the `0x146e8` ret.  Final reference `compare_result` is EQUAL.
Pinned by `vf2_player_14640_compare_escape_live_differential`.  The
`s16(+0x1aa) < s16(+0x62a)` fall-through and the compare-prefix
state-27/state-28/bit-4-clear siblings remain explicit boundaries (see
`decomp/i960/notes/fa_player_14640_compare_escape_v0410.md`).
Status (v0425): the `0x14640` state-27 compare-prefix sibling is native for
the measured signed-less shape. With `+0x654 != 0`, `+0x1aa < +0x62a`,
`+0x197 == 27`, a valid type-15 walk and board bit-20 clear, it reaches
`0x146c4` in 44 instructions with one call/return; the original `+0x654 == 0`
shape remains 41 instructions. The expanded state-27 fixture proves both
with full live-state equality and generic dispatch reaches the new arm.
Other compare-prefix state-28/bit-4-clear and non-less relations remain
explicit boundaries. See
`decomp/i960/notes/fa_player_14640_state27_compare_less_v0425.md`.
Status (v0426): the `0x14640` state-28 compare-prefix sibling is native for
the measured signed-less shape. With `+0x654 != 0`, `+0x1aa < +0x62a` and
`+0x197 == 28`, it reaches `0x146c4` in 16 instructions; the original
`+0x654 == 0` shape remains 13 instructions. The expanded state-28 fixture
proves both with full live-state equality and generic dispatch reaches the
new arm. Other compare-prefix relations remain explicit boundaries. See
`decomp/i960/notes/fa_player_14640_state28_compare_less_v0426.md`.
Status (v0427): the neutral bit-4-clear less-than tail now admits the
measured nonzero `+0x194` sibling. With `+0x654 != 0` and signed
`+0x1aa < +0x62a`, it reaches `0x146d8` in 16 instructions and clears
`+0x654`; the zero `+0x194` shape remains 14 instructions. The neutral
bit-4-set sibling returns at `0x146c4` in 15 instructions. The compare-less
fixture now proves neutral zero, state 13, neutral nonzero and neutral
bit-4-set cases with full live-state equality. Other less-than compositions
remain explicit boundaries.
See `decomp/i960/notes/fa_player_14640_compare_less_nonzero_v0427.md`.

Status (v0428): the measured compare-prefix arms now accept both signed
directions when `+0x1aa` and `+0x62a` are unequal. State 27 and state 28 retain
their 44/16-step compare-prefix tails, while the neutral/state-13 fixture
covers the existing 14/16/15/17-step shapes for the greater witnesses as
well. Equality remains routed to the dedicated escape arm; bit-20 board
shifts, other state compositions and unmeasured siblings remain explicit
boundaries. See the three live fixtures and the v0428 evidence note.

Status (v0431): the compare-prefix equality tail is now dispatched for state
27 and state 28 as well. Both measured shapes take the shared 10-instruction
`0x146dc` tail and return through the helper frame in 11 total instructions /
+1 return; `vf2_player_14640_compare_escape_live` proves the generic dispatch
and full live-state equality. Unmeasured bit-20 shifts and other state
compositions remain explicit boundaries.

Status (v0432): the state-27 type-15 arm now admits the measured board-bit-20
shift at `0x14684`. The compare-prefix witness reaches `0x146c4` in 45
instructions / +1 call / +1 return, one instruction beyond the clear-bit-20
shape; `vf2_player_14640_state27_live` proves the full live-state result.
Other board shifts and state compositions remain explicit boundaries.
Status (v0437): the state-13 neutral tail now admits the measured bit-4-clear
shape alongside the existing bit-4-set path. Both reach `0x146d8` with exact
live-state equality: 13 instructions for bit 4 clear and 14 for bit 4 set,
with no calls or returns. Other state-byte and flag compositions remain
explicit boundaries (see
`decomp/i960/notes/fa_player_14640_state13_bit4_clear_v0437.md`).

Status (v0438): the `0x14528` state-24/state-16 swapped join now admits the
measured `r7 == 24`, `r8 == 16` shape. It takes the existing `0x14564..0x1456c`
swap into the shared type-5 body and matches the ROM in 55 instructions with
one call/return and exact live-state equality; the board-bit-9-clear text
variant matches at 129 instructions with two calls/returns. Other unmeasured
state/scaling combinations and walker misses remain explicit boundaries (see
`decomp/i960/notes/fa_player_1453c_state24_v0438.md`).

Status (v0439): the same `0x14528` fall-through now admits the measured
state-28/state-16 swapped shape (`r7 == 28`, `r8 == 16`). The shared body
matches the ROM in 55 instructions with one call/return and in 129
instructions with the board-bit-9-clear text tail and two calls/returns.
Other unmeasured state/scaling combinations and walker misses remain
explicit boundaries (see
`decomp/i960/notes/fa_player_1453c_state28_v0439.md`).

Status (v0441): a bounded reference sweep of `r7=0..31` with `r8 == 16`
measures the same 55-instruction swapped body for every value except the
dedicated `r7 == 16` and `r7 == 27` arms. Native C now admits that measured
state-byte family; the live fixture proves state 26 and its text-tail sibling
at 55/129 instructions with one/two calls and returns. Values above 31,
other state/scaling compositions and walker misses remain fail-closed (see
`decomp/i960/notes/fa_player_1453c_state16_family_v0441.md`).

Status (v0442): the same bounded dispatch sweep with `r8 == 0` measures a
9-instruction neutral exit for every `r7=0..31` value except the dedicated
state-16 and state-27 arms. Native C now admits that measured neutral family;
the live fixture proves state 26 with zero calls/returns and exact live-state
equality. Values above 31 and other `r8` combinations remain explicit
fail-closed boundaries (see
`decomp/i960/notes/fa_player_1453c_neutral_family_v0442.md`).

Status (v0443): a bounded reference sweep with `r7 == 16` and `r8=0..31`
measures the same 52-instruction direct type-5 body for every value except
the dedicated `r8 == 16` and `r8 == 27` arms. Native C now admits that
measured direct family; the live fixture proves `r8 == 26` and its text-tail
variant with exact live-state equality. Values above 31 and other state-byte
combinations remain explicit fail-closed boundaries (see
`decomp/i960/notes/fa_player_1453c_state16_direct_family_v0443.md`).

Status (v0444): a second full bounded row with `r8 == 1` confirms the same
9-instruction neutral exit for every `r7=0..31` value except `r7 == 16` and
`r7 == 27`. Native C now admits the measured bounded cross-product where both
state bytes are in `0..31` and neither is 16/27; the fixture proves the
state-26/state-1 witness with exact live-state equality. Values above 31 and
dedicated 16/27 combinations remain explicit boundaries (see
`decomp/i960/notes/fa_player_1453c_neutral_cross_product_v0444.md`).

Status (v0445): the integrated `0x1442c` row with fighter 0 state 16 and
fighter 1 state 26 now reaches the direct state-16 type-5 body in 98
instructions with three calls/returns and exact live-state equality. Native C
admits the measured bounded ordinary subset (`f1=0..31` excluding 16, 24, 25
and 27); those special successors and unmeasured state-16 rows remain
explicit boundaries (see
`decomp/i960/notes/fa_player_1442c_state16_ordinary_v0445.md`).

Status (v0446): the integrated state-16/state-27 row is now covered by the
existing post-helper both-state-16 join. The live fixture proves 126
instructions with four calls/returns and exact live-state equality; the
state-24 special continuation remains an explicit boundary (see
`decomp/i960/notes/fa_player_1442c_state16_state27_v0446.md`).

Status (v0447): the integrated state-16/state-24 neutral `+0x19f` miss now
reaches the direct state-16 type-5 body in 105 instructions with three
calls/returns and exact live-state equality. The state-24 `+0x19f` 25/22
write arms and other state-16 compositions remain explicit boundaries (see
`decomp/i960/notes/fa_player_1442c_state16_state24_v0447.md`).

Status (v0448): the integrated state-16/state-24 `+0x19f` write arms now
admit the measured values 25 and 22, matching 59/60 instructions with two
calls/returns and exact live-state equality. Other state-24 flag, scaling and
write compositions remain explicit fail-closed boundaries (see
`decomp/i960/notes/fa_player_1442c_state16_state24_writes_v0448.md`).

Status (v0449): the integrated state-16/state-25 row now admits the measured
zero-selector composition: fighter 0 `+0x194` low halfword `0x6f`, `0x73` or
`0x74`, fighter 1 `+0x194` low halfword zero. The swapped `0x19ef8` zero path
and direct state-16 type-5 tail match in 144 instructions with four
calls/returns and exact live-state equality. Nonzero selectors, other type
indices and sibling compositions remain explicit fail-closed boundaries (see
`decomp/i960/notes/fa_player_1442c_state16_state25_v0449.md`).

Status (v0450): the same integrated row now admits the measured type-8
terminator compositions at fighter 0 low-halfword indices `0x110` and
`0x2cf`. The `0x1ab34` walker returns `g0 == 0`, and the recovered tail
matches in 161/147 instructions with four calls/returns and exact live-state
equality. Other walker misses remain explicit fail-closed boundaries (see
`decomp/i960/notes/fa_player_1442c_state16_state25_walker_miss_v0450.md`).

Status (v0451): the measured `fa_coli` `0x225cc` bit-16 scan-4 witness is now
native through the shared `0x22e24` join. With `g8 + 0x1a4 == 0x00010000`,
`g7 + 0x821 == 4`, and the measured bit-22-clear `g7 + 0x1a4` value, the
`0x22c88` bbs-16 edge matches 217 instructions and exact live state. Other
bit-16 compositions remain explicit fail-closed boundaries (see
`decomp/i960/notes/fa_coli_225cc_bit16_scan4_v0451.md`).

Status (v0452): the measured bit-22-set sibling is now native for the exact
`g7 + 0x1a4 == 0x00400100` flag word. The scan-4/bit-16 path reaches the
g0=5 `0x22d8c` tail and matches 108 instructions with exact live state;
other g7 flag compositions remain explicit fail-closed boundaries (see
`decomp/i960/notes/fa_coli_225cc_bit16_bit22_v0452.md`).

Status (v0453): the same scan-4/bit-16 path now admits the measured g7 flag
family `(flags & bit22) != 0 && (flags & bit4) == 0`. The added witnesses
`0x00400000`, `0x00400001`, `0x00410100` and `0x00c00100` all match the
108-instruction v0452 continuation and full live state; bit-4-set and other
unmeasured compositions remain explicit boundaries (see
`decomp/i960/notes/fa_coli_225cc_bit16_bit22_family_v0453.md`).

Status (v0298): `0x29414` types 6/8/10 are now native for both the
bit-19-clear float tail and the measured bit-19-set siblings. The
`+0x1aa` window uses unsigned compares (`r12 > 20` → path B at
`0x294f8`, `r12 <= 10` → float tail, else path A at `0x294ac..0x294f4`
with indexed `(g11)[g12]` stores and optional halfword adds). Path B
early-returns without storing `+0xc50` on board bit 5, a missing
`+0x614 & 0x9000` mask, or `50 < window`. The unreachable-from-this-entry
constant set at `0x29454` remains unmeasured. Type 0 zero-path is
unchanged. `0x19ef8` flag-bit siblings stay deferred because pure
interpretive replay from the parked snapshot still faults at `0x2704c`;
the hybrid `0x14288` mid-corridor continuation and `vf2probe --set-ip`
enable the next live-context drive (see
`decomp/i960/notes/fa_player_29414_bit19_v0297.md`,
`decomp/i960/notes/fa_player_29414_types_v0296.md`,
`decomp/i960/notes/fa_player_19ef8_29414_defer_v0295.md` and
`decomp/i960/notes/fa_player_drive_base_v0293.md`).

Status (v0298): the `0x180bc` player flag tail and the `0x1441c`
epilogue are native. On the measured warm shape the first-dispatch
player task therefore finishes without `hybrid_execute_interpreted_task`
from `0x180bc`. `hybrid_execute_interpreted_until` already routes
`0x28178`/`0x17710`/`0x1791c`/`0x4b640` through
`vf2_hybrid_i960_run_tail` semantic layers (see
`decomp/i960/notes/fa_player_180bc_v0298.md`). Post-`0x28780`
geometry through `0x2826c`/`0x27d90` remains native; physics/hitboxes
and an input-driven pin distinct from PUNCH remain later dedicated
work.

Status (v0301): the input-17 endurance pin covers **two** extra cycles.
Selector-17 phase `0x8a` / `bit7_index10` admits the measured match-latch
sibling `input=previous=0x0f002100` for state0 (1677/33, `r14=6`,
GREATER, header `LOSE(%)`) and state1 navigation==0 (36756/1438,
`r14=7`, EQUAL).
`vf2cycles --input 17 --cycles 2` from `in17-c1` is **2/2 MATCH**
(74 blocks / 42,696 insns, both `0x1645c`), complementary to PUNCH.
A third cycle fails closed on a later frame (`r14` 8 vs 7). The
park-only latch shape and match-latch state1 exit remain fail-closed
(see `decomp/i960/notes/fa_player_input17_index10_v0299.md`).

Status (v0301c): the cycle-3 work-ram residual is closed. Match-latch
index10 state1 stores measured `r20` (`0x00560000`) at `0x005ff600`.
`vf2cycles --input 17 --cycles 64` from `in17-c1` is **64/64 MATCH**
(2368 blocks / 2,428,988 insns, both `0x1645c`). Park-only latch and
navigation!=0 siblings remain fail-closed.

Status (v0302): campaign Fase 7 taint/layout items are closed.
`tools/python/taint.py` on the v0297 `0x29414` traces reports
`branch 0x0002949c depends on fighter0 + 0x01a4 bit 19`. Window
compares at `0x294a4`/`0x294a8` do not inherit `+0x1aa` taint in the
current heuristic (halfword load); that dependency remains memory- and
recovery-backed. `include/vf2/fighter_candidate.h` now also carries
coli mid-body bilateral offsets and the `0x29414` g7 corridor offsets
with neutral `field_XXXX` names only. Physics/hitbox/damage and
`0x19ef8` flag-bit siblings remain unrecovered (see
`decomp/i960/notes/taint_29414_v0302.md` and
`decomp/i960/notes/fighter_candidate_layout_v0302.md`).

Status (v0303): `0x22404` bit-8-set with non-empty mask after
`andnot` (`g0 = 1`, body 72 on the measured one-hit scan) is native,
including the polygon FIFO via `(g11)[g12]`. Slot-1 scan, the
`g8+0x26 != 0` FIFO cursor branch, and the `0x225cc` resolver remain
fail-closed; the coli mid-body tail still requires both contact
results zero (see `decomp/i960/notes/fa_coli_contact_g0_v0303.md`).

Status (v0304): `0x225cc` compact bit-3 sibling is native (12 insns,
counter++/-- net zero). Mid-body tail admits first-contact `g0=1`
with a warm second contact when `g8+0x1a4` bit 3 selects the compact
exit (measured 132/5/6). The 248-insn `0x225cc` body, the `0x18bd4`
shortcut, and second-contact `g0!=0` remain fail-closed (see
`decomp/i960/notes/fa_coli_225cc_bit3_v0304.md`).

Status (v0305): mid-body tail also admits the reverse sibling — first
contact warm, second contact `g0=1` — via the no-restore jump to
compact `0x225cc` (measured 130/5/6, final `g7=fighter1`,
`g8=fighter0`). Both-non-zero cascade at `0x22244`, slot-1 scan, and
the long `0x225cc` body remain fail-closed (see
`decomp/i960/notes/fa_coli_second_contact_v0305.md`).

Status (v0306): `0x22404` slot-1 15-trip scan is native (body 133).
Both contacts can hit when they use different slots; the cascade
early-out at `0x22258` (fighter1 `+0x804` bit 15) is native
(measured 255/5/6). The `0x2227c` tie-break (long `0x225cc` twice),
other cascade arms, and the long `0x225cc` body remain fail-closed
(see `decomp/i960/notes/fa_coli_cascade_v0306.md`).

Status (v0307): cascade pair-greater `cmpobg` (`f1+0x822 > f0+0x822`)
is native (258/5/6). `bl`/`bg` after non-taken `cmpob*` do not
inherit the compare in the reference executor; those arms and the
tie-break remain fail-closed (see
`decomp/i960/notes/fa_coli_cascade_pair_v0307.md`).

Status (v0308): `g8+0x26 != 0` FIFO cursor in `0x22404` is native
(one-hit slot-0 body 79). Long `0x225cc`, `0x18bd4`, and the
`0x2227c` tie-break remain fail-closed (see
`decomp/i960/notes/fa_coli_fifo_cursor_v0308.md`).

Status (v0309): live hybrid `0x19ef8` bit-5 from `punch10` still
fails closed at `0x16464` with zero compared blocks; reference park
replay cannot leave the `0x10fa0` wait. `frontier.py` now ranks call
edges with function attribution (see
`decomp/i960/notes/fa_player_19ef8_live_v0309.md`).

Status (v0310): coli helper `0x23238` early-out (`g0 != 0x2ce`) is
native (body 2). Float-threshold path and the long `0x225cc` parent
remain fail-closed (see `decomp/i960/notes/fa_coli_23238_v0310.md`).

Status (v0311): coli helper `0x230d4` bit-26 compact path is native
(body 15). Bit-26-clear long path remains fail-closed (see
`decomp/i960/notes/fa_coli_230d4_v0311.md`).

Status (v0312): coli table-walk helper `0x1ab34` is native (miss body
16, match body 6). Long `0x225cc` parent remains fail-closed (see
`decomp/i960/notes/fa_coli_1ab34_v0312.md`).

Status (v0313): coli helper `0x23238` float-threshold path is native
(bodies 2/6/10/11). Long `0x225cc` parent remains fail-closed (see
`decomp/i960/notes/fa_coli_23238_float_v0313.md`).

Status (v0314): coli long body `0x225cc` is native on the v0288 drive
(249/4/5 including completed ret). Integrates `0x230d4` long,
`0x23238` ×2, `0x1ab34` miss, and the float tail. Compact bit-3
sibling unchanged. `0x2227c` tie-break, `0x18bd4` shortcut, and
unmeasured flag siblings remain fail-closed (see
`decomp/i960/notes/fa_coli_225cc_long_v0314.md`).

Status (v0381): the live-midbody shape (`g8+0x1a4 = 0`,
`g7+0x1a4 = 0x100`, no `0x18bd4` shortcut) is native with full
live-state equality (240 steps, 4 nested calls, final CC + `g1`
pinned; new ROM-backed `vf2_coli_225cc_live` fixture). Three
corrections along the way: relaxed over-narrow `g8+0x6d4 == 0xffff`
gate (half feeds only the mask), replaced the unreachable
`0x22628` scanbit-pack arm (`be` always taken at `r11 == 0`;
single-predecessor CFG + CC semantics in both executors) and admitted
float-tail `r9 == 0` (`0.0/24.0`, bit-exact differential). Synthetic
pins move by exactly −9 with FIFO floats recomputed from the
corrected `r9 = 0.0` chain. See
`decomp/i960/notes/fa_coli_225cc_live_v0381.md`.

Status (v0382): `0x22404` live first-contact stale slot is native
(body 77, `g0 = 1`, final CC pinned; new ROM-backed
`vf2_coli_22404_live` fixture). The stale slot falls through `cmpobe`
to `bal 0x225bc` (+5 pending-clear rejoin); all other gates hold, and
stale+empty plus stale slot 1 stay fail-closed. Required an oracle
fix: the arch wrapper re-decided `bo`/`bno` from stale AC, spinning
any scanbit-miss shape unless AC agreed by luck — now scoped to the
integer-compare domain, scanbit domain left to legacy (no pin moves
anywhere, 66/66). See
`decomp/i960/notes/fa_coli_22404_live_v0382.md`.

Status (v0383): coli mid-body tail whole-tail live (first-hit-second-warm
long) is native (371 steps, 9 calls / 10 rets, final CC + `g1` pinned;
new ROM-backed `vf2_coli_midbody_tail_live` fixture via the measured
snapshot base). The v0304 branch now forwards long-body counts/CC/G1
and correctly reports `4,4`/`5,5`/`9,9` nested (warm/compact/long).
Whole-task `0x221e8 -> 0x10dcc` composition (prefix+shell+tail,
measured 9529/21/22) is the explicit next slice and stays fail-closed
at the task gate (`9214/18/19`, `9528/18/19`, `9393/17/18`). See
`decomp/i960/notes/fa_coli_midbody_tail_live_v0383.md`.

Status (v0384): coli g3-scan `0x238a4` live (single fighter `+0x1a4`
bit8) is native (first `136`/`g3=0x18`/`CC=EQUAL`, second `5`,
mirrored `5`/`134`/`g3=0`/`CC=EQUAL`; new ROM-backed
`vf2_coli_238a4_live` fixture via `coli-parked-221e8`). Flag builder
`0x233d0` now allows `xor bit8` with `0x820==1` check (live `g6=2`
vs warm `0`).  Shell `0x23524` now counts `b238+1` per `0x238a4`.  Whole-task
gate now allows `9385/17/18` (mirrored `f1`) alongside `9393` at the
helper level. See `decomp/i960/notes/fa_coli_238a4_live_v0384.md`.

Status (v0385): coli whole-task live single-fighter (f0 `9393/17/18`,
f1 `9385/17/18`) is native with full live-state equality. Flag
builder `0x233d0` live is `53` steps (`g6=2`, table `0x2330c` for f0
vs `0x23324` for f1, `0xFFFFDFFC` at `g13+0xb4/0xb8`, `g13+0x88=3`);
both-live `g6=4` keeps warm table `0x232c4`/`44`. Shell `0x23524`
live threshold at `0x236b0` (`r3==4294959100`, `g6==2`) takes the
`0x236c4` path (26 vs 17, calls 12 vs 13) and leaves the six cluster
words at `0xd4..0xe8` and fighter `+0x18/+0x20` as `0xFFFFDFFC`
(`4294959100`). Whole-task `9393`/`9385` now pass the task gate with
`EQUAL` and `+1`/`+2` counted compares. New ROM-backed
`vf2_coli_whole_task_live` fixture via `coli-parked-221e8`. See
`decomp/i960/notes/fa_coli_whole_task_live_v0385.md`.

Status (v0386): coli whole-task live both-fighters `9528/18/19`
is native with full live-state equality (f0 `9393`, f1 `9385` remain).
Flag builder `g6=4` (both xor `0` / and `1` → warm `44`);
shell second `0x238a4` is long `134` (`g3=0`, fighter1 byte `0` with
`half 0/0`) vs single `5`, so shell `9418` (`9411` body) vs single
`9307` (`9300`); threshold stays warm `17` (both `r3==0`, `g6==4`)
so cluster `0xd4..0xe8` and fighter `+0x18/+0x20` stay `0`.
Midbody tail `110` (`107` body + `EQUAL` `+2`) vs warm `56` /
single `86`; six halfword stores `fighter+0x6dc`, `g13+0xc/0xe`,
`fighter+0x8d4` and `0x0051498c..0xe` (`e8 21 02`) as measured.
Task gate now allows `9528` alongside `9393`/`9385` with `EQUAL`
and `+2` counted compares. Fixture `vf2_coli_whole_task_live`
extended to three modes via `coli-parked-221e8`. See
`decomp/i960/notes/fa_coli_whole_task_both_live_v0386.md`.

Status (v0315): mid-body `0x2227c` tie-break is native (double
`0x225cc`, compact-both 282/7/8). `0x18bd4` shortcut remains DEFER
(v0291). Unmeasured long-body flag siblings remain fail-closed (see
`decomp/i960/notes/fa_coli_tiebreak_2227c_v0315.md`).

Status (v0316): `0x225cc` type-22 shortcut `0x18bd4` is native
(first-hit type-5 unit shape 53/3/4; integrates `0x1ab34` and
`0x18b58` bit-2-clear). `0x18b58` bit-2-set, `notbit 15`, and type-5
miss remain fail-closed (see
`decomp/i960/notes/fa_coli_18bd4_type22_v0316.md`).

Status (v0317): `0x18b58` bit-2-set FIFO path is native (bodies
2/29/13). Type-22 shortcut with `g7` bit 2 set completes 80/3/4.
`notbit 15` (`g8+0x19c` bit 15) is native (type22 first-hit 54/3/4).
Type-5 miss remains fail-closed (see
`decomp/i960/notes/fa_coli_18b58_fifo_v0317.md` and
`fa_coli_18bd4_notbit15_v0317.md`).

Status (v0318): long-body `g7+0x1a4` bits 4+12 scale sibling is
native (263 on the v0288 drive). `g8+0x1a4` bit 13 early-join is
native when `+0x5b8` bit 0 is set (251). Bit 4 without bit 12 (251)
and bits 4+12 with `bbs 15` taken (256) are native. Deeper bit-13
arms and remaining long-body flag siblings fail closed (see
`decomp/i960/notes/fa_coli_long_b4b12_v0318.md`,
`fa_coli_long_b13_v0318.md` and `fa_coli_long_b4_bbs15_v0318.md`).

Status (v0319): long-body `r11 != 0` packing and the
`0x439ac`/`0x43888` diagnostic cascade arm are native (unit shape
`r11b=1` 302/7/8). `0x439ac` multi-trip and `0x43888` branch-byte
sibling remain fail-closed (see
`decomp/i960/notes/fa_coli_long_r11_v0319.md`).

Status (v0320): diagnostic cascade completed. `0x439ac` multi-trip
scan, `count>=4` early-out and table match are native. `0x43888`
store effects (`33`/`0x421` + ring), gate&12 branch-byte siblings and
the bit-20 subtract plus `0x22e74` `shli` are native (unit shapes
302/291/295/306/285/305/309). Bit-20 non-match and the diagnostic-arm
gates `bit 18` / `+0x823` / `r11>=20` remain fail-closed (see
`decomp/i960/notes/fa_coli_diag_v0320.md`).

Status (v0321): diagnostic-arm gates native. `g7+0x1a4` bit 18 skip
(247), r11=20 r4-offset (305), `g8+0x1b1==9` second pair (354) and
`g7+0x823` table-walk loop (353). `g7+0x820` table select corrected
(`0x230bc` when not 5/6). r11≥40, bit-20 non-match and remaining
long-body flag siblings fail closed (see
`decomp/i960/notes/fa_coli_diag_gates_v0321.md`).

Status (v0322, measurement): `g8+0x1a4` bit 16 with `g7+0x821==0`
exits the long body at `0x230a0` in 15 steps (counter--). Bit 14
(289), bit 4 (301) and `g7+0x828` bit 14 (293) measured. Not yet
recovered — a flags-region insert broke the warm body count (see
`decomp/i960/notes/fa_coli_early_exit_v0322.md`).

Status (v0323): early exit at `0x230a0` is native for `g8+0x1a4`
bits 3/15/16 with the measured `g7+0x821` gate (unit bit16 **16**).
Warm `+2 +4` accounting preserved. Bit 14/4 at `0x22c84` and
`g7+0x828` bit 14 remain fail-closed (see
`decomp/i960/notes/fa_coli_early_exit_v0323.md`).

Status (v0324): `g8+0x1a4` bit 14 is native on the long body
(cascade `0x22b44` skip-mask + post-diagnostic `0x22dd4` counter++,
unit **289**). Bit 4/26 and `g7+0x828` bit 14 remain fail-closed
(see `decomp/i960/notes/fa_coli_long_b14_v0324.md`).

Status (v0325): `g7+0x828` bit 14 joins at `0x22e24` (unit **286**).
Bits 10/8 and `r11>=40` remain fail-closed (see
`decomp/i960/notes/fa_coli_long_828b14_v0325.md`).

Status (v0326): `0x43888` bit-20 non-match is native (`cmpobne`
taken, unit **307**). `r11>=40` / `cmpoble 30` and remaining
`+0x828` bits fail closed (see
`decomp/i960/notes/fa_coli_43888_b20nm_v0326.md`).

Status (v0327): `r11>=30` cmpoble gate joins at `0x22e24` (unit
**299**). `r11>=40` diagnostic-arm r4-offset and remaining siblings
fail closed (see `decomp/i960/notes/fa_coli_long_r11_30_v0327.md`).

Status (v0328, measurement): `g8+0x1a4` bit 4 has four sites
(cascade `0x22b7c` r11-transform, post-diag `0x22c98` join,
`0x22e44` g0=0x2ce, miss-tail offsets `0x1c`/`0x10`). Probe 301.
Not recovered — the four sites interact and the unit test still
fail-closes (see `decomp/i960/notes/fa_coli_long_b4_v0328.md`).

Status (v0329): `g7+0x828` bits 10/8 are native (unit **295**/**291**).
Bit 4, `r11>=40` and bit-13 profundo remain fail-closed (see
`decomp/i960/notes/fa_coli_long_828b10b8_v0329.md`).

Status (v0330): `g8+0x1a4` bit 26 is native on the long body
(post-diag `bbs 26 taken` → `0x22e24` join + `0x230d4` compact,
unit **263**). Bit 4 and bit-13 profundo remain fail-closed (see
`decomp/i960/notes/fa_coli_long_b26_v0330.md`).

Status (v0331): `r11>=40` diagnostic-arm shape is native (unit
**300**; r4=8 + `cmpoble 30` join). Bit 4 and bit-13 profundo remain
fail-closed (see `decomp/i960/notes/fa_coli_long_r11_40_v0331.md`).

Status (v0332): `g8+0x1a4` bit 4 is native on the long body (4
sites: cascade `0x22b7c` r11*3>>1 + g0=0x23d6b, post-diag `0x22e24`
join, `0x22e48` g0=0x2ce, miss-tail offsets `0x1c`/`0x10`). Unit
**301**. Bit-13 profundo remains fail-closed (see
`decomp/i960/notes/fa_coli_long_b4_v0332.md`).

Status (v0333): bit-13 profundo (`+0x5b8` bit 0 clear, bit 3 clear,
scan ∉ {2,5,6}, +0x828 bit 9 clear) is native via the `0x22808`
alt tail → float tail (unit **223**). Bit-13 sub-paths (bit 3,
scan 2/5/6, +0x828 bit 9) remain fail-closed (see
`decomp/i960/notes/fa_coli_long_b13profundo_v0333.md`).

Status (v0334, measurement): bit-13 sub-paths measured. Scan 2/5/6
(310) and `+0x828` bit 9 clear (317) join the warm cascade; code
implemented but unit-test shapes need dedicated setup. Bit 13 +
bit 3 set is the v0323 early-exit (12). `0x22744` and `0x22794`
(bit 12 set) remain fail-closed (see
`decomp/i960/notes/fa_coli_long_b13subpaths_v0334.md`).

Status (v0335): `0x22794` scale transform is native (`r11>>=1`,
`r9*=0.5`, `r8=1`, cascade at `0x22918`). Probe 263. `0x22744`
(bit 3 + scan≠{2,5,6}) and `0x227dc` remain fail-closed (see
`decomp/i960/notes/fa_coli_long_b13_22794_v0335.md`).

Status (v0336): bit-13 + bit 3 sub-paths native. `0x22744` (scan
2/5/6 → shared path), `0x22778` (scan∉{2,5,6} → bit 12 scale /
scan==1 `0x227ac` scale / else cascade), `0x227dc` fail-closed
(needs type-5 record). `0x227c4` remains fail-closed (see
`decomp/i960/notes/fa_coli_long_b13_b3_v0336.md`).

Status (v0337): `0x227c4` is native (diag pair + g0=1 + alt tail
join at `0x22848`). Probe 219. `0x227dc` remains fail-closed
(needs type-5). See `decomp/i960/notes/fa_coli_long_b13_227c4_v0337.md`.

Status (v0338): `g7+0x1a4` bit 22 is native at all four `0x22e24`
join sites (probe 305). `g7+0x821=3` already works (probe 301).
Board bit 9 clear on bit-14 hits `call 0x502a4` (correctly
fail-closed). See `decomp/i960/notes/fa_coli_long_g7b22_v0338.md`.

Coli campaign summary (v0330–v0338): all main `g8+0x1a4` flag
bits (3/4/8/10/13/14/16/18/26), the diagnostic cascade, bit-13
profundo alt tail, bit-13 sub-paths (`0x22744`/`0x22778`/`0x22794`/
`0x227ac`/`0x227c4`), and `g7+0x1a4` bit 22 are native. Remaining:
`0x22d8c` g0=5 and sibling continuations, `0x227dc` (type-5), and
`0x502a4` (board bit 9). The measured bit-16 scan-4 edge through
`0x22d8c`/`0x22e24` is now covered by v0451/v0452; other bit-16
compositions remain fail-closed. See
`decomp/i960/notes/fa_coli_campaign_final.md`.

Status (v0339): the `0x22d8c` bbs-11 edge is native at all four
`0x22e24` join sites (`g7+0x1a4` bit 22 set, `g8+0x1a4` bit 11 set,
bit 4 clear): `mov 5, g0`, `0x230d4` g0=5 fork (`cmpobne 5` nt,
`r3 = 42` via bbc-11 nt, shared `0x231ec` tail, `0x23238`
early-out), `0x22d9c` tail (`g8+0x198 = g0 + 0x0c010000`,
caller-r11 halfword at `g8+0x5de`), `0x23070` skip of `0x18a54`,
ret at `0x230b8` (unit **149**). Other `0x22c88` bbs-16 compositions, the
g0=5 fork siblings (r3=40, bit-25-set table return, branch-byte-set,
bbc-20-nt, `0x18a54` call), `0x227dc` and `0x502a4` remain
fail-closed. The separate v0451 bit-16 scan-4 witness reaches the shared
join through `bbc 22`; the measured bit-22-set sibling is covered by v0452,
while other flag compositions remain fail-closed. See
`decomp/i960/notes/fa_coli_22d8c_g05_v0339.md`,
`decomp/i960/notes/fa_coli_225cc_bit16_scan4_v0451.md` and
`decomp/i960/notes/fa_coli_225cc_bit16_bit22_v0452.md`.

Status (v0340): `0x227dc` is native for the miss shape
(`g8+0x1a4` bits 13+3, scan 1, `g7+0x844` bit 30,
walkable `g7+0x848` index → type-8 miss): `ld/st/mov 5/call
0x1ab34`, `g8+0x198 = index`, `g7+0x198 = 0x11000000`,
`g7+0x822 = low byte`, ret (unit **87**). The wrapper admits
scan==1 only for bit13+bit3, no bits 15/16, `0x844` bit 30 set;
the early bit-3 gate is scan-aware (`+3` for scan-1). A type-5
match, `0x848 = 0` (reference faults), the `0x22c88` edge,
`0x502a4`, and all other scan!=0 entries remain fail-closed.
Notably the wrapper had rejected scan!=0 since v0304, so the
v0334/v0336 scan!=0 long-body code was unreachable until now.
See `decomp/i960/notes/fa_coli_227dc_miss_v0340.md`.

Status (v0341, measurement): the `0x502a4` balx is deferred with
evidence. Two ROM sites call it: `0x22948` (cascade, board bit 9
only) and `0x22e04` (bit-14 `0x22dd4` path). The reference
executes into `0x502a4`/`0x502c0`/`0x508c4` and halts on
unimplemented `dmovt` at `0x508d4` (81 steps). Unblock = implement
`dmovt` from the i960 manual + unit test, then attribute the
subtree. Both sites correctly fail-closed.
See `decomp/i960/notes/fa_coli_502a4_defer_v0341.md`.

Status (v0342, measurement): the `0x19ef8` selector-bit14 clrbit
block is measured (20 steps: `+0x1a4` bits 5/6/21 cleared in r7,
`+0xbe4 = 1`) and the mutated path rejoins warm exactly
(985-step identity diff, +5 insert only; g0 self-masks at
`0x1a034`; `+0x1a4` copy/overwrite handled generically). Recovery
deferred: the only player-entry park has degenerate floats and
warm faults identically at `0x2705c cvtri`, so the tail, counts
(predicted `1657`) and final state are unprovable until a
live-valid park exists. The v0309 `+0`-word bit-5 drive has no
clearing mechanism and stays fail-closed.
See `decomp/i960/notes/fa_player_19ef8_prologue_v0342.md`.

Status (v0343): the bit-30-clear scan-1 leaves are native. `+0x828`
empty takes `0x227ac` (unit **275**), bit 12 takes `0x22794`
(unit **271**), bit 13 takes `0x227c4` into the alt tail with
`g0 = 1` (unit **224**). The wrapper admits scan==1 + bit13 +
no bits 15/16 without the `0x844` condition. Executing the
previously-dead code fixed: `0x230d4` bit-3 `r9 += 4`, the
`0x22788` scan re-read, `g0` threading into the alt-join
`0x230d4` call, `r9 += r5` (was `r9 = r5`), the float-tail
`divr` polarity (`r9/r4`), and the `b 0x22848` count. The four
shapes cover the whole admitted gate (only `+0x828` bits 12/13
are tested downstream).
See `decomp/i960/notes/fa_coli_22744_leaves_v0343.md`.

Status (v0344-B/C, helper + site-A wiring): `coli_502a4_body` +
`vf2_hybrid_coli_502a4_execute` recover the full balx-to-bx subtree
natively for both sites (direct unit: exact 140/170-step deltas,
exit regs, stores, bx-out `0x22960`/`0x22e20`, fail-closed
classification control). The wrapper admits scan-1 + bit-13-clear
+ bit-3-clear + board-clear and runs the helper at the cascade
`bbs-9`-nt edge, fail-closing past bx-out with stores applied
(wrapper unit: `UNSUPPORTED` + counter + copy bytes). Site B keeps
its gate (prefix crosses the continuation). Next: the `0x22960+`
continuation (`call 0x7fc0`, `balx 0x9444`, …). See
`decomp/i960/notes/fa_coli_502a4_v0344B.md`.

Status (v0345-A, leaf): `coli_7fc0_body` +
`vf2_hybrid_coli_7fc0_execute` recover the byte-expand leaf called
from all three `0x22960+` continuation sites (direct unit: exact
16/152/72-step deltas, return IPs, stores, guard control).
Unwired; `shlo` operand order settled (`operands[1]<<operands[0]`)
with a clean class audit. Next: `0x9444`/`0x9450` inline spans +
site-B prefix wiring. See
`decomp/i960/notes/fa_coli_7fc0_v0345A.md`.

Status (v0345-B, fa_coli site-A leg): the site-A leg runs natively
end-to-end (OK, delta 883, calls/rets 7/8, all stores proven).
`coli_225cc_sitea_cont` covers `0x22960` → `0x22e24` and joins the
existing tail unchanged; `long_body` reports calls/rets.
See `decomp/i960/notes/fa_coli_full_leg_v0345B.md`.

Status (v0346, coli exit landing): live `call 0x225cc` at `0x22290`
returns to the `ret` at `0x22294`, which pops to scheduler `0x10dcc`.
`vf2_hybrid_coli_225cc_execute` double-pops that ret when entered with
return `0x22294` and a parent frame remains. Site-A live-landing unit
lands `0x10dcc` at exact **884/7/9**. Procedure-only stand-in returns
(`0x22240`) keep the single pop. Remaining coli items: unmeasured
input variants, site-B-only entry, and hybrid whole-task shape pins
for non-warm paths. See
`decomp/i960/notes/fa_coli_exit_landing_v0346.md`.

Status (v0347, tracks B–D): native `native-nth-dispatch` MATCH
through dispatch **40** (CTest pin added at 12); no hard boundary
in range. fa_player `0x4505` completes on `punch10` (1745 steps)
but warm `0x505` still faults — bit-14 sibling recovery deferred.
Coli site-B-only `0x22dd4` board-clear gate is wired with the
proven `0x502a4`#siteB + `0x7fc0`#4 helpers (probe span 237 steps);
a live whole-task drive for that composition remains open. See
`decomp/i960/notes/tracks_bcd_v0347.md`.

Status (v0348, items 1–4): fa_player `0x4505` is native-admitted on
the measured punch10 shape (1745/4/4, table mask `& 0x1fff`,
`+0x1a4` 0 or bit 5 only); warm `0x505`/`0x284` still fail closed.
Site-B-only whole-task is structurally unreachable without a board
write between cascade and `0x22dd4` (none in ROM). Coli dispatcher
now pins measured sibling **9528/18/19** alongside warm
**9214/18/19**. Native dispatch MATCH through **1000**. See
`decomp/i960/notes/items_1to4_v0348.md`.

Status (v0349, open-item closeout): `0x4505` reference replicated
on punch10-t6/pf5/type6 (1745/4/4); `player-14288-*` still faults
`0x2705c` — C gates F0 bits 31/1 fail-closed; unit pins 1745/4/4
plus negative. Coli live tail `g0=1` from `coli-22404-e1` reaches
`0x225cc` then `0x10dcc` (346/6/8); static whole-task mutations pin
**9393/17/18** without `0x225cc`. Native dispatch MATCH through
**5000**. See `decomp/i960/notes/close_open_items_v0349.md`.

Status (v0350, remaining-open closeout): coli **live midbody**
`g0=1` from `coli-midbody-22210` reaches `call 0x22290` / `0x225cc`
and completes `0x10dcc` in **380/9/10** (`g13=0x514940` recipe).
`0x4505` ROM-backed **1745/4/4** now also on `sixth-regen` and
`player-14288-fifth-rt` (not only punch10 files); sibling measured
counts boot **1743**, natres **1659** stay fail-closed in C. Native
dispatch MATCH through **8000**. See
`decomp/i960/notes/close_open_v0350.md`.

Status (v0344-A, executor): `dmovt` reg-reg is implemented
(exact `0x508d4` word + derived pair-copy unit, flag-neutral)
and `mulo` sticky-sets `OVERFLOW` on unsigned overflow, which
unblocks the `0x502a4` digit loop (exits iteration 9; both balx
sites traced to computed `bx`-out `0x22960`). Assumption grade:
architecture-inferred, pins-validated — re-scope if any pin
moves. Both coli gates stay fail-closed: the `0x22960+`
continuation (`0x7fc0`, `0x9444`) is unrecovered, and wiring
without it would strand the caller.
See `decomp/i960/notes/fa_coli_dmovt_v0344A.md`.

The `fa_rob` helper at `0x14640` now also admits the measured signed-less
compare-prefix neutral tail: `+0x198 == 0`, `+0x654 != 0`,
`s16(+0x1aa) < s16(+0x62a)`, neutral `+0x197`, bit 4 clear and
`+0x194 == 0`. This path is exact through `0x146d8` (14 instructions,
no calls/returns) and leaves the `+0x654` value unchanged. Other less-than
compositions and the remaining `0x14640` state/flag siblings remain explicit
boundaries. See
`decomp/i960/notes/fa_player_14640_compare_less_v0412.md`.

The same signed-less prefix also covers the measured state-13 sibling with
bit 4 set and nonzero `+0x194`: it reaches `0x146d8` in 17 instructions,
clears `+0x654` and leaves LESS condition state. The combined compare-less
fixture proves both shapes; other state/flag combinations remain explicit
boundaries. See
`decomp/i960/notes/fa_player_14640_compare_less_state13_v0413.md`.

Status (v0414): the signed-greater compare-prefix now admits the measured
tail variants previously left behind the bit-4 gate. Neutral bit 4 clear with
zero `+0x194` reaches `0x146d8` in 14 instructions and leaves `+0x654`
unchanged with EQUAL condition state; nonzero `+0x194` reaches the same return
in 16 instructions after clearing `+0x654` with LESS condition state. State 13
also reaches `0x146d8` with bit 4 clear (16 instructions) or set (17
instructions), clearing `+0x654` in both cases. The four-case
`vf2_player_14640_compare_tail_live` fixture proves full live-state equality;
other compare-prefix state/flag compositions remain explicit boundaries. See
`decomp/i960/notes/fa_player_14640_compare_tails_v0414.md`.

The measured state-13 neutral tail at the same helper is also native when
`+0x198 == 0`, `+0x654 == 0`, `+0x197 == 13` and bit 4 is set. It reaches
`0x146d8` in 14 instructions, clears `+0x654` and preserves the final LESS
condition state. The ROM-backed `vf2_player_14640_state13_live` fixture pins
the complete live state; other state-byte and flag siblings remain explicit
boundaries. See
`decomp/i960/notes/fa_player_14640_state13_v0411.md`.

## 3. Camera

The startup and recurring camera corridor plus the validated optional viewport
construction paths are recovered. Unobserved movement-dependent tracking,
knockdown/throw cameras, alternate presets, zoom and mode-table transitions
remain unsupported.

## 4. Texture, video and geometry bridge

The observed texture expiration, pending palette upload and first non-zero
five-level stream expansion are recovered. The record publisher at
`0x0004b9b8` now recovers its out-of-range diagnostic: values `> 0x56`
render the signed value plus `tex num error` into tile RAM (19 cells,
`0x01000064`/`0x01000072`, 160 instructions) instead of failing, and the
second publisher (`argument0+1`) is still evaluated with wraparound
(v0196). Counter2 (`0x005502e0` via `0x0004b44c`) now lets that publisher
handle the range: an out-of-range first value skips the `0x00550288`
publication, renders the diagnostic, then continues through the queue
helper at `0x0004ba70` for 198 instructions / 5 calls / 5 returns (v0197,
21/21 exact). Video-layer rejection preflights inputs before writes. Still uncovered:

- alternate texture records, page formats, palette arguments and cache states;
- other stream headers, dimensions, timer states and mip layouts;
- compressed-stream corruption and invalid symbol/pair indexes;
- geometry ring-register patterns outside the accepted sequence;
- polygon FIFO packet protocol, lighting/clipping fidelity and hardware
  renderer behavior beyond the bounded direct/object reference executor; and
- production rendering output.

The texture orchestrator limit cluster at `0x0004bfe0` is now fully recovered:
every `display_mode % 32` selector is decoded — `bbs` with source masks `0xc0`/`0xc000`/`0x0c`,
fall-through `cmpobe` for 12/13, top `bbs 16` for runtime bit16 at `0x00500068`,
plus the `0x00500064 == 6/8` and `0x00500031 < 8` matrix for `mode 9` — and all six
limit pairs are proven ROM-backed via synthetic snapshots at `0x4bfe0`
(`vf2probe --rom-dir D:/ia/vf2-decomp/roms/vf2 --until 0x4c11c --read-u32`)
sweeping `display_mode 0..255 × runtime bit16` (512 cases):
`0x3e80/0x4e20`, `0x4330/0`, `0/0x4e20`, `0x4330/0x4e20`, `0x12a8/0x4330`, `0x32c8/0x4e20`;
write-skip `2,3 mod32` cases remain explicit `VF2_ERROR_UNSUPPORTED` with unchanged RAM
(v0200, `vf2_orchestrator_limits_tests` locks 512 probes).

The observed phase-17 dispatcher path is accepted when phase state is non-zero.
Controlled ROM-backed differential evidence recovers both phase-navigation
directions in `0x00058fe0`: gameplay mask `0x08001008` advances the index with
`11 -> 0` wraparound, while bit 13 (`0x00002000`) decrements it with `0 -> 11`
wraparound. Both mark the old double-indirect phase target with `0x8020` and the
new target with `0x801c`, and the forward mask has ROM-accurate priority. The
`0x04000104` reset/display path sets phase-index bit 7, clears the 48x64 tile
plane through `0x00008ef0`, and centers the phase label via
`0x00060410 -> 0x00007fc0`.

The resulting observed bit-7 entry (`0x8b`) is also recovered: `0x00059154`
selects `0x0005ef60`, whose first visit performs meter+CRC, clears the tile plane,
draws `EXIT TEST MODE` and arms counter 320. Positive countdown visits and the
terminal `counter 1 -> 0` path at `0x0005f07c` are now recovered. The terminal
clears the observed layer/game state, writes the reset diagnostic through
`0x0006116c`, and hands off non-returningly to `0x000000b0`. Warm boot stages 1
and 2 are strict-equal through `0x0000052c`, and the post-reset continuation is
now recovered through `0x000098b0`: 60,078 instructions to `0x0006dd4c`, then
15 strict initializer blocks / 1,498,968 instructions covering descriptor-stream
copies, backup-SRAM restore, palette/table construction and hardware-core setup.
The continuation now crosses the call from `0x000098b0` into `0x0004b020`,
clears its six texture state/counter words, and derives the timer threshold in
`0x0004afb4`. It now composes the already-recovered `0x00000b6c` timer/wait
helper, returns to `0x0004afdc`, captures the initial frame byte and reaches the
status-poll loop at `0x0004afe4`. That loop now injects and resumes the shared
frame interrupt, then follows the observed frame-change exit through the status
store and call to `0x00000f7c`. The early helper's odd/high-byte wait, interrupt
resumption, `0x00002ec4` video-status latch and two caller returns are now
composed through `0x0004b07c`. The following observed equal-identity path now
checks the board/four graphics-data identities and initializes ten texture
records before entering `0x0004b820`; identity failures reject transactionally.
Its four-instruction wrapper is recovered through nested entry `0x0004b9b8`.
The observed nested setup activates record zero, clears the restart words and
unwinds to `0x000098b4`; alternate record IDs, priorities, bit-4 replacement and
mismatch diagnostic branches remain uncovered. The following `0x00011704` luma
expansion is recovered through `0x000098b8`. Its early-wait continuation now
composes the shared helper and video-status latch through `0x000098bc`. The
following `0x00011744` run-length expander's first 8,192-byte geometry pass,
frame commit and early wait are recovered through `0x0001179c`; the second
8,192-byte pass now continues the live decoder through the following frame
commit, the third reaches the next commit, and the fourth and final pass reaches
its own commit. The final commit/wait unwinds the expander, and the caller's next
early wait completes through `0x000098c4`. The following `0x000117f8` geometry
table, frame commit and early wait are recovered through its return at
`0x000098c8`. The following `0x0004ad40` reset is recovered through `0x000098cc`.
The following `0x00007f7c` and `0x00007ef0` constant-table copies are recovered
through `0x000098d4`. The existing `0x00010cbc` task-registry initializer is now
composed through its return at `0x000098d8`, and the `0x00050130` graphics-buffer
initializer returns through `0x000098dc`. The `0x0004e7b4` render-state reset and
nested 216-record clear return through `0x000098e0`. The `0x00044084` game-default
initializer and its two bounded table helpers return through `0x000098e4`. The
following `0x00053750` object-table copy and sentinel setup return through
`0x000098e8`. The `0x0000a0c4` ROM-backed effect-table copy and clear return
through `0x000098ec`. The `0x000012bc` input-ring initializer and the observed
mode-zero `0x00000fa0` diagnostic I/O initializer return through `0x000098f4`.
The inline 192 KiB game-data copy beginning there reaches `0x00009920`. The
observed `0x0000245c` display-offset initializer is recovered through its return
at `0x00009924`; alternate game-state classifications and split-screen flag
paths remain unsupported. The observed `0x0001128c` accumulator and `0x000113f4`
profile defaults are recovered through `0x0000992c`, followed by the inline
gameplay-global initialization through `0x000099fc`. Alternate accumulator
modes and signed profile overrides remain unsupported. The `0x0001fcc0`
input-profile selector is now strict-differentially recovered for the baseline
path and controlled modes 6, 10, 11 and 12. Fighter-mode pairs `2/1` and `1/2`
select mode 12, flag bits 21+20 or a live mode byte exercise mode 10, control
byte 2 redirects mode 10 to mode 11, and `0x0001ff0c` reproduces the mode-10
and mode-6 float override tables after the shared 26-float fill. The subsequent
ROM profile loader also covers `profile == 4`, including its `0x10cc` timeout
write. Seven controlled states match the reference at all three native block
boundaries (21/21 comparisons) through `0x0001fe60`; the palette wrapper then
reaches nested entry `0x00002c38`. The observed palette body is recovered for its 28-row by
32-entry RGB ramp and page latch, including the return stub at `0x00020050`.
The resumed `0x0001fe64` prefix through the `0x4b410` helper and state clears
is recovered, as is the 90-instruction `0x0002eab8` initializer and nested
`0x00031004` setup. Its `0x0001fedc` call into the 66-row `0x00011704` luma
copy and trailing return are now recovered with live pointer poststate. The
frame-dispatch bridge now covers selector 2's `0x0000ab0c` reset and advances
to selector 3. Selector 3's phase-zero `0x0000ae78` mode-table worker is now
recovered for both the live fallback profile and the zero-derived alternate
profile, including its two descriptor expanders and both ROM text sources.
Selector 17's `phase_state == 0` wrapper now follows its separate `0x00055008`
control-menu dispatcher across all 14 idle entries (0-13), every neighboring
forward/reverse transition and both 0/13 wraps. The formerly opaque screens now
reuse recovered MAIN_DATA-backed decimal/hex/text renderers plus motion,
camera/material/polygon and texture state helpers; input bit 5 covers release,
held and latched behavior on every screen, including index 13's special
43-instruction held-button early exit. Ninety-eight controlled cases are strict
ROM-backed complete-live-state matches, so there are no longer missing
phase-zero menu indices or entry transitions. Selector 3's phase table is now
complete: all eighteen entries (phases 0-17) are native, including phase 16's
countdown stay/advance pair at `0x0000c414` and phase 17's phase-reset wrap at
`0x0000c448`, each strict-matched against the reference at the `0x0000a010`
boundary. Remaining input modes, other bit-7 indirect table entries, and
unmeasured branch-level control combinations inside the now-native phase-zero
screens remain unsupported. The subsequent
`0x00002de4` palette-page upload is covered for both its inactive
condition-preserving return and its active 28-page RGB upload path.

## 5. Audio and platform

Only the accepted deterministic sound-task buffer behavior, the SCSP
register/sample/MIDI host boundary, and the ROM's 68000 voice-maintenance
transition and command-dispatcher boundaries are recovered. The populated
sample-table prefix of the `0x90` allocator is also covered. The Motorola
68000 command handlers other than the bounded `0xc0`/`0xe0` paths, selected
no-live-voice `0xb0` entries and that allocator prefix, live voice/DSP synthesis,
native windowing, gamepad mappings,
frame pacing and production platform integration remain unimplemented.

## 6. Transactional rejection coverage

The player interrupt composite now preserves CPU state when a nested player
branch is rejected, and video input validation occurs before writes. Other
large composite blocks have not yet been proven globally transactional for every
unsupported subpath; additional candidate-state or rollback boundaries should
be introduced as new rejection cases are observed.

### v0161 positive state-8 bit-14 high pair

The high-26+high-29 pair `0x24004140` is independently ROM-measured across all 12 fighter-distribution/countdown/mode-bit-6 cases and is now admitted with its mask-local second-call accounting correction. Other unmeasured bit-14 multi-high extensions remain fail-closed.

### v0205–v0210 positive state-8 bit-6+14 high family over 26,29,30,31

The 15 non-empty high subsets over bits 26,29,30,31 on base `0x00004140`
(bit-6+14) have been expanded from single-mask admissions to full
low-bit cubes over bits 1,2,4 (8 masks per high subset, `15×8=120` masks).
Each mask is `36/36 exact` via `validate_game_info_full_dispatch.py`
(3 distributions ×2 countdown ×2 mode6 ×3 thresholds `0..2`) with
`+4 bilateral / +2 unilateral` and the same stale-frame. The high
family was refactored in v0212 into a single compact predicate
`(combined & ~0xE4004156)==0 && (combined & 0x4140)==0x4140 && high!=0`.

### v0211 base low cubes

The no-high base `0x00004140` and the high-16 variant `0x00014140`
were expanded from 1–2 masks to 8 masks each (low 1,2,4), `16` masks
`36/36 exact`, completing `136` masks for the `0x4140` family.

### v0213–v0215 other positive bases low cubes

* `0x00008140` (bits 6+14+15) `1→8` low cube with `-5/-3` (v0213, compacted v0217 to `& ~0x16`)
* `0x00010140` (bits 6+14+16) `1→8` with `+8/+4` plus bit11 write (v0214, compacted v0217 — fixes `0x10144` narrow-check outlier)
* `0x0000C140`/`0x0001C140` (bits 6+14+15 and 6+14+15+16) `2→16` low cubes
  with `+5/+2` (v0215, compacted v0218 to `& ~0x10016`)
* `0x00018140` `1→8` low cube with `+9/+4` plus bit11 (v0216, compacted v0217 — fixes `0x18144` outlier)
* `0x00004140`/`0x00014140` (bits 6+14 with/without 16) `2→16` low cubes
  with `+4/+2` (v0211, compacted v0219 to `& ~0x10016`)

Each `36/36 exact`. Other positive bases (e.g. `0x8140` high combos,
`0xC140` high combos) remain explicit boundaries until their low cubes
are measured. Compact forms now use `& ~0x16` or `& ~0x10016` and share
`hybrid_set_stale_low()` (v0220).

### v0221 high-26 8140 low cube

* `0x04008140` (high-26 + bits 6+14+15) `1→8` low cube with `-5/-3` (v0221)

### v0222 high-29 8140 low cube

* `0x20008140` (high-29 + bits 6+14+15) `1→8` low cube with `-5/-3` (v0222)

### v0223 high-30/31 8140 low cubes

* `0x40008140`/`0x80008140` (high-30/31 + bits 6+14+15) `2×8` low cube with `-5/-3` (v0223)

### v0224 high-26 10140 low cube

* `0x04010140` (high-26 + bits 6+14+16) `1→8` low cube with `+8/+4` plus bit11 (v0224)

### v0225 high-30/31 10140 low cubes

* `0x40010140`/`0x80010140` (high-30/31 + bits 6+14+16) `2×8` low cube with `+8/+4` plus bit11 (v0225) — high-29 `0x20010140` stays `0/36`

### v0226 high pairs 8140

* `0x24008140`, `0x44008140`, `0x84008140`, `0x60008140`, `0xA0008140`, `0xC0008140` (6 pair bases) each `1` mask `36/36 exact` with `0` excess (v0226) — low variants `|low` remain `0/36`

### v0227 high triples/quad 8140

* `0x64008140`, `0xA4008140`, `0xC4008140`, `0xE0008140`, `0xE4008140` (5) each `1` mask `36/36 exact` with `0` excess (v0227)

### v0228 high pairs/triples/quad 10140

* 11 masks for `0x10140` over 26,29,30,31 pairs/triples/quad `0` excess +bit11 (v0228)

### v0229 high pairs/triples/quad C140

* 11 masks for `0xC140` over 26,29,30,31 pairs/triples/quad `0` excess (v0229)

### v0230 high bulk 18140/1C140/14140

* 11 for `0x18140` +11 for `0x1C140` +10 for `0x14140` (quad fail-closed) `0` excess (v0230)

### v0231 high 8140 low variants

* 11 high bases ×7 low variants (77 masks) `-5/-3` (v0231) — base low 0 already `0` excess

### v0232 high C140 low variants

* 11 high bases ×7 low variants (77 masks) `+5/+2` (v0232)

### v0233 high singles low variants 14140/18140/1C140

* 4 highs ×7 low (84 masks) `+4/+9/+5` (v0233) — base low 0 via `4140` high compact for `14140`

### v0234 high singles base 18140/1C140

* 8 base low-0 (`0` excess +bit11) (v0234)

### v0235 high-29 10140 low cube

* `0x20010140` (high-29 + bits 6+14+16) `1→8` low cube with `+6/+3` plus bit11 and `fighter+0x6da=0x1e` for bit29 (v0235)

### v0236 high pairs 10140 low variants

* 11 high bases ×7 low variants (77 masks) — 7 with bit29 `+6/+3` +`0x1e`, 4 without `+8/+4` (v0236)

### v0237 high pairs 18140 low variants

* 11 high bases ×7 low variants (77 masks) — 7 with bit29 `+7/+3` +`0x1e`, 4 without `+9/+4` (v0237)

### v0238 high pairs 1C140/14140 low variants

* 11 high bases ×7 low for `0x1C140` (77 masks) `+5/+2` and 10 bases ×7 low for `0x14140` (70 masks) `+4/+2` (v0238)

### v0239 high singles C140 low variants

* 4 highs ×7 low for `0xC140` (28 masks) `+5/+2` (v0239)

### v0240 14140 quad low variants

* `0xE4014140` quad low `1→8` (7 masks) now `+4/+2` via `& ~0x16` (v0240) — `70→77` for `0x14140`

### v0241 high quad E400 with base 0x140

* `0xE4000140` `1→8` (8 masks) `+0` base / `-3/-6` low (v0241)

### v0242–v0244 high-family base 0x140 low variants

* 5 singles ×7 low (35 masks) `-3/-6` (v0242)
* 10 pairs ×7 low (70 masks) `-3/-6` (v0243)
* 14 triples/quads ×7 low (98 masks) `-3/-6` (v0244) — excludes `0xE4000140` (v0241) and quint `0xE4200140` (18/36)

### v0245–v0246 base 0x140 full closure

* 30 bases without low `30` masks `cd0/mode6` split (`f0 +8/+3, f1 +4/+3, bi +7/+6`) (v0245)
* quint low `7` masks `cd` split (`cd0 +8/+11, cd1 +3/+6`) (v0246) — high-family base 0x140 now `248/248` exact

### v0248–v0253 bit-21 low variants for remaining bases
* `0xC140` bit-21 low `16×7=112` masks `+2/+5` no bit11 (v0248)
* `0x4140` bit-21 low `16×7=112` masks `+2/+4` no bit11 (v0249)
* `0x14140` bit-21 low `16×7=112` masks `+2/+4` no bit11 (v0250)
* `0x10140` bit-21 low `16×7=112` masks `+4/+8` plus bit11 (v0251)
* `0x18140` bit-21 low `16×7=112` masks `+4/+9` plus bit11 (v0252)
* `0x1C140` bit-21 low `16×7=112` masks `+2/+5` no bit11 (v0253) — plus `0x8140` bit-21 low `112` masks `−3/−5` (v0247) already closed. Total `672` masks.

### v0254 middle-high low variants (generalizes v0247–v0253)
* same 7 bases with `low !=0` and `outer 16` but `MIDDLE=0x1B7E3EA9` (20 bits: `0x200`+`0x400`+… excluding `outer`/`base`/`low`/`bit6`/`0x00800000`). Single-bit middle highs `9` bits and multi-bit combos (`0xC0000`, `0x1B7C0000`, `0x1B7E3EA9`) all share per-base `−3/−5`/`+2/+5`/`+4/+8`/`+4/+9` (v0254). Representative `40` combos `36/36 exact`; `outer`-only stays exact, `0x00800000` stays `NATIVE-FAIL` (excluded).

### v0255 middle-high bare+low (extends v0254)
* removes `low !=0` guard — any middle `0x1B7E3EA9` with `outer 16` and `low 8` (incl. bare) admitted, same per-base accounting and bit11 (v0255). Bare `0x00048140` etc `36/36 exact`.

### v0256 bit-23 bridge (extends v0255)
* recovers `0x17b68` helper `bbs 23,r15,0x17fe8` path: `fighter+0x30=0, fighter+0x1c=0, if 0x624!=0 then fighter+0x620=1`, `28/30` instructions vs `31` for `0x624==0/!=0`, converges to common `0x1853c` tail. `MIDDLE` widens to `0x1BFE3EA9` (`0x1B7E3EA9|0x00800000`, 21 bits) and mask `~0xFFFE3EBF`. Same 7 bases now admit any middle including bit23: `0x00808142` etc `36/36 exact` (representative `12` masks, `7` bases x `16x8` outer/low). Bare `0x00808140` etc exact. Counters adjust via same per-base `−3/−5` etc.

### v0257 bare pure-bit21 fix (extends v0256)
* bare `0x00208140` etc (`7` bases, outer `16`, middle `0x00200000` alone, low `0`) were `0/36` DIFF `-3` (middle-high over-corrected); they need `0` excess, not `−3/−5`. Guard bare with `((low & 0x16)!=0 || (middle & 0x1BDE3EA9)!=0)` so pure-bit21 bare falls through to native `0` and now `36/36 exact` (`112` masks `16×7`). Low `0x00208142` etc already `36/36`; bare `0x00808140` etc stay `36/36`.

### v0258 base 0x140 single-middle (extends v0257)

* `0x140` + exactly one `MIDDLE 0x1BFE3EA9` bit (`20` bits:
  `0x1,0x8,0x20,0x80,0x200,0x400,0x800,0x1000,0x2000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x8000000,0x10000000`)
  with any outer `16` and any low `8` (incl. bare) — `20*16*8=2560` but
  `0x00200000` alone was already `36/36`? Actually `0x00200140` was
  `0/36 +3` before, now `36/36` — single-middle uniformly `+3/+6`
  (`0x340/0x940/0x1140/0x2140/0x20140/0x141/0x00200140` etc all `0/36 +3/+6`
  → `36/36`, `16*8*19=2432` masks after excluding pure `bit21`? Wait
  pure `bit21` now included: `19` vs `20` — `0x00200140` also `+3` so
  `20*128=2560` but `0x140` bare without middle stays `0`. Net `2432`
  with `0x1BDE3EA9` guard? Actually `0x1BDE3EA9` guard for `8140` etc
  not needed for `0x140`. Representative `12` masks `36/36`, multi-middle
  `0x1840` (`+3/+7`) and `0x200342` etc stay fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_base0140_single_middle_v0258.md`.

### v0259 base 0x140 any-middle (generalizes v0258)

* `0x140` + any `Mp = 0x1BDE3EA9 !=0` (19 bits: `0x1,0x8,0x20,0x80,0x200,0x400,0x800,0x1000,0x2000,0x20000,0x40000,0x80000,0x100000,0x400000,0x800000,0x1000000,0x2000000,0x8000000,0x10000000`)
  with or without `bit21 0x00200000`, any outer `16` and any low `8` — single 0x340, double 0xB40/0x1940/0x1340, quad 0x3D40, high 0x60140/0x8000340,
  bit21+Mp 0x00200340/0x00200940/0x00260140/0x08200340 etc all measured `+3` uni / `+6` bi → `36/36` (spot ~15 masks). Bare `0x140` and pure `0x00200140` remain `0/0` 36/36.
  Counts: `2^19-1=524287` *128=67,108,736 without bit21 plus same with bit21 = `134,217,472` masks (replaces v0258 `2432`). See
  `decomp/i960/notes/game_info_18644_positive_base0140_any_middle_v0259.md`.

### v0260 base 0x40 any-composition (extends v0259)

* `0x40` (bit6 alone) + any `MIDDLE 0x1BFE3EA9` (20 bits incl. bit23) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `+3` uni / `+7` bi:
  bare `0x40`, single `0x240/0x840`, double `0x1840`, bit21 `0x00200040`, bit21+single `0x00200240`, bit21+double `0x00201840`, low `0x42`, many `0x1BDE3EE9/0x1BFE3EE9` all `0/36 DIFF +3/+7` → `36/36` (spot ~12 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base0040_any_v0260.md`.

### v0261 base 0x4040 any-composition (extends v0260)

* `0x4040` (0x4000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `-2` uni / `-3` bi (native undercounts):
  bare `0x4040`, single `0x4240`, high `0x44040`, bit21 `0x00204040`, many `0x1BDE6E49` all `0/36 DIFF -2/-3` → `36/36` (spot ~8 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base4040_any_v0261.md`.

### v0262 base 0x8040 any-composition (extends v0261)

* `0x8040` (0x8000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `+3` uni / `+6` bi (native overcounts):
  bare `0x8040`, singles `0x8240/0x8440`, high `0xA040`, bit21 `0x00208040`, many `0x1BDE8E49` all `0/36 DIFF +3/+6` → `36/36` (spot ~8 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base8040_any_v0262.md`.
* `0xC040` (0x4000+0x8000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `-2` uni / `-4` bi (native undercounts):
  bare `0xC040`, single `0xC240`, bit21 `0x0020C040`, many `0x1BDECE49` all `0/36 DIFF -2/-4` → `36/36` (spot ~6 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_baseC040_any_v0263.md`.

### v0264 base 0x10040 any-composition (extends v0263)

* `0x10040` (0x10000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `-4` uni / `-7` bi (native undercounts) plus work-RAM `0x510b24`/`0x512b24` `|=0x800`:
  bare `0x10040`, singles `0x10240/0x11240`, bit21 `0x00210040`, many `0x30040/0x50040` all `0/36 DIFF -4/-7` + work-ram `0x08` at `0x10b25`/`0x12b25` → `36/36` (spot ~8 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base10040_any_v0264.md`.
* `0x14040` (0x10000+0x4000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset — uniformly `-2` uni / `-3` bi: bare `0x14040`, single `0x14240`, bit21 `0x00214040` all `0/36 DIFF -2/-3` → `36/36` (spot ~6 masks). Counts: `2^20*128=134,217,728`. See `decomp/i960/notes/game_info_18644_positive_low_family_closure_v0265_v0267.md`.
* `0x18040` (0x10000+0x8000+0x40) + any `MIDDLE` — uniformly `-4` uni / `-8` bi plus work-RAM `0x510b24/0x512b24|=0x800`: bare `0x18040`, single `0x18240`, bit21 `0x00218040` all `0/36 DIFF -4/-8` → `36/36`. Counts: `134,217,728`.
* `0x1C040` (0x10000+0x4000+0x8000+0x40) + any `MIDDLE` — uniformly `-2` uni / `-4` bi: bare `0x1C040`, single `0x1C240`, bit21 `0x0021C040` all `0/36 DIFF -2/-4` → `36/36`. Counts: `134,217,728`.
  Low `0x40` family (all 16 combos of `0x100/0x4000/0x8000/0x10000` with bit6) now fully closed; frontier remains helper `runtime bit5` and any remaining non-low positive compositions.

### Current positive threshold scope

`1,207,961,303` masks are now `36/36 exact` for the positive `0x1645c` corridor (`2007` + `134,217,472` base `0x140` any-middle v0259 + `134,217,728` base `0x40` any v0260 + `134,217,728` base `0x4040` any v0261 + `134,217,728` base `0x8040` any v0262 + `134,217,728` base `0xC040` any v0263 + `134,217,728` base `0x10040` any v0264 + `402,653,184` bases `0x14040/0x18040/0x1C040` any v0265-v0267)
(`120` high family + `991` base/low/high families + `784` bit-21 low variants (`112×7` bases: `8140`/`C140`/`4140`/`14140`/`10140`/`18140`/`1C140`)). All use the measured
stale-frame and compare result. Remaining positive compositions still
fail closed.

### v0268 fa_object handlers

The `fa_object0/1/2` dispatcher at `0x6ca64` was already native; its five
continuations are now recovered: init stubs `0x6cae0`/`0x6caf4` rewrite
`registry+0x0c` to the measured ret continuations `0x6caf0`/`0x6cb04`,
and `0x6caf0`/`0x6cb04`/`0x6cb08` are bare rets. Each of the six cases
(dispatcher + five continuations) is `exact` for full CPU state,
condition state, frame depth, all counters and full work-RAM `memcmp`
plus report kind/exit/counts (`tests/recovered/test_object_handlers.c`,
`vf2_object_handlers_differential`). The per-frame re-arm in
`execute_selector2_body` keeps these dormant in the accepted corridor;
they were proven from synthetic state. Still open: the `0x6ca84`
indirect `callx` service loop (count `3` at `0x6cad0`, control blocks
`[0x500878, 0x50087c, 0x500880]`, called from `0x1dd70`) and the
`fa_coli` recurring body at `0x221e8` with callees `0x22298` (31
blocks), `0x22404` (24 blocks), `0x225cc` (174 blocks) and `0x23524`
(6 blocks). Reachability scouting from regenerated fifth (`out-fifth.vf2snap`,
MATCH, 836 blocks) and sixth snapshots is negative: forcing the coli
slot runnable at either `0x221cc` or `0x221e8` over ~15.3M guest
instructions never dispatches index 10 (identical call/return counters,
`10255/10254` from fifth) — the accepted corridor dispatches only a
fixed few tasks per frame, so flag/entry mutation inside these windows
is exhausted. That parked-window attempt was executed in v0269 (boot
prefix chain + observe-parked boundary) with the same negative outcome;
see below.

### v0269 fa_coli recurring hunt (negative with mechanism)

The scheduler at `0x10d54` full-scans 29 tasks only when `0x500068`
bit 16 is clear at sweep entry; otherwise the single-slot fast path at
`0x10e68` (`0x10ea0`, rewritten to `0x500834` each frame) dispatches one
rotating task. Sweeps are single-pass (`0xa010` front-end, no loop), so
a sweep never revisits an index. The selector2 re-arm writes coli
`flags=1 + entry=0x221cc` coupled and unconditionally, but only in boot
frames. Measured over 61M+ guest instructions across five snapshot
windows (boot prefix chain, post-second observe-parked boundary, fifth,
sixth) with flag/entry forcing and a fast-path slot hijack: index 10
never executes outside the validated second sweep, flags stay 0, and a
forced `entry=0x221e8 + flags=1` state survives 8.87M insns untouched.
The recurring body needs that conjunction at a scanning sweep, which
the attract trajectory never produces — reachable, if at all, only in
frames via driven inputs. Tooling added (all
passive/default-off): `vf2probe --raise-irq/--enter-interrupt`,
`resume-trace` trailing injection args, `VF2_PARK_SNAPSHOT` boundary
parking in `observe-third-sweep`. See
`decomp/i960/notes/coli_recurring_hunt_v0269.md`.

### v0273 fa_coli recurring entry (extends v0269/v0270)

The v0269 conjunction is now reproduced without forcing: holding PUNCH
(`vf2cycles --input 16`) from the sixth-dispatch snapshot runs the
phase-11 countdown to its terminal, clears the phase flag (`0x8b` to
`0x0b`) and arms slot 10 with `entry=0x221e8 + flags=0x80000000`, all
under strict per-block differential. The recurring sweep's scan prefix
through the `callx` dispatch is native and exact (`27 + 16*index`
instructions: 235 for index-13 game_info, 187 measured for index-10
coli; 4 calls / 2 returns; caller-carried r0 preserved). The strict step
ends with both sides at `0x000221e8`; the `fa_coli` body itself
(`bbs 5 -> 0x22294` gate, `0x23524`/`0x22298` callees) remains the
explicit open boundary. See
`decomp/i960/notes/fa_coli_entry_v0273.md`.

### v0274 fa_coli body via measured interpretation

The PUNCH-driven warm body at `0x221e8` is now admitted as an explicit
original-i960 bridge: runtime bit 5 clear, `9,214` instructions,
`18` calls / `19` returns through `0x10dcc`, fighters from
`0x500804`/`0x500808`. Literal `movt 0, r8` (`0x236b8`) is handled in
the executor step used by `vf2probe`/`vf2_i960_run`. The PUNCH corridor
now completes `320/320` cycles (`14,962,620` instructions) MATCH back
to `0x1645c`. See `decomp/i960/notes/fa_coli_body_v0274.md`.

### v0275 fa_coli bit-5-set gate and `0x23524` boundary

Runtime bit 5 set is now a measured native early return: `ld`/`bbs`/
`ret`, **3 instructions**, `0` calls / `1` return, no stores, through
`0x10dcc`. Bit 5 clear remains the v0274 warm-body bridge.

The warm-call boundary of `0x23524` is recorded (not yet native):
**9,158 instructions**, **14** nested calls, `bbc 0, g6` not taken,
468 stores including the `g13+0x40` clear and both-fighter
`+0xe80..+0xf04` clusters. `0x23524` accounts for ~99% of the warm
body; native callees `0x23524`/`0x22298`/`0x22404`/`0x225cc` remain
open. See `decomp/i960/notes/fa_coli_gate_23524_v0275.md`.

### v0276 fa_coli mid-body child `0x22298` + hybrid segmentation

The first mid-body callee is now native. Both PUNCH-driven invocations
of `0x22298` take the measured early exit (bit 8 of `g8+0x1a4` clear):
`stos 0` into `g7+0x6dc`, **7 instructions**, `0` calls / `1` return.
Bit 8 set remains an explicit `VF2_ERROR_UNSUPPORTED` boundary.

`hybrid_execute_coli_body` segments the warm path (interpret `0x23524`
subtree → native `0x22298` ×2 → interpret tail). The whole-task pin is
unchanged (`9214 / 18 / 19`). ROM-backed PUNCH corridor remains
`320/320` MATCH. `0x23524` (still interpreted), `0x22404` and
`0x225cc` remain open. See
`decomp/i960/notes/fa_coli_bitmask_22298_v0276.md`.

### v0277 fa_coli contact query `0x22404` + warm-path skip of `0x225cc`

The second mid-body callee is now native. Both PUNCH-driven invocations
of `0x22404` take the measured early exit (bit 8 of `g7+0x1a4` clear):
snapshot `g7+0x1a8` into `g13+0x8c[slot]`, clear the slot bit in
`g13+0x90`, `g0 = 0`, **14 instructions**, `0` counted calls /
`1` return. Bit 8 set and slot `> 1` fail closed.

The warm tail does **not** reach `0x225cc`: both contact-query results
are zero, so `cmpobe` at `0x2223c`/`0x22284` skip `0x22290`. `0x225cc`
needs a drive where a contact query returns non-zero. `0x23524` remains
the dominant interpreted cost (~9,151 of 9,214 warm instructions).
See `decomp/i960/notes/fa_coli_contact_22404_v0277.md`.

### v0278 fa_coli `0x23524` callee cost attribution

Measurement-only. The 9,158-instruction warm `0x23524` path is
attributed by call-stack walk: `0x2396c` **5236** (×2, 402 stores),
`0x238f8` **2855** (×1, 0 writes; source mask `0x91f880` all-zero),
`0x23878` **810** (×6, pure bit-remap via ROM `0x2007b76`),
shell 179, `0x233d0` 44, `0x2364c` 17, `0x238a4` **10** (×2, bit-8
clear → `g3=0`). First native leaf chosen: `0x238a4`.
See `decomp/i960/notes/fa_coli_23524_attribution_v0278.md`.

### v0279 fa_coli `0x238a4` g3-scan leaf + `0x23524` hybrid parent

The first `0x23524` callee is now native. Both PUNCH-driven invocations
of `0x238a4` take the measured early exit (`g7+0x1a4` bit 8 clear):
`g3 = 0`, **5 instructions**, `0` counted calls / `1` return. Bit 8
set fails closed and leaves `g3` untouched.

`hybrid_execute_coli_body` now stops at `0x23524` and `0x238a4`,
native-recovers the pair, then continues into the v0276/v0277 children.
The whole-task pin is unchanged (`9214 / 18 / 19`). ROM-backed PUNCH
corridor remains `320/320` MATCH. Remaining `0x23524` callees stay
interpreted: `0x2396c`, `0x238f8`, `0x23878`, `0x233d0`, `0x2364c`.
See `decomp/i960/notes/fa_coli_23524_attribution_v0278.md`.

### v0280 fa_coli `0x23878` bit-remap leaf

The second `0x23524` callee is now native. All six PUNCH-driven
invocations remap bits `0..29` of `g3` through the main-data table at
`0x02007b76` (30 words). Empty source is **95 instructions**; full
source is **155**; single-bit is **97**. No memory writes. Table
entries `>= 32` fail closed.

`hybrid_execute_coli_body` now walks the six `0x23878` call sites
inside both `0x2396c` invocations before the `0x238a4` pair. The
whole-task pin is unchanged (`9214 / 18 / 19`). ROM-backed PUNCH
corridor remains `320/320` MATCH. Unit test `test_coli_23878_bit_remap`.
Remaining interpreted: `0x2396c`, `0x238f8`, `0x233d0`, `0x2364c`.

### v0281 fa_coli `0x238f8` nested bit-scan leaf

The third `0x23524` callee is now native. The 30×30 nested scan reads
source masks from buffer RAM `0x91f880` and remaps through ROM bytes at
`0x23284` into `g13+0x40`. Warm PUNCH source is all-zero: **2855
instructions**, **0** stores. Non-zero source executes the measured
`setbit`/`st` path.

`hybrid_execute_coli_body` now stops at `0x238f8` after the `0x238a4`
pair and resumes at `0x23644` (before `bal 0x23694`). The whole-task
pin is unchanged (`9214 / 18 / 19`). ROM-backed PUNCH corridor remains
`320/320` MATCH. Unit test `test_coli_238f8_warm_noop`.
Remaining interpreted inside `0x23524`: `0x2396c` (5236), `0x233d0`
(44), `0x2364c` (17), and the 179-insn shell.

### v0283 remaining-leaf measure (`0x2396c` / `0x233d0` / `0x2364c`)

Measurement-only. Reconfirms PUNCH `320/320` MATCH / `12946` blocks /
`14,962,620` instructions and the warm `0x221e8 → 0x22210` path at
**9158** instructions. Attributes every remaining interpreted block.

- `0x2396c`: **GO** (large). 30 blocks, warm path is fixed; six blocks
  never taken (`0x23a70`, `0x23a88`, `0x23af0`, `0x23b14`, `0x23b24`,
  `0x23b88`). Own cost **2618** per invocation (×2) plus three nested
  `0x23878` bodies. Stores: fighter `stq` cluster `g7+0xd00[0..29]`,
  `stos` `+0x624/+0x614/+0x618`, final `+0x644/+0x64c`, and the `g13`
  `+0xfc..+0x12c` accumulator words. Prefer split prototype then wire-up;
  inline the bit-remap (calling `vf2_hybrid_coli_23878_execute` from
  inside a native parent would pop the wrong frame).
- `0x233d0`: **GO**. Late entry at `0x233d0` jumps back into the shared
  body at `0x23398`. 44 instructions, single warm path, copies the ROM
  row at `0x232c4` into `g13+0xb4/+0xb8/+0xc0/+0xc4/+0xbc/+0x88`,
  leaves `g6 = 0`. Magic `+0x1a8` compares (`0x242`, `0x241`,
  `shlo 2,27 = 108`) and every other sibling fail closed.
- `0x2364c`: **GO**. 17 instructions. Diffs both fighters' `+0x1f4`,
  writes command `0x18003030` plus three delta words to FIFO `0x884000`,
  reads three words back, stores them at `g13+0xc8/+0xcc/+0xd0`.

See `decomp/i960/notes/fa_coli_2396c_measure_v0283.md`.
Next recovery slice is `0x233d0` + `0x2364c` (v0284).

### v0284 recover `0x233d0` + `0x2364c` warm leaves

Both small callees are now native C.

- `vf2_hybrid_coli_233d0_execute` — 44 insns / 0 calls / 1 return.
  Copies the ROM row at `0x232c4` into `g13+0xb4/+0xb8/+0xc0/+0xc4/
  +0xbc/+0x88`, leaves `g6 = 0`. Magic `+0x1a8` states and every
  unmeasured `bbc`/`bbs` fail closed.
- `vf2_hybrid_coli_2364c_execute` — 17 insns / 0 calls / 1 return.
  Both fighters' `+0x1f4` triples zero → writes command `0x18003030`
  plus three zero deltas to FIFO `0x884000`, stores the three hardware
  replies at `g13+0xc8/+0xcc/+0xd0`. Non-zero positions fail closed.

`hybrid_execute_coli_body` stops at `0x233d0` (after the six `0x23878`
walk) and at `0x2364c` (after `0x238f8`). PUNCH remains `320/320`
MATCH / `14,962,620` instructions. Unit tests
`test_coli_233d0_flag_builder` and `test_coli_2364c_fifo_delta`.
See `decomp/i960/notes/fa_coli_small_leaves_v0284.md`.

Remaining interpreted inside `0x23524`: `0x2396c` (5236, ×2) and the
179-insn shell.

### v0285 recover `0x2396c` warm poly-cluster builder

`vf2_hybrid_coli_2396c_execute` is now native C. Two PUNCH-driven
invocations replace the six-step `0x23878` bitremap walk. The leaf
inlines three bit-remaps (calling `vf2_hybrid_coli_23878_execute` from
inside a native parent would pop the wrong frame), copies the 30-trip
`stq` cluster, runs the threshold/inner/max scans, and stores the
measured `stos`/`st` results. Own cost **2618**/invocation plus three
remap bodies (warm sum 405). Unmeasured threshold/min/max/positive
siblings fail closed.

PUNCH remains `320/320` MATCH / `14,962,620` instructions.
Unit test `test_coli_2396c_poly_cluster`.
See `decomp/i960/notes/fa_coli_2396c_v0285.md`.

Remaining interpreted inside `0x23524`: the **179-insn shell** only.

### v0286 measure `0x23524` shell warm path

Measurement-only. The 179-insn shell plus the `bal 0x23694` body are
attributed glue-by-glue from the warm trace. Prologue pushes FIFO
command `0x1f003e3e` and clears 16 words at `g13+0x40`; call glue runs
`0x2396c`×2 / `0x233d0` / `0x238a4`×2 / `0x238f8`; `bal 0x23694`
reloads fighters, takes `bbc 3,g6` and `cmpobl 0,g13+0x148` on the warm
side, calls `0x2364c`, stores the six-word cluster at `g13+0xd4..0xe8`,
pushes FIFO `0x1e803d3d`, updates fighter `+0x18/+0x20`, and clamps
`+0x650` with constant `0x3cf5c28f`. All warm payload/store values are
zero except the four FIFO command words. Verdict **GO** for the native
shell. See `decomp/i960/notes/fa_coli_23524_shell_measure_v0286.md`.

Remaining interpreted inside `0x23524`: the **179-insn shell** only
(native recovery planned as v0287).

### v0287 recover `0x23524` shell as one native procedure

`vf2_hybrid_coli_23524_execute` is now native C. The 179-insn shell
plus the `bal 0x23694` body run as a single procedure, with every
recovered callee (`0x2396c`×2, `0x233d0`, `0x238a4`×2, `0x238f8`,
`0x2364c`) inlined for accounting only — calling the standalone
exports would pop the wrong frame. `coli_2396c_body` was extracted
from the v0285 export (wrapper unchanged). `hybrid_execute_coli_body`
now stops at `0x23524` once and resumes at `0x22210`.

Accounting: **9151** instructions / **13** calls / **14** returns for
the procedure; caller prefix adds 7+1 → path **9158 / 14 / 14**.
Unmeasured shell siblings (`bbc 0,g6` taken, `bbc 3,g6` taken,
`cmpobl 0,g13+0x148` taken) fail closed.

PUNCH remains `320/320` MATCH / `14,962,620` instructions.
Unit test `test_coli_23524_shell`.
See `decomp/i960/notes/fa_coli_23524_shell_v0287.md`.

Remaining interpreted in the coli warm task: mid-body/tail only
(`0x22210`→`0x22298`→`0x22404`→`0x10dcc`).

### v0289 recover coli mid-body/tail warm path

`vf2_hybrid_coli_midbody_tail_execute` is now native C. The 56-instruction
corridor from `0x22210` through the both-zero `cmpobe` pair and final
ret to `0x10dcc` runs as one procedure, inlining body-only
`coli_22298_body` ×2 and `coli_22404_body` ×2 (extracted from the
v0276/v0277 exports). Fighter pointers reload from `0x500804`/`0x500808`.
Final `g7`/`g8` are left swapped (`0x22230`/`0x22234`). Accounting
**56 / 4 / 5**. Bit-8-set siblings and the non-zero contact-result path
to `0x225cc` fail closed.

`hybrid_execute_coli_body` now walks interpret-entry (7 insns) →
native `0x23524` (9151) → native mid-body/tail (56) → `0x10dcc`.
The warm `fa_coli` task has **zero interpreted instructions** after
the entry prefix.

PUNCH remains `320/320` MATCH / `14,962,620` instructions.
Unit test `test_coli_midbody_tail_warm`.
See `decomp/i960/notes/fa_coli_midbody_v0289.md`.

### v0290 coli bit-8-set compact siblings

Two measured compact siblings are now native:

- `0x22298` bits 8 and 1 set — 8 instructions, same `stos 0` into
  `g7+0x6dc` as the warm path.
- `0x22404` bit 8 set with equal snapshots, pending bit clear,
  threshold `a >= b`, helper `r3 = 0` and empty scan mask — 30
  instructions, store 0 into `g8+0x6d4`, `g0 = 0`, `g14 = 0x2244c`.

Body-only statics return dynamic instruction counts so the mid-body
parent accounts correctly. Other bit-8 sub-branches (the 16-trip
float loops in `0x22298`, the non-empty scan / `g0 = 1` path in
`0x22404`) remain explicit boundaries. PUNCH remains `320/320` MATCH.
See `decomp/i960/notes/fa_coli_bit8_siblings_v0290.md`.

### v0291 measure `0x225cc` `g8+0x19f==22` shortcut (defer)

Forcing `g8+0x19f = 22` takes the `call 0x18bd4` shortcut. That helper
itself calls `0x1ab34` (ROM table walk) and `0x18b58` — still a
multi-block subtree, not a compact leaf. **Defer.** Does not affect the
warm PUNCH pin.
See `decomp/i960/notes/fa_coli_225cc_shortcut_v0291.md`.

### v0288 measure `0x225cc` reachability path (defer)

Re-runs the v0282 mutated drive to `0x225cc`. The `0x18bd4` shortcut
is not taken (`g8+0x19f != 22`); the measured path is a 248-instruction
multi-branch body with four nested calls (`0x230d4`, `0x23238` ×2,
`0x1ab34`) through `0x230b8`. Not a compact prefix — **defer**. Does
not affect the warm PUNCH pin.
See `decomp/i960/notes/fa_coli_225cc_prefix_v0288.md`.

### v0282 scanbit/bno NoBit fix + `0x225cc` reachability drive

`bno` after a successful `scanbit` was incorrectly taken (`EQUAL !=
OVERFLOW`). A hit now records OVERFLOW (bno not taken); a miss records
NONE (`dest = 31`) so bno fires. Warm PUNCH is unchanged (no scanbit
on that path).

A three-field mutation of `out/coli-22404-e1` (`g7+0x1a4` bit 8,
`g7+0x820 = 1`, dest slot `0x5149cc = 0xffff`) makes the first contact
query return `g0 = 1` in **73 instructions**. The caller then reaches
`0x225cc` after **96 instructions** (second query still warm-zero).
`0x225cc` remains unmeasured beyond its prefix (`g7+0x1234` counter++,
optional `call 0x18bd4` when `g8+0x19f == 22`).
See `decomp/i960/notes/fa_coli_225cc_drive_v0282.md`.

### v0351 Combate Vivo (arming + live midbody + 0x22298 sibling)

PUNCH arming from a fresh sixth-dispatch park is reproduced under
strict per-block differential: 330 `vf2cycles --input 16` frames leave
slot 10 at `entry=0x000221e8` / `flags=0x80000000` with countdown
`0x00500024` terminal. The live midbody `g0=1` recipe (`g13=0x00514940`,
fighter0 `+0x1a4=0x100`, `+0x820=1`, slot `0x5149cc=0xffff`) completes
from `0x00022210` to `0x00010dcc` in **380** steps with a long
`0x000225cc` (249 steps to `0x00022294`) and call-instruction graph
`0x22298`×2, `0x22404`×2, `0x225bc`×2, `0x223bc`, `0x225cc`,
`0x230d4`, `0x23238`×2, `0x1ab34`. The second `0x00022298` sibling
(bit 8 set, bit 1 clear, measured gates) is now native C (body 13,
`stos 0` at `g7+0x6dc`); bit-14-set, `+0x61c!=0` and scan `{2,5,6}`
remain fail-closed. Whole-task C pin of prefix+380 is **not** claimed:
probe does not re-enter the coli body from the armed park, and the
hybrid still fail-closes unmeasured `0x22404` hit-78 / long-body live
accounting beyond the existing unit shapes. Player sibling shapes
boot 1743 and natres 1659 remain fail-closed in C. Endurance MATCH is
observed through dispatch 9626+ on this MSVC Debug build.
`frontier.py` gained `--fighter-base` offset clustering; see
`decomp/i960/notes/combat_live_v0351.md`.

### v0352 closeout — player shapes, coli entry drive, player frontier

The standard `0x4505` probe drive (`g0=0x4505`, `g7=0x00510980`,
`--set-ip 0x00014288`) reproduces punch10/sixth-regen/fifth-rt at
**1745/4/4**, boot at **1743/4/4** (F0 clear or bit26 clear) and
natres at **1659/4/4** (F0 `0x84000002`, bit31 retained). Native
`hybrid_execute_player_19ef8` now accounts **1745** when player F0
bit 26 is set and **1743** otherwise among admitted F0 shapes;
natres bit-31 remains `VF2_ERROR_UNSUPPORTED`. From the armed coli
park, `native-resume` reaches `0x000221e8`; executing the whole coli
task with the midbody `g0=1` recipe measures **9398/17/18** without
reaching `0x000225cc` (the `0x23524` shell does not preserve the
midbody live contact state). The live first `0x22404` span is **78** steps to `ret 0x225b0` (body
**77** + ret) and is not C-pinned; gates remain those in
`close_open_v0352.md` (`+0x1a4=0x00010000`, `+0x820=1`, slot 0,
mask `0x2007ace`, FIFO `0x884000`, stores `f1+0x6d4/+0x65c..+0x664`).
Player corridor frontier after `0x4505` is measured at unsupported
**`0x00027cc8`** via `0x270e8→0x27b5c`; wrapper `0x270d4` remains
unsupported pending COBR-CC recalibration (v0358). Endurance MATCH is
observed through dispatch **10675** on this MSVC Debug build. See
`decomp/i960/notes/close_open_v0352.md` and
`decomp/i960/notes/player_270d4_slot_pin_v0358.md`.
