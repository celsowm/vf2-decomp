# v0731 F2: rows 2-4 post-edit release frames — measured, still fail-closed

Frontier item **F2** from `v0730_first_action_runbook.md`: "rows 2-4
post-edit release frames (post-edit states with credit index != 2, derived
credits, preset != 0)". The bodies are now **measured**; they are **not
recovered**. Nothing is admitted.

## What the post-edit release frame is

The frame *after* a value-row edit. The edit frame renders the pre-edit
values; the deferred value update lands on the following frame. Latch shape
(admitted for row 1 as `{input parked, previous 0x0f000004, released 4,
nav 0}`):

```text
0x00500700 = 0x0f000000   input parked
0x00500704 = 0x0          nav none
0x00500708 = 0x4          TEST released
0x0050070c = 0x0f000004   previous = the TEST press
```

## Reproducing it (6 probe steps per row)

`base` at the `0xa6c0` boundary is **`0x599000`**, not the `0x59a3d0`
visible in the walk snapshots. Read `0x50016c` at the boundary you are
actually using; patching the wrong `base` silently measures nothing (this
already cost one false result on the a5 = 5 leg).

```sh
# 1. from an idle state at the row, cross the frame IRQ to the cluster entry
vf2probe --snapshot <idle>.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x00009ff8 --output-snapshot f2-<tag>-cl.vf2snap
# 2. patch the TEST latch AT 0x9ff8 (re-latched every frame otherwise)
vf2probe --snapshot f2-<tag>-cl.vf2snap --max-steps 1 --until 0x00009ff8 \
         --set-u32 0x00500700=0x0f000004 --set-u32 0x00500704=0x4 \
         --set-u32 0x00500708=0x0 --set-u32 0x0050070c=0x0f000000 \
         --output-snapshot f2-<tag>-edit.vf2snap
# 3. the edit frame, then onward to the next wait
vf2probe --snapshot f2-<tag>-edit.vf2snap --until 0x0000a010 \
         --output-snapshot f2-<tag>-editend.vf2snap
vf2probe --snapshot f2-<tag>-editend.vf2snap --until 0x00010fa0 \
         --output-snapshot f2-<tag>-wait2.vf2snap
# 4. cross the IRQ again, then patch the RELEASE latch
vf2probe --snapshot f2-<tag>-wait2.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x00009ff8 --output-snapshot f2-<tag>-relcl.vf2snap
vf2probe --snapshot f2-<tag>-relcl.vf2snap --max-steps 1 --until 0x00009ff8 \
         --set-u32 0x00500700=0x0f000000 --set-u32 0x00500704=0x0 \
         --set-u32 0x00500708=0x4 --set-u32 0x0050070c=0x0f000004 \
         --output-snapshot f2-<tag>-rel.vf2snap
# 5. the reference body
vf2probe --snapshot f2-<tag>-rel.vf2snap --until 0x0000a010 \
         --output-snapshot f2-<tag>-ref.vf2snap
```

Starting idles that exist: `out/f1-a2-idle` is at **a5 = 3** and
`out/f1-a3-idle` is at **a5 = 4** (the `f1-aN-*` names are off by one against
`a5`; decode `0x5000a5` with explicit byte shifts, not a raw hex eyeball).
There is **no idle at a5 = 2** — see below.

## Measured results

All from the `0x9ff8` entry to the `0xa010` boundary. The idle render for
contrast is 4420 (232 prefix + 4188 body, 35 calls).

| row (`a5`) | screen label | edit frame | release chain | release body | credits after | preset after |
|---|---|---|---|---|---|---|
| 3 | `COIN/CREDIT SETTING     #  1` | 4637 | **4421** / 41 calls | **4189** | **`[2,2,2,3,3,1]`** | 0 |
| 4 | the `#  1` selector | 4635 | **4418** | **4186** | `[2,2,2,2,2,2]` | **1** |
| 2 | `CREDIT TO 1P START` | — | — | — | — | — |

Both measured rows are **fail-closed on the native side**:
`unsupported operation at 0x0000a6c0 ... entry=0x00009ff8`.

## The derived-credits transform: sampled, and the obvious rule is FALSIFIED

Pushing TEST repeatedly at `a5 = 3` from a real idle walks the six-byte
vector at `base + 0x3329` (indices 0-2 never move; index 3 is the edited
counter; indices 4 and 5 are the derived pair). Nine samples, all from the
`0xa010` end of the edit frame:

| edit | new `credits[3]` (= n) | `credits[4]` | `credits[5]` | edit frame insns |
|---|---|---|---|---|
| 0 | 3 | 3 | 1 | 4637 |
| 1 | 4 | 3 | 2 | 4636 |
| 2 | 5 | 3 | 3 | 4637 |
| 3 | 6 | 4 | 1 | 4637 |
| 4 | 7 | 4 | 2 | 4636 |
| 5 | 8 | 4 | 3 | 4637 |
| 6 | 9 | **4** | **4** | 4637 |
| 7 | 10 | 5 | 1 | 4637 |

(`n = 2` is the untouched default `[2,2,2,2,2,2]`; it is not part of the
sequence, since it is the starting state rather than a result.)

**KICK (nav `0x200`) is the exact inverse**: from n = 9 it produced
`[2,2,2,8,4,3]`, byte-identical to the n = 8 `+1` state. So the `-1` and
`+1` directions agree, which rules out a one-way accumulator bug.

### RESOLVED by static analysis: it is a table lookup, not arithmetic

The falsified formula was a symptom. A `--memory-trace` over one edit
frame (`out/f2-r3-n10.vf2snap` -> `0xa010`) plus a disassembly gives the
whole mechanism.

**The counter** (`0x5b990`):

```asm
0005b990  ld       0x0050016c, r15      ; base
0005b998  ldob     0x0000332c(r15), r15 ; n = credits[3]
0005b9a4  addi     r14, r15, r15        ; r14 = n + delta
0005b9ac  cmpible  r14, r15, 0x5b9b4
0005b9b0  mov      14, r15              ; clamp high to 14
0005b9b4  mov      14, r14
0005b9b8  cmpibge  r14, r15, 0x5b9c0
0005b9bc  mov      0, r15               ; clamp low to 0
0005b9c8  stob     r15, 0x0059c32c      ; credits[3] = clamped n
0005b9d0  cmpi     0, g0
0005b9d4  be       0x5bb48              ; direction 0 -> skip
0005b9d8  call     0x5bb90              ; else recompute the pair
```

**The derived pair** (`0x5bb90`) - both bytes are ROM table lookups
indexed by `credits[3]`:

```asm
0005bb90  ld       0x0050016c, r4
0005bb98  ldob     0x0000332c(r4), r4    ; r4 = credits[3]
0005bba0  ldob     0x0005bc74[r4], r5    ; credits[4] = table1[n]
0005bba8  ldob     0x0005bc84[r4], r6    ; credits[5] = table2[n]
0005bbb8  stob     r5, 0x0059c32d
0005bbc8  stob     r6, 0x0059c32e
```

The two tables, read from memory at `0x5bc74` and `0x5bc84`, index =
`credits[3]`:

```text
idx   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
[4]   1  2  2  3  3  3  4  4  4  4  5  5  5  5  5  0
[5]   1  1  2  1  2  3  1  2  3  4  1  2  3  4  5  0
```

Both reproduce all eight sampled states exactly (`[4]` = 3,3,3,4,4,4,4,5
and `[5]` = 1,2,3,1,2,3,4,1 for n = 3..10). **This is why the arithmetic
failed: the two tables have different periods.** `[4]` holds four consecutive
4s at n = 6..9, while `[5]` cycles 1,2,3 with a 4 only at n = 9. Index 15
is `0` in both and unreachable, since the counter clamps at 14.

**`preset` is also a clamped counter**, not a table. The write is at
`0x5b798` (found by trace, *not* at `0x5bbd4`):

```asm
0005b770  mov      g0, r14
0005b774  addi     r14, r15, r15        ; r14 = preset + delta
0005b77c  cmpible  r14, r15, 0x5b784
0005b780  mov      25, r15              ; clamp high to 25
0005b788  cmpibge  r14, r15, 0x5b790
0005b78c  mov      0, r15               ; clamp low to 0
0005b798  stob     r15, 0x0059c324      ; preset = clamped value
```

So `preset` is a 0..25 counter - which is why one edit took it 0 -> 1.

There **is** a routine at `0x5bbd4` that does `preset = mem8[0x61500 + preset]`,
but it did not run on this frame and `mem8[0x61500]` is 0. Do not attribute
row 4's behaviour to it; the trace points at `0x5b798`.

**Recovered model, in full:**

```text
credits[3] += delta, clamped to [0, 14]
credits[4]  = mem8[0x5bc74 + credits[3]]
credits[5]  = mem8[0x5bc84 + credits[3]]
preset     += delta, clamped to [0, 25]
```

with `delta = +1` for a TEST edge (`nav 0x4`) and `-1` for KICK
(`nav 0x200`), and the derived pair recomputed only when `delta != 0`.

### The falsified prediction (kept as the cautionary tale)

`credits[5]` looks like a period-3 cycle over 1,2,3 with `credits[4]`
incrementing on each wrap:

```text
credits[4] = 3 + floor((n - 3) / 3)
credits[5] = 1 + ((n - 3) mod 3)
```

That formula fits n = 3..8 **exactly**, and predicts n = 9 as
`[2,2,2,9,5,1]`.

**Measured n = 9 is `[2,2,2,9,4,4]`.** The prediction is wrong, and the
measured value is reproducible from two independent chains (the straight
walk and the KICK-then-replay). The observed sequence

```text
(3,1) (3,2) (3,3) (4,1) (4,2) (4,3) (4,4) (5,1)
```

skips `(3,4)` entirely while allowing `(4,4)`, so it is **not** a plain
base-4 or base-3 counter either.

**Do not encode either formula.** A rule that survives eight samples and is
then falsified on the ninth is the trap the runbook warns about; this is a
live example - and the static analysis above shows why: it is a table
lookup, so no single arithmetic expression can fit it.

The edit frame is stable at 4636/4637 instructions across all nine samples
(`credits[4]` = 3 and 7 give 4636, the rest 4637), so the *body* is nearly
row-independent even though the *state* transform is not.

## The value-driven digit render: rows 8 and 9 mapped, rows 6 and 7 not

`runs[]` hardcodes `{6,40,"2"}`, `{7,40,"2"}`, `{8,40,"2"}` and
`{9,40,"2"}`. Tracing three edit frames with `--memory-trace` shows which
of them actually track live state. The render happens **before** the
`credits[3]` write in the same frame, so each trace shows the *pre-update*
vector:

| pre-update `credits` | (6,40) | (7,40) | (8,40) | (9,40) |
|---|---|---|---|---|
| `[2,2,2,5,3,3]` | `'2'` | `'2'` | **`'3'`** | **`'3'`** |
| `[2,2,2,8,4,3]` | `'2'` | `'2'` | **`'4'`** | **`'3'`** |
| `[2,2,2,9,4,4]` | `'2'` | `'2'` | **`'4'`** | **`'4'`** |

- **screen (8,40) tracks `credits[4]`** - `'3'` when `credits[4] = 3`,
  `'4'` when it is 4. Two distinct values observed.
- **screen (9,40) tracks `credits[5]`** - `'3'` and `'4'`. Two distinct
  values observed.
- **screen (6,40) and (7,40) stayed `'2'` in every sample.** Indices 0 and
  1 never moved in any of the nine edit states, so
  "`<- credits[0]` / `<- credits[1]`" is *consistent but unproven* - a
  constant `'2'` fits the same data. Do not encode them until a state with
  `credits[0] != 2` or `credits[1] != 2` is measured; that needs a proper
  `a5 = 2` boundary (see the row-2 section).

Each cell is written with one 16-bit `stos` of `0x8000 | ASCII(digit)`,
i.e. the same attribute form as the text runs, at four sites 100 bytes
apart: `0x5bd04`, `0x5bd68`, `0x5bdcc`, `0x5be30`.

```asm
0005bd04  stos     r15, (g9)
0005bd08  addo     4, g9, g9
0005bd0c  be       0x0005bd24
0005bd10  balx     0x00009444, r14
```

The digit-to-glyph conversion is in the `0x9444` helper, which this note
has not disassembled - the value mapping above is measured, not inferred
from that code.

## The two distinct deferred behaviours

This is the part the runbook compressed into "credit index != 2, derived
credits, preset != 0", now separated:

- **Row 3 is a derived-credit update.** `[2,2,2,2,2,2]` becomes
  `[2,2,2,3,3,1]` on the first edit. Mechanism fully resolved above: a
  clamped counter plus two ROM byte tables.
- **Row 4 is a `preset` update.** Credits are untouched; `preset`
  (`base + 0x3324`) becomes `1`. It is a clamped 0..25 counter at
  `0x5b770`. That is the `preset != 0` gate condition, and it is why the
  existing `preset != 0u` refusal fires here.

The bodies are also **per-row**, not one shared body: 4189 at row 3 versus
4186 at row 4, against 4188 for the idle render. Do not fold these into a
single counted rule without more samples.

## Correction: the value write lands in the EDIT frame, not the release frame

An earlier version of this note said the deferred update "lands on the
release frame". Tracing all six chain snapshots shows otherwise:

| snapshot | row 3 credits | row 4 preset |
|---|---|---|
| `-edit` (0x9ff8 entry, TEST latch) | `[2,2,2,2,2,2]` | 0 |
| `-editend` (0xa010, end of the **edit** frame) | `[2,2,2,3,3,1]` | **1** |
| `-wait2`, `-relcl` | `[2,2,2,3,3,1]` | 1 |
| `-rel` (0x9ff8 entry, release latch) | `[2,2,2,3,3,1]` | 1 |
| `-ref` (0xa010) | `[2,2,2,3,3,1]` | 1 |

**The write happens inside the edit frame.** By the time the release frame
starts, the values are already updated; the release frame only *renders*
them. "Post-edit release frame" means "the frame after an edit", not "the
frame that performs the edit".

## The exact blockers (measured, not inferred)

Admitting the row-3 / row-4 release latch tuples
(`{input parked, previous 0x0f000004, released 4, nav 0}` at `a5 = 3` and
`4`) does **not** make the legs run. The refusal survives the tuple, so it
is a gate condition, and the two rows fail on two different ones:

- **Row 3** fails `for (index...) if (credits[index] != 2) return
  VF2_ERROR_UNSUPPORTED;` because at the release entry
  `credits = [2,2,2,3,3,1]`.
- **Row 4** fails the main gate's `preset != 0u` because at the release
  entry `preset = 1`.

That is the whole of the runbook's "credit index != 2 ... preset != 0":
the existing fail-closed checks are already sitting on the F2 frontier.
Confirming it took admitting the tuples and reading the refusal - the
probe tuples were then reverted, since they admit nothing.

Everything else at the release entry is gate-clean: `indirect_target =
0x5b558`, `selector_mask = 0x20000`, `coin_flags = 0`, `a5` correct,
`a6 = a7 = 0xff`, and `phase_index = 0x85` at `0x5000a4` (so the dispatch
routes to the same index-5 body as the a5 = 5 leg).

## Row 2 is NOT measured

The only a5 = 2 artifact is `out/f1-test-row2`, and its `ip` is `0x10d54` —
past the frame wait, so `--raise-irq --until 0x9ff8` never reached the
cluster entry and both of the first two steps burned the 4,000,000 step cap
(`halt_reason: maximum steps`). The reference run that followed reported
4422 instructions, but from a state that was not the intended boundary.

**Do not use the 4422 figure.** To measure row 2 properly, reach it from a
real idle: `out/f1-a2-idle` (a5 = 3) with an UP tap
(`{input 0x0f002000, previous 0x0f000000, released 0, nav 0x2000,
a5 3}` — already admitted) lands on a5 = 2, then run the six steps above.

## What a recovery still needs

1. Widen the **two gates**, each with a measured rule rather than a
   blanket relaxation: the `credits[index] != 2` refusal must admit the
   *derived* vectors only, and `preset != 0u` must admit `preset = 1` only
   where measured. Neither may become a general "any value" gate.
2. The **value-driven digit render**, partially mapped. Rows (8,40) and
   (9,40) are proven to track `credits[4]` and `credits[5]`; rows (6,40)
   and (7,40) are unproven because indices 0 and 1 never moved in any
   sampled state. `runs[]` must stop hardcoding `"2"` for all four, but
   only two of the four mappings may be written down yet.
3. The **derived-credits transform** is now **fully resolved** - clamped
   counter + `mem8[0x5bc74 + n]` / `mem8[0x5bc84 + n]`. Encode the tables,
   not a formula. Still needs its own differential proof before admission.
4. The **`preset` update** for row 4, including what a second edit does to a
   nonzero `preset`.
5. Per-row bodies at 4189 / 4186 (and row 2's, once measured), each proven
   against its own chained differential.

Note the ordering: items 1 and 2 are what make the leg *reachable*; 3-5 are
what make it *correct*. Widening the gates without the render would just
move the failure from "unsupported" to a poststate mismatch.

Until then these legs stay fail-closed. Nothing in this note admits a tuple.
