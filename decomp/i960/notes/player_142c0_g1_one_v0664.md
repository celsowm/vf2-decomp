# v0664 — `fa_player` `0x142c0` `g1 == 1` sibling

Starting from the parked live `fa_player` corridor, the reference was first
advanced to `0x142c0` (10,869 instructions, +10 calls / +10 returns). The
body-entry `g1` register was then set to `1` in both reference and native
machines. Both executions reached `0x14310` with 55 body instructions,
three calls and three returns, and full CPU, condition, local-frame,
procedure-state and mutable Model-2A equality.

The existing `g1 == 0` witness remains 56 body instructions. No other
`g1` values are admitted by the recovery.
