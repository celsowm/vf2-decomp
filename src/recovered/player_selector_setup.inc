#define VF2_PLAYER_SELECTOR_DATA_TABLE UINT32_C(0x0200d34c)
#define VF2_PLAYER_SELECTOR_SCRATCH_TABLE UINT32_C(0x02120004)
#define VF2_PLAYER_SELECTOR_BRANCH_BASE UINT32_C(0x0050016c)
#define VF2_PLAYER_SELECTOR_MAX_OPCODES 64u

typedef struct vf2_player_selector_setup_plan {
    uint32_t player;
    uint32_t selector;
    uint32_t data_pointer;
    uint32_t table_pointer;
    uint32_t final_cursor;
    uint32_t base_flags;
    uint32_t final_state_flags;
    uint16_t table_value;
    uint8_t branch_byte;
    uint8_t terminator;
    size_t opcode_count;
} vf2_player_selector_setup_plan;

static vf2_status player_selector_read_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return value == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_read(machine, address, value, sizeof(*value));
}

static vf2_status player_selector_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] |
                            ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status player_selector_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static vf2_status player_selector_write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

#include "player_selector_opcodes_extended.inc"

static int player_selector_opcode_supported(uint8_t opcode)
{
    switch (opcode) {
    case 0u:
    case 1u:
    case 3u:
    case 6u:
    case 7u:
    case 8u:
    case 10u:
    case 12u:
    case 13u:
    case 15u:
    case 16u:
    case 17u:
        return 1;
    default:
        return player_selector_extended_opcode_supported(opcode);
    }
}

static vf2_status player_selector_opcode_size(
    const vf2_model2a *machine,
    uint32_t cursor,
    uint8_t opcode,
    uint32_t *size
)
{
    uint8_t count = 0u;
    vf2_status status = VF2_OK;

    if (size == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    switch (opcode) {
    case 0u:
        *size = 0u;
        return VF2_OK;
    case 1u:
    case 7u:
    case 12u:
    case 13u:
    case 15u:
        *size = 3u;
        return VF2_OK;
    case 3u:
        *size = 14u;
        return VF2_OK;
    case 6u:
    case 17u:
        *size = 7u;
        return VF2_OK;
    case 8u:
        *size = 1u;
        return VF2_OK;
    case 10u:
        *size = 6u;
        return VF2_OK;
    case 16u:
        status = player_selector_read_u8(
            machine, cursor + UINT32_C(1), &count
        );
        if (status != VF2_OK) {
            return status;
        }
        *size = UINT32_C(2) + (uint32_t)count * UINT32_C(2);
        return VF2_OK;
    default:
        return player_selector_extended_opcode_size(opcode, size);
    }
}

static vf2_status player_selector_prepare_setup(
    const vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    vf2_player_selector_setup_plan *plan
)
{
    uint32_t branch_base = 0u;
    uint32_t cursor = 0u;
    uint32_t old_state = 0u;
    uint32_t old_selector_state = 0u;
    uint8_t bytes[8] = {0u};
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL || player == 0u ||
        selector > UINT32_C(0x1fff)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    plan->player = player;
    plan->selector = selector;

    if (selector == 0u) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_SELECTOR_BRANCH_BASE, &branch_base
        );
        if (status == VF2_OK && branch_base != 0u) {
            status = player_selector_read_u8(
                machine, branch_base + UINT32_C(0x3351), &plan->branch_byte
            );
        }
        if (status != VF2_OK || branch_base == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(
        machine,
        VF2_PLAYER_SELECTOR_DATA_TABLE + selector * UINT32_C(4),
        &plan->data_pointer
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_PLAYER_SELECTOR_SCRATCH_TABLE + selector * UINT32_C(4),
            &plan->table_pointer
        );
    }
    if (status == VF2_OK && plan->table_pointer != 0u) {
        status = player_selector_read_u16(
            machine, plan->table_pointer, &plan->table_value
        );
    }
    for (index = 0u; status == VF2_OK && index < sizeof(bytes); ++index) {
        status = player_selector_read_u8(
            machine, plan->data_pointer + (uint32_t)index, &bytes[index]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_PLAYER_SELECTOR_BRANCH_BASE, &branch_base
        );
    }
    if (status == VF2_OK && branch_base != 0u) {
        status = player_selector_read_u8(
            machine, branch_base + UINT32_C(0x3351), &plan->branch_byte
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x01a4), &old_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x0804), &old_selector_state
        );
    }
    if (status != VF2_OK || plan->data_pointer == 0u ||
        plan->table_pointer == 0u || branch_base == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    (void)old_state;
    (void)old_selector_state;

    plan->base_flags = (uint32_t)bytes[0] |
                       ((uint32_t)bytes[1] << 8u) |
                       ((uint32_t)bytes[2] << 16u) |
                       ((uint32_t)bytes[3] << 24u);
    if (selector == UINT32_C(40)) {
        plan->base_flags |= UINT32_C(1) << 7u;
    }
    if (selector == UINT32_C(0xdb) || selector == UINT32_C(0x19b)) {
        plan->base_flags &= ~(UINT32_C(1) << 25u);
    }
    if ((plan->branch_byte & (UINT8_C(1) << 6u)) != 0u) {
        if (selector == UINT32_C(0x2de) ||
            selector == UINT32_C(0x8d) ||
            selector == UINT32_C(0x8b) ||
            selector == UINT32_C(0x99)) {
            plan->base_flags &= ~(UINT32_C(1) << 4u);
        }
        if (selector == UINT32_C(0x2c9) ||
            selector == UINT32_C(0x43) ||
            selector == UINT32_C(0x196) ||
            selector == UINT32_C(0x341)) {
            plan->base_flags |= (UINT32_C(1) << 29u) |
                                (UINT32_C(1) << 11u);
        }
    }

    plan->final_state_flags = plan->base_flags & ~(UINT32_C(1) << 21u);
    if ((plan->final_state_flags & (UINT32_C(1) << 8u)) != 0u &&
        (plan->final_state_flags & (UINT32_C(1) << 15u)) != 0u) {
        plan->final_state_flags &= ~(UINT32_C(1) << 15u);
        plan->final_state_flags |= UINT32_C(1) << 16u;
    }

    cursor = plan->data_pointer + UINT32_C(8);
    for (index = 0u; index < VF2_PLAYER_SELECTOR_MAX_OPCODES; ++index) {
        uint8_t opcode = 0u;
        uint32_t size = 0u;

        status = player_selector_read_u8(machine, cursor, &opcode);
        if (status != VF2_OK) {
            return status;
        }
        if (!player_selector_opcode_supported(opcode)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (opcode == 0u) {
            plan->terminator = opcode;
            plan->final_cursor = cursor;
            plan->opcode_count = index + 1u;
            return VF2_OK;
        }
        status = player_selector_opcode_size(machine, cursor, opcode, &size);
        if (status != VF2_OK || size == 0u || cursor > UINT32_MAX - size) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cursor += size;
        if (opcode == 8u) {
            plan->terminator = opcode;
            plan->final_cursor = cursor;
            plan->opcode_count = index + 1u;
            return VF2_OK;
        }
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status player_selector_apply_opcode(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    uint32_t cursor,
    uint8_t opcode,
    uint32_t arithmetic_control
)
{
    uint8_t value = 0u;
    uint8_t count = 0u;
    uint32_t saved_cursor = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    switch (opcode) {
    case 1u:
        for (index = 0u; index < 2u; ++index) {
            status = player_selector_read_u8(
                machine, cursor + UINT32_C(1) + (uint32_t)index, &value
            );
            if (status == VF2_OK) {
                status = player_selector_write_u8(
                    machine, player + UINT32_C(0x802) + (uint32_t)index,
                    value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        return VF2_OK;
    case 3u: {
        static const uint16_t offsets[13] = {
            UINT16_C(0x80e), UINT16_C(0x80f),
            UINT16_C(0x816), UINT16_C(0x817),
            UINT16_C(0x818), UINT16_C(0x819),
            UINT16_C(0x81c), UINT16_C(0x81d), UINT16_C(0x81e),
            UINT16_C(0x81f), UINT16_C(0x840),
            UINT16_C(0x850), UINT16_C(0x851)
        };
        for (index = 0u; index < 13u; ++index) {
            status = player_selector_read_u8(
                machine, cursor + UINT32_C(1) + (uint32_t)index, &value
            );
            if (status == VF2_OK) {
                status = player_selector_write_u8(
                    machine, player + (uint32_t)offsets[index], value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        return VF2_OK;
    }
    case 6u: {
        static const uint16_t offsets[6] = {
            UINT16_C(0x818), UINT16_C(0x819),
            UINT16_C(0x812), UINT16_C(0x813),
            UINT16_C(0x85a), UINT16_C(0x85b)
        };
        for (index = 0u; index < 6u; ++index) {
            status = player_selector_read_u8(
                machine, cursor + UINT32_C(1) + (uint32_t)index, &value
            );
            if (status == VF2_OK) {
                status = player_selector_write_u8(
                    machine, player + (uint32_t)offsets[index], value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        return VF2_OK;
    }
    case 7u:
        for (index = 0u; index < 2u; ++index) {
            status = player_selector_read_u8(
                machine, cursor + UINT32_C(1) + (uint32_t)index, &value
            );
            if (status == VF2_OK) {
                status = player_selector_write_u8(
                    machine, player + UINT32_C(0x812) + (uint32_t)index,
                    value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        return VF2_OK;
    case 10u:
        status = player_selector_read_u8(
            machine, player + UINT32_C(0x83c), &value
        );
        if (status == VF2_OK && value == 0u) {
            status = vf2_model2a_write_u32(
                machine, player + UINT32_C(0x838), cursor
            );
        }
        if (status == VF2_OK) {
            value = (uint8_t)(value + UINT8_C(1));
            status = player_selector_write_u8(
                machine, player + UINT32_C(0x83c), value
            );
        }
        return status;
    case 12u:
        for (index = 0u; index < 2u; ++index) {
            status = player_selector_read_u8(
                machine, cursor + UINT32_C(1) + (uint32_t)index, &value
            );
            if (status == VF2_OK) {
                status = player_selector_write_u8(
                    machine, player + UINT32_C(0x841) + (uint32_t)index,
                    value
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
        if (selector == UINT32_C(0x2e3)) {
            return player_selector_write_u8(
                machine, player + UINT32_C(0x841), UINT8_C(10)
            );
        }
        return VF2_OK;
    case 13u:
        status = player_selector_read_u8(
            machine, cursor + UINT32_C(1), &value
        );
        if (status == VF2_OK) {
            status = player_selector_write_u8(
                machine, player + UINT32_C(0x84d), value
            );
        }
        if (status == VF2_OK) {
            status = player_selector_read_u8(
                machine, cursor + UINT32_C(2), &value
            );
        }
        if (status == VF2_OK) {
            status = player_selector_write_u16(
                machine, player + UINT32_C(0x858), (uint16_t)value
            );
        }
        return status;
    case 15u:
    case 17u:
        return VF2_OK;
    case 16u:
        status = player_selector_read_u8(
            machine, cursor + UINT32_C(1), &count
        );
        if (status == VF2_OK) {
            saved_cursor = cursor + UINT32_C(2);
            status = vf2_model2a_write_u32(
                machine, player + UINT32_C(0x854), saved_cursor
            );
        }
        if (status == VF2_OK) {
            status = player_selector_read_u8(
                machine, cursor + (uint32_t)count * UINT32_C(2), &value
            );
        }
        if (status == VF2_OK) {
            status = player_selector_write_u16(
                machine, player + UINT32_C(0x800), (uint16_t)value
            );
        }
        return status;
    case 0u:
    case 8u:
        /* ROM dispatch at 0x1a340 reads the opcode byte, indexes the
         * bx table at 0x1a350 and jumps: opcode 1 → 0x1a408 (two
         * ldib/stib pairs copying (r11)+1/+2 to +0x802/+0x803,
         * r11 += 3, loops to 0x1a340), opcode 8 → 0x1a398 (`addo
         * 1,r11`, stores r11 to +0x82c/+0x6d0, flag tail).
         * v0389 live 0x505 stream: opcode 1 at 0x200e8af (01 ff ff)
         * then opcode 8 at 0x200e8b2.  Opcode 1's two ldib/stib
         * pairs are recovered in the opcode-1 handler.
         * Opcode 8 performs no fighter stores here (only the cursor
         * stores + the flag tail, handled by the caller); the stale
         * code below copying (r11)+1/+2 on opcode 8 described
         * opcode 1's operation and overwrote +0x802/+0x803 with the
         * opcode-8 window bytes on the live shape (measured diff
         * 0x511182 expected ff).  Other opcodes keep their measured
         * handlers below. */
        return VF2_OK;
    default:
        return player_selector_extended_apply_opcode(
            machine, player, selector, cursor, opcode, arithmetic_control
        );
    }
}

static vf2_status player_selector_apply_setup(
    vf2_model2a *machine,
    const vf2_player_selector_setup_plan *plan,
    uint32_t arithmetic_control
)
{
    uint32_t old_state = 0u;
    uint32_t old_selector_state = 0u;
    uint32_t cursor = 0u;
    uint8_t source_byte = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || plan == NULL || plan->player == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = player_selector_write_u16(
        machine, plan->player + UINT32_C(0x802), 0u
    );
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, plan->player + UINT32_C(0xa00), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x812), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, plan->player + UINT32_C(0x83c), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x844), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, plan->player + UINT32_C(0x841), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u8(
            machine, plan->player + UINT32_C(0x823), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x850), 0u
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x818), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x854), 0u
        );
    }
    if (status != VF2_OK || plan->selector == 0u) {
        goto finish;
    }

    status = vf2_model2a_read_u32(
        machine, plan->player + UINT32_C(0x01a4), &old_state
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, plan->player + UINT32_C(0x0804), &old_selector_state
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x800), plan->table_value
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x80a), plan->table_value
        );
    }
    if (status == VF2_OK) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x80c), plan->table_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0xbd4), old_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0xc30), old_selector_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x804), plan->base_flags
        );
    }
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = player_selector_read_u8(
            machine, plan->data_pointer + UINT32_C(4) + (uint32_t)index,
            &source_byte
        );
        if (status == VF2_OK) {
            status = player_selector_write_u8(
                machine,
                plan->player + (index < 2u
                    ? UINT32_C(0x810) + (uint32_t)index
                    : UINT32_C(0x814) + (uint32_t)index - UINT32_C(2)),
                source_byte
            );
        }
    }
    if (status == VF2_OK &&
        (plan->final_state_flags & (UINT32_C(1) << 6u)) != 0u) {
        status = player_selector_write_u16(
            machine, plan->player + UINT32_C(0x812), UINT16_C(0x8000)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x1a4), plan->final_state_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cursor = plan->data_pointer + UINT32_C(8);
    for (index = 0u; index < plan->opcode_count; ++index) {
        uint8_t opcode = 0u;
        uint32_t size = 0u;

        status = player_selector_read_u8(machine, cursor, &opcode);
        if (status != VF2_OK) {
            return status;
        }
        status = player_selector_apply_opcode(
            machine,
            plan->player,
            plan->selector,
            cursor,
            opcode,
            arithmetic_control
        );
        if (status != VF2_OK) {
            return status;
        }
        if (opcode == 0u || opcode == 8u) {
            break;
        }
        status = player_selector_opcode_size(machine, cursor, opcode, &size);
        if (status != VF2_OK) {
            return status;
        }
        cursor += size;
    }

finish:
    if (status == VF2_OK && plan->selector != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x82c), plan->final_cursor
        );
    }
    if (status == VF2_OK && plan->selector != 0u) {
        status = vf2_model2a_write_u32(
            machine, plan->player + UINT32_C(0x6d0), plan->final_cursor
        );
    }
    if (status == VF2_OK) {
        uint32_t player_flags = 0u;
        status = vf2_model2a_read_u32(machine, plan->player, &player_flags);
        if (status == VF2_OK && (player_flags & (UINT32_C(1) << 6u)) != 0u &&
            plan->selector != 0u) {
            uint16_t signed_value = 0u;
            uint32_t toggle = 0u;
            status = player_selector_read_u16(
                machine, plan->player + UINT32_C(0x812), &signed_value
            );
            if (status == VF2_OK) {
                status = player_selector_write_u16(
                    machine, plan->player + UINT32_C(0x812),
                    (uint16_t)(-(int16_t)signed_value)
                );
            }
            if (status == VF2_OK) {
                status = player_selector_read_u16(
                    machine, plan->player + UINT32_C(0x82a), &signed_value
                );
            }
            if (status == VF2_OK) {
                status = player_selector_write_u16(
                    machine, plan->player + UINT32_C(0x82a),
                    (uint16_t)(-(int16_t)signed_value)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, plan->player + UINT32_C(0x830), &toggle
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, plan->player + UINT32_C(0x830),
                    toggle ^ (UINT32_C(1) << 31u)
                );
            }
        }
    }
    if (status == VF2_OK &&
        (plan->branch_byte & (UINT8_C(1) << 6u)) != 0u) {
        uint16_t active_selector = 0u;
        status = player_selector_read_u16(
            machine, plan->player + UINT32_C(0x1a8), &active_selector
        );
        if (status == VF2_OK &&
            (active_selector == UINT16_C(0x03e4) ||
             active_selector == UINT16_C(0x03e5))) {
            status = vf2_model2a_write_u32(
                machine, plan->player + UINT32_C(0x1218), 0u
            );
        }
    }
    return status;
}

static vf2_status player_selector_execute_setup_with_ac(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    uint32_t arithmetic_control,
    vf2_player_selector_setup_plan *plan_out
)
{
    vf2_player_selector_setup_plan plan;
    vf2_status status = player_selector_prepare_setup(
        machine, player, selector, &plan
    );

    if (status != VF2_OK) {
        return status;
    }
    status = player_selector_apply_setup(machine, &plan, arithmetic_control);
    if (status == VF2_OK && plan_out != NULL) {
        *plan_out = plan;
    }
    return status;
}

static vf2_status player_selector_execute_setup(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t selector,
    vf2_player_selector_setup_plan *plan_out
)
{
    return player_selector_execute_setup_with_ac(
        machine, player, selector, 0u, plan_out
    );
}
