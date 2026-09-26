#include "vf2/recovered.h"

#include <string.h>

#define VF2_TIMER3_IRQ_MASK UINT32_C(0x00000020)
#define VF2_TIMER3_INDEX UINT32_C(3)
#define VF2_TIMER_RELOAD UINT32_C(0x000fffff)
#define VF2_RUNTIME_WAIT_FLAG UINT32_C(0x0050008c)
#define VF2_RUNTIME_TIMER_ENABLE UINT32_C(0x00000421)
#define VF2_INTERRUPT_ACK_ENTRY UINT32_C(0x00000d30)
#define VF2_INTERRUPT_ACK_EXIT UINT32_C(0x00000040)
#define VF2_INTERRUPT_ACK_ADDRESS UINT32_C(0x00e80000)
#define VF2_INTERRUPT_ACK_VALUE UINT32_C(0xfffffffb)

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

vf2_status vf2_recovered_timer_irq_dispatch(
    vf2_model2a *machine,
    vf2_recovered_timer_irq_report *report
)
{
    vf2_recovered_timer_irq_report local_report;
    uint32_t request = 0u;
    uint32_t enable = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    status = vf2_model2a_get_interrupt_state(machine, &request, &enable);
    if (status != VF2_OK) {
        return status;
    }
    local_report.initial_request = request;
    local_report.initial_enable = enable;
    local_report.wait_flag_address = VF2_RUNTIME_WAIT_FLAG;

    /* This accepted recovery intentionally covers the proven VF2 runtime path:
     * timer 3 is enabled, request bit 5 is pending, and no other timer source is
     * selected by the handler's local scan. Broader interrupt combinations are
     * left unsupported until separately captured and compared. */
    if ((request & VF2_TIMER3_IRQ_MASK) == 0u ||
        enable != VF2_RUNTIME_TIMER_ENABLE) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_set_interrupt_enable(
        machine, enable & ~VF2_TIMER3_IRQ_MASK
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TIMER_BASE + VF2_TIMER3_INDEX * 4u, 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_set_interrupt_enable(machine, enable);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_TIMER_BASE + VF2_TIMER3_INDEX * 4u,
            VF2_TIMER_RELOAD
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_INTERRUPT_CONTROL_BASE, ~VF2_TIMER3_IRQ_MASK
        );
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_RUNTIME_WAIT_FLAG, 1u);
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.serviced_mask = VF2_TIMER3_IRQ_MASK;
    local_report.timer_index = VF2_TIMER3_INDEX;
    local_report.timer_reload = VF2_TIMER_RELOAD;
    local_report.interrupts_serviced = 1u;
    local_report.wait_released = 1;
    status = vf2_model2a_get_interrupt_state(
        machine, &local_report.final_request, &local_report.final_enable
    );
    if (status == VF2_OK && report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_recovered_interrupt_ack_dispatch(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_interrupt_ack_report *report
)
{
    vf2_i960_cpu candidate;
    vf2_recovered_interrupt_ack_report local_report;
    vf2_status status = VF2_OK;
    const uint64_t start_returns = cpu != NULL ? cpu->procedure_returns : 0u;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_INTERRUPT_ACK_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* 0xd30 is exactly: lda 0xe80000,r4; subo 5,0,r5; st r5,(r4); ret.
     * Apply the architectural post-state to a candidate CPU so an invalid
     * return frame cannot leave a partially advanced CPU behind. */
    candidate = *cpu;
    candidate.registers[4] = VF2_INTERRUPT_ACK_ADDRESS;
    candidate.registers[5] = VF2_INTERRUPT_ACK_VALUE;
    status = vf2_model2a_write_u32(
        machine, VF2_INTERRUPT_ACK_ADDRESS, VF2_INTERRUPT_ACK_VALUE
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(&candidate, machine);
    }
    if (status != VF2_OK || candidate.ip != VF2_INTERRUPT_ACK_EXIT) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    candidate.executed_instructions += UINT64_C(4);
    *cpu = candidate;

    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = VF2_INTERRUPT_ACK_ENTRY;
    local_report.exit_address = candidate.ip;
    local_report.acknowledge_address = VF2_INTERRUPT_ACK_ADDRESS;
    local_report.acknowledge_value = VF2_INTERRUPT_ACK_VALUE;
    local_report.recovered_instruction_count = UINT64_C(4);
    local_report.recovered_procedure_returns =
        candidate.procedure_returns - start_returns;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
