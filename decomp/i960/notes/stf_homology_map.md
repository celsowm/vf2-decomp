# STF ↔ VF2 homology map (reference navigation only)

`third_party/stfdecomp/` is a source-available (all rights reserved,
research-only) Sonic The Fighters disassembly: same AM2 family, same
i960 CPU, same Model 2 hardware generation (STF targets Model 2B,
VF2 is Model 2A). This note records **address/symbol/count
correspondences only** — compact derived metadata. No STF code is
copied, and no VF2 semantics are claimed from STF alone. Every
`→ VF2` entry below must still be proven by the VF2 oracle
(`vf2probe` / `vf2cycles` / `native-*-dispatch`) before any C recovery.

Status of the snapshot: `src/asm/rom_code1.s` ~136k lines (i960 text),
`rom_code2.s` ~22k lines (data + event table), `rom_code4.s` ~43k
lines (model tables), `rom_code1.ld` 1296 lines (memory map + fixed
RAM globals). Program ROMs (`sfight.zip`) were not present, so this
map is static-only; nothing was assembled or executed.

## 1. Task family (`fa_*` / `mod_fa_*`)

STF event table (`rom_code2.s:660`, `event_count = 30`,
`event_init_data:663`): repeating records
`{init_fn, mod_fa_*, 0, "fa_*  " asciz}`
(e.g. `nameentry_init/mod_fa_nameentry`, `fa_enemy0/mod_fa_enemy0`,
`control_init/mod_fa_control0`, `action_init/fa_rob0`,
`fa_tobi/mod_fa_tobi`, `get_game_info/mod_fa_game_info`,
`camera_init/fa_camera`, `fa_user/mod_fa_user`).

VF2 registry (`decomp/i960/tasks.csv`): 29 descriptors
`0x11dc0–0x124c0`, state addresses `0x500804–0x500888`.

Shared task names (exact string match): `fa_rob0`, `fa_rob1`,
`fa_camera`, `fa_game_info`, `fa_coli`, `fa_user`, `fa_effect`,
`fa_enemy0`, `fa_enemy1`, `fa_sampling`, `fa_pol_test`,
`fa_kill_osage`, `fa_game_disp`, `fa_sel_disp`, `fa_sound`,
`fa_control0`, `fa_record`, `fa_key_play`, `fa_key_record`,
`fa_play`. Shared entry helper family: `fa_tobi`, `fa_burni`,
`fa_sampling`, stage/effect selectors. STF-only: `fa_nameentry`,
`fa_stage_efc`, `fa_pol_string`; VF2-only: `fa_win0/1`,
`fa_selector0/1`, `fa_osage0/1`, `fa_object0/1/2`.

## 2. Shared Work-RAM window `0x5007xx–0x5008xx` (measured on VF2 side)

| STF `.ld` symbol | STF addr | VF2 task state / note |
| --- | --- | --- |
| `INTERUPT_FLAGS_MOMENTARY` | `0x500704` | VF2 attract nav-gate `0xa748`: bit 26/2 force selector 16 (v0362 note) |
| `fa_rob0` / `fa_rob1` | `0x500804` / `0x500808` | VF2 `fa_rob0/fa_rob1` states identical |
| `fa_camera` | `0x500814` | VF2 `fa_camera` state identical |
| `mod_fa_control0` | `0x50081c` | VF2 `fa_control0` state identical |
| `mod_fa_game_info` | `0x500824` | VF2 `fa_game_info` state identical |
| `mod_fa_coli` | `0x500828` | VF2 `fa_coli` state identical |
| `mod_fa_user` | `0x500830` | VF2 `fa_user` state identical |
| `mod_fa_game_disp/sel_disp` | `0x500834/0x500838` | VF2 identical |
| `mod_fa_enemy0/enemy1` | `0x50083c/0x500840` | VF2 `fa_enemy0 0x500844`, `fa_enemy1 0x500848` — **shifted by 2 slots** |
| `mod_fa_effect` | `0x500844` | VF2 `fa_effect 0x50084c` — shifted |
| `debug_flag` | `0x500800` region `0x508000` | VF2 board `0x508000` bit 9 gate (final-status path) |
| `focus_dist_x/y` | `0x501084/0x501088` | VF2 camera-scale globals `600.0f` at same addresses (v0375 note) |
| `win_eye*/win_*` | `0x501400–0x501416` | Same window family as VF2 viewport words |

Rule: identical addresses are still **hypotheses** until a VF2 probe
pins them; shifted enemy/effect slots prove the layouts are cousins,
not copies.

## 3. Hardware map (Model 2A vs 2B base shared)

| STF `.ld` | VF2 `model2a.h` | Match |
| --- | --- | --- |
| `ram ORIGIN 0x500000 len 1M` | `WORK_RAM_BASE 0x500000 SIZE 1M` | exact |
| `GEO_START 0x800000` | `GEOMETRY_BASE 0x800000` | exact |
| `BUFF_RAM 0x900000` | `BUFFER_RAM_BASE 0x900000` | exact |
| `COPRO_CONTROL1 0x980000` | `VIDEO_CONTROL_BASE 0x980000` | exact |
| `CPU_CONTROL 0xE00000` | `CPU_CONTROL_BASE` | exact |
| `IRQ_REQUEST 0xE80000` | `INTERRUPT_CONTROL_BASE` | exact |
| `TIMERS 0xF00000` | `TIMER_BASE` | exact |
| `TILE_DATA 0x1000000` | `TILE_RAM_BASE` | exact |
| `IO_PORTS 0x1C00000` | `IO_CONTROL_BASE` | exact |
| `BACKUP_RAM 0x1D00000` | `BACKUP_SRAM_BASE` | exact |
| `COPRO_SHARC_IOP 0x8C0000` | copro port family `0x880000` | nearby, unmapped |
| `TEXRAM0 0x11000000` | `TEXTURE_RAM0 0x12000000` | **differs** (2A vs 2B texture map) |

## 4. Boot parallel (static only)

STF `start_ip` (`rom_code1.s:49`): clear RAMBASE words, clear
`0x59D000` block, init TEXRAM/LUMA, move intr-table+PRCB to
`0x5FF000/0x5FF410`, `copro_down`, serial/IO/palette setup, register
interrupts, `b main` → `main_loop` (`:7191`), `dsp_exad` (`:15498`),
`init_event` (`:15370`). Same phase order as VF2
`boot_entry → task_registry_initialize (0x10cbc) → scheduler_dispatch
(0x10d54)`. No address is claimed equal beyond the RAMBASE/PRCB
pattern; VF2 boot is already natively recovered and MATCH-pinned.

## 5. Fighter/object hints (unproven — probe targets only)

- STF `rob_info_*` (`code_globals.S:1173–1211`:
  `action/automatic/command/en_flag/mot_kind/motd_*/smooth_*/skeleton_type/parts_weight/rob_weight`)
  suggests VF2 `fighter_candidate.h` probe offsets, but every
  `field_XXXX` still needs dual-fighter/width/IP evidence from
  `trace_case.py` + `infer_structs.py` on VF2 witnesses.
- STF fighter blocks `P1_* 0x510D00–0x513770`, `P2_* 0x514100–0x516B70`
  sit in the same Work-RAM upper region as VF2 live fighter cursors,
  but base addresses differ — do not transplant offsets.
- STF `ec_*` enemy-command bytes (`.ld:416–543`) parallel VF2 CPU
  decision unknowns; no logic is inferred from names.
- STF per-character move-name globals (`aAmy*/aKcs*/...`, ~3000
  symbols) have no VF2 counterpart in-tree and are ignored.

## 6. TGP/copro (static only)

STF `cpres1 0xB6318+0x741C`, `cpres2 0xBD748+0x490E` extracted from
program ROM into `cpres1/2.S`; `TGP_LoadDATA/POLYGON/TEXTURE` split
into `rom_data/pol/tex.bin`. Compare with VF2 `copro_data`,
`polygons.bin`, FIFO `0x00884000`, geo `0x800000`, and the recovered
`0x7c60` submitter — as question sources for missing opcodes/ports,
never as behavior.

## 7. Explicit non-matches (do not paper over)

- Task count 30 (STF) vs 29 (VF2); descriptor base differs
  (`0x510000`-family registry in VF2 recovered code vs STF event data
  in ROM).
- Enemy/effect state slots shifted (see §2).
- Texture map differs (§3); TGP program upload path unverified on STF
  side (no ROMs present).
- Entry points differ (`get_game_info rom_code1.s:21502`,
  `fa_enemy0:56977`, `ALL_INITIALIZE:95864` are STF ROM offsets, not
  VF2 addresses).

## 8. Allowed next uses

1. Candidate function splits for unrecovered VF2 corridors
   (e.g. `fa_enemy*`, `fa_tobi`, `fa_burni` bodies).
2. Candidate RAM probe addresses in `0x5007xx–0x5014xx` for
   `vf2probe --memory-trace` sessions.
3. Candidate fighter-field offsets for `infer_structs.py` ranking.
4. Nothing enters `src/recovered/` without a VF2 oracle measurement
   and a ROM-backed differential fixture.

## 9. Oracle verification appendix (measured 2026-09-19, ROM-backed)

All runs use `roms/vf2` (`vf2rom verify`: 36/36 OK) and the native
Windows build (`.\build.ps1 build` clean).

- `vf2i960 compare-task-registry roms/vf2` → **MATCH**: 29 recovered
  descriptors, table `0x00011dc0–0x00012500` (stride `0x40`), runtime
  registry `0x00510000–0x00516480`, 29 state pointers, 928 scratch
  bytes cleared, 647 interpreted instructions. Record format differs
  from STF (VF2: inline fixed `0x40` descriptors with
  flags/instance/stack/entry/state; STF: 3-long `{init,mod,0}` records
  + separate `event_names` table) — cousin layout, not a copy.
- `vf2i960 native-sixth-dispatch roms/vf2` → **MATCH**
  (8,675,721 continuous recovered instructions, 7,404,901 reference
  instructions compared, registry `0x00515200`).
- Shared-window addresses already pinned in VF2 recovered C with
  differential coverage: `0x00500804` (fighter0 slot,
  `camera_viewport.c`/`hybrid.c`), `0x00500814` (scheduler
  input/camera), `0x00508000` (board/runtime flags), copro FIFO
  `0x00884000` and function port `0x00880000` — all agreeing with the
  STF names/ports in §§2–3. `0x00500704` nav-gate and `0x00501084/88`
  camera scale are measured in the v0362/v0375 notes.
- Fighter bases differ: VF2 measured `0x00510980/0x00512980`
  (`0x2000` apart, `fighter_candidate.h`) vs STF `P1_* 0x00510d00` /
  `P2_* 0x00514100` (`0x3000` apart). Same `0x51xxxx` neighborhood,
  different exact bases — VF2 measured bases win; STF block starts
  are probe targets only.
- TGP program blobs are game-specific (STF `cpres` offsets vs VF2
  `copro_tgp_tables` opr-14742a/43); only the port family
  (FIFO/function/geometry bases) is shared. No DSP semantic is
  transferable.
