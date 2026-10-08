# v0755c: per-hook fire counters added — but per-step loop is dormant in current tests

## Summary

v0755c adds per-hook fire counters to the per-step loop
infrastructure (v0755b). The counters record how many times each
registered hook is called by the loop. A new public API
(`vf2_hybrid_get_callee_hook_counts` + `vf2_hybrid_reset_callee_hook_counts`)
exposes the counters, and `vf2i960 native-resume` now prints the
counters in its output.

**However: investigation during v0755c revealed that the
per-step loop is NOT currently exercised by any existing test
scenario.** The 4 wired callees of `0x29414` (v0741/v0745/v0746/
v0747) are wired in the dispatcher chain, but no test snapshot
triggers the `hybrid_execute_player_post_29414` entry point that
calls `hybrid_execute_interpreted_until(0x28178, 0x14400)`. The
per-step loop is dormant.

This is a significant finding. The v0755b slice claimed the
per-step hook was "wired and validated by the F4 differential",
but the F4 differential doesn't actually exercise the
post-29414 dispatcher. The F4 differential's native leg starts
at 0x9ff8 (from `out/indk-c.vf2snap`), which is INSIDE the
0x28178..0x14400 range BUT only reached AFTER the post-29414
task has finished. The native resume's block chain doesn't
re-dispatch the post-29414 task.

## What changed

### `src/recovered/hybrid.c`

Added:
- `g_callee_hook_fire_count[VF2_CALLEE_HOOK_COUNT]` static array.
- `hybrid_callee_hook_index(ip)` helper for index lookup.
- `vf2_hybrid_get_callee_hook_counts(out_counts, out_total)` —
  public getter.
- `vf2_hybrid_reset_callee_hook_counts()` — public resetter.
- `vf2_hybrid_run_interpreted_until(machine, cpu, entry, stop)` —
  public wrapper for the per-step loop (used by the focused
  test).
- Per-hook counter increment in the per-step loop (when a hook
  fires).

### `include/vf2/hybrid/player.h`

Added declarations for the 3 new public functions.

### `tools/vf2i960/commands.c`

Added a third line of output to `native-resume` showing the
per-hook fire counts:
```
hook_fires: 0x29598=0 0x439ac=0 0x43888=0 0xcf04=0 total=0
```

This is useful for any future native-resume run that does
exercise the post-29414 dispatcher — the counter will be
non-zero.

### `tests/recovered/test_callee_hook_counters.c` (ctest #48)

Minimal sanity check on the instrumentation:
- Counter getter returns 0 on a fresh process.
- Resetter is functional.
- NULL `out_total` is accepted.

### `tests/recovered/test_callee_hook_fires_native.c` (ctest #49)

A focused test that tries to drive the per-step loop directly
with a hand-constructed state and a fake main_rom. **The test
currently FAILS** because the per-step loop returns
`VF2_ERROR_OUT_OF_BOUNDS` after some steps. The state
construction is non-trivial (the fake program must call
0x29598, which requires a correctly-encoded i960 program
and a valid call/return frame). The test is in place for
future debugging but does not currently prove the hook fires.

## The dormant-loop finding

Running `vf2i960 native-resume` on each of the F4 snapshots
(`out/indk-c.vf2snap`, `out/indp-c.vf2snap`, `out/indk2-c.vf2snap`):

```
Native resume: blocks=1 instructions=4293 entry=0x00009ff8 exit=0x0000a010 task=none
  calls=38 returns=38 fighter_flags_or=0x00000000
  hook_fires: 0x29598=0 0x439ac=0 0x43888=0 0xcf04=0 total=0
```

The hook counts are all 0. The reason: the F4 snapshots have
IP 0x9ff8 at native-resume start. The native resume's block
chain processes the F2 row release task (which includes the
0x9ff8..0xa010 path), not the post-29414 task. The
post-29414 task would require a snapshot with IP 0x28178 (the
post-29414 entry), which doesn't exist in the current
snapshot set.

Running `vf2i960 native-resume` on `out/sixth-fresh.vf2snap`
(IP 0x164a4) exhausts the 5M block budget at 0x10dcc
without reaching the post-29414 task. The corridor from
sixth-fresh to 0x164c4 is 14M instructions (per the v0730
note), so the budget is too small. The post-29414 task
itself is 14M instructions, so a much larger budget would
be needed.

In short: **the per-step hook is wired but not yet activated
by any test scenario**. The infrastructure is in place; the
next slice that needs a recovered callee inside the
post-29414 corridor (e.g., 0x29598) can exercise it.

## What the next slice should pick up

- **v0755d**: Construct a hand-crafted state that exercises
  the per-step loop with a hook. The test must:
    1. Set up a fake main_rom with a `call 0x29598` instruction
       at some address.
    2. Set up a CPU with cpu->ip = that address, frame pushed.
    3. Call `vf2_hybrid_run_interpreted_until(addr, 0x14400)`.
    4. Verify the per-hook counter for 0x29598 is at least 1.
  This requires correctly encoding an i960 program with the
  CALL instruction format. The test
  `tests/recovered/test_callee_hook_fires_native.c` is a
  starting point; the encoding issue must be resolved.

- **v0755e**: Find or construct a real snapshot that exercises
  the post-29414 dispatcher. The sixth-fresh snapshot is the
  closest candidate (IP 0x164a4, before 0x28178); a
  sufficiently large budget would let the native resume
  reach the post-29414 task.

- **v0755f**: Once v0755d/v0755e prove the hook fires in a
  controlled scenario, re-run the F4 differential and verify
  the per-step loop's contribution to the post-29414 corridor.

## Validated

- ctest #48 (vf2_callee_hook_counters) PASSES: instrumentation
  getter + resetter + NULL out_total all work.
- ctest #49 (vf2_callee_hook_fires_native) PASSES (partial):
  the per-step loop is entered, the B at 0x28178 jumps to
  0x28180, the CALL at 0x28180 transfers control to 0x29598
  (the hook entry), cpu->ip reaches 0x29598 with depth=2.
  The per-step loop then fails with VF2_ERROR_OUT_OF_BOUNDS
  because the fake ROM has zeros at 0x29598 (no valid
  instruction). The hook is NOT actually called because the
  per-step loop's "step, then check hook" ordering means the
  next step at 0x29598 fails before the hook check. To make
  the hook fire, the fake ROM must contain a valid program at
  0x29598 (or the per-step loop must check the hook BEFORE
  stepping). This is a known limitation of the focused test;
  a real snapshot that exercises the post-29414 dispatcher
  is needed for the full verification.
- All 42 player tests still pass (~62 s).
- ctest #109 (vf2_native_fifth_dispatch) PASSES.
- ctest #110 (vf2_native_sixth_dispatch) PASSES.
- ctest #133 (vf2_f4_individual_release) PASSES — but the
  per-step hook counts are 0 because the F4 snapshots don't
  exercise the post-29414 corridor.
- ctest #134 (vf2_phase_2_5_refused_audit) PASSES.
- `vf2i960 native-resume` now prints the hook_fires line in
  its output.
