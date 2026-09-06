#include "vf2/hybrid.h"

#include <stdbool.h>
#include <stdint.h>

#define VF2_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_GAME_INFO_STATE_OFFSET UINT32_C(0x000001a4)
#define VF2_GAME_INFO_STATE_BYTE_OFFSET UINT32_C(0x00000a00)
#define VF2_GAME_INFO_STATE_CHILD_BIT UINT32_C(0x00000800)
#define VF2_GAME_INFO_LEGACY_FIELD_OFFSET UINT32_C(0x00000b24)
#define VF2_GAME_INFO_LEGACY_FIELD_BIT UINT32_C(0x00008000)
#define VF2_GAME_INFO_FIGHTER0_SLOT UINT32_C(0x00500804)
#define VF2_GAME_INFO_FIGHTER1_SLOT UINT32_C(0x00500808)
#define VF2_GAME_INFO_COUNTDOWN UINT32_C(0x0050a0b6)
#define VF2_GAME_INFO_THRESHOLD UINT32_C(0x0050a028)
#define VF2_GAME_INFO_MASK_14_21 UINT32_C(0x00204000)
#define VF2_GAME_INFO_MASK_15_21 UINT32_C(0x00208000)
#define VF2_GAME_INFO_MASK_16_21 UINT32_C(0x00210000)
#define VF2_GAME_INFO_MASK_15_16_21 UINT32_C(0x00218000)

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

static vf2_status set_state_child_bit(
    vf2_model2a *machine,
    uint32_t fighter
)
{
    uint32_t value = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, fighter + VF2_GAME_INFO_STATE_OFFSET, &value
    );

    if (status == VF2_OK) {
        value |= VF2_GAME_INFO_STATE_CHILD_BIT;
        status = vf2_model2a_write_u32(
            machine, fighter + VF2_GAME_INFO_STATE_OFFSET, value
        );
    }
    return status;
}

static vf2_status clear_legacy_field_bit(
    vf2_model2a *machine,
    uint32_t fighter
)
{
    uint32_t value = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, fighter + VF2_GAME_INFO_LEGACY_FIELD_OFFSET, &value
    );

    if (status == VF2_OK) {
        value &= ~VF2_GAME_INFO_LEGACY_FIELD_BIT;
        status = vf2_model2a_write_u32(
            machine, fighter + VF2_GAME_INFO_LEGACY_FIELD_OFFSET, value
        );
    }
    return status;
}

static vf2_status correct_bit16_fighter_poststate(
    vf2_model2a *machine,
    uint32_t fighter
)
{
    vf2_status status = set_state_child_bit(machine, fighter);

    if (status == VF2_OK) {
        status = clear_legacy_field_bit(machine, fighter);
    }
    return status;
}

static bool measured_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint8_t *countdown,
    uint32_t *mask,
    uint32_t *fighter0,
    uint32_t *fighter1,
    bool *fighter0_only,
    bool *bilateral
)
{
    uint32_t fighter0_base_flags = 0u;
    uint32_t fighter1_base_flags = 0u;
    uint32_t fighter0_state_flags = 0u;
    uint32_t fighter1_state_flags = 0u;
    uint32_t threshold = 0u;
    uint32_t combined = 0u;
    uint8_t fighter0_state = 0u;
    uint8_t fighter1_state = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || countdown == NULL || mask == NULL ||
        fighter0 == NULL || fighter1 == NULL || fighter0_only == NULL ||
        bilateral == NULL || cpu->ip != VF2_GAME_INFO_ENTRY) {
        return false;
    }

    *mask = 0u;
    *fighter0 = 0u;
    *fighter1 = 0u;
    *fighter0_only = false;
    *bilateral = false;
    status = vf2_model2a_read_u32(
        machine, VF2_GAME_INFO_FIGHTER0_SLOT, fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_INFO_FIGHTER1_SLOT, fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, *fighter0, &fighter0_base_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, *fighter1, &fighter1_base_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            *fighter0 + VF2_GAME_INFO_STATE_OFFSET,
            &fighter0_state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            *fighter1 + VF2_GAME_INFO_STATE_OFFSET,
            &fighter1_state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            *fighter0 + VF2_GAME_INFO_STATE_BYTE_OFFSET,
            &fighter0_state,
            sizeof(fighter0_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            *fighter1 + VF2_GAME_INFO_STATE_BYTE_OFFSET,
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
    combined = fighter0_state_flags | fighter1_state_flags;
    if (status != VF2_OK ||
        fighter0_state != UINT8_C(8) || fighter1_state != UINT8_C(8) ||
        (combined != VF2_GAME_INFO_MASK_14_21 &&
         combined != VF2_GAME_INFO_MASK_15_21 &&
         combined != VF2_GAME_INFO_MASK_16_21 &&
         combined != VF2_GAME_INFO_MASK_15_16_21) ||
        threshold > UINT32_C(2) ||
        (*countdown != UINT8_C(0) && *countdown != UINT8_C(1)) ||
        (fighter0_base_flags & UINT32_C(0x80000000)) == 0u ||
        (fighter1_base_flags & UINT32_C(0x80000000)) == 0u ||
        (fighter0_state_flags != 0u && fighter0_state_flags != combined) ||
        (fighter1_state_flags != 0u && fighter1_state_flags != combined)) {
        return false;
    }

    *mask = combined;
    *fighter0_only =
        fighter0_state_flags == combined && fighter1_state_flags == 0u;
    *bilateral =
        fighter0_state_flags == combined && fighter1_state_flags == combined;
    return true;
}

vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    uint8_t countdown = 0u;
    uint32_t mask = 0u;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    bool fighter0_only = false;
    bool bilateral = false;
    const bool apply_measured_poststate = measured_case(
        machine,
        cpu,
        &countdown,
        &mask,
        &fighter0,
        &fighter1,
        &fighter0_only,
        &bilateral
    );
    vf2_status status = vf2_hybrid_first_dispatch_task_execute_base(
        machine, cpu, registry_address, report
    );

    if (status != VF2_OK || !apply_measured_poststate) {
        return status;
    }

    if (mask == VF2_GAME_INFO_MASK_16_21 ||
        mask == VF2_GAME_INFO_MASK_15_16_21) {
        if (fighter0_only || bilateral) {
            status = correct_bit16_fighter_poststate(machine, fighter0);
        }
        if (status == VF2_OK && !fighter0_only) {
            status = correct_bit16_fighter_poststate(machine, fighter1);
        }
        if (status != VF2_OK) {
            return status;
        }
        if (fighter0_only && countdown == 0u) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        } else if (countdown != 0u) {
            set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        }
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_MASK_14_21) {
        if (fighter0_only && countdown == 0u) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        } else if (countdown != 0u) {
            set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        }
        return VF2_OK;
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
        if (fighter0_only) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    } else {
        const uint64_t correction = bilateral ? UINT64_C(7) : UINT64_C(3);
        cpu->executed_instructions += correction;
        if (report != NULL) {
            report->recovered_instruction_count += correction;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    }

    return VF2_OK;
}
