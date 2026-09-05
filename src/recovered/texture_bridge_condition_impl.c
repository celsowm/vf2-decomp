#include "vf2/hybrid.h"

#define VF2_INTERRUPT_BUFFER_GATE_ENTRY UINT32_C(0x00000c0c)
#define VF2_INTERRUPT_PLAYER_LAYER_ENTRY UINT32_C(0x00000c78)
#define VF2_INTERRUPT_GAME_INPUT_ENTRY UINT32_C(0x00000c80)
#define VF2_INTERRUPT_GAME_STATE_ENTRY UINT32_C(0x00000c90)
#define VF2_INTERRUPT_INPUT_RING_ENTRY UINT32_C(0x00000c94)
#define VF2_INTERRUPT_TILE_SYNC_ENTRY UINT32_C(0x00000cd4)
#define VF2_GAME_STATE_RETURN_STUB UINT32_C(0x000020ec)
#define VF2_MAIN_CLEAR_PREFIX_ENTRY UINT32_C(0x00009fb0)
#define VF2_MAIN_FINAL_CLUSTER_ENTRY UINT32_C(0x00009ff8)
#define VF2_MAIN_POST_CLUSTER_ENTRY UINT32_C(0x0000a010)
#define VF2_MAIN_FRAME_TIMER_CALL_ENTRY UINT32_C(0x0000a034)
#define VF2_MAIN_POST_TIMER_ENTRY UINT32_C(0x0000a038)
#define VF2_FRAME_TIMER_WAIT_ENTRY UINT32_C(0x00010f90)
#define VF2_FRAME_TIMER_SUFFIX_ENTRY UINT32_C(0x00010fa4)
#define VF2_TEXTURE_MAINTENANCE_ENTRY UINT32_C(0x0004b8d8)
#define VF2_TEXTURE_COUNTER_UPDATE_ENTRY UINT32_C(0x0004bb98)
#define VF2_TEXTURE_COUNTER_UPDATE_EXIT UINT32_C(0x0004bc58)
#define VF2_TEXTURE_COUNTER0 UINT32_C(0x005502c0)
#define VF2_TEXTURE_COUNTER1 UINT32_C(0x005502d0)
#define VF2_TEXTURE_COUNTER2 UINT32_C(0x005502e0)
#define VF2_TEXTURE_ORCHESTRATOR_ENTRY UINT32_C(0x0004bd00)
#define VF2_TEXTURE_STATUS_DISPATCH_ENTRY UINT32_C(0x0004bd24)
#define VF2_TEXTURE_RECORD_STATUS_ENTRY UINT32_C(0x0004bd5c)
#define VF2_TEXTURE_ACTIVE_PREPARE_ENTRY UINT32_C(0x0004bde0)
#define VF2_TEXTURE_RECORD_STATUS_EXIT UINT32_C(0x0004bde0)
#define VF2_TEXTURE_STATUS_LINE_ENTRY UINT32_C(0x0004d2c0)
#define VF2_TEXTURE_STATUS_TAIL_ENTRY UINT32_C(0x0004d25c)
#define VF2_TEXTURE_BODY_RETURN_ENTRY UINT32_C(0x0004bfdc)
#define VF2_TEXTURE_ACTIVE_PREPARE_TARGET UINT32_C(0x0004d16c)
#define VF2_TEXTURE_TREE_DISPATCH_ENTRY UINT32_C(0x0004c544)
#define VF2_TEXTURE_TREE_DISPATCH_EXIT UINT32_C(0x0004c6e0)
#define VF2_TEXTURE_WORD_PREPARE_ENTRY UINT32_C(0x0004cb64)
#define VF2_TEXTURE_WORD_PREPARE_EXIT UINT32_C(0x0004cc28)
#define VF2_TEXTURE_COLOR_PREPARE_ENTRY UINT32_C(0x0004cd18)
#define VF2_TEXTURE_COLOR_PREPARE_EXIT UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_CONVERT_LOOP_ENTRY UINT32_C(0x0004cdb0)
#define VF2_TEXTURE_CONVERT_POST_ENTRY UINT32_C(0x0004cdd4)
#define VF2_TEXTURE_CONVERT_ENTRY UINT32_C(0x0004ce88)
#define VF2_TEXTURE_CONVERT_LOOP_RETURN UINT32_C(0x0004ce0c)
#define VF2_TEXTURE_RECORD_ADVANCE_ENTRY UINT32_C(0x0004bf60)
#define VF2_TEXTURE_FINAL_STATUS_ENTRY UINT32_C(0x0004bf90)
#define VF2_TEXTURE_FINAL_STATUS_TARGET UINT32_C(0x0004d25c)
#define VF2_TEXTURE_ACTIVE_FLAGS UINT32_C(0x0055c2f4)
#define VF2_FRAME_SELECTOR UINT32_C(0x0050002a)
#define VF2_GAME_STATE_SELECTOR_MASK UINT32_C(0x0050002c)

vf2_status vf2_hybrid_post_frame_bridge_execute_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
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
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
    cpu->compare_result = result;
}

static vf2_status set_active_flag_bit_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t bit
)
{
    uint32_t flags = 0u;
    const vf2_status status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_ACTIVE_FLAGS, &flags
    );

    if (status != VF2_OK) {
        return status;
    }
    set_compare_result(
        cpu,
        (flags & (UINT32_C(1) << bit)) != 0u
            ? VF2_I960_COMPARE_EQUAL
            : VF2_I960_COMPARE_NONE
    );
    return VF2_OK;
}

static vf2_status set_main_final_cluster_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint8_t selector = 0u;
    vf2_status status = vf2_model2a_read(
        machine, VF2_FRAME_SELECTOR, &selector, sizeof(selector)
    );

    if (status != VF2_OK) {
        return status;
    }
    if (selector == UINT8_C(1)) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    } else if (selector == UINT8_C(17)) {
        uint8_t phase_index = 0u;
        uint8_t phase_state = 0u;
        uint32_t countdown = 0u;

        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a4), &phase_index, sizeof(phase_index)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x005000a5), &phase_state, sizeof(phase_state)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500024), &countdown
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (phase_index == UINT8_C(0x8b) &&
            phase_state == UINT8_C(0xff) &&
            (int32_t)countdown > 0) {
            set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        } else {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    }
    return VF2_OK;
}

static vf2_status set_texture_counter_update_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t counter0 = 0u;
    uint32_t counter1 = 0u;
    uint32_t counter2 = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, VF2_TEXTURE_COUNTER0, &counter0
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER1, &counter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TEXTURE_COUNTER2, &counter2
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (counter0 == 0u && counter1 == 0u && counter2 == 0u) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
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
    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (entry == VF2_TEXTURE_ORCHESTRATOR_ENTRY) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (entry == VF2_TEXTURE_STATUS_DISPATCH_ENTRY) {
        if (cpu->ip == VF2_TEXTURE_STATUS_LINE_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_NONE);
        } else if (cpu->ip == VF2_TEXTURE_RECORD_STATUS_ENTRY ||
                   cpu->ip == VF2_TEXTURE_FINAL_STATUS_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    } else if (entry == VF2_TEXTURE_RECORD_STATUS_ENTRY) {
        if (cpu->ip == VF2_TEXTURE_STATUS_DISPATCH_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        } else if (cpu->ip == VF2_TEXTURE_RECORD_STATUS_EXIT) {
            if ((int32_t)entry_r3 >= 0) {
                set_compare_result(
                    cpu,
                    entry_r3 == 0u
                        ? VF2_I960_COMPARE_EQUAL
                        : VF2_I960_COMPARE_LESS
                );
            } else {
                set_compare_result(
                    cpu,
                    cpu->registers[8] == 0u
                        ? VF2_I960_COMPARE_EQUAL
                        : VF2_I960_COMPARE_LESS
                );
            }
        }
    } else if (entry == VF2_TEXTURE_ACTIVE_PREPARE_ENTRY &&
               cpu->ip == VF2_TEXTURE_ACTIVE_PREPARE_TARGET) {
        const uint32_t bits45 = (UINT32_C(1) << 4u) | (UINT32_C(1) << 5u);

        set_compare_result(
            cpu,
            (entry_r7 & bits45) == bits45
                ? VF2_I960_COMPARE_EQUAL
                : VF2_I960_COMPARE_NONE
        );
    } else if (entry == VF2_TEXTURE_TREE_DISPATCH_ENTRY &&
               cpu->ip == VF2_TEXTURE_TREE_DISPATCH_EXIT) {
        const vf2_status status = set_active_flag_bit_condition(
            machine, cpu, UINT32_C(1)
        );
        if (status != VF2_OK) {
            return status;
        }
    } else if (entry == VF2_TEXTURE_WORD_PREPARE_ENTRY &&
               cpu->ip == VF2_TEXTURE_WORD_PREPARE_EXIT) {
        const vf2_status status = set_active_flag_bit_condition(
            machine, cpu, UINT32_C(2)
        );
        if (status != VF2_OK) {
            return status;
        }
    } else if (entry == VF2_TEXTURE_COLOR_PREPARE_ENTRY &&
               cpu->ip == VF2_TEXTURE_COLOR_PREPARE_EXIT) {
        const vf2_status status = set_active_flag_bit_condition(
            machine, cpu, UINT32_C(1)
        );
        if (status != VF2_OK) {
            return status;
        }
    } else if (entry == VF2_TEXTURE_CONVERT_LOOP_ENTRY) {
        if (cpu->ip == VF2_TEXTURE_CONVERT_ENTRY) {
            set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        } else if (cpu->ip == VF2_TEXTURE_CONVERT_LOOP_RETURN) {
            set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    } else if (entry == VF2_TEXTURE_CONVERT_POST_ENTRY &&
               cpu->ip == VF2_TEXTURE_CONVERT_LOOP_ENTRY) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (entry == VF2_TEXTURE_RECORD_ADVANCE_ENTRY &&
               cpu->ip == VF2_TEXTURE_STATUS_DISPATCH_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    } else if (entry == VF2_TEXTURE_FINAL_STATUS_ENTRY &&
               cpu->ip == VF2_TEXTURE_FINAL_STATUS_TARGET &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_NONE);
    } else if (entry == VF2_TEXTURE_STATUS_TAIL_ENTRY &&
               cpu->ip == VF2_TEXTURE_BODY_RETURN_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
    } else if (entry == VF2_TEXTURE_MAINTENANCE_ENTRY &&
               cpu->ip == VF2_TEXTURE_COUNTER_UPDATE_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_LESS);
    } else if (entry == VF2_TEXTURE_COUNTER_UPDATE_ENTRY &&
               cpu->ip == VF2_TEXTURE_COUNTER_UPDATE_EXIT &&
               machine->main_rom != NULL) {
        const vf2_status status = set_texture_counter_update_condition(
            machine, cpu
        );
        if (status != VF2_OK) {
            return status;
        }
    } else if (entry == VF2_MAIN_FRAME_TIMER_CALL_ENTRY &&
               cpu->ip == VF2_FRAME_TIMER_WAIT_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
    } else if (entry == VF2_FRAME_TIMER_SUFFIX_ENTRY &&
               cpu->ip == VF2_MAIN_POST_TIMER_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (entry == VF2_MAIN_POST_TIMER_ENTRY &&
               cpu->ip == VF2_MAIN_CLEAR_PREFIX_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (entry == VF2_MAIN_CLEAR_PREFIX_ENTRY &&
               cpu->ip == VF2_MAIN_FINAL_CLUSTER_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_NONE);
    } else if (entry == VF2_MAIN_FINAL_CLUSTER_ENTRY &&
               cpu->ip == VF2_MAIN_POST_CLUSTER_ENTRY &&
               machine->main_rom != NULL) {
        const vf2_status status = set_main_final_cluster_condition(
            machine, cpu
        );
        if (status != VF2_OK) {
            return status;
        }
    } else if (entry == VF2_INTERRUPT_BUFFER_GATE_ENTRY &&
               cpu->ip == VF2_INTERRUPT_PLAYER_LAYER_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_NONE);
    } else if (entry == VF2_INTERRUPT_PLAYER_LAYER_ENTRY &&
               cpu->ip == VF2_INTERRUPT_GAME_INPUT_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
    } else if (entry == VF2_INTERRUPT_GAME_INPUT_ENTRY &&
               cpu->ip == VF2_INTERRUPT_GAME_STATE_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    } else if (entry == VF2_INTERRUPT_GAME_STATE_ENTRY &&
               cpu->ip == VF2_GAME_STATE_RETURN_STUB &&
               machine->main_rom != NULL) {
        uint32_t selector_mask = 0u;
        const vf2_status status = vf2_model2a_read_u32(
            machine, VF2_GAME_STATE_SELECTOR_MASK, &selector_mask
        );
        if (status != VF2_OK) {
            return status;
        }
        set_compare_result(
            cpu,
            (selector_mask & UINT32_C(0x00030000)) != 0u
                ? VF2_I960_COMPARE_LESS
                : VF2_I960_COMPARE_GREATER
        );
    } else if (entry == VF2_INTERRUPT_INPUT_RING_ENTRY &&
               cpu->ip == VF2_INTERRUPT_TILE_SYNC_ENTRY &&
               machine->main_rom != NULL) {
        set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
    }
    return VF2_OK;
}

vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    const uint32_t entry_r3 = cpu != NULL ? cpu->registers[3] : 0u;
    const uint32_t entry_r7 = cpu != NULL ? cpu->registers[7] : 0u;
    const vf2_status status = vf2_hybrid_post_frame_bridge_execute_impl(
        machine, cpu, report
    );

    if (status != VF2_OK || cpu == NULL) {
        return status;
    }
    return vf2_hybrid_bridge_apply_condition_poststate(
        machine, cpu, entry, entry_r3, entry_r7
    );
}
