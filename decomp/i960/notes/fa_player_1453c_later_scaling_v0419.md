# v0419: recover the later `0x14570` scaling arm

## Verdict

The measured direct state-16 path with the `0x50016c+0x3351` byte bit 6 set
and the g8 word bit 29 set is now native. After the `0x145c0` gate, the ROM
takes the `0x145cc..0x145d4` `shro`/`addo` sequence and selects table
`0x1b982`. It reaches `0x1463c` in 57 instructions with one type-5
call/return.

The recovery admits only the measured direct `(r7,r8)=(16,0)` composition,
with `+0x1a4(g8)` bit 0 clear, `+0x3351` bit 6 set, and the g8 word bit 29
set. Swapped/state-27 variants and the board-controlled text branch remain
fail-closed.

## Pin

`vf2_player_1453c_live` adds the direct state-16 later-scaling case to its
matrix and requires exact instruction count, call/return counters and full
live-state equality.

Measured probe:

```text
vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-ip 0x14528 --set-reg g7=0x510980 --set-reg g8=0x512980 \
  --set-reg r7=16 --set-reg r8=0 --set-reg r10=0x510980 \
  --set-reg r11=0x512980 --set-u16 0x510b14=0x73 \
  --set-u8 0x59c351=0x40 --set-u32 0x512980=0x20000000 \
  --until 0x1463c
```

Observed result: 57 instructions, one call/return, and `0x145d4` selects
`0x1b982`.
