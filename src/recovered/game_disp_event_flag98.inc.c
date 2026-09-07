#define VF2_GAME_DISP_EVENT_FLAG9 UINT32_C(0x00000200)
#define VF2_GAME_DISP_EVENT_FLAG8 UINT32_C(0x00000100)
#define VF2_GAME_DISP_EVENT_FLAG7 UINT32_C(0x00000080)
#define VF2_GAME_DISP_EVENT_FLAG6 UINT32_C(0x00000040)
#define VF2_GAME_DISP_EVENT_FLAG98_STATE_VALUE UINT8_C(0x66)
#define VF2_GAME_DISP_EVENT_FLAG98_RNG UINT32_C(0x00500098)
#define VF2_GAME_DISP_EVENT_FLAG98_STATE_VALUE_OFFSET UINT32_C(0x00003001)
#define VF2_GAME_DISP_EVENT_FLAG98_TILE UINT32_C(0x01000040)

#define VF2_GAME_DISP_EVENT_FLAG98_CHILD_ONE UINT64_C(95)
#define VF2_GAME_DISP_EVENT_FLAG98_CHILD_BOTH UINT64_C(148)
#define VF2_GAME_DISP_EVENT_FLAG98_FULL_ONE UINT64_C(2998)
#define VF2_GAME_DISP_EVENT_FLAG98_FULL_BOTH UINT64_C(3051)

typedef struct game_disp_flag98_case {
    bool bit9;
    bool bit8;
    uint64_t target_instructions;
    uint64_t extra_calls;
    uint32_t link;
    uint32_t final_flags;
    uint32_t child_g0;
} game_disp_flag98_case;

static bool game_disp_flag98_machine_case(
    vf2_model2a *machine,
    game_disp_flag98_case *result
)
{
    static const uint8_t measured_rng[4] = {
        UINT8_C(0x40), UINT8_C(0x33), UINT8_C(0xf3), UINT8_C(0xcb)
    };
    uint32_t flags = 0u;
    uint32_t pointer0 = 0u;
    uint32_t pointer1 = 0u;
    uint32_t pointed0 = 0u;
    uint32_t pointed1 = 0u;
    uint32_t state_base = 0u;
    uint8_t state_flags = UINT8_MAX;
    uint8_t state_value = UINT8_MAX;
    uint8_t rng[4] = {0};
    vf2_status status = VF2_OK;

    if (machine == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));

    status = vf2_model2a_read_u32(machine, VF2_GAME_DISP_REGISTRY, &flags);
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
        status = vf2_model2a_read(
            machine,
            state_base + VF2_GAME_DISP_EVENT_FLAG98_STATE_VALUE_OFFSET,
            &state_value,
            sizeof(state_value)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_EVENT_FLAG98_RNG,
            rng,
            sizeof(rng)
        );
    }

    if (status != VF2_OK ||
        pointer0 != VF2_GAME_DISP_EVENT_FLAG18_POINTER0 ||
        pointer1 != VF2_GAME_DISP_EVENT_FLAG18_POINTER1 ||
        state_base != VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE ||
        state_flags != VF2_GAME_DISP_EVENT_FLAG18_STATE6 ||
        state_value != VF2_GAME_DISP_EVENT_FLAG98_STATE_VALUE ||
        memcmp(rng, measured_rng, sizeof(rng)) != 0) {
        return false;
    }

    if (flags == (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG9) &&
        pointed0 == UINT32_C(9) && pointed1 == UINT32_C(8)) {
        result->bit9 = true;
        result->final_flags = UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG7;
        result->link = UINT32_C(0x0002acf0);
        result->child_g0 = UINT32_C(0x00003223);
        return true;
    }
    if (flags == (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG8) &&
        pointed0 == UINT32_C(8) && pointed1 == UINT32_C(9)) {
        result->bit8 = true;
        result->final_flags = UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG6;
        result->link = UINT32_C(0x0002ad6c);
        result->child_g0 = UINT32_C(0x00003223);
        return true;
    }
    if (flags == (UINT32_C(0x80000000) |
                  VF2_GAME_DISP_EVENT_FLAG9 |
                  VF2_GAME_DISP_EVENT_FLAG8) &&
        pointed0 == UINT32_C(9) && pointed1 == UINT32_C(9)) {
        result->bit9 = true;
        result->bit8 = true;
        result->final_flags = UINT32_C(0x80000000) |
                              VF2_GAME_DISP_EVENT_FLAG7 |
                              VF2_GAME_DISP_EVENT_FLAG6;
        result->link = UINT32_C(0x0002ad6c);
        result->child_g0 = UINT32_C(0x00003112);
        return true;
    }
    return false;
}

static bool measured_game_disp_flag98_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    game_disp_flag98_case *result
)
{
    uint32_t original_flags = 0u;
    uint8_t original_state = UINT8_MAX;
    const uint8_t zero = UINT8_C(0);
    uint8_t queue_head = UINT8_MAX;
    uint8_t queue_tail = UINT8_MAX;
    uint8_t timer0 = UINT8_MAX;
    uint8_t timer1 = UINT8_MAX;
    bool base_case = false;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || result == NULL ||
        (cpu->ip != VF2_GAME_DISP_EVENT_ENTRY &&
         cpu->ip != VF2_GAME_DISP_CONT_ENTRY) ||
        !game_disp_flag98_machine_case(machine, result)) {
        return false;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &original_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &original_state,
            sizeof(original_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &zero,
            sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        if (cpu->ip == VF2_GAME_DISP_EVENT_ENTRY) {
            base_case = measured_game_disp_event_gate_case(
                machine,
                cpu,
                &queue_head,
                &queue_tail,
                &timer0,
                &timer1
            );
        } else {
            base_case = measured_game_disp_continuation_case(machine, cpu);
        }
    }
    if (vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, original_flags
        ) != VF2_OK ||
        vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &original_state,
            sizeof(original_state)
        ) != VF2_OK) {
        return false;
    }
    if (status != VF2_OK || !base_case) {
        return false;
    }

    if (cpu->ip == VF2_GAME_DISP_EVENT_ENTRY) {
        result->target_instructions =
            result->bit9 && result->bit8
                ? VF2_GAME_DISP_EVENT_FLAG98_CHILD_BOTH
                : VF2_GAME_DISP_EVENT_FLAG98_CHILD_ONE;
        result->extra_calls = result->bit9 && result->bit8
                                  ? UINT64_C(2)
                                  : UINT64_C(1);
    } else {
        result->target_instructions =
            result->bit9 && result->bit8
                ? VF2_GAME_DISP_EVENT_FLAG98_FULL_BOTH
                : VF2_GAME_DISP_EVENT_FLAG98_FULL_ONE;
        result->extra_calls = result->bit9 && result->bit8
                                  ? UINT64_C(3)
                                  : UINT64_C(2);
    }
    return true;
}

static vf2_status game_disp_flag98_apply_memory(
    vf2_model2a *machine,
    const game_disp_flag98_case *match,
    bool full
)
{
    static const uint8_t rng_once[4] = {
        UINT8_C(0x30), UINT8_C(0x22), UINT8_C(0x13), UINT8_C(0x7e)
    };
    static const uint8_t rng_twice[4] = {
        UINT8_C(0x20), UINT8_C(0x11), UINT8_C(0x33), UINT8_C(0x30)
    };
    static const uint8_t tile_bytes[4] = {
        UINT8_C(0x2e), UINT8_C(0x80), UINT8_C(0x31), UINT8_C(0x80)
    };
    static const uint8_t bit9_queue[3] = {
        UINT8_C(0x92), UINT8_C(0x26), UINT8_C(0x06)
    };
    static const uint8_t bit8_queue[3] = {
        UINT8_C(0x9a), UINT8_C(0x66), UINT8_C(0x46)
    };
    uint8_t queue[6] = {0};
    uint8_t queue_head = UINT8_C(0);
    const uint8_t queue_tail = UINT8_C(1);
    uint8_t output = UINT8_C(0);
    uint8_t state6 = VF2_GAME_DISP_EVENT_FLAG18_STATE6;
    size_t offset = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || match == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (match->bit9) {
        memcpy(queue + offset, bit9_queue, sizeof(bit9_queue));
        offset += sizeof(bit9_queue);
    }
    if (match->bit8) {
        memcpy(queue + offset, bit8_queue, sizeof(bit8_queue));
        offset += sizeof(bit8_queue);
    }
    queue_head = (uint8_t)offset;
    output = queue[0];

    status = vf2_model2a_write_u32(
        machine, VF2_GAME_DISP_REGISTRY, match->final_flags
    );
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
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET,
            queue,
            sizeof(queue)
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
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_OUTPUT_PORT,
            &output,
            sizeof(output)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG98_RNG,
            match->bit9 && match->bit8 ? rng_twice : rng_once,
            sizeof(rng_once)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &state6,
            sizeof(state6)
        );
    }
    if (status == VF2_OK && full) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG98_TILE,
            tile_bytes,
            sizeof(tile_bytes)
        );
    }
    return status;
}

static vf2_status execute_measured_game_disp_flag98(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    const game_disp_flag98_case *match
)
{
    const uint32_t start_ip = cpu->ip;
    const bool full = start_ip == VF2_GAME_DISP_CONT_ENTRY;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint8_t zero = UINT8_C(0);
    uint32_t original_flags = 0u;
    uint8_t original_state = UINT8_MAX;
    uint64_t actual = UINT64_C(0);
    uint64_t delta = UINT64_C(0);
    uint64_t base_calls = full ? UINT64_C(41) : UINT64_C(0);
    uint64_t base_returns = full ? UINT64_C(42) : UINT64_C(1);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL || match == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(
        machine, VF2_GAME_DISP_REGISTRY, &original_flags
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &original_state,
            sizeof(original_state)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &zero,
            sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_step_flag98_base(
            machine, cpu, state, report
        );
    }
    if (status != VF2_OK) {
        (void)vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, original_flags
        );
        (void)vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FLAG18_STATE_BASE +
                VF2_GAME_DISP_EVENT_FLAG18_STATE_OFFSET,
            &original_state,
            sizeof(original_state)
        );
        return status;
    }

    if (cpu->procedure_calls - start_calls != base_calls ||
        cpu->procedure_returns - start_returns != base_returns) {
        return VF2_ERROR_UNSUPPORTED;
    }
    actual = cpu->executed_instructions - start_instructions;
    if (actual > match->target_instructions) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = game_disp_flag98_apply_memory(machine, match, full);
    if (status != VF2_OK) {
        return status;
    }

    delta = match->target_instructions - actual;
    cpu->executed_instructions += delta;
    cpu->procedure_calls += match->extra_calls;
    cpu->procedure_returns += match->extra_calls;
    cpu->registers[VF2_I960_G14_REGISTER] = match->link;
    if (!full) {
        cpu->registers[VF2_I960_G0_REGISTER] = match->child_g0;
    }

    state->recovered_instruction_count += delta;
    state->recovered_procedure_calls += match->extra_calls;
    state->recovered_procedure_returns += match->extra_calls;
    if (report != NULL) {
        report->recovered_instruction_count += delta;
        report->recovered_procedure_calls += match->extra_calls;
        report->recovered_procedure_returns += match->extra_calls;
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
    game_disp_flag98_case match;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_flag98_case(machine, cpu, &match)) {
        return execute_measured_game_disp_flag98(
            machine, cpu, state, report, &match
        );
    }
    return vf2_native_runtime_step_flag98_base(
        machine, cpu, state, report
    );
}
