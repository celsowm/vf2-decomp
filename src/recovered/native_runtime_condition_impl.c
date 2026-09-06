#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_condition_base
#include "native_runtime_condition_impl_base.c"
#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_condition_impl

#define VF2_GAME_DISP_LEAF_ENTRY UINT32_C(0x0002c5f0)
#define VF2_GAME_DISP_REGISTRY UINT32_C(0x00515b00)
#define VF2_GAME_DISP_FIGHTER0_SLOT UINT32_C(0x00500804)
#define VF2_GAME_DISP_FIGHTER1_SLOT UINT32_C(0x00500808)
#define VF2_GAME_DISP_FIGHTER_SELECTOR_OFFSET UINT32_C(0x00000004)
#define VF2_GAME_DISP_FIGHTER_DELTA_OFFSET UINT32_C(0x000001ac)
#define VF2_GAME_DISP_VALUE0_OFFSET UINT32_C(0x00000040)
#define VF2_GAME_DISP_VALUE1_OFFSET UINT32_C(0x00000042)
#define VF2_GAME_DISP_OUTPUT0_OFFSET UINT32_C(0x00000044)
#define VF2_GAME_DISP_OUTPUT1_OFFSET UINT32_C(0x00000046)
#define VF2_GAME_DISP_RETURN0 UINT32_C(0x0002b218)
#define VF2_GAME_DISP_RETURN1 UINT32_C(0x0002b224)
#define VF2_GAME_DISP_G9_0 UINT32_C(0x010001b4)
#define VF2_GAME_DISP_G9_1 UINT32_C(0x010001c6)
#define VF2_GAME_DISP_LEAF_INSTRUCTIONS UINT64_C(15)

static vf2_status game_disp_leaf_read_u16(
    vf2_model2a *machine,
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
        *value = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status game_disp_leaf_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };

    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static bool measured_game_disp_leaf_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint8_t *selector_out,
    uint32_t *return_address_out
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t registry_flags = 0u;
    uint32_t fighter = 0u;
    uint32_t return_address = 0u;
    uint16_t delta = 0u;
    uint16_t registry_value = 0u;
    uint8_t selector = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || selector_out == NULL ||
        return_address_out == NULL || cpu->ip != VF2_GAME_DISP_LEAF_ENTRY ||
        cpu->local_frame_depth != UINT32_C(3) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40)) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_FIGHTER0_SLOT, &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_FIGHTER1_SLOT, &fighter1);
    }
    fighter = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            fighter + VF2_GAME_DISP_FIGHTER_SELECTOR_OFFSET,
            &selector,
            sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine,
            fighter + VF2_GAME_DISP_FIGHTER_DELTA_OFFSET,
            &delta
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_REGISTRY, &registry_flags);
    }
    if (status != VF2_OK || delta != 0u || registry_flags != 0u || selector > UINT8_C(1)) {
        return false;
    }

    if (selector == UINT8_C(0)) {
        if (fighter != fighter0) {
            return false;
        }
        return_address = VF2_GAME_DISP_RETURN0;
        status = game_disp_leaf_read_u16(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_VALUE0_OFFSET,
            &registry_value
        );
    } else {
        if (fighter != fighter1) {
            return false;
        }
        return_address = VF2_GAME_DISP_RETURN1;
        status = game_disp_leaf_read_u16(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_VALUE1_OFFSET,
            &registry_value
        );
    }
    if (status != VF2_OK || registry_value != 0u ||
        cpu->local_frames[2].registers[2] != return_address ||
        cpu->local_frames[2].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }

    *selector_out = selector;
    *return_address_out = return_address;
    return true;
}

static vf2_status execute_measured_game_disp_leaf(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    uint8_t selector,
    uint32_t return_address,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint32_t output_address = VF2_GAME_DISP_REGISTRY +
        (selector == UINT8_C(0)
            ? VF2_GAME_DISP_OUTPUT0_OFFSET
            : VF2_GAME_DISP_OUTPUT1_OFFSET);
    const uint32_t g9 = selector == UINT8_C(0)
        ? VF2_GAME_DISP_G9_0
        : VF2_GAME_DISP_G9_1;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = game_disp_leaf_write_u16(
        machine, output_address, UINT16_C(0x801f)
    );

    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = g9;
    cpu->executed_instructions += VF2_GAME_DISP_LEAF_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != return_address ||
        cpu->procedure_returns != start_returns + UINT64_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_equal_condition(cpu);

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_LEAF_ENTRY;
    effective_report->exit_address = return_address;
    effective_report->current_registry_address = VF2_GAME_DISP_REGISTRY;
    effective_report->recovered_instruction_count = VF2_GAME_DISP_LEAF_INSTRUCTIONS;
    effective_report->recovered_procedure_returns = UINT64_C(1);

    ++state->blocks_executed;
    state->recovered_instruction_count += VF2_GAME_DISP_LEAF_INSTRUCTIONS;
    state->recovered_procedure_returns += UINT64_C(1);
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint8_t selector = UINT8_MAX;
    uint32_t return_address = 0u;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_leaf_case(
            machine, cpu, &selector, &return_address
        )) {
        return execute_measured_game_disp_leaf(
            machine, cpu, state, selector, return_address, report
        );
    }
    return vf2_native_runtime_step_condition_base(
        machine, cpu, state, report
    );
}
