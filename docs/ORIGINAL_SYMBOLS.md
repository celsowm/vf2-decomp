# Shipped i960 symbol table

`decomp/i960/original_symbols.csv` carries **301 original Sega symbol names**
for the Virtua Fighter 2 Version 2.1 i960 program, recovered from the symbol
table that Sega left inside a later Model 2 port DLL.

Every name in this project up to now was invented. These are not.

## Provenance

Sega's Windows ports of the Model 2 titles keep the original board-side symbol
table inside the host DLL: a run of 16-byte records in `.rdata`, each one a
little-endian `u64` i960 program offset followed by a `u64` pointer to the
NUL-terminated C string that names it. Nothing in the DLL points at those
strings — they are only reachable through the table — so a string search over
the DLL finds the names and loses the addresses.

The recovered table is a single unbroken ascending run of 301 records at
`.rdata:0x18015dd40`, spanning `0xB0 start_ip` to `0x6DD4C check_sram_all`.

The DLL is not redistributable and is **not** in this repository. Only the
`(address, name)` pairs are committed, which is the same class of artifact as
the rest of `decomp/i960/*.csv`.

The table is inert. Nothing in the shipped DLL references those strings; they
are reachable only by walking the table. It is the port's own debugging
apparatus — the emulation layer keeps the board program's symbol table so its
diagnostics can put a name to a guest address — retained in a retail build
because nobody stripped it. Sega has shipped that layer across the 2012
PlayStation 3 and Xbox 360 Model 2 releases and the later Windows builds, and
each title in the family carries its own table. The copy read here came from a
Win64 build.

### This is not leaked source code, and not game data

Worth stating plainly, because a file of vendor-authored names invites the
assumption:

- **Published, not leaked.** The table ships in every retail copy. Reading it
  needs nothing but the product. No source tree, no internal build, no breach of
  confidence — the opposite of leaked source in every respect.
- **Not source code.** A symbol table is `(integer, identifier)` pairs. No
  statements, expressions, control flow, types, layouts, constants, comments or
  algorithms — nothing expressing *how* anything works. It is the same class of
  artifact as a PDB, a DWARF section, a linker map or an export directory.
  Knowing `0x18644` is `get_en_info` tells you what to call it and nothing about
  what it does; that is still measured, and in this repository it is.
- **Not game data.** Not a ROM, not a reconstructed region, not art, audio,
  models, textures or game strings. Nothing renderable, nothing playable, and no
  part of the ROM is reconstructible from it.

### Clean-room note

None of that makes the names independently derived. They are Sega's
identifiers, chosen by Sega's programmers, and this project did not arrive at
them on its own, so adopting them is a deliberate departure from strict
clean-room *naming*. It is recorded here and in `docs/LEGAL.md` rather than
buried: the names are evidence about *what the code is*, they do not describe
*how it works*, and no implementation was derived from them. Rule 1 of
`AGENTS.md` — do not invent names — argues for using them. If the project would
rather stay strictly clean-room, delete
`decomp/i960/original_symbols.csv` and the overlay call that loads it; nothing
else depends on this file.

## Regenerating and verifying

```sh
python tools/python/extract_original_symbols.py --dll <port.dll> --tables
python tools/python/extract_original_symbols.py --dll <port.dll> \
    --out decomp/i960/original_symbols.csv
python tools/python/extract_original_symbols.py --dll <port.dll> \
    --check decomp/i960/original_symbols.csv
```

`--check` exits non-zero on any disagreement and is how the committed file
stays honest without the DLL being in the tree. The table is found by shape,
not by address, so the script is not keyed to one build.

## How the analyzer uses it

`vf2_i960_apply_symbol_overlays` (`src/analysis/symbols.c`) now loads a fourth
overlay, `original_symbols.csv`, **after** `functions.csv`, `symbols.csv` and
`known_entries.csv`. It is applied last so it wins: a shipped name is evidence
and a repository name is a hypothesis. Records that name a label inside a
function rather than a function entry find no matching function and are
ignored, so `vf2i960 analyze` output is unaffected by them.

One address, `0xC45C`, carries two names in the shipped table (`INFO_DSP` and
`INFO_INT`, an empty state handler shared by both hooks). Both rows are kept;
the loader applies the second.

## Independent corroboration

The table was not used to derive any function boundary in this repository, so
the agreements below are a real cross-check of prior work:

- **12 of the 76** distinct function `end` addresses in `decomp/i960/functions.csv`
  — derived from differential execution, with no access to this table — land
  *exactly* on a shipped symbol start: `0xBC0 VsyncScr`, `0x9FB0 main_loop`,
  `0x10D54 event_loop`, `0x16504 calc_unit_mat`, `0x1EFF0 calc_rob_light`,
  `0x1FCC0 change_scene`, `0x4BCD4 unp_send_tex_para_sub`,
  `0x4C180 unpack_lod_data`, `0x4CB64 send_lod_data`, `0x4CD18 send_lod_data_q`,
  `0x4D2C0 dsp_send_msg`, `0x6428C osage_dsp`.
- **11 of the 29** scheduler task entry points in `decomp/i960/tasks.csv`, read
  out of ROM descriptors, match a shipped symbol exactly — including every
  `*_init` entry: `fa_game_info → get_game_info`, `fa_osage0/1 → osage_init`,
  `fa_control0 → control_init`, `fa_selector0/1 → select_init`,
  `fa_sampling → sampling_init`, `fa_key_record → key_rec_init`,
  `fa_key_play → key_play_init`, `fa_record → rec_init`, `fa_play → play_init`.
- Not one shipped symbol contradicts a recovered *boundary*. Every
  disagreement below is a disagreement about meaning, not about where a
  function starts.

## Name adoption

The table names 34 addresses that already carry a repository name. All 34
differ, because all 34 repository names were invented. The shipped name should
replace the provisional one everywhere.

| address | repository name | shipped name |
| --- | --- | --- |
| `0x000000b0` | `boot_entry` / `vf2_boot_entry` | `start_ip` |
| `0x00000530` | `frame_shadow_verify` | `make_lay_col_256_tbl_demon` |
| `0x00000b58` | `iac_reinitialize_candidate` | `move_data` |
| `0x00000bc0` | `interrupt_save_prefix` | `VsyncScr` |
| `0x00001064` | `video_register_compose` | `read_sw` |
| `0x00001290` | `video_input_latch_write` | `write_sw` |
| `0x000012d8` | `input_ring_poll` | `read_data_bd_fifo` |
| `0x0000281c` | `game_state_classify` | `chute_setting_check` |
| `0x00009fb0` | `main_clear_prefix` | `main_loop` |
| `0x0000a6c0` | `frame_dispatch_tick` | `mode_control` |
| `0x0000a748` | `frame_geometry_gate` | `test_sw_chk` |
| `0x00010cbc` | `task_registry_initialize` | `init_event` |
| `0x00010d54` | `scheduler_dispatch` | `event_loop` |
| `0x00010f08` | `frame_timer_prefix` | `interrupt_wait_b` |
| `0x000110b0` | `frame_buffer_gate` | `start_check` |
| `0x000110f4` | `video_input_sync` | `debug_sw_check` |
| `0x000112f8` | `frame_counter_advance` | `variable_diff_calc` |
| `0x0001645c` | `task_game_info` | `get_game_info` |
| `0x0001eff0` | `camera_project_fighter_ranges` | `calc_rob_light` |
| `0x0001f148` | `camera_state_reset` | `game_camera_init` |
| `0x0001facc` | `camera_fill_viewport_table` | `get_ikada_poly` |
| `0x0001fbb4` | `camera_range_window` | `get_ikada_kage_pos` |
| `0x0001fc00` | `camera_mode_dispatch` | `send_lay_realtime` |
| `0x0001fcc0` | `display_profile_apply` | `change_scene` |
| `0x0001fee4` | `display_profile_unit_fill` | `set_default_rob_light` |
| `0x000214dc` | `camera_classify_range` | `area_check` |
| `0x0004b410` | `video_command_submit` | `send_tex_stage` |
| `0x0004bb18` | `texture_orchestrator_save_call` | `unp_send_tex_para` |
| `0x0004bcd4` | `texture_frame_gate_call` | `unp_send_tex_para_sub` |
| `0x0004c180` | `texture_header_decode` | `unpack_lod_data` |
| `0x0004cb64` | `texture_word_prepare` | `send_lod_data` |
| `0x0004cd18` | `texture_color_prepare` | `send_lod_data_q` |
| `0x0004d2c0` | `texture_status_line` | `dsp_send_msg` |
| `0x000640f4` | `task_osage` | `osage_init` |

## Where the shipped name changes the meaning

Most rows above are a better label for the same understood behavior. These are
the ones where the provisional name asserts something the shipped name
contradicts. Each is a hypothesis to re-test, not a fact to write down.

- **`0x0000a748` `frame_geometry_gate` is `test_sw_chk`.** This is the
  test-switch check, not a geometry gate. That reframes the unobserved branch
  tracked in `decomp/i960/notes/frame_geometry_gate_deep_reset_v0203.md`: its
  "deep reset" chain ends at `0x000000b0`, which the table names `start_ip`, and
  passes `0x0006116c`, which sits between `sram_clear_for_game_assign`
  (`0x6010C`) and `sram_clear_for_coin_assign` (`0x60330`). The rejected path is
  very likely *entering the operator test menu*, which reboots the board. It
  should stay rejected until measured, but it should be re-tested as a test-mode
  entry rather than as a geometry reset.
- **`0x000112f8` `frame_counter_advance` is `variable_diff_calc`**, and the
  previously unnamed `0x0001128c` is `variable_diff_init`. This is the variable
  difficulty calculation, not a frame counter. Its "phase mask" is a difficulty
  input.
- **`0x000110b0` `frame_buffer_gate` is `start_check`** and **`0x000110f4`
  `video_input_sync` is `debug_sw_check`.** With `variable_diff_init`,
  `variable_diff_calc` and `set_game_setting` (`0x113F4`) they form one
  contiguous switch/setting cluster, not the frame-buffer and video work the
  provisional names describe.
- **`0x00001064` `video_register_compose` is `read_sw`**, **`0x00001290`
  `video_input_latch_write` is `write_sw`**, and **`0x000012d8`
  `input_ring_poll` is `read_data_bd_fifo`** — one of the
  `init_data_bd_fifo` / `read_data_bd_fifo` / `write_data_bd_fifo` triple at
  `0x12BC`/`0x12D8`/`0x1314`. These are switch I/O and the data-board FIFO, not
  video registers and not an input ring.
- **`0x0000281c` `game_state_classify` is `chute_setting_check`.** Its
  neighbours are `check_credit` (`0x1610`) and `coin_current_clear` (`0x245C`).
  This is coin-chute configuration. The nearby provisional cluster
  `game_color_lookup` (`0x26EC`), `game_threshold_evaluate` (`0x28D4`) and
  `game_meter_update` (`0x20F0`) is therefore very likely coin/credit
  bookkeeping — which fits the recorded "two-meter update" and the
  `0x00010101` adjustment far better as packed byte counters than as color.
- **`0x0001eff0` `camera_project_fighter_ranges` is `calc_rob_light`**, with
  `calc_rob_light_sub` at `0x1F03C`. It computes fighter lighting, not a
  projection of fighter ranges.
- **`0x0001fee4` `display_profile_unit_fill` is `set_default_rob_light`.** The
  recorded behavior — twenty-six `1.0f` entries at `0x0050A0E0` — is the default
  fighter light block. `0x0050A0E0` is the light state, and the provisional
  `display_profile_mode_constants` (`0x1FF0C`) and `display_color_profile_apply`
  (`0x1FFFC`) sit inside the same lighting region.
- **`0x0001facc` `camera_fill_viewport_table` is `get_ikada_poly`** and
  **`0x0001fbb4` `camera_range_window` is `get_ikada_kage_pos`.** *Ikada* is the
  raft stage and *kage* is shadow: these fetch raft polygon data and raft shadow
  position. Neither is a viewport helper.
- **`0x0004c180` `texture_header_decode` is `unpack_lod_data`**, **`0x0004cb64`
  `texture_word_prepare` is `send_lod_data`** and **`0x0004cd18`
  `texture_color_prepare` is `send_lod_data_q`.** This corridor moves
  level-of-detail model data, not textures. The decode tree the repository
  recovered at `0x0004c544` is confirmed as Huffman by `make_huf_8bit`
  (`0x4C928`) immediately after it.
- **`0x0004d2c0` `texture_status_line` is `dsp_send_msg`** — a message to the
  DSP rather than a status-line writer.
- **`0x0004b410` `video_command_submit` is `send_tex_stage`**, one of a family
  with `send_tex_default` (`0x4B020`), `send_tex_efc` (`0x4B820`) and
  `send_tex_rob` (`0x4B838`). The recorded "selector 3" argument is the stage
  variant of a shared texture-send leaf.

## Addresses the project already studies that now have a name

The table names 118 addresses the repository refers to but had never named. The
most heavily referenced:

| address | shipped name | files referencing it |
| --- | --- | --- |
| `0x00018644` | `get_en_info` | 121 |
| `0x0001d458` | `camera_control` | 14 |
| `0x00018144` | `get_my_info` | 14 |
| `0x0000acf8` | `ADV_DSP` | 13 |
| `0x00001344` | `player_entry` | 12 |
| `0x00002edc` | `set_end_mark` | 11 |
| `0x00000f7c` | `interrupt_wait` | 10 |
| `0x0004ce88` | `send_lod_data_q_sub_norm` | 9 |
| `0x0004c928` | `make_huf_8bit` | 9 |
| `0x0004b9b8` | `unp_send_tex_req` | 9 |
| `0x00017710` | `rob_revise_yang` | 9 |
| `0x00016504` | `calc_unit_mat` | 9 |
| `0x0000ae78` | `ADV_RECYCLE_PIC_INT` | 9 |
| `0x00008f1c` | `dsp_pattern` | 9 |
| `0x00007fc0` | `print_mes` | 9 |
| `0x00001200` | `seclet_command_check` | 9 |
| `0x00019ef8` | `set_motion` | 8 |
| `0x0001791c` | `rob_spd_control` | 8 |
| `0x0004b640` | `face_mot_control` | 7 |
| `0x000180bc` | `action_after` | 6 |
| `0x00028780` | `get_start_value` | 6 |
| `0x0006dd4c` | `check_sram_all` | 6 |

Two of these matter more than the rest:

- **`0x00018644` is `get_en_info` — "get enemy info".** It is the single most
  studied address in the repository (121 files, the whole `game_info_18644`
  note series and the `v0122`–`v0247` state-mask work). Its sibling
  `0x00018144` is `get_my_info`, and the task-level `0x0001645c` is
  `get_game_info`. The three-way split is *game* state, *own* fighter state and
  *enemy* fighter state. The `0x18644` state masks under investigation are
  therefore enemy-fighter queries, and the `0x18144`/`0x18644` pair should be
  expected to be near-mirror routines.
- The measured `0x14400`–`0x14418` call chain in `docs/NATIVE_RUNTIME.md` sits
  inside `rob_action` (`0x142F4`, ending before `decide_command` at `0x1442C`),
  and every one of its targets is now named: `0x14400 → rob_revise_yang`,
  `0x14404 → rob_spd_control`, `0x14408 → face_mot_control`,
  `0x14414 → calc_unit_mat`, `0x14418 → action_after`. The corridor the
  `PLAYER_*_RECOVERY.md` documents describe is the per-fighter action step, and
  its five stages have names.

The state-machine skeleton is also fully named now: `WARNING`, `ADV_*`, `INFO`,
`SEL`, `GAME`, `ROUND_MASK`, `ROUND`, `SET`, `READY`, `FIGHT`, `JUDGE`,
`REPLAY`, `VIC`, `INTRUDE`, `CONTINUE`, `OVER`, `ENDING`, `ENDSUB`, `NAME`,
`TEST` and `RANK`, each with an `_INT` (enter) and `_DSP` (display/update)
handler between `0xA804` and `0x10C48`. `mode_control` (`0xA6C0`, 79 references,
provisionally `frame_dispatch_tick`) is the driver of that machine, which is a
much stronger reading of it than "frame dispatch tick".

## Not yet done

The names are committed as evidence and wired into the analyzer, but the C
identifiers in `src/recovered/`, `include/vf2/recovered.h`, `tests/` and
`decomp/i960/{symbols,functions}.csv` still carry the provisional names. That
rename is mechanical but must clear the full build and test gate in `AGENTS.md`,
so it belongs in its own change. Suggested order:

1. rename in `decomp/i960/symbols.csv` and `decomp/i960/functions.csv`;
2. rename the C identifiers and their tests, one subsystem at a time;
3. re-test the four items in *Where the shipped name changes the meaning* that
   assert a different subsystem — `test_sw_chk`, `variable_diff_calc`, the
   coin/credit cluster and the LOD corridor — before renaming anything that
   depends on their semantics;
4. rename the address-keyed notes and `PLAYER_*_RECOVERY.md` documents last, so
   the git history of the address-keyed work stays greppable in between.

The table covers 301 addresses. It is not the whole program: it is the subset
of labels the porting team needed, and it has gaps (nothing between `0x2F5C`
and `0x7B18`, `0x33480` and `0x39C40`, or `0x53CB8` and `0x6010C`). An address
absent from the table is not evidence that it is not a function.
