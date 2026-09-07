#define VF2_GAME_DISP_EVENT_FLAG12 UINT32_C(0x00001000)
#define VF2_GAME_DISP_EVENT_FLAG11 UINT32_C(0x00000800)
#define VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET UINT32_C(0x00001e20)
#define VF2_GAME_DISP_EVENT_STATE89_MODE UINT32_C(0x0050004c)
#define VF2_GAME_DISP_EVENT_STATE89_AUX UINT32_C(0x00500066)
#define VF2_GAME_DISP_EVENT_STATE89_SETTLED8_INSTRUCTIONS UINT64_C(55)
#define VF2_GAME_DISP_EVENT_STATE89_SETTLED9_INSTRUCTIONS UINT64_C(57)
#define VF2_GAME_DISP_EVENT_STATE89_TERMINAL8_INSTRUCTIONS UINT64_C(80)
#define VF2_GAME_DISP_EVENT_STATE89_TERMINAL9_INSTRUCTIONS UINT64_C(82)
#define VF2_GAME_DISP_EVENT_FLAG12_LINK UINT32_C(0x0002afc0)
#define VF2_GAME_DISP_EVENT_FLAG11_LINK UINT32_C(0x0002b060)
#define VF2_GAME_DISP_EVENT_FLAG12_TERMINAL_LINK UINT32_C(0x0002b0c8)
#define VF2_GAME_DISP_EVENT_FLAG11_TERMINAL_LINK UINT32_C(0x0002b11c)

typedef enum game_disp_state89_kind {
    GAME_DISP_STATE89_NONE = 0,
    GAME_DISP_STATE89_SETTLED,
    GAME_DISP_STATE89_INITIAL,
    GAME_DISP_STATE89_PENDING,
    GAME_DISP_STATE89_TERMINAL,
    GAME_DISP_STATE89_FINAL_DRAIN
} game_disp_state89_kind;

typedef struct game_disp_state89_case {
    game_disp_state89_kind kind;
    uint8_t state_byte;
    uint8_t side;
    uint8_t mode;
    uint16_t value;
    uint8_t queue_head;
    uint8_t queue_tail;
    uint8_t timer0;
    uint8_t timer1;
    uint32_t registry_flags;
    uint64_t child_instructions;
    uint32_t link;
} game_disp_state89_case;

typedef struct game_disp_state89_image {
    uint8_t registry[UINT32_C(0x70)];
    uint8_t fighter0_value[2];
    uint8_t fighter1_value[2];
    uint8_t state_byte;
    uint8_t mode;
    uint8_t aux;
    uint8_t state_clear;
    uint8_t output;
} game_disp_state89_image;

static bool game_disp_state89_value_is_measured(uint8_t state_byte, uint16_t value)
{
    if (value == UINT16_C(0)) {
        return true;
    }
    if (state_byte == UINT8_C(9)) {
        return value == UINT16_C(5);
    }
    return value == UINT16_C(1) || value == UINT16_C(5) ||
           value == UINT16_C(9) || value == UINT16_C(10) ||
           value == UINT16_C(30) || value == UINT16_C(31) ||
           value == UINT16_C(42) || value == UINT16_C(99);
}

static uint64_t game_disp_state89_initial_instructions(
    uint8_t state_byte,
    uint8_t side,
    uint16_t value
)
{
    uint64_t result = UINT64_C(0);
    const uint64_t state_bonus = state_byte == UINT8_C(9) ? UINT64_C(2) : UINT64_C(0);

    if (value == UINT16_C(0)) {
        return VF2_GAME_DISP_EVENT_STATE89_SETTLED8_INSTRUCTIONS + state_bonus;
    }
    if (side == UINT8_C(0)) {
        if (value < UINT16_C(10)) {
            result = UINT64_C(106);
        } else if (value == UINT16_C(10)) {
            result = UINT64_C(111);
        } else {
            result = UINT64_C(104);
        }
    } else {
        if (value < UINT16_C(10)) {
            result = UINT64_C(108);
        } else if (value == UINT16_C(10)) {
            result = UINT64_C(113);
        } else {
            result = UINT64_C(106);
        }
    }
    return result + state_bonus;
}

static vf2_status game_disp_state89_read_image(
    vf2_model2a *machine,
    game_disp_state89_image *image
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || image == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(
        machine, VF2_GAME_DISP_REGISTRY, image->registry, sizeof(image->registry)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER0 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            image->fighter0_value,
            sizeof(image->fighter0_value)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER1 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            image->fighter1_value,
            sizeof(image->fighter1_value)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE, &image->state_byte,
            sizeof(image->state_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE89_MODE, &image->mode,
            sizeof(image->mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE89_AUX, &image->aux,
            sizeof(image->aux)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x005000a4), &image->state_clear,
            sizeof(image->state_clear)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_OUTPUT_PORT, &image->output,
            sizeof(image->output)
        );
    }
    return status;
}

static vf2_status game_disp_state89_write_image(
    vf2_model2a *machine,
    const game_disp_state89_image *image
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || image == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_write_u32(
        machine,
        VF2_GAME_DISP_REGISTRY,
        (uint32_t)image->registry[0] |
            ((uint32_t)image->registry[1] << 8u) |
            ((uint32_t)image->registry[2] << 16u) |
            ((uint32_t)image->registry[3] << 24u)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            image->registry + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET,
            image->registry + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET,
            VF2_GAME_DISP_EVENT_QUEUE_CAPACITY
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            image->registry + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            UINT32_C(4)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER0 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            image->fighter0_value,
            sizeof(image->fighter0_value)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER1 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            image->fighter1_value,
            sizeof(image->fighter1_value)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_STATE, &image->state_byte,
            sizeof(image->state_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_STATE89_MODE, &image->mode,
            sizeof(image->mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_STATE89_AUX, &image->aux,
            sizeof(image->aux)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005000a4), &image->state_clear,
            sizeof(image->state_clear)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_OUTPUT_PORT, &image->output,
            sizeof(image->output)
        );
    }
    return status;
}

static vf2_status game_disp_state89_normalize_for_base(vf2_model2a *machine)
{
    uint8_t zero = UINT8_C(0);
    uint8_t zeros[2] = {UINT8_C(0), UINT8_C(0)};
    vf2_status status = vf2_model2a_write_u32(
        machine, VF2_GAME_DISP_REGISTRY, UINT32_C(0x80000000)
    );

    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER0_OFFSET,
            zeros, sizeof(zeros)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_STATE, &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_STATE89_MODE, &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_GAME_DISP_EVENT_STATE89_AUX, &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER0 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            zeros, sizeof(zeros)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER1 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            zeros, sizeof(zeros)
        );
    }
    return status;
}

static bool game_disp_state89_queue_matches_side(
    vf2_model2a *machine,
    uint8_t side,
    uint8_t queue_head
)
{
    uint8_t queue[UINT8_C(8)] = {0};
    vf2_status status = vf2_model2a_read(
        machine,
        VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_DATA_OFFSET,
        queue,
        sizeof(queue)
    );

    if (status != VF2_OK || side > UINT8_C(1)) {
        return false;
    }
    if (side == UINT8_C(0)) {
        if (queue[0] != UINT8_C(0x97) || queue[1] != UINT8_C(0x90) ||
            queue[2] != UINT8_C(0x1f) || queue[3] != UINT8_C(0x3f)) {
            return false;
        }
        if (queue_head == UINT8_C(5)) {
            return queue[4] >= UINT8_C(1) && queue[4] <= UINT8_C(9);
        }
        if (queue_head == UINT8_C(6)) {
            return queue[4] >= UINT8_C(0x21) && queue[4] <= UINT8_C(0x29) &&
                   queue[5] <= UINT8_C(9);
        }
        if (queue_head == UINT8_C(8)) {
            return queue[5] == UINT8_C(0x97) &&
                   queue[6] == UINT8_C(0x1f) &&
                   queue[7] == UINT8_C(0x3f);
        }
    } else {
        if (queue[0] != UINT8_C(0x9f) || queue[1] != UINT8_C(0x98) ||
            queue[2] != UINT8_C(0x5f) || queue[3] != UINT8_C(0x7f)) {
            return false;
        }
        if (queue_head == UINT8_C(5)) {
            return queue[4] >= UINT8_C(0x41) && queue[4] <= UINT8_C(0x49);
        }
        if (queue_head == UINT8_C(6)) {
            return queue[4] >= UINT8_C(0x61) && queue[4] <= UINT8_C(0x69) &&
                   queue[5] >= UINT8_C(0x40) && queue[5] <= UINT8_C(0x49);
        }
        if (queue_head == UINT8_C(8)) {
            return queue[5] == UINT8_C(0x9f) &&
                   queue[6] == UINT8_C(0x5f) &&
                   queue[7] == UINT8_C(0x7f);
        }
    }
    return false;
}

static bool game_disp_state89_machine_case(
    vf2_model2a *machine,
    game_disp_state89_case *result
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t registry_flags = 0u;
    uint16_t value0 = UINT16_MAX;
    uint16_t value1 = UINT16_MAX;
    uint16_t score = UINT16_MAX;
    uint8_t selector = UINT8_MAX;
    uint8_t state_byte = UINT8_MAX;
    uint8_t mode = UINT8_MAX;
    uint8_t aux = UINT8_MAX;
    uint8_t queue_head = UINT8_MAX;
    uint8_t queue_tail = UINT8_MAX;
    uint8_t timer0 = UINT8_MAX;
    uint8_t timer1 = UINT8_MAX;
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
        status = vf2_model2a_read_u32(
            machine, VF2_GAME_DISP_REGISTRY, &registry_flags
        );
    }
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER0 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            &value0
        );
    }
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine,
            VF2_GAME_DISP_EVENT_FIGHTER1 + VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
            &value1
        );
    }
    if (status == VF2_OK) {
        status = game_disp_leaf_read_u16(
            machine, VF2_GAME_DISP_EVENT_FLAG13_VALUE, &score
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
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE89_MODE, &mode, sizeof(mode)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_GAME_DISP_EVENT_STATE89_AUX, &aux, sizeof(aux)
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

    if (status != VF2_OK ||
        fighter0 != VF2_GAME_DISP_EVENT_FIGHTER0 ||
        fighter1 != VF2_GAME_DISP_EVENT_FIGHTER1 ||
        selector != UINT8_C(17) ||
        (state_byte != UINT8_C(8) && state_byte != UINT8_C(9)) ||
        mode > UINT8_C(1) || aux != UINT8_C(0) ||
        queue_head >= VF2_GAME_DISP_EVENT_QUEUE_CAPACITY ||
        queue_tail >= VF2_GAME_DISP_EVENT_QUEUE_CAPACITY) {
        return false;
    }

    result->state_byte = state_byte;
    result->mode = mode;
    result->queue_head = queue_head;
    result->queue_tail = queue_tail;
    result->timer0 = timer0;
    result->timer1 = timer1;
    result->registry_flags = registry_flags;

    if (registry_flags == UINT32_C(0x80000000) &&
        timer0 == UINT8_C(0) && timer1 == UINT8_C(0)) {
        if (value0 == UINT16_C(0) && value1 == UINT16_C(0)) {
            if (queue_head == queue_tail) {
                result->kind = GAME_DISP_STATE89_SETTLED;
                result->side = mode;
                result->child_instructions =
                    state_byte == UINT8_C(8)
                        ? VF2_GAME_DISP_EVENT_STATE89_SETTLED8_INSTRUCTIONS
                        : VF2_GAME_DISP_EVENT_STATE89_SETTLED9_INSTRUCTIONS;
                result->link = UINT32_C(0x00000220);
                return true;
            }
            if (queue_head == UINT8_C(8) &&
                queue_tail >= UINT8_C(6) && queue_tail < queue_head &&
                game_disp_state89_queue_matches_side(machine, mode, queue_head)) {
                result->kind = GAME_DISP_STATE89_FINAL_DRAIN;
                result->side = mode;
                result->child_instructions =
                    (state_byte == UINT8_C(8) ? UINT64_C(59) : UINT64_C(61));
                result->link = UINT32_C(0x00000220);
                return true;
            }
        }
        if (queue_head == UINT8_C(0) && queue_tail == UINT8_C(0) &&
            ((mode == UINT8_C(0) && value1 == UINT16_C(0) &&
              game_disp_state89_value_is_measured(state_byte, value0)) ||
             (mode == UINT8_C(1) && value0 == UINT16_C(0) &&
              game_disp_state89_value_is_measured(state_byte, value1)))) {
            const uint16_t value = mode == UINT8_C(0) ? value0 : value1;
            result->kind =
                value == UINT16_C(0)
                    ? GAME_DISP_STATE89_SETTLED
                    : GAME_DISP_STATE89_INITIAL;
            result->side = mode;
            result->value = value;
            result->child_instructions =
                game_disp_state89_initial_instructions(state_byte, mode, value);
            result->link =
                value == UINT16_C(0)
                    ? UINT32_C(0x00000220)
                    : (mode == UINT8_C(0)
                           ? VF2_GAME_DISP_EVENT_FLAG12_LINK
                           : VF2_GAME_DISP_EVENT_FLAG11_LINK);
            return true;
        }
        return false;
    }

    if (value0 != UINT16_C(0) || value1 != UINT16_C(0) || score != UINT16_C(0)) {
        return false;
    }

    if (registry_flags == (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG12) &&
        mode == UINT8_C(0) && timer1 == UINT8_C(0) &&
        (queue_head == UINT8_C(5) || queue_head == UINT8_C(6)) &&
        queue_tail >= UINT8_C(1) && queue_tail <= queue_head &&
        game_disp_state89_queue_matches_side(machine, UINT8_C(0), queue_head)) {
        result->side = UINT8_C(0);
        result->value = UINT16_C(0);
        if (timer0 != UINT8_C(0)) {
            if (timer0 > UINT8_C(36)) {
                return false;
            }
            result->kind = GAME_DISP_STATE89_PENDING;
            result->child_instructions =
                (state_byte == UINT8_C(8) ? UINT64_C(55) : UINT64_C(57)) +
                (queue_head != queue_tail ? UINT64_C(4) : UINT64_C(0));
            result->link = UINT32_C(0x00000220);
            return true;
        }
        if (queue_head == queue_tail) {
            result->kind = GAME_DISP_STATE89_TERMINAL;
            result->child_instructions =
                state_byte == UINT8_C(8)
                    ? VF2_GAME_DISP_EVENT_STATE89_TERMINAL8_INSTRUCTIONS
                    : VF2_GAME_DISP_EVENT_STATE89_TERMINAL9_INSTRUCTIONS;
            result->link = VF2_GAME_DISP_EVENT_FLAG12_TERMINAL_LINK;
            return true;
        }
    }

    if (registry_flags == (UINT32_C(0x80000000) | VF2_GAME_DISP_EVENT_FLAG11) &&
        mode == UINT8_C(1) && timer0 == UINT8_C(0) &&
        (queue_head == UINT8_C(5) || queue_head == UINT8_C(6)) &&
        queue_tail >= UINT8_C(1) && queue_tail <= queue_head &&
        game_disp_state89_queue_matches_side(machine, UINT8_C(1), queue_head)) {
        result->side = UINT8_C(1);
        result->value = UINT16_C(0);
        if (timer1 != UINT8_C(0)) {
            if (timer1 > UINT8_C(36)) {
                return false;
            }
            result->kind = GAME_DISP_STATE89_PENDING;
            result->child_instructions =
                (state_byte == UINT8_C(8) ? UINT64_C(55) : UINT64_C(57)) +
                (queue_head != queue_tail ? UINT64_C(4) : UINT64_C(0));
            result->link = UINT32_C(0x00000220);
            return true;
        }
        if (queue_head == queue_tail) {
            result->kind = GAME_DISP_STATE89_TERMINAL;
            result->child_instructions =
                state_byte == UINT8_C(8)
                    ? VF2_GAME_DISP_EVENT_STATE89_TERMINAL8_INSTRUCTIONS
                    : VF2_GAME_DISP_EVENT_STATE89_TERMINAL9_INSTRUCTIONS;
            result->link = VF2_GAME_DISP_EVENT_FLAG11_TERMINAL_LINK;
            return true;
        }
    }
    return false;
}

static vf2_status game_disp_state89_write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status game_disp_state89_apply_transition(
    vf2_model2a *machine,
    const game_disp_state89_case *match
)
{
    uint8_t queue_head = match->queue_head;
    uint8_t queue_tail = match->queue_tail;
    uint8_t timer0 = match->timer0;
    uint8_t timer1 = match->timer1;
    uint32_t flags = match->registry_flags;
    uint8_t output = UINT8_C(0);
    vf2_status status = VF2_OK;

    if (machine == NULL || match == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = game_disp_state89_write_u8(
        machine, UINT32_C(0x005000a4), UINT8_C(0)
    );
    if (status != VF2_OK) {
        return status;
    }

    if (match->kind == GAME_DISP_STATE89_INITIAL) {
        const uint8_t prefix0[] = {
            UINT8_C(0x97), UINT8_C(0x90), UINT8_C(0x1f), UINT8_C(0x3f)
        };
        const uint8_t prefix1[] = {
            UINT8_C(0x9f), UINT8_C(0x98), UINT8_C(0x5f), UINT8_C(0x7f)
        };
        const uint8_t *prefix = match->side == UINT8_C(0) ? prefix0 : prefix1;
        uint8_t tens = (uint8_t)(match->value / UINT16_C(10));
        uint8_t ones = (uint8_t)(match->value % UINT16_C(10));
        uint8_t zero_pair[2] = {UINT8_C(0), UINT8_C(0)};
        queue_head = UINT8_C(0);
        queue_tail = UINT8_C(0);

        status = game_disp_event_enqueue_sequence(
            machine, &queue_head, prefix, UINT32_C(4)
        );
        if (status == VF2_OK && tens != UINT8_C(0)) {
            status = game_disp_event_enqueue_byte(
                machine,
                &queue_head,
                (uint8_t)((match->side == UINT8_C(0)
                               ? UINT8_C(0x20)
                               : UINT8_C(0x60)) + tens)
            );
        }
        if (status == VF2_OK) {
            status = game_disp_event_enqueue_byte(
                machine,
                &queue_head,
                (uint8_t)((match->side == UINT8_C(0)
                               ? UINT8_C(0x00)
                               : UINT8_C(0x40)) + ones)
            );
        }
        if (status == VF2_OK) {
            flags = UINT32_C(0x80000000) |
                    (match->side == UINT8_C(0)
                         ? VF2_GAME_DISP_EVENT_FLAG12
                         : VF2_GAME_DISP_EVENT_FLAG11);
            if (match->value >= UINT16_C(30)) {
                if (match->side == UINT8_C(0)) {
                    timer0 = UINT8_C(36);
                } else {
                    timer1 = UINT8_C(36);
                }
            } else {
                if (match->side == UINT8_C(0)) {
                    timer0 = UINT8_C(18);
                } else {
                    timer1 = UINT8_C(18);
                }
            }
            status = vf2_model2a_write(
                machine,
                (match->side == UINT8_C(0)
                     ? VF2_GAME_DISP_EVENT_FIGHTER0
                     : VF2_GAME_DISP_EVENT_FIGHTER1) +
                    VF2_GAME_DISP_EVENT_FIGHTER_VALUE_OFFSET,
                zero_pair,
                sizeof(zero_pair)
            );
        }
    } else if (match->kind == GAME_DISP_STATE89_PENDING) {
        if (match->side == UINT8_C(0)) {
            uint8_t stored[2] = {
                (uint8_t)(timer0 - UINT8_C(1)), UINT8_C(0)
            };
            status = vf2_model2a_write(
                machine,
                VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER0_OFFSET,
                stored,
                sizeof(stored)
            );
            timer0 = stored[0];
            timer1 = UINT8_C(0);
        } else {
            uint8_t stored[2] = {
                (uint8_t)(timer1 - UINT8_C(1)), UINT8_C(0)
            };
            status = vf2_model2a_write(
                machine,
                VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER1_OFFSET,
                stored,
                sizeof(stored)
            );
            timer1 = stored[0];
        }
    } else if (match->kind == GAME_DISP_STATE89_TERMINAL) {
        const uint8_t final0[] = {
            UINT8_C(0x97), UINT8_C(0x1f), UINT8_C(0x3f)
        };
        const uint8_t final1[] = {
            UINT8_C(0x9f), UINT8_C(0x5f), UINT8_C(0x7f)
        };
        flags &= ~(match->side == UINT8_C(0)
                       ? VF2_GAME_DISP_EVENT_FLAG12
                       : VF2_GAME_DISP_EVENT_FLAG11);
        status = game_disp_event_enqueue_sequence(
            machine,
            &queue_head,
            match->side == UINT8_C(0) ? final0 : final1,
            UINT32_C(3)
        );
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
            queue_tail = (uint8_t)((queue_tail + UINT8_C(1)) &
                                   VF2_GAME_DISP_EVENT_QUEUE_MASK);
            status = game_disp_state89_write_u8(
                machine, VF2_GAME_DISP_EVENT_OUTPUT_PORT, output
            );
        }
    }
    if (status == VF2_OK) {
        status = game_disp_state89_write_u8(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET,
            queue_head
        );
    }
    if (status == VF2_OK) {
        status = game_disp_state89_write_u8(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET,
            queue_tail
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_GAME_DISP_REGISTRY, flags
        );
    }
    if (status == VF2_OK &&
        match->kind == GAME_DISP_STATE89_INITIAL) {
        status = game_disp_state89_write_u8(
            machine,
            VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER0_OFFSET,
            timer0
        );
        if (status == VF2_OK) {
            status = game_disp_state89_write_u8(
                machine,
                VF2_GAME_DISP_REGISTRY + VF2_GAME_DISP_EVENT_TIMER1_OFFSET,
                timer1
            );
        }
    }
    return status;
}

static bool measured_game_disp_event_state89_case(
    vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    game_disp_state89_case *match
)
{
    size_t index = 0u;

    if (machine == NULL || cpu == NULL || match == NULL ||
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
    return game_disp_state89_machine_case(machine, match);
}

static vf2_status execute_measured_game_disp_event_state89(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report,
    const game_disp_state89_case *match
)
{
    game_disp_state89_image original;
    game_disp_state89_image desired;
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    uint64_t actual = UINT64_C(0);
    uint64_t delta = UINT64_C(0);
    vf2_status status = game_disp_state89_read_image(machine, &original);

    if (status == VF2_OK) {
        status = game_disp_state89_apply_transition(machine, match);
    }
    if (status == VF2_OK) {
        status = game_disp_state89_read_image(machine, &desired);
    }
    if (status == VF2_OK) {
        status = game_disp_state89_write_image(machine, &original);
    }
    if (status == VF2_OK) {
        status = game_disp_state89_normalize_for_base(machine);
    }
    if (status == VF2_OK) {
        status = execute_measured_game_disp_event_gate_state(
            machine, cpu, state, effective_report,
            UINT8_C(0), UINT8_C(0), UINT8_C(0), UINT8_C(0)
        );
    }
    if (status == VF2_OK) {
        status = game_disp_state89_write_image(machine, &desired);
    }
    if (status != VF2_OK) {
        return status;
    }

    actual = cpu->executed_instructions - start_instructions;
    if (actual > match->child_instructions) {
        return VF2_ERROR_UNSUPPORTED;
    }
    delta = match->child_instructions - actual;
    cpu->executed_instructions += delta;
    cpu->registers[VF2_I960_G14_REGISTER] = match->link;
    state->recovered_instruction_count += delta;
    effective_report->recovered_instruction_count += delta;
    set_game_disp_event_queue_condition(
        cpu,
        desired.registry[VF2_GAME_DISP_EVENT_QUEUE_HEAD_OFFSET],
        original.registry[VF2_GAME_DISP_EVENT_QUEUE_TAIL_OFFSET]
    );
    return VF2_OK;
}

vf2_status vf2_native_runtime_step(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
)
{
    game_disp_state89_case match;

    if (machine != NULL && cpu != NULL && state != NULL &&
        measured_game_disp_event_state89_case(machine, cpu, &match)) {
        return execute_measured_game_disp_event_state89(
            machine, cpu, state, report, &match
        );
    }
    return vf2_native_runtime_step_flag13_base(
        machine, cpu, state, report
    );
}
