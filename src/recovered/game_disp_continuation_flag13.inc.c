static bool measured_game_disp_continuation_flag13_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint16_t *value_out,
    uint8_t *side_out,
    uint8_t *mode_out
)
{
    static const uint32_t expected_globals[15] = {
        UINT32_C(0), UINT32_C(0x3f4f5c29), UINT32_C(0xc0a0a3d7), UINT32_C(0),
        UINT32_C(0x00560000), UINT32_C(0x0050e850), UINT32_C(0x000055b6),
        UINT32_C(0x00512980), UINT32_C(0x00510980), UINT32_C(0x010016ac),
        UINT32_C(0x00800000), UINT32_C(0x00880000), UINT32_C(0x00004000),
        VF2_GAME_DISP_REGISTRY, UINT32_C(0x00000220)
    };
    uint32_t control = 0u;
    uint32_t target = 0u;
    uint16_t counter = UINT16_MAX;
    uint8_t dispatch_index = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || value_out == NULL ||
        side_out == NULL || mode_out == NULL ||
        cpu->ip != VF2_GAME_DISP_CONT_ENTRY ||
        cpu->local_frame_depth != UINT32_C(2) ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[1].registers[2] != VF2_GAME_DISP_CONT_RETURN ||
        cpu->local_frames[1].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 15u; ++index) {
        const uint32_t actual = cpu->registers[VF2_I960_G0_REGISTER + index];
        if (actual == expected_globals[index]) {
            continue;
        }
        if (index == 8u && actual == expected_globals[7u]) {
            continue;
        }
        return false;
    }

    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_CONT_CONTROL, &control);
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine, VF2_GAME_DISP_COUNTER_VALUE, &counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_CONT_DISPATCH_INDEX,
            &dispatch_index,
            sizeof(dispatch_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_CONT_TARGET_TABLE +
                (uint32_t)dispatch_index * UINT32_C(4),
            &target
        );
    }
    if (status != VF2_OK || control != UINT32_C(0x00008a00) ||
        counter != UINT16_C(0) || dispatch_index != UINT8_C(0) ||
        target != VF2_GAME_DISP_MAIN_ENTRY) {
        return false;
    }

    return game_disp_event_flag13_machine_case(
        machine, value_out, side_out, mode_out
    );
}

static vf2_status execute_measured_game_disp_continuation_flag13(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    uint16_t value,
    uint8_t side
)
{
    uint8_t queue_head = UINT8_C(0);
    uint8_t measured_main_mode = side;
    uint8_t base_main_mode = UINT8_C(0);
    uint64_t instruction_delta = UINT64_C(0);
    uint32_t link = 0u;
    vf2_status restore_status = VF2_OK;
    vf2_status status = game_disp_event_flag13_enqueue(
        machine,
        &queue_head,
        value,
        side,
        &instruction_delta,
        &link
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            &queue_head,
            sizeof(queue_head)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK && measured_main_mode == UINT8_C(1)) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_MAIN_MODE,
            &base_main_mode,
            sizeof(base_main_mode)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = execute_measured_game_disp_continuation(
        machine, cpu, state, report
    );
    if (measured_main_mode == UINT8_C(1)) {
        restore_status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_MAIN_MODE,
            &measured_main_mode,
            sizeof(measured_main_mode)
        );
        if (status == VF2_OK && restore_status != VF2_OK) {
            status = restore_status;
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += instruction_delta;
    cpu->registers[VF2_I960_G14_REGISTER] = link;
    state->recovered_instruction_count += instruction_delta;
    if (report != NULL) {
        report->recovered_instruction_count += instruction_delta;
    }
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint16_t value = UINT16_MAX;
    uint8_t side = UINT8_MAX;
    uint8_t mode = UINT8_MAX;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_continuation_flag13_case(
            machine, cpu, &value, &side, &mode
        )) {
        (void)mode;
        return execute_measured_game_disp_continuation_flag13(
            machine, cpu, state, report, value, side
        );
    }
    return vf2_native_runtime_step_condition_flag13_base(
        machine, cpu, state, report
    );
}
