# v0732c part 2: row 24 shows the SELECTED chute, and the a5=4 body count

Extends `f3_row6_credit1_correction_v0732.md`. Two more corrections to v0732,
both found by constructing the row-4 **KICK** release - a state that had never
been measured.

## Correction 1: the chute slot is `0x61550 + 2 * preset`, not slot 2

v0732 pinned row 24 to table slot 2 unconditionally. That is correct at
`preset == 1` and wrong everywhere else. The ROM's `ldob` walk settles it -
one group per five rows, each reading its value slot then four terminator
reads at the odd slot:

```text
preset 0   0x61550, 0x61551 x4,  0x61550, 0x61551 x4
preset 1   0x61550, 0x61551 x4,  0x61552, 0x61553 x4
preset 2   0x61550, 0x61551 x4,  0x61554, 0x61553 x4
```

The first group (rows 13, 15, 17, 19, 21) always reads `0x61550`. The second
group (rows 24, 26, 28, 30, 32) reads `0x61550 + 2 * preset`. So **row 13
shows chute 1 and row 24 shows the selected chute**, which is why the two
groups coincide at `preset == 0`.

Measured directly: at `preset = 0` the reference renders `(24,39)` as `'1'`
with the singular blank, i.e. bank 0's `0x11`, not bank 1's `0x12`.

## Correction 2: the a5=4 body is 4193 at preset 0, 4186 at preset >= 1

| snapshot | a5 | preset | latch | reference | body |
|---|---|---|---|---|---|
| `f2-r4-cl` | 4 | 0 | idle | 4425 | 4193 |
| `f2r4-k-rel` | 4 | 0 | post-edit release | 4425 | 4193 |
| `f2-r4` | 4 | 1 | post-edit release | 4418 | 4186 |
| `f2r4-a-c` | 4 | 1 | release latch | 4418 | 4186 |
| `f2r4-p2-rel` | 4 | 2 | post-edit release | 4418 | 4186 |

4193 is the same value the shared non-release `a5 = 4` branch already uses, so
at `preset == 0` the post-edit release and the idle frame take the same body.

**The 7-instruction delta is NOT the chute render.** The render difference
between `preset = 0` and `preset >= 1` is exactly one tile write - the second
blank at `(24,47)`, 417 writes against 416, and row 24 is the only row whose
count changes:

```text
f2-r4-rel   (preset 1)  total 416  ... row 24: 33
f2r4-k-rel  (preset 0)  total 417  ... row 24: 34
```

One extra write cannot account for seven instructions, and the direction is
wrong for the singular-label rule anyway (more singulars take the *shorter*
handler). So the cause is still unknown. Both values are pinned as measured
with that stated in the code comment; neither is presented as derived.

The singular-label subtraction is **not** applied on the `a5 = 4` path. The
chute pair always carries its own singulars - bank 0 is `0x11`, so 4193 is
already measured with two of them present - and no measured row separates the
credit cells' singulars from the chute's on that path.

## The gate moved from `preset <= 1` to `preset <= 2`

`preset = 2` is now measured, so it is admitted. Beyond 2 the count is
extrapolated and the chute number would still be a single digit, but nothing
proves it, so it fails closed. The INDIVIDUAL variant of this row is still
refused outright.

## How the states were reached

No state was patched. The direction lives in the nav word - `0x4` for PUNCH,
`0x200` for KICK - with input `0x0f000004` held, so:

```sh
# row 4 idle at preset 1 (from one PUNCH edit, preset 0 -> 1)
vf2probe --snapshot out/f2-r4-edit.vf2snap --until 0x7b1c \
         --output-snapshot out/f2r4-a-end.vf2snap
vf2probe --snapshot out/f2r4-a-end.vf2snap --until 0x10fa0 \
         --output-snapshot out/f2r4-a-w.vf2snap
vf2probe --snapshot out/f2r4-a-w.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x9ff8 --output-snapshot out/f2r4-a-c.vf2snap

# KICK press: preset 1 -> 0.  Edit frame is 4625, against the PUNCH's 4635.
vf2probe --snapshot out/f2r4-a-c.vf2snap --set-u32 0x500700=0x0f000004 \
         --set-u32 0x500704=0x200 --until 0x7b1c \
         --output-snapshot out/f2r4-k-end.vf2snap
# ... settle, IRQ, patch the release latch -> out/f2r4-k-rel.vf2snap
```

`preset = 2` is the same chain with `0x500704=0x4` from `f2r4-a-c`.

The row-4 KICK **edit** frame is 4625 instructions against the PUNCH's 4635 -
a third distinct length on the edit path, alongside the row-3 KICK's 4634. The
edit path is still refused for all of them, so nothing is silently accepted
with a wrong count.

## Still open

- The 7-instruction `a5 = 4` delta between `preset == 0` and `preset >= 1`.
- `preset >= 3` on the `a5 = 4` release. Fails closed.
- The INDIVIDUAL variant of the `a5 = 4` release. Fails closed.
- The edit path's 4625 / 4634 / 4636 variations. All refused.
- KICK -1 from INDIVIDUAL mode. Not started.
