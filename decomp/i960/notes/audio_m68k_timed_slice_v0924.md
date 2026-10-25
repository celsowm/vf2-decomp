# v0924 — timed VF2.1 Timer-A reload and FIFO gate

Date: 2026-10-24. Branch: `master`.

The original L2 interrupt uses the following MC68000 sequence
immediately after its 13-register MOVEM save. This revision
admits exact **baseline 68000 instruction costs** for only
the existing stepper's recovered addressing forms:

| PC | Original instruction | Uncontended clocks |
| --- | --- | ---: |
| `0x1F1A` | `MOVE.W #$00CE,$0418(A5)` | 16 |
| `0x1F20` | `MOVE.W #$0040,$0422(A5)` | 16 |
| `0x1F26` | `MOVE.B $1506(A6),D0` | 12 |
| `0x1F2A` | `CMPI.B #$FF,D0` | 8 |
| `0x1F2E` | `BCC.W $1F76` **not taken** on ring count 0 | 12 |
| **Subtotal** | five original opcodes from `0x1F1A` to `0x1F32` | **64** |

The already-validated five-opcode fast-return helper from
`0x1F32` to `0x1F3A` adds **64** clocks, for a **128-clock,
ten-opcode** segment. The existing IRQ entry (44 clocks)
and MOVEM save (112 clocks) are separate, documented
quantities; this is **not** a complete ISR timing result.

The ROM-free stepper regression checks that a 15-cycle
budget refuses the first MOVE without mutation, that a
63-cycle budget executes exactly 52 cycles and suspends at
the BCC, and that a 12-cycle follow-up advances to the next
instruction boundary. It also checks that a different MOVE
addressing mode still returns `VF2_ERROR_UNSUPPORTED`.

The owner-ROM differential fixture is extended to start at
`0x1F1A`, compare the complete RAM/SCSP/CPU state against
uninterrupted ROM execution, and validate the 128-clock
segment. **Private-ROM execution must be re-run**; the
new fixture is not equivalent to a hardware measurement.

Commands:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON -DVF2_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
vf2_m68k_l3_selection_tests /private/vf2-rom --verify-akira-walk
```

References: Motorola/NXP MC68000UM, Section 8, MOVE
byte/word execution and immediate compare cycle tables;
the original ROM listing is in
`decomp/i960/notes/audio_l2_stream_tick_v0852.md`.
Timing excludes memory wait states, bus arbitration,
interrupt-pending semantics and SCSP timer side effects.
Real-time scheduling stays disabled.
