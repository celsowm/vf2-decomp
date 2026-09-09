# fa_coli body (`0x221e8`) via measured interpretation — v0274

## Summary

The v0273 frontier stopped at the `fa_coli` entry with the body
unrecovered. This slice admits the measured PUNCH-driven warm body as an
explicit original-i960 bridge (`hybrid_execute_interpreted_task`) with
pinned poststate counts, after fixing the executor path that the body
requires (`movt` with a literal source).

The PUNCH corridor now continues through `fa_coli` and back to the
repeated `0x1645c` game_info dispatcher under strict per-block
differential.

## Executor fix: literal `movt`

`fa_coli` callee `0x23694` executes `movt 0, r8` at `0x236b8`
(encoding `0x5e401e00`): src1 is an inline literal; r8/r9/r10 receive
`0, 0, 0`.

`executor_arch.c` already implemented this, but `vf2_i960_run` (used by
`vf2probe` / interpreted tasks) resolves `vf2_i960_step` to the
`executor.c` implementation (`vf2_i960_step_legacy` after the rename),
so the arch literal path was not on the probe/run edge. Literal
multi-register moves are now handled in `vf2_i960_step_legacy` as well.
The unit test poisons r0/r1/r2 so a false-positive register-register
copy cannot pass.

## Drive and measure

From regenerated `out/sixth-regen.vf2snap` (`native-sixth-dispatch`,
MATCH, 8,675,721 insns):

1. `vf2cycles --input 16 --cycles 10` → countdown `312`, phase `0x8b`,
   input sample `0x0f000100` (PUNCH).
2. `+320 cycles` stops at `0x221e8` (`unsupported operation` before this
   slice). Failure checkpoint: `out/coli-fail.vf2snap`.
3. Reference body from that checkpoint:

   - `vf2probe --until 0x00010dcc --max-steps 200000`
   - **9,214 instructions**
   - **18 procedure calls / 19 procedure returns**
   - both sides at `0x00010dcc`
   - fighters `0x00510800` / `0x00512800` (from `0x500804` / `0x500808`)
   - runtime flags `0x8a00` (bit 5 clear → full body)

## Recovery shape

- `VF2_HYBRID_TASK_COLI` kind added.
- `hybrid_first_dispatch_task_execute` admits `VF2_TASK_COLI_ENTRY` only
  when `0x508000` bit 5 is clear; the body runs via
  `hybrid_execute_interpreted_task` until `0x10dcc` and must match the
  measured `9214 / 18 / 19` counters or fail closed.
- `native_runtime.c` routes `VF2_NATIVE_COLI_TASK_ENTRY (0x221e8)`.
- Callees `0x23524` (6 blocks), `0x22298` (31), `0x22404` (24) and
  `0x225cc` (174) remain original-i960; bit-5-set remains unsupported.

## Proof

`vf2cycles --snapshot punch10.vf2snap --input 16 --cycles 320`:

- completed **320/320** cycles (was 313 then fail)
- **12,946** compared blocks
- **14,962,620** reference == native instructions
- final `0x1645c` / `0x1645c`, **MATCH**
- scheduler entries/transitions/end `659/1033/334`

Unit/differential suites: `vf2_tests`, `vf2_native_runtime_tests`,
`vf2_native_differential_tests` pass.

## Remaining frontier

Native C for the `fa_coli` entry gate, the `0x23524` polygon-FIFO
prefix, the `0x22298` bit-mask helper, the `0x22404` contact query and
the large `0x225cc` resolver. Bit-5-set early-ret path unmeasured.
