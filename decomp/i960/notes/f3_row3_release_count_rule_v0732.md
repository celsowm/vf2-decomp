# v0732 F3: the row-3 release body is a COUNT of singular labels

Frontier item **F3** ("PUNCH/KICK releases at value rows") is now recovered
**for the row-3 (1P CONTINUE) value row**. The 4421/4422 split that the first
pass could only fit is a counted rule with a mechanism, and the falsified
`credits[3] == 3` hypothesis is retracted below.

This supersedes the "still fail-closed" conclusion in
`f3_punch_kick_release_measured_v0732.md`; the measurement tables there are
still correct, only the conclusion moved.

## The rule

```text
body = 4190 - (number of rendered credit values that equal 1)
```

The four rendered cells are the ones the renderer actually writes:

```text
(6,40) <- credits[0]   (7,40) <- credits[2]
(8,40) <- credits[4]   (9,40) <- credits[5]
```

`a5 == 4` keeps its own measured 4186 and is explicitly **not** covered by the
count - only one `a5 = 4` row is measured and it has no singular value, so the
effect there is unknown rather than absent.

## A hypothesis was falsified on the way

The first reading of the six existing release artifacts was:

> `credits[3] = 3` gives 4421; `credits[3]` in {7, 8, 9} gives 4422.

That correlation is a coincidence. `credits[3] = 3` happens to be the only
row where `credits[5] = 1`, because `t2 = {1,1,2,1,2,3,1,2,3,4,...}` and
`t2[3] = 1`. Completing the domain settled it - `credits[3] = 6` also has
`t2[6] = 1`, and it gives **4421**, not 4422.

The domain was completed by finishing the natural edit sequence rather than by
patching state: `f2-r3-e2end` / `f2-r3-e4end` are real edit-frame ends at
`credits[3] = 4` and `6`, so settle to `0x10fa0`, cross the frame IRQ
(`--raise-irq 0x1 --enter-interrupt 12=1 --until 0x9ff8`), and patch the
release latch at that boundary. Both produce naturally-reachable states.

## The two-singular case

A yes/no test cannot distinguish "any singular costs one" from "each costs
one". A controlled probe at `credits[3] = 0` - where `t1[0] = 1` **and**
`t2[0] = 1`, so two of the four rendered values are 1 - gives **4420**. That
is the count, not a yes/no. The probe patches `credits[3..5]`; it is a
hypothesis test, not a recovery claim, and `credits[3] = 0` is reachable
naturally (clamped counter, KICK from 1) even though no artifact captures it.

## Full measured domain

| `credits[3]` | `credits[4]` | `credits[5]` | singulars | reference | result |
|---|---|---|---|---|---|
| 2 | 2 | 2 | 0 | 4422 | MATCH |
| 3 | 3 | 1 | 1 | 4421 | MATCH |
| 4 | 3 | 2 | 0 | 4422 | MATCH |
| 6 | 4 | 1 | 1 | 4421 | MATCH |
| 7 | 4 | 2 | 0 | 4422 | MATCH |
| 8 | 4 | 3 | 0 | 4422 | MATCH |
| 9 | 4 | 4 | 0 | 4422 | MATCH |
| 0 (probe) | 1 | 1 | 2 | 4420 | count only |

Ten rows now print `Snapshots match.` on registers, memory regions, instruction
count and call count: the original `f2-r2` / `f2-r3` / `f2-r4` plus
`f2-r3-cl`, `f2-r3-n8-c`, `f2-r3-n9-c`, `f2-r3-n10-c`, `f2-r3-n11-c`,
`f2-r3-k1-c`, `f2-r3-c3a-e2` and `f2-r3-c3a-e4`. The table is the parked
`credits[3] = 2` default plus the whole natural range; no residual.

## Why one instruction per singular

The singular label is not a substitution inside the emitter. `write_text` at
`0x007fc0` is a plain `0x8000 | char` writer with no conditional at all:

```text
0x007fcc  ldib (r3), r6
0x007fd4  cmpi r6, 0
0x007fd8  be 0x007fec            ; terminate on NUL
0x007fdc  or r5, r6, r6          ; r5 = 0x8000
0x007fe0  stis r6, (r4)
```

So the trailing `'S'` becomes a space because a **different string** is
passed. The choice is made by a computed dispatch: `cmpobl` at `0x009478`
compares the row's label against known strings and `bx` at `0x00947c` jumps to
a handler. The plural and singular handlers are separate, and the singular one
is one instruction shorter. Traced on `f2-r3-relcl`, the `(9,48)` blank is
written by `0x007fe0 stis` while the four digits come from `stos` at
`0x5bd04` / `0x5bd68` / `0x5bdcc` / `0x5be30` - two different emitters.

This also disposes of the digit-stamper `cmpibne` sites. `sub_0005bd04` is:

```text
0x0005bd04  stos  r15, (g9)       ; write the digit
0x0005bd08  addo  4, g9, g9
0x0005bd0c  be    0x0005bd24      ; skip the glyph helper
0x0005bd10  balx  0x00009444      ; else emit one glyph
```

`r15` is `ldob 0x0000332a(g4)` - the credit byte itself, OR'd with `0x8030`
at `0x5bcfc`. There is no comparison against 1 in the digit path; the
`cmpo 1` belongs to a neighbouring block and does not gate this write.

## Still open in F3

- **Row 2 (CREDIT TO 1P START) and row 4 KICK releases.** Only the PUNCH
  shapes are measured. Row 2 shares the 4190 base, so the count rule should
  carry over, but that is untested and must not be assumed.
- **KICK -1 from INDIVIDUAL mode.** Not started. The `a5 = 4` post-edit
  release refuses the INDIVIDUAL variant explicitly, so this is a clean
  fail-closed starting point.
- **The `a5 = 4` singular case.** One measured row, zero singulars. The
  count is deliberately not applied there.
- **The edit path's own variation.** `f2-r3-e2` is 4636 and `f2-r3-k1` is
  4634 while the code carries a fixed `edit_delta > 0 ? 4405 : 4402`. Every
  one of those states is refused today, so nothing is silently wrong, but
  the edit path is only proven at the rows the CTest cases cover.
