# `fa_player` integrated state-16 ordinary joins (v0445)

The real `0x1442c` body performs its two `0x14640` helper calls before
reaching the `0x14528` dispatch. A controlled reference row from
`out/park-1442c.vf2snap` set fighter 0 `+0x197 == 16`, a valid type-5 index
`0x73` at fighter 0 `+0x194`, and swept ordinary fighter-1 state bytes.

Measured ordinary values such as `0`, `1`, `2`, `15`, `17`, `23`, `26`, `28`
and `31` all reach the direct state-16 type-5 body in 98 instructions with
three calls/returns. The measured state-16, state-24, state-25 and state-27
successors take distinct paths (100, 105, an unresolved long path, and 126
instructions respectively) and remain separate boundaries.

Native C now admits only the measured bounded ordinary subset: fighter-1 state
`0..31` excluding 16, 24, 25 and 27. The live state16 fixture proves the
state-26 witness with exact CPU, condition-state, frame, and Work-RAM equality.
Values outside the measured subset remain fail-closed.
