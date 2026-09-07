#include "vf2/hybrid.h"

#include <string.h>

#define VF2_FRAME_WAIT_EARLY UINT32_C(0x00000f7c)
#define VF2_FRAME_WAIT_MAIN UINT32_C(0x00010f98)
#define VF2_TEXTURE_INIT_WAIT_POLL UINT32_C(0x0004afe4)
#define VF2_FRAME_INTERRUPT_MASK UINT32_C(1)
#define VF2_FRAME_INTERRUPT_VECTOR UINT32_C(12)
#define VF2_FRAME_INTERRUPT_LEVEL UINT32_C(1)

vf2_status vf2_hybrid_frame_wait_initialize(
    vf2_hybrid_frame_wait_state *state,
    size_t visits_before_interrupt
)
{
    if (state == NULL || visits_before_interrupt == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    state->visits_before_interrupt = visits_before_interrupt;
    return VF2_OK;
}

vf2_status vf2_hybrid_frame_wait_observe(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *state,
    vf2_hybrid_frame_wait_report *report
)
{
    vf2_hybrid_frame_wait_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL ||
        state->visits_before_interrupt == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    if (cpu->ip != VF2_FRAME_WAIT_EARLY &&
        cpu->ip != VF2_FRAME_WAIT_MAIN &&
        cpu->ip != VF2_TEXTURE_INIT_WAIT_POLL) {
        if (report != NULL) {
            *report = local_report;
        }
        return VF2_OK;
    }

    local_report.wait_address = cpu->ip;
    local_report.wait_observed = 1;
    ++state->visits;
    local_report.visit_count = state->visits;
    if (state->visits >= state->visits_before_interrupt) {
        if (cpu->ip == VF2_FRAME_WAIT_MAIN) {
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
            cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        }
        status = vf2_model2a_raise_interrupt(
            machine, VF2_FRAME_INTERRUPT_MASK
        );
        if (status == VF2_OK) {
            status = vf2_i960_cpu_enter_interrupt(
                cpu,
                machine,
                VF2_FRAME_INTERRUPT_VECTOR,
                VF2_FRAME_INTERRUPT_LEVEL
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        state->visits = 0u;
        ++state->interrupts_injected;
        local_report.interrupt_vector = VF2_FRAME_INTERRUPT_VECTOR;
        local_report.interrupt_handler = cpu->ip;
        local_report.interrupt_injected = 1;
    }
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_hybrid_frame_wait_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *state,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    const uint64_t start_instructions =
        cpu != NULL ? cpu->executed_instructions : 0u;
    const uint64_t start_calls = cpu != NULL ? cpu->procedure_calls : 0u;
    const uint64_t start_returns = cpu != NULL ? cpu->procedure_returns : 0u;
    vf2_hybrid_bridge_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL ||
        state->visits_before_interrupt == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));

    if (entry == VF2_FRAME_WAIT_EARLY) {
        for (;;) {
            uint8_t frame_byte = 0u;
            vf2_hybrid_frame_wait_report wait_report;
            int repeats = 0;

            status = vf2_model2a_read(
                machine, UINT32_C(0x00500000),
                &frame_byte, sizeof(frame_byte)
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[4] = frame_byte;
            cpu->executed_instructions += UINT64_C(2);
            if (UINT8_C(2) < frame_byte) {
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
                cpu->compare_result = VF2_I960_COMPARE_LESS;
            } else if (UINT8_C(2) > frame_byte) {
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
                cpu->compare_result = VF2_I960_COMPARE_GREATER;
            } else {
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
                cpu->compare_result = VF2_I960_COMPARE_EQUAL;
            }
            repeats = frame_byte < UINT8_C(2);
            if (!repeats) {
                ++cpu->executed_instructions;
                repeats = (frame_byte & UINT8_C(1)) != 0u;
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) |
                    (repeats ? UINT32_C(2) : UINT32_C(0));
                cpu->compare_result = repeats
                    ? VF2_I960_COMPARE_EQUAL
                    : VF2_I960_COMPARE_NONE;
            }
            if (!repeats) {
                const uint8_t zero = 0u;
                cpu->registers[3] = 0u;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00500000), &zero, sizeof(zero)
                );
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_procedure(
                        cpu, UINT32_C(0x00002ec4), UINT32_C(0x00000f9c)
                    );
                }
                if (status != VF2_OK) {
                    return status;
                }
                cpu->executed_instructions += UINT64_C(3);
                break;
            }

            cpu->ip = VF2_FRAME_WAIT_EARLY;
            memset(&wait_report, 0, sizeof(wait_report));
            status = vf2_hybrid_frame_wait_observe(
                machine, cpu, state, &wait_report
            );
            if (status != VF2_OK) {
                return status;
            }
            if (wait_report.interrupt_injected) {
                break;
            }
        }
        local_report.kind = VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL;
    } else if (entry == UINT32_C(0x00010f90)) {
        uint8_t frame_byte = 0u;
        vf2_hybrid_frame_wait_report wait_report;
        size_t phase_seed = 0u;

        /* Native snapshots serialize CPU/machine state but not the host-side
         * frame scheduler phase.  When a mid-run snapshot is restored, the
         * balanced nonzero IRQ counters prove that at least one complete
         * frame interrupt has already occurred.  The measured gameplay
         * corridor resumes one poll into the four-visit VBlank period. */
        if (state->visits == 0u && state->interrupts_injected == 0u &&
            cpu->interrupt_entries != 0u &&
            cpu->interrupt_entries == cpu->interrupt_returns) {
            phase_seed = 1u;
            state->visits = phase_seed;
        }

        status = vf2_model2a_read(
            machine, UINT32_C(0x00500000), &frame_byte, sizeof(frame_byte)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;
        cpu->ip = VF2_FRAME_WAIT_MAIN;
        ++cpu->executed_instructions;

        for (;;) {
            memset(&wait_report, 0, sizeof(wait_report));
            status = vf2_hybrid_frame_wait_observe(
                machine, cpu, state, &wait_report
            );
            if (status != VF2_OK) {
                return status;
            }
            if (wait_report.interrupt_injected) {
                break;
            }

            status = vf2_model2a_read(
                machine,
                UINT32_C(0x00500000),
                &frame_byte,
                sizeof(frame_byte)
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = frame_byte;
            cpu->ip = UINT32_C(0x00010fa0);
            ++cpu->executed_instructions;

            if (cpu->registers[3] !=
                cpu->registers[VF2_I960_G0_REGISTER]) {
                return VF2_ERROR_UNSUPPORTED;
            }
            cpu->ip = VF2_FRAME_WAIT_MAIN;
            ++cpu->executed_instructions;
        }

        local_report.kind = VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL;
        local_report.iterations =
            state->visits_before_interrupt - phase_seed;
    } else if (entry == UINT32_C(0x00000d20)) {
        uint8_t frame_byte = 0u;
        vf2_hybrid_frame_wait_report wait_report;

        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK) {
            return status;
        }
        ++cpu->executed_instructions;

        if (cpu->ip == VF2_TEXTURE_INIT_WAIT_POLL ||
            cpu->ip == VF2_FRAME_WAIT_EARLY) {
            memset(&wait_report, 0, sizeof(wait_report));
            status = vf2_hybrid_frame_wait_observe(
                machine, cpu, state, &wait_report
            );
            if (status != VF2_OK || wait_report.interrupt_injected) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
            local_report.kind =
                VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT;
            local_report.iterations = UINT64_C(1);
        } else if (cpu->ip != VF2_FRAME_WAIT_MAIN) {
            return VF2_ERROR_UNSUPPORTED;
        } else {
            memset(&wait_report, 0, sizeof(wait_report));
            status = vf2_hybrid_frame_wait_observe(
                machine, cpu, state, &wait_report
            );
            if (status != VF2_OK || wait_report.interrupt_injected) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }

            status = vf2_model2a_read(
                machine, UINT32_C(0x00500000), &frame_byte, sizeof(frame_byte)
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[3] = frame_byte;
            cpu->ip = UINT32_C(0x00010fa0);
            ++cpu->executed_instructions;

            if (cpu->registers[3] ==
                cpu->registers[VF2_I960_G0_REGISTER]) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if ((int32_t)cpu->registers[3] <
                (int32_t)cpu->registers[VF2_I960_G0_REGISTER]) {
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
                cpu->compare_result = VF2_I960_COMPARE_LESS;
            } else {
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
                cpu->compare_result = VF2_I960_COMPARE_GREATER;
            }
            cpu->ip = UINT32_C(0x00010fa4);
            ++cpu->executed_instructions;

            local_report.kind =
                VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT;
            local_report.iterations = UINT64_C(1);
        }
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }

    local_report.entry_address = entry;
    local_report.exit_address = cpu->ip;
    local_report.recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    local_report.recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    local_report.recovered_procedure_returns =
        cpu->procedure_returns - start_returns;
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
