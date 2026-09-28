# `fa_coli` whole-task stale-empty witness `9398/17/18`

The parked whole-task snapshot at `0x000221e8` was replayed through the
reference executor with this measured setup:

```text
fighter0 + 0x1a4 = 0x00000100
fighter0 + 0x820 = 0x01
0x005149cc       = 0xffff
g13              = 0x00514940
```

The reference reaches `0x00010dcc` in `9398` instructions, with `17` calls
and `18` returns. The first `0x22404` contact query has slot 0, stale snapshot
`0xffff`, current snapshot `0`, pending mask clear, and an empty computed
result. The native body already modeled the measured stale-empty join, but its
selector gate admitted only indices 0, 2 and 5. Index 1 is now admitted only
for the same measured stale-empty shape.

`tests/recovered/test_coli_whole_task_live.c` runs this case against the real
ROM and compares CPU state, condition state, frames, counters and mutable
Model 2A memory. Unrelated stale-empty combinations remain unsupported.
