#ifndef VF2_RECOVERED_H
#define VF2_RECOVERED_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/analysis/tasks.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

typedef struct vf2_recovered_boot_state {
    uint32_t initial_entry;
    uint32_t initial_prcb;
    uint32_t system_address_table;
} vf2_recovered_boot_state;

typedef struct vf2_recovered_boot_stage1_report {
    uint32_t cpu_control_source;
    uint32_t cpu_control_destination;
    uint32_t interrupt_table_source;
    uint32_t interrupt_stack;
    uint32_t replacement_prcb;
    uint32_t iac_packet;
    uint32_t next_instruction;
    size_t cpu_control_bytes_copied;
    size_t interrupt_state_bytes_copied;
    size_t work_ram_words_cleared;
    size_t buffer_ram_words_cleared;
} vf2_recovered_boot_stage1_report;


typedef struct vf2_recovered_task_registry_report {
    uint32_t source_table_start;
    uint32_t source_table_end;
    uint32_t registry_start;
    uint32_t registry_end;
    uint32_t scratch_start;
    uint32_t scratch_end;
    size_t task_count;
    size_t state_pointers_written;
    size_t scratch_bytes_cleared;
} vf2_recovered_task_registry_report;


enum { VF2_RECOVERED_SCHEDULER_MAX_TASKS = 32u };

typedef struct vf2_recovered_scheduler_report {
    uint32_t registry_start;
    uint32_t registry_end;
    uint32_t scratch_start;
    uint32_t scratch_end;
    size_t descriptor_count;
    size_t runnable_count;
    size_t runnable_task_indices[VF2_RECOVERED_SCHEDULER_MAX_TASKS];
    uint32_t runnable_registry_addresses[VF2_RECOVERED_SCHEDULER_MAX_TASKS];
    uint32_t runnable_entry_points[VF2_RECOVERED_SCHEDULER_MAX_TASKS];
} vf2_recovered_scheduler_report;

typedef struct vf2_recovered_task_report {
    uint32_t entry_point;
    uint32_t registry_address;
    uint32_t continuation;
    size_t bytes_written;
    size_t global_bytes_written;
    uint32_t last_compare_result;
} vf2_recovered_task_report;

typedef struct vf2_recovered_camera_init_report {
    uint32_t entry_point;
    uint32_t registry_address;
    uint32_t continuation;
    size_t task_bytes_written;
    size_t global_bytes_written;
    size_t palette_entries_written;
} vf2_recovered_camera_init_report;

typedef struct vf2_recovered_camera_update_report {
    uint32_t start_address;
    uint32_t stop_address;
    uint32_t registry_address;
    uint32_t mode_handler;
    uint32_t range_flags;
    uint32_t input_flags;
    uint32_t fighter0_profile;
    uint32_t fighter1_profile;
    size_t task_bytes_written;
    size_t global_bytes_written;
    size_t copro_scratch_bytes_written;
    size_t helpers_recovered;
} vf2_recovered_camera_update_report;

typedef struct vf2_recovered_camera_gate_report {
    uint32_t start_address;
    uint32_t stop_address;
    uint32_t registry_address;
    uint32_t input_flags;
    uint32_t control_flags;
    uint32_t initial_task_flags;
    uint32_t final_task_flags;
    size_t task_flag_writes;
    size_t task_bytes_written;
    size_t viewport_entries_written;
    size_t viewport_task_bytes_written;
    int viewport_executed;
    int fast_exit;
} vf2_recovered_camera_gate_report;

typedef struct vf2_recovered_camera_projection_report {
    uint32_t helper_address;
    uint32_t low_offset;
    uint32_t high_offset;
    uint32_t fighter0_profile;
    uint32_t fighter1_profile;
    int32_t fighter0_weight;
    int32_t fighter1_weight;
} vf2_recovered_camera_projection_report;

typedef struct vf2_recovered_camera_viewport_report {
    uint32_t start_address;
    uint32_t stop_address;
    uint32_t registry_address;
    uint32_t input_flags;
    uint32_t first_lower;
    uint32_t first_upper;
    uint32_t first_center;
    uint32_t second_lower;
    uint32_t second_upper;
    uint32_t second_center;
    size_t first_entries_written;
    size_t second_entries_written;
    size_t task_bytes_written;
    size_t global_bytes_written;
    size_t copro_scratch_bytes_written;
    size_t helpers_recovered;
    int first_fixed_table;
    int second_fixed_table;
} vf2_recovered_camera_viewport_report;

typedef struct vf2_recovered_kill_osage_report {
    uint32_t entry_point;
    uint32_t first_registry_address;
    uint32_t second_registry_address;
    uint32_t elapsed_ticks;
    size_t records_evaluated;
    size_t records_marked_for_kill;
    size_t flag_words_written;
    /* vf2_i960_compare_result from the last ROM cmpo* in 0x65838. */
    uint32_t last_compare_result;
} vf2_recovered_kill_osage_report;

typedef struct vf2_recovered_timer_irq_report {
    uint32_t initial_request;
    uint32_t initial_enable;
    uint32_t final_request;
    uint32_t final_enable;
    uint32_t serviced_mask;
    uint32_t timer_index;
    uint32_t timer_reload;
    uint32_t wait_flag_address;
    size_t interrupts_serviced;
    int wait_released;
} vf2_recovered_timer_irq_report;

typedef struct vf2_recovered_boot_stage2_report {
    uint32_t start_address;
    uint32_t stop_address;
    uint64_t interpreted_instruction_equivalent;
    size_t io_reset_writes;
    size_t palette_entries_written;
    size_t color_entries_written;
    size_t tile_halfwords_cleared;
    size_t boot_palette_halfwords_written;
} vf2_recovered_boot_stage2_report;

void vf2_recovered_boot_entry(
    vf2_recovered_boot_state *state,
    uint32_t system_address_table,
    uint32_t initial_prcb,
    uint32_t initial_entry
);

vf2_status vf2_recovered_memory_clear_u32(
    uint8_t *memory,
    size_t memory_size,
    size_t byte_offset,
    size_t word_count
);

/*
 * Semantic C recovery of the complete 0x000000b0 -> 0x000001b0 startup
 * stage. The caller must attach the reconstructed main ROM to the machine.
 */
vf2_status vf2_recovered_boot_stage1_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_boot_stage1_report *report
);

/*
 * Semantic C recovery of the hardware-initialization prefix from
 * 0x000001b0 through 0x0000052c. Stage 1 must already have run.
 */
vf2_status vf2_recovered_boot_stage2_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_boot_stage2_report *report
);


/*
 * Semantic C recovery of the scheduler task-registry initializer at
 * 0x00010cbc. The catalog must have been recovered from the attached ROM.
 */
vf2_status vf2_recovered_task_registry_initialize(
    vf2_model2a *machine,
    const vf2_task_catalog *catalog,
    vf2_recovered_task_registry_report *report
);

/* Semantic C recovery of the vector-14 timer interrupt handler at 0x00000d50. */
vf2_status vf2_recovered_timer_irq_dispatch(
    vf2_model2a *machine,
    vf2_recovered_timer_irq_report *report
);

/* Semantic recovery of the scheduler registry scan at 0x00010d54.
 * This plans runnable tasks without invoking their still-original bodies. */
vf2_status vf2_recovered_scheduler_plan(
    const vf2_model2a *machine,
    const vf2_task_catalog *catalog,
    vf2_recovered_scheduler_report *report
);

/* Exact recovery of the observed no-write fa_game_info first-dispatch path.
 * Returns VF2_ERROR_UNSUPPORTED when the runtime takes an un-recovered branch. */
vf2_status vf2_recovered_task_game_info_first_dispatch(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
);

/* Exact recovery of the empty fa_user task at 0x00029748. */
vf2_status vf2_recovered_task_user_execute(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
);

/* Exact recovery of the fa_sound first-entry initializer at 0x000439fc. */
vf2_status vf2_recovered_task_sound_initialize(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
);

/* Exact recovery of the observed fa_osage first-dispatch initialization path.
 * Covers both task instances while the fighter secondary-simulation flag is clear. */
vf2_status vf2_recovered_task_osage_first_dispatch(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
);

/* Exact recovery of the observed camera initialization prefix from
 * 0x0001d320 through the continuation at 0x0001d458. The later camera update
 * loop remains interpreted. */
vf2_status vf2_recovered_task_camera_initialize(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_init_report *report
);

/* Exact recovery of the observed first recurring camera update from
 * 0x0001d458 through 0x0001d660. It includes helper 0x000214dc and the
 * observed early-return branches of 0x00020558 and 0x0001fc00. */
vf2_status vf2_recovered_task_camera_first_update(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_update_report *report
);

/* Recovery of the camera post-update gate beginning at 0x0001d660.
 * It composes the recovered viewport block when input bit 3 is set, then
 * covers the observed fast exit at 0x0001e524 or the control-flag path through
 * 0x0001d984. */
vf2_status vf2_recovered_task_camera_post_update_gate(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_gate_report *report
);

/* Complete recovery of camera range helper 0x0001fbb4. */
vf2_status vf2_recovered_camera_range_window(
    vf2_model2a *machine,
    uint32_t half_width,
    uint32_t *lower,
    uint32_t *upper,
    uint32_t *center
);

/* Recovery of the two-fighter projection helper at 0x0001eff0. */
vf2_status vf2_recovered_camera_project_fighter_ranges(
    vf2_model2a *machine,
    uint32_t low_offset,
    uint32_t high_offset,
    vf2_recovered_camera_projection_report *report
);

/* Complete table-construction helper at 0x0001facc. */
vf2_status vf2_recovered_camera_fill_viewport_table(
    vf2_model2a *machine,
    uint32_t lower,
    uint32_t upper,
    uint32_t entry_count,
    uint32_t step,
    uint32_t destination,
    uint32_t clamp_low,
    uint32_t clamp_high,
    size_t *entries_written
);

/* Recovery of the input-bit-3 viewport block 0x0001d678-0x0001d8e8. */
vf2_status vf2_recovered_task_camera_viewport_construct(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_viewport_report *report
);

/* Complete scalar recovery of camera helper 0x000214dc. */
uint32_t vf2_recovered_camera_classify_range(
    uint32_t first_value,
    uint32_t vertical_value,
    uint32_t second_value,
    uint32_t range_value,
    uint32_t vertical_limit
);

/* Complete semantic recovery of fa_kill_osage at 0x000657dc, including both
 * helper evaluations and the timer-derived aging accumulator. */
vf2_status vf2_recovered_task_kill_osage_execute(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_kill_osage_report *report
);

/* Compatibility wrapper when only machine state and the report are needed. */
vf2_status vf2_recovered_boot_stage1(
    vf2_model2a *machine,
    vf2_recovered_boot_stage1_report *report
);

#endif
