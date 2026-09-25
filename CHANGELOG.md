# Changelog

## Unreleased

- Extend the measured `fa_player` `0x19ef8` corridor to non-branch mask
  `0x0000000f` (v0566). All 16 compositions with branch bits 5/6/21/23
  match full live state and exact 4-call/4-return accounting; other
  quadruple masks and five-or-more non-branch bits remain fail-closed. See
  `decomp/i960/notes/fa_player_19ef8_quadruple_0f_v0566.md`.

- Recover the measured `fa_coli` whole-task both-bit-8/scan-5 sibling
  (v0565). The new parked witness reaches `0x10dcc` at `9526/18/19` with
  complete live-state equality; the fixture now covers four whole-task
  shapes. The `0x238a4` table-bit test follows the original `bbc r7,r3`
  operand order, while unmeasured collision siblings remain fail-closed.
  See `decomp/i960/notes/fa_coli_whole_task_both_high_v0565.md`.

- Prove the complete bounded `fa_rob` `0x1442c` state-25 successor-byte sweep
  (v0564). For fighter1 `+0x197 = 1..31`, all integrated cases match full
  live state; counts are 144 (16), 105 (24), 126 (27), 92 (28), and 98 for
  the remaining values.

- Extend the measured `fa_rob` `0x1442c` state-25 successor family (v0563).
  Oracle sweeps prove successors `17..23,25..26,28..31` with exact live state;
  the neutral tail is 98 instructions except the 92-instruction successor 28
  arm. Other state values remain fail-closed.

- Recover the measured `fa_rob` `0x1442c` state-25/state-22 successor (v0562).
  The integrated witness matches the existing neutral tail in 98 instructions
  with 3 calls/returns and exact live-state equality; the final compare is
  LESS. No unmeasured state family is admitted. See
  `decomp/i960/notes/fa_player_1442c_state25_state22_v0562.md`.

- Recover the measured `fa_rob` `0x1442c` state-25/state-25 successor (v0561).
  The integrated witness now reaches the existing `0x144b0` neutral tail in
  98 instructions with 3 calls/returns and exact live-state equality. The
  final compare is LESS for state 25; equal/greater `r13` shapes remain
  fail-closed. See
  `decomp/i960/notes/fa_player_1442c_state25_state25_v0561.md`.

- Recover the measured `fa_player` `0x19ef8` selector-`0x284` sibling (v0560).
  The depth-zero `0x00510980` park now matches the selector's opcode-1 plus
  terminator-0 setup stream, 33/24/3 expansion census, exact long-arm byte
  stream, `table+0xbcc` float window and 1804-instruction corridor. Other
  selector and stream shapes remain fail-closed. See
  `decomp/i960/notes/fa_player_19ef8_selector284_v0560.md`.

- Extend the measured `fa_rob` `0x14640` type-15 miss interval to
  `+0x194=1..1359` (v0554). The focused fixture compares the added
  `1025..1359` cases against the reference; selector `1360` remains a
  fail-closed negative control.

- Extend the measured `fa_rob` `0x1453c/0x14570` type-5 miss interval to
  `+0x194=1..1359` (v0553). The focused fixture compares the added
  `1025..1359` cases against the reference; selector `1360` remains a
  fail-closed invalid-table control.

- Extend the measured `fa_player` `0x19ef8` entry-state family to two
  non-branch `fighter+0x1a4` bits (v0555). The complete 378-pair × 16-branch
  matrix adds 6048 full live-state comparisons; three or more non-branch bits
  remain `VF2_ERROR_UNSUPPORTED`.

- Extend the measured `fa_player` `0x19ef8` entry-state family with all
  3276 branch-free triples of non-branch `fighter+0x1a4` bits (v0556). Triple
  combinations with branch bits and four or more non-branch bits remain
  fail-closed.

- Extend the measured `fa_player` `0x19ef8` triple family through branch bit
  5 (v0557). All 3276 branch-free triples and their 3276 bit-5 compositions
  match the reference; other branch-bit triple compositions remain closed.

- Extend the measured `fa_player` `0x19ef8` triple family through all four
  branch bits (v0559). All 3276 non-branch triples across all 16 branch
  subsets match the reference; four or more non-branch bits remain closed.

- Extend the measured `fa_player` `0x19ef8` entry-state corridor (v0552).
  Every one of the 28 non-branch `fighter+0x1a4` singleton bits now matches
  the reference when composed with all 16 subsets of branch bits 5/6/21/23:
  448 additional full live-state comparisons pass. Words with two or more
  non-branch bits remain `VF2_ERROR_UNSUPPORTED`. See
  `decomp/i960/notes/fa_player_19ef8_state_flags_v0550.md`.

- Recover the measured `fa_game_info` positive state-8 composition `0x0e`
  (v0551). The three-distribution matrix through threshold `8` is now exact
  for both countdown values and both mode-bit-6 settings: `108/108` full
  dispatcher cases match CPU state, condition state, frames, procedure state,
  counters and mutable Model 2A memory. The zero-countdown dispatcher uses
  the measured two-instruction positive correction; threshold `9+`, unmeasured
  distributions and neighboring compositions remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_mask0e_v0551.md`.

- Recover the measured `fa_player` `0x19ef8` entry-state siblings (v0550).
  The live `0x505` corridor now accepts the bounded `fighter+0x1a4` bits
  5/6/21/23, including all 15 nonzero combinations, with exact instruction
  deltas and full CPU/condition/frame/procedure/Model 2A equality. All 32
  state-bit singletons are covered; mixed words combining an unmeasured bit
  with a branch-sensitive bit remain closed. Bit 5
  produces the ROM's post-selector `+0x1a4` bit-7 update, bit 21 toggles the
  player-word bit 6, and bit 23 copies `0x0050a010` to `+0x1c`. Other entry
  state bits remain fail-closed. See
  `decomp/i960/notes/fa_player_19ef8_state_flags_v0550.md`.

- Recover the measured `fa_rob` `0x14640` bit-4-clear greater sibling
  (v0539). With `+0x654 != 0`, neutral state byte, `+0x1aa=2` and
  `+0x62a=1`, the generic dispatcher now reaches the `0x146c8` tail and
  returns through `0x146d8` with exact reference/native state equality;
  the direct tail is 14 instructions and the consumed caller return is 15.
  Other greater relations and unmeasured flag/state compositions remain
  fail-closed. See `decomp/i960/notes/fa_player_14640_bit4clear_greater_v0539.md`.

- Extend the measured `fa_rob` `0x14640` greater witness (v0540) to the
  state-13 tail: bit-4-clear `(s16(+0x1aa),s16(+0x62a))=(2,1)` clears
  `+0x654` and reaches `0x146d8` in 16 instructions (17 with the consumed
  return). Other greater state/flag compositions remain fail-closed.

- Recover the measured `pre14288-boot` player shape through the `0x14288`
  corridor and `0x1428c` geometry head (v0538). The boot park now matches the
  reference at `1622/+4/+4` and `10869/+10/+10` with full CPU, condition,
  frame, procedure-counter and Model 2A state equality. Its distinct
  depth-zero entry, null persistent scratch pointer and transient `0x520000`
  scratch base are admitted only for the measured `0x505` / `0x510980` shape;
  other zero-frame and zero-scratch shapes remain fail-closed. See
  `decomp/i960/notes/player_boot_142c0_v0538.md`.

- Recover the measured `pre14288-natres` player shape through the `0x14288`
  corridor and `0x1428c` geometry head (v0537). The corridor now preserves
  the late `player+0xbdd` clear, and the head admits only the measured natres
  F0 word `0x80000882` in addition to the existing `0x00000800` shape. Both
  base and natres reach `0x142c0` at `10869` instructions with `+10/+10` and
  full live-state equality. See
  `decomp/i960/notes/player_natres_142c0_v0537.md`.

- Extend the `fa_coli` type-22 live selector evidence with 35 individually
  returned cases in the mixed high interval `g8+0x19c == 1345..1408`
  (v0536). Each case reaches `0x10dcc` and matches the existing C recovery
  with exact CPU/condition/procedure/Model 2A state; interleaved walker
  faults/loops remain outside the admitted set. See
  `decomp/i960/notes/fa_coli_225cc_type22_high_returns_v0536.md`.

- Extend the measured `fa_coli` type-22 selector sweep through
  `g8+0x19c == 1..1024` (v0535). All 1,024 live selectors reach `0x10dcc` and
  match the reference with exact CPU/condition/procedure/Model 2A state in
  normal and sanitizer builds; selector 0 and values above 1024 remain outside
  the admitted evidence. See
  `decomp/i960/notes/fa_coli_225cc_type22_sweep_v0535.md`.

- Extend the measured `fa_coli` type-22 selector sweep through
  `g8+0x19c == 1..512` (v0534). All 512 live selectors reach `0x10dcc` and
  match the reference with exact CPU/condition/procedure/Model 2A state in
  normal and sanitizer builds; selector 0 and values above 512 remain outside
  the admitted evidence. See
  `decomp/i960/notes/fa_coli_225cc_type22_sweep_v0534.md`.

- Extend the measured `fa_coli` type-22 selector sweep through
  `g8+0x19c == 1..256` (v0533). All 256 live selectors reach `0x10dcc` and
  match the reference with exact CPU/condition/procedure/Model 2A state;
  selector 0 and values above 256 remain outside the admitted evidence. See
  `decomp/i960/notes/fa_coli_225cc_type22_sweep_v0533.md`.

- Extend the measured `fa_coli` `0x227dc` type-5 match tail through the
  controlled selector interval `g7+0x848 == 1..64` (v0532). All selectors
  resolve to the measured `0x02014d75` record and match the reference in 61
  instructions with complete live-state equality; selector 0, other records
  and unmeasured compositions remain fail-closed. See
  `decomp/i960/notes/fa_coli_227dc_match_v0532.md`.

- Preserve the measured resolver `g0` on the `fa_coli` `0x225cc` type-22
  shortcut (v0531). The nine type-5 hits in selector range `1..64` now carry
  their `0x1ab34` record pointers through `0x18bd4`; the complete 64-selector
  matrix matches the reference with exact live-state equality. Selector 0
  remains fail-closed. See
  `decomp/i960/notes/fa_coli_225cc_type22_g0_v0531.md`.

- Extend the measured `fa_coli` `0x225cc` type-22 shortcut through the 55
  type-5 walker misses in selector range `1..64` (v0530). Each miss reaches
  `0x10dcc` with exact CPU/condition/procedure/Model 2A state equality; the
  nine hit selectors in the same bounded probe remain a separate `g0` return
  frontier, and the index-0 control remains fail-closed. See
  `decomp/i960/notes/fa_coli_225cc_type22_miss_v0530.md`.

- Extend the measured `fa_rob` `0x14640` state-27 type-15 miss recovery
  through selector `+0x194 == 1..1024` (v0529). The additional `513..1024`
  sweep reaches `0x146c4` through the existing zero-record tail and matches
  the reference with exact instruction counts and full live-state equality;
  larger selectors and unmeasured state/flag compositions remain fail-closed.
  See `decomp/i960/notes/fa_player_14640_type15_miss_v0529.md`.

- Extend the measured `fa_rob` `0x1453c/0x14570` type-5 miss recovery through
  selector `+0x194 == 1..1024` (v0528). The additional `513..1024` sweep
  reaches `0x1463c` through the existing zero-record tail and matches the
  reference with exact instruction counts and full live-state equality; larger
  selectors and unmeasured state/scaling compositions remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_type5_miss_v0528.md`.

- Recover the measured positive state-8 mask `0x0c` through threshold `8`
  (v0527). Its complete 108-case dispatcher matrix now matches the reference
  exactly with the measured `+2` zero-countdown correction; threshold `9+`,
  other distributions and unmeasured compositions remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_mask0c_v0527.md`.

- Recover the measured positive state-8 mask `0x0a` through threshold `8`
  (v0526). Its complete 108-case dispatcher matrix now matches the reference
  exactly with the measured `+2` instruction correction; threshold `9+`,
  other distributions and unmeasured compositions remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_mask0a_v0526.md`.

- Recover the measured positive state-8 mask `0x16` through threshold `8`
  (v0525). Its complete 108-case dispatcher matrix now matches the reference
  exactly; threshold `9+`, other distributions and unmeasured compositions
  remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_mask16_v0525.md`.

- Recover the measured positive state-8 mask `0x1e` through threshold `8`
  (v0524). Its complete 108-case dispatcher matrix now matches the reference
  exactly; threshold `9+`, other distributions and unmeasured compositions
  remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_mask1e_v0524.md`.

- Recover the measured positive state-8 mask `0x18` through threshold `8`
  (v0523). Its complete 108-case dispatcher matrix now matches the reference
  exactly; threshold `9+`, other distributions and unmeasured compositions
  remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_mask18_v0523.md`.

- Extend all five measured positive state-8 `fa_game_info` mixed masks through
  threshold `8` (v0522). Their combined complete 240-case dispatcher matrix
  now matches the reference exactly; threshold `9+`, adjacent `0x18`, other
  distributions and unmeasured compositions remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_bit2_bit4_threshold8_v0522.md`.

- Extend all five measured positive state-8 `fa_game_info` mixed masks through
  threshold `4` (v0521). Their combined complete 60-case dispatcher matrix
  now matches the reference exactly; threshold `5+`, adjacent `0x18`, other
  distributions and unmeasured compositions remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_bit2_bit4_threshold4_v0521.md`.

- Extend the remaining measured positive state-8 `fa_game_info` masks `0x1c`,
  `0x34` and `0x94` through threshold `3` (v0520). Their combined complete
  36-case dispatcher matrix now matches the reference exactly; thresholds
  `4+`, adjacent `0x18`, other distributions and unmeasured compositions
  remain fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_bit2_bit4_threshold3_v0520.md`.

- Extend the measured positive state-8 `fa_game_info` mask `0x1a` through
  threshold `3` (v0519). Its complete 12-case dispatcher matrix now matches
  the reference exactly; adjacent and unmeasured compositions remain
  fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_bit1_bit3_bit4_v0519.md`.

- Close the measured positive state-8 `fa_game_info` mask `0x14` corridor
  through threshold `3` (v0518). The complete 48-case dispatcher matrix now
  matches the reference exactly; threshold `4+`, adjacent mask `0x18`, other
  v0517 masks at threshold `3` and unmeasured distributions remain fail-closed.
  See `decomp/i960/notes/game_info_18644_positive_bit2_bit4_v0518.md`.

- Recover the measured `fa_coli` bit-16/bit-22-set continuation at `0x225cc`
  (v0452). The exact scan-4 witness now reaches the g0=5 `0x22d8c` tail,
  matching 108 instructions and exact live state, including `g0=0xeb` and
  the `0x0c0100eb` result. Other bit-16 compositions remain fail-closed. See
  `decomp/i960/notes/fa_coli_225cc_bit16_bit22_v0452.md`.

- Recover the measured `fa_coli` bit-16 scan-4 continuation at `0x225cc`
  (v0451). The `0x22c88` bbs-16 edge now reaches the shared `0x22e24` join
  for the exact `g7 + 0x821 == 4`, `g8 + 0x1a4 == 0x00010000` witness,
  matching 217 instructions and exact live state. Other bit-16 compositions
  remain fail-closed. See
  `decomp/i960/notes/fa_coli_225cc_bit16_scan4_v0451.md`.

- Recover the measured state-16/state-25 type-walker misses (v0450). Indices
  `0x110` and `0x2cf` now preserve the `g0 == 0` type-8 terminator and match
  the ROM in 161/147 instructions with four calls/returns and exact live-state
  equality. Other walker misses remain fail-closed. See
  `decomp/i960/notes/fa_player_1442c_state16_state25_walker_miss_v0450.md`.

- Recover the integrated state-16/state-25 zero-selector join (v0449). The
  measured row now follows the swapped zero `0x19ef8` path into the existing
  direct state-16 type-5 tail for indices `0x6f`, `0x73` and `0x74`, matching
  144 instructions with four calls/returns and exact live-state equality.
  Nonzero selectors and other index compositions remain fail-closed. See
  `decomp/i960/notes/fa_player_1442c_state16_state25_v0449.md`.

- Recover integrated state-16/state-24 write arms (v0448). The measured
  `+0x19f == 25` and `22` cases now match in 59/60 instructions with two
  calls/returns and exact live-state equality. Other state-24 compositions
  remain fail-closed. See
  `decomp/i960/notes/fa_player_1442c_state16_state24_writes_v0448.md`.

- Recover the integrated state-16/state-24 neutral continuation (v0447).
  The measured `+0x19f` miss now reaches the direct state-16 type-5 body in
  105 instructions with three calls/returns and exact live-state equality;
  state-24 `+0x19f` values 25/22 remain separate boundaries. See
  `decomp/i960/notes/fa_player_1442c_state16_state24_v0447.md`.

- Prove the integrated state-16/state-27 join (v0446). The state-27 helper
  mutates the post-helper row into the existing both-state-16 join; the live
  fixture now proves the 126-instruction path with four calls/returns and
  exact live-state equality. See
  `decomp/i960/notes/fa_player_1442c_state16_state27_v0446.md`.

- Recover integrated state-16 ordinary joins (v0445). The live `0x1442c`
  row with fighter 0 state 16 and fighter 1 state 26 now reaches the native
  direct `0x14570` body in 98 instructions with three calls/returns and exact
  live-state equality. Measured special state-24/25/27 successors remain
  fail-closed. See
  `decomp/i960/notes/fa_player_1442c_state16_ordinary_v0445.md`.

- Generalize the bounded neutral `0x14528` family (v0444). A second full
  `r7=0..31` reference row at `r8 == 1` matches the same 9-instruction exit,
  with only the dedicated state-16/state-27 rows taking the type-5 paths.
  Native C now admits the measured bounded cross-product and the live fixture
  proves the state-26/state-1 neutral witness; values above 31 remain
  fail-closed. See
  `decomp/i960/notes/fa_player_1453c_neutral_cross_product_v0444.md`.

- Consolidate the bounded direct state-16 `0x14528` family (v0443). A
  reference sweep of `r7 == 16` and `r8=0..31` uses the same 52-instruction
  type-5 body for every value except the dedicated state-16/state-27 joins.
  Native C and the live fixture now cover `r8 == 26`, including its text-tail
  variant; values above 31 remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_state16_direct_family_v0443.md`.

- Recover the bounded neutral `0x14528` state family (v0442). A reference
  sweep of `r8 == 0` and `r7=0..31` takes the same 9-instruction
  `0x14560 -> 0x14628` exit for every value except the dedicated state-16
  and state-27 arms. Native C admits that measured family and the live
  fixture proves state 26 with zero calls/returns; values above 31 and other
  state-byte combinations remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_neutral_family_v0442.md`.

- Consolidate the measured `0x14528` state-16 swapped joins (v0441). A
  bounded `r7=0..31` reference sweep shows one 55-instruction path for every
  value except the dedicated state-16 and state-27 arms; native C now admits
  that measured family and the focused fixture proves state 26 plus text.
  Values outside the measured state-byte domain remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_state16_family_v0441.md`.

- Recover the measured direct `0x144b0` state-25/state-27 successor (v0440).
  The state-25 arm now reaches the recovered swapped state-27 type-5 body and
  matches the ROM in 100 instructions with two calls/returns and exact
  live-state equality. See
  `decomp/i960/notes/fa_player_144b0_state27_v0440.md`.

- Recover the measured `0x1453c` state-28/state-16 swapped join (v0439).
  The shared type-5 body now admits the `r7 == 28`, `r8 == 16` shape and
  matches the ROM in 55 instructions with one call/return; its board/text
  witness matches at 129 instructions with two calls/returns. See
  `decomp/i960/notes/fa_player_1453c_state28_v0439.md`.

- Recover the measured `0x1453c` state-24/state-16 swapped join (v0438).
  The shared type-5 body now admits the `r7 == 24`, `r8 == 16` shape and
  matches the ROM in 55 instructions with one call/return and exact
  live-state equality. The board/text siblings remain bounded by measured
  shapes. See
  `decomp/i960/notes/fa_player_1453c_state24_v0438.md`.

- Recover the measured state-13 bit-4-clear `0x14640` tail (v0437). The
  state-13 helper now admits both flag-bit shapes through `0x146c8` and
  `0x146d8`; the bit-4-clear witness matches 13 instructions with no calls or
  returns and exact live-state equality, while the existing bit-4-set witness
  remains 14 instructions. See
  `decomp/i960/notes/fa_player_14640_state13_bit4_clear_v0437.md`.

- Recover the measured integrated state-25/state-27 neutral continuation
  (v0436). The live state-27 helper clears fighter1 `+0x194`, after which the
  state-25 arm reaches `0x144b0` and the common exit in 126 instructions with
  four calls/returns and exact live-state equality. Direct state-27 entry at
  `0x144b0` remains unsupported. See
  `decomp/i960/notes/fa_player_1442c_state25_state27_v0436.md`.

- Recover the measured integrated state-25/state-24 `0x14474` sibling
  (v0435). When fighter0 `+0x19f` is 25 or 22, the swapped arm now writes
  the original fighter0 `+0x194`/`+0x1a4` state and rejoins the common exit;
  the ROM-backed witnesses match 59/60 instructions with two calls/returns
  and exact live-state equality. See
  `decomp/i960/notes/fa_player_1442c_state25_state24_1474_v0435.md`.

- Recover the measured `0x144b0` state-25 to state-24 successor (v0434).
  The neutral `0x14474`/`0x14498` prefix now reaches the existing common
  `0x144b0` exit: the direct witness matches 53 instructions and the
  integrated `0x1442c` witness matches 105 instructions, with exact live-state
  equality in both forms. See
  `decomp/i960/notes/fa_player_144b0_state24_v0434.md`.

- Recover the measured `0x144b0` state-25 to state-16 successor (v0433).
  The `0x14560` fall-through now reaches the recovered swapped `0x14570`
  type-5 body: the direct arm matches 99 instructions with two calls/returns,
  and the integrated `0x1442c` path matches 144 instructions with four
  calls/returns and exact live-state equality. See
  `decomp/i960/notes/fa_player_144b0_state16_v0433.md`.

- Recover the measured `0x14640` state-27 board-bit-20 shift sibling
  (v0432). The type-15 tail now admits the `shli` arm at `0x14684`, adding one
  instruction to the compare-prefix witness; the expanded fixture proves the
  45-step path with exact live-state equality.

- Recover the measured `0x14640` compare-prefix equality tails for state 27
  and state 28 (v0431). The generic dispatcher now routes both states through
  the shared 10-instruction `0x146dc` tail; the ROM-backed fixture proves the
  resulting 11-step caller return and exact live-state equality.

- Complete the measured 0x1453c/0x14570 board-bit-9-clear text matrix
  (v0430). The recovered 0x7fc0 expander now covers all 35 accepted
  state/scaling shapes, including swapped, both-state-16 and mixed
  first/later-scaling paths. Each text variant matches the ROM with the
  expected +74 instructions and one additional call/return; the expanded
  fixture proves full live-state equality.

- Extend the measured 0x1453c/0x14570 text tail (v0429). The board-bit-9-clear
  0x7fc0 path now covers unscaled state-27/state-16 direct, swapped and
  both-state-16 joins, plus direct state-16 first/second/later single-scaling
  variants. The fixture proves 126/130/129/128/132/129/128/131-step shapes
  with exact live-state equality; mixed text compositions remain fail-closed.

- Extend the measured `0x14640` compare-prefix tails to signed-greater
  variants (v0428). State 27, state 28 and the neutral/state-13 tails now
  accept unequal `+0x1aa`/`+0x62a` values in either signed order; equal values
  remain fail-closed. The live fixtures prove exact state equality for both
  compare directions.

- Recover the measured neutral nonzero `0x14640` less-than tail (v0427).
  With bit 4 clear, `+0x194 != 0`, `+0x654 != 0` and signed
  `+0x1aa < +0x62a`, the native path now matches the ROM through `0x146d8`
  in 16 instructions, clearing `+0x654`; the existing zero tail remains
  14 instructions. The compare-less fixture proves neutral zero, state 13,
  neutral nonzero and neutral bit-4-set shapes with full live-state equality.
  See
  `decomp/i960/notes/fa_player_14640_compare_less_nonzero_v0427.md`.

- Recover the measured `0x14640` state-28 compare-prefix sibling (v0426).
  With `+0x654 != 0` and signed `+0x1aa < +0x62a`, the native arithmetic tail
  now matches the ROM through `0x146c4` in 16 instructions, alongside the
  original 13-instruction path. The state-28 fixture proves both with full
  live-state equality and generic dispatch reaches the recovered arm. See
  `decomp/i960/notes/fa_player_14640_state28_compare_less_v0426.md`.

- Recover the measured `0x14640` state-27 compare-prefix sibling (v0425).
  With `+0x654 != 0` and signed `+0x1aa < +0x62a`, the existing type-15
  state-27 walk now matches the ROM through `0x146c4` in 44 instructions
  (+1 call/return), alongside the original 41-instruction path. The
  standalone fixture proves both with full live-state equality and the
  dispatcher now reaches the recovered arm. See
  `decomp/i960/notes/fa_player_14640_state27_compare_less_v0425.md`.

- Recover the measured mixed first/later scaling matrix (v0424). With
  `+0x1a4(g8)` bit 0, `+0x3351` bit 6 and optionally target g8 bit 29 set,
  direct/swapped state 27 reaches 57/60 and 61/64, swapped state 16 reaches
  60/63, and both-state-16 direct/swapped reaches 59/62 and 63/66. The
  expanded `vf2_player_1453c_live` fixture now proves 35 shapes with exact
  live-state equality; mixed scaled text remains fail-closed. See
  `decomp/i960/notes/fa_player_1453c_mixed_scaling_v0424.md`.

- Extend the measured bit-29 later-scaling arm across the state-27/state-16
  matrix (v0423). With `+0x3351` bit 6 and the target g8 word bit 29 set,
  direct/swapped state 27 reaches 57/61 instructions, swapped state 16
  reaches 60, and both-state-16 direct/swapped reaches 59/63. The expanded
  `vf2_player_1453c_live` fixture now proves 25 shapes with exact live-state
  equality; mixed first/later scaling and scaled text remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_later_scaling_matrix_v0423.md`.

- Extend the measured `0x145c0` second-gate arm across the state-27/state-16
  matrix (v0422). With `+0x3351` bit 6 set and g8 bit 29 clear, direct/
  swapped state 27 reaches 54/58 instructions, swapped state 16 reaches 57,
  and both-state-16 direct/swapped reaches 56/60. The expanded
  `vf2_player_1453c_live` fixture now proves 20 shapes with exact live-state
  equality; bit-29-set and mixed scaling combinations remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_second_gate_matrix_v0422.md`.

- Extend the measured first-scaling arm across the state-27/state-16 swap
  matrix (v0421). The native helper now proves direct/swapped state 27 at
  55/59 instructions, swapped state 16 at 58, and both-state-16 direct/
  swapped at 57/61, with exact live-state equality and one call/return in
  each case. Other scaling compositions remain fail-closed. See the expanded
  `vf2_player_1453c_live` fixture and
  `decomp/i960/notes/fa_player_1453c_scaling_matrix_v0421.md`.

- Recover the measured direct state-16 text tail of `0x14570` (v0420). With
  board `0x508000` bit 9 clear, the native path calls the existing recovered
  `0x7fc0` byte expander from source `0x1b970` to `0x010006e8`, matching the
  ROM through `0x1463c` in 126 instructions with two calls/returns. The
  `vf2_player_1453c_live` fixture now proves ten measured shapes with full
  live-state equality; scaled/swapped text variants remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_text_v0420.md`.

- Recover the measured direct state-16 later scaling arm of `0x14570` (v0419).
  With `+0x3351` bit 6 and the g8 word bit 29 set, the native path performs
  the measured second `shro`/`addo`, selects `0x1b982` and matches the ROM
  through `0x1463c` in 57 instructions with one call/return. The
  `vf2_player_1453c_live` fixture now proves nine measured shapes with full
  live-state equality; other later scaling/text compositions remain
  fail-closed. See `decomp/i960/notes/fa_player_1453c_later_scaling_v0419.md`.

- Recover the measured direct state-16 `0x145c0` second gate (v0418). With
  `+0x3351` bit 6 set and `g8` bit 29 clear, the native path matches the ROM
  through `0x1463c` in 54 instructions with one type-5 call/return. The
  expanded `vf2_player_1453c_live` fixture now proves eight measured
  state-27/state-16 shapes with full live-state equality; other second-gate
  and later scaling/text variants remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_second_gate_v0418.md`.

- Recover the measured direct state-16 first-scaling arm of `0x14570` (v0417).
  With `+0x1a4(g8)` bit 0 set, the native path scales the type-5 record byte,
  selects `0x1b979` and matches the ROM through `0x1463c` in 55 instructions
  with one call/return. The expanded `vf2_player_1453c_live` fixture proves
  the shape with full live-state equality; unmeasured scaled swaps and later
  scaling/text arms remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_scaling_v0417.md`.

- Extend the `0x1453c`/`0x14570` fighter-exchange recovery for the measured
  state-16 joins (v0416). The native helper now covers `(r7,r8)=(16,0)` and
  `(0,16)`, plus both 16 values with the observed `0x500028` bit-0 swap gate,
  matching 52/55/54/58 instruction paths and one type-5 call/return. The
  ROM-backed fixture now proves six direct/swapped state-27/state-16 shapes
  with full live-state equality; other compositions remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_state16_v0416.md`.

- Extend the measured `fa_rob` `0x1453c` state-27 recovery for the f1 == 27
  swap path (v0415). When `r7 != 27` and `r8 == 27`, the native helper now
  models the `0x14530..0x14538` g7/g8 swap and matches the ROM through
  `0x1463c` in 56 instructions with one type-5 walker call/return. The
  existing ROM-backed fixture now proves both direct and swapped shapes with
  full live-state equality; unmeasured scaling, text and broader dispatch
  variants remain fail-closed. See
  `decomp/i960/notes/fa_player_1453c_swap_v0415.md`.

- Recover the measured `fa_rob` `0x14640` signed-greater compare-prefix tails
  (v0414).  The native path now covers neutral bit-4-clear cases with zero or
  nonzero `+0x194` (14/16 instructions to `0x146d8`) and state 13 with bit 4
  clear or set (16/17 instructions), preserving the observed EQUAL/LESS
  condition state and `+0x654` effects.  The new
  `vf2_player_14640_compare_tail_live` fixture proves all four shapes with
  full live-state equality; unmeasured compositions remain fail-closed.  See
  `decomp/i960/notes/fa_player_14640_compare_tails_v0414.md`.

- Extend the `0x14640` compare-prefix less-than recovery for the measured
  state-13 sibling (v0413): bit 4 set, `+0x197 == 13` and nonzero `+0x194`
  reach `0x146d8` in 17 instructions, clear `+0x654` and finish LESS.  The
  existing ROM-backed compare-less fixture now proves both this shape and the
  v0412 neutral shape with full live-state equality.  See
  `decomp/i960/notes/fa_player_14640_compare_less_state13_v0413.md`.

- Recover the measured `fa_rob` `0x14640` signed-less compare-prefix sibling
  (v0412).  For `+0x198 == 0`, `+0x654 != 0`, signed
  `+0x1aa < +0x62a`, neutral `+0x197`, bit 4 clear and `+0x194 == 0`, the
  native path reaches `0x146d8` in 14 instructions, leaves `+0x654` intact
  and matches the final EQUAL condition state.  The new
  `vf2_player_14640_compare_less_live` fixture proves full live-state
  equality, and dispatch now selects this sibling for the measured less-than
  case.  Other less-than compositions remain fail-closed.  See
  `decomp/i960/notes/fa_player_14640_compare_less_v0412.md`.

- Recover the measured `fa_rob` state-13 neutral tail at `0x14640` (v0411),
  including its 14-instruction path to `0x146d8`, `+0x654` clear and final
  LESS condition state.  The ROM-backed
  `vf2_player_14640_state13_live` fixture proves full live-state equality.
  See `decomp/i960/notes/fa_player_14640_state13_v0411.md`.

- Recover the fa_rob compare-prefix escape arm `0x14640` (v0410), reached
  when fighter g7 has `+0x198 == 0` and `+0x654 != 0` (the `0x1464c
  cmpobe 0, r3` not taken, running the `+0x1aa`/`+0x62a` compare prefix
  `0x14650`..`0x14658`) with `s16(+0x1aa) == s16(+0x62a)` (the `0x14658
  cmpobe r13, r14` IS taken to `0x146dc`).  It stores r3 (= the `+0x654`
  value, distinct from the v0408 `+0x198` escape) to `+0x194(g7)`, clears
  `+0x654(g7)` and leaves `r15 = 0`, `r3 = +0x654`,
  `r13 = s16(+0x1aa)`, `r14 = s16(+0x62a)`.  No walker.  Measured path:
  10 steps / +0 call / +0 return, leaving the `0x146e8` ret unconsumed.
  Recovered as standalone `hybrid_execute_player_14640_compare_escape` +
  `vf2_hybrid_player_14640_compare_escape_execute_for_test`, dispatched
  from `hybrid_execute_player_14640` when `+0x654 != 0` and
  `s16(+0x1aa) == s16(+0x62a)` (which then consumes the `0x146e8` ret to
  return through the `0x14640` frame).  Final reference `compare_result`
  is EQUAL.  ROM-backed
  `vf2_player_14640_compare_escape_live_differential` proves the arm
  byte-exact (10/+0/+0, full live state).  `ctest` 92/92. See
  `decomp/i960/notes/fa_player_14640_compare_escape_v0410.md`.

- Recover the fa_rob compare-prefix arm `0x14640` (v0409), reached when
  fighter g7 has `+0x198 == 0` and `+0x654 != 0` (the `0x1464c cmpobe 0,
  r3` not taken, running the `+0x1aa`/`+0x62a` compare prefix
  `0x14650`..`0x14658`) with `s16(+0x1aa) > s16(+0x62a)` (the `0x14658
  cmpobe r13, r14` not taken to `0x146dc`).  On the measured shape
  (`+0x197` not 27/28/13, bit 4 of `(g7)` SET) it clears `+0x194(g7)` and
  leaves `r15 = 0`, `r3 = +0x197`, `r13 = s16(+0x1aa)`,
  `r14 = s16(+0x62a)`.  No walker.  Measured path: 15 steps / +0 call /
  +0 return, leaving the `0x146c4` ret unconsumed.  Recovered as standalone
  `hybrid_execute_player_14640_compare` +
  `vf2_hybrid_player_14640_compare_execute_for_test`, dispatched from
  `hybrid_execute_player_14640` when `+0x654 != 0` (which then consumes
  the `0x146c4` ret to return through the `0x14640` frame).  The
  `s16(+0x1aa) <= s16(+0x62a)` escape jump stays fail-closed.  Final
  reference `compare_result` is GREATER.  ROM-backed
  `vf2_player_14640_compare_live_differential` proves the arm byte-exact
  (15/+0/+0, full live state).  `ctest` 90/90. See
  `decomp/i960/notes/fa_player_14640_compare_v0409.md`.

- Recover the fa_rob escape arm `0x14640` (v0408), reached when fighter
  g7 has `+0x198 != 0` (the `0x14644 cmpobne 0, r3` jumps directly to
  `0x146dc`).  It stores r3 (= the `+0x198` value) to `+0x194(g7)`,
  clears `+0x654(g7)` and leaves `r15 = 0` and `r3 = +0x198`.  No walker.
  Measured path: 5 steps / +0 call / +0 return, leaving the `0x146e8` ret
  unconsumed.  Recovered as standalone
  `hybrid_execute_player_14640_escape` +
  `vf2_hybrid_player_14640_escape_execute_for_test`, dispatched from
  `hybrid_execute_player_14640` when `+0x198 != 0` (which then consumes
  the `0x146e8` ret to return through the `0x14640` frame).  The
  `+0x654 != 0` `+0x1aa`/`+0x62a` compare arm stays fail-closed.  Final
  reference `compare_result` is LESS.  ROM-backed
  `vf2_player_14640_escape_live_differential` proves the arm byte-exact
  (5/+0/+0, full live state).  `ctest` 88/88. See
  `decomp/i960/notes/fa_player_14640_escape_v0408.md`.

- Recover the fa_rob bit-4-set neutral arm `0x14640` (v0407), the
  `+0x197` not 27/28 neutral tail when bit 4 of `(g7)` is SET (the
  `0x146b0 bbc 4, r15` not taken) and r3 (`+0x197`) != 13 (the `0x146b8
  cmpobe 13, r3` not taken).  It clears `+0x194(g7)` and leaves `r15 = 0`
  and `r3 = +0x197`.  No walker.  Measured path: 12 steps / +0 call / +0
  return, leaving the `0x146c4` ret unconsumed.  Recovered as standalone
  `hybrid_execute_player_14640_bit4set` +
  `vf2_hybrid_player_14640_bit4set_execute_for_test`, dispatched from
  `hybrid_execute_player_14640` when bit 4 of `(g7)` is set and `+0x197`
  not 27/28/13 (which then consumes the `0x146c4` ret to return through
  the `0x14640` frame).  The `r3 == 13` sibling (which takes the 0x146c8
  tail), the `+0x654 != 0` `+0x1aa`/`+0x62a` compare arm, and the
  `r198 != 0` escape stay fail-closed.  Final reference `compare_result`
  is GREATER.  ROM-backed
  `vf2_player_14640_bit4set_live_differential` proves the arm byte-exact
  (12/+0/+0, full live state).  `ctest` 86/86. See
  `decomp/i960/notes/fa_player_14640_bit4set_v0407.md`.

- Recover the fa_rob state-28 arm `0x14640` (v0406), the state-28 sibling
  of the v0405 state-27 walk: when fighter g7 has `+0x198 == 0`,
  `+0x654 == 0` and `+0x197 == 28`, the `0x1469c cmpobne 28, r3` falls
  through, adds 3 to `s16(+0x1aa(g7))` and stores the u16 back to
  `+0x1aa(g7)`, clears `+0x194(g7)`, leaves `r15 = 0` and `r3` the new
  `+0x1aa` value.  No walker.  Measured path: 13 steps / +0 call / +0
  return, leaving the `0x146c4` ret unconsumed.  Recovered as standalone
  `hybrid_execute_player_14640_state28` +
  `vf2_hybrid_player_14640_state28_execute_for_test`, dispatched from
  `hybrid_execute_player_14640` when `+0x197 == 28` (which then consumes
  the `0x146c4` ret to return through the `0x14640` frame).  The
  `+0x654 != 0` `+0x1aa`/`+0x62a` compare arm and the `+0x197` not 27/28
  neutral tail stay fail-closed.  Final reference `compare_result` is
  EQUAL.  ROM-backed `vf2_player_14640_state28_live_differential` proves
  the arm byte-exact (13/+0/+0, full live state).  `ctest` 84/84. See
  `decomp/i960/notes/fa_player_14640_state28_v0406.md`.

- Recover the fa_rob state-27 arm `0x14640` (v0405), the natural
  continuation of the f0==27 flow: when fighter g7 has `+0x198 == 0`,
  `+0x654 == 0` and `+0x197 == 27`, the `0x14664` walk indexes a type-15
  record chain via `+0x194(g7)` (`0x1ab34`, `g1 == 15`), then stores
  `r4 = s16(+1(record)) - 1` into `+0x62a(g7)`, moves the original full
  `+0x194(g7)` u32 into `+0x654(g7)` and clears `+0x194(g7)`.  Measured
  short path: 41 steps / +1 call / +1 return (the 0x1ab34 walker), leaving
  the `0x146c4` ret unconsumed.  Recovered as standalone
  `hybrid_execute_player_14640_state27` +
  `vf2_hybrid_player_14640_state27_execute_for_test`, dispatched from
  `hybrid_execute_player_14640` when `+0x197 == 27` (which then consumes
  the `0x146c4` ret to return through the `0x14640` frame).  The walker is
  modelled through `vf2_hybrid_coli_1ab34_execute`, so frame linkage and
  call/return counters match the reference exactly; a walker miss and the
  unmeasured `shli`/`+0x1aa`/`+0x62a` compare siblings stay fail-closed.
  Final reference `compare_result` is NONE.  ROM-backed
  `vf2_player_14640_state27_live_differential` proves the arm byte-exact
  (41/+1/+1, full live state).  `ctest` 82/82. See
  `decomp/i960/notes/fa_player_14640_state27_v0405.md`.

- Recover the fa_rob state-27 arm `0x1453c` (v0404), resolving the v0403
  blocker: mine the `0x0200d34c` type-5 record chains to find a `+0x194`
  whose low 13 bits walk to a type-5 record (index `0x73`), then re-probe
  the `0x14570` tail from the `0x14528` state-27 entry.  The arm sets
  `+0x197(g7)` to 16, walks type-5 via `+0x194(g7)`, stores
  `+0x194(g8) = 0x11000000 + s16(rec+1)` and `u8(rec+3)` to `+0x822(g8)`,
  clears bit 21 of `+0x1a4(g8)`, flips bit 6 of `(g8)` via
  `chkbit`/`alterbit`, and rejoins the `0x14628` common exit.  Measured
  short path: 52 steps / +1 call / +1 return (the 0x1ab34 walker).
  Recovered as standalone `hybrid_execute_player_1453c` +
  `vf2_hybrid_player_1453c_execute_for_test`.  The walker is modelled
  through `vf2_hybrid_coli_1ab34_execute`, so frame linkage and
  call/return counters match the reference exactly; a walker miss and the
  unmeasured scaling/text branches stay fail-closed.
  ROM-backed `vf2_player_1453c_live_differential` proves the arm byte-exact
  (52/+1/+1, full live state).  `ctest` 80/80. See
  `decomp/i960/notes/fa_player_1453c_state27_v0404.md`.

- Scope the fa_rob `0x1453c` arm without recovering it (v0403,
  measurement only): the head is 3 steps but the `0x14570` tail faults
  in `0x1ab34` at `0x1ab4c` when `+0x194(g7)` low half is 0
  (`table[0] == 0`, `g0 == 8` unmapped; a miss-zero would then fault
  at `0x1457c`). The tail needs a `+0x194` indexing a type-5 record
  chain; all such paths stay fail-closed. See
  `decomp/i960/notes/fa_player_1453c_tail_blocked_v0403.md`.

- Recover the fa_rob `0x14498` escape for other `+0x19f` values
  (v0402): on all three `0x14474` entries, any `+0x19f` outside
  {25, 22} restores `g7/g8` and runs the neutral
  `0x144a0 -> 0x14528 -> 0x14548 -> 0x14560 -> 0x14628` exit with no
  `+0x194`/`+0x1a4` stores. Spans 57 (direct) / 60 (swapped) /
  59 (both-24) instructions to `0x1463c` with +2/+2; last compare
  `cmpobne 16, r8` leaves GREATER (neutral f1) or LESS (`r8 == 24`).
  ROM-backed `vf2_player_1442c_live_differential` now runs fourteen
  cases byte-exact. `ctest` 78/78. See
  `decomp/i960/notes/fa_player_14498_escape_v0402.md`.

- Recover the fa_rob `0x144b0` cmpobl-equal point (v0401): when
  `0x1450c cmpobl r13, r3` sees `r13 == r3` (measured `r3 == 0` with
  fighter1 `+0x1aa == 0`), the body takes the same `0x14510`
  fall-through as the not-taken sibling (47 steps / +1 / +1) with
  CC = EQUAL instead of GREATER. ROM-backed
  `vf2_player_1442c_live_differential` now runs eleven cases
  byte-exact. `ctest` 78/78. See
  `decomp/i960/notes/fa_player_144b0_equal_v0401.md`.

- Recover the fa_rob `0x14474` both-24 entry (v0400): when both
  fighters' `+0x197 == 24`, the f0-priority `cmpobe` takes the direct
  path and runs the same `+0x19f(f1)` body on fighter1. Spans 56
  (`+0x19f == 25`) / 57 (`== 22`) instructions to `0x1463c` with
  +2 calls / +2 rets (prefix both siblings 15+15), CC = EQUAL.
  ROM-backed `vf2_player_1442c_live_differential` now runs ten cases
  byte-exact. Other `+0x19f` values stay fail-closed. `ctest` 78/78.
  See `decomp/i960/notes/fa_player_14474_both24_v0400.md`.

- Recover the fa_rob `0x14474` swapped entry (v0399): when fighter1
  `+0x197 == 24` (fighter0 not 24) the body swaps (`g7 = f1`, `g8 =
  f0`) and runs the same `+0x19f` body on fighter0. Spans 57
  (`+0x19f(f0) == 25`) / 58 (`== 22`) instructions to `0x1463c`
  with +2 calls / +2 rets, CC = EQUAL. ROM-backed
  `vf2_player_1442c_live_differential` now runs eight cases byte-exact.
  Other `+0x19f` values and the both-`== 24` shape stay fail-closed.
  `ctest` 78/78. See
  `decomp/i960/notes/fa_player_14474_swapped_v0399.md`.

- Recover the fa_rob `0x14474` arm (v0398): entered at `0x14474` when
  fighter0 `+0x197 == 24`. For fighter1 `+0x19f` in {25, 22} the body
  stores `0x01000000` to `+0x194(f1)`, clears bit 0 of `+0x1a4(f1)`,
  takes `b 0x14628` and clears both `+0x198`. Spans 54 (`+0x19f ==
  25`) / 55 (`== 22`) instructions to `0x1463c` with +2 calls / +2
  rets, CC = EQUAL. The f0 `0x14640` call takes the `+0x194 != 0`
  sibling (15 steps) since `+0x197` is the high byte of `+0x194`;
  the f1 call stays no-op (13). ROM-backed
  `vf2_player_1442c_live_differential` now runs six cases byte-exact.
  Other `+0x19f` values and the `f1 == 24` entry stay fail-closed.
  `ctest` 78/78. See
  `decomp/i960/notes/fa_player_14474_arm_v0398.md`.

- Recover the fa_rob `0x144b0` cmpobl-not-taken sibling (v0397): when
  `0x1450c cmpobl r13, r3` does not take (`r13 > r3` unsigned, measured
  with fighter1 `+0x808 == 1` / `+0x1aa == 100` so `r13 = 100 > r3 =
  0`), the state-25 arm stores `r5` to `+0x194(f1)` (`0x14510`), takes
  `b 0x14628` and clears both `+0x198`, skipping the `+0x654`/`+0x62a`
  stores and the fighter-state chain. Span 47 instructions to `0x1463c`
  with +1 call / +1 return, CC = GREATER. Also fixes two latent
  modeling errors that cancel on the all-zero v0395 pin: ROM `ldos`
  zero-extends (not sign) and `subi r13, r14, r4` computes
  `+0x808(f1) - +0x858(f0)`. ROM-backed `vf2_player_1442c_live` now
  runs all four cases byte-exact (fast path 51 / +2 / +2, `0x14640`
  sibling 14 / +0 / +1, state-25 53 / +1 / +1, not-taken 47 / +1 / +1).
  The cmpobl-equal point stays fail-closed. `ctest` 78/78. See
  `decomp/i960/notes/fa_player_144b0_sibling_v0397.md`.

- Recover the fa_rob `0x144b0` state-25 collision arm (v0395): entered at
  `0x144b0` when fighter0 `+0x197 == 25`. On the measured live shape
  (fighter0 `+0x197 == 25`, fighter1 `+0x197 == 0`, fighter0 `+0x194 ==
  0`) the arm runs the `0x19ef8` g0==0 zero-path (clears `+0x5cc`/`+0x60c`
  and `(g7)` bit 9, `+0x1a4 &= 0x00814068`, `+0x1a8 = 0`), then the
  collision/state-exchange body (`+0x1aa = 1`, `+0x61e = +0x1a8(f1)`,
  `+0x626 = r4`, `+0x822(f1) = +0x822(f0)`, `+0x654(f1) = r5`,
  `+0x62a(f1) = (u16)(r4-1)`) and the `0x14628` common exit clearing both
  `+0x198`. Span 53 instructions to `0x1463c` with +1 call / +1 return.
  ROM-backed `vf2_player_1442c_live_differential` now runs all three cases
  byte-exact (fast path 51 / +2 / +2, sibling 14 / +0 / +1, state-25
  53 / +1 / +1). The `0x14640` state-25 helper path and other `0x1442c`
  heavy arms remain fail-closed. `ctest` 78/78. See
  `decomp/i960/notes/fa_player_144b0_state25_v0395.md`.

- Recover the fa_rob `0x14640` collision/state helper `+0x194 != 0`
  sibling (v0394): `mov 0,r15; st r15,+0x654(g7)`, CC = LESS,
  14 steps / +1 return, alongside the v0393 no-op (CC = EQUAL, 12 steps).
  The shared gates (`+0x198 == 0`, `+0x654 == 0`, `+0x197` not 27/28,
  `(g7)` bit 4 clear) dispatch to both measured exits. ROM-backed
  `vf2_player_1442c_live_differential` now runs both cases byte-exact
  (fast path 51 / +2 / +2, sibling 14 / +0 / +1). The `0x1442c` state-25
  arm and other `0x14640` gates remain fail-closed. `ctest` 78/78. See
  `decomp/i960/notes/fa_player_14640_sibling_v0394.md`.

- Recover the fa_rob fighter-exchange body `0x1442c` live fast path plus
  its two `0x14640` no-op helper calls (v0393): first native block of the
  collision/state-exchange function that follows the recovered player
  corridor. Called at `0x14388` when `+0x04(g7)==0`; on the accepted live
  (neutral) shape both fighters' `+0x197` are not in {16,24,25,27}, the
  body runs the two swapped-g7/g8 helper calls, escapes to the `0x14628`
  common exit and clears both fighters' `+0x198`. Full-function reference
  to `0x1463c` is 51 steps / +2 calls / +2 rets. New focused ROM-backed
  fixture `vf2_player_1442c_live_differential` (restores
  `out/park-1442c.vf2snap`, byte-exact live-state equality) plus
  ROM-independent unit test. Heavy collision arms and non-no-op `0x14640`
  branches remain fail-closed. `ctest` 78/78; `native-sixth-dispatch` and
  `native-twelfth-dispatch` unchanged. AGENTS.md Windows environment
  canonicalized to the native CMake/MSVC `build/` path. See
  `decomp/i960/notes/fa_player_1442c_live_v0393.md`.

- Fix do game_info countdown-0 compare-state (v0388):
  `correct_measured_compare_state` agora deixa EQUAL em countdown 0 em
  todas as distribuicoes (antes so f0-only) e LESS caso contrario,
  igual ao helper irmao `correct_countdown_compare_state`. Revalidados
  ROM-backed via `validate_game_info_full_dispatch.py` thresholds
  0,1,2: `0x06214000` 36/36 (era 8/12 na matriz threshold-0), todas as
  50 plus2_plus3 36/36 (1800 fixtures), todas as 12 condition-only
  36/36 (432 fixtures), caudas `0x00204000`/`0x00210000`/`0x00214000`/
  `0x00218000` 36/36 e spot `0x0021c000` 36/36; snapshot + assinatura
  + contadores exatos ate `0x10dcc`. `ctest -C Debug` 72/72. Sem novas
  admissoes; irmaos nao medidos seguem fail-closed. See
  `decomp/i960/notes/game_info_countdown0_compare_v0388.md`.

- Recovery C do coli whole-task live both-fighters `9528/18/19`
  (v0386): `g6=4` (both-live, warm `44`); shell segundo `0x238a4`
  longo `134` (`g3=0`, fighter1 byte `0` com `half 0/0`) vs single `5`,
  shell `9418` vs `9307`; threshold fica warm `17` (`r3==0`,
  `g6==4`) com cluster `0xd4..0xe8` e fighter `+0x18/+0x20` em `0`.
  Midbody tail `110` (`107` body + `EQUAL` `+2`) vs `56`/`86`; seis
  halfwords `fighter+0x6dc`, `g13+0xc/0xe`, `fighter+0x8d4` e
  `0x0051498c= e8 21 02` medidos. Gate `9528` ao lado de `9393`/`9385`
  com `EQUAL` `+2`. Fixture `vf2_coli_whole_task_live` estendida a 3
  modos. `ctest` 72/72; ASan/UBSan green. See
  `decomp/i960/notes/fa_coli_whole_task_both_live_v0386.md`.

- Recovery C do coli whole-task live single-fighter f0 `9393/17/18`
  e f1 `9385/17/18` (v0385): flag builder `0x233d0` live `53` (`g6=2`,
  tabela `0x2330c` f0 vs `0x23324` f1, `0xFFFFDFFC` em `0xb4/0xb8`,
  `0x88=3`) vs warm `44` (`g6=0`); shell `0x23524` live threshold em
  `0x236b0` (`r3==4294959100`, `g6==2`) com `26` vs `17` e `12` calls
  vs `13`, seis stores `0xd4..0xe8` e fighter `+0x18/+0x20` pinados em
  `0xFFFFDFFC`; whole-task `9393`/`9385` com `EQUAL` e `+1`/`+2`
  counts. Fixture ROM-backed `vf2_coli_whole_task_live` via
  `coli-parked-221e8`. `ctest` 72/72; ASan/UBSan green. See
  `decomp/i960/notes/fa_coli_whole_task_live_v0385.md`.

- Recovery C do coli g3-scan `0x238a4` live (single fighter `+0x1a4`
  bit8) (v0384): helper `0x238a4` nativo com `136`/`0x18`/`EQUAL` e
  `134`/`0`/`EQUAL` (f0/f1 espelhados; fixture ROM-backed
  `vf2_coli_238a4_live` via `coli-parked-221e8`). Flag builder
  `0x233d0` agora permite `xor bit8` com `0x820==1` (`g6` 2 vs 0).
  Shell `0x23524` conta `b238+1`. Gate whole-task agora permite
  `9385/17/18` (f1) ao lado de `9393` no nível helper. `ctest` 70/70;
  ASan/UBSan green. See
  `decomp/i960/notes/fa_coli_238a4_live_v0384.md`.

- Recovery C do coli mid-body tail whole-tail live (primeiro hit, segundo
  warm, long) (v0383): `0x22210 -> 0x10dcc` nativo com igualdade total
  de estado (371 steps, 9 calls / 10 rets, CC final + `g1` pinados;
  fixture ROM-backed `vf2_coli_midbody_tail_live` via snapshot base
  medido). Branch v0304 agora encaminha counts/CC/G1 do long
  (`4,4`/`5,5`/`9,9` nested). `ctest` 68/68; ASan/UBSan green.
  Próximo: composição whole-task `0x221e8` (9529). See
  `decomp/i960/notes/fa_coli_midbody_tail_live_v0383.md`.

- Recovery C do coli `0x22404` live first-contact stale slot (v0382):
  shape (slot velho `0xffff` vs snap 0) nativo com igualdade total
  de estado (body 77, `g0 = 1`, CC final pinado; fixture ROM-backed
  `vf2_coli_22404_live`). Fall-through `cmpobe` → `bal 0x225bc`
  (+5); stale+empty e stale slot 1 seguem fail-closed. Fix de oráculo
  junto: `bo`/`bno` após `scanbit` volta ao domínio do legacy (o
  wrapper re-decidia de AC stale e spinava o scan loop em qualquer
  miss) — override AC agora restrito ao domínio integer-compare. `ctest` 66/66; ASan/UBSan green incl. phase17
  202/202. Próximo: composição whole-tail (371/12/10). See
  `decomp/i960/notes/fa_coli_22404_live_v0382.md`.

- Recovery C do coli `0x225cc` live-midbody (v0381): shape
  (`g8+0x1a4 = 0`, `g7+0x1a4 = 0x100`) nativo com igualdade total
  de estado (240 steps, 4 calls aninhados, CC final + `g1`
  pinados; fixture ROM-backed `vf2_coli_225cc_live`). Correções:
  gate `g8+0x6d4 == 0xffff` relaxado (half alimenta só a máscara),
  pack `0x22628` inalcançável removido (`be` sempre tomado com
  `r11 == 0`; −9 fantasma), float-tail `r9 == 0` admitido
  (`0.0/24.0` bit-exato). Pins sintéticos −9 com floats
  recomputados. `ctest` 64/64. See
  `decomp/i960/notes/fa_coli_225cc_live_v0381.md`.

- Sanitizer gate green on Clang ASan/UBSan (Windows): `ctest` 62/62
  em `build-clang-asan` (`-fsanitize=address,undefined`, runtimes
  linkados confirmadas nos binários), cobrindo as mudanças de
  runtime v0378/v0380. Fix de portabilidade junto: `libm` só é
  linkada fora de Windows (ou sob MinGW); MSYS2 GCC segue sem
  runtime ASan (limitação do ambiente, pré-existente).

- Pin de condition codes por caminho no `phase17_zero` (v0380):
  diferencial 202/202 (era 0/189). Trace de 682k steps agrupa todos
  os casos em 12 tails com cauda comum sem compares
  (`ret @ 0xa6f4 → 0x1004`); cada saída nativa replica seu último
  compare medido com AC lockstep (word-scan unsigned vs `0xffffff`,
  tail `fa_control0` como função do mode byte `*(0x50002b)`,
  preâmbulo `cmpobe 0, *0x5000a6`, saída do countdown do rect, tail
  index8 no `player0+0x158` pré-update, tails por next de transição).
  Estabelecido: no path `vf2_i960_run`, `executor.c` compila com
  `vf2_i960_step=vf2_i960_step_legacy` (COBR `cmpo/cmpi` escreve CC,
  `bbs/bbc` nunca — 3312 execuções, zero mudanças). `ctest` 62/62
  sem regressões. See
  `decomp/i960/notes/phase17_cc_pins_v0380.md`.

- Recovery C de `fa_pol_test` path A + helper `0x7f24` (v0378):
  `vf2_recovered_pol_test_path_a` cobre gate `mode>=2`, pack de 6
  words, FIFO gold de 15 words, loop/final submits `0x986/0x985` via
  `0x7c60` reutilizado (pins 163/162/127). Oracle re-medido: traces
  param em `0x21b00` com 162/161 steps e FIFO gold verbatim no
  memory-trace. Path B, semantica da paleta, vertex stream e dispatch
  do scheduler seguem unsupported. `explore_geo_edges.py`
  reclassifica um push geo-stream fora do `0x7c60` (`0x19684`);
  classes TGP `0x07/09/0b/0c` = 0; consumo de `w2` fora do guest.
  Unit + diferencial ROM-backed. See
  `decomp/i960/notes/pol_test_path_a_v0378.md`,
  `geo_edge_coverage_v0378.md`, `tgp_w2_consumption_v0378.md`.

- Packet boundary pol_test + scene phase5 + matrix ports (v0377):
  **P1** mede `fa_pol_test@0x21a00` + helpers `0x7c60`/`0x7f24`:
  i960 grava **apenas** tabela w0/w1/w2 + `r11=-1` + protocolo FIFO
  (`0x800101,0x1800303,0x3000606,0x1a003434,0x1000202`); **vértices
  de polygons.bin nunca aparecem** no oracle i960 — TGP consome via
  `w2` fora do guest. Decode vertex-stream **ABSENT/UNPROVEN**;
  `skip3_float_link` mantido só como hipótese host (gold hex id
  `0x97d` attr `0xe1001601` → quad). pol_test `w3=((n-1)<<16)|n`.
  **P2**: 11 portos (aperture `0x90e000`, geo `0x800000/804000`,
  function `0x880000`, upload `0x980000`, work-RAM, FIFO…) — matrix
  TGP/focus **ausentes** (`confidence=absent`); aperture só serializa
  display triple; `0x0b001616` é tag copro classe 0x16. **P3**: cena
  attract phase5 com **112** ids medidos + tags FIFO
  `confidence=protocol_tag` (não paleta de jogo); strips temporais
  + grid em `out/attr-render/v0377/scene/`. Logo 3D nomeado
  **fail-closed**. Tools Python + notes; sem mudança de semântica
  recovery C nesta fatia. See
  `decomp/i960/notes/packet_format_p1_v0377.md`,
  `matrix_ports_p2_v0377.md`, `scene_phase5_p3_v0377.md`.

- Recovery C do submit polygon `0x7c60` + camera store IPs (v0376):
  **`vf2_recovered_polygon_object_submit`** (`src/recovered/
  polygon_object_submit.c`) recupera o body medido `0x7c60–0x7d10`:
  gate `0x50101c > 0x501018 → ret` sem stores; preâmbulo FIFO
  `0x1a003434`; tabela `ldq 0x020e0004[g0*16]`; `st w0 → g10+0x10`;
  `stq` via `(g10)[g12]` com **r11=-1**; caminho opcional `g1`
  (`r9 += (w3>>16)*4`, `0x5010d0 += w3_low`); contadores
  `0x501010++` / `0x50101c += w3_low`. Fail-closed: tabela ausente →
  `OUT_OF_BOUNDS`; IP errado → `UNSUPPORTED`. Pin ROM-backed
  **`0x148→0x000b026a`**, **`0x88→0x0008e6de`**. Vizinhos `0x7d14`/
  `0x7d6c`/`0x7e50`/`0x1962c` permanecem unsupported. Oracle de
  estado de câmera: store IPs **`0x31024`** (display triple
  6.0/4.7/18.5 em `*(0x50084c)+0x54`) e **`0x1d34c`/`0x1d35c`**
  (escala `600.0f` em `0x501084/88`); FIFO `0x0b001616` é tag copro
  aritmética (classe 0x16), **não** opcode TGP matrix; matrix/focus
  TGP 3x4 seguem **absent** nos streams. Host render: views de
  análise + fix do contact-sheet; PNGs `out/attr-render/v0376/`.
  Focused CTest Debug observado **9/9 Passed** (incl.
  `vf2_polygon_object_submit` + `_differential`). Logo 3D nomeado
  **fail-closed**. See `decomp/i960/notes/tgp_camera_state_v0376.md`,
  `host_render_views_v0376.md`, `polygon_object_submit` notes via
  UNCOVERED v0376.

- Host mesh pipeline + FIFO transform absence (v0375): **análise visual**
  das malhas polygon-ROM medidas — não recovery, **logo 3D nomeado
  fail-closed**. Tools: `render_mesh_host.py` (multi-view PNG, z-buffer,
  dual-decode skip3/noskip alinhado a `tgp.c`), `rank_poly_objects.py`
  (4096 ids da tabela `0x020e0004`; skip3 primário — malhas densas
  **skip3_dominant**, ex. `0x5c7=515`, `0x1cb/0x33e=384`, `0x08f=257`),
  `extract_fifo_transforms.py` + `apply_tgp_transform.py`. Oracle
  attract/boot: commands TGP class **0x09/0x0b/0x0c** (focus/matrix/
  translate) **ausentes** após filtro de protocolo FIFO; bits 23–27
  colidem com cores (`0x14802929`…). Estado host medido: display triple
  `*(u32*)0x50084c+0x54..5c = (6.0f, 4.7f, 18.5f)`, escala câmera
  `0x501084/0x501088=600.0f`. `vf2probe --max-steps 0` **não** congela
  o park (preferir snap-parser). PNGs em `out/attr-render/` (não git).
  See `decomp/i960/notes/render_mesh_host_v0375.md`,
  `object_rank_v0375.md`, `fifo_transforms_v0375.md`.

- Attract object-id attribution (v0374): natural phase5 geo/FIFO writes
  de table w0 atribuídos a **`0x7d08`/`0x7d0c`** (helper `0x7c60`).
  Callers que carregam `g0`: **`0x190f0`** (ponteiro de estado),
  **`0x61cc8`** (walker de lista stride +24), **`0x21134`** (tabelas
  halfword `0x6ee38`/`0x6fe38`). IDs naturais incluem `0x88`, `0x143–
  0x152`, `0x4d1–0x4eb`, `0xd06–0xd11` — **não** imediatos ROM.
  `display_command_emit 0x31040` usa `0x70cbc[0]=0xee1` (mesmo em
  TEST); sem overlap com a família attract. Sem ASCII SEGA/LOGO/TITLE
  em ROM de objetos; lutadores AKIRA/WOLF/PAI presentes sem ponteiro
  para ids de attract. **Logo 3D nomeado: fail-closed.** See
  `decomp/i960/notes/logo_named_witness_v0373.md`.

- Object-submit attribution + alt selectors (v0373): ~**140** ROM
  `call 0x7c60` sites (dest=`ip+signed24`); clusters display
  `0x19xxx` / `0x20xxx` (54, incl. pol_test ids `0x97d–0x986`) /
  `0x2e4e4–0x31778` (49, `display_command_emit 0x31040` via table
  `0x70cbc`). Attract phase5 still submits **unnamed** polygon ids
  `0x88/0x14x`. Alt sel sweep: only **sel 9** from boot park opens
  FIFO (1408 writes); no SEGA/LOGO ASCII; residual tiles `t4e jef`
  ≠ título. Logo 3D nomeado **fail-closed**. See
  `decomp/i960/notes/logo_object_submit_v0372.md`.

- Objetos polygon no attract (v0372): oracle phase5 **lê** a tabela
  `0x020e0004` (ids **0x148/0x88/0x145…**) e **grava** word0 em
  geo/FIFO (`0xb026a` = id 0x148). Helper `0x7c60` + `fa_pol_test`
  `0x21a00` medidos no oracle: FIFO `0x1a003434` + protocolo
  idêntico ao attract. `w2` com bit `0x800000` → polygons.bin em
  **word index** (`id 0x148` → word `0x40430`, w3=`0x014a0164`).
  Sem ASCII SEGA/LOGO em ROM. Render host produz triângulos 3D
  (ex. id 0x1cb 384 tris) **sem** forma nomeada de logo. Logo 3D
  nomeado continua fail-closed. See
  `decomp/i960/notes/attract_poly_objects_v0372.md`.

- Status-tail oracle ROM real (v0371): park attract, `0x4d25c→0x4d2bc`
  mede mode **0x03** = **156** passos (só dest `0x010000e2`), mode
  **0x0c** = **305** e **0x0d** = **307** (dests special+common;
  delta **+2** = pin C). ROM `0x4d28c`/`0x4d2ac` = 15 espaços+NUL →
  glyphs `0x8020`. Thunk phase14: 11 passos a `0x9444`, 15 a
  `0x9468` sem tiles neste park. Unit `run_common_only_spaces`
  pinna mode3 common-only. Logo 3D fail-closed. See
  `decomp/i960/notes/status_tail_oracle_v0371.md`.

- Phase stores + status-tail bit9 clear (v0370): varredura maincpu
  mapeia **91** sites `ldib/stib` de `0x500030`; no sel3 o avanço é
  `phase+1` nos workers (phase14 **não** tem store). Memory-trace
  oracle 80k passos attract ready=0: **0** writes em `0x500030`
  (só snapshot `0x500031/34`). Thunk `0x7fc0` = blit C-string →
  glyphs `0x80xx`. Final-status counters0 + board bit9 **clear**:
  **13** passos até `0x4d25c`, ready 0; `0x50002b=0x03` → dest thunk
  `0x010000e2`. C tail já fail-closed em mode `0x0c/0x0d`. Unit nova
  pin bit9-clear. Logo 3D fail-closed. See
  `decomp/i960/notes/phase_stores_status_tail_v0370.md`.

- Cauda sel3 + fail-closed phase14/15 (v0369): oráculo a partir do
  park phase14/ready=0 mede thunk **0x9444** em **11** passos (phase
  **não** avança; resume 400k permanece 0x0e em spin `0x4c7xx`).
  Workers forçados: **15→16** (mask=1, ctr=128), **16→17**, **17→0**.
  ROM phase15 compara mask com **0x700/0x540/0x380/0x1c0** e blita
  descriptors main_data (ex. `0x02a69cd2` 7×26 → glyphs **0x88xx** no
  tile plane `0x01000124`); **não** é string SEGA nomeada. C agora
  **fail-closed** nesses masks e no not-ready de phase14. Logo 3D
  continua unwitness. See
  `decomp/i960/notes/attract_tail_phase15_masks_v0369.md`.

- Pin oracle final-status `0x4bf90` (v0368): com counters 0 e ready=1
  o oráculo executa **12** passos até `0x4bfdc` e grava
  **`0x550000=0`**; ctr0=1 ou ctr2=1 mantêm ready=1 (6/10 passos).
  Board bit9 set pula `call 0x4d25c`. C unit já pinava **13** insns
  (12+`ret`) — coerente. Entrada ROM: scan `0x4bd24` → `0x4bf90`.
  Após clear no attract, phase14 **não** ++phase (worker não grava
  phase+1). CTest orchestrator/texture **8/8 Passed**. Logo 3D
  fail-closed. See `decomp/i960/notes/final_status_pin_v0368.md`.

- Ready-latch + FIFO/texture (v0367): ROM stores em `0x550000` =
  set **1** (`0x4b414/0x4b83c/0x4ba14`) e clear **0** em
  **`0x4bfc4`** (r14==0 e ctr2 `0x5502e0`==0). C
  `execute_texture_final_status_call` espelha o clear; **model2a/TGP
  não** limpam o latch sozinhos. Parks phase14: ready=1 com
  contadores 0 — clear não roda no spin de objeto. Worker phase14
  (ROM+C) **não** escreve phase+1 → fases 15+ não alcançadas por
  esse caminho. Correlação: attract FIFO **1157–2016** writes +
  nz_tex **~10154** vs TEST FIFO **9** + nz **0**; sem mesh nomeada
  → logo 3D fail-closed. See
  `decomp/i960/notes/ready_clear_fifo_corr_v0367.md`.

- Attract sel3 phases 3–14 + gate `0x550000` (v0366): oracle alcança
  fases **3..14** com nav limpo e scouts de counters medidos
  (`0x500024`, `0x515b50`, `0x500028` com sel=3 preservado). Workers
  phase8/9/14: ctr=0 e `0x550000==1` → **ret** (espera); not-ready →
  thunk `0x9444` (v0026: 26 insns). Natural pós-phase7 tem ready=1;
  oráculo re-arm ready=1 e phase14 trava. Coli spin `0x224xx` trava
  scheduler sem visitas a `0xa6c0` (recete `--set-ip`). Sem logo 3D
  nomeado. See
  `decomp/i960/notes/attract_phases_ready_gate_v0366.md`.

- Attract longo + FIFO TGP (v0365): com nav limpo o oráculo drena
  phase3 (cd 256→0, ~1/frame), avança **fases 4→8** (sel3, sem TEST).
  Texture 64k nz **0→4360→~10154** no attract vs **0** em TEST.
  Memory-trace FIFO `0x884000`: attract **1157–2016** writes vs TEST
  **9**. Fase 8 fica em loop de objeto `0x4c7xx`; sem tile/malha SEGA
  3D nomeada → logo 3D **fail-closed**. See
  `decomp/i960/notes/attract_long_fifo_v0365.md`.

- Executor COBR `teste` + attract TGP (v0364): `test*` (op 0x20–0x27)
  grava `0xffffffff/0` no operando a partir de `compare_result`
  (unit ROM `0x22780000`); `vf2_tests` all passed; focused CTest **7/7**
  sem regressão sixth-dispatch. Attract nav-clear atravessa `0x19024`
  (sel3/phase3, cd 256→253, sem TEST); texture first-64k **nz=4360**
  no park long vs 0 em TEST. Memory-trace: **1157** writes TGP FIFO
  `0x884000` com function codes e floats no attract fase 3.
  Logo 3D/malha SEGA **não** pinado. See
  `decomp/i960/notes/teste_attract_tgp_v0364.md`.

- Referência MAME TGP/Model 2 (v0363): dump limpo em `third_party/`
  (mb86233 + model2.cpp, BSD-3-Clause) + `tools/python/tgp_disasm_mame.py`
  analysis-only. Política: MAME sugere → oráculo mede → C prova; sem
  link no runtime. Mapa copro/geo/FIO/sincos documentado vs `tgp.c`.
  See `decomp/i960/notes/mame_mb86233_reference_v0363.md` e
  `third_party/README.md`.

- Attract pós-SEGA: gate de input + fase 3 (v0362). O oracle grava
  **sel 2→0x10** em `0xa748` quando `0x500704` tem **bit 26 ou 2**
  (trace: `stib 0x10` em `0xa76c`; park lia `0x0f000000`). Com
  `resume-trace` zerando `0x500704` a cada frame a partir de
  `sega-after-cd`: sel **02→03**, phase3 **0→3**, geometria/buffer
  mudam, **sem** TEST MENU. Oráculo para em **`teste` @ `0x19024`**
  (executor sem semântica COBR `test*`) no corpo de objeto/fighter do
  attract. Logo 3D / malha SEGA **não** witness. COUNTRY não desvia o
  handoff TEST quando o gate está ativo; COUNTRY≠0 pula a SEGA legal.
  Tools: `dump_attract_rom.py`, `dump_attract_state.py`,
  `attract_navclear.py`, etc. See
  `decomp/i960/notes/attract_nav_gate_teste_v0362.md`.

- Attract pós-SEGA medido (v0361): caminho natural
  **SEGA warning (sel 0→1) → sel 2 → sel 3 → 0x10 → 0x11 TEST MENU**
  neste backup/config. Tabelas ROM corretas: sel2=`0xab0c`,
  sel3=`0xacf8` (phases 0–17), sel16=`0x10a0c` (C grava a4=0x0b),
  sel17=`0x10b5c`. Recovery documenta handoff fases 12–15 → sel 16.
  Forçar sel=4..15 também termina em TEST MENU. Texture-ram hash muda no
  handoff sem malha 3D SEGA na FIFO. Logo de jogo permanece fail-closed
  (falta estado de máquina fora do handoff de teste). See
  `decomp/i960/notes/attract_sel3_to_testmenu_v0361.md`.

- Tela SEGA alcançada (v0360): o **frame selector 0** desenha a assinatura
  legal Model 2 cujas strings ROM incluem `SEGA ENTERPRISES,LTD.` (`0xaaad`).
  Witness natural: `park-after-irq` (COUNTRY=JAPAN, assinatura `0xa5a5…` não
  casa) → 1 frame **15853** insns, selector **1**, countdown **640**, glyphs
  `0x89xx` nos destinos `0x01000332..0x01001650`. Warm com assinatura em
  `0x59cfe0` **pula** o desenho (fast path 34 → sel 2) — por que v0353–v0356
  nunca viram SEGA. Decoders `0x80xx` não liam glyphs estilizados. C
  `execute_selector0_body` já cobria o ramo draw; unit
  `test_frame_dispatch_selector0_sega_warning_draw` pin 15853/sel=1/640.
  Tools em `tools/python/dump_sel0_strings.py` e `decode_glyph_tiles.py`.
  See `decomp/i960/notes/sega_warning_screen_v0360.md`.

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
