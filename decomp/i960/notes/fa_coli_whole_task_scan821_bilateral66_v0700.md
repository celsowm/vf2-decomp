# `fa_coli` bilateral 6/6 admitted (v0700)

The last gap in the `field_0821` matrix is closed: bilateral (6,6) is
native with complete live-state equality, and the v0692 fail-closed
pin is removed (gate + test).

## Why the pin existed, and why it is gone

v0692 refused 6/6 at the shell admission gate because the first
0x22298 call takes the scan-6 ordering-fail tail while the recovered
child under-modeled it, and the then-native total (9531) collided with
the whitelisted (2,6)/(6,2) triple. The v0698 joint refit changed the
premises: scan-6 ordering-fail children are trace-exact (body 18,
region 20 — proven by the admitted (6,0)/(0,6)/(6,2)/(2,6) mixes), the
shell base is uniform 9409 with exactly one -1 per shape (F0-special
-1 for F0 scan 6 here), and the spurious +2 dispatch is gone for
820 == 0 shapes.

## Section-split measurement (this slice)

Parked snapshot + matrix recipe (both flags 0x100, scans 6/6,
804/822 zero, `0x005149cc = 0xffff`), whole-task `--trace` to
`0x00010dcc`: total 9528/18/19; 2298 regions 20/20 (ordering-fail
listings both calls: `cmpibe 6 -> 0x2233c`, `ld/ld/cmpr/bge` to the
0x223a8 `lda 0xffff` tail, 19 in-call incl ret); 2404 regions 31/31;
non-call 9488 — fully standard structure. Temporary native section
prints (reverted) on the pin-lifted run: children 18/18, 2404 29/29,
shell 9408, midbody rest identical to (2,6) (mbody 111 vs 114, delta
exactly the 21-vs-18 child bodies). Native total 9528/18/19 with
complete live-state equality.

## Recovery

- `vf2_hybrid_coli_23524_execute`: 6/6 admission-gate block removed.
- `tests/recovered/test_coli_whole_task_live.c`: the grid `continue`
  on (6,6) removed (all 49 pairs x 4 fields assert equality);
  `test_bilateral_66_unsupported` converted to the same equality
  proof; support predicate now admits all pairs.

## Proof

`vf2_coli_whole_task_live_differential` green with 6/6 in the grid;
all 10 `coli` tests + native runtime green; strict build clean. The
`field_0821` matrix (single + bilateral, 820 == 0) is now fully
native with no pins left.
