# Changelog

## Unreleased

- COBR CC + pin `0x270d4` (v0359): executor `cmpo*`/`cmpi*` COBR now write
  `compare_result` **and** AC low condition bits (hardware lockstep). Recovered
  exits recalibrated on measured last-cmpo: `fa_kill_osage` (0x65838 chain),
  `fa_osage0/1` (`cmpobne 0,instance` → EQUAL/LESS), first-sweep scheduler
  finish → **GREATER** at `0xa014`. `native-first-dispatch` and
  `native-sixth-dispatch` **MATCH** (870 blocks / 7,404,901 insns). Player
  wrapper `0x270d4` admitted: C 5×`0x27b5c` equals oracle on five slots,
  **9235** insns, ROM pin `vf2_player_270d4_five_slot_pin`. Selectors
  `{0x0505,0x0039,0x00f1,0x00e7,0x00af}` at main_data `0x0201c2fc`.
  `phase17_zero` differential still fails on per-path CC (open). See
  `player_270d4_slot_pin_v0358.md` (blocked narrative) + this slice.

- Pin `0x270d4` five slots + coli body 77 (v0358): record selectors medidos
  em main_data `0x0201c2fc` = `{0x0505,0x0039,0x00f1,0x00e7,0x00af}`.
  COBR `cmpobl`+`be` no despacho de `0x27b5c`: com CC arquitetural
  (experimental, revertido) o span oracle é **9235** e C iguala os cinco
  slots; com executor master (COBR sem escrita de `compare_result`) o span
  stale é **9378**. O fix de CC quebra MATCH (`fa_kill_osage`, phase17,
  bridges). Master mantém executor legacy e `0x270d4` **fail-closed**.
  Coli live body 77 documentado sem extensão C. See
  `decomp/i960/notes/player_270d4_slot_pin_v0358.md` and
  `decomp/i960/notes/fa_coli_22404_body77_v0358.md`.

- Player `0x27b5c` degenerado vs válido (v0357): sixth/punch têm
  `+0x1a0/+0xbd8 = 0` → reference `cvtri` falha em `0x27cc8`; C agora
  **fail-closed** nessa forma (unit `test_player_27b5c_zero_record_fail_closed`).
  Parks `player-1428c-*` com record `0x0201c2fc` / scratch `0x00520000`
  executam o wrapper **`0x270d4 → 0x2712c` em 9378 insns** com cinco slots
  preenchidos e cursors `g3=0x520630`, `g5=0x50ea98`, `g6=0x50e2d0`.
  Pin diferencial byte a byte dos slots permanece aberto. See
  `decomp/i960/notes/player_27b5c_valid_degenerate_v0357.md`.

- Attract/game-assign + player `0x27cc8` (v0356): backup factory-like no
  sixth; COIN não sai do TEST MENU (v0355). Drive `0x4505` mede
  **1749** insns até `0x270d4` e **1709** até `cvtri` em **`0x27cc8`**
  (soma **3458** = trace v0352). Wrapper `0x270d4` = 5×`0x27b5c`
  (g0/g3 tabelados) — helper C já existe no corredor `0x1428c`; admitir
  `0x270d4` reaproveitando-o continua **fail-closed** sem pin de estado
  final live dos cinco slots. See
  `decomp/i960/notes/attract_player_27cc8_v0356.md`.

- Input-driven display path (v0355): sob `vf2cycles` strict, **COIN+START**
  12 ciclos MATCH permanece **TEST MENU** (26 010 insns); **PUNCH** alcança
  **EXIT TEST MODE** naturalmente (`a4=0x8b`, countdown 320→310→290) e com
  **332** ciclos MATCH (14,9 M insns na perna de 300; 12 216 blocos) o
  countdown expira e o oracle **redesenha TEST MENU** (`a4=0x0b`, cd 0).
  Geometria FIFO e buffer-ram idênticos entre parks de teste (rampa de
  cor, não malha logo). Sem tile `SEGA`. See
  `decomp/i960/notes/input_display_path_v0355.md`.

- EXIT TEST MODE + warm-boot attract (v0354): na fronteira **frame-dispatch**
  `0xa6c0` a partir do sixth MATCH, forçar `a4=0x8b` mede first-visit
  **13286** insns e desenha tile **`EXIT TEST MODE`** (countdown **320**,
  `a5=0xff`); terminal countdown=1 → **13194** insns em **`0x000000b0`**
  com `0x500082=0x8000`. Warm-boot pós-exit usa backup **válido** (CRC
  `0x9480`, não BROKEN), selector **0→2 em 34 insns** (unit pin), depois
  selector `0x10` limpa/redesenha e o oracle **retorna a TEST MENU**
  (`0x11`) armando coli `0x221e8`. **Sem tile `SEGA`** nesta trajetória —
  logo attract permanece fronteira TGP/config. Unit
  `test_frame_dispatch_selector0_signature_fast_path`. See
  `decomp/i960/notes/exit_testmode_attract_v0354.md`.

- Entrada em tela (v0353): landmark de display medido no cold-boot.
  Primeira tela visível no oracle: tile-plane **`BACKUP RAM IS BROKEN.` /
  `INITIALIZED.`** em `0x0004aff8` (2 985 244 insns pós-stage1) e
  continuação **`I/O Initialize ...` / `Sound Initialize ...`**; pin C
  unitário em `0x010008aa` (`test_post_boot_backup_broken_screen`) além
  do I/O text existente em `0x01000c28`. Tela estável do corredor MATCH:
  **TEST MENU** selector `0x11` em `native-sixth-dispatch`
  (`sixth-fresh`, `0x5000a4=0x0b`), com paleta/texture preenchidas.
  Logo SEGA **não** é tile ASCII nesta trajetória — attract/TGP e
  EXIT TEST MODE → warm-boot permanecem fronteira explícita.
  Ferramenta `tools/python/render_tile_plane.py` (grelha 64×48 + PPM
  host-side). See `decomp/i960/notes/display_landmark_v0353.md`.

- Fecho dos abertos Combate Vivo (v0352): player `0x4505`
  reproduzido com drive padrão — punch10/sixth-regen/fifth-rt
  **1745/4/4**, boot **1743/4/4**, natres **1659/4/4**; C seleciona
  1745 vs 1743 por F0 bit26 (natres bit31 fail-closed);
  `native-resume` alcança coli `0x221e8` a partir do park armado;
  task inteira com receita live mede **9398/17/18 sem `0x225cc`**
  (sibling fail-closed); span `0x22404` live **78** passos
  documentado; frontier player pós-`0x4505` → unsupported
  **`0x27cc8`**; endurance MATCH até dispatch **10675**; CTest
  **57/57**. See `decomp/i960/notes/close_open_v0352.md`.

- Campanha Combate Vivo (v0351): arming coli por PUNCH a partir de
  `native-sixth-dispatch` (`sixth-fresh` + 330 ciclos → slot10
  `entry=0x221e8`, countdown 0); live midbody `g0=1` medido
  **380** passos / 12 call-instr / 10 rets com `0x225cc` longo
  249 até `0x22294`; C admite sibling `0x22298` bit8-set/bit1-clear
  (body **13**, unit 14 + 3 negativos fail-closed); frontier
  `--fighter-base` + `fighter_offsets.py`; `+0x0026` bilateral no
  layout candidato; endurance MATCH observada até dispatch **9626+**;
  CTest **57/57**. See `decomp/i960/notes/combat_live_v0351.md`.

- Fecho dos abertos restantes (v0350): coli live midbody
  `g0=1→0x225cc→0x10dcc` medido **380/9/10** (call `0x22290` em
  129 passos; `g13=0x514940` no park midbody); `0x4505` replicado
  em **`sixth-regen`** e **`player-14288-fifth-rt`** (1745/4/4,
  estado final punch10) — prova independente do arquivo punch10;
  shapes irmãos boot **1743** e natres **1659** documentados
  fail-closed; endurance MATCH até **8000**. See
  `decomp/i960/notes/close_open_v0350.md`.

- Fecho dos abertos (v0349): punch10-t6/pf5/type6 reproduzem
  referência `0x4505` **1745/4/4**; C fail-closed em F0 bit31/1
  (parks `player-14288-*`); unit `test_player_19ef8_selector_4505`;
  coli tail live `g0=1→0x225cc→0x10dcc` **346/6/8** a partir de
  `coli-22404-e1`; whole-task estática pinada **9393/17/18** (sem
  `0x225cc`); endurance `native-nth-dispatch` **MATCH até 5000**.
  See `decomp/i960/notes/close_open_items_v0349.md`.

- Itens 1–4 autônomos (v0348): fa_player selector **`0x4505`**
  admitido nativo na forma punch10 (1745/4/4, tabelas mascaradas
  `& 0x1fff`); site-B-only whole-task documentado inalcançável sem
  mutar `0x508000` entre cascade e `0x22dd4`; pin coli não-warm
  medido **9528/18/19** ao lado de 9214/18/19; endurance
  `native-nth-dispatch` **MATCH até 1000**. See
  `decomp/i960/notes/items_1to4_v0348.md`.

- Tracks B–D autonomous slice (v0347): native dispatch **12–40**
  MATCH (CTest pin added for dispatch 12); fa_player `0x4505`
  measured complete on `punch10` (**1745** steps to `0x1428c`)
  but warm `0x505` still faults — recovery stays fail-closed;
  coli site-B-only `0x22dd4` board-clear gate wired via proven
  `0x502a4`#siteB + `0x7fc0`#4 helpers (probe span **237**
  steps). See `decomp/i960/notes/tracks_bcd_v0347.md`.

- coli exit landing (v0346): measured live `call 0x225cc` at `0x22290`
  returns to the `ret` at `0x22294`, which pops to scheduler `0x10dcc`.
  `vf2_hybrid_coli_225cc_execute` now double-pops that ret when the
  entered return is `0x22294` and a parent frame remains. Site-A
  live-landing unit: parent `enter(0x22210,0x10dcc)` + child
  `enter(0x225cc,0x22294)` lands `0x10dcc` with exact **884/7/9**.
  Procedure-only units that enter with stand-in `0x22240` are
  unchanged. ROM-backed third/fourth/fifth/sixth/eleventh dispatch
  pass; `vf2cycles --input 16` **8/8 MATCH**
  (`decomp/i960/notes/fa_coli_exit_landing_v0346.md`);

- Site-A full leg runs natively end-to-end (v0345-B, fa_coli
  done): new `coli_225cc_sitea_cont` models `0x22960` → `0x22e24`
  (three `0x7fc0` calls, `0x9444`, scan tail, `0x2298c` join,
  site-B prefix, `0x502a4`#siteB) and joins the existing
  `0x22e24` tail unchanged. Wrapper unit proves OK, exact delta
  883, calls/rets 7/8 and all stores. `long_body` now reports
  calls/rets (legacy 4/4 default preserved).

- Native `0x7fc0` byte-expand leaf (v0345-A): `coli_7fc0_body` +
  `vf2_hybrid_coli_7fc0_execute` recover the NUL-terminated
  byte-copy/or/store loop called from all three continuation
  sites. Direct unit proves exact step deltas (16/152/72), return
  IPs, stores and a guard control. Unwired; also settles the
  `shlo` operand order (`operands[1]<<operands[0]`) with a class
  audit of every shift modeling (all correct).

- Native `0x502a4` digit-parse helper (v0344-B) plus site-A
  wrapper wiring (v0344-C): `coli_502a4_body` +
  `vf2_hybrid_coli_502a4_execute` recover the balx-to-bx subtree
  for both sites (direct unit: exact 140/170-step deltas, exit
  regs, stores, both bx targets, fail-closed control). The wrapper
  admits scan-1 + bit-13-clear + bit-3-clear + board-clear and runs
  the helper at the cascade `bbs-9`-nt edge, fail-closing past
  bx-out with stores applied (wrapper unit: `UNSUPPORTED` +
  counter + copy bytes). Site B keeps its gate; the `0x22960+`
  continuation is next.
- Executor `dmovt` + `mulo` overflow latch (v0344-A): measured
  reg-reg double copy (exact `0x508d4` word, unit codes 54-59,
  flag-neutral) and sticky `OVERFLOW` on unsigned-64 product >
  32 bits (unblocks the `0x502a4` digit loop; architecture-
  inferred, pins-validated). Reference walks past the v0341 halt
  and exits the loop at iteration 9. Both `balx` sites traced;
  coli gates stay fail-closed until the `0x22960+` continuation
  is recovered. PUNCH `320/320`, input-17 `64/64`
  (`decomp/i960/notes/fa_coli_dmovt_v0344A.md`);

- fa_coli bit-30-clear scan-1 leaves (v0343): `+0x828` empty/bit12/
  bit13 native via `0x227ac`/`0x22794`/`0x227c4` (units
  **275/271/224**). Wrapper admits scan==1 + bit13 without the
  `0x844` condition. Fixed five latent bugs in dead code
  (bit-3 `r9 += 4`, scan re-read, `g0` threading, `r9 += r5`,
  `divr` polarity) plus the `b 0x22848` count. PUNCH `320/320`,
  input-17 `64/64`
  (`decomp/i960/notes/fa_coli_22744_leaves_v0343.md`);

- fa_player `0x19ef8` prologue (v0342, measurement): selector-bit14
  clrbit block measured (20 steps), 985-step warm-identity diff,
  g0 self-mask + `+0x1a4` handling analyzed. Recovery deferred:
  player-entry park floats are degenerate (warm faults too);
  needs a live-valid park
  (`decomp/i960/notes/fa_player_19ef8_prologue_v0342.md`);

- fa_coli `0x502a4` (v0341, measurement): two balx sites mapped
  (`0x22948` cascade, `0x22e04` bit-14 path); reference halts on
  unimplemented `dmovt` at `0x508d4`. Deferred with unblock
  recipe; both sites correctly fail-closed
  (`decomp/i960/notes/fa_coli_502a4_defer_v0341.md`);

- fa_coli `0x227dc` miss (v0340): bit-13 + bit 3 + scan 1 +
  `g7+0x844` bit 30 native for the type-8 miss chain
  (`g8+0x198 = index`, `g7+0x198 = 0x11000000`). Wrapper admits
  scan==1 only for this measured combination; early bit-3 gate
  scan-aware. Unit **87**. PUNCH `320/320`, input-17 `64/64`
  (`decomp/i960/notes/fa_coli_227dc_miss_v0340.md`);

- fa_coli `0x22d8c` g0=5 (v0339): bbs-11 edge native at all four
  `0x22e24` sites (`mov 5`, `0x230d4` fork `r3=42`, `0x23238`
  early-out, `0x22d9c` tail, `0x23070` skip). Unit **149**.
  PUNCH `320/320`, input-17 `64/64`
  (`decomp/i960/notes/fa_coli_22d8c_g05_v0339.md`);

- fa_coli `g7+0x1a4` bit 22 (v0338): native at all four `0x22e24`
  join sites (g8 bit 4 → join, bit 11 → fail). Probe 305.
  `g7+0x821=3` confirmed working (301). PUNCH `320/320`,
  input-17 `64/64` (v0338,
  `decomp/i960/notes/fa_coli_long_g7b22_v0338.md`);

- fa_coli `0x227c4` (v0337): `+0x828` bit 13 on `0x22778` path →
  diag pair + g0=1 + alt tail join. Probe 219. PUNCH `320/320`,
  input-17 `64/64` (v0337,
  `decomp/i960/notes/fa_coli_long_b13_227c4_v0337.md`);

- fa_coli bit-13 + bit 3 sub-paths (v0336): `0x22744`/`0x22778`
  native (scan 2/5/6 shared, scan==1 `0x227ac` r11*3/4 scale,
  bit 12 `0x22794` scale). `0x227dc` fail-closed (needs type-5).
  PUNCH `320/320`, input-17 `64/64` (v0336,
  `decomp/i960/notes/fa_coli_long_b13_b3_v0336.md`);

- fa_coli bit-13 `0x22794` scale (v0335): `+0x828` bit 12 set →
  r11>>=1, r9*=0.5, r8=1, cascade at `0x22918`. PUNCH `320/320`,
  input-17 `64/64` (v0335,
  `decomp/i960/notes/fa_coli_long_b13_22794_v0335.md`);

- fa_coli bit-13 sub-paths (v0334, measurement): scan 2/5/6 (310)
  and `+0x828` bit 9 clear (317) join the warm cascade; code
  implemented. Bit 13 + bit 3 = v0323 early-exit (12). PUNCH
  `320/320`, input-17 `64/64` (v0334,
  `decomp/i960/notes/fa_coli_long_b13subpaths_v0334.md`);

- fa_coli long-body bit-13 profundo (v0333): `+0x5b8` bit 0 clear
  with bit 3 clear, scan ∉ {2,5,6}, +0x828 bit 9 clear → `0x22808`
  alt tail → float tail (unit **223**). PUNCH `320/320`,
  input-17 `64/64` (v0333,
  `decomp/i960/notes/fa_coli_long_b13profundo_v0333.md`);

- fa_coli long-body `g8+0x1a4` bit 4 (v0332): four sites native
  (cascade r11*3>>1 + g0=0x23d6b, post-diag `0x22e24` join,
  `0x22e48` g0=0x2ce → `0x23238` long, miss-tail `0x1c`/`0x10`).
  Unit **301**. PUNCH `320/320`, input-17 `64/64` (v0332,
  `decomp/i960/notes/fa_coli_long_b4_v0332.md`);

- fa_coli long-body `r11>=40` (v0331): diagnostic-arm r4=8 +
  `cmpoble 30` join, unit **300**. PUNCH `320/320`, input-17 `64/64`
  (v0331, `decomp/i960/notes/fa_coli_long_r11_40_v0331.md`);

- fa_coli long-body `g8+0x1a4` bit 26 (v0330): post-diag `bbs 26
  taken` joins at `0x22e24`, `0x230d4` takes the compact path
  (unit **263**). PUNCH `320/320`, input-17 `64/64` (v0330,
  `decomp/i960/notes/fa_coli_long_b26_v0330.md`);

- fa_coli long-body `g7+0x828` bits 10/8 (v0329): bit 10 joins at
  `+0x1ac` (unit **295**), bit 8 joins at `0x22e24` (unit **291**).
  PUNCH `320/320`, input-17 `64/64` (v0329,
  `decomp/i960/notes/fa_coli_long_828b10b8_v0329.md`);

- fa_coli long-body `r11>=30` cmpoble gate (v0327): joins at
  `0x22e24` (unit **299**). PUNCH `320/320`, input-17 `64/64`
  (v0327, `decomp/i960/notes/fa_coli_long_r11_30_v0327.md`);

- fa_coli `0x43888` bit-20 non-match (v0326): `cmpobne` taken skips
  the subtract (unit **307**, g0 unchanged in the ring). PUNCH
  `320/320`, input-17 `64/64` (v0326,
  `decomp/i960/notes/fa_coli_43888_b20nm_v0326.md`);

- fa_coli long-body `g7+0x828` bit 14 (v0325): joins at `0x22e24`
  (unit **286**). PUNCH `320/320`, input-17 `64/64` (v0325,
  `decomp/i960/notes/fa_coli_long_828b14_v0325.md`);

- fa_coli long-body `g8+0x1a4` bit 14 (v0324): cascade `0x22b44`
  skip-mask and post-diagnostic `0x22dd4` counter++ are native
  (unit **289**, `d6d9==1`). PUNCH `320/320`, input-17 `64/64`
  (v0324, `decomp/i960/notes/fa_coli_long_b14_v0324.md`);

- fa_coli long-body early exit at 0x230a0 (v0323): `g8+0x1a4` bits
  3/15/16 with the measured `g7+0x821` gate now take the counter--
  path (unit bit16 **16**). Insert preserves the warm `+2 +4`
  accounting. PUNCH `320/320`, input-17 `64/64` (v0323,
  `decomp/i960/notes/fa_coli_early_exit_v0323.md`);

- fa_coli diagnostic-arm gates (v0321): bit 18 skip (247), r11=20
  r4-offset (305), `g8+0x1b1==9` second pair (354), `g7+0x823`
  table-walk loop (353). Fix `g7+0x820` table select (`0x230bc` when
  not 5/6). PUNCH `320/320`, input-17 `64/64` (v0321,
  `decomp/i960/notes/fa_coli_diag_gates_v0321.md`);

- fa_coli diagnostic cascade completion (v0320): `0x439ac` multi-trip
  scan (`count>=4` early-out, table match, count=1/3 no-match),
  `0x43888` correct store effects (`33`/`0x421` + ring), gate&12
  branch-byte siblings, bit-20 subtract+`shli` at `0x22e74`. Unit
  shapes 302/291/295/306/285/305/309. PUNCH `320/320`, input-17
  `64/64` (v0320, `decomp/i960/notes/fa_coli_diag_v0320.md`);

- fa_coli long-body `r11 != 0` packing + diagnostic arm (v0319):
  `0x22640` scanbit pack plus the `0x439ac`/`0x43888` diagnostic
  cascade when `cmpobe 0,r11` is not taken. Unit shape
  `r11b=1` **302/7/8**. PUNCH `320/320`, input-17 `64/64` (v0319,
  `decomp/i960/notes/fa_coli_long_r11_v0319.md`);

- fa_coli long-body bit-4-only and bbs-15-taken siblings (v0318b/c):
  bit 4 without bit 12 (**251**); bits 4+12 with `bbs 15` taken
  skips the ×0.5 scale (**256**). Fix `subi` operand order in the
  packed halfword (`r10 - half_c + (1<<14)`). PUNCH `320/320`,
  input-17 `64/64` (v0318,
  `decomp/i960/notes/fa_coli_long_b4_bbs15_v0318.md`);

- fa_coli long-body `g8+0x1a4` bit 13 early-join (v0318): admit
  bit 13 when `+0x5b8` bit 0 is set (+2 → **251**). Correct the
  `0x22a28` cascade gate to test bit 8 as in the ROM. PUNCH
  `320/320`, input-17 `64/64` (v0318,
  `decomp/i960/notes/fa_coli_long_b13_v0318.md`);

- fa_coli long-body `g7+0x1a4` bits 4+12 scale sibling (v0318):
  admit bit 4 set when bit 12 is also set — halfword check at
  `+0x5c2` and ×0.5 `mulr` into `+0x2c`/`+0x34` (**263** on the
  v0288 drive, +14). PUNCH `320/320`, input-17 `64/64` (v0318,
  `decomp/i960/notes/fa_coli_long_b4b12_v0318.md`);

- fa_coli `0x18bd4` notbit-15 sibling (v0317): admit `g8+0x19c`
  bit 15 set — flip bit 15 of the `walk+1` halfword before the
  `(17<<24)` pack. Type22 unit shape **54/3/4**. PUNCH `320/320`,
  input-17 `64/64` (v0317,
  `decomp/i960/notes/fa_coli_18bd4_notbit15_v0317.md`);

- fa_coli `0x18b58` bit-2-set FIFO path (v0317): recover the
  `0x2d805b5b` delta (bodies 2/29/13) with float `subr` into
  `g7+0x18`/`+0x20` and the common `g7+0x84` tail. Type-22 shortcut
  with bit 2 set completes **80/3/4**. PUNCH `320/320`, input-17
  `64/64`, ctest `56/56` (v0317,
  `decomp/i960/notes/fa_coli_18b58_fifo_v0317.md`);

- fa_coli type-22 shortcut `0x18bd4` (v0316): admit `g8+0x19f==22`
  in `coli_225cc_body` — `0x1ab34` type-5 walk + `0x18b58` bit-2
  early-out, parent stores and `chkbit`/`alterbit` word update.
  First-hit unit shape **53/3/4**. PUNCH `320/320`, input-17
  `64/64`, ctest `56/56` (v0316,
  `decomp/i960/notes/fa_coli_18bd4_type22_v0316.md`);

- fa_coli mid-body tie-break `0x2227c` (v0315): recover the double
  `0x225cc` call when both bit-15 are clear and `f1+0x822 <=
  f0+0x822` (compact-both **282/7/8**). Pair-greater 258/5/6
  unchanged. `0x18bd4` shortcut remains DEFER (v0291). PUNCH
  `320/320`, input-17 `64/64`, ctest `56/56` (v0315,
  `decomp/i960/notes/fa_coli_tiebreak_2227c_v0315.md`);

- fa_coli long body `0x225cc` (v0314): admit the measured 248-step
  path (bit 3 clear) as native C — flags region with scanbit float
  pack (`be` not taken after non-publishing `cmpibl`), cascade
  all-clear 60, `0x230d4` long via body-only helper, `0x23238` ×2,
  `0x1ab34` miss, miss tail, and float FIFO tail. Completes
  **249/4/5**. Compact bit-3 sibling unchanged. `0x2227c` tie-break
  and `0x18bd4` shortcut remain fail-closed. PUNCH `320/320`,
  input-17 `64/64`, ctest `56/56` (v0314,
  `decomp/i960/notes/fa_coli_225cc_long_v0314.md`);

- fa_coli helper `0x23238` float-threshold path (v0313): complete the
  helper with `g8+0x1f8` compares (~0.9 / ~0.6) → `g0` in
  `{0x2ce, 0xa7, 0x2cf}` (bodies 6/10/11). Early-out body 2 unchanged.
  PUNCH `320/320`, input-17 `64/64`, ctest `56/56` (v0313,
  `decomp/i960/notes/fa_coli_23238_float_v0313.md`);

- fa_coli table-walk helper `0x1ab34` (v0312): recover the measured
  type-chain walk (`g0 & 0x1fff` → main-data `0x0200d34c`, sizes from
  ROM `0x1b7f6`). Two-iteration miss body 16 (`g0=0`); first-hit match
  body 6 (`g0=record+8`). PUNCH `320/320`, input-17 `64/64`, ctest
  `56/56` (v0312, `decomp/i960/notes/fa_coli_1ab34_v0312.md`);

- fa_coli helper `0x230d4` bit-26 compact path (v0311): recover the
  measured index/table select (`g7+0x82a`, `g7+0x26`, `g8+0x5b4`,
  `g8+0x142` bit 15) with body 15 and `g0` from main-data
  `0x0201cc54`/`0x0201cc48`. Long `0x2312c` path remains fail-closed.
  PUNCH `320/320`, input-17 `64/64`, ctest `56/56` (v0311,
  `decomp/i960/notes/fa_coli_230d4_v0311.md`);

- fa_coli helper `0x23238` early-out (v0310): recover the measured
  `g0 != 0x2ce` path (body 2, `g0` unchanged) called twice from the
  long `0x225cc` body. The `g0 == 0x2ce` float-threshold path remains
  fail-closed. PUNCH `320/320`, input-17 `64/64`, ctest `56/56`
  (v0310, `decomp/i960/notes/fa_coli_23238_v0310.md`);

- Fase 8 tooling + 0x19ef8 live-drive measurement (v0309):
  `frontier.py` ranks call/bal edges with source/target function
  attribution (`rank_call_edges`, unit-tested). Live hybrid bit-5
  from `punch10` fails closed at `0x16464` before the first compared
  block; reference park replay still cannot leave the `0x10fa0` wait.
  Documented the `vf2cycles` `.vf2snap.runtime` sidecar requirement.
  No C recovery (v0309,
  `decomp/i960/notes/fa_player_19ef8_live_v0309.md`);

- fa_coli `g8+0x26` FIFO cursor (v0308): recover the measured path
  that writes `-delta` into `0x90e000[byte cursor]`, advances the
  cursor by 4, and emits command-port `0x36806d6d` before the shared
  `0x03000606` header (one-hit slot-0 body **79**). PUNCH `320/320`,
  input-17 `64/64`, ctest `56/56` (v0308,
  `decomp/i960/notes/fa_coli_fifo_cursor_v0308.md`);

- fa_coli cascade pair-greater arm (v0307): when both `+0x804` bit 15
  are clear, admit `cmpobg` taken on `fighter1+0x822 > fighter0+0x822`
  → swapped compact `0x225cc` (measured **258/5/6**). The `bl`/`bg`
  arms do not inherit the compare in the reference executor and stay
  fail-closed along with the `0x2227c` tie-break. PUNCH `320/320`,
  input-17 `64/64`, ctest `56/56` (v0307,
  `decomp/i960/notes/fa_coli_cascade_pair_v0307.md`);

- fa_coli slot-1 contact scan + both-hit cascade early-out (v0306):
  recover the measured 15-trip `bbc`/`setbit` slot-1 loop in
  `0x22404` (body 133, `g0=1`) and admit both contacts hit when the
  cascade early-outs at `0x22258` (fighter1 `+0x804` bit 15, measured
  **255/5/6**). The `0x2227c` tie-break that invokes long `0x225cc`
  twice remains fail-closed. PUNCH `320/320`, input-17 `64/64`,
  ctest `56/56` (v0306,
  `decomp/i960/notes/fa_coli_cascade_v0306.md`);

- fa_coli midbody second-contact `g0=1` (v0305): admit first contact
  warm / second contact hit, then compact `0x225cc` via the no-restore
  jump at `0x22240` (`g7=fighter1`, `g8=fighter0`, measured **130/5/6**).
  Second-contact `coli_22404_body` now plants the swapped CPU `g7/g8`
  so the hit-path exclude/store use `g8=fighter0`. Both-non-zero
  cascade at `0x22244` remains fail-closed. PUNCH `320/320`,
  input-17 `64/64`, ctest `56/56` (v0305,
  `decomp/i960/notes/fa_coli_second_contact_v0305.md`);

- fa_coli `0x225cc` compact bit-3 sibling + midbody non-zero tail
  (v0304): recover the 12-insn early-out (`g8+0x1a4` bit 3 set,
  counter++/-- at `g7+0x1234`) and admit midbody when the first
  contact returns `g0=1` and the second is warm (measured **132/5/6**).
  Long `0x225cc` body, `0x18bd4` shortcut, and second-contact `g0!=0`
  remain fail-closed. Warm midbody `56/4/5`, PUNCH `320/320`,
  input-17 `64/64`, ctest `56/56` (v0304,
  `decomp/i960/notes/fa_coli_225cc_bit3_v0304.md`);

- fa_coli `0x22404` non-empty contact sibling (v0303): recover the
  measured bit-8-set path that returns `g0 = 1` after `andnot` with
  `g8+0x6dc` (body 72 on the one-hit scan, polygon FIFO via
  `(g11)[g12]`, pending setbit, `g9 = 0x01000550`). Slot 1 scan and
  `g8+0x26 != 0` remain fail-closed. Mid-body tail still requires both
  contact results zero. PUNCH `320/320`, input-17 `64/64`, ctest
  `56/56` (v0303, `decomp/i960/notes/fa_coli_contact_g0_v0303.md`);

- Fase 7 closed: measured taint on `fa_player` `0x29414` confirms
  `branch 0x0002949c depends on fighter0 + 0x01a4 bit 19` (path A/B,
  float tail, type 8). Consolidate fighter layout candidates in
  `include/vf2/fighter_candidate.h` with coli mid-body bilateral
  offsets (`+0x4`, `+0x1a8`, `+0x6dc`, `+0x821`) and `0x29414` g7
  corridor offsets (`+0x84`, `+0x17c`, `+0x18a`, `+0x1aa`, `+0x1b1`,
  `+0x0614`, `+0xc50`). No semantic renames. PUNCH `320/320`,
  input-17 `64/64`, ctest hold (v0302,
  `decomp/i960/notes/taint_29414_v0302.md`,
  `decomp/i960/notes/fighter_candidate_layout_v0302.md`);

- Close input-17 cycle-3 residual: match-latch index10 state1 stores
  measured `r20` (`0x00560000`) at work-ram `0x005ff600`. The endurance
  pin extends from `2/2` to **`64/64` MATCH** (2368 blocks /
  2,428,988 insns, both `0x1645c`). PUNCH `320/320` and ctest `56/56`
  hold (v0301c);

- Preserve entry `r14` on match-latch index10 state1 (successive frames
  carry 7, 8, …). Cycle 3 now fails on work-ram (`0xff600`) instead of
  a register. The 2-cycle input-17 pin and PUNCH stay MATCH (v0301b);

- Fase 6 pin extended: recover index10 state1 (`a5==1`, navigation==0)
  for the match latch (36756/1438, `r14=7`, EQUAL).
  `vf2cycles --input 17 --cycles 2` from `in17-c1` is now **2/2 MATCH**
  (74 blocks / 42,696 insns, both `0x1645c`). A third cycle still
  fails closed on a later frame (`r14` 8 vs 7). PUNCH `320/320` and
  ctest `56/56` hold (v0301,
  `decomp/i960/notes/fa_player_input17_index10_v0299.md`);

- Fase 6 pin closed: recover `phase17 bit7_index10` match-latch sibling
  for `--input 17` (`input=previous=0x0f002100`, released=0). State0
  accounts 1677 instructions / 33 calls, poststate `r14=6` + GREATER,
  header text `LOSE(%)`. Skip forcing EQUAL in
  `set_main_final_cluster_condition` for phase `0x8a`.
  `vf2cycles --input 17 --cycles 1` from `in17-c1` is **1/1 MATCH**
  (37 blocks / 3,810 insns, both `0x1645c`) — second endurance pin,
  complementary to PUNCH. Next frame (`a5==1` state1 with match latch)
  remains fail-closed. PUNCH stays `320/320` MATCH and ctest `56/56`
  (v0300, `decomp/i960/notes/fa_player_input17_index10_v0299.md`);

- Fase 6A/6B partial: attribute the input-17 cycle-2 boundary to
  `phase17 bit7_index10` (`0x8a`) and admit the measured match-latch
  sibling `input=previous=0x0f002100, released=0`. Account the match
  state0 corridor as 1677 instructions (1650 `0x5f234` body + 27 for
  the `0x10b5c`/`a6c0` shell) and use the measured poststate (`r14=6`,
  AC `...002`, CC EQUAL). Cycle 2 now matches IP and instruction totals
  (`3575/3575`, both `0xa010`) but still fails a 6-byte `cpu-state`
  compare, so the endurance pin remains open. PUNCH stays `320/320`
  MATCH and ctest `56/56` (v0299,
  `decomp/i960/notes/fa_player_input17_index10_v0299.md`);

- Fase 6 scouting: holding input 17 or 18 from `sixth-regen` opens a
  corridor distinct from PUNCH. Cycle 1 MATCHes; cycle 2 fails closed
  at the `frame_dispatch_tick` `callx` (`0xa6c0` table `0xa6f8[r3*4]`)
  with reference at `0x9ff8`. Parks and the measured split are recorded
  in `decomp/i960/notes/fa_player_input17_fase6_v0298.md`. No C recovery
  and no second pin yet; PUNCH stays `320/320` MATCH.

- fa_player `0x180bc` flag tail and `0x1441c` epilogue: recover the
  measured warm/sibling paths so the first-dispatch player task no
  longer uses `hybrid_execute_interpreted_task` from `0x180bc`.
  `0x180bc` writes `+0x5b4`, player-flags bit 8 (via `+0x1a4` bits 3/6,
  `+0x810` bit 7 and the signed `cmpoble` on `+0x1aa` vs `+0x800>>1`)
  and optionally `+0x6d8`. `0x1441c` sets flags bit 7 and `ret`s the
  player task. Also record that `vf2_i960_run=vf2_hybrid_i960_run_tail`
  already owns `0x28178`/`0x17710`/`0x1791c`/`0x4b640` inside
  `hybrid_execute_interpreted_until`. PUNCH stays `320/320` MATCH /
  `14,962,620` insns and ctest `56/56` (v0298,
  `decomp/i960/notes/fa_player_180bc_v0298.md`);


- fa_player `0x29414`: recover the measured type-6/8/10 bit-19-set
  siblings. The `+0x1aa` halfword is compared **unsigned** and the
  disassembly operand order makes `cmpobl 20,r12` mean `r12 > 20`, so
  path A at `0x294ac..0x294f4` is the reachable window `11..20` (indexed
  `(g11)[g12]` stores, `r9 += r10*r11`, optional `+0x18a`/`+0x17c`
  halfword adds of `r13*r11`, then the float tail). Path B (`window > 20`)
  early-returns on board bit 5, missing `+0x614 & 0x9000`, or
  `50 < window`, otherwise adds the type-constant `r8` to the halfwords
  and never stores `+0xc50`. Type 8 is +2 and type 10 is -1 instructions
  versus type 6. PUNCH stays `320/320` MATCH / `14,962,620` insns and
  ctest `56/56` (v0297,
  `decomp/i960/notes/fa_player_29414_bit19_v0297.md`);


- fa_player `0x29414`: recover the measured type-6/8/10 compact path
  with state-flag bit 19 clear — store `(r9 * scale - scale)` at
  `+0xc50` using the ROM constant sets at `0x29478` (types 6/10) and
  `0x29430` (type 8); type 0 zero-path unchanged; bit-19-set siblings
  stay fail-closed. Add `vf2probe --set-ip`, accept `0x14288` as a
  hybrid player mid-corridor continuation, export
  `vf2_hybrid_player_29414_execute`, and unit-test the measured
  instruction counts and float stores. PUNCH stays `320/320` MATCH /
  `14,962,620` insns and ctest `56/56` (v0296,
  `decomp/i960/notes/fa_player_29414_types_v0296.md`);


- fa_player `0x19ef8` siblings and `0x29414` non-zero: defer after
  measurement blockers — interpretive replay from `player-14288-rt`
  faults at `0x2704c` even on baseline (980 insns), and `vf2probe`
  cannot exit the `0x10FA0` vblank wait loop without the IRQ path
  `vf2cycles` uses; guards and the `0x29414` zero-path C stay
  fail-closed (v0295,
  `decomp/i960/notes/fa_player_19ef8_29414_defer_v0295.md`);

- fa_player `0x19ef8`: replace the manual `0x1a1e4` selector-setup
  block inside `hybrid_execute_player_19ef8` with the recovered
  semantic interpreter `player_selector_execute_setup`; keep the
  `0x505`/`0x284` caller guard, the post-interpreter `+0x1a8`/`+0x1aa`
  stores, the `0x26ef0` scratch expand and the `0x1428c` endpoint;
  PUNCH stays `320/320` MATCH / `14,962,620` insns and ctest `56/56`
  (v0294, `decomp/i960/notes/fa_player_1a1e4_integration_v0294.md`,
  `docs/PLAYER_19EF8_INTEGRATION_BOUNDARY.md`);

- fa_player drive bases: confirm fighter slots `0x510800`/`0x510980`
  on the player parks, document that the `0x29414` parks never
  reached the target (reference loops at `0x10F98`) and that
  interpretive replay from `player-14288-rt` faults at `0x2704c` even
  on baseline (v0293,
  `decomp/i960/notes/fa_player_drive_base_v0293.md`);

- fa_player frontier: record the measured next targets after the coli
  closure — `0x19ef8` flag-bit siblings, `0x29414` non-zero path,
  post-`0x28780` geometry helpers, then physics/hitboxes; tooling
  (`frontier.py` R/W + call targets, `taint.py`, `infer_structs.py`)
  validated on the coli mid-body trace (v0292,
  `decomp/i960/notes/fa_player_next_targets_v0292.md`);

- coli `0x225cc` shortcut: measure the `g8+0x19f==22` path — it does
  `call 0x18bd4`, which itself calls `0x1ab34` (ROM table walk) and
  `0x18b58`; not a compact leaf; verdict defer (v0291,
  `decomp/i960/notes/fa_coli_225cc_shortcut_v0291.md`);

- coli bit-8-set siblings: admit two measured compact shapes —
  `0x22298` bits 8+1 set (8 insns, same store 0) and `0x22404` bit 8
  set with equal snapshots / empty scan mask (30 insns, store 0 into
  `g8+0x6d4`, `g0 = 0`); body-only statics return dynamic instruction
  counts; other bit-8 sub-branches remain boundaries; PUNCH stays
  `320/320` MATCH (v0290,
  `decomp/i960/notes/fa_coli_bit8_siblings_v0290.md`);

- coli mid-body/tail: recover the remaining warm-path glue from
  `0x22210` through the both-zero exit to `0x10dcc` as one native
  procedure — two `0x22298` bitmask calls, two `0x22404` contact-query
  calls and the `cmpobe`/`ret` tail; `hybrid_execute_coli_body` now
  walks interpret-entry → native-shell → native-midbody; the warm
  `fa_coli` task runs with zero interpreted instructions after the
  7-insn entry prefix; PUNCH corridor stays `320/320` MATCH /
  `14,962,620` instructions; unit test `test_coli_midbody_tail_warm`
  (v0289, `decomp/i960/notes/fa_coli_midbody_v0289.md`);

- coli `0x225cc`: measure the v0282 reachability drive — the `0x18bd4`
  shortcut is not taken; the path is a 248-instruction multi-branch
  body with four nested calls through `0x230b8` (not a compact
  prefix); verdict defer (v0288,
  `decomp/i960/notes/fa_coli_225cc_prefix_v0288.md`);

- coli poly shell: recover the full `0x23524` warm shell as one native
  procedure — 179-insn shell plus `bal 0x23694` body, with `0x2396c`×2 /
  `0x233d0` / `0x238a4`×2 / `0x238f8` / `0x2364c` inlined for
  accounting; `hybrid_execute_coli_body` collapses to a single native
  entry; PUNCH corridor stays `320/320` MATCH / `14,962,620`
  instructions; unit test `test_coli_23524_shell` (v0287,
  `decomp/i960/notes/fa_coli_23524_shell_v0287.md`);

- coli poly shell: measure the remaining 179-insn `0x23524` warm shell
  glue-by-glue (prologue FIFO `0x1f003e3e`, 16-word `g13+0x40` clear,
  call sequence, `bal 0x23694` body with `0x2364c`, FIFO `0x1e803d3d`
  push, fighter `+0x18/+0x20` update, `+0x650` clamp); verdict GO for
  native recovery (v0286,
  `decomp/i960/notes/fa_coli_23524_shell_measure_v0286.md`);

- coli poly child: recover the dominant `0x2396c` warm poly-cluster
  builder as native C — two invocations copy 30 remapped `stq` triples
  into `g7+0xd00`, run the threshold/inner/max scans, and inline three
  `0x23878` bit-remaps into `+0x624/+0x614/+0x618` (**2618** own +
  **405** nested per invocation); warm never takes `+0x110`/`+0x114`
  setbits, the inner positive/min paths, or the max-update path (those
  fail closed); `hybrid_execute_coli_body` replaces the six-step
  bitremap walk with two native `0x2396c` entries; PUNCH corridor stays
  `320/320` MATCH / `14,962,620` instructions; unit test
  `test_coli_2396c_poly_cluster` (v0285,
  `decomp/i960/notes/fa_coli_2396c_v0285.md`);

- coli poly children: recover the remaining small `0x23524` callees
  `0x233d0` and `0x2364c` as native C — flag builder copies the ROM row
  at `0x232c4` into `g13+0xb4..` and leaves `g6 = 0` (**44 instructions**);
  FIFO delta push writes command `0x18003030` plus three zero deltas to
  `0x884000` and stores the three replies at `g13+0xc8` (**17 instructions**);
  magic `+0x1a8` states, unmeasured bit tests and non-zero `+0x1f4` fail
  closed; `hybrid_execute_coli_body` stops at `0x233d0` / `0x2364c`;
  PUNCH corridor stays `320/320` MATCH / `14,962,620` instructions; unit
  tests `test_coli_233d0_flag_builder` and `test_coli_2364c_fifo_delta`
  (v0284, `decomp/i960/notes/fa_coli_small_leaves_v0284.md`);

- coli measure: reconfirm the warm PUNCH corridor (`320/320` MATCH,
  `12946` blocks, `14,962,620` instructions) and attribute every remaining
  interpreted block inside `0x23524` — `0x2396c` warm path is fixed
  (six never-taken blocks; own cost **2618** ×2 plus three `0x23878`
  bodies per invocation), `0x233d0` is a 44-insn single-path ROM-table
  copy into `g13`, `0x2364c` is a 17-insn FIFO delta push to `0x884000`;
  all three are **GO** for native recovery; no C in this slice
  (v0283, `decomp/i960/notes/fa_coli_2396c_measure_v0283.md`);

- i960 + coli frontier: fix `bno` after `scanbit` (a hit must not fire NoBit; miss sets dest=31 and NONE) and record a measured drive to the previously unreachable `0x225cc` resolver — three-field mutation (`g7+0x1a4` bit 8, `g7+0x820=1`, dest `0x5149cc=0xffff`) makes the first contact query return `g0=1` in **73 instructions** and the caller reach `0x225cc` in **96**; PUNCH corridor unchanged `320/320` MATCH; executor unit test pins scanbit hit/miss (v0282, `decomp/i960/notes/fa_coli_225cc_drive_v0282.md`);

- coli poly child: recover the `0x238f8` nested bit-scan as native C — 30×30 loop over buffer-RAM source masks at `0x91f880`, remap bytes at ROM `0x23284`, stores into `g13+0x40`; warm PUNCH source is all-zero (**2855 instructions**, **0** stores); non-zero source executes the measured `setbit`/`st` path; `hybrid_execute_coli_body` stops at `0x238f8` after the `0x238a4` pair and resumes at `0x23644`; whole-task pin stays `9214/18/19` and the PUNCH corridor `320/320` MATCH; unit test `test_coli_238f8_warm_noop` (v0281);

- coli poly child: recover the `0x23878` bit-remap helper as native C — six PUNCH-driven invocations remap bits `0..29` of `g3` through the main-data table at `0x02007b76` (30 words); empty source **95 instructions**, full source **155**, single-bit **97**; no memory writes; table entries `>= 32` fail closed; `hybrid_execute_coli_body` walks the six call sites inside both `0x2396c` invocations before the `0x238a4` pair; whole-task pin stays `9214/18/19` and the PUNCH corridor `320/320` MATCH; unit test `test_coli_23878_bit_remap` (v0280);

- coli poly child: attribute the 9,158-instruction warm `0x23524` path by call-stack walk and recover the first leaf `0x238a4` as native C — cost split `0x2396c` 5236 (×2), `0x238f8` 2855 (source mask `0x91f880` all-zero), `0x23878` 810 (×6 bit-remap), shell 179, `0x233d0` 44, `0x2364c` 17, `0x238a4` 10 (×2); both `0x238a4` invocations take `g7+0x1a4` bit 8 clear → `g3 = 0`, **5 instructions** / `0` calls / `1` return; bit 8 set fails closed; `hybrid_execute_coli_body` now stops at `0x23524`/`0x238a4` and native-recovers the pair before the v0276/v0277 children; whole-task pin stays `9214/18/19` and the PUNCH corridor `320/320` MATCH; unit test `test_coli_238a4_early_path` (v0278/v0279, `decomp/i960/notes/fa_coli_23524_attribution_v0278.md`);

- coli child: recover the second mid-body `fa_coli` callee `0x22404` as native C — both PUNCH-driven invocations take the measured early exit (`g7+0x1a4` bit 8 clear → snapshot `g7+0x1a8` into `g13+0x8c[slot]`, clear slot bit in `g13+0x90`, `g0 = 0`), **14 instructions** / `0` counted calls / `1` return (`bal 0x225bc`/`bx` are not counted); bit 8 set and slot `> 1` fail closed; `hybrid_execute_coli_body` now native-recovers `0x22298` ×2 and `0x22404` ×2; the warm tail does **not** reach `0x225cc` (both results zero skip `0x22290`); whole-task pin stays `9214/18/19` and the PUNCH corridor `320/320` MATCH; unit test `test_coli_contact_query_22404_early_path` (v0277, `decomp/i960/notes/fa_coli_contact_22404_v0277.md`);

- coli child: recover the first mid-body `fa_coli` callee `0x22298` as native C and introduce hybrid body segmentation — both PUNCH-driven invocations take the measured early exit (`g8+0x1a4` bit 8 clear → `stos 0` at `g7+0x6dc`), **7 instructions** / `0` calls / `1` return, local regs discarded by frame restore; bit 8 set fails closed; `hybrid_execute_coli_body` now interprets the `0x23524` subtree, native-recovers each `0x22298`, then interprets the tail, keeping the whole-task pin `9214/18/19` and the PUNCH corridor `320/320` MATCH (`14,962,620` instructions); unit test `test_coli_bitmask_22298_early_path` (v0276, `decomp/i960/notes/fa_coli_bitmask_22298_v0276.md`);

- coli gate: recover the measured `fa_coli` bit-5-set early return as native C — `ld 0x508000` / `bbs 5` / `ret` (`0x221e8→0x22294→0x10dcc`), **3 instructions**, `0` calls / `1` return, no stores; record the warm `0x23524` call boundary (**9,158 instructions**, **14** nested calls, `bbc 0, g6` not taken, 468 stores, both-fighter `+0xe80..+0xf04` clusters) as the package for the next native child; bit-5-clear keeps the v0274 `9214/18/19` bridge (v0275, `decomp/i960/notes/fa_coli_gate_23524_v0275.md`);

- coli body: admit the measured PUNCH-driven `fa_coli 0x221e8` warm body as an explicit original-i960 bridge — runtime bit 5 clear, `9,214` instructions, `18` calls / `19` returns through `0x10dcc`, fighters from `0x500804`/`0x500808`; handle literal `movt 0, r8` (`0x236b8`, encoding `0x5e401e00`) in `vf2_i960_step_legacy` (the step `vf2i960_run`/`vf2probe` actually bind) and strengthen the unit test so a register-register copy of zeroed r0..r2 cannot false-pass; the PUNCH corridor now completes `320/320` cycles / `12,946` blocks / `14,962,620` instructions MATCH back to `0x1645c` (was 313 then fail-closed); bit-5-set and native callees `0x23524`/`0x22298`/`0x22404`/`0x225cc` remain open (v0274, `decomp/i960/notes/fa_coli_body_v0274.md`);


- scheduler/coli: reach recurring `fa_coli 0x221e8` via a PUNCH-driven warm boot and recover the scan-to-entry prefix — holding PUNCH (`--input 16`) from the sixth-dispatch snapshot runs the phase-11 countdown `0x00500024` to its terminal at 1/frame, clears the phase flag (`0x8b -> 0x0b`) and arms slot 10 (`entry=0x221e8`, flags bit 31) as the first runnable descriptor of the 29-task recurring sweep; admit `0x221e8` in the scheduler task allowlist with per-index scan accounting `27 + 16*index` (235 for index-13 game_info, 187 measured for index-10 coli) and preserve caller-carried r0 (never written on the `0xa010 -> callx` path: `0x005ff500` cold vs `0x005ff640` warm); the strict per-block step is exact (187/187 instructions, 4/2 calls/returns, both sides at `0x221e8`) and stops at the first unrecovered coli-body block; also handle zero-instruction FRAME_WAIT steps in the per-block/probe runners (mirror of the single-step path — the warm terminal's forced post-boot inject) and zero-init `frame_wait_before` (warning-as-error build fix); synthetic `test_scheduler_selects_coli_entry_at_index10` pins 187/4/2 plus the fail-closed body boundary (v0273, `decomp/i960/notes/fa_coli_entry_v0273.md`);

- test: retarget `vf2_native_seventh_dispatch` to `vf2_native_eleventh_dispatch` (`native-nth-dispatch 11`); the strict sixth-dispatch base now ends at the tenth `fa_game_info` entry (`8` repeated scheduler entries, `870` blocks / `7,404,901` instructions), so target `7` failed closed with `Checkpoint represents dispatch 10` while dispatches 7-10 are already covered per-block inside the sixth command — the continuation now proves one further `37`-block / `2,166`-instruction cycle to dispatch 11 with exact CPU/memory/counter state;
- input/runtime: fix Model 2A P1/P2 active-low double inversion, add snapshot-safe `--input` driving to `vf2probe`/`vf2cycles`, recover the newly exposed interrupt fighter-compare prefix plus `0x1284` callback-table bit-index semantics, and close phase-17 index-11 positive countdown through the terminal `0xb0` reset; strict dispatch baselines remain zero-interpreter at 42/78/830/866 blocks for third/fourth/fifth/sixth, with 318 additional phase-11 cycles MATCH. Warm reboot now preserves measured LESS/EQUAL/NONE condition states through post-boot/timer/input-IRQ boundaries; the remaining frontier is frame-wait IRQ boundary granularity (`0xbc0` native vs `0xbc8` reference) after the terminal reset (v0270, `decomp/i960/notes/input_polarity_phase11_v0270.md`);
- object tasks: recover the five `fa_object` handler continuations (`0x6cae0→0x6caf0`, `0x6caf4→0x6cb04`, bare rets `0x6caf0/0x6cb04/0x6cb08`) in `hybrid.c` plus the `native_runtime.c` router entries; all six dispatcher/handler cases are exact for full CPU/condition/frame/counter state and full work-RAM `memcmp` via synthetic-state differential (`tests/recovered/test_object_handlers.c`, `vf2_object_handlers_differential`); the `0x6ca84` service loop and `fa_coli` recurring `0x221e8` family remain open (v0268, `decomp/i960/notes/object_handlers_v0268.md`);
- coli hunt: `0x221e8` unreachable in boot/attract (61M+ insns, 5 windows, flag/entry forcing, fast-path hijack all negative with identical counters) — full-sweep/fast-path/single-pass mechanism mapped, recurring needs gameplay frames via driven inputs; passive tooling added (`vf2probe --raise-irq/--enter-interrupt`, `resume-trace` injection args, `VF2_PARK_SNAPSHOT` observe parking with post-second boundary `out-postsecond.vf2snap`) (v0269, `decomp/i960/notes/coli_recurring_hunt_v0269.md`);
- coli scout: regenerated fifth-dispatch snapshot (`out-fifth.vf2snap`, MATCH, 836 blocks) and proved flag/entry mutation cannot reach `fa_coli` `0x221cc/0x221e8` from fifth/sixth windows (identical `10255/10254` call/return counters over ~15.3M insns; control run included) — accepted corridor never sweeps index 10 there; next attempt is `snapshot` + `native-resume` into the second-dispatch initializer corridor (`decomp/i960/notes/object_handlers_v0268.md`);
- game_info positive: admit base `0x10040` any-composition `2^20*128=134,217,728` masks `-4/-7` plus work-RAM `0x510b24/0x512b24|=0x800` (bare `0x10040`, singles `0x10240/0x11240`, bit21 `0x00210040`, many `0x30040/0x50040` all `36/36`) total `671,090,391→805,308,119` (v0264, `decomp/i960/notes/game_info_18644_positive_base10040_any_v0264.md`);
- game_info positive: close low `0x40` family — admit `0x14040` `-2/-3`, `0x18040` `-4/-8` + work-RAM `0x510b24/0x512b24`, `0x1C040` `-2/-4` each `134,217,728` masks, total `805,308,119→1,207,961,303` (v0265-v0267, `decomp/i960/notes/game_info_18644_positive_low_family_closure_v0265_v0267.md`);
- game_info positive: admit base `0xC040` any-composition `2^20*128=134,217,728` masks `-2/-4` (bare `0xC040`, single `0xC240`, bit21 `0x0020C040`, many `0x1BDECE49` all `36/36`) total `536,872,663→671,090,391` (v0263, `decomp/i960/notes/game_info_18644_positive_baseC040_any_v0263.md`);
- game_info positive: admit base `0x8040` any-composition `2^20*128=134,217,728` masks `+3/+6` (bare `0x8040`, singles `0x8240/0x8440`, bit21 `0x00208040`, many `0x1BDE8E49` all `36/36`) total `402,654,935→536,872,663` (v0262, `decomp/i960/notes/game_info_18644_positive_base8040_any_v0262.md`);
- game_info positive: admit base `0x4040` any-composition `2^20*128=134,217,728` masks `-2/-3` (bare `0x4040`, single `0x4240`, high `0x44040`, bit21 `0x00204040`, many `0x1BDE6E49` all `36/36`) total `268,437,207→402,654,935` (v0261, `decomp/i960/notes/game_info_18644_positive_base4040_any_v0261.md`);
- game_info positive: admit base `0x40` any-composition `2^20*128=134,217,728` masks `+3/+7` (bare `0x40`, singles `0x240/0x840`, doubles `0x1840`, bit21 `0x00200040`, bit21+Mp `0x00200240/0x1BFE3EE9` etc all `36/36`) total `134,219,479→268,437,207` (v0260, `decomp/i960/notes/game_info_18644_positive_base0040_any_v0260.md`);
- game_info positive: generalize 7×112 bit21 low masks to full middle-high set `0x1B7E3EA9` (20 bits) → `0x1BFE3EA9` (21 bits, incl. bit23 `0x00800000` via `0x17b68→0x17fe8` bridge `0x30/0x1c=0, 0x620=1` 28/30 vs 31, `~0xFFFE3EBF`) with same per-base `−3/−5`/`+2/+5`/`+4/+8`/`+4/+9` accounting — representative 40 single/multi-bit combos `36/36 exact` (v0254, `decomp/i960/notes/game_info_18644_positive_middle_high_v0254.md`);
- game_info positive: generalize base `0x140` single-middle `2432` masks to any-Mp `Mp=0x1BDE3EA9 !=0` (`2^19-1=524287` combos) with/without bit21 `+3/+6` — double `0xB40/0x1940`, quad `0x3D40`, high `0x60140/0x8000340`, bit21+Mp `0x00200340/0x08200340` etc all `36/36`, total `4439→134,219,479` (`134,217,472` new, bare/pure-bit21 stay `0/0`) (v0259, `decomp/i960/notes/game_info_18644_positive_base0140_any_middle_v0259.md`);
- game_info positive: admit base `0x140` single-middle `19*128=2432` masks `+3/+6` (any one `Mp=0x1BDE3EA9` bit, bare/pure-bit21 `0/0`) total `2007→4439` (v0258, `decomp/i960/notes/game_info_18644_positive_base0140_single_middle_v0258.md`);
- game_info positive: fix bare pure-bit21 `0x00208140` etc (7 bases ×16 outer) from `0/36` DIFF `-3/-5` to `36/36` `0` excess via `low!=0 || middle&0x1BDE3EA9` guard (v0257) — total `1895→2007` positive masks, bare `0x00808140` etc stay `36/36`;
- game_info positive: admit bare middle-high variants — remove `low !=0` guard so any middle `0x1BFE3EA9` with `outer 16` and `low 8` (incl. bare) admitted, same accounting (v0255–v0256, `decomp/i960/notes/game_info_18644_positive_middle_high_bare_v0255.md`);
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