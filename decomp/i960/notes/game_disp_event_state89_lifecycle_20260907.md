# `fa_game_disp` event states 8/9 and registry bits 12/11 — 2026-09-07

This note records clean-room recovery of the measured multi-frame event lifecycle
used by event states 8 and 9 in `fa_game_disp`. The lifecycle couples live
per-fighter values to registry bits 12/11, the 16-byte event ring queue, and the
two one-byte timers.

## Boundaries

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- event selector: `u8 0x0050002b = 17`
- event state: `u8 0x00500031 = 8 or 9`
- side/mode: `u8 0x0050004c = 0 or 1`
- auxiliary byte: `u8 0x00500066 = 0`
- state byte preserved by this lifecycle: `u8 0x005000a4`
- fighter 0 value: `u16 (0x00510980 + 0x1e20)`
- fighter 1 value: `u16 (0x00512980 + 0x1e20)`
- bit 12: `0x00001000` (side 0)
- bit 11: `0x00000800` (side 1)

## Reconstructed lifecycle

The measured state machine is:

```text
live fighter value
      |
      v
initial event
      |
      +-- enqueue side-specific prefix + decimal value
      +-- arm bit 12 or bit 11
      +-- arm timer 18 or 36
      v
pending frames
      |
      +-- decrement active timer
      +-- drain at most one ring event per invocation
      v
timer reaches zero
      |
      v
terminal invocation
      |
      +-- clear bit 12 or bit 11
      +-- enqueue three-byte terminal sequence
      +-- drain first terminal byte immediately
      v
final-drain frames
      |
      v
settled
```

Side 0 starts with:

```text
97 90 1f 3f <decimal value>
```

and finishes with:

```text
97 1f 3f
```

Side 1 starts with:

```text
9f 98 5f 7f <decimal value>
```

and finishes with:

```text
9f 5f 7f
```

Decimal glyph encoding follows the same measured convention as the neighboring
dynamic event branches:

- side 0 one digit: `0x00 + digit`
- side 0 tens: `0x20 + tens`
- side 1 one digit: `0x40 + digit`
- side 1 tens: `0x60 + tens`

Values below 30 arm the measured timer value 18. Values 30 and above in the
measured family arm 36.

## Measured instruction classes

Representative exact counts are:

| State | Side / phase | Event child | Full continuation |
| --- | --- | ---: | ---: |
| 8 | side 0, value 5 initial | 106 | 2,971 |
| 8 | side 1, value 5 initial | 108 | 2,973 |
| 8 | side 0, pending with dequeue | 59 | 2,924 |
| 8 | pending timer `1 -> 0`, empty queue | 55 | 2,920 |
| 8 | terminal | 80 | 2,945 |
| 8 | final drain | 59 | 2,924 |
| 8 | settled | 55 | 2,920 |
| 9 | side 0, value 5 initial | 108 | 2,969 |
| 9 | side 1, value 5 initial | 110 | 2,971 |
| 9 | pending with dequeue | 61 | 2,922 |
| 9 | pending timer `1 -> 0`, empty queue | 57 | 2,918 |
| 9 | terminal | 82 | 2,943 |
| 9 | final drain | 61 | 2,922 |
| 9 | settled | 57 | 2,918 |

For state 8, all separately measured initial values `1, 5, 9, 10, 30, 31, 42,
99` are admitted on both sides. State 9 remains deliberately narrower: its
non-zero initial value family is still restricted to the measured value 5.

## `0x005000a4` preservation

A fresh oracle was rebuilt from the sixth-dispatch checkpoint rather than
reusing the earlier synthetic fixtures. The checkpoint enters this work with
`0x005000a4 = 0x0b`.

Tracing the reference i960 from the complete-continuation entry to the event
child and then through the child showed that the state-8/9 lifecycle does not
clear this byte. It is preserved through initial, pending, terminal,
final-drain, and settled phases at both the child and complete-continuation
boundaries.

The earlier apparent clear was therefore a fixture precondition, not a write
performed by this lifecycle. The native implementation now preserves the byte.
Both measured input values `0x00` and `0x0b` are accepted and preserved;
an unmeasured value such as `0x07` remains fail-closed before any native block
executes.

## Differential proof

CI run `34142711216` for commit
`be8aa0352f579606b06f267cdf284a0cf05d5148` passed GCC release, Clang release,
Clang ASan/UBSan, and Python tooling syntax. The exact `vf2i960-linux` artifact
used for the final differential sweep was `10026506201`.

The final sweep rebuilt reference outputs from the uploaded VF2 ROM and tested
26 lifecycle configurations with `0x005000a4 = 0x0b` at both boundaries:

- state 8 and state 9;
- both sides;
- initial, pending timer 18, pending timer 1, terminal, final drain, and settled;
- state-8 value-30 initial cases to cover the timer-36 class.

That produced 52 exact child/full comparisons, all reporting
`Snapshots match.`

A second value sweep exercised all state-8 measured initial values
`1, 5, 9, 10, 30, 31, 42, 99` on both sides: 16 configurations / 32 more exact
child/full comparisons.

A third sweep repeated six representative lifecycle phases with
`0x005000a4 = 0x00`: 12 more exact comparisons.

In total the final validation executed 96 exact snapshot comparisons. Removing
intentional overlap between the value regression sweep and the lifecycle sweep
leaves 44 distinct measured input configurations and 88 distinct child/full
oracle pairs.

The fail-closed probe with `0x005000a4 = 0x07` was rejected at both
`0x0002ab94` and `0x0002b1f8` with zero native blocks executed.

## Remaining fail-closed frontier

This recovery intentionally does not admit:

- state-9 non-zero initial values other than the measured value 5;
- timers above 36;
- malformed or contradictory queue/timer/flag combinations;
- state-8/9 combinations with unrelated registry event flags;
- unmeasured values of `0x005000a4`;
- registry bits 8, 9, or 18 without their own ROM oracle;
- alternate event selector values;
- alternate final dispatch-table indices or targets.

The next narrow event-handler frontier is therefore registry bit 18 or the
remaining bits 8/9, rather than bits 12/11: the measured state-8/9 lifecycle is
now recovered end to end.
