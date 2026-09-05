#include "texture_bridge_internal.h"


vf2_status execute_video_register_compose(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t old_flags = 0u;
    uint32_t old_video = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t callback = 0u;
    uint32_t packed_mask = 0u;
    uint32_t control = 0u;
    uint32_t inverted = 0u;
    uint32_t newly_enabled = 0u;
    uint32_t index = 0u;
    uint64_t instructions = UINT64_C(63);
    uint8_t mode = 0u;
    uint8_t raw = 0u;
    uint8_t mapped = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = write_u16(machine, UINT32_C(0x01c00040), UINT16_C(1));
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &old_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050070c), old_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500f00), &mode, 1u);
    }
    if (status != VF2_OK || mode == UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (index = 0u; index < UINT32_C(3); ++index) {
        const uint32_t source = UINT32_C(0x01c00016) + index * UINT32_C(2);
        status = vf2_model2a_read(machine, source, &raw, 1u);
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00050628) + (uint32_t)raw, &mapped, 1u
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        packed_mask |= (uint32_t)mapped << (index * UINT32_C(8));
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 14u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    packed_mask = (~packed_mask) & UINT32_C(8);
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500710), &old_video);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500714), old_video);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500710), packed_mask);
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < UINT32_C(3); ++index) {
        const uint32_t source = UINT32_C(0x01c00010) + index * UINT32_C(2);
        status = vf2_model2a_read(machine, source, &raw, 1u);
        if (status != VF2_OK) {
            return status;
        }
        control |= (uint32_t)raw << (index * UINT32_C(8));
    }
    status = vf2_model2a_read(machine, UINT32_C(0x01c0001c), &raw, 1u);
    if (status != VF2_OK) {
        return status;
    }
    control |= ((uint32_t)(raw & UINT8_C(15)) | UINT32_C(0xf0)) << 24u;
    if ((control & (UINT32_C(1) << 11u)) == 0u) {
        control &= ~(UINT32_C(1) << 10u);
        control |= UINT32_C(1) << 11u;
    }
    if ((control & (UINT32_C(1) << 19u)) == 0u) {
        control &= ~(UINT32_C(1) << 18u);
        control |= UINT32_C(1) << 19u;
    }
    inverted = ~control;
    newly_enabled = inverted & ~old_flags;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500700), inverted);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500704), newly_enabled
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500708), old_flags & control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x005001dc), &callback);
    }
    if (status != VF2_OK) {
        return status;
    }

    if (callback == 0u) {
        callback = UINT32_C(0x00001284);
    } else if (callback == UINT32_C(0x00001284) &&
               (inverted & UINT32_C(0x00000008)) != 0u &&
               (newly_enabled & UINT32_C(0x00f7f700)) == 0u) {
        /* SERVICE is bit 3 of r8 at 0x1218. When it is held, the ROM takes
         * that bbs directly to 0x1228 and skips the second bbs at 0x121c.
         * Relative to the installed-callback/no-change path this removes
         * exactly one helper instruction. */
        instructions += UINT64_C(1);
    } else if ((newly_enabled & UINT32_C(0x00f7f700)) == 0u) {
        /* The helper at 0x00001200 keeps an installed callback when no
         * relevant video bits changed. Its non-zero callback path executes
         * two more instructions than the initial installation path. */
        instructions += UINT64_C(2);
    } else if (callback == UINT32_C(0x00001284) &&
               (newly_enabled & UINT32_C(0x00f7f700)) != 0u) {
        uint8_t callback_bit = 0u;
        const uint32_t relevant = newly_enabled & UINT32_C(0x00f7f700);

        status = vf2_model2a_read(machine, callback, &callback_bit, 1u);
        if (status != VF2_OK || callback_bit >= UINT8_C(32)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if ((UINT32_C(1) << callback_bit) != relevant) {
            callback = UINT32_C(0x00001284);
            instructions += UINT64_C(6);
        } else {
            uint8_t callback_limit = 0u;
            ++callback;
            status = vf2_model2a_read(machine, callback, &callback_limit, 1u);
            if (status != VF2_OK || callback_limit > UINT8_C(31)) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            instructions += UINT64_C(8);
        }
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x005001dc), callback
    );
    if (status != VF2_OK) {
        return status;
    }

    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE;
    report->entry_address = VF2_VIDEO_REGISTER_COMPOSE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(7);
    report->bytes_written = 30u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_input_latch_write(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t value = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(machine, UINT32_C(0x00500718), &value, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500f00), &mode, 1u);
    }
    if (status != VF2_OK || mode == UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_write(machine, UINT32_C(0x01c0001e), &value, 1u);
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE;
    report->entry_address = VF2_VIDEO_INPUT_LATCH_WRITE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 1u;
    report->recovered_instruction_count = UINT64_C(7);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_layer_commit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t source = 0u;
    uint32_t lookup = 0u;
    uint32_t layer_flags = 0u;
    uint16_t palette_value0 = 0u;
    uint16_t palette_value1 = 0u;
    uint16_t source_value0 = 0u;
    uint16_t source_value1 = 0u;
    uint16_t mode16 = 0u;
    uint8_t index = 0u;
    uint8_t frame_mode = 0u;
    uint8_t auxiliary = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Read and validate the complete observed input state before the first
     * write. Unsupported variants therefore leave video/palette memory intact. */
    status = read_u16(
        machine, UINT32_C(0x00503036), &palette_value0
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine, UINT32_C(0x00503038), &palette_value1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &source
        );
    }
    if (status == VF2_OK) {
        status = read_u16(
            machine, source + UINT32_C(0x44), &source_value0
        );
    }
    if (status == VF2_OK) {
        status = read_u16(
            machine, source + UINT32_C(0x46), &source_value1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050d004), &index, 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00026690) + (uint32_t)index * UINT32_C(4),
            &lookup
        );
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x00503054), &mode16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050304c), &layer_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050002b), &frame_mode, 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500031), &auxiliary, 1u
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((mode16 & UINT16_C(3)) != 0u ||
        (layer_flags & UINT32_C(15)) != 0u ||
        frame_mode == UINT8_C(3)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = write_u16(
        machine, UINT32_C(0x018000bc), palette_value0
    );
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x018000be), palette_value1
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180006c), source_value0
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180008c), source_value1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01801058), lookup
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x01800466), UINT16_C(0x82df)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180046a), UINT16_C(0x815b)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x01801466), UINT16_C(0x82df)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x0180146a), UINT16_C(0x815b)
        );
    }
    if (status == VF2_OK) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(40));
    }
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT;
    report->entry_address = VF2_VIDEO_LAYER_COMMIT_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(9);
    report->bytes_written = 20u;
    report->recovered_instruction_count = UINT64_C(40);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    (void)auxiliary;
    return VF2_OK;
}

vf2_status execute_tile_controller_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report glyph_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t controller = 0u;
    uint32_t controller_flags = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&glyph_report, 0, sizeof(glyph_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 2u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[10] = runtime_flags;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0059e000), &controller);
    if (status != VF2_OK || controller != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = controller;

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0059e008), &controller_flags
    );
    if (status != VF2_OK ||
        (controller_flags & ((UINT32_C(1) << 2u) |
                             (UINT32_C(1) << 4u) |
                             UINT32_C(1))) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[9] = controller_flags;

    if ((controller_flags & (UINT32_C(1) << 1u)) == 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(12));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE;
        report->entry_address = VF2_TILE_CONTROLLER_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(0);
        report->changed_values = UINT64_C(0);
        report->bytes_written = 0u;
        report->recovered_instruction_count = UINT64_C(12);
        report->recovered_procedure_calls = UINT64_C(0);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    controller_flags &= ~(UINT32_C(1) << 1u);
    cpu->registers[9] = controller_flags;
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0059e008), controller_flags
    );
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TILE_GLYPH_EXPAND_ENTRY, UINT32_C(0x0004ebfc)
    );
    if (status == VF2_OK) {
        status = execute_tile_glyph_expand(machine, cpu, &glyph_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0004ebfc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(15));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE;
    report->entry_address = VF2_TILE_CONTROLLER_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1) + glyph_report.changed_values;
    report->bytes_written = sizeof(uint32_t) + glyph_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_status_latch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0098000c), &value);
    if (status == VF2_OK) {
        const uint8_t low = (uint8_t)value;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00501000), &low, sizeof(low)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH;
    report->entry_address = VF2_VIDEO_STATUS_LATCH_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written = 1u;
    report->recovered_instruction_count = UINT64_C(4);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}






vf2_status execute_display_transform_defaults(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t state = 0u;
    vf2_status status = VF2_OK;
    static const uint32_t target[3] = {
        UINT32_C(0x40c00000), UINT32_C(0x40966666), UINT32_C(0x41940000)
    };
    static const uint32_t zero[3] = {0u, 0u, 0u};

    if (machine == NULL || cpu == NULL || report == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    if (cpu->local_frame_depth == 0u) return VF2_ERROR_UNSUPPORTED;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050084c), &state);
    if (status == VF2_OK) status = vf2_model2a_write(machine, state + UINT32_C(0x54), target, sizeof(target));
    if (status == VF2_OK) status = vf2_model2a_write(machine, state + UINT32_C(0x60), zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, state + UINT32_C(0x70), 0u);
    if (status != VF2_OK) return status;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(11));
    if (status != VF2_OK) return status;
    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_TRANSFORM_DEFAULTS;
    report->entry_address = VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(7);
    report->bytes_written = 28u;
    report->recovered_instruction_count = UINT64_C(11);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_display_runtime_initialize(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report child;
    uint32_t object = 0u;
    uint32_t state = 0u;
    uint8_t flags = 0u;
    uint8_t byte = 0u;
    vf2_status status = VF2_OK;
    static const struct { uint32_t off; uint32_t value; } words[] = {
        {0x234u, 0x3c872b02u}, {0x238u, 0x3ca3d70au},
        {0x264u, 0x409851ecu}, {0x268u, 0x40d051ecu},
        {0x240u, 0u}, {0x2bcu, 0u}, {0x2c0u, 0u},
        {0x2c4u, 0x3e19999au}, {0x2c8u, 0u}, {0x2ccu, 0xbcf5c28fu}
    };
    static const uint32_t half_zero[] = {0x260u,0x26cu,0x26eu,0x244u,0x2b0u,0x2b2u,0x2b4u,0x2b6u,0x2b8u,0x2bau};
    size_t i = 0u;

    if (machine == NULL || cpu == NULL || report == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    if (cpu->local_frame_depth == 0u) return VF2_ERROR_UNSUPPORTED;
    memset(&child, 0, sizeof(child));
    status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a160), UINT32_C(0xc0900000));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a164), UINT32_C(0x3dcccccd));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a168), UINT32_C(0x3dcccccd));
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0050a14d), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &object);
    if (status == VF2_OK) status = vf2_model2a_read(machine, object + UINT32_C(0xdf), &flags, 1u);
    flags = (uint8_t)(flags & UINT8_C(0xfe));
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0xdf), &flags, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27c), &byte, 1u);
    for (i = 0u; status == VF2_OK && i < sizeof(words)/sizeof(words[0]); ++i)
        status = vf2_model2a_write_u32(machine, object + words[i].off, words[i].value);
    for (i = 0u; status == VF2_OK && i < sizeof(half_zero)/sizeof(half_zero[0]); ++i)
        status = write_u16(machine, object + half_zero[i], 0u);
    if (status == VF2_OK) status = write_u16(machine, object + UINT32_C(0x23c), UINT16_C(13));
    byte = UINT8_C(88);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x23e), &byte, 1u);
    byte = 0u;
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27d), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x23f), &byte, 1u);
    byte = UINT8_C(0xff);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x246), &byte, 1u);
    byte = 0u;
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27e), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, object + UINT32_C(0x27f), &byte, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050084c), &state);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, state + UINT32_C(0x40), 0u);
    if (status != VF2_OK) return status;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_TRANSFORM_DEFAULTS_ENTRY, VF2_DISPLAY_RUNTIME_INITIALIZE_CHILD_RETURN);
    if (status != VF2_OK) return status;
    cpu->executed_instructions += UINT64_C(72);
    status = execute_display_transform_defaults(machine, cpu, &child);
    if (status != VF2_OK) return status;
    if (cpu->ip != VF2_DISPLAY_RUNTIME_INITIALIZE_CHILD_RETURN) return VF2_ERROR_UNSUPPORTED;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(1));
    if (status != VF2_OK) return status;

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_RUNTIME_INITIALIZE;
    report->entry_address = VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(35) + child.changed_values;
    report->bytes_written = 87u + child.bytes_written;
    report->recovered_instruction_count = UINT64_C(73) + child.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = child.recovered_procedure_returns + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


vf2_status execute_video_table_expand_128(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t encoded_count = 0u;
    uint32_t outer_iterations = 0u;
    uint32_t source = VF2_VIDEO_TABLE_EXPAND_128_SOURCE;
    uint32_t destination = VF2_VIDEO_TABLE_EXPAND_128_DESTINATION;
    uint32_t outer = 0u;
    uint8_t last_value = 0u;
    uint64_t instruction_count = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_VIDEO_TABLE_EXPAND_128_COUNT, &encoded_count
    );
    if (status != VF2_OK) {
        return status;
    }

    outer_iterations = encoded_count == 0u ? 1u : encoded_count;
    for (outer = 0u; outer < outer_iterations; ++outer) {
        uint32_t column = 0u;
        for (column = 0u; column < UINT32_C(128); ++column) {
            status = vf2_model2a_read(machine, source, &last_value, sizeof(last_value));
            if (status != VF2_OK) {
                return status;
            }
            status = vf2_model2a_write_u32(
                machine, destination, (uint32_t)last_value
            );
            if (status != VF2_OK) {
                return status;
            }
            ++source;
            destination += UINT32_C(4);
        }
    }

    cpu->registers[VF2_I960_G0_REGISTER] = destination;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = source;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] =
        encoded_count == 0u ? UINT32_MAX : 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
    cpu->registers[3] = (uint32_t)last_value;
    if (encoded_count == 0u) {
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    } else {
        set_equal_condition(cpu);
    }

    instruction_count = UINT64_C(4) + UINT64_C(771) * outer_iterations;
    status = finish_recovered_procedure(machine, cpu, instruction_count);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_TABLE_EXPAND_128;
    report->entry_address = VF2_VIDEO_TABLE_EXPAND_128_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = (uint64_t)outer_iterations * UINT64_C(128);
    report->rows = outer_iterations;
    report->changed_values = (uint64_t)outer_iterations * UINT64_C(128);
    report->bytes_written = (size_t)outer_iterations * 128u * 4u;
    report->recovered_instruction_count = instruction_count;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_video_command_submit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;
    const uint32_t packet = UINT32_C(0x005502e0);

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(machine, UINT32_C(0x00550000), UINT32_C(1));
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, packet, UINT32_C(3));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, packet + UINT32_C(4),
            cpu->registers[VF2_I960_G0_REGISTER]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, packet + UINT32_C(8),
            cpu->registers[VF2_I960_G0_REGISTER + 1u]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, packet + UINT32_C(12),
            cpu->registers[VF2_I960_G0_REGISTER + 2u]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = packet;
    cpu->registers[15] = UINT32_C(3);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(9));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_VIDEO_COMMAND_SUBMIT;
    report->entry_address = VF2_VIDEO_COMMAND_SUBMIT_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(5);
    report->bytes_written = 20u;
    report->recovered_instruction_count = UINT64_C(9);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


vf2_status execute_display_profile_apply(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report mode_report;
    vf2_hybrid_bridge_report color_report;
    vf2_hybrid_bridge_report command_report;
    vf2_hybrid_bridge_report runtime_report;
    vf2_hybrid_bridge_report table_report;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t flags = 0u;
    uint32_t table = 0u;
    uint32_t value32 = 0u;
    uint16_t value16 = 0u;
    uint8_t state0 = 0u;
    uint8_t state1 = 0u;
    uint8_t mode = 0u;
    uint8_t gate = 0u;
    uint8_t value8 = 0u;
    uint64_t exclusive = 0u;
    uint64_t child_instructions = 0u;
    uint64_t child_calls = 0u;
    uint64_t child_returns = 0u;
    uint64_t changed_values = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&mode_report, 0, sizeof(mode_report));
    memset(&color_report, 0, sizeof(color_report));
    memset(&command_report, 0, sizeof(command_report));
    memset(&runtime_report, 0, sizeof(runtime_report));
    memset(&table_report, 0, sizeof(table_report));

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0); ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &player1);
    }
    ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, player0 + UINT32_C(0x1b1), &state0, 1u);
    }
    ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[3] = player0;
    cpu->registers[4] = player1;
    cpu->registers[14] = state0;

    ++exclusive; /* cmpobne 2,state0 */
    if (state0 == UINT8_C(2)) {
        status = vf2_model2a_read(machine, player1 + UINT32_C(0x1b1), &state1, 1u); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[14] = state1;
        ++exclusive; /* cmpobe 1,state1 */
        if (state1 == UINT8_C(1)) {
            mode = UINT8_C(12); cpu->registers[15] = 12u; ++exclusive;
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
            }
            ++exclusive;
            if (status != VF2_OK) return status;
            flags &= ~(UINT32_C(1) << 20u); cpu->registers[15] = flags; ++exclusive;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
            if (status != VF2_OK) return status;
            changed_values += UINT64_C(2); bytes_written += 5u;
            goto profile_flags;
        }
    }

    status = vf2_model2a_read(machine, player0 + UINT32_C(0x1b1), &state0, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[14] = state0;
    ++exclusive; /* cmpobne 1,state0 */
    if (state0 == UINT8_C(1)) {
        status = vf2_model2a_read(machine, player1 + UINT32_C(0x1b1), &state1, 1u); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[14] = state1;
        ++exclusive; /* cmpobne 2,state1 */
        if (state1 == UINT8_C(2)) {
            mode = UINT8_C(12); cpu->registers[15] = 12u; ++exclusive;
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
            }
            ++exclusive;
            if (status != VF2_OK) return status;
            flags &= ~(UINT32_C(1) << 20u); cpu->registers[15] = flags; ++exclusive;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
            if (status != VF2_OK) return status;
            changed_values += UINT64_C(2); bytes_written += 5u;
        }
    }

profile_flags:
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = flags;
    ++exclusive; /* bbc 21 */
    if ((flags & (UINT32_C(1) << 21u)) != 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[15] = flags;
        ++exclusive; /* bbs 20 */
        if ((flags & (UINT32_C(1) << 20u)) != 0u) {
            goto force_mode10;
        }
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = mode;
    ++exclusive; /* cmpobne 10 */
    if (mode == UINT8_C(10)) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050004c), &gate, 1u); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[14] = gate;
        ++exclusive; /* cmpobe 2 */
        if (gate == UINT8_C(2)) {
            mode = UINT8_C(11); cpu->registers[15] = 11u; ++exclusive;
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
            if (status != VF2_OK) return status;
            changed_values += UINT64_C(1); bytes_written += 1u;
            goto normal_constants;
        }
force_mode10:
        mode = UINT8_C(10); cpu->registers[15] = 10u; ++exclusive;
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode, 1u); ++exclusive;
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
        }
        ++exclusive;
        if (status != VF2_OK) return status;
        flags |= UINT32_C(1) << 20u; cpu->registers[15] = flags; ++exclusive;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
        if (status != VF2_OK) return status;
        cpu->registers[15] = UINT32_C(0x3a3117c4); ++exclusive;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a000), UINT32_C(0x3a3117c4)); ++exclusive;
        if (status == VF2_OK) { cpu->registers[15] = UINT32_C(0x40000000); ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a004), UINT32_C(0x40000000)); ++exclusive; }
        if (status != VF2_OK) return status;
        ++exclusive; /* b 0x1fdd0 */
        changed_values += UINT64_C(4); bytes_written += 13u;
        goto call_mode_constants;
    }

normal_constants:
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags); ++exclusive;
    if (status != VF2_OK) return status;
    flags &= ~(UINT32_C(1) << 20u); cpu->registers[15] = flags; ++exclusive;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = UINT32_C(0x3b32674f); ++exclusive;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a000), UINT32_C(0x3b32674f)); ++exclusive;
    if (status == VF2_OK) { cpu->registers[15] = UINT32_C(0x3f800000); ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a004), UINT32_C(0x3f800000)); ++exclusive; }
    if (status != VF2_OK) return status;
    changed_values += UINT64_C(3); bytes_written += 12u;

call_mode_constants:
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY, VF2_DISPLAY_PROFILE_APPLY_MODE_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_display_profile_mode_constants(machine, cpu, &mode_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_MODE_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    cpu->registers[15] = UINT32_C(0x1388); ++exclusive;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00501018), UINT32_C(0x1388)); ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500064), &mode, 1u);
    }
    ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[12] = mode;
    cpu->registers[4] = (uint32_t)mode << 8u; ++exclusive;
    table = UINT32_C(0x0006ee00) + cpu->registers[4];
    status = vf2_model2a_read(machine, table + UINT32_C(0xae), &value8, 1u); ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[15] = value8;
    ++exclusive; /* cmpobne 4 */
    if (value8 == UINT8_C(4)) {
        cpu->registers[15] = UINT32_C(0x10cc); ++exclusive;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00501018), UINT32_C(0x10cc)); ++exclusive;
        if (status != VF2_OK) return status;
        changed_values += UINT64_C(1); bytes_written += 4u;
    }
    cpu->registers[6] = UINT32_C(0x018021ee); ++exclusive;
    status = read_u16(machine, UINT32_C(0x0006ef0c) + cpu->registers[4], &value16); ++exclusive;
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x018021ee), value16);
    }
    ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, table + UINT32_C(0xae), &value8, 1u);
    }
    ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500170), &value8, 1u);
    }
    ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(0xa4), &value32);
    }
    ++exclusive;
    if (status == VF2_OK) {
        status = read_u16(machine, table + UINT32_C(0xa8), &value16);
    }
    ++exclusive;
    if (status != VF2_OK) return status;
    cpu->registers[5] = value32;
    cpu->registers[6] = value16;
    { uint16_t value16b = 0u;
      status = read_u16(machine, table + UINT32_C(0xaa), &value16b); ++exclusive;
      if (status == VF2_OK) {
          status = vf2_model2a_write_u32(machine, UINT32_C(0x00501098), value32);
      }
      ++exclusive;
      if (status == VF2_OK) {
          status = write_u16(machine, UINT32_C(0x00501020), value16);
      }
      ++exclusive;
      if (status == VF2_OK) {
          status = write_u16(machine, UINT32_C(0x00501022), value16b);
      }
      ++exclusive;
      if (status != VF2_OK) return status;
      cpu->registers[7] = value16b;
    }
    changed_values += UINT64_C(5); bytes_written += 13u;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_COLOR_PROFILE_APPLY_ENTRY, VF2_DISPLAY_PROFILE_APPLY_COLOR_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_display_color_profile_apply(machine, cpu, &color_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_COLOR_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    status = vf2_model2a_read_u32(machine, table + UINT32_C(0xb0), &cpu->registers[VF2_I960_G0_REGISTER]); ++exclusive;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(0xb4), &cpu->registers[VF2_I960_G0_REGISTER + 1u]);
    }
    ++exclusive;
    if (status != VF2_OK) return status;
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_VIDEO_COMMAND_SUBMIT_ENTRY, VF2_DISPLAY_PROFILE_APPLY_COMMAND_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_video_command_submit(machine, cpu, &command_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_COMMAND_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    cpu->registers[15] = 0u; ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a014), 0u); ++exclusive;
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a018), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a01c), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a020), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a022), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a024), 0u); ++exclusive; }
    if (status == VF2_OK) { cpu->registers[15] = 0u; ++exclusive; status = write_u16(machine, UINT32_C(0x0050a026), 0u); ++exclusive; }
    if (status != VF2_OK) return status;
    changed_values += UINT64_C(7); bytes_written += 20u;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_DISPLAY_RUNTIME_INITIALIZE_ENTRY, VF2_DISPLAY_PROFILE_APPLY_RUNTIME_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_display_runtime_initialize(machine, cpu, &runtime_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_RUNTIME_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_VIDEO_TABLE_EXPAND_128_ENTRY, VF2_DISPLAY_PROFILE_APPLY_TABLE_RETURN); ++exclusive;
    if (status != VF2_OK) return status;
    status = execute_video_table_expand_128(machine, cpu, &table_report);
    if (status != VF2_OK || cpu->ip != VF2_DISPLAY_PROFILE_APPLY_TABLE_RETURN) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    ++exclusive; /* parent ret */
    cpu->executed_instructions += exclusive;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) return status;

    child_instructions = mode_report.recovered_instruction_count + color_report.recovered_instruction_count + command_report.recovered_instruction_count + runtime_report.recovered_instruction_count + table_report.recovered_instruction_count;
    child_calls = mode_report.recovered_procedure_calls + color_report.recovered_procedure_calls + command_report.recovered_procedure_calls + runtime_report.recovered_procedure_calls + table_report.recovered_procedure_calls;
    child_returns = mode_report.recovered_procedure_returns + color_report.recovered_procedure_returns + command_report.recovered_procedure_returns + runtime_report.recovered_procedure_returns + table_report.recovered_procedure_returns;
    changed_values += mode_report.changed_values + color_report.changed_values + command_report.changed_values + runtime_report.changed_values + table_report.changed_values;
    bytes_written += mode_report.bytes_written + color_report.bytes_written + command_report.bytes_written + runtime_report.bytes_written + table_report.bytes_written;

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_APPLY;
    report->entry_address = VF2_DISPLAY_PROFILE_APPLY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = exclusive + child_instructions;
    report->recovered_procedure_calls = UINT64_C(5) + child_calls;
    report->recovered_procedure_returns = UINT64_C(1) + child_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_display_profile_mode_constants(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report child_report;
    uint8_t mode = 0u;
    uint64_t exclusive_instructions = UINT64_C(5);
    uint64_t changed_values = UINT64_C(26);
    size_t bytes_written = 26u * 4u;
    vf2_status status = VF2_OK;

    static const uint32_t mode6_addresses[11] = {
        UINT32_C(0x0050a0e4), UINT32_C(0x0050a0e8),
        UINT32_C(0x0050a0f0), UINT32_C(0x0050a0f8),
        UINT32_C(0x0050a100), UINT32_C(0x0050a118),
        UINT32_C(0x0050a11c), UINT32_C(0x0050a124),
        UINT32_C(0x0050a128), UINT32_C(0x0050a12c),
        UINT32_C(0x0050a134)
    };
    static const uint32_t mode6_values[11] = {
        UINT32_C(0x3f0a3d71), UINT32_C(0x3f0a3d71),
        UINT32_C(0x3f6b851f), UINT32_C(0x3f5eb852),
        UINT32_C(0x3f028f5c), UINT32_C(0x3f0a3d71),
        UINT32_C(0x3f0a3d71), UINT32_C(0x3f07ae14),
        UINT32_C(0x3f28f5c3), UINT32_C(0x3f11eb85),
        UINT32_C(0x3f028f5c)
    };

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&child_report, 0, sizeof(child_report));

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY,
        VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(1);
    status = execute_display_profile_unit_fill(machine, cpu, &child_report);
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_DISPLAY_PROFILE_MODE_CONSTANTS_CHILD_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &mode, sizeof(mode)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = (uint32_t)mode;

    if (mode == UINT8_C(10)) {
        exclusive_instructions = UINT64_C(8);
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a124), UINT32_C(0x3f0f5c29)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x0050a128), UINT32_C(0x3ef0a3d7)
            );
        }
        cpu->registers[15] = UINT32_C(0x3ef0a3d7);
        set_equal_condition(cpu);
        changed_values += UINT64_C(2);
        bytes_written += 8u;
    } else if (mode == UINT8_C(6)) {
        size_t index = 0u;
        exclusive_instructions = UINT64_C(27);
        for (index = 0u; status == VF2_OK && index < 11u; ++index) {
            status = vf2_model2a_write_u32(
                machine, mode6_addresses[index], mode6_values[index]
            );
        }
        cpu->registers[15] = UINT32_C(0x3f028f5c);
        set_equal_condition(cpu);
        changed_values += UINT64_C(11);
        bytes_written += 44u;
    } else {
        set_signed_condition(cpu, INT32_C(6), (int32_t)mode);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += exclusive_instructions - UINT64_C(2);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(1));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_MODE_CONSTANTS;
    report->entry_address = VF2_DISPLAY_PROFILE_MODE_CONSTANTS_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count =
        exclusive_instructions + child_report.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns =
        child_report.recovered_procedure_returns + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_display_profile_unit_fill(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; index < UINT32_C(26); ++index) {
        status = vf2_model2a_write_u32(
            machine,
            UINT32_C(0x0050a0e0) + index * UINT32_C(4),
            UINT32_C(0x3f800000)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(108));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_PROFILE_UNIT_FILL;
    report->entry_address = VF2_DISPLAY_PROFILE_UNIT_FILL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(26);
    report->changed_values = UINT64_C(26);
    report->bytes_written = 26u * 4u;
    report->recovered_instruction_count = UINT64_C(108);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_display_color_profile_apply(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report child_report;
    uint8_t profile = 0u;
    uint8_t scale[3] = {0u, 0u, 0u};
    uint32_t runtime_flags = 0u;
    uint32_t table = 0u;
    uint64_t prefix_instructions = UINT64_C(11);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&child_report, 0, sizeof(child_report));

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &profile, sizeof(profile)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((runtime_flags & (UINT32_C(1) << 21u)) != 0u) {
        profile = UINT8_C(3);
        ++prefix_instructions;
    }

    table = UINT32_C(0x0006ee00) + ((uint32_t)profile << 8u);
    status = vf2_model2a_read(
        machine, table + UINT32_C(0xb8), scale, sizeof(scale)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000e0), scale, sizeof(scale)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_COLOR_TABLE_REBUILD_ENTRY,
        VF2_DISPLAY_COLOR_PROFILE_APPLY_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += prefix_instructions;
    status = execute_color_table_rebuild(machine, cpu, &child_report);
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_DISPLAY_COLOR_PROFILE_APPLY_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(1));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_DISPLAY_COLOR_PROFILE_APPLY;
    report->entry_address = VF2_DISPLAY_COLOR_PROFILE_APPLY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = child_report.rows;
    report->changed_values = child_report.changed_values + UINT64_C(3);
    report->bytes_written = child_report.bytes_written + 3u;
    report->recovered_instruction_count =
        prefix_instructions + child_report.recovered_instruction_count +
        UINT64_C(1);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns =
        child_report.recovered_procedure_returns + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_color_table_rebuild(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t source[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    uint8_t scale[3] = {0u, 0u, 0u};
    uint32_t destination = UINT32_C(0x00546008);
    uint64_t saturation_count = 0u;
    uint32_t factor = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500234), source, sizeof(source)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000e0), scale, sizeof(scale)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    {
        uint8_t zero_page[UINT32_C(0x120)] = {0u};
        status = vf2_model2a_write(
            machine, destination, zero_page, sizeof(zero_page)
        );
        if (status != VF2_OK) {
            return status;
        }
        destination += UINT32_C(0x120);
        bytes_written += sizeof(zero_page);
    }

    for (factor = 1u; factor <= UINT32_C(27); ++factor) {
        uint32_t step[3];
        uint32_t accumulator[3] = {0u, 0u, 0u};
        uint32_t sample = 0u;
        uint32_t channel = 0u;

        step[0] = factor * (uint32_t)source[1] * UINT32_C(28) / UINT32_C(18);
        step[1] = factor * (uint32_t)source[3] * UINT32_C(28) / UINT32_C(18);
        step[2] = factor * (uint32_t)source[5] * UINT32_C(28) / UINT32_C(18);

        for (channel = 0u; channel < UINT32_C(3); ++channel) {
            status = write_u16(
                machine, destination + channel * UINT32_C(2), UINT16_C(0)
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        destination += UINT32_C(6);
        bytes_written += 6u;

        for (sample = 0u; sample < UINT32_C(47); ++sample) {
            for (channel = 0u; channel < UINT32_C(3); ++channel) {
                const uint8_t base = source[channel * UINT32_C(2)];
                uint32_t value = 0u;

                accumulator[channel] += step[channel];
                value = (uint32_t)base + (accumulator[channel] >> 8u);
                if (value >= UINT32_C(256)) {
                    value = UINT32_MAX;
                    ++saturation_count;
                }
                value = ((uint32_t)scale[channel] * value) >> 7u;
                status = write_u16(
                    machine,
                    destination + channel * UINT32_C(2),
                    (uint16_t)value
                );
                if (status != VF2_OK) {
                    return status;
                }
            }
            destination += UINT32_C(6);
            bytes_written += 6u;
        }
    }

    status = vf2_model2a_write_u32(machine, UINT32_C(0x00546004), 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00546000), 1u);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(
        machine, cpu, UINT64_C(38938) + saturation_count
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_COLOR_TABLE_REBUILD;
    report->entry_address = VF2_COLOR_TABLE_REBUILD_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(27) * UINT64_C(47) * UINT64_C(3);
    report->rows = UINT64_C(28);
    report->changed_values = UINT64_C(28) * UINT64_C(48) * UINT64_C(3) + UINT64_C(2);
    report->bytes_written = bytes_written + 8u;
    report->recovered_instruction_count = UINT64_C(38938) + saturation_count;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_palette_page_upload(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t active = 0u;
    uint32_t page = 0u;
    uint64_t instructions = UINT64_C(2);
    uint64_t pages_uploaded = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00546000), &active);
    if (status != VF2_OK) {
        return status;
    }
    if (active == 0u) {
        /* cmpobe at 0x00002dec branches on equality without updating the
         * i960 arithmetic condition code. Preserve the incoming condition. */
        status = finish_recovered_procedure(machine, cpu, UINT64_C(3));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD;
        report->entry_address = VF2_PALETTE_PAGE_UPLOAD_ENTRY;
        report->exit_address = cpu->ip;
        report->recovered_instruction_count = UINT64_C(3);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00546004), &page);
    if (status != VF2_OK || page > UINT32_C(27)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    instructions += UINT64_C(13);
    for (;;) {
        uint32_t source = UINT32_C(0x00546008) + page * UINT32_C(288);
        uint32_t destination = UINT32_C(0x01800000) + page * UINT32_C(512);
        uint32_t inner = 0u;
        uint32_t old_page = page;
        uint32_t timer3 = 0u;
        uint32_t elapsed = 0u;

        instructions += UINT64_C(1);
        for (inner = 0u; inner < UINT32_C(48); ++inner) {
            uint16_t first = 0u;
            uint16_t second = 0u;
            uint16_t third = 0u;
            status = read_u16(machine, source, &first);
            if (status == VF2_OK) {
                status = read_u16(machine, source + UINT32_C(2), &second);
            }
            if (status == VF2_OK) {
                status = read_u16(machine, source + UINT32_C(4), &third);
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + UINT32_C(0x10000), first
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + UINT32_C(0x14000), second
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + UINT32_C(0x18000), third
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            source += UINT32_C(6);
            destination += UINT32_C(2);
            bytes_written += 6u;
            instructions += UINT64_C(12);
        }
        destination += UINT32_C(416);
        (void)destination;
        page = old_page + UINT32_C(1);
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00546004), page
        );
        if (status != VF2_OK) {
            return status;
        }
        ++pages_uploaded;
        instructions += UINT64_C(5);
        if (old_page == UINT32_C(27)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00546000), 0u
            );
            if (status != VF2_OK) {
                return status;
            }
            set_equal_condition(cpu);
            instructions += UINT64_C(3);
            break;
        }

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
        if (status != VF2_OK) {
            return status;
        }
        elapsed = (UINT32_C(0x000fffff) -
                   (timer3 & UINT32_C(0x000fffff)) - UINT32_C(18)) /
                  UINT32_C(25);
        instructions += UINT64_C(8);
        if (elapsed > UINT32_C(0x62c)) {
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
            cpu->compare_result = VF2_I960_COMPARE_GREATER;
            ++instructions;
            break;
        }
        ++instructions;
        if (pages_uploaded >= UINT64_C(28)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD;
    report->entry_address = VF2_PALETTE_PAGE_UPLOAD_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = pages_uploaded * UINT64_C(48);
    report->rows = pages_uploaded;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
