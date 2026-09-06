#define VF2_GAME_DISP_CLEAR_ENTRY UINT32_C(0x0002b27c)
#define VF2_GAME_DISP_CLEAR_RETURN UINT32_C(0x0002b234)
#define VF2_GAME_DISP_MODE_BYTE UINT32_C(0x0050004c)
#define VF2_GAME_DISP_STATUS_WORD UINT32_C(0x00500020)
#define VF2_GAME_DISP_UI_FLAGS UINT32_C(0x00500718)
#define VF2_GAME_DISP_RECT_HELPER UINT32_C(0x00008ef0)
#define VF2_GAME_DISP_RECT_INSTRUCTIONS UINT64_C(220)
#define VF2_GAME_DISP_CLEAR_PARENT_INSTRUCTIONS UINT64_C(18)

static bool measured_game_disp_clear_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    uint32_t registry_flags = 0u;
    uint32_t status_word = 0u;
    uint8_t mode = UINT8_MAX;
    uint8_t ui_flags = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_GAME_DISP_CLEAR_ENTRY ||
        cpu->local_frame_depth != UINT32_C(3) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[2].registers[2] != VF2_GAME_DISP_CLEAR_RETURN ||
        cpu->local_frames[2].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_MODE_BYTE, &mode, sizeof(mode)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_STATUS_WORD, &status_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_REGISTRY, &registry_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_UI_FLAGS, &ui_flags, sizeof(ui_flags)
        );
    }
    return status == VF2_OK && mode == UINT8_C(0) &&
           status_word == UINT32_C(4) && registry_flags == UINT32_C(0) &&
           ui_flags == UINT8_C(0);
}

static vf2_status game_disp_clear_rect(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t destination,
    uint32_t return_address
)
{
    vf2_status status = VF2_OK;

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = destination;
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(26);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(2);
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_RECT_HELPER, return_address
    );
    if (status == VF2_OK) {
        status = selector0_rect_helper(machine, cpu);
    }
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += VF2_GAME_DISP_RECT_INSTRUCTIONS;
    return VF2_OK;
}

static vf2_status execute_measured_game_disp_clear(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint8_t zero = UINT8_C(0);
    vf2_status status = VF2_OK;

    cpu->registers[10] = UINT32_C(0);
    cpu->registers[11] = UINT32_C(4);
    cpu->registers[15] = UINT32_C(0);
    cpu->registers[12] = UINT32_C(12);
    cpu->registers[14] = UINT32_C(0);
    status = vf2_model2a_write(
        machine, VF2_GAME_DISP_UI_FLAGS, &zero, sizeof(zero)
    );
    if (status != VF2_OK) {
        return status;
    }

    status = game_disp_clear_rect(
        machine, cpu, UINT32_C(0x01000082), UINT32_C(0x0002b938)
    );
    if (status != VF2_OK) {
        return status;
    }
    status = game_disp_clear_rect(
        machine, cpu, UINT32_C(0x010000c6), UINT32_C(0x0002b94c)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += VF2_GAME_DISP_CLEAR_PARENT_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_CLEAR_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_CLEAR_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_CLEAR_RETURN;
    effective_report->current_registry_address = VF2_GAME_DISP_REGISTRY;
    effective_report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    effective_report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    effective_report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;

    ++state->blocks_executed;
    state->recovered_instruction_count += effective_report->recovered_instruction_count;
    state->recovered_procedure_calls += effective_report->recovered_procedure_calls;
    state->recovered_procedure_returns += effective_report->recovered_procedure_returns;
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
        measured_game_disp_clear_case(machine, cpu)) {
        return execute_measured_game_disp_clear(
            machine, cpu, state, report
        );
    }
    return vf2_native_runtime_step_clear_base(
        machine, cpu, state, report
    );
}
