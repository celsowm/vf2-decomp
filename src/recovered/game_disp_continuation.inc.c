#define VF2_GAME_DISP_CONT_ENTRY UINT32_C(0x0002b1f8)
#define VF2_GAME_DISP_CONT_RETURN UINT32_C(0x00010dcc)
#define VF2_GAME_DISP_CONT_CONTROL UINT32_C(0x00508000)
#define VF2_GAME_DISP_CONT_DISPATCH_INDEX UINT32_C(0x00500048)
#define VF2_GAME_DISP_CONT_TARGET_TABLE UINT32_C(0x0003178c)
#define VF2_GAME_DISP_CONT_INSTRUCTIONS UINT64_C(2903)
#define VF2_GAME_DISP_CONT_CALLS UINT64_C(41)
#define VF2_GAME_DISP_CONT_RETURNS UINT64_C(42)

static bool measured_game_disp_continuation_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    static const uint32_t expected_globals[15] = {
        UINT32_C(0), UINT32_C(0x3f4f5c29), UINT32_C(0xc0a0a3d7), UINT32_C(0),
        UINT32_C(0x00560000), UINT32_C(0x0050e850), UINT32_C(0x000055b6),
        UINT32_C(0x00512980), UINT32_C(0x00510980), UINT32_C(0x010016ac),
        UINT32_C(0x00800000), UINT32_C(0x00880000), UINT32_C(0x00004000),
        VF2_GAME_DISP_REGISTRY, UINT32_C(0x00000220)
    };
    uint32_t control = 0u;
    uint32_t registry_flags = 0u;
    uint32_t target = 0u;
    uint16_t counter = 0u;
    uint8_t dispatch_index = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_GAME_DISP_CONT_ENTRY ||
        cpu->local_frame_depth != UINT32_C(2) ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[1].registers[2] != VF2_GAME_DISP_CONT_RETURN ||
        cpu->local_frames[1].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 15u; ++index) {
        const uint32_t actual = cpu->registers[VF2_I960_G0_REGISTER + index];
        if (actual == expected_globals[index]) {
            continue;
        }
        if (index == 8u && actual == expected_globals[7u]) {
            continue;
        }
        return false;
    }

    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_CONT_CONTROL, &control);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_REGISTRY, &registry_flags);
    }
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(machine, VF2_GAME_DISP_COUNTER_VALUE, &counter);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_CONT_DISPATCH_INDEX,
            &dispatch_index, sizeof(dispatch_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_CONT_TARGET_TABLE + (uint32_t)dispatch_index * UINT32_C(4),
            &target
        );
    }
    return status == VF2_OK && control == UINT32_C(0x00008a00) &&
           (registry_flags == UINT32_C(0x80000000) ||
            registry_flags ==
                (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG10) ||
            registry_flags ==
                (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG16) ||
            registry_flags ==
                (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG17) ||
            registry_flags ==
                (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG19)) &&
           counter == UINT16_C(0) && dispatch_index == UINT8_C(0) &&
           target == VF2_GAME_DISP_MAIN_ENTRY;
}

static vf2_status game_disp_cont_call_leaf(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *child_state,
    uint8_t selector,
    uint32_t fighter,
    uint32_t return_address
)
{
    vf2_status status = VF2_OK;

    cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_LEAF_ENTRY, return_address
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_leaf(
            machine, cpu, child_state, selector, return_address, NULL
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_continuation(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    vf2_native_runtime_state child_state;
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint64_t continuation_instructions = VF2_GAME_DISP_CONT_INSTRUCTIONS;
    uint32_t control = 0u;
    uint32_t registry_flags = 0u;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t counter_flags = 0u;
    uint32_t target = 0u;
    uint16_t counter_value = 0u;
    uint8_t dispatch_index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_CONT_CONTROL, &control
    );

    cpu->registers[15] = control;
    if (status != VF2_OK || (control & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_REGISTRY, &registry_flags);
    cpu->registers[3] = registry_flags;
    if (status != VF2_OK || (registry_flags & (UINT32_C(1) << 30u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_FIGHTER0_SLOT, &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_FIGHTER1_SLOT, &fighter1);
    }
    if (status != VF2_OK) {
        return status;
    }

    memset(&child_state, 0, sizeof(child_state));
    status = game_disp_cont_call_leaf(
        machine, cpu, &child_state, UINT8_C(0), fighter0, VF2_GAME_DISP_RETURN0
    );
    if (status != VF2_OK) {
        return status;
    }
    memset(&child_state, 0, sizeof(child_state));
    status = game_disp_cont_call_leaf(
        machine, cpu, &child_state, UINT8_C(1), fighter1, VF2_GAME_DISP_RETURN1
    );
    if (status != VF2_OK) {
        return status;
    }

    if ((cpu->registers[3] & (UINT32_C(1) << 25u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = game_disp_leaf_read_u16(
        machine, VF2_GAME_DISP_COUNTER_VALUE, &counter_value
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_RUNTIME_FLAGS, &counter_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_COUNTER_ENTRY, VF2_GAME_DISP_COUNTER_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_counter(
            machine, cpu, &child_state, counter_value, counter_flags, NULL
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_HUD_GATE_ENTRY, VF2_GAME_DISP_HUD_GATE_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_hud_gate(
            machine, cpu, &child_state, NULL
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_CLEAR_ENTRY, VF2_GAME_DISP_CLEAR_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_clear(
            machine, cpu, &child_state, NULL
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] &= ~(UINT32_C(1) << 28u);
    status = vf2_model2a_write_u32(
        machine, VF2_GAME_DISP_REGISTRY, cpu->registers[3]
    );
    if (status != VF2_OK ||
        (cpu->registers[3] & ((UINT32_C(1) << 24u) |
                             (UINT32_C(1) << 23u) |
                             (UINT32_C(1) << 21u))) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_EVENT_ENTRY, VF2_GAME_DISP_EVENT_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_event_gate(
            machine, cpu, &child_state, NULL
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    {
        const uint64_t child_instructions = child_state.recovered_instruction_count;
        const uint64_t flag10_queue_delta =
            VF2_GAME_DISP_EVENT_FLAG10_INSTRUCTIONS +
            VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS;
        const uint64_t flag16_queue_delta =
            VF2_GAME_DISP_EVENT_FLAG16_INSTRUCTIONS +
            VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS;
        const uint64_t flag17_queue_delta =
            VF2_GAME_DISP_EVENT_FLAG17_INSTRUCTIONS +
            VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS;
        const uint64_t flag19_queue_delta =
            VF2_GAME_DISP_EVENT_FLAG19_INSTRUCTIONS +
            VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS;
        uint64_t event_delta = 0u;
        if (child_instructions < VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS) {
            return VF2_ERROR_UNSUPPORTED;
        }
        event_delta = child_instructions - VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS;
        if (event_delta != UINT64_C(0) && event_delta != UINT64_C(1) &&
            event_delta != VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS &&
            event_delta != VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS + UINT64_C(1) &&
            event_delta != flag10_queue_delta &&
            event_delta != flag10_queue_delta + UINT64_C(1) &&
            event_delta != flag16_queue_delta &&
            event_delta != flag17_queue_delta &&
            event_delta != flag19_queue_delta) {
            return VF2_ERROR_UNSUPPORTED;
        }
        continuation_instructions += event_delta;
    }
    if ((cpu->registers[3] & (UINT32_C(1) << 26u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_CONT_DISPATCH_INDEX,
        &dispatch_index, sizeof(dispatch_index)
    );
    cpu->registers[3] = (uint32_t)dispatch_index;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_GAME_DISP_CONT_TARGET_TABLE + (uint32_t)dispatch_index * UINT32_C(4),
            &target
        );
    }
    cpu->registers[5] = target;
    if (status != VF2_OK || dispatch_index != UINT8_C(0) ||
        target != VF2_GAME_DISP_MAIN_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(&child_state, 0, sizeof(child_state));
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GAME_DISP_MAIN_ENTRY, VF2_GAME_DISP_MAIN_RETURN
    );
    if (status == VF2_OK) {
        status = execute_measured_game_disp_main(machine, cpu, &child_state, NULL);
    }
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_MAIN_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + continuation_instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_CONT_RETURN ||
        cpu->procedure_calls != start_calls + VF2_GAME_DISP_CONT_CALLS ||
        cpu->procedure_returns != start_returns + VF2_GAME_DISP_CONT_RETURNS) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_equal_condition(cpu);

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_CONT_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_CONT_RETURN;
    effective_report->current_registry_address = VF2_GAME_DISP_REGISTRY;
    effective_report->recovered_instruction_count = continuation_instructions;
    effective_report->recovered_procedure_calls = VF2_GAME_DISP_CONT_CALLS;
    effective_report->recovered_procedure_returns = VF2_GAME_DISP_CONT_RETURNS;
    ++state->blocks_executed;
    state->recovered_instruction_count += continuation_instructions;
    state->recovered_procedure_calls += VF2_GAME_DISP_CONT_CALLS;
    state->recovered_procedure_returns += VF2_GAME_DISP_CONT_RETURNS;
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_continuation_case(machine, cpu)) {
        return execute_measured_game_disp_continuation(
            machine, cpu, state, report
        );
    }
    return vf2_native_runtime_step_cont_base(machine, cpu, state, report);
}
