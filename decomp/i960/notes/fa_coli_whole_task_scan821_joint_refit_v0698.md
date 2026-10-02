# `fa_coli` scan-821 joint refit landed (v0698)

The v0696/v0697 joint refit is implemented, measured, and proven: the
scan-0 fall-through child is trace-exact, both dispatch compensations
are removed for the measured `field_0820 == 0` matrix, every shell gate
is re-derived from section-split reference data, the F1-single-2 pin is
admitted, and 6/6 stays refused. No code change in this note beyond what
`src/recovered/hybrid.c` already carries; this records the evidence
chain and the one discovered follow-up slice.

## Child fix (step 1)

`coli_22298_body` v0351 fall-through (`hybrid.c`, was line 23444):
body 13 -> 14. Trace basis: the v0696 6/0 call-1 listing
(`fa_coli_whole_task_scan821_scan0_child_decomp_v0696.md`) measures 14
pre-`ret` insns (`bbc14` taken, `ldos`, `cmpobne`, `bbs14`, `cmpibe 6`,
`cmpibe 5`, `cmpibne 2` taken, `stos`), 15 in-call including `ret`,
matching the v0697 battery region 16. The v0497 bit-14-set body stays
13 (different tail). ROM-independent unit pin
(`test_native_runtime.c`, v0351 live sibling) updated 14 -> 15 total.

## Dispatch removal (step 2)

Single-live +1 removed outright: the v0697 battery measures the
single-live non-call section scan-independent at 9361 with dispatch
tails identical across shapes, so the reference runs no extra
single-live dispatch compare. Both-live +2 removed for the measured
`field_0820 == 0` matrix (non-call 9488, same argument) but KEPT,
narrowly re-scoped to `field_0820 != 0` on either side: the test_one
`both` pin (F0_820 = 1, scans 0/0, reference 9528/18/19) runs a
different 0x22404/shell structure the matrix battery does not cover
(native 2404a body 35 shape, see below) and was proven exact with the
+2. Narrower than the old unconditional +2, so fail-closed-er; the
820 != 0 world keeps status quo ante pending its own slice.

## Gate re-derivation (step 3, from the measured failing set)

After steps 1+2 the full-matrix run failed exactly the predicted
coupling residue: singles F0s2/F0s5/F0s6/F1s5/F1s6 at -1, bilaterals
+1 per plain-scan side (+2 for plain/plain), double-specials exact.
Temporary native section prints (reverted) gave the decomposition:

- Native 0x22298 regions trace-exact everywhere (16/23/22/20/8).
- Native shell body uniform per live-mode once gated: 9408 bilateral,
  9297 single (F0s5 single 9298 + its midbody -1, fixed below).
- Reference invariant (v0697 battery): everything-except-2298-calls
  scan-independent (9361 single / 9488 both).

Resulting gate changes, each validated by the full differential:

- Bilateral F0 adjust extended to scans {0,1,3,4,5} (was {0,1,3,4}
  plus a dead (0,5) else-if, removed): every 820=0 bilateral needs
  exactly one -1 against the uniform 9409 shell base. Fixes
  (5,2)/(5,5)/(5,6), keeps the rest.
- Bilateral F1-special -1 removed (pure child/dispatch compensation).
- F0-special -2 -> -1, single and bilateral (keeps the real shell
  correction, drops the compensation).
- Single-live F1 scan-5/6 adjust removed; F1-single-2 pin removed
  (admitted, lands 9392/17/18 exact).
- Single-live F0 scan-5 adjust RESTORED in the shell (composition
  fix) and the midbody F0s5 -1 removed (net zero on totals): the
  battery measures single-live 0x22404 regions scan-identical
  (31/15 for scans 0 and 5), refuting the "alternate 0x22404 path"
  comment, and reference shells are measured identical, so the -1
  belongs in the shell like every other single shape.

## Midbody long-resolver +1 removed (v0383 shape)

`vf2_coli_midbody_tail_live_differential` went 371 -> 372 native.
Reference section split of the midbody-park shape (820=1 single,
`coli-midbody-22210.vf2snap` + recipe): call1 warm (region 8), call2
quick tail (region 16, 14 pre-ret), 2404 regions 79/15, then the
g0=1 -> 0x225cc long path. All native children/2404 exact, exposing a
pre-existing +1 rest overcount previously masked by the child -1: the
`+1 when c_calls != 0` ("cmpobe/bbs dispatch") has no trace basis —
the measured dispatch is cmpobe/cmpobe/mov/mov (recs 125-128), already
covered by the 16 prefix. Removed; compact (c_calls == 0) shapes never
took it, 225cc-skipping whole-task shapes unaffected. Back to
371/9/10 with live-state equality.

## Deliberately out of scope: the `field_0820 != 0` world

Probed snapshot defaults are all zero, so the matrix never covers
820 != 0. Measured native behavior there differs structurally
(2404a body 35/40 shapes in test_one singles; `both`-mode native
2404/region composition differs from matrix (0,0) at identical child
inputs). Those pins keep their old accounting (single +0 path
unchanged and still exact; both-live +2 preserved iff 820 != 0).
Follow-up slice: 820=1 section battery (reference splits for the
test_one f0/both shapes) to decompose that world's 2404/shell
structure the same way.

## Proof (this checkout, ROM-backed)

- `vf2_coli_whole_task_live_differential`: full matrix (502+ cells incl.
  bilateral grid minus 6/6, inactive-scan matrix with F1s2 admitted,
  all test_one modes) exact counts/calls/rets + live-state equality;
  6/6 still `VF2_ERROR_UNSUPPORTED` with reference-exact 9528 pin.
- All 10 `coli` tests, `vf2_native_runtime(+_state)`, 15
  dispatch/scheduler/task-recovery tests: green.
- Strict build `VF2_WARNINGS_AS_ERRORS=ON`: clean.
