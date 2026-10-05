# v0733g: the macro removal was measured — 19 of 119 tests break, all of them `compare_result` only

v0733f isolated the cause and refused to flip it, saying the measurement was the
next step. This is that measurement, and it settles the question: **keep the
macro.** It is load-bearing for the current recovery in exactly the way the
`0x23524` `g6`-bit0 refusal is load-bearing.

## What was built

`CMakeLists.txt` now has an option, **default ON, i.e. unchanged behaviour**:

```cmake
option(VF2_LEGACY_STEP_IN_RUN
    "Compile vf2_i960_run against the legacy stepper (default ON)" ON)
```

Both trees print which is in effect at configure time, in the same spirit as the
sanitizer STATUS line:

```text
-- VF2_LEGACY_STEP_IN_RUN=ON:  vf2_i960_run uses vf2_i960_step_legacy (arch_fix_direct_compare is NOT applied on that path)
-- VF2_LEGACY_STEP_IN_RUN=OFF: vf2_i960_run uses the arch stepper - VARIANT BUILD, oracle semantics differ
```

End-to-end confirmation through the real tool, same snapshot, same address:

```text
build            ip=144676 compare=equal
build-archstep   ip=144676 compare=none
```

`build/` is untouched and still passes; `build-archstep/` is gitignored via
`/build-*/`.

## The result: 19 of 119 fail

```text
84% tests passed, 19 tests failed out of 119
```

The failures, in full:

```text
17  vf2_player_270d4_five_slot_pin          92  vf2_native_first_dispatch
51  vf2_phase17_zero_differential           93  vf2_native_second_dispatch
61  vf2_coli_23524_live_differential        94  vf2_native_third_dispatch
66  vf2_player_28918_live_differential      95  vf2_native_fourth_dispatch
91  vf2_hybrid_first_dispatch               96  vf2_native_fifth_dispatch
                                            97  vf2_native_sixth_dispatch
                                            98  vf2_native_eleventh_dispatch
                                            99  vf2_native_twelfth_dispatch
                                           100  vf2_texture_bridge_differential
                                           101  vf2_post_frame_bridge_differential
                                           102  vf2_geometry_boundary_differential
                                           103  vf2_second_scheduler_entry_differential
                                           104  vf2_game_geometry_helpers_differential
                                           105  vf2_third_sweep_observation
```

## Every failure is the same class, and that is the useful part

**All 19 report `component=cpu-state offset=1`**, which
`vf2_i960_compare_live_state` maps to `compare_result`. Representative lines from
the variant log:

```text
FAILED 28184 row 0 diff component=cpu-state offset=1 expected=00000000 actual=00000002
phase17-zero index0-control-test   component=cpu-state offset=1 expected=0x00000002 actual=0x00000001
Task fa_camera mismatch: cpu-state offset=0x1 expected=0x00000002 actual=0x00000003
                         bytes=1 ref_cc=2 nat_cc=3 ref_ac=3f001002 nat_ac=3f001001
```

There are **no branch failures, no instruction-count failures and no memory
failures.** Control flow is byte-identical either way. The macro changes the
condition word and nothing else.

Note the direction in the third line: in the variant the *reference* reads
`EQUAL` and the *native* `GREATER`. In the default tree they agree. **The
recovered `hybrid.c` was written against the legacy path** — every constant in it
came from a `vf2probe` measurement, and `vf2probe` runs on `vf2_i960_run`. So the
recovery encodes the legacy semantics, and the variant moves the reference away
from them.

## Decision: the macro stays

Three things follow, and only the third is new work:

1. **`VF2_LEGACY_STEP_IN_RUN` stays ON by default.** The default tree's behaviour
   is unchanged and its suite is green.
2. **The asymmetry is documented, not "fixed".** v0733f correctly declined to
   flip it; this measurement is the reason that was right rather than merely
   cautious. An agent reading `CMakeLists.txt` will now see the option, its
   default, and a STATUS line saying which semantics are live.
3. **The BBT convention question is still open, and is now clearly a research
   task rather than a bug.** Real i960 `BBT` sets `ac0 = <bit>, ac1 = 0`, i.e.
   *equal* on a clear bit. The repo's `arch_fix_direct_compare` uses clear ->
   `NONE`. The legacy path leaves the word alone. The 19 failures show the
   recovery depends on the legacy behaviour, so choosing the architecture's
   version means re-deriving 19 pinned differentials — which is a decompilation
   project, not a patch. It needs its own slice with its own evidence.

## B48 is revised, because its acceptance criterion was wrong

The completion plan set the bar as *"a regression test that drives both entry
points over a `bbs`/`cmpob` window and asserts they agree."* **That criterion is
withdrawn.** The measurement shows they are not supposed to agree today, and
asserting agreement would have failed 19 tests — i.e. it would have been a test
that could only pass if someone re-derived the whole recovery.

The correct test pins the **difference**: drive `bbs 5, r15` over a window where
the two are known to diverge, assert the legacy path leaves the compare word
alone while the arch path rewrites it, and print the affected-test count. That
turns the asymmetry into documented, guarded behaviour instead of a latent trap,
and it fails loudly if anyone flips the option or edits either stepper.

## Validated

- `build/` (default, `VF2_LEGACY_STEP_IN_RUN=ON`): unchanged, 119/119, and
  `vf2probe` still reports `compare=equal` at `0x23524`.
- `build-archstep/` (`OFF`): builds warning-free, 19/119 fail, every one on
  `compare_result` only.
- Both trees print their semantics at configure time.

## Still open

- The BBT convention, as a research slice with its own evidence.
- The 18 container rows, the 93,748 B gap, the extent oracle, and everything in
  Phases 2-5 of the completion plan — none of which this touches.