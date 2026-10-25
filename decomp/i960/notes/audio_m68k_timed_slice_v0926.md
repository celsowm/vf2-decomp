# v0926 — timed live stream countdown in the original L2 Timer-A ISR

Date: 2026-10-25. Branch: `master`.

The previous v0925 path covered the complete all-idle L2 autovector
through RTE in **972 baseline MC68000 clocks**, but no record with
the armed bit set. This step admits only the first original live
worker branch, where the 16-bit countdown does not expire.

## Recovered hardware path

VF2.1 owner-ROM disassembly from
`decomp/i960/notes/audio_l2_stream_tick_v0852.md`:

| PC | Instruction | Clocks |
| --- | --- | ---: |
| `0x1F7C` | `BTST #7,$0000(A2)`, bit set | 16 |
| `0x1F82` | `BNE.W $1F88`, taken | 10 |
| `0x1F88` | `SUBQ.W #1,$0002(A2)` | 16 |
| `0x1F8C` | `BEQ.W $1F92`, not taken | 12 |
| `0x1F90` | `RTS` | 16 |
| **Worker total** | armed countdown, no expired event | **70** |

This replaces an idle worker's 44 clocks, adding **26 clocks**
to the whole ISR. The entry JSR, other seven idle records,
prescaler fast return, SCSP writes, MOVEM save/restore, and RTE
are unchanged.

| Exact controlled ROM scenario | Baseline cycles | Instructions |
| --- | ---: | ---: |
| all eight descriptors inactive | 972 | 52 |
| descriptor #3 active, countdown 5 → 4 | **998** | **54** |

The `SUBQ.W #1,d16(An)` timing follows Motorola MC68000:
8 clocks of B/W quick memory RMW plus 8 clocks for `d16(An)`.
The cycle-slice dispatcher restricts recognition to this precise
SUBQ, operand size, and displaced addressing mode. Other quick
memory forms remain `VF2_ERROR_UNSUPPORTED` at the boundary.

## Independent checks

The new public ROM-free test executes a byte fixture of the real
worker branch, proves 16-clock atomic memory countdown and
flags, and splits each instruction across cycle budgets. It also
checks the countdown-expired **branch into** the existing applier
at `0x1F92`, without executing or assigning timing to that applier.
Other quick memory modes still refuse before state mutation.

The owner-ROM opt-in checker runs both idle and one-live variants.
Each variant has independently cloned ROM-backed sound RAM,
SCSP internal state, and M68K registers for uninterrupted versus
cycle-sliced execution. The active leg explicitly arms descriptor
#3 (`0x3060`) with countdown 5 and pins countdown 4 after RTE.
It checks CPU registers/flags, whole RAM and SCSP internal
state, exact step counts, and full charged cycles. The
`SUBQ.W` byte and BEQ shape are guarded against ROM changes.

For the active leg, cycle slicing must execute **818 clocks**
before `0x1F76`; the 933-clock budget must stop **before** the
116-clock MOVEM restore. Then 116-clock MOVEM and 20-clock RTE
complete the **998-clock** ideal baseline.

Run with your private original VF2.1 sound ROM:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON -DVF2_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
vf2_m68k_l3_selection_tests /private/vf2-rom --verify-akira-walk
```

Expected owner-only markers (not verified here without ROM):

```text
AKIRA_L2_ISR_TIMING variant=idle matched=1 entry_cycles=44 body_cycles=908 rte_cycles=20 total_cycles=972 original_steps=52 timed_steps=52
AKIRA_L2_ISR_TIMING variant=one-active matched=1 entry_cycles=44 body_cycles=934 rte_cycles=20 total_cycles=998 original_steps=54 timed_steps=54
```

Important boundaries: the **expired counter** goes into an unmeasured
applier and remains deliberately unclocked; prescaler beat emit
is also unmeasured. SCSP timer IRQ acknowledgment,
arbitration, contested bus cycles, DMA and hardware-real-time
scheduling are not modeled. Do not infer hardware audio parity
from ROM-free CI passing.

Source for timing: Motorola/NXP MC68000UM section 8 and
[68000 quick-op/EA tables](https://wiki.neogeodev.org/index.php/68k_instructions_timings).
