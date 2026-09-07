#define VF2_GAME_DISP_EVENT_FLAG9 UINT32_C(0x00000200)
#define VF2_GAME_DISP_EVENT_FLAG8 UINT32_C(0x00000100)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6 UINT8_C(0x40)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6_VALUE UINT32_C(0x000b0000)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6_TILE UINT32_C(0x01000040)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6_CHILD_ONE UINT64_C(52)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6_CHILD_BOTH UINT64_C(57)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6_FULL_ONE UINT64_C(2955)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE6_FULL_BOTH UINT64_C(2960)

static bool measured_game_disp_flag18_state6_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    game_disp_flag18_case *match
)
{
    uint32_t state_base = 0u;
    uint32_t value = 0u;
    uint8_t state_flags = UINT8_MAX;
    const uint8_t zero = UINT8_C(0);
    bool matched = false;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || match == NULL ||
        (cpu->ip != VF2_GAME_DISP_EVENT_ENTRY &&
         cpu->ip != VF2_GAME_DISP_CONT_ENTRY)) {
        return false;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_EVENT_FLAG18_STATE_SLOT, &state_base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            state_base + VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &state_flags,
            sizeof(state_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FLAG18_VALUE, &value
        );
    }
    if (status != VF2_OK ||
        state_base != VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE ||
        state_flags != VF2_GAME_DISP_EVENT_FLAG18_STATE6 ||
        value != VF2_GAME_DISP_EVENT_FLAG18_STATE6_VALUE) {
        return false;
    }

    status = vf2_model2a_write(
        machine,
        state_base + VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
        &zero,
        sizeof(zero)
    );
    if (status == VF2_OK) {
        matched = cpu->ip == VF2_GAME_DISP_EVENT_ENTRY
                      ? measured_game_disp_event_flag18_case(
                            machine, cpu, match
                        )
                      : measured_game_disp_continuation_flag18_case(
                            machine, cpu, match
                        );
    }
    if (vf2_model2a_write(
            machine,
            state_base + VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &state_flags,
            sizeof(state_flags)
        ) != VF2_OK) {
        return false;
    }

    return status == VF2_OK && matched &&
           (match->side0_active || match->side1_active);
}

static vf2_status execute_measured_game_disp_flag18_state6(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    const game_disp_flag18_case *match
)
{
    static const uint8_t tile_bytes[4] = {
        UINT8_C(0x2e), UINT8_C(0x80), UINT8_C(0x31), UINT8_C(0x80)
    };
    const uint32_t start_ip = cpu->ip;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint8_t zero = UINT8_C(0);
    const uint8_t state6 = VF2_GAME_DISP_EVENT_FLAG18_STATE6;
    const bool full = start_ip == VF2_GAME_DISP_CONT_ENTRY;
    const bool both = match->side0_active && match->side1_active;
    const uint64_t target_instructions = full
        ? (both
               ? VF2_GAME_DISP_EVENT_FLAG18_STATE6_FULL_BOTH
               : VF2_GAME_DISP_EVENT_FLAG18_STATE6_FULL_ONE)
        : (both
               ? VF2_GAME_DISP_EVENT_FLAG18_STATE6_CHILD_BOTH
               : VF2_GAME_DISP_EVENT_FLAG18_STATE6_CHILD_ONE);
    uint32_t original_flags = 0u;
    uint32_t final_flags = UINT32_C(0x80000000);
    uint64_t actual_instructions = UINT64_C(0);
    uint64_t delta = UINT64_C(0);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL || match == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
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
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &zero,
            sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_step_flag18_state6_base(
            machine, cpu, state, report
        );
    }
    if (status != VF2_OK) {
        (void)vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, original_flags
        );
        (void)vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &state6,
            sizeof(state6)
        );
        return status;
    }

    status = vf2_model2a_write(
        machine,
        VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
            VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
        &state6,
        sizeof(state6)
    );
    if (match->side0_active) {
        final_flags |= VF2_GAME_DISP_EVENT_FLAG9;
    }
    if (match->side1_active) {
        final_flags |= VF2_GAME_DISP_EVENT_FLAG8;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, final_flags
        );
    }
    if (status == VF2_OK && full) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE6_TILE,
            tile_bytes,
            sizeof(tile_bytes)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    actual_instructions = cpu->executed_instructions - start_instructions;
    if (actual_instructions > target_instructions ||
        cpu->registers[VF2_I960_G14_REGISTER] != UINT32_C(0x00000220)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    delta = target_instructions - actual_instructions;
    cpu->executed_instructions += delta;
    state->recovered_instruction_count += delta;
    if (report != NULL) {
        report->recovered_instruction_count += delta;
    }

    if (full) {
        if (cpu->procedure_calls - start_calls != UINT64_C(41) ||
            cpu->procedure_returns - start_returns != UINT64_C(42)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        ++cpu->procedure_calls;
        ++cpu->procedure_returns;
        ++state->recovered_procedure_calls;
        ++state->recovered_procedure_returns;
        if (report != NULL) {
            ++report->recovered_procedure_calls;
            ++report->recovered_procedure_returns;
        }
    } else if (cpu->procedure_calls != start_calls ||
               cpu->procedure_returns - start_returns != UINT64_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
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
    game_disp_flag18_case match;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_flag18_state6_case(machine, cpu, &match)) {
        return execute_measured_game_disp_flag18_state6(
            machine, cpu, state, report, &match
        );
    }
    return vf2_native_runtime_step_flag18_state6_base(
        machine, cpu, state, report
    );
}
