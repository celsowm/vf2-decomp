#define VF2_GAME_DISP_EVENT_FLAG13 UINT32_C(0x00002000)
#define VF2_GAME_DISP_EVENT_FLAG13_VALUE UINT32_C(0x005000a2)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE UINT32_C(0x0050004c)
#define VF2_GAME_DISP_EVENT_FLAG13_MODE UINT32_C(0x00500066)
#define VF2_GAME_DISP_EVENT_FLAG13_ZERO_INSTRUCTIONS UINT64_C(31)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE0_SINGLE_INSTRUCTIONS UINT64_C(41)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE0_DOUBLE_INSTRUCTIONS UINT64_C(48)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE1_SINGLE_INSTRUCTIONS UINT64_C(43)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE1_DOUBLE_INSTRUCTIONS UINT64_C(50)
#define VF2_GAME_DISP_EVENT_FLAG13_ZERO_LINK UINT32_C(0x0002aea8)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE0_LINK UINT32_C(0x0002aed8)
#define VF2_GAME_DISP_EVENT_FLAG13_SIDE1_LINK UINT32_C(0x0002af10)

static const uint8_t vf2_game_disp_event_flag13_prefix[] = {
    UINT8_C(0x1f), UINT8_C(0x3f), UINT8_C(0x5f), UINT8_C(0x7f)
};

static bool game_disp_event_flag13_machine_case(
    vf2_model2a *machine,
    uint16_t *value_out,
    uint8_t *side_out,
    uint8_t *mode_out
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t registry_flags = 0u;
    uint16_t value = UINT16_MAX;
    uint8_t queue_head = UINT8_MAX;
    uint8_t queue_tail = UINT8_MAX;
    uint8_t timer0 = UINT8_MAX;
    uint8_t timer1 = UINT8_MAX;
    uint8_t selector = UINT8_MAX;
    uint8_t state_byte = UINT8_MAX;
    uint8_t side = UINT8_MAX;
    uint8_t mode = UINT8_MAX;
    vf2_status status = VF2_OK;

    if (machine == NULL || value_out == NULL || side_out == NULL || mode_out == NULL) {
        return false;
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
        status = game_disp_leaf_read_u16(
            machine, VF2_GAME_DISP_EVENT_FLAG13_VALUE, &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG13_SIDE, &side, sizeof(side)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG13_MODE, &mode, sizeof(mode)
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
        registry_flags !=
            (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG13) ||
        queue_head != UINT8_C(0) || queue_tail != UINT8_C(0) ||
        timer0 != UINT8_C(0) || timer1 != UINT8_C(0) ||
        value > UINT16_C(99) || side > UINT8_C(1) || mode > UINT8_C(1) ||
        selector != UINT8_C(17) || state_byte != UINT8_C(0)) {
        return false;
    }

    *value_out = value;
    *side_out = side;
    *mode_out = mode;
    return true;
}

static bool measured_game_disp_event_flag13_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint16_t *value_out,
    uint8_t *side_out,
    uint8_t *mode_out
)
{
    size_t index = 0u;

    if (machine == NULL || cpu == NULL ||
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

    return game_disp_event_flag13_machine_case(
        machine, value_out, side_out, mode_out
    );
}

static vf2_status game_disp_event_flag13_enqueue(
    vf2_model2a *machine,
    uint8_t *queue_head,
    uint16_t value,
    uint8_t side,
    uint64_t *instruction_delta_out,
    uint32_t *link_out
)
{
    uint8_t tens = UINT8_C(0);
    uint8_t ones = UINT8_C(0);
    vf2_status status = VF2_OK;

    if (machine == NULL || queue_head == NULL ||
        instruction_delta_out == NULL || link_out == NULL ||
        value > UINT16_C(99) || side > UINT8_C(1)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = game_disp_event_enqueue_sequence(
        machine,
        queue_head,
        vf2_game_disp_event_flag13_prefix,
        sizeof(vf2_game_disp_event_flag13_prefix)
    );
    if (status != VF2_OK) {
        return status;
    }

    if (value == UINT16_C(0)) {
        *instruction_delta_out = VF2_GAME_DISP_EVENT_FLAG13_ZERO_INSTRUCTIONS;
        *link_out = VF2_GAME_DISP_EVENT_FLAG13_ZERO_LINK;
        return VF2_OK;
    }

    tens = (uint8_t)(value / UINT16_C(10));
    ones = (uint8_t)(value % UINT16_C(10));
    if (side == UINT8_C(0)) {
        if (tens != UINT8_C(0)) {
            status = game_disp_event_enqueue_byte(
                machine, queue_head, (uint8_t)(UINT8_C(0x20) + tens)
            );
        }
        if (status == VF2_OK) {
            status = game_disp_event_enqueue_byte(machine, queue_head, ones);
        }
        *instruction_delta_out =
            tens == UINT8_C(0)
                ? VF2_GAME_DISP_EVENT_FLAG13_SIDE0_SINGLE_INSTRUCTIONS
                : VF2_GAME_DISP_EVENT_FLAG13_SIDE0_DOUBLE_INSTRUCTIONS;
        *link_out = VF2_GAME_DISP_EVENT_FLAG13_SIDE0_LINK;
    } else {
        if (tens != UINT8_C(0)) {
            status = game_disp_event_enqueue_byte(
                machine, queue_head, (uint8_t)(UINT8_C(0x60) + tens)
            );
        }
        if (status == VF2_OK) {
            status = game_disp_event_enqueue_byte(
                machine, queue_head, (uint8_t)(UINT8_C(0x40) + ones)
            );
        }
        *instruction_delta_out =
            tens == UINT8_C(0)
                ? VF2_GAME_DISP_EVENT_FLAG13_SIDE1_SINGLE_INSTRUCTIONS
                : VF2_GAME_DISP_EVENT_FLAG13_SIDE1_DOUBLE_INSTRUCTIONS;
        *link_out = VF2_GAME_DISP_EVENT_FLAG13_SIDE1_LINK;
    }
    return status;
}

static vf2_status execute_measured_game_disp_event_flag13(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    uint16_t value,
    uint8_t side
)
{
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t queue_head = UINT8_C(0);
    uint8_t queue_tail = UINT8_C(0);
    uint8_t queued_event = UINT8_C(0);
    uint64_t instruction_delta = UINT64_C(0);
    uint32_t link = 0u;
    vf2_status status = game_disp_event_flag13_enqueue(
        machine, &queue_head, value, side, &instruction_delta, &link
    );

    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET,
            &queued_event,
            sizeof(queued_event)
        );
    }
    if (status == VF2_OK) {
        queue_tail = UINT8_C(1);
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_OUTPUT_PORT,
            &queued_event,
            sizeof(queued_event)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            &queue_head,
            sizeof(queue_head)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            &queue_tail,
            sizeof(queue_tail)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 7u] = VF2_GAME_DISP_EVENT_FIGHTER0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = VF2_GAME_DISP_EVENT_FIGHTER1;
    cpu->registers[VF2_I960_G14_REGISTER] = link;
    cpu->executed_instructions +=
        VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS + instruction_delta +
        VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_GAME_DISP_EVENT_RETURN ||
        cpu->procedure_calls != start_calls ||
        cpu->procedure_returns != start_returns + UINT64_C(1)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    set_game_disp_event_queue_condition(cpu, queue_head, UINT8_C(0));

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

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint16_t value = UINT16_MAX;
    uint8_t side = UINT8_MAX;
    uint8_t mode = UINT8_MAX;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_event_flag13_case(
            machine, cpu, &value, &side, &mode
        )) {
        (void)mode;
        return execute_measured_game_disp_event_flag13(
            machine, cpu, state, report, value, side
        );
    }
    return vf2_native_runtime_step_event_queue_base(
        machine, cpu, state, report
    );
}
