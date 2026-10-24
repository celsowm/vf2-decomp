# v0922 — Conservative, opcode-timed MC68000 slices for VF2 sound

Date: 2026-10-24. Branch `master`. No ROM, samples or output WAVs
are checked into this repository.

## Progress and scope

The existing owner-ROM driver verified that the PCM walker can be
executed in persistent instruction quanta and that the 68000 Timer-A
interrupt frame is pushed/popped correctly. However, instruction
**count** does not represent elapsed CPU time. Before this change,
the experimental `--verify-akira-tima-10s` harness incorrectly
drained all FIFO commands once per 49-frame interval, even though
the conservative bound showed those operations could not finish
within 12,544 MC68000 clocks.

Added **`vf2_m68k_run_cycle_slice(bus, regs, stop_pc, budget,
&cycles, &steps, &suspended)`**, a strict subset execution API:

- Before every instruction, determine its exact baseline 68000
  cycle cost from **opcode, addressing form, original CCR and
  (where needed) counter-register state**.
- If the next full instruction exceeds the remaining budget,
  return `VF2_OK`, `suspended=1`, and leave PC/registers/memory
  at the **unexecuted instruction boundary**.
- If timing is absent, return `VF2_ERROR_UNSUPPORTED` rather
  than treat the old **4-cycles/instruction** floor as real time.
- Allow repeated invocations on the **same** CPU and bus.
  Do not reset SR, A7, RAM, SCSP or active autovector state.
- This function does **not** deliver pending interrupts,
  advance the SCSP sample engine, or count memory wait states.
  It is not connected to the host's production sound loop.

### Admitted timing subset

| MC68000 instruction | Baseline CPU clocks |
| --- | --- |
| `NOP`, `MOVEQ` | 4 |
| `BRA.B`, `BRA.W` | 10 |
| `Bcc.B` taken / not taken | 10 / 8 |
| `Bcc.W` taken / not taken | 10 / 12 |
| `DBcc` true / false-and-loop / false-and-expired | 12 / 10 / 14 |
| `MOVEM.L registers,-(An)` | `8 + 8*n` |
| `MOVEM.L (An)+,registers` | `12 + 8*n` |
| `RTS` | 16 |
| `RTE` with an actual stacked IRQ frame | 20 |
| Accepted MC68000 autovector IRQ entry | 44 (separately charged) |

Here `n` counts the actual one-bits in the MOVEM register mask.
The dispatcher still lacks correct `BSR` stack semantics;
the new timed runner **rejects BSR explicitly** until that
can be emulated and tested. All other unsupported addressing
modes and instruction costs are refused rather than guessed.

Official timing references (baseline zero-wait MC68000, **not**
the later MC68020/68030):
- [Freescale/NXP MC68000 Microprocessors User's Manual, Section 8](https://www.nxp.com/docs/en/reference-manual/MC68000UM.pdf)
- [Motorola M68000 Family PRM, MOVEM specification](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf)

## Original VF2.1 L2 ISR instruction-boundary proof

The private owner-ROM differential now executes the genuine
VF2.1 interrupt entry through vector 26, retaining the
interrupt frame, and compares it with a separate execution
of the same original 68000 code from `0x1F16`.

- Vector entry consumes **44 cycles**.
- `0x1F16`: `MOVEM.L D0-D7/A0-A4,-(A7)` saves **13** registers
  in **112 cycles**. A 111-cycle budget refuses without mutation.
- Intermediate ISR instructions still run in the old semantic
  oracle because their exact timings are **not yet all covered**.
- `0x1F76`: `MOVEM.L (A7)+,D0-D7/A0-A4` restores **13**
  registers in **116 cycles**. A 115-cycle budget refuses.
- The real `RTE` consumes **20 cycles** and returns the
  interrupted PC to `0x2242` using its six-byte supervisor
  frame.

Therefore the **already established** subparts of this
interrupt take **292 MC68000 clocks** (44+112+116+20),
**not** the complete ISR's cost. Unknown intermediate
instructions, memory wait states and IRQ arbitration are
deliberately excluded. The actual 53-instruction body,
PCM RAM and SCSP register state still match the original
direct-entry oracle.

Unit regressions cover `DBcc` counter expiry and CCR
branches, byte/word displacements, RTE stack non-mutation
when only 19 of 20 cycles are available, and both MOVEM
13-register mask/cost boundaries. All completion events
happen between instructions, never in the middle of a MOVEM.

## Clock phase accounting — no fabricated delivery

Added `vf2_m68k_timer_a_advance()`, a **pure diagnostic**
phase accumulator over the exact Model 2A ratio:

```text
45,158,400 Hz / 4 = 11,289,600 MC68000 CPU clocks/s
SCSP 0x00CE low-byte reload at 44,100Hz:
(255 - 206) = 49 PCM frames/expiry
44,100 / 49 = 900 ideal Timer-A expiries/s
11,289,600 / 900 = 12,544 MC68000 clocks/expiry
```

It accepts actual executed-cycle increments, tracks phase
modulo 12,544 and returns how many **possible Timer-A
expiries** were crossed, including when a large chunk spans
multiple periods. It does *not* set interrupt-pending flags,
coalesce levels, send vector 26 or claim that the CPU
actually serviced each possible expiry. Those are subsequent
hardware-modeling tasks.

Example tested across irregular time slices: IRQ entry
44 cycles + save 112 + restore 116 + RTE 20 = **292**.
Advancing 12,252 further cycles produces exactly one
possible Timer-A boundary; all intermediate phases are
preserved. Invalid clock phases refuse rather than wrap.

## Next safe development frontier

Recover the original VF2.1 ISR's remaining opcode cycle
timings with mode-dependent operands, and implement
`BSR` stack behavior before enabling it in a timed slice.
Only after all executed opcodes are covered can the host
advance one real instruction at a time, charge its actual
cycles, latch SCSP IRQ-pending state on a 12,544-clock
boundary, and deliver interrupt vector 26 according to
IPL/mask and instruction-boundary rules. Independent MAME
trace/reference comparison remains necessary before
claiming cycle-accurate arcade audio.

Do not replace the verified 900Hz experimental WAV
schedule with a real-time scheduler until this frontier
is closed.
