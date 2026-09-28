#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/recovered.h"

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static int vf2_test_interrupt_ack_handler(void)
{
    uint8_t *rom = NULL;
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_recovered_interrupt_ack_report report;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;
    bool reference_initialized = false;
    bool native_initialized = false;
    int test_result = 0;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&report, 0, sizeof(report));
    memset(&diff, 0, sizeof(diff));
    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    if (rom == NULL || !vf2_model2a_initialize(&reference_machine)) {
        test_result = 1;
        goto cleanup;
    }
    reference_initialized = true;
    if (!vf2_model2a_initialize(&native_machine)) {
        test_result = 2;
        goto cleanup;
    }
    native_initialized = true;

    /* The model2recomp-guided lift at 0x0d30 is exactly four instructions:
     * load 0xe80000, form -5, acknowledge, and return. */
    write_le32(rom + 0x00000d30u, 0x8c203000u);
    write_le32(rom + 0x00000d38u, 0x59281905u);
    write_le32(rom + 0x00000d3cu, 0x92291000u);
    write_le32(rom + 0x00000d40u, 0x0a000000u);
    if (vf2_model2a_attach_main_rom(
            &reference_machine, rom, VF2_MAIN_ROM_SIZE
        ) != VF2_OK ||
        vf2_model2a_attach_main_rom(
            &native_machine, rom, VF2_MAIN_ROM_SIZE
        ) != VF2_OK) {
        test_result = 3;
        goto cleanup;
    }

    vf2_i960_cpu_reset(&reference_cpu, 0u, 0u, UINT32_C(0x00000040));
    vf2_i960_cpu_reset(&native_cpu, 0u, 0u, UINT32_C(0x00000040));
    reference_cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    native_cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    if (vf2_i960_cpu_enter_procedure(
            &reference_cpu, UINT32_C(0x00000d30), UINT32_C(0x00000040)
        ) != VF2_OK ||
        vf2_i960_cpu_enter_procedure(
            &native_cpu, UINT32_C(0x00000d30), UINT32_C(0x00000040)
        ) != VF2_OK) {
        test_result = 4;
        goto cleanup;
    }

    memset(&options, 0, sizeof(options));
    options.stop_address = UINT32_C(0x00000040);
    options.max_steps = 8u;
    status = vf2_i960_run(&reference_cpu, &reference_machine, &options, &result);
    if (status != VF2_OK || result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        result.executed_instructions != UINT64_C(4) ||
        reference_cpu.executed_instructions != UINT64_C(4) ||
        reference_cpu.procedure_returns != UINT64_C(1) ||
        reference_cpu.ip != UINT32_C(0x00000040) ||
        reference_cpu.local_frame_depth != 0u) {
        test_result = 5;
        goto cleanup;
    }

    status = vf2_recovered_interrupt_ack_dispatch(
        &native_machine, &native_cpu, &report
    );
    if (status != VF2_OK || report.entry_address != UINT32_C(0x00000d30) ||
        report.exit_address != UINT32_C(0x00000040) ||
        report.acknowledge_address != UINT32_C(0x00e80000) ||
        report.acknowledge_value != UINT32_C(0xfffffffb) ||
        report.recovered_instruction_count != UINT64_C(4) ||
        report.recovered_procedure_returns != UINT64_C(1)) {
        test_result = 6;
        goto cleanup;
    }
    status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine, &native_cpu, &native_machine, &diff
    );
    if (status != VF2_OK || !diff.equal) {
        test_result = 7;
        goto cleanup;
    }

cleanup:
    if (native_initialized) {
        vf2_model2a_shutdown(&native_machine);
    }
    if (reference_initialized) {
        vf2_model2a_shutdown(&reference_machine);
    }
    free(rom);
    return test_result;
}

int vf2_test_i960_interrupts(void)
{
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_recovered_timer_irq_report report;
    vf2_hybrid_frame_wait_state wait_state;
    vf2_hybrid_frame_wait_report wait_report;
    vf2_hybrid_bridge_report wait_bridge_report;
    uint32_t request = 0u;
    uint32_t enable = 0u;
    uint32_t timer = 0u;
    uint8_t flag = 0u;
    vf2_status status = VF2_OK;

    if (vf2_test_interrupt_ack_handler() != 0) {
        return 14;
    }
    memset(&report, 0, sizeof(report));
    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    if (rom == NULL || !vf2_model2a_initialize(&machine)) {
        free(rom);
        return 1;
    }
    write_le32(rom + 0x3000u + 20u, 0x005ff000u);
    write_le32(rom + 0x3000u + 24u, 0x005ff500u);
    /* Handler is in main ROM; point vector 14 to a single RET at 0x100. */
    write_le32(rom + 0x100u, 0x0a000000u);
    if (vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) != VF2_OK ||
        vf2_model2a_write_u32(&machine, 0x005ff000u + 36u + 24u, 0x100u) != VF2_OK ||
        vf2_model2a_write_u32(&machine, 0x005ff410u + 20u, 0x005ff000u) != VF2_OK ||
        vf2_model2a_write_u32(&machine, 0x005ff410u + 24u, 0x005ff500u) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 2;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0x005ff410u, 0x40u);
    cpu.registers[1] = 0x00501000u;
    cpu.registers[31] = 0x00500000u;
    status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 14u, 1u);
    if (status != VF2_OK || cpu.ip != 0x100u || cpu.interrupt_entries != 1u ||
        cpu.local_frame_depth != 1u) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 3;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = 0x40u;
    options.max_steps = 4u;
    options.stop_on_self_branch = true;
    status = vf2_i960_run(&cpu, &machine, &options, &result);
    if (status != VF2_OK || result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu.interrupt_returns != 1u || cpu.local_frame_depth != 0u ||
        cpu.ip != 0x40u) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 4;
    }

    status = vf2_model2a_set_interrupt_enable(&machine, 0x421u);
    if (status == VF2_OK) {
        status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1) << 5u);
    }
    if (status == VF2_OK) {
        status = vf2_recovered_timer_irq_dispatch(&machine, &report);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_get_interrupt_state(&machine, &request, &enable);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(&machine, VF2_TIMER_BASE + 12u, &timer);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(&machine, 0x0050008cu, &flag, 1u);
    }
    if (status != VF2_OK || request != 0u || enable != 0x421u ||
        timer != 0x000fffffu || flag != 1u || report.serviced_mask != 0x20u ||
        report.timer_index != 3u || report.interrupts_serviced != 1u ||
        !report.wait_released) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 5;
    }

    /* The host-side frame wait state machine injects vector 12 after the
     * fourth observed visit and leaves earlier visits untouched. */
    if (vf2_model2a_write_u32(
            &machine, 0x005ff000u + 36u + 16u, 0x100u
        ) != VF2_OK ||
        vf2_hybrid_frame_wait_initialize(&wait_state, 4u) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 6;
    }
    cpu.ip = UINT32_C(0x00010f98);
    memset(&wait_report, 0, sizeof(wait_report));
    for (unsigned visit = 0u; visit < 3u; ++visit) {
        if (vf2_hybrid_frame_wait_observe(
                &machine, &cpu, &wait_state, &wait_report
            ) != VF2_OK ||
            !wait_report.wait_observed || wait_report.interrupt_injected ||
            cpu.ip != UINT32_C(0x00010f98)) {
            vf2_model2a_shutdown(&machine);
            free(rom);
            return 7;
        }
    }
    if (vf2_hybrid_frame_wait_observe(
            &machine, &cpu, &wait_state, &wait_report
        ) != VF2_OK ||
        !wait_report.interrupt_injected ||
        wait_report.interrupt_vector != 12u || cpu.ip != 0x100u ||
        wait_state.visits != 0u || wait_state.interrupts_injected != 1u) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 8;
    }

    /* The recovered polling phases preserve the same four-visit interrupt
     * injection and the changed-frame-byte exit after interrupt return. */
    vf2_i960_cpu_reset(
        &cpu, 0u, UINT32_C(0x005ff410), UINT32_C(0x00010f90)
    );
    cpu.registers[1] = UINT32_C(0x00501000);
    cpu.registers[31] = UINT32_C(0x00500000);
    flag = 0u;
    if (vf2_model2a_write(
            &machine, UINT32_C(0x00500000), &flag, sizeof(flag)
        ) != VF2_OK ||
        vf2_hybrid_frame_wait_initialize(&wait_state, 4u) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 9;
    }
    memset(&wait_bridge_report, 0, sizeof(wait_bridge_report));
    if (vf2_hybrid_frame_wait_execute(
            &machine, &cpu, &wait_state, &wait_bridge_report
        ) != VF2_OK ||
        wait_bridge_report.kind != VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL ||
        wait_bridge_report.entry_address != UINT32_C(0x00010f90) ||
        wait_bridge_report.exit_address != UINT32_C(0x00000100) ||
        wait_bridge_report.recovered_instruction_count != UINT64_C(7) ||
        wait_bridge_report.recovered_procedure_calls != UINT64_C(1) ||
        wait_bridge_report.recovered_procedure_returns != UINT64_C(0) ||
        cpu.ip != UINT32_C(0x00000100) || cpu.local_frame_depth != 1u ||
        cpu.executed_instructions != UINT64_C(7) ||
        cpu.interrupt_entries != UINT64_C(1) ||
        wait_state.visits != 0u || wait_state.interrupts_injected != 1u) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 10;
    }

    flag = 1u;
    cpu.ip = UINT32_C(0x00000d20);
    if (vf2_model2a_write(
            &machine, UINT32_C(0x00500000), &flag, sizeof(flag)
        ) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 11;
    }
    memset(&wait_bridge_report, 0, sizeof(wait_bridge_report));
    if (vf2_hybrid_frame_wait_execute(
            &machine, &cpu, &wait_state, &wait_bridge_report
        ) != VF2_OK ||
        wait_bridge_report.kind !=
            VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT ||
        wait_bridge_report.entry_address != UINT32_C(0x00000d20) ||
        wait_bridge_report.exit_address != UINT32_C(0x00010fa4) ||
        wait_bridge_report.recovered_instruction_count != UINT64_C(3) ||
        wait_bridge_report.recovered_procedure_calls != UINT64_C(0) ||
        wait_bridge_report.recovered_procedure_returns != UINT64_C(1) ||
        cpu.ip != UINT32_C(0x00010fa4) || cpu.local_frame_depth != 0u ||
        cpu.executed_instructions != UINT64_C(10) ||
        cpu.interrupt_returns != UINT64_C(1) ||
        cpu.registers[3] != UINT32_C(1) ||
        cpu.registers[VF2_I960_G0_REGISTER] != UINT32_C(0) ||
        wait_state.visits != 1u || wait_state.interrupts_injected != 1u) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 12;
    }

    /* Request-register writes acknowledge by preserving only set mask bits. */
    if (vf2_model2a_raise_interrupt(&machine, 0x23u) != VF2_OK ||
        vf2_model2a_write_u32(&machine, VF2_INTERRUPT_CONTROL_BASE, ~0x20u) != VF2_OK ||
        vf2_model2a_get_interrupt_state(&machine, &request, &enable) != VF2_OK ||
        request != 0x03u) {
        vf2_model2a_shutdown(&machine);
        free(rom);
        return 13;
    }

    vf2_model2a_shutdown(&machine);
    free(rom);
    return 0;
}
