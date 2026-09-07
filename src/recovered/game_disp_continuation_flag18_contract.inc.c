#define VF2_GAME_DISP_FLAG18_CONT_WORDS UINT32_C(0x00515b44)

static bool measured_game_disp_continuation_flag18_contract_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    game_disp_flag18_case match;

    return machine != NULL && cpu != NULL &&
           cpu->ip == VF2_GAME_DISP_CONT_ENTRY &&
           game_disp_flag18_machine_case(machine, &match);
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    static const uint8_t measured_words[4] = {
        UINT8_C(0x1f), UINT8_C(0x80), UINT8_C(0x1f), UINT8_C(0x80)
    };
    const bool measured_full =
        measured_game_disp_continuation_flag18_contract_case(machine, cpu);
    vf2_status status = vf2_native_runtime_step_flag18_base(
        machine, cpu, state, report
    );

    if (status != VF2_OK || !measured_full) {
        return status;
    }
    return vf2_model2a_write(
        machine,
        VF2_GAME_DISP_FLAG18_CONT_WORDS,
        measured_words,
        sizeof(measured_words)
    );
}
