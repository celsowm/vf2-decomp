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

#endif
