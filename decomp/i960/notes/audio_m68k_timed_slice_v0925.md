# v0925 — full idle Timer-A ISR baseline timing (owner-ROM differential)

Date: 2026-10-24. Branch `master`.

This continuation deliberately stays within the previously recovered
original VF2.1 sound-MC68000 execution path. The v0924 GitHub Actions
GCC, Clang, ASan/UBSan, MSVC, and SDL3 test matrix passed.
The v0924 **private-ROM** validation is not implied by that CI pass.

## New admitted clocked instruction

Original stream worker at PC `0x1F7C` starts with
`BTST #7,$0000(A2)`. Static BTST on byte memory costs
8 MC68000 cycles plus 8 cycles for displaced `(d16,An)`,
**16 clocks** in the Motorola MC68000UM timing tables 8-1 and
8-8. This exact addressing shape is now admitted in
`vf2_m68k_run_cycle_slice`; other bit operations and
address forms remain fail-closed.

When the stream descriptor is *inactive* (bit 7 clear),
the real worker returns through a word-displacement BNE:

| Original PC | Instruction | MC68000 cycles |
| --- | --- | ---: |
| `0x1F7C` | `BTST #7,$0000(A2)` | 16 |
| `0x1F82` | `BNE.W $1F88` not taken | 12 |
| `0x1F86` | `RTS` | 16 |
| **Total** | inactive worker subroutine | **44** |

The caller pays `JSR (d16,PC)` = 18 before each worker,
so each idle `JSR + worker + RTS` costs **62** clocks.
For all eight original records there are seven inter-record
`LEA (d16,An),A2` at 8 clocks each.

## Full, specific idle ISR baseline clock calculation

| Piece | Cycle cost |
| --- | ---: |
| Level-2 autovector 26 entry | 44 |
| `0x1F16` MOVEM.L save, 13 registers | 112 |
| `0x1F1A..0x1F2E` SCSP reload, FIFO count gate (count 0) | 64 |
| `0x1F32..0x2240` original prescaler fast-return | 64 |
| 8 idle stream workers, including calls (`8*62`) | 496 |
| Seven LEA record advances (`7*8`) | 56 |
| `0x1F76` MOVEM.L restore, 13 registers | 116 |
| `0x1F7A` RTE from a real exception frame | 20 |
| **Total ideal baseline** | **972 clocks** |

This is the baseline cost of **one fully specified idle
branch**, not a bound for an active stream worker, not a
measurement of SCSP bus contention, and not a production
interrupt scheduler.

## Regression coverage

New ROM-free tests verify the 16-clock BTST admission,
inactive (12-clock) and active (10-clock) BNE.W outcomes,
a 43-cycle pre-RTS pause, and refusal of a different
BTST addressing form. They preserve X/CCR and return-stack
state at instruction budget boundaries.

The opt-in owner-ROM differential now separately enters
the authentic L2 autovector from PC `0x2242` in two independent
RAM/SCSP/CPU snapshots. It explicitly seeds ring count 0,
prescaler rate 0 and all eight records inactive, then runs
the original program through the real RTE. The instruction-
sliced path checks that a 907-clock budget stops before the
MOVEM restore at `0x1F76` (792 clocks completed), then
resumes for 116 clocks, refuses a 19-clock RTE budget,
and returns on its 20-clock instruction. The fixture
checks **52 completed instructions**, 972 charged clocks,
full RAM/SCSP internal state, all D/A registers, CCR,
SR/stack and IRQ active state.

Run the private original-ROM check with:

```sh
vf2_m68k_l3_selection_tests /private/vf2-rom --verify-akira-walk
```

Expected marker, **not yet measured on private ROM**:

```text
AKIRA_L2_IDLE_ISR_TIMING matched=1 entry_cycles=44 body_cycles=908 rte_cycles=20 total_cycles=972 original_steps=52 timed_steps=52
```

The 900Hz synthetic dispatcher remains unchanged. The
clocked API does **not** independently schedule CPU/SCSP
phase, model IRQ priority and acknowledgment, or assert
hardware-real-time audio accuracy. Active prescaler and
stream-worker paths still require separate ROM-backed timing
coverage before introducing asynchronous interrupts.

Reference: Motorola/NXP
[MC68000UM](https://www.nxp.com/docs/en/reference-manual/MC68000UM.pdf),
MC68000 section 8, tables 8-1, 8-2, 8-8, 8-9 and 8-10.
