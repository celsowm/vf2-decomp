#define VF2_GAME_DISP_RESOURCE_PAIR_ENTRY UINT32_C(0x0002cc8c)
#define VF2_GAME_DISP_RESOURCE_PAIR_RETURN UINT32_C(0x0002c970)
#define VF2_GAME_DISP_RESOURCE_HELPER UINT32_C(0x00008f1c)
#define VF2_GAME_DISP_RESOURCE_RETURN0 UINT32_C(0x0002ccb4)
#define VF2_GAME_DISP_RESOURCE_RETURN1 UINT32_C(0x0002cccc)
#define VF2_GAME_DISP_RESOURCE_TABLE0 UINT32_C(0x0201e044)
#define VF2_GAME_DISP_RESOURCE_TABLE1 UINT32_C(0x0201e070)
#define VF2_GAME_DISP_RESOURCE0 UINT32_C(0x02a6bb9a)
#define VF2_GAME_DISP_RESOURCE1 UINT32_C(0x02a6bd92)
#define VF2_GAME_DISP_RESOURCE_DEST0 UINT32_C(0x01000304)
#define VF2_GAME_DISP_RESOURCE_DEST1 UINT32_C(0x0100036c)
#define VF2_GAME_DISP_RESOURCE_INDEX_OFFSET UINT32_C(0x000001b1)
#define VF2_GAME_DISP_RESOURCE_PAIR_INSTRUCTIONS UINT64_C(243)

static vf2_status game_disp_resource_read_s16(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    int16_t signed_value = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        signed_value = (int16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
        *value = (uint32_t)(int32_t)signed_value;
    }
    return status;
}

static vf2_status game_disp_resource_write_s16(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t value
)
{
    const uint16_t narrowed = (uint16_t)value;
    const uint8_t bytes[2] = {
        (uint8_t)narrowed,
        (uint8_t)(narrowed >> 8u)
    };

    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static bool game_disp_resource_header_is_measured(
    vf2_model2a *machine,
    uint32_t resource
)
{
    uint32_t base = 0u;
    uint32_t format = 0u;
    uint32_t height = 0u;
    uint32_t width = 0u;
    vf2_status status = game_disp_resource_read_s16(machine, resource, &base);

    if (status == VF2_OK) {
        status = game_disp_resource_read_s16(
            machine, resource + UINT32_C(2), &format
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, resource + UINT32_C(4), &height
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, resource + UINT32_C(8), &width
        );
    }
    return status == VF2_OK && base == UINT32_C(0xffff8000) &&
           format == UINT32_C(1) && height == UINT32_C(2) &&
           width == UINT32_C(6);
}

static bool measured_game_disp_resource_pair_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t resource0 = 0u;
    uint32_t resource1 = 0u;
    uint8_t index0 = UINT8_MAX;
    uint8_t index1 = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_GAME_DISP_RESOURCE_PAIR_ENTRY ||
        cpu->local_frame_depth != UINT32_C(4) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[3].registers[2] != VF2_GAME_DISP_RESOURCE_PAIR_RETURN ||
        cpu->local_frames[3].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_FIGHTER0_SLOT, &fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_FIGHTER1_SLOT, &fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            fighter0 + VF2_GAME_DISP_RESOURCE_INDEX_OFFSET,
            &index0,
            sizeof(index0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            fighter1 + VF2_GAME_DISP_RESOURCE_INDEX_OFFSET,
            &index1,
            sizeof(index1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_RESOURCE_TABLE0 + (uint32_t)index0 * UINT32_C(4),
            &resource0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_RESOURCE_TABLE1 + (uint32_t)index1 * UINT32_C(4),
            &resource1
        );
    }

    return status == VF2_OK &&
           fighter0 == UINT32_C(0x00510980) &&
           fighter1 == UINT32_C(0x00512980) &&
           index0 == UINT8_C(0) && index1 == UINT8_C(8) &&
           resource0 == VF2_GAME_DISP_RESOURCE0 &&
           resource1 == VF2_GAME_DISP_RESOURCE1 &&
           game_disp_resource_header_is_measured(machine, resource0) &&
           game_disp_resource_header_is_measured(machine, resource1);
}

static vf2_status game_disp_resource_blit_measured(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    uint32_t row = 0u;
    uint32_t base = 0u;
    uint32_t format = 0u;
    uint32_t saved_g9 = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine,
        cpu->registers[1] - UINT32_C(4),
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
            machine,
            cpu->registers[VF2_I960_G0_REGISTER],
            &cpu->registers[VF2_I960_G0_REGISTER + 3u]
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER],
            &cpu->registers[VF2_I960_G0_REGISTER + 2u]
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(4);
    if (status != VF2_OK ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] != UINT32_C(0xffff8000) ||
        cpu->registers[VF2_I960_G0_REGISTER + 7u] != UINT32_C(1) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] != UINT32_C(2) ||
        cpu->registers[VF2_I960_G0_REGISTER + 2u] != UINT32_C(6)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[3] = UINT32_C(0);
    for (row = 0u; row < UINT32_C(2); ++row) {
        uint32_t column = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] =
            cpu->registers[VF2_I960_G0_REGISTER + 2u];
        for (column = 0u; column < UINT32_C(6); ++column) {
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
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 9u],
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
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_measured_game_disp_resource_pair(
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
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t resource = 0u;
    uint8_t fighter_index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_FIGHTER0_SLOT, &fighter0
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_FIGHTER1_SLOT, &fighter1
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            fighter0 + VF2_GAME_DISP_RESOURCE_INDEX_OFFSET,
            &fighter_index,
            sizeof(fighter_index)
        );
    }
    cpu->registers[3] = (uint32_t)fighter_index;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = VF2_GAME_DISP_RESOURCE_DEST0;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_RESOURCE_TABLE0 + (uint32_t)fighter_index * UINT32_C(4),
            &resource
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] = resource;
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_GAME_DISP_RESOURCE_HELPER, VF2_GAME_DISP_RESOURCE_RETURN0
        );
    }
    if (status == VF2_OK) {
        status = game_disp_resource_blit_measured(
            machine, cpu, VF2_GAME_DISP_RESOURCE_RETURN0
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_read(
        machine,
        fighter1 + VF2_GAME_DISP_RESOURCE_INDEX_OFFSET,
        &fighter_index,
        sizeof(fighter_index)
    );
    cpu->registers[3] = (uint32_t)fighter_index;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_RESOURCE_TABLE1 + (uint32_t)fighter_index * UINT32_C(4),
            &resource
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] = resource;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = VF2_GAME_DISP_RESOURCE_DEST1;
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_GAME_DISP_RESOURCE_HELPER, VF2_GAME_DISP_RESOURCE_RETURN1
        );
    }
    if (status == VF2_OK) {
        status = game_disp_resource_blit_measured(
            machine, cpu, VF2_GAME_DISP_RESOURCE_RETURN1
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_GAME_DISP_RESOURCE_PAIR_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_RESOURCE_PAIR_RETURN ||
        cpu->procedure_calls != start_calls + UINT64_C(2) ||
        cpu->procedure_returns != start_returns + UINT64_C(3)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_equal_condition(cpu);

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_RESOURCE_PAIR_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_RESOURCE_PAIR_RETURN;
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
        measured_game_disp_resource_pair_case(machine, cpu)) {
        return execute_measured_game_disp_resource_pair(
            machine, cpu, state, report
        );
    }
    return vf2_native_runtime_step_resource_base(
        machine, cpu, state, report
    );
}
