#define VF2_GAME_DISP_HUD_BODY_ENTRY UINT32_C(0x0002c9b4)
#define VF2_GAME_DISP_HUD_BODY_RETURN UINT32_C(0x0002c96c)
#define VF2_GAME_DISP_MODE_GATE UINT32_C(0x00063828)
#define VF2_GAME_DISP_MODE_GATE_RETURN UINT32_C(0x0002c9b8)
#define VF2_GAME_DISP_MODE_BASE_PTR UINT32_C(0x0050016c)
#define VF2_GAME_DISP_MODE_OFFSET UINT32_C(0x00003351)
#define VF2_GAME_DISP_MARKER_COUNT UINT32_C(0x00500052)
#define VF2_GAME_DISP_SOUND_RATE UINT32_C(0x00500090)
#define VF2_GAME_DISP_HUD_RUNTIME_FLAGS UINT32_C(0x00500068)
#define VF2_GAME_DISP_HUD_RUNTIME_EXPECTED UINT32_C(0x80004400)
#define VF2_GAME_DISP_RESOURCE_A UINT32_C(0x02a67d0c)
#define VF2_GAME_DISP_RESOURCE_B UINT32_C(0x02a67e54)
#define VF2_GAME_DISP_RESOURCE_C UINT32_C(0x02a67d38)
#define VF2_GAME_DISP_RESOURCE_D UINT32_C(0x02a67e44)
#define VF2_GAME_DISP_MARKER0_GLYPHS UINT32_C(0x02a68126)
#define VF2_GAME_DISP_MARKER1_GLYPHS UINT32_C(0x02a68146)
#define VF2_GAME_DISP_MARKER0_ENTRY UINT32_C(0x0002c8f4)
#define VF2_GAME_DISP_MARKER1_ENTRY UINT32_C(0x0002c90c)
#define VF2_GAME_DISP_MARKER0_HELPER_RETURN UINT32_C(0x0002c908)
#define VF2_GAME_DISP_MARKER1_HELPER_RETURN UINT32_C(0x0002c920)
#define VF2_GAME_DISP_HUD_BODY_INSTRUCTIONS UINT64_C(1241)

static bool game_disp_hud_resource_header_expected(
    vf2_model2a *machine,
    uint32_t resource,
    uint32_t width,
    uint32_t height
)
{
    uint32_t base = 0u;
    uint32_t format = 0u;
    uint32_t actual_height = 0u;
    uint32_t actual_width = 0u;
    vf2_status status = game_disp_resource_read_s16(machine, resource, &base);

    if (status == VF2_OK) {
        status = game_disp_resource_read_s16(
            machine, resource + UINT32_C(2), &format
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, resource + UINT32_C(4), &actual_height
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, resource + UINT32_C(8), &actual_width
        );
    }
    return status == VF2_OK && base == UINT32_C(0xffff8000) &&
           format == UINT32_C(1) && actual_width == width &&
           actual_height == height;
}

static bool game_disp_hud_glyph_header_expected(
    vf2_model2a *machine,
    uint32_t table,
    uint32_t width,
    uint32_t height
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
        status = vf2_model2a_read_u32(
            machine, table + UINT32_C(4), &actual_width
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, table + UINT32_C(8), &actual_height
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, table + UINT32_C(12), &stride
        );
    }
    return status == VF2_OK && base == INT16_MIN && marker == INT16_C(1) &&
           actual_width == width && actual_height == height &&
           stride == UINT32_C(10);
}

static bool measured_game_disp_hud_body_case(
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

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_MODE_BASE_PTR, &mode_base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, mode_base + VF2_GAME_DISP_MODE_OFFSET,
            &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_MARKER_COUNT,
            &marker_count, sizeof(marker_count)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_SOUND_RATE,
            &sound_rate, sizeof(sound_rate)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_HUD_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status != VF2_OK || mode != UINT8_C(0) ||
        marker_count != UINT8_C(2) || sound_rate != UINT8_C(30) ||
        runtime_flags != VF2_GAME_DISP_HUD_RUNTIME_EXPECTED) {
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
           game_disp_hud_glyph_header_expected(
               machine, VF2_GAME_DISP_MARKER0_GLYPHS, UINT32_C(2), UINT32_C(2)) &&
           game_disp_hud_glyph_header_expected(
               machine, VF2_GAME_DISP_MARKER1_GLYPHS, UINT32_C(2), UINT32_C(2)) &&
           game_disp_hud_glyph_header_expected(
               machine, VF2_GAME_DISP_INTEGER_GLYPHS, UINT32_C(2), UINT32_C(3)) &&
           game_disp_hud_glyph_header_expected(
               machine, VF2_GAME_DISP_FRACTION_GLYPHS, UINT32_C(1), UINT32_C(2));
}

static vf2_status game_disp_hud_resource_blit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t resource,
    uint32_t width,
    uint32_t height,
    uint32_t return_address
)
{
    uint32_t row = 0u;
    uint32_t base = 0u;
    uint32_t format = 0u;
    uint32_t saved_g9 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->registers[VF2_I960_G0_REGISTER] != resource) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine,
        cpu->registers[1] - UINT32_C(4),
        cpu->registers[VF2_I960_G0_REGISTER + 9u]
    );
    if (status == VF2_OK) {
        status = game_disp_resource_read_s16(machine, resource, &base);
    }
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = base;
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(2);
    if (status == VF2_OK) {
        status = game_disp_resource_read_s16(
            machine, resource + UINT32_C(2), &format
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = format;
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(2);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, resource + UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + 3u]
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, resource + UINT32_C(8),
            &cpu->registers[VF2_I960_G0_REGISTER + 2u]
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(4);
    if (status != VF2_OK || base != UINT32_C(0xffff8000) ||
        format != UINT32_C(1) ||
        cpu->registers[VF2_I960_G0_REGISTER + 2u] != width ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] != height) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[3] = UINT32_C(0);
    for (row = 0u; row < height; ++row) {
        uint32_t column = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = width;
        for (column = 0u; column < width; ++column) {
            uint32_t source = 0u;
            status = game_disp_resource_read_s16(
                machine, cpu->registers[VF2_I960_G0_REGISTER], &source
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(2);
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = source + base;
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
        cpu->registers[VF2_I960_G0_REGISTER + 9u] -= width;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] -= width;
        cpu->registers[3] = UINT32_C(0x80);
        cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(0x80);
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

static vf2_status game_disp_hud_call_resource(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t resource,
    uint32_t destination,
    uint32_t width,
    uint32_t height,
    uint32_t return_address
)
{
    vf2_status status = VF2_OK;

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = destination;
    cpu->registers[VF2_I960_G0_REGISTER] = resource;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_RESOURCE_HELPER, return_address
    );
    if (status == VF2_OK) {
        status = game_disp_hud_resource_blit(
            machine, cpu, resource, width, height, return_address
        );
    }
    return status;
}

static vf2_status game_disp_hud_mode_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t base = 0u;
    uint8_t mode = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_MODE_BASE_PTR, &base
    );

    cpu->registers[15] = base;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + VF2_GAME_DISP_MODE_OFFSET,
            &mode, sizeof(mode)
        );
    }
    cpu->registers[15] = (uint32_t)mode;
    if (status != VF2_OK || (mode & UINT8_C(0x40)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_none_condition(cpu);
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status game_disp_hud_marker_child(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t table,
    uint32_t glyph,
    uint32_t helper_return,
    uint32_t parent_return
)
{
    const uint32_t destination = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    vf2_status status = game_disp_counter_call_glyph(
        machine, cpu, table, destination, glyph,
        helper_return, UINT32_C(2), UINT32_C(2)
    );

    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != parent_return) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status game_disp_hud_call_marker_child(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t child,
    uint32_t table,
    uint32_t glyph,
    uint32_t helper_return,
    uint32_t parent_return
)
{
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu, child, parent_return
    );

    if (status == VF2_OK) {
        status = game_disp_hud_marker_child(
            machine, cpu, table, glyph, helper_return, parent_return
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_hud_body(
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
        machine, cpu, VF2_GAME_DISP_RESOURCE_A,
        UINT32_C(0x010000b6), UINT32_C(8), UINT32_C(2),
        UINT32_C(0x0002c9cc)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000330);
    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_MARKER_COUNT,
        &marker_count, sizeof(marker_count)
    );
    cpu->registers[3] = (uint32_t)marker_count;
    while (status == VF2_OK && cpu->registers[3] != 0u) {
        status = game_disp_hud_call_marker_child(
            machine, cpu,
            VF2_GAME_DISP_MARKER0_ENTRY,
            VF2_GAME_DISP_MARKER0_GLYPHS,
            UINT32_C(1),
            VF2_GAME_DISP_MARKER0_HELPER_RETURN,
            UINT32_C(0x0002c9e0)
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
        machine, VF2_GAME_DISP_MARKER_COUNT,
        &marker_count, sizeof(marker_count)
    );
    cpu->registers[3] = (uint32_t)marker_count;
    while (status == VF2_OK && cpu->registers[3] != 0u) {
        status = game_disp_hud_call_marker_child(
            machine, cpu,
            VF2_GAME_DISP_MARKER1_ENTRY,
            VF2_GAME_DISP_MARKER1_GLYPHS,
            UINT32_C(0),
            VF2_GAME_DISP_MARKER1_HELPER_RETURN,
            UINT32_C(0x0002ca10)
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
            UINT32_C(0x010001b6), UINT32_C(8), UINT32_C(4),
            UINT32_C(0x0002ca40)
        );
    }
    if (status == VF2_OK) {
        status = game_disp_hud_call_resource(
            machine, cpu, VF2_GAME_DISP_RESOURCE_D,
            UINT32_C(0x010001c0), UINT32_C(2), UINT32_C(1),
            UINT32_C(0x0002ca54)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_SOUND_RATE,
        &sound_rate, sizeof(sound_rate)
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
        machine, cpu,
        VF2_GAME_DISP_INTEGER_GLYPHS, UINT32_C(0x010001b8),
        cpu->registers[VF2_I960_G0_REGISTER + 4u],
        UINT32_C(0x0002ca88), UINT32_C(2), UINT32_C(3)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 4u] =
        (uint32_t)((int32_t)cpu->registers[3] % INT32_C(10));
    status = game_disp_counter_call_glyph(
        machine, cpu,
        VF2_GAME_DISP_INTEGER_GLYPHS, UINT32_C(0x010001bc),
        cpu->registers[VF2_I960_G0_REGISTER + 4u],
        UINT32_C(0x0002caa4), UINT32_C(2), UINT32_C(3)
    );
    if (status != VF2_OK) {
        return status;
    }
    status = game_disp_counter_call_glyph(
        machine, cpu,
        VF2_GAME_DISP_FRACTION_GLYPHS, UINT32_C(0x01000240),
        UINT32_C(0), UINT32_C(0x0002cac0),
        UINT32_C(1), UINT32_C(2)
    );
    if (status == VF2_OK) {
        status = game_disp_counter_call_glyph(
            machine, cpu,
            VF2_GAME_DISP_FRACTION_GLYPHS, UINT32_C(0x01000242),
            UINT32_C(0), UINT32_C(0x0002cadc),
            UINT32_C(1), UINT32_C(2)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_GAME_DISP_HUD_BODY_INSTRUCTIONS;
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
        measured_game_disp_hud_body_case(machine, cpu)) {
        return execute_measured_game_disp_hud_body(
            machine, cpu, state, report
        );
    }
    return vf2_native_runtime_step_hud_body_base(
        machine, cpu, state, report
    );
}
