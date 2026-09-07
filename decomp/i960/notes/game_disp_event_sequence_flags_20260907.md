# `fa_game_disp` event sequence flags 16/17/19 — 2026-09-07

This note records clean-room recovery of three additional registry-flag branches
inside the measured `fa_game_disp` event/queue child and their propagation
through the complete measured continuation.

## Boundaries

- complete continuation: `0x0002b1f8 -> 0x00010dcc`
- event/queue child: `0x0002ab94 -> 0x0002b260`
- task registry: `0x00515b00`
- queue head: registry `+0x5b`
- queue data: registry `+0x5c .. +0x6b`
- queue tail: registry `+0x6c`

The recovered cases are deliberately narrow: each new flag is admitted by
itself on top of the measured baseline registry value `0x80000000`, with an
initial empty queue and zero timers. Combinations among the new flags remain
fail-closed until separately measured.

## ROM semantics

The three branches share the same structure. Each clears its registry bit,
walks a `0xff`-terminated byte sequence embedded in the program ROM, and calls
the queue helper at `0x0002b180` for each byte. That helper stores the byte at
`queue_data[head]`, increments `head`, masks it with `15`, then returns through
the link register used by the loop.

The ROM sequences are:

| Registry flag | Sequence address | Enqueued bytes |
| --- | --- | --- |
| bit 16 (`0x00010000`) | `0x0002b1ad` | `97 9f 8f` |
| bit 17 (`0x00020000`) | `0x0002b1a8` | `1f 3f 5f 7f` |
| bit 19 (`0x00080000`) | `0x0002b1b1` | `1f 3f 5f 7f 97 9f 8f` |

After the sequence is enqueued, the common queue tail consumes one byte in the
same child invocation, advances the ring tail, and writes that byte to the
measured output port `0x01c00008`.

The recovered implementation preserves the observed i960 link-register and
condition-code state rather than only reproducing the work-RAM bytes.

## Instruction accounting

The measured zero-flag event child is 38 instructions. The common dequeue path
adds 4 instructions. The three sequence branches add these measured deltas
before that dequeue:

| Case | Sequence branch delta | Event child | Full continuation |
| --- | ---: | ---: | ---: |
| bit 16 | 30 | 72 | 2,937 |
| bit 17 | 38 | 80 | 2,945 |
| bit 19 | 62 | 104 | 2,969 |

The complete continuation continues to account for 41 architectural procedure
calls and 42 returns in all three cases.

## Differential oracle

The supported uploaded VF2 ROM was executed by the i960 interpreter from the
same measured entry snapshots to produce the reference outputs. The exact
`vf2i960-linux` artifact `10020389055`, built by CI from commit
`ea51ad7f8bf1d95cd4f6b4805414ee6cc709b632`, was then run natively against
those entries.

All six new comparisons were exact:

| Case | Native instructions | CPU + memory snapshot |
| --- | ---: | --- |
| bit 16 child | 72 | exact match |
| bit 17 child | 80 | exact match |
| bit 19 child | 104 | exact match |
| bit 16 full continuation | 2,937 | exact match |
| bit 17 full continuation | 2,945 | exact match |
| bit 19 full continuation | 2,969 | exact match |

Regression checks with the same artifact also remained exact for the previously
recovered bit-10 branch (55 child instructions) and the ring-queue wrap case
`tail=15 -> 0` (42 child instructions).

CI run `34126742227` passed GCC release, Clang release, Clang ASan/UBSan, and
Python tooling syntax before the artifact was used for the differential proof.

## Remaining fail-closed frontier

This recovery intentionally does not admit:

- combinations among registry bits 16, 17, 19, or 10;
- non-zero timers together with bits 16, 17, or 19;
- the larger neighboring registry-flag branches (including bits 8, 9, 11-15,
  and 18) without their own ROM oracle;
- alternate event selector/state bytes;
- alternate final dispatch-table indices or targets;
- downstream score/event paths not exercised by these measured states.

The next useful candidates are bits 14 and 15: both are smaller conditional
event branches, but they depend on additional measured state and should be
recovered independently rather than generalized from the sequence branches.
