# v0732 F3: PUNCH/KICK releases at value rows - measured, still fail-closed

Frontier item **F3** from `v0730_first_action_runbook.md`: "PUNCH/KICK
releases at value rows + KICK -1 from INDIVIDUAL mode". **Evidence only.**
No tuple admitted, no behaviour change. This note records what was measured
after F2 landed and what still blocks recovery.

## The row-3 release count is a function of the credit vector

With the F2 release path recovered, the row-3 post-edit release frames can be
run through the full differential. Six of them exist in `out/`:

| snapshot | `credits[3]` | `credits[4]` | `credits[5]` | reference | native | registers | memory |
|---|---|---|---|---|---|---|---|
| `f2-r3-relcl` | 3 | 3 | 1 | 4421 / 41 | 4421 / 41 | MATCH | MATCH |
| `f2-r3-n8-c` | 7 | 4 | 2 | 4422 / 41 | 4421 / 41 | MATCH | MATCH |
| `f2-r3-n9-c` | 8 | 4 | 3 | 4422 / 41 | 4421 / 41 | MATCH | MATCH |
| `f2-r3-n10-c` | 8 | 4 | 3 | 4422 / 41 | 4421 / 41 | MATCH | MATCH |
| `f2-r3-n11-c` | 9 | 4 | 4 | 4422 / 41 | 4421 / 41 | MATCH | MATCH |
| `f2-r3-k1-c` | 9 | 4 | 4 | 4422 / 41 | 4421 / 41 | MATCH | MATCH |

`credits[4]` / `credits[5]` are `t1[credits[3]]` / `t2[credits[3]]`, so the
five failing rows are all one instruction long. Registers and memory already
match on **every** row - the recovered render, the credit derivation and the
cell encoding are right. The only open item is the body length.

`credits[3] = 3` gives 4421; `credits[3]` in {7, 8, 9} gives 4422. The
interval 4-6 is **unmeasured**, and `credits[5] == 1` versus `credits[5] >= 2`
is the obvious candidate but is not separated by this data: `credits[3] = 3`
is the only row with `credits[5] == 1` *and* the only row with
`credits[4] == 3`. Do not encode either correlation until a row separates
them.

## The edit path has the same problem, one level down

The same variation exists on the edit frames, which is where the states came
from:

| snapshot | `credits[3]` | `credits[5]` | reference | admitted? |
|---|---|---|---|---|
| `f2-r3-edit` | 3 | 1 | 4637 | yes, MATCH |
| `f2-r3-e2` | 4 | 2 | 4636 | **refused** |
| `f2-r3-e3` | 5 | 3 | 4637 | refused |
| `f2-r3-e4` | 6 | 1 | 4637 | refused |
| `f2-r3-e5` | 7 | 2 | 4636 | refused |
| `f2-r3-n8` | 8 | 3 | 4637 | refused |
| `f2-r3-n9` | 9 | 4 | 4637 | refused |
| `f2-r3-n11` | 10 | 1 | 4637 | refused |
| `f2-r3-k1` | 8 | 3 | 4634 (**nav 0x200**, KICK) | **refused** |

The current code carries a fixed `edit_delta > 0 ? 4405 : 4402` for
`a5 == 2 || a5 == 3`, which is 4637 for a +1 edit. That is correct for
`credits[5] != 2` and wrong by one for `credits[5] == 2`. **This is not a
fail-open bug today**: every one of those rows is refused, so nothing is
silently accepted with a wrong count. But it does mean the edit path is only
proven at the two `credits[3] == 3` rows that the existing CTest cases cover.

KICK from row 3 is a third distinct length (4634), and it is refused.

## Why this is not a one-line fix

Pinning 4422 for `credits[3] != 3` would make the six measured rows pass, but
it is a fitted rule over a domain sampled at {3, 7, 8, 9} with a hole at
{4, 5, 6}. AGENTS.md rule 1 and rule 9 both forbid that. The right move is
the same one that resolved the credit tables: disassemble the render routine
that emits the digit cells and read the actual branch, rather than fitting
more samples.

The candidate is the same stamper already identified for the `a5 = 4` chute
section, `sub_00060d30` / its bank-0 twin at `0x060c5c`-`0x060cc0`, whose
`cmpibne 1, r9` sites are the singular-form blanks. A second, different
routine is also in play for rows 6-9: the digit `stos` sites at `0x5bd04` /
`0x5bd68` / `0x5bdcc` / `0x5be30` and the glyph helper `balx 0x9444`, none
of which have been disassembled yet.

## KICK -1 from INDIVIDUAL mode

Not started. No INDIVIDUAL-mode row-3 artifact with a KICK edit exists in
`out/`. The `a5 = 4` post-edit release now **refuses** the INDIVIDUAL variant
explicitly (it is gated to COMMON), so this is a clean fail-closed starting
point rather than a silent hole.

## State check

Everything above is read-only. No snapshot, gate, count or render was changed
while gathering it; the F2 slice is the only behavioural change in flight.
