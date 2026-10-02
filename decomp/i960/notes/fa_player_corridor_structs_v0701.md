# `fa_player` corridor candidate-field update (v0701)

`vf2probe --snapshot out/pre14288.vf2snap --until 0x00010dcc --trace
--memory-trace` (19,639 records, 5,004 successful accesses) through
`tools/python/infer_structs.py --base fighter0=0x00510980 --base
fighter1=0x00512980 --min-count 5`. 4,168/5,004 accesses are
non-fighter traffic (texture, work RAM, FIFO); the window-0x2000
fighter attribution below is what changed in
`include/vf2/fighter_candidate.h` (neutral `field_XXXX` names kept;
no semantic renames).

## New fields (single-corridor provenance, v0302-style)

- `field_001c` 4B RW: ips 0x1791c/0x17ac0/0x17ac8/0x16568. 0x0018-column
  neighbor with identical width/role to the v0696 pair.
- `field_0804` 4B RW: ips 0x1a23c/0x1a2f0/0x1a318 (19ef8 corridor) +
  0x28278/0x1b470.
- `field_0bdc` 1B RW: ips 0x29118 (x3), 0x26fac/0x26fe8/0x26ff0.
  Census byte; corroborates the v0389 `+0xbdc = 0x20` store.

## Role/width refinements (no type changes)

- `field_0000`: R -> RW (+9W at 0x19f0c/0x19f14/0x19f20/0x19f28/0x1a3a4,
  both bases).
- `field_01a4`: R -> RW (+3W, same 0x19f18/0x1a234-family ips, both bases).
- `field_01a8`: R -> RW (+1W).
- `field_01aa`: width stays 2B; 2x 1B + 10x 2B accesses recorded as
  byte-substructure evidence (0x1ad84/0x1b318/0x27ce0), not a change.
- `field_0018`/`field_0020`: same 4B RW role, new corroborating ips
  (0x1791c/0x17axx/0x17bxx game-info-expansion functions).
- `field_0026`: corroborated 2B R (0x16570/0x1664c/0x166d8/0x1690c/
  0x16998/0x16b9c).

## Not admitted

Fighter0-only clusters without repeated-IP stability or with
corridor-dependent widths stay out per the header criteria (same bar
as the v0696 width caution at +0x0644/+0x064c/+0x0650). A second
trace from a different park (bilateral-live player corridor) is the
cheapest way to promote the three new fields to dual-base status.
