static bool game_disp_hud_glyph_header_with_stride(
    vf2_model2a *machine,
    uint32_t table,
    uint32_t width,
    uint32_t height,
    uint32_t expected_stride
)
{
    int16_t base = 0;
    int16_t marker = 0;
    uint32_t actual_width = 0u;
    uint32_t actual_height = 0u;
    uint32_t stride = 0u;
    vf2_status status = game_disp_read_i16(machine, table, &base);

    if (status == VF2_OK) {
        status = game_disp_read_i16(machine, table + UINT32_C(2), &marker);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(4), &actual_width);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(8), &actual_height);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(12), &stride);
    }
    return status == VF2_OK && base == INT16_MIN && marker == INT16_C(1) &&
           actual_width == width && actual_height == height &&
           stride == expected_stride;
}

static bool measured_game_disp_hud_body_stride_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    uint32_t mode_base = 0u;
    uint32_t runtime_flags = 0u;
    uint8_t mode = UINT8_MAX;
    uint8_t marker_count = UINT8_MAX;
    uint8_t sound_rate = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_GAME_DISP_HUD_BODY_ENTRY ||
        cpu->local_frame_depth != UINT32_C(4) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[3].registers[2] != VF2_GAME_DISP_HUD_BODY_RETURN ||
        cpu->local_frames[3].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_MODE_BASE_PTR, &mode_base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, mode_base + VF2_GAME_DISP_MODE_OFFSET, &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_MARKER_COUNT, &marker_count, sizeof(marker_count)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_SOUND_RATE, &sound_rate, sizeof(sound_rate)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_HUD_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status != VF2_OK || mode != UINT8_C(0) || marker_count != UINT8_C(2) ||
        sound_rate != UINT8_C(30) || runtime_flags != VF2_GAME_DISP_HUD_RUNTIME_EXPECTED) {
        return false;
    }

    return game_disp_hud_resource_header_expected(
               machine, VF2_GAME_DISP_RESOURCE_A, UINT32_C(8), UINT32_C(2)) &&
           game_disp_hud_resource_header_expected(
               machine, VF2_GAME_DISP_RESOURCE_B, UINT32_C(3), UINT32_C(1)) &&
           game_disp_hud_resource_header_expected(
               machine, VF2_GAME_DISP_RESOURCE_C, UINT32_C(8), UINT32_C(4)) &&
           game_disp_hud_resource_header_expected(
               machine, VF2_GAME_DISP_RESOURCE_D, UINT32_C(2), UINT32_C(1)) &&
           game_disp_hud_glyph_header_with_stride(
               machine, VF2_GAME_DISP_MARKER0_GLYPHS,
               UINT32_C(2), UINT32_C(2), UINT32_C(2)) &&
           game_disp_hud_glyph_header_with_stride(
               machine, VF2_GAME_DISP_MARKER1_GLYPHS,
               UINT32_C(2), UINT32_C(2), UINT32_C(2)) &&
           game_disp_hud_glyph_header_with_stride(
               machine, VF2_GAME_DISP_INTEGER_GLYPHS,
               UINT32_C(2), UINT32_C(3), UINT32_C(10)) &&
           game_disp_hud_glyph_header_with_stride(
               machine, VF2_GAME_DISP_FRACTION_GLYPHS,
               UINT32_C(1), UINT32_C(2), UINT32_C(10));
}

static vf2_status game_disp_hud_stride_glyph_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t expected_table,
    uint32_t expected_width,
    uint32_t expected_height,
    uint32_t expected_stride
)
{
    const uint32_t table = cpu->registers[VF2_I960_G0_REGISTER];
    const uint32_t destination_start = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    const uint32_t glyph = cpu->registers[VF2_I960_G0_REGISTER + 4u];
    const uint32_t page = cpu->registers[VF2_I960_G0_REGISTER + 5u];
    int16_t base = 0;
    int16_t marker = 0;
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t stride = 0u;
    uint32_t source = 0u;
    uint32_t destination = destination_start;
    uint32_t row = 0u;
    vf2_status status = VF2_OK;

    if (table != expected_table || page != 0u || glyph > UINT32_C(9)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = game_disp_read_i16(machine, table, &base);
    if (status == VF2_OK) {
        status = game_disp_read_i16(machine, table + UINT32_C(2), &marker);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(4), &width);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(8), &height);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(12), &stride);
    }
    if (status != VF2_OK || base != INT16_MIN || marker != INT16_C(1) ||
        width != expected_width || height != expected_height ||
        stride != expected_stride) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] = (uint32_t)(int32_t)base;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = width;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = height;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = stride;
    source = table + UINT32_C(16) +
        UINT32_C(2) * width * (glyph + height * stride * page);

    for (row = 0u; row < height; ++row) {
        uint32_t column = 0u;
        uint32_t row_source = source;
        uint32_t row_destination = destination;
        for (column = 0u; column < width; ++column) {
            int16_t cell = 0;
            uint16_t output = 0u;
            status = game_disp_read_i16(machine, row_source, &cell);
            if (status != VF2_OK) {
                return status;
            }
            output = (uint16_t)((int32_t)cell + (int32_t)base);
            status = game_disp_leaf_write_u16(machine, row_destination, output);
            if (status != VF2_OK) {
                return status;
            }
            row_source += UINT32_C(2);
            row_destination += UINT32_C(2);
        }
        source += UINT32_C(2) * width * stride;
        destination += UINT32_C(128);
    }

    cpu->registers[VF2_I960_G0_REGISTER] = source;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = destination_start;
    cpu->executed_instructions +=
        UINT64_C(26) + (uint64_t)height *
            (UINT64_C(10) + UINT64_C(7) * (uint64_t)width);
    set_runtime_equal_condition(cpu);
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status game_disp_hud_call_stride_glyph(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t table,
    uint32_t destination,
    uint32_t glyph,
    uint32_t return_address,
    uint32_t width,
    uint32_t height,
    uint32_t stride
)
{
    vf2_status status = VF2_OK;

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = destination;
    cpu->registers[VF2_I960_G0_REGISTER] = table;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = glyph;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = UINT32_C(0);
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_GLYPH_HELPER, return_address
    );
    if (status == VF2_OK) {
        status = game_disp_hud_stride_glyph_helper(
            machine, cpu, table, width, height, stride
        );
    }
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status game_disp_hud_stride_marker_child(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t table,
    uint32_t glyph,
    uint32_t helper_return,
    uint32_t parent_return
)
{
    const uint32_t destination = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    vf2_status status = game_disp_hud_call_stride_glyph(
        machine, cpu, table, destination, glyph, helper_return,
        UINT32_C(2), UINT32_C(2), UINT32_C(2)
    );

    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != parent_return) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status game_disp_hud_stride_call_marker_child(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t child,
    uint32_t table,
    uint32_t glyph,
    uint32_t helper_return,
    uint32_t parent_return
)
{
    vf2_status status = vf2_i960_cpu_enter_procedure(cpu, child, parent_return);
    if (status == VF2_OK) {
        status = game_disp_hud_stride_marker_child(
            machine, cpu, table, glyph, helper_return, parent_return
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_hud_body_stride(
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
    uint32_t runtime_flags = 0u;
    uint8_t marker_count = 0u;
    uint8_t sound_rate = 0u;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_MODE_GATE, VF2_GAME_DISP_MODE_GATE_RETURN
    );

    if (status == VF2_OK) {
        status = game_disp_hud_mode_gate(machine, cpu);
    }
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_MODE_GATE_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = game_disp_hud_call_resource(
        machine, cpu, VF2_GAME_DISP_RESOURCE_A, UINT32_C(0x010000b6),
        UINT32_C(8), UINT32_C(2), UINT32_C(0x0002c9cc)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000330);
    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_MARKER_COUNT, &marker_count, sizeof(marker_count)
    );
    cpu->registers[3] = (uint32_t)marker_count;
    while (status == VF2_OK && cpu->registers[3] != 0u) {
        status = game_disp_hud_stride_call_marker_child(
            machine, cpu, VF2_GAME_DISP_MARKER0_ENTRY,
            VF2_GAME_DISP_MARKER0_GLYPHS, UINT32_C(1),
            VF2_GAME_DISP_MARKER0_HELPER_RETURN, UINT32_C(0x0002c9e0)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(4);
        --cpu->registers[3];
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] -= UINT32_C(6);
    status = game_disp_hud_call_resource(
        machine, cpu, VF2_GAME_DISP_RESOURCE_B,
        cpu->registers[VF2_I960_G0_REGISTER + 9u],
        UINT32_C(3), UINT32_C(1), UINT32_C(0x0002c9fc)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000348);
    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_MARKER_COUNT, &marker_count, sizeof(marker_count)
    );
    cpu->registers[3] = (uint32_t)marker_count;
    while (status == VF2_OK && cpu->registers[3] != 0u) {
        status = game_disp_hud_stride_call_marker_child(
            machine, cpu, VF2_GAME_DISP_MARKER1_ENTRY,
            VF2_GAME_DISP_MARKER1_GLYPHS, UINT32_C(0),
            VF2_GAME_DISP_MARKER1_HELPER_RETURN, UINT32_C(0x0002ca10)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(4);
        --cpu->registers[3];
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(4);
    status = game_disp_hud_call_resource(
        machine, cpu, VF2_GAME_DISP_RESOURCE_B,
        cpu->registers[VF2_I960_G0_REGISTER + 9u],
        UINT32_C(3), UINT32_C(1), UINT32_C(0x0002ca2c)
    );
    if (status == VF2_OK) {
        status = game_disp_hud_call_resource(
            machine, cpu, VF2_GAME_DISP_RESOURCE_C,
            UINT32_C(0x010001b6), UINT32_C(8), UINT32_C(4), UINT32_C(0x0002ca40)
        );
    }
    if (status == VF2_OK) {
        status = game_disp_hud_call_resource(
            machine, cpu, VF2_GAME_DISP_RESOURCE_D,
            UINT32_C(0x010001c0), UINT32_C(2), UINT32_C(1), UINT32_C(0x0002ca54)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_SOUND_RATE, &sound_rate, sizeof(sound_rate)
    );
    cpu->registers[3] = (uint32_t)(int32_t)(int8_t)sound_rate;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_HUD_RUNTIME_FLAGS, &runtime_flags
        );
    }
    cpu->registers[15] = runtime_flags;
    if (status != VF2_OK || (runtime_flags & UINT32_C(0x10)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] =
        (uint32_t)((int32_t)cpu->registers[3] / INT32_C(10));
    status = game_disp_counter_call_glyph(
        machine, cpu, VF2_GAME_DISP_INTEGER_GLYPHS, UINT32_C(0x010001b8),
        cpu->registers[VF2_I960_G0_REGISTER + 4u], UINT32_C(0x0002ca88),
        UINT32_C(2), UINT32_C(3)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 4u] =
        (uint32_t)((int32_t)cpu->registers[3] % INT32_C(10));
    status = game_disp_counter_call_glyph(
        machine, cpu, VF2_GAME_DISP_INTEGER_GLYPHS, UINT32_C(0x010001bc),
        cpu->registers[VF2_I960_G0_REGISTER + 4u], UINT32_C(0x0002caa4),
        UINT32_C(2), UINT32_C(3)
    );
    if (status != VF2_OK) {
        return status;
    }
    status = game_disp_counter_call_glyph(
        machine, cpu, VF2_GAME_DISP_FRACTION_GLYPHS, UINT32_C(0x01000240),
        UINT32_C(0), UINT32_C(0x0002cac0), UINT32_C(1), UINT32_C(2)
    );
    if (status == VF2_OK) {
        status = game_disp_counter_call_glyph(
            machine, cpu, VF2_GAME_DISP_FRACTION_GLYPHS, UINT32_C(0x01000242),
            UINT32_C(0), UINT32_C(0x0002cadc), UINT32_C(1), UINT32_C(2)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + VF2_GAME_DISP_HUD_BODY_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_HUD_BODY_RETURN ||
        cpu->procedure_calls != start_calls + UINT64_C(18) ||
        cpu->procedure_returns != start_returns + UINT64_C(19)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_equal_condition(cpu);

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_HUD_BODY_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_HUD_BODY_RETURN;
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
        measured_game_disp_hud_body_stride_case(machine, cpu)) {
        return execute_measured_game_disp_hud_body_stride(
            machine, cpu, state, report
        );
    }
    return vf2_native_runtime_step_hud_body_stride_base(
        machine, cpu, state, report
    );
}
