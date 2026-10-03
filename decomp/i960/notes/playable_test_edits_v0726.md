# TEST+LEFT/RIGHT per-row edits on rows 7-14 and post-edit releases native (v0726)

## Result

The edit family completes: TEST+LEFT and TEST+RIGHT on every settings
row 7-14 run fully native under strict per-block differential (all
MATCH to `0x0a010`/`0x1645c`), and every post-edit release frame
MATCHes through its per-cursor release arm.

| frame | input / prev | nav | rows | measured total |
| --- | --- | --- | --- | --- |
| TEST+RIGHT edit | `0x0f008004` / `0x0f000000` | `0x8004` | 7,8,9,12,13,14 (packed) | 4943 |
| TEST+LEFT edit | `0x0f004004` / `0x0f000000` | `0x4004` | 7,8,9,12,13,14 (packed) | 4945 |
| TEST+RIGHT/LEFT edit, row 10 | same shapes | | 10 (country) | 4961 / 4963 |
| TEST+RIGHT/LEFT edit, row 11 | same shapes | | 11 (special) | 18421 / 18423 |
| post-edit release | `0x0f000000` / edit-input / matching rel | `0` | 8,10,12,14 | 4575 |
| post-edit release | same | `0` | 7,9,13 | 4578 |
| post-edit release, row 11 | same | `0` | 11 | 16116 (prefix rebuild frame) |

16 edit frames + 16 release frames, all MATCH. Row-4/5/6 edits are
unreachable (rows are advance-skipped by construction); rows 0-3 and
15 are not edit rows (header/DIFFICULTY/country-name rows and EXIT).

## Measured oracle behavior

- **LEFT == RIGHT**: both directions take the identical `edit_delta>0`
  branch; only the latch values differ. Packed rows run the 3412 body,
  row 10 the 3427 body, row 11 the 16890 body (bodies probed in the
  v0726 fail-closed captures: `out/ga-edL7-fail`, `ga-edL10-fail`,
  `ga-edL11-fail`).
- **CC**: edit frames finish with CC LESS (`set_main_final_cluster_condition`
  exception for `0x0f008004`/`0x0f004004`).
- **Machinery blocks** pin live g1 at 0x5ff684 (test_edit_entry live
  value else 0x7ae10) and take the post-finish overrides
  (r9=0xffffffff, r14=counter-1, r15=0x8a00, g6 live).
- **Post-edit release, rows != 11**: the cursor gate's release arm
  (latch triple `0x0f000000`/edit-input/`0x8004`|`0x4004`).
- **Post-edit release, row 11**: the prefix rebuild frame via
  `test_edrel11_entry` (14815 body = 232 + 11538 rebuild + 3045; the
  shadow-sync rebuild at 0x5e8-0x738 fires because the edit changed a
  level byte at 0x5000e0-0x5000e2).
- **0x5ff600**: generic index4 write stores the whole g4 (r20) u32
  each frame (`vf2_model2a_write_u32(machine, 0x005ff600, r20)`).

## v0725 up8 gate-ordering regression (found and fixed in v0726)

During the v0726 validation sweep the 7->3 UP skip frame failed the
strict differential with `Endurance stopped: unsupported operation`,
reference at `0x0005a7b8` vs native `0x0000a010`, both sides at 4343
instructions. Root cause: `test_up8_entry` had been added to the UP
single-step predicate in `execute_frame_phase17_bit7_index4`, which is
evaluated before the dedicated 7->3 skip branch — so the skip branch
was dead code and the frame was charged 232+3047=3279 instead of
232+3065=3297. The reference was advanced 18 steps short and stopped
mid-loop at 0x5a7b8.

Evidence chain (all from `out/ga-up73-fail.vf2snap`, state-identical
to the v0725 `out/ga-up7-fail.vf2snap` capture):

- pure probe 0x9ff8->0xa010: exactly 3297 steps (with and without
  `--input 0x20001`), 0x9ff8->0x1645c: 3532 steps;
- vf2cycles from that checkpoint charged 3279 for the block, exposing
  the native-side charge error;
- expected full frame: 1064 pre-block + 3297 + 235 tail = 4596 —
  exactly the v0725 recorded MATCH.

Fix: remove `test_up8_entry` from the single-step predicate. After the
fix the 7->3 frame MATCHes again (4596 instructions, 37 blocks, final
`0x1645c`), the regenerated output snapshot + runtime sidecar are
bit-identical to the v0725 `ga-up7-native` pair (md5
`ea779c97962df89c293a6cdc014ab25a` / `a238411f10c3dac010582a46bfa0c521`),
and the release+idle chain MATCHes (9150). The v0725 validation record
itself was accurate; the regression had been introduced between
validation and the v0725 commit and masked by a stale build.

## Validation

- 16 edit frames (rows 7-14 x L/R): MATCH (totals above).
- 16 post-edit release frames: MATCH (totals above).
- 7->3 UP skip re-proof: MATCH 4596, output bit-identical to v0725.
- 7->3 release+idle chain: MATCH.
- Strict regression suite: 10/10 passed
  (`phase17_zero|texture_bridge_differential|native_sixth_dispatch|
  native_runtime$|native_runtime_state|native_differential|
  post_boot_input_profiles|texture_status|texture_counter|
  texture_upload`).
