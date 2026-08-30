#include "texture_bridge_internal.h"

#include <stdlib.h>

vf2_status write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)(value & UINT16_C(0x00ff)),
        (uint8_t)(value >> 8u)
    };
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

vf2_status read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] |
                            ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

vf2_status texture_bit_reader_initialize(
    const vf2_model2a *machine,
    uint32_t address,
    texture_bit_reader *reader
)
{
    uint16_t word = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || reader == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(reader, 0, sizeof(*reader));
    status = read_u16(machine, address, &word);
    if (status != VF2_OK) {
        return status;
    }
    reader->next_address = address + UINT32_C(2);
    reader->next_word = word;
    return VF2_OK;
}

vf2_status texture_bit_reader_refill(
    const vf2_model2a *machine,
    texture_bit_reader *reader
)
{
    uint16_t word = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || reader == NULL || reader->available_bits > 31u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    reader->last_shifted_word =
        reader->next_word << (reader->available_bits & UINT32_C(31));
    status = read_u16(machine, reader->next_address, &word);
    if (status != VF2_OK) {
        return status;
    }
    reader->next_address += UINT32_C(2);
    reader->accumulator |= reader->last_shifted_word;
    reader->available_bits += UINT32_C(16);
    reader->next_word = word;
    return VF2_OK;
}

vf2_status texture_bit_reader_take(
    const vf2_model2a *machine,
    texture_bit_reader *reader,
    uint32_t width,
    uint32_t *value
)
{
    const uint32_t mask = width == UINT32_C(32)
        ? UINT32_MAX
        : (UINT32_C(1) << width) - UINT32_C(1);
    vf2_status status = VF2_OK;

    if (machine == NULL || reader == NULL || value == NULL ||
        width == 0u || width > UINT32_C(32) ||
        reader->available_bits < width) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *value = reader->accumulator & mask;
    reader->available_bits -= width;
    reader->accumulator >>= (width & UINT32_C(31));
    if (reader->available_bits <= UINT32_C(16)) {
        status = texture_bit_reader_refill(machine, reader);
    }
    return status;
}

vf2_status execute_texture_maintenance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t records[2] = {
        UINT32_C(0x00550188), UINT32_C(0x005501a8)
    };
    const uint32_t first_values[2] = {
        UINT32_C(0x0050083c), UINT32_C(0x00500840)
    };
    const uint32_t second_values[2] = {
        UINT32_C(0x00500804), UINT32_C(0x00500808)
    };
    const uint32_t returns[2] = {
        UINT32_C(0x0004b8f4), UINT32_C(0x0004b910)
    };
    uint32_t index = 0u;
    uint16_t record_value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < UINT32_C(2); ++index) {
        cpu->registers[VF2_I960_G0_REGISTER] = records[index];
        status = vf2_model2a_read_u32(
            machine, first_values[index],
            &cpu->registers[VF2_I960_G0_REGISTER + 1u]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, second_values[index],
                &cpu->registers[VF2_I960_G0_REGISTER + 2u]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_TEXTURE_MAINTENANCE_CHECK_ENTRY, returns[index]
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += UINT64_C(4);
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 2u] + UINT32_C(0x640),
            &cpu->registers[3]
        );
        if (status == VF2_OK) {
            status = read_u16(
                machine, cpu->registers[VF2_I960_G0_REGISTER], &record_value
            );
        }
        cpu->registers[4] = record_value;
        if (status != VF2_OK) {
            return status;
        }
        if (cpu->registers[3] == cpu->registers[4]) {
            uint8_t enabled = 1u;
            status = vf2_model2a_write(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 1u] + UINT32_C(0x44),
                &enabled,
                sizeof(enabled)
            );
            if (status != VF2_OK) {
                return status;
            }
        }
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        if (status != VF2_OK) {
            return status;
        }
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(1));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_MAINTENANCE;
    report->entry_address = VF2_TEXTURE_MAINTENANCE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->recovered_instruction_count = UINT64_C(17);
    report->recovered_procedure_calls = UINT64_C(2);
    report->recovered_procedure_returns = UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_pending_texture_tile_upload(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t translation_bases[3] = {
        UINT32_C(0x00544000), UINT32_C(0x00544200), UINT32_C(0x00544400)
    };
    const uint32_t destination_plane_offsets[3] = {
        UINT32_C(0x00010000), UINT32_C(0x00014000), UINT32_C(0x00018000)
    };
    uint32_t argument0 = cpu->registers[VF2_I960_G0_REGISTER];
    uint32_t argument1 = cpu->registers[VF2_I960_G0_REGISTER + 1u];
    uint32_t source_owner = 0u;
    uint32_t source_offset = 0u;
    uint32_t destination = UINT32_C(0x01800000);
    uint32_t row = 0u;
    uint64_t instructions = 0u;
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
    instructions += UINT64_C(1);
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &source_owner);
    instructions += UINT64_C(1);
    if (status != VF2_OK) return status;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = source_owner;
    instructions += UINT64_C(1);
    if (argument0 != 0u) {
        argument0 = UINT32_C(1) << 10u;
        cpu->registers[VF2_I960_G0_REGISTER] = argument0;
        instructions += UINT64_C(1);
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &source_owner);
        instructions += UINT64_C(1);
        if (status != VF2_OK) return status;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = source_owner;
    }
    status = vf2_model2a_read(machine, source_owner + UINT32_C(0x1b0), &selector, sizeof(selector));
    instructions += UINT64_C(1);
    if (status != VF2_OK) return status;
    instructions += UINT64_C(1);
    if (selector >= UINT8_C(13)) {
        source_offset = UINT32_C(9) << 10u;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = source_offset;
        argument1 -= UINT32_C(13);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
        instructions += UINT64_C(2);
    }
    destination += (UINT32_C(7) << 11u) + argument0;
    instructions += UINT64_C(5);

    for (row = 0u; row < UINT32_C(2); ++row) {
        uint32_t source = source_offset + argument1 * UINT32_C(0x300) + row * UINT32_C(0x180);
        uint32_t source_base = UINT32_C(0x02101000) + source;
        uint32_t work_base = 0u;
        uint8_t mode = 0u;
        uint32_t column = 0u;
        cpu->registers[VF2_I960_G0_REGISTER] = 0u;
        instructions += UINT64_C(7);
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &work_base);
        instructions += UINT64_C(1);
        if (status != VF2_OK) return status;
        status = vf2_model2a_read(machine, work_base + UINT32_C(0x3351), &mode, sizeof(mode));
        instructions += UINT64_C(1);
        if (status != VF2_OK) return status;
        instructions += UINT64_C(1);
        if ((mode & UINT8_C(0x08)) == 0u) {
            instructions += UINT64_C(1);
            if (row == 0u) {
                instructions += UINT64_C(1);
                if (argument1 == UINT32_C(8)) {
                    uint8_t fighter = 0u;
                    status = vf2_model2a_read(machine, source_owner + UINT32_C(0x69c), &fighter, sizeof(fighter));
                    instructions += UINT64_C(1);
                    if (status != VF2_OK) return status;
                    instructions += UINT64_C(1);
                    if (fighter != 0u) {
                        source_base = UINT32_C(0x0210ba40) + ((uint32_t)fighter - UINT32_C(1)) * UINT32_C(0x180);
                        instructions += UINT64_C(5);
                    } else {
                        instructions += UINT64_C(1);
                    }
                } else {
                    instructions += UINT64_C(1);
                }
            } else {
                instructions += UINT64_C(1);
            }
        } else {
            instructions += UINT64_C(1);
        }
        for (column = 0u; column < UINT32_C(64); ++column) {
            uint32_t plane = 0u;
            for (plane = 0u; plane < UINT32_C(3); ++plane) {
                uint16_t index = 0u, color = 0u;
                status = read_u16(machine, source_base + plane * UINT32_C(0x80) + column * UINT32_C(2), &index);
                if (status == VF2_OK) status = read_u16(machine, translation_bases[plane] + (uint32_t)index * UINT32_C(2), &color);
                if (status == VF2_OK) status = write_u16(machine, destination + destination_plane_offsets[plane], color);
                if (status != VF2_OK) return status;
            }
            destination += UINT32_C(2);
            instructions += UINT64_C(13);
        }
        destination += UINT32_C(0x180);
        instructions += UINT64_C(4);
    }
    return finish_recovered_procedure(machine, cpu, instructions + UINT64_C(1));
}

static vf2_status execute_texture_strip_upload(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry_address,
    uint32_t return_address,
    uint32_t rows,
    uint32_t source_stride,
    uint32_t mode
)
{
    const uint32_t translation_bases[3] = {
        UINT32_C(0x00544000), UINT32_C(0x00544200), UINT32_C(0x00544400)
    };
    const uint32_t destination_plane_offsets[3] = {
        UINT32_C(0x00010000), UINT32_C(0x00014000), UINT32_C(0x00018000)
    };
    uint32_t destination = UINT32_C(0x01800000) +
        cpu->registers[VF2_I960_G0_REGISTER] * UINT32_C(0x200);
    uint32_t row = 0u;
    uint64_t instructions = mode == UINT32_C(1)
        ? UINT64_C(7)
        : UINT64_C(8);
    vf2_status status = VF2_OK;

    status = vf2_i960_cpu_enter_procedure(cpu, entry_address, return_address);
    if (status != VF2_OK) {
        return status;
    }

    for (row = 0u; row < rows; ++row) {
        uint32_t source = cpu->registers[VF2_I960_G0_REGISTER + 3u] +
            source_stride * cpu->registers[VF2_I960_G0_REGISTER + 1u] +
            row * UINT32_C(0x60);
        uint32_t column = 0u;

        destination += UINT32_C(0x60);
        instructions += UINT64_C(7);
        for (column = 0u; column < UINT32_C(16); ++column) {
            uint32_t plane = 0u;
            for (plane = 0u; plane < UINT32_C(3); ++plane) {
                uint16_t raw = 0u;
                uint16_t color = 0u;
                uint32_t source_address = source +
                    plane * (mode == UINT32_C(1)
                        ? UINT32_C(0x20)
                        : UINT32_C(0x80));
                uint32_t index = 0u;

                if (mode == UINT32_C(1)) {
                    source_address += column * UINT32_C(2);
                } else {
                    source_address += column * UINT32_C(4);
                }
                status = read_u16(machine, source_address, &raw);
                if (status != VF2_OK) {
                    return status;
                }
                if (mode == UINT32_C(1)) {
                    index = (uint32_t)raw;
                } else {
                    const int32_t signed_raw = (int32_t)(int16_t)raw;
                    index = ((uint32_t)(signed_raw + INT32_C(128))) >> 1u;
                }
                status = read_u16(
                    machine,
                    translation_bases[plane] + index * UINT32_C(2),
                    &color
                );
                if (status == VF2_OK) {
                    status = write_u16(
                        machine,
                        destination + destination_plane_offsets[plane],
                        color
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
            }
            destination += UINT32_C(2);
            instructions += mode == UINT32_C(1)
                ? UINT64_C(15)
                : UINT64_C(27);
        }
        destination += UINT32_C(0x180);
        instructions += UINT64_C(4);
    }
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(16);
    return finish_recovered_procedure(machine, cpu, instructions + UINT64_C(1));
}

static vf2_status execute_texture_upload_7fc(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    return execute_texture_strip_upload(
        machine,
        cpu,
        UINT32_C(0x000007fc),
        return_address,
        UINT32_C(7),
        UINT32_C(0x2a0),
        UINT32_C(1)
    );
}

static vf2_status execute_texture_upload_7f0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    return execute_texture_strip_upload(
        machine,
        cpu,
        UINT32_C(0x000007f0),
        return_address,
        UINT32_C(1),
        UINT32_C(0x300),
        UINT32_C(2)
    );
}

static vf2_status execute_pending_texture_secondary_upload(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t owner = 0u;
    uint8_t selector = 0u;
    uint64_t instructions = UINT64_C(3);
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &owner
    );

    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = owner;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(24);
    if (cpu->registers[VF2_I960_G0_REGISTER] != 0u) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &owner
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = owner;
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(7);
        cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(26);
        instructions += UINT64_C(3);
    }

    cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(0x02105800);
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
    instructions += UINT64_C(3);
    status = vf2_model2a_read(
        machine, owner + UINT32_C(0x1b0), &selector, sizeof(selector)
    );
    if (status != VF2_OK) {
        return status;
    }
    instructions += UINT64_C(1);
    if (selector >= UINT8_C(13)) {
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(0x02107780);
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = UINT32_C(9) << 10u;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] -= UINT32_C(13);
        instructions += UINT64_C(3);
    }

    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(6);
    instructions += UINT64_C(2);
    cpu->executed_instructions += instructions;
    status = execute_texture_upload_7fc(
        machine,
        cpu,
        UINT32_C(0x000007a0)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] =
        cpu->registers[VF2_I960_G0_REGISTER + 4u];
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] =
        UINT32_C(0x02101020) +
        cpu->registers[VF2_I960_G0_REGISTER + 5u];
    cpu->executed_instructions += UINT64_C(5);
    status = execute_texture_upload_7f0(
        machine,
        cpu,
        UINT32_C(0x000007b8)
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] =
        cpu->registers[VF2_I960_G0_REGISTER + 4u] + UINT32_C(1);
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] =
        UINT32_C(0x021011a0) +
        cpu->registers[VF2_I960_G0_REGISTER + 5u];
    cpu->executed_instructions += UINT64_C(5);
    status = execute_texture_upload_7f0(
        machine,
        cpu,
        UINT32_C(0x000007d0)
    );
    if (status != VF2_OK) {
        return status;
    }

    return finish_recovered_procedure(machine, cpu, UINT64_C(1));
}

static vf2_status execute_pending_texture_palette_upload(
    vf2_model2a *machine
)
{
    const uint32_t source_base = UINT32_C(0x0210b3e0);
    const uint32_t destination_base = UINT32_C(0x01811c60);
    const uint32_t translation_bases[3] = {
        UINT32_C(0x00544000),
        UINT32_C(0x00544200),
        UINT32_C(0x00544400)
    };
    const uint32_t destination_plane_offsets[3] = {
        UINT32_C(0),
        UINT32_C(0x00004000),
        UINT32_C(0x00008000)
    };
    uint32_t row = 0u;
    uint32_t plane = 0u;
    uint32_t column = 0u;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    for (row = 0u; row < UINT32_C(7); ++row) {
        const uint32_t row_source =
            source_base + row * UINT32_C(0x60);
        const uint32_t row_destination =
            destination_base + row * UINT32_C(0x200);

        for (plane = 0u; plane < UINT32_C(3); ++plane) {
            const uint32_t plane_source =
                row_source + plane * UINT32_C(0x20);
            const uint32_t plane_destination =
                row_destination + destination_plane_offsets[plane];

            for (column = 0u; column < UINT32_C(16); ++column) {
                uint16_t source_index = 0u;
                uint16_t translated_color = 0u;
                vf2_status status = read_u16(
                    machine,
                    plane_source + column * UINT32_C(2),
                    &source_index
                );

                if (status == VF2_OK) {
                    status = read_u16(
                        machine,
                        translation_bases[plane] +
                            (uint32_t)source_index * UINT32_C(2),
                        &translated_color
                    );
                }
                if (status == VF2_OK) {
                    status = write_u16(
                        machine,
                        plane_destination + column * UINT32_C(2),
                        translated_color
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
            }
        }
    }
    return VF2_OK;
}

vf2_status execute_texture_upload_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t addresses[3] = {
        UINT32_C(0x005502a8),
        UINT32_C(0x005502b0),
        UINT32_C(0x005502b8)
    };
    uint32_t index = 0u;
    uint32_t pending_index = UINT32_C(3);
    uint16_t value = 0u;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t preserve_dispatch_frame = cpu->local_frame_depth >= 3u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < UINT32_C(3); ++index) {
        cpu->registers[5] = addresses[index];
        status = read_u16(machine, addresses[index], &value);
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = value;
        if (value == UINT16_C(1)) {
            pending_index = index;
            break;
        }
    }

    if (pending_index < UINT32_C(3)) {
        uint16_t argument0 = 0u;
        uint16_t argument1 = 0u;

        status = read_u16(
            machine, addresses[pending_index] + UINT32_C(2), &argument0
        );
        if (status == VF2_OK) {
            status = read_u16(
                machine, addresses[pending_index] + UINT32_C(4), &argument1
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (pending_index == UINT32_C(2) &&
            (argument0 != 0u || argument1 != UINT16_C(0x000b))) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = write_u16(machine, addresses[pending_index], 0u);
        if (status == VF2_OK) {
            if (pending_index == UINT32_C(2)) {
                status = execute_pending_texture_palette_upload(machine);
            } else {
                status = vf2_i960_cpu_enter_procedure(
                    cpu, UINT32_C(0x000008e0), UINT32_C(0x0004bae4)
                );
                if (status == VF2_OK) {
                    cpu->registers[VF2_I960_G0_REGISTER] = argument0;
                    cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
                    status = execute_pending_texture_tile_upload(machine, cpu);
                }
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_procedure(
                        cpu, UINT32_C(0x00000754), UINT32_C(0x0004baf4)
                    );
                }
                if (status == VF2_OK) {
                    cpu->registers[VF2_I960_G0_REGISTER] = argument0;
                    cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
                    status = execute_pending_texture_secondary_upload(machine, cpu);
                }
            }
        }
        if (status != VF2_OK) {
            return status;
        }

        if (pending_index != UINT32_C(2)) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH;
            report->entry_address = VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(1);
            report->bytes_written = 2u;
            report->recovered_instruction_count =
                cpu->executed_instructions - start_instructions;
            report->recovered_procedure_calls =
                cpu->procedure_calls - start_calls;
            report->recovered_procedure_returns =
                cpu->procedure_returns - start_returns;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }

        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x10);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(6);
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(0x02109700);
        account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
        if (preserve_dispatch_frame) {
            /* The pending third queue does not return from the dispatch frame
             * at this boundary. The ROM continues at 0x0004bb14 with the
             * frame live; the following interrupt bridge consumes it. */
            cpu->registers[2] = UINT32_C(0x0004bb14);
            cpu->ip = UINT32_C(0x0004bb14);
        } else {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
            if (status != VF2_OK) {
                return status;
            }
        }
        cpu->executed_instructions += UINT64_C(2035);

        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH;
        report->entry_address = VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(125);
        report->changed_values = UINT64_C(337);
        report->bytes_written = 674u;
        report->recovered_instruction_count = UINT64_C(2035);
        report->recovered_procedure_calls = UINT64_C(2);
        report->recovered_procedure_returns =
            preserve_dispatch_frame ? UINT64_C(2) : UINT64_C(3);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_UPLOAD_DISPATCH_TARGET,
        VF2_TEXTURE_UPLOAD_DISPATCH_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(10);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH;
    report->entry_address = VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY;
    report->exit_address = VF2_TEXTURE_UPLOAD_DISPATCH_TARGET;
    report->iterations = UINT64_C(3);
    report->recovered_instruction_count = UINT64_C(10);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_orchestrator_entry_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055000c),
        &cpu->registers[VF2_I960_G0_REGISTER]
    );
    if (status != VF2_OK ||
        cpu->registers[VF2_I960_G0_REGISTER] != 0u ||
        cpu->local_frame_depth == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->ip = VF2_TEXTURE_STATUS_DISPATCH_ENTRY;
    cpu->executed_instructions += UINT64_C(2);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE;
    report->entry_address = VF2_TEXTURE_ORCHESTRATOR_ENTRY_GATE;
    report->exit_address = VF2_TEXTURE_STATUS_DISPATCH_ENTRY;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_record_status_setup(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t short_value = 0u;
    uint32_t flags = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if ((int32_t)cpu->registers[3] >= 0) {
        cpu->ip = VF2_TEXTURE_RECORD_STATUS_SETUP_EXIT;
        cpu->executed_instructions += UINT64_C(1);
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP;
        report->entry_address = VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY;
        report->exit_address = VF2_TEXTURE_RECORD_STATUS_SETUP_EXIT;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = read_u16(
        machine, cpu->registers[5] + UINT32_C(0x1c), &short_value
    );
    cpu->registers[VF2_I960_G0_REGISTER] = short_value;
    if (status == VF2_OK) {
        status = write_u16(machine, VF2_TEXTURE_STATUS_WORD, short_value);
    }
    if (status == VF2_OK) {
        status = read_u16(machine, cpu->registers[5], &short_value);
    }
    cpu->registers[3] = short_value;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0230000c), &cpu->registers[4]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[4] + cpu->registers[3] * UINT32_C(4),
            &cpu->registers[10]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x02300008), &cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[10], &cpu->registers[3]
        );
    }
    cpu->registers[10] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[9] + cpu->registers[3] * UINT32_C(4),
            &cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[9], &cpu->registers[8]
        );
    }
    cpu->registers[9] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x10), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x10), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x10), &flags
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER] = flags;
    if (status != VF2_OK) {
        return status;
    }

    /* These are effective-address overrides in the original routine, not
     * table loads. Bit 4 selects the record's alternate pair at +8/+a;
     * bit 3 or bit 1 selects the pair at +4/+6. */
    if ((flags & (UINT32_C(1) << 4u)) != 0u) {
        uint16_t table_index = 0u;
        uint16_t selected_value = 0u;
        status = read_u16(
            machine, cpu->registers[5] + UINT32_C(8), &table_index
        );
        if (status == VF2_OK) {
            status = read_u16(
                machine, cpu->registers[5] + UINT32_C(0x0a),
                &selected_value
            );
        }
        cpu->registers[3] = table_index;
        cpu->registers[8] = selected_value;
        if (status == VF2_OK) {
            cpu->registers[9] += (uint32_t)table_index * UINT32_C(4);
            cpu->registers[10] += (uint32_t)table_index * UINT32_C(4);
        }
    } else if ((flags & ((UINT32_C(1) << 3u) |
                         (UINT32_C(1) << 1u))) != 0u) {
        uint16_t table_index = 0u;
        uint16_t selected_value = 0u;
        status = read_u16(
            machine, cpu->registers[5] + UINT32_C(4), &table_index
        );
        if (status == VF2_OK) {
            status = read_u16(
                machine, cpu->registers[5] + UINT32_C(6),
                &selected_value
            );
        }
        cpu->registers[3] = table_index;
        cpu->registers[8] = selected_value;
        if (status == VF2_OK) {
            cpu->registers[9] += (uint32_t)table_index * UINT32_C(4);
            cpu->registers[10] += (uint32_t)table_index * UINT32_C(4);
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    status = write_u16(
        machine,
        cpu->registers[5] + UINT32_C(2),
        (uint16_t)cpu->registers[8]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[5] + UINT32_C(0x14), cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[5] + UINT32_C(0x18), cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->ip = cpu->registers[8] == 0u
        ? VF2_TEXTURE_STATUS_DISPATCH_ENTRY
        : VF2_TEXTURE_RECORD_STATUS_SETUP_EXIT;
    cpu->executed_instructions +=
        ((flags & ((UINT32_C(1) << 4u) |
                   (UINT32_C(1) << 3u) |
                   (UINT32_C(1) << 1u))) != 0u)
            ? UINT64_C(26) : UINT64_C(22);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP;
    report->entry_address = VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(4);
    report->bytes_written = 12u;
    report->recovered_instruction_count =
        ((flags & ((UINT32_C(1) << 4u) |
                   (UINT32_C(1) << 3u) |
                   (UINT32_C(1) << 1u))) != 0u)
            ? UINT64_C(26) : UINT64_C(22);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_stream_header_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    enum { VF2_STREAM_MAX_LEVELS = 8 };
    const uint32_t timer_mask = UINT32_C(0x000fffff);
    const uint32_t table_address = UINT32_C(0x0055c2f8);
    vf2_i960_cpu candidate_cpu;
    vf2_hybrid_bridge_report timer_report = {0};
    uint32_t stream = 0u;
    uint32_t first_word = 0u;
    uint32_t tick = 0u;
    uint32_t wait_input = 0u;
    uint32_t timer3 = 0u;
    uint32_t pending_state = 0u;
    uint32_t level_destinations[VF2_STREAM_MAX_LEVELS + 1u] = {0u};
    uint32_t level_rows[VF2_STREAM_MAX_LEVELS] = {0u};
    uint32_t level_blocks[VF2_STREAM_MAX_LEVELS] = {0u};
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t cursor = 0u;
    uint32_t data_start = 0u;
    uint8_t frame_state = 0u;
    uint8_t wait_state = 0u;
    uint8_t raw_width = 0u;
    uint8_t raw_height = 0u;
    uint8_t header_skip = 0u;
    uint8_t probe = 0u;
    uint8_t *source_data = NULL;
    size_t source_size = 0u;
    size_t source_offset = 0u;
    size_t level_count = 0u;
    size_t level = 0u;
    uint64_t loop_instructions = UINT64_C(7);
    uint64_t total_rows = 0u;
    uint64_t total_blocks = 0u;
    uint64_t output_bytes = 0u;
    const uint64_t start_instructions =
        cpu != NULL ? cpu->executed_instructions : 0u;
    const uint64_t start_calls =
        cpu != NULL ? cpu->procedure_calls : 0u;
    const uint64_t start_returns =
        cpu != NULL ? cpu->procedure_returns : 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, cpu->registers[10], &stream);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, stream, &first_word);
    }
    if (status != VF2_OK) {
        return status;
    }

    candidate_cpu = *cpu;
    candidate_cpu.registers[VF2_I960_G0_REGISTER + 3u] =
        stream + UINT32_C(4);
    candidate_cpu.registers[3] = first_word;

    if (first_word == 0u) {
        status = vf2_i960_cpu_enter_procedure(
            &candidate_cpu,
            VF2_TEXTURE_HEADER_DECODE_ENTRY,
            VF2_TEXTURE_STREAM_HEADER_CALL_RETURN
        );
        if (status != VF2_OK) {
            return status;
        }
        candidate_cpu.executed_instructions += UINT64_C(5);
        *cpu = candidate_cpu;

        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL;
        report->entry_address = VF2_TEXTURE_STREAM_HEADER_CALL_ENTRY;
        report->exit_address = VF2_TEXTURE_HEADER_DECODE_ENTRY;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_calls = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (first_word != UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00550004), &tick
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00550008), &wait_input
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00550080), &pending_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500000),
            &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050008c),
            &wait_state, sizeof(wait_state)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != 0u || wait_state != 0u || pending_state != 0u ||
        tick > timer_mask / UINT32_C(25) ||
        (timer3 & timer_mask) == timer_mask ||
        (timer3 & timer_mask) > timer_mask - UINT32_C(25) * tick) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cursor = stream + UINT32_C(4);
    status = vf2_model2a_read(
        machine, cursor, &raw_width, sizeof(raw_width)
    );
    ++cursor;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, cursor, &raw_height, sizeof(raw_height)
        );
    }
    ++cursor;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, cursor, &header_skip, sizeof(header_skip)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    data_start = cursor + (uint32_t)header_skip;
    if ((data_start & UINT32_C(15)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    width = ((uint32_t)raw_width + UINT32_C(1)) >> 1u;
    height = ((uint32_t)raw_height + UINT32_C(1)) >> 1u;
    while (width >= UINT32_C(1) && height >= UINT32_C(8)) {
        uint32_t destination = 0u;
        const uint32_t blocks = height >> 3u;
        const uint64_t level_source =
            (uint64_t)width * (uint64_t)blocks * UINT64_C(16);
        const uint64_t row_output = (uint64_t)blocks * UINT64_C(32);
        uint32_t row = 0u;

        if (level_count >= VF2_STREAM_MAX_LEVELS || blocks == 0u ||
            level_source > SIZE_MAX - source_size) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine,
            table_address + (uint32_t)level_count * UINT32_C(4),
            &destination
        );
        if (status != VF2_OK) {
            return status;
        }
        level_destinations[level_count] = destination;
        level_rows[level_count] = width;
        level_blocks[level_count] = blocks;

        for (row = 0u; row < width; ++row) {
            const uint64_t row_start =
                (uint64_t)destination + (uint64_t)row * UINT64_C(0x800);
            const uint64_t row_end = row_start + row_output - UINT64_C(1);
            if (row_end > UINT32_MAX) {
                return VF2_ERROR_UNSUPPORTED;
            }
            status = vf2_model2a_read(
                machine, (uint32_t)row_start, &probe, sizeof(probe)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, (uint32_t)row_end, &probe, sizeof(probe)
                );
            }
            if (status != VF2_OK) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }

        source_size += (size_t)level_source;
        total_rows += width;
        total_blocks += (uint64_t)width * blocks;
        output_bytes += (uint64_t)width * row_output;
        loop_instructions += UINT64_C(8) +
            (uint64_t)width *
                (UINT64_C(9) + UINT64_C(12) * blocks);
        ++level_count;
        width >>= 1u;
        height >>= 1u;
    }
    if (level_count == 0u || level_count > VF2_STREAM_MAX_LEVELS) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine,
        table_address + (uint32_t)level_count * UINT32_C(4),
        &level_destinations[level_count]
    );
    if (status != VF2_OK) {
        return status;
    }

    source_data = (uint8_t *)malloc(source_size);
    if (source_data == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    status = vf2_model2a_read(
        machine, data_start, source_data, source_size
    );
    if (status != VF2_OK) {
        free(source_data);
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_TEXTURE_STREAM_EXPAND_ENTRY,
        VF2_TEXTURE_STREAM_EXPAND_RETURN
    );
    if (status != VF2_OK) {
        free(source_data);
        return status;
    }
    candidate_cpu.executed_instructions += UINT64_C(5);

    candidate_cpu.registers[VF2_I960_G0_REGISTER] = wait_input;
    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_TIMER_WAIT_UPDATE_ENTRY,
        UINT32_C(0x0004ca28)
    );
    if (status == VF2_OK) {
        candidate_cpu.executed_instructions += UINT64_C(13);
        status = execute_timer_wait_update(
            machine, &candidate_cpu, &timer_report
        );
    }
    if (status != VF2_OK || candidate_cpu.ip != UINT32_C(0x0004ca28)) {
        free(source_data);
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    candidate_cpu.executed_instructions += UINT64_C(14);

    for (level = 0u; level < level_count; ++level) {
        uint32_t row = 0u;
        for (row = 0u; row < level_rows[level]; ++row) {
            uint32_t block = 0u;
            const uint32_t row_destination =
                level_destinations[level] + row * UINT32_C(0x800);
            for (block = 0u; block < level_blocks[level]; ++block) {
                uint32_t words[4] = {0u, 0u, 0u, 0u};
                uint32_t word_index = 0u;
                const uint32_t destination =
                    row_destination + block * UINT32_C(32);
                const uint8_t *source = source_data + source_offset;

                for (word_index = 0u; word_index < 4u; ++word_index) {
                    const uint8_t *word = source + word_index * 4u;
                    words[word_index] =
                        (uint32_t)word[0] |
                        ((uint32_t)word[1] << 8u) |
                        ((uint32_t)word[2] << 16u) |
                        ((uint32_t)word[3] << 24u);
                }
                status = vf2_model2a_write(
                    machine, destination, source, 16u
                );
                for (word_index = 0u;
                     status == VF2_OK && word_index < 4u;
                     ++word_index) {
                    status = vf2_model2a_write_u32(
                        machine,
                        destination + UINT32_C(16) + word_index * UINT32_C(4),
                        words[word_index] >> 16u
                    );
                }
                if (status != VF2_OK) {
                    free(source_data);
                    return status;
                }
                source_offset += 16u;
            }
        }
    }
    if (source_offset != source_size || total_blocks == 0u) {
        free(source_data);
        return VF2_ERROR_UNSUPPORTED;
    }
    {
        const uint8_t *last = source_data + source_size - 16u;
        uint32_t word_index = 0u;
        for (word_index = 0u; word_index < 4u; ++word_index) {
            const uint8_t *word = last + word_index * 4u;
            const uint32_t value =
                (uint32_t)word[0] |
                ((uint32_t)word[1] << 8u) |
                ((uint32_t)word[2] << 16u) |
                ((uint32_t)word[3] << 24u);
            candidate_cpu.registers[VF2_I960_G0_REGISTER + word_index] = value;
            candidate_cpu.registers[VF2_I960_G0_REGISTER + 4u + word_index] =
                value >> 16u;
        }
    }
    free(source_data);
    candidate_cpu.registers[VF2_I960_G0_REGISTER] = 0u;
    candidate_cpu.registers[VF2_I960_G0_REGISTER + 12u] = 0u;
    candidate_cpu.registers[VF2_I960_G0_REGISTER + 13u] = 0u;
    candidate_cpu.executed_instructions += loop_instructions;
    status = vf2_i960_cpu_return_procedure(&candidate_cpu, machine);
    if (status != VF2_OK ||
        candidate_cpu.ip != VF2_TEXTURE_STREAM_EXPAND_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    *cpu = candidate_cpu;

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL;
    report->entry_address = VF2_TEXTURE_STREAM_HEADER_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = total_blocks;
    report->rows = total_rows;
    report->changed_values = total_blocks * UINT64_C(8);
    report->bytes_written =
        (size_t)output_bytes + timer_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


vf2_status execute_texture_stream_resume_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t pending_state = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00550080), &pending_state
    );
    if (status != VF2_OK || pending_state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    cpu->ip = VF2_TEXTURE_RECORD_ADVANCE_ENTRY;
    cpu->executed_instructions += UINT64_C(3);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL;
    report->entry_address = VF2_TEXTURE_STREAM_RESUME_GATE_ENTRY;
    report->exit_address = VF2_TEXTURE_RECORD_ADVANCE_ENTRY;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


vf2_status execute_texture_convert_loop(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r12 = cpu->registers[12] >> 1u;
    uint32_t r13 = cpu->registers[13] >> 1u;
    uint64_t instructions = UINT64_C(3);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[12] = r12;
    cpu->registers[13] = r13;
    if (r12 == 0u) {
        set_equal_condition(cpu);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        instructions = UINT64_C(4);
        if (status != VF2_OK) {
            return status;
        }
        report->recovered_procedure_returns = UINT64_C(1);
    } else if (r13 == 0u) {
        set_equal_condition(cpu);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        instructions = UINT64_C(5);
        if (status != VF2_OK) {
            return status;
        }
        report->recovered_procedure_returns = UINT64_C(1);
    } else {
        uint32_t source = 0u;
        set_equal_condition(cpu);
        cpu->registers[11] += UINT32_C(4);
        status = vf2_model2a_read_u32(machine, cpu->registers[11], &source);
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[27] = source;
        cpu->registers[28] = r12;
        cpu->registers[29] = r13;
        cpu->executed_instructions += UINT64_C(9);
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_TEXTURE_CONVERT_ENTRY, VF2_TEXTURE_CONVERT_POST_ENTRY
        );
        instructions = UINT64_C(9);
        if (status != VF2_OK) {
            return status;
        }
        report->recovered_procedure_calls = UINT64_C(1);
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP;
    report->entry_address = VF2_TEXTURE_CONVERT_LOOP_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_convert_post(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t state = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_CONVERT_STATE, &state
    );
    if (status != VF2_OK || state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = 0u;
    set_equal_condition(cpu);
    cpu->ip = VF2_TEXTURE_CONVERT_LOOP_ENTRY;
    cpu->executed_instructions += UINT64_C(3);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST;
    report->entry_address = VF2_TEXTURE_CONVERT_POST_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_timer_wait_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t mask = UINT32_C(0x000fffff);
    const uint32_t input = cpu->registers[16];
    uint32_t timer3 = 0u;
    int32_t delta = 0;
    uint8_t wait_value = 0u;
    uint64_t instructions = UINT64_C(11);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00f0000c), mask
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    delta = (int32_t)((timer3 & mask) - (mask - UINT32_C(25) * input));
    set_signed_condition(cpu, 0, delta);
    if (delta > 0) {
        wait_value = 0u;
        cpu->registers[16] = 0u;
        instructions = UINT64_C(12);
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050008c), &wait_value, sizeof(wait_value)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00f0000c), (uint32_t)delta
            );
        }
    } else {
        wait_value = 1u;
        cpu->registers[16] = 1u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050008c), &wait_value, sizeof(wait_value)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    /* The observed caller-facing post-state leaves CC equal on return. */
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE;
    report->entry_address = VF2_TIMER_WAIT_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->bytes_written = delta > 0 ? 5u : 1u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_word_prepare(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report timer_report;
    const uint32_t mask = UINT32_C(0x000fffff);
    uint32_t tick = 0u;
    uint32_t timer2 = 0u;
    uint32_t timer3 = 0u;
    uint32_t input = 0u;
    uint32_t child_state = 0u;
    uint32_t flags = 0u;
    uint64_t wrapper_instructions = UINT64_C(22);
    uint8_t frame_state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&timer_report, 0, sizeof(timer_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550004), &tick);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f00008), &timer2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00f0000c), &timer3
        );
    }
    if (status != VF2_OK || frame_state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[13] = mask - UINT32_C(25) * tick;
    cpu->registers[14] = mask;
    cpu->registers[15] = timer3 & mask;
    if (cpu->registers[15] != mask) {
        ++wrapper_instructions;
        if (cpu->registers[15] > cpu->registers[13]) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    (void)timer2;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550008), &input);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[16] = input;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TIMER_WAIT_UPDATE_ENTRY, VF2_TEXTURE_WORD_PREPARE_TIMER_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += wrapper_instructions - UINT64_C(10);
    status = execute_timer_wait_update(machine, cpu, &timer_report);
    if (status != VF2_OK || cpu->ip != VF2_TEXTURE_WORD_PREPARE_TIMER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_HEADER_STATE, &child_state
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags
        );
    }
    if (status != VF2_OK || child_state != 0u ||
        (flags & (UINT32_C(1) << 1u)) != 0u ||
        (flags & (UINT32_C(1) << 2u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = flags;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055c340), &cpu->registers[8]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0055c2f8), &cpu->registers[11]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[9] = UINT32_C(0x005502f0);
    cpu->ip = VF2_TEXTURE_WORD_PREPARE_EXIT;
    cpu->executed_instructions += UINT64_C(10);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE;
    report->entry_address = VF2_TEXTURE_WORD_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_WORD_PREPARE_EXIT;
    report->iterations = UINT64_C(1);
    report->bytes_written = timer_report.bytes_written;
    report->recovered_instruction_count =
        wrapper_instructions + timer_report.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns =
        timer_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_color_prepare(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report timer_report;
    const uint32_t mask = UINT32_C(0x000fffff);
    uint32_t tick = 0u;
    uint32_t timer2 = 0u;
    uint32_t timer3 = 0u;
    uint32_t input = 0u;
    uint32_t child_state = 0u;
    uint32_t flags = 0u;
    uint16_t width = 0u;
    uint16_t height = 0u;
    uint8_t frame_state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&timer_report, 0, sizeof(timer_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550004), &tick);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00f00008), &timer2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00f0000c), &timer3);
    }
    if (status != VF2_OK || frame_state != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[13] = mask - UINT32_C(25) * tick;
    cpu->registers[14] = mask;
    cpu->registers[15] = timer3 & mask;
    if (cpu->registers[15] == mask ||
        cpu->registers[15] > cpu->registers[13]) {
        return VF2_ERROR_UNSUPPORTED;
    }
    (void)timer2;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550008), &input);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[16] = input;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TIMER_WAIT_UPDATE_ENTRY, VF2_TEXTURE_COLOR_PREPARE_TIMER_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(13);
    status = execute_timer_wait_update(machine, cpu, &timer_report);
    if (status != VF2_OK || cpu->ip != VF2_TEXTURE_COLOR_PREPARE_TIMER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_HEADER_STATE, &child_state);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags);
    }
    if (status != VF2_OK || child_state != 0u ||
        (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = flags;
    status = read_u16(machine, UINT32_C(0x0055c320), &width);
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c322), &height);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[11] = UINT32_C(0x0055c2f8);
    cpu->registers[12] = width;
    cpu->registers[13] = height;
    cpu->registers[26] = UINT32_C(1) << 11u;
    cpu->ip = VF2_TEXTURE_COLOR_PREPARE_EXIT;
    cpu->executed_instructions += UINT64_C(8);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE;
    report->entry_address = VF2_TEXTURE_COLOR_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_COLOR_PREPARE_EXIT;
    report->iterations = UINT64_C(1);
    report->bytes_written = timer_report.bytes_written;
    report->recovered_instruction_count =
        UINT64_C(21) + timer_report.recovered_instruction_count;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = timer_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status texture_tree_write_leaf(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t level,
    uint32_t packed,
    uint32_t start_index,
    texture_tree_stats *stats
)
{
    uint32_t index = start_index;
    uint32_t stride = 0u;
    uint32_t table_value = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[20] = packed | ((level & UINT32_C(0xff)) << 24u);
    cpu->registers[16] = cpu->registers[20] >> 28u;
    status = vf2_model2a_read_u32(
        machine,
        VF2_TEXTURE_TREE_TABLE + cpu->registers[16] * UINT32_C(4),
        &table_value
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[21] = table_value;
    stride = UINT32_C(1) << (level & UINT32_C(31));
    if (stride == 0u || cpu->registers[25] == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    stats->instructions += UINT64_C(5);
    do {
        const uint32_t address =
            cpu->registers[26] + index * UINT32_C(8);
        status = vf2_model2a_write_u32(
            machine, address, cpu->registers[20]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, address + UINT32_C(4), cpu->registers[21]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        stats->instructions += UINT64_C(3);
        ++stats->writes;
        stats->bytes_written += 8u;
        index += stride;
    } while (index < cpu->registers[25]);
    stats->instructions += UINT64_C(1);
    return VF2_OK;
}

vf2_status texture_tree_expand_recursive(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t level,
    uint32_t node_address,
    uint32_t output_index,
    uint32_t nested_depth,
    texture_tree_stats *stats
)
{
    const uint32_t next_level = level + UINT32_C(1);
    uint32_t child_address = 0u;
    uint32_t packed = 0u;
    uint32_t second_index = 0u;
    vf2_status status = VF2_OK;

    stats->instructions += UINT64_C(1);
    if (level == UINT32_C(8)) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[26] + output_index * UINT32_C(8),
            node_address
        );
        if (status != VF2_OK) {
            return status;
        }
        stats->instructions += UINT64_C(2);
        ++stats->writes;
        stats->bytes_written += 4u;
        return VF2_OK;
    }
    if (level > UINT32_C(8) || nested_depth > UINT32_C(16)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    child_address = node_address + UINT32_C(4);
    stats->instructions += UINT64_C(8);
    status = vf2_model2a_read_u32(machine, child_address, &packed);
    if (status != VF2_OK) {
        return status;
    }
    if ((int32_t)packed >= 0) {
        cpu->registers[30] = next_level;
        cpu->registers[29] = child_address;
        cpu->registers[28] = output_index;
        stats->instructions += UINT64_C(4);
        ++stats->nested_calls;
        if (nested_depth + UINT32_C(1) > stats->max_nested_depth) {
            stats->max_nested_depth = nested_depth + UINT32_C(1);
        }
        status = texture_tree_expand_recursive(
            machine, cpu, next_level, child_address, output_index,
            nested_depth + UINT32_C(1), stats
        );
    } else {
        status = texture_tree_write_leaf(
            machine, cpu, next_level, packed, output_index, stats
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    stats->instructions += UINT64_C(5);
    status = vf2_model2a_read_u32(machine, node_address, &child_address);
    if (status != VF2_OK) {
        return status;
    }
    second_index = output_index +
        (UINT32_C(1) << (level & UINT32_C(31)));
    status = vf2_model2a_read_u32(machine, child_address, &packed);
    if (status != VF2_OK) {
        return status;
    }
    if ((int32_t)packed >= 0) {
        cpu->registers[30] = next_level;
        cpu->registers[29] = child_address;
        cpu->registers[28] = second_index;
        stats->instructions += UINT64_C(4);
        ++stats->nested_calls;
        if (nested_depth + UINT32_C(1) > stats->max_nested_depth) {
            stats->max_nested_depth = nested_depth + UINT32_C(1);
        }
        status = texture_tree_expand_recursive(
            machine, cpu, next_level, child_address, second_index,
            nested_depth + UINT32_C(1), stats
        );
    } else {
        status = texture_tree_write_leaf(
            machine, cpu, next_level, packed, second_index, stats
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    stats->instructions += UINT64_C(1);
    return VF2_OK;
}

vf2_status execute_texture_byte_run(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t count = cpu->registers[17];
    const uint8_t value = (uint8_t)cpu->registers[8];
    uint32_t address = cpu->registers[10];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (count == 0u || count > VF2_TEXTURE_MAX_LOOP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < count; ++index) {
        status = vf2_model2a_write(machine, address, &value, sizeof(value));
        if (status != VF2_OK) {
            return status;
        }
        address -= UINT32_C(2);
    }
    cpu->registers[10] = address;
    cpu->registers[17] = 0u;
    cpu->ip = VF2_TEXTURE_BYTE_RUN_EXIT;
    cpu->executed_instructions += (uint64_t)count * UINT64_C(4);
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_BYTE_RUN;
    report->entry_address = VF2_TEXTURE_BYTE_RUN_ENTRY;
    report->exit_address = VF2_TEXTURE_BYTE_RUN_EXIT;
    report->iterations = count;
    report->bytes_written = count;
    report->recovered_instruction_count =
        (uint64_t)count * UINT64_C(4);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_byte_decode(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r4 = cpu->registers[4];
    uint32_t r5 = cpu->registers[5];
    uint32_t r6 = cpu->registers[6];
    uint32_t r7 = cpu->registers[7];
    uint32_t r8 = cpu->registers[8];
    uint32_t r9 = cpu->registers[9];
    uint32_t r10 = cpu->registers[10];
    uint32_t r11 = cpu->registers[11];
    uint32_t r12 = cpu->registers[12];
    uint32_t r13 = cpu->registers[13];
    uint32_t r14 = cpu->registers[14];
    uint32_t r15 = cpu->registers[15];
    uint32_t g0 = cpu->registers[16];
    uint32_t g1 = cpu->registers[17];
    uint32_t g2 = cpu->registers[18];
    uint32_t g3 = cpu->registers[19];
    uint32_t g4 = cpu->registers[20];
    uint32_t g5 = cpu->registers[21];
    uint32_t g11 = cpu->registers[27];
    uint32_t g12 = cpu->registers[28];
    uint32_t g13 = cpu->registers[29];
    uint32_t g14 = cpu->registers[30];
    uint64_t instructions = 0u;
    uint64_t outputs = 0u;
    uint64_t encoded_runs = 0u;
    uint32_t outer_iterations = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || g14 == 0u || g14 > UINT32_C(4096)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    while (g14 != 0u) {
        uint8_t frame_state = 0u;
        uint16_t row_count = 0u;

        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
        if (status != VF2_OK) {
            return status;
        }
        instructions += UINT64_C(2);
        if (frame_state != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = read_u16(machine, UINT32_C(0x0055c322), &row_count);
        if (status != VF2_OK || row_count == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        g13 = row_count;
        instructions += UINT64_C(2);

        while (g13 != 0u) {
            uint32_t handler = 0u;

            instructions += UINT64_C(3);
            g2 = g4 >> 24u;
            if ((int32_t)g4 >= 0) {
                g3 = g4;
                r4 = UINT32_C(8);
                instructions += UINT64_C(2);
                for (;;) {
                    const bool bit_set =
                        (r13 & (UINT32_C(1) << (r4 & UINT32_C(31)))) != 0u;
                    status = vf2_model2a_read_u32(machine, g3, &g4);
                    if (status != VF2_OK) {
                        return status;
                    }
                    ++r4;
                    instructions += UINT64_C(4);
                    if (bit_set) {
                        instructions += UINT64_C(3);
                        g3 = g4;
                        if ((int32_t)g4 < 0) {
                            break;
                        }
                    } else {
                        instructions += UINT64_C(3);
                        g3 += UINT32_C(4);
                        if ((int32_t)g4 < 0) {
                            ++instructions;
                            break;
                        }
                    }
                    if (r4 > UINT32_C(31)) {
                        return VF2_ERROR_UNSUPPORTED;
                    }
                }
                g0 = g4 >> 28u;
                --r4;
                status = vf2_model2a_read_u32(
                    machine,
                    VF2_TEXTURE_TREE_TABLE + g0 * UINT32_C(4),
                    &handler
                );
                if (status != VF2_OK) {
                    return status;
                }
                g3 = handler;
                r14 -= r4;
                r13 >>= (r4 & UINT32_C(31));
                instructions += UINT64_C(7);
                if (r14 <= UINT32_C(16)) {
                    uint16_t next_bits = 0u;
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                ++instructions;
            } else {
                g2 &= UINT32_C(15);
                r14 -= g2;
                r13 >>= (g2 & UINT32_C(31));
                instructions += UINT64_C(5);
                if (r14 <= UINT32_C(16)) {
                    uint16_t next_bits = 0u;
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                handler = g5;
                ++instructions;
            }

            if (handler == UINT32_C(0x0004c798)) {
                uint32_t table_value = 0u;
                g4 = r13 & r12;
                r14 -= r6;
                r13 >>= (r6 & UINT32_C(31));
                instructions += UINT64_C(5);
                if (r14 <= UINT32_C(16)) {
                    uint16_t next_bits = 0u;
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0055cd50) + g4 * UINT32_C(4),
                    &table_value
                );
                if (status != VF2_OK) {
                    return status;
                }
                g4 = table_value;
                ++instructions;
                handler = UINT32_C(0x0004c7c8);
            }

            if (handler == UINT32_C(0x0004c7c8)) {
                uint16_t palette = 0u;
                r8 = (r8 + g4) & UINT32_C(15);
                status = read_u16(
                    machine,
                    UINT32_C(0x0004adb0) + r8 * UINT32_C(2),
                    &palette
                );
                if (status != VF2_OK) {
                    return status;
                }
                r7 = palette;
                g4 >>= 8u;
                instructions += UINT64_C(4);
                handler = UINT32_C(0x0004c7dc);
            }

            if (handler == UINT32_C(0x0004c7dc)) {
                uint8_t byte_value = (uint8_t)r8;
                status = vf2_model2a_write(
                    machine, r10, &byte_value, sizeof(byte_value)
                );
                if (status == VF2_OK) {
                    status = write_u16(machine, r11, (uint16_t)(g4 + r7));
                }
                if (status != VF2_OK) {
                    return status;
                }
                r10 -= UINT32_C(2);
                g4 += r7;
                r11 += UINT32_C(2);
                g0 = (r13 << 24u) >> 24u;
                status = vf2_model2a_read_u32(
                    machine, r5 + g0 * UINT32_C(8), &g4
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, r5 + g0 * UINT32_C(8) + UINT32_C(4), &g5
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                --g13;
                ++outputs;
                instructions += UINT64_C(10);
                continue;
            }

            if (handler == UINT32_C(0x0004c854)) {
                uint8_t run_value = (uint8_t)r8;
                uint32_t run_length = (g4 << 24u) >> 24u;
                uint32_t run_index = 0u;
                status = write_u16(machine, r11, (uint16_t)cpu->registers[26]);
                if (status != VF2_OK || run_length == 0u || run_length > g13) {
                    return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
                }
                r11 += UINT32_C(2);
                g1 = run_length;
                g13 -= run_length;
                instructions += UINT64_C(5);
                for (run_index = 0u; run_index < run_length; ++run_index) {
                    status = vf2_model2a_write(
                        machine, r10, &run_value, sizeof(run_value)
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                    r10 -= UINT32_C(2);
                    instructions += UINT64_C(4);
                }
                g1 = 0u;
                g2 = (r7 << 8u) | g4;
                status = write_u16(machine, r11, (uint16_t)g2);
                if (status != VF2_OK) {
                    return status;
                }
                r11 += UINT32_C(2);
                g0 = (r13 << 24u) >> 24u;
                status = vf2_model2a_read_u32(
                    machine, r5 + g0 * UINT32_C(8), &g4
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, r5 + g0 * UINT32_C(8) + UINT32_C(4), &g5
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                instructions += UINT64_C(10);
                outputs += run_length;
                ++encoded_runs;
                continue;
            }

            if (handler == UINT32_C(0x0004c89c)) {
                uint16_t next_bits = 0u;
                uint16_t palette = 0u;
                g2 = (r13 << 16u) >> 16u;
                r14 -= UINT32_C(16);
                r13 >>= 16u;
                instructions += UINT64_C(6);
                if (r14 <= UINT32_C(16)) {
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                g1 = r13 & UINT32_C(15);
                r14 -= UINT32_C(4);
                r13 >>= 4u;
                instructions += UINT64_C(5);
                if (r14 <= UINT32_C(16)) {
                    g0 = g11 << (r14 & UINT32_C(31));
                    status = read_u16(machine, r15, &next_bits);
                    if (status != VF2_OK) {
                        return status;
                    }
                    g11 = next_bits;
                    r15 += UINT32_C(2);
                    r13 |= g0;
                    r14 += UINT32_C(16);
                    instructions += UINT64_C(5);
                }
                g0 = (r13 << 24u) >> 24u;
                status = vf2_model2a_read_u32(
                    machine, r5 + g0 * UINT32_C(8), &g4
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, r5 + g0 * UINT32_C(8) + UINT32_C(4), &g5
                    );
                }
                r8 = (r8 + g1) & UINT32_C(15);
                if (status == VF2_OK) {
                    status = read_u16(
                        machine,
                        UINT32_C(0x0004adb0) + r8 * UINT32_C(2),
                        &palette
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                r7 = palette;
                --g13;
                {
                    const uint8_t byte_value = (uint8_t)r8;
                    status = vf2_model2a_write(
                        machine, r10, &byte_value, sizeof(byte_value)
                    );
                }
                g2 += r7;
                if (status == VF2_OK) {
                    status = write_u16(machine, r11, (uint16_t)g2);
                }
                if (status != VF2_OK) {
                    return status;
                }
                r10 -= UINT32_C(2);
                r11 += UINT32_C(2);
                ++outputs;
                instructions += UINT64_C(14);
                continue;
            }

            return VF2_ERROR_UNSUPPORTED;
        }

        g0 = r9;
        r9 = r10;
        r10 = g0;
        --g14;
        ++outer_iterations;
        instructions += UINT64_C(5);
    }
    ++instructions;

    cpu->registers[16] = g0;
    cpu->registers[17] = g1;
    cpu->registers[18] = g2;
    cpu->registers[19] = g3;
    cpu->registers[20] = g4;
    cpu->registers[21] = g5;
    cpu->registers[27] = g11;
    cpu->registers[28] = g12;
    cpu->registers[29] = g13;
    cpu->registers[30] = g14;
    set_equal_condition(cpu);
    cpu->executed_instructions += instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE;
    report->entry_address = VF2_TEXTURE_BYTE_DECODE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = outputs;
    report->rows = outer_iterations;
    report->changed_values = encoded_runs;
    report->bytes_written = (size_t)outputs * 3u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_symbol_table_build(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r3 = cpu->registers[3];
    const uint32_t r4 = cpu->registers[4];
    const uint32_t r5 = cpu->registers[5];
    uint32_t r7 = cpu->registers[7];
    const uint32_t r12 = cpu->registers[12];
    uint32_t r13 = cpu->registers[13];
    uint32_t r14 = cpu->registers[14];
    uint32_t r15 = cpu->registers[15];
    uint32_t g0 = cpu->registers[16];
    uint32_t g2 = cpu->registers[18];
    const uint32_t g3 = cpu->registers[19];
    const uint32_t g4 = cpu->registers[20];
    const uint32_t g5 = cpu->registers[21];
    const uint32_t g6 = cpu->registers[22];
    uint32_t g11 = cpu->registers[27];
    const uint32_t input_count = r7;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    if (r5 == 0u || r5 > UINT32_C(31) || r7 == 0u ||
        r7 > VF2_TEXTURE_MAX_LOOP || r12 == 0u ||
        g3 != UINT32_C(0x00000100) ||
        g4 != UINT32_C(0x00000102) ||
        g5 != UINT32_C(0x00000122) ||
        g6 != UINT32_C(0x00000142)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    do {
        g2 = r13 & r12;
        r14 -= r5;
        r13 >>= (r5 & UINT32_C(31));
        instructions += UINT64_C(5);
        if (r14 <= UINT32_C(16)) {
            uint16_t next_bits = 0u;
            g0 = g11 << (r14 & UINT32_C(31));
            status = read_u16(machine, r15, &next_bits);
            if (status != VF2_OK) {
                return status;
            }
            g11 = next_bits;
            r15 += UINT32_C(2);
            r13 |= g0;
            r14 += UINT32_C(16);
            instructions += UINT64_C(5);
        }

        ++instructions;
        if (g2 >= g6) {
            g2 = r4 + ((g2 - g6) << 2u);
            instructions += UINT64_C(4);
        } else {
            ++instructions;
            if (g2 < g4) {
                ++instructions;
                if (g2 < g3) {
                    uint32_t table_value = 0u;
                    status = vf2_model2a_read_u32(
                        machine,
                        UINT32_C(0x02300010) + g2 * UINT32_C(4),
                        &table_value
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                    g2 = table_value;
                    instructions += UINT64_C(3);
                    if ((g2 & UINT32_C(15)) != 0u) {
                        g0 = UINT32_C(1) << 31u;
                        g2 |= g0;
                        instructions += UINT64_C(3);
                    } else {
                        g2 >>= 8u;
                        g0 = UINT32_C(3) << 30u;
                        g2 |= g0;
                        instructions += UINT64_C(4);
                    }
                } else {
                    ++instructions;
                    if (g2 == g3) {
                        g2 = UINT32_C(9) << 28u;
                    } else {
                        g2 = UINT32_C(5) << 29u;
                    }
                    instructions += UINT64_C(2);
                }
            } else {
                ++instructions;
                if (g2 < g5) {
                    g2 = g2 - g4 + UINT32_C(1);
                    instructions += UINT64_C(3);
                    if (g2 >= UINT32_C(17)) {
                        g2 = (g2 - UINT32_C(16)) << 4u;
                        instructions += UINT64_C(2);
                    }
                    g0 = UINT32_C(11) << 28u;
                    g2 |= g0;
                    instructions += UINT64_C(3);
                } else {
                    g2 = g2 - g5 + UINT32_C(1);
                    instructions += UINT64_C(3);
                    if (g2 >= UINT32_C(17)) {
                        g2 = (g2 - UINT32_C(16)) << 4u;
                        instructions += UINT64_C(2);
                    }
                    g0 = UINT32_C(13) << 28u;
                    g2 |= g0;
                    instructions += UINT64_C(3);
                }
            }
        }

        status = vf2_model2a_write_u32(machine, r3, g2);
        if (status != VF2_OK) {
            return status;
        }
        r3 += UINT32_C(4);
        --r7;
        instructions += UINT64_C(4);
    } while (r7 != 0u);

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055c338), &r7
    );
    if (status != VF2_OK) {
        return status;
    }
    r3 = UINT32_C(0x0055cd50);
    instructions += UINT64_C(2);

    cpu->registers[3] = r3;
    cpu->registers[7] = r7;
    cpu->registers[13] = r13;
    cpu->registers[14] = r14;
    cpu->registers[15] = r15;
    cpu->registers[16] = g0;
    cpu->registers[18] = g2;
    cpu->registers[27] = g11;
    cpu->ip = UINT32_C(0x0004c4d4);
    cpu->executed_instructions += instructions;
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD;
    report->entry_address = VF2_TEXTURE_SYMBOL_TABLE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = input_count;
    report->bytes_written = (size_t)input_count * 4u;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_pair_table_build(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t r3 = cpu->registers[3];
    uint32_t r7 = cpu->registers[7];
    uint32_t r13 = cpu->registers[13];
    uint32_t r14 = cpu->registers[14];
    uint32_t r15 = cpu->registers[15];
    uint32_t g0 = cpu->registers[16];
    uint32_t g2 = cpu->registers[18];
    uint32_t g3 = cpu->registers[19];
    uint32_t g11 = cpu->registers[27];
    const uint32_t input_count = r7;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    ++instructions;
    if (r7 != 0u) {
        do {
            g2 = (r13 << 16u) >> 16u;
            r14 -= UINT32_C(16);
            r13 >>= 16u;
            instructions += UINT64_C(6);
            if (r14 <= UINT32_C(16)) {
                uint16_t next_bits = 0u;
                g0 = g11 << (r14 & UINT32_C(31));
                status = read_u16(machine, r15, &next_bits);
                if (status != VF2_OK) {
                    return status;
                }
                g11 = next_bits;
                r15 += UINT32_C(2);
                r13 |= g0;
                r14 += UINT32_C(16);
                instructions += UINT64_C(5);
            }

            g3 = r13 & UINT32_C(15);
            r14 -= UINT32_C(4);
            r13 >>= 4u;
            instructions += UINT64_C(5);
            if (r14 <= UINT32_C(16)) {
                uint16_t next_bits = 0u;
                g0 = g11 << (r14 & UINT32_C(31));
                status = read_u16(machine, r15, &next_bits);
                if (status != VF2_OK) {
                    return status;
                }
                g11 = next_bits;
                r15 += UINT32_C(2);
                r13 |= g0;
                r14 += UINT32_C(16);
                instructions += UINT64_C(5);
            }

            g2 = (g2 << 8u) | g3;
            status = vf2_model2a_write_u32(machine, r3, g2);
            if (status != VF2_OK) {
                return status;
            }
            r3 += UINT32_C(4);
            --r7;
            instructions += UINT64_C(6);
        } while (r7 != 0u);
    }

    cpu->registers[3] = r3;
    cpu->registers[7] = r7;
    cpu->registers[13] = r13;
    cpu->registers[14] = r14;
    cpu->registers[15] = r15;
    cpu->registers[16] = g0;
    cpu->registers[18] = g2;
    cpu->registers[19] = g3;
    cpu->registers[27] = g11;
    cpu->ip = UINT32_C(0x0004c544);
    cpu->executed_instructions += instructions;
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD;
    report->entry_address = VF2_TEXTURE_PAIR_TABLE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = input_count;
    report->bytes_written = (size_t)input_count * 4u;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_word_run(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t count = cpu->registers[19];
    const uint16_t value = (uint16_t)cpu->registers[18];
    uint32_t address = cpu->registers[10];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (count == 0u || count > VF2_TEXTURE_MAX_LOOP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < count; ++index) {
        status = write_u16(machine, address, value);
        if (status != VF2_OK) {
            return status;
        }
        address += UINT32_C(4);
    }
    cpu->registers[10] = address;
    cpu->registers[19] = 0u;
    cpu->ip = VF2_TEXTURE_WORD_RUN_EXIT;
    cpu->executed_instructions += (uint64_t)count * UINT64_C(4);
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_WORD_RUN;
    report->entry_address = VF2_TEXTURE_WORD_RUN_ENTRY;
    report->exit_address = VF2_TEXTURE_WORD_RUN_EXIT;
    report->iterations = count;
    report->bytes_written = (size_t)count * 2u;
    report->recovered_instruction_count =
        (uint64_t)count * UINT64_C(4);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_word_decode(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t frame_state = 0u;
    uint8_t frame_wait = 0u;
    uint16_t width_value = 0u;
    uint16_t height_value = 0u;
    uint16_t current_value = 0u;
    uint32_t source = cpu->registers[9];
    uint32_t row_destination = cpu->registers[11];
    uint32_t rows = 0u;
    uint64_t instructions = 0u;
    uint64_t output_values = 0u;
    uint64_t encoded_runs = 0u;
    uint16_t last_encoded = (uint16_t)cpu->registers[17];
    uint16_t last_high_byte = (uint16_t)cpu->registers[20];
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(
        machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_WAIT, &frame_wait, sizeof(frame_wait)
        );
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c320), &width_value);
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c322), &height_value);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != 0u || frame_wait != 0u || width_value == 0u ||
        height_value == 0u || width_value > UINT16_C(2048) ||
        height_value > UINT16_C(2048)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = read_u16(machine, source, &current_value);
    if (status != VF2_OK) {
        return status;
    }
    source += UINT32_C(2);
    instructions += UINT64_C(3);
    rows = width_value;

    while (rows != 0u) {
        uint32_t destination = row_destination;
        int32_t remaining = (int32_t)height_value;

        instructions += UINT64_C(7);
        row_destination += UINT32_C(0x800);
        while (remaining > 0) {
            instructions += UINT64_C(3);
            --remaining;
            if (current_value == (uint16_t)cpu->registers[8]) {
                uint16_t encoded = 0u;
                uint32_t run_length = 0u;
                uint16_t repeated = 0u;

                status = read_u16(machine, source, &encoded);
                if (status != VF2_OK) {
                    return status;
                }
                source += UINT32_C(2);
                ++remaining;
                repeated = (uint16_t)(((uint16_t)(encoded >> 8u)) |
                                      ((uint16_t)(encoded >> 8u) << 8u));
                last_encoded = encoded;
                last_high_byte = (uint16_t)(encoded & UINT16_C(0xff00));
                run_length = (uint32_t)((uint16_t)(encoded ^
                    ((uint16_t)(encoded >> 8u) << 8u)));
                if (run_length == 0u || run_length > (uint32_t)remaining) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                remaining -= (int32_t)run_length;
                instructions += UINT64_C(8);
                while (run_length != 0u) {
                    status = write_u16(machine, destination, repeated);
                    if (status != VF2_OK) {
                        return status;
                    }
                    destination += UINT32_C(4);
                    --run_length;
                    ++output_values;
                    instructions += UINT64_C(4);
                }
                ++encoded_runs;
                status = read_u16(machine, source, &current_value);
                if (status != VF2_OK) {
                    return status;
                }
                source += UINT32_C(2);
                instructions += UINT64_C(4);
            } else {
                status = write_u16(machine, destination, current_value);
                if (status != VF2_OK) {
                    return status;
                }
                destination += UINT32_C(4);
                status = read_u16(machine, source, &current_value);
                if (status != VF2_OK) {
                    return status;
                }
                source += UINT32_C(2);
                ++output_values;
                instructions += UINT64_C(6);
            }
        }
        --rows;
        instructions += UINT64_C(2);
    }
    instructions += UINT64_C(1);

    cpu->registers[16] = 0u;
    cpu->registers[17] = last_encoded;
    cpu->registers[18] = current_value;
    cpu->registers[19] = 0u;
    cpu->registers[20] = last_high_byte;
    set_equal_condition(cpu);
    cpu->executed_instructions += instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE;
    report->entry_address = VF2_TEXTURE_WORD_DECODE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = output_values;
    report->rows = width_value;
    report->changed_values = encoded_runs;
    report->bytes_written = (size_t)output_values * 2u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_tree(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    texture_tree_stats stats;
    const uint32_t entry_depth = cpu->local_frame_depth;
    vf2_status status = VF2_OK;

    if (entry_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&stats, 0, sizeof(stats));
    status = texture_tree_expand_recursive(
        machine,
        cpu,
        cpu->registers[30],
        cpu->registers[29],
        cpu->registers[28],
        0u,
        &stats
    );
    if (status != VF2_OK) {
        return status;
    }
    if (entry_depth + stats.max_nested_depth >
        VF2_I960_MAX_LOCAL_FRAMES) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (cpu->maximum_local_frame_depth <
        entry_depth + stats.max_nested_depth) {
        cpu->maximum_local_frame_depth =
            entry_depth + stats.max_nested_depth;
    }
    cpu->executed_instructions += stats.instructions;
    cpu->procedure_calls += stats.nested_calls;
    cpu->procedure_returns += stats.nested_calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND;
    report->entry_address = VF2_TEXTURE_TREE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = stats.writes;
    report->bytes_written = stats.bytes_written;
    report->max_recursion_depth = stats.max_nested_depth;
    report->recovered_instruction_count = stats.instructions;
    report->recovered_procedure_calls = stats.nested_calls;
    report->recovered_procedure_returns = stats.nested_calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_tree_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report tree_report;
    const uint32_t stack_start = cpu->registers[1];
    const uint32_t saved_registers[] = {
        3u,
        4u, 5u, 6u, 7u,
        8u, 9u, 10u, 11u,
        12u, 13u, 14u, 15u,
        16u, 17u, 18u, 19u,
        20u, 21u, 22u, 23u,
        24u, 25u, 26u, 27u,
        28u, 29u, 30u
    };
    uint32_t index = 0u;
    uint32_t flags = 0u;
    uint32_t table_index = 0u;
    uint32_t value = 0u;
    uint16_t width = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u ||
        stack_start > UINT32_MAX - UINT32_C(112)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&tree_report, 0, sizeof(tree_report));
    for (index = 0u;
         index < sizeof(saved_registers) / sizeof(saved_registers[0]);
         ++index) {
        status = vf2_model2a_write_u32(
            machine,
            stack_start + index * UINT32_C(4),
            cpu->registers[saved_registers[index]]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    cpu->registers[1] = stack_start + UINT32_C(112);
    cpu->registers[26] = UINT32_C(0x00545000);
    cpu->registers[25] = UINT32_C(256);
    cpu->registers[30] = 0u;
    cpu->registers[29] = UINT32_C(0x0055c344);
    cpu->registers[28] = 0u;

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_TEXTURE_TREE_ENTRY, VF2_TEXTURE_TREE_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(26);
    status = execute_texture_tree(machine, cpu, &tree_report);
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_TEXTURE_TREE_RETURN ||
        cpu->registers[1] != stack_start + UINT32_C(112)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u;
         index < sizeof(saved_registers) / sizeof(saved_registers[0]);
         ++index) {
        status = vf2_model2a_read_u32(
            machine,
            stack_start + index * UINT32_C(4),
            &value
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[saved_registers[index]] = value;
    }
    cpu->registers[1] = stack_start;
    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags
    );
    if (status != VF2_OK || (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[16] = flags;
    cpu->registers[11] = UINT32_C(0x005502f0);
    cpu->registers[8] = 0u;
    cpu->registers[7] = 0u;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0055c33c), &cpu->registers[6]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0055c340), &cpu->registers[26]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[5] = UINT32_C(0x00545000);
    cpu->registers[10] = UINT32_C(0x0055c2ef);
    cpu->registers[9] = UINT32_C(0x0055c2ee);
    cpu->registers[12] = cpu->registers[6] >= UINT32_C(32)
        ? UINT32_MAX
        : (UINT32_C(1) << cpu->registers[6]) - UINT32_C(1);
    table_index = cpu->registers[13] & UINT32_C(0xff);
    cpu->registers[16] = table_index;
    status = vf2_model2a_read_u32(
        machine,
        cpu->registers[5] + table_index * UINT32_C(8),
        &cpu->registers[20]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[5] + table_index * UINT32_C(8) + UINT32_C(4),
            &cpu->registers[21]
        );
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c320), &width);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[30] = width;
    cpu->ip = VF2_TEXTURE_TREE_DISPATCH_EXIT;
    cpu->executed_instructions += UINT64_C(37);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH;
    report->entry_address = VF2_TEXTURE_TREE_DISPATCH_ENTRY;
    report->exit_address = VF2_TEXTURE_TREE_DISPATCH_EXIT;
    report->iterations = tree_report.iterations;
    report->changed_values = tree_report.changed_values;
    report->bytes_written = 112u + tree_report.bytes_written;
    report->max_recursion_depth = tree_report.max_recursion_depth;
    report->recovered_instruction_count =
        UINT64_C(63) + tree_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        UINT64_C(1) + tree_report.recovered_procedure_calls;
    report->recovered_procedure_returns =
        tree_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_convert(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t saved_state = 0u;
    uint8_t frame_state = 0u;
    uint8_t frame_wait = 0u;
    uint32_t source = VF2_TEXTURE_CONVERT_SOURCE;
    uint32_t odd_address = VF2_TEXTURE_CONVERT_ODD;
    uint32_t even_address = VF2_TEXTURE_CONVERT_EVEN;
    uint32_t current = 0u;
    uint32_t previous = UINT32_MAX;
    uint32_t rows = cpu->registers[28];
    const uint32_t input_rows = rows;
    const uint32_t columns = cpu->registers[29];
    uint32_t repeat_value = 0u;
    uint32_t nibble_value = 0u;
    uint64_t instructions = UINT64_C(8);
    uint64_t changed_pixels = 0u;
    uint64_t pixels = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || rows == 0u || columns == 0u ||
        rows > UINT32_C(1024) || columns > UINT32_C(1024)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_CONVERT_STATE, &saved_state);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_STATE, &frame_state, sizeof(frame_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_FRAME_WAIT, &frame_wait, sizeof(frame_wait)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (saved_state != 0u || frame_state != 0u || frame_wait != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[24] = UINT32_MAX;
    status = vf2_model2a_read_u32(machine, source, &current);
    if (status != VF2_OK) {
        return status;
    }

    while (rows != 0u) {
        uint32_t destination = cpu->registers[27];
        uint32_t remaining = columns;
        instructions += UINT64_C(12);
        cpu->registers[16] = 0u;
        cpu->registers[27] += cpu->registers[26];

        while (remaining != 0u) {
            source -= UINT32_C(4);
            instructions += UINT64_C(3);
            if (current == previous) {
                status = write_u16(
                    machine, destination, (uint16_t)repeat_value
                );
                if (status == VF2_OK) {
                    const uint8_t nibble = (uint8_t)nibble_value;
                    status = vf2_model2a_write(
                        machine, odd_address, &nibble, sizeof(nibble)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(machine, source, &current);
                }
                instructions += UINT64_C(7);
            } else {
                uint32_t mixed = 0u;
                uint8_t nibble = 0u;
                repeat_value = current | (current >> 12u);
                status = write_u16(
                    machine, destination, (uint16_t)repeat_value
                );
                previous = current;
                mixed = current + (current >> 16u);
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(machine, source, &current);
                }
                cpu->registers[18] = mixed + (mixed >> 8u);
                cpu->registers[18] >>= 2u;
                nibble_value = cpu->registers[18] & UINT32_C(15);
                cpu->registers[19] = mixed >> 8u;
                nibble = (uint8_t)nibble_value;
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, odd_address, &nibble, sizeof(nibble)
                    );
                }
                instructions += UINT64_C(16);
                ++changed_pixels;
            }
            if (status != VF2_OK) {
                return status;
            }
            destination += UINT32_C(4);
            odd_address -= UINT32_C(2);
            --remaining;
            ++pixels;
        }
        {
            const uint32_t swap = odd_address;
            cpu->registers[16] = swap;
            odd_address = even_address;
            even_address = swap;
        }
        --rows;
    }

    instructions += UINT64_C(1);
    cpu->registers[24] = previous;
    cpu->executed_instructions += instructions;
    set_equal_condition(cpu);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT;
    report->entry_address = VF2_TEXTURE_CONVERT_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = pixels;
    report->rows = input_rows;
    report->changed_values = changed_pixels;
    report->bytes_written = (size_t)pixels * 3u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_header_decode(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    texture_bit_reader reader;
    uint32_t raw_width = 0u;
    uint32_t raw_height = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t area = 0u;
    uint32_t code_bits = 0u;
    uint32_t symbol_count = 0u;
    uint32_t table_a = 0u;
    uint32_t table_b = 0u;
    uint32_t nibble = 0u;
    uint32_t table_c = 0u;
    uint32_t child_state = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || cpu->registers[19] == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_HEADER_STATE, &child_state
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = child_state;
    if (child_state != 0u) {
        uint32_t restore_base = UINT32_C(0x00550084);
        uint32_t target = 0u;
        size_t index = 0u;

        /* The nonzero-state arm is the ROM's small context dispatcher. It
         * clears the child state, reloads g1..g14 and r3..r15 from the
         * saved register image, then branches through the saved g0. */
        status = vf2_model2a_write_u32(machine, VF2_TEXTURE_HEADER_STATE, 0u);
        for (index = 0u; status == VF2_OK && index < 14u; ++index) {
            status = vf2_model2a_read_u32(
                machine, restore_base + (uint32_t)index * UINT32_C(4),
                &cpu->registers[VF2_I960_G0_REGISTER + 1u + index]
            );
        }
        for (index = 0u; status == VF2_OK && index < 13u; ++index) {
            status = vf2_model2a_read_u32(
                machine, restore_base + UINT32_C(56) +
                    (uint32_t)index * UINT32_C(4),
                &cpu->registers[3u + index]
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, restore_base + UINT32_C(108), &target
            );
        }
        if (status != VF2_OK || target == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = target;
        cpu->ip = target;
        cpu->executed_instructions += UINT64_C(62);
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE;
        report->entry_address = VF2_TEXTURE_HEADER_DECODE_ENTRY;
        report->exit_address = cpu->ip;
        report->changed_values = UINT64_C(28);
        report->bytes_written = sizeof(uint32_t);
        report->recovered_instruction_count = UINT64_C(62);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = texture_bit_reader_initialize(
        machine, cpu->registers[19], &reader
    );
    if (status == VF2_OK) {
        status = texture_bit_reader_refill(machine, &reader);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_refill(machine, &reader);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 8u, &raw_width);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 8u, &raw_height);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 8u, &code_bits);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &symbol_count);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &table_a);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &table_b);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 4u, &nibble);
    }
    if (status == VF2_OK) {
        status = texture_bit_reader_take(machine, &reader, 16u, &table_c);
    }
    if (status != VF2_OK) {
        return status;
    }

    width = (raw_width + UINT32_C(1)) >> 1u;
    height = (raw_height + UINT32_C(1)) >> 1u;
    area = width * height;
    if (code_bits == 0u || code_bits > UINT32_C(31) ||
        symbol_count == 0u || symbol_count > VF2_TEXTURE_MAX_LOOP) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = write_u16(
        machine, VF2_TEXTURE_HEADER_OUTPUT, (uint16_t)width
    );
    if (status == VF2_OK) {
        status = write_u16(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(2),
            (uint16_t)height
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(4), area
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(8),
            (symbol_count << 1u) - UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(12), symbol_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(16), code_bits
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(20), table_a
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(24), table_b
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(28), nibble
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_HEADER_OUTPUT + UINT32_C(32), table_c
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = UINT32_C(0x0055c344);
    cpu->registers[4] = UINT32_C(0x0055c344);
    cpu->registers[5] = code_bits;
    cpu->registers[7] = (symbol_count << 1u) - UINT32_C(1);
    cpu->registers[12] =
        (UINT32_C(1) << code_bits) - UINT32_C(1);
    cpu->registers[13] = reader.accumulator;
    cpu->registers[14] = reader.available_bits;
    cpu->registers[15] = reader.next_address;
    cpu->registers[16] = reader.last_shifted_word;
    cpu->registers[18] = table_c;
    cpu->registers[19] = UINT32_C(0x00000100);
    cpu->registers[20] = UINT32_C(0x00000102);
    cpu->registers[21] = UINT32_C(0x00000122);
    cpu->registers[22] = UINT32_C(0x00000142);
    cpu->registers[27] = reader.next_word;
    set_signed_condition(cpu, 0, 1);
    cpu->ip = VF2_TEXTURE_HEADER_DECODE_EXIT;
    cpu->executed_instructions += UINT64_C(120);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE;
    report->entry_address = VF2_TEXTURE_HEADER_DECODE_ENTRY;
    report->exit_address = VF2_TEXTURE_HEADER_DECODE_EXIT;
    report->iterations = UINT64_C(8);
    report->changed_values = UINT64_C(10);
    report->bytes_written = 36u;
    report->recovered_instruction_count = UINT64_C(120);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_active_prepare_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t raw_count = 0u;
    uint16_t raw_x = 0u;
    uint16_t raw_y = 0u;
    uint32_t flags = 0u;
    uint32_t stream_word = 0u;
    uint32_t table_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, cpu->registers[5] + UINT32_C(0x10), &flags
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine, cpu->registers[5] + UINT32_C(2), &raw_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x14), &cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x18), &cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[7] = flags;
    cpu->registers[8] = (uint32_t)(int32_t)(int16_t)raw_count;
    if ((flags & (UINT32_C(1) << 3u)) != 0u ||
        (flags & (UINT32_C(1) << 4u)) != 0u ||
        (int32_t)cpu->registers[8] <= 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, VF2_TEXTURE_ACTIVE_FLAGS, flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[9], &stream_word);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[6] = 1u;
    cpu->registers[18] = stream_word;
    cpu->registers[24] = (stream_word ^ flags) & UINT32_C(1);
    table_index = (stream_word & UINT32_C(0xffff)) >> 1u;
    cpu->registers[16] = table_index;
    status = read_u16(
        machine,
        VF2_TEXTURE_COORD_TABLE + table_index * UINT32_C(4),
        &raw_x
    );
    if (status == VF2_OK) {
        status = read_u16(
            machine,
            VF2_TEXTURE_COORD_TABLE + table_index * UINT32_C(4) + UINT32_C(2),
            &raw_y
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[22] = (uint32_t)(int32_t)(int16_t)raw_x;
    cpu->registers[23] = (uint32_t)(int32_t)(int16_t)raw_y;
    cpu->registers[16] = stream_word >> 24u;
    cpu->registers[17] = (stream_word >> 16u) & UINT32_C(0xff);
    cpu->registers[22] += cpu->registers[16];
    cpu->registers[23] += cpu->registers[17];

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ACTIVE_PREPARE_TARGET,
        VF2_TEXTURE_ACTIVE_PREPARE_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(22);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL;
    report->entry_address = VF2_TEXTURE_ACTIVE_PREPARE_ENTRY;
    report->exit_address = VF2_TEXTURE_ACTIVE_PREPARE_TARGET;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(22);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_status_dispatch_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_i960_cpu candidate_cpu;
    size_t inactive_records = 0u;
    uint16_t active_count = 0u;
    uint16_t status_value = 0u;
    uint32_t runtime_flags = 0u;
    uint64_t instructions = UINT64_C(2);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Scan against a CPU candidate so an unsupported active-record variant
     * cannot leave the caller midway through the record table. */
    candidate_cpu = *cpu;
    candidate_cpu.registers[5] = VF2_TEXTURE_RECORD_START;
    candidate_cpu.registers[6] = VF2_TEXTURE_RECORD_END;

    for (;;) {
        status = read_u16(
            machine,
            candidate_cpu.registers[5] + UINT32_C(2),
            &active_count
        );
        if (status != VF2_OK) {
            return status;
        }
        candidate_cpu.registers[3] =
            (uint32_t)(int32_t)(int16_t)active_count;

        if (candidate_cpu.registers[3] != 0u) {
            instructions += UINT64_C(2);
            break;
        }

        candidate_cpu.registers[5] += VF2_ORCHESTRATOR_RECORD_STRIDE;
        ++inactive_records;
        if (candidate_cpu.registers[5] >= candidate_cpu.registers[6]) {
            if (inactive_records != VF2_ORCHESTRATOR_RECORD_COUNT ||
                candidate_cpu.registers[5] != VF2_TEXTURE_RECORD_END) {
                return VF2_ERROR_UNSUPPORTED;
            }

            candidate_cpu.ip = VF2_TEXTURE_FINAL_STATUS_ENTRY;
            candidate_cpu.executed_instructions += UINT64_C(43);
            *cpu = candidate_cpu;

            report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END;
            report->entry_address = VF2_TEXTURE_STATUS_DISPATCH_ENTRY;
            report->exit_address = VF2_TEXTURE_FINAL_STATUS_ENTRY;
            report->iterations = inactive_records;
            report->recovered_instruction_count = UINT64_C(43);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        instructions += UINT64_C(4);
    }

    status = read_u16(
        machine, candidate_cpu.registers[5], &status_value
    );
    if (status == VF2_OK) {
        candidate_cpu.registers[16] = status_value;
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    candidate_cpu.registers[15] = runtime_flags;

    instructions += UINT64_C(3);
    if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) {
        candidate_cpu.ip = VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY;
        candidate_cpu.executed_instructions += instructions;
        *cpu = candidate_cpu;

        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL;
        report->entry_address = VF2_TEXTURE_STATUS_DISPATCH_ENTRY;
        report->exit_address = VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY;
        report->iterations = inactive_records + 1u;
        report->recovered_instruction_count = instructions;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_TEXTURE_STATUS_DISPATCH_TARGET,
        VF2_TEXTURE_STATUS_DISPATCH_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    ++instructions;
    candidate_cpu.executed_instructions += instructions;
    *cpu = candidate_cpu;

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL;
    report->entry_address = VF2_TEXTURE_STATUS_DISPATCH_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = inactive_records + 1u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_record_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint16_t raw_count = 0u;
    uint32_t count = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || cpu->registers[6] != 1u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = read_u16(machine, cpu->registers[5] + UINT32_C(2), &raw_count);
    if (status != VF2_OK) {
        return status;
    }
    count = (uint32_t)(int32_t)(int16_t)raw_count;
    cpu->registers[VF2_I960_G0_REGISTER] = count;
    if ((int32_t)count <= 0 || cpu->registers[8] != count) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[6] = 0u;
    cpu->registers[8] -= UINT32_C(1);
    cpu->registers[9] += UINT32_C(4);
    cpu->registers[10] += UINT32_C(4);

    status = write_u16(
        machine,
        cpu->registers[5] + UINT32_C(2),
        (uint16_t)cpu->registers[8]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[5] + UINT32_C(0x14),
            cpu->registers[9]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            cpu->registers[5] + UINT32_C(0x18),
            cpu->registers[10]
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    set_equal_condition(cpu);
    cpu->ip = VF2_TEXTURE_RECORD_ADVANCE_EXIT;
    cpu->executed_instructions += UINT64_C(12);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE;
    report->entry_address = VF2_TEXTURE_RECORD_ADVANCE_ENTRY;
    report->exit_address = VF2_TEXTURE_RECORD_ADVANCE_EXIT;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(3);
    report->bytes_written = 10u;
    report->recovered_instruction_count = UINT64_C(12);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_final_status_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t counter0 = 0u;
    uint32_t counter1 = 0u;
    uint32_t counter2 = 0u;
    uint32_t runtime_flags = 0u;
    uint64_t instructions = UINT64_C(4);
    uint64_t changed_values = UINT64_C(1);
    size_t bytes_written = 2u;
    size_t counters_read = 1u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[3] = 0u;
    status = write_u16(machine, VF2_TEXTURE_STATUS_WORD, 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER0, &counter0
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[14] = counter0;
    if (counter0 == 0u) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER1, &counter1
        );
        instructions += UINT64_C(2);
        ++counters_read;
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[14] = counter1;
        if (counter1 == 0u) {
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER2, &counter2
            );
            instructions += UINT64_C(2);
            ++counters_read;
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[14] = counter2;
            if (counter2 == 0u) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00550000), 0u
                );
                instructions += UINT64_C(2);
                ++changed_values;
                bytes_written += 4u;
                if (status != VF2_OK) {
                    return status;
                }
            }
        }
    }

    status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_RUNTIME_FLAGS, &runtime_flags
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = runtime_flags;
    instructions += UINT64_C(2);

    if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK) {
            return status;
        }
        ++instructions;
        report->recovered_procedure_returns = UINT64_C(1);
    } else {
        status = vf2_i960_cpu_enter_procedure(
            cpu,
            VF2_TEXTURE_FINAL_STATUS_TARGET,
            VF2_TEXTURE_FINAL_STATUS_RETURN
        );
        if (status != VF2_OK) {
            return status;
        }
        ++instructions;
        report->recovered_procedure_calls = UINT64_C(1);
    }
    cpu->executed_instructions += instructions;

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL;
    report->entry_address = VF2_TEXTURE_FINAL_STATUS_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = counters_read;
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = instructions;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_body_return(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->executed_instructions += UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN;
    report->entry_address = VF2_TEXTURE_BODY_RETURN_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_post_body_call(
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_POST_BODY_CALL_TARGET,
        VF2_TEXTURE_POST_BODY_CALL_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(1);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL;
    report->entry_address = VF2_TEXTURE_POST_BODY_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_counter_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t counter0 = 0u;
    uint32_t counter1 = 0u;
    uint32_t counter2 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER0;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER0, &counter0);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter0 - UINT32_C(1);
    if (counter0 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER1;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER1, &counter1);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter1 - UINT32_C(1);
    if (counter1 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = VF2_TEXTURE_COUNTER2;
    status = vf2_model2a_read_u32(machine, VF2_TEXTURE_COUNTER2, &counter2);
    if (status != VF2_OK) {
        return status;
    }
    if (counter2 == 0u) {
        cpu->registers[4] = UINT32_MAX;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        cpu->ip = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
        cpu->executed_instructions += UINT64_C(12);

        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE;
        report->entry_address = VF2_TEXTURE_COUNTER_UPDATE_ENTRY;
        report->exit_address = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
        report->iterations = UINT64_C(3);
        report->recovered_instruction_count = UINT64_C(12);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (counter2 == UINT32_C(1)) {
        uint32_t argument0 = 0u;
        uint32_t argument1 = 0u;
        uint32_t argument2 = 0u;
        uint16_t status_word = 0u;
        uint32_t helper_stack = 0u;

        cpu->registers[4] = 0u;
        set_equal_condition(cpu);
        status = vf2_model2a_write_u32(
            machine, VF2_TEXTURE_COUNTER2, 0u
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER2 + UINT32_C(4), &argument0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER2 + UINT32_C(8), &argument1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER2 + UINT32_C(12), &argument2
            );
        }
        if (status == VF2_OK) {
            status = read_u16(machine, VF2_TEXTURE_STATUS_WORD, &status_word);
        }
        if (status != VF2_OK || argument0 > UINT32_C(0x56) ||
            status_word >= UINT16_C(1)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        cpu->registers[VF2_I960_G0_REGISTER] = argument0;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = argument2;
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x0004b44c), VF2_TEXTURE_COUNTER_UPDATE_EXIT
        );
        if (status != VF2_OK) {
            return status;
        }
        helper_stack = cpu->registers[1];
        status = vf2_model2a_write_u32(machine, helper_stack, argument1);
        if (status != VF2_OK) {
            return status;
        }

        cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(1);
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x00550288);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00550000), UINT32_C(1)
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x00550288), (uint16_t)argument0
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x0055028a), UINT16_MAX
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00550298), 0u
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x005502a4), UINT16_C(1)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x0055000c), 0u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00550080), 0u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005500f4), 0u
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = 0u;

        cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x005502b8);
        status = write_u16(
            machine, UINT32_C(0x005502b8), UINT16_C(1)
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x005502bc), (uint16_t)argument1
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != VF2_TEXTURE_COUNTER_UPDATE_EXIT) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->executed_instructions += UINT64_C(55);

        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE;
        report->entry_address = VF2_TEXTURE_COUNTER_UPDATE_ENTRY;
        report->exit_address = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
        report->iterations = UINT64_C(3);
        report->changed_values = UINT64_C(12);
        report->bytes_written = 38u;
        report->recovered_instruction_count = UINT64_C(55);
        report->recovered_procedure_calls = UINT64_C(3);
        report->recovered_procedure_returns = UINT64_C(3);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    cpu->registers[4] = counter2 - UINT32_C(1);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
    status = vf2_model2a_write_u32(
        machine, VF2_TEXTURE_COUNTER2, cpu->registers[4]
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
    cpu->executed_instructions += UINT64_C(14);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE;
    report->entry_address = VF2_TEXTURE_COUNTER_UPDATE_ENTRY;
    report->exit_address = VF2_TEXTURE_COUNTER_UPDATE_EXIT;
    report->iterations = UINT64_C(3);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 4u;
    report->recovered_instruction_count = UINT64_C(14);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_orchestrator_epilogue(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t stack_end = cpu->registers[1];
    uint32_t register_index = 0u;
    uint32_t value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || stack_end < UINT32_C(112)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (register_index = 30u; register_index >= 3u;
         --register_index) {
        status = vf2_model2a_read_u32(
            machine,
            stack_end - (UINT32_C(31) - register_index) * UINT32_C(4),
            &value
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[register_index] = value;
        if (register_index == 3u) {
            break;
        }
    }
    cpu->registers[1] = stack_end - UINT32_C(112);
    cpu->executed_instructions += UINT64_C(21);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind =
        VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE;
    report->entry_address = VF2_TEXTURE_ORCHESTRATOR_EPILOGUE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(28);
    report->changed_values = UINT64_C(28);
    report->recovered_instruction_count = UINT64_C(21);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_orchestrator_save_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t stack_start = cpu->registers[1];
    uint32_t register_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (register_index = 3u; register_index <= 30u;
         ++register_index) {
        status = vf2_model2a_write_u32(
            machine,
            stack_start + (register_index - 3u) * UINT32_C(4),
            cpu->registers[register_index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[1] = stack_start + UINT32_C(112);
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ORCHESTRATOR_BODY_ENTRY,
        VF2_TEXTURE_ORCHESTRATOR_BODY_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(21);

    report->kind =
        VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL;
    report->entry_address = VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(28);
    report->bytes_written = 112u;
    report->recovered_instruction_count = UINT64_C(21);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_frame_gate_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint8_t frame_state = 0u;
    const uint8_t latch_value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(
        machine,
        VF2_FRAME_STATE,
        &frame_state,
        sizeof(frame_state)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[16] = 0u;
    status = vf2_model2a_write(
        machine,
        VF2_TEXTURE_FRAME_GATE_LATCH,
        &latch_value,
        sizeof(latch_value)
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_DEFAULT_LIMITS_ENTRY,
        VF2_TEXTURE_DEFAULT_LIMITS_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(5);

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL;
    report->entry_address = VF2_TEXTURE_FRAME_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 1u;
    report->recovered_instruction_count = UINT64_C(5);
    report->recovered_procedure_calls = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_default_limits(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_orchestrator_limits_report limits_report;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&limits_report, 0, sizeof(limits_report));
    status = vf2_orchestrator_apply_default_limits(
        machine, &limits_report
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions +=
        limits_report.interpreted_instruction_equivalent;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS;
    report->entry_address = VF2_TEXTURE_DEFAULT_LIMITS_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written = limits_report.bytes_written;
    report->recovered_instruction_count =
        limits_report.interpreted_instruction_equivalent;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_orchestrator_gate(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_orchestrator_gate_report gate_report;
    vf2_status status = VF2_OK;

    memset(&gate_report, 0, sizeof(gate_report));
    if (cpu->ip == VF2_ORCHESTRATOR_LOOP_GATE_ENTRY) {
        status = vf2_orchestrator_apply_zero_loop_gate(
            machine, cpu, &gate_report
        );
    } else {
        status = vf2_orchestrator_enter_zero_child_gate(
            machine, cpu, &gate_report
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    switch (gate_report.kind) {
    case VF2_ORCHESTRATOR_GATE_CHILD_A:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A;
        break;
    case VF2_ORCHESTRATOR_GATE_CHILD_B:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B;
        break;
    case VF2_ORCHESTRATOR_GATE_LOOP_TAIL:
        report->kind = VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE;
        break;
    case VF2_ORCHESTRATOR_GATE_NONE:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    report->entry_address = gate_report.entry_address;
    report->exit_address = gate_report.exit_address;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count =
        gate_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        gate_report.recovered_procedure_calls;
    report->cpu_poststate_applied = gate_report.cpu_poststate_applied;
    return VF2_OK;
}

vf2_status execute_texture_status_tail(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report text_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t selector = 0u;
    vf2_status status = vf2_model2a_read(
        machine, UINT32_C(0x0050002b), &selector, 1u
    );
    memset(&text_report, 0, sizeof(text_report));
    if (status != VF2_OK || selector == UINT8_C(12) || selector == UINT8_C(13)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = selector;
    cpu->registers[14] = UINT32_C(13);
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010000e2);
    cpu->registers[14] = UINT32_C(0x0004d2ac);
    cpu->ip = VF2_INLINE_TEXT_THUNK_ENTRY;
    status = execute_inline_text_thunk(machine, cpu, &text_report);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0004d2bc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(8));
    if (status != VF2_OK) return status;
    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL;
    report->entry_address = VF2_TEXTURE_STATUS_TAIL_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written = text_report.bytes_written;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
