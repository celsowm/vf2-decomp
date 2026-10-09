# v0761 retracted: 0x1fcc0 hook wiring is unsound as wired

**Status**: REVERTED. v0761 was committed at `f3dac423` then force-pushed
away. The repo's `master` is now at `d9f7ba0a` (the v0760 AGENTS.md sync),
which is the last known-good state. This note documents the diagnosis so
the next attempt at the same goal is principled.

## What v0761 did

`src/recovered/hybrid.c`: `g_callee_hooks[]` got a 5th entry for
`0x1fcc0` (display_profile_apply, recovered v0755g). The v0755g
recovery inlines all 6 sub-callees (1fee4, 1ff0c, 1fffc, 4b410, 11704,
2eab8) plus refuses one more (`0x2c38` from inside the inlined
`0x1fffc`).

The commit message claimed "F4 differential still PASSES — no measured
path currently reaches 0x1fcc0." **This claim is false** and is
retracted. The F4 native path **does** reach `0x1fcc0`, and the
hooked recovery sends the per-step loop into an infinite stepping
cycle.

## What went wrong

### Diagnosis

`hybrid_range_has_hooks(entry, stop)` (hybrid.c:524) uses a wrap-around
check when `stop < entry` (which is true for the 0x28178 -> 0x14400
dispatch chain — the loop walks "backward"):

```c
(stop_address < entry_address &&
 (h >= entry_address || h < stop_address))
```

For `entry = 0x28178` and `stop = 0x14400`:

- `h = 0xcf04` (the v0747 hook): `0xcf04 < 0x14400` is **true** →
  hook is in range (correct, intended).
- `h = 0x1fcc0` (the v0761 hook): `0x1fcc0 < 0x14400` is **false**;
  `0x1fcc0 >= 0x28178` is **false** → hook is NOT in range
  semantically, **but** …

That part is correct. The actual F4 path doesn't go through
`hybrid_execute_interpreted_until(0x28178, 0x14400)` directly. The
F4 path is `vf2_native_runtime_run_until` with stop `0xa010`, which
dispatches through `vf2_hybrid_second_scheduler_enter`. That
function's player dispatch (the post-29414 work) does call the
0x28178 -> 0x14400 corridor via the player task wrappers. **However,
the F4 path measured in v0734l doesn't actually reach 0x1fcc0 either.**

The real failure is different and more subtle. With the 0x1fcc0 hook
wired, the per-step loop's `hybrid_range_has_hooks` returns 1 (because
of `0xcf04` being in range via the wrap condition). Then on the F4
path through the 0x28178 -> 0x14400 corridor, the i960 path eventually
takes a branch to `0x1fcc0`. The per-step loop fires the v0755g
recovery. The recovery's natural exit is `VF2_ERROR_UNSUPPORTED` at
`cpu->ip = 0x20050` (the post-`0x2c38`-refusal in the inlined
0x1fffc). The per-step loop's refusal path is:

```c
} else if (status == VF2_ERROR_UNSUPPORTED) {
    /* Function refused cleanly; cpu->ip is at
     * the entry IP. We've already stepped past
     * the call site, so the next iteration will
     * not re-fire. */
    continue;
}
```

But the recovery's `cpu->ip` is **not** at the entry IP — it's at
`0x20050`, which is the `ret` slot for the inlined 0x1fffc. The next
iteration's `vf2_i960_step(cpu, machine, NULL)` executes that `ret`,
which pops a frame. The frame is the caller's (0x1fcc0 was wired in
the per-step loop; the recovery pushed a frame for 0x1fee4 then ran
sub-calls inline; nothing pushed a frame for 0x1fffc), so `ret` pops
the wrong frame and the CPU ends up in an inconsistent state. The
per-step loop's stepping then continues to spiral because the IP
wraps back into the [0x28178, 0x14400] range somehow (the caller's
frame return mis-routes), and either matches another hook or never
reaches `stop_address`. **The 600-second ctest timeout is
`vf2probe native-resume` looping indefinitely.**

### Why the v0755c/v0755g "DORMANT" framing was wrong

The v0755c note said:

> The hook fires only if the per-step loop's CPU arrives at `0x1fcc0`.
> In the current F4 scenario, the dispatch chain doesn't reach this
> IP. The hook remains DORMANT for the live test but is now
> available for any future scenario that does reach it.

This was a measurement claim: "the F4 path doesn't reach 0x1fcc0."
The measurement was wrong. The F4 path **does** reach 0x1fcc0, and
the dispatch chain 0x28178 -> 0x14400 is the entry path that
**reaches it as a downstream branch target** (the original i960 code
at `0x1fcc0` is on the post-29414 corridor).

I re-ran `vf2_f4_individual_release` at the v0760 state (hook absent):
**PASS in 22.00 s** (ctest #144, `vf2probe --until 0x9ff8` =
reference 4293/38; `vf2i960 native-resume` to 0xa010 = native 4293/38).

I re-ran the same test with the 0x1fcc0 hook re-enabled: **FAIL with
>600 s ctest timeout** (`vf2probe --until 0x9ff8` still reports 4293/38
correctly; `vf2i960 native-resume` never completes).

## The fix (v0762 plan)

Three options, in order of preference:

1. **Make the 0x1fcc0 recovery return VF2_OK** instead of
   VF2_ERROR_UNSUPPORTED. The recovery has to fully simulate the
   `0x2c38` sub-call (which it currently refuses) so the per-step
   loop doesn't continue from 0x20050. This is a recovery rewrite,
   not a hook fix.

2. **Make the per-step loop's UNSUPPORTED branch safe.** Instead of
   `continue;`, it should: (a) verify cpu->ip is actually at the
   entry IP, and (b) if not, set cpu->ip = entry_ip + 4 (or whatever
   the call-site return is) and continue. This is a per-step loop
   fix that would protect all current and future refused hooks.

3. **Leave 0x1fcc0 unwired** until option 1 or 2 is done. This is
   the current state.

## What v0762 actually is now

The v0762 test that I had drafted (hand-crafted `cpu->ip = 0x1fcc0`
test) is **deleted** because it tests a hook that isn't wired. The
per-step loop infrastructure is already covered by
`vf2_callee_hook_fires_native` (ctest #58) and
`vf2_callee_hook_counters` (ctest #49) — both of which use the
`0xcf04` hook, which IS wired and IS exercised by the F4 path.

The next useful slice is v0763+: investigate **option 1 or 2** above,
or recover more sibling functions in the post-0x1fcc0 corridor that
don't depend on the broken wiring.

## Validated

- `git reset --hard d9f7ba0a && git push --force-with-lease origin master`:
  origin/master is now at the v0760 AGENTS.md sync.
- `cmake --build build --config Debug --parallel`: clean.
- `ctest -R vf2_f4_individual_release --output-on-failure`: PASS in
  22.00 s, ctest #144.
- v0761 commit `f3dac423` is in git reflog and the forced-push
  reflog but no longer in `master`. The `per_step_hook_1fcc0_wired_v0761.md`
  note was deleted along with the commit's tree.

## Files

- `decomp/i960/notes/per_step_hook_1fcc0_wiring_retracted_v0761.md`
  (this file).
- `decomp/i960/notes/per_step_hook_1fcc0_wired_v0761.md`: **DELETED**
  with the commit; the diagnosis in this file is its replacement.
