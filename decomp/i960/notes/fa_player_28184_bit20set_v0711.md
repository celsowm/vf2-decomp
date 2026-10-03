# 0x28184 bit-20-set siblings native; counter/curve frontier pinned (v0711)

## Native now (9-row matrix + 3 frontier pins, full live-state equality)

Extended `test_player_28184_head_matrix_rom_pin` with per-row
counter/curve and three bit-20-set rows, all reaching `0x28268`:

| bit20 | bit17 | bit0 | edge==ctr | ctr | steps (oracle) |
| --- | --- | --- | --- | --- | --- |
| set | 0 | - | - | 1 | 11 |
| set | 1 | 0 | - | 1 | 13 |
| set | 1 | 1 | yes | 1 | 15 |

Same deltas as bit-20-clear (S0/S1/S2 = base/+2/+4); the set family
merges one step earlier via `0x281cc`, so native costs are 10/12/14.
`hybrid_execute_player_28184_prefix` drops the old
`sibling+bit20 refuses` hole; register/CC/tail behavior unchanged
(`r14`/`r13` loads, EQUAL pinned, status-bit0 still refused).

## Frontier pinned in-test (fail closed, defined next slice)

`counter != 1` (via the `0x28270 call 0x28918`) and bit-20-clear
`curve != 0` (via the `0x28208` table path, 50 steps) both leave the
branch-only head for **`0x28918`** — a 25-block float/transform
function (`0x28918 cvtir g6`; the v0707 float path joins it at
`0x2891c`). Three frontier rows assert oracle ip `0x28944` with
native `VF2_ERROR_UNSUPPORTED`. The synthetic fixture faults inside
`0x28918` (zeroed RAM), so recovering it needs the live lift-witness
fixture, not this matrix. Float `cvtir` executor semantics +
`0x28918` body recovery is the next 28184 slice.
