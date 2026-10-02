# `fa_player` corridor frontier scouting (v0696)

No C change in this slice. Method: `vf2probe --trace --memory-trace`
from `out/pre14288.vf2snap` to `0x00010dcc` (14,634 steps, 5,004
memory accesses), ranked with `tools/python/frontier.py`
(`--fighter-base 0x00510980/0x00512980`, `--exclude-recovered`,
`--json`, exercising the v0696 access-width tracking).

## Findings

- The whole span sits inside recovered ranges: the
  `--exclude-recovered` ranking is empty. The next player-corridor
  boundary lies past `0x10dcc` or in a sibling drive (nonzero
  selector/state), not on this trajectory.
- Hottest edges by witness count are the `0x27bxx` expansion loops
  (`main_texture_orchestrator_call`, 1B accesses at `0x0217d0ax` /
  `0x0050ea5x`) — recovered control blocks, not frontier.
- Shared fighter offsets on this corridor (both bases):
  - +0x0000 u32 R+W (ips 0x146b0, 0x19f0c/0x19f14/0x19f20);
  - +0x01a4 u32 R+W (ips 0x19f18, 0x1a234, 0x1a33c, 0x1a058);
  - +0x0194 u32 R+W (ips 0x146c8, 0x146ec, 0x143f0);
  - +0x0197 u8 R (ips 0x1465c, 0x1445c, 0x14460).
- +0x0194/+0x0197 corroborate existing C usage (0x144xx
  state-exchange reads, 0x14690 store chain, 0x1ab34 type-15 record
  chain) rather than new layout: single-trace evidence only, and
  the offsets already have behavioral coverage. No header change.

## Recommended next drives (unchanged order from v0292)

1. `0x19ef8` nonzero-state siblings from a player-boundary
   snapshot (`--set-u32 <fighter>+0x1a4=<mask> --until 0x0001428c`);
2. `0x29414` non-zero path (measure first);
3. geometry helpers after `0x28780`, ranked by a `--memory-trace`
   of the current player boundary;
4. physics/hitboxes only after 1-3 close (dedicated input-driven
   witness + `infer_structs.py` + new differential contract).
