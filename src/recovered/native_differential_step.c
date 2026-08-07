#include "vf2/native_differential.h"

#include <string.h>

static void compare_execution_counters(
    const vf2_i960_cpu *reference_cpu,
    const vf2_i960_cpu *native_cpu,
    vf2_i960_snapshot_diff *diff
)
{
    const uint64_t reference_values[] = {
        reference_cpu->executed_instructions,
        reference_cpu->procedure_calls,
        reference_cpu->procedure_returns,
        reference_cpu->interrupt_entries,
        reference_cpu->interrupt_returns,
        (uint64_t)reference_cpu->maximum_local_frame_depth
    };
    const uint64_t native_values[] = {
        native_cpu->executed_instructions,
        native_cpu->procedure_calls,
        native_cpu->procedure_returns,
        native_cpu->interrupt_entries,
        native_cpu->interrupt_returns,
        (uint64_t)native_cpu->maximum_local_frame_depth
    };
    size_t index = 0u;

    for (index = 0u;
         index < sizeof(reference_values) / sizeof(reference_values[0]);
         ++index) {
        if (reference_values[index] != native_values[index]) {
            if (diff->equal) {
                diff->equal = false;
                (void)strcpy(diff->component, "cpu-counters");
                diff->first_offset = index;
                diff->expected_value = (uint32_t)reference_values[index];
                diff->actual_value = (uint32_t)native_values[index];
            }
            ++diff->differing_bytes;
        }
    }
}

static vf2_status compare_complete_state(
    const vf2_model2a *reference_machine,
    const vf2_i960_cpu *reference_cpu,
    const vf2_model2a *native_machine,
    const vf2_i960_cpu *native_cpu,
    vf2_i960_snapshot *reference_snapshot,
    vf2_i960_snapshot *native_snapshot,
    vf2_i960_snapshot_diff *diff
)
{
    vf2_status status = vf2_i960_snapshot_capture(
        reference_snapshot,
        reference_cpu,
        reference_machine
    );
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            native_snapshot,
            native_cpu,
            native_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(
            reference_snapshot,
            native_snapshot,
            diff
        );
    }
    if (status == VF2_OK && diff->equal) {
        compare_execution_counters(reference_cpu, native_cpu, diff);
    }
    return status;
}

static void record_ip_mismatch(
    vf2_i960_snapshot_diff *diff,
    uint32_t reference_ip,
    uint32_t native_ip
)
{
    memset(diff, 0, sizeof(*diff));
    diff->equal = false;
    (void)strcpy(diff->component, "cpu-ip");
    diff->differing_bytes = sizeof(reference_ip);
    diff->expected_value = reference_ip;
    diff->actual_value = native_ip;
}

static void compare_frame_wait_state(
    const vf2_hybrid_frame_wait_state *reference_state,
    const vf2_hybrid_frame_wait_state *native_state,
    vf2_i960_snapshot_diff *diff
)
{
    const size_t reference_values[] = {
        reference_state->visits,
        reference_state->visits_before_interrupt,
        reference_state->interrupts_injected
    };
    const size_t native_values[] = {
        native_state->visits,
        native_state->visits_before_interrupt,
        native_state->interrupts_injected
    };
    size_t index = 0u;

    memset(diff, 0, sizeof(*diff));
    diff->equal = true;
    for (index = 0u;
         index < sizeof(reference_values) / sizeof(reference_values[0]);
         ++index) {
        if (reference_values[index] != native_values[index]) {
            if (diff->equal) {
                diff->equal = false;
                (void)strcpy(diff->component, "frame-wait-state");
                diff->first_offset = index;
                diff->expected_value = (uint32_t)reference_values[index];
                diff->actual_value = (uint32_t)native_values[index];
            }
            ++diff->differing_bytes;
        }
    }
}

static vf2_status advance_reference(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    const vf2_native_runtime_step_report *native_step,
    const vf2_hybrid_frame_wait_state *frame_wait_before,
    vf2_hybrid_frame_wait_state *frame_wait_after
)
{
    uint64_t index = 0u;
    vf2_status status = VF2_OK;

    *frame_wait_after = *frame_wait_before;
    for (index = 0u;
         status == VF2_OK &&
         index < native_step->recovered_instruction_count;
         ++index) {
        status = vf2_i960_step(reference_cpu, reference_machine, NULL);
        if (status == VF2_OK &&
            native_step->kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
            vf2_hybrid_frame_wait_report wait_report;

            memset(&wait_report, 0, sizeof(wait_report));
            status = vf2_hybrid_frame_wait_observe(
                reference_machine,
                reference_cpu,
                frame_wait_after,
                &wait_report
            );
        }
    }
    return status;
}

vf2_status vf2_native_differential_step(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    vf2_native_differential_step_report *report
)
{
    vf2_native_differential_step_report local_report;
    vf2_i960_snapshot reference_snapshot;
    vf2_i960_snapshot native_snapshot;
    vf2_hybrid_frame_wait_state reference_frame_wait;
    vf2_hybrid_frame_wait_state frame_wait_before;
    uint64_t reference_instruction_start = 0u;
    vf2_status status = VF2_OK;

    if (reference_machine == NULL || reference_cpu == NULL ||
        native_machine == NULL || native_cpu == NULL ||
        native_state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = native_cpu->ip;
    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    local_report.diff.equal = true;
    vf2_i960_snapshot_init(&reference_snapshot);
    vf2_i960_snapshot_init(&native_snapshot);

    if (reference_cpu->ip != native_cpu->ip) {
        record_ip_mismatch(
            &local_report.diff,
            reference_cpu->ip,
            native_cpu->ip
        );
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        memset(&local_report.diff, 0, sizeof(local_report.diff));
        status = compare_complete_state(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            &reference_snapshot,
            &native_snapshot,
            &local_report.diff
        );
    }
    if (status == VF2_OK && !local_report.diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        frame_wait_before = native_state->frame_wait;
        reference_instruction_start = reference_cpu->executed_instructions;
        status = vf2_native_runtime_step(
            native_machine,
            native_cpu,
            native_state,
            &local_report.native_step
        );
    }
    if (status == VF2_OK &&
        local_report.native_step.recovered_instruction_count == 0u) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = advance_reference(
            reference_machine,
            reference_cpu,
            &local_report.native_step,
            &frame_wait_before,
            &reference_frame_wait
        );
        local_report.reference_instructions_executed =
            reference_cpu->executed_instructions -
            reference_instruction_start;
        local_report.native_recovered_instructions =
            local_report.native_step.recovered_instruction_count;
    }
    if (status == VF2_OK &&
        local_report.reference_instructions_executed !=
            local_report.native_recovered_instructions) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK &&
        local_report.native_step.kind ==
            VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
        compare_frame_wait_state(
            &reference_frame_wait,
            &native_state->frame_wait,
            &local_report.diff
        );
        if (!local_report.diff.equal) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        memset(&local_report.diff, 0, sizeof(local_report.diff));
        status = compare_complete_state(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            &reference_snapshot,
            &native_snapshot,
            &local_report.diff
        );
    }
    if (status == VF2_OK && !local_report.diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        local_report.matched = 1;
    }

    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    if (report != NULL) {
        *report = local_report;
    }
    vf2_i960_snapshot_destroy(&reference_snapshot);
    vf2_i960_snapshot_destroy(&native_snapshot);
    return status;
}
