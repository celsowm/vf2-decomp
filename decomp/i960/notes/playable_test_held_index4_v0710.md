# TEST-held GAME ASSIGNMENT entry native (v0710)

## Result

The TEST-button frame on the GAME ASSIGNMENT cursor (selector `0x11`,
`a4=0x84`, `a5=0`, `a6=0xff`) now runs fully native under strict
per-block differential: 37 blocks, 5180/5180 instructions, MATCH through
the second scheduler to `0x1645c`. The native run renders the GAME
ASSIGNMENT submenu with live values (2/2, NORMAL, 160/200, 1500,
ON/ON/...) — TEST-button operation works natively.

## Recipe (ROM-backed, reproducible; snapshots in `out/`, not committed)

1. `vf2cycles --snapshot out/sixth-fresh --cycles 1` (MATCH baseline).
2. Five DOWN edge-taps (`--input 0x02` one cycle + one release cycle each)
   -> cursor `?.GAME ASSIGNMENT` (`out/walk5`, tile-rendered).
3. `vf2cycles --snapshot out/walk5 --cycles 1 --input 0x20000` -> MATCH.
4. `vf2cycles --snapshot out/test1 --cycles 1 --input 0x20000` -> MATCH
   (was: stop at `0x9ff8` ref vs `0xa6c0` native, unsupported).

## What was missing (three measured gaps, all in existing scaffolding)

1. **Gate**: `execute_frame_phase17_bit7_index4` required
   input/previous `== 0x0ff7f700`. With TEST held the oracle latches
   `0x0f000004` (execution-trace confirmed; the worker never consumes
   these values on the `a5=0/nav=0` path). Admitted exactly
   `(input,previous)==0x0f000004 && nav==0 && a5==0`; siblings refused.
2. **CC poststate**: `set_main_final_cluster_condition` forced EQUAL for
   phase `0x84`. The measured tail ends at `cmpibne 1,g0` (g0=0, taken)
   with GREATER, which the bridge already carries. Added a leave-intact
   exception for `(phase 0x84, a5 0, input TEST-held)`, mirroring the
   `0x8a` precedent.
3. **Register/stack poststate**: on this path the whole dispatch runs in
   sub-frames, so cluster `r14/r15` (locals) and `g1/g2/g6` (globals)
   keep entry values (`0x10`/`0x8a00`/`0x3f4f5c29`/`0xc0a0a3d7`/`0x55b6`,
   entry==exit in the oracle), and frame slot `0x5ff684` spills the
   live g1. The base-combo pins (`0x9f9c`/`0x8800`/`0x7ae10`/...) are
   restored-from-caller-frame / snapshotted-at-gate for the TEST-held
   combo only. (Also fixed: an out-of-bounds `local_frames[d]` read for
   globals during development — frames store 16 locals only.)

## Method notes (tooling traps hit)

- `vf2cycles` needs the `<snap>.runtime` sidecar (or `--state`); copies
  without it fail with `I/O error`.
- `vf2probe --read-u32` after `--max-steps 0` is unreliable (phantom
  +2M counter, stale bytes; cf. v0361). Trust execution traces and
  memory-trace records; verify `--read` against them. (One hex-arithmetic
  slip on my side compounded this; execution always agreed with itself.)
- `vf2probe --until` works with hex; probe runs need `--input` to match
  cycles' environment (here the trajectory was input-insensitive, but
  verify per slice).

## Still open (next)

- TEST-held follow frames (submenu navigation/edit with nav!=0, TEST
  release, EXIT path) — each a new measured combo.
- Base-combo behavior unchanged (102/102 ctest green, incl. phase17).
