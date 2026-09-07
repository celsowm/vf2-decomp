# `fa_game_disp` conditional event flags 14/15 — 2026-09-07

This note records clean-room recovery of two additional conditional event
branches in the measured `fa_game_disp` event/queue child and their propagation
through the complete measured continuation.

## Boundaries

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- queue head/data/tail: registry `+0x5b`, `+0x5c..+0x6b`, `+0x6c`

Both recovered cases are intentionally tied to the exact auxiliary state seen
in the measured snapshot. The code does not generalize the alternate outcomes
of either conditional branch.

## Bit 14

Registry bit 14 (`0x00004000`) reaches the branch at `0x0002ae0c`.
The measured state is:

- byte `0x00500065 = 0`;
- byte `0x0050004c = 0`;
- empty event queue;
- both event timers zero.

For that state the ROM clears bit 14 and enqueues event byte `0x92` through the
common helper at `0x0002b194`. The helper link observed by the recovered path is
`0x0002ae48`.

The alternate `0x9a`/skip outcomes remain fail-closed because they depend on
other values of the two auxiliary bytes and have not yet been promoted from a
separate oracle.

## Bit 15

Registry bit 15 (`0x00008000`) reaches the branch at `0x0002ae48`.
The measured state is:

- `*(uint32_t *)0x0050083c = 0x00515c00`;
- `*(uint32_t *)0x00515c00 = 0x00000008`;
- therefore bit 3 of the pointed word is set;
- empty event queue;
- both event timers zero.

The ROM clears bit 15 and enqueues `0x9b`; if that pointed bit were clear it
would instead enqueue `0x93`. The measured recovered path preserves the link
value `0x0002ae6c`. The unmeasured `0x93` outcome remains fail-closed.

## Instruction accounting

Both measured branches add 11 instructions before the common 4-instruction
queue dequeue. Relative to the 38-instruction zero-flag child:

| Case | Event child | Full continuation |
| --- | ---: | ---: |
| bit 14 measured state | 53 | 2,918 |
| bit 15 measured state | 53 | 2,918 |

The complete continuation remains at 41 architectural procedure calls and 42
returns.

## Differential oracle

The uploaded supported VF2 ROM was executed by the i960 interpreter from the
measured event and full-continuation snapshots after changing only the target
registry bit. The exact `vf2i960-linux` artifact `10020632530`, produced by CI
from commit `4c862bf5bb9ca3387f80951e8c17257c7223f77a`, was then run natively from
the same entry snapshots.

All four new comparisons were exact:

| Case | Native instructions | CPU + memory snapshot |
| --- | ---: | --- |
| bit 14 child | 53 | exact match |
| bit 15 child | 53 | exact match |
| bit 14 full continuation | 2,918 | exact match |
| bit 15 full continuation | 2,918 | exact match |

With the same artifact, representative regressions also remained exact for:

- bit 19 sequence branch: 104 child instructions;
- bit 10 event-pair branch: 55 child instructions;
- ring-queue wrap `tail=15 -> 0`: 42 child instructions.

CI run `34127352885` passed GCC release, Clang release, Clang ASan/UBSan, and
Python tooling syntax before the artifact was used for this differential proof.

## Remaining fail-closed frontier

This slice deliberately does not admit:

- the alternate bit-14 `0x9a` or no-enqueue outcomes;
- the alternate bit-15 `0x93` outcome;
- combinations among the recovered registry flags;
- non-zero timers with bits 14, 15, 16, 17, or 19;
- larger neighboring branches such as bits 8, 9, 11-13, and 18;
- alternate selector/state bytes or final dispatch-table targets.

Bit 13 is now the nearest substantial event branch, but it formats multiple
queue bytes from additional game state and should get its own measured oracle
rather than being folded into this conditional slice.
