# v0732d: INDIVIDUAL mode is reachable, and its walk differs - F3/F4 evidence

**Evidence only. No behaviour change, no tuple admitted.** This is the last
F3 item ("KICK -1 from INDIVIDUAL mode") plus a concrete F4 finding, measured
after v0732c.

## There is no INDIVIDUAL artifact, and the names lie

Every `*-ind*` snapshot in `out/` - `entry-a5-ind`, `entry-a5-ind2`,
`native-a5-ind`, `native-a5-ind2`, `ref-a5-ind`, `v-ind`, `f1-ind` - reads
`coin_flags = 0x00000000` at the `0x9ff8` boundary. That is **COMMON** mode;
bit 0 clear is the COMMON filter, not INDIVIDUAL. `f1-ind` and `f1-common` are
byte-identical in the state that matters. The INDIVIDUAL leg had to be built
from scratch.

## Building INDIVIDUAL mode

The toggle is the `a5 = 1` PUNCH edit, and it is reachable by walking up one
row from the row-2 idle. No state was patched:

```sh
# f2r2-c has already consumed its nav (it walked 3 -> 2); settle to a clean
# a5=2 idle at the dispatch boundary
vf2probe --snapshot out/f2r2-c.vf2snap --until 0x7b1c --output-snapshot out/ind-a-end.vf2snap
vf2probe --snapshot out/ind-a-end.vf2snap --until 0x10fa0 --output-snapshot out/ind-a-w.vf2snap
vf2probe --snapshot out/ind-a-w.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x9ff8 --output-snapshot out/ind-a-c.vf2snap      # a5=2, COMMON

# nav up to row 1
vf2probe --snapshot out/ind-a-c.vf2snap --set-u32 0x500700=0x0f002000 \
         --set-u32 0x500704=0x2000 --until 0x7b1c --output-snapshot out/ind-b-end.vf2snap
# ... settle + IRQ -> out/ind-b-c.vf2snap                            # a5=1

# PUNCH at row 1: coin_flags ^= 1.  Frame is 4637, ends at flags = 0x1.
vf2probe --snapshot out/ind-b-c.vf2snap --set-u32 0x500700=0x0f000004 \
         --set-u32 0x500704=0x4 --until 0x7b1c --output-snapshot out/ind-c-end.vf2snap
# ... settle + IRQ -> out/ind-c-c.vf2snap                            # a5=1, INDIVIDUAL idle
```

## F4 finding: the INDIVIDUAL walk is not the COMMON walk

From the INDIVIDUAL idle at row 1, one nav-down reaches `a5 = 2` with
`coin_flags = 0x1` as expected. A **second** nav-down does not go to row 3:

```text
step 1   a5=2   flags=0x00000001
step 2   a5=0   flags=0x00000000
```

Row 2's down-neighbour in INDIVIDUAL mode is **row 0**, and the step also
**leaves INDIVIDUAL mode** (`coin_flags` returns to 0). So rows 3 and 4 are
not reachable by walking while in INDIVIDUAL mode, and the selection list
wraps early. That is the F4 shape difference in one measurement, and it means
the F3 item "KICK -1 from INDIVIDUAL mode" has no row-3 or row-4 instance at
all - only row 2, and possibly row 1.

## F3's last item, measured at row 2

KICK at row 2 while in INDIVIDUAL mode:

| leg | mode | body | ins | calls |
|---|---|---|---|---|
| row-2 KICK edit | COMMON | 4638 | 4638 | - |
| row-2 KICK edit | **INDIVIDUAL** | 4506 | 4506 | - |
| row-2 KICK release | COMMON | 4189 | 4421 | 41 |
| row-2 KICK release | **INDIVIDUAL** | 4061 | **4293** | **38** |

`credits = [1,2,1,2]` in both - the mode does not change the derivation, only
the rendering. The release is a **fourth distinct shape**: the COMMON value-row
release is 4189/41, the existing INDIVIDUAL branch carries 4060/32, and this
measures 4061/38.

**The native correctly refuses it** - `unsupported operation at 0x0000a6c0`,
before the block is even entered. Nothing is silently accepted.

## Why this is not recovered here

One sample. To pin 4061/38 I would need the PUNCH counterpart, and the
singular-count question is open: the credits here give exactly one singular
rendered value (`credits[2] = 1`), and the existing INDIVIDUAL 4060/32 figure
was measured on **row 1** after the COMMON->INDIVIDUAL toggle, which is a
different row with a different render. One point cannot tell whether the
singular subtraction applies, what the base is, or whether 38 calls is the
right call count for every INDIVIDUAL release.

The INDIVIDUAL render also differs structurally - the mode erases rows 24-33
and drops the chute section - so the digit cells and the `runs[]` filter both
need re-deriving against a reference trace before any count is pinned. That is
a full slice, not a follow-up line.

## What is still fail-closed

- INDIVIDUAL post-edit release at a value row: refused.
- INDIVIDUAL `a5 = 4` release: refused explicitly by the gate.
- `preset >= 3` on the `a5 = 4` release: refused.
- The edit path's 4506 / 4625 / 4634 / 4636 variations: refused.
- The 7-instruction `a5 = 4` delta between `preset == 0` and `preset >= 1`:
  pinned as measured, cause unknown.
