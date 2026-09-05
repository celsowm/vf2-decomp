# fa_object handlers (`0x6ca64` family) — v0268

## Static shape (measured with `vf2i960 disasm/function`)

- `0x6ca64` dispatcher (1 block, 4 insns): `ldob 0x04(g13),r3` (instance),
  `ld 0x6ca78[r3*4],r5`, `st r5,0x0c(g13)`, `ret`. Already native in
  `hybrid.c` (`VF2_TASK_OBJECT_ENTRY`); this work adds its continuations.
- Table `0x6ca78` (probe-read from live snapshot): `[0x6cae0, 0x6caf4,
  0x6cb08]` for instances 0/1/2 (`fa_object0/1/2`).
- `0x6cae0` (3 insns): `lda 0x6caf0,r15; st r15,0x0c(g13); ret`.
- `0x6caf0` (1 insn): `ret`.
- `0x6caf4` (3 insns): `lda 0x6cb04,r15; st r15,0x0c(g13); ret`.
- `0x6cb04` (1 insn): `ret`.
- `0x6cb08` (1 insn): `ret`.
- Same self-disarming init idiom as the `fa_coli` stub at `0x221cc`
  (`scheduler.c:execute_initializer_task`), minus the `+0x90`/`+0x13c`
  zeroing. The per-frame re-arm lives in
  `native_runtime_condition.c:execute_selector2_body` (writes `0x6ca64`
  to both object control blocks), so these continuations are dormant in
  the accepted corridor and were proven from synthetic state (v0200
  orchestrator-limits precedent).

## Reference poststate (synthetic state + in-process executor)

Setup: zeroed machine, `enter_procedure(entry, 0x10dcc)`, `r29`
(= `g13`) = scratch registry with `+0x00=0x11111111`,
`+0x04=instance`, `+0x0c=0xdeadbeef`, `+0x90/+0x13c` canaries,
`AC&7=5`, `compare=OVERFLOW`. Step until `ip == 0x10dcc`.

| entry   | steps | `+0x0c`    | other RAM | AC/cmp | local r15 |
|---------|-------|------------|-----------|--------|-----------|
| 0x6ca64 | 4     | 0x6cae0    | unchanged | kept   | 0         |
| 0x6cae0 | 3     | 0x6caf0    | unchanged | kept   | 0         |
| 0x6caf0 | 1     | 0xdeadbeef | unchanged | kept   | 0         |
| 0x6caf4 | 3     | 0x6cb04    | unchanged | kept   | 0         |
| 0x6cb04 | 1     | 0xdeadbeef | unchanged | kept   | 0         |
| 0x6cb08 | 1     | 0xdeadbeef | unchanged | kept   | 0         |

Local `r15` (index 15) is the callee scratch register: `lda` targets it
and `ret` restores the caller frame, so the caller-frame value (zero)
survives. No other word in the 1 MiB work RAM changes (full `memcmp`).

## Recovery

- `src/recovered/hybrid.c`: `HANDLER0/1_ENTRY` cases write the measured
  continuation with `body_instructions=2`; the three ret continuations
  use `body_instructions=0`; all complete via `hybrid_complete_procedure`
  and report `VF2_HYBRID_TASK_OBJECT`. Unmeasured entries still hit
  `default: VF2_ERROR_UNSUPPORTED`.
- `src/recovered/native_runtime.c`: router admits the five handler
  addresses to `vf2_hybrid_first_dispatch_task_execute`.
- `tests/recovered/test_object_handlers.c`: ROM-independent
  invalid-argument checks plus ROM-backed 6/6 exact differential (full
  32 registers, AC, compare state, IP, frame depth, all counters, full
  work-RAM `memcmp`, report kind/exit/counts). Registered as
  `vf2_object_handlers_differential`.

## Still open (next slices)

- `sub_0x6ca84` (5 blocks, indirect `callx` loop over count at
  `0x6cad0` = 3 with control blocks from table `0x6cad4` =
  `[0x500878, 0x50087c, 0x500880]`, checking bit 31): called once from
  `0x1dd70` (camera region). Unrecovered.
- `fa_coli` recurring `0x221e8` (16 blocks) + callees `0x22298`
  (31 blocks), `0x22404` (24 blocks), `0x225cc` (174 blocks), `0x23524`
  (6 blocks). Touches fighter `+0x1a4/+0x1f4/+0x61c/+0x804/+0x808/+0x822`
  — prime `fighter_candidate` field evidence. Scout via probe mutation
  of the coli registry entry past the selector2 re-arm.

## Negative scouts (recorded so the next slice skips them)

Sixth window (2026-09-05): from `scratch-sixth.vf2snap` (live coli entry
`0x51498c = 0x221cc`, flags `0x514980 = 0`, fighters `0x510a40/0x512a40`),
forcing `0x514980=0x80000000` + `0x51498c=0x221e8` with `--until 0x221e8`
over 1M probe steps / 15.3M guest instructions never reaches `0x221e8`
(end `ip=0x10fa0`, call/return counters identical to the unmutated run)
and the entry is never re-armed to `0x221cc` either.

Fifth window (2026-09-05): regenerated `out-fifth.vf2snap` via
`vf2i960 native-fifth-dispatch roms/vf2 out-fifth.vf2snap` (MATCH, 836
blocks, 7,402,744 insns, fifth entry `0x1645c`). Baseline reads:
`0x514980=0`, `0x51498c=0x221cc`, fighters `0x510a40/0x512a40`,
`0x500828→0x514980` (control pointer confirmed). Mutated run
(`0x514980=0x80000000`, `0x51498c=0x221e8`, `--until 0x221e8`, 1M steps /
15.28M insns): halt `maximum steps`, end `ip=0x10f98`, calls/returns
`10255/10254` identical to baseline, entry stays `0x221e8`, flags stay
`0x80000000`. Control run (flags forced, entry left `0x221cc`,
`--until 0x221cc`): also never reached, identical counters
(`10255/10254`, end `ip=0x10f98`).

Conclusion: the fifth/sixth corridor windows contain no sweep that
dispatches task index 10 (`fa_coli`) at all — runnable or not. The fifth
validation itself reports only 3 repeated scheduler entries, so the
accepted corridor dispatches a fixed few tasks per frame and coli is not
among them. Directed flag/entry mutation inside these windows is
exhausted; do not repeat.

Next attempt must park a snapshot inside a window that actually sweeps
index 10. Concrete direction: `vf2i960 snapshot roms/vf2 boot.vf2snap`
then `native-resume` with a stop address in the second-dispatch
initializer corridor (where `execute_initializer_task` is known to run;
`native-first/second/third/fourth-dispatch` validate but take no snapshot
arg, so `snapshot` + `native-resume … [stop] [output.vf2snap]` is the
available builder). From there, let the init stub run
(`entry→0x221e8`) and watch whether a later same-frame pass dispatches
`0x221e8` — the likely mechanism is a scan-until-quiescent sweep
revisiting index 10 after init without an intervening re-arm.
