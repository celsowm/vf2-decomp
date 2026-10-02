# Player-corridor geometry call attribution (v0703, scouting)

Measured from `pre14288` (F0/F1 `+0x1a4 = 0x59f`) -> `0x10dcc`
(14634 steps, 54 calls, 30 targets; trace consumed for analysis only,
not committed). No native recovery is claimed; this is exact
call-target attribution for the frontier item "geometry helpers after
0x28780".

## Geometry chain (single pass, in order)

- `0x27d00 -> 0x28184` (step 14341598)
- `0x28268 -> 0x28780` (step 14341610): the frontier helper, called
  exactly once from `0x28268`, immediately after the `0x28184` region.
- `0x27d90 / 0x27dcc / 0x27fa0 -> 0x2901c` (3x, steps 14342693/805/976)
- `0x28174 -> 0x29414` (step 14343107): non-zero path entry, after the
  `0x2901c` cluster.

Preceding context: `0x143e4 -> 0x1b5c8`, `0x143f8 -> 0x1b568`,
`0x1abf4 -> 0x27ce0` (steps 14341567/582/593) feed the `0x27xxx`
region; `0x270d4` and `0x27130` (the corridor tail calls) bracket it.

## Repeating helpers

- `0x27b5c` x5 from consecutive siblings `0x270e8 / 0x270f8 /
  0x27108 / 0x27118 / 0x27128` (steps 14332186..14339601, ~1900 steps
  apart): a 5-station sequence, not a loop -- each station ~1900 steps.
- `0x176a0` x16 from `0x16688`..`0x175f0` (game-info expansion sweep,
  steps 14343547..14345114): 16 records processed through one helper.
- `0x4bxxx` cluster x7 (`0x4b5d0`, `0x4b604` x2, `0x4b640`,
  `0x4b7b0`, `0x4b7e8`, `0x4b838`, `0x4b9b8` x2): texture-side calls
  inside the corridor window.

## What recovery needs next

Native `0x28780` needs its input contract: dump the register/frame
state at the `0x28268 -> 0x28780` call step (caller frame + `+0x1a4`
family) across the 49 mask rows, then prove a minimal C shape
differentially. The single-call shape makes this a bounded sweep, not
an open exploration.
