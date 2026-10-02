# Playable push: test-menu navigation + TEST-button frontier (v0709)

## What runs today (all strict-MATCH, видимые via tile render)

- Boot session (`vf2.exe --native-snapshot out/boot.vf2snap`) runs
  300/300 frames natively through POST (copro-init + delay depth
  shapes, v0708) into the mode-0 TEST MENU idle
  (`0x10fa4`/`0x0bc0` poll loop, fighters allocated, no credit path).
- Menu navigation from `out/sixth-fresh.vf2snap` via `vf2cycles`:
  DOWN edge-taps move exactly one item per tap (EXIT -> MEMORY ->
  INPUT -> SOUND -> DISPLAY -> GAME ASSIGNMENT -> COIN ASSIGNMENT
  -> BOOKKEEPING -> BACKUP RAM CLEAR, wrap-around); PUNCH on EXIT
  reaches EXIT TEST MODE (12c MATCH, 57,785 insns, v0355 trajectory
  reproduced). Tile text rendered host-side per checkpoint.
- GAME ASSIGNMENT reached (cursor `?.GAME ASSIGNMENT`); START is a
  no-op there; PUNCH diverges wide (ref `0x9ff8` vs native `0xa6c0`).

## Gate-count fix (this slice, proven)

`execute_frame_geometry_gate` TEST-taken shape (`0xA748` bit26-clear
+ bit2-set, TEST button = game input bit2): both BBS instructions
execute, so the `frame_state == 17 && alt != 0` path costs **8**,
not the corridor-measured 7 (bit26-set skips the second BBS).
CC unchanged (trailing CMPobne decides). The old count desynced
the differential by exactly one instruction (ref `0xa028` vs native
`0xa030`). Fixed via the already-read flags word; corridor shapes
keep 7. Proof: 1-cycle TEST repro goes from mismatch to the next
frontier; texture/bridge/native suites green.

## TEST-button frontier (measured, open)

TEST on GAME ASSIGNMENT enters the submenu in the oracle (storyboard
from the failure snapshot: 2000 steps = item list, 5000 steps =
values MATCH COUNT 2/2, NORMAL, 160/200, 1500, ON/ON/OK, then
`0x10f98` idle). Natively it stops at the `0x9ff8`
final-cluster with frame selector `0x11` (only selector 0 and 2
variants exist). The cluster chain is fixed calls
(`0x530`, `0x29744`, `0x110b0`, `0x2f5c`, `0xa154`, `0xa6c0`
table-dispatch by `0x50002a`, `callx` to `0x10b5c` -> `0x58fe0` ->
`0x5a6b4` -> `0x61260` -> `0x7fc0` xN) into ~16k steps of submenu
code; most callees have no native block yet. Next slice: recover
the `0x11` cluster piece by piece from the captured failure state.

## Coin/credit (measured negative)

Held coin latches input bits (`0x500700`/`0x700c`, `0x500704`
bit0) and a mid-idle coin edge shows the same latch-then-clear
with no credit word and no mode change across 12/30-frame
snapshot diffs (10-12 bytes move: frame ticks + latch only).
With factory-default backup (v0356), the oracle never leaves the
operator menu: no attract, no SEGA tile.

## Prime next hypothesis (not yet tested)

Boot lands in TEST MENU possibly because backup-SRAM config
checksums are not valid (classic arcade behavior). The schemas
are documented (15-byte coin block -> `0x1d03300`, 29-byte game
block at `+0x3340` -> `0x1d03302`, `0x5ff54` algorithm). Find the
boot branch on backup validity, write valid checksums, reboot,
watch for attract. The TEST switch itself reads OFF correctly
(active-low `0x04` at `0x01c00010`, v0137).
