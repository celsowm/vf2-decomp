#include "texture_bridge_internal.h"

static void geometry_gate_set_compare(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t condition_bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        condition_bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        condition_bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        condition_bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
}

vf2_status execute_frame_geometry_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint8_t frame_state = 0u;
    uint8_t alt_byte = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &flags);
    if (status != VF2_OK) {
        return status;
    }

    if ((flags & ((UINT32_C(1) << 26u) | UINT32_C(4))) == 0u) {
        /* Clear-flags path: ld, bbs(26,fail), bbs(2,fail), b 0x0000a800, ret.
         * The second BBS leaves the architectural condition code unordered. */
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        if (status != VF2_OK) {
            return status;
        }
        geometry_gate_set_compare(cpu, VF2_I960_COMPARE_NONE);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    /* Busy path observed on the third scheduler sweep via 0x0000a010. The
     * reference i960 reads 0x0050002a after the bit-26/bit-2 branch. The
     * observed live values are 1 (sweep #1) and 17 (sweep #2); sweep #3 and
     * #4 fall through the gate with flags=0. Only the value 17 forwards to
     * the alt-byte check at 0x0000a778; any other value writes 16 into the
     * state byte and returns through 0x0000a800. */
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &frame_state, sizeof(frame_state)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (frame_state != UINT8_C(17)) {
        const uint8_t compared_frame_state = frame_state;

        /* ld, bbs(26,taken), ldob, cmpobe(fail), mov 16, stib, b 0x0000a800,
         * ret = 8 instructions. CMPobe leaves the result of 17 vs state in CC. */
        frame_state = UINT8_C(16);
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &frame_state, sizeof(frame_state)
        );
        if (status != VF2_OK) {
            return status;
        }
        status = finish_recovered_procedure(machine, cpu, UINT64_C(8));
        if (status != VF2_OK) {
            return status;
        }
        geometry_gate_set_compare(
            cpu,
            UINT8_C(17) < compared_frame_state
                ? VF2_I960_COMPARE_LESS
                : VF2_I960_COMPARE_GREATER
        );
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(frame_state);
        report->recovered_instruction_count = UINT64_C(8);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    /* frame_state == 17 forwards to 0x0000a778 which reads 0x005000a6 and
     * returns through 0x0000a800 when the alt byte is non-zero. The observed
     * third-sweep live values for 0x005000a6 are all 255, so the deep reset
     * call sequence (calls to 0x00008ef0 and 0x0006116c followed by an
     * unconditional branch to the reset entry 0x000000b0) never executes on
     * the observed path and remains unsupported. */
    status = vf2_model2a_read(
        machine, UINT32_C(0x005000a6), &alt_byte, sizeof(alt_byte)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (alt_byte != 0u) {
        /* ld, bbs(26,*), [bbs(2,taken)], ldob, cmpobe(17,taken), ldob,
         * cmpobne(0,taken), ret. Bit26 set skips the second BBS
         * (7 instructions, corridor-measured); bit26 clear with bit2
         * set executes both (8, TEST-menu measured). The trailing
         * CMPobne decides CC either way (0 < alt_byte). */
        const uint64_t taken_count =
            (flags & (UINT32_C(1) << 26u)) != 0u ? UINT64_C(7)
                                                 : UINT64_C(8);
        status = finish_recovered_procedure(machine, cpu, taken_count);
        if (status != VF2_OK) {
            return status;
        }
        geometry_gate_set_compare(cpu, VF2_I960_COMPARE_LESS);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = taken_count;
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    /* alt_byte == 0 forwards the deep reset sequence at 0x0000a784.
     * Measured via synthetic vf2probe at 0x0000a748 with flags
     * 0x04000000, frame_state 17, alt 0: 12566 instructions,
     * 2 calls (0x08ef0, 0x6116c), 2 returns, branch to 0x000000b0.
     * The original i960 zeroes 0x00500700/0x00500704/0x00500708/
     * 0x0050070c, writes 0 to 0x00e80004, fills 48×64 cells of
     * 0x20 at 0x01000000 (stride 0x80), writes the 16-byte magic
     * 0x52455320/0x4e4c2053/0x4e204544/0x20514555 to 0x0059cfe0,
     * and branches to reset. Previously unsupported. */
    {
        uint32_t zero = 0u;
        size_t row, col;
        // zero 0x00500700, 0x00500704, 0x00500708, 0x0050070c
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500700), zero);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500704), zero);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500708), zero);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050070c), zero);
        // MMIO
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00e80004), zero);
        // tile fill: 48 rows, 64 cells of 0x0020
        for (row = 0; status == VF2_OK && row < 48; ++row) {
            uint32_t base = UINT32_C(0x01000000) + (uint32_t)row * UINT32_C(0x80);
            for (col = 0; col < 64; ++col) {
                uint8_t bytes[2] = {0x20, 0x00};
                status = vf2_model2a_write(machine, base + (uint32_t)col * 2u, bytes, 2u);
                if (status != VF2_OK) break;
            }
        }
        // magic
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0059cfe0), UINT32_C(0x52455320));
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0059cfe4), UINT32_C(0x4e4c2053));
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0059cfe8), UINT32_C(0x4e204544));
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0059cfec), UINT32_C(0x20514555));
        if (status != VF2_OK) return status;
        // Emulate the two nested calls and branch: set IP to reset entry, keep frame depth (parent+gate)
        // The reference i960 executes 12566 instructions from gate entry to 0x000000b0
        // including the two calls (2) and their returns (2). We mirror that.
        cpu->ip = UINT32_C(0x000000b0);
        cpu->executed_instructions += UINT64_C(12566);
        cpu->procedure_calls += UINT64_C(2);
        cpu->procedure_returns += UINT64_C(2);
        // The original branch leaves CC unordered (NONE) after the sequence
        geometry_gate_set_compare(cpu, VF2_I960_COMPARE_NONE);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE;
        report->entry_address = VF2_FRAME_GEOMETRY_GATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(6);
        report->bytes_written = 16u + 4u + 6144u + 16u; // 6180
        report->recovered_instruction_count = UINT64_C(12566);
        report->recovered_procedure_calls = UINT64_C(2);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
}

vf2_status execute_geometry_frame_commit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t geometry_base = cpu->registers[26];
    const uint32_t mask = UINT32_C(0x0007fffc);
    uint32_t previous_command = 0u;
    uint32_t read_pointer = 0u;
    uint32_t max_distance = 0u;
    int32_t distance = 0;
    uint8_t ring_index = 0u;
    uint32_t next_command = 0u;
    uint64_t instructions = UINT64_C(20);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || geometry_base != VF2_GEOMETRY_BASE) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00501004), &previous_command
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + UINT32_C(0x000000f0), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + UINT32_C(0x00003008), previous_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, geometry_base + UINT32_C(0x00002008), &read_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00501008), &max_distance
        );
    }
    distance = (int32_t)((read_pointer & mask) - (previous_command & mask));
    set_signed_condition(cpu, distance, (int32_t)max_distance);
    if (status == VF2_OK && distance > (int32_t)max_distance) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501008), (uint32_t)distance
        );
        ++instructions;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050100c), &ring_index, sizeof(ring_index)
        );
    }
    ring_index = (uint8_t)((ring_index + 1u) % 4u);
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050100c), &ring_index, sizeof(ring_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00007a00) + (uint32_t)ring_index * UINT32_C(4),
            &next_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501004), next_command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + UINT32_C(0x00001008), next_command
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT;
    report->entry_address = VF2_GEOMETRY_FRAME_COMMIT_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written = 17u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_geometry_command_setup(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t geometry_base = cpu->registers[26];
    const uint32_t destination_offset = cpu->registers[28];
    uint16_t signed_source = 0u;
    uint8_t command_class = 0u;
    uint32_t command = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || geometry_base != VF2_GEOMETRY_BASE) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, geometry_base + UINT32_C(0x80), 0u
    );
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x005010de), &signed_source);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005010dc), &command_class,
            sizeof(command_class)
        );
    }
    command = ((uint32_t)(int32_t)(int16_t)signed_source << 8u) &
              UINT32_C(0x807fffff);
    command |= (uint32_t)command_class << 23u;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005010e0), command
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, geometry_base + destination_offset, command
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(12));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP;
    report->entry_address = VF2_GEOMETRY_COMMAND_SETUP_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->bytes_written = 12u;
    report->recovered_instruction_count = UINT64_C(12);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_scratch_clear(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < UINT32_C(43); ++index) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501800) + index * UINT32_C(4), 0u
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(176));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR;
    report->entry_address = VF2_FRAME_SCRATCH_CLEAR_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(43);
    report->bytes_written = 43u * sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(176);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
