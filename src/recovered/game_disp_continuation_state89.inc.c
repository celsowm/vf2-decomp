static bool measured_game_disp_continuation_state89_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    game_disp_state89_case *match
)
{
    uint32_t original_flags = 0u;
    bool base_match = false;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || match == NULL ||
        !game_disp_state89_machine_case(machine, match)) {
        return false;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &original_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        base_match = measured_game_disp_continuation_case(machine, cpu);
    }
    if (vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, original_flags
        ) != VF2_OK) {
        return false;
    }
    return status == VF2_OK && base_match;
}

static uint64_t game_disp_state89_full_instructions(
    const game_disp_state89_case *match
)
{
    const uint64_t outside =
        match->state_byte == UINT8_C(8)
            ? UINT64_C(2865)
            : UINT64_C(2861);
    return match->child_instructions + outside;
}

static vf2_status execute_measured_game_disp_continuation_state89(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    const game_disp_state89_case *match
)
{
    game_disp_state89_image original;
    game_disp_state89_image desired;
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t target_instructions =
        game_disp_state89_full_instructions(match);
    uint64_t actual = UINT64_C(0);
    uint64_t delta = UINT64_C(0);
    vf2_status status = game_disp_state89_read_image(machine, &original);

    if (status == VF2_OK) {
        status = game_disp_state89_apply_transition(machine, match);
    }
    if (status == VF2_OK) {
        status = game_disp_state89_read_image(machine, &desired);
    }
    if (status == VF2_OK) {
        status = game_disp_state89_write_image(machine, &original);
    }
    if (status == VF2_OK) {
        status = game_disp_state89_normalize_for_base(machine);
    }
    if (status == VF2_OK) {
        status = execute_measured_game_disp_continuation(
            machine, cpu, state, effective_report
        );
    }
    if (status == VF2_OK) {
        status = game_disp_state89_write_image(machine, &desired);
    }
    if (status != VF2_OK) {
        return status;
    }

    actual = cpu->executed_instructions - start_instructions;
    if (actual > target_instructions) {
        return VF2_ERROR_UNSUPPORTED;
    }
    delta = target_instructions - actual;
    cpu->executed_instructions += delta;
    cpu->registers[VF2_I960_G14_REGISTER] = match->link;
    state->recovered_instruction_count += delta;
    effective_report->recovered_instruction_count += delta;
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    game_disp_state89_case match;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_continuation_state89_case(
            machine, cpu, &match
        )) {
        return execute_measured_game_disp_continuation_state89(
            machine, cpu, state, report, &match
        );
    }
    return vf2_native_runtime_step_condition_state89_base(
        machine, cpu, state, report
    );
}
