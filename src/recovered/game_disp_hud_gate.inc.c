#define VF2_GAME_DISP_HUD_GATE_ENTRY UINT32_C(0x0002c770)
#define VF2_GAME_DISP_HUD_GATE_RETURN UINT32_C(0x0002b230)
#define VF2_GAME_DISP_HUD_STATE UINT32_C(0x00500031)
#define VF2_GAME_DISP_HUD_LEFT UINT32_C(0x0050004f)
#define VF2_GAME_DISP_HUD_RIGHT UINT32_C(0x00500051)
#define VF2_GAME_DISP_HUD_GATE_INSTRUCTIONS UINT64_C(8)

static bool measured_game_disp_hud_gate_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    uint8_t state_value = UINT8_MAX;
    uint8_t left = UINT8_MAX;
    uint8_t right = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_GAME_DISP_HUD_GATE_ENTRY ||
        cpu->local_frame_depth != UINT32_C(3) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[2].registers[2] != VF2_GAME_DISP_HUD_GATE_RETURN ||
        cpu->local_frames[2].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_HUD_STATE, &state_value, sizeof(state_value)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_HUD_LEFT, &left, sizeof(left)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_HUD_RIGHT, &right, sizeof(right)
        );
    }
    return status == VF2_OK && state_value == UINT8_C(0) &&
           left == UINT8_C(0) && right == UINT8_C(0);
}

static vf2_status execute_measured_game_disp_hud_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    cpu->registers[3] = 0u;
    cpu->registers[4] = 0u;
    cpu->executed_instructions += VF2_GAME_DISP_HUD_GATE_INSTRUCTIONS;
    set_runtime_equal_condition(cpu);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_HUD_GATE_RETURN ||
        cpu->procedure_returns != start_returns + UINT64_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_HUD_GATE_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_HUD_GATE_RETURN;
    effective_report->current_registry_address = VF2_GAME_DISP_REGISTRY;
    effective_report->recovered_instruction_count = VF2_GAME_DISP_HUD_GATE_INSTRUCTIONS;
    effective_report->recovered_procedure_returns = UINT64_C(1);

    ++state->blocks_executed;
    state->recovered_instruction_count += VF2_GAME_DISP_HUD_GATE_INSTRUCTIONS;
    state->recovered_procedure_returns += UINT64_C(1);
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_hud_gate_case(machine, cpu)) {
        return execute_measured_game_disp_hud_gate(
            machine, cpu, state, report
        );
    }
    return vf2_native_runtime_step_hud_base(
        machine, cpu, state, report
    );
}
