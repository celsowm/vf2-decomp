#define VF2_GAME_DISP_MAIN_ENTRY UINT32_C(0x0002c968)
#define VF2_GAME_DISP_MAIN_RETURN UINT32_C(0x0002b278)
#define VF2_GAME_DISP_MAIN_FINAL_RESOURCE UINT32_C(0x02a6ba92)
#define VF2_GAME_DISP_MAIN_FINAL_DEST UINT32_C(0x01001758)
#define VF2_GAME_DISP_MAIN_FINAL_RETURN UINT32_C(0x0002c9b0)
#define VF2_GAME_DISP_MAIN_MODE UINT32_C(0x0050004c)
#define VF2_GAME_DISP_MAIN_STATUS UINT32_C(0x00500054)
#define VF2_GAME_DISP_MAIN_VALUE UINT32_C(0x00500070)
#define VF2_GAME_DISP_MAIN_COUNTDOWN UINT32_C(0x0050004a)
#define VF2_GAME_DISP_MAIN_INSTRUCTIONS UINT64_C(1993)
#define VF2_GAME_DISP_MAIN_CALLS UINT64_C(28)
#define VF2_GAME_DISP_MAIN_RETURNS UINT64_C(29)

static bool measured_game_disp_main_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    static const uint32_t expected_globals[15] = {
        UINT32_C(0x0000001a), UINT32_C(0), UINT32_C(2), UINT32_C(10),
        UINT32_C(0), UINT32_C(0), UINT32_C(0xffff8000),
        UINT32_C(0x00510980), UINT32_C(0x00512980), UINT32_C(0x010001c6),
        UINT32_C(0x00800000), UINT32_C(0x00880000), UINT32_C(0x00004000),
        VF2_GAME_DISP_REGISTRY, UINT32_C(0x00000220)
    };
    vf2_i960_cpu probe;
    uint32_t value = 0u;
    uint16_t countdown = 0u;
    uint8_t mode = UINT8_MAX;
    uint8_t status_byte = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_GAME_DISP_MAIN_ENTRY ||
        cpu->local_frame_depth != UINT32_C(3) ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[2].registers[2] != VF2_GAME_DISP_MAIN_RETURN ||
        cpu->local_frames[2].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 15u; ++index) {
        if (cpu->registers[VF2_I960_G0_REGISTER + index] != expected_globals[index]) {
            return false;
        }
    }

    status = vf2_model2a_read(machine, VF2_GAME_DISP_MAIN_MODE, &mode, sizeof(mode));
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_MAIN_STATUS, &status_byte, sizeof(status_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_MAIN_VALUE, &value);
    }
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine, VF2_GAME_DISP_MAIN_COUNTDOWN, &countdown
        );
    }
    if (status != VF2_OK || mode != UINT8_C(0) || status_byte != UINT8_C(1) ||
        value != UINT32_C(0) || countdown != UINT16_C(0) ||
        !game_disp_hud_resource_header_expected(
            machine, VF2_GAME_DISP_MAIN_FINAL_RESOURCE, UINT32_C(16), UINT32_C(1))) {
        return false;
    }

    probe = *cpu;
    status = vf2_i960_cpu_enter_procedure(
        &probe, VF2_GAME_DISP_HUD_BODY_ENTRY, VF2_GAME_DISP_HUD_BODY_RETURN
    );
    if (status != VF2_OK || !measured_game_disp_hud_body_stride_case(machine, &probe)) {
        return false;
    }

    probe = *cpu;
    status = vf2_i960_cpu_enter_procedure(
        &probe, VF2_GAME_DISP_RESOURCE_PAIR_ENTRY, VF2_GAME_DISP_RESOURCE_PAIR_RETURN
    );
    if (status != VF2_OK || !measured_game_disp_resource_pair_case(machine, &probe)) {
        return false;
    }

    probe = *cpu;
    probe.registers[VF2_I960_G0_REGISTER] = UINT32_C(0);
    probe.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010015f4);
    status = vf2_i960_cpu_enter_procedure(
        &probe, VF2_GAME_DISP_NUMBER_ENTRY, VF2_GAME_DISP_NUMBER_RETURN
    );
    return status == VF2_OK && measured_game_disp_number_case(machine, &probe);
}

static vf2_status game_disp_main_final_resource(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t base = 0u;
    uint32_t format = 0u;
    uint32_t saved_g9 = 0u;
    uint32_t row = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4),
        cpu->registers[VF2_I960_G0_REGISTER + 9u]
    );
    if (status == VF2_OK) {
        status = game_disp_resource_read_s16(
            machine, cpu->registers[VF2_I960_G0_REGISTER], &base
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = base;
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(2);
    if (status == VF2_OK) {
        status = game_disp_resource_read_s16(
            machine, cpu->registers[VF2_I960_G0_REGISTER], &format
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = format;
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(2);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[VF2_I960_G0_REGISTER],
            &cpu->registers[VF2_I960_G0_REGISTER + 3u]
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[VF2_I960_G0_REGISTER],
            &cpu->registers[VF2_I960_G0_REGISTER + 2u]
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(4);
    if (status != VF2_OK || base != UINT32_C(0xffff8000) ||
        format != UINT32_C(1) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] != UINT32_C(1) ||
        cpu->registers[VF2_I960_G0_REGISTER + 2u] != UINT32_C(16)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[3] = UINT32_C(0);
    for (row = 0u; row < UINT32_C(1); ++row) {
        uint32_t column = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] =
            cpu->registers[VF2_I960_G0_REGISTER + 2u];
        for (column = 0u; column < UINT32_C(16); ++column) {
            uint32_t source = 0u;
            status = game_disp_resource_read_s16(
                machine, cpu->registers[VF2_I960_G0_REGISTER], &source
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(2);
            cpu->registers[VF2_I960_G0_REGISTER + 6u] =
                source + cpu->registers[VF2_I960_G0_REGISTER + 4u];
            status = game_disp_resource_write_s16(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 9u],
                cpu->registers[VF2_I960_G0_REGISTER + 6u]
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(2);
            --cpu->registers[VF2_I960_G0_REGISTER + 5u];
        }
        cpu->registers[VF2_I960_G0_REGISTER + 9u] -=
            cpu->registers[VF2_I960_G0_REGISTER + 2u];
        cpu->registers[VF2_I960_G0_REGISTER + 9u] -=
            cpu->registers[VF2_I960_G0_REGISTER + 2u];
        cpu->registers[3] = UINT32_C(0x80);
        cpu->registers[VF2_I960_G0_REGISTER + 9u] += cpu->registers[3];
        --cpu->registers[VF2_I960_G0_REGISTER + 3u];
    }

    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4), &saved_g9
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = saved_g9;
    cpu->registers[1] -= UINT32_C(4);
    cpu->registers[VF2_I960_G0_REGISTER + 2u] <<= 1u;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] +=
        cpu->registers[VF2_I960_G0_REGISTER + 2u];
    set_runtime_equal_condition(cpu);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_MAIN_FINAL_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_measured_game_disp_main(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_state child_state;
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint16_t countdown = 0u;
    uint32_t value = 0u;
    uint8_t mode = 0u;
    uint8_t status_byte = 0u;
    vf2_status status = VF2_OK;

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_HUD_BODY_ENTRY, VF2_GAME_DISP_HUD_BODY_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_hud_body_stride(
            machine, cpu, &child_state, NULL
        );
    }
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_HUD_BODY_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_RESOURCE_PAIR_ENTRY, VF2_GAME_DISP_RESOURCE_PAIR_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_resource_pair(
            machine, cpu, &child_state, NULL
        );
    }
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_RESOURCE_PAIR_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read(machine, VF2_GAME_DISP_MAIN_MODE, &mode, sizeof(mode));
    cpu->registers[14] = (uint32_t)mode;
    if (status != VF2_OK || mode != UINT8_C(0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_MAIN_STATUS, &status_byte, sizeof(status_byte)
    );
    cpu->registers[14] = (uint32_t)status_byte;
    if (status != VF2_OK || status_byte != UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_MAIN_VALUE, &value);
    cpu->registers[VF2_I960_G0_REGISTER] = value;
    if (status != VF2_OK || value != UINT32_C(0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010015f4);

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_NUMBER_ENTRY, VF2_GAME_DISP_NUMBER_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_number(machine, cpu, &child_state, NULL);
    }
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_NUMBER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = VF2_GAME_DISP_MAIN_FINAL_DEST;
    cpu->registers[VF2_I960_G0_REGISTER] = VF2_GAME_DISP_MAIN_FINAL_RESOURCE;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_RESOURCE_HELPER, VF2_GAME_DISP_MAIN_FINAL_RETURN
    );
    if (status == VF2_OK) {
        status = game_disp_main_final_resource(machine, cpu);
    }
    if (status != VF2_OK) {
        return status;
    }

    status = game_disp_leaf_read_u16(
        machine, VF2_GAME_DISP_MAIN_COUNTDOWN, &countdown
    );
    if (status != VF2_OK || countdown != UINT16_C(0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = UINT32_MAX;
    status = game_disp_leaf_write_u16(
        machine, VF2_GAME_DISP_MAIN_COUNTDOWN, UINT16_C(0xffff)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + VF2_GAME_DISP_MAIN_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_MAIN_RETURN ||
        cpu->procedure_calls != start_calls + VF2_GAME_DISP_MAIN_CALLS ||
        cpu->procedure_returns != start_returns + VF2_GAME_DISP_MAIN_RETURNS) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_equal_condition(cpu);

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_MAIN_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_MAIN_RETURN;
    effective_report->current_registry_address = VF2_GAME_DISP_REGISTRY;
    effective_report->recovered_instruction_count = VF2_GAME_DISP_MAIN_INSTRUCTIONS;
    effective_report->recovered_procedure_calls = VF2_GAME_DISP_MAIN_CALLS;
    effective_report->recovered_procedure_returns = VF2_GAME_DISP_MAIN_RETURNS;
    ++state->blocks_executed;
    state->recovered_instruction_count += VF2_GAME_DISP_MAIN_INSTRUCTIONS;
    state->recovered_procedure_calls += VF2_GAME_DISP_MAIN_CALLS;
    state->recovered_procedure_returns += VF2_GAME_DISP_MAIN_RETURNS;
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
        measured_game_disp_main_case(machine, cpu)) {
        return execute_measured_game_disp_main(machine, cpu, state, report);
    }
    return vf2_native_runtime_step_main_base(machine, cpu, state, report);
}
