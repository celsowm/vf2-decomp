# Downstream dual-base witness (v0706, measured)

## Window

`out/tmp-lift-27ce0.vf2snap` parked at `0x27ce4`. All-true guard drive
(`--set-ip 0x27ce0`, F0 `+0x0c4e=1`, `w(+0x0c4c)=0x0505`; baseline
`+0x1a8=0x0505`, `+0x1aa=1`), `--max-steps 20000 --trace
--memory-trace`: 4811 steps to an idle self-branch halt, 2825 accesses,
615 in the fighter window (bases `0x00510980` / `0x00512980`).
Raw stream consumed for analysis only, not committed.

## Path shape (no semantics invented)

Guard `ret` path returns into the `0x179xx` caller region, which walks
both fighters (`0x1791c` 0018/0020/001c group, `0x1f258`/`0x1f274`/
`0x1f278` paired reads, `0x164d4`/`0x164e0` zeroing loop). The 0x28184
chain windows (early-out and main-path, prior runs) stay fighter0-only.

## Dual-base results (same offset, same width, same guest functions)

- `+0x0008` 4B R NEW, dual-base from birth: same ip `0x10e3c` both.
- `+0x0018` / `+0x0020` 4B: same-ip-both R at `0x1f274` / `0x1f278`;
  F0 RW elsewhere, F1 R-only in this window.
- `+0x0000` 4B dual-base R (F1 at `0x10dbc` / `0x16470`); W F0-only.
- `+0x01a4` fourth window (F1 at `0x17718` pairing F0 `0x17714`;
  same-ip `0x1f258`); W stays F0-only.
- `+0x01b1` 1B dual-base R (F1 `0x1d628` beside F0 `0x1d60c`).
- `+0x1200` 1B dual-base W (F1 `0x164d4` / F0 `0x164e0`, one loop).
- `+0x0804` third window (`0x1b470` / `0x17bec` 4B R), still F0-only.
- `+0x0bdc` untouched downstream: note-only.
- `0x186c-0x188c` extends to `0x1884-0x188c` + `0x18a8-0x18b0`
  (same `0x16fbc` / `0x17024` / `0x17040` RW ips), still F0-only:
  struct extension deferred until dual-base or native need.

## Measured negative

g7-swapped F1 subject (`--set-reg g7=0x512980`) faults at `0x28944`:
F1 `+0x0bd8`-chained dereference lands near-null (`0x780`-range).
Cross-fighter object swap is not a faithful experiment; nothing is
promoted from it (fail closed).
