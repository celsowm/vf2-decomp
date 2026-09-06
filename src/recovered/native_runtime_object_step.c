#include "vf2/native_runtime.h"

#include <stdint.h>
#include <string.h>

#define VF2_OBJECT_SERVICE_ENTRY UINT32_C(0x0006ca84)

vf2_status vf2_native_runtime_step_base_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
);

const char *vf2_native_runtime_step_kind_name_base(
    vf2_native_runtime_step_kind kind
);

vf2_status vf2_recovered_object_service_loop_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

vf2_status vf2_native_runtime_step_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_step_report local_report;
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL ||
        cpu->ip != VF2_OBJECT_SERVICE_ENTRY) {
        return vf2_native_runtime_step_base_impl(
            machine, cpu, state, report
        );
    }

    memset(effective_report, 0, sizeof(*effective_report));
    start_instructions = cpu->executed_instructions;
    start_calls = cpu->procedure_calls;
    start_returns = cpu->procedure_returns;

    status = vf2_recovered_object_service_loop_execute(machine, cpu);
    if (status != VF2_OK) {
        return status;
    }

    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_OBJECT_SERVICE;
    effective_report->entry_address = VF2_OBJECT_SERVICE_ENTRY;
    effective_report->exit_address = cpu->ip;
    effective_report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    effective_report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    effective_report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;

    ++state->blocks_executed;
    state->recovered_instruction_count +=
        effective_report->recovered_instruction_count;
    state->recovered_procedure_calls +=
        effective_report->recovered_procedure_calls;
    state->recovered_procedure_returns +=
        effective_report->recovered_procedure_returns;
    return VF2_OK;
}

const char *vf2_native_runtime_step_kind_name(
    vf2_native_runtime_step_kind kind
)
{
    if (kind == VF2_NATIVE_RUNTIME_STEP_OBJECT_SERVICE) {
        return "object-service";
    }
    return vf2_native_runtime_step_kind_name_base(kind);
}
