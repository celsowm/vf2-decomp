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
    vf2_status status = VF2_OK;

    (void)reference_snapshot;
    (void)native_snapshot;
    status = vf2_i960_compare_live_state(
        reference_cpu,
        reference_machine,
        native_cpu,
        native_machine,
        diff
    );
    if (status == VF2_OK && diff->equal) {
        compare_execution_counters(reference_cpu, native_cpu, diff);
    }
    return status;
}

static void record_initial_ip_mismatch(
    vf2_native_differential_report *report,
    uint32_t reference_ip,
    uint32_t native_ip
)
{
    report->diff.equal = false;
    (void)strcpy(report->diff.component, "cpu-ip");
    report->diff.differing_bytes = sizeof(reference_ip);
    report->diff.first_offset = 0u;
    report->diff.expected_value = reference_ip;
    report->diff.actual_value = native_ip;
}

static void record_frame_wait_state_mismatch(
    vf2_native_differential_report *report,
    const vf2_hybrid_frame_wait_state *reference_state,
    const vf2_hybrid_frame_wait_state *native_state
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

    report->diff.equal = true;
    report->diff.differing_bytes = 0u;
    for (index = 0u;
         index < sizeof(reference_values) / sizeof(reference_values[0]);
         ++index) {
        if (reference_values[index] != native_values[index]) {
            if (report->diff.equal) {
                report->diff.equal = false;
                (void)strcpy(
                    report->diff.component,
                    "frame-wait-state"
                );
                report->diff.first_offset = index;
                report->diff.expected_value =
                    (uint32_t)reference_values[index];
                report->diff.actual_value =
                    (uint32_t)native_values[index];
            }
            ++report->diff.differing_bytes;
        }
    }
}

static vf2_status advance_reference_block(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    const vf2_native_runtime_step_report *step_report,
    const vf2_hybrid_frame_wait_state *frame_wait_before,
    vf2_hybrid_frame_wait_state *frame_wait_after
)
{
    uint64_t reference_step = 0u;
    vf2_status status = VF2_OK;

    *frame_wait_after = *frame_wait_before;
    for (reference_step = 0u;
         status == VF2_OK &&
         reference_step < step_report->recovered_instruction_count;
         ++reference_step) {
        status = vf2_i960_step(
            reference_cpu,
            reference_machine,
            NULL
        );
        if (status == VF2_OK &&
            step_report->kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
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

vf2_status vf2_native_differential_run_until_after(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t stop_address,
    size_t minimum_blocks,
    size_t max_blocks,
    vf2_native_differential_report *report
)
{
    vf2_native_differential_report local_report;
    vf2_i960_snapshot reference_snapshot;
    vf2_i960_snapshot native_snapshot;
    vf2_status status = VF2_OK;

    if (reference_machine == NULL || reference_cpu == NULL ||
        native_machine == NULL || native_cpu == NULL ||
        native_state == NULL || minimum_blocks > max_blocks) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = native_cpu->ip;
    local_report.stop_address = stop_address;
    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    local_report.minimum_blocks = minimum_blocks;
    local_report.diff.equal = true;

    vf2_i960_snapshot_init(&reference_snapshot);
    vf2_i960_snapshot_init(&native_snapshot);

    if (reference_cpu->ip != native_cpu->ip) {
        record_initial_ip_mismatch(
            &local_report,
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

    while (status == VF2_OK &&
           (native_cpu->ip != stop_address ||
            local_report.blocks_compared < minimum_blocks) &&
           local_report.blocks_compared < max_blocks) {
        vf2_native_runtime_step_report step_report;
        const uint64_t reference_instruction_start =
            reference_cpu->executed_instructions;
        const vf2_hybrid_frame_wait_state frame_wait_before =
            native_state->frame_wait;
        vf2_hybrid_frame_wait_state reference_frame_wait;

        if (reference_cpu->ip != native_cpu->ip) {
            record_initial_ip_mismatch(
                &local_report,
                reference_cpu->ip,
                native_cpu->ip
            );
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        memset(&step_report, 0, sizeof(step_report));
        status = vf2_native_runtime_step(
            native_machine,
            native_cpu,
            native_state,
            &step_report
        );
        local_report.last_step = step_report;
        if (status != VF2_OK) {
            break;
        }
        if (step_report.recovered_instruction_count == 0u) {
            if (step_report.kind !=
                VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            reference_frame_wait = frame_wait_before;
            if (reference_frame_wait.visits_before_interrupt == 0u) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            {
                vf2_hybrid_frame_wait_report wait_report;

                memset(&wait_report, 0, sizeof(wait_report));
                reference_frame_wait.visits =
                    reference_frame_wait.visits_before_interrupt - 1u;
                status = vf2_hybrid_frame_wait_observe(
                    reference_machine,
                    reference_cpu,
                    &reference_frame_wait,
                    &wait_report
                );
                if (status == VF2_OK && !wait_report.interrupt_injected) {
                    status = VF2_ERROR_UNSUPPORTED;
                }
            }
            if (status != VF2_OK) {
                break;
            }
        } else {
            status = advance_reference_block(
                reference_machine,
                reference_cpu,
                &step_report,
                &frame_wait_before,
                &reference_frame_wait
            );
            if (status != VF2_OK) {
                break;
            }
        }

        local_report.reference_instructions_executed +=
            reference_cpu->executed_instructions -
            reference_instruction_start;
        local_report.native_recovered_instructions +=
            step_report.recovered_instruction_count;

        if (reference_cpu->executed_instructions -
                reference_instruction_start !=
            step_report.recovered_instruction_count) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }
        if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
            record_frame_wait_state_mismatch(
                &local_report,
                &reference_frame_wait,
                &native_state->frame_wait
            );
            if (!local_report.diff.equal) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
        }

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
        if (status != VF2_OK) {
            break;
        }
        if (!local_report.diff.equal) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        ++local_report.blocks_compared;
    }

    if (status == VF2_OK &&
        reference_cpu->ip == stop_address &&
        native_cpu->ip == stop_address &&
        local_report.blocks_compared >= minimum_blocks) {
        local_report.reached_stop = 1;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
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

vf2_status vf2_native_differential_run_until(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t stop_address,
    size_t max_blocks,
    vf2_native_differential_report *report
)
{
    return vf2_native_differential_run_until_after(
        reference_machine,
        reference_cpu,
        native_machine,
        native_cpu,
        native_state,
        stop_address,
        0u,
        max_blocks,
        report
    );
}

vf2_status vf2_native_differential_run_cycles(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t repeated_address,
    size_t cycle_count,
    size_t minimum_blocks_per_cycle,
    size_t max_blocks_per_cycle,
    vf2_native_differential_cycles_report *report
)
{
    vf2_native_differential_cycles_report local_report;
    vf2_status status = VF2_OK;
    size_t cycle = 0u;

    if (reference_machine == NULL || reference_cpu == NULL ||
        native_machine == NULL || native_cpu == NULL ||
        native_state == NULL ||
        minimum_blocks_per_cycle > max_blocks_per_cycle ||
        (cycle_count != 0u && minimum_blocks_per_cycle == 0u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.repeated_address = repeated_address;
    local_report.requested_cycles = cycle_count;
    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;

    if (cycle_count == 0u) {
        status = vf2_native_differential_run_until(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            native_state,
            repeated_address,
            0u,
            &local_report.last_cycle
        );
    }

    for (cycle = 0u;
         status == VF2_OK && cycle < cycle_count;
         ++cycle) {
        vf2_native_differential_report cycle_report;

        memset(&cycle_report, 0, sizeof(cycle_report));
        status = vf2_native_differential_run_until_after(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            native_state,
            repeated_address,
            minimum_blocks_per_cycle,
            max_blocks_per_cycle,
            &cycle_report
        );

        local_report.blocks_compared += cycle_report.blocks_compared;
        local_report.reference_instructions_executed +=
            cycle_report.reference_instructions_executed;
        local_report.native_recovered_instructions +=
            cycle_report.native_recovered_instructions;
        local_report.last_cycle = cycle_report;

        if (status == VF2_OK) {
            ++local_report.completed_cycles;
        }
    }

    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    if (status == VF2_OK &&
        local_report.completed_cycles == cycle_count) {
        local_report.completed = 1;
    }
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_native_differential_probe_cycles(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *native_state,
    uint32_t repeated_address,
    size_t cycle_count,
    size_t minimum_blocks_per_cycle,
    size_t max_blocks_per_cycle,
    vf2_native_differential_cycles_report *report
)
{
    vf2_native_differential_cycles_report local_report;
    vf2_i960_snapshot reference_snapshot;
    vf2_i960_snapshot native_snapshot;
    vf2_i960_snapshot cycle_start_snapshot;
    vf2_native_runtime_state cycle_start_state;
    vf2_status status = VF2_OK;
    size_t cycle = 0u;

    if (reference_machine == NULL || reference_cpu == NULL ||
        native_machine == NULL || native_cpu == NULL ||
        native_state == NULL ||
        minimum_blocks_per_cycle > max_blocks_per_cycle ||
        (cycle_count != 0u && minimum_blocks_per_cycle == 0u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(&cycle_start_state, 0, sizeof(cycle_start_state));
    local_report.repeated_address = repeated_address;
    local_report.requested_cycles = cycle_count;
    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    local_report.last_cycle.diff.equal = true;
    vf2_i960_snapshot_init(&reference_snapshot);
    vf2_i960_snapshot_init(&native_snapshot);
    vf2_i960_snapshot_init(&cycle_start_snapshot);

    if (reference_cpu->ip != native_cpu->ip) {
        record_initial_ip_mismatch(
            &local_report.last_cycle,
            reference_cpu->ip,
            native_cpu->ip
        );
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        memset(&local_report.last_cycle.diff, 0,
               sizeof(local_report.last_cycle.diff));
        status = compare_complete_state(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            &reference_snapshot,
            &native_snapshot,
            &local_report.last_cycle.diff
        );
    }
    if (status == VF2_OK && !local_report.last_cycle.diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK && cycle_count == 0u) {
        local_report.last_cycle.start_address = native_cpu->ip;
        local_report.last_cycle.stop_address = repeated_address;
        local_report.last_cycle.final_reference_address = reference_cpu->ip;
        local_report.last_cycle.final_native_address = native_cpu->ip;
        local_report.last_cycle.reached_stop =
            reference_cpu->ip == repeated_address &&
            native_cpu->ip == repeated_address;
        if (!local_report.last_cycle.reached_stop) {
            status = VF2_ERROR_UNSUPPORTED;
        } else {
            local_report.completed = 1;
        }
    }

    for (cycle = 0u;
         status == VF2_OK && cycle < cycle_count;
         ++cycle) {
        vf2_native_differential_report cycle_report;
        vf2_status cycle_status = VF2_OK;

        memset(&cycle_report, 0, sizeof(cycle_report));
        cycle_report.start_address = native_cpu->ip;
        cycle_report.stop_address = repeated_address;
        cycle_report.final_reference_address = reference_cpu->ip;
        cycle_report.final_native_address = native_cpu->ip;
        cycle_report.minimum_blocks = minimum_blocks_per_cycle;
        cycle_report.diff.equal = true;

        cycle_status = vf2_i960_snapshot_capture(
            &cycle_start_snapshot,
            native_cpu,
            native_machine
        );
        if (cycle_status == VF2_OK) {
            cycle_start_state = *native_state;
        }

        while (cycle_status == VF2_OK &&
               (native_cpu->ip != repeated_address ||
                cycle_report.blocks_compared < minimum_blocks_per_cycle) &&
               cycle_report.blocks_compared < max_blocks_per_cycle) {
            vf2_native_runtime_step_report step_report;
            const uint64_t reference_instruction_start =
                reference_cpu->executed_instructions;
            const vf2_hybrid_frame_wait_state frame_wait_before =
                native_state->frame_wait;
            vf2_hybrid_frame_wait_state reference_frame_wait;

            if (reference_cpu->ip != native_cpu->ip) {
                record_initial_ip_mismatch(
                    &cycle_report,
                    reference_cpu->ip,
                    native_cpu->ip
                );
                cycle_status = VF2_ERROR_UNSUPPORTED;
                break;
            }

            memset(&step_report, 0, sizeof(step_report));
            cycle_status = vf2_native_runtime_step(
                native_machine,
                native_cpu,
                native_state,
                &step_report
            );
            cycle_report.last_step = step_report;
            if (cycle_status != VF2_OK) {
                break;
            }
            if (step_report.recovered_instruction_count == 0u) {
                if (step_report.kind !=
                    VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
                    cycle_status = VF2_ERROR_UNSUPPORTED;
                    break;
                }
                reference_frame_wait = frame_wait_before;
                if (reference_frame_wait.visits_before_interrupt == 0u) {
                    cycle_status = VF2_ERROR_UNSUPPORTED;
                    break;
                }
                {
                    vf2_hybrid_frame_wait_report wait_report;

                    memset(&wait_report, 0, sizeof(wait_report));
                    reference_frame_wait.visits =
                        reference_frame_wait.visits_before_interrupt - 1u;
                    cycle_status = vf2_hybrid_frame_wait_observe(
                        reference_machine,
                        reference_cpu,
                        &reference_frame_wait,
                        &wait_report
                    );
                    if (cycle_status == VF2_OK &&
                        !wait_report.interrupt_injected) {
                        cycle_status = VF2_ERROR_UNSUPPORTED;
                    }
                }
                if (cycle_status != VF2_OK) {
                    break;
                }
            } else {
                cycle_status = advance_reference_block(
                    reference_machine,
                    reference_cpu,
                    &step_report,
                    &frame_wait_before,
                    &reference_frame_wait
                );
                if (cycle_status != VF2_OK) {
                    break;
                }
            }
            cycle_report.reference_instructions_executed +=
                reference_cpu->executed_instructions -
                reference_instruction_start;
            cycle_report.native_recovered_instructions +=
                step_report.recovered_instruction_count;
            if (reference_cpu->executed_instructions -
                    reference_instruction_start !=
                step_report.recovered_instruction_count) {
                cycle_status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
                record_frame_wait_state_mismatch(
                    &cycle_report,
                    &reference_frame_wait,
                    &native_state->frame_wait
                );
                if (!cycle_report.diff.equal) {
                    cycle_status = VF2_ERROR_UNSUPPORTED;
                    break;
                }
            }
            if (reference_cpu->ip != native_cpu->ip) {
                record_initial_ip_mismatch(
                    &cycle_report,
                    reference_cpu->ip,
                    native_cpu->ip
                );
                cycle_status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            ++cycle_report.blocks_compared;
        }

        if (cycle_status == VF2_OK &&
            reference_cpu->ip == repeated_address &&
            native_cpu->ip == repeated_address &&
            cycle_report.blocks_compared >= minimum_blocks_per_cycle) {
            memset(&cycle_report.diff, 0, sizeof(cycle_report.diff));
            cycle_status = compare_complete_state(
                reference_machine,
                reference_cpu,
                native_machine,
                native_cpu,
                &reference_snapshot,
                &native_snapshot,
                &cycle_report.diff
            );
            if (cycle_status == VF2_OK && cycle_report.diff.equal) {
                cycle_report.reached_stop = 1;
            } else if (cycle_status == VF2_OK) {
                cycle_status = VF2_ERROR_UNSUPPORTED;
            }
        } else if (cycle_status == VF2_OK) {
            cycle_status = VF2_ERROR_UNSUPPORTED;
        }

        cycle_report.final_reference_address = reference_cpu->ip;
        cycle_report.final_native_address = native_cpu->ip;
        local_report.blocks_compared += cycle_report.blocks_compared;
        local_report.reference_instructions_executed +=
            cycle_report.reference_instructions_executed;
        local_report.native_recovered_instructions +=
            cycle_report.native_recovered_instructions;
        local_report.last_cycle = cycle_report;

        if (cycle_status == VF2_OK) {
            ++local_report.completed_cycles;
            continue;
        }

        status = vf2_i960_snapshot_restore(
            &cycle_start_snapshot,
            reference_cpu,
            reference_machine
        );
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_restore(
                &cycle_start_snapshot,
                native_cpu,
                native_machine
            );
        }
        if (status == VF2_OK) {
            *native_state = cycle_start_state;
            status = cycle_status;
        }
        break;
    }

    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    if (status == VF2_OK &&
        local_report.completed_cycles == cycle_count) {
        local_report.completed = 1;
    }
    if (report != NULL) {
        *report = local_report;
    }

    vf2_i960_snapshot_destroy(&cycle_start_snapshot);
    vf2_i960_snapshot_destroy(&native_snapshot);
    vf2_i960_snapshot_destroy(&reference_snapshot);
    return status;
}
