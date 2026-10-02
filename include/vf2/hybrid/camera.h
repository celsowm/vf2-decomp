#ifndef VF2_HYBRID_CAMERA_H
#define VF2_HYBRID_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

typedef enum vf2_hybrid_block_kind {
    VF2_HYBRID_BLOCK_NONE = 0,
    VF2_HYBRID_BLOCK_CAMERA_INITIALIZE,
    VF2_HYBRID_BLOCK_CAMERA_UPDATE,
    VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE
} vf2_hybrid_block_kind;

typedef struct vf2_hybrid_block_report {
    vf2_hybrid_block_kind kind;
    uint32_t entry_address;
    uint32_t exit_address;
    uint32_t registry_address;
    size_t task_bytes_written;
    size_t global_bytes_written;
    size_t viewport_entries_written;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    int viewport_executed;
    int fast_exit;
    int cpu_poststate_applied;
} vf2_hybrid_block_report;

/* Apply only the semantic memory effects of one accepted camera block. This
 * compatibility entry point is useful for isolated memory tests, but it does
 * not advance an i960 CPU. New hybrid runners should call
 * vf2_hybrid_camera_execute(). */
vf2_status vf2_hybrid_camera_apply(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t instruction_pointer,
    vf2_hybrid_block_report *report
);

/* Execute one accepted first-dispatch camera block entirely in recovered C.
 * Both machine memory and the architectural i960 post-state are produced by
 * C; no ROM-derived register synchronization is required. The current CPU IP
 * selects the block, and g13/r29 must identify the supplied task registry. */
vf2_status vf2_hybrid_camera_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_block_report *report
);

const char *vf2_hybrid_block_kind_name(vf2_hybrid_block_kind kind);

#endif
