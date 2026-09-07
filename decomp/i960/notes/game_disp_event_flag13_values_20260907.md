# `fa_game_disp` dynamic event flag 13 — 2026-09-07

This note records clean-room recovery of registry bit 13 inside the measured
`fa_game_disp` event/queue child and its propagation through the complete
measured continuation.

## Boundaries

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- dynamic value: `u16 0x005000a2`
- side/main mode: `u8 0x0050004c`
- auxiliary mode: `u8 0x00500066`

The admitted family remains fail-closed: registry is exactly baseline
`0x80000000 | (1 << 13)`, the queue starts empty, both timers are zero,
selector is 17, event state is zero, value is `0..99`, side is `0..1`, and
auxiliary mode is `0..1`.

## ROM semantics

Bit 13 always starts by enqueueing:

```text
1f 3f 5f 7f
```

It then formats the measured 16-bit value as decimal glyph bytes when the value
is non-zero.

For side 0:

- optional tens glyph: `0x20 + tens`
- ones glyph: `0x00 + ones`

For side 1:

- optional tens glyph: `0x60 + tens`
- ones glyph: `0x40 + ones`

Examples measured directly from the ROM oracle:

| State | Queue payload before common dequeue |
| --- | --- |
| value 0 | `1f 3f 5f 7f` |
| side 0, value 7 | `1f 3f 5f 7f 07` |
| side 0, value 42 | `1f 3f 5f 7f 24 02` |
| side 1, value 7 | `1f 3f 5f 7f 47` |
| side 1, value 42 | `1f 3f 5f 7f 64 42` |

The common tail consumes the first queued event in the same invocation, advances
queue tail to 1, and writes `0x1f` to the measured output port `0x01c00008`.

The resulting i960 link register is also preserved:

- value 0: `g14 = 0x0002aea8`
- side 0, value non-zero: `g14 = 0x0002aed8`
- side 1, value non-zero: `g14 = 0x0002af10`

## Instruction accounting

| Measured class | Event child | Full continuation |
| --- | ---: | ---: |
| value 0 | 73 | 2,938 |
| side 0, 1 digit | 83 | 2,948 |
| side 0, 2 digits | 90 | 2,955 |
| side 1, 1 digit | 85 | 2,950 |
| side 1, 2 digits | 92 | 2,957 |

The complete continuation remains at 41 architectural procedure calls and 42
returns in every admitted case.

The side-1 path exposed an older artificially narrow native guard in
`fa_game_disp` main. The ROM at `0x0002c970` performs `cmpobl 1, mode`, so both
mode 0 and mode 1 follow the same measured body. Differential oracle comparison
confirmed that matched side-0/side-1 states differ only in the side byte, the
side-specific event glyph, `g14`, and the two additional bit-13 instructions.
The bit-13 composition layer therefore reuses the already-proved main body for
mode 1 while preserving the measured mode byte in the final snapshot.

## Differential proof

CI run `34131133845` for commit
`4a37a1b69bf513807869a2ebc53f92d473113259` passed GCC release, Clang release,
Clang ASan/UBSan, and Python tooling syntax. The exact `vf2i960-linux` artifact
used for differential validation was `10022113847`.

Sixteen measured states were validated at both the child boundary and the full
continuation boundary, for 32 exact snapshot comparisons:

- zero;
- side 0 values 1, 7, 9, 10, 42, and 99;
- side 1 values 0, 1, 7, 9, 10, 42, and 99;
- value 42 with auxiliary mode 1 on both sides.

Every comparison reported `Snapshots match.`

Regression checks with the same artifact also remained exact for registry bits
10, 14, 15, 16, 17, and 19, plus the ring-queue wrap case `tail=15 -> 0`.

Auxiliary mode 2 was tested on both sides and remains explicitly rejected with
`VF2_ERROR_UNSUPPORTED` at both child and complete-continuation entries.

## Remaining fail-closed frontier

This recovery intentionally does not admit:

- values above 99;
- side values above 1;
- auxiliary mode values above 1;
- bit 13 combined with non-zero timers, a non-empty initial queue, or another
  registry event flag;
- registry bits 8, 9, or 18 without their own ROM oracle;
- alternate event selector/state bytes outside their independently recovered
  state-8/9 family;
- alternate final dispatch-table indices or targets.

Registry bits 12/11 and their coupled state-8/9 timer/queue lifecycle are now
recovered separately in `game_disp_event_state89_lifecycle_20260907.md`.
The next useful event-handler targets are bit 18 and the remaining bits 8/9.
