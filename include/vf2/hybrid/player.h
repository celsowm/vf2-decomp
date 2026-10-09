#ifndef VF2_HYBRID_PLAYER_H
#define VF2_HYBRID_PLAYER_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

/* Recover the measured fa_player 0x29414 type-0/6/8/10 compact paths.
 * The CPU must already be inside the callee (IP == 0x29414, frame pushed).
 * Type 0 stores zero at +0xc50. Types 6/8/10 with state-flag bit 19 clear
 * store (r9 * scale - scale) using the measured constant sets. Bit-19-set
 * siblings fail closed. */
vf2_status vf2_hybrid_player_29414_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured player flag tail at 0x180bc (warm + siblings).
 * IP must be 0x180bc with a pushed frame. Writes +0x5b4, player-flags
 * bit 8 and optionally +0x6d8; rets to the saved return. */
vf2_status vf2_hybrid_player_180bc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1441c..0x14428: set player-flags bit 7 and ret the player task. */
vf2_status vf2_hybrid_player_1441c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured fa_player 0x29598 bit-4 dispatch
 * (selector2_queue_path). IP must be 0x29598 with a pushed frame.
 * Three paths recovered: skip (g0 bit 4 clear), bbc-taken (g1=0),
 * addo-1 g1 (g1 != 15). Path D (g1 == 15, sub-calls) REFUSED
 * (sub-callees not yet recovered). */
vf2_status vf2_hybrid_player_29598_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x439ac: queue dedup-append (selector2 caller path).
 * IP must be 0x439ac with a pushed frame. Reads count at
 * 0x50406a (1B); if count >= 4 returns immediately. Otherwise
 * searches 0x504074[count+1..1] for g0; if found returns
 * (idempotent). Otherwise writes g0 to 0x504078[count] and
 * increments the count. Replaces the i960 search/append with
 * a counted loop. */
vf2_status vf2_hybrid_player_439ac_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x43888: selector2 queue entry (200 B, 6 paths).
 * IP must be 0x43888 with a pushed frame. Gates on
 * (0x50002c & 0xc) and 0x500068 bit 20 and a g0 magic-value
 * check; on accept, writes g0 to the 16-entry ring buffer
 * at 0x504020 indexed by 0x504003, and updates 0x504001 count.
 * Also pokes the video register at 0xe80004 with 33 and 0x421. */
vf2_status vf2_hybrid_player_43888_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0xcf04: post-frame IRQ handler (184 B, 2 paths).
 * IP must be 0xcf04 with a pushed frame. Sets bit 21 of
 * 0x500068, dispatches on bit 15 of 0x500068 (path A: set ->
 * r3 = *(0x50005b) [+1 if bit 21 of 0x500068 clear] mod 11,
 * store to 0x50005b and 0x500064, write 0x50a700 to 0x50a00c;
 * path B: clear -> r3 = *(0x500054), r3 = *(0x12508[r3*2]),
 * store to 0x500064, write 0x50a704 to 0x50a00c), then
 * common tail: clrbit 15 of 0x500068, REFUSE the sub-call to
 * 0x1fcc0 (display_profile_apply, not yet recovered), clrbit
 * 21 of 0x500068, ret. The clrbit 21 is NOT applied; the
 * dispatcher's interpreted fallback runs the call and the
 * clrbit 21 from 0xcfa4 onward. */
vf2_status vf2_hybrid_player_cf04_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1fee4: trivial init (36 B, 5 blocks).
 * IP must be 0x1fee4 with a pushed frame. Writes the IEEE 754
 * float 1.0 (0x3f800000) to 26 consecutive 4-byte locations
 * starting at 0x50a0e0 (covering 0x50a0e0..0x50a144). This is
 * the initializer called by 0x1ff0c. */
vf2_status vf2_hybrid_player_1fee4_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1ff0c: display_profile_mode_constants (240 B, 5
 * blocks, 3 paths). IP must be 0x1ff0c with a pushed frame.
 * REFUSES the sub-call to 0x1fee4 (the dispatcher's interpreted
 * fallback will run the 26-iter init); then dispatches on
 * 0x500064: == 10 -> write 2 floats; == 6 -> write 10 floats;
 * default -> ret. */
vf2_status vf2_hybrid_player_1ff0c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1fffc: display_color_profile_apply (88 B, 3
 * blocks, 2 paths). IP must be 0x1fffc with a pushed frame.
 * Reads 0x500064 and 0x500068; if bit 21 of 0x500068 clear,
 * uses the 0x500064 value; else uses 3. Shifts left by 8 to
 * get an index, reads 3 bytes from a ROM-resident table at
 * 0x6eeb8 + index, and stores them to 0x5000e0/0x5000e1/
 * 0x5000e2. REFUSES the sub-call to 0x2c38 (color_table_rebuild,
 * not yet recovered). */
vf2_status vf2_hybrid_player_1fffc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x4b410: video_command_submit (60 B, 1 block).
 * IP must be 0x4b410 with a pushed frame. Writes 1 to
 * 0x550000 (a control word), then writes 3 to 0x5502e0 (a
 * status), and 4 words (g0, g1, g2) to 0x5502e0 + 0..0xc. */
vf2_status vf2_hybrid_player_4b410_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x11704: video_table_expand_128 (64 B, 5 blocks).
 * IP must be 0x11704 with a pushed frame. Reads a 32-bit outer
 * count from 0x78d0c, then for each of those iterations
 * copies 128 bytes from 0x78d10 to 0x12800000. The copy is
 * byte-by-byte (ldob/st) with 4-byte pointer increments. */
vf2_status vf2_hybrid_player_11704_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x2eab8: display_runtime_initialize (364 B, 1 block).
 * IP must be 0x2eab8 with a pushed frame. Performs a long
 * struct init at *0x500814 (offset 0x234..0x2cc) plus 3
 * work-RAM stores at 0x50a160..0x168 and a zero byte at
 * 0x50a14d. The sub-call to 0x31004 is fully inlined (writes
 * to *0x50084c+0x40, +0x54..+0x5c, +0x60..+0x68, +0x70). */
vf2_status vf2_hybrid_player_2eab8_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x2c38: color_table_rebuild (432 B, 11 blocks).
 * IP must be 0x2c38 with a pushed frame. Zeros the color
 * table at 0x546008..0x54612d, then runs a 27x47 nested loop
 * that fills 0x54612e..0x?? with computed color values.
 * The "subo 1, 0, g1" at 0x2d40 is the disasm-vs-executor
 * ambiguity flagged in v0755; the recovery matches the
 * executor's interpretation (g1 = 0xFFFFFFFF when g1 >= 256).
 * See color_table_rebuild_executor_v0755f.md for the design
 * rationale. Stand-alone execute hook, not yet wired. */
vf2_status vf2_hybrid_player_2c38_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x1fcc0: display_profile_apply (548 B, 15 blocks,
 * 6 sub-calls). IP must be 0x1fcc0 with a pushed frame.
 * Decides a mode byte (0x500064) from a 2+1 / 1+2 fighter-type
 * combo plus the pre-existing 0x500064/0x50004c/0x500068
 * state, and writes 0x500064/0x500068/0x50a000/0x50a004
 * accordingly. Then runs the 5 inlined sub-callees:
 *   0x1ff0c (mode constants, which itself inlines 0x1fee4),
 *   0x1fffc (color profile apply; refuses 0x2c38),
 *   0x4b410 (video command submit),
 *   0x2eab8 (display runtime initialize),
 *   0x11704 (video table expand_128).
 * The refused 0x2c38 sub-call comes from inside the inlined
 * 0x1fffc, so this function returns VF2_ERROR_UNSUPPORTED with
 * cpu->ip == 0x20050 (the 0x1fffc sub-call's post-return slot)
 * and the per-step loop's fallback will run the call+ret. */
vf2_status vf2_hybrid_player_1fcc0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x323fc: post_cf04_combat_state_clear (164 B, 7
 * blocks). IP must be 0x323fc with a pushed frame. Calls
 * 0xcf04 first (which has its own hybrid recovery v0747),
 * then clears bit 19 of 0x500068, stores 0 to (g13 + 0x47),
 * clears bit 8 of *(g13), writes 0x53f to 0x500024, writes
 * 100 to (g13 + 0x40), then dispatches on bit 0 of *(g13):
 *   - bit 0 set: read 0x500056, invert (toggle 0<->1), set
 *     bits 1 and 3 of *(g13), store 0x324a0 to (g13 + 0xc),
 *     ret to 0x324a0.
 *   - bit 0 clear (and bit 3 clear / bit 3 set): no-op, ret
 *     to 0x3244c.
 * g13 (cpu->registers[16+13]=29) points to a work-RAM struct
 * set up by the caller. Stand-alone execute hook, not yet
 * wired into the per-step hook table. */
vf2_status vf2_hybrid_player_323fc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover 0x32284: post_cf04_combat_state_clear_v18 sibling of
 * 0x323fc (v0756). Same shape as 0x323fc but clrbits bit 18 of
 * 0x500068 instead of bit 19, then jumps to the shared body at
 * 0x3240c. Stand-alone execute hook. */
vf2_status vf2_hybrid_player_32284_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Per-step hook instrumentation (v0755c).
 *
 * Read-only access to the per-hook fire counters maintained by
 * the per-step loop in hybrid_execute_interpreted_until. Each
 * counter records how many times the corresponding callee hook
 * was called. Ctest entries use this to verify the hook actually
 * fires (vs. the interpretation fallback being used).
 *
 * out_counts must point to an array of at least 4 uint64_t
 * values. The order is: 0=0x29598, 1=0x439ac, 2=0x43888,
 * 3=0xcf04. out_total (may be NULL) receives the sum of all
 * counters.
 */
void vf2_hybrid_get_callee_hook_counts(
    uint64_t *out_counts,
    uint64_t *out_total);

/* Reset all per-hook fire counters to zero. Use at the start of
 * a test to scope counter measurements. */
void vf2_hybrid_reset_callee_hook_counts(void);

/* Public wrapper (v0755c) for the per-step loop in
 * hybrid_execute_interpreted_until. Same semantics as the
 * internal function: cpu->ip must equal entry_address with a
 * pushed frame, and the loop runs until cpu->ip == stop_address
 * or a registered hook fires (and either returns VF2_OK to
 * continue or VF2_ERROR_UNSUPPORTED for clean refusal). The
 * per-step loop is used only for ranges that have at least one
 * registered callee hook; other ranges use vf2_i960_run.
 */
vf2_status vf2_hybrid_run_interpreted_until(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry_address,
    uint32_t stop_address);

#endif
