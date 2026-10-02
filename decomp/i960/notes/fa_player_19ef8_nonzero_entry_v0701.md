# `fa_player` 0x19ef8 nonzero-entry normalization (v0701, scouting)

Frontier item "0x19ef8 nonzero-state siblings" (`--set-u32
<fighter>+0x1a4=<mask> --until 0x0001428c`) is retired as a *measured
negative* for the corridor entry, complementing the v0690 mask-family
differential (which proves per-row equality through corridor+head).

## Measurement

`vf2probe --snapshot out/pre14288.vf2snap --set-u32 0x00510b24=0x59f
--until 0x0001428c --trace` (0x59f = admitted mask-family row) vs the
unmutated baseline:

- Identical path: calls 0x19ef8 / 0x1a1e4 / 0x26ef0 / 0x27130,
  rets at 0x1a048 / 0x1a0c8 / 0x1a0cc / tail `ret 0x1428c`.
- Identical totals: 1622 steps, 4 calls / 4 rets.
- Identical final: `+0x1a4 == 0x200` in both runs (probed
  `--read-u32 0x00510b24`).

The corridor normalizes nonzero entry state before / without changing
its shape: the entry mask selects no divergent branch on this
trajectory (bit-14-clear skips the clrbit prologue per v0389, and the
final 0x200 is produced downstream in both runs).

## Implication

Do not spend recovery effort on entry-mask variants of the 0x14288
corridor: v0690 already proves 49 rows x 16 branch subsets, and this
probe shows the mechanism (early normalization, identical streams).
The remaining player frontier is items 2-4 of
`fa_player_corridor_frontier_v0696.md`: the 0x29414 non-zero path
(mostly recovered since v0295; bit-19-set arms in C), geometry
helpers after 0x28780, and physics/hitboxes (needs a dedicated
input-driven witness + new differential contract).
