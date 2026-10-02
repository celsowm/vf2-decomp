#ifndef VF2_HYBRID_TASK_H
#define VF2_HYBRID_TASK_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

typedef enum vf2_hybrid_task_kind {
    VF2_HYBRID_TASK_NONE = 0,
    VF2_HYBRID_TASK_GAME_INFO,
    VF2_HYBRID_TASK_CAMERA,
    VF2_HYBRID_TASK_USER,
    VF2_HYBRID_TASK_SOUND,
    VF2_HYBRID_TASK_KILL_OSAGE,
    VF2_HYBRID_TASK_OSAGE0,
    VF2_HYBRID_TASK_OSAGE1,
    VF2_HYBRID_TASK_PLAYER,
    VF2_HYBRID_TASK_OBJECT,
    VF2_HYBRID_TASK_GAME_DISP,
    VF2_HYBRID_TASK_COLI
} vf2_hybrid_task_kind;

typedef struct vf2_hybrid_task_report {
    vf2_hybrid_task_kind kind;
    uint32_t entry_address;
    uint32_t exit_address;
    uint32_t registry_address;
    size_t task_bytes_written;
    size_t global_bytes_written;
    size_t camera_blocks_executed;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    int cpu_poststate_applied;
} vf2_hybrid_task_report;

typedef struct vf2_hybrid_scheduler_transition_report {
    size_t current_task_index;
    size_t next_task_index;
    size_t descriptors_scanned;
    uint32_t current_registry_address;
    uint32_t next_registry_address;
    uint32_t next_entry_address;
    uint32_t current_scratch_address;
    uint32_t next_scratch_address;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    int cpu_poststate_applied;
} vf2_hybrid_scheduler_transition_report;

typedef struct vf2_hybrid_scheduler_finish_report {
    size_t current_task_index;
    size_t inactive_descriptors_scanned;
    size_t final_task_index;
    uint32_t current_registry_address;
    uint32_t inactive_registry_address;
    uint32_t end_registry_address;
    uint32_t continuation_address;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    int cpu_poststate_applied;
} vf2_hybrid_scheduler_finish_report;

typedef struct vf2_hybrid_second_scheduler_report {
    size_t descriptors_scanned;
    size_t inactive_descriptors_scanned;
    size_t selected_task_index;
    uint32_t registry_start;
    uint32_t selected_registry_address;
    uint32_t selected_entry_address;
    uint32_t scheduler_entry_address;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    int cpu_poststate_applied;
} vf2_hybrid_second_scheduler_report;

/* Execute the observed second scheduler entry from the main-loop call at
 * 0x0000a010 through the callx into the first runnable task. The scan keeps
 * the observed timer/frame behavior and accepts the recovered runnable task
 * entry variants used by later repeated cycles. */
vf2_status vf2_hybrid_second_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
);

/* Execute one of the seven naturally runnable first-dispatch task bodies in
 * recovered C, including its architectural RET to the scheduler at 0x10dcc.
 * The caller must present the CPU exactly at the task entry with g13 pointing
 * at the supplied registry. Unsupported branches are rejected. */
vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
);

/* Raw dispatch core compiled from hybrid.c (no game_disp poststate
 * fixup).  Same contract as above. */
vf2_status vf2_hybrid_first_dispatch_task_execute_base(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
);

const char *vf2_hybrid_task_kind_name(vf2_hybrid_task_kind kind);

/* Advance the accepted first-dispatch scheduler path from the return checkpoint
 * of one task to the architectural entry of the next task. This replaces the
 * descriptor scan, timing-accounting and diagnostic-name helper calls with C. */
vf2_status vf2_hybrid_first_dispatch_scheduler_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t current_task_index,
    size_t next_task_index,
    uint32_t current_registry_address,
    uint32_t next_registry_address,
    uint32_t next_entry_address,
    vf2_hybrid_scheduler_transition_report *report
);

/* Finish the accepted first scheduler sweep after fa_osage1 returns. This
 * accounts the last runnable descriptor, scans the final inactive descriptor,
 * executes the end-of-pass diagnostic state in C, and returns architecturally
 * to the main loop at 0x0000a014. */
vf2_status vf2_hybrid_first_dispatch_scheduler_finish(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t current_task_index,
    uint32_t current_registry_address,
    vf2_hybrid_scheduler_finish_report *report
);

#endif
