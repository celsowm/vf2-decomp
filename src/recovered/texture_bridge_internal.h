#ifndef VF2_TEXTURE_BRIDGE_INTERNAL_H
#define VF2_TEXTURE_BRIDGE_INTERNAL_H

#include "vf2/hybrid.h"
#include "vf2/analysis/orchestrator_gates.h"
#include "vf2/analysis/orchestrator_limits.h"
#include "vf2/analysis/orchestrator_scan.h"

#include <limits.h>
#include <string.h>

#define VF2_TEXTURE_BYTE_RUN_ENTRY UINT32_C(0x0004c868)
#define VF2_TEXTURE_BYTE_DECODE_ENTRY UINT32_C(0x0004c6e0)
#define VF2_TEXTURE_BYTE_RUN_EXIT UINT32_C(0x0004c878)
#define VF2_TEXTURE_WORD_RUN_ENTRY UINT32_C(0x0004cce8)
#define VF2_TEXTURE_WORD_DECODE_ENTRY UINT32_C(0x0004cc28)
#define VF2_TEXTURE_WORD_PREPARE_ENTRY UINT32_C(0x0004cb64)
#define VF2_TEXTURE_WORD_PREPARE_EXIT UINT32_C(0x0004cc28)
#define VF2_TEXTURE_WORD_PREPARE_TIMER_RETURN UINT32_C(0x0004cbb0)
#define VF2_TEXTURE_WORD_RUN_EXIT UINT32_C(0x0004ccf8)
#define VF2_TEXTURE_SYMBOL_TABLE_ENTRY UINT32_C(0x0004c3f0)
#define VF2_TEXTURE_PAIR_TABLE_ENTRY UINT32_C(0x0004c4d4)
#define VF2_TEXTURE_TREE_DISPATCH_ENTRY UINT32_C(0x0004c544)
#define VF2_TEXTURE_TREE_DISPATCH_EXIT UINT32_C(0x0004c6e0)
#define VF2_TEXTURE_TREE_RETURN UINT32_C(0x0004c5dc)
#define VF2_TEXTURE_TREE_ENTRY UINT32_C(0x0004c928)
#define VF2_TEXTURE_CONVERT_ENTRY UINT32_C(0x0004ce88)
#define VF2_TEXTURE_ADDRESS_TABLE_ENTRY UINT32_C(0x0004d16c)
#define VF2_DIAGNOSTIC_TEXT_COPY_ENTRY UINT32_C(0x00007fc0)
#define VF2_TILE_GLYPH_EXPAND_ENTRY UINT32_C(0x0004f944)
#define VF2_VIDEO_STATUS_LATCH_ENTRY UINT32_C(0x00002ec4)
#define VF2_GEOMETRY_FRAME_COMMIT_ENTRY UINT32_C(0x00002edc)
#define VF2_GEOMETRY_COMMAND_SETUP_ENTRY UINT32_C(0x00002f5c)
#define VF2_FRAME_SCRATCH_CLEAR_ENTRY UINT32_C(0x0000a154)
#define VF2_COLOR_TABLE_REBUILD_ENTRY UINT32_C(0x00002c38)
#define VF2_DISPLAY_COLOR_PROFILE_APPLY_ENTRY UINT32_C(0x0001fffc)
#define VF2_DISPLAY_COLOR_PROFILE_APPLY_RETURN UINT32_C(0x00020050)
#define VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY UINT32_C(0x0001fee4)
#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY UINT32_C(0x0001ff0c)
#define VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN UINT32_C(0x0001ff10)
#define VF2_DISPLAY_PROFILE_APPLY_ENTRY UINT32_C(0x0001fcc0)
#define VF2_DISPLAY_PROFILE_APPLY_MODE_RETURN UINT32_C(0x0001fdd4)
#define VF2_DISPLAY_PROFILE_APPLY_COLOR_RETURN UINT32_C(0x0001fe64)
#define VF2_DISPLAY_PROFILE_APPLY_COMMAND_RETURN UINT32_C(0x0001fe78)
#define VF2_DISPLAY_PROFILE_APPLY_RUNTIME_RETURN UINT32_C(0x0001fedc)
#define VF2_DISPLAY_PROFILE_APPLY_TABLE_RETURN UINT32_C(0x0001fee0)
#define VF2_VIDEO_COMMAND_SUBMIT_ENTRY UINT32_C(0x0004b410)
#define VF2_VIDEO_TABLE_EXPAND_128_ENTRY UINT32_C(0x00011704)
#define VF2_VIDEO_TABLE_EXPAND_128_COUNT UINT32_C(0x00078d0c)
#define VF2_VIDEO_TABLE_EXPAND_128_SOURCE UINT32_C(0x00078d10)
#define VF2_VIDEO_TABLE_EXPAND_128_DESTINATION UINT32_C(0x12800000)
#define VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY UINT32_C(0x0002eab8)
#define VF2_DISPLAY_RUNTIME_INITIALIZE_CHILD_RETURN UINT32_C(0x0002ec20)
#define VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY UINT32_C(0x00031004)
#define VF2_PALETTE_PAGE_UPLOAD_ENTRY UINT32_C(0x00002de4)
#define VF2_TEXTURE_COLOR_PREPARE_ENTRY UINT32_C(0x0004cd18)
#define VF2_TEXTURE_COLOR_PREPARE_EXIT UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_COLOR_PREPARE_TIMER_RETURN UINT32_C(0x0004cd64)
#define VF2_TEXTURE_CONVERT_LOOP_ENTRY UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_CONVERT_POST_ENTRY UINT32_C(0x0004cdd4)
#define VF2_TIMER_WAIT_UPDATE_ENTRY UINT32_C(0x00000b6c)
#define VF2_INLINE_TEXT_THUNK_ENTRY UINT32_C(0x00009444)
#define VF2_TEXTURE_STATUS_LINE_ENTRY UINT32_C(0x0004d2c0)
#define VF2_GAME_STATE_CLASSIFY_ENTRY UINT32_C(0x0000281c)
#define VF2_GAME_COLOR_LOOKUP_ENTRY UINT32_C(0x000026ec)
#define VF2_GAME_THRESHOLD_EVALUATE_ENTRY UINT32_C(0x000028d4)
#define VF2_GAME_METER_UPDATE_ENTRY UINT32_C(0x000020f0)
#define VF2_GAME_METER_COMPONENT_ENTRY UINT32_C(0x00002548)
#define VF2_GAME_METER_VALUE_ENTRY UINT32_C(0x00002640)
#define VF2_GAME_METER_OFFSET_ENTRY UINT32_C(0x000026b0)
#define VF2_GAME_INPUT_UPDATE_ENTRY UINT32_C(0x00001abc)
#define VF2_GAME_STATE_UPDATE_ENTRY UINT32_C(0x00001f5c)
#define VF2_TILE_CONTROLLER_UPDATE_ENTRY UINT32_C(0x0004e808)
#define VF2_FRAME_TIMER_PREFIX_ENTRY UINT32_C(0x00010f08)
#define VF2_INTERRUPT_SAVE_PREFIX_ENTRY UINT32_C(0x00000bc0)
#define VF2_INTERRUPT_BUFFER_GATE_ENTRY UINT32_C(0x00000c0c)
#define VF2_INTERRUPT_INPUT_RING_ENTRY UINT32_C(0x00000c94)
#define VF2_INTERRUPT_RESTORE_PREFIX_ENTRY UINT32_C(0x00000ce0)
#define VF2_FRAME_TIMER_SUFFIX_ENTRY UINT32_C(0x00010fa4)
#define VF2_TEXTURE_STATUS_TAIL_ENTRY UINT32_C(0x0004d25c)
#define VF2_INTERRUPT_PLAYER_LAYER_ENTRY UINT32_C(0x00000c78)
#define VF2_INTERRUPT_GAME_INPUT_ENTRY UINT32_C(0x00000c80)
#define VF2_INTERRUPT_GAME_STATE_ENTRY UINT32_C(0x00000c90)
#define VF2_INTERRUPT_TILE_SYNC_ENTRY UINT32_C(0x00000cd4)
#define VF2_MAIN_POST_TIMER_ENTRY UINT32_C(0x0000a038)
#define VF2_MAIN_CLEAR_PREFIX_ENTRY UINT32_C(0x00009fb0)
#define VF2_MAIN_FINAL_CLUSTER_ENTRY UINT32_C(0x00009ff8)
#define VF2_MAIN_GEOMETRY_PREFIX_ENTRY UINT32_C(0x0000a014)
#define VF2_MAIN_TEXTURE_ORCHESTRATOR_CALL_ENTRY UINT32_C(0x0000a030)
#define VF2_MAIN_FRAME_TIMER_CALL_ENTRY UINT32_C(0x0000a034)
#define VF2_INTERRUPT_INITIAL_CLUSTER_ENTRY UINT32_C(0x00000c00)
#define VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY UINT32_C(0x0006dcb8)
#define VF2_VIDEO_INPUT_SYNC_ENTRY UINT32_C(0x000110f4)
#define VF2_FRAME_COUNTER_ADVANCE_ENTRY UINT32_C(0x000112f8)
#define VF2_FRAME_PHASE_ADVANCE_ENTRY UINT32_C(0x00011c78)
#define VF2_FRAME_SHADOW_VERIFY_ENTRY UINT32_C(0x00000530)
#define VF2_FRAME_BUFFER_GATE_ENTRY UINT32_C(0x000110b0)
#define VF2_FRAME_DISPATCH_TICK_ENTRY UINT32_C(0x0000a6c0)
#define VF2_FRAME_GEOMETRY_GATE_ENTRY UINT32_C(0x0000a748)
#define VF2_VIDEO_REGISTER_COMPOSE_ENTRY UINT32_C(0x00001064)
#define VF2_VIDEO_INPUT_LATCH_WRITE_ENTRY UINT32_C(0x00001290)
#define VF2_PLAYER_UPDATE_GATE_ENTRY UINT32_C(0x00024534)
#define VF2_VIDEO_LAYER_COMMIT_ENTRY UINT32_C(0x00023f6c)
#define VF2_INPUT_BIT0_SEQUENCE_GATE_ENTRY UINT32_C(0x00001e6c)
#define VF2_INPUT_BIT1_SEQUENCE_GATE_ENTRY UINT32_C(0x00001edc)
#define VF2_INPUT_RING_POLL_ENTRY UINT32_C(0x000012d8)
#define VF2_TILE_RUNTIME_GATE_ENTRY UINT32_C(0x00044268)
#define VF2_MEMORY_PROBE_ADDRESS UINT32_C(0x01d00000)
#define VF2_GAME_EVENT_QUEUE_WRITE_ENTRY UINT32_C(0x000438ec)
#define VF2_TEXTURE_MAINTENANCE_ENTRY UINT32_C(0x0004b8d8)
#define VF2_TEXTURE_MAINTENANCE_CHECK_ENTRY UINT32_C(0x0004b914)
#define VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY UINT32_C(0x0004ba80)
#define VF2_TEXTURE_UPLOAD_DISPATCH_TARGET UINT32_C(0x00002de4)
#define VF2_TEXTURE_UPLOAD_DISPATCH_RETURN UINT32_C(0x0004bab4)
#define VF2_GAME_STATE_RETURN_STUB UINT32_C(0x000020ec)
#define VF2_TEXTURE_ORCHESTRATOR_ENTRY_GATE UINT32_C(0x0004bd00)
#define VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY UINT32_C(0x0004bd5c)
#define VF2_TEXTURE_RECORD_STATUS_SETUP_EXIT UINT32_C(0x0004bde0)
#define VF2_TEXTURE_STREAM_HEADER_CALL_ENTRY UINT32_C(0x0004be6c)
#define VF2_TEXTURE_STREAM_EXPAND_ENTRY UINT32_C(0x0004c9dc)
#define VF2_TEXTURE_STREAM_EXPAND_RETURN UINT32_C(0x0004be80)
#define VF2_TEXTURE_STREAM_RESUME_GATE_ENTRY UINT32_C(0x0004be80)
#define VF2_TEXTURE_STREAM_HEADER_CALL_RETURN UINT32_C(0x0004bebc)
#define VF2_GAME_EVENT_QUEUE_MMIO UINT32_C(0x00e80004)
#define VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY UINT32_C(0x0004bb18)
#define VF2_TEXTURE_ORCHESTRATOR_BODY_ENTRY UINT32_C(0x0004bcd4)
#define VF2_TEXTURE_ORCHESTRATOR_BODY_RETURN UINT32_C(0x0004bb94)
#define VF2_TEXTURE_FRAME_GATE_ENTRY UINT32_C(0x0004bcd4)
#define VF2_TEXTURE_FRAME_GATE_LATCH UINT32_C(0x0050006d)
#define VF2_TEXTURE_DEFAULT_LIMITS_ENTRY UINT32_C(0x0004bfe0)
#define VF2_TEXTURE_DEFAULT_LIMITS_RETURN UINT32_C(0x0004bd00)
#define VF2_TEXTURE_STATUS_DISPATCH_ENTRY UINT32_C(0x0004bd24)
#define VF2_TEXTURE_STATUS_DISPATCH_TARGET UINT32_C(0x0004d2c0)
#define VF2_TEXTURE_STATUS_DISPATCH_RETURN UINT32_C(0x0004bd5c)
#define VF2_TEXTURE_ACTIVE_PREPARE_ENTRY UINT32_C(0x0004bde0)
#define VF2_TEXTURE_ACTIVE_PREPARE_TARGET UINT32_C(0x0004d16c)
#define VF2_TEXTURE_ACTIVE_PREPARE_RETURN UINT32_C(0x0004be6c)
#define VF2_TEXTURE_ACTIVE_FLAGS UINT32_C(0x0055c2f4)
#define VF2_TEXTURE_COORD_TABLE UINT32_C(0x0004c120)
#define VF2_TEXTURE_HEADER_DECODE_ENTRY UINT32_C(0x0004c180)
#define VF2_TEXTURE_HEADER_DECODE_EXIT UINT32_C(0x0004c3f0)
#define VF2_TEXTURE_HEADER_STATE UINT32_C(0x00550080)
#define VF2_TEXTURE_HEADER_OUTPUT UINT32_C(0x0055c320)
#define VF2_TEXTURE_RECORD_START UINT32_C(0x00550168)
#define VF2_TEXTURE_RECORD_END UINT32_C(0x005502a8)
#define VF2_TEXTURE_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_TEXTURE_RECORD_ADVANCE_ENTRY UINT32_C(0x0004bf60)
#define VF2_TEXTURE_RECORD_ADVANCE_EXIT UINT32_C(0x0004bd24)
#define VF2_TEXTURE_FINAL_STATUS_ENTRY UINT32_C(0x0004bf90)
#define VF2_TEXTURE_STATUS_WORD UINT32_C(0x0055c2f0)
#define VF2_TEXTURE_COUNTER0 UINT32_C(0x005502c0)
#define VF2_TEXTURE_COUNTER1 UINT32_C(0x005502d0)
#define VF2_TEXTURE_COUNTER2 UINT32_C(0x005502e0)
#define VF2_TEXTURE_FINAL_STATUS_TARGET UINT32_C(0x0004d25c)
#define VF2_TEXTURE_FINAL_STATUS_RETURN UINT32_C(0x0004bfdc)
#define VF2_TEXTURE_BODY_RETURN_ENTRY UINT32_C(0x0004bfdc)
#define VF2_TEXTURE_POST_BODY_CALL_ENTRY UINT32_C(0x0004bb94)
#define VF2_TEXTURE_POST_BODY_CALL_TARGET UINT32_C(0x0004b8d8)
#define VF2_TEXTURE_POST_BODY_CALL_RETURN UINT32_C(0x0004bb98)
#define VF2_TEXTURE_COUNTER_UPDATE_ENTRY UINT32_C(0x0004bb98)
#define VF2_TEXTURE_COUNTER_UPDATE_EXIT UINT32_C(0x0004bc58)
#define VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY UINT32_C(0x0004bc58)
#define VF2_TEXTURE_TREE_TABLE UINT32_C(0x0004ad78)
#define VF2_TEXTURE_CONVERT_STATE UINT32_C(0x005500f4)
#define VF2_TEXTURE_CONVERT_SOURCE UINT32_C(0x0055c2ec)
#define VF2_TEXTURE_CONVERT_ODD UINT32_C(0x0055c2ef)
#define VF2_TEXTURE_CONVERT_EVEN UINT32_C(0x0055c2ee)
#define VF2_FRAME_STATE UINT32_C(0x00500000)
#define VF2_FRAME_WAIT UINT32_C(0x0050008c)
#define VF2_TEXTURE_MAX_LOOP UINT32_C(0x00100000)

typedef struct texture_tree_stats {
    uint64_t instructions;
    uint64_t nested_calls;
    uint64_t writes;
    size_t bytes_written;
    uint32_t max_nested_depth;
} texture_tree_stats;

typedef struct texture_bit_reader {
    uint32_t next_address;
    uint32_t accumulator;
    uint32_t available_bits;
    uint32_t next_word;
    uint32_t last_shifted_word;
} texture_bit_reader;

/* Inline conditions / helper functions to share across modular files */
static inline void set_equal_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
}

static inline void set_signed_condition(
    vf2_i960_cpu *cpu,
    int32_t left,
    int32_t right
)
{
    uint32_t condition = UINT32_C(2);
    vf2_i960_compare_result result = VF2_I960_COMPARE_EQUAL;

    if (left < right) {
        condition = UINT32_C(4);
        result = VF2_I960_COMPARE_LESS;
    } else if (left > right) {
        condition = UINT32_C(1);
        result = VF2_I960_COMPARE_GREATER;
    }
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition;
    cpu->compare_result = result;
}

static inline void set_greater_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
}

static inline void set_unsigned_condition(
    vf2_i960_cpu *cpu,
    uint32_t left,
    uint32_t right
)
{
    uint32_t condition = UINT32_C(2);
    vf2_i960_compare_result result = VF2_I960_COMPARE_EQUAL;

    if (left < right) {
        condition = UINT32_C(4);
        result = VF2_I960_COMPARE_LESS;
    } else if (left > right) {
        condition = UINT32_C(1);
        result = VF2_I960_COMPARE_GREATER;
    }
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition;
    cpu->compare_result = result;
}

static inline void account_nested_procedure(
    vf2_i960_cpu *cpu,
    uint64_t calls,
    uint64_t returns
)
{
    const uint32_t nested_depth = cpu->local_frame_depth + (calls != 0u ? 1u : 0u);

    cpu->procedure_calls += calls;
    cpu->procedure_returns += returns;
    if (nested_depth > cpu->maximum_local_frame_depth) {
        cpu->maximum_local_frame_depth = nested_depth;
    }
}

static inline void finish_recovered_control_block(
    vf2_i960_cpu *cpu,
    uint32_t exit_address,
    uint64_t instructions
)
{
    cpu->executed_instructions += instructions;
    cpu->ip = exit_address;
}

/* Shared helper utility function declarations */
vf2_status write_u16(vf2_model2a *machine, uint32_t address, uint16_t value);
vf2_status read_u16(const vf2_model2a *machine, uint32_t address, uint16_t *value);
vf2_status finish_recovered_procedure(vf2_model2a *machine, vf2_i960_cpu *cpu, uint64_t instructions);

/* TEXTURE Subsystem */
vf2_status execute_texture_byte_run(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_byte_decode(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_word_run(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_word_prepare(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_word_decode(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_symbol_table_build(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_pair_table_build(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_tree_dispatch(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_tree(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_color_prepare(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_convert(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_orchestrator_gate(const vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_header_decode(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_active_prepare_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_status_dispatch_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_record_advance(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_final_status_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_body_return(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_post_body_call(vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_counter_update(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_orchestrator_epilogue(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_orchestrator_save_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_frame_gate_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_default_limits(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_status_tail(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_maintenance(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_upload_dispatch(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_orchestrator_entry_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_record_status_setup(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_stream_header_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_stream_resume_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_convert_loop(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_convert_post(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_timer_wait_update(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);

/* VIDEO Subsystem */
vf2_status execute_video_status_latch(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_color_table_rebuild(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_display_color_profile_apply(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_display_profile_unit_fill(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_display_profile_mode_constants(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_display_profile_apply(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_video_command_submit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_video_table_expand_128(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_display_transform_defaults(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_display_runtime_initialize(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_palette_page_upload(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_video_register_compose(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_video_input_latch_write(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_video_layer_commit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_tile_controller_update(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);

/* GEOMETRY Subsystem */
vf2_status execute_geometry_frame_commit(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_geometry_command_setup(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_scratch_clear(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_geometry_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);

/* INPUT Subsystem */
vf2_status execute_video_input_sync(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_input_ring_poll(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_tile_runtime_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_game_event_queue_write(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_game_input_update(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_input_sequence_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry_ip,
    uint32_t active_counter,
    uint32_t passive_counter,
    vf2_hybrid_bridge_kind report_kind,
    vf2_hybrid_bridge_report *report
);
vf2_status execute_diagnostic_text_copy(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status copy_diagnostic_text(vf2_model2a *machine, uint32_t source, uint32_t destination, uint64_t *characters_written);
vf2_status execute_tile_glyph_expand(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);

/* MATCH/GAMEPLAY Subsystem */
vf2_status execute_game_state_classify(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_game_color_lookup(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_game_threshold_evaluate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_game_meter_update(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_game_state_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    bool leave_return_stub
);
vf2_status execute_inline_text_thunk(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_status_line(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_system_memory_diagnostic(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_counter_advance(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_phase_advance(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_shadow_verify(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_buffer_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_dispatch_tick(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_player_update_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_texture_address_table(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);

/* Interrupt/Wait Handling (Match Subsystem) */
vf2_status execute_frame_timer_prefix(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_save_prefix(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_buffer_gate(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_input_ring(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_restore_prefix(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_frame_timer_suffix(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_player_layer(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_game_input(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_game_state(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_tile_sync(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_main_post_timer(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_main_clear_prefix(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_main_final_cluster(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_main_geometry_prefix(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_main_texture_orchestrator_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_main_frame_timer_call(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);
vf2_status execute_interrupt_initial_cluster(vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report);

#endif
