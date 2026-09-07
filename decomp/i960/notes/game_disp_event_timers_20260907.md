# `fa_game_disp` event timer branches — 2026-09-07

This note records the clean-room recovery of the two timer branches inside the
measured `fa_game_disp` event/queue child and their propagation through the
complete measured continuation.

## Boundaries

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- queue head: registry `+0x5b`
- queue tail: registry `+0x6c`
- timer 0: registry `+0x6d`
- timer 1: registry `+0x6e`

The recovered continuation now admits both measured register shapes seen at
`0x0002b1f8`: the previously recovered activation with `g8=0x00510980` and the
first post-init activation with `g8=0x00512980`. All other guarded globals and
frame invariants remain exact.

## ROM semantics

The timer code is not two independent byte decrements. The i960 `stos`
instruction stores 16 bits:

```text
0x0002abb0  ldob  +0x6d(g13), r11
0x0002abb8  subo  1, r11, r15
0x0002abbc  be    0x0002abc4
0x0002abc0  stos  r15, +0x6d(g13)
0x0002abc4  ldob  +0x6e(g13), r12
0x0002abcc  subo  1, r12, r15
0x0002abd0  be    0x0002abd8
0x0002abd4  stos  r15, +0x6e(g13)
```

Therefore:

- `timer0 == 0 && timer1 == 0`: no store, 38 child instructions;
- `timer0 != 0`: write the halfword `{timer0 - 1, 0}` at `+0x6d`, so the
  original `timer1` byte at `+0x6e` is cleared before the second `ldob`;
- `timer0 == 0 && timer1 != 0`: write the halfword `{timer1 - 1, 0}` at
  `+0x6e`;
- `timer0 != 0 && timer1 != 0`: only the first store executes because it clears
  `timer1`; the child still gains exactly one instruction.

The complete continuation consequently stays at 2,903 instructions for the
zero/zero baseline and becomes 2,904 instructions whenever either measured
timer store executes. Procedure accounting remains 41 calls and 42 returns.

## Differential oracle

The uploaded supported VF2 ROM was executed with the interpreter to build the
expected snapshots, then the exact `vf2i960-linux` artifact produced by commit
`24e34fa7c4cb21aa85d529806d972b8913cd1f9a` was run natively from the same
entry snapshots.

| Entry state | Event child | Full continuation | CPU + memory snapshot |
| --- | ---: | ---: | --- |
| `timer0=0, timer1=0` | 38 | 2,903 | exact match |
| `timer0=1, timer1=0` | 39 | 2,904 | exact match |
| `timer0=0, timer1=1` | 39 | 2,904 | exact match |
| `timer0=1, timer1=1` | 39 | 2,904 | exact match |

For the both-nonzero case, an intermediate interpreter snapshot at
`0x0002abc4` also showed registry bytes `+0x6c..+0x6f = 00 00 00 00`, directly
confirming the overlapping halfword store.

Before this recovery the native path rejected all three nonzero timer variants
at `0x0002ab94`. The recovered child and complete continuation now match the
interpreter exactly for these measured states.

## Frontier update

The nearby frontiers identified when this timer slice was first recovered have
since moved forward:

- the full valid 16-slot event ring, including non-empty queues and wraparound,
  is recovered and documented in `game_disp_event_queue_20260907.md`;
- measured registry flag bit 10, including its `0xa0`/`0xa8` event enqueue and
  composition with the timer path, is recovered and documented in
  `game_disp_event_flag10_20260907.md`.

The remaining fail-closed frontier is now narrower:

- other guarded `fa_game_disp` registry flags;
- alternate event selector/state bytes;
- alternate final dispatch-table indices/targets; and
- downstream score/event branches not exercised by the current oracle corridor.
