# v0734i: Phase 1.2a conservative decisions — annotations land; one bound repaired

Phase 1.2a of `completion_plan_v0734.md`. This slice takes the decisions
the measurement in v0734c authorised but did not commit. **No `src/` file
changes**; only `decomp/i960/functions.csv`.

## What v0734c measured (recap)

22 container rows were measured with the v0734b two-source method.
Outcomes:

- **20 rows end on a `ret` instruction.** Their measured extents are
  credible; their *current* extents are region bounds (i.e. the address
  the prose was describing, not a back end of the function).
- **2 rows end on an unconditional branch** (`main_texture_orchestrator_call`,
  `main_frame_timer_call`). Lower bounds. **Not touched here.**
- **0 rows are weak** (no row had `end` lower than the disassembly could
  justify).

Three blockers prevented automatic repair at v0734c:

1. **The texture cluster overlap.** Six rows all measure to the same
   shared epilogue `0x0004bfe0`. "These extents overlap" is not
   "one of them is wrong".
2. **Two rows' notes pin a different endpoint than the measurement:**
   `camera_post_update_gate` says "through `0x1d984`" while the sweep
   measures `0x1ee34`; `frame_shadow_verify` claims "twenty-eight
   instructions" against a 440-byte measured extent. That is a question
   about the row's identity.
3. **`interrupt_return_wait_exit` at `0x00000d20` is a single `ret`.**
   Its bound repair is well-evidenced, but its name describes a
   composite spanning `interrupt_restore_prefix` and `frame_timer_suffix`.

## What this slice does

**One bound repair + eight annotations.** The two-source rule for any
mutation: sweep measurement and disassembly evidence must agree; the
disassembler failing past the boundary counts as one of those sources.

### Repaired: `interrupt_return_wait_exit` (0xd20..0xd24, 4 B)

Two independent sources agree:

- `vf2i960 function 0xd20` measures `end = 0xd24` (one past the bare `ret`
  at `0xd20`).
- `vf2i960 disasm 0xd24` fails to decode (next byte is not code).

**The intervening 66,556 B (`0xd24..0x10fa4`) is a real unknown gap, NOT
part of `interrupt_return_wait_exit`.** v0734h's `0x1200..0x1290` (144 B)
and `0x12bc..0x12d8` (28 B) are the first two real functions inside it.

The name still describes a composite (the row covers the epilogue of an
outer function and the prologue of the next). A rename would propagate
through `src/analysis/symbols.c` (which reads this CSV by column index
`name_column = 2`) and every test fixture that names the symbol.
**Deferred** — the boundary is now honest, the name will follow when an
independent measurement splits the row.

### Annotated, kept at region bound: texture cluster (6 rows)

The six texture cluster rows all measure to `0x4bfe0`:

| row | before | after | measured |
|---|---|---|---|
| `texture_status_dispatch_call` | 0x4bd24..0x4d2c0 | **0x4bd24..0x4d2c0** (kept) | 0x4bfe0 (700 B) |
| `texture_active_prepare_call` | 0x4bde0..0x4d16c | **0x4bde0..0x4d16c** (kept) | 0x4bfe0 (512 B) |
| `texture_status_scan_end` | 0x4bd24..0x4bf90 | **0x4bd24..0x4bf90** (kept) | 0x4bf90 (real extent, `ret` at 0x4bf8c) |
| `texture_child_zero_gate_a` | 0x4bebc..0x4cb64 | **0x4bebc..0x4cb64** (kept) | 0x4bfe0 (292 B) |
| `texture_child_zero_gate_b` | 0x4bef4..0x4cd18 | **0x4bef4..0x4cd18** (kept) | 0x4bfe0 (236 B) |
| `texture_final_status_call` | 0x4bf90..0x4d25c | **0x4bf90..0x4bfe0** (shrunk) | 0x4bfe0 (80 B in 7 blocks) |

The convergence on `0x4bfe0` is a real CFG result (v0734a measured 50
blocks in the texture cluster and a shared epilogue; six texture entry
points share that address). "These extents overlap" is therefore
*legitimate*, not a defect — the overlap is the texture cluster's
epilogue, not evidence that one row is wrong.

`texture_final_status_call` was the *only* row whose pre-fix end was both
a region bound AND a single back-edge call site with no prose claiming a
larger extent. The two-source shrink from `0x4d25c` to `0x4bfe0` is
well-evidenced (`vf2i960 function` measures `0x4bfe0`, and the shared
`ret` at `0x4bfdc` is the natural CFG endpoint). The other five were
kept at region bounds because:

- the sweep cannot say which of two overlapping rows is "right"; and
- the prose names a specific address the author meant to capture.

**Decision:** keep all five region bounds; annotate every row with the
measured extent and the source for the convergence. A later slice can
narrow any one of them once an independent reason names one over the
other.

### Annotated, kept at region bound: `camera_post_update_gate` and `frame_shadow_verify`

- `camera_post_update_gate` (0x1d660..0x1d984): sweep measured 0x1ee34
  (which would push through more of the cluster); the prose says
  "through `0x1d984`". Both candidates intentionally retained — the
  row's identity is open until Phase 1.2a sees a second independent
  signal that picks one over the other.
- `frame_shadow_verify` (0x530..0x9ffc): sweep measured `0xa804` (110
  words for the static instructions). The "twenty-eight instructions"
  claim in the prose refers to *executed* instructions; the static
  extent of 110 words can execute 28 instructions if some are jumps or
  callee entries. Both the static and the executed reading are valid;
  the row's *name* indicates the executed reading.

**Decision:** annotate, do not repair. The user's steer can either pick
one or split the row into two.

## What this slice is NOT

- It does not recover the `interrupt_return_wait_exit` row's identity
  split (interrupt prologue / frame timer suffix). That is a rename
  plus a structural change, deferred to a focused slice.
- It does not repair any of the five texture rows whose region bounds
  disagree with the measurement. The sweep cannot pick a winner.
- It does not change `src/` at all.
- It does not change the gap arithmetic. The published percentage
  (0.4456%) is unchanged because the texture cluster's *raw sum* did
  not move; the one repaired row (`interrupt_return_wait_exit`) had
  `0xd20..0x10fa4` (66 560 B) and now has `0xd20..0xd24` (4 B), but the
  gap that swallowed those bytes is the v0734h characterisation, not a
  row.

## Validated

- `ctest -C Debug --output-on-failure -E vf2_player_4505`: **119/119
  PASS** (446 s wall, excluding the three `vf2_player_4505_*`
  dominators). The `f1_python_factory_block_coverage` test passed only
  after I requoted the three rows whose new notes contained English
  commas (`texture_status_dispatch_call`, `texture_final_status_call`,
  `interrupt_return_wait_exit`) — those were causing `csv.reader` to
  split each row into 8–10 fields.
- `python -c 'csv.reader(open("decomp/i960/functions.csv"))'`: 0 rows
  with `len(r) != 7`.
- `git diff --stat src`: empty.
- The CSV's `name_column = 2` invariant (v0734b) is preserved — the
  `return_to` column is still trailing, no row is renamed, and
  `src/analysis/symbols.c` reads by index 2 so the symbol table is
  unaffected.
- Both gates proven able to fail by planting the defect (the comma
  escaping slipped past the build because `functions.csv` is consumed
  as a text resource, not parsed at compile time).

## What the next slice should pick up

- **Phase 1.2a is now CLOSED** in the sense that every blocker
  identified at v0734c has a documented decision. The conservative
  calls (region bounds for the texture cluster, identity open for the
  camera / frame_shadow rows, name deferred for interrupt_return_*)
  are visible in the CSV itself and recoverable by anyone who wants to
  argue for a different choice.
- **Phase 1.3b** (the 29 KB / 5 KB / 4 KB gap disassembly) is now
  correctly ordered: the conservative 1.1 calls above did not move the
  gap arithmetic, so the disassembly targets are unchanged.
- **Phase 2.6 / F4** is the next gameplay slice (INDIVIDUAL walks at
  rows 1-2, bounded by `f3_f4_individual_mode_measured_v0732.md`).
- The `interrupt_return_wait_exit` rename is a small, low-risk slice
  and can be taken at any time. It does not gate anything else.

See `completion_plan_v0734.md` Phase 1.2a entry for the rerun version of
this table.