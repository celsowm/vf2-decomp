#include "texture_bridge_internal.h"

vf2_status execute_texture_status_tail_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
);

static vf2_status execute_texture_convert_post_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t save_base = UINT32_C(0x00550084);
    const uint32_t resume_slot = save_base + UINT32_C(108);
    uint32_t state = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_CONVERT_STATE, &state
    );
    if (status != VF2_OK) {
        return status;
    }
    if (state == 0u) {
        return execute_texture_convert_post(machine, cpu, report);
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(
        machine, VF2_TEXTURE_HEADER_STATE, UINT32_C(1)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, resume_slot, UINT32_C(0x0004cdd0)
        );
    }
    for (index = 0u; status == VF2_OK && index < 14u; ++index) {
        status = vf2_model2a_write_u32(
            machine,
            save_base + (uint32_t)index * UINT32_C(4),
            cpu->registers[VF2_I960_G0_REGISTER + 1u + index]
        );
    }
    for (index = 0u; status == VF2_OK && index < 13u; ++index) {
        status = vf2_model2a_write_u32(
            machine,
            save_base + UINT32_C(56) + (uint32_t)index * UINT32_C(4),
            cpu->registers[3u + index]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = resume_slot;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
    cpu->executed_instructions += UINT64_C(63);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST;
    report->entry_address = VF2_TEXTURE_CONVERT_POST_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(27);
    report->changed_values = UINT64_C(29);
    report->bytes_written = 116u;
    report->recovered_instruction_count = UINT64_C(63);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_return_stub(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t entry = cpu->ip;
    const uint32_t expected_exit =
        entry == VF2_TEXTURE_UPLOAD_DISPATCH_RETURN
            ? UINT32_C(0x00000c0c)
            : (entry == VF2_GAME_STATE_RETURN_STUB
                ? UINT32_C(0x00000c94)
                : 0u);
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (expected_exit == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != expected_exit) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    ++cpu->executed_instructions;

    report->kind = VF2_HYBRID_BRIDGE_RETURN_STUB;
    report->entry_address = entry;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report local_report;
    vf2_status status = VF2_ERROR_UNSUPPORTED;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    switch (cpu->ip) {
    case VF2_TEXTURE_UPLOAD_DISPATCH_RETURN:
    case VF2_GAME_STATE_RETURN_STUB:
        status = execute_return_stub(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_BYTE_RUN_ENTRY:
        status = execute_texture_byte_run(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_BYTE_DECODE_ENTRY:
        status = execute_texture_byte_decode(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_WORD_RUN_ENTRY:
        status = execute_texture_word_run(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_WORD_PREPARE_ENTRY:
        status = execute_texture_word_prepare(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_WORD_DECODE_ENTRY:
        status = execute_texture_word_decode(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_SYMBOL_TABLE_ENTRY:
        status = execute_texture_symbol_table_build(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_PAIR_TABLE_ENTRY:
        status = execute_texture_pair_table_build(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_TREE_DISPATCH_ENTRY:
        status = execute_texture_tree_dispatch(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_TREE_ENTRY:
        status = execute_texture_tree(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_COLOR_PREPARE_ENTRY:
        status = execute_texture_color_prepare(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_CONVERT_ENTRY:
        status = execute_texture_convert(machine, cpu, &local_report);
        break;
    case VF2_ORCHESTRATOR_CHILD_GATE_A_ENTRY:
    case VF2_ORCHESTRATOR_CHILD_GATE_B_ENTRY:
    case VF2_ORCHESTRATOR_LOOP_GATE_ENTRY:
        status = execute_texture_orchestrator_gate(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_HEADER_DECODE_ENTRY:
        status = execute_texture_header_decode(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ACTIVE_PREPARE_ENTRY:
        status = execute_texture_active_prepare_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_STATUS_DISPATCH_ENTRY:
        status = execute_texture_status_dispatch_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_RECORD_ADVANCE_ENTRY:
        status = execute_texture_record_advance(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_FINAL_STATUS_ENTRY:
        status = execute_texture_final_status_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_BODY_RETURN_ENTRY:
        status = execute_texture_body_return(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_POST_BODY_CALL_ENTRY:
        status = execute_texture_post_body_call(
            cpu, &local_report
        );
        break;
    case VF2_TEXTURE_COUNTER_UPDATE_ENTRY:
        status = execute_texture_counter_update(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY:
        status = execute_texture_orchestrator_epilogue(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY:
        status = execute_texture_orchestrator_save_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_FRAME_GATE_ENTRY:
        status = execute_texture_frame_gate_call(
            machine, cpu, &local_report
        );
        if (status == VF2_OK) {
            set_equal_condition(cpu);
        }
        break;
    case VF2_TEXTURE_DEFAULT_LIMITS_ENTRY:
        status = execute_texture_default_limits(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ADDRESS_TABLE_ENTRY:
        status = execute_texture_address_table(machine, cpu, &local_report);
        break;
    case VF2_DIAGNOSTIC_TEXT_COPY_ENTRY:
        status = execute_diagnostic_text_copy(machine, cpu, &local_report);
        break;
    case VF2_TILE_GLYPH_EXPAND_ENTRY:
        status = execute_tile_glyph_expand(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_STATUS_LATCH_ENTRY:
        status = execute_video_status_latch(machine, cpu, &local_report);
        break;
    case VF2_GEOMETRY_FRAME_COMMIT_ENTRY:
        status = execute_geometry_frame_commit(machine, cpu, &local_report);
        break;
    case VF2_GEOMETRY_COMMAND_SETUP_ENTRY:
        status = execute_geometry_command_setup(machine, cpu, &local_report);
        break;
    case VF2_FRAME_SCRATCH_CLEAR_ENTRY:
        status = execute_frame_scratch_clear(machine, cpu, &local_report);
        break;
    case VF2_COLOR_TABLE_REBUILD_ENTRY:
        status = execute_color_table_rebuild(machine, cpu, &local_report);
        break;
    case VF2_DISPLAY_COLOR_PROFILE_APPLY_ENTRY:
        status = execute_display_color_profile_apply(machine, cpu, &local_report);
        break;
    case VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY:
        status = execute_display_profile_unit_fill(machine, cpu, &local_report);
        break;
    case VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY:
        status = execute_display_profile_mode_constants(machine, cpu, &local_report);
        break;
    case VF2_DISPLAY_PROFILE_APPLY_ENTRY:
        status = execute_display_profile_apply(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_COMMAND_SUBMIT_ENTRY:
        status = execute_video_command_submit(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_TABLE_EXPAND_128_ENTRY:
        status = execute_video_table_expand_128(machine, cpu, &local_report);
        break;
    case VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY:
        status = execute_display_transform_defaults(machine, cpu, &local_report);
        break;
    case VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY:
        status = execute_display_runtime_initialize(machine, cpu, &local_report);
        break;
    case VF2_PALETTE_PAGE_UPLOAD_ENTRY:
        status = execute_palette_page_upload(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_CONVERT_LOOP_ENTRY:
        status = execute_texture_convert_loop(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_CONVERT_POST_ENTRY:
        status = execute_texture_convert_post_dispatch(
            machine, cpu, &local_report
        );
        break;
    case VF2_TIMER_WAIT_UPDATE_ENTRY:
        status = execute_timer_wait_update(machine, cpu, &local_report);
        break;
    case VF2_INLINE_TEXT_THUNK_ENTRY:
        status = execute_inline_text_thunk(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_STATUS_LINE_ENTRY:
        status = execute_texture_status_line(machine, cpu, &local_report);
        break;
    case VF2_GAME_STATE_CLASSIFY_ENTRY:
        status = execute_game_state_classify(machine, cpu, &local_report);
        break;
    case VF2_GAME_COLOR_LOOKUP_ENTRY:
        status = execute_game_color_lookup(machine, cpu, &local_report);
        break;
    case VF2_GAME_THRESHOLD_EVALUATE_ENTRY:
        status = execute_game_threshold_evaluate(
            machine, cpu, &local_report
        );
        break;
    case VF2_GAME_METER_UPDATE_ENTRY:
        status = execute_game_meter_update(machine, cpu, &local_report);
        break;
    case VF2_GAME_INPUT_UPDATE_ENTRY:
        status = execute_game_input_update(machine, cpu, &local_report);
        break;
    case VF2_GAME_STATE_UPDATE_ENTRY:
        status = execute_game_state_update(machine, cpu, &local_report, false);
        break;
    case VF2_TILE_CONTROLLER_UPDATE_ENTRY:
        status = execute_tile_controller_update(machine, cpu, &local_report);
        break;
    case VF2_FRAME_TIMER_PREFIX_ENTRY:
        status = execute_frame_timer_prefix(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_SAVE_PREFIX_ENTRY:
        status = execute_interrupt_save_prefix(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_BUFFER_GATE_ENTRY:
        status = execute_interrupt_buffer_gate(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_INPUT_RING_ENTRY:
        status = execute_interrupt_input_ring(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_RESTORE_PREFIX_ENTRY:
        status = execute_interrupt_restore_prefix(machine, cpu, &local_report);
        break;
    case VF2_FRAME_TIMER_SUFFIX_ENTRY:
        status = execute_frame_timer_suffix(machine, cpu, &local_report);
        break;
    case VF2_TEXTURE_STATUS_TAIL_ENTRY:
        status = execute_texture_status_tail_dispatch(
            machine, cpu, &local_report
        );
        break;
    case VF2_INTERRUPT_PLAYER_LAYER_ENTRY:
        status = execute_interrupt_player_layer(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_GAME_INPUT_ENTRY:
        status = execute_interrupt_game_input(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_GAME_STATE_ENTRY:
        status = execute_interrupt_game_state(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_TILE_SYNC_ENTRY:
        status = execute_interrupt_tile_sync(machine, cpu, &local_report);
        break;
    case VF2_MAIN_POST_TIMER_ENTRY:
        status = execute_main_post_timer(machine, cpu, &local_report);
        break;
    case VF2_MAIN_CLEAR_PREFIX_ENTRY:
        status = execute_main_clear_prefix(machine, cpu, &local_report);
        break;
    case VF2_MAIN_FINAL_CLUSTER_ENTRY:
        status = execute_main_final_cluster(machine, cpu, &local_report);
        break;
    case VF2_MAIN_GEOMETRY_PREFIX_ENTRY:
        status = execute_main_geometry_prefix(machine, cpu, &local_report);
        break;
    case VF2_MAIN_TEXTURE_ORCHESTRATOR_CALL_ENTRY:
        status = execute_main_texture_orchestrator_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_MAIN_FRAME_TIMER_CALL_ENTRY:
        status = execute_main_frame_timer_call(machine, cpu, &local_report);
        break;
    case VF2_INTERRUPT_INITIAL_CLUSTER_ENTRY:
        status = execute_interrupt_initial_cluster(machine, cpu, &local_report);
        break;
    case VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY:
        status = execute_system_memory_diagnostic(
            machine, cpu, &local_report
        );
        break;
    case VF2_VIDEO_INPUT_SYNC_ENTRY:
        status = execute_video_input_sync(machine, cpu, &local_report);
        break;
    case VF2_FRAME_COUNTER_ADVANCE_ENTRY:
        status = execute_frame_counter_advance(machine, cpu, &local_report);
        break;
    case VF2_FRAME_PHASE_ADVANCE_ENTRY:
        status = execute_frame_phase_advance(machine, cpu, &local_report);
        break;
    case VF2_FRAME_SHADOW_VERIFY_ENTRY:
        status = execute_frame_shadow_verify(machine, cpu, &local_report);
        break;
    case VF2_FRAME_BUFFER_GATE_ENTRY:
        status = execute_frame_buffer_gate(machine, cpu, &local_report);
        break;
    case VF2_FRAME_DISPATCH_TICK_ENTRY:
        status = execute_frame_dispatch_tick(machine, cpu, &local_report);
        break;
    case VF2_FRAME_GEOMETRY_GATE_ENTRY:
        status = execute_frame_geometry_gate(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_REGISTER_COMPOSE_ENTRY:
        status = execute_video_register_compose(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_INPUT_LATCH_WRITE_ENTRY:
        status = execute_video_input_latch_write(machine, cpu, &local_report);
        break;
    case VF2_PLAYER_UPDATE_GATE_ENTRY:
        status = execute_player_update_gate(machine, cpu, &local_report);
        break;
    case VF2_VIDEO_LAYER_COMMIT_ENTRY:
        status = execute_video_layer_commit(machine, cpu, &local_report);
        break;
    case VF2_INPUT_BIT0_SEQUENCE_GATE_ENTRY:
        status = execute_input_sequence_gate(
            machine, cpu, VF2_INPUT_BIT0_SEQUENCE_GATE_ENTRY,
            UINT32_C(0x00500148), UINT32_C(0x0050014a),
            VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE, &local_report
        );
        break;
    case VF2_INPUT_BIT1_SEQUENCE_GATE_ENTRY:
        status = execute_input_sequence_gate(
            machine, cpu, VF2_INPUT_BIT1_SEQUENCE_GATE_ENTRY,
            UINT32_C(0x00500149), UINT32_C(0x0050014b),
            VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE, &local_report
        );
        break;
    case VF2_INPUT_RING_POLL_ENTRY:
        status = execute_input_ring_poll(machine, cpu, &local_report);
        break;
    case VF2_TILE_RUNTIME_GATE_ENTRY:
        status = execute_tile_runtime_gate(machine, cpu, &local_report);
        break;
    case VF2_GAME_EVENT_QUEUE_WRITE_ENTRY:
        status = execute_game_event_queue_write(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_MAINTENANCE_ENTRY:
        status = execute_texture_maintenance(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY:
        status = execute_texture_upload_dispatch(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_ORCHESTRATOR_ENTRY_GATE:
        status = execute_texture_orchestrator_entry_gate(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY:
        status = execute_texture_record_status_setup(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_STREAM_HEADER_CALL_ENTRY:
        status = execute_texture_stream_header_call(
            machine, cpu, &local_report
        );
        break;
    case VF2_TEXTURE_STREAM_RESUME_GATE_ENTRY:
        status = execute_texture_stream_resume_gate(
            machine, cpu, &local_report
        );
        break;
    default:
        status = VF2_ERROR_UNSUPPORTED;
        break;
    }
    if (status == VF2_OK && report != NULL) {
        *report = local_report;
    }
    return status;
}

const char *vf2_hybrid_bridge_kind_name(vf2_hybrid_bridge_kind kind)
{
    switch (kind) {
    case VF2_HYBRID_BRIDGE_TEXTURE_BYTE_RUN:
        return "texture-byte-run";
    case VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE:
        return "texture-byte-decode";
    case VF2_HYBRID_BRIDGE_TEXTURE_WORD_RUN:
        return "texture-word-run";
    case VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE:
        return "texture-word-decode";
    case VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD:
        return "texture-symbol-table-build";
    case VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD:
        return "texture-pair-table-build";
    case VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH:
        return "texture-tree-dispatch";
    case VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND:
        return "texture-tree-expand";
    case VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE:
        return "texture-word-prepare";
    case VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE:
        return "texture-color-prepare";
    case VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT:
        return "texture-color-convert";
    case VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE:
        return "texture-address-table";
    case VF2_HYBRID_BRIDGE_DIAGNOSTIC_TEXT_COPY:
        return "diagnostic-text-copy";
    case VF2_HYBRID_BRIDGE_TILE_GLYPH_EXPAND:
        return "tile-glyph-expand";
    case VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH:
        return "video-status-latch";
    case VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT:
        return "geometry-frame-commit";
    case VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP:
        return "geometry-command-setup";
    case VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR:
        return "frame-scratch-clear";
    case VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD:
        return "palette-page-upload";
    case VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP:
        return "texture-convert-loop";
    case VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST:
        return "texture-convert-post";
    case VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE:
        return "timer-wait-update";
    case VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK:
        return "inline-text-thunk";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE:
        return "texture-status-line";
    case VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY:
        return "game-state-classify";
    case VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP:
        return "game-color-lookup";
    case VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE:
        return "game-threshold-evaluate";
    case VF2_HYBRID_BRIDGE_GAME_METER_UPDATE:
        return "game-meter-update";
    case VF2_HYBRID_BRIDGE_GAME_INPUT_UPDATE:
        return "game-input-update";
    case VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE:
        return "game-state-update";
    case VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE:
        return "tile-controller-update";
    case VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX:
        return "frame-timer-prefix";
    case VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX:
        return "interrupt-save-prefix";
    case VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE:
        return "interrupt-buffer-gate";
    case VF2_HYBRID_BRIDGE_INTERRUPT_INPUT_RING:
        return "interrupt-input-ring";
    case VF2_HYBRID_BRIDGE_INTERRUPT_RESTORE_PREFIX:
        return "interrupt-restore-prefix";
    case VF2_HYBRID_BRIDGE_FRAME_TIMER_SUFFIX:
        return "frame-timer-suffix";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL:
        return "texture-status-tail";
    case VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER:
        return "interrupt-player-layer";
    case VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT:
        return "interrupt-game-input";
    case VF2_HYBRID_BRIDGE_INTERRUPT_GAME_STATE:
        return "interrupt-game-state";
    case VF2_HYBRID_BRIDGE_INTERRUPT_TILE_SYNC:
        return "interrupt-tile-sync";
    case VF2_HYBRID_BRIDGE_MAIN_POST_TIMER:
        return "main-post-timer";
    case VF2_HYBRID_BRIDGE_MAIN_CLEAR_PREFIX:
        return "main-clear-prefix";
    case VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER:
        return "main-final-cluster";
    case VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX:
        return "main-geometry-prefix";
    case VF2_HYBRID_BRIDGE_MAIN_TEXTURE_ORCHESTRATOR_CALL:
        return "main-texture-orchestrator-call";
    case VF2_HYBRID_BRIDGE_MAIN_FRAME_TIMER_CALL:
        return "main-frame-timer-call";
    case VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER:
        return "interrupt-initial-cluster";
    case VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL:
        return "frame-wait-poll";
    case VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT:
        return "interrupt-return-wait-exit";
    case VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC:
        return "system-memory-diagnostic";
    case VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC:
        return "video-input-sync";
    case VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE:
        return "frame-counter-advance";
    case VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE:
        return "frame-phase-advance";
    case VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY:
        return "frame-shadow-verify";
    case VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE:
        return "frame-buffer-gate";
    case VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK:
        return "frame-dispatch-tick";
    case VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE:
        return "frame-geometry-gate";
    case VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE:
        return "video-register-compose";
    case VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE:
        return "video-input-latch-write";
    case VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE:
        return "player-update-gate";
    case VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT:
        return "video-layer-commit";
    case VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE:
        return "input-bit0-sequence-gate";
    case VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE:
        return "input-bit1-sequence-gate";
    case VF2_HYBRID_BRIDGE_INPUT_RING_POLL:
        return "input-ring-poll";
    case VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE:
        return "tile-runtime-gate";
    case VF2_HYBRID_BRIDGE_GAME_EVENT_QUEUE_WRITE:
        return "game-event-queue-write";
    case VF2_HYBRID_BRIDGE_TEXTURE_MAINTENANCE:
        return "texture-maintenance";
    case VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH:
        return "texture-upload-dispatch";
    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE:
        return "texture-orchestrator-entry-gate";
    case VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP:
        return "texture-record-status-setup";
    case VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL:
        return "texture-stream-header-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL:
        return "texture-orchestrator-save-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL:
        return "texture-frame-gate-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS:
        return "texture-default-limits";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL:
        return "texture-status-dispatch-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL:
        return "texture-active-prepare-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE:
        return "texture-header-decode";
    case VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END:
        return "texture-status-scan-end";
    case VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A:
        return "texture-child-gate-a";
    case VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B:
        return "texture-child-gate-b";
    case VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE:
        return "texture-loop-gate";
    case VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE:
        return "texture-record-advance";
    case VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL:
        return "texture-final-status-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN:
        return "texture-body-return";
    case VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL:
        return "texture-post-body-call";
    case VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE:
        return "texture-counter-update";
    case VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE:
        return "texture-orchestrator-epilogue";
    case VF2_HYBRID_BRIDGE_RETURN_STUB:
        return "return-stub";
    case VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY:
        return "second-scheduler-entry";
    case VF2_HYBRID_BRIDGE_CAMERA_BIT7_INTERPRETER:
        return "camera-bit7-interpreter";
    case VF2_HYBRID_BRIDGE_TEXTURE_SELECTOR_INTERPRETER:
        return "texture-selector-interpreter";
    case VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_INTERPRETER:
        return "texture-counter-interpreter";
    case VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_WRAPPER_INTERPRETER:
        return "second-scheduler-wrapper-interpreter";
    case VF2_HYBRID_BRIDGE_TEXTURE_DECODER_CONTINUATION_INTERPRETER:
        return "texture-decoder-continuation-interpreter";
    case VF2_HYBRID_BRIDGE_NONE:
    default:
        return "none";
    }
}
