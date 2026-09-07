#define vf2_hybrid_bridge_apply_condition_poststate \
    vf2_hybrid_bridge_apply_condition_poststate_core
#define vf2_hybrid_post_frame_bridge_execute \
    vf2_hybrid_post_frame_bridge_execute_condition_core
#include "texture_bridge_condition_impl.c"
#undef vf2_hybrid_post_frame_bridge_execute
#undef vf2_hybrid_bridge_apply_condition_poststate

#define VF2_SELECTOR2_FRAME_DISPATCH_ENTRY UINT32_C(0x0000a6c0)
#define VF2_SELECTOR2_FRAME_MASK UINT32_C(0x0050002c)
#define VF2_SELECTOR2_MASK UINT32_C(0x00000004)
#define VF2_SELECTOR2_QUEUE_COUNT UINT32_C(0x00504001)
#define VF2_SELECTOR2_MODEL_BASE UINT32_C(0x0050016c)
#define VF2_START_PHASE_INDEX UINT32_C(0x005000a4)
#define VF2_START_PHASE_STATE UINT32_C(0x005000a5)

static vf2_status apply_selector2_queue_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry
)
{
    uint32_t selector_mask = 0u;
    uint32_t base = 0u;
    uint8_t queue_count = 0u;
    uint8_t profile_mode = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_MAIN_POST_CLUSTER_ENTRY ||
        (entry != VF2_MAIN_FINAL_CLUSTER_ENTRY &&
         entry != VF2_SELECTOR2_FRAME_DISPATCH_ENTRY)) {
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_SELECTOR2_FRAME_MASK, &selector_mask
    );
    if (status != VF2_OK || selector_mask != VF2_SELECTOR2_MASK) {
        return status;
    }
    status = vf2_model2a_read(
        machine, VF2_SELECTOR2_QUEUE_COUNT, &queue_count, sizeof(queue_count)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SELECTOR2_MODEL_BASE, &base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3351),
            &profile_mode, sizeof(profile_mode)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if ((profile_mode & UINT8_C(1)) != 0u || queue_count < UINT8_C(16)) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    } else if (queue_count > UINT8_C(16)) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
    }
    return VF2_OK;
}

static vf2_status apply_start_final_cluster_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry
)
{
    uint8_t phase_index = 0u;
    uint8_t phase_state = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        entry != VF2_MAIN_FINAL_CLUSTER_ENTRY ||
        cpu->ip != VF2_MAIN_POST_CLUSTER_ENTRY) {
        return VF2_OK;
    }

    status = vf2_model2a_read(
        machine,
        VF2_START_PHASE_INDEX,
        &phase_index,
        sizeof(phase_index)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_START_PHASE_STATE,
            &phase_state,
            sizeof(phase_state)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (phase_index == UINT8_C(0x8b) && phase_state == UINT8_C(0)) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    }
    return VF2_OK;
}

vf2_status vf2_hybrid_bridge_apply_condition_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t entry_r3,
    uint32_t entry_r7
)
{
    vf2_status status = vf2_hybrid_bridge_apply_condition_poststate_core(
        machine, cpu, entry, entry_r3, entry_r7
    );

    if (status == VF2_OK) {
        status = apply_selector2_queue_condition(machine, cpu, entry);
    }
    if (status == VF2_OK) {
        status = apply_start_final_cluster_condition(machine, cpu, entry);
    }
    return status;
}

vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    vf2_status status = vf2_hybrid_post_frame_bridge_execute_condition_core(
        machine, cpu, report
    );

    if (status == VF2_OK) {
        status = apply_selector2_queue_condition(machine, cpu, entry);
    }
    if (status == VF2_OK) {
        status = apply_start_final_cluster_condition(machine, cpu, entry);
    }
    return status;
}
