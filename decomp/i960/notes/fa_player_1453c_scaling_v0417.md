# v0417: recover the first `0x14570` scaling arm

## Verdict

The measured direct state-16 tail with bit 0 of `+0x1a4(g8)` set is now
native. After loading the type-5 record byte at `0x1458c`, the ROM takes the
`0x1459c` fall-through:

```text
shro 2, r3, r15
addo r3, r15, r3
lda 0x1b979, g0
```

The scaled byte is stored at `+0x822(g8)`, and the path reaches `0x1463c` in
55 instructions with one `0x1ab34` call and return. The recovery admits this
variant only for the measured direct `(r7,r8)=(16,0)` shape. Scaled swaps,
state-27 scaling, the later `0x145c0` scaling arm and text submission remain
unsupported.

## Pin

`vf2_player_1453c_live` adds the direct state-16 bit-0-set case to the prior
six-case matrix and requires exact instruction count, call/return counters and
full live-state equality.
