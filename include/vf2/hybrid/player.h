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

#endif
