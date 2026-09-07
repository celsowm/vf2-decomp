# `fa_game_disp` registry flag 10 event enqueue — 2026-09-07

This note records the clean-room recovery of registry flag bit 10 inside the
measured `fa_game_disp` event/queue child and its propagation through the full
measured continuation.

## Boundaries

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- recovered flag: registry bit 10 (`0x00000400`)
- queue head/write index: registry `+0x5b`
- queue storage: registry `+0x5c..+0x6b`
- queue tail/read index: registry `+0x6c`

Admission remains deliberately narrow: the flag-10 path is accepted only for
the measured empty queue shape `head=0, tail=0`, with the baseline high registry
bit set. Other registry-flag combinations remain fail-closed.

## ROM semantics

The observed branch is:

```text
0x0002ac5c  bbc     10, r10, 0x0002ac74
0x0002ac60  clrbit  10, r10, r10
0x0002ac64  shlo    5, 5, r3
0x0002ac68  bal     0x0002b194
0x0002ac6c  shlo    3, 21, r3
0x0002ac70  bal     0x0002b194
```

The `0x0002b194` helper stores `r3` into the 16-byte event ring at the current
head, advances the head and masks it with 15 before returning through `g14`.
Consequently flag 10:

1. clears itself from the registry flags;
2. enqueues event byte `0xa0`;
3. enqueues event byte `0xa8`;
4. reaches the already recovered final queue gate;
5. immediately consumes `0xa0` when starting from the measured empty queue.

The measured post-state is therefore `head=2`, `tail=1`, slots 0/1 equal to
`0xa0/0xa8`, registry flags restored to the baseline value, and the output port
contains `0xa0`. The final `g14` link is `0x0002ac74`.

## Instruction accounting

The flag-specific enqueue sequence contributes 13 instructions over the ordinary
38-instruction child baseline. Its immediate queue dequeue contributes the
already measured 4-instruction queue delta. Thus:

| State | Event child | Full continuation | Calls | Returns |
| --- | ---: | ---: | ---: | ---: |
| flag 10, timers zero | 55 | 2,920 | 41 | 42 |
| flag 10 + timer store | 56 | 2,921 | 41 | 42 |

At the isolated child boundary the helper has no architectural procedure calls;
the two `bal` links use `g14`. The child itself performs its ordinary single
procedure return.

## Differential oracle

ROM-backed expected snapshots were produced from controlled entry snapshots and
compared against the exact `vf2i960-linux` GitHub Actions artifact built from
commit `f1f92c5ce18cd0321ce0fb29d2a71b1d416a4e74` (artifact
`10018628377`). All measured cases matched CPU state and mutable machine state
exactly:

- flag 10 child: 55 instructions — exact snapshot match;
- flag 10 + timer child: 56 instructions — exact snapshot match;
- flag 10 full continuation: 2,920 instructions, 41 calls, 42 returns — exact;
- flag 10 + timer full continuation: 2,921 instructions, 41 calls, 42 returns — exact.

A ring-buffer wrap fixture was rerun with the same artifact as a regression and
also remained exact.

## Remaining nearby frontier

The recovered event subsystem now covers the full valid 16-slot ring behavior,
the timer branches, and the measured flag-10 enqueue branch. Nearby fail-closed
frontiers include:

- other guarded registry flags;
- alternate event selector/state bytes;
- alternate final dispatch-table indices/targets; and
- downstream score/event branches not reached by the current oracle corridor.
