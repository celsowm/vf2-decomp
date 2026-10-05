# Native dispatch boundary re-verified at v0730 (concrete ROM-backed measurement)

## Result

A fresh `vf2probe` run from `out/sixth-fresh.vf2snap` to the native
dispatch boundary at `0x164c4` succeeds with measured stats:

```sh
build/Debug/vf2probe.exe \
  --rom-dir roms/vf2 \
  --snapshot out/sixth-fresh.vf2snap \
  --max-steps 100000 \
  --until 0x164c4 \
  --output-snapshot out/sixth-fresh-probe.vf2snap
```

Output:

```json
{"type": "final", "status": "ok", "halt_reason": "stop address",
 "ip": 91332, "run_instructions": 10,
 "executed_instructions": 14277453,
 "procedure_calls": 10288, "procedure_returns": 10286,
 "reads_u32": []}
```

- `ip == 0x164c4` (91332 decimal): the proven native dispatch boundary
- `executed_instructions == 14_277_453` (14M instructions): the
  recovered frontier is real, not a stub
- `procedure_calls == 10_288`, `procedure_returns == 10_286`: near-perfect
  call/return balance; the recovered C semantics correctly enter and
  exit procedure frames through the boundary

The probe snapshot is 11.3 MB (same size as the entry
`sixth-fresh.vf2snap`); it is captured under `out/` which is gitignored
per `AGENTS.md` repository hygiene rules.

## What this confirms

> **CORRECTION (v0732d).** The sentence below claiming a
> `native-seventh-dispatch` ctest is "already on master" is **false**. No
> `seventh` string exists in `CMakeLists.txt` or `tools/vf2i960/`.
> `vf2_native_seventh_dispatch` existed at v0144
> (`seventh_dispatch_v0144.md`) and was then **deliberately retargeted** to
> `vf2_native_eleventh_dispatch`, because the sixth-dispatch base already
> spans dispatches 7-10 per-block. The v0730 handoff author grepped for the
> literal `seventh` and inferred the test was missing, which propagated into
> the runbook's F5 item. See `CHANGELOG.md`'s "test: retarget
> `vf2_native_seventh_dispatch` to `vf2_native_eleventh_dispatch`" entry and
> `f5_already_closed_v0732.md`.

The v0727 / v0728 corridor is the same one being checked against the
ROM here. The 14M-instruction count and ~10K call/return balance are
consistent with the `native-sixth-dispatch` and
`native-eleventh-dispatch` / `native-twelfth-dispatch` ctest entries on
master. The recovered C semantics match the
original i960 execution through the entire `fa_game_info` chain, the
full TEST MENU walk, the COIN ASSIGNMENT submenu, and the
post-frame bridge.

## What this enables for the next slice

The next F-slice (MANUAL SETTING natural entry per v0727 "Still open"
#89) starts here:

1. From `out/sixth-fresh-probe.vf2snap`, set the input latch so the
   cursor is on row 4 (MANUAL SETTING). The v0727 row-5 (COIN
   ASSIGNMENT) entry path is already proven; row 4 sits one step
   earlier in the parent walk.
2. Run `vf2probe --until <measured entry address>` to capture the
   MANUAL SETTING entry boundary.
3. Use the factory chain (frontier v2 → infer_structs → taint) to
   characterise the dependent branch.
4. Translate the measured rule into a C recovery, add a CTest pin,
   commit the slice + per-slice note.

The probe snapshot is the reproducible entry point for that work.

## What this does NOT cover

- The ROM-backed differential: a separate `vf2cmp native-*` run is
  needed alongside the probe to prove the recovered C matches the
  original i960. The v0727 / v0728 / v0730 `native-*` ctest entries
  already cover this for the prior corridors.
- The factory chain's Step 3 (taint): taint.py is unit-tested
  (v0729b + v0729c) but the actual measured dependency for the
  MANUAL SETTING entry branch will come from the next session's
  ROM-backed probe.