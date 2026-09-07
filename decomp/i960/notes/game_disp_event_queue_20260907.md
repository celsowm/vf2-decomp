# `fa_game_disp` event ring queue — 2026-09-07

This note records the clean-room recovery of the event ring-buffer path inside
`fa_game_disp`, including non-empty queues, wraparound and composition with the
previously recovered timer branches.

## Boundaries and layout

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- queue head/write index: registry `+0x5b`
- queue storage: registry `+0x5c..+0x6b` (16 bytes)
- queue tail/read index: registry `+0x6c`
- timer 0 / timer 1: registry `+0x6d / +0x6e`
- event output port: `0x01c00008`

Both queue indices are bounded to `0..15`. Values outside that range remain
fail-closed in recovered C.

## ROM semantics

The final queue gate is:

```text
0x0002b154  cmpobe  r8, r9, 0x0002b170
0x0002b158  ldob    +0x5c(g13)[r9], r3
0x0002b160  addo    1, r9, r9
0x0002b164  and     15, r9, r9
0x0002b168  stob    r3, 0x01c00008
0x0002b170  stob    r8, +0x5b(g13)
0x0002b174  stob    r9, +0x6c(g13)
0x0002b178  st      r10, (g13)
```

Therefore `head == tail` means empty. Otherwise exactly one queued byte is
consumed per invocation, the tail advances modulo 16, the consumed slot is not
cleared, and the byte is emitted to `0x01c00008`.

The `cmpobe` condition is also architecturally observable at the isolated child
boundary, so the recovered child preserves LESS/EQUAL/GREATER and the matching
low arithmetic-control bits from the original head-versus-tail comparison.

## Measured matrix

ROM-backed fixtures were generated from the same exact event-entry snapshot and
only the queue indices/data were changed.

| Entry | Result | Child instructions |
| --- | --- | ---: |
| `head=7 tail=7` | empty, no dequeue | 38 |
| `head=1 tail=0 slot[0]=0x42` | output `0x42`, tail `1` | 42 |
| `head=2 tail=0 slots=0x11,0x22` | output `0x11`, tail `1` | 42 |
| `head=2 tail=1 slot[1]=0x7e` | output `0x7e`, tail `2` | 42 |
| `head=0 tail=15 slot[15]=0xa5` | output `0xa5`, tail wraps to `0` | 42 |
| `head=15 tail=14 slot[14]=0x33` | output `0x33`, tail `15` | 42 |
| `head=0 tail=1 slot[1]=0x66` | output `0x66`, tail `2` | 42 |

The queue path costs four instructions beyond the 38-instruction empty baseline.
A timer store costs one additional instruction, so queue + timer fixtures execute
43 child instructions. The overlapping 16-bit timer-store semantics documented
in `game_disp_event_timers_20260907.md` remain unchanged.

## Complete continuation

The same states were propagated through the full measured `fa_game_disp`
continuation:

| State | Full instructions | Calls | Returns |
| --- | ---: | ---: | ---: |
| empty queue, timers zero | 2,903 | 41 | 42 |
| timer only | 2,904 | 41 | 42 |
| dequeue only | 2,907 | 41 | 42 |
| dequeue + timer | 2,908 | 41 | 42 |

ROM oracles covered ordinary dequeue, a non-zero empty index pair, wrap
`15 -> 0`, and dequeue composed with both timer alternatives. The recovered
full continuation matched the reference snapshots exactly for the measured
cases.

## Remaining fail-closed frontier

The queue itself is no longer restricted to the original zero/zero state. The
remaining nearby boundaries are now:

- alternate event selector/state bytes;
- guarded `fa_game_disp` registry-flag branches;
- alternate final dispatch-table indices/targets;
- downstream score/event branches not exercised by the measured corridor.

The next gameplay-oriented target should be one guarded registry flag or one
selector/state branch, again promoted only after a ROM-backed differential
fixture establishes exact semantics and accounting.
