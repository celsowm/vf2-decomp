# Changelog

## Unreleased

- game_info positive: generalize 7×112 bit21 low masks to full middle-high set `0x1B7E3EA9` (20 bits, ~1M combos per base) with same per-base `−3/−5`/`+2/+5`/`+4/+8`/`+4/+9` accounting — representative 40 single/multi-bit combos `36/36 exact` (v0254, `decomp/i960/notes/game_info_18644_positive_middle_high_v0254.md`);
- game_info positive: admit bare middle-high variants — remove `low !=0` guard so any middle `0x1B7E3EA9` with `outer 16` and `low 8` (incl. bare) admitted, same accounting (v0255, `decomp/i960/notes/game_info_18644_positive_middle_high_bare_v0255.md`);
- game_info positive: admit 672 base-bit21 low variants (`0x8140` `−3/−5` plus `0xC140` `+2/+5`, `0x4140`/`0x14140` `+2/+4`, `0x10140` `+4/+8`+bit11, `0x18140` `+4/+9`+bit11, `0x1C140` `+2/+5`) — six `16×7` low cubes with mandatory high `0x00200000`, total `1223→1895` positive masks (v0247–v0253, `decomp/i960/notes/game_info_18644_positive_bit21_low_v0248.md`);
- compacted the positive state-8 `0x8140`/`0x10140`/`0x18140` low-bit cubes from
  twelve explicit `pair ==` blocks to three `& ~0x16 == base` predicates
  (each covering its 8 low variations over bits 1,2,4). The generic
  `fighter+0x1a4` bit-11 write now triggers for any low variant, fixing the
  `0x10144`/`0x18144` single-mask `0/36` outlier. All `24` masks remain
  `36/36 exact` with identical `+5/-3`, `+8/+4` and `+9/+4` accounting and
  stale-frame postconditions (v0217,
  `decomp/i960/notes/game_info_18644_positive_compact_low_v0217.md`);
- compacted the positive state-8 `0xC140`/`0x1C140` low-bit cubes from four
  explicit `4-mask ==` blocks to one `& ~0x10016 == 0xC140` predicate
  covering all 16 low variations (both bases with/without bit 16, each with
  8 low combos). All `16` masks remain `36/36 exact` with `+5/+2` accounting
  (v0218, `decomp/i960/notes/game_info_18644_positive_compact_low_v0218.md`);
- compacted the positive state-8 `0x4140`/`0x14140` low-bit cubes from eight
  explicit `pair ==` blocks to one `& ~0x10016 == 0x4140` predicate
  covering all 16 low variations (both bases with/without bit 16). All
  `16` masks remain `36/36 exact` with `+4/+2` accounting (v0219,
  `decomp/i960/notes/game_info_18644_positive_compact_low_v0219.md`);
- extracted the duplicated stale-frame postcondition for low cubes into
  `hybrid_set_stale_low()` — 13 identical `r3/r4/r7/r8/r12/r13/r14/r15`
  blocks replaced by one helper, saving 260 lines with identical
  `LESS/EQUAL` behavior (v0220);
- admitted the positive state-8 `0x04008140` (high-26 + bits 6+14+15) low
  cube over bits 1,2,4 — 8 masks `0x04008140|low` each `36/36 exact` with
  `-5/-3` accounting and `hybrid_set_stale_low` (v0221,
  `decomp/i960/notes/game_info_18644_positive_high26_8140_low_v0221.md`);
- admitted the positive state-8 `0x20008140` (high-29 + bits 6+14+15) low
  cube over bits 1,2,4 — 8 masks `0x20008140|low` each `36/36 exact` with
  `-5/-3` (v0222,
  `decomp/i960/notes/game_info_18644_positive_high29_8140_low_v0222.md`);
- admitted the positive state-8 `0x40008140`/`0x80008140` (high-30/31 + bits
  6+14+15) low cubes over bits 1,2,4 — 16 masks each `36/36 exact` with
  `-5/-3` (v0223,
  `decomp/i960/notes/game_info_18644_positive_high30_31_8140_low_v0223.md`);
- admitted the positive state-8 `0x04010140` (high-26 + bits 6+14+16) low
  cube over bits 1,2,4 — 8 masks `0x04010140|low` each `36/36 exact` with
  `+8/+4` plus bit11 (v0224,
  `decomp/i960/notes/game_info_18644_positive_high26_10140_low_v0224.md`);
- admitted the positive state-8 `0x40010140`/`0x80010140` (high-30/31 + bits
  6+14+16) low cubes over bits 1,2,4 — 16 masks each `36/36 exact` with
  `+8/+4` plus bit11 (v0225,
  `decomp/i960/notes/game_info_18644_positive_high30_31_10140_low_v0225.md`);
  `0x20010140` (high-29) remains `0/36` with `+2` diff and stays
  fail-closed;
- admitted six high-pair masks for base `0x8140` over highs 26,29,30,31
  (`0x24008140`, `0x44008140`, `0x84008140`, `0x60008140`, `0xA0008140`,
  `0xC0008140`) — each `1` mask `36/36 exact` with `0` excess and
  `hybrid_set_stale_low`, low variants `|0x02` remain `0/36` (v0226,
  `decomp/i960/notes/game_info_18644_positive_high_pairs_8140_v0226.md`);
- admitted five high-triple/quad masks for base `0x8140`
  (`0x64008140`, `0xA4008140`, `0xC4008140`, `0xE0008140`, `0xE4008140`)
  each `1` mask `36/36 exact` with `0` excess (v0227,
  `decomp/i960/notes/game_info_18644_positive_high_triples_quad_8140_v0227.md`);
- admitted eleven high-pair/triple/quad masks for base `0x10140`
  (6 pairs +4 triples +1 quad, `0` excess +bit11, v0228,
  `decomp/i960/notes/game_info_18644_positive_high_pairs_triples_10140_v0228.md`);
- admitted eleven high-pair/triple/quad masks for base `0xC140`
  (6 pairs +4 triples +1 quad, `0` excess, v0229,
  `decomp/i960/notes/game_info_18644_positive_high_pairs_triples_C140_v0229.md`);
- admitted 32 high masks for bases `0x18140`/`0x1C140`/`0x14140`
  (11+11+10, `0` excess, v0230,
  `decomp/i960/notes/game_info_18644_positive_high_bulk_v0230.md`);
- admitted 77 low-variant masks for high `0x8140` family
  (11 bases ×7 low, `-5/-3` excess, v0231,
  `decomp/i960/notes/game_info_18644_positive_high_8140_low_variants_v0231.md`);
- admitted 77 low-variant masks for high `0xC140` family
  (11 bases ×7 low, `+5/+2` excess, v0232,
  `decomp/i960/notes/game_info_18644_positive_high_C140_low_variants_v0232.md`);
- admitted 84 low-variant masks for high singles `0x14140`/`0x18140`/`0x1C140`
  (4 bases ×7 ×3, `+4/+9/+5`, v0233,
  `decomp/i960/notes/game_info_18644_positive_high_singles_low_variants_v0233.md`);
- admitted 8 base low-0 masks for high singles `0x18140`/`0x1C140`
  (`0` excess +bit11, v0234,
  `decomp/i960/notes/game_info_18644_positive_high_singles_base_v0234.md`);
- admitted the positive state-8 `0x20010140` (high-29 + bits 6+14+16) low
  cube over bits 1,2,4 — 8 masks `0x20010140|low` each `36/36 exact` with
  `+6/+3` plus bit11 and `fighter+0x6da=0x1e` for bit29 (v0235,
  `decomp/i960/notes/game_info_18644_positive_high29_10140_low_v0235.md`);
- admitted 77 low-variant masks for high `0x10140` family
  (11 bases ×7 low, v0236,
  `decomp/i960/notes/game_info_18644_positive_high_pairs_10140_low_v0236.md`) — 7 bases with bit29 `+6/+3` +`0x1e`, 4 bases `+8/+4`;
- admitted 77 low-variant masks for high `0x18140` family
  (11 bases ×7 low, v0237,
  `decomp/i960/notes/game_info_18644_positive_high_pairs_18140_low_v0237.md`) — 7 bases with bit29 `+7/+3` +`0x1e`, 4 bases `+9/+4`;
- admitted 147 low-variant masks for high `0x1C140`/`0x14140` families
  (11×7 + 10×7, v0238,
  `decomp/i960/notes/game_info_18644_positive_high_pairs_1C140_14140_low_v0238.md`) — `+5/+2` and `+4/+2`;
- admitted 28 low-variant masks for high `0xC140` singles
  (4 highs ×7 low, v0239,
  `decomp/i960/notes/game_info_18644_positive_high_singles_C140_low_v0239.md`) — `+5/+2`;
- admitted 7 low-variant masks for the `0x14140` quad `0xE4014140`
  (`+4/+2`, v0240, `decomp/i960/notes/game_info_18644_positive_14140_quad_low_v0240.md`) — `0x14140` now `11×7=77`;
- admitted 8 masks for high quad `0xE4000140` (`+0` base / `-3/-6` low, v0241,
  `decomp/i960/notes/game_info_18644_positive_E400_140_low_v0241.md`);
- admitted 35 low-variant masks for base `0x140` singles (`-3/-6`, v0242), 70 for pairs (`-3/-6`, v0243) and 98 for triples/quads (`-3/-6`, v0244) — base-no-low and quint remain `0/36`;
- closed base `0x140` high-family to `248/248` via 30 bases without low with `cd0/m1` split (`f0 +8/+3, f1 +4/+3, bi +7/+6`, v0245) and quint low 7 masks with `cd` split (`cd0 +8/+11, cd1 +3/+6`, v0246, `decomp/i960/notes/game_info_18644_positive_base140_full_v0245.md`) — `1074→1111`;
- admitted 112 low-variant masks for base `0x8140` with mandatory high-21 (16 outers x7 lows, uniform `-5/-3` with `LESS/EQUAL` + `hybrid_set_stale_low`, v0247, `decomp/i960/notes/game_info_18644_positive_8140_bit21_low_v0247.md`) — `1111→1223`;
- admitted the positive state-8 `0x40010140`/`0x80010140` (high-30/31 + bits
  6+14+16) low cubes over bits 1,2,4 — 16 masks each `36/36 exact` with
  `+8/+4` plus bit11 (v0225,
  `decomp/i960/notes/game_info_18644_positive_high30_31_10140_low_v0225.md`);
- recovered the full texture-orchestrator limit cluster at `0x0004bfe0`:
  `bbs` with source-mask `0xc0`/`0xc000`/`0x0c` tests `display_mode %32`,
  `cmpobe` for `12`/`13`, `bbs 16` for `0x00500068` bit 16,
  plus `0x00500064` (`6`/`8`) and `0x00500031 <8` for `mode 9` —
  six supported limit pairs `0x3e80/0x4e20`, `0x4330/0`, `0/0x4e20`,
  `0x4330/0x4e20`, `0x12a8/0x4330`, `0x32c8/0x4e20`
  (skip `2,3 mod32` remains `VF2_ERROR_UNSUPPORTED` with no store);
  synthetic snapshots at `0x4bfe0` swept `display_mode 0..255 × runtime bit16`
  via `vf2probe --rom-dir D:/ia/vf2-decomp/roms/vf2 --until 0x0004c11c --read-u32`
  (512 cases: 258 `0/0x4e20`, 16 `0x12a8/0x4330`, 16 `0x32c8/0x4e20`,
  1 `0x4330/0`, 205 `0x3e80/0x4e20`, 16 unsupported skips, plus
  `mode 9` extra-field matrix); `vf2_orchestrator_limits_tests` now locks the
  full 512-case matrix and the secondary `0x50064`/`0x50031` branches;
  `vf2_orchestrator_select_limits`/`apply` and the hybrid bridge at `0x4bfe0`
  now report measured instruction equivalents (8/11/13/15/18/22/25/31) —
  see `decomp/i960/notes/orchestrator_limits_full_v0200.md`;

- removed the spurious `0x0055c2f0 >= 1` guard in the `0x0004bb98` counter2 (`0x005502e0 == 1` via `0x0004b44c`) expiry: the i960 never consults `VF2_TEXTURE_STATUS_WORD` on this corridor — reference reaches `0x0004bc58` with the same 42 instructions for `status_word` in `{0,1,0xFFFF}` and for all 27 Cartesian counter values `{0,1,2}`; out-of-range texture numbers (`>0x56`) via `0x0004b934`/`0x0004b9b8` reach `0x0004bc58` in 376 instructions (double diagnostic) — see `decomp/i960/notes/texture_counter_status_word_v0198.md`;
- recovered the counter2 (`0x005502e0` via `0x0004b44c`) out-of-range texture diagnostic: for `argument0 > 0x56` the publisher at `0x0004b9b8` now renders the `tex num error` diagnostic (6 numeric cells at `0x01000064` + 13 literal cells at `0x01000072`), skips the `0x00550288` record publication and continues through the `0x0004ba70` queue helper for 198 instructions / 5 calls / 5 returns; 21/21 exact ROM-backed snapshots (`0x0004bb98`..`0x0004bc58`, 7 texture values × 3 `argument1` values) — see `decomp/i960/notes/texture_counter2_diagnostic_v0197.md`;
- recovered the texture-number diagnostic at the record publisher `0x0004b9b8`: values `> 0x56` no longer fail before the helper but render the signed value plus `tex num error` into tile RAM (19 cells / 38 bytes, `g0=0x72`/`g9=0x010000e4`, stale frame `r3=0x56`/`r4=0x400ccccd`/`r14=0x0004b9e4`/`r15=0x01000064`, 160 instructions / 2 calls / 2 returns per diagnostic, second publisher still evaluated with wraparound); 10/10 focused + 42/42 counter0/counter1 matrix snapshots exact, `vf2_texture_bridge_differential` passed — see `decomp/i960/notes/texture_number_diagnostic_v0196.md`;
- recovered the positive state-8 `fa_game_info` bit-14 + high-26/high-29 mask `0x24004140`; all 12 fighter-distribution/countdown/mode-bit-6 fixtures match the original i960 exactly in snapshot state and instruction/call/return accounting, while the other high-bit pairs remain fail-closed;
- attributed ROM range `0x00065838..0x000658a0` to the existing clean-room `kill_osage_evaluate_record()` semantic helper; the continuation/flag gate, `record+0x128` age accumulation, `0x4268` threshold, bit-3 mark/clear behavior and `0x00500164` kill counter match the i960 block exactly, closing the first unattributed edge in a fresh sixth-dispatch frontier trace;
- extended the positive-threshold `fa_game_info` `0x18644` state-8 bit-6
  recovery with the measured `0x154` (bit 8 + bits 2 + 4 + 6) composition;
  all three physical distributions, both countdown values and both mode-bit-6
  settings match the ROM with exact CPU, mutable-memory and
  instruction/call/return state;
- extended the positive-threshold `fa_game_info` `0x18644` state-8 bit-6
  recovery with the measured `0x146` (bit 8 + bits 1 + 2 + 6) composition;
  all three physical distributions, both countdown values and both mode-bit-6
  settings match the ROM with exact CPU, mutable-memory and
  instruction/call/return state;
- extended the positive-threshold `fa_game_info` `0x18644` state-8 bit-6
  recovery with the measured `0x150` (bit 8 + bits 4 + 6) composition; all
  three physical distributions, both countdown values and both mode-bit-6
  settings match the ROM with exact CPU, mutable-memory and
  instruction/call/return state;
- extended the positive-threshold `fa_game_info` `0x18644` state-8 bit-6
  recovery with the measured `0x144` (bit 8 + bits 2 + 6) composition; all
  three physical distributions, both countdown values and both mode-bit-6
  settings match the ROM with exact CPU, mutable-memory and
  instruction/call/return state;
- extended the positive-threshold `fa_game_info` `0x18644` state-8 bit-6
  recovery with the measured `0x140` (bit 8 + bit 6) and `0x142` (bit 8 +
  bits 1 + 6) fighter-state compositions; all three physical distributions,
  both countdown values and both mode-bit-6 settings match the ROM with exact
  CPU, mutable-memory and instruction/call/return state;
- added `tools/python/frontier.py`, the initial queryable guest-i960 recovery
  frontier: it ingests `explore_state.py` corpus manifests, `sweep_state.py`
  JSONL sweeps and `vf2probe --trace` JSONL streams, ranks candidate edges by
  measured witnesses, reproducible `.vf2snap` availability, unsupported-final
  counts and recovered-range attribution from `decomp/i960/functions.csv`,
  and supports `--exclude-recovered` plus stable `--json` output;
  aggregation is streaming so memory use scales with distinct edges rather
  than trace length; standalone unit tests live in
  `tools/python/test_frontier.py`;
- expanded `fa_game_info` state-bit-8 recovery at `0x00018644`: isolated
  bit8+bit1 and bit8+bit4 remain recovered; bilateral bit8 matches the existing
  zero/nonzero-countdown matrices; the asymmetric bilateral bit8+bit4 state
  (`0x100/0x110` in either physical orientation) was revalidated from the real
  `fighter+0x1a4` state source at both `0x18644` call sites, with exact CPU,
  mutable-memory, instruction, call and return equality; bilateral both-bit4
  (`0x110/0x110`) matches 202/207 caller-to-task instructions with mode bit 6
  clear/set for either countdown state, while bilateral both-bit1
  (`0x102/0x102`) matches 232/237 with zero countdown and 208/213 with nonzero
  countdown; unrelated unmeasured mixed-extra-state combinations remain
  explicitly fail-closed;
- completed selector 3's phase table: phases 16 (`0x0000c414`) and 17
  (`0x0000c448`) are recovered, closing the last two entries of the
  eighteen-entry table at `0x0000aac4`. Phase 16 decrements the task
  countdown at `[0x00500834]+0x50`, staying on the phase for a measured
  34-instruction tick and advancing the phase byte to 17 through its
  three-instruction zero epilogue (37 instructions); phase 17 clears the
  phase byte to zero and wraps the cycle in 31 instructions. Controlled
  native-versus-reference runs from a natural `0x0000a6c0` snapshot match
  exactly — instruction/call/return accounting (3 calls / 4 returns per
  tick), complete CPU condition state and every mutable memory region —
  with the harness calibrated against the published phase-8 (37/3/4) and
  phase-11 (34/3/4) corridors first;
- fixed a pre-existing master regression in the post-boot input-profile
  differential exposed by re-running it: three recovered blocks left stale
  comparison state after the reference executor learned the architectural
  condition effects of compare- and bit-branch instructions. The
  input-profile entry now tracks the last executed compare/bit test per
  path, the float-defaults block reproduces the `0x1ff0c` closing compares
  instead of hard-coded EQUAL, and the profile loader reproduces its
  closing `cmpobne 4`. All seven controlled cases match again at all three
  block boundaries and the full 49-target local CTest suite is green;
- closed selector 17's former `phase_state == 0` control-menu entry wall through
  `0x00055008`: all 14 idle entries (0-13), every neighboring forward/reverse
  transition and both 0/13 wraps are native, with input bit-5 release/held/latch
  behavior covered on every screen; the recovered bodies reuse MAIN_DATA-backed
  decimal/hex/text helpers and camera/texture diagnostics rather than snapshots,
  while the texture screen's distinct 43-instruction held-button early exit is
  modeled explicitly; 98 controlled ROM-backed states match complete live CPU
  and mutable-memory state;
- implemented the i960 `ediv` instruction in the reference executor, including
  quotient/remainder pair semantics and overflow/zero-divisor guards, unlocking
  strict execution of the control-menu decimal formatter outside its small table;
- recovered the post-boot `0x0001fcc0` input-profile selector for controlled
  modes 6, 10, 11 and 12, including both fighter-order mode-12 branches, the
  flag-driven mode-10 path, control-byte mode-11 redirect, mode-6/mode-10 float
  overrides and the `profile == 4` `0x10cc` timeout path; seven controlled
  states match the original i960 after each of the three recovered blocks
  (`0x99fc -> 0x1fdd0 -> 0x1fdd4 -> 0x1fe60`), 21/21 strict comparisons;
- restored warning-as-error portability across GCC/Clang by removing the unused
  ROM runner, making bit masks/shifts and signed sample accumulation explicit,
  and linking libm on non-MSVC builds;
- recovered the observed post-boot initializer from `0x0006dd4c` through the
  caller boundary at `0x000098b0` as 15 strict native blocks: 1,498,968
  reference/recovered instructions with exact CPU, local-frame, procedure-counter
  and mutable-memory equality at every checkpoint;
- replaced the two large initialization copy loops with shared descriptor-stream
  semantics instead of address-specific copies: the `0x00023f30` stream consumes
  4 descriptors / 464 32-bit words, while `0x00023ee8` consumes 22 descriptors /
  92,672 halfwords; the same parsers are reused by the `0x00011b48` aggregate
  initializer and its embedded streams;
- recovered the valid backup-SRAM path including destructive-safe SRAM probing,
  the `VIRTUA FIGHTER 2` / version-24 checks, both table-driven CRC validations
  and the observed `0x3fe0`-byte restore into the work-RAM mirror; the shared CRC
  implementation is now used by both phase recovery and post-boot restore;
- made video-ramp instruction accounting state-derived rather than call-site
  hardcoded: the initial controls reproduce 11,563 instructions and the restored
  `0x40/0x25` controls reproduce 11,245, matching the ROM's clamp branches;
- recovered the following palette/table and hardware-core setup through
  `0x000098b0`, including the `0x00011b48` aggregate and the observed geometry /
  video control path; the next concrete call is the texture/graphics initializer
  at `0x0004b020`;
- recovered the first post-boot initialization prefix from the warm-reset
  boundary at `0x0000052c` through the call entry at `0x0006dd4c`: the native
  block reproduces the `0x00009798` prologue, diagnostic-byte/mode writes, the
  serial-control initialization at `0x0004372c` including six architectural
  delay calls, the command-queue write at `0x000438ec`, and the `0x0000a048`
  return stub; strict ROM-backed replay matches 60,078 instructions, 10 calls /
  9 returns, CPU/local-frame state and all mutable memory exactly;
- recovered the phase-17 bit-7 terminal countdown transition at `0x0005f07c`:
  the `counter 1 -> 0` path now reproduces layer-bit clearing, global/gameplay
  resets, the 48x64 tile clear, the `RESET` diagnostic write through
  `0x0006116c`, and the non-returning branch to boot entry `0x000000b0`; strict
  replay matches the enclosing `main-final-cluster` at 13,426 instructions;
- made boot stages 1 and 2 warm-reset safe by preserving i960 registers and
  control state that the ROM does not overwrite, while retaining the existing
  cold-start differential contracts; the native runtime now dispatches the
  `0x000000b0` and `0x000001b0` boot stages as recovered blocks;
- validated the complete soft-reset handoff strictly from the phase-17
  preterminal checkpoint through boot stage 1 (1,180,053 instructions) and boot
  stage 2 (182,514 instructions) to `0x0000052c`: 1,375,993 total reference and
  recovered-native instructions with exact CPU, local-frame, procedure-counter
  and mutable-memory equality after each of the three native blocks;
- added ROM-independent regression coverage for the terminal phase-17 memory,
  register and procedure-count contract, warm boot-context preservation, and
  the new native-runtime boot-stage step kinds;
- accelerated strict per-block differential acceptance without weakening its
  equality contract: live CPU/mutable-memory state is compared directly,
  snapshot captures reuse same-sized buffers, and equal regions use `memcmp`
  before falling back to byte-precise mismatch diagnostics;
- added ROM-independent regression coverage for live-state equality/mismatch
  reporting and snapshot-buffer reuse;
- extended the verified 36/36-ROM strict fifth-dispatch endurance corridor to
  10,000 chained repeated-address cycles: 360,000 blocks / 15,689,445 recovered
  i960 instructions with per-block equality, ending again at `0x0001645c` with
  scheduler entry / frame IRQ 10,003; the existing 16,384-cycle boundary probe
  remains scouting evidence under its intentionally weaker cycle-end contract;
- recovered the phase-17 bit-7 indirect dispatch for observed index `0x8b`:
  `0x00059154` clears the flag and dispatches through table entry `0x0005ff00`
  to `0x0005ef60`; the first visit now reproduces the ROM's game-meter update,
  15-byte table CRC, 48x64 tile clear and `EXIT TEST MODE` diagnostic draw, with
  13,286 frame-dispatch instructions / 27 calls / 28 returns and a strict-equal
  13,518-instruction enclosing `main-final-cluster`;
- recovered the positive phase-17 bit-7 countdown path: after the first visit
  arms counter 320 and latches phase byte `0xff`, each subsequent visit repeats
  meter+CRC and decrements the counter with a 626-instruction dispatcher /
  858-instruction enclosing cluster; strict per-block replay covers consecutive
  countdown visits, while a resumable cycle-boundary probe validates 319 cycles
  (11,484 blocks / 701,481 reference and native instructions) and stops exactly
  at the still-fail-closed `counter 1 -> 0` terminal transition;
- fixed phase-17 bit-7 return tracking to read the caller return address from the
  saved i960 local frame rather than the freshly-cleared current-frame `r2`;
- added ROM-independent coverage for the bit-7 first-visit and positive-countdown
  contracts, including game-meter fixtures, CRC backing data, tile/text output,
  procedure counts and countdown state;
- recovered the phase-17 gameplay mask `0x04000104` reset/display branch in
  `0x00058fe0`: controlled ROM-backed replay sets phase-index bit 7, clears the
  phase auxiliary byte, latches `0xff`, zeroes the object marker, reproduces the
  ROM's 48x64 tile-plane clear through `0x00008ef0`, and centers/copies the
  phase label through `0x00060410 -> 0x00007fc0`; the resulting dispatcher is
  12,657 instructions with 5 calls / 6 returns and the enclosing
  `main-final-cluster` matches the reference exactly at 12,889 instructions;
- strict replay remains equal for the next 35 recovered blocks after that
  controlled reset and now stops at the following `main-final-cluster`, where
  the newly-set phase-index bit 7 selects the still-unrecovered indirect branch
  at `0x00059154`;
- recovered the phase-17 forward-step gameplay mask `0x08001008` in
  `0x00058fe0`: controlled ROM-backed differential execution proves the same
  double-indirect old/new phase-marker protocol as the step-back branch, with
  `11 -> 0` wrap taking 50 frame-dispatch instructions and ordinary
  `10 -> 11` taking 49; forward-step has the original ROM's priority over
  simultaneous bit 13, the enclosing `main-final-cluster` matches at 282
  instructions, and the complete following 36-block / 2,186-instruction cycle
  matches the reference i960 exactly;
- recovered the phase-17 gameplay bit-13 (`0x00002000`) step-back branch in
  `0x00058fe0`: controlled ROM-backed differential execution from the exact
  pre-`main-final-cluster` checkpoint proved the 49-instruction `11 -> 10`
  path, the `0 -> 11` wrap variant accounts one additional instruction, the
  old/new phase targets receive 16-bit `0x8020`/`0x801c` markers through the
  ROM's double-indirect table, and the enclosing `main-final-cluster` plus a
  complete 36-block scheduler cycle now match the reference i960 exactly;
- added ROM-independent phase-17 tests for forward/backward ordinary and wrap
  cases, forward-over-bit13 priority, reset/display, and the observed index-11
  bit-7 first-visit/countdown paths; phase-state-zero, other bit-7 table entries
  and the countdown terminal transition remain fail-closed;
- added `vf2_native_differential_probe_cycles` and `vf2cycles --boundary-probe` for long-horizon repeated-frame scouting: reference and native execution remain instruction-count locked per recovered block, frame-wait host state is checked on each wait block, complete CPU/mutable-memory state is compared at cycle boundaries, and any failing cycle restores both machines plus native runtime state to its exact start for strict replay;
- added `vf2cycles --output-snapshot <file>` to persist successful endurance boundaries together with the versioned `.runtime` sidecar, allowing long ROM-backed probes to resume without replaying earlier cycles;
- added ROM-independent coverage for the zero-cycle probe contract and retained the existing strict per-block runner unchanged as the acceptance path;
- ROM-backed cycle-boundary probing from the proven fifth-dispatch corridor reached scheduler entry / frame IRQ 16,384 with complete cycle-end state equality; this remains scouting evidence distinct from the published 10,000-cycle strict per-block claim. The repeated state remains on the same mode/phase/gameplay fast paths, so v0.2.0 work now shifts toward controlled state-transition evidence rather than passive endurance of the same attractor;
- recovered the `0x0000a75c` busy subpath of `frame_geometry_gate`: the two
  observed transitions through `0x0000a748 -> 0x0000a800` (the
  `state[0x0050002a] != 17` retry-write, eight instructions and one byte,
  and the `state[0x005000a6] != 0` alt-return, seven instructions) are now
  handled by `execute_frame_geometry_gate` instead of rejected with
  `VF2_ERROR_UNSUPPORTED`;
- retained the unobserved deep reset subpath at `0x0000a784` (calls to
  `0x00008ef0` and `0x0006116c` followed by an unconditional branch to
  `0x000000b0`) as `VF2_ERROR_UNSUPPORTED` because no live sweep observed
  via `vf2i960 observe-third-sweep` reaches `state[0x005000a6] == 0`;
- documented the static decode of the unrecovered callees in
  `decomp/i960/notes/frame_geometry_gate_busy_path_v0010.md`:
  `0x00008ef0` is a 48-row 64-cell stride-fill of value `0x20` starting at
  `0x01000000`, and `0x0006116c` is a 16-byte magic write to `0x0059cfe0`
  with no static xrefs;
- added a ROM-independent `test_frame_geometry_gate_busy_paths` unit test
  covering the busy-frame-state retry, the busy-alt return and the unobserved
  deep-reset rejection. The 29-test Release run is still warning-clean with
  warnings treated as errors under C17;
- preserved all v0.0.24 strict totals on the accepted second-dispatch path:
  `frame geometry gates: 0` on that path because the geometry prefix calls
  into the gate with `0x00500704 == 0`, so the busy subpaths are exercised
  only by the new unit test, not by the differential validator.
- removed the `VF2_NATIVE_RUNTIME_STEP_THIRD_SCHEDULER` runtime guard and its
  `third_scheduler_attempts` accounting: the recovered
  `vf2_hybrid_second_scheduler_enter` is now dispatched on every visit to the
  main-loop scheduler call site `0x0000a010`. Reference i960 evidence gathered
  via `vf2i960 observe-third-sweep` confirmed the architectural preconditions
  and the live task selection (descriptor index 13, `fa_game_info`,
  `0x0001645c`, registry `0x00515200`) are identical across the four observed
  sweeps, so the previously distinct third-scheduler step kind is no longer
  reported. The `STEP_THIRD_SCHEDULER` enum constant and the
  `third_scheduler_attempts` fields on `vf2_native_runtime_state` and
  `vf2_native_runtime_run_report` are removed;
- replaced `test_third_scheduler_attempt_is_unsupported` with
  `test_repeated_scheduler_entry_dispatches_recovery`, a ROM-independent unit
  test that proves a second entry at `0x0000a010` after the second sweep is
  now forwarded to the actual scheduler recovery instead of being
  short-circuited;
- added `_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_WARNINGS` and
  `_CRT_NONSTDC_NO_DEPRECATE` to `cmake/VF2Warnings.cmake` for non-MinGW
  Windows builds (cl.exe and clang.exe against the MSVC UCRT headers);
- enabled a clang 22.1.1 AddressSanitizer + UndefinedBehaviorSanitizer build
  with `-fsanitize=address,undefined -fno-omit-frame-pointer -Werror` against
  the MSVC SDK. All 73 targets compile and link cleanly, and all 29 CTest
  tests pass with no sanitizer violations under the dynamic
  `clang_rt.asan_dynamic-x86_64.dll` runtime;
- made `build.ps1` forward `-DVF2_ROM_DIR=$Repo\roms\vf2` by default on `cfg`,
  `build` and `asan` so ROM-backed CTest targets are registered without
  per-invocation configuration;
- set `MSYSTEM=UCRT64` in `build.ps1` before invoking MSYS2 UCRT64 GCC, so the
  compiler does not silently exit non-zero from a non-MSYS2 PowerShell session.
- added a `vf2i960 observe-third-sweep <rom-directory>` developer command and
  associated CTest target `vf2_third_sweep_observation` (test 29) that runs
  the strict v0.0.24 second-dispatch validator and then continues the reference
  i960 forward through subsequent scheduler sweeps while manually injecting
  vector-12 interrupts at the frame-wait poll loop;
- confirmed through four observed sweep visits that the reference i960 always
  reaches the main-loop scheduler call site `0x0000a010` with the exact
  architectural preconditions `vf2_hybrid_second_scheduler_enter` already
  validates (`frame_depth == 0`, `fp == 0x005ff500`, `r1 == 0x005ff580`,
  `ready_flags == 0x80004400`, `runtime_flags == 0x00008a00`,
  `task_count == 29`, both timers parked at `0x000fffff`) and always selects
  task descriptor index 13 (`fa_game_info`, `0x0001645c`,
  registry `0x00515200`); the recovered second-sweep scheduler entry is
  therefore provably generic for repeated scheduler sweeps, so the v0.1.0
  blocker is no longer the scheduler scan;
- recorded per-sweep evidence that the `0x0000a75c` busy path on
  `frame_geometry_gate` (gate at `0x0000a748`, flag source `0x00500704`)
  fires on the third scheduler sweep due to `(flags & 0x04000004)` becoming
  non-zero (`0x0ff7f7ff`); the busy path runs through `0x0000a778`, calls
  `0x00008ef0` and `0x0006116c`, then jumps to `0x000000b0`, and is the
  v0.1.0 recovery target rather than the scheduler scan;
- documented the third-sweep evidence in `docs/STATUS.md` and
  `docs/UNCOVERED_BRANCHES.md`.

## 0.0.24 — 2026-08-02

- completed recovery of the accepted post-scheduler second-dispatch path: all
  1,270,822 original bridge instructions now execute as recovered C, with zero
  native-side interpreter fallbacks;
- composed the gameplay input/state/meter, tile controller, interrupt support,
  video, texture-orchestrator and main-loop tails from the previously recovered
  helpers without duplicating their semantics;
- replaced the final ten polling/return instructions with an explicit recovered
  frame-wait executor that preserves four observed visits, vector-12 interrupt
  injection, the i960 interrupt frame, return state and changed-frame-byte exit;
- retained step-by-step execution of the reference interpreter in the ROM-backed
  validator and compared complete CPU and mutable Model 2 memory after all 190
  recovered blocks;
- reached strict totals of 1,270,822 recovered, 0 interpreted, 190 blocks and
  memory checkpoints, and 342/340 recovered procedure calls/returns;
- added ROM-independent coverage for both recovered frame-wait phases and kept
  the full build warning-clean under C17 with warnings treated as errors;
- preserved the scope boundary: this proves one observed VF2 2.1 startup path,
  not a complete playable port, and unsupported branches remain rejected.

## 0.0.23 — 2026-08-02

- pure-evidence release: no new recovered blocks, no new `vf2_hybrid_bridge_kind`, no `case` added to `vf2_hybrid_post_frame_bridge_execute`, and no change to the `bridge_candidate` IP list in the differential validator;
- added the read-only `trace-orchestrator` developer command, which reuses `command_native_dispatch` and emits a CSV row per interpreted native step in the `[0x0004bb18, 0x0004c180]` cluster;
- the command aborts (and writes no usable evidence) unless the existing strict total assertions still hold, keeping it provably non-behavior-changing relative to v0.0.22;
- recorded observations of the texture orchestrator cluster in `decomp/i960/notes/texture_orchestrator_v0023.md` and the default CSV path `decomp/i960/notes/texture_orchestrator_v0023.csv`;
- backfilled `decomp/i960/symbols.csv` and `decomp/i960/functions.csv` with the four v0.0.22 helpers (`0x00009444`, `0x0004d2c0`, `0x0000281c`, `0x000026ec`) that were missing from those tables;
- preserved all v0.0.22 headline totals: 1,270,822 bridge instructions, 1,268,752 recovered, 2,070 interpreted, 143 blocks/checkpoints, 250/297 calls/returns;
- deferred the `0x00001f5c` geometry-preparation cluster to v0.0.24 to avoid diluting the orchestrator evidence collection.

## 0.0.22 — 2026-08-01

- recovered the inline diagnostic thunk at `0x00009444`, including its nested text-copy call, inline-data scan, destination-row advance and architectural `balx` continuation;
- recovered four live calls to the texture-status line procedure at `0x0004d2c0`, including the `TEX`/`t4e` label, indexed texture name and tilemap destinations;
- recovered the observed game-state classifier at `0x0000281c` for three direct calls and eight nested calls;
- recovered eight live game color/control lookups at `0x000026ec`, including selector-dependent table lookup, stack preservation and the `0x00010101` adjustment;
- restricted gameplay helpers to the states actually observed by the startup bridge, returning `VF2_ERROR_UNSUPPORTED` for unproved modes and flag combinations;
- replaced 513 additional bridge instructions, increasing recovered execution to 1,268,752 instructions and reducing the interpreted remainder from 2,583 to 2,070;
- increased recovered blocks and complete differential checkpoints from 136 to 143;
- increased recovered call/return accounting from 233/274 to 250/297;
- added strict v0.0.22 aggregate and per-kind assertions;
- added `compare-game-geometry-helpers` and a twenty-second CTest target.

## 0.0.21 — 2026-08-01

- recovered the observed second scheduler entry from main-loop call site `0x0000a010` through `callx` into `fa_game_info`;
- reproduced the two geometry-status helper calls at `0x00007b18`, including writes to `0x00800070` and `0x00804000`;
- recovered scanning of thirteen inactive descriptors and selection of runnable task index 13 at registry `0x00515200`;
- reproduced per-descriptor current-index, timer reload and timing-scratch updates;
- reconstructed the scheduler local frame at `0x00010dc8` before entering the task, preserving the exact cached continuation at `0x00010dcc`;
- replaced 235 additional bridge instructions, four procedure calls and two returns with recovered C;
- increased recovered bridge execution to 1,268,239 instructions and reduced the interpreted remainder to 2,583;
- increased accepted blocks and full memory checkpoints from 135 to 136;
- added `vf2_hybrid_second_scheduler_enter`, `compare-second-scheduler-entry` and a twenty-first CTest target;
- retained the remaining texture orchestration, gameplay preparation and geometry helpers as interpreted code.

## 0.0.20 — 2026-08-01

- recovered texture-address table construction at `0x0004d16c`, validating four live invocations and ten pointer outputs per table;
- recovered nine diagnostic text copies at `0x00007fc0` and the 48-glyph tile expansion at `0x0004f944`, including its 3,072-byte tile-RAM output;
- recovered the observed palette-page uploader at `0x00002de4`, including 28 pages and 8,064 bytes of palette writes;
- recovered 32 texture-conversion loop controllers at `0x0004cdb0` and 28 continuation blocks at `0x0004cdd4`;
- recovered eight timer/wait updates at `0x00000b6c` with explicit timer-3 and wait-flag postconditions;
- recovered the video-status latch at `0x00002ec4`, frame scratch clear at `0x0000a154`, geometry frame commit at `0x00002edc` and command setup at `0x00002f5c`;
- decoded the first geometry register sequence: previous command to `0x00803008`, read pointer at `0x00802008`, next ring command to `0x00801008`, and ring state in `0x00501004–0x0050100c`;
- increased recovered bridge execution from 1,262,476 to 1,268,004 instructions and reduced the interpreted remainder from 8,346 to 2,818 instructions;
- increased accepted bridge blocks and complete memory checkpoints from 48 to 135;
- added strict v0.0.20 totals and per-kind invocation assertions;
- added `compare-geometry-boundary` and a twentieth CTest target;
- retained the remaining 2,818 scheduler, gameplay and geometry-preparation instructions as interpreted code rather than claiming a complete native frame loop.

## 0.0.19 — 2026-08-01

- recovered the complete byte texture decoder at `0x0004c6e0`, replacing all 1,752 inner byte-run invocations with four bounded decoder calls;
- recovered the complete word texture decoder at `0x0004cc28`, replacing all 1,752 inner word-run invocations with four bounded decoder calls;
- recovered the symbol-table builder at `0x0004c3f0` and pair-table builder at `0x0004c4d4`, including bitstream refill, ROM lookup and exact i960 condition-code postconditions;
- increased recovered post-frame execution from 712,821 to 1,262,476 instructions and reduced the interpreted remainder from 558,001 to 8,346 instructions;
- reduced the bridge from 3,536 fine-grained invocations to 48 semantically complete recovered blocks, each checked with a full mutable-memory comparison;
- added `vf2_hybrid_frame_wait_initialize` and `vf2_hybrid_frame_wait_observe`, replacing the ad-hoc frame-event counter with a native state machine that injects vector 12 after four observed wait visits;
- identified the first geometry-facing instruction at `0x00002eec`, targeting geometry address `0x00803008` with first changed byte `0x00803009`;
- added explicit v0.0.19 bridge totals and geometry-boundary assertions to the differential validator;
- added `compare-post-frame-bridge` and a nineteenth CTest target;
- retained the remaining 8,346 orchestration, hardware-helper and geometry-facing instructions as interpreted code rather than claiming a fully native frame bridge.

## 0.0.18 — 2026-08-01

- decomposed the 1,270,822-instruction post-frame interval into recovered and interpreted execution;
- added `src/recovered/texture_bridge.c` and the public `vf2_hybrid_post_frame_bridge_execute` API;
- recovered the repeated byte-store loop at `0x0004c868`, validating 1,752 live invocations;
- recovered the repeated word-store loop at `0x0004cce8`, validating 1,752 live invocations;
- recovered recursive texture-tree expansion at `0x0004c928`, including observed leaf-table decoding, nested calls, returns and recursion depth;
- recovered the proved no-suspend texture color-conversion path at `0x0004ce88`, validating 28 calls;
- replaced 712,821 bridge instructions with recovered C while retaining 558,001 interpreted instructions;
- validated 3,536 recovered blocks with 58 intermediate mutable-memory comparisons and a complete final CPU/memory comparison at the second `fa_game_info` entry;
- added ROM-independent unit coverage for all four block kinds;
- added `compare-texture-bridge` and an eighteenth CTest target;
- retained unsupported suspend/frame-state branches and the remaining bridge orchestration as interpreted code instead of generalizing unproved behavior.

## 0.0.17 — 2026-08-01

- initialized i960 `FP` and `SP` from the interrupt-stack pointer at `PRCB + 24`, matching the architectural reset state used by the original runtime;
- added `vf2_i960_cpu_reset_from_machine` and updated recovered boot postconditions;
- modeled both 2 MiB Model 2A texture-RAM banks and their hardware mirrors at `0x12000000–0x127fffff`;
- advanced snapshot format to v5 with 18 mutable regions, adding texture RAM 0 and texture RAM 1;
- recovered the end of the first scheduler sweep after `fa_osage1`, including final task accounting, inactive descriptor 28, diagnostic tile state, timer reload and return to `0x0000a014`;
- added `vf2_hybrid_first_dispatch_scheduler_finish`, representing 281 additional i960 instructions and five architectural returns/calls;
- extended the recovered first traversal from 4,342 to 4,623 instructions;
- reached the second scheduler traversal without restoring any snapshot after the initial live task-entry fixture;
- preserved all 29 task contexts through the post-frame path and one real frame interrupt;
- validated the second `fa_game_info` entry at `0x0001645c` with registry `0x00515200`;
- added `native-second-dispatch`, a seventeenth CTest target and unit coverage for PRCB stack reset plus texture-RAM mirroring;
- proved complete CPU, local-frame, counter and all 18 mutable-region equality at the first-sweep exit and second task entry.

## 0.0.16 — 2026-08-01

- added native recovered-C execution for all seven naturally runnable first-dispatch task bodies;
- added explicit architectural postconditions and native procedure returns for `fa_game_info`, `fa_user`, `fa_sound`, `fa_kill_osage`, `fa_osage0` and `fa_osage1`;
- completed the observed `fa_camera` task by replacing its final interpreted `ret` with an architectural C return;
- added `vf2_i960_cpu_return_procedure` as the public recovered-code procedure-return primitive;
- recovered all six scheduler transitions between the seven runnable records;
- reproduced descriptor scanning, timing scratch, current-index updates, timer state, diagnostic names and tile-RAM task-name rendering;
- reproduced exact scheduler local/global register postconditions and architectural `callx` entry into each next task;
- replaced 2,808 task instructions and 1,534 scheduler instructions, for 4,342 recovered instructions in the first traversal;
- eliminated all interpreted task-body and scheduler steps from the native first-dispatch validation path;
- added `native-first-dispatch` and a sixteenth ROM-backed CTest target;
- proved complete independent CPU, local-frame, counter and mutable-memory equality at every task/transition boundary and final checkpoint `0x00010dcc`.

## 0.0.15 — 2026-08-01

- added `vf2_hybrid_camera_execute`, which applies accepted camera memory effects and advances the i960 architectural state entirely in recovered C;
- recovered explicit register postconditions for camera initialization, the first recurring update and the observed post-update fast gate;
- recovered exact instruction, procedure-call and procedure-return counter deltas for the three blocks;
- proved active saved local frames remain unchanged across all three accepted intervals;
- removed the `hybrid_cpu = original_cpu` register synchronization from `hybrid-first-dispatch`;
- changed per-block validation from memory-only comparison to complete CPU-and-memory snapshot comparison;
- required independent instruction/call/return/interrupt counters and maximum frame depth to match at the final scheduler checkpoint;
- added ROM-independent stateful post-update tests and explicit unsupported handling for non-observed architectural exits;
- retained the original execution only as an independent differential oracle;
- validated 2,699 recovered camera instructions and final scheduler checkpoint `0x00010dcc` without ROM-derived CPU state.

## 0.0.14 — 2026-08-01

- introduced composable hybrid execution for the three accepted live camera intervals;
- substituted 2,699 original camera instructions with recovered memory blocks during the first dispatch;
- compared mutable memory at every accepted continuation;
- used an independent original run to bridge register postconditions while the explicit C post-state was still unknown;
- interpreted only the final camera return instruction before continuing through the remaining initial tasks;
- proved final CPU and mutable-memory equality at scheduler checkpoint `0x00010dcc`;
- added `include/vf2/hybrid.h`, `src/recovered/hybrid.c`, `docs/HYBRID_EXECUTION.md` and the fifteenth ROM-backed CTest target.

## 0.0.13 — 2026-08-01

- recovered the optional camera viewport-construction block from `0x0001d678` through `0x0001d8e8`;
- recovered helper `0x0001fbb4` for centered range construction and its work-RAM outputs;
- recovered helper `0x0001eff0` for projecting the two fighter states into camera profiles and signed weights;
- recovered helper `0x0001facc` for selecting and interpolating the 8-entry and 10-entry viewport tables;
- validated both the fixed-table path and the calculated-table path against the original i960 implementation;
- reproduced all 18 task-table entries, normalized range globals, fighter profile/weight updates and coprocessor scratch state;
- added `compare-camera-viewport`, a fourteenth ROM-backed CTest target and a ROM-independent fixed-path unit test;
- documented that the real first dispatch still carries input flags `0x0006` and therefore does not execute this optional block naturally;
- kept hybrid replacement and the camera body after `0x0001d984` for the next release instead of claiming a complete camera task.

## 0.0.12 — 2026-08-01

- recovered the camera post-update gate beginning at `0x0001d660`;
- proved that first-dispatch input flags `0x0006` skip the viewport construction block at `0x0001d678`;
- proved that control byte `0x0050009c = 1` selects the fast return at `0x0001e524`;
- recovered the complete non-viewport control-flag path through `0x0001d984`, including task flag bits 1 and 2 and the mode/phase override byte at task offset `0x2d4`;
- added `vf2_recovered_task_camera_post_update_gate` with explicit unsupported handling for the still-unrecovered input-bit-3 viewport path;
- expanded the live camera differential boundaries from two to three and increased first-dispatch C-validated paths/prefixes from eight to nine;
- proved the stable scheduler checkpoint after all seven initial task returns at `0x00010dcc`;
- added ROM-independent tests for fast exit, normal control updates, override writes and the unsupported viewport branch;
- retained the viewport construction helpers and later camera body as interpreted code instead of generalizing unproven behavior.

## 0.0.11 — 2026-08-01

- recovered and differentially validated the observed first recurring `fa_camera` prefix from `0x0001d458` through `0x0001d660`;
- completely recovered scalar helper `0x000214dc` as `vf2_recovered_camera_classify_range` and validated nine directional/boundary cases against the original;
- recovered the observed early-return branch of helper `0x00020558`, including task flag bit 8;
- recovered the observed early-return branch of mode dispatcher `0x0001fc00`;
- described the eight-entry camera mode table at `0x0006e2e4` and proved first-dispatch mode 1 targets `0x0001f148`;
- modeled the camera arithmetic scratch writes at coprocessor-port offset `0x4000` without claiming geometry submission;
- recovered fighter profile selection and the camera globals written before the mode-specific body;
- expanded live camera validation from the initializer boundary `0x0001d458` to update boundary `0x0001d660`;
- added ROM-independent tests for the range classifier and recurring-prefix recovery plus ROM-backed `compare-camera-classifier`;
- kept the mode-specific camera body after `0x0001d660` interpreted and explicitly documented that no geometry RAM write occurs in the first camera dispatch.

## 0.0.10 — 2026-08-01

- completely recovered `fa_kill_osage` and its two-record helper in semantic C;
- reproduced timer-derived osage aging, processing order, kill flag bit 3 and the global kill counter;
- recovered the `fa_camera` initialization prefix through continuation `0x0001d458`;
- recovered the camera palette helper at `0x000216b8`, including 125 indexed palette conversions;
- recovered the observed no-secondary-setup branch of camera reset helper `0x0001f148`;
- added live first-dispatch call-site and call-target capture, including indirect-call marking;
- differentially validated all seven initially runnable task paths, with the camera explicitly bounded to its initializer prefix;
- added `compare-first-dispatch`, focused camera/osage unit tests and a twelfth ROM-backed CTest target;
- retained the recurring camera body as interpreted code instead of claiming unsupported recovery.

## 0.0.9 — 2026-08-01

- profiled all seven initially runnable tasks from real scheduler entry through procedure return;
- added per-task instruction, call-depth and tracked-memory-change measurements;
- added optional CSV export through `task-profile`;
- completely recovered the one-instruction `fa_user` task in C;
- recovered and differentially validated the complete `fa_sound` first-entry initializer;
- recovered the observed first-dispatch branch of `fa_game_info`, including direct reset/countdown behavior;
- recovered the observed initialization branch shared by `fa_osage0` and `fa_osage1`;
- cloned live task-entry snapshots and validated five original task paths against recovered C memory state;
- added `compare-task-recoveries`, `task-profile`, focused unit tests and two new ROM-backed CTest targets;
- expanded the supported validation matrix to eleven passing targets.

## 0.0.8 — 2026-08-01

- proved the natural runtime-ready transition at `0x00009ca4`, setting work-RAM bit `0x00500068[31]`;
- modeled the frame-event sequence required to reach the non-idle runtime path;
- identified the scheduler registry consumer at `0x00010d54`;
- recovered the scheduler registry scan and runnable-task planning in semantic C;
- validated 29 runtime descriptors and seven initially runnable tasks;
- observed and distinguished the first seven real task dispatches through entry point plus registry address;
- added `scheduler-dispatch` and a ninth ROM-backed CTest target;
- expanded i960 semantics used by the path, including carry arithmetic, bit scans, rotation and basic floating-point operations;
- corrected ROM no-write and Model 2A I/O handshake behavior required by the natural transition;
- preserved the explicit validation boundary before the still-unmodeled geometry path.

## 0.0.7 — 2026-08-01

- added architectural i960 external-interrupt entry through PRCB vector tables;
- added type-7 interrupt frames and restoration of process/arithmetic control on `ret`;
- modeled Model 2 interrupt request, enable and acknowledge semantics;
- injected timer IRQ vector 14 and deterministically released the wait at `0x0004aff8`;
- returned to the wait caller at `0x0004b07c` and executed the idle path of frame IRQ vector 12;
- recovered the vector-14 timer dispatcher at `0x00000d50` in semantic C;
- validated the recovered timer handler byte-for-byte against 33 interpreted instructions;
- added interrupt-entry/return counters and focused ROM-independent interrupt tests;
- added snapshot format version 4 and fixed duplicate local-frame serialization;
- added `scheduler-pass` and `compare-timer-irq` commands;
- expanded ROM-backed validation to eight CTest targets.

## 0.0.6 — 2026-08-01

- implemented architectural 64-byte-aligned local-register frames for nested `call`/`callx`/`ret`;
- corrected `balx` link-register behavior and effective-address wrapping;
- added additional shift, division, remainder and bit-branch instruction semantics;
- mapped main-data ROM, backup SRAM, timers and evidence-backed runtime MMIO regions;
- added circular execution history and complete register dumps on failure;
- added snapshot format version 3 with local frames and expanded mutable regions;
- reached deterministic runtime checkpoint `0x0004aff8` after 2,985,244 instructions;
- added `VF2_ENABLE_SANITIZERS` and validated GCC, Clang, ASan and UBSan builds.

## 0.0.5 — 2026-08-01

- extended deterministic execution from `0x000001b0` through `0x0000052c`;
- modeled interrupt control, tile RAM, palette RAM, I/O control, coprocessor control and color-translation memory;
- recovered post-IAC hardware initialization in semantic C and validated it byte-for-byte;
- discovered the contiguous 29-record `fa_*` task descriptor table at `0x00011dc0`;
- recovered task flags, instances, stack sizes, entry points, state pointers and scheduler slots;
- added task entry points as analysis roots and stable task-derived function names;
- increased measured static discovery to 263 functions, 16,821 instructions and 6,248 cross-references;
- recovered the task-registry initializer at `0x00010cbc` in C;
- validated the registry initializer against 647 original i960 instructions with a complete memory match;
- added snapshot format version 2 and memory-only differential comparison;
- added `tasks`, `compare-init` and `compare-task-registry` commands;
- added ROM-independent tests for task parsing, registry construction and the expanded executor.

## 0.0.4 — 2026-08-01

- added a deterministic C17 semantic executor for the i960 startup subset;
- expanded the bounded Model 2A memory model with video, CPU and system-control regions;
- added real IAC reinitialization handling for `synmovq` packets;
- executed the supported ROM startup path from `0x000000b0` to `0x000001b0`;
- added CSV instruction tracing and versioned binary machine snapshots;
- added snapshot restore and first-difference comparison;
- completed semantic C recovery of startup stage 1, including control-table and interrupt-state copies;
- added `execute`, `trace`, `snapshot`, `compare-boot` and `compare-snapshots` commands;
- added ROM-independent executor, snapshot and complete recovered-startup tests;
- validated 1,180,053 interpreted startup instructions against recovered C with a byte-for-byte state match.

## 0.0.3 — 2026-08-01

- added forward abstract interpretation for all 32 i960 registers;
- added constant, address, stack-relative, argument and table-lookup values;
- added constant-indirect and indexed jump-table target recovery;
- added i960 ABI recognition for `bx (g14)` returns;
- added stack-frame, argument-mask, return-mask and leaf-function heuristics;
- added tail-branch and overlapping-entry split candidates;
- added stable symbol overlays from `decomp/i960` CSV files;
- added `vf2i960 frame` and `vf2i960 pseudoc` commands;
- added generated `values.csv`, `indirect-targets.csv`, `stack-frames.csv`,
  `function-splits.csv` and per-function `pseudo-c/*.c`;
- added ROM-independent tests for constant indirect branches, jump tables,
  boundary candidates and pseudocode output;
- preserved conservative behavior when real-ROM indirect targets cannot be
  proven.

## 0.0.2 — 2026-08-01

- added a structured C17 Intel i960KB decoder;
- added instruction formatting without reparsing text;
- added conservative function and basic-block discovery;
- added direct call, branch, memory and string cross-references;
- added image classification for code, strings, padding and unknown bytes;
- added `vf2i960` commands for disassembly, function inspection, analysis and
  cross-reference lookup;
- added JSON, CSV, assembly and Graphviz analysis output;
- added a semantic C recovery of the startup RAM-clear operations;
- expanded the Model 2A memory skeleton with buffer RAM;
- added decoder, CFG and recovered-boot tests;
- tested the analysis against the supported 36-file VF2 Version 2.1 set.

## 0.0.1 — 2026-08-01

- created the C17/CMake repository structure;
- added the exact 36-file VF2 Version 2.1 ROM manifest;
- added CRC-32 and SHA-1 validation;
- added Model 2A region reconstruction;
- added i960 reset-vector parsing and string extraction;
- added initial documentation, CI and tests.
