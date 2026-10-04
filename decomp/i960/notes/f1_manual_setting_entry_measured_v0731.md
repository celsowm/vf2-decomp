# F1 slice: COIN ASSIGNMENT row-5 (MANUAL SETTING) entry, measured (v0731)

Follow-up to `f1_manual_setting_target_remeasured_v0731.md`. That note
redefined F1 as "which input path selects the row-35 MANUAL SETTING entry".
This note answers it, using the frame-advance workflow from
`frame_advance_workflow_v0731.md`.

## 1. Answer: a TEST press on cursor row 5 selects MANUAL SETTING

The COIN ASSIGNMENT SERVICE cursor ring is **six positions**
(`phase_a5 = 0..5`, `cursor_addresses[6]` at
`texture_bridge_match.c:7916`-`7920`). Row 5 is the **MANUAL SETTING** row
— the label rendered at screen row 35 in the measured run table
(`:7957`) — not a seventh position, and not a separate selector index.

Measured from a legitimate walked state (`out/f1-a4-next.vf2snap`,
`phase_index 0x85`, `phase_a5 = 5`, frame-entry `ip = 0x538`), one bridge
frame stopped at the next frame wait:

| leg | latch patch | `run_instructions` | `a4` | `a5` |
| --- | --- | --- | --- | --- |
| SERVICE/DOWN tap | `0x500700=0x0f001000`, `0x500704=0x1000`, `0x500708=0`, `0x50070c=0x0f000000` | 5343 | 0x85 | **0** (wrap) |
| TEST press | `0x500700=0x0f000004`, `0x500704=0x4`, `0x500708=0`, `0x50070c=0x0f000000` | **15214** | 0x85 | 5 |

The forward tap at row 5 wraps to row 0, confirming the six-position ring
and the `phase_a5 == 5 -> 4195` wrap body. The TEST press does **not**
move the cursor and does **not** change `phase_index`; it enters the
nested page.

## 2. Measured poststate of the entry frame

From `--memory-trace` on the TEST press (15214 `step` records, 3067 writes,
889 reads). Writes to the state and cursor cells, in order:

```text
0x0050002c <- 0x00000200   selector mask (was 0x00020000)
0x010011a0 <- 0x801c       cursor ON  at row 5 (0x010011a0)
0x005000a7 <- 0x00         phase_a7: 0xff -> 0x00   (nested page entered)
0x010002a0 <- 0x0020       cursor off
0x01000320 <- 0x0020       cursor off
0x01000420 <- 0x0020       cursor off
0x010005a0 <- 0x0020       cursor off
0x010011a0 <- 0x0020       cursor off
0x01001320 <- 0x0020       cursor off
0x00500020 <- 0x0000001e   = 30
0x0050006d <- 0x00
0x0050015c <- 0xffffee25   frame counter
```

2835 of the 3067 writes land in `0x10xxxx` (the tile plane): a **full page
render**, not an incremental redraw. That is the MANUAL SETTING page
appearing.

`phase_a7` moving `0xff -> 0x00` is the signature of the nested page, and
it matches the v0727 note's "the whole nested a7 editor" language.

## 3. The recovered C already implements this path

`execute_frame_phase17_bit7_index5` already contains the entry:

```c
if (phase_a5 == UINT8_C(5) && phase_a7 == UINT8_C(0xff) && edit_delta != 0) {
    const uint8_t manual_state = 0u;
    ... "SELECT BY SERVICE BUTTON" / "AND PUSH TEST BUTTON" ...
    vf2_model2a_write(machine, UINT32_C(0x005000a7), &manual_state, ...);
    ...
    instructions = edit_delta > 0 ? UINT64_C(14063) : UINT64_C(14060);
    calls = UINT64_C(36);
```

(`texture_bridge_match.c:8206`-`8284`.)

So the write of `a7 = 0`, the "SELECT BY SERVICE BUTTON" footer, and the
return to `0x0000a010` are all already recovered, and the **nested a7
navigation arms are present too** (`:8185`-`8190`,
`manual_navigation_delta` on `nav` `0x1000`/`0x2000` when `a7 != 0xff`).

### 3.1 Measuring the body correctly (and retracting a first attempt)

**A first attempt at this number was wrong and is retracted here.** Stopping
the leg at the frame *wait* (`0x10f98`) gives 15214, and subtracting a
calibrated overhead appeared to land on 14062. That was a coincidence. An
instruction histogram of the 15214-step leg shows it is dominated by a
**poll loop**, not by the entry body:

```text
0x08f00 x2418   0x08efc x2418   0x08f04 x2418   0x08f08 x2418
0x07fcc x425    0x07fd0 x425    0x07fd4 x425    0x07fd8 x425
0x07fdc x397    0x07fe0 x397    0x07fe4 x397    0x07fe8 x397
```

Those four `0x08fxx` addresses alone are 9672 of the 15214 instructions —
the newly entered MANUAL SETTING page's own wait — so the "overhead" was
not a fixed cost and that arithmetic was meaningless.

The correct boundary is the block return the recovered code itself
asserts, `0x0000a010`. Measured to that boundary from the same frame-entry
chain points:

| leg | measured to `0xa010` | recovered body | expected (+232 prefix) |
| --- | --- | --- | --- |
| `a5 = 4` TEST (proven) | 4634 | 4403 | 4635 |
| **`a5 = 5` TEST (new)** | **14294** | — | 14063 + 232 = 14295 |

Both agree to one instruction, which is the `ip = 0x538` vs `0x530` chain
offset. The call count reconciles exactly: the `a5 = 5` leg advanced
`procedure_calls` by 41 = the 232-step cluster prefix (5 calls) + the
recovered body (36 calls).

So `14063` / `36` is **confirmed by direct measurement at the correct
boundary**. The 15214 figure stays in section 1 because it is the correct
measurement of a *different* boundary — the whole leg to the next wait,
including the page's poll loop. Both are true; keep them labelled.

### 3.2 The strict differential is not yet established

Adding the tuple needs a `native-resume` MATCH, and that path is currently
blocked. Three resume points were tried, all from otherwise-correct states:

| resume ip | how obtained | `native-resume` result |
| --- | --- | --- |
| `0x00000538` | frame-entry chain point, TEST latch patched in | unsupported after 0 blocks |
| `0x0000a6c0` | captured with `--until 0x0000a6c0` (231 insns in) | unsupported after 0 blocks |
| `0x00010b5c` | captured with `--until 0x00010b5c` (239 insns in) | unsupported after 0 blocks |

The `0x00010b5c` capture is a useful result in its own right: that address
**is** reachable 239 instructions into the frame, and the latch is still
live there — `0x500700 = 0x0f000004`, `0x500704 = 0x4`,
`0x50070c = 0x0f000000`, `phase_index 0x85`, `a5 5`, `a6/a7 0xff`. So the
entry state the native path needs is buildable; the blocker is purely the
router.

Why it is refused: the bridge step is a `switch` on `cpu->ip`
(`texture_bridge.c:335` onward) with roughly 40 recognised cases, and
`0x00010b5c` is not one of them — `VF2_FRAME_DISPATCH_TICK_ENTRY`
(`0x0000a6c0`) is only a *reported* address inside the selector-0/2
handlers, not a router case. The selector-17 route is guarded by
`if (target != UINT32_C(0x00010b5c)) return VF2_ERROR_UNSUPPORTED;`
(`texture_bridge_match.c:21244`), which is a check *inside* the phase
dispatcher, so the entry that routes *into* that dispatcher is a different
address and is still unidentified.

The v0727 session's differential driver was never committed, so whatever
harness produced those chained proofs is absent from the tree. Until the
router entry is identified, the tuple stays out of the table: per AGENTS.md
rule 1 an unproven admission must not be added, and rule 5 forbids
weakening validation to make a recovery pass.

## 4. Gate preconditions verified at the entry state

Read from `out/f1-a4-next.vf2snap` with `--max-steps 1`, against every
condition in the `execute_frame_phase17_bit7_index5` gate
(`texture_bridge_match.c:8167`-`8182`):

| gate condition | required | measured | ok |
| --- | --- | --- | --- |
| `indirect_target` @ `0x0005fed0` | `0x0005b558` | 374104 = `0x0005b558` | yes |
| `selector_mask` @ `0x0050002c` | `0x00020000` | 131072 = `0x00020000` | yes |
| `phase_a5` | `<= 5` | 5 | yes |
| `phase_a6` | `0xff` | `0xff` | yes |
| `phase_a7` | `0xff` (and `<= 4` once set) | `0xff` | yes |
| `preset` @ `base+0x3324`, `base = 0x00599000` | `0` | 0 | yes |
| `coin_flags` @ `base+0x3320` | COMMON (bit 0 clear) | 0 | yes |
| `released_flags` | 0 | 0 (patched) | yes |

**Every precondition of the recovered handler is already satisfied at the
measured entry state.** The only thing standing between this frame and the
native path is the absent `natural_latches[]` row. The candidate tuple,
matching the proven shape of the `a5 = 1..4` TEST rows (`:8096`-`8099`) and
this measurement, is:

```c
{UINT32_C(0x0f000004), UINT32_C(0x0f000000), 0u, UINT32_C(0x4), UINT8_C(5), 0u, 0u},
/*  input                previous              rel  nav   a5             keep coin */
```

`keep_entry_globals = 0` because the entry is an edit-class shape that pins
`g1`/`g2` rather than keeping the flat globals, and `coin_mode = 0` because
`coin_flags` is 0 (COMMON) at the measured state.

This tuple is **not committed here**: the differential proof below is still
outstanding, and per AGENTS.md rule 1 an unproven admission must not be
added.

## 5. What is actually missing

Not the C. **The admitting latch tuple.**

`test_held_entry` is set only by an exact match against the
`natural_latches[]` table (`texture_bridge_match.c:8032`-`8120`). The
table has TEST (`nav 0x4`) tuples for `a5 = 1, 2, 3, 4` (`:8096`-`8099`)
and the EXIT row `a5 = 0` (`:8111`), but **no tuple for `a5 = 5`**. With no
match, `test_held_entry` stays 0, and the gate at `:8167`-`8182` rejects the
frame:

```c
input_match = (input_flags == base_input && previous_flags == base_input)
              || test_held_entry != 0;
...
if (... input_match == 0 ...) return VF2_ERROR_UNSUPPORTED;
```

because `input_flags = 0x0f000004 != base_input = 0x0ff7f700`. The
recovered path is therefore currently **fail-closed** for the exact latch
the ROM accepts. That is correct behaviour, not a bug — the tuple is
simply unmeasured.

The slice to finish is:

1. ~~Reconcile the instruction/call accounting against the recovered
   `14063 / 36`.~~ **Done — section 3.1.** Measured to the `0xa010` block
   boundary the `a5 = 5` TEST leg is 14294 (14063 + 232 prefix, within the
   1-instruction `0x538` chain offset) and the call delta is 41 = 5 prefix
   + 36 body. The recovered `14063 / 36` is confirmed.
2. Reconstruct a `0x00010b5c` entry snapshot carrying the TEST latch, so
   `native-resume` accepts the resume (section 3.2: the router rejects both
   `0x538` and `0xa6c0`, and the v0727 driver was never committed).
3. Add the measured tuple to `natural_latches[]` **only after** that resume
   MATCHes, and nothing else.
4. Add a ctest pin for the admitted leg.

Steps 2-3 are deliberately ordered: the router entry has to exist before a
MATCH can be observed, and the tuple must not land before the MATCH.
Do not widen the gate,
do not special-case `a5 == 5` outside the table, and do not relax the
`phase_a7` range check at `:8179`-`8180`.

## 6. Reproducing

Entry state for all of the above is `out/f1-a4-next.vf2snap` (gitignored,
regenerable by the walk in `frame_advance_workflow_v0731.md` section 3).
The two decisive legs:

```sh
# MANUAL SETTING entry (15214 instructions, a7 0xff -> 0x00)
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/f1-a4-next.vf2snap --max-steps 4000000 --until 0x00010f98 \
  --set-u32 0x00500700=0x0f000004 --set-u32 0x00500704=0x00000004 \
  --set-u32 0x00500708=0x00000000 --set-u32 0x0050070c=0x0f000000 \
  --memory-trace

# ring wrap at the last position (5343 instructions, a5 5 -> 0)
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/f1-a4-next.vf2snap --max-steps 4000000 --until 0x00010f98 \
  --set-u32 0x00500700=0x0f001000 --set-u32 0x00500704=0x00001000 \
  --set-u32 0x00500708=0x00000000 --set-u32 0x0050070c=0x0f000000
```

## 7. Anti-traps

- Do not conclude the body size is proven from the leg total. Section 3
  is an estimate with an unreconciled call count.
- Do not widen the `a5` ring to seven positions. The measured forward tap
  at `a5 = 5` wraps to 0; row 35 is reached by the TEST press, not a
  seventh cursor slot.
- Do not treat the `0x5000a7` write alone as the whole entry. The frame
  also flips the selector mask (`0x50002c` -> `0x00000200`), sets
  `0x500020 = 30`, and renders 2835 tile writes.
- Do not reuse the v0730 F1 note's `0x59f34` / `0x5a0a4` identification.
  It remains unproven; see `f1_manual_setting_target_remeasured_v0731.md`.
- Do not commit `out/` snapshots, traces or the `f1_*.ps1` scratch scripts.


