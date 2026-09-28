# v0383 — coli mid-body tail whole-tail live (first-hit-second-warm long)

**Status:** recovered C + ROM-backed differential green; committed.

Reopens `fa_coli_22404_live_v0382.md` §6 (whole-tail composition).

## 1. Witness

Whole mid-body from `out/coli-midbody-22210.vf2snap` + v0381 recipe
(`--set-ip 0x22210 --set-reg g7=0x510980 --set-reg g8=0x512980
--set-reg g13=0x514940 --set-u32 0x510b24=0x100 --set-u8 0x5111a0=0x01
--set-u32 0x5149cc=0xffff --until 0x10dcc`):
**371 steps** `0x22210 -> ret 0x10dcc` (body 370 + ret), **9 calls / 10 rets**.
Breakdown: `0x22298` warm 7 + hit 13, `0x22404` hit 78 + warm 13,
`0x225cc` long 240 (site-A, `0x230d4`+`0x23238`x2+`0x1ab34`).

Warm both-zero is **56/4/5** (no 225cc). Compact first-hit-second-warm
is **144/5/6** (bit3 set, 12 insns). Live long is **371/9/10**
(bit3 clear, scan 1).

## 2. Delta vs covered warm/compact

The v0304 branch (`r6 !=0`, first hit, second warm) previously hard-coded
compact (`130/5/6` in other tests, `144/5/6` for this fixture) with
`5,5` nested and no CC/G1 threading. Live reaches the long resolver
(bit3 clear) via the same tail, so the branch must forward the long
body's call/ret counts and CC/G1.

## 3. Native changes (`src/recovered/hybrid.c`, `vf2_hybrid_coli_midbody_tail_execute`)

- `g13` read moved after null check (ASan fix for unit test).
- Warm both-zero now correctly reports `4,4` nested (gives 5 total via
  `cpu_return` increment).
- v0304 branch now threads `c_calls/c_rets/c_cc/c_g1` from
  `coli_225cc_body`:
  * `total_calls =4+1+c_calls` (4 children + outer 225cc + inner long calls)
  * `body =16 + children + c225 +1 (+1 when long, for the resolver dispatch)`
  * `hybrid_set_compare_result` + `g1` (17) forwarded when present
  * `hybrid_complete(..., total_calls, total_calls)` (gives total+1 via
    the final `return` increment)
  Compact stays `5,5` nested → 6 total, long becomes `9,9` nested → 10 total.

## 4. Oracle fix (`src/i960/executor_arch.c` already in v0382)

The `bo`/`bno` after `scanbit` fix from v0382 is required for the
`0x22404` hit's scan loop; no further oracle change.

## 5. Tests

- `tests/recovered/test_native_runtime.c`: warm 56/4/5 and compact
  132/5/6 pins remain (verified, not assumed). The warm `4,4` vs `4,5`
  confusion was clarified: expected `4,4` nested gives 5 total.
- New ROM-backed `vf2_coli_midbody_tail_live` (+ `_differential`):
  loads the measured snapshot as base (to avoid zeroed-RAM divergence),
  applies the live recipe, 371-step lockstep + full live-state equality
  (registers, CC/AC, frames, work RAM, buffer triple). Uses the same
  `enter_procedure` convention both sides.

## 6. Validation

- Full `ctest` 68/68 (was 66/68, +2 new).
- Clang ASan/UBSan green on `vf2_coli_midbody_tail_live`,
  `vf2_native_runtime` (both modes), `vf2_coli_225cc_live`,
  `vf2_phase17_zero` etc.
- No ROM/snap/trace committed; scratch traces deleted.

## 7. Next (explicitly deferred)

Whole-task `0x221e8 -> 0x10dcc` composition (prefix 7 + shell `0x23524`
+ mid-body tail) is the next slice. The shell is already native
(`vf2_hybrid_coli_23524_execute`), but the task gate at
`hybrid_execute_coli_body` still pins `9214/18/19`, `9528/18/19`,
`9393/17/18` (warm and two non-warm siblings). Live whole-task would be
`9214-56+371=9529/21/22` (prefix+shell+live tail), but that triple is
not yet measured via a task-entry probe, so it stays fail-closed.
