# v0403: fa_rob 0x1453c arm scoped, tail blocked on type-5 record (measurement only)

## Verdict

No C recovery in this slice. The `0x1453c` arm head is 3 steps
(`mov 16, r15`; `stib r15, +0x197(g7)`; `b 0x14570`), but the
`0x14570` tail cannot complete from the live park shape: it calls
`0x1ab34` with `g0 = (u16)(+0x194(g7))` and `g1 = 5`, and the parked
fighter's `+0x194` low half is 0.

## Measured probes

```sh
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-ip 0x14528 --set-reg g7=0x510980 --set-reg g8=0x512980 \
  --set-reg r7=27 --set-reg r8=0 --set-reg r10=0x510980 \
  --set-reg r11=0x512980 --until 0x1463c
# -> out of bounds, memory fault at ip 109388 (0x1ab34+24 = 0x1ab4c
#    ldob (g0), r3), after 11 steps
```

Disassembly of the walker (`0x1ab34`):

```text
0x1ab34 lda 0x1fff, r13
0x1ab3c and r13, g0, r3
0x1ab40 ld 0x0200d34c[r3*4], g0
0x1ab48 addo 8, g0, g0
0x1ab4c ldob (g0), r3      <- faults when table[index] == 0 (g0 == 8)
```

With `+0x194` low half 0, index is 0 and `table[0]` is 0, so `g0 == 8`
is unmapped. A walker miss returning `g0 == 0` would then fault at
`0x1457c ldos 0x01(g0)`, so the tail needs a `+0x194` whose low 13
bits index a chain containing a type-5 record (g1 == 5 match returns
`g0 = record+8`). No such live fighter shape is measured yet.

The same block applies to the f1 == 27 swap path (`0x14530`) and the
`0x14548..0x1456c` variants that join the tail: all stay fail-closed.

## Next

Mine a valid type-5 index from measured `+0x194` values (e.g. sweep
the low 13 bits against the `0x0200d34c` table for a chain containing
type 5, following `fa_coli_1ab34_v0312.md`), then re-probe the tail
from a park carrying that `+0x194` before writing any C.
