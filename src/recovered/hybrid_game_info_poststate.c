#include "vf2/hybrid.h"

#include <stddef.h>
#include <stdint.h>

#define vf2_hybrid_first_dispatch_task_execute \
    vf2_hybrid_first_dispatch_task_execute_poststate_base
#include "hybrid_game_info_poststate_base.inc"
#undef vf2_hybrid_first_dispatch_task_execute

vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    uint32_t mode_flags = 0u;
    uint8_t mode_bytes[5] = {0u};
    uint32_t previous_frame_pointer = 0u;
    uint32_t task_stack_pointer = 0u;
    uint32_t index = 0u;
    const bool correct_game_disp_frame2 = measured_game_disp_init_case(
        machine,
        cpu,
        registry_address,
        &mode_flags,
        mode_bytes
    );
    vf2_status status = VF2_OK;

    if (correct_game_disp_frame2) {
        previous_frame_pointer = cpu->registers[0];
        task_stack_pointer = cpu->registers[1];
    }

    status = vf2_hybrid_first_dispatch_task_execute_poststate_base(
        machine, cpu, registry_address, report
    );
    if (status != VF2_OK || !correct_game_disp_frame2) {
        return status;
    }
    if (cpu->ip != VF2_GAME_DISP_SCHEDULER_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        cpu->local_frames[2].registers[index] = 0u;
    }
    cpu->local_frames[2].registers[0] = previous_frame_pointer;
    cpu->local_frames[2].registers[1] = task_stack_pointer;
    cpu->local_frames[2].registers[2] = VF2_GAME_DISP_INIT_LAST_CALL_RETURN;
    cpu->local_frames[2].registers[15] = VF2_GAME_DISP_INIT_CONTINUATION;
    return VF2_OK;
}
