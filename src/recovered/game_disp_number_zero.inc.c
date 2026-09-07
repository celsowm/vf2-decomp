#define VF2_GAME_DISP_NUMBER_ENTRY UINT32_C(0x0002cba0)
#define VF2_GAME_DISP_NUMBER_RETURN UINT32_C(0x0002c99c)
#define VF2_GAME_DISP_NUMBER_TABLE UINT32_C(0x02a68186)
#define VF2_GAME_DISP_NUMBER_LOOKUP UINT32_C(0x0201ed50)
#define VF2_GAME_DISP_NUMBER_HELPER_DIRECT UINT32_C(0x0000932c)
#define VF2_GAME_DISP_NUMBER_HELPER_LOOKUP UINT32_C(0x00009294)
#define VF2_GAME_DISP_NUMBER_INSTRUCTIONS UINT64_C(355)

static bool measured_game_disp_number_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    uint16_t base = 0u;
    uint16_t marker = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_GAME_DISP_NUMBER_ENTRY ||
        cpu->local_frame_depth != UINT32_C(4) ||
        cpu->registers[VF2_I960_G0_REGISTER] != UINT32_C(0) ||
        cpu->registers[VF2_I960_G0_REGISTER + 9u] != UINT32_C(0x010015f4) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[3].registers[2] != VF2_GAME_DISP_NUMBER_RETURN ||
        cpu->local_frames[3].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }
    status = game_disp_leaf_read_u16(machine, VF2_GAME_DISP_NUMBER_TABLE, &base);
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine, VF2_GAME_DISP_NUMBER_TABLE + UINT32_C(2), &marker
        );
    }
    return status == VF2_OK && base == UINT16_C(0x8000) && marker == UINT16_C(1);
}

static vf2_status game_disp_number_symbol_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    bool lookup,
    uint32_t return_address
)
{
    uint16_t base = 0u;
    uint16_t marker = 0u;
    uint8_t symbol = 0u;
    uint32_t data = VF2_GAME_DISP_NUMBER_TABLE + UINT32_C(16);
    uint32_t group = 0u;
    uint32_t remainder = 0u;
    uint32_t source = 0u;
    uint32_t row = 0u;
    vf2_status status = game_disp_leaf_read_u16(
        machine, VF2_GAME_DISP_NUMBER_TABLE, &base
    );

    cpu->registers[3] = (uint32_t)base;
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine, VF2_GAME_DISP_NUMBER_TABLE + UINT32_C(2), &marker
        );
    }
    cpu->registers[15] = (uint32_t)marker;
    if (status != VF2_OK || base != UINT16_C(0x8000) || marker != UINT16_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[11] = data;
    cpu->registers[10] = UINT32_C(0x80);

    if (lookup) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_NUMBER_LOOKUP + cpu->registers[VF2_I960_G0_REGISTER],
            &symbol,
            sizeof(symbol)
        );
        cpu->registers[12] = (uint32_t)symbol;
    } else {
        cpu->registers[12] = cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(40);
    }
    if (status != VF2_OK) {
        return status;
    }

    group = (cpu->registers[12] >> 3u) << 3u;
    remainder = cpu->registers[12] - group;
    cpu->registers[13] = group << 2u;
    cpu->registers[14] = remainder << 2u;
    cpu->registers[15] =
        cpu->registers[13] * UINT32_C(3) + cpu->registers[14];
    source = data + cpu->registers[15];
    cpu->registers[6] = source;

    for (row = 0u; row < UINT32_C(3); ++row) {
        uint32_t column = 0u;
        cpu->registers[7] = UINT32_C(3) - row;
        cpu->registers[8] = UINT32_C(2);
        for (column = 0u; column < UINT32_C(2); ++column) {
            uint16_t cell = 0u;
            uint16_t output = 0u;
            status = game_disp_leaf_read_u16(machine, cpu->registers[6], &cell);
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[15] = (uint32_t)cell + cpu->registers[3];
            output = (uint16_t)cpu->registers[15];
            status = game_disp_leaf_write_u16(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 9u], output
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[6] += UINT32_C(2);
            cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(2);
            --cpu->registers[8];
        }
        cpu->registers[6] += UINT32_C(28);
        cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(128);
        cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(4);
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(128);
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(128);
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(128);
    cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(4);
    set_runtime_equal_condition(cpu);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status game_disp_number_call_symbol(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    bool lookup,
    uint32_t return_address
)
{
    const uint32_t target = lookup
        ? VF2_GAME_DISP_NUMBER_HELPER_LOOKUP
        : VF2_GAME_DISP_NUMBER_HELPER_DIRECT;
    vf2_status status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);

    if (status == VF2_OK) {
        status = game_disp_number_symbol_helper(
            machine, cpu, lookup, return_address
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_number(
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
    vf2_status status = VF2_OK;

    cpu->registers[14] = UINT32_C(0x00057e40);
    cpu->registers[11] = cpu->registers[VF2_I960_G0_REGISTER];
    cpu->registers[VF2_I960_G0_REGISTER] = cpu->registers[11] % UINT32_C(10);
    status = game_disp_number_call_symbol(
        machine, cpu, false, UINT32_C(0x0002cbb8)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(8);
    cpu->registers[11] /= UINT32_C(10);
    cpu->registers[VF2_I960_G0_REGISTER] = cpu->registers[11] % UINT32_C(10);
    status = game_disp_number_call_symbol(
        machine, cpu, false, UINT32_C(0x0002cbc8)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(8);
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(34);
    status = game_disp_number_call_symbol(
        machine, cpu, true, UINT32_C(0x0002cbd4)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(8);
    cpu->registers[11] /= UINT32_C(10);
    cpu->registers[VF2_I960_G0_REGISTER] = cpu->registers[11] % UINT32_C(10);
    status = game_disp_number_call_symbol(
        machine, cpu, false, UINT32_C(0x0002cbe4)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[11] /= UINT32_C(10);
    if (cpu->registers[11] != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    set_runtime_equal_condition(cpu);

    cpu->executed_instructions = start_instructions + VF2_GAME_DISP_NUMBER_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_NUMBER_RETURN ||
        cpu->procedure_calls != start_calls + UINT64_C(4) ||
        cpu->procedure_returns != start_returns + UINT64_C(5)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_NUMBER_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_NUMBER_RETURN;
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
        measured_game_disp_number_case(machine, cpu)) {
        return execute_measured_game_disp_number(machine, cpu, state, report);
    }
    return vf2_native_runtime_step_number_base(machine, cpu, state, report);
}
