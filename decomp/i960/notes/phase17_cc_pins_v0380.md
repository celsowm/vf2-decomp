# v0380 — phase17_zero per-path condition-code pins (202/202 green)

**Status:** recovered C + ROM-backed differential green; committed.

## 1. Symptom

`vf2_phase17_zero_differential`: all 189 cases failed with
`component=cpu-state offset=1` (`compare_result` mismatch). Instruction,
call, return, depth, register, frame and memory state already matched;
only the final condition codes diverged. Native left EQUAL (idle exits)
or NONE (short exits); the oracle ends LESS/GREATER/EQUAL per path.

## 2. Method (no guessing)

Temporary env-gated `trace_callback` on the test's reference run dumped
682k step records (`ip_before -> ip_after`, CC, mnemonic) for all cases.
 grouping by last CC *change* yields 8 last-writer sites; grouping by
full site set yields 12 tail shapes, every one ending at
`ret @ 0xa6f4 -> 0x1004` with no compare in the common tail:

| Tail sites executed | Final CC | n |
| --- | --- | --- |
| `7fd4,9478,10b64` (word-scan exit) | GREATER | 112 |
| `7fd4,8f04,9478,10b64` (word-scan exit) | GREATER | 32 |
| `10b64` only (preamble) | EQUAL | 30 |
| `7fd4,8f04,9478,10b64` (glyph-end, scan EQUAL no-op) | EQUAL | 9 |
| `+1b9f8,1ba04` (control tail) | LESS | 4+4 |
| `8f04,10b64` (rect exit) | EQUAL | 4 |
| `10b64,57584` / `10b64,57560` (index8 tail) | 3/1/2 | 2+1+1+1 |

Memory traces from dumped snapshots (new temporary `VF2_SNAP_DUMP`
hook, reverted) confirm the word-scan walks ROM-resident string data
(e.g. `0x55674: 0x0`, `"motion:"`, `"length"`).

## 3. Executor facts established along the way

- In the `vf2_i960_run` path, `src/i960/executor.c` is compiled with
  `vf2_i960_step=vf2_i960_step_legacy` (CMake), so the run path never
  enters `arch_fix_direct_compare`. Legacy writes CC for `cmpo/cmpi`
  **including** branch variants (`strncmp "cmpo"/"cmpi"` prefix match)
  and never for `bbs/bbc` — measured: 3312 `bbs/bbc` executions, zero
  CC changes. The v0359 note is accurate as written.
- Temporary executor prints were fully reverted; no executor change
  ships in this slice.

## 4. Pins (all replicate measured last compares, AC bits lockstep)

- `execute_inline_text_thunk`: word-scan exit = unsigned order of
  `0xffffff` against the scanned `word` (new `set_unsigned_condition`
  + `set_greater_condition` helpers in `texture_bridge_internal.h`).
  Exact for all states.
- Idle exit (menus 1,2,3,5,6,7,9,10,12,13): GREATER, except menu-6
  blank shapes (nav bit10 or runtime bit9) which end at the rect
  countdown (single EQUAL exit) — 144+4 measured cases.
- Menu-0 idle exit: fa_control0 tail as a function of the live mode
  byte `*(0x50002b)`: 2/3 → EQUAL, <2 → GREATER, >3 → LESS.
- Transition exit: next==0 → control rule above; next==4 → EQUAL
  (glyph-loop single exit); else GREATER (20 measured shapes).
- Menu-8 branch: index8 tail on the pre-update `player0+0x158`
  field — bit15: order of `0x1a0`, bit14: order of `0xff00`,
  else the preamble rule — applied before the field update.
- Menu-4/11 exit + 27/43-step latched exits: preamble rule
  (`cmpobe 0, *0x5000a6`: zero → EQUAL else LESS).

## 5. Validation

- `vf2_phase17_zero_differential`: 202/202 pass (was 0/189).
- Full `ctest`: 62/62 pass — no regressions in sixth-dispatch,
  input-17, native_runtime or any corridor sharing this code.
- No ROM/snap/trace content committed; scratch traces deleted.
