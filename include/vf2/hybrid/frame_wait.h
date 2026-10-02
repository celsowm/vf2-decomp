#ifndef VF2_HYBRID_FRAME_WAIT_H
#define VF2_HYBRID_FRAME_WAIT_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"
#include "vf2/hybrid/bridge.h"

typedef struct vf2_hybrid_frame_wait_state {
    size_t visits;
    size_t visits_before_interrupt;
    size_t interrupts_injected;
} vf2_hybrid_frame_wait_state;

typedef struct vf2_hybrid_frame_wait_report {
    uint32_t wait_address;
    uint32_t interrupt_vector;
    uint32_t interrupt_handler;
    size_t visit_count;
    int wait_observed;
    int interrupt_injected;
} vf2_hybrid_frame_wait_report;

/* Initialize the deterministic host-side frame event scheduler used by the
 * recovered bridge. The observed VF2 path injects one frame interrupt after
 * four visits to a recognized frame-wait address. */
vf2_status vf2_hybrid_frame_wait_initialize(
    vf2_hybrid_frame_wait_state *state,
    size_t visits_before_interrupt
);

/* Observe the CPU after one native/interpreted step. At 0x00000f7c or
 * 0x00010f98 the state machine counts a wait visit and, at the configured
 * threshold, raises Model 2 interrupt bit 0 and enters i960 vector 12. */
vf2_status vf2_hybrid_frame_wait_observe(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *state,
    vf2_hybrid_frame_wait_report *report
);

/* Recover one complete observed frame-wait phase. At 0x00010f90 this
 * executes the polling loop through interrupt injection. At 0x00000d20 it
 * returns from the interrupt, records the resumed wait visit and follows the
 * observed changed-frame-byte exit to 0x00010fa4. */
vf2_status vf2_hybrid_frame_wait_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *state,
    vf2_hybrid_bridge_report *report
);

#endif
