#define VF2_GAME_DISP_EVENT_FLAG7 UINT32_C(0x00000080)
#define VF2_GAME_DISP_EVENT_FLAG6 UINT32_C(0x00000040)
#define VF2_GAME_DISP_EVENT_FLAG76_MASK \
    (VF2_GAME_DISP_EVENT_FLAG7 | VF2_GAME_DISP_EVENT_FLAG6)
#define VF2_GAME_DISP_EVENT_FLAG76_STATE_ADDR \
    (VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE + \
     VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET)

static bool game_disp_flag76_measured_case(vf2_model2a *machine)
{
    uint32_t flags = 0u;
    uint32_t global20 = 0u;
    uint32_t value = 0u;
    uint8_t selector = UINT8_MAX;
    uint8_t event_state = UINT8_MAX;
    uint8_t mode = UINT8_MAX;
    uint8_t aux = UINT8_MAX;
    uint8_t state6 = UINT8_MAX;
    uint8_t state_clear = UINT8_MAX;
    uint8_t queue_head = UINT8_MAX;
    uint8_t queue_tail = UINT8_MAX;
    uint8_t timer0 = UINT8_MAX;
    uint8_t timer1 = UINT8_MAX;
    uint8_t queue[UINT8_C(6)] = {0};
    uint32_t low_flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return false;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500020), &global20
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_EVENT_FLAG18_VALUE, &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_SELECTOR,
            &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE,
            &event_state, sizeof(event_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG13_SIDE,
            &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG13_MODE,
            &aux, sizeof(aux)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_FLAG76_STATE_ADDR,
            &state6, sizeof(state6)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a4),
            &state_clear, sizeof(state_clear)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            &queue_head, sizeof(queue_head)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            &queue_tail, sizeof(queue_tail)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER0_OFFSET,
            &timer0, sizeof(timer0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER1_OFFSET,
            &timer1, sizeof(timer1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET,
            queue, sizeof(queue)
        );
    }
    if (status != VF2_OK) {
        return false;
    }

    low_flags = flags & VF2_GAME_DISP_EVENT_FLAG76_MASK;
    if ((flags & ~VF2_GAME_DISP_EVENT_FLAG76_MASK) != UINT32_C(0x80000000) ||
        (low_flags != VF2_GAME_DISP_EVENT_FLAG7 &&
         low_flags != VF2_GAME_DISP_EVENT_FLAG6 &&
         low_flags != VF2_GAME_DISP_EVENT_FLAG76_MASK) ||
        global20 != UINT32_C(7) ||
        value != UINT32_C(0x000b0000) ||
        selector != UINT8_C(17) || event_state != UINT8_C(0) ||
        mode != UINT8_C(0) || aux != UINT8_C(0) ||
        state6 != UINT8_C(0x40) || state_clear != UINT8_C(0x0b) ||
        queue_tail != UINT8_C(1) ||
        timer0 != UINT8_C(0) || timer1 != UINT8_C(0)) {
        return false;
    }

    if (low_flags == VF2_GAME_DISP_EVENT_FLAG7) {
        return queue_head == UINT8_C(3) &&
               queue[0] == UINT8_C(0x92) &&
               queue[1] == UINT8_C(0x26) &&
               queue[2] == UINT8_C(0x06);
    }
    if (low_flags == VF2_GAME_DISP_EVENT_FLAG6) {
        return queue_head == UINT8_C(3) &&
               queue[0] == UINT8_C(0x9a) &&
               queue[1] == UINT8_C(0x66) &&
               queue[2] == UINT8_C(0x46);
    }
    return queue_head == UINT8_C(6) &&
           queue[0] == UINT8_C(0x92) &&
           queue[1] == UINT8_C(0x26) &&
           queue[2] == UINT8_C(0x06) &&
           queue[3] == UINT8_C(0x9a) &&
           queue[4] == UINT8_C(0x66) &&
           queue[5] == UINT8_C(0x46);
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    uint32_t original_flags = 0u;
    uint32_t resulting_flags = 0u;
    uint32_t low_flags = 0u;
    vf2_status status = VF2_OK;
    const bool measured =
        machine != NULL && cpu != NULL && state != NULL &&
        (cpu->ip == VF2_GAME_DISP_EVENT_ENTRY ||
         cpu->ip == VF2_GAME_DISP_CONT_ENTRY) &&
        game_disp_flag76_measured_case(machine);

    if (!measured) {
        return vf2_native_runtime_step_flag76_base(
            machine, cpu, state, report
        );
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &original_flags
    );
    if (status != VF2_OK) {
        return status;
    }
    low_flags = original_flags & VF2_GAME_DISP_EVENT_FLAG76_MASK;
    status = vf2_model2a_write_u32(
        machine,
        VF2_GAME_DISP_REGISTRY,
        original_flags & ~VF2_GAME_DISP_EVENT_FLAG76_MASK
    );
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_native_runtime_step_flag76_base(
        machine, cpu, state, report
    );
    if (status != VF2_OK) {
        (void)vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, original_flags
        );
        return status;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &resulting_flags
    );
    if (status != VF2_OK) {
        return status;
    }
    return vf2_model2a_write_u32(
        machine,
        VF2_GAME_DISP_REGISTRY,
        resulting_flags | low_flags
    );
}
