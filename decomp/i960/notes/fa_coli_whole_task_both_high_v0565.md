# `fa_coli` whole-task both-bit-8 scan-5 shape (v0565)

The measured parked snapshot `out/coli-parked-221e8.vf2snap` was replayed
with both fighter flag words set to `0x100`, fighter0 `+0x821 = 5`,
fighter0 `+0x822 = 0`, and fighter0 `+0x804 = 0`. The reference reaches the
scheduler return `0x00010dcc` in `9526` instructions, with `18` procedure
calls and `19` returns.

The trace selects the `0x22298` second-loop path through `0x22338`; the
ordering comparison is not taken and the helper stores `0xffff` at the
second fighter's `+0x6dc`. The two `0x22404` contact queries then take the
empty-result exits. The preceding `0x238a4` scan has fighter0 byte `+0x820`
clear and no set-bit iterations; its `bbc r7,r3` semantics require testing
the table bit indexed by the `0x23284` byte.

Native recovery now admits only this measured both-bit-8/scan-5 shape in the
existing whole-task corridor. The fixture compares the complete live state,
including registers, condition state, local frames, procedure state, counters
and mutable Model 2A memory. The existing f0-only, f1-only and both-live
cases remain covered at `9393/17/18`, `9385/17/18` and `9528/18/19`.

Unmeasured `0x22298` flag/scan combinations, non-empty contact results and
the remaining `fa_coli` cascade/resolver branches stay fail-closed.
