#include "vf2/hybrid.h"

#include <stdbool.h>
#include <stdint.h>

#define VF2_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_GAME_INFO_STATE_OFFSET UINT32_C(0x000001a4)
#define VF2_GAME_INFO_STATE_BYTE_OFFSET UINT32_C(0x00000a00)
#define VF2_GAME_INFO_FIGHTER0_SLOT UINT32_C(0x00500804)
#define VF2_GAME_INFO_FIGHTER1_SLOT UINT32_C(0x00500808)
#define VF2_GAME_INFO_COUNTDOWN UINT32_C(0x0050a0b6)
#define VF2_GAME_INFO_THRESHOLD UINT32_C(0x0050a028)
#define VF2_GAME_INFO_MASK UINT32_C(0x00208000)

vf2_status vf2_hybrid_first_dispatch_task_execute_base(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
);

static void set_compare_result(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

static bool measured_fighter0_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint8_t *countdown
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_base_flags = 0u;
    uint32_t fighter1_base_flags = 0u;
    uint32_t fighter0_state_flags = 0u;
    uint32_t fighter1_state_flags = 0u;
    uint32_t threshold = 0u;
    uint8_t fighter0_state = 0u;
    uint8_t fighter1_state = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || countdown == NULL ||
        cpu->ip != VF2_GAME_INFO_ENTRY) {
        return false;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_INFO_FIGHTER0_SLOT, &fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_INFO_FIGHTER1_SLOT, &fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_base_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_base_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            fighter0 + VF2_GAME_INFO_STATE_OFFSET,
            &fighter0_state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            fighter1 + VF2_GAME_INFO_STATE_OFFSET,
            &fighter1_state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            fighter0 + VF2_GAME_INFO_STATE_BYTE_OFFSET,
            &fighter0_state,
            sizeof(fighter0_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            fighter1 + VF2_GAME_INFO_STATE_BYTE_OFFSET,
            &fighter1_state,
            sizeof(fighter1_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_INFO_COUNTDOWN,
            countdown,
            sizeof(*countdown)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_INFO_THRESHOLD, &threshold
        );
    }

    return status == VF2_OK &&
           fighter0_state == UINT8_C(8) &&
           fighter1_state == UINT8_C(8) &&
           fighter0_state_flags == VF2_GAME_INFO_MASK &&
           fighter1_state_flags == 0u &&
           threshold <= UINT32_C(2) &&
           (*countdown == UINT8_C(0) || *countdown == UINT8_C(1)) &&
           (fighter0_base_flags & UINT32_C(0x80000000)) != 0u &&
           (fighter1_base_flags & UINT32_C(0x80000000)) != 0u;
}

vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    uint8_t countdown = 0u;
    const bool apply_measured_poststate =
        measured_fighter0_case(machine, cpu, &countdown);
    vf2_status status = vf2_hybrid_first_dispatch_task_execute_base(
        machine, cpu, registry_address, report
    );

    if (status != VF2_OK || !apply_measured_poststate) {
        return status;
    }

    if (countdown == 0u) {
        if (cpu->executed_instructions == 0u ||
            (report != NULL && report->recovered_instruction_count == 0u)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        --cpu->executed_instructions;
        if (report != NULL) {
            --report->recovered_instruction_count;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else {
        cpu->executed_instructions += UINT64_C(3);
        if (report != NULL) {
            report->recovered_instruction_count += UINT64_C(3);
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    }

    return VF2_OK;
}
