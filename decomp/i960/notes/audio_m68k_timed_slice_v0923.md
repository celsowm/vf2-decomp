# v0923 — 64-cycle original VF2.1 ISR helper, BSR stack fix, and long-sequence regression

Date: 2026-10-24. Branch: `master`.

## What changed

The initial `vf2_m68k_run_cycle_slice()` implementation (v0922)
covered basic branch, MOVEM, NOP, MOVEQ, RTE and RTS timings,
but admitted no real sound-ROM subroutine calls. This update:

1. **Fixes the 68000 `BSR.B` / `BSR.W` execution semantics.**
   `BSR` is unconditional and MUST push the PC after its
   displacement onto the A7 stack before branching. The old
   `step_bcc()` incorrectly treated condition code 1 like
   an ordinary false conditional branch. The new implementation
   writes a big-endian long return address and refuses an
   invalid/unaligned stack instead of pretending to execute.
2. Counts **18 cycles for BSR.B/W**, and selected
   real MC68000 JSR/JMP/LEA effective-address forms:
   `JSR (d16,PC)=18`, `JSR (An)=16`,
   `JSR abs.l=20`, `JMP (d16,PC)=10`,
   `JMP (An)=8`, `JMP abs.l=12`,
   `LEA (d16,An)=8`, `LEA (An)=4`.
3. Adds `TST.B (d16,An)=12` cycles
   (**4 CPU + 8 effective-address cycles**, from the
   original Motorola manual), used in the first ISR helper.
4. Adds ROM-free regressions for BSR.B/W pushed return
   addresses, JSR PC-relative return, branch and stack
   preflight limits, and the exact five-opcode helper cost.
   Unknown instruction timing still returns UNSUPPORTED
   without executing that opcode.

Reference: [Motorola MC68000UM, tables 8-1, 8-5, 8-9 and 8-10](https://www.nxp.com/docs/en/reference-manual/MC68000UM.pdf).
The timings are for an **uncontended MC68000**, excluding
external bus wait states and arbitration.

## Owner-ROM proof: authentic interrupt dispatch snippet

The code at these original sound-MC68000 ROM offsets was
compared against a continuous original-ROM execution,
starting from the same sound RAM, SCSP and register snapshot.

| Original ROM PC | Instruction | MC68000 clock cycles |
| --- | --- | ---: |
| `0x1F32` | `LEA 0x2000(A6),A2` | 8 |
| `0x1F36` | `JSR (d16,PC)` to `0x21BA` | 18 |
| `0x21BA` | `TST.B 0x000C(A2)` | 12 |
| `0x21BE` | `BEQ.W` taken to `0x2240` | 10 |
| `0x2240` | `RTS`, restoring `0x1F3A` | 16 |
| **Total** | **Five instructions** | **64** |

The return from JSR was pushed as `0x00001F3A`;
the original stack pointer was restored after RTS.
The original full execution and the sliced timed
execution matched **all D/A registers, CCR, entire
sound RAM image and SCSP registers**. A 17-cycle
JSR budget and a 7-cycle LEA budget each refused
before execution. The callee's 37-cycle budget
executed only `TST.B` and `BEQ.W` (22 cycles)
and stopped **before** the 16-cycle RTS. The next
16-cycle slice resumed and returned normally.

Owner-only test command (no proprietary data is committed):

```sh
vf2_m68k_l3_selection_tests /private/extracted-vf2-rom \
    --verify-akira-walk
```

Result on the owner's authentic chip ROM using the
Linux GitHub Actions artifact for commit `6821d34`:

```text
AKIRA_L2_JSR_CYCLES matched=1 lea_pc=01f32 lea_cycles=8 jsr_pc=01f36 jsr_cycles=18 callee_pc=021ba callee_cycles=38 full_five_opcode_cycles=64 return_pc=01f3a full_steps=5 split_steps=5 full_isr_timing_not_covered=1
AKIRA_L2_AUTOVECTOR_FRAME matched=1 enter=0 direct=0 body=0 rte=0 body_steps=53 legacy_steps=53 rte_steps=1 restored_pc=02242 stack=05000 irq_active=0 irq_entry_cycles=44 prologue_cycles=112 epilogue_cycles=116 full_isr_cycle_total_not_modeled=1
AKIRA_ORIGINAL_WALK_STRICT parity=1 passes=32 pages=4 native_admitted=4 steps=588 pcm_bytes_changed=2039
```

**Exit status 0.** The older long owner-ROM regression
also passed on the updated binary:

```text
AKIRA_COMPOSITE_10S_STRICT parity=1 ticks=9000 passes=288000 original_pages=2205 original_steps=5078020 modeled_cadence=900Hz realtime_verified=0
```

## What this result is *not*

This is the verified **64-cycle fragment** of the ROM.
It is **not** a measurement of the entire 53-opcode ISR,
of every 68000 command-drainer instruction, or of a
cycle-exact arcade SCSP waveform. Real hardware timer
interrupts have not been routed through the cycle-sliced
CPU main loop; the 900 Hz Akira playback mode still
uses a synthetic dispatcher schedule.

Next: recover more opcode-specific costs actually
encountered from `0x1F1A` through the `0x1F7A`
ISR return and within the `0x2242` walker, then
test pending/autovector delivery under the strict
12,544 CPU-cycle Timer-A phase budget. Do not
enable scheduling or IRQ preemption in production
until unknown timings and interrupt arbitration
have been explicitly handled.
