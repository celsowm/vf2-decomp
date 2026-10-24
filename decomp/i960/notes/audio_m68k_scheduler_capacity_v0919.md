# v0919 — Model 2A sound-MC68000 Timer-A CPU budget and FIFO backlog

Date: 2026-10-24. Scope: **diagnostic lower bounds** only; no sound
ROM, proprietary WAV, sample bank, original instruction dump, or
third-party emulator code is committed.

## New finding: one full synthetic Akira tick is physically impossible

The Model 2A/SCSP setup in upstream MAME
[`model2.cpp`](https://github.com/mamedev/mame/blob/master/src/mame/sega/model2.cpp)
configures the sound CPU as:

```text
M68000 clock = 45,158,400 / 4 = 11,289,600 Hz
SCSP clock   = 45,158,400 / 2
```

The VF2 sound ROM independently programs Timer A reload `0x00CE`
and enables its IRQ (v0918). On the 44,100 Hz SCSP sound sample
clock, Timer A theoretically expires every **49** frames:

```text
Timer A theoretical IRQ opportunities = 44,100 / 49 = 900 per sec
Optimistic M68000 CPU budget per interval = 11,289,600 / 900
                                         = 12,544 CPU cycles
Motorola 68000 minimum instruction execution = 4 cycles
Maximum theoretical instruction count = floor(12,544 / 4)
                                       = 3,136 instructions
```

The upper limit ignores interrupt-entry/exit cost, bus contention,
memory wait states, and realistic (often >4-cycle) opcode timing.
Thus it is **far too generous**, but can still prove some sampled
test intervals impossible. A tick satisfying the lower bound is
**not** proven to fit the actual hardware.

### Experiment

The existing owner-ROM
`vf2_m68k_l3_selection_tests /private/vf2 --verify-akira-tima-10s`
runs the original 68000 L2 ISR, drains the entire command FIFO and
runs one complete 32-descriptor `0x2242` walk for each synthetic
49-frame tick. v0919 instruments original opcode retirement at the
three boundaries and checks `4 * instructions > 12,544`.
The full C-versus-original walker differential still runs.

Measured with the private VF2.1 ROM and CI Linux artifact for
`e4ea07c9f534ed39bf5630d36dd1873eef5f3ec2`:

| Tick-0 segment | Original 68000 instructions | Optimistic minimum CPU cycles |
| --- | ---: | ---: |
| L2 ISR | 1,073 | 4,292 |
| Command FIFO, 32 descriptors drained | 12,176 | 48,704 |
| Stream walker, 32 voice records | 588 | 2,352 |
| **Total** | **13,837** | **55,348** |
| **One Timer-A interval available** | — | **12,544** |

Even under the impossibly fast instruction cost, this is a
**4.41× capacity violation**. Over the full ten-second synthetic
timeline, **36 of 9,000 intervals** exceed the lower-bound
capacity; maximum load is the first tick, with mean 646
retired MC68000 instructions per tick. The original **288,000**
PCM descriptors still matched the native-C walk, with
**2,205 pages** transferred and **zero RAM/SCSP differences**.

### FIFO upper bound on the first tick

The first three original FIFO dispatches take respectively
**336, 629 and 1,941 MC68000 instructions**. Reserving the
L2 and one full PCM walk leaves at most:

```text
optimistic CPU instructions remaining = 3,136 - 1,073 - 588 = 1,475
first FIFO command = 336 instructions -> 1,139 remain
second command = 629 -> 510 remain
third command = 1,941 -> cannot complete
```

As the original FIFO is ordered, at most **two of the 32**
queued commands can finish inside the same idealized interval
when reserving these same L2/walker operations. At least **30
whole commands** remain. The CPU could already be partway
through the third command when the IRQ arrives; this experiment
does not model interrupted instructions/registers.

The strict owner-ROM gate now emits:

```text
AKIRA_TIMA_CPU_BUDGET checked=9000 overloaded=36 first_l2=1073 first_drain=12176 first_walker=588 first_total=13837 first_minimum_cycles=55348 budget_cycles=12544 peak_tick=0 peak_steps=13837 first_fifo_commands=32 first_fifo_optimistic_completed_ceiling=2 first_fifo_backlog_floor=30 average_steps=646 lower_bound_only=1 one_scan_per_irq_proven=0
```

The measured owner-ROM test **exits 0**. Its purpose is to
**detect and report** the impossible batch/tick schedule,
not to declare the batch historically accurate.

## ROM-free regressions

`tools/python/audio_m68k_scheduler_budget.py` uses exact
rational arithmetic to compare a conservative 4-cycle lower
bound against the MAME CPU-clock / ROM Timer-A interval.
`tools/python/test_audio_m68k_scheduler_budget.py` tests
clock and period, a 3,136-instruction boundary, tick-0
13,837-instruction contradiction, and FIFO-completion prefix.
These tests run in the normal GitHub Actions Python tooling job.

Example independent calculation, with **derived counters only**:

```sh
python3 tools/python/audio_m68k_scheduler_budget.py \
  --l2-instructions 1073 \
  --drain-instructions 12176 \
  --walker-instructions 588
```

## Specific work needed next — not yet implemented

1. Give the sound-MC68000 emulator a persistent CPU execution
   context across interrupts: PC, D/A registers, supervisor SR,
   interrupt mask and stack states. Current isolated tests
   reset CPU registers before each L2/drain/walker segment.
2. Account for actual 68000 opcode cycles, branch-dependent timing,
   512-byte MOVEM loop timing, interrupt entry/return, and bus
   memory wait states. A 4-cycle floor is a contradiction
   detector **only**, not a scheduler.
3. Advance the SCSP Timer-A phase using 44.1kHz sample-frame
   events, deliver pending IRQs at real instruction boundaries,
   and preserve command FIFO plus partially executed handler
   frames between IRQ arrivals.
4. Test whether and how many `0x2242` descriptor scans occur
   between Timer-A interrupts from **observed persisted CPU PCs**,
   rather than hard-code one scan per tick.
5. Only then regenerate complete Akira PCM and compare first
   against a second accurate SCSP emulation reference before
   asserting actual arcade audio quality.

**Consequences:** Previously generated native-vs-original
WAVs remain bitwise equal **under the same imposed batch
schedule**, but those equal hashes do not rescue physically
impossible per-tick CPU workloads. Do not silently promote
the 900Hz opt-in preview into production playback.

## v0919-b — resumable sound-MC68000 instruction quantum

An explicitly separate `vf2_m68k_run_slice()` now runs a
fixed **instruction-count** quantum and returns two distinct
outcomes: `VF2_OK/suspended=1` on an exhausted quantum and
`VF2_ERROR_UNSUPPORTED/suspended=0` on a genuinely
unrecognized opcode. It retains original registers/PC, RAM,
SCSP and bus event state, and never resets the CPU between
slices. Normal `vf2_m68k_run()` behavior is unchanged.

ROM-free tests check pause/resume over loops, branches, a
subroutine call/return and sound-RAM writes; the result must
match an uninterrupted execution, including flags and memory.
Additional owner-ROM strict differential compares the genuine
first Akira note's 32-voice stream-walker circuit, run either
uninterrupted or in quanta of up to **seven instructions**:

```text
AKIRA_M68K_SLICES_ORIGINAL_DIFF parity=1 descriptors=32 steps_full=588 steps_sliced=588 quanta=100 quantum_max=7 next_a0=02000 irq_and_cpu_cycles_not_modeled=1
```

Executed against the owner's original VF2.1 ROM on 2026-10-24,
using the CI Linux binary for commit `44e73478039d455b7e5f2d9621476cfebfbff576`:
**exit code 0**. The original Akira strict 32-record branch
fixture also passed (`parity=1`, `pages=4`, `steps=588`).

**Important limit:** an instruction-count quantum is *not* an
MC68000-cycle budget. The register struct still lacks supervisor
SR/interrupt mask and complete interrupt exceptions; we have
**not** delivered a timer IRQ to this resuming CPU. The next
implementation step is a cycle-counted `run_slice` plus
accurate 68000 exception entry/RTE, preserving execution
across intervening SCSP Timer-A pending events. Do not treat
the 100 quanta as 100 real hardware interrupts.
