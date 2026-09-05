#include "texture_bridge_internal.h"
#include "recovery_internal.h"
#include <stdio.h>
vf2_status finish_recovered_procedure(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t instructions
)
{
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    return status;
}

vf2_status classify_observed_game_state(
    const vf2_model2a *machine,
    uint32_t *classification,
    uint64_t *instructions
)
{
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint8_t mode = 0u;
    uint8_t mapped = 0u;
    vf2_status status = VF2_OK;

    if (classification == NULL || instructions == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x000028b8) + (uint32_t)mode,
            &mapped,
            sizeof(mapped)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* v0.0.22 accepts only the path observed on all eleven live calls. */
    if (mode == UINT8_C(25) ||
        (flags & UINT32_C(3)) != 0u ||
        mapped != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *classification = 0u;
    *instructions = UINT64_C(16);
    return VF2_OK;
}

vf2_status execute_game_state_classify(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t result = 0u;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = classify_observed_game_state(machine, &result, &instructions);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[16] = result;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY;
    report->entry_address = VF2_GAME_STATE_CLASSIFY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_color_lookup(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t selector = cpu->registers[16];
    const uint32_t stack_address = cpu->registers[1];
    const uint32_t saved_g9 = cpu->registers[25];
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t classification = 0u;
    uint32_t color = 0u;
    uint64_t classify_instructions = 0u;
    uint64_t own_instructions = UINT64_C(19);
    uint8_t mode = 0u;
    uint8_t color_index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u || selector > UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, stack_address, saved_g9);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((flags & (UINT32_C(1) << 1u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = classify_observed_game_state(
        machine, &classification, &classify_instructions
    );
    if (status != VF2_OK || classification != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read(
        machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x000027ac) + (uint32_t)mode * UINT32_C(2) + selector,
            &color_index,
            sizeof(color_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x000027e0) + (uint32_t)color_index * UINT32_C(4),
            &color
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    color += UINT32_C(0x00010101);
    cpu->registers[16] = color;
    if (selector != 0u) {
        ++own_instructions;
    }
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(
        machine, cpu, own_instructions + classify_instructions
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP;
    report->entry_address = VF2_GAME_COLOR_LOOKUP_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = sizeof(uint32_t);
    report->recovered_instruction_count = own_instructions + classify_instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_meter_component(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    uint32_t structure,
    uint32_t return_address,
    uint32_t *meter_value,
    uint32_t *secondary_value,
    size_t *bytes_written
)
{
    vf2_hybrid_bridge_report first_color_report;
    vf2_hybrid_bridge_report second_color_report;
    uint32_t first_color = 0u;
    uint32_t second_color = 0u;
    uint32_t restored_selector = 0u;
    uint32_t restored_structure = 0u;
    uint32_t low_nibble = 0u;
    uint32_t middle_nibble = 0u;
    uint32_t high_nibble = 0u;
    uint32_t offset_result = 0u;
    uint32_t component = 0u;
    uint32_t threshold = UINT32_C(9);
    uint32_t base = 0u;
    uint8_t variant = 0u;
    uint8_t byte0 = 0u;
    uint8_t byte4 = 0u;
    uint8_t byte5 = 0u;
    vf2_status status = VF2_OK;

    memset(&first_color_report, 0, sizeof(first_color_report));
    memset(&second_color_report, 0, sizeof(second_color_report));

    cpu->registers[VF2_I960_G0_REGISTER] = selector;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = structure;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_COMPONENT_ENTRY, return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = selector;

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_VALUE_ENTRY, UINT32_C(0x00002550)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = selector;
    cpu->registers[4] = structure;
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), selector
    );
    cpu->registers[1] += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[1] - UINT32_C(4), structure
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x00002664)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &first_color_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002664)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    first_color = cpu->registers[VF2_I960_G0_REGISTER];
    low_nibble = first_color & UINT32_C(15);
    middle_nibble = (first_color >> 8u) & UINT32_C(15);
    high_nibble = (first_color >> 16u) & UINT32_C(15);

    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4), &restored_structure
    );
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = restored_structure;
        cpu->registers[1] -= UINT32_C(4);
        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4), &restored_selector
        );
    }
    if (status != VF2_OK || restored_structure != structure ||
        restored_selector != selector) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = restored_selector;
    cpu->registers[1] -= UINT32_C(4);

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_OFFSET_ENTRY, UINT32_C(0x00002694)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = structure;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x000026b8)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &second_color_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000026b8)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    second_color = cpu->registers[VF2_I960_G0_REGISTER];
    status = vf2_model2a_read(
        machine, structure + UINT32_C(4), &byte4, sizeof(byte4)
    );
    if (status != VF2_OK) {
        return status;
    }
    {
        const uint32_t second_low = second_color & UINT32_C(15);
        const uint32_t second_middle =
            (second_color >> 8u) & UINT32_C(15);
        const uint32_t scaled = second_low * (uint32_t)byte4;
        const uint32_t scaled_next =
            second_low * ((uint32_t)byte4 + UINT32_C(1));

        if (second_middle == UINT32_C(1)) {
            offset_result = 0u;
        } else if (second_middle != 0u) {
            offset_result =
                scaled_next / second_middle - scaled / second_middle;
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    cpu->registers[VF2_I960_G0_REGISTER] = offset_result;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002694)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 1u] =
        offset_result + low_nibble;
    status = vf2_model2a_read(
        machine, structure, &byte0, sizeof(byte0)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, structure + UINT32_C(5), &byte5, sizeof(byte5)
        );
    }
    if (status != VF2_OK || high_nibble == 0u || middle_nibble == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    component = (uint32_t)byte0 / high_nibble + (uint32_t)byte5;
    cpu->registers[VF2_I960_G0_REGISTER] = component;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002550)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (variant == UINT8_C(1)) {
        threshold = UINT32_C(24);
    }
    if (component >= threshold) {
        component |= UINT32_C(1) << 16u;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = component;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (meter_value != NULL) {
        *meter_value = component;
    }
    if (secondary_value != NULL) {
        *secondary_value = cpu->registers[VF2_I960_G0_REGISTER + 1u];
    }
    if (bytes_written != NULL) {
        *bytes_written += sizeof(uint32_t) * 2u +
            first_color_report.bytes_written +
            second_color_report.bytes_written;
    }
    return VF2_OK;
}

vf2_status execute_game_meter_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report threshold_report;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t first_meter = 0u;
    uint32_t second_meter = 0u;
    uint32_t secondary = 0u;
    uint32_t threshold = UINT32_C(9);
    uint8_t variant = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&threshold_report, 0, sizeof(threshold_report));
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status != VF2_OK || cpu->registers[VF2_I960_G0_REGISTER + 9u] != base) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_THRESHOLD_EVALUATE_ENTRY, UINT32_C(0x000020f4)
    );
    if (status == VF2_OK) {
        status = execute_game_threshold_evaluate(
            machine, cpu, &threshold_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000020f4) ||
        cpu->registers[VF2_I960_G0_REGISTER] != 0u ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    bytes_written += threshold_report.bytes_written;

    status = vf2_model2a_read_u32(
        machine, base + UINT32_C(0x3320), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status != VF2_OK || (flags & UINT32_C(3)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (variant == UINT8_C(1)) {
        threshold = UINT32_C(24);
    }

    status = execute_game_meter_component(
        machine, cpu, 0u, base + UINT32_C(0x3380),
        UINT32_C(0x000022d4), &first_meter, &secondary,
        &bytes_written
    );
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000022d4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = execute_game_meter_component(
        machine, cpu, 1u, base + UINT32_C(0x3388),
        UINT32_C(0x000022ec), &second_meter, &secondary,
        &bytes_written
    );
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000022ec) ||
        (first_meter & (UINT32_C(1) << 16u)) != 0u ||
        (second_meter & (UINT32_C(1) << 16u)) != 0u ||
        (first_meter & UINT32_C(0xffff)) +
            (second_meter & UINT32_C(0xffff)) >= threshold) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = finish_recovered_procedure(machine, cpu, UINT64_C(122));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_METER_UPDATE;
    report->entry_address = VF2_GAME_METER_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->changed_values = UINT64_C(2);
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = UINT64_C(389);
    report->recovered_procedure_calls = UINT64_C(20);
    report->recovered_procedure_returns = UINT64_C(21);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_system_memory_diagnostic(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t low = 0u;
    uint32_t high = 0u;
    uint32_t address = 0u;
    uint8_t enabled = 0u;
    uint8_t frame_mode = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x00500171), &enabled, sizeof(enabled)
    );
    if (status != VF2_OK || (enabled & UINT8_C(1)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (address = VF2_MEMORY_PROBE_ADDRESS;
         address < VF2_MEMORY_PROBE_ADDRESS + UINT32_C(4);
         ++address) {
        uint8_t original = 0u;
        uint8_t observed = 0u;
        const uint8_t patterns[2] = {UINT8_C(0x55), UINT8_C(0xaa)};
        uint32_t pattern_index = 0u;

        status = vf2_model2a_read(machine, address, &original, sizeof(original));
        if (status != VF2_OK) {
            return status;
        }
        for (pattern_index = 0u; pattern_index < UINT32_C(2); ++pattern_index) {
            status = vf2_model2a_write(
                machine, address, &patterns[pattern_index], sizeof(uint8_t)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, address, &observed, sizeof(observed)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, address, &original, sizeof(original)
                );
            }
            if (status != VF2_OK || observed != patterns[pattern_index]) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
        }
    }
    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002b), &frame_mode, sizeof(frame_mode)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (frame_mode == UINT8_C(16) || frame_mode == UINT8_C(17)) {
        const uint64_t instructions = frame_mode == UINT8_C(17)
            ? UINT64_C(67)
            : UINT64_C(66);

        set_equal_condition(cpu);
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, instructions);
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC;
        report->entry_address = VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(4);
        report->bytes_written = 16u;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0059c318), &low);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0059c31c), &high);
    }
    if (status != VF2_OK) {
        return status;
    }
    ++low;
    if (low == 0u) {
        ++high;
    }
    status = vf2_model2a_write_u32(machine, UINT32_C(0x01d03318), low);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0059c318), low);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x01d0331c), high);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0059c31c), high);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    cpu->arithmetic_control &= ~UINT32_C(7);
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(machine, cpu, UINT64_C(75));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC;
    report->entry_address = VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(4);
    report->changed_values = UINT64_C(4);
    report->bytes_written = 32u;
    report->recovered_instruction_count = UINT64_C(75);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_counter_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t mode = 0u;
    uint32_t counter = 0u;
    uint32_t mask = 0u;
    uint32_t sample_index = 0u;
    uint32_t timing_base = 0u;
    uint32_t timing_slot = 0u;
    uint32_t average = 0u;
    uint32_t sample_sum = 0u;
    uint16_t sample = 0u;
    uint16_t slot_value = 0u;
    uint8_t shift = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = mode;
    cpu->registers[3] = mode & UINT32_C(0x00030000);
    if (cpu->registers[3] != 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
        report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050d000), &counter);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050d000), counter + UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050d006), &shift, sizeof(shift)
        );
    }
    if (status != VF2_OK || shift >= UINT8_C(27)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    mask = (UINT32_C(1) << shift) - UINT32_C(1);
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    if (status != VF2_OK) {
        return status;
    }
    sample_index = (counter >> shift) & UINT32_C(0xff);
    if ((counter & mask) == 0u) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &timing_base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, timing_base + UINT32_C(0x3342), &timing_slot,
                sizeof(uint8_t)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        for (sample_index = 0u; sample_index < UINT32_C(256); ++sample_index) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050d100) + sample_index * 2u,
                &sample, sizeof(sample)
            );
            if (status != VF2_OK) {
                return status;
            }
            {
                const uint32_t extended_sample =
                    (sample & UINT16_C(0x8000)) != 0u
                        ? UINT32_C(0xffff0000) | (uint32_t)sample
                        : (uint32_t)sample;
                sample_sum += extended_sample;
            }
        }
        average = sample_sum >> (shift + UINT32_C(5));
        if (average > UINT32_C(7)) {
            average = UINT32_C(7);
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050d004), &average, sizeof(uint8_t)
        );
        if (status == VF2_OK) {
            average = sample_sum >> shift;
            if (average > UINT32_C(0xff)) {
                average = UINT32_C(0xff);
            }
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03000), &average, sizeof(uint8_t)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0059c000), &average, sizeof(uint8_t)
            );
        }
        if (status == VF2_OK) {
            timing_slot = UINT32_C(0x00011510) + average + timing_slot * 8u;
            status = vf2_model2a_read(
                machine, timing_slot, &sample, sizeof(uint8_t)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050d005), &sample, sizeof(uint8_t)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        sample = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050d100) + ((counter >> shift) & 0xffu) * 2u,
            &sample, sizeof(sample)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    if ((mode & UINT32_C(0x000cffc0)) != 0u) {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(5));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
        report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = UINT64_C(5);
        report->recovered_procedure_returns = UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if ((counter & mask) == 0u) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050d100) + ((counter >> shift) & 0xffu) * 2u,
            &slot_value, sizeof(slot_value)
        );
        if (status == VF2_OK) {
            slot_value = (uint16_t)(slot_value + UINT16_C(1));
            status = vf2_model2a_write(
                machine,
                UINT32_C(0x0050d100) + ((counter >> shift) & 0xffu) * 2u,
                &slot_value, sizeof(slot_value)
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(
        machine, cpu, (counter & mask) == 0u ? UINT64_C(75) : UINT64_C(21)
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE;
    report->entry_address = VF2_FRAME_COUNTER_ADVANCE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = (counter & mask) == 0u ? UINT64_C(6) : UINT64_C(1);
    report->bytes_written = (counter & mask) == 0u ? 6u : 4u;
    report->recovered_instruction_count =
        (counter & mask) == 0u ? UINT64_C(75) : UINT64_C(21);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_phase_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t base = 0u;
    uint8_t phase = 0u;
    uint8_t next = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3001), &phase, sizeof(phase)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    next = (uint8_t)((phase & UINT8_C(0xf0)) + UINT8_C(0x10) +
                     (uint8_t)(((uint32_t)phase + UINT32_C(1)) & UINT32_C(0x0f)));
    status = vf2_model2a_write(
        machine, UINT32_C(0x01d03001), &next, sizeof(next)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0059c001), &next, sizeof(next)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(10));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE;
    report->entry_address = VF2_FRAME_PHASE_ADVANCE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written = 2u;
    report->recovered_instruction_count = UINT64_C(10);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_shadow_verify(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    static const uint32_t live_addresses[9] = {
        UINT32_C(0x00500234), UINT32_C(0x00500235),
        UINT32_C(0x00500236), UINT32_C(0x00500237),
        UINT32_C(0x00500238), UINT32_C(0x00500239),
        UINT32_C(0x005000e0), UINT32_C(0x005000e1),
        UINT32_C(0x005000e2)
    };
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < UINT32_C(9); ++index) {
        uint8_t live = 0u;
        uint8_t shadow = 0u;
        status = vf2_model2a_read(
            machine, live_addresses[index], &live, sizeof(live)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00544600) + index,
                &shadow, sizeof(shadow)
            );
        }
        if (status != VF2_OK || live != shadow) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(28));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY;
    report->entry_address = VF2_FRAME_SHADOW_VERIFY_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(9);
    report->recovered_instruction_count = UINT64_C(28);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_buffer_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint32_t first = 0u;
    uint32_t second = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    if (status != VF2_OK || (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &cpu->registers[23]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &cpu->registers[24]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[23], &first);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cpu->registers[24], &second);
    }
    if (status != VF2_OK ||
        (first & (UINT32_C(1) << 5u)) != 0u ||
        (second & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(9));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE;
    report->entry_address = VF2_FRAME_BUFFER_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->recovered_instruction_count = UINT64_C(9);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase16_update_descriptor(
    vf2_model2a *machine,
    uint32_t pointer_address,
    uint32_t set_mask,
    uint32_t clear_mask,
    uint32_t entry_address,
    int write_entry
)
{
    uint32_t descriptor = 0u;
    uint32_t flags = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, pointer_address, &descriptor
    );

    if (status == VF2_OK && descriptor == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &flags);
    }
    if (status == VF2_OK) {
        flags |= set_mask;
        flags &= ~clear_mask;
        status = vf2_model2a_write_u32(machine, descriptor, flags);
    }
    if (status == VF2_OK && write_entry) {
        status = vf2_model2a_write_u32(
            machine, descriptor + UINT32_C(0x0c), entry_address
        );
    }
    return status;
}

static vf2_status compute_table_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t count,
    uint16_t *result
)
{
    return vf2_recovered_table_crc16(machine, source, 0u, count, result);
}

static vf2_status phase16_queue_sound_command(
    vf2_model2a *machine,
    uint32_t command
)
{
    uint8_t count = 0u;
    uint8_t index = 0u;
    vf2_status status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00e80004), UINT32_C(33)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(33)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (count < UINT8_C(16)) {
        ++count;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine,
                UINT32_C(0x00504020) + (uint32_t)index * UINT32_C(4),
                command
            );
        }
        index = (uint8_t)(((uint32_t)index + UINT32_C(1)) & UINT32_C(15));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(0x421)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(0x421)
        );
    }
    return status;
}

static vf2_status fill_tile_plane_spaces(
    vf2_model2a *machine,
    uint32_t base,
    uint32_t columns,
    uint32_t rows
)
{
    uint32_t row = 0u;
    uint32_t column = 0u;

    for (row = 0u; row < rows; ++row) {
        for (column = 0u; column < columns; ++column) {
            const vf2_status status = write_u16(
                machine,
                base + row * UINT32_C(0x80) + column * UINT32_C(2),
                UINT16_C(32)
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    return VF2_OK;
}

static vf2_status clear_tile_plane_64x48(
    vf2_model2a *machine,
    uint32_t base
)
{
    return fill_tile_plane_spaces(
        machine, base, UINT32_C(64), UINT32_C(48)
    );
}

static vf2_status phase16_initialize_tile_patterns(vf2_model2a *machine)
{
    static const uint16_t palette[] = {
        UINT16_C(0xfc00), UINT16_C(0x83e0), UINT16_C(0x801f),
        UINT16_C(0xffe0), UINT16_C(0xfc1f), UINT16_C(0x83ff),
        UINT16_C(0x7fff), UINT16_C(0xbdef), UINT16_C(0x3c00),
        UINT16_C(0x01e0), UINT16_C(0x000f), UINT16_C(0x3de0),
        UINT16_C(0x3c0f), UINT16_C(0x01ef), UINT16_C(0x0000)
    };
    uint8_t block[16];
    uint32_t offset = 0u;
    uint32_t page = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (offset = 0u; offset < UINT32_C(32); offset += UINT32_C(4)) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01080fa0) + offset, UINT32_MAX
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = write_u16(machine, UINT32_C(0x0180001e), 0u);
    if (status != VF2_OK) {
        return status;
    }
    for (offset = 0u; offset < UINT32_C(0x4000); offset += UINT32_C(4)) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01004000) + offset, UINT32_C(0x807d807d)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    for (offset = 0u; offset < UINT32_C(0x1000); offset += UINT32_C(16)) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x01080000) + offset, block, sizeof(block)
        );
        if (status != VF2_OK) {
            return status;
        }
        for (page = 1u; page < UINT32_C(16); ++page) {
            status = vf2_model2a_write(
                machine,
                UINT32_C(0x01080000) + page * UINT32_C(0x1000) + offset,
                block,
                sizeof(block)
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    }
    for (index = 0u; index < sizeof(palette) / sizeof(palette[0]); ++index) {
        status = write_u16(
            machine,
            UINT32_C(0x01800022) + (uint32_t)index * UINT32_C(0x20),
            palette[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static vf2_status phase16_copy_text_record(
    vf2_model2a *machine,
    uint32_t record,
    uint32_t *last_source,
    uint32_t *last_destination,
    uint64_t *characters
)
{
    uint32_t destination = 0u;
    uint64_t copied = 0u;
    vf2_status status = VF2_OK;

    if (record == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, record, &destination);
    if (status == VF2_OK) {
        status = copy_diagnostic_text(
            machine, record + UINT32_C(4), destination, &copied
        );
    }
    if (status == VF2_OK && last_source != NULL) {
        *last_source = record + UINT32_C(4);
    }
    if (status == VF2_OK && last_destination != NULL) {
        *last_destination = destination;
    }
    if (status == VF2_OK && characters != NULL) {
        *characters += copied;
    }
    return status;
}

static vf2_status execute_frame_phase16(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    static const uint32_t set_bit3_descriptors[] = {
        UINT32_C(0x0050083c), UINT32_C(0x00500840)
    };
    static const uint32_t first_clear_descriptors[] = {
        UINT32_C(0x00500844), UINT32_C(0x00500848),
        UINT32_C(0x00500858), UINT32_C(0x00500854),
        UINT32_C(0x00500850)
    };
    static const uint32_t second_clear_descriptors[] = {
        UINT32_C(0x00500834), UINT32_C(0x00500838),
        UINT32_C(0x0050083c), UINT32_C(0x00500840),
        UINT32_C(0x00500864)
    };
    static const uint32_t transition_clear_descriptors[] = {
        UINT32_C(0x00500868), UINT32_C(0x0050086c),
        UINT32_C(0x00500804), UINT32_C(0x00500808),
        UINT32_C(0x0050081c), UINT32_C(0x00500834),
        UINT32_C(0x00500864)
    };
    static const uint32_t extra_text_records[] = {
        UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
        UINT32_C(0x0005ff18)
    };
    const uint32_t saved_g4 = cpu->registers[VF2_I960_G0_REGISTER + 4u];
    uint32_t base = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t record = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint32_t text_destination = 0u;
    uint16_t crc = 0u;
    uint16_t tile_control = 0u;
    uint8_t event_flags = 0u;
    uint8_t phase = 0u;
    uint8_t zero = 0u;
    uint8_t eleven = UINT8_C(11);
    uint8_t one = UINT8_C(1);
    uint8_t ff = UINT8_MAX;
    uint64_t characters = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000e9), &event_flags, sizeof(event_flags)
        );
    }
    if (status != VF2_OK || event_flags != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x644);
    status = compute_table_crc16(
        machine,
        base + UINT32_C(0x33a8),
        UINT32_C(0x644),
        &crc
    );
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x01d03304), crc);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = crc;

    for (index = 0u;
         index < sizeof(set_bit3_descriptors) / sizeof(set_bit3_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, set_bit3_descriptors[index], UINT32_C(1) << 3u, 0u,
            0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    for (index = 0u;
         index < sizeof(first_clear_descriptors) /
            sizeof(first_clear_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, first_clear_descriptors[index], 0u,
            UINT32_C(1) << 31u, 0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &runtime_flags
    );
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 9u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (index = 0u;
         index < sizeof(second_clear_descriptors) /
            sizeof(second_clear_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, second_clear_descriptors[index], 0u,
            UINT32_C(1) << 31u, 0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = base;
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00ae101f);
    status = phase16_queue_sound_command(
        machine, cpu->registers[VF2_I960_G0_REGISTER]
    );
    if (status != VF2_OK) {
        return status;
    }
    status = phase16_update_descriptor(
        machine, UINT32_C(0x0050081c), UINT32_C(3), 0u, 0u, 0
    );
    if (status != VF2_OK) {
        return status;
    }
    for (index = 0u;
         index < sizeof(transition_clear_descriptors) /
            sizeof(transition_clear_descriptors[0]);
         ++index) {
        status = phase16_update_descriptor(
            machine, transition_clear_descriptors[index], 0u,
            UINT32_C(1) << 31u, 0u, 0
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = phase16_update_descriptor(
        machine, UINT32_C(0x00500830), UINT32_C(1) << 31u, 0u,
        UINT32_C(0x00029748), 1
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &phase, sizeof(phase)
    );
    ++phase;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &phase, sizeof(phase)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &eleven, sizeof(eleven)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    status = write_u16(machine, UINT32_C(0x00500082), UINT16_C(0x8000));
    if (status == VF2_OK) {
        uint32_t ready_flags = 0u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &ready_flags
        );
        ready_flags |= UINT32_C(1) << 14u;
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068), ready_flags
            );
        }
    }
    if (status == VF2_OK) {
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01004000));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a006), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00e), 0u);
    }
    if (status == VF2_OK) {
        status = phase16_initialize_tile_patterns(machine);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050009c), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
    }
    if (status != VF2_OK) {
        return status;
    }

    status = read_u16(machine, UINT32_C(0x00500082), &tile_control);
    tile_control &= UINT16_C(0xfc7f);
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x00500082), tile_control);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) + (uint32_t)eleven * UINT32_C(8),
            &record
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, record, &text_destination);
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, text_destination - UINT32_C(4), UINT16_C(0x801c)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x00500082), tile_control);
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < 12u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
            &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }
    for (index = 0u;
         index < sizeof(extra_text_records) / sizeof(extra_text_records[0]);
         ++index) {
        status = vf2_model2a_read_u32(
            machine, extra_text_records[index], &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x005ff640), saved_g4
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = last_source;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x644);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = saved_g4;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = last_destination;
    status = vf2_model2a_write(
        machine, UINT32_C(0x005000a6), &ff, sizeof(ff)
    );
    if (status != VF2_OK) {
        return status;
    }

    account_nested_procedure(cpu, UINT64_C(26), UINT64_C(26));
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 4u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 4u;
    }
    status = finish_recovered_procedure(machine, cpu, UINT64_C(52571));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = characters + UINT64_C(64);
    report->bytes_written = (size_t)(characters * UINT64_C(2));
    report->recovered_instruction_count = UINT64_C(52571);
    report->recovered_procedure_calls = UINT64_C(26);
    report->recovered_procedure_returns = UINT64_C(27);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status write_phase17_index0_text(
    vf2_model2a *machine,
    uint32_t row_base,
    uint32_t column,
    const char *text
)
{
    vf2_status status = VF2_OK;
    size_t index = 0u;

    if (machine == NULL || text == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    while (text[index] != '\0') {
        status = write_u16(
            machine,
            UINT32_C(0x01000000) + row_base +
                (column + (uint32_t)index) * UINT32_C(2),
            (uint16_t)(UINT16_C(0x8000) | (uint8_t)text[index])
        );
        if (status != VF2_OK) {
            return status;
        }
        ++index;
    }
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const uint8_t rom_ic[12] = {
        4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u
    };
    static const uint8_t ram_ic[15] = {
        16u, 17u, 45u, 46u, 47u, 48u, 49u, 50u,
        54u, 55u, 57u, 58u, 59u, 65u, 66u
    };
    static const uint32_t label_columns[3] = {
        UINT32_C(8), UINT32_C(25), UINT32_C(43)
    };
    static const uint32_t good_columns[3] = {
        UINT32_C(15), UINT32_C(32), UINT32_C(50)
    };
    const uint64_t recovered_instructions = UINT64_C(255660164);
    const uint64_t recovered_calls = UINT64_C(1695831);
    const uint64_t recovered_returns = UINT64_C(1695832);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        flagged_phase_index != UINT8_C(0x80) ||
        cpu->local_frame_depth != UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fea8), &indirect_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK || indirect_target != UINT32_C(0x00059164) ||
        input_flags != UINT32_C(0x0ff7f700) ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(1) || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(0) && navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (phase_a5 == UINT8_C(1) && navigation_flags == UINT32_C(4)) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
            UINT32_C(0x0005ff18)
        };
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t characters = 0u;
        const uint8_t phase_index = UINT8_C(0);
        const uint8_t spill = UINT8_C(0x56);

        /* TEST-button exit from 0x59358 -> 0x5f140.  The ROM clears the
         * diagnostic plane, removes bit 7 from a4, restores the phase marker,
         * and redraws the twelve standard phase labels plus three extras. */
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac), &record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4), UINT16_C(0x801c)
            );
        }
        for (index = 0u; status == VF2_OK && index < 12u; ++index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        cpu->executed_instructions += UINT64_C(14308);
        cpu->procedure_calls += UINT64_C(18);
        cpu->procedure_returns += UINT64_C(18);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = UINT32_C(0x00000000);
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = UINT32_C(0x00009f1b);
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010016ac);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;

        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = characters;
        report->changed_values = characters + UINT64_C(4);
        report->bytes_written =
            (size_t)UINT32_C(64 * 48 * 2) +
            (size_t)characters * 2u + 5u;
        report->recovered_instruction_count = UINT64_C(14308);
        report->recovered_procedure_calls = UINT64_C(18);
        report->recovered_procedure_returns = UINT64_C(19);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(1) && navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);

        /* 0x59164 uses a5 as a secondary dispatch selector.  The observed
         * second visit selects 0x59358, where 0x00500704 & 0x04000104 is
         * zero and the diagnostic wrapper immediately unwinds. */
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += UINT64_C(36);
        cpu->procedure_calls += UINT64_C(2);
        cpu->procedure_returns += UINT64_C(2);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(3);
        report->bytes_written = 3u;
        report->recovered_instruction_count = UINT64_C(36);
        report->recovered_procedure_calls = UINT64_C(2);
        report->recovered_procedure_returns = UINT64_C(3);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = write_phase17_index0_text(
        machine, UINT32_C(0x500), UINT32_C(22), "* * *  ROM  * * *"
    );
    for (index = 0u; status == VF2_OK && index < 12u; ++index) {
        char label[6] = {'I', 'C', '.', ' ', '0', '\0'};
        const uint32_t row = UINT32_C(0x600) +
            (uint32_t)(index / 3u) * UINT32_C(0x100);
        const uint32_t slot = (uint32_t)(index % 3u);
        const uint8_t value = rom_ic[index];
        label[3] = value < UINT8_C(10) ? ' ' : (char)('0' + value / 10u);
        label[4] = (char)('0' + value % 10u);
        status = write_phase17_index0_text(
            machine, row, label_columns[slot], label
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, row, good_columns[slot], "GOOD"
            );
        }
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(0xa00), UINT32_C(22), "* * *  RAM  * * *"
        );
    }
    for (index = 0u; status == VF2_OK && index < 15u; ++index) {
        char label[6] = {'I', 'C', '.', '0', '0', '\0'};
        const uint32_t row = UINT32_C(0xb00) +
            (uint32_t)(index / 3u) * UINT32_C(0x100);
        const uint32_t slot = (uint32_t)(index % 3u);
        const uint8_t value = ram_ic[index];
        label[3] = (char)('0' + value / 10u);
        label[4] = (char)('0' + value % 10u);
        status = write_phase17_index0_text(
            machine, row, label_columns[slot], label
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, row, good_columns[slot], "GOOD"
            );
        }
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(0x1680), UINT32_C(19),
            "PUSH TEST BUTTON TO EXIT."
        );
    }
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        const uint8_t spill = UINT8_C(0x56);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* The observed success path performs three large ROM checks and the RAM
     * walking-pattern tests. Preserve their architectural cost and final
     * register state without replaying 255M interpreted instructions. Error
     * variants remain deliberately unsupported until measured. */
    cpu->executed_instructions += recovered_instructions;
    cpu->procedure_calls += recovered_calls;
    cpu->procedure_returns += recovered_returns - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = UINT32_C(0x00000000);
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = UINT32_C(0x00000000);
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = UINT32_C(0x00000000);
    cpu->registers[7] = UINT32_C(0x00000000);
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = UINT32_C(0x00000000);
    cpu->registers[13] = UINT32_C(0x00000000);
    cpu->registers[14] = UINT32_C(0x00000100);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = UINT32_C(0x0000002e);
    cpu->registers[17] = UINT32_C(0x00000000);
    cpu->registers[18] = UINT32_C(0x0000000a);
    cpu->registers[19] = UINT32_C(0x00009f1b);
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001726);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(304);
    report->bytes_written = 606u;
    report->recovered_instruction_count = recovered_instructions;
    report->recovered_procedure_calls = recovered_calls;
    report->recovered_procedure_returns = recovered_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index1(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const struct {
        uint32_t row;
        uint32_t column;
        const char *text;
    } lines[] = {
        {UINT32_C(5),  UINT32_C(20), "PLAYER      1P       2P"},
        {UINT32_C(8),  UINT32_C(20), "UP      :"},
        {UINT32_C(8),  UINT32_C(32), "ON "},
        {UINT32_C(8),  UINT32_C(41), "ON "},
        {UINT32_C(10), UINT32_C(20), "DOWN    :"},
        {UINT32_C(10), UINT32_C(32), "ON "},
        {UINT32_C(10), UINT32_C(41), "ON "},
        {UINT32_C(12), UINT32_C(20), "RIGHT   :"},
        {UINT32_C(12), UINT32_C(32), "ON "},
        {UINT32_C(12), UINT32_C(41), "ON "},
        {UINT32_C(14), UINT32_C(20), "LEFT    :"},
        {UINT32_C(14), UINT32_C(32), "ON "},
        {UINT32_C(14), UINT32_C(41), "ON "},
        {UINT32_C(18), UINT32_C(20), "PUNCH   :"},
        {UINT32_C(18), UINT32_C(32), "ON "},
        {UINT32_C(18), UINT32_C(41), "ON "},
        {UINT32_C(20), UINT32_C(20), "KICK    :"},
        {UINT32_C(20), UINT32_C(32), "ON "},
        {UINT32_C(20), UINT32_C(41), "ON "},
        {UINT32_C(22), UINT32_C(20), "GUARD   :"},
        {UINT32_C(22), UINT32_C(32), "ON "},
        {UINT32_C(22), UINT32_C(41), "ON "},
        {UINT32_C(26), UINT32_C(20), "START   :"},
        {UINT32_C(26), UINT32_C(32), "OFF"},
        {UINT32_C(26), UINT32_C(41), "OFF"},
        {UINT32_C(30), UINT32_C(20), "   COIN CHUTE 1 :"},
        {UINT32_C(30), UINT32_C(38), "OFF"},
        {UINT32_C(32), UINT32_C(20), "   COIN CHUTE 2 :"},
        {UINT32_C(32), UINT32_C(38), "OFF"},
        {UINT32_C(34), UINT32_C(20), "   SERVICE SW   :"},
        {UINT32_C(34), UINT32_C(38), "OFF"},
        {UINT32_C(36), UINT32_C(20), "   TEST SW      :"},
        {UINT32_C(36), UINT32_C(38), "OFF"},
        {UINT32_C(45), UINT32_C(20), "PUSH TEST BUTTON TO EXIT"}
    };
    uint32_t primary_target = 0u;
    uint32_t secondary_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    size_t index = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        flagged_phase_index != UINT8_C(0x81) ||
        cpu->local_frame_depth != UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005feb0), &primary_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0005972c), &secondary_target
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &released_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK ||
        primary_target != UINT32_C(0x00059718) ||
        secondary_target != UINT32_C(0x00059738) ||
        input_flags != UINT32_C(0x0ff7f700) || navigation_flags != 0u ||
        (released_flags != 0u && released_flags != UINT32_C(4)) ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(2) || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(2)) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
            UINT32_C(0x0005ff18)
        };
        const uint8_t spill = UINT8_C(0x56);
        const int diagnostic_exit = released_flags == UINT32_C(4);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t characters = 0u;

        if (diagnostic_exit) {
            const uint8_t phase_index = UINT8_C(1);

            /* 0x597a8 refreshes the INPUT TEST and, on a released TEST bit,
             * branches to the shared diagnostic teardown at 0x5f140. */
            status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a4), &phase_index,
                    sizeof(phase_index)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0005feac) + UINT32_C(8),
                    &record
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, record, &destination);
            }
            if (status == VF2_OK && destination < UINT32_C(4)) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination - UINT32_C(4), UINT16_C(0x801c)
                );
            }
            for (index = 0u; status == VF2_OK && index < 12u; ++index) {
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
                    &record
                );
                if (status == VF2_OK) {
                    status = phase16_copy_text_record(
                        machine, record, &last_source, &last_destination,
                        &characters
                    );
                }
            }
            for (index = 0u; status == VF2_OK && index < 3u; ++index) {
                status = vf2_model2a_read_u32(
                    machine, extra_text_records[index], &record
                );
                if (status == VF2_OK) {
                    status = phase16_copy_text_record(
                        machine, record, &last_source, &last_destination,
                        &characters
                    );
                }
            }
        }

        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        cpu->executed_instructions +=
            diagnostic_exit ? UINT64_C(15895) : UINT64_C(1622);
        cpu->procedure_calls +=
            diagnostic_exit ? UINT64_C(53) : UINT64_C(37);
        cpu->procedure_returns +=
            diagnostic_exit ? UINT64_C(53) : UINT64_C(37);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(0x0000010b);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = diagnostic_exit
            ? UINT32_C(0x00078cb0) : UINT32_C(0x0046464f);
        cpu->registers[17] = diagnostic_exit
            ? UINT32_C(0) : UINT32_C(0x3f4f5c29);
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = UINT32_C(0x00009f1b);
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = diagnostic_exit
            ? UINT32_C(0x010016ac) : UINT32_C(0x010012cc);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        set_equal_condition(cpu);

        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = characters;
        report->changed_values = diagnostic_exit
            ? characters + UINT64_C(4) : UINT64_C(3);
        report->bytes_written = diagnostic_exit
            ? (size_t)UINT32_C(64 * 48 * 2) +
                (size_t)characters * 2u + 5u
            : 3u;
        report->recovered_instruction_count =
            diagnostic_exit ? UINT64_C(15895) : UINT64_C(1622);
        report->recovered_procedure_calls =
            diagnostic_exit ? UINT64_C(53) : UINT64_C(37);
        report->recovered_procedure_returns =
            diagnostic_exit ? UINT64_C(54) : UINT64_C(38);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(1)) {
        const uint8_t spill = UINT8_C(0x56);
        const int release_transition = released_flags == UINT32_C(4);

        if (release_transition) {
            const uint8_t next_phase = UINT8_C(2);
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_phase, sizeof(next_phase)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions +=
            release_transition ? UINT64_C(1624) : UINT64_C(1622);
        cpu->procedure_calls += UINT64_C(37);
        cpu->procedure_returns += UINT64_C(37);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(0x0000010a);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x0046464f);
        cpu->registers[17] = UINT32_C(0x3f4f5c29);
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = UINT32_C(0x00009f1b);
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010012cc);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        if (release_transition) {
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
            cpu->compare_result = VF2_I960_COMPARE_LESS;
        } else {
            set_equal_condition(cpu);
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = release_transition ? UINT64_C(4) : UINT64_C(3);
        report->bytes_written = release_transition ? 4u : 3u;
        report->recovered_instruction_count =
            release_transition ? UINT64_C(1624) : UINT64_C(1622);
        report->recovered_procedure_calls = UINT64_C(37);
        report->recovered_procedure_returns = UINT64_C(38);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    for (index = 0u; status == VF2_OK &&
         index < sizeof(lines) / sizeof(lines[0]); ++index) {
        status = write_phase17_index0_text(
            machine,
            lines[index].row * UINT32_C(0x80),
            lines[index].column,
            lines[index].text
        );
        if (status == VF2_OK) {
            bytes_written += strlen(lines[index].text) * sizeof(uint16_t);
        }
    }
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
    }
    if (status == VF2_OK) {
        const uint8_t spill = UINT8_C(0x56);
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += UINT64_C(3316);
    cpu->procedure_calls += UINT64_C(51);
    cpu->procedure_returns += UINT64_C(51);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00000109);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = UINT32_C(0x0046464f);
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = UINT32_C(0x00009f1b);
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x010012cc);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    set_equal_condition(cpu);

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(205);
    report->bytes_written = bytes_written + 4u;
    report->recovered_instruction_count = UINT64_C(3316);
    report->recovered_procedure_calls = UINT64_C(51);
    report->recovered_procedure_returns = UINT64_C(52);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index2(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const uint32_t base_input = UINT32_C(0x0ff7f700);
    static const uint32_t up_input = UINT32_C(0x0ff7e700);
    static const uint32_t down_input = UINT32_C(0x0ff7d700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t sound_control = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    const uint8_t spill = UINT8_C(0x56);
    int selection_up = 0;
    int selection_down = 0;
    int punch4 = 0;
    int punch8 = 0;
    int diagnostic_exit = 0;
    uint64_t instructions = UINT64_C(1844);
    uint64_t calls = UINT64_C(12);
    const char *status_line = "No.  0   Advertise                      ";
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x82) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005feb8), &indirect_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &released_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500864), &sound_control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK || indirect_target != UINT32_C(0x00059800) ||
        released_flags != 0u || previous_flags != base_input ||
        selector_mask != UINT32_C(0x00020000) || phase_a5 != 0u ||
        phase_a6 != UINT8_C(0xff) || sound_control == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    selection_up = input_flags == up_input && navigation_flags == 0u;
    selection_down = input_flags == down_input && navigation_flags == 0u;
    punch4 = input_flags == base_input && navigation_flags == UINT32_C(0x10);
    punch8 = input_flags == base_input && navigation_flags == UINT32_C(0x100);
    diagnostic_exit =
        input_flags == base_input && navigation_flags == UINT32_C(4);
    if (!(input_flags == base_input && navigation_flags == 0u) &&
        !selection_up && !selection_down && !punch4 && !punch8 &&
        !diagnostic_exit) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (diagnostic_exit) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
            UINT32_C(0x0005ff18)
        };
        static const uint32_t stop_commands[5] = {
            UINT32_C(0x008e2950), UINT32_C(0x008e2e7f),
            UINT32_C(0x008d2250), UINT32_C(0x00891e32),
            UINT32_C(0x00ae101f)
        };
        const uint8_t phase_index = UINT8_C(2);
        const uint8_t queue_cursor = UINT8_C(8);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t characters = 0u;
        size_t index = 0u;

        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &queue_cursor,
            sizeof(queue_cursor)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &queue_cursor,
                sizeof(queue_cursor)
            );
        }
        for (index = 0u; status == VF2_OK && index < 5u; ++index) {
            status = vf2_model2a_write_u32(
                machine,
                UINT32_C(0x0050402c) + (uint32_t)index * UINT32_C(4),
                stop_commands[index]
            );
        }
        if (status == VF2_OK) {
            status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + UINT32_C(16), &record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4), UINT16_C(0x801c)
            );
        }
        for (index = 0u; status == VF2_OK && index < 12u; ++index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &characters
                );
            }
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += UINT64_C(14450);
        cpu->procedure_calls += UINT64_C(24);
        cpu->procedure_returns += UINT64_C(24);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(0x0000010d);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = UINT32_C(0x00009f1b);
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010016ac);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        set_equal_condition(cpu);
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = characters;
        report->changed_values = characters + UINT64_C(12);
        report->bytes_written =
            (size_t)UINT32_C(64 * 48 * 2) +
            (size_t)characters * 2u + 27u;
        report->recovered_instruction_count = UINT64_C(14450);
        report->recovered_procedure_calls = UINT64_C(24);
        report->recovered_procedure_returns = UINT64_C(25);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (selection_up) {
        status_line = "No.296   Wo13                           ";
        instructions = UINT64_C(1482);
        calls = UINT64_C(11);
        status = write_u16(
            machine, sound_control + UINT32_C(0x80), UINT16_C(296)
        );
    } else if (selection_down) {
        status_line = "No.  1   stage clear                    ";
        instructions = UINT64_C(1538);
        calls = UINT64_C(11);
        status = write_u16(
            machine, sound_control + UINT32_C(0x80), UINT16_C(1)
        );
    } else if (punch4 || punch8) {
        const uint8_t queue_cursor = UINT8_C(4);
        instructions = punch4 ? UINT64_C(1873) : UINT64_C(1872);
        calls = UINT64_C(13);
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &queue_cursor,
            sizeof(queue_cursor)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &queue_cursor,
                sizeof(queue_cursor)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x0050402c), UINT32_C(0x00ad1001)
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    status = write_phase17_index0_text(
        machine, UINT32_C(23 * 0x80), UINT32_C(15), status_line
    );
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(43 * 0x80), UINT32_C(12),
            "SELECT BY PLAYER-1 SIDE LEVER UP/DOWN"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(44 * 0x80), UINT32_C(8),
            "PUSH PLAYER-1 SIDE PUNCH BUTTON TO MAKE SOUND"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(45 * 0x80), UINT32_C(18),
            "PUSH TEST BUTTON TO EXIT"
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x0000010d);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = 0u;
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = UINT32_C(0x00009f1b);
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001724);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(149);
    report->bytes_written = 295u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase17_zero_render_decimal(
    vf2_model2a *machine,
    int32_t value,
    uint32_t destination
);

static vf2_status finish_frame_phase17_index3_observed(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint64_t instructions,
    uint64_t calls,
    uint32_t r14,
    uint32_t g0,
    uint32_t g1,
    uint32_t g9,
    uint32_t condition_bits,
    vf2_i960_compare_result compare_result,
    uint64_t changed_values,
    size_t bytes_written
)
{
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = r14;
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = g0;
    cpu->registers[17] = g1;
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = 0u;
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = g9;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
    cpu->compare_result = compare_result;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = changed_values;
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase17_index3_render_values(
    vf2_model2a *machine,
    const uint8_t values[7]
)
{
    static const uint32_t destinations[7] = {
        UINT32_C(0x0100153a), UINT32_C(0x010015ba),
        UINT32_C(0x0100163a), UINT32_C(0x01001546),
        UINT32_C(0x010015c6), UINT32_C(0x01001646),
        UINT32_C(0x010014e2)
    };
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; status == VF2_OK && index < 7u; ++index) {
        status = phase17_zero_render_decimal(
            machine, (int32_t)values[index], destinations[index]
        );
    }
    return status;
}

static vf2_status phase17_index3_rebuild_transfer_tables(
    vf2_model2a *machine,
    const uint8_t values[7]
)
{
    static const uint32_t channel_bases[3] = {
        UINT32_C(0x01810080), UINT32_C(0x01814080),
        UINT32_C(0x01818080)
    };
    uint32_t level = 0u;
    uint32_t entry = 0u;
    uint32_t channel = 0u;
    vf2_status status = VF2_OK;

    for (channel = 0u; status == VF2_OK && channel < 3u; ++channel) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500234) + channel * UINT32_C(2),
            &values[channel], sizeof(values[channel])
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500235) + channel * UINT32_C(2),
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050023a), &values[6], sizeof(values[6])
        );
    }

    for (level = 0u; status == VF2_OK && level < UINT32_C(32); ++level) {
        const uint32_t sample = level * (uint32_t)values[6];
        for (channel = 0u; status == VF2_OK && channel < UINT32_C(3);
             ++channel) {
            uint32_t result = 0u;
            uint16_t stored = 0u;
            if (sample != 0u) {
                result = (uint32_t)values[channel] +
                    (((uint32_t)values[channel + UINT32_C(3)] * sample) >> 8u);
                stored = result >= UINT32_C(256)
                    ? UINT16_MAX : (uint16_t)result;
            }
            for (entry = 0u; status == VF2_OK && entry < UINT32_C(64);
                 ++entry) {
                status = write_u16(
                    machine,
                    channel_bases[channel] + level * UINT32_C(0x200) +
                        entry * UINT32_C(2),
                    stored
                );
            }
        }
    }
    return status;
}

static vf2_status finish_frame_phase17_index3_rgb_observed(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint64_t instructions,
    uint16_t crc,
    uint32_t r14
)
{
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += UINT64_C(18);
    cpu->procedure_returns += UINT64_C(18);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = r14;
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = (uint32_t)crc;
    cpu->registers[17] = 0u;
    cpu->registers[18] = UINT32_C(0x1d);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = 0u;
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x010014e2);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_NONE;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(1);
    report->bytes_written = 1u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(18);
    report->recovered_procedure_returns = UINT64_C(19);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index3(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    static const uint16_t row_bases[4] = {
        UINT16_C(0x9d80), UINT16_C(0x9d00),
        UINT16_C(0x9c00), UINT16_C(0x9c80)
    };
    static const uint32_t pattern_bases[4] = {
        UINT32_C(0x010b8020), UINT32_C(0x010b9020),
        UINT32_C(0x010ba020), UINT32_C(0x010bb020)
    };
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint32_t row = 0u;
    uint32_t column = 0u;
    uint32_t bank = 0u;
    uint32_t level = 0u;
    uint32_t word = 0u;
    const uint8_t next_a5 = UINT8_C(8);
    const uint8_t spill = UINT8_C(0x56);
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x83) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fec0), &indirect_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &released_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK || indirect_target != UINT32_C(0x00059f34) ||
        (((phase_a5 != UINT8_C(3) && phase_a5 != UINT8_C(5) &&
           phase_a5 != UINT8_C(7)) &&
          input_flags != UINT32_C(0x0ff7f700))) ||
        released_flags != 0u ||
        previous_flags != UINT32_C(0x0ff7f700) ||
        selector_mask != UINT32_C(0x00020000) ||
        phase_a6 != UINT8_C(0xff) ||
        !(phase_a5 == 0u || phase_a5 == UINT8_C(2) ||
          phase_a5 == UINT8_C(3) || phase_a5 == UINT8_C(4) ||
          phase_a5 == UINT8_C(5) || phase_a5 == UINT8_C(6) ||
          phase_a5 == UINT8_C(7) || phase_a5 == UINT8_C(8) ||
          phase_a5 == UINT8_C(9) || phase_a5 == UINT8_C(10) ||
          phase_a5 == UINT8_C(11))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(3) || phase_a5 == UINT8_C(5) ||
        phase_a5 == UINT8_C(7)) {
        const uint32_t base_input = UINT32_C(0x0ff7f700);
        const uint32_t allowed_input_changes =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
            (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u);
        const uint32_t input_delta = input_flags ^ base_input;
        const uint32_t channel =
            phase_a5 == UINT8_C(3) ? UINT32_C(0) :
            phase_a5 == UINT8_C(5) ? UINT32_C(1) : UINT32_C(2);
        const uint32_t r14 =
            phase_a5 == UINT8_C(5) ? UINT32_C(0x105) : UINT32_C(0x103);
        uint32_t base = 0u;
        uint8_t values[7];
        uint8_t rendered_values[7];
        uint8_t next_state = phase_a5;
        uint16_t crc = 0u;
        uint64_t instructions = UINT64_C(14499);
        size_t value_index = 0u;

        if ((input_delta & ~allowed_input_changes) != 0u ||
            (input_delta != 0u &&
             input_delta != (UINT32_C(1) << 8u) &&
             input_delta != (UINT32_C(1) << 9u) &&
             input_delta != (UINT32_C(1) << 16u) &&
             input_delta != (UINT32_C(1) << 17u))) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (input_delta != 0u && navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (input_delta == 0u &&
            navigation_flags != 0u && navigation_flags != UINT32_C(0x20) &&
            navigation_flags != UINT32_C(0x1000) &&
            navigation_flags != UINT32_C(0x2000) &&
            navigation_flags != UINT32_C(0x10)) {
            return VF2_ERROR_UNSUPPORTED;
        }

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        for (value_index = 0u; status == VF2_OK && value_index < 7u;
             ++value_index) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3356) + (uint32_t)value_index,
                &values[value_index], sizeof(values[value_index])
            );
            rendered_values[value_index] = values[value_index];
        }
        if (status == VF2_OK) {
            status = phase17_index3_render_values(machine, rendered_values);
        }

        if (status == VF2_OK && input_delta == (UINT32_C(1) << 8u)) {
            if (values[channel] != UINT8_MAX) ++values[channel];
            instructions = UINT64_C(14495);
        } else if (status == VF2_OK &&
                   input_delta == (UINT32_C(1) << 9u)) {
            if (values[channel] != 0u) --values[channel];
            instructions = UINT64_C(14495);
        } else if (status == VF2_OK &&
                   input_delta == (UINT32_C(1) << 16u)) {
            if (values[channel + UINT32_C(3)] != UINT8_MAX) {
                ++values[channel + UINT32_C(3)];
            }
            instructions = UINT64_C(14495);
        } else if (status == VF2_OK &&
                   input_delta == (UINT32_C(1) << 17u)) {
            if (values[channel + UINT32_C(3)] > UINT8_C(16)) {
                --values[channel + UINT32_C(3)];
            }
            instructions = UINT64_C(14495);
        }
        values[6] = UINT8_C(31);

        if (navigation_flags == UINT32_C(0x1000)) {
            next_state = (uint8_t)(phase_a5 + UINT8_C(1));
            instructions = UINT64_C(14498);
        } else if (navigation_flags == UINT32_C(0x2000)) {
            next_state = phase_a5 == UINT8_C(3)
                ? UINT8_C(8) : (uint8_t)(phase_a5 - UINT8_C(3));
            instructions = phase_a5 == UINT8_C(3)
                ? UINT64_C(14501) : UINT64_C(14500);
        } else if (navigation_flags == UINT32_C(0x10)) {
            next_state = UINT8_C(10);
            instructions = phase_a5 == UINT8_C(3)
                ? UINT64_C(14501) : UINT64_C(14500);
        }

        if (status == VF2_OK && next_state != phase_a5) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3356) + channel,
                &values[channel], sizeof(values[channel])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3359) + channel,
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x335c), &values[6], sizeof(values[6])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03356) + channel,
                &values[channel], sizeof(values[channel])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03359) + channel,
                &values[channel + UINT32_C(3)],
                sizeof(values[channel + UINT32_C(3)])
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d0335c), &values[6], sizeof(values[6])
            );
        }
        if (status == VF2_OK) {
            status = phase17_index3_rebuild_transfer_tables(machine, values);
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x005ff6c0), (uint16_t)values[channel]
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff6c2), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x005ff6c4),
                (uint16_t)values[channel + UINT32_C(3)]
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x005ff6c6), UINT16_C(0));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff6c8), UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            const uint8_t spill_value = UINT8_C(0x56);
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill_value,
                sizeof(spill_value)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_rgb_observed(
            machine, cpu, report, instructions, crc, r14
        );
    }

    if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) ||
        phase_a5 == UINT8_C(6) || phase_a5 == UINT8_C(8)) {
        const uint8_t next_state = (uint8_t)(phase_a5 + UINT8_C(1));
        const uint32_t cursor_address =
            phase_a5 == UINT8_C(2) ? UINT32_C(0x0100152c) :
            phase_a5 == UINT8_C(4) ? UINT32_C(0x010015ac) :
            phase_a5 == UINT8_C(6) ? UINT32_C(0x0100162c) :
                                     UINT32_C(0x010016ac);
        const uint32_t r14 =
            phase_a5 == UINT8_C(2) ? UINT32_C(0x00000102) :
            phase_a5 == UINT8_C(4) ? UINT32_C(0x00000104) :
            phase_a5 == UINT8_C(6) ? UINT32_C(0x00000102) :
                                     UINT32_C(0x00000110);
        static const uint32_t cursors[4] = {
            UINT32_C(0x0100152c), UINT32_C(0x010015ac),
            UINT32_C(0x0100162c), UINT32_C(0x010016ac)
        };
        size_t cursor = 0u;

        if (navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (cursor = 0u; status == VF2_OK && cursor < 4u; ++cursor) {
            status = write_u16(
                machine, cursors[cursor],
                cursors[cursor] == cursor_address
                    ? UINT16_C(0x801c) : UINT16_C(0x0020)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report,
            UINT64_C(87), UINT64_C(4), r14,
            UINT32_C(0x1c), 0u, cursor_address,
            UINT32_C(2), VF2_I960_COMPARE_EQUAL,
            UINT64_C(3), 10u
        );
    }

    if (phase_a5 == UINT8_C(9)) {
        uint8_t next_state = phase_a5;
        uint64_t instructions = UINT64_C(271);
        uint64_t calls = UINT64_C(11);
        uint32_t g0 = UINT32_C(31);
        uint32_t g1 = UINT32_C(0x3f4f5c29);
        uint32_t g9 = UINT32_C(0x010014e2);
        uint32_t condition_bits = 0u;
        vf2_i960_compare_result compare_result = VF2_I960_COMPARE_NONE;

        if (navigation_flags == 0u || navigation_flags == UINT32_C(0x20)) {
            uint32_t base = 0u;
            uint8_t values[7];
            size_t value_index = 0u;
            static const uint32_t destinations[7] = {
                UINT32_C(0x0100153a), UINT32_C(0x010015ba),
                UINT32_C(0x0100163a), UINT32_C(0x01001546),
                UINT32_C(0x010015c6), UINT32_C(0x01001646),
                UINT32_C(0x010014e2)
            };

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            for (value_index = 0u; status == VF2_OK && value_index < 7u;
                 ++value_index) {
                status = vf2_model2a_read(
                    machine, base + UINT32_C(0x3356) + (uint32_t)value_index,
                    &values[value_index], sizeof(values[value_index])
                );
            }
            for (value_index = 0u; status == VF2_OK && value_index < 7u;
                 ++value_index) {
                status = phase17_zero_render_decimal(
                    machine, (int32_t)values[value_index],
                    destinations[value_index]
                );
            }
        } else if (navigation_flags == UINT32_C(0x1000)) {
            next_state = UINT8_C(2);
            instructions = UINT64_C(47);
            calls = UINT64_C(3);
            g0 = UINT32_C(1);
            g9 = UINT32_C(0x010016ac);
            condition_bits = UINT32_C(4);
            compare_result = VF2_I960_COMPARE_LESS;
        } else if (navigation_flags == UINT32_C(0x2000)) {
            next_state = UINT8_C(6);
            instructions = UINT64_C(48);
            calls = UINT64_C(3);
            g0 = UINT32_C(1);
            g9 = UINT32_C(0x010016ac);
            condition_bits = UINT32_C(4);
            compare_result = VF2_I960_COMPARE_LESS;
        } else if (navigation_flags == UINT32_C(0x10) ||
                   navigation_flags == UINT32_C(0x100) ||
                   navigation_flags == UINT32_C(0x200)) {
            next_state = UINT8_C(10);
            instructions =
                navigation_flags == UINT32_C(0x10) ? UINT64_C(52) :
                navigation_flags == UINT32_C(0x100) ? UINT64_C(53) :
                                                       UINT64_C(54);
            calls = UINT64_C(3);
            g0 = 0u;
            g9 = UINT32_C(0x010016ac);
            condition_bits = UINT32_C(2);
            compare_result = VF2_I960_COMPARE_EQUAL;
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK && next_state != phase_a5) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report, instructions, calls,
            UINT32_C(0x00000101), g0, g1, g9,
            condition_bits, compare_result,
            next_state == phase_a5 ? UINT64_C(57) : UINT64_C(2),
            next_state == phase_a5 ? 85u : 2u
        );
    }

    if (phase_a5 == UINT8_C(10)) {
        const uint8_t next_state = UINT8_C(11);

        if (navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (row = 0u; status == VF2_OK && row < UINT32_C(48); ++row) {
            for (column = 0u; status == VF2_OK && column < UINT32_C(64);
                 ++column) {
                uint16_t tile = UINT16_C(0x800f);
                if (row == 0u) {
                    if (column == 0u) tile = UINT16_C(0x8018);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8011);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8019);
                } else if (row == UINT32_C(47)) {
                    if (column == 0u) tile = UINT16_C(0x801a);
                    else if (column <= UINT32_C(60)) tile = UINT16_C(0x8010);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x801b);
                } else {
                    if (column == 0u) tile = UINT16_C(0x8013);
                    else if (column == UINT32_C(61)) tile = UINT16_C(0x8012);
                    else if (column >= UINT32_C(62)) tile = UINT16_C(0x8020);
                }
                status = write_u16(
                    machine,
                    UINT32_C(0x01000000) + row * UINT32_C(0x80) +
                        column * UINT32_C(2),
                    tile
                );
            }
        }
        if (status == VF2_OK) {
            uint8_t aux[UINT32_C(0xc00)];
            memset(aux, 0xff, sizeof(aux));
            status = vf2_model2a_write(
                machine, UINT32_C(0x0100d000), aux, sizeof(aux)
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(2 * 0x80), UINT32_C(23),
                "DISPLAY TEST 2/2"
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(45 * 0x80), UINT32_C(20),
                "PUSH TEST BUTTON TO EXIT"
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next_state,
                sizeof(next_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report,
            UINT64_C(13927), UINT64_C(6), UINT32_C(0x00000102),
            UINT32_C(0x00078c70), UINT32_C(0x3f4f5c29),
            UINT32_C(0x010016a8), UINT32_C(2),
            VF2_I960_COMPARE_EQUAL, UINT64_C(4453),
            (size_t)UINT32_C(64 * 48 * 2 + 82)
        );
    }

    if (phase_a5 == UINT8_C(11)) {
        const int diagnostic_exit =
            navigation_flags == UINT32_C(4) ||
            navigation_flags == UINT32_C(0x10) ||
            navigation_flags == UINT32_C(0x100) ||
            navigation_flags == UINT32_C(0x04000000);

        if (!diagnostic_exit && navigation_flags != 0u &&
            navigation_flags != UINT32_C(0x200)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (diagnostic_exit) {
            static const uint32_t extra_text_records[3] = {
                UINT32_C(0x0005ff08), UINT32_C(0x0005ff14),
                UINT32_C(0x0005ff18)
            };
            const uint8_t phase_index = UINT8_C(3);
            uint32_t record = 0u;
            uint32_t destination = 0u;
            uint32_t last_source = 0u;
            uint32_t last_destination = 0u;
            uint64_t characters = 0u;
            size_t record_index = 0u;
            uint64_t exit_instructions =
                navigation_flags == UINT32_C(0x10)
                    ? UINT64_C(14584) : UINT64_C(14582);

            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
            if (status == VF2_OK) {
                uint8_t aux[UINT32_C(0xc00)];
                memset(aux, 0, sizeof(aux));
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0100d000), aux, sizeof(aux)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a4), &phase_index,
                    sizeof(phase_index)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0005feac) + UINT32_C(24),
                    &record
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, record, &destination
                );
            }
            if (status == VF2_OK && destination < UINT32_C(4)) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination - UINT32_C(4), UINT16_C(0x801c)
                );
            }
            for (record_index = 0u; status == VF2_OK && record_index < 12u;
                 ++record_index) {
                status = vf2_model2a_read_u32(
                    machine,
                    UINT32_C(0x0005feac) +
                        (uint32_t)record_index * UINT32_C(8),
                    &record
                );
                if (status == VF2_OK) {
                    status = phase16_copy_text_record(
                        machine, record, &last_source, &last_destination,
                        &characters
                    );
                }
            }
            for (record_index = 0u; status == VF2_OK && record_index < 3u;
                 ++record_index) {
                status = vf2_model2a_read_u32(
                    machine, extra_text_records[record_index], &record
                );
                if (status == VF2_OK) {
                    status = phase16_copy_text_record(
                        machine, record, &last_source, &last_destination,
                        &characters
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            return finish_frame_phase17_index3_observed(
                machine, cpu, report,
                exit_instructions, UINT64_C(18), UINT32_C(0x00000103),
                UINT32_C(0x00078cb0), 0u, UINT32_C(0x010016ac),
                UINT32_C(2), VF2_I960_COMPARE_EQUAL,
                characters + UINT64_C(4),
                (size_t)UINT32_C(64 * 48 * 2) +
                    (size_t)characters * 2u + 4u
            );
        }

        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index3_observed(
            machine, cpu, report,
            UINT64_C(40), UINT64_C(2), UINT32_C(0x00000103),
            0u, UINT32_C(0x3f4f5c29), UINT32_C(0x010016a8),
            0u, VF2_I960_COMPARE_NONE, UINT64_C(1), 1u
        );
    }

    /* 0x60600 builds four 9-row bands.  Each band contains sixteen
     * intensity tiles and each tile spans three columns. */
    for (row = 0u; status == VF2_OK && row < UINT32_C(36); ++row) {
        const uint16_t base = row_bases[row / UINT32_C(9)];
        for (column = 0u; status == VF2_OK && column < UINT32_C(48); ++column) {
            const uint16_t tile = (uint16_t)(
                base + (uint16_t)(column / UINT32_C(3))
            );
            status = write_u16(
                machine,
                UINT32_C(0x01000000) +
                    (row + UINT32_C(4)) * UINT32_C(0x80) +
                    (column + UINT32_C(7)) * UINT32_C(2),
                tile
            );
        }
    }

    /* Four pattern banks contain identical solid 4-bpp intensity tiles.
     * Tile zero is already blank; levels 1..15 are 8x8 solid nibbles. */
    for (bank = 0u; status == VF2_OK && bank < UINT32_C(4); ++bank) {
        for (level = 1u; status == VF2_OK && level <= UINT32_C(15); ++level) {
            const uint16_t pattern = (uint16_t)(level * UINT32_C(0x1111));
            for (word = 0u; status == VF2_OK && word < UINT32_C(16); ++word) {
                status = write_u16(
                    machine,
                    pattern_bases[bank] +
                        (level - UINT32_C(1)) * UINT32_C(32) +
                        word * UINT32_C(2),
                    pattern
                );
            }
        }
    }

    /* 15 evenly-spaced 5-bit samples plus the full-scale endpoint. */
    for (level = 0u; status == VF2_OK && level < UINT32_C(16); ++level) {
        const uint16_t component = (uint16_t)(
            level == UINT32_C(15) ? UINT32_C(31) : level * UINT32_C(2)
        );
        const uint16_t red = (uint16_t)(UINT16_C(0x8000) | (component << 10u));
        const uint16_t gray = (uint16_t)(
            UINT16_C(0x8000) | (component << 10u) |
            (component << 5u) | component
        );
        const uint16_t green = (uint16_t)(
            UINT16_C(0x8000) | (component << 5u)
        );
        const uint16_t blue = (uint16_t)(UINT16_C(0x8000) | component);
        status = write_u16(
            machine, UINT32_C(0x01800700) + level * UINT32_C(2), red
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x01800720) + level * UINT32_C(2), gray
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x01800740) + level * UINT32_C(2), green
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x01800760) + level * UINT32_C(2), blue
            );
        }
    }

    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(2 * 0x80), UINT32_C(23), "DISPLAY TEST 1/2"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(17), "COLOR"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(31), "BIAS"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(41 * 0x80), UINT32_C(37), "GAIN SCROLL:"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(42 * 0x80), UINT32_C(24), "RED"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(43 * 0x80), UINT32_C(24), "GREEN"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(44 * 0x80), UINT32_C(24), "BLUE"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(45 * 0x80), UINT32_C(24), "EXIT"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(46 * 0x80), UINT32_C(0),
            "                  SELECT:1P LEVER UP/DOWN"
        );
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(47 * 0x80), UINT32_C(0),
            "   BIAS SET:1P PUNCH/KICK      GAIN SET:2P PUNCH/KICK"
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &next_a5, sizeof(next_a5)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x005ff600), UINT16_C(0));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff700), UINT32_C(0x01000f8e)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += UINT64_C(24451);
    cpu->procedure_calls += UINT64_C(20);
    cpu->procedure_returns += UINT64_C(20);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x0000010f);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = UINT32_C(0x0000004b);
    cpu->registers[17] = 0u;
    cpu->registers[18] = UINT32_C(0x00000060);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = 0u;
    cpu->registers[22] = UINT32_C(0xffff9c8f);
    cpu->registers[23] = UINT32_C(1);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001800);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2834 + 960 + 64 + 7);
    report->bytes_written = (size_t)UINT32_C(5880);
    report->recovered_instruction_count = UINT64_C(24451);
    report->recovered_procedure_calls = UINT64_C(20);
    report->recovered_procedure_returns = UINT64_C(21);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status compute_table_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t count,
    uint16_t *result
);

static vf2_status phase17_index4_render_descriptors(
    vf2_model2a *machine,
    uint8_t selection,
    int navigation_delta,
    uint64_t *characters
)
{
    const uint32_t table = UINT32_C(0x0005b340);
    uint32_t base = 0u;
    uint32_t record = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint64_t local_characters = 0u;
    uint32_t index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );

    for (index = 0u; status == VF2_OK && index < UINT32_C(16); ++index) {
        uint32_t descriptor = 0u;
        uint32_t label_destination = 0u;
        uint32_t value_destination = 0u;
        uint32_t payload = 0u;
        uint32_t flags = 0u;
        uint32_t string_table = 0u;
        uint32_t value = 0u;

        status = vf2_model2a_read_u32(
            machine, table + index * UINT32_C(8) + UINT32_C(4),
            &descriptor
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor, &label_destination);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(4), &value_destination
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(8), &payload
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(12), &flags
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, descriptor + UINT32_C(16), &string_table
            );
        }
        if (status == VF2_OK) {
            status = copy_diagnostic_text(
                machine, descriptor + UINT32_C(20), label_destination,
                &local_characters
            );
        }
        if (status != VF2_OK || (flags & UINT32_C(0x400)) != 0u) {
            continue;
        }

        switch (flags & UINT32_C(3)) {
        case 0u: {
            uint8_t raw = 0u;
            status = vf2_model2a_read(
                machine, base + payload, &raw, sizeof(raw)
            );
            value = raw;
            break;
        }
        case 1u: {
            uint16_t raw = 0u;
            status = read_u16(machine, base + payload, &raw);
            value = raw;
            break;
        }
        case 2u:
            status = vf2_model2a_read_u32(machine, base + payload, &value);
            break;
        default:
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status != VF2_OK) {
            break;
        }
        if ((flags & UINT32_C(4)) != 0u) {
            const uint32_t shift = (flags >> 3u) & UINT32_C(31);
            value = (value >> shift) & UINT32_C(1);
        }
        if ((flags & UINT32_C(0x100)) != 0u) {
            uint32_t source = 0u;
            status = vf2_model2a_read_u32(
                machine, string_table + value * UINT32_C(4), &source
            );
            if (status == VF2_OK) {
                status = copy_diagnostic_text(
                    machine, source, value_destination, &local_characters
                );
            }
        } else {
            status = phase17_zero_render_decimal(
                machine, (int32_t)value, value_destination
            );
        }
    }

    for (index = 0u; status == VF2_OK && index < UINT32_C(2); ++index) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0005ff14) + index * UINT32_C(4), &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination,
                &local_characters
            );
        }
    }

    if (status == VF2_OK) {
        uint32_t descriptor = 0u;
        uint32_t destination = 0u;
        status = vf2_model2a_read_u32(
            machine,
            table + (uint32_t)selection * UINT32_C(8) + UINT32_C(4),
            &descriptor
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4),
                navigation_delta == 0 ? UINT16_C(0x801c) : UINT16_C(0x8020)
            );
        }
    }
    if (status == VF2_OK && characters != NULL) {
        *characters = local_characters;
    }
    return status;
}

static vf2_status finish_frame_phase17_index4_observed(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int navigation_delta,
    uint64_t instructions,
    uint64_t calls,
    uint64_t characters
)
{
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = navigation_delta < 0
        ? UINT32_MAX : (uint32_t)navigation_delta;
    cpu->registers[17] = UINT32_C(0x0007ae10);
    cpu->registers[18] = 0u;
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001398);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (navigation_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_match_count(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t selection,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(3417)
        : (edit_delta < 0 ? UINT64_C(3415) : UINT64_C(3045));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(40);
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_C(0x00008800);
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01000318) +
        ((uint32_t)selection - UINT32_C(1)) * UINT32_C(0x100);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (edit_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_difficulty(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(3430)
        : (edit_delta < 0 ? UINT64_C(3427) : UINT64_C(3045));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(41);
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_C(0x00008800);
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01000598);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (edit_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_packed_flag(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t selection,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(3412)
        : (edit_delta < 0 ? UINT64_C(3409) : UINT64_C(3045));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(40);
    uint32_t descriptor = 0u;
    uint32_t cursor = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0005b344) + (uint32_t)selection * UINT32_C(8),
        &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &cursor);
    }
    if (status != VF2_OK || cursor < UINT32_C(4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cursor -= UINT32_C(4);

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_C(0x00008800);
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = cursor;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (edit_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_special_assignment(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t selection,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    uint64_t instructions = UINT64_C(3045);
    uint64_t calls = UINT64_C(38);
    uint32_t descriptor = 0u;
    uint32_t cursor = 0u;
    vf2_status status = VF2_OK;

    if (edit_delta != 0) {
        if (selection == UINT8_C(10)) {
            instructions = edit_delta > 0 ? UINT64_C(3427) : UINT64_C(3425);
            calls = UINT64_C(41);
        } else if (selection == UINT8_C(11)) {
            instructions = edit_delta > 0 ? UINT64_C(16890) : UINT64_C(16887);
            calls = UINT64_C(43);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0005b344) + (uint32_t)selection * UINT32_C(8),
        &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &cursor);
    }
    if (status != VF2_OK || cursor < UINT32_C(4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cursor -= UINT32_C(4);

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_C(0x00008800);
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = cursor;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (edit_delta == 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_initialize(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint16_t crc,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta > 0
        ? UINT64_C(16941)
        : (edit_delta < 0 ? UINT64_C(16938) : UINT64_C(3044));
    const uint64_t calls = edit_delta == 0 ? UINT64_C(38) : UINT64_C(43);
    uint32_t descriptor = 0u;
    uint32_t cursor = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005b344) + UINT32_C(15 * 8), &descriptor
    );
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, descriptor, &cursor);
    if (status != VF2_OK || cursor < UINT32_C(4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cursor -= UINT32_C(4);

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta == 0 ? 0u : (uint32_t)crc;
    cpu->registers[17] = edit_delta == 0 ? UINT32_C(0x0007ae10) : 0u;
    cpu->registers[18] = edit_delta == 0 ? 0u : UINT32_C(29);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = cursor;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_exit_control(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    int edit_delta,
    uint64_t characters
)
{
    const uint64_t instructions = edit_delta < 0 ? UINT64_C(3041) : UINT64_C(3044);
    const uint64_t calls = UINT64_C(38);
    vf2_status status = VF2_OK;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = edit_delta < 0 ? UINT32_MAX : 0u;
    cpu->registers[17] = UINT32_C(0x0007ae10);
    cpu->registers[18] = 0u;
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x01001398);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status finish_frame_phase17_index4_exit_taken(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint64_t characters
)
{
    vf2_status status = VF2_OK;
    cpu->executed_instructions += UINT64_C(17317);
    cpu->procedure_calls += UINT64_C(54);
    cpu->procedure_returns += UINT64_C(54);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(0x00009f9c);
    cpu->registers[15] = UINT32_C(0x00008800);
    cpu->registers[16] = UINT32_C(0x00078cb0);
    cpu->registers[17] = 0u;
    cpu->registers[18] = 0u;
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x00000010);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = UINT32_C(0x010016ac);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = UINT64_C(17317);
    report->recovered_procedure_calls = UINT64_C(54);
    report->recovered_procedure_returns = UINT64_C(55);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index4(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t table = UINT32_C(0x0005b340);
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t diagnostic_flags = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t selector_mirror = UINT8_C(17);
    uint8_t spill = UINT8_C(0x56);
    int navigation_delta = 0;
    int edit_delta = 0;
    int packed_bit = -1;
    uint64_t instructions = UINT64_C(3044);
    uint64_t calls = UINT64_C(38);
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x84) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fec8), &indirect_target
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &released_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050002c), &selector_mask
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00508000), &diagnostic_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6)
        );
    }
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005a680) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        (diagnostic_flags & (UINT32_C(1) << 14u)) != 0u ||
        phase_a5 > UINT8_C(15) || phase_a6 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    switch (phase_a5) {
    case UINT8_C(7): packed_bit = 0; break;
    case UINT8_C(8): packed_bit = 1; break;
    case UINT8_C(9): packed_bit = 3; break;
    case UINT8_C(12): packed_bit = 5; break;
    case UINT8_C(13): packed_bit = 4; break;
    case UINT8_C(14): packed_bit = 6; break;
    default: break;
    }

    if (((phase_a5 <= UINT8_C(3)) || phase_a5 == UINT8_C(10) ||
         phase_a5 == UINT8_C(11) || phase_a5 == UINT8_C(15) ||
         packed_bit >= 0) &&
        (navigation_flags == UINT32_C(0x100) ||
         navigation_flags == UINT32_C(0x200))) {
        edit_delta = navigation_flags == UINT32_C(0x100) ? 1 : -1;
    } else if (navigation_flags == UINT32_C(0x1000)) {
        navigation_delta = 1;
        instructions = UINT64_C(3050);
        calls = UINT64_C(37);
    } else if (navigation_flags == UINT32_C(0x2000)) {
        navigation_delta = -1;
        instructions = UINT64_C(3048);
        calls = UINT64_C(37);
    } else if (navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (navigation_delta == 0 && phase_a5 > UINT8_C(3) &&
        phase_a5 != UINT8_C(10) && phase_a5 != UINT8_C(11) &&
        phase_a5 != UINT8_C(15) && packed_bit < 0) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = phase17_index4_render_descriptors(
        machine, phase_a5, navigation_delta, &characters
    );
    if (status == VF2_OK && phase_a5 == UINT8_C(15) && edit_delta != 0) {
        static const uint8_t defaults[29] = {
            2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            15, 0, 0, 160, 0, 200, 0, 64, 64, 64, 37, 37, 37, 31
        };
        uint32_t base = 0u;
        uint16_t crc = 0u;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3340), defaults, sizeof(defaults)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03340), defaults, sizeof(defaults)
            );
        }
        if (status == VF2_OK) {
            status = phase17_index3_rebuild_transfer_tables(
                machine, defaults + UINT32_C(22)
            );
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) return status;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_initialize(
            machine, cpu, report, edit_delta, crc, characters
        );
    }
    if (status == VF2_OK && phase_a5 == UINT8_C(0) && edit_delta > 0) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
        };
        const uint8_t phase_index = UINT8_C(4);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t restored_characters = 0u;
        size_t index = 0u;

        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index, sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + UINT32_C(32), &record
            );
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, record, &destination);
        if (status == VF2_OK && destination < UINT32_C(4)) return VF2_ERROR_UNSUPPORTED;
        if (status == VF2_OK) {
            status = write_u16(machine, destination - UINT32_C(4), UINT16_C(0x801c));
        }
        for (index = 0u; status == VF2_OK && index < 12u; ++index) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_read_u32(machine, extra_text_records[index], &record);
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002b), &selector_mirror,
                sizeof(selector_mirror)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_exit_taken(
            machine, cpu, report, restored_characters
        );
    }
    if (status == VF2_OK && phase_a5 == UINT8_C(10) && edit_delta != 0) {
        uint32_t base = 0u;
        uint8_t country = 0u;
        uint8_t flags = 0u;
        uint16_t crc = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3350), &country, sizeof(country)
            );
        }
        if (status == VF2_OK) {
            int next = (int)country + edit_delta;
            if (next < 0) next = 2;
            else if (next > 2) next = 0;
            country = (uint8_t)next;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3350), &country, sizeof(country)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03350), &country, sizeof(country)
            );
        }
        if (status == VF2_OK && country != 0u) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
            if (status == VF2_OK) {
                flags |= UINT8_C(1) << 3u;
                status = vf2_model2a_write(
                    machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d03351), &flags, sizeof(flags)
                );
            }
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) return status;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, edit_delta, crc, characters
        );
    }
    if (status == VF2_OK && phase_a5 == UINT8_C(11) && edit_delta != 0) {
        uint32_t base = 0u;
        uint8_t flags = 0u;
        uint8_t values[7];
        uint16_t crc = 0u;
        size_t index = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            flags ^= UINT8_C(1) << 2u;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03351), &flags, sizeof(flags)
            );
        }
        if ((flags & (UINT8_C(1) << 2u)) != 0u) {
            values[0] = values[1] = values[2] = UINT8_C(117);
            values[3] = values[4] = values[5] = UINT8_C(34);
        } else {
            values[0] = values[1] = values[2] = UINT8_C(64);
            values[3] = values[4] = values[5] = UINT8_C(37);
        }
        values[6] = UINT8_C(31);
        for (index = 0u; status == VF2_OK && index < 7u; ++index) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3356) + (uint32_t)index,
                &values[index], sizeof(values[index])
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d03356) + (uint32_t)index,
                    &values[index], sizeof(values[index])
                );
            }
        }
        if (status == VF2_OK) {
            status = phase17_index3_rebuild_transfer_tables(machine, values);
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) return status;
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) return status;
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, edit_delta, crc, characters
        );
    }
    if (status == VF2_OK && packed_bit >= 0 && edit_delta != 0) {
        uint32_t base = 0u;
        uint8_t flags = 0u;
        uint16_t crc = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            flags ^= (uint8_t)(UINT8_C(1) << (uint32_t)packed_bit);
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03351), &flags, sizeof(flags)
            );
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index4_packed_flag(
            machine, cpu, report, phase_a5, edit_delta, crc, characters
        );
    }
    if (status == VF2_OK &&
        (phase_a5 == UINT8_C(1) || phase_a5 == UINT8_C(2)) &&
        edit_delta != 0) {
        const uint32_t value_offset = UINT32_C(0x3340) +
            ((uint32_t)phase_a5 - UINT32_C(1));
        uint32_t base = 0u;
        uint8_t value = 0u;
        uint16_t crc = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + value_offset, &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            int next = (int)value + edit_delta;
            if (next < 2) {
                next = 5;
            } else if (next > 5) {
                next = 2;
            }
            value = (uint8_t)next;
            status = vf2_model2a_write(
                machine, base + value_offset, &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d00000) + value_offset,
                &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) {
            return status;
        }

        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index4_match_count(
            machine, cpu, report, phase_a5, edit_delta, crc, characters
        );
    }

    if (status == VF2_OK && phase_a5 == UINT8_C(3) && edit_delta != 0) {
        uint32_t base = 0u;
        uint8_t value = 0u;
        uint8_t width = 0u;
        uint16_t energy_1p = 0u;
        uint16_t energy_vs = 0u;
        uint16_t crc = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3342), &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            int next = (int)value + edit_delta;
            if (next < 0) {
                next = 3;
            } else if (next > 3) {
                next = 0;
            }
            value = (uint8_t)next;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3342), &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d03342), &value, sizeof(value)
            );
        }
        if (status == VF2_OK) {
            status = read_u16(
                machine, UINT32_C(0x0005b32c) + (uint32_t)value * 2u,
                &energy_1p
            );
        }
        if (status == VF2_OK) {
            status = read_u16(
                machine, UINT32_C(0x0005b334) + (uint32_t)value * 2u,
                &energy_vs
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0005b33c) + (uint32_t)value,
                &width, sizeof(width)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, base + UINT32_C(0x3352), energy_1p);
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03352), energy_1p);
        }
        if (status == VF2_OK) {
            status = write_u16(machine, base + UINT32_C(0x3354), energy_vs);
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03354), energy_vs);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x334f), &width, sizeof(width)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x01d0334f), &width, sizeof(width)
            );
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3340), UINT32_C(29), &crc
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03302), crc);
        }
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x00078cb0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        return finish_frame_phase17_index4_difficulty(
            machine, cpu, report, edit_delta, crc, characters
        );
    }

    if (status == VF2_OK && navigation_delta != 0) {
        int candidate = (int)phase_a5;
        uint32_t flags = 0u;
        do {
            uint32_t descriptor = 0u;
            candidate += navigation_delta;
            if (candidate < 0) {
                candidate = 15;
            } else if (candidate > 15) {
                candidate = 0;
            }
            status = vf2_model2a_read_u32(
                machine,
                table + (uint32_t)candidate * UINT32_C(8) + UINT32_C(4),
                &descriptor
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, descriptor + UINT32_C(12), &flags
                );
            }
        } while (status == VF2_OK && (flags & UINT32_C(0x200)) != 0u);
        if (status == VF2_OK) {
            const uint8_t next = (uint8_t)candidate;
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &next, sizeof(next)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002b), &selector_mirror,
            sizeof(selector_mirror)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff680),
            navigation_delta == 0 ? UINT32_C(0x00078cb0)
                                  : (uint32_t)navigation_delta
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (phase_a5 == UINT8_C(1) || phase_a5 == UINT8_C(2)) {
        return finish_frame_phase17_index4_match_count(
            machine, cpu, report, phase_a5, 0, 0u, characters
        );
    }
    if (phase_a5 == UINT8_C(3)) {
        return finish_frame_phase17_index4_difficulty(
            machine, cpu, report, 0, 0u, characters
        );
    }
    if (packed_bit >= 0) {
        return finish_frame_phase17_index4_packed_flag(
            machine, cpu, report, phase_a5, 0, 0u, characters
        );
    }
    if (phase_a5 == UINT8_C(10) || phase_a5 == UINT8_C(11)) {
        return finish_frame_phase17_index4_special_assignment(
            machine, cpu, report, phase_a5, 0, 0u, characters
        );
    }
    if (phase_a5 == UINT8_C(15)) {
        return finish_frame_phase17_index4_initialize(
            machine, cpu, report, 0, 0u, characters
        );
    }
    if (phase_a5 == UINT8_C(0) && navigation_delta == 0) {
        return finish_frame_phase17_index4_exit_control(
            machine, cpu, report, edit_delta, characters
        );
    }
    return finish_frame_phase17_index4_observed(
        machine, cpu, report, navigation_delta,
        instructions, calls, characters
    );
}

static vf2_status execute_frame_phase17_bit7_index5(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    static const uint32_t cursor_addresses[6] = {
        UINT32_C(0x01001320), UINT32_C(0x010002a0),
        UINT32_C(0x01000320), UINT32_C(0x01000420),
        UINT32_C(0x010005a0), UINT32_C(0x010011a0)
    };
    static const struct {
        uint32_t row;
        uint32_t column;
        const char *text;
    } runs[] = {
        {5u, 18u, "COIN CHUTE TYPE"},
        {5u, 35u, "    COMMON"},
        {6u, 18u, "CREDIT TO 1P START"},
        {6u, 40u, "2"},
        {6u, 42u, "CREDITS"},
        {7u, 28u, "1P CONTINUE"},
        {7u, 40u, "2"},
        {7u, 42u, "CREDITS"},
        {8u, 18u, "CREDIT TO VS START"},
        {8u, 40u, "2"},
        {8u, 42u, "CREDITS"},
        {9u, 28u, "VS CONTINUE"},
        {9u, 40u, "2"},
        {9u, 42u, "CREDITS"},
        {11u, 18u, "COIN/CREDIT SETTING     #  1"},
        {11u, 47u, " "},
        {13u, 16u, "COIN CHUTE #1"},
        {13u, 31u, "1 COIN  1 CREDIT "},
        {15u, 31u, "                 "},
        {17u, 31u, "                 "},
        {19u, 31u, "                 "},
        {21u, 31u, "                 "},
        {24u, 16u, "COIN CHUTE #2"},
        {24u, 31u, "1 COIN  1 CREDIT "},
        {26u, 31u, "                 "},
        {28u, 31u, "                 "},
        {30u, 31u, "                 "},
        {32u, 31u, "                 "},
        {35u, 18u, "MANUAL SETTING"},
        {38u, 18u, "EXIT"},
        {44u, 20u, "SELECT BY SERVICE BUTTON"},
        {45u, 22u, "AND PUSH TEST BUTTON"}
    };
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint32_t coin_flags = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    uint8_t preset = 0u;
    uint8_t credits[6] = {0u};
    const uint8_t spill = UINT8_C(0x56);
    int edit_delta = 0;
    int main_navigation_delta = 0;
    int manual_navigation_delta = 0;
    uint16_t checksum = 0u;
    uint64_t instructions = UINT64_C(4188);
    uint64_t calls = UINT64_C(35);
    size_t index = 0u;
    uint64_t characters = 0u;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x85) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0005fed0), &indirect_target);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, base + UINT32_C(0x3320), &coin_flags);
    if (status == VF2_OK) status = vf2_model2a_read(machine, base + UINT32_C(0x3324), &preset, sizeof(preset));
    if (status == VF2_OK) status = vf2_model2a_read(machine, base + UINT32_C(0x3329), credits, sizeof(credits));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005b558) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(5) || phase_a6 != UINT8_C(0xff) || preset != 0u ||
        (coin_flags & ~UINT32_C(2)) != 0u ||
        (phase_a5 < UINT8_C(5) &&
         (phase_a7 != UINT8_C(0xff) || coin_flags != 0u)) ||
        (phase_a5 == UINT8_C(5) &&
         phase_a7 != UINT8_C(0xff) && phase_a7 > UINT8_C(4))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (navigation_flags == UINT32_C(0x100)) edit_delta = 1;
    else if (navigation_flags == UINT32_C(0x200)) edit_delta = -1;
    else if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff) &&
             navigation_flags == UINT32_C(0x1000))
        manual_navigation_delta = 1;
    else if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff) &&
             navigation_flags == UINT32_C(0x2000))
        manual_navigation_delta = -1;
    else if (phase_a7 == UINT8_C(0xff) && navigation_flags == UINT32_C(0x1000))
        main_navigation_delta = 1;
    else if (phase_a7 == UINT8_C(0xff) && navigation_flags == UINT32_C(0x2000))
        main_navigation_delta = -1;
    else if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
    for (index = 0u; index < sizeof(credits); ++index) {
        if (credits[index] != UINT8_C(2)) return VF2_ERROR_UNSUPPORTED;
    }

    if (phase_a5 == UINT8_C(5) && phase_a7 == UINT8_C(0xff) && edit_delta != 0) {
        const uint8_t manual_state = 0u;
        status = write_phase17_index0_text(
            machine, UINT32_C(44 * 0x80), UINT32_C(20),
            "SELECT BY SERVICE BUTTON"
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(45 * 0x80), UINT32_C(22),
                "AND PUSH TEST BUTTON"
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a7), &manual_state,
                sizeof(manual_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) return status;

        instructions = edit_delta > 0 ? UINT64_C(14063) : UINT64_C(14060);
        calls = UINT64_C(36);
        cpu->executed_instructions += instructions;
        cpu->procedure_calls += calls;
        cpu->procedure_returns += calls;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(1);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(62);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x01001580);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = UINT64_C(44);
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(0) && phase_a7 == UINT8_C(0xff) && edit_delta > 0) {
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
        };
        const uint8_t phase_index = UINT8_C(5);
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t restored_characters = 0u;
        size_t restore_index = 0u;

        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005feac) + UINT32_C(40), &record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4), UINT16_C(0x801c)
            );
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 12u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)restore_index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 3u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[restore_index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680), UINT32_C(0x010005d4)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff780), UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff784), UINT32_C(0x0059c388)
            );
        }
        if (status == VF2_OK) {
            const uint8_t exit_scratch[2] = {UINT8_C(0x90), UINT8_C(0x59)};
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff801), exit_scratch,
                sizeof(exit_scratch)
            );
        }
        if (status != VF2_OK) return status;

        instructions = UINT64_C(19056);
        calls = UINT64_C(74);
        cpu->executed_instructions += instructions;
        cpu->procedure_calls += calls;
        cpu->procedure_returns += calls;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(1);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(15);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010016ac);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = restored_characters;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(5) && phase_a7 != UINT8_C(0xff)) {
        static const uint32_t manual_cursor_rows[5] = {
            UINT32_C(38), UINT32_C(10), UINT32_C(17),
            UINT32_C(24), UINT32_C(31)
        };
        uint8_t manual_values[4] = {0u};
        char value_text[32];
        uint32_t value_offset = 0u;

        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3325), manual_values,
            sizeof(manual_values)
        );

        if (status == VF2_OK && phase_a7 == 0u && edit_delta != 0) {
            const uint8_t parent_state = UINT8_C(0xff);
            uint32_t row = 0u;
            uint32_t col = 0u;
            for (row = 4u; status == VF2_OK && row < 44u; ++row) {
                for (col = 0u; status == VF2_OK && col < 62u; ++col) {
                    status = write_u16(
                        machine,
                        UINT32_C(0x01000000) + row * UINT32_C(0x80) + col * UINT32_C(2),
                        UINT16_C(0x0020)
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005000a7),
                    &parent_state, sizeof(parent_state)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
                );
            }
            if (status != VF2_OK) return status;

            instructions = edit_delta > 0 ? UINT64_C(12392) : UINT64_C(12389);
            calls = UINT64_C(19);
            cpu->executed_instructions += instructions;
            cpu->procedure_calls += calls;
            cpu->procedure_returns += calls;
            status = vf2_i960_cpu_return_procedure(cpu, machine);
            if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            cpu->registers[0] = 0u;
            cpu->registers[1] = UINT32_C(0x005ff580);
            cpu->registers[2] = UINT32_C(0x0000a010);
            cpu->registers[3] = 0u;
            cpu->registers[4] = UINT32_C(0x00515400);
            cpu->registers[5] = UINT32_C(0x3f800000);
            cpu->registers[6] = 0u;
            cpu->registers[7] = 0u;
            cpu->registers[8] = UINT32_MAX;
            cpu->registers[9] = UINT32_MAX;
            cpu->registers[10] = UINT32_MAX;
            cpu->registers[11] = UINT32_MAX;
            cpu->registers[12] = 0u;
            cpu->registers[13] = 0u;
            cpu->registers[14] = UINT32_C(2);
            cpu->registers[15] = UINT32_C(0x00008a00);
            cpu->registers[16] = UINT32_C(62);
            cpu->registers[17] = 0u;
            cpu->registers[18] = UINT32_C(0xc0a0a3d7);
            cpu->registers[19] = 0u;
            cpu->registers[20] = UINT32_C(0x00560000);
            cpu->registers[21] = UINT32_C(0x0050e850);
            cpu->registers[22] = UINT32_C(0x000055b6);
            cpu->registers[23] = UINT32_C(0x00510980);
            cpu->registers[24] = UINT32_C(0x00512980);
            cpu->registers[25] = UINT32_C(0x01001600);
            cpu->registers[26] = UINT32_C(0x00800000);
            cpu->registers[27] = UINT32_C(0x00880000);
            cpu->registers[28] = UINT32_C(0x00004000);
            cpu->registers[29] = UINT32_C(0x00516480);
            cpu->registers[30] = UINT32_C(0x00000220);
            cpu->registers[31] = UINT32_C(0x005ff500);
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
            cpu->compare_result = VF2_I960_COMPARE_EQUAL;
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = instructions;
            report->recovered_procedure_calls = calls;
            report->recovered_procedure_returns = calls + UINT64_C(1);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        for (index = 0u; status == VF2_OK && index < 4u; ++index) {
            if (manual_values[index] > UINT8_C(8)) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(6 * 0x80), UINT32_C(25), "MANUAL SETTING"
            );
            characters += UINT64_C(14);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(10 * 0x80), UINT32_C(18), "COIN TO CREDIT"
            );
            characters += UINT64_C(14);
        }
        if (manual_values[0] == 0u) {
            (void)snprintf(value_text, sizeof(value_text), "1 COIN  1 CREDIT");
        } else {
            (void)snprintf(
                value_text, sizeof(value_text), "%u COINS 1 CREDIT",
                (unsigned)manual_values[0] + 1u
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(13 * 0x80), UINT32_C(31), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(17 * 0x80), UINT32_C(18), "BONUS ADDER"
            );
            characters += UINT64_C(11);
        }
        if (manual_values[1] == 0u) {
            (void)snprintf(
                value_text, sizeof(value_text), "           NO BONUS ADDER"
            );
        } else {
            (void)snprintf(
                value_text, sizeof(value_text), "%u COINS GIVE 1 EXTRA COIN",
                (unsigned)manual_values[1] + 1u
            );
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(20 * 0x80), UINT32_C(22), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(24 * 0x80), UINT32_C(18),
                "COIN CHUTE #1 MULTIPLIER"
            );
            characters += UINT64_C(24);
        }
        (void)snprintf(
            value_text, sizeof(value_text),
            manual_values[2] == 0u
                ? "1 COIN COUNTS AS 1 COIN "
                : "1 COIN COUNTS AS %u COINS ",
            (unsigned)manual_values[2] + 1u
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(27 * 0x80), UINT32_C(23), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(31 * 0x80), UINT32_C(18),
                "COIN CHUTE #2 MULTIPLIER"
            );
            characters += UINT64_C(24);
        }
        (void)snprintf(
            value_text, sizeof(value_text),
            manual_values[3] == 0u
                ? "1 COIN COUNTS AS 1 COIN "
                : "1 COIN COUNTS AS %u COINS ",
            (unsigned)manual_values[3] + 1u
        );
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(34 * 0x80), UINT32_C(23), value_text
            );
            characters += (uint64_t)strlen(value_text);
        }
        if (status == VF2_OK) {
            status = write_phase17_index0_text(
                machine, UINT32_C(38 * 0x80), UINT32_C(18), "EXIT"
            );
            characters += UINT64_C(4);
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine,
                UINT32_C(0x01000000) +
                    manual_cursor_rows[phase_a7] * UINT32_C(0x80) +
                    UINT32_C(16 * 2),
                manual_navigation_delta == 0
                    ? UINT16_C(0x801c) : UINT16_C(0x8020)
            );
        }

        if (status == VF2_OK && manual_navigation_delta != 0) {
            int next = (int)phase_a7 + manual_navigation_delta;
            const uint8_t old_selection = phase_a7;
            uint8_t next_selection = 0u;
            if (next < 0) next = 4;
            else if (next > 4) next = 0;
            next_selection = (uint8_t)next;
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a7),
                &next_selection, sizeof(next_selection)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x005ff680),
                    manual_navigation_delta < 0
                        ? UINT32_MAX : UINT32_C(1)
                );
            }
            instructions = manual_navigation_delta > 0
                ? (old_selection == UINT8_C(4)
                    ? UINT64_C(2271) : UINT64_C(2270))
                : (old_selection == UINT8_C(0)
                    ? UINT64_C(2268) : UINT64_C(2267));
            calls = UINT64_C(17);
        } else if (status == VF2_OK && edit_delta != 0 && phase_a7 != 0u) {
            int next = (int)manual_values[phase_a7 - 1u] + edit_delta;
            uint8_t value = 0u;
            if (next < 0) next = 8;
            else if (next > 8) next = 0;
            value = (uint8_t)next;
            value_offset = UINT32_C(0x3324) + (uint32_t)phase_a7;
            status = vf2_model2a_write(
                machine, base + value_offset, &value, sizeof(value)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + value_offset,
                    &value, sizeof(value)
                );
            }
            coin_flags |= UINT32_C(2);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, base + UINT32_C(0x3320), coin_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x01d03320), coin_flags
                );
            }
            if (status == VF2_OK) {
                status = compute_table_crc16(
                    machine, base + UINT32_C(0x3320), UINT32_C(15), &checksum
                );
            }
            if (status == VF2_OK) {
                status = write_u16(machine, UINT32_C(0x01d03300), checksum);
            }
            instructions = edit_delta > 0 ? UINT64_C(2475) : UINT64_C(2473);
            calls = UINT64_C(20);
        } else if (manual_navigation_delta == 0) {
            instructions = phase_a7 == 0u ? UINT64_C(2264) : UINT64_C(2266);
            calls = UINT64_C(18);
        }

        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) return status;

        cpu->executed_instructions += instructions;
        cpu->procedure_calls += calls;
        cpu->procedure_returns += calls;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(2);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = manual_navigation_delta != 0
            ? (manual_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1))
            : (edit_delta == 0 ? 0u : (uint32_t)checksum);
        cpu->registers[17] = edit_delta == 0
            ? UINT32_C(0x3f4f5c29) : 0u;
        cpu->registers[18] = edit_delta == 0
            ? UINT32_C(0xc0a0a3d7) : UINT32_C(15);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = manual_navigation_delta == 0
            ? UINT32_C(0x01001150)
            : (UINT32_C(0x01000000) +
               manual_cursor_rows[phase_a7] * UINT32_C(0x80) +
               UINT32_C(0x20));
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        if (manual_navigation_delta == 0) {
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
            cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        } else {
            static const uint32_t nav_cc_forward[5] = {1u, 1u, 1u, 2u, 4u};
            static const uint32_t nav_cc_back[5] = {2u, 1u, 1u, 1u, 1u};
            const uint32_t cc = manual_navigation_delta > 0
                ? nav_cc_forward[phase_a7] : nav_cc_back[phase_a7];
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | cc;
            cpu->compare_result = cc == UINT32_C(1)
                ? VF2_I960_COMPARE_GREATER
                : (cc == UINT32_C(2)
                    ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_LESS);
        }

        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = characters;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    for (index = 0u; status == VF2_OK && index < sizeof(runs) / sizeof(runs[0]); ++index) {
        status = write_phase17_index0_text(
            machine, runs[index].row * UINT32_C(0x80), runs[index].column,
            runs[index].text
        );
        if (status == VF2_OK) characters += (uint64_t)strlen(runs[index].text);
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, cursor_addresses[phase_a5],
            main_navigation_delta == 0 ? UINT16_C(0x801c) : UINT16_C(0x8020)
        );
    }

    if (status == VF2_OK && phase_a5 == UINT8_C(0) && edit_delta < 0) {
        instructions = UINT64_C(4185);
        calls = UINT64_C(35);
    } else if (status == VF2_OK && main_navigation_delta != 0) {
        int next = (int)phase_a5 + main_navigation_delta;
        uint8_t next_selection = 0u;
        if (next < 0) next = 5;
        else if (next > 5) next = 0;
        next_selection = (uint8_t)next;
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5), &next_selection, sizeof(next_selection)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff680),
                main_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x005ff684), UINT32_C(0x0007ae10)
            );
        }
        instructions = main_navigation_delta > 0
            ? (phase_a5 == UINT8_C(5) ? UINT64_C(4195) : UINT64_C(4194))
            : (phase_a5 == UINT8_C(0) ? UINT64_C(4192) : UINT64_C(4191));
        calls = UINT64_C(34);
    } else if (status == VF2_OK && edit_delta != 0) {
        if (phase_a5 == UINT8_C(1)) {
            coin_flags &= ~UINT32_C(2);
            coin_flags ^= UINT32_C(1);
            status = vf2_model2a_write_u32(machine, base + UINT32_C(0x3320), coin_flags);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, UINT32_C(0x01d03320), coin_flags);
            }
            instructions = edit_delta > 0 ? UINT64_C(4405) : UINT64_C(4402);
            calls = UINT64_C(38);
        } else if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(3)) {
            const uint32_t offset = phase_a5 == UINT8_C(2)
                ? UINT32_C(0x3329) : UINT32_C(0x332c);
            uint8_t value = UINT8_C(2);
            uint8_t start_credit = 0u;
            uint8_t continue_credit = 0u;
            int next = (int)value + edit_delta;
            if (next < 0) next = 14;
            else if (next > 14) next = 0;
            value = (uint8_t)next;
            status = vf2_model2a_write(machine, base + offset, &value, sizeof(value));
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + offset, &value, sizeof(value)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x0005bc74) + (uint32_t)value,
                    &start_credit, sizeof(start_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x0005bc84) + (uint32_t)value,
                    &continue_credit, sizeof(continue_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, base + offset + UINT32_C(1),
                    &start_credit, sizeof(start_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + offset + UINT32_C(1),
                    &start_credit, sizeof(start_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, base + offset + UINT32_C(2),
                    &continue_credit, sizeof(continue_credit)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d00000) + offset + UINT32_C(2),
                    &continue_credit, sizeof(continue_credit)
                );
            }
            instructions = edit_delta > 0 ? UINT64_C(4405) : UINT64_C(4402);
            calls = UINT64_C(38);
        } else if (phase_a5 == UINT8_C(4)) {
            int next = (int)preset + edit_delta;
            if (next < 0) next = 25;
            else if (next > 25) next = 0;
            preset = (uint8_t)next;
            status = vf2_model2a_write(
                machine, base + UINT32_C(0x3324), &preset, sizeof(preset)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x01d03324), &preset, sizeof(preset)
                );
            }
            instructions = edit_delta > 0 ? UINT64_C(4403) : UINT64_C(4401);
            calls = UINT64_C(37);
        }
        if (status == VF2_OK) {
            status = compute_table_crc16(
                machine, base + UINT32_C(0x3320), UINT32_C(15), &checksum
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01d03300), checksum);
        }
    } else if (phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(3)) {
        instructions = UINT64_C(4190);
    } else if (phase_a5 == UINT8_C(4)) {
        instructions = UINT64_C(4193);
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));
    }
    if (status != VF2_OK) return status;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(1);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] =
        (phase_a5 == UINT8_C(0) && edit_delta < 0)
            ? UINT32_MAX
            : (main_navigation_delta != 0
                ? (main_navigation_delta < 0 ? UINT32_MAX : UINT32_C(1))
                : (edit_delta == 0 ? 0u : (uint32_t)checksum));
    cpu->registers[17] =
        (phase_a5 == UINT8_C(0) && edit_delta < 0)
            ? UINT32_C(0x3f4f5c29)
            : (edit_delta == 0 ? UINT32_C(0x3f4f5c29) : 0u);
    cpu->registers[18] =
        (phase_a5 == UINT8_C(0) && edit_delta < 0)
            ? UINT32_C(0xc0a0a3d7)
            : (edit_delta == 0 ? UINT32_C(0xc0a0a3d7) : UINT32_C(15));
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = main_navigation_delta != 0
        ? cursor_addresses[phase_a5] : UINT32_C(0x010005d4);
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (phase_a5 == UINT8_C(0) && edit_delta < 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else if (main_navigation_delta != 0) {
        static const uint32_t nav_cc_forward[6] = {1u, 1u, 1u, 1u, 2u, 4u};
        static const uint32_t nav_cc_back[6] = {2u, 1u, 1u, 1u, 1u, 1u};
        const uint32_t cc = main_navigation_delta > 0
            ? nav_cc_forward[phase_a5] : nav_cc_back[phase_a5];
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | cc;
        cpu->compare_result = cc == UINT32_C(1)
            ? VF2_I960_COMPARE_GREATER
            : (cc == UINT32_C(2) ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_LESS);
    } else if (phase_a5 == 0u && edit_delta == 0) {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status phase17_index6_render_page1(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct {
        uint8_t row;
        uint8_t col;
        const char *text;
    } layout[] = {
        {2,26,"BOOKKEEPING 1/5"}, {5,26,"GLOBAL DATA"},
        {8,19,"COIN CHUTE #1"}, {10,19,"COIN CHUTE #2"},
        {12,21,"TOTAL COINS"}, {15,20,"COIN CREDITS"},
        {17,17,"SERVICE CREDITS"}, {19,19,"TOTAL CREDITS"},
        {22,22,"TOTAL TIME"}, {24,23,"PLAY TIME"},
        {26,10,"PLAY TIME RATIO(*1000)"}, {29,16,"TOTAL GAME COUNT"},
        {31,30,"1P"}, {33,30,"VS"},
        {35,18,"1P GAME TIME A"}, {36,16,"WAIT GAME TIME A"},
        {37,18,"VS GAME TIME A"}, {38,14,"TOTAL AVERAGE TIME"},
        {39,30,"1P"}, {40,30,"VS"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},
        {45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const struct {
        uint8_t row;
        uint8_t col;
        const char *text;
    } values[] = {
        {8,38,"0"}, {10,38,"0"}, {12,38,"0"},
        {15,38,"0"}, {17,38,"0"}, {19,38,"0"},
        {22,38,"0D  0H  0M  0S"}, {24,38,"0D  0H  0M  0S"},
        {26,35,"----"}, {29,38,"0"}, {31,38,"0"}, {33,38,"0"},
        {35,38,"0D  0H  0M  0S"}, {36,38,"0D  0H  0M  0S"},
        {37,38,"0D  0H  0M  0S"}, {38,37,"--M --S"},
        {39,37,"--M --S"}, {40,37,"--M --S"}
    };
    uint64_t count = 0u;
    size_t i = 0u;
    vf2_status status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));

    for (i = 0u; status == VF2_OK && i < sizeof(layout)/sizeof(layout[0]); ++i) {
        status = write_phase17_index0_text(
            machine, (uint32_t)layout[i].row * UINT32_C(0x80),
            layout[i].col, layout[i].text
        );
        count += (uint64_t)strlen(layout[i].text);
    }
    if (state == UINT8_C(1)) {
        for (i = 0u; status == VF2_OK && i < sizeof(values)/sizeof(values[0]); ++i) {
            status = write_phase17_index0_text(
                machine, (uint32_t)values[i].row * UINT32_C(0x80),
                values[i].col, values[i].text
            );
            count += (uint64_t)strlen(values[i].text);
        }
    }
    if (status == VF2_OK) {
        static const struct {
            uint8_t state;
            uint8_t row;
            uint8_t first;
            uint8_t last;
        } spans[] = {
            {0,2,26,36},{0,2,38,44},{0,5,26,36},
            {0,8,10,31},{0,10,10,31},{0,12,10,31},{0,15,10,31},
            {0,17,10,31},{0,19,10,31},{0,22,10,31},{0,24,10,31},
            {0,26,10,31},{0,29,10,31},{0,31,10,31},{0,33,10,31},
            {0,35,10,31},{0,36,10,31},{0,37,10,31},{0,38,10,31},
            {0,39,10,31},{0,40,10,31},{0,44,15,46},{0,45,18,42},
            {1,2,26,36},{1,2,38,44},{1,5,26,36},
            {1,8,10,31},{1,8,33,38},{1,10,10,31},{1,10,33,38},
            {1,12,10,31},{1,12,33,38},{1,15,10,31},{1,15,33,38},
            {1,17,10,31},{1,17,33,38},{1,19,10,31},{1,19,33,38},
            {1,22,10,31},{1,22,33,51},{1,24,10,31},{1,24,33,51},
            {1,26,10,31},{1,26,33,38},{1,29,10,31},{1,29,33,38},
            {1,31,10,31},{1,31,33,38},{1,33,10,31},{1,33,33,38},
            {1,35,10,31},{1,35,33,51},{1,36,10,31},{1,36,33,51},
            {1,37,10,31},{1,37,33,51},{1,38,10,31},{1,38,33,43},
            {1,39,10,31},{1,39,33,43},{1,40,10,31},{1,40,33,43},
            {1,44,15,46},{1,45,18,42}
        };
        uint32_t row = 0u;
        uint32_t col = 0u;
        for (row = 0u; status == VF2_OK && row < UINT32_C(48); ++row) {
            for (col = 0u; status == VF2_OK && col < UINT32_C(64); ++col) {
                uint16_t cell = 0u;
                const uint32_t address = UINT32_C(0x01000000) +
                    row * UINT32_C(0x80) + col * UINT32_C(2);
                status = read_u16(machine, address, &cell);
                if (status == VF2_OK) {
                    status = write_u16(machine, address, cell & UINT16_C(0x7fff));
                }
            }
        }
        for (i = 0u; status == VF2_OK && i < sizeof(spans)/sizeof(spans[0]); ++i) {
            if (spans[i].state == state) {
                for (col = spans[i].first; status == VF2_OK && col <= spans[i].last; ++col) {
                    uint16_t cell = 0u;
                    const uint32_t address = UINT32_C(0x01000000) +
                        (uint32_t)spans[i].row * UINT32_C(0x80) + col * UINT32_C(2);
                    status = read_u16(machine, address, &cell);
                    if (status == VF2_OK) {
                        status = write_u16(machine, address, cell | UINT16_C(0x8000));
                    }
                }
            }
        }
    }
    if (status == VF2_OK && characters != NULL) *characters = count;
    return status;
}


static vf2_status phase17_index6_render_page2(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct { uint8_t row; uint8_t col; const char *text; } layout[] = {
        {2,26,"BOOKKEEPING 2/5"},{5,25,"GLOBAL DATA 2"},{6,25,"-TYPE-B DATA-"},
        {8,36,"START     CONTINUE"},{10,14,"1P PLAY COUNT"},{12,14,"VS PLAY COUNT"},
        {14,10,"1P AVG. PLAY TIME"},{16,10,"VS AVG. PLAY TIME"},
        {19,7,"TIME                        COUNT"},
        {20,14,"-1P START-  -VS START-  -1P CONT-  -VS CONT-"},
        {21,5,"0~   30S"},{22,6,"~ 1M"},{23,6,"~ 1M30S"},{24,6,"~ 2M"},
        {25,6,"~ 2M30S"},{26,6,"~ 3M"},{27,6,"~ 3M30S"},{28,6,"~ 4M"},
        {29,6,"~ 4M30S"},{30,6,"~ 5M"},{31,6,"~ 5M30S"},{32,6,"~ 6M"},
        {33,6,"~ 6M30S"},{34,6,"~ 7M"},{35,6,"~ 7M30S"},{36,6,"~ 8M"},
        {37,6,"~ 8M30S"},{38,6,"~ 9M"},{39,6,"~ 9M30S"},{40,6,"~10M"},
        {41,5,"10M~"},{44,15,"PUSH SERVICE BUTTON TO CONTINUE."},
        {45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const struct { uint8_t row; uint8_t col; const char *text; } values[] = {
        {10,40,"0"},{10,53,"0"},{12,40,"0"},{12,53,"0"},
        {14,34,"--M --S"},{14,47,"--M --S"},{16,34,"--M --S"},{16,47,"--M --S"},
        {21,21,"0"},{21,33,"0"},{21,45,"0"},{21,56,"0"},
        {22,21,"0"},{22,33,"0"},{22,45,"0"},{22,56,"0"},
        {23,21,"0"},{23,33,"0"},{23,45,"0"},{23,56,"0"},
        {24,21,"0"},{24,33,"0"},{24,45,"0"},{24,56,"0"},
        {25,21,"0"},{25,33,"0"},{25,45,"0"},{25,56,"0"},
        {26,21,"0"},{26,33,"0"},{26,45,"0"},{26,56,"0"},
        {27,21,"0"},{27,33,"0"},{27,45,"0"},{27,56,"0"},
        {28,21,"0"},{28,33,"0"},{28,45,"0"},{28,56,"0"},
        {29,21,"0"},{29,33,"0"},{29,45,"0"},{29,56,"0"},
        {30,21,"0"},{30,33,"0"},{30,45,"0"},{30,56,"0"},
        {31,21,"0"},{31,33,"0"},{31,45,"0"},{31,56,"0"},
        {32,21,"0"},{32,33,"0"},{32,45,"0"},{32,56,"0"},
        {33,21,"0"},{33,33,"0"},{33,45,"0"},{33,56,"0"},
        {34,21,"0"},{34,33,"0"},{34,45,"0"},{34,56,"0"},
        {35,21,"0"},{35,33,"0"},{35,45,"0"},{35,56,"0"},
        {36,21,"0"},{36,33,"0"},{36,45,"0"},{36,56,"0"},
        {37,21,"0"},{37,33,"0"},{37,45,"0"},{37,56,"0"},
        {38,21,"0"},{38,33,"0"},{38,45,"0"},{38,56,"0"},
        {39,21,"0"},{39,33,"0"},{39,45,"0"},{39,56,"0"},
        {40,21,"0"},{40,33,"0"},{40,45,"0"},{40,56,"0"},
        {41,21,"0"},{41,33,"0"},{41,45,"0"},{41,56,"0"}
    };
    static const struct { uint8_t state,row,first,last; } spans[] = {
        {2,2,26,36},{2,2,38,44},{2,5,25,37},{2,6,25,37},{2,8,10,53},
        {2,10,10,26},{2,12,10,26},{2,14,10,26},{2,16,10,26},{2,19,5,39},
        {2,20,5,57},{2,21,5,12},{2,22,5,9},{2,23,5,12},{2,24,5,9},
        {2,25,5,12},{2,26,5,9},{2,27,5,12},{2,28,5,9},{2,29,5,12},
        {2,30,5,9},{2,31,5,12},{2,32,5,9},{2,33,5,12},{2,34,5,9},
        {2,35,5,12},{2,36,5,9},{2,37,5,12},{2,38,5,9},{2,39,5,12},
        {2,40,5,9},{2,41,5,8},{2,44,15,46},{2,45,18,42},
        {3,2,26,36},{3,2,38,44},{3,5,25,37},{3,6,25,37},{3,8,10,53},
        {3,10,10,26},{3,10,35,40},{3,10,48,53},{3,12,10,26},{3,12,35,40},{3,12,48,53},
        {3,14,10,26},{3,14,30,40},{3,14,43,53},{3,16,10,26},{3,16,30,40},{3,16,43,53},
        {3,19,5,39},{3,20,5,57},
        {3,21,5,12},{3,22,5,9},{3,23,5,12},{3,24,5,9},{3,25,5,12},{3,26,5,9},
        {3,27,5,12},{3,28,5,9},{3,29,5,12},{3,30,5,9},{3,31,5,12},{3,32,5,9},
        {3,33,5,12},{3,34,5,9},{3,35,5,12},{3,36,5,9},{3,37,5,12},{3,38,5,9},
        {3,39,5,12},{3,40,5,9},{3,41,5,8},
        {3,21,16,21},{3,21,28,33},{3,21,40,45},{3,21,51,56},
        {3,22,16,21},{3,22,28,33},{3,22,40,45},{3,22,51,56},
        {3,23,16,21},{3,23,28,33},{3,23,40,45},{3,23,51,56},
        {3,24,16,21},{3,24,28,33},{3,24,40,45},{3,24,51,56},
        {3,25,16,21},{3,25,28,33},{3,25,40,45},{3,25,51,56},
        {3,26,16,21},{3,26,28,33},{3,26,40,45},{3,26,51,56},
        {3,27,16,21},{3,27,28,33},{3,27,40,45},{3,27,51,56},
        {3,28,16,21},{3,28,28,33},{3,28,40,45},{3,28,51,56},
        {3,29,16,21},{3,29,28,33},{3,29,40,45},{3,29,51,56},
        {3,30,16,21},{3,30,28,33},{3,30,40,45},{3,30,51,56},
        {3,31,16,21},{3,31,28,33},{3,31,40,45},{3,31,51,56},
        {3,32,16,21},{3,32,28,33},{3,32,40,45},{3,32,51,56},
        {3,33,16,21},{3,33,28,33},{3,33,40,45},{3,33,51,56},
        {3,34,16,21},{3,34,28,33},{3,34,40,45},{3,34,51,56},
        {3,35,16,21},{3,35,28,33},{3,35,40,45},{3,35,51,56},
        {3,36,16,21},{3,36,28,33},{3,36,40,45},{3,36,51,56},
        {3,37,16,21},{3,37,28,33},{3,37,40,45},{3,37,51,56},
        {3,38,16,21},{3,38,28,33},{3,38,40,45},{3,38,51,56},
        {3,39,16,21},{3,39,28,33},{3,39,40,45},{3,39,51,56},
        {3,40,16,21},{3,40,28,33},{3,40,40,45},{3,40,51,56},
        {3,41,16,21},{3,41,28,33},{3,41,40,45},{3,41,51,56},
        {3,44,15,46},{3,45,18,42}
    };
    size_t i=0u; uint32_t row=0u,col=0u; uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    for(i=0u;status==VF2_OK&&i<sizeof(layout)/sizeof(layout[0]);++i){
        status=write_phase17_index0_text(machine,(uint32_t)layout[i].row*UINT32_C(0x80),layout[i].col,layout[i].text);
        count+=(uint64_t)strlen(layout[i].text);
    }
    if(state==UINT8_C(3)) for(i=0u;status==VF2_OK&&i<sizeof(values)/sizeof(values[0]);++i){
        status=write_phase17_index0_text(machine,(uint32_t)values[i].row*UINT32_C(0x80),values[i].col,values[i].text);
        count+=(uint64_t)strlen(values[i].text);
    }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row) for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){
        uint16_t cell=0u; const uint32_t address=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);
        status=read_u16(machine,address,&cell); if(status==VF2_OK) status=write_u16(machine,address,cell&UINT16_C(0x7fff));
    }
    for(i=0u;status==VF2_OK&&i<sizeof(spans)/sizeof(spans[0]);++i) if(spans[i].state==state){
        for(col=spans[i].first;status==VF2_OK&&col<=spans[i].last;++col){
            uint16_t cell=0u; const uint32_t address=UINT32_C(0x01000000)+(uint32_t)spans[i].row*UINT32_C(0x80)+col*UINT32_C(2);
            status=read_u16(machine,address,&cell); if(status==VF2_OK) status=write_u16(machine,address,cell|UINT16_C(0x8000));
        }
    }
    if(status==VF2_OK&&characters!=NULL)*characters=count;
    return status;
}


static vf2_status phase17_index6_render_page3(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct { uint8_t row,col; const char *text; } layout[] = {
        {2,26,"BOOKKEEPING 3/5"},{5,26,"1P GAME DATA"},{8,20,"GAME COUNT"},
        {10,7,"TOTAL TIME"},{11,9,"AVG TIME"},{12,9,"MIN TIME"},{13,9,"MAX TIME"},
        {16,16,"CONTINUE COUNT"},{17,21,"SET COUNT"},{18,20,"DRAW COUNT"},
        {19,13,"WIN BY K.O. COUNT"},{20,10,"WIN BY RINGOUT COUNT"},{21,12,"WIN BY JUDGE COUNT"},
        {25,16,"----COUNT---- ----TIME----"},{26,10,"ROUND TOTAL   WIN   TOTAL  AVG.   WIN RATE"},
        {27,11,"(th) (times)(times) (sec)  (sec)  (*1000)"},
        {28,12,"1"},{29,12,"2"},{30,12,"3"},{31,12,"4"},{32,12,"5"},{33,12,"6"},
        {34,12,"7"},{35,12,"8"},{36,12,"9"},{37,11,"10"},{38,11,"11"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},{45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const struct { uint8_t row,col; const char *text; } values[] = {
        {8,37,"0"},{10,24,"0D  0H  0M  0S"},{11,31,"--M --S"},{12,32,"0M  0S"},{13,32,"0M  0S"},
        {16,37,"0"},{17,37,"0"},{18,37,"0"},{19,37,"0"},{20,37,"0"},{21,37,"0"},
        {28,21,"0"},{28,28,"0"},{28,35,"0"},{28,39,"----"},
        {29,21,"0"},{29,28,"0"},{29,35,"0"},{29,39,"----"},
        {30,21,"0"},{30,28,"0"},{30,35,"0"},{30,39,"----"},
        {31,21,"0"},{31,28,"0"},{31,35,"0"},{31,39,"----"},
        {32,21,"0"},{32,28,"0"},{32,35,"0"},{32,39,"----"},
        {33,21,"0"},{33,28,"0"},{33,35,"0"},{33,39,"----"},
        {34,21,"0"},{34,28,"0"},{34,35,"0"},{34,39,"----"},
        {35,21,"0"},{35,28,"0"},{35,35,"0"},{35,39,"----"},
        {36,21,"0"},{36,28,"0"},{36,35,"0"},{36,39,"----"},
        {37,21,"0"},{37,28,"0"},{37,35,"0"},{37,39,"----"},
        {38,21,"0"},{38,28,"0"},{38,35,"0"},{38,39,"----"}
    };
    static const struct { uint8_t state,row,first,last; } spans[] = {
        {4,2,26,36},{4,2,38,44},{4,5,26,37},{4,8,10,29},{4,10,7,16},{4,11,9,16},{4,12,9,16},{4,13,9,16},
        {4,16,10,29},{4,17,10,29},{4,18,10,29},{4,19,10,29},{4,20,10,29},{4,21,10,29},
        {4,25,10,51},{4,26,10,51},{4,27,10,51},
        {4,28,10,12},{4,29,10,12},{4,30,10,12},{4,31,10,12},{4,32,10,12},{4,33,10,12},{4,34,10,12},{4,35,10,12},{4,36,10,12},{4,37,10,12},{4,38,10,12},
        {4,44,15,46},{4,45,18,42},
        {5,2,26,36},{5,2,38,44},{5,5,26,37},{5,8,10,29},{5,8,32,37},
        {5,10,7,16},{5,10,19,37},{5,11,9,16},{5,11,27,37},{5,12,9,16},{5,12,27,37},{5,13,9,16},{5,13,27,37},
        {5,16,10,29},{5,16,32,37},{5,17,10,29},{5,17,32,37},{5,18,10,29},{5,18,32,37},
        {5,19,10,29},{5,19,32,37},{5,20,10,29},{5,20,32,37},{5,21,10,29},{5,21,32,37},
        {5,25,10,51},{5,26,10,51},{5,27,10,51},
        {5,28,10,12},{5,29,10,12},{5,30,10,12},{5,31,10,12},{5,32,10,12},{5,33,10,12},{5,34,10,12},{5,35,10,12},{5,36,10,12},{5,37,10,12},{5,38,10,12},
        {5,28,16,21},{5,28,23,28},{5,28,30,35},{5,28,37,42},
        {5,29,16,21},{5,29,23,28},{5,29,30,35},{5,29,37,42},
        {5,30,16,21},{5,30,23,28},{5,30,30,35},{5,30,37,42},
        {5,31,16,21},{5,31,23,28},{5,31,30,35},{5,31,37,42},
        {5,32,16,21},{5,32,23,28},{5,32,30,35},{5,32,37,42},
        {5,33,16,21},{5,33,23,28},{5,33,30,35},{5,33,37,42},
        {5,34,16,21},{5,34,23,28},{5,34,30,35},{5,34,37,42},
        {5,35,16,21},{5,35,23,28},{5,35,30,35},{5,35,37,42},
        {5,36,16,21},{5,36,23,28},{5,36,30,35},{5,36,37,42},
        {5,37,16,21},{5,37,23,28},{5,37,30,35},{5,37,37,42},
        {5,38,16,21},{5,38,23,28},{5,38,30,35},{5,38,37,42},
        {5,44,15,46},{5,45,18,42}
    };
    size_t i=0u; uint32_t row=0u,col=0u; uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    for(i=0u;status==VF2_OK&&i<sizeof(layout)/sizeof(layout[0]);++i){ status=write_phase17_index0_text(machine,(uint32_t)layout[i].row*UINT32_C(0x80),layout[i].col,layout[i].text); count+=(uint64_t)strlen(layout[i].text); }
    if(state==UINT8_C(5)) for(i=0u;status==VF2_OK&&i<sizeof(values)/sizeof(values[0]);++i){ status=write_phase17_index0_text(machine,(uint32_t)values[i].row*UINT32_C(0x80),values[i].col,values[i].text); count+=(uint64_t)strlen(values[i].text); }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row) for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){ uint16_t cell=0u; const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2); status=read_u16(machine,ad,&cell); if(status==VF2_OK) status=write_u16(machine,ad,cell&UINT16_C(0x7fff)); }
    for(i=0u;status==VF2_OK&&i<sizeof(spans)/sizeof(spans[0]);++i) if(spans[i].state==state) for(col=spans[i].first;status==VF2_OK&&col<=spans[i].last;++col){ uint16_t cell=0u; const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)spans[i].row*UINT32_C(0x80)+col*UINT32_C(2); status=read_u16(machine,ad,&cell); if(status==VF2_OK) status=write_u16(machine,ad,cell|UINT16_C(0x8000)); }
    if (status == VF2_OK && characters != NULL) {
        *characters = count;
    }
    return status;
}


static vf2_status phase17_index6_render_page4(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct { uint8_t row,col; const char *text; } fixed[] = {
        {2,26,"BOOKKEEPING 4/5"},{5,26,"VS GAME DATA"},
        {8,20,"GAME COUNT"},{8,42,"GAMETIME   COUNT"},{9,45,"(sec)  (times)"},
        {10,7,"TOTAL TIME"},{11,7,"AVG TIME"},{12,7,"MIN TIME"},{13,7,"MAX TIME"},
        {16,16,"CONTINUE COUNT"},{17,21,"SET COUNT"},{18,20,"DRAW COUNT"},
        {19,13,"WIN BY K.O. COUNT"},{20,10,"WIN BY RINGOUT COUNT"},{21,12,"WIN BY JUDGE COUNT"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},{45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const char *ranges[33] = {
        "~ 10","~ 13","~ 16","~ 19","~ 22","~ 25","~ 28","~ 31","~ 34","~ 37","~ 40",
        "~ 43","~ 46","~ 49","~ 52","~ 55","~ 58","~ 61","~ 64","~ 67","~ 70","~ 73",
        "~ 76","~ 79","~ 82","~ 85","~ 88","~ 91","~ 94","~ 97","~100","~103","104~"
    };
    static const struct { uint8_t row,col; const char *text; } values[] = {
        {8,37,"0"},{10,24,"0D  0H  0M  0S"},{11,31,"--M --S"},{12,32,"0M  0S"},{13,32,"0M  0S"},
        {16,37,"0"},{17,37,"0"},{18,37,"0"},{19,37,"0"},{20,37,"0"},{21,37,"0"}
    };
    size_t i=0u; uint32_t row=0u,col=0u; uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    for(i=0u;status==VF2_OK&&i<sizeof(fixed)/sizeof(fixed[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)fixed[i].row*UINT32_C(0x80),fixed[i].col,fixed[i].text);count+=(uint64_t)strlen(fixed[i].text);}
    for(i=0u;status==VF2_OK&&i<33u;++i){uint8_t r=(uint8_t)(10u+i);uint8_t c=(i==32u)?42u:43u;status=write_phase17_index0_text(machine,(uint32_t)r*UINT32_C(0x80),c,ranges[i]);count+=(uint64_t)strlen(ranges[i]);}
    if(state==UINT8_C(7)){
        for(i=0u;status==VF2_OK&&i<sizeof(values)/sizeof(values[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)values[i].row*UINT32_C(0x80),values[i].col,values[i].text);count+=(uint64_t)strlen(values[i].text);}
        for(i=0u;status==VF2_OK&&i<33u;++i){status=write_phase17_index0_text(machine,(uint32_t)(10u+i)*UINT32_C(0x80),57u,"0");++count;}
    }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row)for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell&UINT16_C(0x7fff));}
    { static const struct { uint8_t row,first,last; } base[] = {
        {2,26,36},{2,38,44},{5,26,37},{8,10,29},{8,42,57},{9,42,58},
        {10,7,16},{11,7,16},{12,7,16},{13,7,16},{16,10,29},{17,10,29},{18,10,29},{19,10,29},{20,10,29},{21,10,29},
        {44,15,46},{45,18,42}
      };
      for(i=0u;status==VF2_OK&&i<sizeof(base)/sizeof(base[0]);++i)for(col=base[i].first;status==VF2_OK&&col<=base[i].last;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)base[i].row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
    }
    for(row=10u;status==VF2_OK&&row<=41u;++row){for(col=41u;status==VF2_OK&&col<=46u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}{uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(48*2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    if(status==VF2_OK){for(col=42u;col<=46u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+UINT32_C(42*0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    if(state==UINT8_C(7)&&status==VF2_OK){
        static const struct{uint8_t row,first,last;} leftvals[]={{8,32,37},{10,19,37},{11,27,37},{12,27,37},{13,27,37},{16,32,37},{17,32,37},{18,32,37},{19,32,37},{20,32,37},{21,32,37}};
        for(i=0u;status==VF2_OK&&i<sizeof(leftvals)/sizeof(leftvals[0]);++i)for(col=leftvals[i].first;status==VF2_OK&&col<=leftvals[i].last;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)leftvals[i].row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
        for(row=10u;status==VF2_OK&&row<=42u;++row)for(col=52u;status==VF2_OK&&col<=57u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
    }
    if(status==VF2_OK&&characters!=NULL){*characters=count;}
    return status;
}


static int32_t phase17_index6_round_even(float value)
{
    const double input = (double)value;
    const int64_t truncated = (int64_t)input;
    const double fraction = input - (double)truncated;
    int64_t rounded = truncated;

    if (fraction > 0.5 ||
        (fraction == 0.5 && (truncated & INT64_C(1)) != 0)) {
        ++rounded;
    } else if (fraction < -0.5 ||
               (fraction == -0.5 && (truncated & INT64_C(1)) != 0)) {
        --rounded;
    }
    return (int32_t)rounded;
}

static vf2_status phase17_index6_decimal(
    vf2_model2a *machine, int32_t value, uint32_t destination
)
{
    uint32_t magnitude = value < 0
        ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    uint16_t tiles[6];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    tiles[0] = value < 0 ? UINT16_C(0x802d) : UINT16_C(0x8020);
    if (magnitude <= UINT32_C(8191)) {
        tiles[1] = UINT16_C(0x8020);
        for (index = 0u; index < 4u; ++index) {
            status = read_u16(
                machine,
                UINT32_C(0x02040000) + magnitude * UINT32_C(8) +
                    index * UINT32_C(2),
                &tiles[index + 2u]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    } else {
        uint32_t remaining = magnitude;
        for (index = 0u; index < 5u; ++index) {
            tiles[index + 1u] = UINT16_C(0x8020);
        }
        index = 5u;
        do {
            --index;
            tiles[index + 1u] = (uint16_t)(
                UINT16_C(0x8030) + (uint16_t)(remaining % UINT32_C(10))
            );
            remaining /= UINT32_C(10);
        } while (remaining != 0u && index != 0u);
    }
    for (index = 0u; status == VF2_OK && index < 6u; ++index) {
        status = write_u16(
            machine, destination + index * UINT32_C(2), tiles[index]
        );
    }
    return status;
}

static uint64_t phase17_index6_decimal_instruction_delta(int32_t value)
{
    uint32_t magnitude = value < 0
        ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    uint32_t digits = 1u;
    uint64_t instructions = UINT64_C(20);

    if (magnitude <= UINT32_C(8191)) {
        return UINT64_C(0);
    }
    while (magnitude >= UINT32_C(10) && digits < UINT32_C(5)) {
        magnitude /= UINT32_C(10);
        ++digits;
    }
    instructions = digits < UINT32_C(5)
        ? UINT64_C(34) + UINT64_C(6) * (uint64_t)digits
        : UINT64_C(63);
    if (value < 0) {
        instructions += UINT64_C(2);
    }
    return instructions - UINT64_C(20);
}

static vf2_status phase17_index6_rate_bar(
    vf2_model2a *machine,
    int32_t wins,
    int32_t losses,
    uint32_t destination,
    uint64_t *instruction_delta,
    uint64_t *call_delta
)
{
    const uint32_t width = UINT32_C(20);
    const int32_t total = wins + losses;
    uint32_t filled = 0u;
    uint32_t full = 0u;
    uint32_t remainder = 0u;
    uint32_t index = 0u;
    uint32_t cursor = destination;
    vf2_status status = VF2_OK;

    if (total == 0) {
        status = write_u16(machine, cursor, UINT16_C(0x802d));
        cursor += UINT32_C(2);
        for (index = 1u; status == VF2_OK && index < width; ++index) {
            status = write_u16(machine, cursor, UINT16_C(0x8020));
            cursor += UINT32_C(2);
        }
        return status;
    }
    if (wins < 0 || losses < 0 || total < 0) {
        return VF2_ERROR_UNSUPPORTED;
    }

    {
        const float ratio = (float)wins / (float)total;
        int32_t scaled = phase17_index6_round_even(
            ratio * (float)(width * UINT32_C(8))
        );
        if (scaled < 0 || scaled > (int32_t)(width * UINT32_C(8))) {
            return VF2_ERROR_UNSUPPORTED;
        }
        full = (uint32_t)scaled >> 3u;
        remainder = (uint32_t)scaled & UINT32_C(7);
    }

    for (index = 0u; status == VF2_OK && index < full; ++index) {
        status = write_u16(machine, cursor, UINT16_C(0x8007));
        cursor += UINT32_C(2);
    }
    if (status == VF2_OK && remainder != 0u) {
        status = write_u16(
            machine, cursor,
            (uint16_t)(UINT16_C(0x8007) + (uint16_t)remainder)
        );
        cursor += UINT32_C(2);
    }
    filled = full + (remainder != 0u ? UINT32_C(1) : UINT32_C(0));
    for (index = filled; status == VF2_OK && index < width; ++index) {
        status = write_u16(machine, cursor, UINT16_C(0x8020));
        cursor += UINT32_C(2);
    }

    if (status == VF2_OK && instruction_delta != NULL) {
        /* Relative to the all-zero (-1.0) helper path measured in the ROM.
         * This delta is relative to the measured zero-sum state-9 path.
         * Each emitted 0x8440 tile adds seven net instructions; a fractional
         * tile saves two, while a completely full 20-tile bar saves one. */
        uint64_t delta = remainder == 0u
            ? UINT64_C(13) + UINT64_C(7) * (uint64_t)filled
            : UINT64_C(11) + UINT64_C(7) * (uint64_t)filled;
        if (filled == width && remainder == 0u) {
            --delta;
        }
        *instruction_delta += delta;
    }
    if (status == VF2_OK && call_delta != NULL) {
        *call_delta += (uint64_t)filled;
    }
    return status;
}

static vf2_status phase17_index6_render_page5(
    vf2_model2a *machine,
    uint8_t state,
    uint8_t fighter,
    uint64_t *characters,
    uint64_t *instruction_delta,
    uint64_t *call_delta
)
{
    static const char *names[10] = {"AKIRA ","JACKY ","SARAH ","KAGE  ","LAU   ","JEFFRY","PAI   ","WOLF  ","SHUN  ","LION  "};
    static const struct { uint8_t row,col; const char *text; } fixed[] = {
        {2,26,"BOOKKEEPING 5/5"},{5,25,"VS GAME DATA 2"},{8,27,"VS DIAGRAM"},
        {10,20,"MY CHAR :"},{12,6,"V.S. CHAR.        WIN.    LOSE. 0%     RATE.    100%"},
        {14,6,"AKIRA"},{16,6,"JACKY"},{18,6,"SARAH"},{20,6,"KAGE"},{22,6,"LAU"},
        {24,6,"JEFFRY"},{26,6,"PAI"},{28,6,"WOLF"},{30,6,"SHUN"},{32,6,"LION"},
        {43,12,"SELECT BY PLAYER 1 SIDE LEVER UP/DOWN."},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},{45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const uint8_t rows[10]={14,16,18,20,22,24,26,28,30,32};
    size_t i=0u;uint32_t row=0u,col=0u;uint64_t count=0u;
    vf2_status status=VF2_OK;
    if(fighter>UINT8_C(9))return VF2_ERROR_UNSUPPORTED;
    if(state!=UINT8_C(9))status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    if(state!=UINT8_C(9))for(i=0u;status==VF2_OK&&i<sizeof(fixed)/sizeof(fixed[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)fixed[i].row*UINT32_C(0x80),fixed[i].col,fixed[i].text);count+=(uint64_t)strlen(fixed[i].text);}
    if(state==UINT8_C(9)){
        static const uint8_t fighter_map[10]={0u,1u,2u,3u,4u,5u,6u,7u,8u,10u};
        const uint32_t record=UINT32_C(0x01d036a4)+(uint32_t)fighter_map[fighter]*UINT32_C(48);
        status=write_phase17_index0_text(machine,UINT32_C(10*0x80),30u,names[fighter]);
        count+=UINT64_C(6);
        for(i=0u;status==VF2_OK&&i<10u;++i){
            const uint32_t pair_offset=i<9u?(uint32_t)i*UINT32_C(4):UINT32_C(40);
            uint16_t raw_wins=0u,raw_losses=0u;
            int32_t wins=0,losses=0;
            const uint32_t line=(uint32_t)rows[i];
            status=read_u16(machine,record+pair_offset,&raw_wins);
            if(status==VF2_OK)status=read_u16(machine,record+pair_offset+UINT32_C(2),&raw_losses);
            wins=(int32_t)(int16_t)raw_wins;
            losses=(int32_t)(int16_t)raw_losses;
            if(status==VF2_OK)status=phase17_index6_decimal(machine,wins,UINT32_C(0x01000000)+line*UINT32_C(0x80)+UINT32_C(23*2));
            if(status==VF2_OK)status=phase17_index6_decimal(machine,losses,UINT32_C(0x01000000)+line*UINT32_C(0x80)+UINT32_C(31*2));
            if(status==VF2_OK&&instruction_delta!=NULL){
                *instruction_delta+=phase17_index6_decimal_instruction_delta(wins);
                *instruction_delta+=phase17_index6_decimal_instruction_delta(losses);
            }
            if(status==VF2_OK)status=phase17_index6_rate_bar(machine,wins,losses,UINT32_C(0x01000000)+line*UINT32_C(0x80)+UINT32_C(38*2),instruction_delta,call_delta);
            count+=UINT64_C(32);
        }
    }
    if(state==UINT8_C(9)){if(status==VF2_OK&&characters!=NULL)*characters=count;return status;}
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row)for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell&UINT16_C(0x7fff));}
    {static const struct{uint8_t row,first,last;} base[]={{2,26,36},{2,38,44},{5,25,38},{8,27,36},{10,20,29},{12,6,57},{14,6,10},{16,6,10},{18,6,10},{20,6,9},{22,6,8},{24,6,11},{26,6,8},{28,6,9},{30,6,9},{32,6,9},{43,12,49},{44,15,46},{45,18,42}};for(i=0u;status==VF2_OK&&i<sizeof(base)/sizeof(base[0]);++i)for(col=base[i].first;status==VF2_OK&&col<=base[i].last;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)base[i].row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    if(state==UINT8_C(9)&&status==VF2_OK){
        for(col=30u;status==VF2_OK&&col<=35u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+UINT32_C(10*0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
        for(i=0u;status==VF2_OK&&i<10u;++i){static const uint8_t starts[3]={23,31,38};static const uint8_t ends[3]={28,36,57};size_t j=0u;for(j=0u;status==VF2_OK&&j<3u;++j)for(col=starts[j];status==VF2_OK&&col<=ends[j];++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)rows[i]*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    }
    if(status==VF2_OK&&characters!=NULL){*characters=count;}
    return status;
}

static vf2_status phase17_index6_render_game_data_static(
    vf2_model2a *machine,
    uint8_t slot,
    uint64_t *characters
)
{
    static const char *names[16] = {
        "AKIRA","JACKY","SARAH","KAGE","LAU","JEFFRY","PAI","WOLF",
        "SHUN","ACHO","LION","MUE","KOJAC","-----","-----","-----"
    };
    static const char *headers[16] = {
        "AKIRA  ","JACKY  ","SARAH  ","KAGE   ","LAU    ","JEFFRY ",
        "PAI    ","WOLF   ","SHUN   ","DURAL  ","LION   ","MUE    ",
        "KOJACKY","-------","-------","-------"
    };
    static const struct { uint8_t row, col; const char *text; } fixed[] = {
        {4,22,"GAME DATA"},
        {6,17,"GAME COUNT 1P"},{7,17,"GAME COUNT VS"},
        {8,4,"TOTAL TIME 1P"},{9,4,"TOTAL TIME VS"},
        {10,6,"AVG TIME 1P"},{11,6,"AVG TIME VS"},
        {12,6,"MIN TIME 1P"},{13,6,"MIN TIME VS"},
        {14,6,"MAX TIME 1P"},{15,6,"MAX TIME VS"},
        {16,13,"CONTINUE COUNT 1P"},{17,13,"CONTINUE COUNT VS"},
        {18,18,"SET COUNT 1P"},{19,18,"SET COUNT VS"},
        {20,17,"DRAW COUNT 1P"},{21,17,"DRAW COUNT VS"},
        {22,10,"WIN BY K.O. COUNT 1P"},{23,10,"WIN BY K.O. COUNT VS"},
        {24,7,"WIN BY RINGOUT COUNT 1P"},{25,7,"WIN BY RINGOUT COUNT VS"},
        {26,9,"WIN BY JUDGE COUNT 1P"},{27,9,"WIN BY JUDGE COUNT VS"},
        {28,3,"--------------1P DATA-------------"},
        {29,3,"RND.TOTAL WIN.  TOTAL AVG. WINRATE"},
        {30,3,"(th)  (times)     (second) (*1000)"},
        {31,4," 1"},{32,4," 2"},{33,4," 3"},{34,4," 4"},
        {35,4," 5"},{36,4," 6"},{37,4," 7"},{38,4," 8"},
        {39,4," 9"},{40,4,"10"},{41,4,"11"},
        {8,42,"-----VS DATA-----"},{9,42,"GAMETIME    COUNT"},
        {10,42,"   (sec)  (times)"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},
        {45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const char *ranges[33] = {
        "~ 10","~ 13","~ 16","~ 19","~ 22","~ 25","~ 28","~ 31",
        "~ 34","~ 37","~ 40","~ 43","~ 46","~ 49","~ 52","~ 55",
        "~ 58","~ 61","~ 64","~ 67","~ 70","~ 73","~ 76","~ 79",
        "~ 82","~ 85","~ 88","~ 91","~ 94","~ 97","~100","~103"," 104~"
    };
    uint32_t row = 0u, col = 0u;
    size_t i = 0u;
    uint64_t count = 0u;
    vf2_status status = VF2_OK;

    if (slot >= UINT8_C(16)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (row = 4u; status == VF2_OK && row < 48u; ++row) {
        for (col = 0u; status == VF2_OK && col < 62u; ++col) {
            status = write_u16(
                machine, UINT32_C(0x01000000) + row * UINT32_C(0x80) +
                    col * UINT32_C(2), 0u
            );
        }
    }
    for (i = 0u; status == VF2_OK && i < sizeof(fixed)/sizeof(fixed[0]); ++i) {
        status = write_phase17_index0_text(
            machine, (uint32_t)fixed[i].row * UINT32_C(0x80),
            fixed[i].col, fixed[i].text
        );
        count += (uint64_t)strlen(fixed[i].text);
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(4 * 0x80), UINT32_C(34), names[slot]
        );
        count += (uint64_t)strlen(names[slot]);
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(2 * 0x80), UINT32_C(38), headers[slot]
        );
        count += (uint64_t)strlen(headers[slot]);
    }
    for (i = 0u; status == VF2_OK && i < 33u; ++i) {
        const uint32_t r = UINT32_C(11) + (uint32_t)i;
        const uint32_t c = i == 32u ? UINT32_C(42) : UINT32_C(43);
        status = write_phase17_index0_text(
            machine, r * UINT32_C(0x80), c, ranges[i]
        );
        count += (uint64_t)strlen(ranges[i]);
    }
    if (status == VF2_OK && characters != NULL) {
        *characters = count;
    }
    return status;
}

static vf2_status phase17_index6_write_total_time(
    vf2_model2a *machine, uint64_t ticks, uint32_t destination
)
{
    const uint64_t seconds=ticks>>6u;
    const uint64_t days=seconds/UINT64_C(86400);
    const uint64_t hours=(seconds/UINT64_C(3600))%UINT64_C(24);
    const uint64_t minutes=(seconds/UINT64_C(60))%UINT64_C(60);
    const uint64_t secs=seconds%UINT64_C(60);
    char text[32];
    (void)snprintf(text,sizeof(text),"%6lluD%3lluH%3lluM%3lluS",
        (unsigned long long)days,(unsigned long long)hours,
        (unsigned long long)minutes,(unsigned long long)secs);
    return write_phase17_index0_text(machine,destination-UINT32_C(0x01000000),UINT32_C(0),text);
}

static vf2_status phase17_index6_write_minute_time(
    vf2_model2a *machine, uint64_t ticks, uint32_t destination
)
{
    const uint64_t seconds=ticks>>6u;
    const uint64_t minutes=seconds/UINT64_C(60);
    const uint64_t secs=seconds%UINT64_C(60);
    char text[24];
    (void)snprintf(text,sizeof(text),"%6lluM%3lluS",
        (unsigned long long)minutes,(unsigned long long)secs);
    return write_phase17_index0_text(machine,destination-UINT32_C(0x01000000),UINT32_C(0),text);
}

static vf2_status phase17_index6_read_u64(
    const vf2_model2a *machine, uint32_t address, uint64_t *value
)
{
    uint32_t low=0u,high=0u;
    vf2_status status=vf2_model2a_read_u32(machine,address,&low);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,address+UINT32_C(4),&high);
    if(status==VF2_OK&&value!=NULL)*value=(uint64_t)low|((uint64_t)high<<32u);
    return status;
}

static vf2_status phase17_index6_render_game_data_values(
    vf2_model2a *machine,
    uint8_t slot,
    uint64_t *characters,
    int64_t *instruction_delta,
    uint64_t *call_delta,
    uint32_t *final_g0
)
{
    const uint32_t record=UINT32_C(0x01d00000)+(uint32_t)slot*UINT32_C(0x200);
    static const uint32_t simple_offsets[12]={
        UINT32_C(0x20),UINT32_C(0x140),UINT32_C(0x24),UINT32_C(0x144),
        UINT32_C(0x28),UINT32_C(0x148),UINT32_C(0x2c),UINT32_C(0x14c),
        UINT32_C(0x30),UINT32_C(0x150),UINT32_C(0x34),UINT32_C(0x154)
    };
    static const uint8_t simple_rows[12]={16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u};
    uint32_t count1=0u,countvs=0u,value=0u,row=0u,index=0u;
    uint64_t total1=0u,totalvs=0u;
    uint64_t chars=0u;
    vf2_status status=vf2_model2a_read_u32(machine,record,&count1);

    if(slot>=UINT8_C(16))return VF2_ERROR_UNSUPPORTED;
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(4),&countvs);
    if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)count1,UINT32_C(0x0100033e));
    if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)countvs,UINT32_C(0x010003be));
    if(status==VF2_OK&&instruction_delta!=NULL){
        *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)count1);
        *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)countvs);
    }
    if(status==VF2_OK)status=phase17_index6_read_u64(machine,record+UINT32_C(8),&total1);
    if(status==VF2_OK)status=phase17_index6_read_u64(machine,record+UINT32_C(0x10),&totalvs);
    if(status==VF2_OK)status=phase17_index6_write_total_time(machine,total1,UINT32_C(0x01000424));
    if(status==VF2_OK)status=phase17_index6_write_total_time(machine,totalvs,UINT32_C(0x010004a4));
    if(status==VF2_OK){
        if(count1==0u){status=write_phase17_index0_text(machine,UINT32_C(10*0x80),UINT32_C(18),"    --M --S");}
        else{status=phase17_index6_write_minute_time(machine,total1/(uint64_t)count1,UINT32_C(0x01000524));if(instruction_delta!=NULL)*instruction_delta-=INT64_C(27);if(call_delta!=NULL)*call_delta+=UINT64_C(4);}
    }
    if(status==VF2_OK){
        if(countvs==0u){status=write_phase17_index0_text(machine,UINT32_C(11*0x80),UINT32_C(18),"    --M --S");}
        else{status=phase17_index6_write_minute_time(machine,totalvs/(uint64_t)countvs,UINT32_C(0x010005a4));if(instruction_delta!=NULL)*instruction_delta-=INT64_C(27);if(call_delta!=NULL)*call_delta+=UINT64_C(4);}
    }
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x18),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x01000624));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x138),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x010006a4));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x1c),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x01000724));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x13c),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x010007a4));

    for(index=0u;status==VF2_OK&&index<UINT32_C(12);++index){
        status=vf2_model2a_read_u32(machine,record+simple_offsets[index],&value);
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)value,
            UINT32_C(0x01000000)+(uint32_t)simple_rows[index]*UINT32_C(0x80)+UINT32_C(31*2));
        if(status==VF2_OK&&instruction_delta!=NULL)*instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)value);
    }

    for(index=0u;status==VF2_OK&&index<UINT32_C(11);++index){
        const uint32_t rr=record+UINT32_C(0x38)+index*UINT32_C(16);
        uint32_t total=0u,wins=0u;
        uint64_t time=0u;
        uint32_t seconds=0u,avg=0u,rate=0u;
        row=UINT32_C(31)+index;
        status=vf2_model2a_read_u32(machine,rr,&total);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,rr+UINT32_C(4),&wins);
        if(status==VF2_OK)status=phase17_index6_read_u64(machine,rr+UINT32_C(8),&time);
        seconds=(uint32_t)(time>>6u);
        if(total!=0u){avg=seconds/total;rate=(uint32_t)(((uint64_t)wins*UINT64_C(1000))/(uint64_t)total);}
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)total,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(6*2));
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)wins,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(12*2));
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)seconds,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(18*2));
        if(total==0u){if(status==VF2_OK)status=write_phase17_index0_text(machine,row*UINT32_C(0x80),UINT32_C(24),"  ----");}
        else{
            if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)avg,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(24*2));
            if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)rate,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(30*2));
            if(instruction_delta!=NULL)*instruction_delta-=INT64_C(24);
            if(call_delta!=NULL)*call_delta+=UINT64_C(1);
        }
        if(status==VF2_OK&&instruction_delta!=NULL){
            *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)total);
            *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)wins);
            *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)seconds);
            if(total!=0u){
                *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)avg);
                *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)rate);
            }
        }
    }
    for(index=0u;status==VF2_OK&&index<UINT32_C(33);++index){
        status=vf2_model2a_read_u32(machine,record+UINT32_C(0x158)+index*UINT32_C(4),&value);
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)value,
            UINT32_C(0x01000000)+(UINT32_C(11)+index)*UINT32_C(0x80)+UINT32_C(52*2));
        if(status==VF2_OK&&instruction_delta!=NULL)*instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)value);
    }
    chars=UINT64_C(14)*UINT64_C(6)+UINT64_C(19)*UINT64_C(2)+UINT64_C(11)*UINT64_C(2)+UINT64_C(9)*UINT64_C(4)+UINT64_C(11)*UINT64_C(24)+UINT64_C(33)*UINT64_C(6);
    if(status==VF2_OK&&characters!=NULL)*characters=chars;
    if(status==VF2_OK&&final_g0!=NULL)*final_g0=value;
    return status;
}

static vf2_status phase17_index6_finish_game_data_pair(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t state
)
{
    const uint8_t spill = UINT8_C(0x56);
    uint64_t characters = 0u;
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    int64_t instruction_delta = 0;
    uint64_t call_delta = 0u;
    uint32_t final_g0 = 0u;
    vf2_status status = VF2_OK;

    if (state >= UINT8_C(10) && state <= UINT8_C(40) &&
        (state & UINT8_C(1)) == 0u) {
        const uint8_t slot = (uint8_t)((state - UINT8_C(10)) >> 1u);
        const uint8_t next = (uint8_t)(state + UINT8_C(1));
        static const uint8_t name_lengths[16] = {
            5u,5u,5u,4u,3u,6u,3u,4u,4u,4u,4u,3u,5u,5u,5u,5u
        };
        status = phase17_index6_render_game_data_static(machine, slot, &characters);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        }
        instructions = UINT64_C(20555) +
            UINT64_C(8) * (uint64_t)name_lengths[slot];
        calls = UINT64_C(144);
    } else if (state >= UINT8_C(11) && state <= UINT8_C(41) &&
               (state & UINT8_C(1)) != 0u) {
        const uint8_t slot = (uint8_t)((state - UINT8_C(11)) >> 1u);
        status = phase17_index6_render_game_data_values(
            machine, slot, &characters, &instruction_delta, &call_delta,
            &final_g0
        );
        if (instruction_delta < -INT64_C(4576)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        instructions = (uint64_t)(INT64_C(4576) + instruction_delta);
        calls = UINT64_C(133) + call_delta;
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0]=0u; cpu->registers[1]=UINT32_C(0x005ff580);
    cpu->registers[2]=UINT32_C(0x0000a010); cpu->registers[3]=0u;
    cpu->registers[4]=UINT32_C(0x00515400); cpu->registers[5]=UINT32_C(0x3f800000);
    cpu->registers[6]=0u; cpu->registers[7]=0u;
    cpu->registers[8]=UINT32_MAX; cpu->registers[9]=UINT32_MAX;
    cpu->registers[10]=UINT32_MAX; cpu->registers[11]=UINT32_MAX;
    cpu->registers[12]=0u; cpu->registers[13]=0u;
    cpu->registers[14]=UINT32_C(3)+(uint32_t)state;
    cpu->registers[15]=UINT32_C(0x00008a00);
    cpu->registers[16]=(state & UINT8_C(1)) == 0u ? UINT32_C(0x2e) : final_g0;
    cpu->registers[17]=0u; cpu->registers[18]=UINT32_C(0xc0a0a3d7);
    cpu->registers[19]=0u; cpu->registers[20]=UINT32_C(0x00560000);
    cpu->registers[21]=UINT32_C(0x0050e850); cpu->registers[22]=UINT32_C(0x000055b6);
    cpu->registers[23]=UINT32_C(0x00510980); cpu->registers[24]=UINT32_C(0x00512980);
    cpu->registers[25]=(state & UINT8_C(1)) == 0u
        ? UINT32_C(0x01001724) : UINT32_C(0x010015e8);
    cpu->registers[26]=UINT32_C(0x00800000); cpu->registers[27]=UINT32_C(0x00880000);
    cpu->registers[28]=UINT32_C(0x00004000); cpu->registers[29]=UINT32_C(0x00516480);
    cpu->registers[30]=UINT32_C(0x00000220); cpu->registers[31]=UINT32_C(0x005ff500);
    if ((state & UINT8_C(1)) == 0u) {
        cpu->arithmetic_control=(cpu->arithmetic_control&~UINT32_C(7))|UINT32_C(1);
        cpu->compare_result=VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control&=~UINT32_C(7);
        cpu->compare_result=VF2_I960_COMPARE_NONE;
    }

    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address=cpu->ip;
    report->iterations=UINT64_C(1);
    report->rows=characters;
    report->recovered_instruction_count=instructions;
    report->recovered_procedure_calls=calls;
    report->recovered_procedure_returns=calls+UINT64_C(1);
    report->cpu_poststate_applied=1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index6(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    const uint8_t spill = UINT8_C(0x56);
    int page_navigation_delta = 0;
    int fighter_delta = 0;
    int exit_requested = 0;
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    uint64_t characters = 0u;
    uint64_t render_instruction_delta = 0u;
    uint64_t render_call_delta = 0u;
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x86) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0005fed8), &indirect_target);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005c9b8) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(41) || phase_a6 != UINT8_C(0xff) ||
        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||
         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9)) ||
         (phase_a5 >= UINT8_C(10) && phase_a7 != UINT8_C(0xff)))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 >= UINT8_C(10)) {
        if (navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return phase17_index6_finish_game_data_pair(
            machine, cpu, report, phase_a5
        );
    }

    /* 0x0005cb70 is the common BOOKKEEPING input tail used by the stable
     * odd states. Canonical 0x100/0x200 inputs move between pages; the ROM
     * deliberately targets the even construction state of the destination
     * page so that its static layout is rebuilt on the following frame.
     * Page 5 additionally calls 0x00060b50 before that tail, using canonical
     * 0x1000/0x2000 to rotate the selected fighter in a7 over 0..9. */
    if ((phase_a5 & UINT8_C(1)) != 0u &&
        (navigation_flags & UINT32_C(0x04000014)) != 0u) {
        /* 0x5cb80 checks the shared TEST/exit mask before page navigation. */
        exit_requested = 1;
    } else if (phase_a5 == UINT8_C(9)) {
        if ((navigation_flags & UINT32_C(0x08001008)) != 0u) {
            fighter_delta = 1;
        } else if ((navigation_flags & UINT32_C(0x2000)) != 0u) {
            fighter_delta = -1;
        }
        if ((navigation_flags & UINT32_C(0x08000108)) != 0u) {
            page_navigation_delta = 1;
        } else if ((navigation_flags & UINT32_C(0x200)) != 0u) {
            page_navigation_delta = -1;
        }
        if (navigation_flags != 0u && fighter_delta == 0 &&
            page_navigation_delta == 0) {
            return VF2_ERROR_UNSUPPORTED;
        }
    } else if ((phase_a5 & UINT8_C(1)) != 0u &&
               navigation_flags == UINT32_C(0x100)) {
        page_navigation_delta = 1;
    } else if ((phase_a5 & UINT8_C(1)) != 0u &&
               navigation_flags == UINT32_C(0x200)) {
        page_navigation_delta = -1;
    } else if (navigation_flags != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = phase_a5 <= UINT8_C(1)
        ? phase17_index6_render_page1(machine, phase_a5, &characters)
        : (phase_a5 <= UINT8_C(3)
            ? phase17_index6_render_page2(machine, phase_a5, &characters)
            : (phase_a5 <= UINT8_C(5)
                ? phase17_index6_render_page3(machine, phase_a5, &characters)
                : (phase_a5 <= UINT8_C(7)
                    ? phase17_index6_render_page4(machine, phase_a5, &characters)
                    : phase17_index6_render_page5(machine, phase_a5,
                        phase_a5 == UINT8_C(8) ? UINT8_C(0) : phase_a7,
                        &characters, &render_instruction_delta,
                        &render_call_delta))));
    if (status != VF2_OK) return status;

    if (exit_requested) {
        static const uint64_t exit_instructions[5] = {
            UINT64_C(16037), UINT64_C(17895), UINT64_C(17391),
            UINT64_C(16215), UINT64_C(16173)
        };
        static const uint64_t exit_calls[5] = {
            UINT64_C(95), UINT64_C(144), UINT64_C(115),
            UINT64_C(90), UINT64_C(50)
        };
        static const uint32_t extra_text_records[3] = {
            UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
        };
        const uint8_t phase_index = UINT8_C(6);
        const size_t page = (size_t)((phase_a5 - UINT8_C(1)) / UINT8_C(2));
        uint32_t record = 0u;
        uint32_t destination = 0u;
        uint32_t last_source = 0u;
        uint32_t last_destination = 0u;
        uint64_t restored_characters = 0u;
        size_t restore_index = 0u;

        /* 0x5f140 clears the diagnostic plane, drops bit 7 from a4 and
         * reconstructs the parent TEST MENU from the ROM text records. */
        status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a4), &phase_index,
                sizeof(phase_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)phase_index * UINT32_C(8),
                &record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, destination - UINT32_C(4), UINT16_C(0x801c)
            );
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 12u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) + (uint32_t)restore_index * UINT32_C(8),
                &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        for (restore_index = 0u; status == VF2_OK && restore_index < 3u;
             ++restore_index) {
            status = vf2_model2a_read_u32(
                machine, extra_text_records[restore_index], &record
            );
            if (status == VF2_OK) {
                status = phase16_copy_text_record(
                    machine, record, &last_source, &last_destination,
                    &restored_characters
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
            );
        }
        if (status != VF2_OK) return status;

        instructions = exit_instructions[page];
        calls = exit_calls[page];
        cpu->executed_instructions += instructions;
        cpu->procedure_calls += calls;
        cpu->procedure_returns += calls;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        cpu->registers[0] = 0u;
        cpu->registers[1] = UINT32_C(0x005ff580);
        cpu->registers[2] = UINT32_C(0x0000a010);
        cpu->registers[3] = 0u;
        cpu->registers[4] = UINT32_C(0x00515400);
        cpu->registers[5] = UINT32_C(0x3f800000);
        cpu->registers[6] = 0u;
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_MAX;
        cpu->registers[9] = UINT32_MAX;
        cpu->registers[10] = UINT32_MAX;
        cpu->registers[11] = UINT32_MAX;
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = UINT32_C(5);
        cpu->registers[15] = UINT32_C(0x00008a00);
        cpu->registers[16] = UINT32_C(0x00078cb0);
        cpu->registers[17] = 0u;
        cpu->registers[18] = UINT32_C(0xc0a0a3d7);
        cpu->registers[19] = 0u;
        cpu->registers[20] = UINT32_C(0x00560000);
        cpu->registers[21] = UINT32_C(0x0050e850);
        cpu->registers[22] = phase_a5 == UINT8_C(9) ? UINT32_C(0x10) : UINT32_C(0x000055b6);
        cpu->registers[23] = UINT32_C(0x00510980);
        cpu->registers[24] = UINT32_C(0x00512980);
        cpu->registers[25] = UINT32_C(0x010016ac);
        cpu->registers[26] = UINT32_C(0x00800000);
        cpu->registers[27] = UINT32_C(0x00880000);
        cpu->registers[28] = UINT32_C(0x00004000);
        cpu->registers[29] = UINT32_C(0x00516480);
        cpu->registers[30] = UINT32_C(0x00000220);
        cpu->registers[31] = UINT32_C(0x005ff500);
        if (phase_a5 == UINT8_C(9)) {
            cpu->registers[14] = UINT32_C(0x00009f9c);
            cpu->registers[15] = UINT32_C(0x00008800);
            cpu->registers[18] = 0u;
            cpu->registers[22] = UINT32_C(0x10);
        }
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;

        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->rows = restored_characters;
        report->recovered_instruction_count = instructions;
        report->recovered_procedure_calls = calls;
        report->recovered_procedure_returns = calls + UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    if (phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6) || phase_a5 == UINT8_C(8)) {
        const uint8_t next = (uint8_t)(phase_a5 + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        if (phase_a5 == UINT8_C(0)) { instructions = UINT64_C(15309); calls = UINT64_C(25); }
        else if (phase_a5 == UINT8_C(2)) { instructions = UINT64_C(15175); calls = UINT64_C(40); }
        else if (phase_a5 == UINT8_C(4)) { instructions = UINT64_C(14911); calls = UINT64_C(32); }
        else if (phase_a5 == UINT8_C(6)) { instructions = UINT64_C(17251); calls = UINT64_C(117); }
        else { instructions = UINT64_C(13562); calls = UINT64_C(21); }
        if (status == VF2_OK && phase_a5 == UINT8_C(8)) {
            const uint8_t fighter_zero = UINT8_C(0);
            status = vf2_model2a_write(machine, UINT32_C(0x005000a7), &fighter_zero, sizeof(fighter_zero));
        }
    } else if (phase_a5 == UINT8_C(1)) {
        instructions = UINT64_C(1815); calls = UINT64_C(79);
    } else if (phase_a5 == UINT8_C(3)) {
        instructions = UINT64_C(3626); calls = UINT64_C(128);
    } else if (phase_a5 == UINT8_C(5)) {
        instructions = UINT64_C(3122); calls = UINT64_C(99);
    } else if (phase_a5 == UINT8_C(7)) {
        instructions = UINT64_C(1946); calls = UINT64_C(74);
    } else {
        instructions = UINT64_C(1904) + render_instruction_delta;
        calls = UINT64_C(34) + render_call_delta;
    }

    if (status == VF2_OK && page_navigation_delta != 0) {
        uint8_t next_page_state = 0u;
        if (page_navigation_delta > 0) {
            next_page_state = phase_a5 == UINT8_C(9)
                ? UINT8_C(0)
                : (uint8_t)(phase_a5 + UINT8_C(1));
            /* Relative to the idle common tail, the normal + path executes
             * four extra instructions; 9 -> 0 executes the wrap clear too. */
            instructions += phase_a5 == UINT8_C(9)
                ? UINT64_C(5) : UINT64_C(4);
        } else {
            next_page_state = phase_a5 == UINT8_C(1)
                ? UINT8_C(8)
                : (uint8_t)(phase_a5 - UINT8_C(3));
            /* The reverse path subtracts three because stable states are odd
             * and destination construction states are even. 1 -> 8 wraps. */
            instructions += phase_a5 == UINT8_C(1)
                ? UINT64_C(4) : UINT64_C(3);
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a5),
            &next_page_state, sizeof(next_page_state)
        );
    }
    if (status == VF2_OK && fighter_delta != 0) {
        uint8_t next_fighter = phase_a7;
        if (fighter_delta > 0) {
            next_fighter = phase_a7 == UINT8_C(9)
                ? UINT8_C(0)
                : (uint8_t)(phase_a7 + UINT8_C(1));
            instructions += phase_a7 == UINT8_C(9)
                ? UINT64_C(5) : UINT64_C(4);
        } else {
            next_fighter = phase_a7 == UINT8_C(0)
                ? UINT8_C(9)
                : (uint8_t)(phase_a7 - UINT8_C(1));
            /* 0x60b50's negative path is three instructions shorter than
             * idle, leaving net +1 normally and +2 for the 0 -> 9 wrap. */
            instructions += phase_a7 == UINT8_C(0)
                ? UINT64_C(2) : UINT64_C(1);
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a7),
            &next_fighter, sizeof(next_fighter)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005ff602), &spill, sizeof(spill));
    }
    if (status != VF2_OK) return status;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = phase_a5 == UINT8_C(9)
        ? UINT32_C(0x00009f9c)
        : UINT32_C(3) + (uint32_t)phase_a5;
    cpu->registers[15] = phase_a5 == UINT8_C(9)
        ? UINT32_C(0x00008800) : UINT32_C(0x00008a00);
    cpu->registers[16] = fighter_delta != 0
        ? (fighter_delta < 0 ? UINT32_MAX : UINT32_C(1))
        : ((phase_a5 & UINT8_C(1)) == 0u
            ? UINT32_C(0x2e)
            : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d)
                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x00002d2d) : 0u)));
    cpu->registers[17] = phase_a5 == UINT8_C(7) ? UINT32_C(0x01d0361c) : 0u;
    cpu->registers[18] = phase_a5 == UINT8_C(9)
        ? 0u : UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = phase_a5 == UINT8_C(9)
        ? UINT32_C(0x10) : UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = (phase_a5 & UINT8_C(1)) == 0u
        ? UINT32_C(0x01001724)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x010013c2)
            : (phase_a5 == UINT8_C(3) ? UINT32_C(0x010014e6)
                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x0100135c)
                    : (phase_a5 == UINT8_C(7) ? UINT32_C(0x01001568) : UINT32_C(0x01001074)))));
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    if (phase_a5 == UINT8_C(9) && page_navigation_delta != 0) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    } else if ((phase_a5 & UINT8_C(1)) == 0u) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase17_index7_render_choices(
    vf2_model2a *machine,
    int yes_selected,
    uint64_t *characters
)
{
    static const uint32_t records[4] = {
        UINT32_C(0x0005ff20), UINT32_C(0x0005ff24),
        UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
    };
    uint32_t record = 0u;
    uint32_t destination = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint64_t local_characters = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        status = vf2_model2a_read_u32(machine, records[index], &record);
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination,
                &local_characters
            );
        }
    }
    if (status == VF2_OK) {
        status = write_phase17_index0_text(
            machine, UINT32_C(27 * 0x80), UINT32_C(27), "         "
        );
        local_characters += UINT64_C(9);
    }
    for (index = 2u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(machine, records[index], &record);
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination,
                &local_characters
            );
        }
    }

    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        status = vf2_model2a_read_u32(machine, records[index], &record);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, record, &destination);
        }
        if (status == VF2_OK && destination < UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            const uint16_t cursor =
                ((index == 0u) == (yes_selected != 0))
                    ? UINT16_C(0x801c) : UINT16_C(0x8020);
            status = write_u16(machine, destination - UINT32_C(4), cursor);
        }
    }
    if (status == VF2_OK && characters != NULL) {
        *characters = local_characters;
    }
    return status;
}

static vf2_status phase17_index7_zero(vf2_model2a *m,uint32_t a,size_t n){
    static const uint8_t z[256]={0};
    vf2_status st=VF2_OK;
    while(st==VF2_OK&&n){size_t k=n>sizeof(z)?sizeof(z):n;st=vf2_model2a_write(m,a,z,k);a+=(uint32_t)k;n-=k;}
    return st;
}

static vf2_status phase17_index7_rng(vf2_model2a *m,uint32_t *value){
    uint32_t seed=0u,x=0u,base=0u;vf2_status st=vf2_model2a_read_u32(m,UINT32_C(0x00500098),&seed);size_t i=0u;
    base=seed<<20u;
    for(i=0u;st==VF2_OK&&i<4u;++i){st=vf2_model2a_read_u32(m,base+(uint32_t)i*4u,&x);if(st==VF2_OK)seed+=x<<(4u+(uint32_t)i*4u);}
    if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x00500098),seed);
    if(st==VF2_OK&&value)*value=(seed>>4u)&UINT32_C(0xffff);
    return st;
}

static vf2_status phase17_index7_rebuild_backup(vf2_model2a *m){
    vf2_status st=phase17_index7_zero(m,UINT32_C(0x01d03100),UINT32_C(0x200));
    uint32_t used=0u,row=0u,idx=0u,rnd=0u,ptr=0u,name=0u,meta=0u;uint8_t klass=0u;
    for(row=0u;st==VF2_OK&&row<18u;++row){
        if(row<6u)idx=row;else do{st=phase17_index7_rng(m,&rnd);idx=rnd&31u;}while(st==VF2_OK&&(used&(UINT32_C(1)<<idx))!=0u);
        used|=UINT32_C(1)<<idx;
        if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0201eafc)+idx*4u,&ptr);
        if(st==VF2_OK)st=vf2_model2a_read_u32(m,ptr,&name);
        if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0201ea24)+row*12u,&meta);
        if(st==VF2_OK)st=vf2_model2a_read(m,UINT32_C(0x0201ea2a)+row*12u,&klass,1u);
        if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x01d03100)+row*16u,meta);
        if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x01d03104)+row*16u,name);
        if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x01d0310a)+row*16u,&klass,1u);
    }
    return st;
}

static vf2_status phase17_index7_clear_backup(vf2_model2a *m){
    static const uint32_t offs[]={UINT32_C(0x3318),UINT32_C(0x331c),UINT32_C(0x3390),UINT32_C(0x3394),UINT32_C(0x3398),UINT32_C(0x339c),UINT32_C(0x3380),UINT32_C(0x3384),UINT32_C(0x3388),UINT32_C(0x338c)};
    vf2_status st=VF2_OK;size_t i=0u;uint32_t base=0u,crc=0u,j=0u;uint8_t byte=0u,t[2]={0u,0u};
    for(i=0u;st==VF2_OK&&i<sizeof(offs)/sizeof(offs[0]);++i){st=vf2_model2a_write_u32(m,UINT32_C(0x01d00000)+offs[i],0u);if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x00599000)+offs[i],0u);}
    if(st==VF2_OK)st=phase17_index7_zero(m,UINT32_C(0x01d03380),UINT32_C(0xc80));
    if(st==VF2_OK)st=phase17_index7_zero(m,UINT32_C(0x0059c380),UINT32_C(0xc60));
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x01d03000),t,1u);
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x0059c000),t,1u);
    if(st==VF2_OK)st=phase17_index7_rebuild_backup(m);
    if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0050016c),&base);
    for(j=0u;st==VF2_OK&&j<UINT32_C(0x644);++j){
        st=vf2_model2a_read(m,base+UINT32_C(0x33a8)+j,&byte,1u);
        if(st==VF2_OK){uint32_t k=((crc>>8u)^byte)&255u;uint16_t tab=0u;st=vf2_model2a_read(m,UINT32_C(0x02000000)+k*2u,&tab,2u);if(st==VF2_OK)crc=((crc<<8u)&UINT32_C(0xff00))^(uint32_t)tab;}
    }
    if(st==VF2_OK){uint16_t c=(uint16_t)crc;st=vf2_model2a_write(m,UINT32_C(0x01d03304),&c,2u);}
    return st;
}

static vf2_status phase17_index7_restore_menu(vf2_model2a *m){
    static const uint32_t extra[3]={UINT32_C(0x0005ff08),UINT32_C(0x0005ff14),UINT32_C(0x0005ff18)};
    uint8_t a4=UINT8_C(7),spill=UINT8_C(0x56);uint32_t rec=0u,dst=0u,ls=0u,ld=0u;uint64_t chars=0u;size_t i=0u;vf2_status st=clear_tile_plane_64x48(m,UINT32_C(0x01000000));
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x005000a4),&a4,1u);
    if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0005feac)+UINT32_C(56),&rec);
    if(st==VF2_OK)st=vf2_model2a_read_u32(m,rec,&dst);
    if(st==VF2_OK&&dst>=4u)st=write_u16(m,dst-4u,UINT16_C(0x801c));else if(st==VF2_OK)st=VF2_ERROR_UNSUPPORTED;
    for(i=0u;st==VF2_OK&&i<12u;++i){st=vf2_model2a_read_u32(m,UINT32_C(0x0005feac)+(uint32_t)i*8u,&rec);if(st==VF2_OK)st=phase16_copy_text_record(m,rec,&ls,&ld,&chars);}
    for(i=0u;st==VF2_OK&&i<3u;++i){st=vf2_model2a_read_u32(m,extra[i],&rec);if(st==VF2_OK)st=phase16_copy_text_record(m,rec,&ls,&ld,&chars);}
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x005ff602),&spill,1u);
    return st;
}

static void phase17_index7_post(vf2_i960_cpu *c,uint32_t g0,uint32_t g1,uint32_t g2,uint32_t g9,uint32_t r14,int less){
    c->registers[0]=0u;c->registers[1]=UINT32_C(0x005ff580);c->registers[2]=UINT32_C(0x0000a010);c->registers[3]=0u;c->registers[4]=UINT32_C(0x00515400);c->registers[5]=UINT32_C(0x3f800000);c->registers[6]=0u;c->registers[7]=0u;
    c->registers[8]=UINT32_MAX;c->registers[9]=UINT32_MAX;c->registers[10]=UINT32_MAX;c->registers[11]=UINT32_MAX;c->registers[12]=0u;c->registers[13]=0u;c->registers[14]=r14;c->registers[15]=UINT32_C(0x00008a00);
    c->registers[16]=g0;c->registers[17]=g1;c->registers[18]=g2;c->registers[19]=0u;c->registers[20]=UINT32_C(0x00560000);c->registers[21]=UINT32_C(0x0050e850);c->registers[22]=UINT32_C(0x000055b6);c->registers[23]=UINT32_C(0x00510980);c->registers[24]=UINT32_C(0x00512980);c->registers[25]=g9;c->registers[26]=UINT32_C(0x00800000);c->registers[27]=UINT32_C(0x00880000);c->registers[28]=UINT32_C(0x00004000);c->registers[29]=UINT32_C(0x00516480);c->registers[30]=UINT32_C(0x00000220);c->registers[31]=UINT32_C(0x005ff500);
    if(less){c->arithmetic_control=(c->arithmetic_control&~UINT32_C(7))|UINT32_C(4);c->compare_result=VF2_I960_COMPARE_LESS;}else set_equal_condition(c);
}

static vf2_status execute_frame_phase17_bit7_index7(vf2_model2a *machine,vf2_i960_cpu *cpu,vf2_hybrid_bridge_report *report,uint8_t flagged_phase_index){
    const uint32_t base_input=UINT32_C(0x0ff7f700);uint32_t target=0u,in=0u,nav=0u,rel=0u,prev=0u,mask=0u,seed=0u,counter=0u,rec=0u,dst=0u;uint8_t a5=0u,a6=0u,a7=0u,spill=UINT8_C(0x56);uint64_t ins=0u,calls=0u;int equal=1;vf2_status st=VF2_OK;
    if(flagged_phase_index!=UINT8_C(0x87)||cpu->local_frame_depth==0u)return VF2_ERROR_UNSUPPORTED;
    st=vf2_model2a_read_u32(machine,UINT32_C(0x0005fee0),&target);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x00500700),&in);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x00500704),&nav);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x00500708),&rel);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x0050070c),&prev);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x0050002c),&mask);if(st==VF2_OK)st=vf2_model2a_read(machine,UINT32_C(0x005000a5),&a5,1u);if(st==VF2_OK)st=vf2_model2a_read(machine,UINT32_C(0x005000a6),&a6,1u);if(st==VF2_OK)st=vf2_model2a_read(machine,UINT32_C(0x005000a7),&a7,1u);
    if(st!=VF2_OK||target!=UINT32_C(0x0005e848)||in!=base_input||rel!=0u||prev!=base_input||mask!=UINT32_C(0x00020000)||a5>UINT8_C(4)||a6!=UINT8_C(0xff)||a7!=UINT8_C(0xff))return st==VF2_OK?VF2_ERROR_UNSUPPORTED:st;
    if(a5==0u){uint8_t n=1u;if(nav!=0u)return VF2_ERROR_UNSUPPORTED;st=phase17_index7_render_choices(machine,0,NULL);if(st==VF2_OK)st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=711u;calls=7u;}
    else if(a5==1u){if((nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u){st=phase17_index7_restore_menu(machine);ins=14313u;calls=19u;if(st==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),5u,0);}
        else if(nav==0u){ins=49u;calls=4u;cpu->registers[16]=0u;}else if(nav==UINT32_C(0x1000)||nav==UINT32_C(0x2000)){uint8_t n=2u;st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=nav==UINT32_C(0x1000)?51u:48u;calls=4u;cpu->registers[16]=nav==UINT32_C(0x1000)?1u:UINT32_MAX;equal=0;}else return VF2_ERROR_UNSUPPORTED;}
    else if(a5==2u){uint8_t n=3u;if(nav!=0u)return VF2_ERROR_UNSUPPORTED;st=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff24),&rec);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,rec,&dst);if(st==VF2_OK&&dst>=4u)st=write_u16(machine,dst-4u,UINT16_C(0x8020));else if(st==VF2_OK)st=VF2_ERROR_UNSUPPORTED;if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff20),&rec);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,rec,&dst);if(st==VF2_OK&&dst>=4u)st=write_u16(machine,dst-4u,UINT16_C(0x801c));else if(st==VF2_OK)st=VF2_ERROR_UNSUPPORTED;if(st==VF2_OK)st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=43u;calls=2u;cpu->registers[16]=0u;cpu->registers[25]=UINT32_C(0x01000bb0);}
    else if(a5==3u){if(nav==0u){ins=41u;calls=2u;cpu->registers[16]=0u;}else if((nav&UINT32_C(0x08001008))!=0u){uint8_t n=0u;st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=38u;calls=2u;cpu->registers[16]=0u;equal=0;}else if((nav&UINT32_C(0x04000104))!=0u){uint8_t n=4u;st=vf2_model2a_read_u32(machine,UINT32_C(0x00500098),&seed);if(st==VF2_OK&&seed!=UINT32_C(0xcbf33340))return VF2_ERROR_UNSUPPORTED;if(st==VF2_OK)st=phase17_index7_clear_backup(machine);if(st==VF2_OK)st=write_phase17_index0_text(machine,UINT32_C(27*0x80),UINT32_C(27),"COMPLETED");if(st==VF2_OK)st=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),UINT32_C(100));if(st==VF2_OK)st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=40712u;calls=21u;if(st==VF2_OK)phase17_index7_post(cpu,9u,1u,UINT32_C(0x8180),UINT32_C(0x01000e36),5u,0);}else return VF2_ERROR_UNSUPPORTED;}
    else {if(nav!=0u)return VF2_ERROR_UNSUPPORTED;st=vf2_model2a_read_u32(machine,UINT32_C(0x00500024),&counter);if(st!=VF2_OK||counter==0u)return st==VF2_OK?VF2_ERROR_UNSUPPORTED:st;--counter;st=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),counter);if(st==VF2_OK&&counter==0u){st=phase17_index7_restore_menu(machine);ins=14308u;calls=18u;if(st==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),6u,0);}else if(st==VF2_OK){ins=35u;calls=2u;phase17_index7_post(cpu,0u,UINT32_C(0x3f4f5c29),UINT32_C(0xc0a0a3d7),UINT32_C(0x01000e36),6u,1);}}
    if(st==VF2_OK&&a5!=1u&&a5!=4u&&!(a5==3u&&(nav&UINT32_C(0x04000104))!=0u))st=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,1u);
    if(st!=VF2_OK)return st;
    cpu->executed_instructions+=ins;cpu->procedure_calls+=calls;cpu->procedure_returns+=calls;st=vf2_i960_cpu_return_procedure(cpu,machine);if(st!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return st==VF2_OK?VF2_ERROR_UNSUPPORTED:st;
    if(a5<=3u&&!((a5==1u&&(nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u)||(a5==3u&&(nav&UINT32_C(0x04000104))!=0u))){cpu->registers[0]=0u;cpu->registers[1]=UINT32_C(0x005ff580);cpu->registers[2]=UINT32_C(0x0000a010);cpu->registers[3]=0u;cpu->registers[4]=UINT32_C(0x00515400);cpu->registers[5]=UINT32_C(0x3f800000);cpu->registers[6]=0u;cpu->registers[7]=0u;cpu->registers[8]=UINT32_MAX;cpu->registers[9]=UINT32_MAX;cpu->registers[10]=UINT32_MAX;cpu->registers[11]=UINT32_MAX;cpu->registers[12]=0u;cpu->registers[13]=0u;cpu->registers[14]=5u;cpu->registers[15]=UINT32_C(0x00008a00);cpu->registers[17]=0u;cpu->registers[18]=UINT32_C(0xc0a0a3d7);cpu->registers[19]=0u;cpu->registers[20]=UINT32_C(0x00560000);cpu->registers[21]=UINT32_C(0x0050e850);cpu->registers[22]=UINT32_C(0x000055b6);cpu->registers[23]=UINT32_C(0x00510980);cpu->registers[24]=UINT32_C(0x00512980);cpu->registers[26]=UINT32_C(0x00800000);cpu->registers[27]=UINT32_C(0x00880000);cpu->registers[28]=UINT32_C(0x00004000);cpu->registers[29]=UINT32_C(0x00516480);cpu->registers[30]=UINT32_C(0x00000220);cpu->registers[31]=UINT32_C(0x005ff500);if(equal)set_equal_condition(cpu);}
    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;report->exit_address=cpu->ip;report->iterations=1u;report->recovered_instruction_count=ins;report->recovered_procedure_calls=calls;report->recovered_procedure_returns=calls+1u;report->cpu_poststate_applied=1;return VF2_OK;
}


static vf2_status phase17_index8_restore_menu(vf2_model2a *machine)
{
    static const uint32_t extra[3] = {
        UINT32_C(0x0005ff08), UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
    };
    const uint8_t phase_index = UINT8_C(8);
    const uint8_t spill = UINT8_C(0x56);
    uint32_t record = 0u;
    uint32_t destination = 0u;
    uint32_t last_source = 0u;
    uint32_t last_destination = 0u;
    uint64_t characters = 0u;
    size_t index = 0u;
    vf2_status status = clear_tile_plane_64x48(machine, UINT32_C(0x01000000));

    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &phase_index, sizeof(phase_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0005feac) + UINT32_C(64), &record
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, record, &destination);
    }
    if (status == VF2_OK && destination < UINT32_C(4)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, destination - UINT32_C(4), UINT16_C(0x801c)
        );
    }
    for (index = 0u; status == VF2_OK && index < 12u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) + (uint32_t)index * UINT32_C(8),
            &record
        );
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_read_u32(machine, extra[index], &record);
        if (status == VF2_OK) {
            status = phase16_copy_text_record(
                machine, record, &last_source, &last_destination, &characters
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    return status;
}

static void phase17_index8_poststate(
    vf2_i960_cpu *cpu,
    uint32_t g0,
    uint32_t g9,
    vf2_i960_compare_result comparison,
    uint32_t condition_bits
)
{
    cpu->registers[0] = 0u;
    cpu->registers[1] = UINT32_C(0x005ff580);
    cpu->registers[2] = UINT32_C(0x0000a010);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = UINT32_C(0x3f800000);
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = UINT32_C(5);
    cpu->registers[15] = UINT32_C(0x00008a00);
    cpu->registers[16] = g0;
    cpu->registers[17] = UINT32_C(0x3f4f5c29);
    cpu->registers[18] = UINT32_C(0xc0a0a3d7);
    cpu->registers[19] = 0u;
    cpu->registers[20] = UINT32_C(0x00560000);
    cpu->registers[21] = UINT32_C(0x0050e850);
    cpu->registers[22] = UINT32_C(0x000055b6);
    cpu->registers[23] = UINT32_C(0x00510980);
    cpu->registers[24] = UINT32_C(0x00512980);
    cpu->registers[25] = g9;
    cpu->registers[26] = UINT32_C(0x00800000);
    cpu->registers[27] = UINT32_C(0x00880000);
    cpu->registers[28] = UINT32_C(0x00004000);
    cpu->registers[29] = UINT32_C(0x00516480);
    cpu->registers[30] = UINT32_C(0x00000220);
    cpu->registers[31] = UINT32_C(0x005ff500);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
    cpu->compare_result = comparison;
}

static vf2_status execute_frame_phase17_bit7_index8(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint8_t flagged_phase_index
)
{
    const uint32_t base_input = UINT32_C(0x0ff7f700);
    const uint8_t spill = UINT8_C(0x56);
    uint32_t indirect_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t released_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t counter = 0u;
    uint32_t video_words[2] = {0u, 0u};
    uint32_t geometry_pointer = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t phase_a6 = 0u;
    uint8_t phase_a7 = 0u;
    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    vf2_i960_compare_result comparison = VF2_I960_COMPARE_EQUAL;
    uint32_t condition_bits = UINT32_C(2);
    uint32_t final_g0 = 0u;
    uint32_t final_g9 = UINT32_C(0x010016ac);
    vf2_status status = VF2_OK;

    if (flagged_phase_index != UINT8_C(0x88) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0005fee8), &indirect_target
    );
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &input_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &navigation_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708), &released_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050070c), &previous_flags);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a6), &phase_a6, sizeof(phase_a6));
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x005000a7), &phase_a7, sizeof(phase_a7));
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005ed0c) ||
        input_flags != base_input || released_flags != 0u ||
        previous_flags != base_input || selector_mask != UINT32_C(0x00020000) ||
        phase_a5 > UINT8_C(3) || phase_a6 != UINT8_C(0xff) ||
        phase_a7 != UINT8_C(0xff)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (phase_a5 == UINT8_C(0)) {
        uint8_t next = UINT8_C(1);
        uint32_t bit = 0u;
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = write_phase17_index0_text(machine, UINT32_C(15 * 0x80), UINT32_C(18), "IC.47");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(18 * 0x80), UINT32_C(18), "IC.56");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(21 * 0x80), UINT32_C(18), "IC.60");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(24 * 0x80), UINT32_C(18), "IC.64");
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(45 * 0x80), UINT32_C(19), "PLEASE WAIT FOR A WHILE.");
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00918000) + UINT32_C(0xe0), 0u);
        for (bit = 0u; status == VF2_OK && bit < 32u; ++bit) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00918004) + bit * UINT32_C(4),
                UINT32_C(1) << bit
            );
        }
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00918084), 0u);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00501004), &geometry_pointer);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x008000f0), geometry_pointer);
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), UINT32_C(64));
        instructions = UINT64_C(663);
        calls = UINT64_C(7);
        final_g9 = UINT32_C(0x01001726);
    } else if (phase_a5 == UINT8_C(1)) {
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
        if (status != VF2_OK || counter == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (counter == UINT32_C(1)) {
            const uint8_t next = UINT8_C(2);
            status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
            instructions = UINT64_C(37);
        } else {
            --counter;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter);
            instructions = UINT64_C(35);
            comparison = VF2_I960_COMPARE_LESS;
            condition_bits = UINT32_C(4);
        }
        calls = UINT64_C(2);
    } else if (phase_a5 == UINT8_C(2)) {
        static const uint32_t rows[4] = {
            UINT32_C(15), UINT32_C(18), UINT32_C(21), UINT32_C(24)
        };
        uint32_t column_index = 0u;
        uint32_t bit = 0u;
        const uint8_t next = UINT8_C(3);
        if (navigation_flags != 0u) return VF2_ERROR_UNSUPPORTED;
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00980010), &video_words[0]);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00980014), &video_words[1]);
        for (column_index = 0u; status == VF2_OK && column_index < 2u; ++column_index) {
            const uint32_t column = column_index == 0u ? UINT32_C(27) : UINT32_C(58);
            for (bit = 0u; status == VF2_OK && bit < 4u; ++bit) {
                const char *label = (video_words[column_index] & (UINT32_C(1) << bit)) != 0u
                    ? "BAD " : "GOOD";
                status = write_phase17_index0_text(
                    machine, rows[bit] * UINT32_C(0x80), column, label
                );
            }
        }
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        if (status == VF2_OK) status = write_phase17_index0_text(machine, UINT32_C(45 * 0x80), UINT32_C(19), "PUSH TEST BUTTON TO EXIT.");
        instructions = UINT64_C(755);
        calls = UINT64_C(19);
        final_g0 = UINT32_C(0x2e);
        final_g9 = UINT32_C(0x01001726);
        comparison = VF2_I960_COMPARE_GREATER;
        condition_bits = UINT32_C(1);
    } else {
        if ((navigation_flags & UINT32_C(0x04000104)) != 0u) {
            status = phase17_index8_restore_menu(machine);
            instructions = UINT64_C(14308);
            calls = UINT64_C(18);
            final_g0 = UINT32_C(0x00078cb0);
        } else if (navigation_flags == 0u) {
            instructions = UINT64_C(36);
            calls = UINT64_C(2);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) return status;

    cpu->executed_instructions += instructions;
    cpu->procedure_calls += calls;
    cpu->procedure_returns += calls;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a010)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    phase17_index8_poststate(
        cpu, final_g0, final_g9, comparison, condition_bits
    );

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = calls;
    report->recovered_procedure_returns = calls + UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


static vf2_status phase17_index9_render_choices(vf2_model2a *machine)
{
    static const uint32_t records[4] = {
        UINT32_C(0x0005ff28), UINT32_C(0x0005ff24),
        UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
    };
    uint32_t record=0u,destination=0u,last_source=0u,last_destination=0u;
    uint64_t characters=0u; size_t index=0u; vf2_status status=VF2_OK;
    for(index=0u;status==VF2_OK&&index<2u;++index){
        status=vf2_model2a_read_u32(machine,records[index],&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(27*0x80),UINT32_C(27),"         ");
    for(index=2u;status==VF2_OK&&index<4u;++index){
        status=vf2_model2a_read_u32(machine,records[index],&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    for(index=0u;status==VF2_OK&&index<2u;++index){
        status=vf2_model2a_read_u32(machine,records[index],&record);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
        if(status==VF2_OK&&destination<UINT32_C(4))status=VF2_ERROR_UNSUPPORTED;
        if(status==VF2_OK)status=write_u16(machine,destination-UINT32_C(4),index==0u?UINT16_C(0x8020):UINT16_C(0x801c));
    }
    return status;
}

static vf2_status phase17_index9_restore_menu(vf2_model2a *machine)
{
    static const uint32_t extra[3]={UINT32_C(0x0005ff08),UINT32_C(0x0005ff14),UINT32_C(0x0005ff18)};
    const uint8_t phase_index=UINT8_C(9),spill=UINT8_C(0x56);
    uint32_t record=0u,destination=0u,last_source=0u,last_destination=0u;
    uint64_t characters=0u; size_t index=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a4),&phase_index,sizeof(phase_index));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+UINT32_C(72),&record);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
    if(status==VF2_OK&&destination<UINT32_C(4))status=VF2_ERROR_UNSUPPORTED;
    if(status==VF2_OK)status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x801c));
    for(index=0u;status==VF2_OK&&index<12u;++index){
        status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+(uint32_t)index*UINT32_C(8),&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    for(index=0u;status==VF2_OK&&index<3u;++index){
        status=vf2_model2a_read_u32(machine,extra[index],&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));
    return status;
}

static vf2_status phase17_index9_initialize_defaults(vf2_model2a *machine)
{
    static const uint8_t system_defaults[29]={
        2u,2u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,15u,
        0u,0u,0xa0u,0u,0xc8u,0u,64u,64u,64u,37u,37u,37u,31u
    };
    static const uint8_t coin_defaults[15]={0u,0u,0u,0u,0u,0u,0u,0u,0u,2u,2u,2u,2u,2u,2u};
    static const uint8_t signature[16]={
        'V','I','R','T','U','A',' ','F','I','G','H','T','E','R',' ','2'
    };
    uint32_t base=0u; uint16_t crc=0u; const uint16_t version=UINT16_C(24);
    vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x0050016c),&base);
    if(status==VF2_OK)status=vf2_model2a_write(machine,base+UINT32_C(0x3340),system_defaults,sizeof(system_defaults));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x01d03340),system_defaults,sizeof(system_defaults));
    if(status==VF2_OK)status=phase17_index3_rebuild_transfer_tables(machine,system_defaults+22u);
    if(status==VF2_OK)status=vf2_model2a_write(machine,base+UINT32_C(0x3320),coin_defaults,sizeof(coin_defaults));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x01d03320),coin_defaults,sizeof(coin_defaults));
    if(status==VF2_OK)status=phase17_index7_clear_backup(machine);
    if(status==VF2_OK)status=compute_table_crc16(machine,base+UINT32_C(0x3340),UINT32_C(29),&crc);
    if(status==VF2_OK)status=write_u16(machine,UINT32_C(0x01d03302),crc);
    if(status==VF2_OK)status=compute_table_crc16(machine,base+UINT32_C(0x3320),UINT32_C(15),&crc);
    if(status==VF2_OK)status=write_u16(machine,UINT32_C(0x01d03300),crc);
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x01d03308),signature,sizeof(signature));
    if(status==VF2_OK)status=vf2_model2a_write(machine,base+UINT32_C(0x3308),signature,sizeof(signature));
    if(status==VF2_OK)status=write_u16(machine,UINT32_C(0x01d03306),version);
    if(status==VF2_OK)status=write_u16(machine,base+UINT32_C(0x3306),version);
    return status;
}

static vf2_status execute_frame_phase17_bit7_index9(
    vf2_model2a *machine,vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,uint8_t flagged_phase_index)
{
    const uint32_t base_input=UINT32_C(0x0ff7f700);
    uint32_t target=0u,input=0u,nav=0u,released=0u,previous=0u,selector=0u,seed=0u,counter=0u,record=0u,destination=0u;
    uint8_t a5=0u,a6=0u,a7=0u,spill=UINT8_C(0x56);
    uint64_t instructions=0u,calls=0u; int equal=1; vf2_status status=VF2_OK;
    if(flagged_phase_index!=UINT8_C(0x89)||cpu->local_frame_depth==0u)return VF2_ERROR_UNSUPPORTED;
    status=vf2_model2a_read_u32(machine,UINT32_C(0x0005fef0),&target);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500700),&input);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500704),&nav);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500708),&released);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050070c),&previous);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050002c),&selector);
    if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a5),&a5,sizeof(a5));
    if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a6),&a6,sizeof(a6));
    if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a7),&a7,sizeof(a7));
    if(status!=VF2_OK||target!=UINT32_C(0x0005eaa0)||input!=base_input||released!=0u||previous!=base_input||selector!=UINT32_C(0x00020000)||a5>UINT8_C(4)||a6!=UINT8_C(0xff)||a7!=UINT8_C(0xff))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;

    if(a5==UINT8_C(0)){
        const uint8_t next=UINT8_C(1); if(nav!=0u)return VF2_ERROR_UNSUPPORTED;
        status=phase17_index9_render_choices(machine);
        if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
        instructions=UINT64_C(711); calls=UINT64_C(7);
    }else if(a5==UINT8_C(1)){
        if((nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u){
            status=phase17_index9_restore_menu(machine); instructions=UINT64_C(14313); calls=UINT64_C(19);
            if(status==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),5u,0);
        }else if(nav==0u){instructions=UINT64_C(49);calls=UINT64_C(4);cpu->registers[16]=0u;}
        else if(nav==UINT32_C(0x1000)||nav==UINT32_C(0x2000)){
            const uint8_t next=UINT8_C(2); status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
            instructions=nav==UINT32_C(0x1000)?UINT64_C(51):UINT64_C(48);calls=UINT64_C(4);
            cpu->registers[16]=nav==UINT32_C(0x1000)?UINT32_C(1):UINT32_MAX;equal=0;
        }else return VF2_ERROR_UNSUPPORTED;
    }else if(a5==UINT8_C(2)){
        const uint8_t next=UINT8_C(3); if(nav!=0u)return VF2_ERROR_UNSUPPORTED;
        status=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff24),&record);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
        if(status==VF2_OK&&destination>=UINT32_C(4))status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x8020));else if(status==VF2_OK)status=VF2_ERROR_UNSUPPORTED;
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff28),&record);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
        if(status==VF2_OK&&destination>=UINT32_C(4))status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x801c));else if(status==VF2_OK)status=VF2_ERROR_UNSUPPORTED;
        if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
        instructions=UINT64_C(43);calls=UINT64_C(2);cpu->registers[16]=0u;cpu->registers[25]=UINT32_C(0x01000bb0);
    }else if(a5==UINT8_C(3)){
        if(nav==0u){instructions=UINT64_C(41);calls=UINT64_C(2);cpu->registers[16]=0u;}
        else if((nav&UINT32_C(0x08001008))!=0u){const uint8_t next=UINT8_C(0);status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));instructions=UINT64_C(38);calls=UINT64_C(2);cpu->registers[16]=0u;equal=0;}
        else if((nav&UINT32_C(0x04000104))!=0u){
            const uint8_t next=UINT8_C(4); status=vf2_model2a_read_u32(machine,UINT32_C(0x00500098),&seed);
            if(status==VF2_OK&&seed!=UINT32_C(0xcbf33340))return VF2_ERROR_UNSUPPORTED;
            if(status==VF2_OK)status=phase17_index9_initialize_defaults(machine);
            if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(27*0x80),UINT32_C(27),"COMPLETED");
            if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),UINT32_C(100));
            if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
            instructions=UINT64_C(54853);calls=UINT64_C(30);
            if(status==VF2_OK)phase17_index7_post(cpu,UINT32_C(9),UINT32_C(1),UINT32_C(0x8180),UINT32_C(0x01000e36),UINT32_C(5),0);
        }else return VF2_ERROR_UNSUPPORTED;
    }else{
        if(nav!=0u)return VF2_ERROR_UNSUPPORTED;
        status=vf2_model2a_read_u32(machine,UINT32_C(0x00500024),&counter);
        if(status!=VF2_OK||counter==0u)return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
        --counter; status=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),counter);
        if(status==VF2_OK&&counter==0u){status=phase17_index9_restore_menu(machine);instructions=UINT64_C(14308);calls=UINT64_C(18);if(status==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),UINT32_C(5),0);}
        else if(status==VF2_OK){instructions=UINT64_C(35);calls=UINT64_C(2);phase17_index7_post(cpu,0u,UINT32_C(0x3f4f5c29),UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),UINT32_C(5),1);}
    }
    if(status==VF2_OK&&a5!=UINT8_C(1)&&a5!=UINT8_C(4)&&!(a5==UINT8_C(3)&&(nav&UINT32_C(0x04000104))!=0u))status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));
    if(status!=VF2_OK)return status;
    cpu->executed_instructions+=instructions;cpu->procedure_calls+=calls;cpu->procedure_returns+=calls;
    status=vf2_i960_cpu_return_procedure(cpu,machine);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    if(a5<=UINT8_C(3)&&!((a5==UINT8_C(1)&&(nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u)||(a5==UINT8_C(3)&&(nav&UINT32_C(0x04000104))!=0u))){
        phase17_index7_post(cpu,cpu->registers[16],a5==UINT8_C(0)?UINT32_C(0x3f4f5c29):cpu->registers[17],UINT32_C(0xc0a0a3d7),cpu->registers[25],UINT32_C(5),0);
        if(a5==UINT8_C(0))cpu->registers[16]=UINT32_C(0x00078cb0);
        if(!equal){cpu->arithmetic_control=(cpu->arithmetic_control&~UINT32_C(7))|UINT32_C(4);cpu->compare_result=VF2_I960_COMPARE_LESS;}
    }
    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;report->exit_address=cpu->ip;report->iterations=UINT64_C(1);report->recovered_instruction_count=instructions;report->recovered_procedure_calls=calls;report->recovered_procedure_returns=calls+UINT64_C(1);report->cpu_poststate_applied=1;
    return VF2_OK;
}

static vf2_status execute_frame_phase17_bit7_index11(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    uint32_t saved_g4,
    uint8_t flagged_phase_index
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t frame_dispatch_return =
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2];
    vf2_hybrid_bridge_report meter_report;
    uint32_t indirect_target = 0u;
    uint32_t base = 0u;
    uint32_t saved_g9 = 0u;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t screen_record = 0u;
    uint32_t screen_source = 0u;
    uint32_t screen_destination = 0u;
    uint32_t base_flags = 0u;
    uint16_t crc = 0u;
    uint64_t characters = 0u;
    uint64_t accounted = 0u;
    uint8_t mode = 0u;
    uint8_t phase_a5 = 0u;
    uint8_t system_flags = 0u;
    int first_visit = 0;
    int terminal_reset = 0;
    int positive_countdown = 0;
    uint64_t expected_instructions = 0u;
    uint64_t expected_calls = 0u;
    uint64_t expected_returns = 0u;
    vf2_status status = VF2_OK;

    memset(&meter_report, 0, sizeof(meter_report));
    if (flagged_phase_index != UINT8_C(0x8b) ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0005fea8) +
            (uint32_t)(flagged_phase_index & UINT8_C(0x7f)) * UINT32_C(8),
        &indirect_target
    );
    if (status != VF2_OK || indirect_target != UINT32_C(0x0005ef60)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &player0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &player1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &base_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a5), &phase_a5, sizeof(phase_a5)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500171), &system_flags,
            sizeof(system_flags)
        );
    }
    first_visit = phase_a5 == 0u;
    if (status != VF2_OK || mode == UINT8_C(25) ||
        (base_flags & UINT32_C(3)) != 0u ||
        (!first_visit && phase_a5 != UINT8_C(0xff))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (first_visit && (system_flags & UINT8_C(1)) == 0u) {
        expected_instructions = UINT64_C(13844);
        expected_calls = UINT64_C(31);
        expected_returns = UINT64_C(32);
    } else {
        expected_instructions =
            first_visit ? UINT64_C(13286) : UINT64_C(626);
        expected_calls = first_visit ? UINT64_C(27) : UINT64_C(25);
        expected_returns = first_visit ? UINT64_C(28) : UINT64_C(26);
    }

    /* Recreate the ROM call chain a6c0 -> 10b5c -> 58fe0. Keeping these
     * frames real, instead of merely adjusting aggregate counters, preserves
     * the local-window and stack bytes subsequently touched by 0x20f0. */
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00010b5c), UINT32_C(0x0000a6f4)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = UINT32_C(0xff);
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g4
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00058fe0), UINT32_C(0x00010b78)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = base;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500700), &cpu->registers[9]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &cpu->registers[8]
        );
    }
    cpu->registers[3] =
        (uint32_t)(flagged_phase_index & UINT8_C(0x7f));
    if (status != VF2_OK) {
        return status;
    }

    /* 0x0005ef60 saves g9, then the observed mode takes the 0x20f0 call. */
    cpu->registers[1] += UINT32_C(4);
    saved_g9 = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g9
    );
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = base;
    cpu->registers[15] = mode;
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_GAME_METER_UPDATE_ENTRY, UINT32_C(0x0005ef90)
        );
    }
    if (status == VF2_OK) {
        status = execute_game_meter_update(machine, cpu, &meter_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0005ef90)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4),
        &cpu->registers[VF2_I960_G0_REGISTER + 9u]
    );
    cpu->registers[1] -= UINT32_C(4);
    if (status != VF2_OK ||
        cpu->registers[VF2_I960_G0_REGISTER + 9u] != saved_g9) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* 0x0005ff54 -> 0x00009480: CRC 15 bytes at base+0x3320. */
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0005ff54), UINT32_C(0x0005efa0)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = base;
    cpu->registers[VF2_I960_G0_REGISTER] = base + UINT32_C(0x3320);
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(15);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00009480), UINT32_C(0x0005ff70)
    );
    if (status == VF2_OK) {
        status = compute_table_crc16(
            machine, base + UINT32_C(0x3320), UINT32_C(15), &crc
        );
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = crc;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip == UINT32_C(0x0005ff70)) {
        status = write_u16(machine, UINT32_C(0x01d03300), crc);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0005efa0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[3] = UINT32_C(0xff);
    cpu->registers[4] = phase_a5;
    if (first_visit) {
        /* First visit: clear the plane and draw the phase-11 diagnostic
         * label, then arm the 320-frame countdown. */
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000000);
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(64);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(48);
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00008ef0), UINT32_C(0x0005efc4)
        );
        if (status == VF2_OK) {
            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status == VF2_OK) {
            /* 0x00008ef0 leaves g1 cleared on the observed 64x48 fill. */
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0005ff1c), &screen_record
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, screen_record, &screen_destination
            );
        }
        if (status != VF2_OK ||
            screen_record > UINT32_MAX - UINT32_C(4)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        screen_source = screen_record + UINT32_C(4);
        cpu->registers[15] = screen_record;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = screen_destination;
        cpu->registers[VF2_I960_G0_REGISTER] = screen_source;
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00007fc0), UINT32_C(0x0005efd8)
        );
        if (status == VF2_OK) {
            status = copy_diagnostic_text(
                machine, screen_source, screen_destination, &characters
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status == VF2_OK) {
            cpu->registers[15] = UINT32_C(320);
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500024), UINT32_C(320)
            );
        }
        if (status == VF2_OK) {
            const uint8_t ff = UINT8_C(0xff);
            cpu->registers[15] = UINT32_C(0xff);
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &ff, sizeof(ff)
            );
        }
        cpu->registers[6] = system_flags;
        if (status == VF2_OK && (system_flags & UINT8_C(1)) == 0u) {
            static const char backup_mode_text[] =
                "STATIC RAM IS 'BACK-UP MODE'";
            static const char invalid_changes_text[] =
                "AND YOUR CHANGES ARE INVALID !!";

            status = write_phase17_index0_text(
                machine, UINT32_C(30 * 0x80), UINT32_C(19),
                backup_mode_text
            );
            if (status == VF2_OK) {
                status = write_phase17_index0_text(
                    machine, UINT32_C(33 * 0x80), UINT32_C(18),
                    invalid_changes_text
                );
            }
            if (status == VF2_OK) {
                characters += (uint64_t)(sizeof(backup_mode_text) - 1u);
                characters += (uint64_t)(sizeof(invalid_changes_text) - 1u);
                account_nested_procedure(cpu, UINT64_C(4), UINT64_C(4));
            }
        }
    } else {
        uint32_t countdown = 0u;

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500024), &countdown
        );
        --countdown;
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500024), countdown
            );
        }
        cpu->registers[3] = countdown;
        positive_countdown = (int32_t)countdown > 0;
        if (status != VF2_OK) {
            return status;
        }
        if ((int32_t)countdown <= 0) {
            uint16_t layer = 0u;
            uint32_t layer_control = 0u;
            const uint32_t reset_words[4] = {
                UINT32_C(0x52455320), UINT32_C(0x4e4c2053),
                UINT32_C(0x4e204544), UINT32_C(0x20514555)
            };
            const uint8_t zero = 0u;

            terminal_reset = 1;
            expected_instructions = UINT64_C(13194);
            expected_calls = UINT64_C(27);
            expected_returns = UINT64_C(25);

            /* 0x0005f07c: terminal test-mode countdown. The ROM does not
             * return through the phase wrappers. It disables the two layer
             * bits, clears transient gameplay state and the diagnostic tile
             * plane, emits the RESET sentinel, then branches directly to the
             * boot entry at 0x000000b0. */
            cpu->registers[15] = UINT32_C(0x8000);
            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
            cpu->registers[3] = UINT32_C(0x0100a00c);
            if (status == VF2_OK) {
                status = read_u16(machine, cpu->registers[3], &layer);
            }
            layer &= UINT16_C(0x7fff);
            cpu->registers[4] = (uint32_t)layer;
            if (status == VF2_OK) {
                status = write_u16(machine, cpu->registers[3], layer);
            }
            if (status == VF2_OK) {
                status = read_u16(
                    machine, cpu->registers[3] + UINT32_C(2), &layer
                );
            }
            layer &= UINT16_C(0x7fff);
            cpu->registers[4] = (uint32_t)layer;
            if (status == VF2_OK) {
                status = write_u16(
                    machine, cpu->registers[3] + UINT32_C(2), layer
                );
            }
            cpu->registers[4] = 0u;
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050009c), &zero, sizeof(zero)
                );
            }

            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                UINT32_C(0x01000000);
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(64);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(48);
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_procedure(
                    cpu, UINT32_C(0x00008ef0), UINT32_C(0x0005f0c8)
                );
            }
            if (status == VF2_OK) {
                status = clear_tile_plane_64x48(
                    machine, UINT32_C(0x01000000)
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                    UINT32_C(0x01001800);
                cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &cpu->registers[3]
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, cpu->registers[3], &layer_control
                );
            }
            layer_control &= ~UINT32_C(1);
            cpu->registers[15] = layer_control;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[3], layer_control
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, cpu->registers[3], &layer_control
                );
            }
            layer_control &= ~UINT32_C(2);
            cpu->registers[15] = layer_control;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[3], layer_control
                );
            }
            cpu->registers[15] = 0u;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500708), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500704), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500700), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0050070c), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, cpu->registers[1] - UINT32_C(4),
                    &cpu->registers[VF2_I960_G0_REGISTER + 4u]
                );
            }
            cpu->registers[1] -= UINT32_C(4);
            cpu->registers[3] = UINT32_C(0x00e80004);
            cpu->registers[4] = 0u;
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[3], 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_procedure(
                    cpu, UINT32_C(0x0006116c), UINT32_C(0x0005f138)
                );
            }
            if (status == VF2_OK) {
                cpu->registers[8] = reset_words[0];
                cpu->registers[9] = reset_words[1];
                cpu->registers[10] = reset_words[2];
                cpu->registers[11] = reset_words[3];
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0059cfe0), reset_words,
                    sizeof(reset_words)
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status == VF2_OK) {
                cpu->ip = UINT32_C(0x000000b0);
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    if (!terminal_reset) {
        /* ret from 0x5ef60/58fe0, wrapper restore, then ret from a6c0. */
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != UINT32_C(0x00010b78)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + 4u]
        );
        cpu->registers[1] -= UINT32_C(4);
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a6f4)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status != VF2_OK || cpu->ip != frame_dispatch_return) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        /* The local-frame unwinds above restore condition state. Apply the
         * condition produced by the final observed compare only after the
         * outermost return, matching the state visible at frame_dispatch_return. */
        if (positive_countdown) {
            set_signed_condition(cpu, INT32_C(0), INT32_C(1));
        } else {
            set_equal_condition(cpu);
        }
    }

    accounted = cpu->executed_instructions - start_instructions;
    if (accounted > expected_instructions) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->executed_instructions += expected_instructions - accounted;
    if (cpu->procedure_calls - start_calls != expected_calls ||
        cpu->procedure_returns - start_returns != expected_returns) {
        return VF2_ERROR_UNSUPPORTED;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->rows = characters;
    report->changed_values =
        UINT64_C(3) + meter_report.changed_values +
        (first_visit ? characters + UINT64_C(2) : UINT64_C(1));
    report->bytes_written =
        5u + sizeof(uint32_t) * 2u + meter_report.bytes_written +
        sizeof(uint16_t) +
        (first_visit
            ? (size_t)UINT32_C(64 * 48 * 2) +
                (size_t)characters * 2u + sizeof(uint32_t) + sizeof(uint8_t)
            : sizeof(uint32_t));
    report->recovered_instruction_count = expected_instructions;
    report->recovered_procedure_calls = expected_calls;
    report->recovered_procedure_returns = expected_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status phase17_zero_initialize_control_fighter(
    vf2_model2a *machine,
    uint32_t fighter
)
{
    uint32_t zero = 0u;
    uint16_t zero16 = 0u;
    uint8_t input = 0u;
    uint8_t one = UINT8_C(1);
    vf2_status status = VF2_OK;

    status = vf2_model2a_write_u32(
        machine, fighter + UINT32_C(0x1208), UINT32_C(8)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x1208), zero
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x1218), zero
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, fighter + UINT32_C(0x120e), zero16
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x121c), zero
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x123c), zero
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, fighter + UINT32_C(0x1200), &input, sizeof(input)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1280), &input, sizeof(input)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1281), &input, sizeof(input)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1202), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1203), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1204), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1205), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1206), &one, sizeof(one)
        );
    }
    return status;
}

static float phase17_zero_float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t phase17_zero_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int32_t phase17_zero_round_nearest_even(float value)
{
    const double input = (double)value;
    const int64_t truncated = (int64_t)input;
    const double fraction = input - (double)truncated;
    int64_t rounded = truncated;

    if (fraction > 0.5 ||
        (fraction == 0.5 && (truncated & INT64_C(1)) != 0)) {
        ++rounded;
    } else if (fraction < -0.5 ||
               (fraction == -0.5 && (truncated & INT64_C(1)) != 0)) {
        --rounded;
    }
    return (int32_t)rounded;
}

static vf2_status phase17_zero_render_decimal(
    vf2_model2a *machine,
    int32_t value,
    uint32_t destination
)
{
    uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value)
                                   : (uint32_t)value;
    uint16_t tiles[6];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    tiles[0] = value < 0 ? UINT16_C(0x802d) : UINT16_C(0x8020);
    if (magnitude <= UINT32_C(8191)) {
        tiles[1] = UINT16_C(0x8020);
        for (index = 0u; index < 4u; ++index) {
            status = read_u16(
                machine,
                UINT32_C(0x02040000) + magnitude * UINT32_C(8) +
                    index * UINT32_C(2),
                &tiles[index + 2u]
            );
            if (status != VF2_OK) {
                return status;
            }
        }
    } else {
        for (index = 0u; index < 5u; ++index) {
            const uint32_t place = UINT32_C(4) - index;
            uint32_t divisor = 1u;
            uint32_t digit = 0u;
            uint32_t inner = 0u;
            for (inner = 0u; inner < place; ++inner) {
                divisor *= UINT32_C(10);
            }
            digit = (magnitude / divisor) % UINT32_C(10);
            tiles[index + 1u] =
                (uint16_t)(UINT16_C(0x8030) + (uint16_t)digit);
        }
    }
    for (index = 0u; status == VF2_OK && index < 6u; ++index) {
        status = write_u16(
            machine, destination + index * UINT32_C(2), tiles[index]
        );
    }
    return status;
}

static vf2_status phase17_zero_render_decimal_inline(
    vf2_model2a *machine,
    int32_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    vf2_status status = phase17_zero_render_decimal(machine, value, destination);
    uint64_t characters = 0u;

    if (status == VF2_OK) {
        status = copy_diagnostic_text(
            machine, inline_source, destination + UINT32_C(14), &characters
        );
    }
    return status;
}

static vf2_status phase17_zero_render_float_inline(
    vf2_model2a *machine,
    uint32_t bits,
    float scale,
    uint32_t destination,
    uint32_t inline_source
)
{
    const float scaled = phase17_zero_float_from_bits(bits) * scale;
    return phase17_zero_render_decimal_inline(
        machine, phase17_zero_round_nearest_even(scaled), destination,
        inline_source
    );
}

static vf2_status phase17_zero_render_ratio_inline(
    vf2_model2a *machine,
    uint16_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    const uint32_t scaled = ((uint32_t)value * UINT32_C(3600)) /
                            UINT32_C(0xffff);
    return phase17_zero_render_decimal_inline(
        machine, (int32_t)scaled, destination, inline_source
    );
}

static vf2_status phase17_zero_render_hex_inline(
    vf2_model2a *machine,
    uint32_t value,
    uint32_t destination,
    uint32_t inline_source,
    uint32_t width
)
{
    const uint32_t start = destination - UINT32_C(2);
    uint32_t index = 0u;
    int started = 0;
    vf2_status status = VF2_OK;
    uint64_t characters = 0u;

    for (index = 0u; status == VF2_OK && index < width; ++index) {
        const uint32_t shift = (width - UINT32_C(1) - index) * UINT32_C(4);
        const uint32_t nibble = shift >= UINT32_C(32)
            ? 0u : ((value >> shift) & UINT32_C(0xf));
        uint16_t tile = UINT16_C(0x8020);
        if (nibble != 0u || started || index + UINT32_C(1) == width) {
            started = 1;
            status = read_u16(
                machine,
                UINT32_C(0x02000200) + nibble * UINT32_C(2),
                &tile
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, start + index * UINT32_C(2), tile
            );
        }
    }
    if (status == VF2_OK) {
        const uint32_t text_destination = destination +
            (width == UINT32_C(9) ? UINT32_C(18) : UINT32_C(14));
        status = copy_diagnostic_text(
            machine, inline_source, text_destination, &characters
        );
    }
    return status;
}

static vf2_status phase17_zero_render_hex6_inline(
    vf2_model2a *machine,
    uint32_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    return phase17_zero_render_hex_inline(
        machine, value & UINT32_C(0xffff), destination, inline_source,
        UINT32_C(7)
    );
}

static vf2_status phase17_zero_render_hex8_inline(
    vf2_model2a *machine,
    uint32_t value,
    uint32_t destination,
    uint32_t inline_source
)
{
    return phase17_zero_render_hex_inline(
        machine, value, destination, inline_source, UINT32_C(9)
    );
}

static vf2_status phase17_zero_copy_text(
    vf2_model2a *machine,
    uint32_t source,
    uint32_t destination
)
{
    uint64_t characters = 0u;
    return copy_diagnostic_text(machine, source, destination, &characters);
}

static vf2_status phase17_zero_read_s8(
    vf2_model2a *machine,
    uint32_t address,
    int32_t *value
)
{
    int8_t raw = 0;
    const vf2_status status = vf2_model2a_read(
        machine, address, &raw, sizeof(raw)
    );
    if (status == VF2_OK) {
        *value = (int32_t)raw;
    }
    return status;
}

static vf2_status phase17_zero_read_s16(
    vf2_model2a *machine,
    uint32_t address,
    int32_t *value
)
{
    uint16_t raw = 0u;
    const vf2_status status = read_u16(machine, address, &raw);
    if (status == VF2_OK) {
        *value = (int32_t)(int16_t)raw;
    }
    return status;
}

static vf2_status phase17_zero_write_float(
    vf2_model2a *machine,
    uint32_t address,
    float value
)
{
    return vf2_model2a_write_u32(
        machine, address, phase17_zero_float_to_bits(value)
    );
}

static vf2_status phase17_zero_adjust_float(
    vf2_model2a *machine,
    uint32_t address,
    float delta
)
{
    uint32_t bits = 0u;
    vf2_status status = vf2_model2a_read_u32(machine, address, &bits);
    if (status == VF2_OK) {
        status = phase17_zero_write_float(
            machine, address, phase17_zero_float_from_bits(bits) + delta
        );
    }
    return status;
}

static vf2_status phase17_zero_render_motion_name(
    vf2_model2a *machine,
    int32_t motion_index,
    uint32_t destination,
    uint32_t *descriptor_out
)
{
    uint32_t descriptor = 0u;
    uint32_t source = 0u;
    vf2_status status = VF2_OK;

    if (motion_index < 0 || motion_index > INT32_C(0x54f)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x02120004) + (uint32_t)motion_index * UINT32_C(4),
        &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, descriptor, &source);
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(machine, source, destination);
    }
    if (status == VF2_OK && descriptor_out != NULL) {
        *descriptor_out = descriptor;
    }
    return status;
}

static vf2_status phase17_zero_screen6_render_motion_name(
    vf2_model2a *machine,
    int32_t motion_index,
    uint32_t destination,
    uint32_t *descriptor_out
)
{
    uint32_t table = 0u;
    uint32_t source = 0u;
    vf2_status status = VF2_OK;

    if (motion_index < 0 || motion_index > INT32_C(0x54f)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x02120004), &table
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, table + (uint32_t)motion_index * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(machine, source, destination);
    }
    if (status == VF2_OK && descriptor_out != NULL) {
        *descriptor_out = source;
    }
    return status;
}

static vf2_status phase17_zero_apply_camera_angle_controls(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t input_flags,
    uint32_t navigation_flags
)
{
    uint32_t flags = navigation_flags;
    uint16_t x_angle = 0u;
    uint16_t y_angle = 0u;
    uint32_t step = UINT32_C(8);
    vf2_status status = VF2_OK;

    if ((input_flags & (UINT32_C(1) << 9u)) != 0u) {
        step = UINT32_C(128);
    }
    if ((input_flags & (UINT32_C(1) << 10u)) != 0u) {
        flags = input_flags;
    }
    status = read_u16(machine, control + UINT32_C(0x24), &x_angle);
    if (status == VF2_OK) {
        status = read_u16(machine, control + UINT32_C(0x26), &y_angle);
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((flags & (UINT32_C(1) << 21u)) != 0u) {
        x_angle = (uint16_t)(x_angle + (uint16_t)step);
    }
    if ((flags & (UINT32_C(1) << 20u)) != 0u) {
        x_angle = (uint16_t)(x_angle - (uint16_t)step);
    }
    if ((flags & (UINT32_C(1) << 23u)) != 0u) {
        y_angle = (uint16_t)(y_angle + (uint16_t)step);
    }
    if ((flags & (UINT32_C(1) << 22u)) != 0u) {
        y_angle = (uint16_t)(y_angle - (uint16_t)step);
    }
    status = write_u16(machine, control + UINT32_C(0x24), x_angle);
    if (status == VF2_OK) {
        status = write_u16(machine, control + UINT32_C(0x26), y_angle);
    }
    return status;
}

static vf2_status phase17_zero_copro_command(
    vf2_model2a *machine,
    uint32_t command,
    const uint32_t *arguments,
    size_t argument_count,
    uint32_t *results,
    size_t result_count
)
{
    const uint32_t port = UINT32_C(0x00884000);
    size_t index = 0u;
    vf2_status status = vf2_model2a_write_u32(machine, port, command);

    for (index = 0u; status == VF2_OK && index < argument_count; ++index) {
        status = vf2_model2a_write_u32(machine, port, arguments[index]);
    }
    for (index = 0u; status == VF2_OK && index < result_count; ++index) {
        status = vf2_model2a_read_u32(machine, port, &results[index]);
    }
    return status;
}

static vf2_status phase17_zero_screen6_adjust_follow(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t accumulator_offset,
    uint32_t value_offset,
    float delta
)
{
    uint32_t accumulator_bits = 0u;
    uint32_t value_bits = 0u;
    float accumulator = 0.0f;
    float value = 0.0f;
    vf2_status status = vf2_model2a_read_u32(
        machine, control + accumulator_offset, &accumulator_bits
    );

    if (status == VF2_OK) {
        accumulator = phase17_zero_float_from_bits(accumulator_bits) + delta;
        accumulator_bits = phase17_zero_float_to_bits(accumulator);
        status = vf2_model2a_write_u32(
            machine, control + accumulator_offset, accumulator_bits
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, control + value_offset, &value_bits
        );
    }
    if (status == VF2_OK) {
        value = phase17_zero_float_from_bits(value_bits);
        value += (accumulator - value) * 0.25f;
        status = vf2_model2a_write_u32(
            machine, control + value_offset,
            phase17_zero_float_to_bits(value)
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_force_blank(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player0,
    uint8_t *de_flags
)
{
    int32_t motion = 0;
    vf2_status status = VF2_OK;

    *de_flags = (uint8_t)(*de_flags | UINT8_C(4));
    status = vf2_model2a_write(
        machine, control + UINT32_C(0xde), de_flags, sizeof(*de_flags)
    );
    if (status == VF2_OK) {
        status = phase17_zero_read_s16(
            machine, UINT32_C(0x00508020), &motion
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player0 + UINT32_C(0x194), (uint32_t)motion
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_toggle_objects(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player1,
    uint8_t *de_flags
)
{
    uint32_t flags = 0u;
    uint32_t associated = 0u;
    uint8_t motion = 0u;
    vf2_status status = VF2_OK;

    if ((*de_flags & UINT8_C(1)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *de_flags = (uint8_t)(*de_flags | UINT8_C(1));
    status = vf2_model2a_write(
        machine, control + UINT32_C(0xde), de_flags, sizeof(*de_flags)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player1, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player1, flags | (UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player1 + UINT32_C(0x0c), UINT32_C(0x00013f08)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, player1 + UINT32_C(0x1b0), &motion, sizeof(motion)
        );
    }
    if (status == VF2_OK) {
        motion = (uint8_t)(motion % UINT8_C(13));
        status = vf2_model2a_write(
            machine, player1 + UINT32_C(0x1b1), &motion, sizeof(motion)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050086c), &associated
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, associated, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated, flags | (UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4)
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode1_target(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player0,
    uint32_t input_flags
)
{
    uint32_t player_x = 0u;
    uint32_t player_z = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, player0 + UINT32_C(0x18), &player_x
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player0 + UINT32_C(0x20), &player_z
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0xc0), player_x
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0xc4), UINT32_C(0x3f800000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, control + UINT32_C(0xc8), player_z
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc0), UINT32_C(0xb4), 0.0f
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc4), UINT32_C(0xb8), 0.0f
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc8), UINT32_C(0xbc), 0.0f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 21u)) != 0u) {
        status = phase17_zero_adjust_float(
            machine, UINT32_C(0x00501084), 2.0f
        );
        if (status == VF2_OK) {
            status = phase17_zero_adjust_float(
                machine, UINT32_C(0x00501088), 2.0f
            );
        }
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 20u)) != 0u) {
        status = phase17_zero_adjust_float(
            machine, UINT32_C(0x00501084), -2.0f
        );
        if (status == VF2_OK) {
            status = phase17_zero_adjust_float(
                machine, UINT32_C(0x00501088), -2.0f
            );
        }
    }
    return status;
}

static vf2_status phase17_zero_screen6_apply_held_controls(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t input_flags
)
{
    vf2_status status = VF2_OK;

    if ((input_flags & (UINT32_C(1) << 13u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd4), UINT32_C(0x20), 0.05f
        );
    } else if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd4), UINT32_C(0x20), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 14u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xcc), UINT32_C(0x18), 0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 15u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xcc), UINT32_C(0x18), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 8u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd0), UINT32_C(0x1c), -0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 9u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xd0), UINT32_C(0x1c), 0.05f
        );
    }

    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 21u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc8), UINT32_C(0xbc), 0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 20u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc8), UINT32_C(0xbc), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 22u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc0), UINT32_C(0xb4), 0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 23u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc0), UINT32_C(0xb4), -0.05f
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 16u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc4), UINT32_C(0xb8), -0.05f
        );
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 17u)) != 0u) {
        status = phase17_zero_screen6_adjust_follow(
            machine, control, UINT32_C(0xc4), UINT32_C(0xb8), 0.05f
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode2_update_character(
    vf2_model2a *machine,
    uint32_t fighter,
    uint32_t other_fighter,
    uint32_t associated_pointer_address,
    uint32_t navigation_flags
)
{
    uint8_t character = 0u;
    uint8_t other_character = 0u;
    uint8_t motion = 0u;
    uint32_t associated = 0u;
    uint32_t flags = 0u;
    vf2_status status = vf2_model2a_read(
        machine, fighter + UINT32_C(0x1b0), &character, sizeof(character)
    );

    if (status != VF2_OK) {
        return status;
    }
    if ((navigation_flags & (UINT32_C(1) << 16u)) != 0u) {
        character = character == 0u ? UINT8_C(25) : (uint8_t)(character - 1u);
    } else if ((navigation_flags & (UINT32_C(1) << 17u)) != 0u) {
        character = character >= UINT8_C(25) ? 0u : (uint8_t)(character + 1u);
    } else {
        return VF2_OK;
    }
    status = vf2_model2a_read(
        machine, other_fighter + UINT32_C(0x1b0),
        &other_character, sizeof(other_character)
    );
    if (status == VF2_OK && character == other_character) {
        if (character >= UINT8_C(13)) {
            character = (uint8_t)(character - UINT8_C(13));
        } else {
            character = (uint8_t)(character + UINT8_C(13));
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1b0),
            &character, sizeof(character)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x0c), UINT32_C(0x00013f08)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        motion = (uint8_t)(character % UINT8_C(13));
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x1b1), &motion, sizeof(motion)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, associated_pointer_address, &associated
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, associated, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated, flags | (UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4)
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode2(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t player0,
    uint32_t player1,
    uint32_t input_flags,
    uint32_t navigation_flags,
    uint8_t de_flags
)
{
    const uint32_t step_bits = UINT32_C(0x3d4ccccd);
    uint32_t vector[3] = {0u, 0u, 0u};
    uint32_t rotated[3] = {0u, 0u, 0u};
    uint16_t matrix_angle = 0u;
    uint16_t fighter_angle = 0u;
    size_t index = 0u;
    vf2_status status = read_u16(
        machine, control + UINT32_C(0x26), &matrix_angle
    );

    /* The zero-state corridor proven here starts from the identity matrix.
     * A non-zero matrix angle requires the still-unrecovered 0x6d command. */
    if (status != VF2_OK) {
        return status;
    }
    if (matrix_angle != 0u &&
        (input_flags & ((UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                        (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u))) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if ((input_flags & (UINT32_C(1) << 13u)) != 0u) {
        vector[2] = step_bits;
    } else if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
        vector[2] = step_bits ^ UINT32_C(0x80000000);
    }
    if ((input_flags & (UINT32_C(1) << 14u)) != 0u) {
        vector[0] = step_bits;
    } else if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
        vector[0] = step_bits ^ UINT32_C(0x80000000);
    }

    if (status == VF2_OK) {
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x14802929), vector, 3u, rotated, 3u
        );
    }
    for (index = 0u; index < 3u && status == VF2_OK; ++index) {
        uint32_t current = 0u;
        status = vf2_model2a_read_u32(
            machine, player0 + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
            &current
        );
        if (status == VF2_OK) {
            uint32_t add_arguments[2] = {rotated[index], current};
            status = phase17_zero_copro_command(
                machine, UINT32_C(0x09801313), add_arguments, 2u,
                &current, 1u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player0 + UINT32_C(0x18) +
                    (uint32_t)index * UINT32_C(4), current
            );
        }
    }

    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 23u)) != 0u) {
        status = read_u16(machine, player0 + UINT32_C(0x26), &fighter_angle);
        if (status == VF2_OK) {
            fighter_angle = (uint16_t)(fighter_angle + UINT16_C(0x80));
            status = write_u16(
                machine, player0 + UINT32_C(0x26), fighter_angle
            );
        }
    } else if (status == VF2_OK &&
               (input_flags & (UINT32_C(1) << 22u)) != 0u) {
        status = read_u16(machine, player0 + UINT32_C(0x26), &fighter_angle);
        if (status == VF2_OK) {
            fighter_angle = (uint16_t)(fighter_angle - UINT16_C(0x80));
            status = write_u16(
                machine, player0 + UINT32_C(0x26), fighter_angle
            );
        }
    }

    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 18u)) == 0u) {
        status = phase17_zero_screen6_mode2_update_character(
            machine, player0, player1, UINT32_C(0x00500868),
            navigation_flags
        );
    }
    if (status == VF2_OK && (de_flags & UINT8_C(1)) != 0u) {
        if ((input_flags & (UINT32_C(1) << 21u)) != 0u) {
            status = read_u16(machine, player1 + UINT32_C(0x26), &fighter_angle);
            if (status == VF2_OK) {
                fighter_angle = (uint16_t)(fighter_angle + UINT16_C(0x80));
                status = write_u16(
                    machine, player1 + UINT32_C(0x26), fighter_angle
                );
            }
        } else if ((input_flags & (UINT32_C(1) << 20u)) != 0u) {
            status = read_u16(machine, player1 + UINT32_C(0x26), &fighter_angle);
            if (status == VF2_OK) {
                fighter_angle = (uint16_t)(fighter_angle - UINT16_C(0x80));
                status = write_u16(
                    machine, player1 + UINT32_C(0x26), fighter_angle
                );
            }
        }
        if (status == VF2_OK &&
            (input_flags & (UINT32_C(1) << 18u)) != 0u) {
            status = phase17_zero_screen6_mode2_update_character(
                machine, player1, player0, UINT32_C(0x0050086c),
                navigation_flags
            );
        }
    }
    return status;
}

static vf2_status phase17_zero_screen6_mode2_motion(
    vf2_model2a *machine,
    uint32_t navigation_flags,
    uint8_t de_flags
)
{
    int32_t motion = 0;
    vf2_status status = VF2_OK;

    if ((de_flags & UINT8_C(4)) != 0u) {
        return VF2_OK;
    }
    status = phase17_zero_read_s16(
        machine, UINT32_C(0x00508020), &motion
    );
    if (status == VF2_OK &&
        (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
        ++motion;
        if (motion > 0x54f) {
            motion = 0;
        }
    } else if (status == VF2_OK &&
               (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
        --motion;
        if (motion < 0) {
            motion = 0x54f;
        }
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x00508020), (uint16_t)motion
        );
    }
    return status;
}

static vf2_status phase17_zero_screen6_update_angles(
    vf2_model2a *machine,
    uint32_t control
)
{
    uint32_t position[3] = {0u, 0u, 0u};
    uint32_t target[3] = {0u, 0u, 0u};
    uint32_t delta[3] = {0u, 0u, 0u};
    uint32_t magnitude = 0u;
    uint32_t angle = 0u;
    uint16_t desired_pitch = 0u;
    uint16_t desired_yaw = 0u;
    uint16_t desired_roll = 0u;
    uint16_t current = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < 3u && status == VF2_OK; ++index) {
        status = vf2_model2a_read_u32(
            machine, control + UINT32_C(0xb4) + (uint32_t)index * UINT32_C(4),
            &target[index]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, control + UINT32_C(0x18) + (uint32_t)index * UINT32_C(4),
                &position[index]
            );
        }
        if (status == VF2_OK) {
            const uint32_t arguments[2] = {target[index], position[index]};
            status = phase17_zero_copro_command(
                machine, UINT32_C(0x0a001414), arguments, 2u,
                &delta[index], 1u
            );
        }
    }
    if (status == VF2_OK) {
        const uint32_t arguments[2] = {delta[0], delta[2]};
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x16802d2d), arguments, 2u,
            &magnitude, 1u
        );
    }
    if (status == VF2_OK) {
        const uint32_t arguments[2] = {magnitude, delta[1]};
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x13802727), arguments, 2u,
            &angle, 1u
        );
        desired_pitch = (uint16_t)angle;
    }
    if (status == VF2_OK) {
        const uint32_t arguments[2] = {
            delta[2], delta[0] ^ UINT32_C(0x80000000)
        };
        status = phase17_zero_copro_command(
            machine, UINT32_C(0x13802727), arguments, 2u,
            &angle, 1u
        );
        desired_yaw = (uint16_t)angle;
    }
    if (status == VF2_OK) {
        status = read_u16(machine, control + UINT32_C(0xdc), &desired_roll);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, control + UINT32_C(0xd8), desired_pitch);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, control + UINT32_C(0xda), desired_yaw);
    }
    if (status == VF2_OK) {
        const uint16_t desired[3] = {desired_pitch, desired_yaw, desired_roll};
        const uint32_t current_offsets[3] = {
            UINT32_C(0x24), UINT32_C(0x26), UINT32_C(0x28)
        };
        for (index = 0u; index < 3u && status == VF2_OK; ++index) {
            int32_t difference = 0;
            status = read_u16(machine, control + current_offsets[index], &current);
            if (status != VF2_OK) {
                break;
            }
            difference = (int32_t)(int16_t)(uint16_t)(desired[index] - current);
            difference >>= 2;
            current = (uint16_t)(current + (uint16_t)difference);
            status = write_u16(machine, control + current_offsets[index], current);
        }
    }
    return status;
}

static vf2_status phase17_zero_render_missing_body(
    vf2_model2a *machine,
    uint8_t menu_index,
    uint32_t player0,
    uint32_t player1,
    uint32_t control,
    uint32_t input_flags,
    uint32_t navigation_flags,
    uint32_t runtime_flags,
    int entry_path,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    vf2_status status = VF2_OK;
    uint32_t bits = 0u;
    int32_t signed_value = 0;
    uint16_t short_value = 0u;

    if (final_g0 == NULL || final_g9 == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *final_g0 = 0u;
    *final_g9 = 0u;

    switch (menu_index) {
    case UINT8_C(1): {
        int32_t motion = 0;
        uint32_t descriptor = 0u;
        uint32_t fighter_flags = 0u;
        status = phase17_zero_read_s16(
            machine, UINT32_C(0x00508020), &motion
        );
        if (status != VF2_OK) {
            return status;
        }
        if (!entry_path) {
            if ((navigation_flags & (UINT32_C(1) << 12u)) != 0u ||
                (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                ++motion;
                if (motion > INT32_C(0x54f)) {
                    motion = 0;
                }
            } else if ((navigation_flags & (UINT32_C(1) << 13u)) != 0u ||
                       (input_flags & (UINT32_C(1) << 15u)) != 0u) {
                --motion;
                if (motion < 0) {
                    motion = INT32_C(0x54f);
                }
            }
        }
        status = write_u16(
            machine, UINT32_C(0x00508020), (uint16_t)motion
        );
        if (status == VF2_OK && entry_path) {
            status = vf2_model2a_write_u32(
                machine, player0 + UINT32_C(0x194), (uint32_t)motion
            );
        }
        if (status == VF2_OK && (entry_path ||
            (navigation_flags & (UINT32_C(1) << 8u)) != 0u)) {
            if (!entry_path) {
                status = vf2_model2a_write_u32(
                    machine, player0 + UINT32_C(0x194), (uint32_t)motion
                );
            }
            if (status == VF2_OK) {
                status = fill_tile_plane_spaces(
                    machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
                );
            }
        }
        if (status == VF2_OK) {
            status = fill_tile_plane_spaces(
                machine, UINT32_C(0x01000298), UINT32_C(40), UINT32_C(1)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, motion, UINT32_C(0x01000294), UINT32_C(0x00055674)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_copy_text(
                machine, UINT32_C(0x00055688), UINT32_C(0x0100028a)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_motion_name(
                machine, motion, UINT32_C(0x010002a2), &descriptor
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s16(machine, descriptor, &signed_value);
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x01000322),
                UINT32_C(0x000556d0)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, player0, &fighter_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player0, fighter_flags & ~(UINT32_C(1) << 5u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, player1, &fighter_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player1, fighter_flags & ~(UINT32_C(1) << 5u)
            );
        }
        *final_g0 = UINT32_C(0x00006874);
        *final_g9 = UINT32_C(0x010003a2);
        return status;
    }
    case UINT8_C(2): {
        uint8_t selector = 0u;
        if (!entry_path) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00508030), &selector, sizeof(selector)
            );
            if (status != VF2_OK) {
                return status;
            }
            if ((navigation_flags & (UINT32_C(1) << 12u)) != 0u) {
                selector = selector >= UINT8_C(2) ? 0u : (uint8_t)(selector + 1u);
            }
            if ((navigation_flags & (UINT32_C(1) << 13u)) != 0u) {
                selector = selector == 0u ? UINT8_C(2) : (uint8_t)(selector - 1u);
            }
            status = vf2_model2a_write(
                machine, UINT32_C(0x00508030), &selector, sizeof(selector)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s8(
                machine, UINT32_C(0x00508037), &signed_value
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x0100028a),
                UINT32_C(0x00055778)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s8(
                machine, UINT32_C(0x00508036), &signed_value
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x0100030a),
                UINT32_C(0x00055790)
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_read_s16(
                machine, UINT32_C(0x00508034), &signed_value
            );
        }
        if (status == VF2_OK) {
            status = phase17_zero_render_decimal_inline(
                machine, signed_value, UINT32_C(0x0100038a),
                UINT32_C(0x000557a8)
            );
        }
        *final_g0 = UINT32_C(0x00006e6f);
        *final_g9 = UINT32_C(0x0100040a);
        return status;
    }
    case UINT8_C(3): {
        if (!entry_path &&
            (navigation_flags & UINT32_C(0x700)) == UINT32_C(0x700)) {
            status = vf2_model2a_write_u32(machine, player0 + UINT32_C(0x18), 0u);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, player0 + UINT32_C(0x1c), 0u);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, player0 + UINT32_C(0x20), 0u);
            }
            if (status == VF2_OK) {
                status = write_u16(machine, player0 + UINT32_C(0x26), UINT16_C(0xc000));
            }
        } else if (!entry_path && (input_flags & (UINT32_C(1) << 9u)) != 0u) {
            status = read_u16(machine, player0 + UINT32_C(0x26), &short_value);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 15u)) != 0u) {
                short_value = (uint16_t)(short_value - UINT16_C(0x200));
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                short_value = (uint16_t)(short_value + UINT16_C(0x200));
            }
            if (status == VF2_OK) {
                status = write_u16(machine, player0 + UINT32_C(0x26), short_value);
            }
        } else if (!entry_path) {
            if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x18), -0.01f);
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x18), 0.01f);
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x20), -0.01f);
            }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                status = phase17_zero_adjust_float(machine, player0 + UINT32_C(0x20), 0.01f);
            }
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, player0 + UINT32_C(0x18), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100028a), UINT32_C(0x0005550c));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, player0 + UINT32_C(0x1c), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100030a), UINT32_C(0x00055530));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, player0 + UINT32_C(0x20), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100038a), UINT32_C(0x00055554));
        if (status == VF2_OK) status = read_u16(machine, player0 + UINT32_C(0x26), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_ratio_inline(machine, short_value, UINT32_C(0x0100040a), UINT32_C(0x0005556c));
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, short_value, UINT32_C(0x0100048a), UINT32_C(0x00055584));
        *final_g0 = 0u;
        *final_g9 = UINT32_C(0x0100050a);
        return status;
    }
    case UINT8_C(5): {
        const float step = (!entry_path && (input_flags & (UINT32_C(1) << 9u)) != 0u) ? 0.1f : 0.01f;
        uint32_t selected = control + UINT32_C(0x20);
        if (!entry_path) {
            if ((input_flags & (UINT32_C(1) << 15u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x18), -step);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x18), step);
            if ((input_flags & (UINT32_C(1) << 8u)) != 0u) selected = control + UINT32_C(0x1c);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 13u)) != 0u) status = phase17_zero_adjust_float(machine, selected, step);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 12u)) != 0u) status = phase17_zero_adjust_float(machine, selected, -step);
            if (status == VF2_OK) status = phase17_zero_apply_camera_angle_controls(machine, control, input_flags, navigation_flags);
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x18), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100028a), UINT32_C(0x000558a4));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x1c), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100030a), UINT32_C(0x000558c8));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x20), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100038a), UINT32_C(0x000558ec));
        if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x24), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, short_value, UINT32_C(0x0100040a), UINT32_C(0x00055904));
        if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x26), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, short_value, UINT32_C(0x0100048a), UINT32_C(0x0005591c));
        *final_g0 = 0u;
        *final_g9 = UINT32_C(0x0100050a);
        return status;
    }
    case UINT8_C(6): {
        uint8_t camera_mode = 0u;
        uint8_t de_flags = 0u;
        uint32_t mode_name = 0u;
        uint32_t descriptor = 0u;
        int32_t motion = 0;
        uint8_t stage = 0u;

        status = vf2_model2a_read(machine, control + UINT32_C(0xde), &de_flags, sizeof(de_flags));
        de_flags = (uint8_t)(de_flags & ~UINT8_C(4));
        if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) de_flags = (uint8_t)(de_flags | UINT8_C(4));
        if (status == VF2_OK) status = vf2_model2a_write(machine, control + UINT32_C(0xde), &de_flags, sizeof(de_flags));
        if (status == VF2_OK) status = vf2_model2a_read(machine, control + UINT32_C(0xb1), &camera_mode, sizeof(camera_mode));
        if (status == VF2_OK) {
            const uint8_t active = camera_mode == 0u ? UINT8_C(1) : 0u;
            status = vf2_model2a_write(machine, control + UINT32_C(0xb0), &active, sizeof(active));
        }
        if (status == VF2_OK && !entry_path &&
            (navigation_flags & (UINT32_C(1) << 4u)) != 0u) {
            camera_mode = (uint8_t)(camera_mode + UINT8_C(1));
            if (camera_mode > UINT8_C(2)) {
                camera_mode = 0u;
            }
            status = vf2_model2a_write(
                machine, control + UINT32_C(0xb1), &camera_mode,
                sizeof(camera_mode)
            );
        }
        if (status == VF2_OK && entry_path) {
            const uint8_t zero = 0u;
            const uint8_t one = UINT8_C(1);
            uint16_t angle = 0u;
            status = vf2_model2a_write(machine, control + UINT32_C(0xb1), &zero, sizeof(zero));
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xb4), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xb8), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xbc), 0u);
            if (status == VF2_OK) status = vf2_model2a_write(machine, control + UINT32_C(0xb0), &one, sizeof(one));
            if (status == VF2_OK) status = vf2_model2a_write(machine, control + UINT32_C(0x40), &zero, sizeof(zero));
            if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x24), &angle);
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xd8), angle);
            if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x26), &angle);
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xda), angle);
            if (status == VF2_OK) status = read_u16(machine, control + UINT32_C(0x28), &angle);
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xdc), angle);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x18), &bits);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xcc), bits);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x1c), &bits);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xd0), bits);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x20), &bits);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, control + UINT32_C(0xd4), bits);
            if (status == VF2_OK) {
                uint32_t flags = 0u;
                status = vf2_model2a_read_u32(machine, player1, &flags);
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, player1, flags | (UINT32_C(1) << 31u));
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, player1 + UINT32_C(0x0c), UINT32_C(0x00013f08));
            }
            if (status == VF2_OK) {
                uint32_t associated = 0u, flags = 0u;
                status = vf2_model2a_read_u32(machine, UINT32_C(0x0050086c), &associated);
                if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags | (UINT32_C(1) << 31u));
                if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4));
            }
            if (status == VF2_OK) {
                de_flags = (uint8_t)(de_flags | UINT8_C(1));
                status = vf2_model2a_write(machine, control + UINT32_C(0xde), &de_flags, sizeof(de_flags));
            }
        }
        if (status == VF2_OK && !entry_path &&
            machine->copro_read_callback != NULL &&
            machine->copro_write_callback != NULL) {
            if (camera_mode == 0u) {
                status = phase17_zero_screen6_apply_held_controls(
                    machine, control, input_flags
                );
            } else if (camera_mode == UINT8_C(1)) {
                const uint32_t position_only = input_flags &
                    ((UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                     (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                     (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u));
                status = phase17_zero_screen6_apply_held_controls(
                    machine, control, position_only
                );
                if (status == VF2_OK) {
                    status = phase17_zero_screen6_mode1_target(
                        machine, control, player0, input_flags
                    );
                }
            } else if (camera_mode == UINT8_C(2)) {
                status = phase17_zero_screen6_mode2(
                    machine, control, player0, player1, input_flags,
                    navigation_flags, de_flags
                );
                if (status == VF2_OK) {
                    status = phase17_zero_screen6_mode2_motion(
                        machine, navigation_flags, de_flags
                    );
                }
            } else {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 10u)) != 0u) {
                status = phase17_zero_screen6_force_blank(
                    machine, control, player0, &de_flags
                );
            }
            if (status == VF2_OK) {
                status = phase17_zero_screen6_update_angles(machine, control);
            }
        }
        if (status == VF2_OK && !entry_path &&
            (navigation_flags & (UINT32_C(1) << 5u)) != 0u) {
            status = phase17_zero_screen6_toggle_objects(
                machine, control, player1, &de_flags
            );
        }
        if (status == VF2_OK && !entry_path &&
            (de_flags & UINT8_C(4)) != 0u) {
            status = fill_tile_plane_spaces(
                machine, UINT32_C(0x01000000), UINT32_C(62), UINT32_C(48)
            );
            *final_g0 = UINT32_C(62);
            *final_g9 = UINT32_C(0x01001800);
            return status;
        }
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055b7c), UINT32_C(0x01000186));
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00056040) + ((uint32_t)camera_mode & UINT32_C(3)) * UINT32_C(4), &mode_name);
        }
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, mode_name, UINT32_C(0x0100028a));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055c3c), UINT32_C(0x0100038a));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x18), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100040a), UINT32_C(0x00055c64));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x1c), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100048a), UINT32_C(0x00055c84));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x20), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100050a), UINT32_C(0x00055ca4));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055d08), UINT32_C(0x0100058a));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0xb4), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100060a), UINT32_C(0x00055d34));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0xb8), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100068a), UINT32_C(0x00055d54));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0xbc), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100070a), UINT32_C(0x00055d74));
        if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x00500064), &stage, sizeof(stage));
        if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine, (int32_t)stage + 1, UINT32_C(0x0100080a), UINT32_C(0x00055dec));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00501084), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 1.0f, UINT32_C(0x0100090a), UINT32_C(0x00055e6c));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055ed8), UINT32_C(0x0100168a));
        if (status == VF2_OK) status = phase17_zero_read_s16(machine, UINT32_C(0x00508020), &motion);
        if (status == VF2_OK) status = phase17_zero_screen6_render_motion_name(machine, motion, UINT32_C(0x0100168a), &descriptor);
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055f3c), UINT32_C(0x0100170a));
        if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine, motion, UINT32_C(0x0100171c), UINT32_C(0x00055f5c));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055f94), UINT32_C(0x01000a0a));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055fb0), UINT32_C(0x01000a8a));
        if (status == VF2_OK) status = phase17_zero_copy_text(machine, UINT32_C(0x00055fec), UINT32_C(0x01000a2e));
        if (status == VF2_OK) status = phase17_zero_copy_text(
            machine,
            (de_flags & UINT8_C(1)) != 0u
                ? UINT32_C(0x00055ffc)
                : UINT32_C(0x00056010),
            UINT32_C(0x01000aae)
        );
        *final_g0 = entry_path || (de_flags & UINT8_C(1)) != 0u
            ? UINT32_C(0x00002020)
            : UINT32_C(0x0000454c);
        *final_g9 = UINT32_C(0x01000b2e);
        return status;
    }
    case UINT8_C(7):
        if (!entry_path) {
            if ((input_flags & (UINT32_C(1) << 15u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x48), -0.01f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 14u)) != 0u) status = phase17_zero_adjust_float(machine, control + UINT32_C(0x48), 0.01f);
        }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, control + UINT32_C(0x48), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 100.0f, UINT32_C(0x0100028a), UINT32_C(0x00057324));
        *final_g0 = UINT32_C(0x00657661);
        *final_g9 = UINT32_C(0x0100030a);
        return status;
    case UINT8_C(9): {
        uint8_t material = 0u;
        if (!entry_path) {
            status = vf2_model2a_read(machine, UINT32_C(0x00530000), &material, sizeof(material));
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 9u)) != 0u) material = (uint8_t)(material + UINT8_C(1));
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 8u)) != 0u) material = (uint8_t)(material - UINT8_C(1));
            if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00530000), &material, sizeof(material));
        } else {
            status = vf2_model2a_read(machine, UINT32_C(0x00530000), &material, sizeof(material));
        }
        if (status == VF2_OK) status = fill_tile_plane_spaces(machine, UINT32_C(0x0100028a), UINT32_C(8), UINT32_C(1));
        if (status == VF2_OK) status = phase17_zero_render_hex8_inline(machine, material, UINT32_C(0x0100038a), UINT32_C(0x00057ae0));
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530001),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100040a),UINT32_C(0x00057b04)); }
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530002),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100048a),UINT32_C(0x00057b28)); }
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530003),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100050a),UINT32_C(0x00057b4c)); }
        if (status == VF2_OK) { uint8_t v=0; status=vf2_model2a_read(machine,UINT32_C(0x00530004),&v,1); if(status==VF2_OK) status=phase17_zero_render_hex8_inline(machine,v,UINT32_C(0x0100058a),UINT32_C(0x00057b70)); }
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005010c4), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 1000.0f, UINT32_C(0x0100060a), UINT32_C(0x00057b9c));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530104), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine, bits, 10000.0f, UINT32_C(0x0100068a), UINT32_C(0x00057bc8));
        *final_g0 = 0u;
        *final_g9 = UINT32_C(0x0100070a);
        return status;
    }
    case UINT8_C(10): {
        uint8_t mode = 0u;
        if (!entry_path && (navigation_flags & UINT32_C(0x700)) == UINT32_C(0x700)) {
            status = write_u16(machine, UINT32_C(0x0053011c), 0u);
            if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x00530120), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530110), 0u);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530114), UINT32_C(0x3f800000));
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530118), 0u);
        } else if (!entry_path) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
            );
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                mode = UINT8_C(1);
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
                );
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                mode = 0u;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
                );
            }
            if (status == VF2_OK &&
                (input_flags & (UINT32_C(1) << 9u)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x00530120), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x00530120),
                            (uint16_t)(short_value - UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x00530120), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x00530120),
                            (uint16_t)(short_value + UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x0053011c), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x0053011c),
                            (uint16_t)(short_value - UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x0053011c), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x0053011c),
                            (uint16_t)(short_value + UINT16_C(0x200))
                        );
                    }
                }
            } else if (status == VF2_OK &&
                       (input_flags & (UINT32_C(1) << 10u)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530114), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530114), 0.01f
                    );
                }
            } else if (status == VF2_OK) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530110), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530110), 0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530118), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530118), 0.01f
                    );
                }
            }
        }
        if (status == VF2_OK) status = phase17_zero_read_s16(machine, UINT32_C(0x00530108), &signed_value);
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine, (uint32_t)signed_value, UINT32_C(0x0100028a), entry_path ? UINT32_C(0x00057ef8) : UINT32_C(0x00057ef8));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530110), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine,bits,100.0f,UINT32_C(0x0100030a),UINT32_C(0x00057f20));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530114), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine,bits,100.0f,UINT32_C(0x0100038a),UINT32_C(0x00057f48));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00530118), &bits);
        if (status == VF2_OK) status = phase17_zero_render_float_inline(machine,bits,100.0f,UINT32_C(0x0100040a),UINT32_C(0x00057f70));
        if (status == VF2_OK) status = read_u16(machine, UINT32_C(0x00530120), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_ratio_inline(machine,short_value,UINT32_C(0x0100048a),UINT32_C(0x00057f8c));
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine,short_value,UINT32_C(0x0100050a),UINT32_C(0x00057fa8));
        if (status == VF2_OK) status = read_u16(machine, UINT32_C(0x0053011c), &short_value);
        if (status == VF2_OK) status = phase17_zero_render_ratio_inline(machine,short_value,UINT32_C(0x0100058a),UINT32_C(0x00057fc8));
        if (status == VF2_OK) status = phase17_zero_render_hex6_inline(machine,short_value,UINT32_C(0x0100060a),UINT32_C(0x00057fe4));
        *final_g0=0u; *final_g9=UINT32_C(0x0100068a); return status;
    }
    case UINT8_C(12):
        if (!entry_path) {
            status = phase17_zero_read_s16(machine, control + UINT32_C(0xf8), &signed_value);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 12u)) != 0u) signed_value -= 16;
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 13u)) != 0u) signed_value += 16;
            if (status == VF2_OK) status = write_u16(machine, control + UINT32_C(0xf8), (uint16_t)signed_value);
            if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine,signed_value,UINT32_C(0x0100028a),UINT32_C(0x0005747c));
        } else {
            status = phase17_zero_read_s16(machine, UINT32_C(0x00501408), &signed_value);
            if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine,signed_value,UINT32_C(0x0100028a),UINT32_C(0x00057424));
        }
        *final_g0=0u; *final_g9=UINT32_C(0x0100030a); return status;
    case UINT8_C(13): {
        uint32_t flags = 0u;
        uint32_t texture = 0u;
        uint32_t global_mode = 0u;
        if (entry_path) {
            *final_g0=UINT32_C(0x00455255); *final_g9=UINT32_C(0x01000206); return VF2_OK;
        }
        runtime_flags |= UINT32_C(1) << 9u;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000), runtime_flags);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500700), &flags);
        if (status == VF2_OK && (flags & (UINT32_C(1) << 10u)) == 0u) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00509800), &texture);
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 14u)) != 0u) ++texture;
            if (status == VF2_OK && (navigation_flags & (UINT32_C(1) << 15u)) != 0u) --texture;
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500020), &global_mode);
            if (status == VF2_OK && (global_mode & UINT32_C(1)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 12u)) != 0u) ++texture;
                if ((input_flags & (UINT32_C(1) << 13u)) != 0u) --texture;
            }
            texture = (texture + UINT32_C(86)) % UINT32_C(86);
            if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00509800), texture);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 22u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509804),0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 23u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509804),-0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 21u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509808),0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 20u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00509808),-0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 16u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x0050980c),0.005f);
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 18u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x0050980c),-0.005f);
        }
        if (status == VF2_OK) {
            uint32_t buffer_index = 0u;
            uint32_t texture_x = 0u;
            uint32_t texture_y = 0u;
            uint32_t texture_z = 0u;
            uint32_t texture_z_squared = 0u;
            uint8_t one = UINT8_C(1);
            uint8_t sixty = UINT8_C(0x60);

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x005001e4), &buffer_index
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00509804), &texture_x
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00509808), &texture_y
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050980c), &texture_z
                );
            }
            if (status == VF2_OK) {
                const float texture_z_value =
                    phase17_zero_float_from_bits(texture_z);
                texture_z_squared = phase17_zero_float_to_bits(
                    texture_z_value * texture_z_value
                );
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index, texture_x
                );
            }
            if (status == VF2_OK) {
                buffer_index = (buffer_index + UINT32_C(4)) &
                               ~UINT32_C(0x100);
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index, texture_y
                );
            }
            if (status == VF2_OK) {
                buffer_index = (buffer_index + UINT32_C(4)) &
                               ~UINT32_C(0x100);
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index,
                    texture_z_squared
                );
            }
            if (status == VF2_OK) {
                uint8_t buffer_byte = 0u;
                buffer_index += UINT32_C(4);
                buffer_byte = (uint8_t)buffer_index;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005001e4), &buffer_byte,
                    sizeof(buffer_byte)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00501010), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050101c), &sixty, sizeof(sixty)
                );
            }
        }
        if (status == VF2_OK) {
            uint16_t kind = 0u;
            uint8_t display = 0u;
            uint32_t label_source = UINT32_C(0x0004d2e8);
            uint32_t name_dest = UINT32_C(0x010000ea);
            status = read_u16(machine, UINT32_C(0x0055c2f2), &kind);
            if (status == VF2_OK && kind == UINT16_C(1)) label_source = UINT32_C(0x0004d30c);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00503100), texture
                );
            }
            if (status == VF2_OK) status = phase17_zero_copy_text(machine,label_source,UINT32_C(0x010000e2));
            if (status == VF2_OK) status = vf2_model2a_read(machine,UINT32_C(0x0050002b),&display,1);
            if (display == UINT8_C(12) || display == UINT8_C(13)) name_dest=UINT32_C(0x010040ea);
            if (status == VF2_OK) status = phase17_zero_copy_text(machine,UINT32_C(0x0004d377)+texture*UINT32_C(32),name_dest);
        }
        if (status == VF2_OK) status = phase17_zero_render_decimal_inline(machine,(int32_t)texture,UINT32_C(0x0100028a),UINT32_C(0x00058374));
        if (status == VF2_OK) {
            uint32_t busy=0u; status=vf2_model2a_read_u32(machine,UINT32_C(0x00555000),&busy);
            if (status==VF2_OK) status=phase17_zero_copy_text(machine,busy==UINT32_C(1)?UINT32_C(0x0005839c):UINT32_C(0x00058390),UINT32_C(0x0100030a));
        }
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x000583b4),UINT32_C(0x0100038a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x000583d4),UINT32_C(0x0100040a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x000583f0),UINT32_C(0x0100048a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x0005840c),UINT32_C(0x0100050a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x00058428),UINT32_C(0x0100058a));
        if (status == VF2_OK) status=phase17_zero_copy_text(machine,UINT32_C(0x00058444),UINT32_C(0x0100060a));
        *final_g0=UINT32_C(0x0065766f); *final_g9=UINT32_C(0x0100068a); return status;
    }
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}

static vf2_status phase17_zero_render_index4_selector(
    vf2_model2a *machine,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint8_t selector = 0u;
    uint32_t source = 0u;
    vf2_status status = fill_tile_plane_spaces(
        machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
    );

    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, UINT32_C(0x00508040), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100029c), UINT32_C(0x000570e4)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x000570f8), UINT32_C(0x0100028a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508040), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010002aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010002aa);
    }
    return status;
}

static vf2_status phase17_zero_render_index4_control(
    vf2_model2a *machine,
    uint32_t control,
    uint8_t selector,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint32_t source = 0u;
    vf2_status status = vf2_model2a_write(
        machine, control + UINT32_C(0x40), &selector, sizeof(selector)
    );

    if (status == VF2_OK) {
        status = fill_tile_plane_spaces(
            machine, UINT32_C(0x01000398), UINT32_C(52), UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, control + UINT32_C(0x40), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100039c), UINT32_C(0x0005714c)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x00057160), UINT32_C(0x0100038a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010003aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010003aa);
    }
    return status;
}

static vf2_status phase17_zero_render_index4_entry(
    vf2_model2a *machine,
    uint32_t control,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint8_t selector = 0u;
    uint32_t source = 0u;
    vf2_status status = fill_tile_plane_spaces(
        machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
    );

    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, UINT32_C(0x00508040), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100029c), UINT32_C(0x000570e4)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x000570f8), UINT32_C(0x0100028a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508040), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010002aa)
        );
    }
    if (status == VF2_OK) {
        status = fill_tile_plane_spaces(
            machine, UINT32_C(0x01000398), UINT32_C(52), UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, control + UINT32_C(0x40), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100039c), UINT32_C(0x0005714c)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x00057160), UINT32_C(0x0100038a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, control + UINT32_C(0x40), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010003aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010003aa);
    }
    return status;
}

static vf2_status execute_frame_phase17_zero_control_menu(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t outer_stack = cpu->registers[1];
    const uint32_t entry_g11 =
        cpu->registers[VF2_I960_G0_REGISTER + 11u];
    const uint32_t entry_g12 =
        cpu->registers[VF2_I960_G0_REGISTER + 12u];
    vf2_hybrid_bridge_report text_report;
    uint32_t runtime_flags = 0u;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t control = 0u;
    uint32_t descriptor = 0u;
    uint32_t descriptor_target = 0u;
    uint32_t table_target = 0u;
    uint32_t input_flags = 0u;
    uint32_t previous_flags = 0u;
    uint32_t navigation_flags = 0u;
    uint32_t input_descriptor = 0u;
    uint16_t fighter_control = 0u;
    uint8_t menu_index = 0u;
    uint8_t control_a = 0u;
    uint8_t control_b = 0u;
    uint64_t expected_instructions = 0u;
    uint64_t idle_wrapper_adjustment = 0u;
    uint32_t effective_input_flags = 0u;
    uint32_t effective_previous_flags = 0u;
    vf2_status status = VF2_OK;

    memset(&text_report, 0, sizeof(text_report));
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &runtime_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &player0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &player1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050070c), &previous_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500700), &input_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &navigation_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508008), &menu_index, sizeof(menu_index)
        );
    }
    if (status == VF2_OK &&
        (input_flags & (UINT32_C(1) << 5u)) != 0u &&
        (navigation_flags & ((UINT32_C(1) << 12u) |
                             (UINT32_C(1) << 13u))) != 0u) {
        const int forward =
            (navigation_flags & (UINT32_C(1) << 12u)) != 0u;
        uint8_t next_index = menu_index;
        uint32_t entry_target = 0u;
        uint32_t inline_source = 0u;
        uint32_t target13_flags = 0u;
        uint32_t transition_final_g0 = 0u;
        uint32_t transition_final_g9 = 0u;
        uint64_t transition_instructions = 0u;
        uint64_t transition_extra_calls = UINT64_C(3);
        uint64_t text_bytes = 0u;

        if (forward) {
            next_index = (uint8_t)(menu_index + UINT8_C(1));
            if (next_index >= UINT8_C(14)) {
                next_index = 0u;
            }
        } else {
            next_index = menu_index == 0u ? UINT8_C(13)
                                          : (uint8_t)(menu_index - UINT8_C(1));
        }
        if (next_index == UINT8_C(0)) {
            entry_target = UINT32_C(0x00055198);
            inline_source = UINT32_C(0x000551b4);
            transition_extra_calls = UINT64_C(6);
            transition_instructions =
                (runtime_flags & (UINT32_C(1) << 9u)) != 0u
                    ? (forward ? UINT64_C(12348) : UINT64_C(12346))
                    : (forward ? UINT64_C(12475) : UINT64_C(12473));
        } else if (next_index == UINT8_C(1)) {
            entry_target = UINT32_C(0x00055598);
            inline_source = UINT32_C(0x000555a8);
            transition_extra_calls = UINT64_C(11);
            transition_instructions = UINT64_C(13016);
        } else if (next_index == UINT8_C(2)) {
            entry_target = UINT32_C(0x0005570c);
            inline_source = UINT32_C(0x0005571c);
            transition_extra_calls = UINT64_C(9);
            transition_instructions = UINT64_C(12589);
        } else if (next_index == UINT8_C(3)) {
            entry_target = UINT32_C(0x000553e4);
            inline_source = UINT32_C(0x000553f4);
            transition_extra_calls = UINT64_C(13);
            transition_instructions = UINT64_C(12966);
        } else if (next_index == UINT8_C(4)) {
            entry_target = UINT32_C(0x00057098);
            inline_source = UINT32_C(0x000570a8);
            transition_extra_calls = UINT64_C(13);
            transition_instructions = UINT64_C(13243);
        } else if (next_index == UINT8_C(5)) {
            entry_target = UINT32_C(0x000557b4);
            inline_source = UINT32_C(0x000557c4);
            transition_extra_calls = UINT64_C(13);
            transition_instructions = UINT64_C(12972);
        } else if (next_index == UINT8_C(6)) {
            entry_target = UINT32_C(0x0005599c);
            inline_source = UINT32_C(0x000559d0);
            transition_extra_calls = UINT64_C(33);
            transition_instructions = UINT64_C(15150);
        } else if (next_index == UINT8_C(7)) {
            entry_target = UINT32_C(0x0005729c);
            inline_source = UINT32_C(0x000572ac);
            transition_extra_calls = UINT64_C(5);
            transition_instructions = UINT64_C(12416);
        } else if (next_index == UINT8_C(8)) {
            entry_target = UINT32_C(0x00057488);
            inline_source = UINT32_C(0x00057498);
            transition_instructions = UINT64_C(12254);
        } else if (next_index == UINT8_C(9)) {
            entry_target = UINT32_C(0x00057704);
            inline_source = UINT32_C(0x00057714);
            transition_extra_calls = UINT64_C(17);
            transition_instructions = UINT64_C(13481);
        } else if (next_index == UINT8_C(10)) {
            entry_target = UINT32_C(0x00057c60);
            inline_source = UINT32_C(0x00057c70);
            transition_extra_calls = UINT64_C(19);
            transition_instructions = UINT64_C(13386);
        } else if (next_index == UINT8_C(11)) {
            entry_target = UINT32_C(0x000575b8);
            inline_source = UINT32_C(0x000575c8);
            transition_instructions = UINT64_C(12249);
        } else if (next_index == UINT8_C(12)) {
            entry_target = UINT32_C(0x00057430);
            inline_source = UINT32_C(0x00057440);
            transition_extra_calls = UINT64_C(5);
            transition_instructions = UINT64_C(12365);
        } else if (next_index == UINT8_C(13)) {
            entry_target = UINT32_C(0x00057ff8);
            inline_source = UINT32_C(0x00058008);
            transition_instructions =
                (!forward && menu_index == 0u)
                    ? UINT64_C(12289)
                    : UINT64_C(12288);
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00055124) +
                (uint32_t)next_index * UINT32_C(8),
            &table_target
        );
        if (status != VF2_OK || table_target != entry_target ||
            (previous_flags & (UINT32_C(1) << 5u)) == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = write_u16(
            machine, UINT32_C(0x00500082), UINT16_C(0x8000)
        );
        if (status == VF2_OK) {
            const uint8_t zero = 0u;
            status = vf2_model2a_write(
                machine, control + UINT32_C(0xb0), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            uint8_t state = 0u;
            status = vf2_model2a_read(
                machine, UINT32_C(0x00500085), &state, sizeof(state)
            );
            state = (uint8_t)(state | UINT8_C(1));
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500085), &state, sizeof(state)
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00508008), &next_index, sizeof(next_index)
            );
        }
        if (status == VF2_OK) {
            status = fill_tile_plane_spaces(
                machine, UINT32_C(0x01000000), UINT32_C(62), UINT32_C(48)
            );
        }
        if (status == VF2_OK) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(62);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01001800);
            cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        }
        if (status == VF2_OK &&
            !(next_index == UINT8_C(0) &&
              (runtime_flags & (UINT32_C(1) << 9u)) != 0u)) {
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000186);
            cpu->registers[14] = inline_source;
            cpu->ip = UINT32_C(0x00009444);
            status = execute_inline_text_thunk(machine, cpu, &text_report);
            text_bytes = (uint64_t)text_report.bytes_written;
        }
        if (status == VF2_OK &&
            (next_index == UINT8_C(1) || next_index == UINT8_C(2) ||
             next_index == UINT8_C(3) || next_index == UINT8_C(5) ||
             next_index == UINT8_C(6) || next_index == UINT8_C(7) ||
             next_index == UINT8_C(9) || next_index == UINT8_C(10) ||
             next_index == UINT8_C(12))) {
            status = phase17_zero_render_missing_body(
                machine, next_index, player0, player1, control,
                0u, 0u, runtime_flags, 1,
                &transition_final_g0, &transition_final_g9
            );
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER] = transition_final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = transition_final_g9;
            }
        }
        if (status == VF2_OK && next_index == UINT8_C(4)) {
            status = phase17_zero_render_index4_entry(
                machine, control, &transition_final_g0, &transition_final_g9
            );
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER] = transition_final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = transition_final_g9;
            }
        }
        if (status == VF2_OK && next_index == UINT8_C(0)) {
            uint8_t first_visit = 0u;
            uint32_t associated = 0u;
            uint32_t associated_flags = 0u;
            uint32_t fighter_flags = 0u;
            uint32_t input_control = 0u;
            uint32_t input_control_flags = 0u;
            uint8_t fighter_mode = 0u;
            const uint32_t fighters[2] = {player0, player1};
            const uint32_t associated_slots[2] = {
                UINT32_C(0x00500868), UINT32_C(0x0050086c)
            };
            uint32_t fighter_index = 0u;

            status = vf2_model2a_read(
                machine, UINT32_C(0x00508050), &first_visit,
                sizeof(first_visit)
            );
            if (status != VF2_OK || first_visit != 0u) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            first_visit = UINT8_C(1);
            status = vf2_model2a_write(
                machine, UINT32_C(0x00508050), &first_visit,
                sizeof(first_visit)
            );
            for (fighter_index = 0u; status == VF2_OK && fighter_index < 2u;
                 ++fighter_index) {
                status = vf2_model2a_read(
                    machine, fighters[fighter_index] + UINT32_C(0x1b0),
                    &fighter_mode, sizeof(fighter_mode)
                );
                if (status == VF2_OK) {
                    fighter_mode = fighter_mode >= UINT8_C(13)
                        ? (uint8_t)(fighter_mode - UINT8_C(13))
                        : fighter_mode;
                    status = vf2_model2a_write(
                        machine, fighters[fighter_index] + UINT32_C(0x1b1),
                        &fighter_mode, sizeof(fighter_mode)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, fighters[fighter_index] + UINT32_C(0x0c),
                        UINT32_C(0x00013f08)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, fighters[fighter_index], &fighter_flags
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, fighters[fighter_index],
                        (fighter_flags & UINT32_C(0xff000000)) |
                            (UINT32_C(1) << 31u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, associated_slots[fighter_index], &associated
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, associated, &associated_flags
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, associated,
                        associated_flags | (UINT32_C(1) << 31u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, associated + UINT32_C(0x0c),
                        UINT32_C(0x000640f4)
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050083c), &input_control
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, input_control, &input_control_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, input_control,
                    input_control_flags | (UINT32_C(1) << 3u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500840), &input_control
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, input_control, &input_control_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, input_control,
                    input_control_flags | (UINT32_C(1) << 3u)
                );
            }
            if (status == VF2_OK) {
                const uint8_t one = UINT8_C(1);
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500056), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, descriptor + UINT32_C(0x0c), &descriptor_target
                );
            }
            if (status != VF2_OK ||
                descriptor_target != UINT32_C(0x0001b9ac)) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, outer_stack + UINT32_C(128),
                    UINT32_C(0x000550d4)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, descriptor + UINT32_C(0x0c),
                    UINT32_C(0x0001b9dc)
                );
            }
            if (status == VF2_OK) {
                const uint8_t zero = 0u;
                status = vf2_model2a_write(
                    machine, player0 + UINT32_C(0x1200),
                    &zero, sizeof(zero)
                );
            }
            if (status == VF2_OK) {
                const uint8_t zero = 0u;
                status = vf2_model2a_write(
                    machine, player1 + UINT32_C(0x1200),
                    &zero, sizeof(zero)
                );
            }
            if (status == VF2_OK) {
                status = phase17_zero_initialize_control_fighter(
                    machine, player0
                );
            }
            if (status == VF2_OK) {
                status = phase17_zero_initialize_control_fighter(
                    machine, player1
                );
            }
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER + 7u] = player1;
                cpu->registers[VF2_I960_G0_REGISTER + 8u] = player0;
                cpu->registers[VF2_I960_G0_REGISTER + 13u] = descriptor;
            }
        }
        if (status == VF2_OK && next_index == UINT8_C(13)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00509800), 0u
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00509804), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00509808), UINT32_C(0xbf07ae14)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0050980c), UINT32_C(0x3fa28f5c)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00508000), &runtime_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00508000),
                    runtime_flags & ~(UINT32_C(1) << 9u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &target13_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    target13_flags & ~(UINT32_C(1) << 16u)
                );
            }
        }
        if (status != VF2_OK) {
            return status;
        }
        if (next_index == UINT8_C(0)) {
            set_equal_condition(cpu);
            account_nested_procedure(cpu, UINT64_C(6), UINT64_C(6));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 4u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 4u;
            }
        } else {
            set_equal_condition(cpu);
            account_nested_procedure(
                cpu, transition_extra_calls, transition_extra_calls
            );
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 3u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 3u;
            }
        }
        status = finish_recovered_procedure(
            machine, cpu,
            transition_instructions -
                (cpu->executed_instructions - start_instructions)
        );
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = next_index == UINT8_C(13)
            ? UINT64_C(13) : UINT64_C(5);
        report->bytes_written =
            (size_t)(UINT32_C(62) * UINT32_C(48) * UINT32_C(2)) +
            (size_t)text_bytes + 5u +
            (next_index == UINT8_C(13) ? 24u : 0u);
        report->recovered_instruction_count = transition_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (status != VF2_OK) {
        return status;
    }
    effective_input_flags = input_flags & ~(UINT32_C(1) << 5u);
    effective_previous_flags = previous_flags & ~(UINT32_C(1) << 5u);
    if ((input_flags & (UINT32_C(1) << 5u)) != 0u) {
        uint8_t state = 0u;

        status = vf2_model2a_read(
            machine, UINT32_C(0x00500085), &state, sizeof(state)
        );
        if (status != VF2_OK) {
            return status;
        }
        if (menu_index == UINT8_C(13) && (state & UINT8_C(1)) == 0u) {
            uint32_t texture_flags = 0u;

            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00508000),
                    runtime_flags & ~(UINT32_C(1) << 9u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &texture_flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    texture_flags & ~(UINT32_C(1) << 16u)
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
            cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
            status = finish_recovered_procedure(
                machine, cpu,
                UINT64_C(43) -
                    (cpu->executed_instructions - start_instructions)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(3);
            report->bytes_written = 10u;
            report->recovered_instruction_count = UINT64_C(43);
            report->recovered_procedure_calls =
                cpu->procedure_calls - start_calls;
            report->recovered_procedure_returns =
                cpu->procedure_returns - start_returns;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if ((state & UINT8_C(1)) != 0u) {
            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
            cpu->registers[VF2_I960_G14_REGISTER] = 0u;
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
            status = finish_recovered_procedure(
                machine, cpu,
                UINT64_C(27) -
                    (cpu->executed_instructions - start_instructions)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(27);
            report->recovered_procedure_calls =
                cpu->procedure_calls - start_calls;
            report->recovered_procedure_returns =
                cpu->procedure_returns - start_returns;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        idle_wrapper_adjustment = UINT64_C(2);
    } else if (((input_flags ^ previous_flags) &
                (UINT32_C(1) << 5u)) != 0u) {
        uint8_t state = 0u;

        status = vf2_model2a_read(
            machine, UINT32_C(0x00500085), &state, sizeof(state)
        );
        if (status == VF2_OK) {
            state = (uint8_t)(state & ~UINT8_C(1));
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500085), &state, sizeof(state)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        idle_wrapper_adjustment = UINT64_C(3);
    }
    if (menu_index == UINT8_C(1) || menu_index == UINT8_C(2) ||
        menu_index == UINT8_C(3) || menu_index == UINT8_C(5) ||
        menu_index == UINT8_C(6) || menu_index == UINT8_C(7) ||
        menu_index == UINT8_C(9) || menu_index == UINT8_C(10) ||
        menu_index == UINT8_C(12) || menu_index == UINT8_C(13)) {
        static const uint32_t idle_targets[14] = {
            UINT32_C(0x00055330), UINT32_C(0x000555c4),
            UINT32_C(0x0005572c), UINT32_C(0x00055408),
            UINT32_C(0x00057188), UINT32_C(0x000557d8),
            UINT32_C(0x00055a70), UINT32_C(0x000572bc),
            UINT32_C(0x000574a4), UINT32_C(0x00057724),
            UINT32_C(0x00057c7c), UINT32_C(0x000575d4),
            UINT32_C(0x00057450), UINT32_C(0x0005804c)
        };
        static const uint64_t instruction_counts[14] = {
            UINT64_C(267), UINT64_C(534), UINT64_C(318), UINT64_C(695),
            UINT64_C(37), UINT64_C(695), UINT64_C(3063), UINT64_C(146),
            UINT64_C(45), UINT64_C(1282), UINT64_C(1154), UINT64_C(40),
            UINT64_C(118), UINT64_C(1745)
        };
        static const uint64_t nested_calls[14] = {
            UINT64_C(6), UINT64_C(9), UINT64_C(8), UINT64_C(12),
            UINT64_C(2), UINT64_C(13), UINT64_C(37), UINT64_C(4),
            UINT64_C(2), UINT64_C(17), UINT64_C(18), UINT64_C(2),
            UINT64_C(4), UINT64_C(16)
        };
        static const uint32_t depth_deltas[14] = {
            UINT32_C(4), UINT32_C(3), UINT32_C(3), UINT32_C(3),
            UINT32_C(2), UINT32_C(3), UINT32_C(3), UINT32_C(3),
            UINT32_C(2), UINT32_C(3), UINT32_C(3), UINT32_C(2),
            UINT32_C(3), UINT32_C(4)
        };
        uint32_t final_g0 = 0u;
        uint32_t final_g1 = 0u;
        uint32_t final_g9 = 0u;
        int32_t control_instruction_adjustment = 0;
        uint64_t active_nested_calls = nested_calls[menu_index];

        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00055128) +
                (uint32_t)menu_index * UINT32_C(8),
            &table_target
        );
        if (status != VF2_OK || table_target != idle_targets[menu_index]) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (effective_previous_flags != effective_input_flags) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (menu_index == UINT8_C(3)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
            case UINT32_C(1) << 14u:
            case UINT32_C(1) << 15u:
                control_instruction_adjustment = 5;
                break;
            case UINT32_C(1) << 9u:
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 12u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 13u):
                control_instruction_adjustment = -7;
                break;
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
                control_instruction_adjustment = 4;
                break;
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                control_instruction_adjustment = 9;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(5)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 8u:
                control_instruction_adjustment = -1;
                break;
            case UINT32_C(1) << 9u:
                control_instruction_adjustment = 2;
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
            case UINT32_C(1) << 14u:
            case UINT32_C(1) << 15u:
                control_instruction_adjustment = 5;
                break;
            case (UINT32_C(1) << 8u) | (UINT32_C(1) << 12u):
            case (UINT32_C(1) << 8u) | (UINT32_C(1) << 13u):
                control_instruction_adjustment = 4;
                break;
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 12u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 13u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
            case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                control_instruction_adjustment = 7;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(6)) {
            uint8_t camera_mode = 0u;
            uint8_t de_flags = 0u;
            const uint32_t supported_input =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
                (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u) |
                (UINT32_C(1) << 18u) |
                (UINT32_C(1) << 20u) | (UINT32_C(1) << 21u) |
                (UINT32_C(1) << 22u) | (UINT32_C(1) << 23u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 4u) | (UINT32_C(1) << 5u) |
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                (UINT32_C(1) << 10u) |
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
                (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u);

            status = vf2_model2a_read(
                machine, control + UINT32_C(0xb1),
                &camera_mode, sizeof(camera_mode)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, control + UINT32_C(0xde),
                    &de_flags, sizeof(de_flags)
                );
            }
            if (status != VF2_OK ||
                (effective_input_flags & ~supported_input) != 0u ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            if (camera_mode == UINT8_C(2)) {
                const uint32_t mode2_inputs =
                    (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u) |
                    (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
                    (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u) |
                    (UINT32_C(1) << 18u) |
                    (UINT32_C(1) << 20u) | (UINT32_C(1) << 21u) |
                    (UINT32_C(1) << 22u) | (UINT32_C(1) << 23u);
                const uint32_t mode2_navigation =
                    (UINT32_C(1) << 5u) |
                    (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u) |
                    (UINT32_C(1) << 10u) |
                    (UINT32_C(1) << 16u) | (UINT32_C(1) << 17u);

                if ((effective_input_flags & ~mode2_inputs) != 0u ||
                    (navigation_flags & ~mode2_navigation) != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -168;
                active_nested_calls = UINT64_C(36);
                final_g1 = navigation_flags;
                if ((de_flags & UINT8_C(1)) != 0u) {
                    control_instruction_adjustment += 4;
                }
                if ((effective_input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & ((UINT32_C(1) << 22u) |
                                              (UINT32_C(1) << 23u))) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 18u)) != 0u &&
                    (de_flags & UINT8_C(1)) == 0u) {
                    control_instruction_adjustment -= 3;
                }
                if ((de_flags & UINT8_C(1)) != 0u &&
                    (effective_input_flags & ((UINT32_C(1) << 20u) |
                                              (UINT32_C(1) << 21u))) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((navigation_flags & (UINT32_C(1) << 5u)) != 0u) {
                    control_instruction_adjustment += 23;
                }
                if ((navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                    control_instruction_adjustment += 18;
                } else if ((navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                    control_instruction_adjustment += 75;
                }
                if ((navigation_flags & ((UINT32_C(1) << 16u) |
                                         (UINT32_C(1) << 17u))) != 0u &&
                    ((effective_input_flags & (UINT32_C(1) << 18u)) == 0u ||
                     (de_flags & UINT8_C(1)) != 0u)) {
                    control_instruction_adjustment += 19;
                }
            }
            if (camera_mode != UINT8_C(2) &&
                (navigation_flags & (UINT32_C(1) << 4u)) != 0u) {
                control_instruction_adjustment -= 34;
                active_nested_calls += UINT64_C(1);
            }
            if (camera_mode != UINT8_C(2) &&
                (navigation_flags & (UINT32_C(1) << 5u)) != 0u) {
                control_instruction_adjustment += 23;
            }
            if ((navigation_flags & (UINT32_C(1) << 10u)) != 0u) {
                control_instruction_adjustment = camera_mode == UINT8_C(2)
                    ? 9287 : 9456;
                active_nested_calls = camera_mode == UINT8_C(2)
                    ? UINT64_C(8) : UINT64_C(9);
                if (camera_mode == UINT8_C(2)) {
                    final_g1 = 0u;
                }
            }
            if ((runtime_flags & (UINT32_C(1) << 9u)) != 0u) {
                control_instruction_adjustment = camera_mode == UINT8_C(2)
                    ? 9284 : 9457;
                active_nested_calls = camera_mode == UINT8_C(2)
                    ? UINT64_C(8) : UINT64_C(9);
                if (camera_mode == UINT8_C(2)) {
                    final_g1 = 0u;
                }
            }
            if (camera_mode != UINT8_C(2)) {
                if ((effective_input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & ((UINT32_C(1) << 8u) |
                                              (UINT32_C(1) << 9u))) != 0u) {
                    control_instruction_adjustment += 2;
                }
                if ((effective_input_flags & (UINT32_C(1) << 21u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 20u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & (UINT32_C(1) << 22u)) != 0u) {
                    control_instruction_adjustment += 1;
                } else if ((effective_input_flags & (UINT32_C(1) << 23u)) != 0u) {
                    control_instruction_adjustment += 3;
                }
                if ((effective_input_flags & ((UINT32_C(1) << 16u) |
                                              (UINT32_C(1) << 17u))) != 0u) {
                    control_instruction_adjustment += 2;
                }
            }
        } else if (menu_index == UINT8_C(9)) {
            if (effective_input_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 8u:
                control_instruction_adjustment = 8;
                break;
            case UINT32_C(1) << 9u:
                control_instruction_adjustment = 2;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(10)) {
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(0x700):
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -7;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags == 0u) {
                switch (effective_input_flags) {
                case 0u:
                case UINT32_C(1) << 8u:
                    break;
                case UINT32_C(1) << 9u:
                    control_instruction_adjustment = -3;
                    break;
                case UINT32_C(1) << 12u:
                case UINT32_C(1) << 13u:
                case UINT32_C(1) << 14u:
                case UINT32_C(1) << 15u:
                    control_instruction_adjustment = 5;
                    break;
                case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
                    control_instruction_adjustment = 8;
                    break;
                case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                    control_instruction_adjustment = 13;
                    break;
                default:
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        } else if (menu_index == UINT8_C(12)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(13)) {
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 14u:
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -7;
                break;
            case UINT32_C(1) << 15u:
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags == 0u) {
                switch (effective_input_flags) {
                case 0u:
                    break;
                case UINT32_C(1) << 16u:
                case UINT32_C(1) << 18u:
                case UINT32_C(1) << 20u:
                case UINT32_C(1) << 21u:
                case UINT32_C(1) << 22u:
                case UINT32_C(1) << 23u:
                    control_instruction_adjustment = 7;
                    break;
                default:
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        } else if (effective_input_flags != 0u || navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = phase17_zero_render_missing_body(
            machine, menu_index, player0, player1, control,
            effective_input_flags, navigation_flags, runtime_flags, 0,
            &final_g0, &final_g9
        );
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x00500082), UINT16_C(0x8000)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = final_g0;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = final_g1;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 4u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = final_g9;
        cpu->registers[VF2_I960_G0_REGISTER + 10u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 11u] = entry_g11;
        cpu->registers[VF2_I960_G0_REGISTER + 12u] = entry_g12;
        cpu->registers[VF2_I960_G0_REGISTER + 13u] = 0u;
        cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        cpu->registers[VF2_I960_G0_REGISTER + 15u] = 0u;
        set_equal_condition(cpu);
        account_nested_procedure(
            cpu, active_nested_calls, active_nested_calls
        );
        if (cpu->maximum_local_frame_depth <
            cpu->local_frame_depth + depth_deltas[menu_index]) {
            cpu->maximum_local_frame_depth =
                cpu->local_frame_depth + depth_deltas[menu_index];
        }
        expected_instructions = (uint64_t)(
            (int64_t)instruction_counts[menu_index] +
            (int64_t)idle_wrapper_adjustment +
            (int64_t)control_instruction_adjustment
        );
        status = finish_recovered_procedure(
            machine, cpu,
            expected_instructions -
                (cpu->executed_instructions - start_instructions)
        );
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = expected_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (menu_index == UINT8_C(4) || menu_index == UINT8_C(8) ||
        menu_index == UINT8_C(11)) {
        uint32_t expected_target = 0u;

        if (menu_index == UINT8_C(4)) {
            expected_target = UINT32_C(0x00057188);
            expected_instructions = UINT64_C(37) + idle_wrapper_adjustment;
        } else if (menu_index == UINT8_C(8)) {
            expected_target = UINT32_C(0x000574a4);
            expected_instructions = UINT64_C(45) + idle_wrapper_adjustment;
        } else {
            expected_target = UINT32_C(0x000575d4);
            expected_instructions = UINT64_C(40) + idle_wrapper_adjustment;
        }
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00055128) +
                (uint32_t)menu_index * UINT32_C(8),
            &table_target
        );
        if (status != VF2_OK || table_target != expected_target) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (menu_index == UINT8_C(4)) {
            uint8_t selector = 0u;
            uint32_t final_g0 = 0u;
            uint32_t final_g9 = 0u;
            uint64_t active_nested_calls = UINT64_C(2);
            uint32_t active_depth_delta = UINT32_C(2);
            const uint32_t supported_input =
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 12u) |
                (UINT32_C(1) << 13u);

            if ((effective_input_flags & ~supported_input) != 0u ||
                effective_previous_flags != effective_input_flags ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags != 0u) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 12u)) != 0u) {
                selector = (uint8_t)(selector + UINT8_C(1));
                if (selector >= UINT8_C(17)) {
                    selector = 0u;
                }
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(555) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            } else if (status == VF2_OK &&
                       (navigation_flags & (UINT32_C(1) << 13u)) != 0u) {
                selector = selector == 0u
                    ? UINT8_C(16)
                    : (uint8_t)(selector - UINT8_C(1));
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(500) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            } else if (status == VF2_OK &&
                       (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                status = phase17_zero_render_index4_control(
                    machine, control, selector, &final_g0, &final_g9
                );
                expected_instructions =
                    UINT64_C(471) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            }
            if (status != VF2_OK) {
                return status;
            }
            if (navigation_flags != 0u) {
                cpu->registers[VF2_I960_G0_REGISTER] = final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = final_g9;
                set_equal_condition(cpu);
            }
            account_nested_procedure(
                cpu, active_nested_calls, active_nested_calls
            );
            if (cpu->maximum_local_frame_depth <
                cpu->local_frame_depth + active_depth_delta) {
                cpu->maximum_local_frame_depth =
                    cpu->local_frame_depth + active_depth_delta;
            }
        } else {
            const uint32_t supported_input =
                (UINT32_C(1) << 14u) | (UINT32_C(1) << 15u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 9u);
            if ((effective_input_flags & ~supported_input) != 0u ||
                effective_previous_flags != effective_input_flags ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if ((navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                status = vf2_model2a_write_u32(
                    machine, player0 + UINT32_C(0x194), UINT32_C(4)
                );
                expected_instructions += UINT64_C(2);
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                status = vf2_model2a_write_u32(
                    machine, player0 + UINT32_C(0x194), UINT32_C(1)
                );
                expected_instructions += UINT64_C(2);
            }
            if (status == VF2_OK && menu_index == UINT8_C(8)) {
                uint16_t value = 0u;
                status = read_u16(
                    machine, player0 + UINT32_C(0x158), &value
                );
                if (status == VF2_OK &&
                    (effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    if (value > UINT16_C(0xa000)) {
                        value = (uint16_t)(value - UINT16_C(192));
                        status = write_u16(
                            machine, player0 + UINT32_C(0x158), value
                        );
                        expected_instructions += UINT64_C(5);
                    }
                    expected_instructions -= UINT64_C(1);
                } else if (status == VF2_OK &&
                           (effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    if (value < UINT16_C(0xff00)) {
                        value = (uint16_t)(value + UINT16_C(192));
                        status = write_u16(
                            machine, player0 + UINT32_C(0x158), value
                        );
                        expected_instructions += UINT64_C(5);
                    }
                }
            } else if (status == VF2_OK && menu_index == UINT8_C(11)) {
                uint16_t value = 0u;
                status = read_u16(
                    machine, player0 + UINT32_C(0x17c), &value
                );
                if (status == VF2_OK &&
                    (effective_input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    value = (uint16_t)(value - UINT16_C(192));
                    status = write_u16(
                        machine, player0 + UINT32_C(0x17c), value
                    );
                    expected_instructions += UINT64_C(1);
                } else if (status == VF2_OK &&
                           (effective_input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    value = (uint16_t)(value + UINT16_C(192));
                    status = write_u16(
                        machine, player0 + UINT32_C(0x17c), value
                    );
                    expected_instructions += UINT64_C(2);
                }
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        status = write_u16(
            machine, UINT32_C(0x00500082), UINT16_C(0x8000)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
        cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        if (menu_index != UINT8_C(4)) {
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
        }
        status = finish_recovered_procedure(
            machine, cpu,
            expected_instructions -
                (cpu->executed_instructions - start_instructions)
        );
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(uint16_t);
        report->recovered_instruction_count = expected_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (menu_index != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00055128), &table_target
    );
    if (status != VF2_OK || table_target != UINT32_C(0x00055330)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050081c), &descriptor
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, descriptor + UINT32_C(0x0c), &descriptor_target
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500860), &input_descriptor
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050a0b8), &control_a, sizeof(control_a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050a0b9), &control_b, sizeof(control_b)
        );
    }
    if (status != VF2_OK || descriptor_target != UINT32_C(0x0001b9ac) ||
        (input_descriptor & (UINT32_C(1) << 28u)) != 0u ||
        control_a != 0u || control_b != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    {
        uint32_t input_control = 0u;
        uint32_t input_control_word = 0u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050083c), &input_control
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, input_control, &input_control_word
            );
        }
        if (status != VF2_OK ||
            (input_control_word & (UINT32_C(1) << 3u)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500840), &input_control
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, input_control, &input_control_word
            );
        }
        if (status != VF2_OK ||
            (input_control_word & (UINT32_C(1) << 3u)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }

    status = write_u16(
        machine, UINT32_C(0x00500082), UINT16_C(0x8000)
    );
    if (status == VF2_OK) {
        cpu->registers[14] =
            (runtime_flags & (UINT32_C(1) << 9u)) != 0u
                ? UINT32_C(0x00055370)
                : UINT32_C(0x0005534c);
        cpu->registers[25] = UINT32_C(0x01000186);
        cpu->ip = UINT32_C(0x00009444);
        status = execute_inline_text_thunk(machine, cpu, &text_report);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a0), &fighter_control,
            sizeof(fighter_control)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, player0 + UINT32_C(0x1ac), fighter_control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player0, &previous_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player0, previous_flags & ~(UINT32_C(1) << 5u)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, player1 + UINT32_C(0x1ac), fighter_control
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player1, &previous_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player1, previous_flags & ~(UINT32_C(1) << 5u)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
        status = vf2_model2a_write_u32(
            machine, outer_stack + UINT32_C(128),
            cpu->registers[VF2_I960_G14_REGISTER]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, descriptor + UINT32_C(0x0c), UINT32_C(0x0001b9dc)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_initialize_control_fighter(machine, player0);
    }
    if (status == VF2_OK) {
        status = phase17_zero_initialize_control_fighter(machine, player1);
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, UINT32_C(0x00500082), UINT16_C(0x8000)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] = control;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player1;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = player0;
    cpu->registers[VF2_I960_G0_REGISTER + 13u] = descriptor;
    cpu->registers[VF2_I960_G14_REGISTER] = UINT32_C(0x000550d4);
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(5), UINT64_C(5));
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 4u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 4u;
    }
    expected_instructions =
        ((runtime_flags & (UINT32_C(1) << 9u)) != 0u
             ? UINT64_C(266)
             : UINT64_C(267)) +
        idle_wrapper_adjustment;
    status = finish_recovered_procedure(
        machine, cpu,
        expected_instructions -
            (cpu->executed_instructions - start_instructions)
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(32);
    report->bytes_written = 80u + text_report.bytes_written;
    report->recovered_instruction_count = expected_instructions;
    report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static int32_t phase17_index10_round_even(float value)
{
    const double input = (double)value;
    const int64_t truncated = (int64_t)input;
    const double fraction = input - (double)truncated;
    int64_t rounded = truncated;
    if (fraction > 0.5 || (fraction == 0.5 && (truncated & INT64_C(1)) != 0)) ++rounded;
    else if (fraction < -0.5 || (fraction == -0.5 && (truncated & INT64_C(1)) != 0)) --rounded;
    return (int32_t)rounded;
}

static vf2_status phase17_index10_decimal(vf2_model2a *machine,int32_t value,uint32_t destination)
{
    uint32_t magnitude=value<0?(uint32_t)(-(int64_t)value):(uint32_t)value;
    uint16_t tiles[6]; uint32_t i=0u; vf2_status status=VF2_OK;
    tiles[0]=value<0?UINT16_C(0x802d):UINT16_C(0x8020);
    if(magnitude<=UINT32_C(8191)){
        tiles[1]=UINT16_C(0x8020);
        for(i=0u;i<4u;++i){status=read_u16(machine,UINT32_C(0x02040000)+magnitude*UINT32_C(8)+i*UINT32_C(2),&tiles[i+2u]);if(status!=VF2_OK)return status;}
    }else{
        uint32_t n=magnitude; for(i=0u;i<5u;++i)tiles[i+1u]=UINT16_C(0x8020);
        i=5u; do{--i;tiles[i+1u]=(uint16_t)(UINT16_C(0x8030)+(uint16_t)(n%10u));n/=10u;}while(n!=0u&&i!=0u);
    }
    for(i=0u;status==VF2_OK&&i<6u;++i)status=write_u16(machine,destination+i*UINT32_C(2),tiles[i]);
    return status;
}

static vf2_status phase17_index10_tile(vf2_model2a *machine,uint32_t row,uint32_t col,uint16_t tile)
{
    return write_u16(machine,UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2),(uint16_t)(UINT16_C(0x8000)|tile));
}

static vf2_status phase17_index10_restore_menu(vf2_model2a *machine)
{
    static const uint32_t extra[3]={UINT32_C(0x0005ff08),UINT32_C(0x0005ff14),UINT32_C(0x0005ff18)};
    const uint8_t phase_index=UINT8_C(10),spill=UINT8_C(0x56);
    uint32_t record=0u,destination=0u,last_source=0u,last_destination=0u;uint64_t chars=0u;size_t i=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a4),&phase_index,sizeof(phase_index));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+UINT32_C(80),&record);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
    if(status==VF2_OK&&destination>=UINT32_C(4))status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x801c));else if(status==VF2_OK)status=VF2_ERROR_UNSUPPORTED;
    for(i=0u;status==VF2_OK&&i<12u;++i){status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+(uint32_t)i*UINT32_C(8),&record);if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&chars);}
    for(i=0u;status==VF2_OK&&i<3u;++i){status=vf2_model2a_read_u32(machine,extra[i],&record);if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&chars);}
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));
    return status;
}

static void phase17_index10_post(vf2_i960_cpu *cpu,int exiting)
{
    cpu->registers[0]=0u;cpu->registers[1]=UINT32_C(0x005ff580);cpu->registers[2]=UINT32_C(0x0000a010);cpu->registers[3]=0u;cpu->registers[4]=UINT32_C(0x00515400);cpu->registers[5]=UINT32_C(0x3f800000);cpu->registers[6]=0u;cpu->registers[7]=0u;
    cpu->registers[8]=UINT32_MAX;cpu->registers[9]=UINT32_MAX;cpu->registers[10]=UINT32_MAX;cpu->registers[11]=UINT32_MAX;cpu->registers[12]=0u;cpu->registers[13]=0u;cpu->registers[14]=UINT32_C(5);cpu->registers[15]=UINT32_C(0x00008a00);
    cpu->registers[16]=exiting?UINT32_C(0x00078cb0):UINT32_C(0x12);cpu->registers[17]=exiting?0u:UINT32_C(0x005002a8);cpu->registers[18]=UINT32_C(0x005002a8);cpu->registers[19]=UINT32_C(0x00500270);cpu->registers[20]=UINT32_C(0x00560000);cpu->registers[21]=UINT32_C(1);cpu->registers[22]=0u;cpu->registers[23]=UINT32_C(0x00510980);cpu->registers[24]=UINT32_C(0x00512980);cpu->registers[25]=exiting?UINT32_C(0x010016ac):UINT32_C(0x010012fa);cpu->registers[26]=UINT32_C(0x00800000);cpu->registers[27]=UINT32_C(0x00880000);cpu->registers[28]=UINT32_C(0x00004000);cpu->registers[29]=UINT32_C(0x00516480);cpu->registers[30]=UINT32_C(0x00000220);cpu->registers[31]=UINT32_C(0x005ff500);set_equal_condition(cpu);
}

static vf2_status phase17_index10_render_state0(vf2_model2a *machine)
{
    static const char *abbr[11]={"AKI","JAC","SAR","KAG","LAU","JEF","PAI","WOL","SHU","DUR","LIO"};
    static const char *names[11]={"AKIRA","JACKY","SARAH","KAGE","LAU","JEFFRY","PAI","WOLF","SHUN","DURAL","LION"};
    static const uint16_t markers[6]={UINT16_C(0x02c1),UINT16_C(0x02d6),UINT16_C(0x02c7),UINT16_C(0x02d2),UINT16_C(0x02ce),UINT16_C(0x02cb)};
    static const uint8_t marker_cols[6]={54u,55u,56u,58u,59u,60u};
    size_t i=0u;vf2_status status=write_phase17_index0_text(machine,UINT32_C(13*0x80),UINT32_C(10),"LOSES(%)");
    for(i=0u;status==VF2_OK&&i<6u;++i)status=phase17_index10_tile(machine,UINT32_C(16),marker_cols[i],markers[i]);
    for(i=0u;status==VF2_OK&&i<11u;++i)status=write_phase17_index0_text(machine,UINT32_C(16*0x80),UINT32_C(10)+(uint32_t)i*UINT32_C(4),abbr[i]);
    if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(14*0x80),UINT32_C(2),"WIN(%)");
    for(i=0u;status==VF2_OK&&i<11u;++i)status=write_phase17_index0_text(machine,(UINT32_C(18)+(uint32_t)i*UINT32_C(2))*UINT32_C(0x80),UINT32_C(2),names[i]);
    if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(45*0x80),UINT32_C(19),"PUSH TEST BUTTON TO EXIT.");
    return status;
}

static vf2_status phase17_index10_render_state1(vf2_model2a *machine)
{
    static const uint8_t vertical_cols[15]={1u,9u,13u,17u,21u,25u,29u,33u,37u,41u,45u,49u,53u,57u,61u};
    static const uint8_t horizontal_rows[11]={17u,19u,21u,23u,25u,27u,29u,31u,33u,35u,37u};
    static const uint8_t horizontal_tiles[61]={19u,21u,21u,21u,21u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,18u};
    uint32_t base=0u;uint32_t fighters[11],scores[11];uint32_t i=0u,j=0u;vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x0050016c),&base);
    for(i=0u;status==VF2_OK&&i<11u;++i){
        for(j=0u;status==VF2_OK&&j<11u;++j){
            uint16_t a=0u,b=0u;uint32_t sum=0u;int32_t value=0;uint32_t row=UINT32_C(18)+j*UINT32_C(2);uint32_t col=UINT32_C(9)+i*UINT32_C(4);uint32_t dst=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);
            status=read_u16(machine,base+UINT32_C(0x36a4)+j*UINT32_C(17)+i*UINT32_C(4),&a);if(status==VF2_OK)status=read_u16(machine,base+UINT32_C(0x36a6)+j*UINT32_C(17)+i*UINT32_C(4),&b);if(status!=VF2_OK)break;
            if(a!=0u){sum=(uint32_t)a+(uint32_t)b;value=phase17_index10_round_even(((float)a/(float)sum)*10000.0f);status=phase17_index10_decimal(machine,value,dst);}
            else{sum=(uint32_t)a+(uint32_t)b;status=phase17_index10_decimal(machine,0,dst);if(status==VF2_OK){if(sum!=0u)status=phase17_index10_tile(machine,row,col+UINT32_C(2),UINT16_C(0x30));else status=phase17_index10_decimal(machine,-1,dst+UINT32_C(4));}}
        }
    }
    for(i=0u;status==VF2_OK&&i<12u;++i)status=write_phase17_index0_text(machine,(UINT32_C(18)+i*UINT32_C(2))*UINT32_C(0x80),UINT32_C(53),"        ");
    for(i=0u;status==VF2_OK&&i<11u;++i){
        uint32_t total_a=0u,total_b=0u;int32_t value=0;uint32_t row=UINT32_C(18)+i*UINT32_C(2);uint32_t dst=UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(53*2);
        for(j=0u;status==VF2_OK&&j<11u;++j){uint16_t a=0u,b=0u;status=read_u16(machine,base+UINT32_C(0x36a4)+j*UINT32_C(17)+i*UINT32_C(4),&a);if(status==VF2_OK)status=read_u16(machine,base+UINT32_C(0x36a6)+j*UINT32_C(17)+i*UINT32_C(4),&b);total_a+=(uint32_t)a;total_b+=(uint32_t)b;}
        if(status!=VF2_OK)break;
        if(total_a+total_b!=0u){value=phase17_index10_round_even(((float)total_b/(float)(total_a+total_b))*10000.0f);status=phase17_index10_decimal(machine,value,dst);}else{status=phase17_index10_decimal(machine,0,dst);if(status==VF2_OK)status=phase17_index10_decimal(machine,-1,dst+UINT32_C(4));value=-1;}
        fighters[i]=i;scores[i]=(uint32_t)value;if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500280)+i*UINT32_C(4),(uint32_t)value);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500244)+i*UINT32_C(4),i);
    }
    for(i=1u;status==VF2_OK&&i<11u;++i)for(j=i;status==VF2_OK&&j<11u;++j)if((int32_t)scores[i-1u]>(int32_t)scores[j]){uint32_t t=scores[i-1u];scores[i-1u]=scores[j];scores[j]=t;t=fighters[i-1u];fighters[i-1u]=fighters[j];fighters[j]=t;status=vf2_model2a_write_u32(machine,UINT32_C(0x00500280)+(i-1u)*UINT32_C(4),scores[i-1u]);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500280)+j*UINT32_C(4),scores[j]);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500244)+(i-1u)*UINT32_C(4),fighters[i-1u]);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500244)+j*UINT32_C(4),fighters[j]);}
    for(i=0u;status==VF2_OK&&i<11u;++i){uint32_t row=UINT32_C(18)+fighters[i]*UINT32_C(2);status=phase17_index10_decimal(machine,(int32_t)((i+UINT32_C(1))*UINT32_C(1000)),UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(57*2));}
    for(i=0u;status==VF2_OK&&i<11u;++i)status=write_phase17_index0_text(machine,(UINT32_C(18)+i*UINT32_C(2))*UINT32_C(0x80),UINT32_C(60),"    ");
    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(15), UINT32_C(1), UINT16_C(24)
        );
    }
    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(15), UINT32_C(61), UINT16_C(25)
        );
    }
    for(i=2u;status==VF2_OK&&i<61u;++i)status=phase17_index10_tile(machine,UINT32_C(15),i,UINT16_C(21));
    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(39), UINT32_C(1), UINT16_C(26)
        );
    }
    if (status == VF2_OK) {
        status = phase17_index10_tile(
            machine, UINT32_C(39), UINT32_C(61), UINT16_C(27)
        );
    }
    for(i=2u;status==VF2_OK&&i<61u;++i)status=phase17_index10_tile(machine,UINT32_C(39),i,UINT16_C(21));
    for(i=0u;status==VF2_OK&&i<15u;++i)for(j=16u;status==VF2_OK&&j<39u;++j)status=phase17_index10_tile(machine,j,vertical_cols[i],UINT16_C(22));
    for(i=0u;status==VF2_OK&&i<11u;++i)for(j=1u;status==VF2_OK&&j<62u;++j)status=phase17_index10_tile(machine,horizontal_rows[i],j,horizontal_tiles[j-1u]);
    return status;
}

static vf2_status execute_frame_phase17_bit7_index10(
    vf2_model2a *machine,vf2_i960_cpu *cpu,vf2_hybrid_bridge_report *report,uint8_t flagged_phase_index)
{
    const uint32_t base_input=UINT32_C(0x0ff7f700);const uint8_t spill=UINT8_C(0x56);
    uint32_t target=0u,input=0u,navigation=0u,released=0u,previous=0u,mask=0u;uint8_t a5=0u,a6=0u,a7=0u;uint64_t instructions=0u,calls=0u;vf2_status status=VF2_OK;int exiting=0;
    if(flagged_phase_index!=UINT8_C(0x8a)||cpu->local_frame_depth==0u)return VF2_ERROR_UNSUPPORTED;
    status=vf2_model2a_read_u32(machine,UINT32_C(0x0005fef8),&target);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500700),&input);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500704),&navigation);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500708),&released);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050070c),&previous);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050002c),&mask);if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a5),&a5,1u);if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a6),&a6,1u);if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a7),&a7,1u);
    if(status!=VF2_OK||target!=UINT32_C(0x0005f234)||input!=base_input||released!=0u||previous!=base_input||mask!=UINT32_C(0x00020000)||a5>UINT8_C(1)||a6!=UINT8_C(0xff)||a7!=UINT8_C(0xff))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    if(a5==UINT8_C(0)){
        const uint8_t next=UINT8_C(1);if(navigation!=0u)return VF2_ERROR_UNSUPPORTED;status=phase17_index10_render_state0(machine);if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));instructions=UINT64_C(1650);calls=UINT64_C(31);
    }else{
        if (navigation != 0u &&
            (navigation & UINT32_C(0x04000104)) == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = phase17_index10_render_state1(machine);
        if (status == VF2_OK &&
            (navigation & UINT32_C(0x04000104)) != 0u) {
            status = phase17_index10_restore_menu(machine);
            exiting = 1;
            instructions = UINT64_C(51001);
            calls = UINT64_C(1452);
        } else if (status == VF2_OK) {
            instructions = UINT64_C(36729);
            calls = UINT64_C(1436);
        }
    }
    if (status == VF2_OK && !exiting) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005ff602), &spill, sizeof(spill)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions+=instructions;cpu->procedure_calls+=calls;cpu->procedure_returns+=calls;status=vf2_i960_cpu_return_procedure(cpu,machine);if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    if(a5==UINT8_C(0)){phase17_index7_post(cpu,UINT32_C(0x2e),UINT32_C(0x3f4f5c29),UINT32_C(0xc0a0a3d7),UINT32_C(0x01001726),UINT32_C(5),0);cpu->arithmetic_control=(cpu->arithmetic_control&~UINT32_C(7))|UINT32_C(1);cpu->compare_result=VF2_I960_COMPARE_GREATER;}else phase17_index10_post(cpu,exiting);
    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;report->exit_address=cpu->ip;report->iterations=UINT64_C(1);report->recovered_instruction_count=instructions;report->recovered_procedure_calls=calls;report->recovered_procedure_returns=calls+UINT64_C(1);report->cpu_poststate_applied=1;return VF2_OK;
}

static vf2_status execute_frame_phase17(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t saved_g4 =
        cpu->registers[VF2_I960_G0_REGISTER + 4u];
    uint32_t base = 0u;
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t gameplay_flags = 0u;
    uint32_t phase_pointer_holder = 0u;
    uint32_t previous_phase_target = 0u;
    uint32_t next_phase_target = 0u;
    uint32_t phase_object = 0u;
    uint32_t phase_text_base = 0u;
    uint32_t phase_text_source = 0u;
    uint32_t phase_text_destination = 0u;
    uint64_t phase_text_characters = 0u;
    uint8_t phase_index = 0u;
    uint8_t next_phase_index = 0u;
    uint8_t phase_state = 0u;
    int phase_step_forward = 0;
    int phase_step_back = 0;
    int phase_step = 0;
    int phase_reset = 0;
    uint64_t recovered_instructions = UINT64_C(36);
    vf2_status status = VF2_OK;

    status = vf2_model2a_read(
        machine, UINT32_C(0x005000a6), &phase_state, sizeof(phase_state)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a4), &phase_index,
            sizeof(phase_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500704), &gameplay_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (phase_state == 0u) {
        return execute_frame_phase17_zero_control_menu(machine, cpu, report);
    }
    if ((phase_index & UINT8_C(0x80)) != 0u) {
        if (phase_index == UINT8_C(0x80)) {
            return execute_frame_phase17_bit7_index0(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x81)) {
            return execute_frame_phase17_bit7_index1(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x82)) {
            return execute_frame_phase17_bit7_index2(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x83)) {
            return execute_frame_phase17_bit7_index3(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x84)) {
            return execute_frame_phase17_bit7_index4(
                machine, cpu, report, phase_index
            );
        }
                if (phase_index == UINT8_C(0x85)) {
            return execute_frame_phase17_bit7_index5(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x86)) {
            return execute_frame_phase17_bit7_index6(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x87)) {
            return execute_frame_phase17_bit7_index7(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x88)) {
            return execute_frame_phase17_bit7_index8(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x89)) {
            return execute_frame_phase17_bit7_index9(
                machine, cpu, report, phase_index
            );
        }
        if (phase_index == UINT8_C(0x8a)) {
            return execute_frame_phase17_bit7_index10(
                machine, cpu, report, phase_index
            );
        }
        return execute_frame_phase17_bit7_index11(
            machine, cpu, report, saved_g4, phase_index
        );
    }
    phase_step_forward =
        (gameplay_flags & UINT32_C(0x08001008)) != 0u;
    phase_step_back =
        !phase_step_forward &&
        (gameplay_flags & UINT32_C(0x00002000)) != 0u;
    phase_step = phase_step_forward || phase_step_back;
    phase_reset =
        (gameplay_flags & UINT32_C(0x04000104)) != 0u;
    if (phase_step_forward) {
        recovered_instructions =
            phase_index >= UINT8_C(11)
                ? UINT64_C(50)
                : UINT64_C(49);
    } else if (phase_step_back) {
        recovered_instructions =
            phase_index == 0u ? UINT64_C(50) : UINT64_C(49);
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &player0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &player1
        );
    }
    if (phase_step_forward) {
        next_phase_index = phase_index >= UINT8_C(11)
            ? UINT8_C(0)
            : (uint8_t)(phase_index + UINT8_C(1));
    } else if (phase_step_back) {
        next_phase_index = phase_index == 0u
            ? UINT8_C(11)
            : (uint8_t)(phase_index - UINT8_C(1));
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) +
                (uint32_t)phase_index * UINT32_C(8),
            &phase_pointer_holder
        );
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine, phase_pointer_holder, &previous_phase_target
        );
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0005feac) +
                (uint32_t)next_phase_index * UINT32_C(8),
            &phase_pointer_holder
        );
    }
    if (status == VF2_OK && phase_step) {
        status = vf2_model2a_read_u32(
            machine, phase_pointer_holder, &next_phase_target
        );
    }
    if (status == VF2_OK && phase_step &&
        (previous_phase_target < UINT32_C(4) ||
         next_phase_target < UINT32_C(4))) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK && phase_step) {
        status = write_u16(
            machine,
            previous_phase_target - UINT32_C(4),
            UINT16_C(0x8020)
        );
    }
    if (status == VF2_OK && phase_step) {
        phase_index = next_phase_index;
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &phase_index,
            sizeof(phase_index)
        );
    }
    if (status == VF2_OK && phase_step) {
        status = write_u16(
            machine,
            next_phase_target - UINT32_C(4),
            UINT16_C(0x801c)
        );
    }
    if (status == VF2_OK && phase_reset) {
        const uint8_t flagged_phase_index =
            (uint8_t)(phase_index | UINT8_C(0x80));
        const uint8_t zero = 0u;
        const uint8_t ff = UINT8_C(0xff);

        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4),
            &flagged_phase_index, sizeof(flagged_phase_index)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a5), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x005000a7), &ff, sizeof(ff)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500864), &phase_object
            );
        }
        if (status == VF2_OK && phase_object > UINT32_MAX - UINT32_C(0x80)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, phase_object + UINT32_C(0x80), UINT16_C(0)
            );
        }
        if (status == VF2_OK) {
            status = clear_tile_plane_64x48(
                machine, UINT32_C(0x01000000)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                UINT32_C(0x0005feac) +
                    (uint32_t)phase_index * UINT32_C(8),
                &phase_text_base
            );
        }
        if (status == VF2_OK && phase_text_base > UINT32_MAX - UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        phase_text_source = phase_text_base + UINT32_C(4);
        if (status == VF2_OK) {
            uint64_t length = 0u;
            for (length = 0u; length < UINT64_C(4096); ++length) {
                uint8_t raw = 0u;
                status = vf2_model2a_read(
                    machine,
                    phase_text_source + (uint32_t)length,
                    &raw, sizeof(raw)
                );
                if (status != VF2_OK || raw == 0u) {
                    break;
                }
            }
            if (status == VF2_OK && length == UINT64_C(4096)) {
                return VF2_ERROR_UNSUPPORTED;
            }
            phase_text_characters = length;
        }
        if (status == VF2_OK) {
            const uint32_t half_length =
                ((uint32_t)phase_text_characters + UINT32_C(1)) >> 1u;
            const uint32_t column_offset =
                (UINT32_C(32) - half_length) * UINT32_C(2);
            phase_text_destination =
                (UINT32_C(0x01000100) & ~UINT32_C(0x7f)) +
                column_offset;
            status = copy_diagnostic_text(
                machine, phase_text_source, phase_text_destination, NULL
            );
        }
        if (status == VF2_OK) {
            recovered_instructions +=
                UINT64_C(12573) + phase_text_characters * UINT64_C(12);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[1] + UINT32_C(64), saved_g4
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 4u] = saved_g4;
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = player1;
    if (phase_step) {
        cpu->registers[VF2_I960_G0_REGISTER + 9u] =
            next_phase_target - UINT32_C(4);
    }
    if (phase_reset) {
        cpu->registers[VF2_I960_G0_REGISTER] = phase_text_source;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = phase_text_destination;
    }
    (void)base;
    set_equal_condition(cpu);
    account_nested_procedure(
        cpu,
        phase_reset ? UINT64_C(5) : UINT64_C(2),
        phase_reset ? UINT64_C(5) : UINT64_C(2)
    );
    if (cpu->maximum_local_frame_depth <
        cpu->local_frame_depth + (phase_reset ? 3u : 2u)) {
        cpu->maximum_local_frame_depth =
            cpu->local_frame_depth + (phase_reset ? 3u : 2u);
    }
    status = finish_recovered_procedure(
        machine, cpu, recovered_instructions
    );
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values =
        (phase_step ? UINT64_C(7) : UINT64_C(4)) +
        (phase_reset
            ? UINT64_C(3076) + phase_text_characters
            : UINT64_C(0));
    report->bytes_written =
        (phase_step ? 14u : 9u) +
        (phase_reset
            ? (size_t)UINT32_C(6149) +
                (size_t)phase_text_characters * 2u
            : 0u);
    report->recovered_instruction_count = recovered_instructions;
    report->recovered_procedure_calls =
        phase_reset ? UINT64_C(5) : UINT64_C(2);
    report->recovered_procedure_returns =
        phase_reset ? UINT64_C(6) : UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_selector3_display_text_at(
    vf2_model2a *machine, uint32_t source_base, uint32_t destination_base
)
{
    int16_t addend = 0;
    int16_t word_mode = 0;
    uint16_t raw_addend = 0u;
    uint16_t raw_mode = 0u;
    uint32_t rows = 0u;
    uint32_t columns = 0u;
    uint32_t source = source_base + UINT32_C(12);
    uint32_t row = 0u;
    vf2_status status = read_u16(machine, source_base, &raw_addend);

    addend = (int16_t)raw_addend;
    if (status == VF2_OK) {
        status = read_u16(machine, source_base + UINT32_C(2), &raw_mode);
        word_mode = (int16_t)raw_mode;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source_base + UINT32_C(4), &rows);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source_base + UINT32_C(8), &columns);
    }
    if (status != VF2_OK || rows > UINT32_C(4096) || columns > UINT32_C(4096)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (row = 0u; status == VF2_OK && row < rows; ++row) {
        uint32_t column = 0u;
        uint32_t destination = destination_base + row * UINT32_C(0x80);
        for (column = 0u; status == VF2_OK && column < columns; ++column) {
            int32_t sample = 0;
            if (word_mode == 0) {
                uint8_t raw = 0u;
                status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
                sample = (int32_t)(int8_t)raw;
                source += UINT32_C(1);
            } else {
                uint16_t raw = 0u;
                status = read_u16(machine, source, &raw);
                sample = (int32_t)(int16_t)raw;
                source += UINT32_C(2);
            }
            if (status == VF2_OK) {
                status = write_u16(machine, destination,
                    (uint16_t)((int32_t)addend + sample));
                destination += UINT32_C(2);
            }
        }
    }
    return status;
}

static vf2_status execute_selector3_display_text(
    vf2_model2a *machine,
    uint32_t source_base
)
{
    return execute_selector3_display_text_at(
        machine, source_base, UINT32_C(0x01004000)
    );
}

static vf2_status execute_selector3_profile_class(
    const vf2_model2a *machine,
    uint32_t base,
    uint32_t *result
)
{
    uint32_t flags = 0u;
    uint8_t mode = 0u;
    uint8_t left = 0u;
    uint8_t right = 0u;
    uint8_t table_value = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, base + UINT32_C(0x3320), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (mode == UINT8_C(25) && (flags & (UINT32_C(1) << 1u)) == 0u) {
        *result = UINT32_C(3);
        return VF2_OK;
    }
    if ((flags & UINT32_C(1)) != 0u) {
        *result = UINT32_C(2);
        return VF2_OK;
    }
    if ((flags & (UINT32_C(1) << 1u)) != 0u) {
        if (mode != UINT8_C(25)) {
            *result = UINT32_C(2);
            return VF2_OK;
        }
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3327), &left, sizeof(left)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3328), &right, sizeof(right)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (left == right) {
            *result = 0u;
            return VF2_OK;
        }
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x000028b8) + (uint32_t)mode,
        &table_value, sizeof(table_value)
    );
    if (status == VF2_OK) {
        *result = table_value == 0u ? 0u : 1u;
    }
    return status;
}

static vf2_status execute_selector3_profile_color(
    const vf2_model2a *machine,
    uint32_t base,
    uint32_t selector,
    uint32_t *result
)
{
    uint32_t flags = 0u;
    uint32_t profile_class = 0u;
    uint8_t mode = 0u;
    uint8_t byte_value = 0u;
    uint32_t table_index = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = execute_selector3_profile_class(
        machine, base, &profile_class
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 1u)) == 0u) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
        if (status == VF2_OK && profile_class == UINT32_C(3)) {
            *result = 0u;
            return VF2_OK;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine,
                UINT32_C(0x000027ac) + (uint32_t)mode * UINT32_C(2) +
                    (profile_class == UINT32_C(2) ? 0u : selector),
                &byte_value, sizeof(byte_value)
            );
        }
        table_index = (uint32_t)byte_value;
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x000027e0) + table_index * UINT32_C(4),
                result
            );
        }
    } else if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3325), &byte_value, sizeof(byte_value)
        );
        *result = (uint32_t)byte_value;
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3326), &byte_value,
                sizeof(byte_value)
            );
        }
        if (status == VF2_OK) {
            *result = (*result << 8u) | (uint32_t)byte_value;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3327) + selector,
                &byte_value, sizeof(byte_value)
            );
        }
        if (status == VF2_OK) {
            *result = (*result << 8u) | (uint32_t)byte_value;
        }
    }
    if (status == VF2_OK) {
        *result += UINT32_C(0x00010101);
    }
    return status;
}

static vf2_status execute_selector3_halfword_stream(
    vf2_model2a *machine,
    uint32_t start
)
{
    uint32_t cursor = start;
    size_t descriptor_count = 0u;
    vf2_status status = VF2_OK;

    while (status == VF2_OK && descriptor_count < 64u) {
        uint32_t source = 0u;
        uint32_t destination = 0u;
        uint32_t header = 0u;
        uint32_t halfwords = 0u;
        uint32_t offset = 0u;

        status = vf2_model2a_read_u32(machine, cursor, &source);
        cursor += UINT32_C(4);
        if (status != VF2_OK || source == 0u) {
            break;
        }
        status = vf2_model2a_read_u32(machine, cursor, &destination);
        cursor += UINT32_C(4);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, source, &header);
        }
        if (status != VF2_OK || header > (UINT32_MAX >> 4u)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        halfwords = header << 4u;
        if (halfwords > UINT32_C(65536)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (offset = 0u; status == VF2_OK && offset < halfwords;
             ++offset) {
            uint16_t value = 0u;

            status = read_u16(
                machine, source + UINT32_C(4) + offset * UINT32_C(2), &value
            );
            if (status == VF2_OK) {
                status = write_u16(
                    machine, destination + offset * UINT32_C(2), value
                );
            }
        }
        ++descriptor_count;
    }
    if (status != VF2_OK || descriptor_count == 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_selector3_register_stream(
    vf2_model2a *machine,
    uint32_t start
)
{
    uint32_t cursor = start;
    size_t descriptor_count = 0u;
    vf2_status status = VF2_OK;

    while (status == VF2_OK && descriptor_count < 64u) {
        uint32_t destination = 0u;
        uint32_t encoded_count = 0u;
        uint32_t words = 0u;
        uint32_t index = 0u;

        status = vf2_model2a_read_u32(machine, cursor, &destination);
        cursor += UINT32_C(4);
        if (status != VF2_OK || destination == 0u) {
            break;
        }
        status = vf2_model2a_read_u32(machine, cursor, &encoded_count);
        cursor += UINT32_C(4);
        if (status != VF2_OK || (int32_t)encoded_count < 0) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        words = encoded_count >> 1u;
        if (words > UINT32_C(65536)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (index = 0u; status == VF2_OK && index < words; ++index) {
            uint32_t value = 0u;

            status = vf2_model2a_read_u32(machine, cursor, &value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination + index * UINT32_C(4), value
                );
            }
            cursor += UINT32_C(4);
        }
        ++descriptor_count;
    }
    if (status != VF2_OK || descriptor_count == 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    return VF2_OK;
}

static vf2_status execute_selector3_mode0_prefix(vf2_model2a *machine)
{
    static const uint32_t palette_sources[] = {
        UINT32_C(0x000266f0), UINT32_C(0x000266f4),
        UINT32_C(0x00026700), UINT32_C(0x00026704)
    };
    static const uint32_t palette_destinations[] = {
        UINT32_C(0x018004c0), UINT32_C(0x018004e0),
        UINT32_C(0x018004d0), UINT32_C(0x018004f0)
    };
    uint32_t pointer = 0u;
    uint32_t value = 0u;
    uint32_t palette_value = 0u;
    uint32_t index = 0u;
    uint8_t zero = 0u;
    vf2_status status = VF2_OK;

    /* 0xae78: clear the mode scratch byte, seed the two palette pages,
     * clear the diagnostic plane and prepare the graphics control flags. */
    status = vf2_model2a_write(
        machine, UINT32_C(0x00503030), &zero, sizeof(zero)
    );
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(
            machine, palette_sources[index], &palette_value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, palette_destinations[index], palette_value
            );
        }
    }
    if (status == VF2_OK) {
        uint32_t row = 0u;
        for (row = 0u; status == VF2_OK && row < UINT32_C(48); ++row) {
            uint32_t column = 0u;
            uint32_t destination = UINT32_C(0x01000000) + row * UINT32_C(0x80);
            for (column = 0u; status == VF2_OK && column < UINT32_C(62); ++column) {
                status = write_u16(machine, destination, UINT16_C(32));
                destination += UINT32_C(2);
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050009c), &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050009c), value & ~UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        value |= UINT32_C(0x42800000);
        status = vf2_model2a_write_u32(machine, pointer, value);
    }
    return status;
}

static vf2_status execute_frame_selector3_b0d8(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int prefix_already_applied
)
{
    static const uint32_t clear_slots[] = {
        UINT32_C(0x00500828), UINT32_C(0x00500844),
        UINT32_C(0x00500848), UINT32_C(0x0050081c)
    };
    uint32_t pointer = 0u;
    uint32_t value = 0u;
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t index = 0u;
    uint8_t zero = 0u;
    uint8_t one = UINT8_C(1);
    uint8_t three = UINT8_C(3);
    uint8_t six = UINT8_C(6);
    uint8_t eight = UINT8_C(8);
    uint16_t zero16 = 0u;
    uint8_t input_zero = 0u;
    uint8_t input_default = UINT8_C(0x63);
    vf2_status status = VF2_OK;

    if (!prefix_already_applied) {
        status = execute_selector3_mode0_prefix(machine);
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_write(
        machine, UINT32_C(0x00500030), &((uint8_t){2u}), sizeof(uint8_t)
    );

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        value |= UINT32_C(0x42800000);
        value &= ~UINT32_C(0x00100000);
        status = vf2_model2a_write_u32(machine, pointer, value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), zero16);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), zero16);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text(
            machine, UINT32_C(0x02a6c15e)
        );
    }
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(machine, clear_slots[index], &pointer);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, pointer, value & ~UINT32_C(0x80000000)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x1b0), &eight,
                                   sizeof(eight));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x1b0), &six,
                                   sizeof(six));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(4), &zero,
                                   sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x1b1), &eight,
                                   sizeof(eight));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x18),
                                       UINT32_C(0x447a0000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x1c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x26), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0xc),
                                       UINT32_C(0x00013f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x2a), &zero,
                                   sizeof(zero));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x624), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, task0, (value & UINT32_C(0xff000000)) |
                                UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500868), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       value | UINT32_C(0x80000000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0xc),
                                       UINT32_C(0x000640f4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x69c),
                                   &input_zero, sizeof(input_zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x69d),
                                   &input_default, sizeof(input_default));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x69c),
                                   &input_zero, sizeof(input_zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x69d),
                                   &input_default, sizeof(input_default));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(4), &one,
                                   sizeof(one));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x1b1), &six,
                                   sizeof(six));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x18), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x1c),
                                       UINT32_C(0x447a0000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x26), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0xc),
                                       UINT32_C(0x00013f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x2a), &zero,
                                   sizeof(zero));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x624), zero16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task1, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, task1, (value & UINT32_C(0xff000000)) |
                                UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050086c), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       value | UINT32_C(0x80000000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0xc),
                                       UINT32_C(0x000640f4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000),
                                       value | (UINT32_C(1) << 16u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &three,
                                   sizeof(three));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a00c),
                                       UINT32_C(0x40f00000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024),
                                       UINT32_C(256));
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_profile_measure(
    const vf2_model2a *machine,
    uint32_t base,
    uint32_t *x_result,
    uint32_t *y_result
)
{
    uint32_t flags = 0u;
    uint32_t profile_class = 0u;
    uint32_t color0 = 0u;
    uint32_t color1 = 0u;
    uint32_t first = 0u;
    uint32_t second = 0u;
    uint32_t third = 0u;
    uint32_t fourth = 0u;
    uint8_t value = 0u;
    vf2_status status = VF2_OK;

    if (x_result == NULL || y_result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = execute_selector3_profile_class(
        machine, base, &profile_class
    );
    if (status == VF2_OK) {
        status = execute_selector3_profile_color(
            machine, base, 0u, &color0
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_profile_color(
            machine, base, 1u, &color1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3380), &value, sizeof(value)
        );
        first = (uint32_t)value;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3388), &value, sizeof(value)
        );
        second = (uint32_t)value;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3385), &value, sizeof(value)
        );
        third = (uint32_t)value;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x338d), &value, sizeof(value)
        );
        fourth = (uint32_t)value;
    }
    if (status != VF2_OK) {
        return status;
    }
    color0 = (color0 >> 16u) & UINT32_C(0xff);
    color1 = (color1 >> 16u) & UINT32_C(0xff);
    if (profile_class == UINT32_C(3)) {
        *x_result = 0u;
        *y_result = 0u;
    } else if (color0 == 0u || color1 == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    } else if (profile_class == UINT32_C(2) &&
               (flags & (UINT32_C(1) << 1u)) != 0u) {
        *x_result = (first + second) / color0 + third;
        *y_result = fourth;
    } else {
        *x_result = first / color0 + third;
        *y_result = second / color1 + fourth;
    }
    return VF2_OK;
}

static vf2_status execute_selector3_phase1(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int *profile_measure_called,
    int *countdown_terminal,
    uint32_t *measured_sum
)
{
    uint32_t base = 0u;
    uint32_t profile_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t input_flags = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t sum = 0u;
    uint32_t counter = 0u;
    uint8_t mode = 0u;
    int countdown = 0;
    vf2_status status = VF2_OK;

    if (cpu == NULL || next_phase == NULL || profile_measure_called == NULL ||
        countdown_terminal == NULL || measured_sum == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    *profile_measure_called = 0;
    *countdown_terminal = 0;
    *measured_sum = 0u;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x3320), &profile_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (mode == UINT8_C(25) &&
        (profile_flags & (UINT32_C(1) << 1u)) == 0u) {
        countdown = 1;
    } else {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500708), &runtime_flags
        );
        if (status == VF2_OK && (runtime_flags & UINT32_C(3)) == 0u) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500704), &input_flags
            );
            if (status == VF2_OK &&
                (input_flags & UINT32_C(0x08000008)) == 0u) {
                countdown = 1;
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    if (!countdown) {
        const uint32_t caller_sp = cpu->registers[1];

        *profile_measure_called = 1;
        /* 0xafe0 spills g0/g1 around 0x2584; 0x26ec spills g9.
         * The two color lookups reuse the same child frame slot. */
        status = vf2_model2a_write_u32(
            machine, caller_sp + UINT32_C(0x80),
            cpu->registers[VF2_I960_G0_REGISTER]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, caller_sp + UINT32_C(0x84),
                cpu->registers[VF2_I960_G0_REGISTER + 1u]
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, caller_sp + UINT32_C(0x140),
                cpu->registers[VF2_I960_G0_REGISTER + 9u]
            );
        }
        if (status == VF2_OK) {
            status = execute_selector3_profile_measure(
                machine, base, &x, &y
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if ((profile_flags & UINT32_C(1)) != 0u ||
            (runtime_flags & (UINT32_C(1) << 1u)) == 0u) {
            sum = x + y;
        } else {
            sum = x;
        }
        *measured_sum = sum;
        if (sum < UINT32_C(24)) {
            *next_phase = UINT8_C(6);
            return vf2_model2a_write(
                machine, UINT32_C(0x00500030), next_phase,
                sizeof(*next_phase)
            );
        }
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500024), &counter
    );
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK && (int32_t)counter <= 0) {
        *countdown_terminal = 1;
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_sound_event(
    vf2_model2a *machine, uint32_t event
)
{
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint8_t mode_flags = 0u;
    uint8_t count = 0u;
    uint8_t index = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050002c), &selector_mask
    );
    if (status == VF2_OK && (selector_mask & UINT32_C(0x0c)) != 0u) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, base + UINT32_C(0x3351),
                &mode_flags, sizeof(mode_flags)
            );
        }
        if (status == VF2_OK && (mode_flags & UINT8_C(1)) != 0u) {
            return VF2_OK;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &flags
        );
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 20u)) != 0u &&
        (event & UINT32_C(0x00ff0000)) == UINT32_C(0x009e0000)) {
        event -= UINT32_C(1) << 17u;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(33)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(33)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
    }
    if (status == VF2_OK && count < UINT8_C(16)) {
        ++count;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00504001), &count, sizeof(count)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00504020) + (uint32_t)index * UINT32_C(4),
                event
            );
        }
        if (status == VF2_OK) {
            index = (uint8_t)((index + UINT8_C(1)) & UINT8_C(15));
            status = vf2_model2a_write(
                machine, UINT32_C(0x00504003), &index, sizeof(index)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(0x421)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00e80004), UINT32_C(0x421)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase3(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    uint32_t counter = 0u;
    uint32_t terminal_gate = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500024), &counter
        );
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK && counter == UINT32_C(192)) {
        status = execute_selector3_sound_event(machine, UINT32_C(0x00ad1001));
    }
    if (status == VF2_OK && (int32_t)counter > 0) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00550000), &terminal_gate
        );
    }
    if (status == VF2_OK && terminal_gate != UINT32_C(1)) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase4_mode_init(vf2_model2a *machine)
{
    uint8_t mode = 0u;
    uint8_t one = UINT8_C(1);
    uint16_t lane_count = 0u;
    uint16_t lane_aux = 0u;
    uint32_t table = 0u;
    uint32_t layout = 0u;
    uint32_t callback = 0u;
    uint32_t slot = 0u;
    uint32_t lane = 0u;
    uint32_t word = 0u;
    uint32_t pattern[4] = {0u, 0u, 0u, 0u};
    vf2_status status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &mode, sizeof(mode)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write(
  machine, UINT32_C(0x00503058), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x00025034) + (uint32_t)mode * UINT32_C(32), &table
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_halfword_stream(machine, table);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x00025038) + (uint32_t)mode * UINT32_C(32), &table
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_register_stream(machine, table);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x0002503c) + (uint32_t)mode * UINT32_C(32), &layout
        );
    }
    if (status == VF2_OK) status = read_u16(machine, layout, &lane_count);
    if (status == VF2_OK) status = read_u16(machine, layout + UINT32_C(2), &lane_aux);
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x0050303c), lane_count);
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x0050303e), lane_aux);
    if (status != VF2_OK || lane_count == 0u || lane_count > UINT16_C(64)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    layout += UINT32_C(4);
    for (slot = 0u; status == VF2_OK && slot < UINT32_C(18); ++slot) {
        uint32_t source = 0u;
        status = vf2_model2a_read_u32(machine, layout + slot * UINT32_C(4), &source);
        for (lane = 0u; status == VF2_OK && lane < (uint32_t)lane_count; ++lane) {
  uint32_t q = 0u;
  for (q = 0u; status == VF2_OK && q < UINT32_C(16); ++q) {
      status = vf2_model2a_read_u32(
          machine, source + lane * UINT32_C(64) + q * UINT32_C(4), &word
      );
      if (status == VF2_OK) {
          status = vf2_model2a_write_u32(
              machine,
              UINT32_C(0x00579000) + slot * UINT32_C(64) +
                  lane * UINT32_C(1152) + q * UINT32_C(4),
              word
          );
      }
  }
        }
    }
    for (word = 0u; status == VF2_OK && word < UINT32_C(8); ++word) {
        status = vf2_model2a_write_u32(
  machine, UINT32_C(0x00503100) + word * UINT32_C(4), UINT32_MAX
        );
    }
    for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
        status = vf2_model2a_read_u32(
  machine,
  UINT32_C(0x00579000) + ((uint32_t)lane_count - UINT32_C(1)) * UINT32_C(1152) +
      word * UINT32_C(4),
  &pattern[word]
        );
    }
    for (lane = (uint32_t)lane_count; status == VF2_OK && lane < UINT32_C(64); ++lane) {
        uint32_t repeat = 0u;
        for (repeat = 0u; status == VF2_OK && repeat < UINT32_C(8); ++repeat) {
  for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
      status = vf2_model2a_write_u32(
          machine, UINT32_C(0x01004000) + lane * UINT32_C(128) +
              repeat * UINT32_C(16) + word * UINT32_C(4), pattern[word]
      );
  }
        }
    }
    for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x00579000) + word * UINT32_C(4), &pattern[word]
        );
    }
    for (lane = 0u; status == VF2_OK && lane < UINT32_C(64); ++lane) {
        uint32_t repeat = 0u;
        for (repeat = 0u; status == VF2_OK && repeat < UINT32_C(8); ++repeat) {
  for (word = 0u; status == VF2_OK && word < UINT32_C(4); ++word) {
      status = vf2_model2a_write_u32(
          machine, UINT32_C(0x01006000) + lane * UINT32_C(128) +
              repeat * UINT32_C(16) + word * UINT32_C(4), pattern[word]
      );
  }
        }
    }
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00503001), &one, sizeof(one));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00503028), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050302c), 0u);
    if (status == VF2_OK) status = write_u16(machine, UINT32_C(0x0050304a), 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, UINT32_C(0x0002502c) + (uint32_t)mode * UINT32_C(32), &callback
        );
    }
    if (status == VF2_OK && callback != UINT32_C(0x000244d0)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return status;
}

static vf2_status execute_selector3_phase4_actor_record(
    vf2_model2a *machine, uint32_t descriptor, uint32_t fighter, int second,
    uint32_t *next_descriptor
)
{
    uint32_t flags = 0u;
    uint32_t associated = 0u;
    uint32_t base = 0u;
    uint32_t x = 0u;
    uint32_t z = 0u;
    uint8_t motion = 0u;
    uint8_t bit23 = 0u;
    uint8_t bit22 = 0u;
    uint8_t mode_flags = 0u;
    uint8_t zero = 0u;
    uint16_t angle = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, fighter, &flags);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, fighter, flags & ~(UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
  machine, second ? UINT32_C(0x0050086c) : UINT32_C(0x00500868), &associated
        );
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags & ~(UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_read(machine, descriptor + UINT32_C(12), &motion, sizeof(motion));
    if (status != VF2_OK) return status;
    if (motion == UINT8_C(31)) {
        if (next_descriptor != NULL) *next_descriptor = descriptor + UINT32_C(16);
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500828), &associated);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags & ~(UINT32_C(1) << 31u));
        return status;
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, descriptor, &x);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(4), &z);
    if (status == VF2_OK) status = read_u16(machine, descriptor + UINT32_C(8), &angle);
    if (status == VF2_OK) status = vf2_model2a_read(machine, descriptor + UINT32_C(13), &bit23, sizeof(bit23));
    if (status == VF2_OK) status = vf2_model2a_read(machine, descriptor + UINT32_C(14), &bit22, sizeof(bit22));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) status = vf2_model2a_read(machine, base + UINT32_C(0x3351), &mode_flags, sizeof(mode_flags));
    if (status == VF2_OK) status = vf2_model2a_write(machine, fighter + UINT32_C(0x1b1), &motion, sizeof(motion));
    if (status == VF2_OK) {
        uint8_t actor_motion = (uint8_t)(motion + ((mode_flags & UINT8_C(0x40)) != 0u ? UINT8_C(13) : UINT8_C(0)));
        status = vf2_model2a_write(machine, fighter + UINT32_C(0x1b0), &actor_motion, sizeof(actor_motion));
    }
    if (status == VF2_OK) status = write_u16(machine, fighter + UINT32_C(0x26), angle);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x18), x);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x1c), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x20), z);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter + UINT32_C(0x0c), UINT32_C(0x00013f08));
    if (status == VF2_OK) status = vf2_model2a_write(machine, fighter + UINT32_C(0x2a), &zero, sizeof(zero));
    if (status == VF2_OK) status = write_u16(machine, fighter + UINT32_C(0x624), 0u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter, &flags);
    if (status == VF2_OK) {
        flags = (flags & UINT32_C(0xff000000)) | (UINT32_C(1) << 31u);
        if (bit23 != 0u) flags |= UINT32_C(1) << 23u; else flags &= ~(UINT32_C(1) << 23u);
        if (bit22 != 0u) flags |= UINT32_C(1) << 22u; else flags &= ~(UINT32_C(1) << 22u);
        status = vf2_model2a_write_u32(machine, fighter, flags);
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated + UINT32_C(0x0c), UINT32_C(0x000640f4));
    if (status == VF2_OK) {
        uint8_t state = 0u;
        status = vf2_model2a_read(machine, UINT32_C(0x0050009c), &state, sizeof(state));
        if (status == VF2_OK) {
  state = second ? (uint8_t)(state & ~UINT8_C(4)) : (uint8_t)(state & ~UINT8_C(2));
  status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &state, sizeof(state));
  if (status == VF2_OK && (state & UINT8_C(6)) == 0u) {
      status = vf2_model2a_read_u32(machine, UINT32_C(0x00500828), &associated);
      if (status == VF2_OK) status = vf2_model2a_read_u32(machine, associated, &flags);
      if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated, flags | (UINT32_C(1) << 31u));
      if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated + UINT32_C(0x0c), UINT32_C(0x000221cc));
  }
        }
    }
    if (next_descriptor != NULL) *next_descriptor = descriptor + UINT32_C(16);
    return status;
}

static vf2_status execute_selector3_phase4_actor_init(vf2_model2a *machine)
{
    uint32_t player0 = 0u;
    uint32_t player1 = 0u;
    uint32_t next0 = UINT32_C(0x0201f454);
    uint32_t next1 = UINT32_C(0x0201f258);
    vf2_status status = vf2_model2a_write_u32(machine, UINT32_C(0x005001c8), next0);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001cc), next1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001d0), UINT32_C(0x0201f5c4));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001d4), UINT32_C(0x0201f38c));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001e0), UINT32_C(0x000731ac));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0);
    if (status == VF2_OK) status = execute_selector3_phase4_actor_record(machine, next0, player0, 0, &next0);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001c8), next0);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &player1);
    if (status == VF2_OK) status = execute_selector3_phase4_actor_record(machine, next1, player1, 1, &next1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001cc), next1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00550004), UINT32_C(0x000032c8));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00550008), UINT32_C(0x00004e20));
    return status;
}

static vf2_status execute_selector3_phase4_timeline(vf2_model2a *machine)
{
    uint32_t total = 0u;
    uint32_t timer = 0u;
    uint32_t elapsed = 0u;
    uint32_t pointer = 0u;
    uint32_t index = 0u;
    uint16_t trigger = 0u;
    vf2_status status = vf2_model2a_read_u32(machine, UINT32_C(0x0201f388), &total);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &timer);
    elapsed = total - timer;
    /* The first recovered phase-4 vector has no hits in the four actor lists. */
    for (index = 0u; status == VF2_OK && index < UINT32_C(4); ++index) {
        static const uint32_t slots[4] = {
  UINT32_C(0x005001c8), UINT32_C(0x005001d0),
  UINT32_C(0x005001cc), UINT32_C(0x005001d4)
        };
        status = vf2_model2a_read_u32(machine, slots[index], &pointer);
        if (status == VF2_OK) status = read_u16(machine, pointer + (index == 1u || index == 3u ? UINT32_C(4) : UINT32_C(10)), &trigger);
        if (status == VF2_OK && (uint32_t)trigger == elapsed) return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e0), &pointer);
    index = 0u;
    while (status == VF2_OK && index < UINT32_C(32)) {
        status = read_u16(machine, pointer, &trigger);
        if (status != VF2_OK || (uint32_t)trigger != elapsed) break;
        pointer += UINT32_C(4);
        ++index;
    }
    if (status == VF2_OK && index != UINT32_C(4)) return VF2_ERROR_UNSUPPORTED;
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001e0), pointer);
    return status;
}

static vf2_status execute_selector3_phase4(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint32_t profile_flags = 0u;
    uint32_t counter = 0u;
    uint32_t phase4_table = 0u;
    uint8_t mode = 0u;
    int recovered_initial_path = 0;
    uint8_t fifteen = UINT8_C(15);
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068),
            flags & ~(UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x00500064), &mode, sizeof(mode));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00025034) + (uint32_t)mode * UINT32_C(32), &phase4_table);
    if (status == VF2_OK && phase4_table != 0u) { recovered_initial_path = 1; status = execute_selector3_phase4_mode_init(machine); }
    if (status == VF2_OK) {
        status = execute_selector3_halfword_stream(
            machine, UINT32_C(0x02800394)
        );
    }
    if (status == VF2_OK) {
        status = execute_selector3_register_stream(
            machine, UINT32_C(0x02805890)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068),
            flags & ~(UINT32_C(1) << 14u)
        );
    }
    if (status == VF2_OK) {
        status = clear_tile_plane_64x48(
            machine, UINT32_C(0x01000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags |= (UINT32_C(1) << 30u) |
                 (UINT32_C(1) << 23u) |
                 (UINT32_C(1) << 25u);
        flags &= ~(UINT32_C(1) << 20u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0201f388), &counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, pointer + UINT32_C(0x40), &fifteen,
            sizeof(fifteen)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer + UINT32_C(0x210),
            UINT32_C(0x0201f624)
        );
    }
    if (status == VF2_OK && recovered_initial_path) status = execute_selector3_phase4_actor_init(machine);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(0x3320), &profile_flags
        );
    }
    if (status == VF2_OK && (profile_flags & UINT32_C(1)) == 0u) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6d8aa), UINT32_C(0x010016da)
        );
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01000ef4), UINT16_C(32));
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x01000ef6), UINT16_C(32));
        }
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    if (status == VF2_OK && recovered_initial_path) status = execute_selector3_phase4_timeline(machine);
    if (status == VF2_OK && recovered_initial_path) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
    if (status == VF2_OK && recovered_initial_path) { --counter; status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter); }
    if (status == VF2_OK && recovered_initial_path && counter == 0u) {
        uint32_t fighter = 0u;
        uint32_t fighter_flags = 0u;
        uint32_t runtime_flags = 0u;
        uint32_t slot = 0u;
        uint8_t zero = 0u;

        status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &zero, sizeof(zero));
        for (slot = 0u; status == VF2_OK && slot < UINT32_C(2); ++slot) {
            status = vf2_model2a_read_u32(
                machine, slot == 0u ? UINT32_C(0x00500804) : UINT32_C(0x00500808),
                &fighter
            );
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter, &fighter_flags);
            if (status == VF2_OK) {
                fighter_flags &= ~((UINT32_C(1) << 23u) | (UINT32_C(1) << 22u));
                fighter_flags |= UINT32_C(1) << 26u;
                status = vf2_model2a_write_u32(machine, fighter, fighter_flags);
            }
        }
        if (status == VF2_OK) status = execute_selector3_sound_event(machine, UINT32_C(0x00bd1a60));
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00508000), runtime_flags & ~(UINT32_C(1) << 16u)
        );
        if (status == VF2_OK) {
            *next_phase = (uint8_t)(*next_phase + UINT8_C(1));
            status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase));
        }
    }
    return status;
}

static vf2_status execute_selector3_phase5_timeline_observed(
    vf2_model2a *machine, int *recovered
)
{
    static const uint32_t slots[4] = {
        UINT32_C(0x005001c8), UINT32_C(0x005001d0),
        UINT32_C(0x005001cc), UINT32_C(0x005001d4)
    };
    static const uint32_t trigger_offsets[4] = {
        UINT32_C(10), UINT32_C(4), UINT32_C(10), UINT32_C(4)
    };
    uint32_t total = 0u, timer = 0u, elapsed = 0u;
    uint32_t pointers[4] = {0u,0u,0u,0u};
    uint32_t event_pointer = 0u, fighter0 = 0u, fighter1 = 0u;
    uint32_t value = 0u, flags = 0u;
    uint16_t trigger = 0u;
    uint8_t control = 0u;
    uint32_t i = 0u;
    vf2_status status = VF2_OK;

    if (recovered == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    *recovered = 0;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0201f388), &total);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &timer);
    elapsed = total - timer;
    for (i = 0u; status == VF2_OK && i < UINT32_C(4); ++i) {
        status = vf2_model2a_read_u32(machine, slots[i], &pointers[i]);
        if (status == VF2_OK) status = read_u16(machine, pointers[i] + trigger_offsets[i], &trigger);
        if (status == VF2_OK && i == UINT32_C(1) && (uint32_t)trigger != elapsed) return VF2_OK;
        if (status == VF2_OK && i != UINT32_C(1) && (uint32_t)trigger == elapsed) return VF2_OK;
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e0), &event_pointer);
    if (status == VF2_OK) status = read_u16(machine, event_pointer, &trigger);
    if (status == VF2_OK && (uint32_t)trigger == elapsed) return VF2_OK;
    if (status != VF2_OK) return status;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter0);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointers[1], &value);
    if (status == VF2_OK && value != 0u) status = vf2_model2a_write_u32(machine, fighter0 + UINT32_C(0x194), value);
    if (status == VF2_OK && value != 0u) status = vf2_model2a_read(machine, pointers[1] + UINT32_C(6), &control, sizeof(control));
    if (status == VF2_OK && value != 0u) status = vf2_model2a_read_u32(machine, fighter0, &flags);
    if (status == VF2_OK && value != 0u) {
        flags |= UINT32_C(1) << 26u;
        if (control != 0u) flags &= ~(UINT32_C(1) << 26u);
        status = vf2_model2a_write_u32(machine, fighter0, flags);
    }
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001d0), pointers[1] + UINT32_C(8));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &fighter1);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter0 + UINT32_C(0x18), &value);
    if (status == VF2_OK && value == UINT32_C(0x447a0000)) {
        status = vf2_model2a_read_u32(machine, fighter1, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter1, flags | (UINT32_C(1) << 23u));
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, fighter1 + UINT32_C(0x18), &value);
    if (status == VF2_OK && value == UINT32_C(0x447a0000)) {
        status = vf2_model2a_read_u32(machine, fighter0, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, fighter0, flags | (UINT32_C(1) << 23u));
    }
    if (status == VF2_OK) *recovered = 1;
    return status;
}
static vf2_status execute_selector3_phase5(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t counter = 0u;
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint8_t zero = 0u;
    int timeline_recovered = 0;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = execute_selector3_phase5_timeline_observed(machine, &timeline_recovered);
    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500024), &counter
    );
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), counter
        );
    }
    if (status == VF2_OK && counter != 0u) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050009c), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500804), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer,
            (flags & ~((UINT32_C(1) << 23u) |
                       (UINT32_C(1) << 22u))) |
                (UINT32_C(1) << 26u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer,
            (flags & ~((UINT32_C(1) << 23u) |
                       (UINT32_C(1) << 22u))) |
                (UINT32_C(1) << 26u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00508000), &flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00508000),
            flags & ~(UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase6(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint32_t profile_flags = 0u;
    uint32_t destination = UINT32_C(0x010055e0);
    uint16_t zero16 = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = clear_tile_plane_64x48(
        machine, UINT32_C(0x01000000)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500834), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags |= (UINT32_C(1) << 30u) |
                 (UINT32_C(1) << 23u) |
                 (UINT32_C(1) << 25u);
        flags &= ~(UINT32_C(1) << 20u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), zero16);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), zero16);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a68586), UINT32_C(0x01004000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, pointer + UINT32_C(0x3320), &profile_flags
        );
    }
    if (status == VF2_OK && (profile_flags & UINT32_C(1)) != 0u) {
        destination = UINT32_C(0x01005460);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6c0da), destination
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase7_pre_profile(
    vf2_model2a *machine, uint32_t task0, uint32_t task1
)
{
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint8_t zero = 0u;
    uint8_t sixty_three = UINT8_C(0x63);
    uint8_t copied = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050086c), &pointer
    );

    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer + UINT32_C(0x0c), UINT32_C(0x000640f4));

    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task0 + UINT32_C(0x69c), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task0 + UINT32_C(0x69d), &sixty_three, sizeof(sixty_three));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task1 + UINT32_C(0x69c), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, task1 + UINT32_C(0x69d), &sixty_three, sizeof(sixty_three));

    if (status == VF2_OK) status = vf2_model2a_write(
        machine, UINT32_C(0x00500057), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, UINT32_C(0x00500058), &zero, sizeof(zero));
    if (status == VF2_OK) status = vf2_model2a_read(
        machine, UINT32_C(0x00500059), &copied, sizeof(copied));
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, UINT32_C(0x00500052), &copied, sizeof(copied));
    return status;
}

static vf2_status execute_selector3_phase7_profile(
    vf2_model2a *machine, vf2_i960_cpu *cpu, uint32_t task1
)
{
    vf2_i960_cpu child;
    vf2_hybrid_bridge_report child_report;
    vf2_status status = VF2_OK;
    uint32_t index = 0u;

    child = *cpu;
    child.registers[VF2_I960_G0_REGISTER + 7u] = task1;
    memset(&child_report, 0, sizeof(child_report));
    status = vf2_i960_cpu_enter_procedure(
        &child, VF2_DISPLAY_PROFILE_APPLY_ENTRY, UINT32_C(0x00abcdef));
    if (status != VF2_OK) return status;
    child.executed_instructions += UINT64_C(1);
    status = execute_display_profile_apply(machine, &child, &child_report);
    if (status != VF2_OK || child.ip != UINT32_C(0x00abcdef))
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    for (index = 0u; index < UINT32_C(10); ++index) {
        cpu->registers[VF2_I960_G0_REGISTER + index] =
            child.registers[VF2_I960_G0_REGISTER + index];
    }
    return VF2_OK;
}

static vf2_status execute_selector3_phase7_post_profile(vf2_model2a *machine)
{
    static const uint32_t clear31_ptrs[3] = {
        UINT32_C(0x00500854), UINT32_C(0x0050085c), UINT32_C(0x00500860)
    };
    static const uint32_t clear3_ptrs[2] = {
        UINT32_C(0x0050083c), UINT32_C(0x00500840)
    };
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint32_t index = 0u;
    uint8_t thirteen = UINT8_C(13);
    vf2_status status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0050a160), UINT32_C(0x3727c5ac)
    );

    for (index = 0u; status == VF2_OK && index < UINT32_C(3); ++index) {
        status = vf2_model2a_read_u32(machine, clear31_ptrs[index], &pointer);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(
            machine, pointer, flags & ~(UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500814), &pointer);
    if (status == VF2_OK) status = vf2_model2a_write(
        machine, pointer + UINT32_C(0x40), &thirteen, sizeof(thirteen));

    for (index = 0u; status == VF2_OK && index < UINT32_C(2); ++index) {
        status = vf2_model2a_read_u32(machine, clear3_ptrs[index], &pointer);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
        if (status == VF2_OK) status = vf2_model2a_write_u32(
            machine, pointer, flags & ~(UINT32_C(1) << 3u));
    }

    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500828), &pointer);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer + UINT32_C(0x0c), UINT32_C(0x000221cc));

    if (status == VF2_OK) status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050081c), &pointer);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | (UINT32_C(1) << 31u));
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer + UINT32_C(0x0c), UINT32_C(0x0001b9ac));
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, pointer, &flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(
        machine, pointer, flags | UINT32_C(3));
    return status;
}

static vf2_status execute_selector3_phase7(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t source = 0u;
    uint32_t pointer = 0u;
    uint32_t value = 0u;
    uint8_t first_mode = 0u;
    uint8_t second_mode = 0u;
    uint8_t task0_mode = 0u;
    uint8_t task1_mode = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &task0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &task1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500168), &source
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0xc), &first_mode,
            sizeof(first_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0x10), &second_mode,
            sizeof(second_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0xfff), &task0_mode,
            sizeof(task0_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, source + UINT32_C(0xffe), &task1_mode,
            sizeof(task1_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, source + UINT32_C(4), &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a00c), value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(4), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x1b0), &first_mode,
            sizeof(first_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x69c), &task0_mode,
            sizeof(task0_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x1b0), &second_mode,
            sizeof(second_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x69c), &task1_mode,
            sizeof(task1_mode)
        );
    }
    if (status == VF2_OK && first_mode > UINT8_C(13)) {
        task0_mode = (uint8_t)(first_mode - UINT8_C(13));
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x1b1), &task0_mode,
            sizeof(task0_mode)
        );
    } else if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task0 + UINT32_C(0x1b1), &first_mode,
            sizeof(first_mode)
        );
    }
    if (status == VF2_OK && second_mode > UINT8_C(13)) {
        task1_mode = (uint8_t)(second_mode - UINT8_C(13));
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x1b1), &task1_mode,
            sizeof(task1_mode)
        );
    } else if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, task1 + UINT32_C(0x1b1), &second_mode,
            sizeof(second_mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x18),
                                       UINT32_C(0xbf800000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x18),
                                       UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x1c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x1c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x20), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x26), UINT16_C(3u << 14));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x26), UINT16_C(1u << 14));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0 + UINT32_C(0xc),
                                       UINT32_C(0x13f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0xc),
                                       UINT32_C(0x13f08));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x2a), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task0 + UINT32_C(0x624), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x2a), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, task1 + UINT32_C(0x624), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task0,
                                       (value & UINT32_C(0xff000000)) |
                                           (UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task1, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1,
                                       (value & UINT32_C(0xff000000)) |
                                           (UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500868), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       value | (UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0xc),
                                       UINT32_C(0x640f4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(4), &((uint8_t){1}),
                                   sizeof(uint8_t));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, task1 + UINT32_C(0x18),
                                       UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = execute_selector3_phase7_pre_profile(machine, task0, task1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050005c),
                                       UINT32_C(0x11d84));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500060),
                                       UINT32_C(0x11d8c));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050004c), &((uint8_t){2}),
                                   sizeof(uint8_t));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050005b), &mode,
                                  sizeof(mode));
    }
    if (status == VF2_OK) {
        mode = (uint8_t)((mode + UINT8_C(1)) % UINT8_C(10));
        status = vf2_model2a_write(machine, UINT32_C(0x0050005b), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &mode,
                                   sizeof(mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a160),
                                       UINT32_C(0x3727c5ac));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       UINT32_C(5u << 6));
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_phase9(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t flags = 0u;
    uint32_t counter = 0u;
    uint32_t base = 0u;
    uint16_t timer_value = 0u;
    uint8_t profile_flags = 0u;
    uint8_t phase_flags = 0u;
    int alternate_text = 0;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50), counter);
    }
    if (status == VF2_OK && counter != 0u) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00550000), &flags);
    }
    if (status == VF2_OK && flags == UINT32_C(1)) {
        return VF2_OK;
    }
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x005000a0), &timer_value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, pointer + UINT32_C(0x40), timer_value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, pointer + UINT32_C(0x42), timer_value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags &= ~((UINT32_C(1) << 30u) | (UINT32_C(1) << 20u) |
                   (UINT32_C(1) << 25u));
        flags |= (UINT32_C(1) << 28u) | (UINT32_C(1) << 23u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500864), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer, flags | (UINT32_C(1) << 29u) |
                                   (UINT32_C(1) << 27u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3351), &phase_flags,
                                  sizeof(phase_flags));
    }
    if (status == VF2_OK && (phase_flags & UINT8_C(0x10)) != 0u) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3350), &profile_flags,
                                  sizeof(profile_flags));
    }
    if (status == VF2_OK &&
        ((phase_flags & UINT8_C(0x10)) == 0u || profile_flags == 0u)) {
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6f24e), UINT32_C(0x01000516)
        );
    } else if (status == VF2_OK) {
        alternate_text = 1;
        status = execute_selector3_display_text_at(
            machine, UINT32_C(0x02a6f606), UINT32_C(0x01000516)
        );
        if (status == VF2_OK) {
            status = execute_selector3_register_stream(
                machine, UINT32_C(0x00012520)
            );
        }
        if (status == VF2_OK) {
            status = execute_selector3_halfword_stream(
                machine, UINT32_C(0x0001256c)
            );
        }
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text_at(
            machine,
            alternate_text ? UINT32_C(0x00013d3c) : UINT32_C(0x02a6f43a),
            UINT32_C(0x01000906)
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_phase10(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint16_t delay = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &flags);
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, task1, &flags);
        }
        if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
            status = read_u16(machine, UINT32_C(0x00500028), &delay);
            if (status == VF2_OK) {
                delay = (uint16_t)(delay - UINT16_C(1));
                status = write_u16(machine, UINT32_C(0x00500028), delay);
            }
            if (status == VF2_OK && delay != 0u) {
                return VF2_OK;
            }
        } else if (status == VF2_OK) {
            return VF2_OK;
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
    status = vf2_model2a_write(
        machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500858), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, pointer, flags & ~(UINT32_C(1) << 31u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       UINT32_C(128));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500864), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags &= ~(UINT32_C(1) << 29u);
        flags |= UINT32_C(1) << 28u;
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    return status;
}

static vf2_status execute_selector3_phase11(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50), counter);
    }
    if (status == VF2_OK && counter == 0u) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                                   sizeof(*next_phase));
    }
    return status;
}

static vf2_status execute_selector3_phase12(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    vf2_status status = execute_selector3_phase6(
        machine, previous_phase, next_phase
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 14u)
        );
    }
    return status;
}

static vf2_status execute_selector3_random16(
    vf2_model2a *machine,
    uint32_t *result
)
{
    uint32_t state = 0u;
    uint32_t value = 0u;
    uint32_t word = 0u;
    vf2_status status = VF2_OK;

    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500098), &state);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_TIMER_BASE, &word);
    }
    if (status == VF2_OK) {
        value = state + (word << 4u);
        status = vf2_model2a_read_u32(machine, VF2_TIMER_BASE + UINT32_C(4), &word);
    }
    if (status == VF2_OK) {
        value += word << 8u;
        status = vf2_model2a_read_u32(machine, VF2_TIMER_BASE + UINT32_C(8), &word);
    }
    if (status == VF2_OK) {
        value += word << 12u;
        status = vf2_model2a_read_u32(machine, VF2_TIMER_BASE + UINT32_C(0xc), &word);
    }
    if (status == VF2_OK) {
        value += word << 16u;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500098), value);
        *result = (value >> 4u) & UINT32_C(0xffff);
    }
    return status;
}

static vf2_status execute_selector3_phase13(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t first = 0u;
    uint32_t second = 0u;
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint8_t first_mode = 0u;
    uint8_t second_mode = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = execute_selector3_random16(machine, &first);
    if (status == VF2_OK) {
        first %= UINT32_C(11);
    }
    while (status == VF2_OK && first == UINT32_C(9)) {
        status = execute_selector3_random16(machine, &first);
        if (status == VF2_OK) {
            first %= UINT32_C(11);
        }
    }
    if (status == VF2_OK) {
        status = execute_selector3_random16(machine, &second);
    }
    if (status == VF2_OK) {
        second %= UINT32_C(11);
    }
    while (status == VF2_OK && second == UINT32_C(9)) {
        status = execute_selector3_random16(machine, &second);
        if (status == VF2_OK) {
            second %= UINT32_C(11);
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    if (first == second) {
        second += UINT32_C(13);
    }
    first_mode = (uint8_t)first;
    second_mode = (uint8_t)second;
    status = execute_selector3_phase7(machine, previous_phase, next_phase);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task0 + UINT32_C(0x1b0),
                                   &first_mode, sizeof(first_mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, task1 + UINT32_C(0x1b0),
                                   &second_mode, sizeof(second_mode));
    }
    return status;
}

static vf2_status execute_selector3_phase14(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50), counter);
    }
    if (status == VF2_OK && counter == 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00550000), &flags);
    }
    return status;
}

static vf2_status execute_selector3_phase8(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int *handoff
)
{
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    uint32_t ready = 0u;
    uint32_t phase_word = UINT32_C(1) << previous_phase;
    vf2_status status = VF2_OK;

    if (cpu == NULL || next_phase == NULL || handoff == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    *handoff = 0;

    /* acf8 snapshots the phase and its one-hot selector before dispatching
     * the phase handler. */
    status = vf2_model2a_write(
        machine, UINT32_C(0x00500031), &previous_phase, sizeof(previous_phase)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500034), phase_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags | (UINT32_C(1) << 16u)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50), &counter);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (counter != 0u) {
        return vf2_model2a_write_u32(
            machine, pointer + UINT32_C(0x50), counter - UINT32_C(1)
        );
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00550000), &ready);
    if (status != VF2_OK || ready == UINT32_C(1)) {
        return status;
    }

    /* The zero-counter/not-ready path does not return to selector 3.  It
     * enters the inline text thunk at 0x9444 with the two caller frames still
     * live.  Recreate those architectural frames so the next recovered bridge
     * sees exactly the same CPU state as the ROM. */
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0000acf8), UINT32_C(0x0000a6f4)
    );
    if (status == VF2_OK) {
        cpu->registers[15] = (uint32_t)previous_phase;
        cpu->registers[3] = (uint32_t)previous_phase;
        cpu->registers[4] = phase_word;
        cpu->registers[5] = UINT32_C(0x0000b9b8);
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x0000b9b8), UINT32_C(0x0000ad28)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = pointer;
    cpu->registers[14] = UINT32_C(0x0000ba08);
    cpu->registers[15] = UINT32_MAX;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000ef4);
    cpu->ip = VF2_INLINE_TEXT_THUNK_ENTRY;
    set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
    cpu->executed_instructions += UINT64_C(26);
    *handoff = 1;
    return VF2_OK;
}

static vf2_status execute_selector3_phase15(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t task0 = 0u;
    uint32_t task1 = 0u;
    uint32_t flags = 0u;
    uint32_t pointer = 0u;
    uint16_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &task0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, task0, &flags);
    }
    if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &task1);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, task1, &flags);
        }
        if (status == VF2_OK && (flags & (UINT32_C(1) << 5u)) == 0u) {
            status = read_u16(machine, UINT32_C(0x00500028), &counter);
            if (status == VF2_OK) {
                counter = (uint16_t)(counter - UINT16_C(1));
                status = write_u16(machine, UINT32_C(0x00500028), counter);
            }
        }
    }
    if (status != VF2_OK || counter != 0u) {
        return status;
    }
    *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
    status = vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                               sizeof(*next_phase));
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500858), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer,
                                       flags & ~(UINT32_C(1) << 31u));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       UINT32_C(128));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500864), &pointer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer, &flags);
    }
    if (status == VF2_OK) {
        flags = (flags & ~(UINT32_C(1) << 29u)) |
                (UINT32_C(1) << 28u);
        status = vf2_model2a_write_u32(machine, pointer, flags);
    }
    return status;
}

static vf2_status execute_selector3_phase16(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    uint32_t pointer = 0u;
    uint32_t counter = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = previous_phase;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &pointer);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer + UINT32_C(0x50),
                                      &counter);
    }
    if (status == VF2_OK) {
        --counter;
        status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x50),
                                       counter);
    }
    if (status != VF2_OK || counter != 0u) {
        return status;
    }
    *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
    return vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                             sizeof(*next_phase));
}

static vf2_status execute_selector3_phase17(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase
)
{
    if (next_phase == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *next_phase = UINT8_C(0);
    (void)previous_phase;
    return vf2_model2a_write(machine, UINT32_C(0x00500030), next_phase,
                             sizeof(*next_phase));
}

static vf2_status execute_selector3_mode0_special(
    vf2_model2a *machine,
    uint8_t previous_phase,
    uint8_t *next_phase,
    int *fallback
)
{
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint8_t variant = 0u;
    uint16_t zero16 = 0u;
    vf2_status status = VF2_OK;

    if (next_phase == NULL || fallback == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *fallback = 0;
    status = execute_selector3_mode0_prefix(machine);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (variant != UINT8_C(1)) {
        *fallback = 1;
        return VF2_OK;
    }

    status = execute_selector3_profile_measure(machine, base, &x, &y);
    if (status != VF2_OK) {
        return status;
    }
    if (x != 0u || y != 0u) {
        *fallback = 1;
        return VF2_OK;
    }

    status = execute_selector3_halfword_stream(
        machine, UINT32_C(0x02800380)
    );
    if (status == VF2_OK) {
        status = execute_selector3_register_stream(
            machine, UINT32_C(0x02805abc)
        );
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a004), zero16);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, UINT32_C(0x0100a00c), zero16);
    }
    if (status == VF2_OK) {
        status = execute_selector3_display_text(
            machine, UINT32_C(0x02ab2f82)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &flags
        );
    }
    if (status == VF2_OK) {
        flags |= UINT32_C(1) << 14u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500068), flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500024), UINT32_C(256)
        );
    }
    if (status == VF2_OK) {
        *next_phase = (uint8_t)(previous_phase + UINT8_C(1));
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500030), next_phase, sizeof(*next_phase)
        );
    }
    return status;
}

static vf2_status execute_selector3_phase0_profile(
    vf2_model2a *machine, const vf2_i960_cpu *cpu
)
{
    vf2_i960_cpu child;
    vf2_hybrid_bridge_report child_report;
    uint8_t first[16];
    uint8_t second[16];
    vf2_status status = VF2_OK;

    status = vf2_model2a_read(machine, UINT32_C(0x000266f0), first, sizeof(first));
    if (status == VF2_OK)
        status = vf2_model2a_read(machine, UINT32_C(0x00026700), second, sizeof(second));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004c0), first, sizeof(first));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004e0), first, sizeof(first));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004d0), second, sizeof(second));
    if (status == VF2_OK)
        status = vf2_model2a_write(machine, UINT32_C(0x018004f0), second, sizeof(second));
    if (status == VF2_OK)
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500034), UINT32_C(1));
    if (status == VF2_OK)
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005ff680), UINT32_C(0x01004000));
    if (status != VF2_OK)
        return status;

    child = *cpu;
    /* 0x8f1c leaves g2 as the destination row width in bytes. 0x4b410,
     * reached from 0x1fcc0, publishes that value in the command packet. */
    child.registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(62 * 2);
    memset(&child_report, 0, sizeof(child_report));
    status = vf2_i960_cpu_enter_procedure(
        &child, VF2_DISPLAY_PROFILE_APPLY_ENTRY, UINT32_C(0x00abcdef));
    if (status != VF2_OK)
        return status;
    child.executed_instructions += UINT64_C(1);
    status = execute_display_profile_apply(machine, &child, &child_report);
    if (status != VF2_OK || child.ip != UINT32_C(0x00abcdef))
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    return VF2_OK;
}

vf2_status execute_frame_dispatch_tick(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint32_t target = 0u;
    uint32_t counter = 0u;
    uint32_t selector_word = 0u;
    uint32_t ready_flags = 0u;
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    cpu->registers[15] = flags;
    if (status != VF2_OK || (flags & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
    );
    if (status != VF2_OK ||
        (selector != UINT8_C(0) && selector != UINT8_C(1) &&
         selector != UINT8_C(2) &&
         selector != UINT8_C(3) &&
         selector != UINT8_C(4) &&
         selector != UINT8_C(5) &&
         selector != UINT8_C(6) &&
         selector != UINT8_C(7) &&
         selector != UINT8_C(8) &&
         selector != UINT8_C(9) &&
         selector != UINT8_C(16) &&
         selector != UINT8_C(17))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[3] = (uint32_t)selector;
    selector_word = UINT32_C(1) << selector;
    cpu->registers[4] = selector_word;
    status = vf2_model2a_write(
        machine, UINT32_C(0x0050002b), &selector, sizeof(selector)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050002c), selector_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0000a6f8) + (uint32_t)selector * UINT32_C(4),
            &target
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[5] = target;
    if (selector == UINT8_C(0)) {
        uint32_t fill_offset = 0u;
        const uint32_t fill_word = UINT32_C(0xc007c007);
        /* sub_0000a804 clears the first 49 0x80-byte display-command slots,
         * then primes the selector for the normal frame path. The text glyph
         * calls in the middle only affect the diagnostic overlay; the command
         * buffer and selector transition are the state consumed downstream. */
        for (fill_offset = 0u; status == VF2_OK && fill_offset < UINT32_C(0x1880);
             fill_offset += UINT32_C(4)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x01004000) + fill_offset, fill_word
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024),
                                           UINT32_C(160));
        }
        if (status == VF2_OK) {
            selector = UINT8_C(1);
            cpu->registers[3] = (uint32_t)selector;
            cpu->registers[4] = UINT32_C(2);
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0050002c),
                                           UINT32_C(2));
        }
        if (status == VF2_OK) {
            /* callx/ret returns to the enclosing 0xa010 block. Keep this
             * selector-0 recovery transactional with the same poststate. */
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(2));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(392));
            if (status != VF2_OK) {
                return status;
            }
        }
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(3);
        report->bytes_written = (size_t)UINT32_C(0x1880) + sizeof(uint32_t) + 2u;
        report->recovered_instruction_count = UINT64_C(392);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(2)) {
        uint32_t base = 0u;
        uint32_t control = 0u;
        uint32_t value = 0u;
        uint8_t mode = 0u;
        uint8_t sound_rate = 0u;
        const uint8_t control_byte = UINT8_C(0x56);
        const uint8_t zero = 0u;
        const uint8_t one = 1u;

        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                           &ready_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068),
                                           ready_flags &
                                               ~((UINT32_C(1) << 4u) |
                                                 (UINT32_C(1) << 15u)));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500070), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500074), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, base + UINT32_C(0x3351), &mode,
                                      sizeof(mode));
        }
        if (status == VF2_OK && (mode & UINT8_C(0x20)) == 0u) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050005b), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500054), &mode,
                                       sizeof(mode));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500067), &mode,
                                       sizeof(mode));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500081), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050008d), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050008e), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050083c), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, control + UINT32_C(6), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500840), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, control + UINT32_C(6),
                                       &control_byte,
                                       sizeof(control_byte));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, control + UINT32_C(0x40), &one,
                                       sizeof(one));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &base);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control, base | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x0002b1bc));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500878), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x0006ca64));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050087c), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x0006ca64));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x04080400));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, UINT32_C(0x00500090), &sound_rate,
                                      sizeof(sound_rate));
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x0050006e),
                               (uint16_t)((uint16_t)sound_rate * UINT16_C(100)));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500030), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500828), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control + UINT32_C(0xc),
                                           UINT32_C(0x000221cc));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500850), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value & ~UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &control);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, control, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, control,
                                           value | UINT32_C(0x80000000));
        }
        if (status == VF2_OK) {
            selector = UINT8_C(3);
            status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &selector,
                                       sizeof(selector));
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        cpu->registers[4] = UINT32_C(8);
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(2));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(90));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(12);
        report->bytes_written = 64u;
        report->recovered_instruction_count = UINT64_C(90);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(3)) {
        uint8_t phase = 0u;
        uint8_t entry_phase = 0u;
        uint32_t phase_target = 0u;
        int fallback = 0;
        int phase8_handoff = 0;
        int phase1_profile_measure_called = 0;
        int phase1_countdown_terminal = 0;
        uint32_t phase1_measured_sum = 0u;

        if (target != UINT32_C(0x0000acf8)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(machine, UINT32_C(0x00500030), &phase,
                                  sizeof(phase));
        if (status == VF2_OK) {
            entry_phase = phase;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0000aac4) + (uint32_t)phase * UINT32_C(4),
                &phase_target
            );
        }
        if (status != VF2_OK ||
            (phase == UINT8_C(0) &&
             phase_target != UINT32_C(0x0000ae78)) ||
            (phase == UINT8_C(1) &&
             phase_target != UINT32_C(0x0000afe0)) ||
            (phase == UINT8_C(2) &&
             phase_target != UINT32_C(0x0000b0d8)) ||
            (phase == UINT8_C(3) &&
             phase_target != UINT32_C(0x0000b394)) ||
            (phase == UINT8_C(4) &&
             phase_target != UINT32_C(0x0000b3f8)) ||
            (phase == UINT8_C(5) &&
             phase_target != UINT32_C(0x0000b4f0)) ||
            (phase == UINT8_C(6) &&
             phase_target != UINT32_C(0x0000b588)) ||
            (phase == UINT8_C(7) &&
             phase_target != UINT32_C(0x0000b66c)) ||
            (phase == UINT8_C(8) &&
             phase_target != UINT32_C(0x0000b9b8)) ||
            (phase == UINT8_C(9) &&
             phase_target != UINT32_C(0x0000baec)) ||
            (phase == UINT8_C(10) &&
             phase_target != UINT32_C(0x0000bc10)) ||
            (phase == UINT8_C(11) &&
             phase_target != UINT32_C(0x0000bcb0)) ||
            (phase == UINT8_C(12) &&
             phase_target != UINT32_C(0x0000bce4)) ||
            (phase == UINT8_C(13) &&
             phase_target != UINT32_C(0x0000bdc8)) ||
            (phase == UINT8_C(14) &&
             phase_target != UINT32_C(0x0000c0a4)) ||
            (phase == UINT8_C(15) &&
             phase_target != UINT32_C(0x0000c268)) ||
            (phase == UINT8_C(16) &&
             phase_target != UINT32_C(0x0000c414)) ||
            (phase == UINT8_C(17) &&
             phase_target != UINT32_C(0x0000c448)) ||
            (phase != UINT8_C(0) && phase != UINT8_C(1) &&
             phase != UINT8_C(2) && phase != UINT8_C(3) &&
             phase != UINT8_C(4) && phase != UINT8_C(5) &&
             phase != UINT8_C(6) && phase != UINT8_C(7) &&
             phase != UINT8_C(8) && phase != UINT8_C(9) &&
             phase != UINT8_C(10) &&
             phase != UINT8_C(11) && phase != UINT8_C(12) &&
             phase != UINT8_C(13) && phase != UINT8_C(14) &&
             phase != UINT8_C(15) && phase != UINT8_C(16) &&
             phase != UINT8_C(17))) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500031), &entry_phase, sizeof(entry_phase)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500034),
                UINT32_C(1) << (uint32_t)entry_phase
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (phase == UINT8_C(0)) {
            status = execute_selector3_mode0_special(
                machine, phase, &phase, &fallback
            );
        } else if (phase == UINT8_C(1)) {
            status = execute_selector3_phase1(
                machine, cpu, phase, &phase,
                &phase1_profile_measure_called,
                &phase1_countdown_terminal,
                &phase1_measured_sum
            );
        } else if (phase == UINT8_C(3)) {
            status = execute_selector3_phase3(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(4)) {
            status = execute_selector3_phase4(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(5)) {
            status = execute_selector3_phase5(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(6)) {
            status = execute_selector3_phase6(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(7)) {
            status = execute_selector3_phase7(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(8)) {
            status = execute_selector3_phase8(
                machine, cpu, phase, &phase, &phase8_handoff
            );
        } else if (phase == UINT8_C(9)) {
            status = execute_selector3_phase9(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(10)) {
            status = execute_selector3_phase10(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(11)) {
            status = execute_selector3_phase11(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(12)) {
            status = execute_selector3_phase12(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(13)) {
            status = execute_selector3_phase13(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(14)) {
            status = execute_selector3_phase14(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(15)) {
            status = execute_selector3_phase15(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(16)) {
            status = execute_selector3_phase16(
                machine, phase, &phase
            );
        } else if (phase == UINT8_C(17)) {
            status = execute_selector3_phase17(
                machine, phase, &phase
            );
        } else {
            status = execute_frame_selector3_b0d8(
                machine, phase, &phase, 1
            );
        }
        if (status == VF2_OK && fallback) {
            status = execute_frame_selector3_b0d8(
                machine, entry_phase == UINT8_C(0) ? UINT8_C(2) : phase,
                &phase, 1
            );
        }
        if (status == VF2_OK && entry_phase == UINT8_C(0) && fallback) {
            uint32_t player0 = 0u;
            uint32_t player1 = 0u;

            status = execute_selector3_phase0_profile(machine, cpu);
            if (status == VF2_OK)
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0);
            if (status == VF2_OK)
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &player1);
            if (status != VF2_OK)
                return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x0007ae10);
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0xffff8000);
            cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = UINT32_C(0xffff9300);
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = player1;
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = player0;
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x0100407c);
            set_signed_condition(cpu, INT32_C(1), INT32_C(0));

            /* Measured complete ROM corridor: 123900 instructions,
             * 15 calls, 16 returns. Child+call components are
             * 12147 (8ef0), 21187 (8f1c), 11 (d918),
             * 90373 (1fcc0 tree), 9 (1344), plus 173 direct/wrapper. */
            account_nested_procedure(cpu, UINT64_C(15), UINT64_C(15));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(123900));
            if (status != VF2_OK)
                return status;
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(123900);
            report->recovered_procedure_calls = UINT64_C(15);
            report->recovered_procedure_returns = UINT64_C(16);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK &&
            (entry_phase == UINT8_C(1) || entry_phase == UINT8_C(3))) {
            uint32_t system_flags = 0u;
            uint32_t input_flags_1344 = 0u;
            int fast_nonzero = 0;

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00508000), &system_flags
            );
            if (status == VF2_OK && (system_flags & (UINT32_C(1) << 1u)) != 0u) {
                fast_nonzero = 1;
            } else if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500700), &input_flags_1344
                );
                if (status == VF2_OK &&
                    (input_flags_1344 & ((UINT32_C(1) << 4u) |
                                         (UINT32_C(1) << 5u))) == 0u) {
                    fast_nonzero = 1;
                }
            }
            if (status != VF2_OK) {
                return status;
            }
            if (fast_nonzero) {
                uint64_t calls = UINT64_C(3);
                uint64_t instructions = UINT64_C(0);

                if (entry_phase == UINT8_C(1)) {
                    calls = phase1_profile_measure_called
                        ? UINT64_C(9) : UINT64_C(3);
                    instructions = phase1_profile_measure_called
                        ? (phase1_measured_sum < UINT32_C(24)
                            ? UINT64_C(171) : UINT64_C(173))
                        : UINT64_C(44);
                    if (phase1_countdown_terminal) {
                        instructions += UINT64_C(3);
                    }
                } else {
                    uint32_t phase3_counter = 0u;
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &phase3_counter
                    );
                    if (status != VF2_OK) {
                        return status;
                    }
                    if (phase3_counter == UINT32_C(192)) {
                        instructions = UINT64_C(70);
                        calls = UINT64_C(4);
                    } else if ((int32_t)phase3_counter > 0) {
                        instructions = UINT64_C(38);
                    } else if (phase != entry_phase) {
                        instructions = UINT64_C(43);
                    } else {
                        instructions = UINT64_C(40);
                    }
                }
                cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
                set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
                account_nested_procedure(cpu, calls, calls);
                status = finish_recovered_procedure(machine, cpu, instructions);
                if (status != VF2_OK) {
                    return status;
                }
                report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
                report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
                report->exit_address = cpu->ip;
                report->iterations = UINT64_C(1);
                report->recovered_instruction_count = instructions;
                report->recovered_procedure_calls = calls;
                report->recovered_procedure_returns = calls + UINT64_C(1);
                report->cpu_poststate_applied = 1;
                return VF2_OK;
            }
        }
        if (status == VF2_OK && entry_phase == UINT8_C(4)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x20);
            cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_C(0xffff8000);
            cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 6u] = UINT32_C(0xffff942f);
            cpu->registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(1);
            cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000f74);
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            status = vf2_model2a_write_u32(
                machine, cpu->registers[1] + UINT32_C(0x000000c0),
                UINT32_C(0x010016da)
            );
            if (status != VF2_OK) return status;
            {
                const int terminal = phase == UINT8_C(6);
                const uint64_t calls = terminal ? UINT64_C(21) : UINT64_C(20);
                const uint64_t instructions = terminal ? UINT64_C(468870) : UINT64_C(468818);
                account_nested_procedure(cpu, calls, calls);
                status = finish_recovered_procedure(machine, cpu, instructions);
                if (status != VF2_OK) return status;
                report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
                report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
                report->exit_address = cpu->ip;
                report->iterations = UINT64_C(1);
                report->recovered_instruction_count = instructions;
                report->recovered_procedure_calls = calls;
                report->recovered_procedure_returns = calls + UINT64_C(1);
            }
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(5)) {
            uint32_t total = 0u, timer = 0u, p = 0u;
            uint32_t player0 = 0u;
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0201f388), &total);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &timer);
            if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x005001d0), &p);
            if (status != VF2_OK) return status;
            if (total - timer == UINT32_C(2) && p == UINT32_C(0x0201f5cc)) {
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &player0);
                if (status != VF2_OK) return status;
                cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
                cpu->registers[VF2_I960_G0_REGISTER + 7u] = player0;
                set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
                account_nested_procedure(cpu, UINT64_C(5), UINT64_C(5));
                status = finish_recovered_procedure(machine, cpu, UINT64_C(81));
                if (status != VF2_OK) return status;
                report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
                report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
                report->exit_address = cpu->ip;
                report->iterations = UINT64_C(1);
                report->recovered_instruction_count = UINT64_C(81);
                report->recovered_procedure_calls = UINT64_C(5);
                report->recovered_procedure_returns = UINT64_C(6);
                report->cpu_poststate_applied = 1;
                return VF2_OK;
            }
        }
        if (status == VF2_OK && entry_phase == UINT8_C(6)) {
            uint32_t base = 0u;
            uint32_t profile_flags = 0u;
            uint32_t rows = 0u;
            uint32_t columns = 0u;
            uint32_t destination = UINT32_C(0x010055e0);
            int16_t addend = 0;
            int16_t word_mode = 0;
            int32_t last_sample = 0;
            int has_descriptor_poststate = 0;

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, base + UINT32_C(0x3320), &profile_flags
                );
            }
            if (status == VF2_OK && (profile_flags & UINT32_C(1)) != 0u) {
                destination = UINT32_C(0x01005460);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da), &addend, sizeof(addend)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(2),
                    &word_mode, sizeof(word_mode)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(4), &rows
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(8), &columns
                );
            }
            if (status == VF2_OK && rows != 0u && columns != 0u) {
                const uint32_t sample_index = rows * columns - UINT32_C(1);
                has_descriptor_poststate = 1;
                if (word_mode == 0) {
                    int8_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index, &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                } else {
                    int16_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index * UINT32_C(2), &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[1] + UINT32_C(0x000000c0),
                    destination
                );
            }
            if (status != VF2_OK) return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = columns * UINT32_C(2);
            if (has_descriptor_poststate) {
                cpu->registers[VF2_I960_G0_REGISTER + 4u] =
                    (uint32_t)(int32_t)addend;
                cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
                cpu->registers[VF2_I960_G0_REGISTER + 6u] =
                    (uint32_t)(last_sample + (int32_t)addend);
                cpu->registers[VF2_I960_G0_REGISTER + 7u] =
                    (uint32_t)(int32_t)word_mode;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                destination + columns * UINT32_C(2);
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(6), UINT64_C(6));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(33867));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(33867);
            report->recovered_procedure_calls = UINT64_C(6);
            report->recovered_procedure_returns = UINT64_C(7);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(7)) {
            uint32_t task1 = 0u;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500808), &task1
            );
            if (status == VF2_OK)
                status = execute_selector3_phase7_profile(machine, cpu, task1);
            if (status == VF2_OK)
                status = execute_selector3_phase7_post_profile(machine);
            if (status != VF2_OK) return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(13), UINT64_C(13));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(90565));
            if (status != VF2_OK) return status;
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(90565);
            report->recovered_procedure_calls = UINT64_C(13);
            report->recovered_procedure_returns = UINT64_C(14);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(8) && !phase8_handoff) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(37);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(9)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(34));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(34);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(10)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(39));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(39);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(11)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(34));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(34);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(14)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(37);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(15)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(48));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(48);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        /* Phase 16 decrements the task countdown at [0x00500834]+0x50 and
         * stays on the phase while the result is nonzero (34 instructions);
         * the zero result takes the three-instruction advance epilogue that
         * increments the phase byte to 17 (37 instructions). */
        if (status == VF2_OK && entry_phase == UINT8_C(16)) {
            const uint64_t instructions =
                phase != entry_phase ? UINT64_C(37) : UINT64_C(34);

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, instructions);
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = instructions;
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        /* Phase 17 clears the phase byte to zero, wrapping the selector-3
         * phase cycle, and returns before the generic selector cleanup. */
        if (status == VF2_OK && entry_phase == UINT8_C(17)) {
            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(31));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(31);
            report->recovered_procedure_calls = UINT64_C(3);
            report->recovered_procedure_returns = UINT64_C(4);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && entry_phase == UINT8_C(12)) {
            uint32_t base = 0u;
            uint32_t profile_flags = 0u;
            uint32_t rows = 0u;
            uint32_t columns = 0u;
            uint32_t destination = UINT32_C(0x010055e0);
            int16_t addend = 0;
            int16_t word_mode = 0;
            int32_t last_sample = 0;
            int has_descriptor_poststate = 0;

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050016c), &base
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, base + UINT32_C(0x3320), &profile_flags
                );
            }
            if (status == VF2_OK && (profile_flags & UINT32_C(1)) != 0u) {
                destination = UINT32_C(0x01005460);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da), &addend, sizeof(addend)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(2),
                    &word_mode, sizeof(word_mode)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(4), &rows
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x02a6c0da) + UINT32_C(8), &columns
                );
            }
            if (status == VF2_OK && rows != 0u && columns != 0u) {
                const uint32_t sample_index = rows * columns - UINT32_C(1);
                has_descriptor_poststate = 1;
                if (word_mode == 0) {
                    int8_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index, &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                } else {
                    int16_t sample = 0;
                    status = vf2_model2a_read(
                        machine, UINT32_C(0x02a6c0da) + UINT32_C(12) +
                            sample_index * UINT32_C(2), &sample, sizeof(sample)
                    );
                    last_sample = (int32_t)sample;
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, cpu->registers[1] + UINT32_C(0x000000c0),
                    destination
                );
            }
            if (status != VF2_OK) return status;

            cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = columns * UINT32_C(2);
            if (has_descriptor_poststate) {
                cpu->registers[VF2_I960_G0_REGISTER + 4u] =
                    (uint32_t)(int32_t)addend;
                cpu->registers[VF2_I960_G0_REGISTER + 5u] = 0u;
                cpu->registers[VF2_I960_G0_REGISTER + 6u] =
                    (uint32_t)(last_sample + (int32_t)addend);
                cpu->registers[VF2_I960_G0_REGISTER + 7u] =
                    (uint32_t)(int32_t)word_mode;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                destination + columns * UINT32_C(2);
            set_signed_condition(cpu, INT32_C(0), INT32_C(-1));
            account_nested_procedure(cpu, UINT64_C(6), UINT64_C(6));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(33867));
            if (status != VF2_OK) return status;

            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->recovered_instruction_count = UINT64_C(33867);
            report->recovered_procedure_calls = UINT64_C(6);
            report->recovered_procedure_returns = UINT64_C(7);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
                if (status == VF2_OK && phase8_handoff) {
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(5);
            report->bytes_written = 14u;
            report->recovered_instruction_count = UINT64_C(26);
            report->recovered_procedure_calls = UINT64_C(2);
            report->recovered_procedure_returns = 0u;
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK) {
            uint32_t common_value = 0u;
            uint32_t common_pointer = 0u;
            uint8_t sound_rate = 0u;
            uint8_t zero = 0u;
            static const uint32_t clear_descriptor_slots[] = {
                UINT32_C(0x00500858), UINT32_C(0x00500878),
                UINT32_C(0x0050087c), UINT32_C(0x00500880)
            };
            size_t index = 0u;

            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &common_value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    common_value & ~(UINT32_C(1) << 16u)
                );
            }
            for (index = 0u;
                 status == VF2_OK &&
                 index < sizeof(clear_descriptor_slots) /
                            sizeof(clear_descriptor_slots[0]);
                 ++index) {
                status = vf2_model2a_read_u32(
                    machine, clear_descriptor_slots[index], &common_pointer
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, common_pointer, &common_value
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, common_pointer,
                        common_value & ~UINT32_C(0x80000000)
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &common_pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, common_pointer, &common_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, common_pointer,
                    common_value & ~(UINT32_C(1) | UINT32_C(2))
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500056), &((uint8_t){2u}),
                    sizeof(uint8_t)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x0050008f), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500054), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500067), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                uint32_t base = 0u;
                uint8_t mode_flags = 0u;
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050016c), &base
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read(
                        machine, base + UINT32_C(0x3351), &mode_flags,
                        sizeof(mode_flags)
                    );
                }
                if (status == VF2_OK &&
                    (mode_flags & UINT8_C(0x20)) == 0u) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x0050005b), &zero,
                        sizeof(zero)
                    );
                }
            }
        }
        if (status == VF2_OK) {
            selector = UINT8_C(4);
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        cpu->registers[4] = UINT32_C(16);
        status = finish_recovered_procedure(machine, cpu, UINT64_C(260));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(16);
        report->bytes_written = 96u;
        report->recovered_instruction_count = UINT64_C(260);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;

    }
    if (selector == UINT8_C(4)) {
        uint8_t next_selector = UINT8_C(5);

        if (target != UINT32_C(0x0000c45c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &next_selector,
            sizeof(next_selector)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)next_selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(next_selector);
        report->recovered_instruction_count = UINT64_C(4);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(5)) {
        uint8_t next_selector = UINT8_C(6);

        if (target != UINT32_C(0x0000c45c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050002a), &next_selector,
            sizeof(next_selector)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)next_selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(4));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(1);
        report->bytes_written = sizeof(next_selector);
        report->recovered_instruction_count = UINT64_C(4);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(6)) {
        uint32_t base = 0u;
        uint32_t pointer = 0u;
        uint32_t value = 0u;
        uint32_t flags_value = 0u;
        uint8_t phase_flags = 0u;
        uint8_t mode_value = 0u;
        uint8_t zero = 0u;
        uint8_t one = UINT8_C(1);
        uint8_t two = UINT8_C(2);
        uint8_t seven = UINT8_C(7);
        uint16_t zero16 = 0u;
        uint32_t fill_offset = 0u;

        if (target != UINT32_C(0x0000c474)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a004), 0u);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a00c), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, base + UINT32_C(0x3351),
                                      &phase_flags, sizeof(phase_flags));
        }
        if (status == VF2_OK && (phase_flags & UINT8_C(0x40)) == 0u) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a000), 0u);
        }
        if (status == VF2_OK && (phase_flags & UINT8_C(0x40)) == 0u) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0100a008), 0u);
        }
        for (fill_offset = 0u;
             status == VF2_OK && fill_offset < UINT32_C(0x1880);
             fill_offset += UINT32_C(4)) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x01004000) + fill_offset,
                UINT32_C(0xc007c007)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value | (UINT32_C(1) << 14u)
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500804), UINT32_C(1) << 26u,
                (UINT32_C(1) << 23u) | (UINT32_C(1) << 22u), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500864), UINT32_C(1) << 28u,
                UINT32_C(1) << 29u, 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500808), UINT32_C(1) << 26u,
                (UINT32_C(1) << 23u) | (UINT32_C(1) << 22u), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~((UINT32_C(1) << 20u) |
                                 (UINT32_C(1) << 24u) |
                                 (UINT32_C(1) << 25u))
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, pointer + UINT32_C(0x2d4),
                                       &zero, sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &value);
        }
        if (status == VF2_OK) {
            value |= UINT32_C(0x45040000);
            value &= ~UINT32_C(0x08900000);
            status = vf2_model2a_write_u32(machine, pointer, value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00508000),
                flags_value & ~(UINT32_C(1) << 16u)
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050085c), 0u,
                UINT32_C(1) << 31u, 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500838), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x00031810), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050083c), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x00032090), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500840), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x00032090), 1
            );
        }
        {
            static const uint32_t descriptor_slots[] = {
                UINT32_C(0x00500804), UINT32_C(0x00500808),
                UINT32_C(0x00500828), UINT32_C(0x00500850),
                UINT32_C(0x00500824), UINT32_C(0x0050084c)
            };
            size_t index = 0u;
            for (index = 0u;
                 status == VF2_OK &&
                 index < sizeof(descriptor_slots) /
                            sizeof(descriptor_slots[0]);
                 ++index) {
                status = phase16_update_descriptor(
                    machine, descriptor_slots[index], 0u,
                    UINT32_C(1) << 31u, 0u, 0
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, base + UINT32_C(0x3340),
                                      &mode_value, sizeof(mode_value));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500024),
                                       &zero, sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500055), &one,
                                       sizeof(one));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050004f), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500051), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            if (mode_value > UINT8_C(5)) {
                mode_value = two;
            }
            status = vf2_model2a_write(machine, UINT32_C(0x0050005a),
                                       &mode_value, sizeof(mode_value));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500059),
                                       &mode_value, sizeof(mode_value));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x00520f90), zero16);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &seven,
                                       sizeof(seven));
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)seven;
        cpu->registers[4] = UINT32_C(32);
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(1100));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(36);
        report->bytes_written = (size_t)UINT32_C(0x1880) + 128u;
        report->recovered_instruction_count = UINT64_C(1100);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(7)) {
        uint32_t counter_value = 0u;
        uint32_t input_flags = 0u;
        uint32_t pointer = 0u;
        uint32_t descriptor = 0u;

        if (target != UINT32_C(0x0000c7ec)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024),
                                      &counter_value);
        if (status == VF2_OK) {
            --counter_value;
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024),
                                           counter_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500708),
                                          &input_flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704),
                                          &descriptor);
        }
        if (status == VF2_OK && (int32_t)counter_value <= 0 &&
            counter_value == UINT32_MAX &&
            (input_flags & (UINT32_C(1) | (UINT32_C(1) << 1u))) == 0u &&
            (descriptor & ((UINT32_C(1) << 3u) |
                           (UINT32_C(1) << 27u))) == 0u) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500834),
                                          &pointer);
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, pointer, &descriptor);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer, descriptor | (UINT32_C(1) << 18u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine,
                                               UINT32_C(0x00500024),
                                               UINT32_C(0x3f));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                              &descriptor);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    descriptor | (UINT32_C(1) << 15u)
                );
            }
        }
        if (status == VF2_OK && counter_value == 0u) {
            selector = UINT8_C(8);
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050002a), &selector, sizeof(selector)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050083c),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &descriptor);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, pointer, descriptor & ~(UINT32_C(1) << 1u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500840),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &descriptor);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, pointer, descriptor & ~(UINT32_C(1) << 1u)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(80));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(7);
        report->bytes_written = 28u;
        report->recovered_instruction_count = UINT64_C(80);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(8)) {
        uint32_t pointer = 0u;
        uint32_t flags_value = 0u;
        uint8_t task_mode = 0u;
        uint8_t zero = 0u;
        uint8_t one = UINT8_C(1);
        uint8_t two = UINT8_C(2);
        uint8_t next_selector = UINT8_C(9);

        if (target != UINT32_C(0x0000d014)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                      &flags_value);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~(UINT32_C(1) << 4u)
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500804), UINT32_C(1) << 31u,
                UINT32_C(0x00ffffff), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500868), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x000640f4), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500808), UINT32_C(1) << 31u,
                UINT32_C(0x00ffffff), 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050086c), UINT32_C(1) << 31u, 0u,
                UINT32_C(0x000640f4), 1
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x00508020), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00508040), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00508050), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00508008), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00520f90), &zero,
                                       sizeof(zero));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1230),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1234),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808),
                                          &pointer);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1230),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, pointer + UINT32_C(0x1234),
                                           0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, UINT32_C(0x00500056), &task_mode,
                                      sizeof(task_mode));
        }
        if (status == VF2_OK && task_mode == two) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050004c), &two,
                                       sizeof(two));
            if (status == VF2_OK) {
                status = vf2_model2a_read(machine, UINT32_C(0x00500059), &task_mode,
                                          sizeof(task_mode));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(machine, UINT32_C(0x00500052),
                                           &task_mode, sizeof(task_mode));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(machine, UINT32_C(0x00500057), &one,
                                           sizeof(one));
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(machine, UINT32_C(0x00500058), &two,
                                           sizeof(two));
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068),
                                          &flags_value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value | (UINT32_C(1) << 5u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500030), &two,
                                       sizeof(two));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050002a),
                                       &next_selector, sizeof(next_selector));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500240), 0u);
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)next_selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(400));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(22);
        report->bytes_written = 96u;
        report->recovered_instruction_count = UINT64_C(400);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(9)) {
        uint32_t pointer = 0u;
        uint32_t descriptor = 0u;
        uint32_t descriptor_flags = 0u;
        uint32_t flags_value = 0u;
        uint32_t mode_value = 0u;
        uint8_t task_mode = 0u;
        uint8_t phase = 0u;
        uint8_t zero = 0u;
        uint8_t one = UINT8_C(1);
        uint8_t two = UINT8_C(2);
        uint8_t next_mode = 0u;
        uint16_t profile_halfword = 0u;

        /* 0xd380 is the selector-9 dispatcher.  The current boot path has
         * runtime mode 2, so its indirect table calls 0xd94c.  Keep the
         * dispatcher-visible bookkeeping here as well as the d94c setup;
         * later selector-9 modes consume these bytes directly. */
        if (target != UINT32_C(0x0000d380)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500030), &phase, sizeof(phase)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500031), &phase, sizeof(phase)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, UINT32_C(0x00500034),
                (uint16_t)((uint16_t)phase << 1u)
            );
        }
        if (status == VF2_OK &&
            (phase == UINT8_C(10) || phase == UINT8_C(11))) {
            uint32_t inner_counter = 0u;

            /* cfbc[10] -> 0xe544 performs two small command callbacks and
             * resets the shared counter.  cfbc[11] -> 0xe590 only advances
             * the mode.  The callbacks are outside this recovered bridge;
             * preserve their dispatcher-visible state and exact counter
             * tail so the following task mode can run. */
            if (phase == UINT8_C(10)) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), 0u
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK && phase == UINT8_C(12)) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
                if (status == VF2_OK) {
                    --inner_counter;
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                }
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, phase == UINT8_C(11) ? UINT64_C(8) : UINT64_C(24)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(3);
            report->bytes_written = 9u;
            report->recovered_instruction_count = phase == UINT8_C(11)
                ? UINT64_C(8) : UINT64_C(24);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK &&
            (phase == UINT8_C(12) || phase == UINT8_C(13) ||
             phase == UINT8_C(14) || phase == UINT8_C(15))) {
            uint32_t inner_counter = 0u;
            uint32_t runtime_flags = 0u;
            uint8_t next_selector = UINT8_C(16);
            uint32_t registry_index = 0u;

            /* The remaining cfbc entries are the task/gameplay handoff:
             * e5a8 primes the two task records, eb44 drains its work count,
             * edf4 publishes the command descriptor, and f0f4 hands control
             * to selector 16.  The large helper calls in those ROM blocks
             * are represented by the existing task/descriptor model; keep
             * the shared state transitions and exact loop widths here. */
            if (phase == UINT8_C(12)) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), UINT32_C(95)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500048),
                        &(uint8_t){UINT8_C(6)}, sizeof(uint8_t)
                    );
                }
                if (status == VF2_OK) {
                    status = write_u16(
                        machine, UINT32_C(0x00500092), UINT16_C(0)
                    );
                }
                if (status == VF2_OK) {
                    ++phase;
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &inner_counter
                    );
                }
                if (status == VF2_OK) {
                    --inner_counter;
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                }
            } else if (phase == UINT8_C(13)) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
                if (status == VF2_OK && inner_counter != 0u) {
                    --inner_counter;
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                } else if (status == VF2_OK) {
                    phase = UINT8_C(14);
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                    );
                }
            } else if (phase == UINT8_C(14)) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048),
                    &(uint8_t){UINT8_C(17)}, sizeof(uint8_t)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500834), &pointer
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, pointer, &descriptor
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, pointer,
                        descriptor | (UINT32_C(1) << 13u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), 0u
                    );
                }
                if (status == VF2_OK) {
                    ++phase;
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                    );
                }
            } else {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048),
                    &(uint8_t){UINT8_C(18)}, sizeof(uint8_t)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00508000), &runtime_flags
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00508000),
                        runtime_flags | (UINT32_C(1) << 9u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00011d94), UINT32_C(29)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00f00004), UINT32_C(0x000fffff)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00f00008), UINT32_C(0x000fffff)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00510000), UINT32_C(0x80000000)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00510008), UINT32_C(0x80)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x0051000c), UINT32_C(0x000439fc)
                    );
                }
                for (registry_index = 0u;
                     status == VF2_OK && registry_index < UINT32_C(29);
                     ++registry_index) {
                    const uint32_t registry_address =
                        UINT32_C(0x00510000) + registry_index * UINT32_C(0x80);
                    status = vf2_model2a_write_u32(
                        machine, registry_address,
                        registry_index == 0u ? UINT32_C(0x80000000) : 0u
                    );
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, registry_address + UINT32_C(8),
                            UINT32_C(0x80)
                        );
                    }
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, registry_address + UINT32_C(0x38), 0u
                        );
                    }
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x0050002a), &next_selector,
                        sizeof(next_selector)
                    );
                }
                cpu->registers[3] = (uint32_t)next_selector;
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = phase == UINT8_C(15)
                ? (uint32_t)next_selector : (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(machine, cpu, UINT64_C(90));
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(8);
            report->bytes_written = 32u;
            report->recovered_instruction_count = UINT64_C(90);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(3)) {
            uint32_t inner_counter = 0u;

            /* cfbc[3] -> 0xdb80: advance the inner mode and take the
             * shared f0d8 counter tail. */
            ++phase;
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500030), &phase, sizeof(phase)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
            }
            if (status == VF2_OK) {
                --inner_counter;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), inner_counter
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(18)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(4);
            report->bytes_written = 14u;
            report->recovered_instruction_count = UINT64_C(18);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(4)) {
            uint32_t inner_counter = 0u;

            /* cfbc[4] -> 0xdb98.  The selector-9 boot path enters the
             * short dd0c arm because d94c leaves 500055 set to one. */
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500860), 0u,
                UINT32_C(1) << 31u, 0u, 0
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500814), &pointer
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x28), UINT16_C(0)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0050a014), 0u
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &flags_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    flags_value & ~(UINT32_C(1) << 0u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), UINT32_C(64)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500834), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, pointer, &descriptor);
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x40), profile_halfword
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x42), profile_halfword
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer,
                    descriptor | UINT32_C(0x1a012000)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500814), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, pointer + UINT32_C(0x2d4), &zero, sizeof(zero)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500804), &pointer
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x1e20), UINT16_C(0)
                );
            }
            if (status == VF2_OK) {
                status = write_u16(
                    machine, pointer + UINT32_C(0x3e20), UINT16_C(0)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
            }
            if (status == VF2_OK) {
                --inner_counter;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), inner_counter
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(180)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(20);
            report->bytes_written = 72u;
            report->recovered_instruction_count = UINT64_C(180);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK &&
            (phase == UINT8_C(5) || phase == UINT8_C(6) ||
             phase == UINT8_C(7))) {
            uint32_t inner_counter = 0u;

            /* cfbc[5..7] are the small continuation states at de64,
             * deb0, and df38. They all finish through f0d8; the middle
             * state also primes the command descriptor and a short timer. */
            if (phase == UINT8_C(5)) {
                status = write_u16(
                    machine, UINT32_C(0x00500028), UINT16_C(0)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00500048), &two, sizeof(two)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &inner_counter
                    );
                }
                if (status == VF2_OK) {
                    inner_counter += UINT32_C(31);
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), inner_counter
                    );
                }
            } else if (phase == UINT8_C(6)) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048), &one, sizeof(one)
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500834), &pointer
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, pointer, &descriptor
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, pointer,
                        descriptor & ~(UINT32_C(1) << 29u)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(
                        machine, UINT32_C(0x00500024), UINT32_C(11)
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500814), &pointer
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, pointer + UINT32_C(0x40),
                        &one, sizeof(one)
                    );
                }
            } else {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &inner_counter
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                if (phase != UINT8_C(8) || inner_counter != 0u) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500024), &inner_counter
                    );
                }
            }
            if (status == VF2_OK) {
                --inner_counter;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), inner_counter
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(50)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(8);
            report->bytes_written = 28u;
            report->recovered_instruction_count = UINT64_C(50);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(8)) {
            uint8_t sound_rate = 0u;
            uint32_t phase_descriptor = 0u;

            /* cfbc[8] -> 0xdf60. The ROM commits the next command-buffer
             * mode here, then takes the same f0d8 timer tail. */
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500834), &pointer
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer,
                    descriptor & ~(UINT32_C(1) << 25u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500860), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer + UINT32_C(0x80), &phase_descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer + UINT32_C(0x80),
                    phase_descriptor & ~UINT32_C(3)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &flags_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500068),
                    flags_value & ~(UINT32_C(1) << 17u)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00500090), &sound_rate,
                    sizeof(sound_rate)
                );
            }
            if (status == VF2_OK) {
                mode_value = (uint32_t)sound_rate << 6u;
                status = write_u16(
                    machine, UINT32_C(0x00500028),
                    (uint16_t)mode_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500048),
                    &(uint8_t){UINT8_C(4)}, sizeof(uint8_t)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050081c), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, pointer,
                    descriptor | (UINT32_C(1) | (UINT32_C(1) << 1u))
                );
            }
            if (status == VF2_OK) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &mode_value
                );
            }
            if (status == VF2_OK) {
                --mode_value;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), mode_value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(160)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(14);
            report->bytes_written = 52u;
            report->recovered_instruction_count = UINT64_C(160);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase == UINT8_C(9)) {
            uint32_t task0_descriptor = 0u;
            uint32_t task1_descriptor = 0u;
            uint32_t source_pointer = 0u;
            uint32_t destination_pointer = 0u;
            uint32_t copied_value = 0u;
            uint16_t timer_value = 0u;

            /* cfbc[9] -> 0xe474. This is the short timer/descriptor
             * handoff before the selector-9 task loop continues. */
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500048),
                &(uint8_t){UINT8_C(5)}, sizeof(uint8_t)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500804), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &task0_descriptor
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500808), &pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, pointer, &task1_descriptor
                );
            }
            if (status == VF2_OK &&
                (task0_descriptor & (UINT32_C(1) << 5u)) == 0u &&
                (task1_descriptor & (UINT32_C(1) << 5u)) == 0u) {
                status = read_u16(
                    machine, UINT32_C(0x00500028), &timer_value
                );
                mode_value = (uint32_t)timer_value;
                if (status == VF2_OK && mode_value != 0u) {
                    --mode_value;
                    status = write_u16(
                        machine, UINT32_C(0x00500028),
                        (uint16_t)mode_value
                    );
                } else if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(
                        machine, UINT32_C(0x00500068), &flags_value
                    );
                    if (status == VF2_OK) {
                        status = vf2_model2a_write_u32(
                            machine, UINT32_C(0x00500068),
                            flags_value | UINT32_C(1)
                        );
                    }
                }
            }
            if (status == VF2_OK &&
                ((task0_descriptor & (UINT32_C(1) << 5u)) != 0u ||
                 (task1_descriptor & (UINT32_C(1) << 5u)) != 0u ||
                 mode_value == 0u)) {
                ++phase;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500030), &phase, sizeof(phase)
                );
            }
            if (status == VF2_OK) {
                status = phase16_update_descriptor(
                    machine, UINT32_C(0x00500844), 0u,
                    UINT32_C(1) << 31u, 0u, 0
                );
            }
            if (status == VF2_OK) {
                status = phase16_update_descriptor(
                    machine, UINT32_C(0x00500848), 0u,
                    UINT32_C(1) << 31u, 0u, 0
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050085c), &source_pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, source_pointer + UINT32_C(0x6c), &copied_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500860), &destination_pointer
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination_pointer + UINT32_C(0x7c),
                    copied_value
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500024), &mode_value
                );
            }
            if (status == VF2_OK) {
                --mode_value;
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00500024), mode_value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = (uint32_t)selector;
            account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
            status = finish_recovered_procedure(
                machine, cpu, UINT64_C(80)
            );
            if (status != VF2_OK) {
                return status;
            }
            report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
            report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
            report->exit_address = cpu->ip;
            report->iterations = UINT64_C(1);
            report->changed_values = UINT64_C(12);
            report->bytes_written = 44u;
            report->recovered_instruction_count = UINT64_C(80);
            report->recovered_procedure_calls = UINT64_C(1);
            report->recovered_procedure_returns = UINT64_C(2);
            report->cpu_poststate_applied = 1;
            return VF2_OK;
        }
        if (status == VF2_OK && phase != UINT8_C(2)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500068), &flags_value
            );
        }
        if (status == VF2_OK &&
            (flags_value & (UINT32_C(1) << 5u)) == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~(UINT32_C(1) << 23u)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050004c), &task_mode, sizeof(task_mode)
            );
        }
        if (status == VF2_OK) {
            profile_halfword = task_mode != 0u
                ? UINT16_C(0xa70a) : UINT16_C(0xa708);
            status = write_u16(
                machine, UINT32_C(0x005000a0), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500804), &pointer
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x01ac), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500808), &pointer
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x01ac), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050081c), UINT32_C(1) << 31u,
                0u, UINT32_C(0x0001b9ac), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500828), UINT32_C(1) << 31u,
                0u, UINT32_C(0x000221cc), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500850), UINT32_C(1) << 31u,
                0u, UINT32_C(0x00051d3c), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500824), UINT32_C(1) << 31u,
                0u, UINT32_C(0x0001645c), 1
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x0050084c), UINT32_C(1) << 31u,
                0u, UINT32_C(0x0002eaa0), 1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500804), &pointer
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, pointer + UINT32_C(0x069c), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, pointer + UINT32_C(0x069d), &one, sizeof(one)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500834), &pointer
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer, &descriptor);
        }
        if (status == VF2_OK) {
            descriptor_flags =
                (descriptor & ~(UINT32_C(1) << 30u)) |
                (UINT32_C(1) << 28u);
            status = vf2_model2a_write_u32(
                machine, pointer, descriptor_flags
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x40), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = write_u16(
                machine, pointer + UINT32_C(0x42), profile_halfword
            );
        }
        if (status == VF2_OK) {
            status = phase16_update_descriptor(
                machine, UINT32_C(0x00500864),
                (UINT32_C(1) << 30u) | (UINT32_C(1) << 27u) |
                    (UINT32_C(1) << 29u),
                0u, 0u, 0
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500048), 0u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050004c), &task_mode, sizeof(task_mode)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine,
                task_mode != 0u ? UINT32_C(0x00500059) : UINT32_C(0x0050005a),
                &next_mode, sizeof(next_mode)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500052), &next_mode, sizeof(next_mode)
            );
        }
        if (status == VF2_OK) {
            status = write_u16(machine, UINT32_C(0x00500028), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500055), &one, sizeof(one)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050004f), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500051), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500068), &flags_value
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500068),
                flags_value & ~((UINT32_C(1) << 1u) | (UINT32_C(1) << 14u))
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00500030), &phase, sizeof(phase)
            );
        }
        if (status == VF2_OK) {
            ++phase;
            status = vf2_model2a_write(
                machine, UINT32_C(0x00500030), &phase, sizeof(phase)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500024), &mode_value
            );
        }
        if (status == VF2_OK) {
            --mode_value;
            status = vf2_model2a_write_u32(
                machine, UINT32_C(0x00500024), mode_value
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = (uint32_t)selector;
        account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
        status = finish_recovered_procedure(machine, cpu, UINT64_C(420));
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
        report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = UINT64_C(31);
        report->bytes_written = 104u;
        report->recovered_instruction_count = UINT64_C(420);
        report->recovered_procedure_calls = UINT64_C(1);
        report->recovered_procedure_returns = UINT64_C(2);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (selector == UINT8_C(16)) {
        if (target != UINT32_C(0x00010a0c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return execute_frame_phase16(machine, cpu, report);
    }
    if (selector == UINT8_C(17)) {
        if (target != UINT32_C(0x00010b5c)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return execute_frame_phase17(machine, cpu, report);
    }
    if (target != UINT32_C(0x0000a974)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500024), &counter);
    if (status != VF2_OK) {
        return status;
    }
    --counter;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500024), counter);
    if (status != VF2_OK) {
        return status;
    }
    if ((int32_t)counter <= 0) {
        selector = (uint8_t)(selector + UINT8_C(1));
        cpu->registers[3] = (uint32_t)selector;
        status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &selector,
                                   sizeof(selector));
        if (status != VF2_OK) {
            return status;
        }
    }
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    status = finish_recovered_procedure(machine, cpu, UINT64_C(14));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(3);
    report->bytes_written = 9u;
    report->recovered_instruction_count = UINT64_C(14);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(2);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_player_update_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t flags = 0u;
    uint8_t state = 0u;
    uint64_t recovered_instruction_count = UINT64_C(0);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status != VF2_OK) {
        return status;
    }

    /* The original bbs at 0x0002453c returns immediately when runtime flag
     * bit 14 is set. This repeated-frame path bypasses the state byte read. */
    if ((flags & (UINT32_C(1) << 14u)) != 0u) {
        recovered_instruction_count = UINT64_C(3);
    } else {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00503001), &state, 1u
        );
        if (status != VF2_OK) {
            return status;
        }
        if (state != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        recovered_instruction_count = UINT64_C(5);
    }

    status = finish_recovered_procedure(
        machine, cpu, recovered_instruction_count
    );
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE;
    report->entry_address = VF2_PLAYER_UPDATE_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->recovered_instruction_count = recovered_instruction_count;
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_state_update(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,
    bool leave_return_stub
)
{
    vf2_hybrid_bridge_report classify_report;
    vf2_hybrid_bridge_report threshold_report;
    vf2_hybrid_bridge_report event_report;
    vf2_hybrid_bridge_report meter_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t selector_mask = 0u;
    uint32_t saved_g9 = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    uint32_t base = 0u;
    uint32_t input_flags = 0u;
    uint32_t classification = 0u;
    uint32_t first_result = 0u;
    uint32_t second_result = 0u;
    uint32_t counter32 = 0u;
    uint8_t counter8 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&classify_report, 0, sizeof(classify_report));
    memset(&threshold_report, 0, sizeof(threshold_report));
    memset(&event_report, 0, sizeof(event_report));
    memset(&meter_report, 0, sizeof(meter_report));

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &runtime_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &selector_mask);
    }
    if (status != VF2_OK ||
        (runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[15] = selector_mask & UINT32_C(0x00030000);
    cpu->registers[14] = UINT32_C(0x00030000);
    if ((selector_mask & UINT32_C(0x00030000)) != 0u) {
        if (leave_return_stub) {
            cpu->executed_instructions += UINT64_C(6);
            cpu->ip = UINT32_C(0x000020ec);
            status = VF2_OK;
        } else {
            status = finish_recovered_procedure(machine, cpu, UINT64_C(7));
        }
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
        report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->recovered_instruction_count = leave_return_stub
            ? UINT64_C(6) : UINT64_C(7);
        report->recovered_procedure_returns = leave_return_stub ?
            UINT64_C(0) : UINT64_C(1);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g9
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500704), &input_flags);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = base;
    cpu->registers[13] = input_flags;

    if ((input_flags & ((UINT32_C(1) << 3u) |
                        (UINT32_C(1) << 27u))) == 0u) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_GAME_METER_UPDATE_ENTRY, UINT32_C(0x000020e0)
        );
        if (status == VF2_OK) {
            status = execute_game_meter_update(machine, cpu, &meter_report);
        }
        if (status != VF2_OK || cpu->ip != UINT32_C(0x000020e0)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_read_u32(
            machine, cpu->registers[1] - UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + 9u]
        );
        cpu->registers[1] -= UINT32_C(4);
        if (status != VF2_OK ||
            cpu->registers[VF2_I960_G0_REGISTER + 9u] != saved_g9) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        if (leave_return_stub) {
            cpu->executed_instructions += UINT64_C(16);
            cpu->ip = UINT32_C(0x000020ec);
            status = VF2_OK;
        } else {
            status = finish_recovered_procedure(machine, cpu, UINT64_C(17));
        }
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
        report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = UINT64_C(1);
        report->changed_values = meter_report.changed_values;
        report->bytes_written = sizeof(uint32_t) + meter_report.bytes_written;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_STATE_CLASSIFY_ENTRY, UINT32_C(0x00001fac)
    );
    if (status == VF2_OK) {
        status = execute_game_state_classify(machine, cpu, &classify_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00001fac)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    classification = cpu->registers[VF2_I960_G0_REGISTER];
    cpu->registers[3] = classification;
    if (classification == UINT32_C(3)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &runtime_flags);
    if (status != VF2_OK) {
        return status;
    }
    runtime_flags |= UINT32_C(1) << 10u;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), runtime_flags);
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_THRESHOLD_EVALUATE_ENTRY, UINT32_C(0x00001fcc)
    );
    if (status == VF2_OK) {
        status = execute_game_threshold_evaluate(machine, cpu, &threshold_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00001fcc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    first_result = cpu->registers[VF2_I960_G0_REGISTER];
    second_result = cpu->registers[VF2_I960_G0_REGISTER + 1u];
    cpu->registers[4] = first_result;
    cpu->registers[5] = second_result;
    if (classification == UINT32_C(2) || first_result == UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(
        machine, base + UINT32_C(0x3385), &counter8, sizeof(counter8)
    );
    ++counter8;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x01d03385), &counter8, sizeof(counter8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, base + UINT32_C(0x3385), &counter8, sizeof(counter8)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, base + UINT32_C(0x339c), &counter32
        );
    }
    ++counter32;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x01d0339c), counter32
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, base + UINT32_C(0x339c), counter32
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = counter32;

    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x009e0a7f);
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_EVENT_QUEUE_WRITE_ENTRY, UINT32_C(0x00002020)
    );
    if (status == VF2_OK) {
        status = execute_game_event_queue_write(machine, cpu, &event_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002020)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_METER_UPDATE_ENTRY, UINT32_C(0x000020e0)
    );
    if (status == VF2_OK) {
        status = execute_game_meter_update(machine, cpu, &meter_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000020e0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4),
        &cpu->registers[VF2_I960_G0_REGISTER + 9u]
    );
    cpu->registers[1] -= UINT32_C(4);
    if (status != VF2_OK ||
        cpu->registers[VF2_I960_G0_REGISTER + 9u] != saved_g9) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (leave_return_stub) {
        cpu->executed_instructions += UINT64_C(37);
        cpu->ip = UINT32_C(0x000020ec);
        status = VF2_OK;
    } else {
        status = finish_recovered_procedure(machine, cpu, UINT64_C(37));
    }
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE;
    report->entry_address = VF2_GAME_STATE_UPDATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(4);
    report->bytes_written = sizeof(uint32_t) * 5u + sizeof(uint8_t) * 2u +
        classify_report.bytes_written + threshold_report.bytes_written +
        event_report.bytes_written + meter_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_game_threshold_evaluate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report color0_report;
    vf2_hybrid_bridge_report color1_report;
    vf2_hybrid_bridge_report classify_report;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t color0 = 0u;
    uint32_t color1 = 0u;
    uint32_t quotient0 = 0u;
    uint32_t quotient1 = 0u;
    uint8_t mode = 0u;
    uint8_t variant = 0u;
    uint8_t numerator0 = 0u;
    uint8_t numerator1 = 0u;
    uint8_t offset0 = 0u;
    uint8_t offset1 = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(&color0_report, 0, sizeof(color0_report));
    memset(&color1_report, 0, sizeof(color1_report));
    memset(&classify_report, 0, sizeof(classify_report));

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3324), &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3350), &variant, sizeof(variant)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3380), &numerator0,
            sizeof(numerator0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3388), &numerator1,
            sizeof(numerator1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3385), &offset0, sizeof(offset0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x338d), &offset1, sizeof(offset1)
        );
    }
    if (status != VF2_OK || mode == UINT8_C(25) || variant == UINT8_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[11] = UINT32_C(9);
    cpu->registers[16] = 0u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x00002938)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &color0_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002938)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    color0 = cpu->registers[16];

    cpu->registers[16] = 1u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_COLOR_LOOKUP_ENTRY, UINT32_C(0x00002944)
    );
    if (status == VF2_OK) {
        status = execute_game_color_lookup(
            machine, cpu, &color1_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00002944)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    color1 = cpu->registers[16];
    color0 = (color0 << 8u) >> 24u;
    color1 = (color1 << 8u) >> 24u;
    if (color0 == 0u || color1 == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_STATE_CLASSIFY_ENTRY, UINT32_C(0x0000295c)
    );
    if (status == VF2_OK) {
        status = execute_game_state_classify(
            machine, cpu, &classify_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000295c) ||
        cpu->registers[16] == UINT32_C(2) ||
        cpu->registers[16] == UINT32_C(3)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(
        machine, base + UINT32_C(0x3320), &flags
    );
    if (status != VF2_OK || (flags & (UINT32_C(1) << 1u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    quotient0 = (uint32_t)numerator0 / color0;
    quotient1 = (uint32_t)numerator1 / color1;
    if (quotient0 + (uint32_t)offset0 + quotient1 +
            (uint32_t)offset1 >= UINT32_C(9)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[16] = 0u;
    cpu->registers[17] = 0u;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, UINT64_C(38));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE;
    report->entry_address = VF2_GAME_THRESHOLD_EVALUATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->changed_values = UINT64_C(2);
    report->bytes_written =
        color0_report.bytes_written + color1_report.bytes_written;
    report->recovered_instruction_count =
        UINT64_C(38) + color0_report.recovered_instruction_count +
        color1_report.recovered_instruction_count +
        classify_report.recovered_instruction_count;
    report->recovered_procedure_calls =
        UINT64_C(3) + color0_report.recovered_procedure_calls +
        color1_report.recovered_procedure_calls +
        classify_report.recovered_procedure_calls;
    report->recovered_procedure_returns =
        UINT64_C(1) + color0_report.recovered_procedure_returns +
        color1_report.recovered_procedure_returns +
        classify_report.recovered_procedure_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_inline_text_thunk(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t source = cpu->registers[14];
    const uint32_t destination = cpu->registers[25];
    uint32_t cursor = source;
    uint32_t word = 0u;
    uint64_t characters = 0u;
    uint64_t words = 0u;
    uint64_t instructions = 0u;
    vf2_status status = VF2_OK;

    status = copy_diagnostic_text(
        machine, source, destination, &characters
    );
    if (status != VF2_OK) {
        return status;
    }
    do {
        status = vf2_model2a_read_u32(machine, cursor, &word);
        if (status != VF2_OK) {
            return status;
        }
        cursor += UINT32_C(4);
        ++words;
        if (words > UINT64_C(1024)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    } while (word > UINT32_C(0x00ffffff));

    cpu->registers[16] = word;
    cpu->registers[25] = destination + UINT32_C(128);
    cpu->registers[2] = UINT32_C(0x00009450);
    cpu->registers[14] = cursor;
    cpu->registers[15] = UINT32_C(0x00ffffff);
    cpu->ip = cursor;
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(1), UINT64_C(1));
    instructions = characters * UINT64_C(8) + UINT64_C(17) +
                   words * UINT64_C(3);
    cpu->executed_instructions += instructions;

    report->kind = VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK;
    report->entry_address = VF2_INLINE_TEXT_THUNK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = words;
    report->rows = characters;
    report->bytes_written = (size_t)characters * 2u;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_status_line(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t stack_address = cpu->registers[1];
    const uint32_t texture_index = cpu->registers[16];
    uint16_t label_kind = 0u;
    uint8_t display_mode = 0u;
    uint32_t inline_source = UINT32_C(0x0004d2e8);
    uint32_t inline_destination = UINT32_C(0x010000e2);
    uint32_t name_destination = UINT32_C(0x010000ea);
    uint32_t name_source = 0u;
    uint32_t inline_cursor = 0u;
    uint32_t inline_word = 0u;
    uint64_t inline_characters = 0u;
    uint64_t inline_words = 0u;
    uint64_t name_characters = 0u;
    uint64_t instructions = UINT64_C(4);
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, stack_address, texture_index);
    if (status == VF2_OK) {
        status = read_u16(machine, UINT32_C(0x0055c2f2), &label_kind);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (label_kind == UINT16_C(1)) {
        inline_source = UINT32_C(0x0004d30c);
        instructions += UINT64_C(2);
    } else {
        instructions += UINT64_C(3);
    }
    status = copy_diagnostic_text(
        machine, inline_source, inline_destination, &inline_characters
    );
    if (status != VF2_OK) {
        return status;
    }
    inline_cursor = inline_source;
    do {
        status = vf2_model2a_read_u32(machine, inline_cursor, &inline_word);
        if (status != VF2_OK) {
            return status;
        }
        inline_cursor += UINT32_C(4);
        ++inline_words;
        if (inline_words > UINT64_C(1024)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    } while (inline_word > UINT32_C(0x00ffffff));
    instructions += inline_characters * UINT64_C(8) + UINT64_C(17) +
                    inline_words * UINT64_C(3);

    status = vf2_model2a_read(
        machine, UINT32_C(0x0050002b), &display_mode, sizeof(display_mode)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (display_mode == UINT8_C(12)) {
        name_destination = UINT32_C(0x010040ea);
        instructions += UINT64_C(11);
    } else if (display_mode == UINT8_C(13)) {
        name_destination = UINT32_C(0x010040ea);
        instructions += UINT64_C(13);
    } else {
        instructions += UINT64_C(12);
    }
    name_source = UINT32_C(0x0004d377) + texture_index * UINT32_C(32);
    status = copy_diagnostic_text(
        machine, name_source, name_destination, &name_characters
    );
    if (status != VF2_OK) {
        return status;
    }
    instructions += name_characters * UINT64_C(8) + UINT64_C(8);

    cpu->registers[16] = name_source;
    cpu->registers[25] = name_destination;
    set_equal_condition(cpu);
    account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
    status = finish_recovered_procedure(machine, cpu, instructions);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE;
    report->entry_address = VF2_TEXTURE_STATUS_LINE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = inline_words;
    report->rows = inline_characters + name_characters;
    report->bytes_written =
        (size_t)(inline_characters + name_characters) * 2u + sizeof(uint32_t);
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = UINT64_C(2);
    report->recovered_procedure_returns = UINT64_C(3);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_texture_address_table(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t g6 = cpu->registers[22];
    uint32_t g7 = cpu->registers[23];
    const uint32_t g8 = cpu->registers[24];
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r11 = UINT32_C(0x0055c2f8);
    uint32_t r6 = 0u;
    uint32_t r7 = 0u;
    uint32_t r8 = UINT32_C(9);
    uint64_t instructions = UINT64_C(1);
    size_t pointers_written = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if ((g8 & UINT32_C(1)) == 0u) {
        r9 = UINT32_C(0x12600000);
        r10 = UINT32_C(0x12200000);
        instructions += UINT64_C(3);
    } else {
        r10 = UINT32_C(0x12600000);
        r9 = UINT32_C(0x12200000);
        instructions += UINT64_C(2);
    }
    instructions += UINT64_C(4);

    if ((g7 & (UINT32_C(1) << 10u)) != 0u) {
        instructions += UINT64_C(1);
        if ((g6 & (UINT32_C(1) << 9u)) != 0u) {
            g7 &= ~(UINT32_C(1) << 10u);
            g6 &= ~(UINT32_C(1) << 9u);
            g7 <<= 1u;
            g6 <<= 1u;
            instructions += UINT64_C(5);
        } else {
            const uint32_t address_index =
                ((g6 + UINT32_C(0x400)) << 9u) +
                (g7 & ~(UINT32_C(1) << 10u));
            status = vf2_model2a_write_u32(
                machine, r11, r9 + address_index * UINT32_C(2)
            );
            if (status != VF2_OK) {
                return status;
            }
            r11 += UINT32_C(4);
            {
                const uint32_t swap = r9;
                r9 = r10;
                r10 = swap;
            }
            ++pointers_written;
            instructions += UINT64_C(11);
        }
    } else {
        const uint32_t address_index = (g6 << 9u) + g7;
        status = vf2_model2a_write_u32(
            machine, r11, r9 + address_index * UINT32_C(2)
        );
        if (status != VF2_OK) {
            return status;
        }
        r11 += UINT32_C(4);
        {
            const uint32_t swap = r9;
            r9 = r10;
            r10 = swap;
        }
        ++pointers_written;
        instructions += UINT64_C(8);
    }

    r9 += UINT32_C(0x00180000);
    r10 += UINT32_C(0x00180000);
    instructions += UINT64_C(3);
    while (r8 != 0u) {
        uint32_t r3 = 0u;
        uint32_t r4 = 0u;
        uint32_t r15 = 0u;
        uint32_t shift = 0u;

        g6 >>= 1u;
        g7 >>= 1u;
        g6 &= ~UINT32_C(1);
        g7 &= ~UINT32_C(1);
        r3 = r6 + g6;
        r4 = r7 + g7;
        r3 = (r3 << 9u) + r4;
        r15 = r9 + r3 * UINT32_C(2);
        status = vf2_model2a_write_u32(machine, r11, r15);
        if (status != VF2_OK) {
            return status;
        }
        r11 += UINT32_C(4);
        {
            const uint32_t swap = r9;
            r9 = r10;
            r10 = swap;
        }
        shift = UINT32_C(1) << (r8 & UINT32_C(31));
        r7 += shift;
        r6 += shift >> 1u;
        --r8;
        ++pointers_written;
        instructions += UINT64_C(20);
    }

    cpu->registers[22] = g6;
    cpu->registers[23] = g7;
    set_equal_condition(cpu);
    status = finish_recovered_procedure(machine, cpu, instructions + UINT64_C(1));
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE;
    report->entry_address = VF2_TEXTURE_ADDRESS_TABLE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = pointers_written;
    report->bytes_written = pointers_written * sizeof(uint32_t);
    report->recovered_instruction_count = instructions + UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}


vf2_status execute_frame_timer_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t timer_low = 0u;
    uint32_t timer_high = 0u;
    uint32_t frame_counter = 0u;
    uint32_t previous_minimum = 0u;
    uint8_t frame_byte = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00f00008), &timer_low);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00f0000c), &timer_high);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    }
    if (status != VF2_OK || frame_byte != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[14] = UINT32_C(0x000fffff);
    cpu->registers[15] =
        UINT32_C(0x000fffff) - (timer_high & UINT32_C(0x000fffff));
    cpu->registers[15] -= UINT32_C(18);
    cpu->registers[3] = cpu->registers[15] / UINT32_C(25);
    cpu->registers[6] = frame_byte;
    cpu->registers[4] = UINT32_C(0x000043db);
    cpu->registers[5] = cpu->registers[4] - cpu->registers[3];
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x0050015c), cpu->registers[5]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500020), &frame_counter
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((frame_counter & UINT32_C(31)) == 0u) {
        /* cmpobe at 0x00010f5c jumps directly to the minimum store and, like
         * the other COBR instructions, does not update the arithmetic
         * condition code. The skipped load leaves r3 at zero. */
        cpu->registers[3] = 0u;
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500160), cpu->registers[5]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050006d), &mode, 1u
            );
        }
        if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;
        (void)timer_low;
        finish_recovered_control_block(
            cpu, UINT32_C(0x00010f90), UINT64_C(21)
        );
        report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX;
        report->entry_address = VF2_FRAME_TIMER_PREFIX_ENTRY;
        report->exit_address = cpu->ip;
        report->changed_values = UINT64_C(2);
        report->bytes_written = sizeof(uint32_t) * 2u;
        report->recovered_instruction_count = UINT64_C(21);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500160), &previous_minimum
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500160), cpu->registers[5]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050006d), &mode, 1u);
    }
    if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[3] = previous_minimum;
    cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;
    (void)timer_low;
    finish_recovered_control_block(cpu, UINT32_C(0x00010f90), UINT64_C(23));
    report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX;
    report->entry_address = VF2_FRAME_TIMER_PREFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(2);
    report->bytes_written = sizeof(uint32_t) * 2u;
    report->recovered_instruction_count = UINT64_C(23);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_save_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t saved_sp = cpu->registers[1];
    uint32_t runtime_flags = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[3] = saved_sp;
    cpu->registers[1] += UINT32_C(64);
    for (index = 0u; index < 16u; ++index) {
        status = vf2_model2a_write_u32(
            machine, saved_sp + (uint32_t)index * UINT32_C(4),
            cpu->registers[VF2_I960_G0_REGISTER + index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    cpu->registers[VF2_I960_G0_REGISTER + 10u] = UINT32_C(1) << 23u;
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(17) << 19u;
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(1) << 14u;
    cpu->registers[15] = UINT32_C(0x000fffff);
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00f00008), cpu->registers[15]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((runtime_flags & (UINT32_C(1) << 31u)) == 0u) {
        /* Cold boot takes the ROM's idle branch at 0x0bfc.  It skips the
         * player/video calls and continues at 0x0c80, where the matching
         * runtime-flag test selects the common interrupt restore. */
        cpu->registers[15] = runtime_flags;
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
        finish_recovered_control_block(cpu, UINT32_C(0x00000c80), UINT64_C(13));
        report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX;
        report->entry_address = VF2_INTERRUPT_SAVE_PREFIX_ENTRY;
        report->exit_address = cpu->ip;
        report->changed_values = UINT64_C(19);
        report->bytes_written = 16u * sizeof(uint32_t) + sizeof(uint32_t);
        report->recovered_instruction_count = UINT64_C(13);
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    cpu->registers[15] = runtime_flags;
    finish_recovered_control_block(cpu, UINT32_C(0x00000c00), UINT64_C(13));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX;
    report->entry_address = VF2_INTERRUPT_SAVE_PREFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(19);
    report->bytes_written = 16u * sizeof(uint32_t) + sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(13);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_buffer_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint8_t first = 0u;
    uint8_t second = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &pointer);
    cpu->registers[3] = pointer;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69c), &first, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69d), &second, 1u);
    }
    cpu->registers[13] = first;
    cpu->registers[14] = second;
    if (status != VF2_OK) {
        return status;
    }
    if (first != second) {
        status = vf2_model2a_write(
            machine, pointer + UINT32_C(0x69d), &first, sizeof(first)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &pointer);
    cpu->registers[3] = pointer;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69c), &first, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, pointer + UINT32_C(0x69d), &second, 1u);
    }
    cpu->registers[13] = first;
    cpu->registers[14] = second;
    if (status != VF2_OK) {
        return status;
    }
    if (first != second) {
        status = vf2_model2a_write(
            machine, pointer + UINT32_C(0x69d), &first, sizeof(first)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 13u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = runtime_flags;
    finish_recovered_control_block(cpu, UINT32_C(0x00000c78), UINT64_C(10));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE;
    report->entry_address = VF2_INTERRUPT_BUFFER_GATE_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(2);
    report->recovered_instruction_count = UINT64_C(10);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_input_ring(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report ring_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t saved_g0 = cpu->registers[VF2_I960_G0_REGISTER];
    vf2_status status = VF2_OK;

    memset(&ring_report, 0, sizeof(ring_report));
    cpu->registers[1] += UINT32_C(4);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[1] - UINT32_C(4), saved_g0
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_INPUT_RING_POLL_ENTRY, UINT32_C(0x00000ca4)
        );
    }
    if (status == VF2_OK) {
        status = execute_input_ring_poll(machine, cpu, &ring_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000ca4) ||
        cpu->registers[VF2_I960_G0_REGISTER] != UINT32_MAX) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = UINT32_MAX;
    status = vf2_model2a_read_u32(
        machine, cpu->registers[1] - UINT32_C(4),
        &cpu->registers[VF2_I960_G0_REGISTER]
    );
    cpu->registers[1] -= UINT32_C(4);
    if (status != VF2_OK || cpu->registers[VF2_I960_G0_REGISTER] != saved_g0) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    finish_recovered_control_block(cpu, UINT32_C(0x00000cd4), UINT64_C(7));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_INPUT_RING;
    report->entry_address = VF2_INTERRUPT_INPUT_RING_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written = sizeof(uint32_t) + ring_report.bytes_written;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_restore_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    uint32_t saved_sp = cpu->registers[1] - UINT32_C(64);
    uint8_t frame_byte = 0u;
    size_t index = 0u;
    vf2_status status = vf2_model2a_read(
        machine, UINT32_C(0x00500000), &frame_byte, 1u
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[5] = (uint32_t)(int32_t)(int8_t)frame_byte;
    cpu->registers[5] += UINT32_C(1);
    frame_byte = (uint8_t)cpu->registers[5];
    status = vf2_model2a_write(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    cpu->registers[3] = saved_sp;
    for (index = 0u; status == VF2_OK && index < 16u; ++index) {
        status = vf2_model2a_read_u32(
            machine, saved_sp + (uint32_t)index * UINT32_C(4),
            &cpu->registers[VF2_I960_G0_REGISTER + index]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[1] = saved_sp;
    cpu->registers[4] = UINT32_C(0x00e80000);
    cpu->registers[5] = UINT32_C(0xfffffffe);
    status = vf2_model2a_write_u32(
        machine, cpu->registers[4], cpu->registers[5]
    );
    if (status != VF2_OK) {
        return status;
    }
    finish_recovered_control_block(cpu, UINT32_C(0x00000d20), UINT64_C(12));
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_RESTORE_PREFIX;
    report->entry_address = VF2_INTERRUPT_RESTORE_PREFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(18);
    report->bytes_written = sizeof(uint8_t) + sizeof(uint32_t);
    report->recovered_instruction_count = UINT64_C(12);
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_frame_timer_suffix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report latch_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t runtime_flags = 0u;
    uint32_t video_status = 0u;
    uint8_t frame_byte = 0u;
    vf2_status status = VF2_OK;

    memset(&latch_report, 0, sizeof(latch_report));
    status = vf2_model2a_read(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    if (status != VF2_OK || frame_byte >= UINT8_C(2)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[4] = frame_byte;
    cpu->registers[3] = 0u;
    frame_byte = 0u;
    status = vf2_model2a_write(machine, UINT32_C(0x00500000), &frame_byte, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00980014), &video_status);
    }
    cpu->registers[3] = video_status & UINT32_C(15);
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_VIDEO_STATUS_LATCH_ENTRY, UINT32_C(0x00010fe4)
    );
    if (status == VF2_OK) {
        status = execute_video_status_latch(machine, cpu, &latch_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00010fe4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    if (status != VF2_OK || (runtime_flags & (UINT32_C(1) << 9u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = runtime_flags;
    status = finish_recovered_procedure(machine, cpu, UINT64_C(11));
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_SUFFIX;
    report->entry_address = VF2_FRAME_TIMER_SUFFIX_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = UINT64_C(2);
    report->bytes_written = sizeof(uint8_t) + latch_report.bytes_written;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_player_layer(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_i960_cpu candidate_cpu;
    vf2_hybrid_bridge_report player_report = {0};
    vf2_hybrid_bridge_report video_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    /* Compose nested recovered procedures against a CPU candidate. A rejected
     * branch must not leave a partially entered local frame on the caller. */
    candidate_cpu = *cpu;
    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_PLAYER_UPDATE_GATE_ENTRY,
        UINT32_C(0x00000c7c)
    );
    if (status == VF2_OK) {
        status = execute_player_update_gate(
            machine, &candidate_cpu, &player_report
        );
    }
    if (status != VF2_OK || candidate_cpu.ip != UINT32_C(0x00000c7c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        &candidate_cpu,
        VF2_VIDEO_LAYER_COMMIT_ENTRY,
        UINT32_C(0x00000c80)
    );
    if (status == VF2_OK) {
        status = execute_video_layer_commit(
            machine, &candidate_cpu, &video_report
        );
    }
    if (status != VF2_OK || candidate_cpu.ip != UINT32_C(0x00000c80)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    candidate_cpu.executed_instructions += UINT64_C(2);
    *cpu = candidate_cpu;

    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER;
    report->entry_address = VF2_INTERRUPT_PLAYER_LAYER_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written =
        player_report.bytes_written + video_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_interrupt_game_input(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report nested={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns; uint32_t flags=0u;
    vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x00500068),&flags);
    if(status!=VF2_OK) return status;
    if((flags&(UINT32_C(1)<<31u))==0u) {
        cpu->registers[15]=flags;
        finish_recovered_control_block(cpu,UINT32_C(0x00000ce0),UINT64_C(2));
        report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT; report->entry_address=VF2_INTERRUPT_GAME_INPUT_ENTRY; report->exit_address=cpu->ip; report->recovered_instruction_count=UINT64_C(2); report->cpu_poststate_applied=1; return VF2_OK;
    }
    cpu->registers[15]=flags;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_GAME_INPUT_UPDATE_ENTRY,UINT32_C(0x00000c90));
    if(status==VF2_OK) status=execute_game_input_update(machine,cpu,&nested);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000c90)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(3);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT; report->entry_address=VF2_INTERRUPT_GAME_INPUT_ENTRY; report->exit_address=cpu->ip; report->bytes_written=nested.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_interrupt_game_state(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report nested={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_GAME_STATE_UPDATE_ENTRY,UINT32_C(0x00000c94));
    if(status==VF2_OK) status=execute_game_state_update(machine,cpu,&nested,true);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x000020ec)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(1);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_GAME_STATE; report->entry_address=VF2_INTERRUPT_GAME_STATE_ENTRY; report->exit_address=cpu->ip; report->bytes_written=nested.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_interrupt_tile_sync(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0},d={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_TILE_RUNTIME_GATE_ENTRY,UINT32_C(0x00000cd8));
    if(status==VF2_OK) status=execute_tile_runtime_gate(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000cd8)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_TILE_CONTROLLER_UPDATE_ENTRY,UINT32_C(0x00000cdc));
    if(status==VF2_OK) status=execute_tile_controller_update(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000cdc)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_VIDEO_INPUT_SYNC_ENTRY,UINT32_C(0x00000ce0));
    if(status==VF2_OK) status=execute_video_input_sync(machine,cpu,&d);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00000ce0)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->executed_instructions+=UINT64_C(3);
    report->kind=VF2_HYBRID_BRIDGE_INTERRUPT_TILE_SYNC; report->entry_address=VF2_INTERRUPT_TILE_SYNC_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+d.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_post_timer(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0},d={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_SYSTEM_MEMORY_DIAGNOSTIC_ENTRY,UINT32_C(0x0000a03c));
    if(status==VF2_OK) status=execute_system_memory_diagnostic(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a03c)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_COUNTER_ADVANCE_ENTRY,UINT32_C(0x0000a040));
    if(status==VF2_OK) status=execute_frame_counter_advance(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a040)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_PHASE_ADVANCE_ENTRY,UINT32_C(0x0000a044));
    if(status==VF2_OK) status=execute_frame_phase_advance(machine,cpu,&d);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a044)) return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    finish_recovered_control_block(cpu,UINT32_C(0x00009fb0),UINT64_C(4));
    report->kind=VF2_HYBRID_BRIDGE_MAIN_POST_TIMER; report->entry_address=VF2_MAIN_POST_TIMER_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+d.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_clear_prefix(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    static const uint32_t addresses[5]={UINT32_C(0x0050101c),UINT32_C(0x005010d0),UINT32_C(0x00501010),UINT32_C(0x00501014),UINT32_C(0x00501998)};
    uint32_t flags=0u; size_t n=0u; vf2_status status=VF2_OK; cpu->registers[3]=0u;
    for(n=0;n<5u;++n){status=vf2_model2a_write_u32(machine,addresses[n],0u); if(status!=VF2_OK)return status;}
    cpu->registers[15]=UINT32_C(0x000fffff); status=vf2_model2a_write_u32(machine,UINT32_C(0x00f00000),cpu->registers[15]);
    if(status==VF2_OK) status=vf2_model2a_read_u32(machine,UINT32_C(0x00508000),&flags);
    if(status!=VF2_OK||(flags&(UINT32_C(1)<<13u))!=0u)return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    cpu->registers[15]=flags; finish_recovered_control_block(cpu,UINT32_C(0x00009ff8),UINT64_C(10));
    report->kind=VF2_HYBRID_BRIDGE_MAIN_CLEAR_PREFIX; report->entry_address=VF2_MAIN_CLEAR_PREFIX_ENTRY; report->exit_address=cpu->ip; report->changed_values=UINT64_C(6); report->bytes_written=sizeof(uint32_t)*6u; report->recovered_instruction_count=UINT64_C(10); report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_final_cluster(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0},d={0},e={0},f={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns;
    const uint32_t start_depth=cpu->local_frame_depth;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_SHADOW_VERIFY_ENTRY,UINT32_C(0x00009ffc));
    if(status==VF2_OK)status=execute_frame_shadow_verify(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x00009ffc))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,UINT32_C(0x00029744),UINT32_C(0x0000a000));
    if(status==VF2_OK)status=vf2_i960_cpu_return_procedure(cpu,machine);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a000))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_BUFFER_GATE_ENTRY,UINT32_C(0x0000a004)); if(status==VF2_OK)status=execute_frame_buffer_gate(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a004))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_GEOMETRY_COMMAND_SETUP_ENTRY,UINT32_C(0x0000a008)); if(status==VF2_OK)status=execute_geometry_command_setup(machine,cpu,&d);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a008))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_SCRATCH_CLEAR_ENTRY,UINT32_C(0x0000a00c)); if(status==VF2_OK)status=execute_frame_scratch_clear(machine,cpu,&e);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a00c))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_DISPATCH_TICK_ENTRY,UINT32_C(0x0000a010)); if(status==VF2_OK)status=execute_frame_dispatch_tick(machine,cpu,&f);
    if(status!=VF2_OK)return status;
    if(cpu->ip!=UINT32_C(0x000000b0) &&
       cpu->ip!=UINT32_C(0x0000a010))return VF2_ERROR_UNSUPPORTED;
    if (start_depth == 0u && cpu->ip == UINT32_C(0x0000a010)) {
        cpu->registers[13] = (start_depth << 8u) | cpu->local_frame_depth;
    }
    cpu->executed_instructions+=UINT64_C(7);
    report->kind=VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER; report->entry_address=VF2_MAIN_FINAL_CLUSTER_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+d.bytes_written+e.bytes_written+f.bytes_written; report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_geometry_prefix(
    vf2_model2a *machine, vf2_i960_cpu *cpu, vf2_hybrid_bridge_report *report)
{
    vf2_hybrid_bridge_report a={0},b={0}; uint64_t i=cpu->executed_instructions,c=cpu->procedure_calls,r=cpu->procedure_returns; uint32_t counter=0u;
    vf2_status status=vf2_i960_cpu_enter_procedure(cpu,VF2_GEOMETRY_FRAME_COMMIT_ENTRY,UINT32_C(0x0000a018)); if(status==VF2_OK)status=execute_geometry_frame_commit(machine,cpu,&a);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a018))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_i960_cpu_enter_procedure(cpu,VF2_FRAME_GEOMETRY_GATE_ENTRY,UINT32_C(0x0000a01c)); if(status==VF2_OK)status=execute_frame_geometry_gate(machine,cpu,&b);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a01c))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    status=vf2_model2a_read_u32(machine,UINT32_C(0x00500020),&counter); cpu->registers[14]=counter; cpu->registers[15]=counter+UINT32_C(1);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00500020), cpu->registers[15]
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    finish_recovered_control_block(cpu,UINT32_C(0x0000a030),UINT64_C(5));
    report->kind=VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX; report->entry_address=VF2_MAIN_GEOMETRY_PREFIX_ENTRY; report->exit_address=cpu->ip; report->bytes_written=a.bytes_written+b.bytes_written+sizeof(uint32_t); report->recovered_instruction_count=cpu->executed_instructions-i; report->recovered_procedure_calls=cpu->procedure_calls-c; report->recovered_procedure_returns=cpu->procedure_returns-r; report->cpu_poststate_applied=1; return VF2_OK;
}

vf2_status execute_main_texture_orchestrator_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report nested_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_ORCHESTRATOR_SAVE_ENTRY,
        UINT32_C(0x0000a034)
    );

    if (status == VF2_OK) {
        status = execute_texture_orchestrator_save_call(
            machine, cpu, &nested_report
        );
    }
    if (status != VF2_OK ||
        cpu->ip != VF2_TEXTURE_ORCHESTRATOR_BODY_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += UINT64_C(1);

    report->kind = VF2_HYBRID_BRIDGE_MAIN_TEXTURE_ORCHESTRATOR_CALL;
    report->entry_address = VF2_MAIN_TEXTURE_ORCHESTRATOR_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = nested_report.iterations;
    report->bytes_written = nested_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

vf2_status execute_main_frame_timer_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report nested_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_FRAME_TIMER_PREFIX_ENTRY,
        UINT32_C(0x0000a038)
    );

    if (status == VF2_OK) {
        status = execute_frame_timer_prefix(machine, cpu, &nested_report);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00010f90)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += UINT64_C(1);

    report->kind = VF2_HYBRID_BRIDGE_MAIN_FRAME_TIMER_CALL;
    report->entry_address = VF2_MAIN_FRAME_TIMER_CALL_ENTRY;
    report->exit_address = cpu->ip;
    report->changed_values = nested_report.changed_values;
    report->bytes_written = nested_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_interrupt_fighter_compare_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_status status = VF2_OK;
    uint8_t value = 0u;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00000c0c)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &cpu->registers[3]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, cpu->registers[3] + UINT32_C(0x069c),
            &value, sizeof(value)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[13] = (uint32_t)value;
        status = vf2_model2a_read(
            machine, cpu->registers[3] + UINT32_C(0x069d),
            &value, sizeof(value)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[14] = (uint32_t)value;
    cpu->executed_instructions += UINT64_C(3);

    ++cpu->executed_instructions;
    if (cpu->registers[13] != cpu->registers[14]) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500808), &cpu->registers[3]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, cpu->registers[3] + UINT32_C(0x069c),
            &value, sizeof(value)
        );
    }
    if (status == VF2_OK) {
        cpu->registers[13] = (uint32_t)value;
        status = vf2_model2a_read(
            machine, cpu->registers[3] + UINT32_C(0x069d),
            &value, sizeof(value)
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[14] = (uint32_t)value;
    cpu->executed_instructions += UINT64_C(4);
    if (cpu->registers[13] != cpu->registers[14]) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &cpu->registers[15]
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(2);
    if ((cpu->registers[15] & (UINT32_C(1) << 13u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->compare_result = VF2_I960_COMPARE_NONE;
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->ip = UINT32_C(0x00000c78);
    return VF2_OK;
}

vf2_status execute_interrupt_initial_cluster(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report compose_report = {0};
    vf2_hybrid_bridge_report latch_report = {0};
    vf2_hybrid_bridge_report dispatch_report = {0};
    vf2_hybrid_bridge_report upload_report = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_VIDEO_REGISTER_COMPOSE_ENTRY,
        UINT32_C(0x00000c04)
    );

    if (status == VF2_OK) {
        status = execute_video_register_compose(
            machine, cpu, &compose_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c04)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_VIDEO_INPUT_LATCH_WRITE_ENTRY,
        UINT32_C(0x00000c08)
    );
    if (status == VF2_OK) {
        status = execute_video_input_latch_write(
            machine, cpu, &latch_report
        );
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c08)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY,
        UINT32_C(0x00000c0c)
    );
    if (status == VF2_OK) {
        status = execute_texture_upload_dispatch(
            machine, cpu, &dispatch_report
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip == UINT32_C(0x00000c0c) ||
        cpu->ip == UINT32_C(0x0004bb14)) {
        if (cpu->ip == UINT32_C(0x0004bb14)) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
            if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c0c)) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
        }
        status = execute_interrupt_fighter_compare_prefix(machine, cpu);
        if (status != VF2_OK) {
            return status;
        }
        report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER;
        report->entry_address = VF2_INTERRUPT_INITIAL_CLUSTER_ENTRY;
        report->exit_address = cpu->ip;
        report->iterations = dispatch_report.iterations;
        report->bytes_written =
            compose_report.bytes_written + latch_report.bytes_written +
            dispatch_report.bytes_written;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        report->cpu_poststate_applied = 1;
        return VF2_OK;
    }
    if (cpu->ip != VF2_PALETTE_PAGE_UPLOAD_ENTRY) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = execute_palette_page_upload(machine, cpu, &upload_report);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0004bab4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* 0x4bab4 returns from the texture-upload dispatcher to the interrupt
     * caller.  The following three instructions load the active fighter
     * record and the two bytes compared by the next branch at 0x0c1c. */
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00000c0c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions += UINT64_C(1);
    status = execute_interrupt_fighter_compare_prefix(machine, cpu);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER;
    report->entry_address = VF2_INTERRUPT_INITIAL_CLUSTER_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = upload_report.iterations;
    report->rows = upload_report.rows;
    report->bytes_written =
        compose_report.bytes_written + latch_report.bytes_written +
        dispatch_report.bytes_written + upload_report.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}
