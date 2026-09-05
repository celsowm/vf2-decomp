# fa_coli recurring (`0x221e8`) hunt — v0269 (negative with mechanism)

## Static mechanism (measured with `vf2i960 disasm/function`)

- `scheduler_dispatch` at `0x10d54` (24 blocks) walks the live registry
  from `0x510000` by descriptor strides, checks bit 31 per index
  (`bbc 31,r4 → skip`), and dispatches inline via `callx (r4)` with
  `r4 = *(registry+0x0c)`.
- At sweep entry (`0x10d64`) it reads `0x500068`: bit 16 set selects the
  **fast path** at `0x10e68` — a single-slot loop (`0x10ea0..0x10ea4`,
  one `u32`) dispatching through an indirection chain
  (`slot → control → registry → flags → entry → callx`). Bit 16 clear
  selects the **full 29-task scan**.
- The `0xa010` front-end calls `0x10d54` exactly once per frame
  (falls through to `0xa014/0xa018/...`); sweeps are single-pass, so a
  sweep never revisits an index it already passed.
- The selector2 re-arm (`native_runtime_condition.c:execute_selector2_body`,
  frame-dispatch tick gated on runtime-flags bit 5 clear and frame
  selector 2) **unconditionally** writes coli flags `|= 0x80000000` and
  entry `0x221cc`. Both are coupled; it runs in boot frames, then goes
  quiet (a forced `entry=0x221e8` survived 8.87M insns untouched).

## Dynamic negatives (reference executor, all deterministic)

| # | window | mutation | budget | result |
|---|--------|----------|--------|--------|
| 1 | sixth snapshot | flags=1, entry=`0x221e8`, until `0x221e8` | 1M steps / 15.3M insns | miss, identical counters |
| 2 | fifth snapshot (regen, MATCH 836 blocks) | same | 1M / 15.3M | miss, identical `10255/10254` |
| 3 | fifth snapshot | flags=1, entry stays `0x221cc`, until `0x221cc` (control) | 1M / 15.3M | miss — no sweep dispatches index 10 at all |
| 4 | boot → 50M resume-trace | none, until `0x221cc` | 50M insns | miss (control: until `0x1645c` hits at 5,597,176 insns, so the tool/stop works) |
| 5 | validated boot prefix chain (see below) | none, until `0x221cc` | 8M + 50M insns (61M total, 1.5M calls, ~30k frame IRQs) | miss; coli flags read 0 end to end |
| 6 | post-second snapshot (observe-parked, entry `0x221cc`, flags 0) | flags=1, until `0x221e8` | 2M / 8.87M | miss; forced state preserved untouched |
| 7 | post-second snapshot | flags=1, entry=`0x221e8`, slot `0x10ea0=0x500828` (fast-path hijack), until `0x221e8` | 2M / 8.87M | miss; slot rewritten to `0x500834` each frame, nothing consumed the hijack |
| 8 | fifth snapshot | until `0x10d70` (full-sweep entry), until `0x10e78` (fast loop) | 1M each | both miss — neither sweep path executes in steady frames as entered here |

Run 5 detail: `snapshot` (reset→`0x1b0`) → resume-trace to `0x4aff8`
(2,985,244 insns) → resume-trace to `0x4b07c` (96 steps) →
resume-trace 8M + 50M toward `0x221cc`. End `0x6e1d0`, coli flags 0,
entry `0x221cc`, scheduler preconditions identical to the validated
live snapshot (`0x500068=0x80004400`, `0x508000=0x8a00`, count 29).
The validated prefix differs by one manual timer IRQ+int14 (see
tooling); without it the run converges to an attract-idle where
nothing ever sets coli runnable.

Run 7 detail: the dereference chain
`[0x10ea0]=0x500828 → [0x500828]=0x514980 → flags → entry → callx`
is correct, but the slot is re-armed to `0x500834` before any sweep
consumes the hijack (end slot read `0x500834`, counters identical to
run 6).

## Conclusion

`0x221e8` needs `entry=0x221e8 + flags=1` at a scanning sweep. The
attract trajectory never produces that conjunction: the init runs once
(second sweep, gate + MATCH proven), the re-arm rewrites
`0x221e8→0x221cc` during the bridge, and steady frames fast-path past
index 10 forever (flags permanently 0). The body is reachable, if at
all, only in gameplay (demo/match) frames, which need driven inputs —
a new workstream (post-boot input profiles). No native C was touched;
`fa_coli` stays fail-closed past the init stub.

## Tooling added (passive, all default-off, backward-compat proven)

- `vf2probe --raise-irq <bits>` + `--enter-interrupt <vector=level>`
  (`tools/vf2probe/main.c`): one-shot raise-then-enter after
  restore/mutations, mirroring `commands.c` order. Smoke: option-free
  run reproduces the fifth baseline bit-identically (ip `0x10f98`,
  14,375,290 insns, `10255/10254`).
- `resume-trace` trailing `[raise-irq] [enter-vector] [enter-level]`
  (`tools/vf2i960/commands.c`, argc 14; existing forms byte-identical:
  11-arg control reproduces stop `0x1645c` at 5,597,176 insns).
- `observe-third-sweep`: `VF2_PARK_SNAPSHOT` env-gated boundary park
  (post-second-dispatch reference state) + per-step `0x221cc/0x221e8`
  watch lines (no break on `0x221cc`; break + re-park on `0x221e8`).
  Observe result with parking: MATCH, sweeps #1–4 all select index 13
  (`fa_game_info`), zero coli hits — the watch is live but silent.
- Parked `out-postsecond.vf2snap` (gitignored; regenerate with
  `VF2_PARK_SNAPSHOT=out-postsecond.vf2snap observe-third-sweep`):
  ip second `0x1645c`, 6,872,547 insns, coli entry `0x221cc`,
  flags 0 — the proven base for the gameplay-inputs workstream.
