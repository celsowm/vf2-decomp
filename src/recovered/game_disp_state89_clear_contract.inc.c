#define VF2_GAME_DISP_STATE89_CLEAR_BYTE UINT32_C(0x005000a4)
#define VF2_GAME_DISP_STATE89_CLEAR_OBSERVED UINT8_C(0x0b)

static bool game_disp_state89_clear_value_is_measured(uint8_t value)
{
    return value == UINT8_C(0) ||
           value == VF2_GAME_DISP_STATE89_CLEAR_OBSERVED;
}

static bool game_disp_state89_full_clears_byte(
    const game_disp_state89_case *match
)
{
    if (match == NULL) {
        return false;
    }
    if (match->kind == GAME_DISP_STATE89_INITIAL) {
        return true;
    }
    return match->kind == GAME_DISP_STATE89_SETTLED &&
           match->state_byte == UINT8_C(8) &&
           match->mode == UINT8_C(0);
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    game_disp_state89_case match;
    uint8_t original_clear = UINT8_MAX;
    bool state89 = false;
    bool preserve_clear = false;
    vf2_status status = VF2_OK;

    if (machine != NULL && cpu != NULL && state != NULL &&
        (cpu->ip == VF2_GAME_DISP_EVENT_ENTRY ||
         cpu->ip == VF2_GAME_DISP_CONT_ENTRY) &&
        game_disp_state89_machine_case(machine, &match)) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_STATE89_CLEAR_BYTE,
            &original_clear,
            sizeof(original_clear)
        );
        if (status != VF2_OK) {
            return status;
        }
        if (!game_disp_state89_clear_value_is_measured(original_clear)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        state89 = true;
        preserve_clear =
            cpu->ip == VF2_GAME_DISP_EVENT_ENTRY ||
            !game_disp_state89_full_clears_byte(&match);
    }

    status = vf2_native_runtime_step_condition_state89_clear_base(
        machine, cpu, state, report
    );
    if (status != VF2_OK || !state89) {
        return status;
    }

    return game_disp_state89_write_u8(
        machine,
        VF2_GAME_DISP_STATE89_CLEAR_BYTE,
        preserve_clear ? original_clear : UINT8_C(0)
    );
}
