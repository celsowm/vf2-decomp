#define VF2_GAME_DISP_FLAG98_CHILD_AC UINT32_C(0x3f001001)

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    game_disp_flag98_case match;
    const bool measured_child =
        machine != NULL && cpu != NULL && state != NULL &&
        cpu->ip == VF2_GAME_DISP_EVENT_ENTRY &&
        measured_game_disp_flag98_case(machine, cpu, &match);
    vf2_status status = vf2_native_runtime_step_flag98_contract_base(
        machine, cpu, state, report
    );

    if (status != VF2_OK || !measured_child) {
        return status;
    }
    cpu->arithmetic_control = VF2_GAME_DISP_FLAG98_CHILD_AC;
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    return VF2_OK;
}
