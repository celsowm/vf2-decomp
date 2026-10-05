# v0732c: (6,40) is credits[1], not credits[0] - a correction to v0731l

**v0731l was wrong and this note retracts it.** The digit cell at screen
(6,40) reads `credits[1]`, not `credits[0]`. The value was unfalsifiable with
the samples available at the time, and the row-2 KICK case separates the two.

The v0731b/v0732 body-length count was wrong by exactly one on the same case
for the same reason. Both failures had a single cause and are fixed together.

## The ROM says which byte, directly

The four digit cells are emitted by four near-identical blocks. Each sets its
destination with `lda` and loads its source with `ldob`, so the mapping is
read out of the instruction stream rather than fitted:

| cell | destination | `ldob` source | index | site |
|---|---|---|---|---|
| (6,40) | `lda 0x01000350, g9` | `0x0000332a(g4)` | **credits[1]** | `0x0005bcd8` |
| (7,40) | `0x010003d0` | `0x0000332b(g4)` | credits[2] | `0x0005bd3c` |
| (8,40) | `lda 0x01000450, g9` | `0x0000332d(g4)` | credits[4] | `0x0005bda0` |
| (9,40) | `lda 0x010004d0, g9` | `0x0000332e(g4)` | credits[5] | `0x0005be04` |

Each block then does `cmpo 1, r15` / `lda 0x00008030, r14` / OR in the
attribute byte and `stos r15, (g9)` at `0x5bd04` / `0x5bd68` / `0x5bdcc` /
`0x5be30`.

`0x0000332a` with `g4` as the coin base is `credits[1]`. The four bytes the
renderer consumes are **1, 2, 4, 5** - the odd gap at index 3 is not an
omission, `credits[3]` is the row-3 counter and is never rendered directly,
only through its derived pair `credits[4]` / `credits[5]`.

## Why the earlier mapping survived every test

`t1 = {1,2,2,3,3,3,4,4,4,4,5,5,5,5,5,0}` is the identity at exactly two
indices, 2 and 3. Every PUNCH sample had `credits[0]` in {2, 3}, so
`credits[0] == t1[credits[0]] == credits[1]` always, and the two candidates
were numerically identical in every observation. The mapping was not merely
unproven - it was unfalsifiable from that sample set.

The row-2 KICK case breaks the identity. `credits[0]` goes 2 -> 1, so
`credits[1] = t1[1] = 2` while `credits[0] = 1`:

```text
credits = [1, 2, 1, 2]
         c0  c1 c2  c3

(6,40) expected 0x8032 '2'   <- credits[1]
(6,48) expected 0x8053 'S'   <- plural, because the rendered value is 2
```

The v0731b code wrote `credits[0] = 1` there and blanked the `S`, so it
produced `0x8031` and `0x8020` at both cells - exactly the two differing
bytes the differential reported. The same wrong byte also fed the singular
count, giving 2 singulars (4420) where the truth is 1 (4421), which is why
the instruction counter was off by one in the same run.

## How the state was reached naturally

No state was patched. The row-2 KICK uses the same latch shape as the row-3
one - input `0x0f000004` held, with the direction in the nav word, `0x4` for
PUNCH and `0x200` for KICK:

```sh
# the KICK press frame, from the row-2 idle
vf2probe --snapshot out/f2r2-edit.vf2snap --set-u32 0x500704=0x200 \
         --until 0x7b1c --output-snapshot out/f2r2k-end.vf2snap
# settle to the frame wait, cross the IRQ to the dispatch boundary
vf2probe --snapshot out/f2r2k-end.vf2snap --until 0x10fa0 \
         --output-snapshot out/f2r2k-w.vf2snap
vf2probe --snapshot out/f2r2k-w.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x9ff8 --output-snapshot out/f2r2k-c.vf2snap
# patch the release latch at the boundary
vf2probe --snapshot out/f2r2k-c.vf2snap \
         --set-u32 0x500700=0x0f000000 --set-u32 0x500704=0x0 \
         --set-u32 0x500708=0x4    --set-u32 0x50070c=0x0f000004 \
         --until 0x9ff8 --output-snapshot out/f2r2k-rel.vf2snap
```

## Result

The corrected body-length count is

```text
body = 4190 - (number of {credits[1], credits[2], credits[4], credits[5]}
               that equal 1)
```

14 release frames now match on registers, memory regions, instruction count
and call count:

```text
f2-r2        f2-r3        f2-r4        f2r2-c       f2r2b-c
f2r2rel-c    f2r2k-rel    f2-r3-cl     f2-r3-n8-c   f2-r3-n9-c
f2-r3-n10-c  f2-r3-n11-c  f2-r3-k1-c   f2-r3-c3a-e2 f2-r3-c3a-e4
```

`f2r2k-rel` is the new row-2 KICK case, and `f2r2b-c` / `f2r2rel-c` are
pre-existing row-2 PUNCH releases at `credits[0] = 3` that had never been run
through the differential - they pass, which is what first showed the counted
rule was not row-3-specific.

## The lesson worth keeping

v0731l declared the value subsystem "COMPLETE" from a mapping with two
observations per cell. The samples could not separate the hypothesis from its
alternative, and completeness was asserted anyway. The row-2 KICK cost one
committed round trip to find. When a mapping has a natural alias - here
`t1[i] == i` at two indices, so `credits[0]` and `credits[1]` coincide over
the whole sample set - say so explicitly rather than reporting the cell as
proved, and prefer the `ldob` that reads the byte over a value that happens
to agree.
