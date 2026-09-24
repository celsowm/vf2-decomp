# v0420: recover the direct `0x14570` text tail

## Verdict

The measured direct state-16 short tail with board `0x508000` bit 9 clear is
now native. The ROM takes the `0x145ec` text setup and calls the recovered
`0x7fc0` byte expander with source `0x1b970` and destination `0x010006e8`.
The source contains eight nonzero bytes followed by NUL, so the expander
writes eight glyph shorts and returns to `0x145f8`. The full path reaches
`0x1463c` in 126 instructions with two calls/returns: the type-5 walk and
the text expander.

The recovery admits only the measured direct `(r7,r8)=(16,0)` composition,
with `+0x1a4(g8)` bit 0 clear, the `0x50016c+0x3351` byte bit 6 clear, and
the g8 word bit 29 clear. Scaled, swapped and state-27 text variants remain
fail-closed.

## Pin

`vf2_player_1453c_live` adds the direct text case to its matrix and requires
exact instruction count, call/return counters and full live-state equality,
including the eight glyph writes at `0x010006e8`.

Measured probe:

```text
vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-ip 0x14528 --set-reg g7=0x510980 --set-reg g8=0x512980 \
  --set-reg r7=16 --set-reg r8=0 --set-reg r10=0x510980 \
  --set-reg r11=0x512980 --set-u16 0x510b14=0x73 \
  --set-u32 0x508000=0 --until 0x1463c
```

Observed result: 126 instructions, two calls/returns, and the `0x7fc0`
byte-expander writes eight glyph shorts before returning to the common tail.
