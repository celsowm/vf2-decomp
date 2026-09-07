# `fa_game_disp` registry bit 18 — 2026-09-07

This note records the measured clean-room recovery of registry bit 18 in the
`fa_game_disp` event child and complete continuation, including the state-bit-6
variant that arms registry bits 9/8 for a later invocation.

## Boundaries

- event child: `0x0002ab94 -> 0x0002b260`
- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- task registry: `0x00515b00`
- player pointer slots: `0x0050083c`, `0x00500840`
- measured player records: `0x00515c00`, `0x00515c80`
- state-base slot: `0x0050016c`
- measured state base: `0x00599000`
- state-bit byte: `state_base + 0x3351`
- secondary-value word: `0x005000a2`

The recovery is deliberately fail-closed. It admits only the exact measured
pointer, selector, queue, timer, mode, state and value families.

## Ordinary bit-18 path

With state flags zero, pointer value `8` is inactive and `9` is active.

| Active side | Secondary zero | Secondary nonzero |
| --- | --- | --- |
| none | keep bit 18, no queue | keep bit 18, no queue |
| P1 | `a1` | `a1 9a` |
| P2 | `a9` | `a9 92` |
| P1 + P2 | `a1 a9` | `a1 9a a9 92` |

The common queue tail consumes the first byte in the same invocation and writes
it to `0x01c00008`.

Measured instruction counts:

| Case | Child | Full |
| --- | ---: | ---: |
| deferred | 44 | 2,909 |
| P1/P2 short | 62 | 2,927 |
| both short | 73 | 2,938 |
| P1/P2 long | 68 | 2,933 |
| both long | 85 | 2,950 |

The full continuation exposed four common-path registry bytes at
`0x00515b44..0x00515b47`: `1f 80 1f 80`. They are produced by the ordinary
continuation, not by the bit-18 child, so the native composition preserves that
post-child work instead of transplanting an entire registry image.

## State-bit-6 path

When `state_base + 0x3351 == 0x40`, bit 18 does not enqueue the `a1/a9`
sequence. Instead it clears bit 18 and arms a later event:

- P1 active -> registry bit 9 (`0x00000200`)
- P2 active -> registry bit 8 (`0x00000100`)
- both active -> bits 9 + 8 (`0x00000300`)

The complete continuation also executes the measured helper at `0x00063828`.
Its state-bit-6 body calls the `0x00009444` path and writes:

```text
0x01000040: 2e 80 31 80
```

Relative to the ordinary 2,903-instruction continuation, this helper contributes
38 instructions and one architectural call/return. Bit 18 then contributes 14
instructions for one active side or 19 for both.

Exact totals:

| Case | Child | Full | Full calls/returns |
| --- | ---: | ---: | ---: |
| P1 state6 | 52 | 2,955 | 42 / 43 |
| P2 state6 | 52 | 2,955 | 42 / 43 |
| both state6 | 57 | 2,960 | 42 / 43 |

## Differential proof

Commit `a3967fa8c0fe16c95d04bfbecda3b44ce3cf01d7` passed GCC release,
Clang release, Clang ASan/UBSan and Python tooling in CI run `34146097241`.
The exact `vf2i960-linux` artifact used for validation was `10027703112`.

Fifteen measured input states were validated at both the child and complete
continuation boundaries: twelve ordinary/deferred/secondary variants plus P1,
P2 and bilateral state-bit-6 variants. All 30 comparisons reported
`Snapshots match.`

## Remaining frontier

Bit 18 itself is recovered for the measured families above. The state-bit-6
variant intentionally leaves bits 9/8 armed; their later event-handler
invocation is a separate dynamic branch and must be recovered against its own
ROM oracle.
