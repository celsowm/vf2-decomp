static bool measured_game_disp_counter_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint16_t *value_out,
    uint32_t *flags_out
)
{
    uint16_t value = 0u;
    uint32_t flags = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || value_out == NULL ||
        flags_out == NULL || cpu->ip != VF2_GAME_DISP_COUNTER_ENTRY ||
        cpu->local_frame_depth != UINT32_C(3) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[2].registers[2] != VF2_GAME_DISP_COUNTER_RETURN ||
        cpu->local_frames[2].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = game_disp_leaf_read_u16(
        machine, VF2_GAME_DISP_COUNTER_VALUE, &value
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_RUNTIME_FLAGS, &flags
        );
    }
    if (status != VF2_OK || value > VF2_GAME_DISP_COUNTER_MAX ||
        (flags & ~VF2_GAME_DISP_RUNTIME_HALF_BIT) !=
            VF2_GAME_DISP_RUNTIME_BASE_FLAGS) {
        return false;
    }

    *value_out = value;
    *flags_out = flags;
    return true;
}

static vf2_status game_disp_counter_call_glyph(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t table,
    uint32_t destination,
    uint32_t digit,
    uint32_t return_address,
    uint32_t width,
    uint32_t height
)
{
    vf2_status status = VF2_OK;

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = destination;
    cpu->registers[VF2_I960_G0_REGISTER] = table;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = digit;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_GLYPH_HELPER, return_address
    );
    if (status == VF2_OK) {
        status = game_disp_tile_glyph_helper(
            machine, cpu, table, width, height
        );
    }
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_measured_game_disp_counter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    uint16_t raw_value,
    uint32_t flags,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t value = (uint32_t)raw_value;
    uint32_t integer_value = 0u;
    uint32_t integer_tens = 0u;
    uint32_t integer_units = 0u;
    uint32_t fraction_value = 0u;
    uint32_t fraction_tens = 0u;
    uint32_t fraction_units = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[3] = value;
    cpu->registers[15] = flags;
    if ((flags & VF2_GAME_DISP_RUNTIME_HALF_BIT) != 0u) {
        cpu->registers[3] >>= 1u;
    }
    value = cpu->registers[3];
    cpu->registers[4] = value;
    cpu->registers[3] >>= 6u;
    integer_value = cpu->registers[3];
    cpu->registers[5] = integer_value / UINT32_C(10);
    integer_tens = cpu->registers[5];

    status = game_disp_counter_call_glyph(
        machine, cpu,
        VF2_GAME_DISP_INTEGER_GLYPHS,
        VF2_GAME_DISP_INTEGER_TENS,
        integer_tens,
        UINT32_C(0x0002cb20),
        UINT32_C(2), UINT32_C(3)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[5] *= UINT32_C(10);
    cpu->registers[6] = cpu->registers[3] - cpu->registers[5];
    integer_units = cpu->registers[6];
    status = game_disp_counter_call_glyph(
        machine, cpu,
        VF2_GAME_DISP_INTEGER_GLYPHS,
        VF2_GAME_DISP_INTEGER_UNITS,
        integer_units,
        UINT32_C(0x0002cb44),
        UINT32_C(2), UINT32_C(3)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[7] = UINT32_C(0x3f);
    cpu->registers[4] &= cpu->registers[7];
    cpu->registers[7] = UINT32_C(100);
    cpu->registers[4] *= cpu->registers[7];
    cpu->registers[4] >>= 6u;
    fraction_value = cpu->registers[4];
    cpu->registers[5] = fraction_value / UINT32_C(10);
    fraction_tens = cpu->registers[5];
    status = game_disp_counter_call_glyph(
        machine, cpu,
        VF2_GAME_DISP_FRACTION_GLYPHS,
        VF2_GAME_DISP_FRACTION_TENS,
        fraction_tens,
        UINT32_C(0x0002cb78),
        UINT32_C(1), UINT32_C(2)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[5] *= UINT32_C(10);
    cpu->registers[6] = cpu->registers[4] - cpu->registers[5];
    fraction_units = cpu->registers[6];
    status = game_disp_counter_call_glyph(
        machine, cpu,
        VF2_GAME_DISP_FRACTION_GLYPHS,
        VF2_GAME_DISP_FRACTION_UNITS,
        fraction_units,
        UINT32_C(0x0002cb9c),
        UINT32_C(1), UINT32_C(2)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += VF2_GAME_DISP_COUNTER_PARENT_INSTRUCTIONS +
        (((flags & VF2_GAME_DISP_RUNTIME_HALF_BIT) != 0u) ? UINT64_C(1) : UINT64_C(0));
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_COUNTER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_COUNTER_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_COUNTER_RETURN;
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
    uint16_t counter_value = 0u;
    uint32_t counter_flags = 0u;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_counter_case(
            machine, cpu, &counter_value, &counter_flags
        )) {
        return execute_measured_game_disp_counter(
            machine, cpu, state, counter_value, counter_flags, report
        );
    }
    return vf2_native_runtime_step_counter_base(
        machine, cpu, state, report
    );
}
