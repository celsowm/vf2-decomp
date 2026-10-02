# `fa_coli` whole-task bilateral 6/6 — measured fail-closed attribution (v0692)

The bilateral `field_0821 = 6/6` shape left open in v0691 is now
decomposed by full instruction-trace diff against the admitted 6/0
bilateral neighbor. The shape stays **fail-closed**: admitting it would
require either a differential shell correction on trace-identical
streams (curve-fitting) or a joint refit of the scan-0 child body and
every admitted scan-0 shape correction (broad speculative rewrite).
Both are rejected; the boundary is now pinned by a ROM-backed
fail-closed test instead.

## Measured reference (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at `0x00010dcc`:

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/coli-parked-221e8.vf2snap \
  --set-u32 0x00510b24=0x100 --set-u32 0x00512b24=0x100 \
  --set-u32 0x00510d84=0 --set-u8 0x005111a1=6 --set-u16 0x005111a2=<0|1|16|256> \
  --set-u32 0x00512d84=0 --set-u8 0x005131a1=6 --set-u16 0x005131a2=0 \
  --set-u32 0x005149cc=0xffff --until 0x00010dcc
```

| `field_0822` (F0) | reference |
| --- | --- |
| 0 | **9528 / 18 / 19** |
| 1 | **9528 / 18 / 19** |
| 16 | **9528 / 18 / 19** |
| 256 | **9528 / 18 / 19** |

The shape is field-independent over the same sweep axis the v0683/v0684
matrices cover, exactly as the recovered scan-6 shapes are. Final-state
reads confirm the only state delta versus the admitted 6/0 bilateral
(`9524/18/19`, `fighter0+0x6dc == 0`):

- `fighter0+0x6dc = 0xffff`, `fighter1+0x6dc = 0xffff` (6/0 has `0` / `0xffff`);
- `fighter0+0x61c = 0` (the `half_61c == 0` gate holds);
- `fighter0+0x1f8 = 0`, `fighter0+0x6e4 = 0` (zero ordering: the `cmpr` at
  `0x22344` takes `bge` to the `0x223a8` fail tail).

## Trace attribution

Full `--trace` runs of bilateral 6/6 (9528 steps) and bilateral 6/0
(9524 steps) were diffed instruction by instruction. Exactly two
divergent regions exist, both inside the `0x22298` calls; every other
instruction — shell, contacts, tail — is identical:

- Call 1 (from `0x22210`, `g7` = fighter 0, `r6` = fighter-1 scan):
  6/6 takes `0x2232c cmpibe 6 -> 0x2233c`, then `ld/ld/cmpr/bge` to the
  `0x223a8` ordering-fail tail (`stos 0xffff`, span **20**). 6/0 falls
  through `cmpibe 6/5`, takes `cmpibne 2 -> 0x223b4`, stores `0` (span
  **16**). Delta **+4**, the whole-task delta.
- Call 2 (from `0x2221c`, `g7` = fighter 1, `r6` = fighter-0 scan 6):
  identical 20-step ordering-fail spans in both shapes.

So `ref(6/6) - ref(6/0) = +4` lives entirely in call 1, and the shell
outside the calls is instruction-identical between the shapes.

## Native decomposition of the residual

Native children are exact for both call spans (ordering-fail scan-6
body 18 = span 20 minus call/ret, trace-proven in v0691), and native
stores match the reference (`0xffff` on the scan-6 path, `0` on the
scan-0 path). The count residual is:

- children: body 18 (scan-6 fail) vs body 13 (scan-0 quick tail) = **+5**,
  while the reference spans differ by **+4** (20 vs 16);
- shell: the bilateral `-2` correction gate
  (`vf2_hybrid_coli_23524_execute`, `scan_821 == 2 || 6`) requires
  `scan_821_other == 0`, so it fires for 6/0 but not 6/6 = **+2**;
- the `hybrid_execute_coli_body` bilateral `+2` fires for both (F0 scan
  6 in both) and cancels, as do the contact/tail counts.

Native delta is therefore `+5 + 2 = +7` against a true `+4`: native
reaches **9531**, i.e. reference `+3`. The `+3` is exactly the v0691
residual, now attributed: **+2** shell-gate scope plus **+1**
scan-0-child-relative gap (native child delta 5 vs reference span
delta 4; the scan-6 body is trace-exact, so the scan-0 body 13 counts
one less than the 16-step reference span admits).

## Why it stays fail-closed

- The shell streams are proven instruction-identical, so any shell
  correction that differs between 6/6 and 6/0 (e.g. `-3` for 6/6 vs
  `-2` for 6/0) would be curve-fitting with no trace basis.
- Raising the scan-0 child body 13 to 14 would cascade into every
  admitted scan-0 shape and its jointly fitted shell corrections
  (single-live `-1`, bilateral `-2`): a broad refit, not the smallest
  proven change.
- The 9531 triple is not whitelisted, so
  `vf2_hybrid_first_dispatch_task_execute` already returns
  `VF2_ERROR_UNSUPPORTED` for this composition.

Resolving the `+1` needs an independent trace-proven decomposition of
the scan-0 quick-tail child against the shell base — for example,
proving a scan-0 shape's shell stream identical to a shape with a
known-exact shell, isolating the child count. That is future work; no
code change is committed here.

## Proof

`tests/recovered/test_coli_whole_task_live.c` drives all four
`field_0822` cells through `test_bilateral_66_unsupported`: the
reference re-measures `9528/18/19` on every run and the native path is
asserted `VF2_ERROR_UNSUPPORTED`. The admitted scan-6 matrix is
unchanged and still exact.
