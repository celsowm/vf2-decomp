# v0920 — MC68000 Timer-A autovector and RTE frame recovery

Date: 2026-10-24. Repository `celsowm/vf2-decomp`, direct
`master`. No proprietary ROM or PCM committed.

## Recovered interrupt semantics

The recovered ROM at offset `0x0068` contains the 68000
**level-2 autovector (vector 26)**, targeting `0x601F16`
in the original Model 2A audio program mapping. The existing
standalone stepper stores ROM instructions at normalized
zero-based offsets, so the entry PC becomes `0x1F16`.

Introduced an opt-in API:

```c
vf2_status vf2_m68k_irq_autovector_enter(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    unsigned level);
```

An accepted interrupt raises the IPL mask to its level,
switches to the supervisor stack where necessary, pushes
the **six-byte MC68000 exception frame** in big-endian order,
and changes PC to the autovector target:

```text
new SSP + 0..1  previous SR (S, IPL and X/N/Z/V/C)
new SSP + 2..5  previous PC
```

The opt-in context tracks `sr_high`, `usp`, `ssp` and
`irq_active`; the stepper preserves its previous flag
fields for existing test compatibility. When the actual
`RTE` instruction executes under a known stacked IRQ frame,
it restores SR, flags, stack pointer and PC. The original
legacy sound fixtures, which enter handlers directly with
**no exception frame**, still stop on `RTE` as before.

The implementation refuses masked IRQs, malformed vectors,
misaligned/out-of-range supervisor stacks and **nested
interrupts** without pretending to emulate them. That
nested-IRQ limit is deliberate; true IPL arbitration is
outstanding. No cycle timestamps are generated.

## Verified real VF2.1 ISR

The opt-in owner-ROM test uses independently copied initial
RAM and SCSP states. It executes the *same original 68000
instructions* twice:

1. Old entry: synthetic direct PC `0x1F16`, stop before `RTE`
   at `0x1F7A`.
2. New entry: preserve interrupted PC `0x2242`, deliver level-2
   autovector 26, execute the original body from `0x1F16`
   through `0x1F7A`, execute the authentic **`RTE`**, and
   verify that PC resumes at `0x2242`.

The 53 original instructions before RTE agree with the
direct-entry oracle. The original bytes outside the stack
scratch region, SCSP register writes, sound CPU registers and
SCSP voice state match. The opt-in path restores the
supervisor stack pointer to `0x5000` and clears its active
interrupt flag.

Owner-ROM output, Linux CI executable from commit
`9589aeb32d98104595b05cff86fe33546199b855`:

```text
AKIRA_L2_AUTOVECTOR_FRAME matched=1 enter=0 direct=0 body=0 rte=0 body_steps=53 legacy_steps=53 rte_steps=1 restored_pc=02242 stack=05000 irq_active=0 cpu_cycles_not_modeled=1
AKIRA_M68K_SLICES_ORIGINAL_DIFF parity=1 descriptors=32 steps_full=588 steps_sliced=588 quanta=100 quantum_max=7 next_a0=02000 irq_and_cpu_cycles_not_modeled=1
AKIRA_ORIGINAL_WALK_STRICT parity=1 passes=32 pages=4 native_admitted=4 steps=588 pcm_bytes_changed=2039
```

Command used with the owner's uncommitted ROM:

```sh
vf2_m68k_l3_selection_tests /private/extracted-vf2-rom \
  --verify-akira-walk
```

It **exited 0**, and the full existing CI matrix
for the implementation (GCC, Clang, ASan/UBSan, MSVC,
SDL3 Linux/Windows and Python) passed.

## Work remaining before audio scheduling

This establishes the **architectural frame transition**
and an authentic original ISR-body replay, not timing.

- Add opcode-specific MC68000 cycle accounting, including
  operand addressing, branch outcomes, MOVEM size and memory
  contention. The conservative four-cycle-per-instruction
  impossibility proof of v0919 is *not* a cycle model.
- Recover any nested interrupt and pending IRQ register
  behavior before enabling repeated genuine audio IRQs.
- Keep CPU PC/SR/A7 persistent over the entire main loop,
  rather than resetting them between test L2/drain/walker
  calls. Split work at actual time boundaries.
- Trigger SCSP Timer-A events on 49-sample boundaries,
  after modeling reset/reload and the IRQ lines.
- Compare CPU interrupt/frame transitions against an
  independent 68000 reference, and rendered output
  against an accurate external SCSP-chip reference.
- Do not enable this partial IRQ helper implicitly
  in production host/session audio.

## v0921 correction — 68000 MOVEM reversed register mask

A **stronger** test after the initial v0920 run compared every
sound CPU D/A register plus the SCSP's internal voice/envelope
and MIDI state. It caught an emulator error: when
`MOVEM.L D0-D7/A0-A4,-(A7)` executed at `0x1F16`,
the prior implementation interpreted its predecrement
`0xFFF8` mask as if it were in normal register order.
The subsequent `MOVEM.L (A7)+,D0-D7/A0-A4` therefore
reloaded the old SSP **into A4** (0x5000 with direct entry,
0x4FFA with a stacked IRQ), even though the original A4
is the sound FIFO command count pointer (0x2506).

Commit `22381b2a1437a97212070044e4182a707193ed1a`
corrected the MC68000 predecrement register mapping.
The standalone ROM-free test now asserts all 13 saved
and restored registers and every stack-frame longword.

After the correction, the **strong owner-ROM parity passed**:

```text
AKIRA_L2_AUTOVECTOR_FRAME matched=1 enter=0 direct=0 body=0 rte=0 body_steps=53 legacy_steps=53 rte_steps=1 restored_pc=02242 stack=05000 irq_active=0 cpu_cycles_not_modeled=1
AKIRA_ORIGINAL_WALK_STRICT parity=1 passes=32 pages=4 native_admitted=4 steps=588 pcm_bytes_changed=2039
```

`--verify-akira-tima-10s` also now passes again, with
**9,000 ticks**, **288,000 native/original matching
PCM descriptor services**, and **2,205 PCM pages**.
The C walker and original ROM interpreter regenerated
**byte-for-byte identical 10-second WAV files** after
this opcode fix (SHA-256 in both paths:
`463d20c14383c408fd5b55dd78c36675ed3ca3c3bc06a76a92dd0e5f36de5b3e`).
The recordings are still sound-chip **model** outputs,
not independently captured arcade hardware.

Fixing A4 also corrected how the actual ROM driver counts
commands: the first FIFO drain rose from 12,176 to
14,665 executed instructions. Refer to the **v0921
erratum** in `audio_m68k_scheduler_capacity_v0919.md`
for the new lower bound and corrected optimistic
first-period limit of *one command*, not two.
