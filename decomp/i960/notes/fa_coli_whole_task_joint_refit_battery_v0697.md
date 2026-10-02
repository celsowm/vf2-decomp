# Joint-refit section battery (v0697, scouting only, no code change)

Parked snapshot `out/coli-parked-221e8.vf2snap`, `--until 0x00010dcc`
reference traces split at the two `call 0x22298` (0x22210/0x2221c)
and the two `call 0x22404` (0x22228/0x22238) sites. Region size =
call insn + in-call steps including `ret`.

## Measured reference section table

| shape | total | 2298 regions (call1/call2) | 2404 regions | non-4-call |
| --- | --- | --- | --- | --- |
| F0/F1 single, scan 0 | 9385 | 8 / 16 or 16 / 8 | 31 / 15 or 15 / 31 | 9361 |
| F0/F1 single, scan 2 | 9392 | 8 / 23 or 23 / 8 | 31 / 15 or 15 / 31 | 9361 |
| F0/F1 single, scan 5 | 9391 | 8 / 22 or 22 / 8 | 31 / 15 or 15 / 31 | 9361 |
| F0/F1 single, scan 6 | 9389 | 8 / 20 or 20 / 8 | 31 / 15 or 15 / 31 | 9361 |
| bilateral (0,0) | 9520 | 16 / 16 | 31 / 31 | 9488 |
| bilateral (2,0)/(0,2) | 9527 | 16 / 23 or 23 / 16 | 31 / 31 | 9488 |
| bilateral (0,5) | 9526 | 22 / 16 | 31 / 31 | 9488 |
| bilateral (6,6) | 9528 | 20 / 20 | 31 / 31 | 9488 |

Non-4-call steps are CONSTANT per live-mode: 9361 single,
9488 both. Every scan/side delta lives strictly inside the two
0x22298 call regions. Reference region sizes: warm(dead) 8,
scan-0/1/3/4 tail 16, scan-2 tail 23, scan-5 tail 22, scan-6
tail 20. Call 1 (0x22210) keys on the F1-side scan, call 2
(0x2221c, swapped) on the F0-side scan. Dispatch tails are
identical across all shapes (0x2223c/0x22284/0x22294).

## Corrected attribution (supersedes the v0696 F1s2 note's location)

The scan-0 tail that undercounts by 1 (13 vs 14 pre-ret) is the
v0351 fall-through at hybrid.c:23447 (bit-14-clear path), NOT the
v0497 bit-14-set body. Native call regions are exact everywhere
else (20/18/21 bodies give 22/20/23; warm 6 gives 8).

## Refit prescription (not yet applied)

1. 23447 body 13 -> 14 (trace-exact, 15 in-call incl ret).
2. Remove the single-live dispatch +1 (reference has no
   counterpart; tails identical). Both-live +2 needs its own
   trace proof before touching.
3. Re-derive shell gates from the section table: failing cells
   after (1)+(2) are exactly shapes whose gate fitted the old
   child/dispatch coupling (F0-live scan 2/5/6, F1-live scan 5/6
   at -1; bilateral +1 per plain-scan side).
4. Re-proof the full matrix, then flip the 6/6 and F1s2 pins only
   on measured equality.

Attempt log: a first try edited v0497 (wrong tail) and removed
the dispatch +1 globally, breaking 496 cells; both edits were
reverted. This note preserves the battery so the next attempt
starts from the table above.
