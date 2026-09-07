#define VF2_GAME_DISP_EVENT_ENTRY UINT32_C(0x0002ab94)
#define VF2_GAME_DISP_EVENT_RETURN UINT32_C(0x0002b260)
#define VF2_GAME_DISP_EVENT_FIGHTER0 UINT32_C(0x00510980)
#define VF2_GAME_DISP_EVENT_FIGHTER1 UINT32_C(0x00512980)
#define VF2_GAME_DISP_EVENT_FIGHTER0_SLOT UINT32_C(0x00500804)
#define VF2_GAME_DISP_EVENT_FIGHTER1_SLOT UINT32_C(0x00500808)
#define VF2_GAME_DISP_EVENT_SELECTOR UINT32_C(0x0050002b)
#define VF2_GAME_DISP_EVENT_STATE UINT32_C(0x00500031)
#define VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET UINT32_C(0x0000005b)
#define VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET UINT32_C(0x0000005c)
#define VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET UINT32_C(0x0000006c)
#define VF2_GAME_DISP_EVENT_TIMER0_OFFSET UINT32_C(0x0000006d)
#define VF2_GAME_DISP_EVENT_TIMER1_OFFSET UINT32_C(0x0000006e)
#define VF2_GAME_DISP_EVENT_OUTPUT_PORT UINT32_C(0x01c00008)
#define VF2_GAME_DISP_EVENT_QUEUE_CAPACITY UINT8_C(16)
#define VF2_GAME_DISP_EVENT_QUEUE_MASK UINT8_C(15)
#define VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS UINT64_C(38)
#define VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS UINT64_C(4)
#define VF2_GAME_DISP_EVENT_FLAG19 UINT32_C(0x00080000)
#define VF2_GAME_DISP_EVENT_FLAG19_INSTRUCTIONS UINT64_C(62)
#define VF2_GAME_DISP_EVENT_FLAG19_LINK UINT32_C(0x0002ac04)
#define VF2_GAME_DISP_EVENT_FLAG17 UINT32_C(0x00020000)
#define VF2_GAME_DISP_EVENT_FLAG17_INSTRUCTIONS UINT64_C(38)
#define VF2_GAME_DISP_EVENT_FLAG17_LINK UINT32_C(0x0002ac30)
#define VF2_GAME_DISP_EVENT_FLAG16 UINT32_C(0x00010000)
#define VF2_GAME_DISP_EVENT_FLAG16_INSTRUCTIONS UINT64_C(30)
#define VF2_GAME_DISP_EVENT_FLAG16_LINK UINT32_C(0x0002ac5c)
#define VF2_GAME_DISP_EVENT_FLAG10 UINT32_C(0x00000400)
#define VF2_GAME_DISP_EVENT_FLAG10_INSTRUCTIONS UINT64_C(13)
#define VF2_GAME_DISP_EVENT_FLAG10_LINK UINT32_C(0x0002ac74)

static const uint8_t vf2_game_disp_event_flag19_sequence[] = {
    UINT8_C(0x1f), UINT8_C(0x3f), UINT8_C(0x5f), UINT8_C(0x7f),
    UINT8_C(0x97), UINT8_C(0x9f), UINT8_C(0x8f)
};
static const uint8_t vf2_game_disp_event_flag17_sequence[] = {
    UINT8_C(0x1f), UINT8_C(0x3f), UINT8_C(0x5f), UINT8_C(0x7f)
};
static const uint8_t vf2_game_disp_event_flag16_sequence[] = {
    UINT8_C(0x97), UINT8_C(0x9f), UINT8_C(0x8f)
};

static bool measured_game_disp_event_gate_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint8_t *queue_head_out,
    uint8_t *queue_tail_out,
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

    if (machine == NULL || cpu == NULL || queue_head_out == NULL ||
        queue_tail_out == NULL || timer0_out == NULL || timer1_out == NULL ||
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
        queue_head >= VF2_GAME_DISP_EVENT_QUEUE_CAPACITY ||
        queue_tail >= VF2_GAME_DISP_EVENT_QUEUE_CAPACITY ||
        selector != UINT8_C(17) || state_byte != UINT8_C(0)) {
        return false;
    }
    if (registry_flags == UINT32_C(0x80000000)) {
        /* The recovered ring queue admits every valid measured index pair. */
    } else if (registry_flags ==
                   (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG10)) {
        if (queue_head != UINT8_C(0) || queue_tail != UINT8_C(0)) {
            return false;
        }
    } else if (registry_flags ==
                   (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG16) ||
               registry_flags ==
                   (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG17) ||
               registry_flags ==
                   (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG19)) {
        if (queue_head != UINT8_C(0) || queue_tail != UINT8_C(0) ||
            timer0 != UINT8_C(0) || timer1 != UINT8_C(0)) {
            return false;
        }
    } else {
        return false;
    }

    *queue_head_out = queue_head;
    *queue_tail_out = queue_tail;
    *timer0_out = timer0;
    *timer1_out = timer1;
    return true;
}

static void set_game_disp_event_queue_condition(
    vf2_i960_cpu *cpu,
    uint8_t queue_head,
    uint8_t queue_tail
)
{
    vf2_i960_compare_result result = VF2_I960_COMPARE_EQUAL;
    uint32_t bits = UINT32_C(2);

    if (queue_head < queue_tail) {
        result = VF2_I960_COMPARE_LESS;
        bits = UINT32_C(4);
    } else if (queue_head > queue_tail) {
        result = VF2_I960_COMPARE_GREATER;
        bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

static vf2_status game_disp_event_enqueue_byte(
    vf2_model2a *machine,
    uint8_t *queue_head,
    uint8_t value
)
{
    vf2_status status = vf2_model2a_write(
        machine,
        VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET +
            (uint32_t)*queue_head,
        &value,
        sizeof(value)
    );

    if (status == VF2_OK) {
        *queue_head = (uint8_t)((*queue_head + UINT8_C(1)) &
                                VF2_GAME_DISP_EVENT_QUEUE_MASK);
    }
    return status;
}

static vf2_status game_disp_event_enqueue_sequence(
    vf2_model2a *machine,
    uint8_t *queue_head,
    const uint8_t *sequence,
    size_t sequence_size
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; index < sequence_size && status == VF2_OK; ++index) {
        status = game_disp_event_enqueue_byte(
            machine, queue_head, sequence[index]
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_event_gate_state(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    uint8_t queue_head,
    uint8_t queue_tail,
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
    uint8_t compared_queue_tail = queue_tail;
    uint32_t registry_flags = 0u;
    uint64_t instruction_count = VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS;
    vf2_status status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &registry_flags
    );

    if (status != VF2_OK) {
        return status;
    }
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

    if ((registry_flags & VF2_GAME_DISP_EVENT_FLAG19) != 0u) {
        registry_flags &= ~VF2_GAME_DISP_EVENT_FLAG19;
        status = game_disp_event_enqueue_sequence(
            machine,
            &queue_head,
            vf2_game_disp_event_flag19_sequence,
            sizeof(vf2_game_disp_event_flag19_sequence)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G14_REGISTER] = VF2_GAME_DISP_EVENT_FLAG19_LINK;
        instruction_count += VF2_GAME_DISP_EVENT_FLAG19_INSTRUCTIONS;
    }
    if ((registry_flags & VF2_GAME_DISP_EVENT_FLAG17) != 0u) {
        registry_flags &= ~VF2_GAME_DISP_EVENT_FLAG17;
        status = game_disp_event_enqueue_sequence(
            machine,
            &queue_head,
            vf2_game_disp_event_flag17_sequence,
            sizeof(vf2_game_disp_event_flag17_sequence)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G14_REGISTER] = VF2_GAME_DISP_EVENT_FLAG17_LINK;
        instruction_count += VF2_GAME_DISP_EVENT_FLAG17_INSTRUCTIONS;
    }
    if ((registry_flags & VF2_GAME_DISP_EVENT_FLAG16) != 0u) {
        registry_flags &= ~VF2_GAME_DISP_EVENT_FLAG16;
        status = game_disp_event_enqueue_sequence(
            machine,
            &queue_head,
            vf2_game_disp_event_flag16_sequence,
            sizeof(vf2_game_disp_event_flag16_sequence)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G14_REGISTER] = VF2_GAME_DISP_EVENT_FLAG16_LINK;
        instruction_count += VF2_GAME_DISP_EVENT_FLAG16_INSTRUCTIONS;
    }
    if ((registry_flags & VF2_GAME_DISP_EVENT_FLAG10) != 0u) {
        registry_flags &= ~VF2_GAME_DISP_EVENT_FLAG10;
        status = game_disp_event_enqueue_byte(machine, &queue_head, UINT8_C(0xa0));
        if (status == VF2_OK) {
            status = game_disp_event_enqueue_byte(
                machine, &queue_head, UINT8_C(0xa8)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G14_REGISTER] = VF2_GAME_DISP_EVENT_FLAG10_LINK;
        instruction_count += VF2_GAME_DISP_EVENT_FLAG10_INSTRUCTIONS;
    }

    compared_queue_tail = queue_tail;
    if (queue_head != queue_tail) {
        uint8_t queued_event = UINT8_C(0);
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET +
                (uint32_t)queue_tail,
            &queued_event,
            sizeof(queued_event)
        );
        if (status == VF2_OK) {
            queue_tail = (uint8_t)((queue_tail + UINT8_C(1)) &
                                   VF2_GAME_DISP_EVENT_QUEUE_MASK);
            status = vf2_model2a_write(
                machine,
                VF2_GAME_DISP_EVENT_OUTPUT_PORT,
                &queued_event,
                sizeof(queued_event)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        instruction_count += VF2_GAME_DISP_EVENT_QUEUE_INSTRUCTIONS;
    }

    status = vf2_model2a_write(
        machine,
        VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
        &queue_head,
        sizeof(queue_head)
    );
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
            machine, VF2_GAME_DISP_REGISTRY, registry_flags
        );
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
    set_game_disp_event_queue_condition(cpu, queue_head, compared_queue_tail);

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
    uint8_t queue_head = UINT8_C(0);
    uint8_t queue_tail = UINT8_C(0);
    uint8_t timer0 = UINT8_C(0);
    uint8_t timer1 = UINT8_C(0);

    if (machine == NULL || cpu == NULL || state == NULL ||
        !measured_game_disp_event_gate_case(
            machine, cpu, &queue_head, &queue_tail, &timer0, &timer1
        )) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return execute_measured_game_disp_event_gate_state(
        machine, cpu, state, report, queue_head, queue_tail, timer0, timer1
    );
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint8_t queue_head = UINT8_C(0);
    uint8_t queue_tail = UINT8_C(0);
    uint8_t timer0 = UINT8_C(0);
    uint8_t timer1 = UINT8_C(0);

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_event_gate_case(
            machine, cpu, &queue_head, &queue_tail, &timer0, &timer1
        )) {
        return execute_measured_game_disp_event_gate_state(
            machine, cpu, state, report, queue_head, queue_tail, timer0, timer1
        );
    }
    return vf2_native_runtime_step_event_base(
        machine, cpu, state, report
    );
}