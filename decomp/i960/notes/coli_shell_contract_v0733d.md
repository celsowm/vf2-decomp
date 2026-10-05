# v0733d: the 0x23524 shell differential — the g3 question was void, and three real gaps are now named

**`hybrid.c` carried a standing TODO in the `0x23524` shell: "whether 0x23524
should publish the g3 its children left behind is a separate question that needs
a shell differential to answer."** This slice built that differential. The
question turned out to have no content, and the differential found three
divergences nobody had named.

## The shell never had a child's g3 to publish

Grepping every `g3` mention across `0x23524..0x23874` (the shell's real extent)
gives two reads and no writes:

```text
000235b8  mov  g3, r11      <- read, after call 0x238a4
000235c8  mov  g3, r12      <- read, after call 0x238a4
00023888  bbc  r4, g3, ...  <- 0x23878, the NEXT function
0002389c  mov  r6, g3
000238a8  mov  0, g3        <- 0x238a4, a callee: the only write
```

`0x238a4` opens with `mov 0, r3` / `mov 0, g3` unconditionally, so **every
admitted path has g3 = 0 before it is read at `0x235b8`/`0x235c8`**, whatever
`0x2396c` left behind. The children *do* set it — measured mid-shell at
`0x2359c`: `g3 = 0x0000fffe`, `g4 = 0xffffffff`, exactly the v0732r body
contract, live inside the shell. The shell then discards both. g4 is reloaded
from the FIFO by the ROM's own `ld (g11)[g12], g4` at `0x23600`, so the `~g4` is
dropped by the ROM, not by the C.

So `cpu->registers[g3] = 0` at `hybrid.c:31308` was never a decision; it was
correct, and the "should we propagate?" question had nothing to propagate.

## The one leg where that is false is refused, and the refusal is load-bearing

`0x235a0 bbc 0, g6, 0x235ac`: when the flag builder `0x233d0` returns g6 with
bit 0 set, the shell takes `call 0x2364c` / `b 0x23648` and **never calls
`0x238a4`**. Nothing re-zeros g3, and nothing reloads g4 from the FIFO.

One bit reaches it: `0x233fc bbc 18, r9, 0x23408` → `0x23400 setbit 0, g6, g6`,
where `r9 = fighter0.flags | fighter1.flags` at `+0x1a4`. So setting **bit 18 of
fighter0's `+0x1a4`** is enough.

| | insn | calls | rets | cc | g3 | g4 | g6 | g14 |
|---|---|---|---|---|---|---|---|---|
| A warm (admitted) | 9151 | 13 | 14 | 2 | `0x00000000` | `0x00000000` | 0 | `0x23648` |
| B live f0 (admitted) | 9300 | 12 | 13 | 2 | `0x00000000` | `0xffffdffc` | 2 | `0x23648` |
| C f0 `+0x1a4` bit 18 | 6193 | 10 | 11 | 2 | `0x0000fffe` | `0xffffffff` | 1 | `0x22428` |

Leg C is 2958 instructions and 3 calls shorter, and carries the two 0x2396c
leftovers to the exit. **A native shell that admitted it would publish
`g3 = 0` and a FIFO `g4` while the reference has `0x0000fffe` / `0xffffffff`** —
it would fail while appearing to succeed. The refusal at `hybrid.c:30814` is
therefore load-bearing, not caution, and the new test asserts the reference
registers *first* so a refusal that stopped being necessary would fail the test
rather than quietly pass it.

## The boundary is not where `vf2i960 function` says

`vf2i960 function roms/vf2 0x23524` reports `end=0x2364c`. That is `0x235a4`'s
**callee**. The shell's real tail is

```text
00023644  bal  0x00023694      <- leaves g14 = 0x23648
...
00023874  bx   (g14)           <- jumps to 0x23648
00023648  ret                  <- finally pops the caller's frame
```

so `bx (g14)` is a trampoline, and the procedure's last instruction is the
`ret` at `0x23648`. The natural boundary is `0x22210`, the caller's return
address, reached in **9151** instructions with 14 returns.

A first draft of the test stopped at `0x23648` and reported **"native 9151 !==
reference 9150"** — a fabricated defect in a recovery that was right. v0386's
whole-task `9151` pin was correct all along. Same first-ret-lower-bound trap
v0733c recorded for `functions.csv`, now found a second time.

## Three divergences, all measured, none pinnable

Admitted legs A and B match the native on instruction count, call/return
counts, `g3`, `g4`, `g6`, every memory region, and every other register. They
differ in exactly three fields, and the test pins that set:

| field | reference | native | pinnable? |
|---|---|---|---|
| `g14` | `0x23648` | `0x22428` | **no** — path-dependent. Legs A and B execute the `bal` at `0x23644`; leg C branches to `0x23648` at `0x235a8` and keeps the entry value `0x22428`. |
| `compare_result` | 2 (EQUAL) | 0 (NONE) | **no** — see below. |
| `arithmetic_control` | `0x3f001002` | `0x3f001000` | **no** — bit 1, the executor's "a compare has been executed" flag, kept in lockstep with `compare_result`. |

`compare_result` is the reason no C was changed. **It is measurably dependent on
the state inherited at the entry.** Over the identical 6193 instructions of leg
C:

- entered with `cc = NONE` (from the committed `coli-parked-221e8` snapshot, 7
  real steps) → exits **EQUAL**
- entered with `cc = EQUAL` (from a `vf2probe --output-snapshot` of the same
  point) → exits **LESS**

A value that is not a function of the shell alone cannot be pinned, and pinning
either observed constant would be a guess dressed as a recovery. Worse, a
plausible-looking rule was already falsified while building this: the obvious
candidate is the shell's last compare, `bbc 31, r15, 0x23870`, but
`compare_result` is **EQUAL for every value of fighter `+0x650`** tried
(`0x00000000`, `0x7fffffff`, `0x40000000`, `0x3cf5c28f`, `0xffffffff`, on both
fighters). So that hypothesis is wrong too, and the rule is still unknown.

This also means the two harnesses disagree for a reason worth recording: a
`vf2probe --output-snapshot` and a `vf2_i960_step` walk to the *same address* do
**not** agree on `compare_result`. The snapshot format does persist the field
(`snapshot.c:266/344/357`), so the discrepancy is in what each harness has
executed by that point, not in serialisation. **Trust the test, which executes
from the committed fixture; treat `compare_result` read off a probe-produced
snapshot as unverified.**

## The test can fail

A planted defect — publishing `g3 = 0x0000fffe` instead of `0` — makes both
admitted legs report `2 register difference(s), expected exactly 1 at g14`, and
leg C keeps refusing. So the gate detects precisely the mistake this slice is
about: a widened gate, or a child-value propagation that should not happen.

## Standing rules this adds

- **A refusal must be justified by a measurement, and the measurement belongs in
  the test.** "Conservative" is not a reason; `g3 = 0x0000fffe` surviving to the
  exit is.
- **Run the neighbouring leg before pinning anything.** cc is EQUAL on the warm
  leg and the naive constant is wrong on the live one; g14 agrees on A and B and
  is wrong on C. One leg would have produced two plausible, wrong pins.
- **Check the harness agrees with itself.** The first `vf2i960 function` defect
  in this repo was a fabricated +1, and the first `cc` conflict was two correct
  harnesses with different entry states. Both looked like recovery bugs.
- **An unrecovered field should be named in a test, not left implicit.** Three
  fields are now pinned-as-divergent; if someone recovers one, the test tells
  them this file is stale.

## Validated

- `vf2_coli_23524_live` (ROM-independent) and `vf2_coli_23524_live_differential`
  (3 legs) pass.
- `ctest -R vf2_coli`: 14/14.
- Full non-dominator suite: see the commit message.

## Still open

- `g14`, `compare_result`, `arithmetic_control` at the shell's return. The rule
  for `compare_result` is unknown and the obvious one is falsified; it needs a
  real model of which instruction sets the compare word on each path, not
  another sample.
- `vf2probe --output-snapshot` vs `vf2_i960_step` disagreeing on `compare_result`
  at the same address. Undiagnosed; it makes probe-read cc values suspect.
- The `+0x110`/`+0x114` setbit sides, the `0x281b0`/`0x28208` join at `0x2891c`,
  the mode-5 loop at `0x28a04` and the fighter bit-6 test at `0x28af8` are all
  untouched by this slice.
