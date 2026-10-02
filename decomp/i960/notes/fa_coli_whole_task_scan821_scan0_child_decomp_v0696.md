# `fa_coli` scan-0 quick-tail child decomposition — measured +1 gap (v0696)

The v0692 `+1` scan-0-child-relative gap now has an independent
trace-proven decomposition against the shell base. No code change is
committed here: the composition stays fail-closed and every admitted
shape is unchanged.

## Method (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at
`0x00010dcc` (both fighter bit-8 flags set, `field_0804 = 0`,
`field_0820 = 0`, `0x005149cc = 0xffff`):

- 6/0: `--set-u8 0x005111a1=6 --set-u8 0x005131a1=0` → **9524** steps.
- 6/6: `--set-u8 0x005111a1=6 --set-u8 0x005131a1=6` → **9528** steps.

Full `--trace` runs were split at the two `call 0x22298` instructions
(from `0x22210` and `0x2221c`):

- call 1 region (call entry to second call): 6/0 = 17 steps, 6/6 = 21.
- call 2 region: 19 steps in **both** shapes, instruction-identical.

## Call-1 instruction listing (measured)

6/0 (scan-0 quick tail, 15 in-call steps including `ret`):

```text
0x22298 mov      0, r11
0x2229c ld       0x1a4(g7), r7
0x222a0 ld       0x1a4(g8), r8
0x222a4 ldob     0x821(g8), r6
0x222a8 bbc      8, r8, 0x223b4      (not taken: F1 bit 8 set)
0x222ac bbs      1, r8, 0x223b4      (not taken: F1 bit 1 clear)
0x222b0 bbc      14, r7, 0x22320     (taken: F0 bit 14 clear)
0x22320 ldos     0x61c(g7), r14
0x22324 cmpobne  0, r14, 0x223b4     (not taken: F0+0x61c == 0)
0x22328 bbs      14, r7, 0x223b4     (not taken)
0x2232c cmpibe   6, r6, 0x2233c      (not taken: F1 scan 0)
0x22330 cmpibe   5, r6, 0x22338      (not taken)
0x22334 cmpibne  2, r6, 0x223b4      (taken)
0x223b4 stos     r11, 0x6dc(g7)      (stores 0)
0x223b8 ret
```

6/6 takes `cmpibe 6 -> 0x2233c`, then `ld/ld/cmpr/bge` to the
`0x223a8` ordering-fail tail (`lda 0xffff`, `stos`), for 19 in-call
steps including `ret`. The disassembly (`vf2i960 disasm roms/vf2
0x22298` / `0x22368`) confirms every edge above statically.

## Accounting comparison

The call site adds `child body + 1` for the child `ret`
(`hybrid.c`, `body += child + UINT64_C(1)`):

| path | reference pre-`ret` insns | native body | native call total | reference call total (excl. `call`) |
| --- | --- | --- | --- | --- |
| scan-0 quick tail | **14** | 13 (v0497 zero-mask tail) | 14 | 15 |
| scan-6 fail tail | **18** | 18 (v0691, trace-exact) | 19 | 19 |

The scan-0 quick-tail native body undercounts its own reference path
by exactly one instruction; the scan-6 body is exact. This is the
v0692 `+1` (native child delta 5 vs reference span delta 4), now
isolated from the `+2` shell-gate scope component: no shell stream
differs between the shapes outside these two call regions.

## Why no code change yet

Raising the v0497 body 13 to 14 shifts every admitted shape whose
call takes that tail (all bilateral scan-0 compositions plus the
single-live shapes that share it), invalidating the jointly fitted
shell corrections (`-1`/`-2` gates) and the v0694 additive-rule
whitelist. That joint refit — re-measure the full matrix, refit
gates, re-prove — is the correct vehicle, not a one-constant edit
in this slice. Bilateral 6/6 remains refused at the admission gate
with reference-exact counts.
