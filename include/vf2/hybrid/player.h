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

#endif
