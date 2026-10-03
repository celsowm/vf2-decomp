# 0x28918 curve/keyframe evaluator native via counter!=1 route (v0713)

## Result

The `0x28918` 60-vertex keyframe evaluator now runs natively through
the `0x28270` counter!=1 route, proven by a committed 4-row live
differential (`test_player_28918_live.c`, ref == native step/call/ret
lockstep + full live-state equality at `0x28274`):

| row | fighter/table | counter | ref steps | coverage |
| --- | --- | --- | --- | --- |
| F0 | `0x510980`/`0x520000` | 2 | 1088 | copy/zero/skip/strict-lerp |
| F1 | `0x512980`/`0x5207c8` | 2 | 1920 | 27 lerp verts |
| F0 | same | 0 | 1058 | small-key lerps |
| F0 | same | 30 | 1060 | 2 exact-key hits |

Fixture: `out/player-14288-natres.vf2snap` + counter mutation, zeroed
regs except sp/fp/g7/g11 (VRAM scratch `0x1008000`)/g12, from `0x27d00`
to `0x28274`. Live routing fields already match the S0 bit20-clear
curve==0 shape (mode `0x80004400`, sense 0, F0 edge `0x78`).

## Measured shape (75 blocks, `0x28918`-`0x28cd8`)

`cvtir` counter->float; per-vertex mode bytes at `table+0x78c+i`:
mode 4 copies `(g2)->(g5)`; mode 3 zeroes `(g5)` (with trailing `b`);
modes 0/1/2 skip (`g2+=4`, trailing `b`); mode 6 walks keyframes
comparing float curve vs keyframe words as UNSIGNED BIT PATTERNS
(`cmpoble`, executor-verified) and lerps with float `subr`s, staging
7 words through the loop-invariant `(g11)[g12]` scratch word (all hit
one word; reload passes the last to `(g5)`); exact float equality
takes the 3-step copy. Epilogue: rewind 240, 36x `cvtri`/`stis` in
place, rewind 144, fighter-word0-bit6-clear `ret`.

## Gaps closed / rules found

1. **`(g11)[g12]` is a loop-invariant swap temp** (write-before-read,
   same address all 60 iterations): any writable scratch is faithful
   (probe-verified at `0x1003000`/`0x1008000`).
2. **Counter!=1 skips the status test**: `cmpobne` jumps direct to
   `0x28270`; `+0x0bdd`/bit0 unread on this route (v0711's bit0
   refusal belongs to the counter==1 fallthrough).
3. **End register forensics**: `g4`=`table+0x78c+60`, `g5` arithmetically
   closed (`+240-240+144-144`), `g0`/`g2`/`g3` deltas cross-check the
   5-lerp count (`+5`, `+300=12x25`, `+100=4x25`), `g6`=`2.0`,
   `r2`=`0x28274` (call side effect), end CC=EQUAL/AC-010 pinned
   (mechanism open: tail `bbc` taken yet EQUAL).
4. **Step audit by trace census**: copy 10, zero 12, skip 11,
   strict-lerp 49 (incl. trailing `b`s and the `lda` marker),
   epilogue 6/iter, entry 7. The audit caught 4 counting slips
   (missing `b`s, one 8-byte `ld`, an inverted float compare caught
   by a 30-step shortfall).

## Still open (next, all fail-closed)

- Mode-5 loop (`0x28a04`): no witness in any of 1082 snapshots' tables.
- Exhausted keyframe walks (`0x2896c`): zero hits in five live runs.
- Modes above 6; fighter bit6 set (`0x28af8` stores/`ldt`/`stt`).
- The `0x281b0` float-entry and `0x28208` integer-table heads (both
  join at `0x2891c` with float `g6`); committed v0711 curve!=0 row
  still refuses there. Synthetic zeroed fixtures still refuse at the
  null-table check (committed frontier pins unchanged).
- The post-call `0x28274` continuation (runtime chain falls through
  to the exact interpreter there).
