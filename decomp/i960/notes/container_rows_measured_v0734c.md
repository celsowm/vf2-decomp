# v0734c: container rows measured — and they block the gap arithmetic, not the other way round

**Phase 1.2a measurement + Phase 1.3 recomputation.** No row is mutated here.
The measurement says the plan had 1.2a and 1.3 in the wrong order.

## The 22 container rows, measured with the v0734b method

Same two sources: **A** — the instruction at `measured_end - 4` decodes as `ret`
from the ROM; **B** — `measured_end` equals some other row's `address`. A third,
independent signal from v0734b's hand-check: does the first byte *past*
`measured_end` fail to decode?

| verdict | count | rows |
|---|---|---|
| A (`ret`), 6 of which also B | 6 | `texture_stream_header_call`, `texture_status_dispatch_call`, `texture_active_prepare_call`, `texture_status_scan_end`, `texture_child_zero_gate_a`, `texture_child_zero_gate_b`, `texture_record_advance`, `texture_final_status_call`, `texture_post_body_call`, `texture_orchestrator_save_call`, `texture_frame_gate_call`, `input_bit0_sequence_gate`, `frame_wait_poll` |
| A (`ret`) only | 14 | includes `task_camera`, `frame_shadow_verify`, `texture_maintenance`, `input_ring_poll`, `input_bit1_sequence_gate`, `interrupt_return_wait_exit` |
| branch terminal — LOWER BOUND | 2 | `main_texture_orchestrator_call`, `main_frame_timer_call` |
| **weak / refused** | **0** | |

**0 weak is not the same as 0 safe to repair**, and that is the whole finding.

## Why none of them is repaired yet

Three things block a bulk rewrite, each of which a "just take the measurement"
pass would have hidden.

**1. The texture cluster would still be containers of each other.** Six rows
(`texture_status_dispatch_call`, `texture_active_prepare_call`,
`texture_status_scan_end`, `texture_child_zero_gate_a/_b`,
`texture_final_status_call`) all measure to the same shared epilogue
`0x0004bfe0`. Repairing their `end` would give six rows the *same* end and
**overlapping** spans — `texture_child_zero_gate_b 0x4bef4..0x4bfe0` would sit
entirely inside `texture_child_zero_gate_a 0x4bebc..0x4bfe0`. The convergence is
a real CFG result (v0734a measured 50 blocks and a shared epilogue), but
"this row's extent overlaps that row's extent" is a different statement from
"one of them is wrong", and the sweep cannot say which.

**2. Two rows' own notes pin a *different* endpoint than the measurement.**

| row | note says | measured | csv `end` |
|---|---|---|---|
| `camera_post_update_gate` | "normal flag path through **0x1d984**" | `0x1ee34` | `0x1d984` |
| `frame_shadow_verify` | "returns in twenty-eight instructions" | 440 B (110 words) | `0x9ffc` |

The measurement and the prose disagree about what the row *is*. That is a
question about the row's identity, not its bound, and it needs a decision, not
a script.

**3. One row's bound is fine and its *name* is wrong.** `interrupt_return_wait_exit`
at `0x00000d20` is literally a single instruction:

```text
00000d20  0a000000  ret
```

Its note — *"Returns from the interrupt, records the resumed wait visit and
exits on the changed frame byte"* — describes a composite spanning
`interrupt_restore_prefix` (`0xce0..0xd20`) and `frame_timer_suffix`
(`0x10fa4..0x110b0`). Repairing its `end` to `0xd24` is well-evidenced (A and the
post-boundary decode failure both agree) but leaves a 4-byte row carrying a
name that describes 300+ bytes. **Fixing the bound alone would make the table
look tidier and be less true.** The attribution needs settling first.

## The finding that reorders the plan: container rows hide real gaps

Phase 1.3 asked for the gap list to be recomputed. It has been, and the result
is that **the gap arithmetic is not trustworthy while container rows stand** —
because a container's span *absorbs* code that has no row of its own, and the
gap arithmetic only sees uncovered bytes.

The 144-byte gap v0733c exposed at `0x00001200..0x00001290` has **vanished** from
the recomputed list. Not because it was closed — because three rows span it:

```text
video_register_compose     0x00001064..0x00001200   <- ends exactly at the gap
interrupt_return_wait_exit 0x00000d20..0x00010fa4   <- a 66 180 B CONTAINER whose
                                                         real extent is 4 bytes
```

`interrupt_return_wait_exit`'s bogus 66 KB span makes **66 KB of unmeasured code
look covered**. The same is true of `main_texture_orchestrator_call` (269 KB) and
the five 45-48 KB input rows. **Every gap number in the table is a lower bound
on the real unknown, by exactly the amount those seven rows overstate.**

So 1.2a does not merely tidy the table — it is the **input** to 1.3. The
completion plan had them independent and listed 1.3 first; that ordering is wrong.

## Recomputed arithmetic on the v0734b table

| metric | as quoted through v0733g | recomputed at v0734c |
|---|---|---|
| rows with a real range | 69 | **94** |
| raw sum of `end - start` | — | **591,048 B** |
| merged union | 316,664 B | **317,828 B** |
| double-counted overlap | 267,628 B | **273,220 B** |
| merged span | 445,020 B | **449,692 B** (`0xb0..0x6dd4c`) |
| gap bytes | 128,356 B in 6 pieces | **131,864 B in 8 pieces** |
| largest gap | `0x4d2c0..0x640f4` — 93,748 B | **`0x4ec00..0x640f4` — 87,284 B** |
| rows > 20 KB | 7 | 7 |

The largest gap **moved and shrank**: `tile_controller_update` now has a real
extent ending at `0x4ec00` (it was a continuation row pointing at `0xcdc` and
was invisible), which split the old 93,748 B run into an 87,284 B gap plus a new
5,448 B gap at `0x4d2c0..0x4e808`. The full gap list is now:

```text
0x0004ec00..0x000640f4   87,284 B
0x000658a4..0x0006ca64   29,120 B
0x0006428c..0x000657dc    5,456 B
0x0004d2c0..0x0004e808    5,448 B
0x0006cb0c..0x0006dcb8    4,524 B
0x0006cad0..0x0006cae0       16 B
0x0006ca78..0x0006ca84       12 B
0x0000052c..0x00000530        4 B
```

**These are still lower bounds on the real unknown**, for the reason above.

## What this slice changed

No `functions.csv` row, no `src/`, no test. This is a measurement and a
correction of numbers that were being quoted from a contaminated table:

- the completion plan's measured-state table is recomputed, and 1.3 is reordered
  behind 1.2a with the reason;
- `executor_step_macro_measured_v0733g.md`'s reference to "the 93,748 B gap" is
  corrected.

## Still open, in dependency order

1. **1.2a** — decide the texture cluster's overlap, settle
   `camera_post_update_gate`'s and `frame_shadow_verify`'s identity, and settle
   `interrupt_return_wait_exit`'s attribution. Then repair. The two-source
   evidence for all 22 is already collected above.
2. **1.3** — recompute gaps *after* 1.2a. The numbers in this note are
   provisional for exactly one reason.
3. The four labelled lower bounds from v0734b, and the two here.
4. `coverage_ratio` is still `0.00` for every ranked row.