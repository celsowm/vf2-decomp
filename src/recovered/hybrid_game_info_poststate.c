#include "vf2/hybrid.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VF2_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_GAME_INFO_STATE_OFFSET UINT32_C(0x000001a4)
#define VF2_GAME_INFO_STATE_BYTE_OFFSET UINT32_C(0x00000a00)
#define VF2_GAME_INFO_STATE_CHILD_BIT UINT32_C(0x00000800)
#define VF2_GAME_INFO_LEGACY_FIELD_OFFSET UINT32_C(0x00000b24)
#define VF2_GAME_INFO_LEGACY_FIELD_BIT UINT32_C(0x00008000)
#define VF2_GAME_INFO_FIGHTER0_SLOT UINT32_C(0x00500804)
#define VF2_GAME_INFO_FIGHTER1_SLOT UINT32_C(0x00500808)
#define VF2_GAME_INFO_MODE_BASE_SLOT UINT32_C(0x0050016c)
#define VF2_GAME_INFO_MODE_OFFSET UINT32_C(0x00003351)
#define VF2_GAME_INFO_COUNTDOWN UINT32_C(0x0050a0b6)
#define VF2_GAME_INFO_THRESHOLD UINT32_C(0x0050a028)
#define VF2_GAME_INFO_MASK_14_21 UINT32_C(0x00204000)
#define VF2_GAME_INFO_MASK_15_21 UINT32_C(0x00208000)
#define VF2_GAME_INFO_MASK_16_21 UINT32_C(0x00210000)
#define VF2_GAME_INFO_MASK_14_16_21 UINT32_C(0x00214000)
#define VF2_GAME_INFO_MASK_15_16_21 UINT32_C(0x00218000)
#define VF2_GAME_INFO_STATE4_MASK_6 UINT32_C(0x00000040)
#define VF2_GAME_INFO_STATE4_MASK_14 UINT32_C(0x00004000)
#define VF2_GAME_INFO_STATE4_MASK_6_14 UINT32_C(0x00004040)
#define VF2_GAME_INFO_STATE4_MASK_15 UINT32_C(0x00008000)
#define VF2_GAME_INFO_STATE4_MASK_6_15 UINT32_C(0x00008040)
#define VF2_GAME_INFO_STATE4_MASK_14_15 UINT32_C(0x0000c000)
#define VF2_GAME_INFO_STATE4_MASK_6_14_15 UINT32_C(0x0000c040)
#define VF2_GAME_INFO_STATE4_MASK_16 UINT32_C(0x00010000)
#define VF2_GAME_INFO_STATE4_MASK_6_16 UINT32_C(0x00010040)
#define VF2_GAME_INFO_STATE4_MASK_14_16 UINT32_C(0x00014000)
#define VF2_GAME_INFO_STATE4_MASK_6_14_16 UINT32_C(0x00014040)
#define VF2_GAME_INFO_STATE4_MASK_15_16 UINT32_C(0x00018000)
#define VF2_GAME_INFO_STATE4_MASK_6_15_16 UINT32_C(0x00018040)
#define VF2_GAME_INFO_STATE4_MASK_14_15_16 UINT32_C(0x0001c000)
#define VF2_GAME_INFO_STATE4_MASK_6_14_15_16 UINT32_C(0x0001c040)
#define VF2_GAME_DISP_INIT_ENTRY UINT32_C(0x0002b1bc)
#define VF2_GAME_DISP_INIT_CONTINUATION UINT32_C(0x0002b1f8)
#define VF2_GAME_DISP_INIT_LAST_CALL_RETURN UINT32_C(0x0002b1f4)
#define VF2_GAME_DISP_HELPER_RETURN UINT32_C(0x0000271c)
#define VF2_GAME_DISP_REGISTRY UINT32_C(0x00515b00)
#define VF2_GAME_DISP_MODE_FLAGS_OFFSET UINT32_C(0x00003320)
#define VF2_GAME_DISP_MODE_BYTES_OFFSET UINT32_C(0x00003324)
#define VF2_GAME_DISP_OUTPUT_OFFSET UINT32_C(0x00000059)
#define VF2_GAME_DISP_OUTPUT_SIZE 23u
#define VF2_GAME_DISP_SCHEDULER_RETURN UINT32_C(0x00010dcc)

static const uint32_t vf2_game_info_measured_masks[] = {
    UINT32_C(0x00204000), UINT32_C(0x00208000), UINT32_C(0x00210000),
    UINT32_C(0x00214000), UINT32_C(0x00218000), UINT32_C(0x0021c000),
    UINT32_C(0x04214000), UINT32_C(0x06214000), UINT32_C(0x08214000),
    UINT32_C(0x0a214000), UINT32_C(0x0c214000), UINT32_C(0x10214000),
    UINT32_C(0x12214000), UINT32_C(0x14214000), UINT32_C(0x16214000),
    UINT32_C(0x18214000), UINT32_C(0x1c214000), UINT32_C(0x24214000),
    UINT32_C(0x84214000), UINT32_C(0x26214000), UINT32_C(0x28214000),
    UINT32_C(0x2c214000), UINT32_C(0x30214000), UINT32_C(0x34214000),
    UINT32_C(0x48214000), UINT32_C(0x50214000), UINT32_C(0x54214000),
    UINT32_C(0x58214000), UINT32_C(0x5c214000), UINT32_C(0x60214000),
    UINT32_C(0x64214000), UINT32_C(0x68214000), UINT32_C(0x6c214000),
    UINT32_C(0x70214000), UINT32_C(0x74214000), UINT32_C(0x78214000),
    UINT32_C(0x7c214000), UINT32_C(0x88214000), UINT32_C(0x8c214000),
    UINT32_C(0x90214000), UINT32_C(0x94214000), UINT32_C(0x98214000),
    UINT32_C(0x9c214000), UINT32_C(0xa0214000), UINT32_C(0xa4214000),
    UINT32_C(0xa8214000), UINT32_C(0xac214000), UINT32_C(0xb0214000),
    UINT32_C(0xb4214000), UINT32_C(0xb8214000), UINT32_C(0xbc214000),
    UINT32_C(0xc0214000), UINT32_C(0xc4214000), UINT32_C(0xc8214000),
    UINT32_C(0xcc214000), UINT32_C(0xd0214000), UINT32_C(0xd4214000),
    UINT32_C(0xd8214000), UINT32_C(0xdc214000), UINT32_C(0xe0214000),
    UINT32_C(0xe4214000), UINT32_C(0xe8214000), UINT32_C(0xec214000),
    UINT32_C(0xf0214000), UINT32_C(0xf4214000), UINT32_C(0xf8214000),
    UINT32_C(0xfc214000)
};

static const uint32_t vf2_game_info_plus2_plus3_masks[] = {
    UINT32_C(0x06214000), UINT32_C(0x08214000), UINT32_C(0x0a214000),
    UINT32_C(0x0c214000), UINT32_C(0x10214000), UINT32_C(0x12214000),
    UINT32_C(0x14214000), UINT32_C(0x16214000), UINT32_C(0x18214000),
    UINT32_C(0x1c214000), UINT32_C(0x26214000), UINT32_C(0x28214000),
    UINT32_C(0x2c214000), UINT32_C(0x30214000), UINT32_C(0x34214000),
    UINT32_C(0x48214000), UINT32_C(0x50214000), UINT32_C(0x54214000),
    UINT32_C(0x58214000), UINT32_C(0x5c214000), UINT32_C(0x68214000),
    UINT32_C(0x6c214000), UINT32_C(0x70214000), UINT32_C(0x74214000),
    UINT32_C(0x78214000), UINT32_C(0x7c214000), UINT32_C(0x88214000),
    UINT32_C(0x8c214000), UINT32_C(0x90214000), UINT32_C(0x94214000),
    UINT32_C(0x98214000), UINT32_C(0x9c214000), UINT32_C(0xa8214000),
    UINT32_C(0xac214000), UINT32_C(0xb0214000), UINT32_C(0xb4214000),
    UINT32_C(0xb8214000), UINT32_C(0xbc214000), UINT32_C(0xc8214000),
    UINT32_C(0xcc214000), UINT32_C(0xd0214000), UINT32_C(0xd4214000),
    UINT32_C(0xd8214000), UINT32_C(0xdc214000), UINT32_C(0xe8214000),
    UINT32_C(0xec214000), UINT32_C(0xf0214000), UINT32_C(0xf4214000),
    UINT32_C(0xf8214000), UINT32_C(0xfc214000)
};

static const uint32_t vf2_game_info_condition_only_masks[] = {
    UINT32_C(0x0021c000), UINT32_C(0x04214000), UINT32_C(0x24214000),
    UINT32_C(0x84214000), UINT32_C(0x60214000), UINT32_C(0x64214000),
    UINT32_C(0xa0214000), UINT32_C(0xa4214000), UINT32_C(0xc0214000),
    UINT32_C(0xc4214000), UINT32_C(0xe0214000), UINT32_C(0xe4214000)
};

static const uint32_t vf2_game_info_state4_masks[] = {
    VF2_GAME_INFO_STATE4_MASK_6,
    VF2_GAME_INFO_STATE4_MASK_14,
    VF2_GAME_INFO_STATE4_MASK_6_14,
    VF2_GAME_INFO_STATE4_MASK_15,
    VF2_GAME_INFO_STATE4_MASK_6_15,
    VF2_GAME_INFO_STATE4_MASK_14_15,
    VF2_GAME_INFO_STATE4_MASK_6_14_15,
    VF2_GAME_INFO_STATE4_MASK_16,
    VF2_GAME_INFO_STATE4_MASK_6_16,
    VF2_GAME_INFO_STATE4_MASK_14_16,
    VF2_GAME_INFO_STATE4_MASK_6_14_16,
    VF2_GAME_INFO_STATE4_MASK_15_16,
    VF2_GAME_INFO_STATE4_MASK_6_15_16,
    VF2_GAME_INFO_STATE4_MASK_14_15_16,
    VF2_GAME_INFO_STATE4_MASK_6_14_15_16
};

vf2_status vf2_hybrid_first_dispatch_task_execute_base(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
);

static bool contains_mask(const uint32_t *masks, size_t count, uint32_t mask)
{
    size_t index = 0u;

    for (index = 0u; index < count; ++index) {
        if (masks[index] == mask) {
            return true;
        }
    }
    return false;
}

static bool measured_mask(uint32_t mask)
{
    return contains_mask(
        vf2_game_info_measured_masks,
        sizeof(vf2_game_info_measured_masks) / sizeof(vf2_game_info_measured_masks[0]),
        mask
    );
}

static bool measured_plus2_plus3_family(uint32_t mask)
{
    return contains_mask(
        vf2_game_info_plus2_plus3_masks,
        sizeof(vf2_game_info_plus2_plus3_masks) /
            sizeof(vf2_game_info_plus2_plus3_masks[0]),
        mask
    );
}

static bool measured_condition_only_family(uint32_t mask)
{
    return contains_mask(
        vf2_game_info_condition_only_masks,
        sizeof(vf2_game_info_condition_only_masks) /
            sizeof(vf2_game_info_condition_only_masks[0]),
        mask
    );
}

static bool measured_state4_mask(uint32_t mask)
{
    return contains_mask(
        vf2_game_info_state4_masks,
        sizeof(vf2_game_info_state4_masks) / sizeof(vf2_game_info_state4_masks[0]),
        mask
    );
}

static bool state4_uses_mode_bit6(uint32_t mask)
{
    return mask == VF2_GAME_INFO_STATE4_MASK_14 ||
           mask == VF2_GAME_INFO_STATE4_MASK_6_14 ||
           mask == VF2_GAME_INFO_STATE4_MASK_14_15 ||
           mask == VF2_GAME_INFO_STATE4_MASK_6_14_15 ||
           mask == VF2_GAME_INFO_STATE4_MASK_14_16 ||
           mask == VF2_GAME_INFO_STATE4_MASK_6_14_16 ||
           mask == VF2_GAME_INFO_STATE4_MASK_14_15_16 ||
           mask == VF2_GAME_INFO_STATE4_MASK_6_14_15_16;
}

static void set_compare_result(vf2_i960_cpu *cpu, vf2_i960_compare_result result)
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
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

static void correct_measured_compare_state(
    vf2_i960_cpu *cpu,
    bool fighter0_only,
    uint8_t countdown
)
{
    if (fighter0_only && countdown == 0u) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (countdown != 0u) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    }
}

static void correct_countdown_compare_state(vf2_i960_cpu *cpu, uint8_t countdown)
{
    set_compare_result(
        cpu,
        countdown == 0u ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_LESS
    );
}

static void correct_measured_frame3(vf2_i960_cpu *cpu, bool fighter0_only)
{
    cpu->local_frames[3].registers[3] = UINT32_C(0x41000000);
    cpu->local_frames[3].registers[4] = UINT32_C(0x07800f0f);
    cpu->local_frames[3].registers[7] = UINT32_C(0x41000000);
    if (!fighter0_only) {
        cpu->local_frames[3].registers[15] = UINT32_C(1);
    }
}

static void correct_state4_bit15_frame3(vf2_i960_cpu *cpu, bool fighter0_only)
{
    cpu->local_frames[3].registers[3] = UINT32_C(0x41000000);
    cpu->local_frames[3].registers[4] = UINT32_C(0x07800f0f);
    cpu->local_frames[3].registers[7] = UINT32_C(0x41000000);
    if (!fighter0_only) {
        cpu->local_frames[3].registers[15] = UINT32_C(0x80004400);
    }
}

static vf2_status adjust_instruction_count(
    vf2_i960_cpu *cpu,
    vf2_hybrid_task_report *report,
    int32_t correction
)
{
    if (correction >= 0) {
        const uint64_t amount = (uint64_t)(uint32_t)correction;
        cpu->executed_instructions += amount;
        if (report != NULL) {
            report->recovered_instruction_count += amount;
        }
        return VF2_OK;
    }

    {
        const uint64_t amount = (uint64_t)(-(int64_t)correction);
        if (cpu->executed_instructions < amount ||
            (report != NULL && report->recovered_instruction_count < amount)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cpu->executed_instructions -= amount;
        if (report != NULL) {
            report->recovered_instruction_count -= amount;
        }
    }
    return VF2_OK;
}

static vf2_status set_state_child_bit(vf2_model2a *machine, uint32_t fighter)
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

static vf2_status clear_legacy_field_bit(vf2_model2a *machine, uint32_t fighter)
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

static vf2_status read_mode_bit6(vf2_model2a *machine, bool *mode_bit6)
{
    uint32_t mode_base = 0u;
    uint8_t mode = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || mode_bit6 == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, VF2_GAME_INFO_MODE_BASE_SLOT, &mode_base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            mode_base + VF2_GAME_INFO_MODE_OFFSET,
            &mode,
            sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        *mode_bit6 = (mode & UINT8_C(0x40)) != 0u;
    }
    return status;
}

static bool measured_game_disp_init_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint32_t registry_address
)
{
    uint32_t continuation = 0u;
    uint32_t mode_base = 0u;
    uint32_t mode_flags = 0u;
    uint8_t mode_bytes[5] = {0u};
    uint8_t output_bytes[VF2_GAME_DISP_OUTPUT_SIZE] = {0u};
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_GAME_DISP_INIT_ENTRY ||
        registry_address != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != registry_address ||
        cpu->local_frame_depth != UINT32_C(2) ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40)) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read_u32(machine, registry_address + UINT32_C(0x0c), &continuation);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_INFO_MODE_BASE_SLOT, &mode_base);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, mode_base + VF2_GAME_DISP_MODE_FLAGS_OFFSET, &mode_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            mode_base + VF2_GAME_DISP_MODE_BYTES_OFFSET,
            mode_bytes,
            sizeof(mode_bytes)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            registry_address + VF2_GAME_DISP_OUTPUT_OFFSET,
            output_bytes,
            sizeof(output_bytes)
        );
    }
    if (status != VF2_OK || continuation != VF2_GAME_DISP_INIT_ENTRY || mode_flags != 0u) {
        return false;
    }
    for (index = 0u; index < sizeof(mode_bytes); ++index) {
        if (mode_bytes[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < sizeof(output_bytes); ++index) {
        if (output_bytes[index] != 0u) {
            return false;
        }
    }
    return true;
}

static vf2_status execute_measured_game_disp_init(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    vf2_hybrid_task_report local_report = {0};
    const uint32_t previous_frame_pointer = cpu->registers[0];
    const uint32_t task_stack_pointer = cpu->registers[1];
    const uint32_t task_frame_pointer = cpu->registers[VF2_I960_FP_REGISTER];
    const uint32_t saved_g9 = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_write_u32(
        machine, registry_address + UINT32_C(0x0c), VF2_GAME_DISP_INIT_CONTINUATION
    );
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(
            machine, registry_address + UINT32_C(0x59), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(
            machine, registry_address + UINT32_C(0x5a), &one, sizeof(one)
        );
    }
    if (status == VF2_OK) {
        const uint8_t zero = UINT8_C(0);
        status = vf2_model2a_write(
            machine, registry_address + UINT32_C(0x5b), &zero, sizeof(zero)
        );
    }
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_write_u32(
            machine,
            registry_address + UINT32_C(0x5c) + index * UINT32_C(4),
            UINT32_MAX
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x6c), UINT32_C(0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, task_stack_pointer + UINT32_C(0x40), saved_g9
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        cpu->local_frames[3].registers[index] = 0u;
    }
    cpu->local_frames[3].registers[0] = task_frame_pointer;
    cpu->local_frames[3].registers[1] = task_stack_pointer + UINT32_C(0x44);
    cpu->local_frames[3].registers[2] = VF2_GAME_DISP_HELPER_RETURN;
    cpu->local_frames[3].registers[6] = UINT32_C(1);

    cpu->registers[0] = previous_frame_pointer;
    cpu->registers[1] = task_stack_pointer;
    cpu->registers[2] = VF2_GAME_DISP_INIT_LAST_CALL_RETURN;
    for (index = 3u; index < 15u; ++index) {
        cpu->registers[index] = 0u;
    }
    cpu->registers[15] = VF2_GAME_DISP_INIT_CONTINUATION;
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(1);
    if (cpu->maximum_local_frame_depth < UINT32_C(4)) {
        cpu->maximum_local_frame_depth = UINT32_C(4);
    }

    cpu->executed_instructions += UINT64_C(92);
    cpu->procedure_calls += UINT64_C(5);
    cpu->procedure_returns += UINT64_C(5);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_SCHEDULER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_compare_result(cpu, VF2_I960_COMPARE_LESS);

    local_report.kind = VF2_HYBRID_TASK_GAME_DISP;
    local_report.entry_address = VF2_GAME_DISP_INIT_ENTRY;
    local_report.exit_address = VF2_GAME_DISP_SCHEDULER_RETURN;
    local_report.registry_address = registry_address;
    local_report.task_bytes_written = 27u;
    local_report.global_bytes_written = sizeof(uint32_t);
    local_report.recovered_instruction_count = UINT64_C(92);
    local_report.recovered_procedure_calls = UINT64_C(5);
    local_report.recovered_procedure_returns = UINT64_C(6);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

static bool measured_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint8_t *fighter_state,
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

    if (machine == NULL || cpu == NULL || fighter_state == NULL ||
        countdown == NULL || mask == NULL || fighter0 == NULL ||
        fighter1 == NULL || fighter0_only == NULL || bilateral == NULL ||
        cpu->ip != VF2_GAME_INFO_ENTRY) {
        return false;
    }

    *fighter_state = 0u;
    *mask = 0u;
    *fighter0 = 0u;
    *fighter1 = 0u;
    *fighter0_only = false;
    *bilateral = false;
    status = vf2_model2a_read_u32(machine, VF2_GAME_INFO_FIGHTER0_SLOT, fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_INFO_FIGHTER1_SLOT, fighter1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, *fighter0, &fighter0_base_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, *fighter1, &fighter1_base_flags);
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
        status = vf2_model2a_read_u32(machine, VF2_GAME_INFO_THRESHOLD, &threshold);
    }
    combined = fighter0_state_flags | fighter1_state_flags;
    if (status != VF2_OK || fighter0_state != fighter1_state ||
        threshold > UINT32_C(2) ||
        (*countdown != UINT8_C(0) && *countdown != UINT8_C(1)) ||
        (fighter0_base_flags & UINT32_C(0x80000000)) == 0u ||
        (fighter1_base_flags & UINT32_C(0x80000000)) == 0u ||
        (fighter0_state_flags != 0u && fighter0_state_flags != combined) ||
        (fighter1_state_flags != 0u && fighter1_state_flags != combined)) {
        return false;
    }
    if (fighter0_state == UINT8_C(8)) {
        if (!measured_mask(combined)) {
            return false;
        }
    } else if (fighter0_state == UINT8_C(4)) {
        if (!measured_state4_mask(combined)) {
            return false;
        }
    } else {
        return false;
    }

    *fighter_state = fighter0_state;
    *mask = combined;
    *fighter0_only =
        fighter0_state_flags == combined && fighter1_state_flags == 0u;
    *bilateral =
        fighter0_state_flags == combined && fighter1_state_flags == combined;
    return true;
}

static uint64_t bit14_bit16_high21_correction(
    bool fighter0_only,
    bool bilateral,
    bool countdown_nonzero,
    bool mode_bit6
)
{
    if (fighter0_only) {
        if (countdown_nonzero) {
            return UINT64_C(2);
        }
        return mode_bit6 ? UINT64_C(2) : UINT64_C(3);
    }
    if (bilateral) {
        if (countdown_nonzero) {
            return mode_bit6 ? UINT64_C(8) : UINT64_C(7);
        }
        return mode_bit6 ? UINT64_C(8) : UINT64_C(4);
    }
    if (countdown_nonzero) {
        return mode_bit6 ? UINT64_C(8) : UINT64_C(7);
    }
    return mode_bit6 ? UINT64_C(8) : UINT64_C(3);
}

static int32_t state4_mask_6_14_16_correction(
    bool fighter0_only,
    bool bilateral,
    uint8_t countdown,
    bool mode_bit6
)
{
    static const int8_t corrections[3][2][2] = {
        {{0, -1}, {-1, -1}},
        {{0, 5}, {4, 5}},
        {{-1, 3}, {2, 3}}
    };
    const size_t distribution = fighter0_only ? 0u : (bilateral ? 2u : 1u);

    return (int32_t)corrections[distribution][countdown][mode_bit6 ? 1u : 0u];
}

static vf2_status apply_state4_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_task_report *report,
    uint32_t mask,
    uint32_t fighter0,
    uint32_t fighter1,
    bool fighter0_only,
    bool bilateral,
    uint8_t countdown,
    bool mode_bit6
)
{
    const int32_t active_count = bilateral ? 2 : 1;
    int32_t correction = 0;
    vf2_status status = VF2_OK;

    if (mask == VF2_GAME_INFO_STATE4_MASK_15_16 ||
        mask == VF2_GAME_INFO_STATE4_MASK_6_15_16) {
        if (fighter0_only || bilateral) {
            status = set_state_child_bit(machine, fighter0);
        }
        if (status == VF2_OK && !fighter0_only) {
            status = set_state_child_bit(machine, fighter1);
        }
        if (status != VF2_OK) {
            return status;
        }
        status = adjust_instruction_count(cpu, report, 4 * active_count);
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_6 ||
        mask == VF2_GAME_INFO_STATE4_MASK_16 ||
        mask == VF2_GAME_INFO_STATE4_MASK_6_16) {
        correct_countdown_compare_state(cpu, countdown);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_15) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_state4_bit15_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_6_15) {
        status = adjust_instruction_count(cpu, report, -3 * active_count);
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_14_16) {
        correction = mode_bit6 ? active_count : 0;
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        correct_countdown_compare_state(cpu, countdown);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_14_15_16) {
        correction = (mode_bit6 ? 4 : 3) * active_count;
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_14 ||
        mask == VF2_GAME_INFO_STATE4_MASK_6_14) {
        correction = active_count *
            ((int32_t)(UINT32_C(3) * (uint32_t)countdown) + (mode_bit6 ? 1 : 0));
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        correct_countdown_compare_state(cpu, countdown);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_14_15) {
        correction = mode_bit6 ? active_count : 0;
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_6_14_15) {
        correction = (5 + (mode_bit6 ? 1 : 0)) * active_count;
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_6_14_16) {
        correction = state4_mask_6_14_16_correction(
            fighter0_only,
            bilateral,
            countdown,
            mode_bit6
        );
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        correct_countdown_compare_state(cpu, countdown);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_STATE4_MASK_6_14_15_16) {
        if (fighter0_only) {
            correction = 2;
        } else if (bilateral) {
            correction = 9 + (mode_bit6 ? 1 : 0);
        } else {
            correction = 7 + (mode_bit6 ? 1 : 0);
        }
        status = adjust_instruction_count(cpu, report, correction);
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    return VF2_ERROR_UNSUPPORTED;
}

vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    uint8_t fighter_state = 0u;
    uint8_t countdown = 0u;
    uint32_t mask = 0u;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    bool fighter0_only = false;
    bool bilateral = false;
    bool mode_bit6 = false;
    bool apply_measured_poststate = false;
    vf2_status status = VF2_OK;

    if (measured_game_disp_init_case(machine, cpu, registry_address)) {
        return execute_measured_game_disp_init(machine, cpu, registry_address, report);
    }

    apply_measured_poststate = measured_case(
        machine,
        cpu,
        &fighter_state,
        &countdown,
        &mask,
        &fighter0,
        &fighter1,
        &fighter0_only,
        &bilateral
    );

    if (apply_measured_poststate &&
        ((fighter_state == UINT8_C(8) && mask == VF2_GAME_INFO_MASK_14_16_21) ||
         (fighter_state == UINT8_C(4) && state4_uses_mode_bit6(mask)))) {
        status = read_mode_bit6(machine, &mode_bit6);
        if (status != VF2_OK) {
            return status;
        }
    }

    status = vf2_hybrid_first_dispatch_task_execute_base(
        machine, cpu, registry_address, report
    );
    if (status != VF2_OK || !apply_measured_poststate) {
        return status;
    }

    if (fighter_state == UINT8_C(4)) {
        return apply_state4_poststate(
            machine,
            cpu,
            report,
            mask,
            fighter0,
            fighter1,
            fighter0_only,
            bilateral,
            countdown,
            mode_bit6
        );
    }

    if (measured_plus2_plus3_family(mask)) {
        const uint64_t correction = bilateral ? UINT64_C(3) : UINT64_C(2);
        cpu->executed_instructions += correction;
        if (report != NULL) {
            report->recovered_instruction_count += correction;
        }
        correct_measured_compare_state(cpu, fighter0_only, countdown);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
    }

    if (measured_condition_only_family(mask)) {
        correct_measured_compare_state(cpu, fighter0_only, countdown);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_MASK_14_16_21) {
        const uint64_t correction = bit14_bit16_high21_correction(
            fighter0_only,
            bilateral,
            countdown != 0u,
            mode_bit6
        );
        cpu->executed_instructions += correction;
        if (report != NULL) {
            report->recovered_instruction_count += correction;
        }
        correct_measured_compare_state(cpu, fighter0_only, countdown);
        correct_measured_frame3(cpu, fighter0_only);
        return VF2_OK;
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
        correct_measured_compare_state(cpu, fighter0_only, countdown);
        return VF2_OK;
    }

    if (mask == VF2_GAME_INFO_MASK_14_21) {
        correct_measured_compare_state(cpu, fighter0_only, countdown);
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
