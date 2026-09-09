#ifndef VF2_HYBRID_H
#define VF2_HYBRID_H

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



typedef enum vf2_hybrid_bridge_kind {
    VF2_HYBRID_BRIDGE_NONE = 0,
    VF2_HYBRID_BRIDGE_TEXTURE_BYTE_RUN,
    VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE,
    VF2_HYBRID_BRIDGE_TEXTURE_WORD_RUN,
    VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE,
    VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD,
    VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD,
    VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH,
    VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND,
    VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE,
    VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE,
    VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT,
    VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE,
    VF2_HYBRID_BRIDGE_DIAGNOSTIC_TEXT_COPY,
    VF2_HYBRID_BRIDGE_TILE_GLYPH_EXPAND,
    VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH,
    VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT,
    VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP,
    VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR,
    VF2_HYBRID_BRIDGE_COLOR_TABLE_REBUILD,
    VF2_HYBRID_BRIDGE_DISPLAY_COLOR_PROFILE_APPLY,
    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_UNIT_FILL,
    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS,
    VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_APPLY,
    VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT,
    VF2_HYBRID_BRIDGE_VIDEO_TABLE_EXPAND_128,
    VF2_HYBRID_BRIDGE_DISPLAY_TRANSFORM_DEFAULTS,
    VF2_HYBRID_BRIDGE_DISPLAY_RUNTIME_INITIALIZE,
    VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD,
    VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP,
    VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST,
    VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE,
    VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK,
    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE,
    VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY,
    VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP,
    VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE,
    VF2_HYBRID_BRIDGE_GAME_METER_UPDATE,
    VF2_HYBRID_BRIDGE_GAME_INPUT_UPDATE,
    VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE,
    VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE,
    VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX,
    VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX,
    VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE,
    VF2_HYBRID_BRIDGE_INTERRUPT_INPUT_RING,
    VF2_HYBRID_BRIDGE_INTERRUPT_RESTORE_PREFIX,
    VF2_HYBRID_BRIDGE_FRAME_TIMER_SUFFIX,
    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL,
    VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER,
    VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT,
    VF2_HYBRID_BRIDGE_INTERRUPT_GAME_STATE,
    VF2_HYBRID_BRIDGE_INTERRUPT_TILE_SYNC,
    VF2_HYBRID_BRIDGE_MAIN_POST_TIMER,
    VF2_HYBRID_BRIDGE_MAIN_CLEAR_PREFIX,
    VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER,
    VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX,
    VF2_HYBRID_BRIDGE_MAIN_TEXTURE_ORCHESTRATOR_CALL,
    VF2_HYBRID_BRIDGE_MAIN_FRAME_TIMER_CALL,
    VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER,
    VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL,
    VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT,
    VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC,
    VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC,
    VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE,
    VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE,
    VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY,
    VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE,
    VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK,
    VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE,
    VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE,
    VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE,
    VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE,
    VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT,
    VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE,
    VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE,
    VF2_HYBRID_BRIDGE_INPUT_RING_POLL,
    VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE,
    VF2_HYBRID_BRIDGE_GAME_EVENT_QUEUE_WRITE,
    VF2_HYBRID_BRIDGE_TEXTURE_MAINTENANCE,
    VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH,
    VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE,
    VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP,
    VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS,
    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE,
    VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END,
    VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A,
    VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B,
    VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE,
    VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE,
    VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN,
    VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL,
    VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE,
    VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE,
    VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY,
    VF2_HYBRID_BRIDGE_CAMERA_BIT7_INTERPRETER,
    VF2_HYBRID_BRIDGE_TEXTURE_SELECTOR_INTERPRETER,
    VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_INTERPRETER,
    VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_WRAPPER_INTERPRETER,
    VF2_HYBRID_BRIDGE_TEXTURE_DECODER_CONTINUATION_INTERPRETER,
    VF2_HYBRID_BRIDGE_RETURN_STUB,
    VF2_HYBRID_BRIDGE_COUNT
} vf2_hybrid_bridge_kind;

typedef struct vf2_hybrid_bridge_report {
    vf2_hybrid_bridge_kind kind;
    uint32_t entry_address;
    uint32_t exit_address;
    uint64_t iterations;
    uint64_t rows;
    uint64_t changed_values;
    size_t bytes_written;
    uint32_t max_recursion_depth;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    int cpu_poststate_applied;
} vf2_hybrid_bridge_report;


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

/* Execute one accepted post-scheduler texture bridge block in recovered C.
 * The current IP selects a complete byte/word decoder, symbol/pair table
 * builder, bounded inner run, recursive expansion or color-conversion
 * procedure. Both memory and architectural CPU state are advanced without
 * copying state from the reference interpreter. */
vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
);

const char *vf2_hybrid_bridge_kind_name(vf2_hybrid_bridge_kind kind);

#endif
