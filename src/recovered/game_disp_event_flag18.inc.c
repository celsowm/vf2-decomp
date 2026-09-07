#define VF2_GAME_DISP_EVENT_FLAG18 UINT32_C(0x00040000)
#define VF2_GAME_DISP_EVENT_FLAG18_POINTER0_SLOT UINT32_C(0x0050083c)
#define VF2_GAME_DISP_EVENT_FLAG18_POINTER1_SLOT UINT32_C(0x00500840)
#define VF2_GAME_DISP_EVENT_FLAG18_POINTER0 UINT32_C(0x00515c00)
#define VF2_GAME_DISP_EVENT_FLAG18_POINTER1 UINT32_C(0x00515c80)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE_SLOT UINT32_C(0x0050016c)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE UINT32_C(0x00599000)
#define VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET UINT32_C(0x00003351)
#define VF2_GAME_DISP_EVENT_FLAG18_VALUE UINT32_C(0x005000a2)
#define VF2_GAME_DISP_EVENT_FLAG18_REGISTRY_IMAGE_SIZE UINT32_C(0x70)

typedef struct game_disp_flag18_case {
    bool side0_active;
    bool side1_active;
    bool secondary;
    uint64_t child_instructions;
    uint64_t full_instructions;
    uint32_t link;
} game_disp_flag18_case;

typedef struct game_disp_flag18_image {
    uint8_t registry[VF2_GAME_DISP_EVENT_FLAG18_REGISTRY_IMAGE_SIZE];
    uint8_t output;
} game_disp_flag18_image;

static bool game_disp_flag18_value_is_measured(uint32_t value)
{
    return value == UINT32_C(0) || value == UINT32_C(1) ||
           value == UINT32_C(0x000b0000) ||
           value == UINT32_C(0x000b0001);
}

static bool game_disp_flag18_machine_case(
    vf2_model2a *machine,
    game_disp_flag18_case *result
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t flags = 0u;
    uint32_t pointer0 = 0u;
    uint32_t pointer1 = 0u;
    uint32_t pointed0 = 0u;
    uint32_t pointed1 = 0u;
    uint32_t state_base = 0u;
    uint32_t value = 0u;
    uint8_t selector = UINT8_MAX;
    uint8_t state = UINT8_MAX;
    uint8_t mode = UINT8_MAX;
    uint8_t aux = UINT8_MAX;
    uint8_t queue_head = UINT8_MAX;
    uint8_t queue_tail = UINT8_MAX;
    uint8_t timer0 = UINT8_MAX;
    uint8_t timer1 = UINT8_MAX;
    uint8_t state_flags = UINT8_MAX;
    vf2_status status = VF2_OK;

    if (machine == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_EVENT_FIGHTER0_SLOT, &fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FIGHTER1_SLOT, &fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_REGISTRY, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FLAG18_POINTER0_SLOT, &pointer0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FLAG18_POINTER1_SLOT, &pointer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer0, &pointed0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, pointer1, &pointed1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FLAG18_STATE_SLOT, &state_base
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            state_base + VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &state_flags,
            sizeof(state_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FLAG18_VALUE, &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_SELECTOR, &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE, &state, sizeof(state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG13_SIDE, &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG13_MODE, &aux, sizeof(aux)
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

    if (status != VF2_OK ||
        fighter0 != VF2_GAME_DISP_EVENT_FIGHTER0 ||
        fighter1 != VF2_GAME_DISP_EVENT_FIGHTER1 ||
        flags != (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG18) ||
        pointer0 != VF2_GAME_DISP_EVENT_FLAG18_POINTER0 ||
        pointer1 != VF2_GAME_DISP_EVENT_FLAG18_POINTER1 ||
        (pointed0 != UINT32_C(8) && pointed0 != UINT32_C(9)) ||
        (pointed1 != UINT32_C(8) && pointed1 != UINT32_C(9)) ||
        state_base != VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE ||
        state_flags != UINT8_C(0) ||
        !game_disp_flag18_value_is_measured(value) ||
        selector != UINT8_C(17) || state != UINT8_C(0) ||
        mode != UINT8_C(0) || aux != UINT8_C(0) ||
        queue_head != UINT8_C(0) || queue_tail != UINT8_C(0) ||
        timer0 != UINT8_C(0) || timer1 != UINT8_C(0)) {
        return false;
    }

    result->side0_active = (pointed0 & UINT32_C(1)) != 0u;
    result->side1_active = (pointed1 & UINT32_C(1)) != 0u;
    result->secondary = value != UINT32_C(0);

    if (!result->side0_active && !result->side1_active) {
        result->child_instructions = UINT64_C(44);
        result->link = UINT32_C(0x00000220);
    } else if (result->side0_active && result->side1_active) {
        result->child_instructions =
            result->secondary ? UINT64_C(85) : UINT64_C(73);
        result->link = result->secondary
                           ? UINT32_C(0x0002ae0c)
                           : UINT32_C(0x0002adf8);
    } else if (result->side0_active) {
        result->child_instructions =
            result->secondary ? UINT64_C(68) : UINT64_C(62);
        result->link = result->secondary
                           ? UINT32_C(0x0002add0)
                           : UINT32_C(0x0002adbc);
    } else {
        result->child_instructions =
            result->secondary ? UINT64_C(68) : UINT64_C(62);
        result->link = result->secondary
                           ? UINT32_C(0x0002ae0c)
                           : UINT32_C(0x0002adf8);
    }
    result->full_instructions =
        VF2_GAME_DISP_CONT_INSTRUCTIONS +
        result->child_instructions - VF2_GAME_DISP_EVENT_BASE_INSTRUCTIONS;
    return true;
}

static bool measured_game_disp_event_flag18_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    game_disp_flag18_case *result
)
{
    size_t index = 0u;

    if (machine == NULL || cpu == NULL || result == NULL ||
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
    return game_disp_flag18_machine_case(machine, result);
}

static bool measured_game_disp_continuation_flag18_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    game_disp_flag18_case *result
)
{
    uint32_t measured_flags = 0u;
    bool base_case = false;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || result == NULL ||
        cpu->ip != VF2_GAME_DISP_CONT_ENTRY ||
        !game_disp_flag18_machine_case(machine, result)) {
        return false;
    }
    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &measured_flags
    );
    if (status != VF2_OK) {
        return false;
    }
    status = vf2_model2a_write_u32(
        machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
    );
    if (status == VF2_OK) {
        base_case = measured_game_disp_continuation_case(machine, cpu);
    }
    (void)vf2_model2a_write_u32(
        machine, VF2_GAME_DISP_REGISTRY, measured_flags
    );
    return status == VF2_OK && base_case;
}

static vf2_status game_disp_flag18_read_image(
    vf2_model2a *machine,
    game_disp_flag18_image *image
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || image == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(
        machine,
        VF2_GAME_DISP_REGISTRY,
        image->registry,
        sizeof(image->registry)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_EVENT_OUTPUT_PORT,
            &image->output,
            sizeof(image->output)
        );
    }
    return status;
}

static vf2_status game_disp_flag18_write_image(
    vf2_model2a *machine,
    const game_disp_flag18_image *image
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || image == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_write(
        machine,
        VF2_GAME_DISP_REGISTRY,
        image->registry,
        sizeof(image->registry)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_OUTPUT_PORT,
            &image->output,
            sizeof(image->output)
        );
    }
    return status;
}

static vf2_status game_disp_flag18_apply_transition(
    vf2_model2a *machine,
    const game_disp_flag18_case *match
)
{
    uint32_t flags = UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG18;
    uint8_t queue_head = UINT8_C(0);
    uint8_t queue_tail = UINT8_C(0);
    uint8_t output = UINT8_C(0);
    vf2_status status = VF2_OK;

    if (machine == NULL || match == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (match->side0_active || match->side1_active) {
        flags &= ~VF2_GAME_DISP_EVENT_FLAG18;
    }
    if (match->side0_active) {
        status = game_disp_event_enqueue_byte(
            machine, &queue_head, UINT8_C(0xa1)
        );
        if (status == VF2_OK && match->secondary) {
            status = game_disp_event_enqueue_byte(
                machine, &queue_head, UINT8_C(0x9a)
            );
        }
    }
    if (status == VF2_OK && match->side1_active) {
        status = game_disp_event_enqueue_byte(
            machine, &queue_head, UINT8_C(0xa9)
        );
        if (status == VF2_OK && match->secondary) {
            status = game_disp_event_enqueue_byte(
                machine, &queue_head, UINT8_C(0x92)
            );
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    if (queue_head != queue_tail) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET +
                (uint32_t)queue_tail,
            &output,
            sizeof(output)
        );
        if (status == VF2_OK) {
            queue_tail = UINT8_C(1);
            status = vf2_model2a_write(
                machine,
                VF2_GAME_DISP_EVENT_OUTPUT_PORT,
                &output,
                sizeof(output)
            );
        }
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
            machine, VF2_GAME_DISP_REGISTRY, flags
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_event_flag18(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    const game_disp_flag18_case *match
)
{
    game_disp_flag18_image original;
    game_disp_flag18_image desired;
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    uint64_t delta = UINT64_C(0);
    vf2_status status = game_disp_flag18_read_image(machine, &original);

    if (status == VF2_OK) {
        status = game_disp_flag18_apply_transition(machine, match);
    }
    if (status == VF2_OK) {
        status = game_disp_flag18_read_image(machine, &desired);
    }
    if (status == VF2_OK) {
        status = game_disp_flag18_write_image(machine, &original);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = execute_measured_game_disp_event_gate_state(
            machine,
            cpu,
            state,
            effective_report,
            UINT8_C(0), UINT8_C(0), UINT8_C(0), UINT8_C(0)
        );
    }
    if (status == VF2_OK) {
        status = game_disp_flag18_write_image(machine, &desired);
    }
    if (status != VF2_OK) {
        return status;
    }

    if (match->child_instructions < effective_report->recovered_instruction_count) {
        return VF2_ERROR_UNSUPPORTED;
    }
    delta = match->child_instructions - effective_report->recovered_instruction_count;
    cpu->executed_instructions += delta;
    cpu->registers[VF2_I960_G14_REGISTER] = match->link;
    state->recovered_instruction_count += delta;
    effective_report->recovered_instruction_count += delta;
    set_game_disp_event_queue_condition(
        cpu,
        desired.registry[VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET],
        UINT8_C(0)
    );
    return VF2_OK;
}

static vf2_status execute_measured_game_disp_continuation_flag18(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    const game_disp_flag18_case *match
)
{
    game_disp_flag18_image original;
    game_disp_flag18_image desired;
    uint64_t delta = UINT64_C(0);
    vf2_status status = game_disp_flag18_read_image(machine, &original);

    if (status == VF2_OK) {
        status = game_disp_flag18_apply_transition(machine, match);
    }
    if (status == VF2_OK) {
        status = game_disp_flag18_read_image(machine, &desired);
    }
    if (status == VF2_OK) {
        status = game_disp_flag18_write_image(machine, &original);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = execute_measured_game_disp_continuation(
            machine, cpu, state, report
        );
    }
    if (status == VF2_OK) {
        status = game_disp_flag18_write_image(machine, &desired);
    }
    if (status != VF2_OK) {
        return status;
    }

    delta = match->full_instructions - VF2_GAME_DISP_CONT_INSTRUCTIONS;
    cpu->executed_instructions += delta;
    cpu->registers[VF2_I960_G14_REGISTER] = match->link;
    state->recovered_instruction_count += delta;
    if (report != NULL) {
        report->recovered_instruction_count += delta;
    }
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    game_disp_flag18_case match;

    if (machine != NULL && cpu != NULL && state != NULL) {
        if (measured_game_disp_event_flag18_case(machine, cpu, &match)) {
            return execute_measured_game_disp_event_flag18(
                machine, cpu, state, report, &match
            );
        }
        if (measured_game_disp_continuation_flag18_case(machine, cpu, &match)) {
            return execute_measured_game_disp_continuation_flag18(
                machine, cpu, state, report, &match
            );
        }
    }
    return vf2_native_runtime_step_condition_flag18_base(
        machine, cpu, state, report
    );
}
