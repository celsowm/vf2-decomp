# `fa_player` `0x1453c/0x14570`: bounded type-5 selector sweep v0475

## Evidence

The reference executor was driven from `out/park-1442c.vf2snap` at
`0x00014528` with the direct state-27 shape, fighter bases
`g7 = 0x00510980` and `g8 = 0x00512980`. The low-half selector at
`g7 + 0x194` was swept through `1..32` and the run was stopped at the common
`0x0001463c` return.

The reference reaches the return for every selector. The measured walker
misses are:

```text
1..12, 16..21, 23..24, 26, 28..29, 31..32
```

Selectors `13, 14, 15, 22, 25, 27, 30` return type-5 records through the
already recovered record path. All 32 cases take one nested call and return;
their measured instruction counts are pinned in the live differential fixture
and range from 52 to 69 instructions.

## Recovery boundary

The native state-27 body now admits exactly the measured miss selectors above,
plus the earlier `0x0110` and `0x02cf` witnesses. A miss still produces the
measured zero-record data path. Any other miss selector remains
`VF2_ERROR_UNSUPPORTED`; no general selector predicate is inferred from this
bounded sweep.

## Acceptance

`tests/recovered/test_player_1453c_live.c` compares all 32 selectors against
the reference for exact instruction counts, registers, condition state,
frames, call/return counters and mutable Model 2A memory.
