#ifndef VF2_NATIVE_RUNTIME_H
#define VF2_NATIVE_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/hybrid.h"
#include "vf2/status.h"

#define VF2_NATIVE_RUNTIME_STATE_VERSION 1u

typedef enum vf2_native_runtime_step_kind {
    VF2_NATIVE_RUNTIME_STEP_NONE = 0,
    VF2_NATIVE_RUNTIME_STEP_BRIDGE,
    VF2_NATIVE_RUNTIME_STEP_TASK,
    VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT,
    VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER,
    VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION,
    VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH,
    VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE1,
    VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE2,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INIT_PREFIX,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_INIT_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_RAMP,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COLOR_TABLES,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MEMORY_CLEAR,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_REGISTER_STREAM,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BLOCK_STREAM,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_SRAM_PROBE,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_RESTORE,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESTORED_VIDEO_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_SEED,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TABLE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_HARDWARE_CORE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_INIT_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_TIMER_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_VERIFY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_SETUP,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_LUMA_TABLE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN_RETURN,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_RETURN,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_STATE_RESET,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_CONSTANTS,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_CONSTANTS,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TASK_REGISTRY_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_BUFFER_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RENDER_STATE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DEFAULTS_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_OBJECT_TABLE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EFFECT_TABLE_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_RING_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_IO_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DATA_COPY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_OFFSET_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FRAME_ACCUMULATOR_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PROFILE_DEFAULTS_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAMEPLAY_GLOBALS_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FLOAT_DEFAULTS_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_LOAD,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_RAMP_ENTRY,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD_RETURN,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_WRAPPER_PREFIX,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_HELPER_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_TABLE,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_RETURN,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MAIN_LOOP_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COPRO_INIT,
    VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DELAY,
    VF2_NATIVE_RUNTIME_STEP_SCHEDULER_EPILOGUE,
    VF2_NATIVE_RUNTIME_STEP_SCHEDULER_SCAN,
    VF2_NATIVE_RUNTIME_STEP_OBJECT_SERVICE
} vf2_native_runtime_step_kind;

typedef struct vf2_native_runtime_state {
    vf2_hybrid_frame_wait_state frame_wait;
    size_t blocks_executed;
    size_t task_bodies_executed;
    size_t frame_wait_phases;
    size_t scheduler_entries;
    size_t scheduler_transitions;
    size_t scheduler_finishes;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
} vf2_native_runtime_state;

typedef struct vf2_native_runtime_step_report {
    vf2_native_runtime_step_kind kind;
    vf2_hybrid_bridge_kind bridge_kind;
    vf2_hybrid_task_kind task_kind;
    uint32_t entry_address;
    uint32_t exit_address;
    size_t current_task_index;
    size_t next_task_index;
    size_t descriptors_scanned;
    uint32_t current_registry_address;
    uint32_t next_registry_address;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
} vf2_native_runtime_step_report;

typedef struct vf2_native_runtime_run_report {
    uint32_t start_address;
    uint32_t stop_address;
    uint32_t final_address;
    size_t blocks_executed;
    size_t task_bodies_executed;
    size_t frame_wait_phases;
    size_t scheduler_entries;
    size_t scheduler_transitions;
    size_t scheduler_finishes;
    uint64_t recovered_instruction_count;
    uint64_t recovered_procedure_calls;
    uint64_t recovered_procedure_returns;
    uint32_t last_entry_address;
    vf2_native_runtime_step_kind last_step_kind;
    vf2_hybrid_bridge_kind last_bridge_kind;
    vf2_hybrid_task_kind last_task_kind;
    int reached_stop;
} vf2_native_runtime_run_report;

/* Initialize a persistent recovered-runtime state. The observed frame scheduler
 * injects vector 12 after four visits, but tests and future platform backends
 * may supply a different positive threshold. */
vf2_status vf2_native_runtime_initialize(vf2_native_runtime_state *state,
                                         size_t frame_wait_visits_before_interrupt);

/* Execute exactly one accepted recovered block at cpu->ip. The explicitly
 * labeled fighter-state bridge may execute the original i960 task until its
 * scheduler return; unknown addresses and other unobserved branches return
 * VF2_ERROR_UNSUPPORTED without changing aggregate counters. */
vf2_status vf2_native_runtime_step(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                   vf2_native_runtime_state *state,
                                   vf2_native_runtime_step_report *report);

/* Execute recovered blocks until stop_address is reached or max_blocks is
 * exhausted. Reaching the stop address before executing a block is a successful
 * zero-length run. Budget exhaustion returns VF2_ERROR_UNSUPPORTED and leaves a
 * complete partial report for diagnostics. */
vf2_status vf2_native_runtime_run_until(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_state *state,
                                        uint32_t stop_address, size_t max_blocks,
                                        vf2_native_runtime_run_report *report);

/* Execute recovered blocks until one complete recovered frame-wait phase has
 * completed. The machine, CPU and runtime state remain owned by the caller;
 * this is the frame-oriented entry point used by the game facade. */
vf2_status vf2_native_runtime_run_frame(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_state *state,
                                        size_t max_blocks,
                                        vf2_native_runtime_run_report *report);

/* Persist the host-side state that is intentionally outside the
 * architectural i960 snapshot. The fixed-width, little-endian
 * format is versioned and protected by CRC32. */
vf2_status vf2_native_runtime_state_write_file(const vf2_native_runtime_state *state,
                                               const char *path);

vf2_status vf2_native_runtime_state_read_file(vf2_native_runtime_state *state,
                                              const char *path);

#ifdef vf2_native_runtime_step_impl
#define vf2_native_runtime_step_kind_name vf2_native_runtime_step_kind_name_base
#endif
const char *vf2_native_runtime_step_kind_name(vf2_native_runtime_step_kind kind);

#endif