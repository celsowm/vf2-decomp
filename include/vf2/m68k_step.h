#ifndef VF2_M68K_STEP_H
#define VF2_M68K_STEP_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/scsp.h"
#include "vf2/status.h"

/* Minimal 68000 stepper for the recovered sound-corridor subset.
 *
 * This is a measurement oracle, not a CPU model: it executes only the
 * opcodes on already-recovered paths and fails closed
 * (VF2_ERROR_UNSUPPORTED) on everything else. Instruction admission
 * and length come from vf2_m68k_decode, so stepper and disassembler
 * agree by construction; a length mismatch trips UNSUPPORTED.
 *
 * Address map (mirrors sound_board.c's access-kind split):
 *   fetch and PC-relative data  -> rom (code lives at low addresses)
 *   0x600000 + rom_size         -> rom mirror (tables)
 *   0x800000 + sample size      -> sample_rom reads (writes refuse)
 *   0x100000 + 0x1000           -> scsp model (with its semantics)
 *   below ram_size              -> ram
 *   $400000/$400001             -> event log (mapped port, unmodeled
 *                                  behavior; reads give 0, writes are
 *                                  recorded) and execution continues
 *   anywhere else               -> VF2_ERROR_UNSUPPORTED (silicon
 *                                  traps unmapped accesses: fail closed).
 * Triangle tests assert the event log they expect (usually empty).
 */

enum {
    VF2_M68K_STEP_MAX_EVENTS = 64u
};

typedef struct vf2_m68k_regs {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint8_t x;
    uint8_t n;
    uint8_t z;
    uint8_t v;
    uint8_t c;
    /* Optional interrupt context for sound-MC68000 oracle:
     * sr_high contains upper SR bits including S/I2:I0;
     * irq_active tracks ONE stacked autovector exception.
     * Legacy RTE without a frame still terminates old fixtures. */
    uint16_t sr_high;
    uint32_t usp;
    uint32_t ssp;
    uint8_t irq_active;
} vf2_m68k_regs;

typedef struct vf2_m68k_bus_event {
    uint32_t address;
    uint32_t value;
    uint8_t size;
    uint8_t is_write;
} vf2_m68k_bus_event;

typedef struct vf2_m68k_bus {
    uint8_t *ram;
    size_t ram_size;
    const uint8_t *rom;
    size_t rom_size;
    const uint8_t *sample_rom;
    size_t sample_rom_size;
    vf2_scsp *scsp;
    vf2_m68k_bus_event events[VF2_M68K_STEP_MAX_EVENTS];
    size_t event_count;
} vf2_m68k_bus;

void vf2_m68k_regs_reset(vf2_m68k_regs *regs);

/* In-memory snapshot of stepper-side mutable state (oracle save/resume).
 *
 * Captures the register file, the full RAM image, the SCSP model state
 * and the bus event log. ROM/sample-ROM pointers are inputs, not state:
 * the SCSP struct is copied shallow, so a snapshot shares the sample
 * attachments of the bus it was captured from (both are immutable for
 * the run). Restore requires the destination bus to carry the same RAM
 * size; anything else is a fixture mismatch and fails closed.
 *
 * The C side (`vf2_sound_board`) needs no helper: its state is the
 * board struct itself and copies with assignment. */

typedef struct vf2_m68k_snapshot {
    vf2_m68k_regs regs;
    uint8_t *ram;
    size_t ram_size;
    vf2_scsp scsp;
    size_t event_count;
    vf2_m68k_bus_event events[VF2_M68K_STEP_MAX_EVENTS];
} vf2_m68k_snapshot;

/* Enter one 68000 level 1..7 autovector exception at a legal
 * instruction boundary. Reads vector 24+level from ROM, pushes the
 * architectural 6-byte SR/PC frame to supervisor RAM stack,
 * raises the SR interrupt mask, and preserves the interrupted PC.
 * ROM offsets are normalized from the sound CPU's 0x600000 map.
 * Requires adequate aligned supervisor RAM; rejects nested IRQs
 * and masked levels without changing CPU/RAM.
 *
 * This is NOT Timer A delivery or real-cycle scheduling. */
vf2_status vf2_m68k_irq_autovector_enter(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs, unsigned level
);

/* ROM-programmed Timer A: 49 SCSP sample frames at 44.1 kHz.
 * Model 2A sound CPU: 11,289,600 cycles/s. Therefore one ideal
 * Timer-A interval spans EXACTLY 12,544 MC68000 clocks.
 * This is a pure CPU-phase accumulator; event count is diagnostic.
 * It does not deliver, prioritize or acknowledge actual SCSP IRQs
 * and does not emulate contention or wait states. */
typedef struct vf2_m68k_timer_a_phase {
    uint32_t cpu_cycle_phase;
    uint64_t total_expiries;
} vf2_m68k_timer_a_phase;

vf2_status vf2_m68k_timer_a_advance(
    vf2_m68k_timer_a_phase *clock, uint32_t executed_cpu_cycles,
    uint32_t *new_expiries
);

/* Measured MC68000 autovector-entry exception overhead: 44 clock
 * cycles for the baseline instruction-free transition. Excludes
 * external bus wait states. No cycle is charged on refused entry.
 * This complements (does not replace) run_cycle_slice: IRQ
 * delivery timing and pending SCSP IRQ arbitration are not wired. */
vf2_status vf2_m68k_irq_autovector_enter_timed(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    unsigned level, uint32_t *entry_cycles
);

/* Execute as many fully timed MC68000 instructions as fit within
 * cycle_budget. Unknown opcode timings refuse BEFORE execution, with
 * VF2_ERROR_UNSUPPORTED and all completed prior work preserved.
 * Only the explicitly verified timing subset is admitted: MOVEQ,
 * NOP, BRA/BSR/Bcc/DBcc, selected JSR/JMP/LEA addressing,
 * the measured L2 ISR MOVE.W #imm,d16(An), MOVE.B d16(An),Dn,
 * CMPI.B #imm,Dn, TST, BTST #imm,d16(An), and
 * SUBQ.W #1,d16(An), MOVEM.L -(An)/(An)+, RTS and RTE.
 * All values are baseline Motorola 68000 clock cycles, excluding
 * optional external wait states and interrupt-entry overhead.
 *
 * Returns VF2_OK + suspended=1 when the NEXT instruction cannot
 * fit this budget, leaving PC at the instruction boundary.
 * The caller must model IRQ arrival separately. Zero cycles or
 * pointers are invalid. Does not silently use 4 cycles for an
 * unknown opcode. */
vf2_status vf2_m68k_run_cycle_slice(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    uint32_t stop_pc, uint32_t cycle_budget,
    uint32_t *cycles_done, uint32_t *steps_done,
    int *suspended
);

/* Run a cooperative, fixed-INSTRUCTION quantum. Unlike vf2_m68k_run,
 * reaching max_steps is NOT conflated with an unknown opcode:
 * returns VF2_OK and sets *suspended=1 if execution may resume.
 * A genuine unsupported instruction still returns ERROR_UNSUPPORTED
 * with suspended=0. On PC==stop_pc, completes (suspended=0).
 *
 * This preserves PC/D/A/flags/RAM/SCSP as in ordinary run.
 * Does NOT count MC68000 cycles, accept/stack hardware IRQs,
 * or simulate bus wait states. It is an executor building block
 * for a future timed preemption model, NOT a timing scheduler. */
vf2_status vf2_m68k_run_slice(
    vf2_m68k_bus *bus, vf2_m68k_regs *regs,
    uint32_t stop_pc, int stop_on_return,
    uint32_t max_instructions, uint32_t *steps_done,
    int *suspended
);

void vf2_m68k_snapshot_init(vf2_m68k_snapshot *snap);

vf2_status vf2_m68k_snapshot_capture(
    vf2_m68k_snapshot *snap,
    const vf2_m68k_bus *bus,
    const vf2_m68k_regs *regs
);

vf2_status vf2_m68k_snapshot_restore(
    const vf2_m68k_snapshot *snap,
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs
);

/* Field-wise equality over regs, RAM image, SCSP state and event log.
 * Returns nonzero when equal. Reports nothing; the caller pins. */
int vf2_m68k_snapshot_equal(
    const vf2_m68k_snapshot *a,
    const vf2_m68k_snapshot *b
);

void vf2_m68k_snapshot_destroy(vf2_m68k_snapshot *snap);

/* Run until pc == stop_pc (checked before fetch), a return with
 * stop_on_return set (rts/rte halt OK), step exhaustion, or an
 * unsupported/out-of-bounds condition. steps_done counts the
 * instructions actually executed. */
vf2_status vf2_m68k_run(
    vf2_m68k_bus *bus,
    vf2_m68k_regs *regs,
    uint32_t stop_pc,
    int stop_on_return,
    uint32_t max_steps,
    uint32_t *steps_done
);

#endif
