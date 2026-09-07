#define VF2_GAME_DISP_EVENT_ENTRY UINT32_C(0x0002ab94)
#define VF2_GAME_DISP_EVENT_RETURN UINT32_C(0x0002b260)
#define VF2_GAME_DISP_EVENT_FIGHTER0 UINT32_C(0x00510980)
#define VF2_GAME_DISP_EVENT_FIGHTER1 UINT32_C(0x00512980)
#define VF2_GAME_DISP_EVENT_FIGHTER0_SLOT UINT32_C(0x00500804)
#define VF2_GAME_DISP_EVENT_FIGHTER1_SLOT UINT32_C(0x00500808)
#define VF2_GAME_DISP_EVENT_SELECTOR UINT32_C(0x0050002b)
#define VF2_GAME_DISP_EVENT_STATE UINT32_C(0x00500031)
#define VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET UINT32_C(0x0000005b)
#define VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET UINT32_C(0x0000006c)
#define VF2_GAME_DISP_EVENT_TIMER0_OFFSET UINT32_C(0x0000006d)
#define VF2_GAME_DISP_EVENT_TIMER1_OFFSET UINT32_C(0x0000006e)
#define VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS UINT64_C(38)

static bool measured_game_disp_event_gate_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint8_t *timer0_out,
    uint8_t *timer1_out
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t registry_flags = 0u;
    uint8_t queue_head = UINT8_MAX;
    uint8_t queue_tail = UINT8_MAX;
    uint8_t timer0 = UINT8_MAX;
    uint8_t timer1 = UINT8_MAX;
    uint8_t selector = UINT8_MAX;
    uint8_t state_byte = UINT8_MAX;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || timer0_out == NULL || timer1_out == NULL ||
        cpu->ip != VF2_GAME_DISP_EVENT_ENTRY ||
        cpu->local_frame_depth != UINT32_C(3) ||
        cpu->registers[VF2_I960_G0_REGISTER + 13u] != VF2_GAME_DISP_REGISTRY ||
        cpu->registers[VF2_I960_FP_REGISTER] != cpu->registers[0] + UINT32_C(0x40) ||
        cpu->registers[1] != cpu->registers[VF2_I960_FP_REGISTER] + UINT32_C(0x40) ||
        cpu->local_frames[2].registers[2] != VF2_GAME_DISP_EVENT_RETURN ||
        cpu->local_frames[2].registers[1] != cpu->registers[VF2_I960_FP_REGISTER]) {
        return false;
    }
    for (index = 2u; index < VF2_I960_LOCAL_REGISTER_COUNT; ++index) {
        if (cpu->registers[index] != 0u) {
            return false;
        }
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_EVENT_FIGHTER0_SLOT, &fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FIGHTER1_SLOT, &fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_REGISTRY, &registry_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            &queue_head,
            sizeof(queue_head)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            &queue_tail,
            sizeof(queue_tail)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER0_OFFSET,
            &timer0,
            sizeof(timer0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER1_OFFSET,
            &timer1,
            sizeof(timer1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_SELECTOR, &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE, &state_byte, sizeof(state_byte)
        );
    }

    if (status != VF2_OK ||
        fighter0 != VF2_GAME_DISP_EVENT_FIGHTER0 ||
        fighter1 != VF2_GAME_DISP_EVENT_FIGHTER1 ||
        registry_flags != UINT32_C(0x80000000) ||
        queue_head != UINT8_C(0) || queue_tail != UINT8_C(0) ||
        selector != UINT8_C(17) || state_byte != UINT8_C(0)) {
        return false;
    }

    *timer0_out = timer0;
    *timer1_out = timer1;
    return true;
}

static vf2_status execute_measured_game_disp_event_gate_timers(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    uint8_t timer0,
    uint8_t timer1
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint64_t instruction_count = VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS;
    vf2_status status = VF2_OK;

    if (timer0 != UINT8_C(0)) {
        const uint8_t stored[2] = {
            (uint8_t)(timer0 - UINT8_C(1)), UINT8_C(0)
        };
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER0_OFFSET,
            stored,
            sizeof(stored)
        );
        ++instruction_count;
    } else if (timer1 != UINT8_C(0)) {
        const uint8_t stored[2] = {
            (uint8_t)(timer1 - UINT8_C(1)), UINT8_C(0)
        };
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER1_OFFSET,
            stored,
            sizeof(stored)
        );
        ++instruction_count;
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 7u] = VF2_GAME_DISP_EVENT_FIGHTER0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = VF2_GAME_DISP_EVENT_FIGHTER1;
    cpu->executed_instructions += instruction_count;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_EVENT_RETURN ||
        cpu->procedure_calls != start_calls ||
        cpu->procedure_returns != start_returns + UINT64_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_runtime_equal_condition(cpu);

    memset(effective_report, 0, sizeof(*effective_report));
    effective_report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
    effective_report->task_kind = VF2_HYBRID_TASK_GAME_DISP;
    effective_report->entry_address = VF2_GAME_DISP_EVENT_ENTRY;
    effective_report->exit_address = VF2_GAME_DISP_EVENT_RETURN;
    effective_report->current_registry_address = VF2_GAME_DISP_REGISTRY;
    effective_report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    effective_report->recovered_procedure_calls =
        cpu->procedure_calls - start_calls;
    effective_report->recovered_procedure_returns =
        cpu->procedure_returns - start_returns;

    ++state->blocks_executed;
    state->recovered_instruction_count += effective_report->recovered_instruction_count;
    state->recovered_procedure_calls += effective_report->recovered_procedure_calls;
    state->recovered_procedure_returns += effective_report->recovered_procedure_returns;
    return VF2_OK;
}

static vf2_status execute_measured_game_disp_event_gate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint8_t timer0 = UINT8_C(0);
    uint8_t timer1 = UINT8_C(0);

    if (machine == NULL || cpu == NULL || state == NULL ||
        !measured_game_disp_event_gate_case(machine, cpu, &timer0, &timer1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return execute_measured_game_disp_event_gate_timers(
        machine, cpu, state, report, timer0, timer1
    );
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint8_t timer0 = UINT8_C(0);
    uint8_t timer1 = UINT8_C(0);

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_event_gate_case(machine, cpu, &timer0, &timer1)) {
        return execute_measured_game_disp_event_gate_timers(
            machine, cpu, state, report, timer0, timer1
        );
    }
    return vf2_native_runtime_step_event_base(
        machine, cpu, state, report
    );
}
