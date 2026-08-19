#include "vf2/hybrid.h"

#include <math.h>
#include <string.h>

#include "vf2/recovered.h"

#define VF2_CAMERA_INITIALIZE_ENTRY UINT32_C(0x0001d320)
#define VF2_CAMERA_INITIALIZE_EXIT UINT32_C(0x0001d458)
#define VF2_CAMERA_UPDATE_ENTRY UINT32_C(0x0001d458)
#define VF2_CAMERA_UPDATE_EXIT UINT32_C(0x0001d660)
#define VF2_CAMERA_GATE_ENTRY UINT32_C(0x0001d660)
#define VF2_CAMERA_GATE_FAST_EXIT UINT32_C(0x0001e524)
#define VF2_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_PLAYER_TASK_ENTRY UINT32_C(0x00013f08)
#define VF2_TASK_CAMERA_ENTRY UINT32_C(0x0001d320)
#define VF2_TASK_USER_ENTRY UINT32_C(0x00029748)
#define VF2_TASK_SOUND_ENTRY UINT32_C(0x000439fc)
#define VF2_TASK_KILL_OSAGE_ENTRY UINT32_C(0x000657dc)
#define VF2_TASK_OSAGE_ENTRY UINT32_C(0x000640f4)
#define VF2_PLAYER_TASK_WRAPPER_ENTRY UINT32_C(0x000142f4)
#define VF2_SCHEDULER_RETURN UINT32_C(0x00010dcc)
#define VF2_INTERPRETED_TASK_STEP_LIMIT UINT64_C(20000000)

static vf2_status hybrid_read_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return value == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_read(machine, address, value, sizeof(*value));
}

static vf2_status hybrid_read_u16(
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

static vf2_status hybrid_write_u16(
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

static vf2_status hybrid_read_u32_triple(
    const vf2_model2a *machine,
    uint32_t address,
    uint32_t *first,
    uint32_t *second,
    uint32_t *third
)
{
    vf2_status status = VF2_OK;

    if (first == NULL || second == NULL || third == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, address, first);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, address + UINT32_C(4), second);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, address + UINT32_C(8), third);
    }
    return status;
}

static void hybrid_set_compare_result(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t condition_bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        condition_bits = 4u;
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        condition_bits = 2u;
    } else if (result == VF2_I960_COMPARE_GREATER) {
        condition_bits = 1u;
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | condition_bits;
}

static float hybrid_float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t hybrid_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static vf2_status hybrid_game_info_interpreter_needed(
    const vf2_model2a *machine,
    int *needed
)
{
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || needed == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500804), &fighter0
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500808), &fighter1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        *needed = ((fighter0_flags | fighter1_flags) &
                   UINT32_C(0x80000000)) != 0u;
    }
    return status;
}

/* Keep unrecovered fighter tasks exact while their C recovery is pending:
 * execute the original task until its architectural RET returns to the
 * scheduler. This is an explicit bridge, not a silent native fallback. */
static vf2_status hybrid_execute_interpreted_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    uint32_t entry_address,
    vf2_recovered_task_report *report
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != entry_address || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = VF2_SCHEDULER_RETURN;
    options.max_steps = VF2_INTERPRETED_TASK_STEP_LIMIT;
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    if (result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu->ip != VF2_SCHEDULER_RETURN) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(report, 0, sizeof(*report));
    report->entry_point = entry_address;
    report->registry_address = registry_address;
    report->continuation = cpu->ip;
    (void)start_instructions;
    (void)start_calls;
    (void)start_returns;
    return VF2_OK;
}

static vf2_status hybrid_execute_interpreted_until(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry_address,
    uint32_t stop_address
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != entry_address ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = stop_address;
    options.max_steps = VF2_INTERPRETED_TASK_STEP_LIMIT;
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    return result.halt_reason == VF2_I960_HALT_STOP_ADDRESS &&
            cpu->ip == stop_address
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

/* Recover the observed player-task bootstrap through the first nested call.
 * The accepted sixth-entry corridor reaches 0x14288 after 842 instructions;
 * all work before that CALL is local structure setup and has no procedure
 * calls on the observed state.  The accepted 0x19ef8 corridor is recovered
 * separately; the later player body remains an explicit ROM continuation. */
static vf2_status hybrid_execute_player_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t registry = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 13u] : 0u;
    const uint32_t player_base = registry;
    uint32_t profile = 0u;
    uint32_t profile_value = 0u;
    uint32_t source = 0u;
    uint32_t value = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t profile_index = 0u;
    uint32_t triple[3] = {0u, 0u, 0u};
    uint8_t byte_value = 0u;
    uint16_t short_value = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_PLAYER_TASK_ENTRY ||
        cpu->local_frame_depth == 0u || registry == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = hybrid_read_u8(
        machine, player_base + UINT32_C(0x000001b0), &byte_value
    );
    profile_index = byte_value;
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0200620c) + profile_index * UINT32_C(4),
            &profile
        );
    }
    if (status != VF2_OK || profile == 0u) {
        /* Keep synthetic/minimal ROM fixtures on the original bridge. */
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = player_base;
    cpu->registers[3] = profile_index;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = profile;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player_base + UINT32_C(0x00000190), profile
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x00000014),
            &profile_value
        );
    }
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = profile_value;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player_base + UINT32_C(0x000001a0), profile_value
        );
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, profile, &source);
    }
    for (index = 0u; status == VF2_OK && index < 16u; ++index) {
        uint32_t word = 0u;
        status = vf2_model2a_read_u32(
            machine, source + (uint32_t)(index * 4u), &word
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x40) +
                    (uint32_t)(index * 4u), word
            );
        }
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(8), &source
        );
    }
    for (index = 0u; status == VF2_OK && index < 16u; ++index) {
        uint32_t word = 0u;
        status = vf2_model2a_read_u32(
            machine, source + (uint32_t)(index * 4u), &word
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x1b4) +
                    (uint32_t)(index * 4u), word
            );
        }
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(4), &source
        );
    }
    for (index = 0u; status == VF2_OK && index < 45u; ++index) {
        uint32_t word = 0u;
        status = vf2_model2a_read_u32(
            machine, source + (uint32_t)(index * 4u), &word
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x8c) +
                    (uint32_t)(index * 4u), word
            );
        }
    }

    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player_base + UINT32_C(4), &byte_value
        );
        value = byte_value == 1u
            ? UINT32_C(0x005207c8) : UINT32_C(0x00520000);
        cpu->registers[15] = value;
        status = vf2_model2a_write_u32(
            machine, player_base + UINT32_C(0x00000bd8), value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x0c), &source
        );
        status = status == VF2_OK
            ? vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x5ec), source
            )
            : status;
    }
    if (status == VF2_OK && source != 0u) {
        const uint32_t source_offsets[4] = {0u, 4u, 0x88u, 0x8cu};
        const uint32_t player_offsets[4] = {0x4cu, 0x50u, 0x58u, 0x5cu};
        for (index = 0u; status == VF2_OK && index < 4u; ++index) {
            status = vf2_model2a_read_u32(
                machine, source + source_offsets[index], &value
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, player_base + player_offsets[index], value
                );
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x10), &value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x5f0), value
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x3c), &value
        );
        if (status == VF2_OK) {
            cpu->registers[6] = value;
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x67c), value
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x40), &source
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, source, &value);
    }
    if (status == VF2_OK && value != 0u) {
        /* The accepted player entry has a null optional state block. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player_base + UINT32_C(0x700), &value
        );
        value &= ~UINT32_C(1);
        status = vf2_model2a_write_u32(
            machine, player_base + UINT32_C(0x700), value
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player_base + UINT32_C(0x1b1), &byte_value
        );
        cpu->registers[3] = byte_value;
        status = vf2_model2a_read_u32(
            machine, player_base + UINT32_C(0x700), &value
        );
        if (status == VF2_OK) {
            value = byte_value == 3u
                ? value | (UINT32_C(1) << 3u)
                : value & ~(UINT32_C(1) << 3u);
            value &= ~(UINT32_C(1) << 4u);
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x700), value
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player_base, &value);
        if (status == VF2_OK) {
            value &= ~(UINT32_C(1) << 27u);
            status = vf2_model2a_write_u32(machine, player_base, value);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x48), &source
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, source, &value);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x768), value
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    if (status == VF2_OK &&
        (runtime_flags & (UINT32_C(1) << 25u)) != 0u) {
        /* The observed bridge takes the clear-bit-5 path; the alternate
         * command-port call at 0x19aac remains an explicit boundary. */
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player_base + UINT32_C(0x700), &value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x700),
                value & ~(UINT32_C(1) << 5u)
            );
        }
    }
    for (index = 0u; status == VF2_OK && index < 31u; ++index) {
        status = hybrid_write_u16(
            machine, player_base + UINT32_C(0x140) +
                (uint32_t)(index * 2u), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, player_base + UINT32_C(0x18), triple,
            sizeof(triple)
        );
    }
    if (status == VF2_OK) {
        /* The observed extended-register operation carries the loaded g6
         * value in the middle word; the surrounding two words are zero. */
        triple[1] = cpu->registers[VF2_I960_G0_REGISTER + 6u];
        status = vf2_model2a_write(
            machine, player_base + UINT32_C(0x1f4), triple,
            sizeof(triple)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player_base + UINT32_C(0x6e0), triple,
            sizeof(triple)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, player_base + UINT32_C(0x26), &short_value
        );
        cpu->registers[15] = short_value;
        status = hybrid_write_u16(
            machine, player_base + UINT32_C(0x616), short_value
        );
    }
    if (status == VF2_OK) {
        const uint32_t zero_words[] = {
            0x198u, 0x194u, 0x654u, 0x19cu, 0x1a4u, 0x804u,
            0xbd4u, 0xc30u, 0x2cu, 0x30u, 0x34u, 0x5c8u,
            0x5ccu, 0x5d0u, 0x5d4u, 0x620u, 0x1224u, 0x1222u,
            0x650u, 0x6b4u, 0x60cu
        };
        for (index = 0u; status == VF2_OK && index <
                    sizeof(zero_words) / sizeof(zero_words[0]); ++index) {
            status = vf2_model2a_write_u32(
                machine, player_base + zero_words[index], 0u
            );
        }
    }
    if (status == VF2_OK) {
        const uint32_t zero_shorts[] = {
            0x6b2u, 0x6bcu, 0x6d4u, 0x614u, 0x624u, 0x61cu,
            0x60eu, 0x618u, 0x1a8u, 0x1aau, 0xc4cu, 0x770u,
            0xbe6u, 0x6dau
        };
        for (index = 0u; status == VF2_OK && index <
                    sizeof(zero_shorts) / sizeof(zero_shorts[0]); ++index) {
            status = hybrid_write_u16(
                machine, player_base + zero_shorts[index], 0u
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050a0b6), &byte_value, sizeof(byte_value)
            );
        }
        byte_value = 0u;
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, player_base + UINT32_C(0x6d8),
                &byte_value, sizeof(byte_value)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, player_base + UINT32_C(0x6d9),
                &byte_value, sizeof(byte_value)
            );
        }
    }
    if (status == VF2_OK) {
        byte_value = 1u;
        status = vf2_model2a_write(
            machine, player_base + UINT32_C(0xbe4),
            &byte_value, sizeof(byte_value)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player_base, &value);
        if (status == VF2_OK) {
            value &= ~(UINT32_C(1) << 26u);
            status = vf2_model2a_write_u32(machine, player_base, value);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x2c), &value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x5d8), value
            );
        }
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, UINT32_C(0x005000a0), &short_value
        );
        if (status == VF2_OK) {
            status = hybrid_write_u16(
                machine, player_base + UINT32_C(0x1ac), short_value
            );
        }
    }
    {
        const uint32_t source_offsets[] = {0x18u, 0x1cu, 0x20u, 0x24u, 0x28u};
        const uint32_t player_offsets[] = {0x62cu, 0x668u, 0x630u, 0x634u, 0x698u};
        for (index = 0u; status == VF2_OK && index < 5u; ++index) {
            status = vf2_model2a_read_u32(
                machine, profile + source_offsets[index], &value
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, player_base + player_offsets[index], value
                );
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, profile + UINT32_C(0x30), &value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, player_base + UINT32_C(0x640), value
            );
        }
    }
    if (status == VF2_OK) {
        uint8_t branch_byte = 0u;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &value
        );
        if (status == VF2_OK) {
            status = hybrid_read_u8(
                machine, value + UINT32_C(0x00003351), &branch_byte
            );
        }
        if (status == VF2_OK && (branch_byte & (UINT8_C(1) << 6u)) != 0u) {
            status = hybrid_read_u8(
                machine, player_base + UINT32_C(0x1b0), &byte_value
            );
            if (status == VF2_OK && byte_value == UINT8_C(0x12)) {
                status = vf2_model2a_write_u32(
                    machine, player_base + UINT32_C(0x698),
                    UINT32_C(0x40200000)
                );
            } else if (status == VF2_OK) {
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, player_base + UINT32_C(0x18),
            &cpu->registers[4], &cpu->registers[5], &cpu->registers[6]
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player_base + UINT32_C(0x1b1), &byte_value
        );
        if (byte_value == 8u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 6u],
            &short_value
        );
        cpu->registers[VF2_I960_G0_REGISTER] = short_value;
    }
    if (status != VF2_OK) {
        return status;
    }

    /* Match the live register aliases at the 0x14288 call boundary. */
    cpu->registers[3] = 0u;
    cpu->registers[4] = 0u;
    cpu->registers[5] = cpu->registers[VF2_I960_G0_REGISTER + 6u];
    cpu->registers[6] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[8] = 0u;
    cpu->registers[9] = 0u;
    cpu->registers[10] = 0u;
    cpu->registers[11] = 0u;
    cpu->registers[12] = 0u;
    cpu->registers[13] = 0u;
    cpu->registers[14] = 0u;
    cpu->registers[15] = 1u;
    cpu->ip = UINT32_C(0x00014288);
    cpu->executed_instructions += UINT64_C(842);
    return VF2_OK;
}

/* Recover the accepted 0x14288 -> 0x19ef8 corridor.  The observed profile
 * selector is 0x505.  Its nested 0x1a1e4 setup consumes ROM data, 0x26ef0
 * expands the corresponding 60-byte scratch stream, and 0x27130 takes the
 * early clear/return path.  Other selectors and branch combinations remain
 * explicit ROM continuations. */
static vf2_status hybrid_execute_player_19ef8(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t selector = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER] : 0u;
    uint32_t player_flags = 0u;
    uint32_t player_state_flags = 0u;
    uint32_t data_pointer = 0u;
    uint32_t table_pointer = 0u;
    uint32_t scratch_base = 0u;
    uint32_t source_value = 0u;
    uint32_t packed_value = 0u;
    uint32_t scratch_r9 = 0u;
    uint32_t scratch_r8 = 0u;
    uint32_t scratch_r7 = 0u;
    uint32_t scratch_sum = 0u;
    uint32_t scratch_count = 0u;
    uint32_t scratch_output_offset = 0u;
    uint32_t scratch_alignment = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t board_flags = 0u;
    uint8_t branch_byte = 0u;
    uint8_t player_type = 0u;
    uint8_t source_bytes[8] = {0u};
    uint8_t record_byte_9 = 0u;
    uint8_t record_byte_10 = 0u;
    uint16_t table_value = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00014288) ||
        cpu->local_frame_depth == 0u || player == 0u ||
        selector != UINT32_C(0x00000505)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Read all branch selectors before mutating the state. */
    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1a4), &player_state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00508000), &board_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &source_value
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, source_value + UINT32_C(0x00003351), &branch_byte
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player + UINT32_C(0x1b1), &player_type
        );
        if (status == VF2_OK && player_type == UINT8_C(8)) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0200d34c) + selector * UINT32_C(4),
            &data_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x02120004) + selector * UINT32_C(4),
            &table_pointer
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, table_pointer, &table_value);
    }
    for (index = 0u; status == VF2_OK && index < sizeof(source_bytes); ++index) {
        status = hybrid_read_u8(
            machine, data_pointer + (uint32_t)index, &source_bytes[index]
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, data_pointer + UINT32_C(9), &record_byte_9
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, data_pointer + UINT32_C(10), &record_byte_10
        );
    }
    packed_value = (uint32_t)source_bytes[0] |
                   ((uint32_t)source_bytes[1] << 8u) |
                   ((uint32_t)source_bytes[2] << 16u) |
                   ((uint32_t)source_bytes[3] << 24u);
    if (status != VF2_OK || player_state_flags != 0u ||
        (runtime_flags & (UINT32_C(1) << 20u)) != 0u ||
        (board_flags & (UINT32_C(1) << 16u)) != 0u ||
        (packed_value != UINT32_C(0x00000200)) ||
        (source_bytes[4] | source_bytes[5] | source_bytes[6] |
         source_bytes[7]) != 0u ||
        (player_flags & (UINT32_C(1) << 6u)) != 0u ||
        (player_flags & (UINT32_C(1) << 5u)) != 0u ||
        (player_flags & (UINT32_C(1) << 23u)) != 0u ||
        (player_flags & (UINT32_C(1) << 21u)) != 0u ||
        (selector & (UINT32_C(1) << 13u)) != 0u ||
        (selector & (UINT32_C(1) << 14u)) != 0u ||
        (branch_byte & (UINT8_C(1) << 6u)) != 0u ||
        (player_type == UINT8_C(8))) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* 0x19ef8 prologue. */
    status = vf2_model2a_write_u32(
        machine, player + UINT32_C(0x5cc), 0u
    );
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x60c), 0u);
    }
    player_flags &= ~(UINT32_C(1) << 9u);
    player_flags |= UINT32_C(1) << 11u;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player, player_flags);
    }

    /* Observed 0x1a1e4 setup before its indirect record walk. */
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x802), 0u);
    }
    if (status == VF2_OK) {
        uint8_t zero = 0u;
        status = vf2_model2a_write(
            machine, player + UINT32_C(0xa00), &zero, sizeof(zero)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x812), 0u);
    }
    if (status == VF2_OK) {
        uint8_t zero = 0u;
        const uint32_t byte_offsets[] = {0x83cu, 0x844u, 0x841u, 0x823u};
        for (index = 0u; status == VF2_OK &&
                    index < sizeof(byte_offsets) / sizeof(byte_offsets[0]);
             ++index) {
            status = vf2_model2a_write(
                machine, player + byte_offsets[index], &zero, sizeof(zero)
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x844), 0u
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x850), 0u);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x818), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x854), 0u
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x800), table_value);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, player + UINT32_C(0x80a), table_value
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, player + UINT32_C(0x80c), table_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0x802), &record_byte_9, 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0x803), &record_byte_10, 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x804), packed_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0x810), &source_bytes[4], 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0x811), &source_bytes[5], 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0x814), &source_bytes[6], 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0x815), &source_bytes[7], 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x1a4), packed_value
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, player + UINT32_C(0x1a8), (uint16_t)selector
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x1aa), 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x6d0), data_pointer + 12u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x82c), data_pointer + 12u
        );
    }

    /* 0x26ef0: expand the 20 x 3 selector stream into the scratch RAM. */
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0xbd8), &scratch_base
        );
    }
    scratch_r9 = table_pointer + 2u;
    scratch_r8 = scratch_r9 + 20u;
    for (index = 0u; status == VF2_OK && index < 20u; ++index) {
        uint32_t shift = 0u;
        uint8_t raw = 0u;
        for (shift = 0u; status == VF2_OK && shift < 6u; shift += 2u) {
            uint8_t expanded = 0u;
            status = hybrid_read_u8(machine, scratch_r9, &raw);
            if (status == VF2_OK) {
                const uint32_t mode = ((uint32_t)raw >> 6u) & 3u;
                expanded = mode != 0u
                    ? (uint8_t)(mode - 1u)
                    : (uint8_t)((((uint32_t)raw >> shift) & 3u) + 3u);
                status = vf2_model2a_write(
                    machine,
                    scratch_base + UINT32_C(0x78c) + scratch_output_offset,
                    &expanded, sizeof(expanded)
                );
                if (status == VF2_OK) {
                    ++scratch_output_offset;
                }
                if (status == VF2_OK && expanded > 4u) {
                    status = hybrid_read_u8(machine, scratch_r8, &raw);
                    if (status == VF2_OK) {
                        scratch_sum += raw;
                        ++scratch_r8;
                        ++scratch_count;
                    }
                }
            }
        }
        ++scratch_r9;
    }
    if (status == VF2_OK) {
        scratch_alignment = (scratch_count + 22u) & 3u;
        scratch_alignment = (4u - scratch_alignment) & 3u;
        scratch_r8 += scratch_alignment;
        scratch_r7 = (scratch_sum << 2u) + scratch_r8;
        status = vf2_model2a_write_u32(
            machine, scratch_base + UINT32_C(0x780), scratch_r7
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, scratch_base + UINT32_C(0x784), scratch_r9
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, scratch_base + UINT32_C(0x788), scratch_r8
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0xbdc), "\0", 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0xbe4), "\0", 1u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, player + UINT32_C(0xbe5), "\0", 1u
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0xbe2), 0u);
    }

    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 2u] =
        scratch_r9 + scratch_output_offset;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = scratch_base + 0x7c8u;
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = scratch_base;
    cpu->ip = UINT32_C(0x0001428c);
    cpu->executed_instructions += UINT64_C(1652);
    cpu->procedure_calls += UINT64_C(4);
    cpu->procedure_returns += UINT64_C(4);
    return VF2_OK;
}

static float hybrid_player_bits_to_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static vf2_status hybrid_player_convert_real_to_integer(
    const vf2_i960_cpu *cpu,
    uint32_t bits,
    uint32_t *result
)
{
    const float value = hybrid_player_bits_to_float(bits);
    double rounded = 0.0;
    unsigned mode = 0u;

    if (cpu == NULL || result == NULL || !isfinite(value)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    mode = (unsigned)((cpu->arithmetic_control >> 30u) & 3u);
    switch (mode) {
    case 0u: {
        const double lower = floor((double)value);
        const double fraction = (double)value - lower;
        if (fraction < 0.5) {
            rounded = lower;
        } else if (fraction > 0.5) {
            rounded = lower + 1.0;
        } else {
            rounded = fmod(fabs(lower), 2.0) == 0.0
                ? lower : lower + 1.0;
        }
        break;
    }
    case 1u:
        rounded = floor((double)value);
        break;
    case 2u:
        rounded = ceil((double)value);
        break;
    case 3u:
    default:
        rounded = trunc((double)value);
        break;
    }
    if (rounded < (double)INT32_MIN || rounded > (double)INT32_MAX) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *result = (uint32_t)(int32_t)rounded;
    return VF2_OK;
}

/* Translate the accepted 0x27b5c record expander.  The second pass is
 * intentionally expressed as the behavior observed from this emulator's
 * direct-compare instructions: all accepted status bytes take the one-word
 * copy path, while the pointer stream supplies the source advances. */
static vf2_status hybrid_execute_player_27b5c(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t selector,
    uint32_t destination
)
{
    const uint32_t scratch_base = UINT32_C(0x0050e2d0);
    uint32_t table_pointer = 0u;
    uint32_t source_pointer = 0u;
    uint32_t source_cursor = 0u;
    uint32_t source_tail = 0u;
    uint32_t jump_cursor = 0u;
    uint32_t data_cursor = 0u;
    uint32_t output_cursor = scratch_base + UINT32_C(0x78c);
    uint32_t destination_cursor = destination;
    uint32_t source_sum = 0u;
    uint32_t source_count = 0u;
    uint32_t output_offset = 0u;
    uint32_t alignment = 0u;
    uint32_t converted = 0u;
    uint8_t raw = 0u;
    uint8_t jump = 0u;
    uint8_t status_byte = 0u;
    uint8_t expanded = 0u;
    uint32_t value = 0u;
    size_t row = 0u;
    size_t column = 0u;
    uint32_t shift = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x02120004) + selector * UINT32_C(4),
        &table_pointer
    );
    source_pointer = table_pointer + 2u;
    source_cursor = source_pointer;
    for (row = 0u; status == VF2_OK && row < 20u; ++row) {
        for (shift = 0u; status == VF2_OK && shift < 6u; shift += 2u) {
            uint32_t mode = 0u;
            status = hybrid_read_u8(machine, source_cursor, &raw);
            if (status != VF2_OK) {
                break;
            }
            mode = ((uint32_t)raw >> 6u) & 3u;
            expanded = mode != 0u
                ? (uint8_t)(mode - 1u)
                : (uint8_t)((((uint32_t)raw >> shift) & 3u) + 3u);
            status = vf2_model2a_write(
                machine, output_cursor + output_offset,
                &expanded, sizeof(expanded)
            );
            if (status == VF2_OK) {
                ++output_offset;
            }
            if (status == VF2_OK && expanded > 4u) {
                status = hybrid_read_u8(
                    machine, source_pointer + 20u + source_count, &jump
                );
                if (status == VF2_OK) {
                    source_sum += jump;
                    ++source_count;
                }
            }
        }
        ++source_cursor;
    }
    source_tail = source_pointer + 20u + source_count;
    alignment = (source_count + 22u) & 3u;
    alignment = (4u - alignment) & 3u;
    source_tail += alignment;
    value = (source_sum << 2u) + source_tail;
    jump_cursor = source_cursor;
    data_cursor = value;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = jump_cursor;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = output_cursor;
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = scratch_base;

    for (row = 0u; status == VF2_OK && row < 20u; ++row) {
        for (column = 0u; status == VF2_OK && column < 3u; ++column) {
            status = hybrid_read_u8(
                machine,
                output_cursor + (uint32_t)(row * 3u + column),
                &status_byte
            );
            if (status != VF2_OK) {
                break;
            }
            status = vf2_model2a_read_u32(machine, data_cursor, &value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination_cursor, value
                );
            }
            if (status == VF2_OK) {
                status = hybrid_read_u8(machine, jump_cursor, &jump);
            }
            if (status == VF2_OK) {
                ++jump_cursor;
                data_cursor += status_byte > 5u
                    ? (uint32_t)jump * 12u
                    : (uint32_t)jump << 2u;
                destination_cursor += 4u;
            }
        }
    }

    /* The direct cmpobl/be sequence leaves the emulator compare state
     * unchanged, so this accepted data set takes the 27c88 one-word path
     * for all 60 cells.  The ROM then rewinds the complete block and runs
     * cvtri/stis over its first 36 words. */
    if (status == VF2_OK) {
        destination_cursor -= 60u * 4u;
    }
    for (row = 0u; status == VF2_OK && row < 36u; ++row) {
        status = vf2_model2a_read_u32(
            machine, destination_cursor, &value
        );
        if (status == VF2_OK) {
            status = hybrid_player_convert_real_to_integer(
                cpu, value, &converted
            );
        }
        if (status == VF2_OK) {
            status = hybrid_write_u16(
                machine, destination_cursor, (uint16_t)converted
            );
        }
        if (status == VF2_OK) {
            destination_cursor += 4u;
        }
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = jump_cursor;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = output_cursor;
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = scratch_base;
    }
    return status;
}

static vf2_status hybrid_execute_player_1428c(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint32_t record_pointer = 0u;
    uint32_t scratch_base = 0u;
    uint32_t destinations[5];
    const uint32_t record_offsets[] = {0u, 2u, 0x10u, 0x14u, 0x3eu};
    uint32_t player_flags = 0u;
    uint32_t selector = 0u;
    uint32_t index = 0u;
    uint16_t record_selector = 0u;
    uint8_t player_byte = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x0001428c) ||
        player == 0u || cpu->local_frame_depth < 2u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1a0), &record_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0xbd8), &scratch_base
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    destinations[0] = scratch_base + 0x1e0u;
    destinations[1] = scratch_base + 0x2d0u;
    destinations[2] = scratch_base + 0x3c0u;
    destinations[3] = scratch_base + 0x4b0u;
    destinations[4] = scratch_base + 0x5a0u;
    if (status == VF2_OK) {
        player_flags |= UINT32_C(1) << 26u;
        status = vf2_model2a_write_u32(machine, player, player_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x10), UINT32_C(0x00501500)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x0c), UINT32_C(0x000142f4)
        );
    }
    for (index = 0u; status == VF2_OK && index < 5u; ++index) {
        status = hybrid_read_u16(
            machine, record_pointer + record_offsets[index], &record_selector
        );
        selector = record_selector;
        if (status == VF2_OK) {
            status = hybrid_execute_player_27b5c(
                machine, cpu, selector, destinations[index]
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x640), &selector
        );
        cpu->registers[VF2_I960_G0_REGISTER] = selector;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player + UINT32_C(4), &player_byte
        );
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = player_byte;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player + UINT32_C(0x1b0), &player_byte
        );
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = player_byte;
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[2] = UINT32_C(0x0001429c);
    cpu->registers[15] = UINT32_C(0x000142f4);
    cpu->registers[19] = UINT32_C(0x00520630);
    cpu->registers[VF2_I960_G0_REGISTER + 5u] =
        UINT32_C(0x0050ea98);
    cpu->registers[VF2_I960_G0_REGISTER + 6u] =
        UINT32_C(0x0050e2d0);
    cpu->local_frames[2].registers[2] = UINT32_C(0x0001429c);
    cpu->local_frames[2].registers[15] = player_flags;
    cpu->local_frames[3].registers[2] = UINT32_C(0x0002712c);
    cpu->local_frames[3].registers[8] = scratch_base;
    cpu->local_frames[3].registers[9] = record_pointer;
    cpu->local_frames[3].registers[11] = 0u;
    cpu->local_frames[3].registers[13] = 0u;
    cpu->local_frames[3].registers[15] = 0u;
    cpu->ip = UINT32_C(0x000142c0);
    cpu->executed_instructions += UINT64_C(9726);
    cpu->procedure_calls += UINT64_C(6);
    cpu->procedure_returns += UINT64_C(6);
    return VF2_OK;
}

static vf2_status hybrid_execute_game_info_18144_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
);

static vf2_status hybrid_execute_game_info_18644(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
);

static vf2_status hybrid_execute_game_info_child_rom(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t target,
    uint32_t return_address
)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status = VF2_OK;


    status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);
    if (status != VF2_OK) {
        return status;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = return_address;
    options.max_steps = VF2_INTERPRETED_TASK_STEP_LIMIT;
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    if (result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu->ip != return_address) {
        return VF2_ERROR_UNSUPPORTED;
    }
    ++cpu->executed_instructions;
    return VF2_OK;
}

/* Recover the two small command-port helpers used by the 0x181c0 corridor.
 * They share one body at 0x18e0c; 0x18e08 selects r11=0 and publishes the two
 * returned triples through g1, while 0x18e00 selects r11=1 and keeps those
 * triples private. */
static vf2_status hybrid_execute_game_info_18e_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t target,
    uint32_t return_address
)
{
    const uint32_t port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                          cpu->registers[VF2_I960_G0_REGISTER + 12u];
    const bool publish = target == UINT32_C(0x00018e08);
    uint32_t r3 = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r6 = 0u;
    uint32_t r7 = 0u;
    uint32_t r8 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r11 = publish ? 0u : 1u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t r15 = 0u;
    uint32_t threshold = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        (target != UINT32_C(0x00018e00) &&
         target != UINT32_C(0x00018e08)) ||
        (return_address != UINT32_C(0x00018208) &&
         return_address != UINT32_C(0x00018240) &&
         return_address != UINT32_C(0x00018278)) ||
        (return_address == UINT32_C(0x00018208)
            ? cpu->ip != UINT32_C(0x00018204)
            : (return_address == UINT32_C(0x00018240)
                ? cpu->ip != UINT32_C(0x0001823c)
                : cpu->ip != UINT32_C(0x00018274)))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    status = hybrid_read_u32_triple(
        machine, cpu->registers[VF2_I960_G0_REGISTER],
        &r12, &r13, &r14
    );
    r15 = UINT32_C(0x14802929);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r15);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(machine, port, &r4, &r5, &r6);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER] +
                UINT32_C(0x0000000c), &r12, &r13, &r14
        );
    }
    if (status == VF2_OK) {
        r15 = UINT32_C(0x14802929);
        status = vf2_model2a_write_u32(machine, port, r15);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(machine, port, &r8, &r9, &r10);
    }
    if (status == VF2_OK && publish) {
        status = vf2_model2a_write_u32(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 1u], r4
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 1u] +
                    UINT32_C(4), r5
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 1u] +
                    UINT32_C(8), r6
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 1u] +
                    UINT32_C(0x0000000c), r8
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 1u] +
                    UINT32_C(0x00000010), r9
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 1u] +
                    UINT32_C(0x00000014), r10
            );
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a0bc), &threshold
        );
    }
    if (status == VF2_OK) {
        if ((int32_t)r5 > (int32_t)threshold) {
            r7 ^= UINT32_C(1) << 15u;
        } else {
            r12 = threshold | UINT32_C(0x80000000);
            hybrid_set_compare_result(
                cpu,
                hybrid_float_from_bits(r5) < hybrid_float_from_bits(r12)
                    ? VF2_I960_COMPARE_LESS
                    : (hybrid_float_from_bits(r5) >
                           hybrid_float_from_bits(r12)
                        ? VF2_I960_COMPARE_GREATER
                        : VF2_I960_COMPARE_EQUAL)
            );
            if (cpu->compare_result == VF2_I960_COMPARE_LESS) {
                r15 = UINT32_C(0x17802f2f);
                status = vf2_model2a_write_u32(machine, port, r15);
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, 0u);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, r10);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, r8);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, 0u);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(machine, port, &r3);
                }
            } else {
                r15 = UINT32_C(0x17802f2f);
                status = vf2_model2a_write_u32(machine, port, r15);
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, 0u);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, r6);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, r4);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write_u32(machine, port, 0u);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_read_u32(machine, port, &r3);
                }
                if (status == VF2_OK && (r9 & UINT32_C(0x80000000)) != 0u) {
                    r7 ^= UINT32_C(1) << 15u;
                }
            }
        }
    }
    if (status == VF2_OK) {
        r3 += r7;
        cpu->registers[3] = r3;
        cpu->registers[4] = r4;
        cpu->registers[5] = r5;
        cpu->registers[6] = r6;
        cpu->registers[7] = r7;
        cpu->registers[8] = r8;
        cpu->registers[9] = r9;
        cpu->registers[10] = r10;
        cpu->registers[11] = r11;
        cpu->registers[12] = r12;
        cpu->registers[13] = r13;
        cpu->registers[14] = r14;
        cpu->registers[15] = r15;
        cpu->registers[VF2_I960_G0_REGISTER] = r3;
        cpu->ip = UINT32_C(0x00018ed8);
        cpu->executed_instructions += UINT64_C(30);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    return status;
}

static vf2_status hybrid_execute_player_4b5d0(
    vf2_model2a *machine,
    uint32_t player,
    uint32_t *table_pointer,
    uint32_t *last_value
)
{
    uint8_t table_index = 0u;
    uint32_t table_entry = 0u;
    uint16_t value16 = 0u;
    uint32_t value32 = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || table_pointer == NULL || last_value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_write_u32(
        machine, player + UINT32_C(0x76c), 0u
    );
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x6b0), 0u);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x6a8), 0u);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x6aa), 0u);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player + UINT32_C(0x1b1), &table_index
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x020062a0) + (uint32_t)table_index * 4u,
            &table_entry
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    *table_pointer = table_entry;
    *last_value = 0u;
    if (table_entry == 0u) {
        return VF2_OK;
    }
    status = vf2_model2a_write_u32(
        machine, player + UINT32_C(0x76c), table_entry
    );
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, table_entry, &value16);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x6b0), value16);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, table_entry + 4u, &value16);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x6ac), value16);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, table_entry + 6u, &value16);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(machine, player + UINT32_C(0x6ae), value16);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table_entry + 8u, &value32);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player + UINT32_C(0x6a0), value32);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table_entry + 0xcu, &value32);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player + UINT32_C(0x6a4), value32);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table_entry + 0x10u, &value32);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, player + UINT32_C(0x6b8), value32);
        *last_value = value32;
    }
    return status;
}

static vf2_status hybrid_execute_player_142c0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t g0 = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER] : 0u;
    const uint32_t g1 = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 1u] : 0u;
    const uint32_t g2 = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 2u] : 0u;
    uint32_t task_record = 0u;
    uint32_t table_pointer = 0u;
    uint32_t table_last_value = 0u;
    uint32_t player_flags = 0u;
    uint8_t phase = 0u;
    uint8_t frame_phase = 0u;
    uint16_t frame_selector = 0u;
    uint8_t cleared = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x000142c0) ||
        player == 0u || g1 != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00550000), 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050083c), &task_record
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502c0), 3u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502c4), g0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502c8), g1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502cc), g2
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, UINT32_C(0x00550188), &frame_selector
        );
    }
    if (status == VF2_OK && g0 != (uint32_t)frame_selector) {
        cleared = 0u;
        status = vf2_model2a_write(
            machine, task_record + UINT32_C(0x44), &cleared, sizeof(cleared)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(machine, UINT32_C(0x0050002b), &phase);
    }
    if (status != VF2_OK || phase == 6u || phase == 7u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = hybrid_execute_player_4b5d0(
        machine, player, &table_pointer, &table_last_value
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &player_flags);
    }
    if (status == VF2_OK) {
        player_flags |= UINT32_C(1) << 21u;
        status = vf2_model2a_write_u32(machine, player, player_flags);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(machine, UINT32_C(0x00530005), &frame_phase);
    }
    if (status != VF2_OK || frame_phase != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = table_pointer;
    cpu->registers[2] = UINT32_C(0x000142e8);
    cpu->registers[15u] = table_last_value;
    cpu->ip = UINT32_C(0x00014310);
    cpu->executed_instructions += UINT64_C(56);
    cpu->procedure_calls += UINT64_C(3);
    cpu->procedure_returns += UINT64_C(3);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_14310(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint32_t player_flags = 0u;
    uint32_t player_mode = 0u;
    uint32_t record_pointer = 0u;
    uint16_t player_selector = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00014310) ||
        player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1a4), &player_mode
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, player + UINT32_C(0x1a8), &player_selector
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1a0), &record_pointer
        );
    }
    if (status == VF2_OK) {
        player_flags &= ~(UINT32_C(1) << 11u);
        status = vf2_model2a_write_u32(machine, player, player_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x1e18), player_mode
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, player + UINT32_C(0x1e1c), player_selector
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[2] = UINT32_C(0x0001438c);
    cpu->registers[3] = (uint32_t)player_selector;
    cpu->registers[5] = player_flags;
    cpu->registers[12] = 1u;
    cpu->registers[15] = player_flags;
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = record_pointer;
    cpu->ip = UINT32_C(0x000143e4);
    cpu->executed_instructions += UINT64_C(86);
    cpu->procedure_calls += UINT64_C(3);
    cpu->procedure_returns += UINT64_C(3);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_143e4_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x000143e4) ||
        player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* The four observed helpers at 1b5c8, 146ec, 1499c and 1b568 are
     * state-neutral for this accepted player record.  Preserve their
     * observed call/return cost and post-call register state. */
    cpu->registers[2] = UINT32_C(0x000143fc);
    cpu->registers[15] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    cpu->ip = UINT32_C(0x000143fc);
    cpu->executed_instructions += UINT64_C(19);
    cpu->procedure_calls += UINT64_C(4);
    cpu->procedure_returns += UINT64_C(4);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_1ab74_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint16_t selector = 0u;
    uint16_t counter = 0u;
    uint16_t player_selector = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x000143fc) ||
        player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = hybrid_read_u16(
        machine, player + UINT32_C(0x1aa), &counter
    );
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, player + UINT32_C(0x800), &selector
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, player + UINT32_C(0x1a8), &player_selector
        );
    }
    if (status == VF2_OK && (counter == 0u || counter > selector)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0001ab74), UINT32_C(0x00014400)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = counter;
    cpu->registers[4] = selector;
    cpu->registers[14] = player_selector;
    cpu->ip = UINT32_C(0x0001abf4);
    cpu->executed_instructions += UINT64_C(7);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_27ce0_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_status status = VF2_OK;

    (void)machine;
    if (cpu == NULL || cpu->ip != UINT32_C(0x0001abf4)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00027ce0), UINT32_C(0x0001abf8)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->ip = UINT32_C(0x00027d00);
    cpu->executed_instructions += UINT64_C(5);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_27d00_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_status status = VF2_OK;

    (void)machine;
    if (cpu == NULL || cpu->ip != UINT32_C(0x00027d00)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00028184), UINT32_C(0x00027d04)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->ip = UINT32_C(0x00028184);
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_player_28184_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint16_t counter = 0u;
    uint32_t scratch = 0u;
    uint32_t mode = 0u;
    uint32_t curve = 0u;
    uint8_t status_byte = 0u;
    vf2_status result = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00028184) ||
        player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    result = hybrid_read_u16(
        machine, player + UINT32_C(0x1aa), &counter
    );
    if (result == VF2_OK) {
        result = vf2_model2a_read_u32(
            machine, player + UINT32_C(0xbd8), &scratch
        );
    }
    if (result == VF2_OK) {
        result = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500068), &mode
        );
    }
    /* The controlled bit-31 snapshot reaches the bit-20-clear table path.
     * Its table pointer is null, so the ROM branches directly to the shared
     * counter/status tail at 0x2825c. */
    if (result == VF2_OK &&
        (mode & (UINT32_C(1) << 20u)) == 0u) {
        result = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x854), &curve
        );
    }
    if (result == VF2_OK) {
        result = hybrid_read_u8(
            machine, player + UINT32_C(0xbdd), &status_byte
        );
    }
    if (result != VF2_OK || counter != 1u ||
        (mode & (UINT32_C(1) << 17u)) != 0u ||
        ((mode & (UINT32_C(1) << 20u)) == 0u && curve != 0u)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = counter;
    cpu->registers[6] = scratch;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = scratch + UINT32_C(0x690);
    cpu->registers[10] = (mode & (UINT32_C(1) << 20u)) != 0u
        ? mode : curve;
    cpu->registers[15] = status_byte;
    cpu->ip = UINT32_C(0x00028268);
    cpu->executed_instructions +=
        (mode & (UINT32_C(1) << 20u)) != 0u
            ? UINT64_C(10) : UINT64_C(11);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_28268_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_status status = VF2_OK;

    (void)machine;
    if (cpu == NULL || cpu->ip != UINT32_C(0x00028268)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00028780), UINT32_C(0x0002826c)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->ip = UINT32_C(0x00028780);
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_player_28780(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint32_t scratch = 0u;
    uint32_t g2 = 0u;
    uint32_t g3 = 0u;
    uint32_t g4 = 0u;
    uint32_t g5 = 0u;
    uint32_t output_start = 0u;
    uint32_t player_flags = 0u;
    uint32_t value = 0u;
    uint32_t converted = 0u;
    uint32_t index = 0u;
    uint32_t offset = 0u;
    uint8_t type = 0u;
    uint8_t table_index = 0u;
    uint32_t last_value = 0u;
    uint32_t last_offset = 0u;
    uint8_t last_type = 0u;
    uint8_t last_index = 0u;
    size_t cell = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00028780) ||
        player == 0u || cpu->local_frame_depth < 6u ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, player, &player_flags);
    if (status != VF2_OK || (player_flags & (UINT32_C(1) << 6u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(
        machine, player + UINT32_C(0xbd8), &scratch
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, scratch + UINT32_C(0x780), &g2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, scratch + UINT32_C(0x784), &g3
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    g4 = scratch + UINT32_C(0x78c);
    g5 = cpu->registers[VF2_I960_G0_REGISTER + 5u];
    output_start = g5;

    /* cmpobe does not update the i960 arithmetic condition.  On the
     * observed entry it is EQUAL, so the following bg at 0x2879c is never
     * taken: every non-type-4 cell consumes one byte from g3. */
    for (cell = 0u; status == VF2_OK && cell < 60u; ++cell) {
        status = hybrid_read_u8(machine, g4, &type);
        if (status != VF2_OK) {
            break;
        }
        if (type == 4u) {
            status = vf2_model2a_read_u32(machine, g2, &value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, g5, value);
            }
            g2 += UINT32_C(4);
            offset = UINT32_C(4);
        } else {
            status = hybrid_read_u8(machine, g3, &table_index);
            if (status == VF2_OK) {
                ++g3;
                status = vf2_model2a_read_u32(machine, g2, &value);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, g5, value);
            }
            if (type == 5u) {
                offset = (uint32_t)table_index << 2u;
            } else {
                index = (uint32_t)table_index;
                offset = (index * UINT32_C(3)) << 2u;
            }
            g2 += offset;
        }
        last_value = value;
        last_offset = offset;
        last_type = type;
        last_index = table_index;
        ++g4;
        g5 += UINT32_C(4);
    }
    if (status != VF2_OK) {
        return status;
    }
    g5 = output_start;
    for (cell = 0u; status == VF2_OK && cell < 36u; ++cell) {
        status = vf2_model2a_read_u32(machine, g5, &value);
        if (status == VF2_OK) {
            status = hybrid_player_convert_real_to_integer(
                cpu, value, &converted
            );
        }
        if (status == VF2_OK) {
            status = hybrid_write_u16(machine, g5, (uint16_t)converted);
        }
        g5 += UINT32_C(4);
    }
    if (status != VF2_OK) {
        return status;
    }
    g5 = output_start;
    cpu->registers[3u] = last_type == 3u ? 0u : cpu->registers[3u];
    cpu->registers[4u] = last_offset;
    cpu->registers[6u] = last_type;
    cpu->registers[11u] = last_index;
    cpu->registers[14u] = last_value;
    cpu->registers[15u] = player_flags;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = g2;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = g3;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = g4;
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = g5;
    cpu->registers[12u] = 0u;
    cpu->registers[13u] = converted;
    cpu->ip = UINT32_C(0x0002826c);
    cpu->executed_instructions += UINT64_C(1024);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    return status;
}


static vf2_status hybrid_execute_player_2826c_to_27d90(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t scratch = cpu != NULL ? cpu->registers[6u] : 0u;
    const uint32_t copro_address = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t source = scratch + UINT32_C(0x690);
    uint32_t flags = 0u;
    uint32_t phase_word = 0u;
    uint32_t relative = 0u;
    uint32_t values[3] = {0u, 0u, 0u};
    uint32_t word = 0u;
    uint16_t counter = 0u;
    uint16_t state_code = 0u;
    uint16_t half = 0u;
    uint8_t status_byte = 0u;
    uint8_t control_byte = 0u;
    size_t index = 0u;
    size_t component = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != UINT32_C(0x0002826c) ||
        player == 0u || scratch == 0u || cpu->local_frame_depth < 5u ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = hybrid_read_u16(machine, player + UINT32_C(0x1aa), &counter);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x804), &word
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, player + UINT32_C(0xbdd), &status_byte
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500034), &phase_word
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, player + UINT32_C(0x1a8), &state_code
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(machine, player + UINT32_C(4), &control_byte);
    }
    if (status != VF2_OK || counter != UINT16_C(1) ||
        (word & UINT32_C(0x00020010)) != 0u || status_byte != 0u ||
        (flags & (UINT32_C(1) << 2u)) != 0u || phase_word > UINT32_C(6) ||
        state_code == UINT16_C(0x00d8) || state_code == UINT16_C(0x0519) ||
        (control_byte & UINT8_C(1)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* status == 0 takes the 0x283c0 path and clears the 72-byte staging area. */
    {
        uint8_t zero[72] = {0};
        status = vf2_model2a_write(
            machine, player + UINT32_C(0xbe8), zero, sizeof(zero)
        );
    }

    /* 0x2859c distributes the 240-byte expansion emitted by 0x28780. */
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00029714) + (uint32_t)index * UINT32_C(4),
            &relative
        );
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine, source + (uint32_t)component * UINT32_C(4),
                &values[component]
            );
            if (status == VF2_OK) {
                status = hybrid_write_u16(
                    machine,
                    player + relative + (uint32_t)component * UINT32_C(2),
                    (uint16_t)values[component]
                );
            }
        }
        source += UINT32_C(12);
    }
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_read_u32(
            machine, source + (uint32_t)component * UINT32_C(4),
            &values[component]
        );
        if (status == VF2_OK) {
            status = hybrid_write_u16(
                machine,
                player + UINT32_C(0x140) + (uint32_t)component * UINT32_C(2),
                (uint16_t)values[component]
            );
        }
    }
    source += UINT32_C(12);
    for (index = 0u; status == VF2_OK && index < 7u; ++index) {
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine, source + (uint32_t)component * UINT32_C(4),
                &values[component]
            );
            if (status == VF2_OK) {
                status = hybrid_write_u16(
                    machine,
                    player + UINT32_C(0xba8) + (uint32_t)index * UINT32_C(6) +
                        (uint32_t)component * UINT32_C(2),
                    (uint16_t)values[component]
                );
            }
        }
        source += UINT32_C(12);
    }
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_read_u32(
            machine, source + (uint32_t)component * UINT32_C(4),
            &values[component]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine,
                player + UINT32_C(0x80) + (uint32_t)component * UINT32_C(4),
                values[component]
            );
        }
    }
    source += UINT32_C(12);
    for (index = 0u; status == VF2_OK && index < 7u; ++index) {
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = vf2_model2a_read_u32(
                machine, source + (uint32_t)component * UINT32_C(4),
                &values[component]
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine,
                    player + UINT32_C(0xb00) + (uint32_t)index * UINT32_C(12) +
                        (uint32_t)component * UINT32_C(4),
                    values[component]
                );
            }
        }
        source += UINT32_C(12);
    }
    if (status != VF2_OK) {
        return status;
    }

    /* The observed phase/state gates branch directly to the procedure RET. */
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00027d04)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* 0x27d04..0x27d8c emits three overwritten triples followed by the
     * persistent 0x00800101 command and prepares the next geometry call. */
    status = vf2_model2a_write_u32(
        machine, copro_address, UINT32_C(0x31006262)
    );
    for (component = 0u; status == VF2_OK && component < 3u; ++component) {
        status = vf2_model2a_read_u32(
            machine,
            player + UINT32_C(0x80) + (uint32_t)component * UINT32_C(4),
            &values[component]
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, copro_address + (uint32_t)component * UINT32_C(4),
                values[component]
            );
        }
    }
    {
        static const uint32_t offsets[3] = {
            UINT32_C(0x144), UINT32_C(0x142), UINT32_C(0x140)
        };
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = hybrid_read_u16(
                machine, player + offsets[component], &half
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, copro_address + (uint32_t)component * UINT32_C(4),
                    (uint32_t)(int32_t)(int16_t)half
                );
            }
        }
    }
    {
        static const uint32_t offsets[3] = {
            UINT32_C(0xc02), UINT32_C(0xc00), UINT32_C(0xc04)
        };
        for (component = 0u; status == VF2_OK && component < 3u; ++component) {
            status = hybrid_read_u16(
                machine, player + offsets[component], &half
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, copro_address + (uint32_t)component * UINT32_C(4),
                    (uint32_t)(int32_t)(int16_t)half
                );
            }
            cpu->registers[12u + component] =
                (uint32_t)(int32_t)(int16_t)half;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, copro_address, UINT32_C(0x33806767)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, copro_address, UINT32_C(29) << 9u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, copro_address, UINT32_C(0x00800101)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[15u] = UINT32_C(0x00800101);
    cpu->registers[VF2_I960_G0_REGISTER] = player + UINT32_C(0x146);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(1);
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = player + UINT32_C(0xc06);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = player + UINT32_C(0xb00);
    cpu->registers[VF2_I960_G0_REGISTER + 5u] = player + UINT32_C(0xba8);
    cpu->registers[VF2_I960_G0_REGISTER + 6u] = UINT32_C(0x000296a0);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = UINT32_C(0x00027d90);
    cpu->executed_instructions += UINT64_C(222);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_2901c_first_to_27dcc(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t state_flags = 0u;
    uint32_t player_flags = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r8 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r11 = 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t cursor = 0u;
    uint16_t half = 0u;
    uint8_t bdc = 0u;
    uint8_t control = 0u;
    uint8_t cursor_byte = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != UINT32_C(0x0002901c) || player == 0u ||
        cpu->local_frame_depth < 5u ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != UINT32_C(1) ||
        cpu->registers[VF2_I960_G0_REGISTER] != player + UINT32_C(0x146) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] !=
            player + UINT32_C(0xc06) ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] !=
            player + UINT32_C(0xb00) ||
        cpu->registers[VF2_I960_G0_REGISTER + 5u] !=
            player + UINT32_C(0xba8) ||
        cpu->registers[VF2_I960_G0_REGISTER + 6u] !=
            UINT32_C(0x000296a0) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = hybrid_read_u8(machine, player + UINT32_C(0xbdc), &bdc);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x1a4), &state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &player_flags);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(machine, player + UINT32_C(4), &control);
    }
    if (status != VF2_OK || (bdc & UINT8_C(2)) != 0u ||
        (state_flags & UINT32_C(0x00016000)) != 0u ||
        (state_flags & UINT32_C(0x0000020c)) == 0u ||
        (player_flags & (UINT32_C(1) << 6u)) != 0u ||
        (control & UINT8_C(1)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* 0x29050: publish the fixed command header and the three observed
     * input triples.  Scalar command operands share the FIFO address while
     * ldt/stt triples use its adjacent words, matching the i960 bus accesses. */
    status = vf2_model2a_write_u32(machine, port, UINT32_C(0x03000606));
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 6u],
            &r12, &r13, &r14
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x1f803f3f));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(4), &half
        );
        r12 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(2), &half
        );
        r13 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 5u], &half
        );
        r14 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    cpu->registers[VF2_I960_G0_REGISTER + 5u] += UINT32_C(6);

    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(2), &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 3u], &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(4), &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 3u] += UINT32_C(6);

    /* g1 == 1 reaches the 0x29158 adjustment path. */
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x35006a6a));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, player + UINT32_C(0xb0c), &r8, &r9, &r10
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(machine, port, &r8, &r9, &r10);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r11 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status != VF2_OK || (int32_t)r11 < 0 || r11 > UINT32_C(0x2000)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    r11 >>= 1u;

    status = hybrid_read_u16(machine, player + UINT32_C(0xbe6), &half);
    if (status == VF2_OK) {
        r10 = (uint32_t)(int32_t)(int16_t)half;
        {
            const int32_t delta = (int32_t)r10 - (int32_t)r11;
            if (delta < -256 || delta > 256) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }
        status = hybrid_write_u16(
            machine, player + UINT32_C(0xbe6), (uint16_t)r11
        );
    }

    /* 0x29214: transform the first full-precision triple.  g1 != 2 selects
     * the compact 0x29354 branch used on this first invocation. */
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x35006a6a));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 4u],
            &r8, &r9, &r10
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(machine, port, &r8, &r9, &r10);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r9);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r4 = (uint32_t)(0u - (uint32_t)(int32_t)(int16_t)half);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(4),
            (uint16_t)r4
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x16802d2d));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r5);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r5);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r5 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(2),
            (uint16_t)r5
        );
    }
    if (status != VF2_OK || r4 == 0u || r5 != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* r4 is nonzero on the measured path, so the ROM appends one signed
     * diagnostic word to the geometry log ring and advances its byte cursor. */
    status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e4), &cursor);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0090e000) + cursor, r4
        );
    }
    cursor += UINT32_C(4);
    cursor_byte = (uint8_t)cursor;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005001e4), &cursor_byte, sizeof(cursor_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x37006e6e));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] += UINT32_C(12);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] += UINT32_C(12);
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(6);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] += UINT32_C(1);
    cpu->executed_instructions += UINT64_C(94);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00027d94)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    ++cpu->executed_instructions;

    /* 0x27d94..0x27dc8 is the caller tail before the second 0x2901c call. */
    status = vf2_model2a_write_u32(machine, port, UINT32_C(0x33806767));
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x00003a0c));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x00800101));
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15u] = UINT32_C(0x00800101);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = UINT32_C(0x00027dcc);
    cpu->executed_instructions += UINT64_C(9);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_2901c_second_to_27dd0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t player_flags = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r8 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint16_t half = 0u;
    uint8_t bdc = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != UINT32_C(0x0002901c) || player == 0u ||
        cpu->local_frame_depth < 5u ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != UINT32_C(2) ||
        cpu->registers[VF2_I960_G0_REGISTER] != player + UINT32_C(0x14c) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] !=
            player + UINT32_C(0xc0c) ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] !=
            player + UINT32_C(0xb0c) ||
        cpu->registers[VF2_I960_G0_REGISTER + 5u] !=
            player + UINT32_C(0xbae) ||
        cpu->registers[VF2_I960_G0_REGISTER + 6u] !=
            UINT32_C(0x000296ac) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = hybrid_read_u16(machine, player + UINT32_C(0xbe6), &half);
    if (status == VF2_OK) {
        status = hybrid_read_u8(machine, player + UINT32_C(0xbdc), &bdc);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, player, &player_flags);
    }
    if (status != VF2_OK || half != 0u || (bdc & UINT8_C(4)) != 0u ||
        (player_flags & (UINT32_C(1) << 21u)) == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_model2a_write_u32(machine, port, UINT32_C(0x03000606));
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 6u],
            &r12, &r13, &r14
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x1f803f3f));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(4), &half
        );
        r12 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(2), &half
        );
        r13 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 5u], &half
        );
        r14 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    cpu->registers[VF2_I960_G0_REGISTER + 5u] += UINT32_C(6);

    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(2), &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 3u], &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(4), &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 3u] += UINT32_C(6);

    status = vf2_model2a_write_u32(machine, port, UINT32_C(0x35006a6a));
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 4u],
            &r8, &r9, &r10
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(machine, port, &r8, &r9, &r10);
    }
    if (status != VF2_OK) {
        return status;
    }

    player_flags &= ~(UINT32_C(1) << 21u);
    status = vf2_model2a_write_u32(machine, player, player_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r9);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r4 = (uint32_t)(0u - (uint32_t)(int32_t)(int16_t)half);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(4),
            (uint16_t)r4
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x16802d2d));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r5);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r5);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r5 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(2),
            (uint16_t)r5
        );
    }
    if (status != VF2_OK || r4 != 0u || r5 != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] += UINT32_C(12);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] += UINT32_C(12);
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(6);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] += UINT32_C(1);
    cpu->executed_instructions += UINT64_C(59);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00027dd0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_player_27dd0_to_27fa0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t discard = 0u;
    uint16_t half = 0u;
    uint8_t control = 0u;
    unsigned iteration = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != UINT32_C(0x00027dd0) || player == 0u ||
        cpu->local_frame_depth < 4u ||
        cpu->registers[VF2_I960_G0_REGISTER] != player + UINT32_C(0x152) ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != UINT32_C(3) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] !=
            player + UINT32_C(0xc12) ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] !=
            player + UINT32_C(0xb18) ||
        cpu->registers[VF2_I960_G0_REGISTER + 5u] !=
            player + UINT32_C(0xbb4) ||
        cpu->registers[VF2_I960_G0_REGISTER + 6u] !=
            UINT32_C(0x000296b8) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = hybrid_read_u8(machine, player + UINT32_C(4), &control);
    if (status != VF2_OK || (control & UINT8_C(1)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (iteration = 0u; iteration < 2u && status == VF2_OK; ++iteration) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x01000202));
        if (status == VF2_OK && iteration == 0u) {
            status = vf2_model2a_write_u32(
                machine, port, UINT32_C(0x00800101)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x09801313));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x09801313));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x09801313));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, port, &discard);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x35806b6b));
        }
        if (status == VF2_OK) {
            status = hybrid_read_u32_triple(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 6u],
                &r12, &r13, &r14
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(4), &half
            );
            r12 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(2), &half
            );
            r13 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 5u], &half
            );
            r14 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(2), &half
            );
            r12 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 3u], &half
            );
            r13 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(4), &half
            );
            r14 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = hybrid_read_u32_triple(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 4u],
                &r12, &r13, &r14
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 6u] + UINT32_C(16),
                &r12
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 6u] + UINT32_C(12),
                &r13
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            const uint32_t base = iteration == 0u
                ? UINT32_C(0x00003a30) : UINT32_C(0x00003a54);
            status = vf2_model2a_write_u32(machine, port, base);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, port, base - UINT32_C(12)
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, 0u);
        }
        if (status != VF2_OK) {
            return status;
        }

        cpu->registers[VF2_I960_G0_REGISTER + 6u] += UINT32_C(20);
        cpu->registers[VF2_I960_G0_REGISTER + 5u] += UINT32_C(6);
        cpu->registers[VF2_I960_G0_REGISTER + 4u] += UINT32_C(12);
        cpu->registers[VF2_I960_G0_REGISTER + 3u] += UINT32_C(6);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] += UINT32_C(1);
        cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(14);
        status = vf2_model2a_read_u32(machine, port, &discard);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x01000202));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[12u] = r12;
    cpu->registers[13u] = r13;
    cpu->registers[14u] = r14;
    cpu->registers[15u] = UINT32_C(0x01000202);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = UINT32_C(0x00027fa0);
    cpu->executed_instructions += UINT64_C(92);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_2901c_third_to_27fa4(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r8 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t cursor = 0u;
    uint16_t half = 0u;
    uint8_t bdc = 0u;
    uint8_t cursor_byte = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != UINT32_C(0x0002901c) || player == 0u ||
        cpu->local_frame_depth < 5u ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != UINT32_C(5) ||
        cpu->registers[VF2_I960_G0_REGISTER] != player + UINT32_C(0x16e) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] !=
            player + UINT32_C(0xc1e) ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] !=
            player + UINT32_C(0xb30) ||
        cpu->registers[VF2_I960_G0_REGISTER + 5u] !=
            player + UINT32_C(0xbc0) ||
        cpu->registers[VF2_I960_G0_REGISTER + 6u] !=
            UINT32_C(0x000296e0) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = hybrid_read_u8(machine, player + UINT32_C(0xbdc), &bdc);
    if (status != VF2_OK || (bdc & (UINT8_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* g1 == 5 bypasses both earlier special cases and follows the compact
     * transform path.  The three g3 halfwords are zero on the measured
     * state, so no optional scalar commands are emitted. */
    status = vf2_model2a_write_u32(machine, port, UINT32_C(0x03000606));
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 6u],
            &r12, &r13, &r14
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x1f803f3f));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(4), &half
        );
        r12 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(2), &half
        );
        r13 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 5u], &half
        );
        r14 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
    }
    cpu->registers[VF2_I960_G0_REGISTER + 5u] += UINT32_C(6);

    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(2), &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 3u], &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(4), &half
        );
    }
    if (status == VF2_OK && half != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 3u] += UINT32_C(6);

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x35006a6a));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, cpu->registers[VF2_I960_G0_REGISTER + 4u],
            &r8, &r9, &r10
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(machine, port, &r8, &r9, &r10);
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r9);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r4 = (uint32_t)(0u - (uint32_t)(int32_t)(int16_t)half);
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(4),
            (uint16_t)r4
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x16802d2d));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r5);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x13802727));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r5);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r10);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, port, &half);
        r5 = (uint32_t)(int32_t)(int16_t)half;
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER] + UINT32_C(2),
            (uint16_t)r5
        );
    }
    if (status != VF2_OK || r4 == 0u || r5 == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* Both signed results are nonzero on this invocation, so the ROM appends
     * both to the geometry log ring and emits their respective diagnostics. */
    status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e4), &cursor);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0090e000) + cursor, r4
        );
    }
    cursor += UINT32_C(4);
    cursor_byte = (uint8_t)cursor;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005001e4), &cursor_byte, sizeof(cursor_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x37006e6e));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e4), &cursor);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0090e000) + cursor, r5
        );
    }
    cursor += UINT32_C(4);
    cursor_byte = (uint8_t)cursor;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x005001e4), &cursor_byte, sizeof(cursor_byte)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x36806d6d));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] += UINT32_C(12);
    cpu->registers[VF2_I960_G0_REGISTER + 4u] += UINT32_C(12);
    cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(6);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] += UINT32_C(1);
    cpu->executed_instructions += UINT64_C(64);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00027fa4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_player_27fa4_to_28174(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t discard = 0u;
    uint16_t half = 0u;
    uint8_t control = 0u;
    unsigned iteration = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != UINT32_C(0x00027fa4) || player == 0u ||
        cpu->local_frame_depth < 4u ||
        cpu->registers[VF2_I960_G0_REGISTER] != player + UINT32_C(0x174) ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != UINT32_C(6) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] !=
            player + UINT32_C(0xc24) ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] !=
            player + UINT32_C(0xb3c) ||
        cpu->registers[VF2_I960_G0_REGISTER + 5u] !=
            player + UINT32_C(0xbc6) ||
        cpu->registers[VF2_I960_G0_REGISTER + 6u] !=
            UINT32_C(0x000296ec) ||
        cpu->compare_result != VF2_I960_COMPARE_EQUAL) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = hybrid_read_u8(machine, player + UINT32_C(4), &control);
    if (status != VF2_OK || (control & UINT8_C(1)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (iteration = 0u; iteration < 2u && status == VF2_OK; ++iteration) {
        if (iteration == 0u) {
            status = vf2_model2a_write_u32(
                machine, port, UINT32_C(0x00800101)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x09801313));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x09801313));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x09801313));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, port, &discard);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x35806b6b));
        }
        if (status == VF2_OK) {
            status = hybrid_read_u32_triple(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 6u],
                &r12, &r13, &r14
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(4), &half
            );
            r12 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 5u] + UINT32_C(2), &half
            );
            r13 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 5u], &half
            );
            r14 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(2), &half
            );
            r12 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 3u], &half
            );
            r13 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = hybrid_read_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 3u] + UINT32_C(4), &half
            );
            r14 = (uint32_t)(int32_t)(int16_t)half;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = hybrid_read_u32_triple(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 4u],
                &r12, &r13, &r14
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(8), r14);
        }

        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 6u] + UINT32_C(16),
                &r12
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 6u] + UINT32_C(12),
                &r13
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r12);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port + UINT32_C(4), r13);
        }
        if (status == VF2_OK) {
            const uint32_t high = iteration == 0u
                ? UINT32_C(0x00003a84) : UINT32_C(0x00003aa8);
            status = vf2_model2a_write_u32(machine, port, high);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, port, high - UINT32_C(12)
                );
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(1));
        }
        if (status != VF2_OK) {
            return status;
        }

        cpu->registers[VF2_I960_G0_REGISTER + 6u] += UINT32_C(20);
        cpu->registers[VF2_I960_G0_REGISTER + 5u] += UINT32_C(6);
        cpu->registers[VF2_I960_G0_REGISTER + 4u] += UINT32_C(12);
        cpu->registers[VF2_I960_G0_REGISTER + 3u] += UINT32_C(6);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] += UINT32_C(1);
        cpu->registers[VF2_I960_G0_REGISTER] += UINT32_C(14);
        status = vf2_model2a_read_u32(machine, port, &discard);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, UINT32_C(0x01000202));
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[12u] = r12;
    cpu->registers[13u] = r13;
    cpu->registers[14u] = r14;
    cpu->registers[15u] = UINT32_C(0x01000202);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = UINT32_C(0x00028174);
    cpu->executed_instructions += UINT64_C(93);
    return VF2_OK;
}

static vf2_status hybrid_execute_player_27d90_call(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    vf2_status status = VF2_OK;

    (void)machine;
    if (cpu == NULL || cpu->ip != UINT32_C(0x00027d90)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0002901c), UINT32_C(0x00027d94)
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->ip = UINT32_C(0x0002901c);
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_player_repeated_call(
    vf2_i960_cpu *cpu,
    uint32_t call_site,
    uint32_t target,
    uint32_t return_address
)
{
    vf2_status status = VF2_OK;

    if (cpu == NULL || cpu->ip != call_site) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);
    if (status != VF2_OK) {
        return status;
    }
    cpu->ip = target;
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status hybrid_execute_player_post_29414(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_recovered_task_report *report
)
{
    vf2_status status = VF2_OK;

    status = hybrid_execute_interpreted_until(
        machine, cpu, UINT32_C(0x00028178), UINT32_C(0x00014400)
    );
    if (status == VF2_OK) {
        status = hybrid_execute_player_repeated_call(
            cpu, UINT32_C(0x00014400), UINT32_C(0x00017710),
            UINT32_C(0x00014404)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_interpreted_until(
            machine, cpu, UINT32_C(0x00017710), UINT32_C(0x00014404)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_player_repeated_call(
            cpu, UINT32_C(0x00014404), UINT32_C(0x0001791c),
            UINT32_C(0x00014408)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_interpreted_until(
            machine, cpu, UINT32_C(0x0001791c), UINT32_C(0x00014408)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_player_repeated_call(
            cpu, UINT32_C(0x00014408), UINT32_C(0x0004b640),
            UINT32_C(0x0001440c)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_interpreted_until(
            machine, cpu, UINT32_C(0x0004b640), UINT32_C(0x00014414)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_player_repeated_call(
            cpu, UINT32_C(0x00014414), UINT32_C(0x00016504),
            UINT32_C(0x00014418)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_interpreted_until(
            machine, cpu, UINT32_C(0x00016504), UINT32_C(0x00014418)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_player_repeated_call(
            cpu, UINT32_C(0x00014418), UINT32_C(0x000180bc),
            UINT32_C(0x0001441c)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_interpreted_task(
            machine, cpu, registry_address, UINT32_C(0x000180bc), report
        );
    }
    return status;
}

static vf2_status hybrid_execute_player_29414_zero_path(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00029414) ||
        player == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = hybrid_read_u8(
        machine, player + UINT32_C(0x1b1), &selector
    );
    if (status != VF2_OK) {
        return status;
    }
    if (selector == 6u || selector == 8u || selector == 10u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(
        machine, player + UINT32_C(0xc50), 0u
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[14u] = 0u;
    cpu->ip = UINT32_C(0x00028178);
    cpu->executed_instructions += UINT64_C(7);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    return status;
}

/* The bit-31 fa_game_info path is a dispatcher around the two large fighter
 * procedures at 0x18144 and 0x18644. Recover only the observed corridors in C;
 * each native child still uses the same architectural call frame the ROM CALL
 * would create, and unobserved branches remain explicit ROM boundaries. */
static vf2_status hybrid_execute_game_info_child(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t target,
    uint32_t return_address
)
{
    if (machine == NULL || cpu == NULL || cpu->ip != return_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (target == UINT32_C(0x00018144) &&
        (return_address == UINT32_C(0x0001647c) ||
         return_address == UINT32_C(0x00016494))) {
        return hybrid_execute_game_info_18144_prefix(
            machine, cpu, return_address
        );
    }
    if (target == UINT32_C(0x00018644) &&
        (return_address == UINT32_C(0x000164b0) ||
         return_address == UINT32_C(0x000164c4))) {
        return hybrid_execute_game_info_18644(
            machine, cpu, return_address
        );
    }
    return hybrid_execute_game_info_child_rom(
        machine, cpu, target, return_address
    );
}

/* The two observed 0x18d44 calls use the zero-result Model 2 command-port
 * path.  Preserve the call frame and port writes, while rejecting the
 * unobserved non-zero result branches. */
static vf2_status hybrid_execute_game_info_18d44(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    uint32_t port = 0u;
    uint32_t r3 = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r6 = 0u;
    uint32_t r7 = 0u;
    uint32_t r8 = UINT32_C(0x461c4000);
    uint32_t r9 = UINT32_C(0x461c4000);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != return_address ||
        (return_address != UINT32_C(0x00018544) &&
         return_address != UINT32_C(0x00018550))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
           cpu->registers[VF2_I960_G0_REGISTER + 12u];
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00018d44), return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;

    cpu->registers[12] = 0u - cpu->registers[VF2_I960_G0_REGISTER];
    cpu->registers[15] = UINT32_C(0x10802121);
    status = vf2_model2a_write_u32(machine, port, cpu->registers[15]);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, cpu->registers[12]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r5);
    }
    if (status == VF2_OK) {
        cpu->registers[15] = UINT32_C(0x11002222);
        status = vf2_model2a_write_u32(machine, port, cpu->registers[15]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, cpu->registers[12]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r6);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a00c), &r7
        );
    }
    if (status == VF2_OK) {
        cpu->registers[8] = r8;
        cpu->registers[9] = r9;
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 7u] +
                UINT32_C(0x000001f4),
            &r3
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 7u] +
                UINT32_C(0x000001fc),
            &r4
        );
    }
    if (status == VF2_OK) {
        uint32_t body_instructions = 0u;

        if (r5 != 0u) {
            r3 = hybrid_float_to_bits(
                hybrid_float_from_bits(r7) - hybrid_float_from_bits(r3)
            );
            r4 = hybrid_float_to_bits(
                hybrid_float_from_bits(r6) / hybrid_float_from_bits(r5)
            );
            r4 = hybrid_float_to_bits(
                hybrid_float_from_bits(r4) * hybrid_float_from_bits(r3)
            );
            cpu->registers[15] = UINT32_C(0x16802d2d);
            status = vf2_model2a_write_u32(machine, port, cpu->registers[15]);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, port, r3);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, port, r4);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, port, &r8);
            }
            body_instructions +=
                (int32_t)cpu->registers[VF2_I960_G0_REGISTER] < 0
                    ? 9u : 10u;
        }
        /* 0x18db8 reloads fighter+0x1fc after the first branch.  r4 is
         * scratch inside 0x18d8c..0x18db4, so carrying that transformed
         * value into the second branch changes the ROM's min(r8,r9). */
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 7u] +
                    UINT32_C(0x000001fc),
                &r4
            );
        }
        if (status == VF2_OK && r6 != 0u) {
            r4 = hybrid_float_to_bits(
                hybrid_float_from_bits(r7) -
                hybrid_float_from_bits(r4)
            );
            r3 = hybrid_float_to_bits(
                hybrid_float_from_bits(r5) / hybrid_float_from_bits(r6)
            );
            r3 = hybrid_float_to_bits(
                hybrid_float_from_bits(r3) * hybrid_float_from_bits(r4)
            );
            cpu->registers[15] = UINT32_C(0x16802d2d);
            status = vf2_model2a_write_u32(machine, port, cpu->registers[15]);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, port, r3);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(machine, port, r4);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, port, &r9);
            }
            body_instructions +=
                (int32_t)cpu->registers[VF2_I960_G0_REGISTER] < 0
                    ? 9u : 10u;
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = r8;
        if (hybrid_float_from_bits(r8) < hybrid_float_from_bits(r9)) {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        } else if (hybrid_float_from_bits(r8) > hybrid_float_from_bits(r9)) {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = r9;
        } else {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
        cpu->ip = UINT32_C(0x00018dfc);
        cpu->executed_instructions += UINT64_C(19) + body_instructions;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    (void)r3;
    (void)r4;
    (void)r7;
    return status;
}

static vf2_status hybrid_execute_game_info_18c64(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    uint32_t flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != return_address ||
        return_address != UINT32_C(0x00018584)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00018c64), return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    status = vf2_model2a_read_u32(
        machine,
        cpu->registers[VF2_I960_G0_REGISTER + 7u],
        &flags
    );
    if (status == VF2_OK && (flags & (UINT32_C(1) << 2u)) != 0u) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        cpu->ip = UINT32_C(0x00018d40);
        cpu->executed_instructions += UINT64_C(3);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    return status;
}

static vf2_status hybrid_execute_game_info_18144_suffix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    const uint32_t fighter = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    uint32_t r3 = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r6 = 0u;
    uint32_t r7 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t r15 = 0u;
    uint16_t short_value = 0u;
    uint8_t byte_value = 0u;
    uint32_t suffix_instruction_delta = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != UINT32_C(0x00018550)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_write_u32(
        machine, fighter + UINT32_C(0x000005fc),
        cpu->registers[VF2_I960_G0_REGISTER + 1u]
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a00c), &r7
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001f4), &r4
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001fc), &r6
        );
    }
    if (status == VF2_OK) {
        r4 &= ~UINT32_C(0x80000000);
        r6 &= ~UINT32_C(0x80000000);
        if ((int32_t)r4 < (int32_t)r6) {
            status = VF2_ERROR_UNSUPPORTED;
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        } else if ((int32_t)r4 > (int32_t)r6) {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
        } else {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
    }
    if (status == VF2_OK) {
        r3 = hybrid_float_to_bits(
            hybrid_float_from_bits(r7) - hybrid_float_from_bits(r4)
        );
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x00000678), r3
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x0000081c),
            &cpu->registers[VF2_I960_G0_REGISTER]
        );
    }
    if (status == VF2_OK) {
        cpu->ip = UINT32_C(0x00018584);
        status = hybrid_execute_game_info_18c64(
            machine, cpu, UINT32_C(0x00018584)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x000005bc),
            (uint16_t)cpu->registers[VF2_I960_G0_REGISTER + 1u]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00500814), &r6
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, r6 + UINT32_C(0x26), &short_value);
        r13 = (uint32_t)(int32_t)(int16_t)short_value;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x000005b4), &short_value
        );
        r14 = (uint32_t)(int32_t)(int16_t)short_value;
    }
    if (status == VF2_OK) {
        r4 = r14 - r13;
        r5 = 0u;
        status = vf2_model2a_read_u32(machine, fighter, &r5);
    }
    if (status == VF2_OK) {
        r12 = r4 ^ UINT32_C(0x00008000);
        hybrid_set_compare_result(
            cpu,
            (r12 & UINT32_C(0x00008000)) != 0u
                ? VF2_I960_COMPARE_EQUAL
                : VF2_I960_COMPARE_NONE
        );
        r5 = (cpu->arithmetic_control & UINT32_C(2)) != 0u
            ? r5 | UINT32_C(2)
            : r5 & ~UINT32_C(2);
        status = vf2_model2a_write_u32(machine, fighter, r5);
    }
    if (status == VF2_OK) {
        r7 = 0u;
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001a4), &r7
        );
    }
    if (status == VF2_OK &&
        (r7 & (UINT32_C(1) << 8u)) != 0u) {
        /* 0x185d8..0x185f0 takes the state-bit-8 side path in the common
         * 0x18144 suffix. The dispatcher already accounts for its final
         * state-bit-8 branch, so this helper contributes the six remaining
         * instructions. Controlled first/second fighter probes measure +7
         * instructions end-to-end relative to the corresponding non-bit-8
         * 0x18144 path. */
        suffix_instruction_delta += UINT32_C(6);
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter + UINT32_C(0x00000821), &byte_value
        );
        r9 = byte_value;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x00000828), &short_value
        );
        r10 = (uint32_t)(int32_t)(int16_t)short_value;
        (void)r9;
        (void)r10;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x000006b4),
            (r7 & (UINT32_C(1) << 8u)) != 0u ? UINT32_C(5) : 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x00508000), &r15
        );
        if (status == VF2_OK && (r15 & (UINT32_C(1) << 5u)) != 0u) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        r5 = 30u;
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001a4), &r4
        );
    }
    if (status == VF2_OK) {
        const bool state29 = (r4 & (UINT32_C(1) << 29u)) != 0u;
        const bool state11 = (r4 & (UINT32_C(1) << 11u)) != 0u;

        /* 0x18628..0x1863c: bit 29 only inserts the BBS 11 test.  With
         * bit 11 clear it falls through to the ordinary +0x6da countdown;
         * with bit 11 set the preloaded value 30 is stored directly. */
        if (state29 && !state11) {
            ++suffix_instruction_delta;
        }
        if (state29 && state11) {
            status = hybrid_write_u16(
                machine, fighter + UINT32_C(0x000006da), (uint16_t)r5
            );
        } else {
            status = hybrid_read_u16(
                machine, fighter + UINT32_C(0x000006da), &short_value
            );
            r5 = (uint32_t)(int32_t)(int16_t)short_value;
            if (status == VF2_OK && r5 != 0u) {
                --r5;
                status = hybrid_write_u16(
                    machine, fighter + UINT32_C(0x000006da), (uint16_t)r5
                );
                suffix_instruction_delta += UINT32_C(2);
            }
        }
    }
    if (status == VF2_OK) {
        cpu->ip = UINT32_C(0x00018640);
        cpu->executed_instructions += UINT64_C(33) +
            (uint64_t)suffix_instruction_delta;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    return status;
}

static vf2_status hybrid_resolve_game_info_record(
    const vf2_model2a *machine,
    uint16_t handle,
    uint8_t target_type,
    uint32_t *record_address,
    uint32_t *instruction_count
)
{
    uint32_t cursor = 0u;
    uint32_t instructions = 4u;
    uint8_t type = 0u;
    uint8_t stride = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || record_address == NULL || instruction_count == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(
        machine,
        UINT32_C(0x0200d34c) +
            ((uint32_t)handle & UINT32_C(0x1fff)) * UINT32_C(4),
        &cursor
    );
    if (status != VF2_OK) {
        return status;
    }
    cursor += UINT32_C(8);

    for (uint32_t iteration = 0u; iteration < UINT32_C(256); ++iteration) {
        status = hybrid_read_u8(machine, cursor, &type);
        if (status != VF2_OK) {
            return status;
        }
        instructions += UINT32_C(2);
        if (type == target_type) {
            ++instructions;
            *record_address = cursor;
            *instruction_count = instructions;
            return VF2_OK;
        }

        ++instructions;
        if (type == 0u) {
            instructions += UINT32_C(2);
            *record_address = 0u;
            *instruction_count = instructions;
            return VF2_OK;
        }

        ++instructions;
        if (type == UINT8_C(8)) {
            instructions += UINT32_C(2);
            *record_address = 0u;
            *instruction_count = instructions;
            return VF2_OK;
        }

        status = hybrid_read_u8(
            machine, UINT32_C(0x0001b7f6) + (uint32_t)type, &stride
        );
        if (status != VF2_OK) {
            return status;
        }
        instructions += UINT32_C(3);
        if (stride == 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cursor += (uint32_t)stride;
    }
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status hybrid_execute_game_info_type22_equal(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t fighter0,
    uint32_t fighter1,
    uint32_t *instruction_count
)
{
    uint32_t record = 0u;
    uint32_t resolver_instructions = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    uint32_t fighter0_state = 0u;
    uint32_t value = 0u;
    uint16_t handle = 0u;
    uint16_t record_value = 0u;
    uint8_t fighter_scale_byte = 0u;
    uint8_t record_scale_byte = 0u;
    int32_t scaled = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || instruction_count == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = hybrid_read_u16(
        machine, fighter1 + UINT32_C(0x0000019c), &handle
    );
    if (status == VF2_OK) {
        status = hybrid_resolve_game_info_record(
            machine, handle, UINT8_C(5), &record, &resolver_instructions
        );
    }
    if (status != VF2_OK || record == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(machine, record + UINT32_C(1), &record_value);
    }
    if ((handle & UINT16_C(0x8000)) != 0u) {
        record_value ^= UINT16_C(0x8000);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x00000198),
            UINT32_C(0x11000000) + (uint32_t)record_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x0000001c), &value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x0000001c), value
        );
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK &&
        (fighter0_flags & (UINT32_C(1) << 2u)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter0 + UINT32_C(0x0000069c), &fighter_scale_byte
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, record + UINT32_C(3), &record_scale_byte
        );
    }
    if (status == VF2_OK) {
        const int32_t fighter_scale = (int32_t)(int8_t)fighter_scale_byte;
        const int32_t record_scale = (int32_t)(int8_t)record_scale_byte;
        scaled = ((fighter_scale * 5 + 100) * record_scale) / 100;
        status = vf2_model2a_write(
            machine, fighter0 + UINT32_C(0x00000822),
            &(uint8_t){(uint8_t)scaled}, sizeof(uint8_t)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter1 + UINT32_C(0x00000198),
            UINT32_C(0x10000000) + (uint32_t)handle
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        fighter1_flags &= ~(UINT32_C(1) << 4u);
        status = vf2_model2a_write_u32(machine, fighter1, fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001a4), &fighter0_state
        );
    }
    if (status == VF2_OK) {
        fighter0_state &= ~(UINT32_C(1) << 21u);
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x000001a4), fighter0_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        const bool bit10_set =
            (fighter1_flags & (UINT32_C(1) << 10u)) != 0u;
        /* 0x18c54 CHKBIT 10 supplies the condition consumed by the
         * following ALTERBIT 6 and remains the helper's final condition. */
        hybrid_set_compare_result(
            cpu, bit10_set ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_NONE
        );
        fighter0_flags = bit10_set
            ? fighter0_flags | (UINT32_C(1) << 6u)
            : fighter0_flags & ~(UINT32_C(1) << 6u);
        status = vf2_model2a_write_u32(machine, fighter0, fighter0_flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = record;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(5);
    cpu->procedure_calls += UINT64_C(3);
    cpu->procedure_returns += UINT64_C(3);
    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
    }

    *instruction_count = UINT32_C(38) + resolver_instructions +
        ((handle & UINT16_C(0x8000)) != 0u ? UINT32_C(1) : 0u);
    return VF2_OK;
}

/* Recover the observed shared-fighter 0x18644 port/flag corridors at both
 * dispatcher call sites. The controlled bit-4/8/14/16/6 probes also take the
 * high-result branch, including its 0x5b6 update; the remaining directions
 * stay explicitly guarded below. */
static vf2_status hybrid_execute_game_info_18644(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    const uint32_t fighter0 = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    const uint32_t fighter1 = cpu->registers[VF2_I960_G0_REGISTER + 8u];
    const uint32_t port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                          cpu->registers[VF2_I960_G0_REGISTER + 12u];
    uint32_t r3 = 0u;
    uint32_t r4 = 0u;
    uint32_t r7 = 0u;
    uint32_t r8 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r11 = 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t r15 = 0u;
    uint32_t body_instructions = 101u;
    uint32_t tail_instruction_delta = 0u;
    uint32_t relative_position_setbits = 0u;
    uint32_t signed_distance_instruction_delta = 0u;
    uint32_t state8_bit1_instruction_delta = 0u;
    uint16_t fighter0_distance_raw = 0u;
    uint16_t fighter1_distance_raw = 0u;
    bool high_result = false;
    bool countdown_path = false;
    bool mode_bit6 = false;
    bool mode_bit6_supported_bit8 = false;
    bool shared_bit1_path = false;
    uint16_t short_value = 0u;
    uint8_t byte_value = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != return_address ||
        (return_address != UINT32_C(0x000164b0) &&
         return_address != UINT32_C(0x000164c4))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00018644), return_address
    );
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;

    status = vf2_model2a_read_u32(
        machine, fighter0 + UINT32_C(0x000001a4), &r7
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000001a4), &r8
        );
    }
    if (status == VF2_OK) {
        r10 = 0u;
        r15 = UINT32_C(0x15802b2b);
        status = vf2_model2a_write_u32(machine, port, r15);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001f4), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000001f4), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001fc), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000001fc), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r9);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x000005f4), r9
        );
    }
    if (status == VF2_OK) {
        r15 = UINT32_C(0x17802f2f);
        status = vf2_model2a_write_u32(machine, port, r15);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001fc), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000001fc), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000001f4), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001f4), &r15
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, port, r15);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r11);
    }
    high_result = (int32_t)r9 > (int32_t)UINT32_C(0x3ecccccd);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a028), &r3
        );
    }
    if (status == VF2_OK && !high_result &&
        (int32_t)r9 > (int32_t)r3) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK && high_result &&
        (int32_t)r9 > (int32_t)r3) {
        status = hybrid_write_u16(
            machine, fighter0 + UINT32_C(0x000005b6), (uint16_t)r11
        );
    }
    /* 0x18738..0x18768 accumulates relative-position bits; crossing
     * the signed 0x4000 window is ordinary state, not an unsupported path. */
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter0 + UINT32_C(0x000005b4), &short_value
        );
        fighter0_distance_raw = short_value;
        r4 = (uint32_t)(int32_t)(int16_t)short_value;
        r4 = (r11 - r4) + UINT32_C(0x00004000);
        if ((r4 & UINT32_C(0x00008000)) != 0u) {
            r10 |= UINT32_C(1);
            ++relative_position_setbits;
        }
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter1 + UINT32_C(0x000005b4), &short_value
        );
        fighter1_distance_raw = short_value;
        r4 = (uint32_t)(int32_t)(int16_t)short_value;
        r4 = (r11 - r4) + UINT32_C(0x00004000);
        if ((r4 & UINT32_C(0x00008000)) == 0u) {
            r10 |= UINT32_C(2);
            ++relative_position_setbits;
        }
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, UINT32_C(0x0050a0b6), &byte_value
        );
        countdown_path = byte_value != 0u;
    }
    if (status == VF2_OK &&
        (r7 & (UINT32_C(1) << 8u)) != 0u &&
        (r8 & (UINT32_C(1) << 8u)) != 0u &&
        (r7 != (UINT32_C(1) << 8u) ||
         r8 != (UINT32_C(1) << 8u))) {
        const uint32_t state8 = UINT32_C(1) << 8u;
        const uint32_t state8_bit1 = state8 | (UINT32_C(1) << 1u);
        const uint32_t state8_bit4 = state8 | (UINT32_C(1) << 4u);
        const bool bilateral_bit1 =
            (r7 == state8 && r8 == state8_bit1) ||
            (r8 == state8 && r7 == state8_bit1);
        const bool bilateral_bit4 =
            (r7 == state8 && r8 == state8_bit4) ||
            (r8 == state8 && r7 == state8_bit4);
        const bool bilateral_both_bit4 =
            r7 == state8_bit4 && r8 == state8_bit4;
        const bool bilateral_both_bit1 =
            r7 == state8_bit1 && r8 == state8_bit1;
        const bool bilateral_cross_bit1_bit4 =
            (r7 == state8_bit1 && r8 == state8_bit4) ||
            (r7 == state8_bit4 && r8 == state8_bit1);
        const uint32_t state8_bit1_bit4 =
            state8 | (UINT32_C(1) << 1u) | (UINT32_C(1) << 4u);
        const bool bilateral_both_bit1_bit4 =
            r7 == state8_bit1_bit4 && r8 == state8_bit1_bit4;
        const uint32_t state8_bit2_bit4 =
            state8 | (UINT32_C(1) << 2u) | (UINT32_C(1) << 4u);
        const bool bilateral_both_bit2_bit4 =
            r7 == state8_bit2_bit4 && r8 == state8_bit2_bit4;
        const uint32_t state8_bit2 =
            state8 | (UINT32_C(1) << 2u);
        const bool bilateral_both_bit2 =
            r7 == state8_bit2 && r8 == state8_bit2;
        const bool bilateral_asym_bit2 =
            (r7 == state8 && r8 == state8_bit2) ||
            (r8 == state8 && r7 == state8_bit2);
        const bool bilateral_class5_110_112 =
            (r7 == state8_bit4 && r8 == state8_bit1_bit4) ||
            (r8 == state8_bit4 && r7 == state8_bit1_bit4);
        const bool bilateral_class6_102_112 =
            (r7 == state8_bit1 && r8 == state8_bit1_bit4) ||
            (r8 == state8_bit1 && r7 == state8_bit1_bit4);
        if (!bilateral_bit1 && !bilateral_bit4 && !bilateral_both_bit4 &&
            !bilateral_both_bit1 && !bilateral_cross_bit1_bit4 &&
            !bilateral_both_bit1_bit4 && !bilateral_both_bit2_bit4 &&
            !bilateral_both_bit2 && !bilateral_asym_bit2 &&
            !bilateral_class5_110_112 && !bilateral_class6_102_112) {
            /* The measured bilateral bit1/bit4 compositions are admitted;
             * other mixed states remain explicit unsupported boundaries. */
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    /* The nonzero countdown corridor enters the shared 0x18890 tail. */
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050016c), &r13
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, r13 + UINT32_C(0x00003351), &byte_value
        );
    }
    if (status == VF2_OK) {
        /* 0x18898..0x188a8 tests bit 6, not byte != 0.  Values such as
         * 0x01 therefore remain on the ordinary path.  With bit 6 set,
         * the observed neutral corridor performs the extra fighter-flag
         * load/BBS 29 pair and rejoins at 0x188ac. */
        mode_bit6 = (byte_value & (UINT8_C(1) << 6u)) != 0u;
        if (mode_bit6) {
            status = vf2_model2a_read_u32(machine, fighter1, &r15);
            if (status == VF2_OK &&
                (r15 & (UINT32_C(1) << 29u)) != 0u) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK &&
                (r8 & (UINT32_C(1) << 8u)) != 0u) {
                const uint32_t extra_state =
                    r8 & ~(UINT32_C(1) << 8u);
                const bool bilateral_first_order =
                    return_address == UINT32_C(0x000164b0) &&
                    (r7 & (UINT32_C(1) << 8u)) != 0u;
                const bool bilateral_second_order =
                    return_address == UINT32_C(0x000164c4) &&
                    (r7 & (UINT32_C(1) << 8u)) != 0u;
                mode_bit6_supported_bit8 =
                    (r7 == 0u &&
                     (extra_state == 0u ||
                     extra_state == (UINT32_C(1) << 1u) ||
                     (!countdown_path &&
                      extra_state == (UINT32_C(1) << 4u)) ||
                     extra_state == (UINT32_C(1) << 6u) ||
                     extra_state == (UINT32_C(1) << 14u) ||
                     extra_state == (UINT32_C(1) << 15u) ||
                     extra_state == (UINT32_C(1) << 16u) ||
                     extra_state == (UINT32_C(1) << 21u) ||
                     extra_state == (UINT32_C(1) << 26u) ||
                     extra_state == (UINT32_C(1) << 29u) ||
                     extra_state == (UINT32_C(1) << 30u) ||
                     extra_state ==
                         ((UINT32_C(1) << 14u) |
                          (UINT32_C(1) << 15u)) ||
                     extra_state ==
                         ((UINT32_C(1) << 15u) |
                          (UINT32_C(1) << 16u)))) ||
                    bilateral_first_order;
                if (!mode_bit6_supported_bit8 && !bilateral_second_order) {
                    /* The isolated bit-4 fast path is ROM-backed only
                     * with a zero countdown. Two-sided bit 8 and mixed
                     * state combinations remain fail-closed. */
                    status = VF2_ERROR_UNSUPPORTED;
                }
            }
        }
    }
    if (status == VF2_OK && mode_bit6 && countdown_path &&
        r7 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 4u)) &&
        r8 == 0u) {
        /* The swapped isolated state8+bit4 countdown corridor has not
         * been recovered; keep the direct-entry orientation fail-closed. */
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK &&
        (((r7 | r8) & (UINT32_C(1) << 1u)) != 0u) &&
        (((r7 | r8) & (UINT32_C(1) << 8u)) != 0u)) {
        const uint32_t isolated_state8_bit1 =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
        const bool forward_isolated =
            r7 == 0u && r8 == isolated_state8_bit1;
        const bool reverse_isolated =
            r7 == isolated_state8_bit1 && r8 == 0u;
        const bool forward_bilateral =
            r7 == (UINT32_C(1) << 8u) && r8 == isolated_state8_bit1;
        const bool reverse_bilateral =
            r7 == isolated_state8_bit1 && r8 == (UINT32_C(1) << 8u);
        const bool both_bilateral =
            r7 == isolated_state8_bit1 && r8 == isolated_state8_bit1;
        const uint32_t state8_bit4 =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 4u);
        const bool cross_bilateral =
            (r7 == isolated_state8_bit1 && r8 == state8_bit4) ||
            (r7 == state8_bit4 && r8 == isolated_state8_bit1);
        const uint32_t state8_bit1_bit4 =
            isolated_state8_bit1 | (UINT32_C(1) << 4u);
        const bool both_bit1_bit4 =
            r7 == state8_bit1_bit4 && r8 == state8_bit1_bit4;
        const bool class5_110_112 =
            (r7 == state8_bit4 && r8 == state8_bit1_bit4) ||
            (r8 == state8_bit4 && r7 == state8_bit1_bit4);
        const bool class6_102_112 =
            (r7 == isolated_state8_bit1 && r8 == state8_bit1_bit4) ||
            (r8 == isolated_state8_bit1 && r7 == state8_bit1_bit4);
        if (!forward_isolated && !reverse_isolated &&
            !forward_bilateral && !reverse_bilateral && !both_bilateral &&
            !cross_bilateral && !both_bit1_bit4 && !class5_110_112 &&
            !class6_102_112) {
            /* Only the measured isolated and bilateral state8+bit1
             * compositions are admitted here. */
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK &&
        (r8 & ((UINT32_C(1) << 15u) | (UINT32_C(1) << 8u))) == 0u) {
        /* 0x188ac..0x188c8 is ordered control flow, not independent flag
         * tests: bit 15 and then bit 8 skip the SETBIT 11 path before bit 16
         * and bit 14 are considered. For bit 14 + bit 4 the ROM consults
         * fighter0+0x5bc and sets bit 11 only when that halfword is zero. */
        bool set_bit11 = (r8 & (UINT32_C(1) << 16u)) != 0u;
        if (!set_bit11 && (r8 & (UINT32_C(1) << 14u)) != 0u) {
            if ((r8 & (UINT32_C(1) << 4u)) == 0u) {
                set_bit11 = true;
            } else {
                uint16_t guard = 0u;
                status = hybrid_read_u16(
                    machine, fighter0 + UINT32_C(0x000005bc), &guard
                );
                set_bit11 = status == VF2_OK && guard == 0u;
            }
        }
        if (status == VF2_OK && set_bit11) {
            r10 |= UINT32_C(1) << 11u;
        }
    }
    /* 0x188cc..0x18978: the isolated state8+bit1 corridor enters a
     * distance/type sub-tree before the shared tail. Account this relative
     * to the ordinary bit-8 path, whose two BBC instructions are already
     * included by the exact-state formula. */
    if (status == VF2_OK &&
        (r7 == 0u || r7 == (UINT32_C(1) << 8u)) &&
        r8 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u))) {
        uint8_t fighter1_type = 0u;
        status = hybrid_read_u8(
            machine, fighter1 + UINT32_C(0x0000019f), &fighter1_type
        );
        if (status == VF2_OK && fighter1_type == UINT8_C(24)) {
            state8_bit1_instruction_delta = UINT32_C(2);
        } else if (status == VF2_OK) {
            const uint16_t distance0 = (uint16_t)(
                (uint32_t)(int32_t)(int16_t)fighter0_distance_raw - r11
            );
            const bool first_exact =
                distance0 == UINT16_C(0x1554) ||
                distance0 == UINT16_C(0xeaac);

            if (!first_exact) {
                state8_bit1_instruction_delta = UINT32_C(11);
            } else {
                const uint16_t distance1 = (uint16_t)(
                    (uint32_t)(int32_t)(int16_t)fighter1_distance_raw - r11
                );
                const bool second_mirrored_exact =
                    distance1 == UINT16_C(0x6aac) ||
                    distance1 == UINT16_C(0x9554);

                if (!second_mirrored_exact) {
                    state8_bit1_instruction_delta = UINT32_C(20);
                } else {
                    uint32_t state844 = 0u;
                    uint32_t state_mask = UINT32_C(0x03ff8000);

                    state8_bit1_instruction_delta = UINT32_C(30);
                    if ((int32_t)r9 > (int32_t)UINT32_C(0x40000000)) {
                        state_mask &= ~((UINT32_C(1) << 22u) |
                                        (UINT32_C(1) << 23u));
                        state8_bit1_instruction_delta += UINT32_C(2);
                    }
                    if ((int32_t)r9 > (int32_t)UINT32_C(0x3fe66666)) {
                        state_mask &= ~((UINT32_C(1) << 18u) |
                                        (UINT32_C(1) << 21u) |
                                        (UINT32_C(1) << 19u));
                        state8_bit1_instruction_delta += UINT32_C(3);
                    }
                    if ((int32_t)r9 > (int32_t)UINT32_C(0x3fcccccd)) {
                        state_mask &= ~(UINT32_C(1) << 20u);
                        state8_bit1_instruction_delta += UINT32_C(1);
                    }
                    status = vf2_model2a_read_u32(
                        machine, fighter1 + UINT32_C(0x00000844), &state844
                    );
                    if (status == VF2_OK) {
                        r10 |= state844 & state_mask;
                    }
                }
            }
        }
    }
    if (status == VF2_OK) {
        uint16_t progress = 0u;
        uint16_t limit = 0u;
        status = vf2_model2a_read_u32(machine, fighter1 + UINT32_C(0x00000844), &r14);
        r3 = r14 & UINT32_C(0x3c000000);
        if (status == VF2_OK && r3 != 0u) {
            status = hybrid_read_u16(machine, fighter1 + UINT32_C(0x0000084e), &progress);
            if (status == VF2_OK) status = hybrid_read_u16(machine, fighter1 + UINT32_C(0x000001aa), &limit);
            if (status == VF2_OK) {
                tail_instruction_delta += UINT32_C(3);
                if (progress >= limit) { r10 |= r3; ++tail_instruction_delta; }
            }
        }
    }
    if (status == VF2_OK) r10 |= r14 & UINT32_C(0xc0000000);
    if (status == VF2_OK) {
        /* 0x189a8..0x189bc: the first SHLO result is overwritten by SHRO,
         * then CHKBIT 10 controls ALTERBIT 3 and remains the condition code
         * unless the later type-22 helper executes its own CHKBIT. */
        status = vf2_model2a_read_u32(machine, fighter0, &r4);
        if (status == VF2_OK) {
            r3 = (r10 >> 5u) ^ r4;
            const bool bit10_set =
                (r3 & (UINT32_C(1) << 10u)) != 0u;
            hybrid_set_compare_result(
                cpu, bit10_set ? VF2_I960_COMPARE_EQUAL
                               : VF2_I960_COMPARE_NONE
            );
            r10 = bit10_set
                ? r10 | (UINT32_C(1) << 3u)
                : r10 & ~(UINT32_C(1) << 3u);
        }
    }
    /* 0x18770..0x18890: countdown, r7.bit4 and the r8 fast-mask skip
     * the signed-distance tree entirely.  Otherwise the first comparison
     * continues only for exactly +/-0x1554.  The second comparison either
     * selects 0x18858 for the same exact pair, or toggles bit 15 and tests
     * again; that mirrored equality corresponds to +/-0x6aac.  Recover the
     * neutral rejoin-to-0x18890 corridors and keep state-setting subbranches
     * explicit until separately measured. */
    if (status == VF2_OK && !countdown_path &&
        (r7 & (UINT32_C(1) << 4u)) == 0u &&
        (r8 & ((UINT32_C(1) << 4u) | (UINT32_C(1) << 14u) |
               (UINT32_C(1) << 15u) | (UINT32_C(1) << 16u) |
               (UINT32_C(1) << 26u))) == 0u) {
        const uint16_t distance0 = (uint16_t)(
            (uint32_t)(int32_t)(int16_t)fighter0_distance_raw - r11
        );
        const uint16_t distance1 = (uint16_t)(
            (uint32_t)(int32_t)(int16_t)fighter1_distance_raw - r11
        );
        const bool first_exact = distance0 == UINT16_C(0x1554) ||
                                 distance0 == UINT16_C(0xeaac);
        const bool second_exact = distance1 == UINT16_C(0x1554) ||
                                  distance1 == UINT16_C(0xeaac);
        const bool second_mirrored_exact =
            distance1 == UINT16_C(0x6aac) ||
            distance1 == UINT16_C(0x9554);
        const uint32_t branch_mask =
            (UINT32_C(1) << 2u) | (UINT32_C(1) << 9u) |
            (UINT32_C(1) << 10u) | (UINT32_C(1) << 13u) |
            (UINT32_C(1) << 28u);

        if (first_exact) {
            if (second_exact) {
                signed_distance_instruction_delta = UINT32_C(11);
                if ((r8 & branch_mask) != 0u) {
                    status = vf2_model2a_read_u32(
                        machine, fighter0 + UINT32_C(0x00000634), &r3
                    );
                    if (status == VF2_OK) {
                        status = vf2_model2a_read_u32(
                            machine, fighter0 + UINT32_C(0x000005f4), &r4
                        );
                    }
                    if (status == VF2_OK &&
                        hybrid_float_from_bits(r4) > hybrid_float_from_bits(r3)) {
                        signed_distance_instruction_delta += UINT32_C(4);
                    } else if (status == VF2_OK) {
                        status = vf2_model2a_read_u32(machine, fighter1, &r15);
                        if (status == VF2_OK &&
                            (r15 & (UINT32_C(1) << 8u)) != 0u) {
                            r10 |= UINT32_C(1) << 13u;
                            signed_distance_instruction_delta += UINT32_C(7);
                        } else if (status == VF2_OK &&
                                   (r8 & (UINT32_C(1) << 25u)) != 0u) {
                            r10 |= UINT32_C(1) << 13u;
                            signed_distance_instruction_delta += UINT32_C(8);
                        } else if (status == VF2_OK) {
                            r10 |= UINT32_C(1) << 10u;
                            signed_distance_instruction_delta += UINT32_C(9);
                        }
                    }
                }
            } else if (second_mirrored_exact) {
                uint32_t mirror_extra = 0u;
                bool mirror_finished = false;

                signed_distance_instruction_delta = UINT32_C(20);
                if ((r8 & (UINT32_C(1) << 1u)) != 0u) {
                    uint32_t state844 = 0u;
                    status = vf2_model2a_read_u32(
                        machine, fighter1 + UINT32_C(0x00000844), &state844
                    );
                    if (status == VF2_OK &&
                        (state844 & (UINT32_C(1) << 27u)) != 0u) {
                        uint32_t threshold668 = 0u;
                        r10 |= UINT32_C(1) << 8u;
                        status = vf2_model2a_read_u32(
                            machine, fighter0 + UINT32_C(0x00000668),
                            &threshold668
                        );
                        mirror_extra = UINT32_C(3);
                        if (status == VF2_OK &&
                            !(hybrid_float_from_bits(0u) >
                              hybrid_float_from_bits(threshold668))) {
                            r10 |= UINT32_C(1) << 12u;
                            mirror_extra += UINT32_C(2);
                        }
                        mirror_finished = status == VF2_OK;
                    } else if (status == VF2_OK) {
                        mirror_extra = UINT32_C(2);
                    }
                }

                if (status == VF2_OK && !mirror_finished) {
                    if ((r8 & branch_mask) == 0u) {
                        signed_distance_instruction_delta += mirror_extra;
                    } else {
                        uint32_t fighter1_base = 0u;
                        uint32_t fighter0_position = 0u;
                        status = vf2_model2a_read_u32(
                            machine, fighter1, &fighter1_base
                        );
                        if (status == VF2_OK) {
                            status = vf2_model2a_read_u32(
                                machine, fighter0 + UINT32_C(0x000005f4),
                                &fighter0_position
                            );
                        }
                        if (status == VF2_OK &&
                            (((fighter1_base & (UINT32_C(1) << 8u)) != 0u) ||
                             ((r8 & (UINT32_C(1) << 25u)) != 0u))) {
                            uint32_t threshold630 = 0u;
                            status = vf2_model2a_read_u32(
                                machine, fighter0 + UINT32_C(0x00000630),
                                &threshold630
                            );
                            if (status == VF2_OK) {
                                mirror_extra +=
                                    (fighter1_base & (UINT32_C(1) << 8u)) != 0u
                                        ? UINT32_C(6) : UINT32_C(7);
                                if (!(hybrid_float_from_bits(fighter0_position) >
                                  hybrid_float_from_bits(threshold630))) {
                                    r10 |= UINT32_C(1) << 9u;
                                        mirror_extra += UINT32_C(2);
                                }
                            }
                        } else if (status == VF2_OK) {
                            uint32_t threshold62c = 0u;
                            uint32_t threshold668 = 0u;
                          status = vf2_model2a_read_u32(
                              machine, fighter0 + UINT32_C(0x0000062c),
                              &threshold62c
                            );
                            if (status == VF2_OK) {
                                status = vf2_model2a_read_u32(
                                    machine, fighter0 + UINT32_C(0x00000668),
                                    &threshold668
                              );
                          }
                            if (status == VF2_OK) {
                                mirror_extra += UINT32_C(10);
                                if (!(hybrid_float_from_bits(fighter0_position) >
                                  hybrid_float_from_bits(threshold62c))) {
                                    r10 |= UINT32_C(1) << 8u;
                                        ++mirror_extra;
                                }
                                if (!(hybrid_float_from_bits(fighter0_position) >
                                  hybrid_float_from_bits(threshold668))) {
                                    r10 |= UINT32_C(1) << 12u;
                                    mirror_extra += UINT32_C(2);
                                }
                            }
                        }
                        if (status == VF2_OK) {
                            signed_distance_instruction_delta += mirror_extra;
                        }
                    }
                } else if (status == VF2_OK) {
                    signed_distance_instruction_delta += mirror_extra;
                }
            } else {
                /* 0x187e8 branches directly to 0x18890 for every
                 * non-exact, non-mirrored second distance. State bits are
                 * consumed only by the shared tail after that join. */
                signed_distance_instruction_delta = UINT32_C(15);
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000005b8), &r13
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000005b8), &r14
        );
        r3 = r13 | r14;
        shared_bit1_path = (r3 & (UINT32_C(1) << 1u)) != 0u;
        if (shared_bit1_path) {
            r10 |= UINT32_C(4);
        }
        if ((r8 & (UINT32_C(1) << 14u)) != 0u) {
            r10 &= ~(UINT32_C(1) << 2u);
        }

        {
            const uint32_t observed_state_mask =
                (UINT32_C(1) << 4u) | (UINT32_C(1) << 6u) |
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 14u) |
                (UINT32_C(1) << 15u) | (UINT32_C(1) << 16u) |
                (UINT32_C(1) << 21u) | (UINT32_C(1) << 26u) |
                (UINT32_C(1) << 29u) | (UINT32_C(1) << 30u);
            const uint32_t fast_mask =
                (UINT32_C(1) << 4u) | (UINT32_C(1) << 14u) |
                (UINT32_C(1) << 15u) | (UINT32_C(1) << 16u) |
                (UINT32_C(1) << 26u);
            const uint32_t active_state = (r7 | r8) & observed_state_mask;
            const uint32_t state8_bit1 =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
            const bool isolated_state8_bit1_forward =
                r7 == 0u && r8 == state8_bit1;
            const bool isolated_state8_bit1_reverse =
                r7 == state8_bit1 && r8 == 0u;
            const bool bilateral_state8_bit1_forward =
                r7 == (UINT32_C(1) << 8u) && r8 == state8_bit1;
            const bool bilateral_state8_bit1_reverse =
                r7 == state8_bit1 && r8 == (UINT32_C(1) << 8u);
            const bool exact_state_accounting =
                !countdown_path &&
                (!mode_bit6 || mode_bit6_supported_bit8) &&
                active_state != 0u &&
                (isolated_state8_bit1_forward ||
                 isolated_state8_bit1_reverse ||
                 bilateral_state8_bit1_forward ||
                 bilateral_state8_bit1_reverse ||
                 ((r7 | r8) & ~observed_state_mask) == 0u);

            if (exact_state_accounting) {
                uint32_t prefix_count = UINT32_C(43);
                uint32_t early_count = 0u;
                uint32_t tail_count = 0u;
                uint32_t shared_tail_count = UINT32_C(3);

                if (((r7 | r8) & (UINT32_C(1) << 4u)) != 0u) {
                    prefix_count += UINT32_C(4);
                }

                if ((r8 & (UINT32_C(1) << 15u)) != 0u) {
                    shared_tail_count += UINT32_C(1);
                } else if ((r8 & (UINT32_C(1) << 8u)) != 0u) {
                    shared_tail_count += UINT32_C(2);
                } else if ((r8 & (UINT32_C(1) << 16u)) != 0u) {
                    shared_tail_count += UINT32_C(4);
                } else if ((r8 & (UINT32_C(1) << 14u)) != 0u) {
                    shared_tail_count += UINT32_C(4);
                    if ((r8 & (UINT32_C(1) << 4u)) == 0u) {
                        shared_tail_count += UINT32_C(2);
                    } else {
                        uint16_t guard = 0u;
                        status = hybrid_read_u16(
                            machine,
                            fighter0 + UINT32_C(0x000005bc),
                            &guard
                        );
                        if (status == VF2_OK) {
                            shared_tail_count += UINT32_C(3);
                            if (guard == 0u) {
                                ++shared_tail_count;
                            }
                        }
                    }
                } else {
                    shared_tail_count += UINT32_C(4);
                }
                ++shared_tail_count;
                if ((r8 & (UINT32_C(1) << 8u)) != 0u) {
                    ++shared_tail_count;
                }

                if (status == VF2_OK) {
                    if ((r7 & (UINT32_C(1) << 4u)) != 0u) {
                        early_count = UINT32_C(3) + shared_tail_count;
                    } else if ((r8 & fast_mask) != 0u) {
                        early_count = UINT32_C(6) + shared_tail_count;
                    } else {
                        early_count = UINT32_C(23) -
                            (((r8 & (UINT32_C(1) << 8u)) != 0u)
                                ? UINT32_C(1) : UINT32_C(0));
                    }

                    tail_count = (r8 & (UINT32_C(1) << 14u)) != 0u
                        ? UINT32_C(31)
                        : (shared_bit1_path ? UINT32_C(30)
                                            : UINT32_C(35));

                    body_instructions =
                        prefix_count + early_count + tail_count;
                    if (mode_bit6_supported_bit8) {
                        const uint32_t priority_state =
                            r8 & ((UINT32_C(1) << 14u) |
                                  (UINT32_C(1) << 15u) |
                                  (UINT32_C(1) << 16u));
                        const bool compound_priority =
                            priority_state ==
                                ((UINT32_C(1) << 14u) |
                                 (UINT32_C(1) << 15u)) ||
                            priority_state ==
                                ((UINT32_C(1) << 15u) |
                                 (UINT32_C(1) << 16u));
                        if (!compound_priority) {
                            /* Controlled ROM-backed mode-bit-6 probes with
                             * bit 8 plus one supported state flag rejoin two
                             * instructions earlier. The measured 14+15 and
                             * 15+16 priority pairs already match the generic
                             * exact-state formula. */
                            body_instructions -= UINT32_C(2);
                        }
                    }
                }
            } else {
                if (shared_bit1_path) {
                    body_instructions = 96u;
                }
                if ((r8 & (UINT32_C(1) << 14u)) != 0u) {
                    body_instructions = 90u;
                }
                if ((r8 & (UINT32_C(1) << 16u)) != 0u) {
                    body_instructions = shared_bit1_path ? 87u : 92u;
                }
                if ((r8 & (UINT32_C(1) << 15u)) != 0u) {
                    body_instructions = shared_bit1_path ? 84u : 89u;
                }
                if (((r7 | r8) & (UINT32_C(1) << 4u)) != 0u) {
                    body_instructions = 92u;
                }
                if (((r7 | r8) & (UINT32_C(1) << 8u)) != 0u) {
                    body_instructions = 101u;
                }
                if (countdown_path &&
                    (r8 & (UINT32_C(1) << 15u)) == 0u) {
                    body_instructions =
                        shared_bit1_path ? 83u : 88u;
                }
                if (mode_bit6) {
                    body_instructions +=
                        shared_bit1_path ? 2u : 3u;
                }
                if ((r8 & (UINT32_C(1) << 26u)) != 0u &&
                    !countdown_path &&
                    (r7 & (UINT32_C(1) << 4u)) == 0u &&
                    (r8 & ((UINT32_C(1) << 4u) |
                           (UINT32_C(1) << 14u) |
                           (UINT32_C(1) << 15u) |
                           (UINT32_C(1) << 16u))) == 0u) {
                    body_instructions -= UINT32_C(9);
                }
            }
        }
        if (status == VF2_OK) {
            if (!countdown_path && mode_bit6 &&
                r7 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 4u)) &&
                (r8 == 0u ||
                 (return_address == UINT32_C(0x000164c4) &&
                  r8 == (UINT32_C(1) << 8u)))) {
                /* In the swapped fighter order, mode bit 6 plus isolated
                 * state bits 8+4 rejoins eight instructions earlier than
                 * the generic mode-bit-6 fallback accounting. */
                body_instructions -= UINT32_C(8);
            }
            if (!countdown_path && mode_bit6 &&
                return_address == UINT32_C(0x000164c4) &&
                r7 == (UINT32_C(1) << 8u) &&
                r8 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 4u))) {
                body_instructions -= UINT32_C(5);
            }
            if (relative_position_setbits == 0u) {
                --body_instructions;
            } else if (relative_position_setbits == 2u) {
                ++body_instructions;
            }
            body_instructions += signed_distance_instruction_delta;
            body_instructions += state8_bit1_instruction_delta;
            if (r7 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u)) &&
                r8 == ((UINT32_C(1) << 8u) | (UINT32_C(1) << 1u))) {
                /* Both fighters carrying state8+bit1 execute the shared
                 * bit1 corridor on both sides: +9 without countdown and
                 * +11 when countdown takes the earlier shared rejoin. */
                body_instructions += countdown_path ? UINT32_C(11) : UINT32_C(9);
            }
            if (countdown_path &&
                return_address == UINT32_C(0x000164b0) &&
                (r7 & (UINT32_C(1) << 8u)) != 0u &&
                (r8 & (UINT32_C(1) << 8u)) != 0u) {
                /* The measured bilateral countdown corridors rejoin one
                 * instruction earlier in the first fighter order. */
                --body_instructions;
            }
            if (!countdown_path &&
                return_address == UINT32_C(0x000164b0) &&
                (r7 & (UINT32_C(1) << 8u)) != 0u &&
                (r8 & (UINT32_C(1) << 8u)) != 0u) {
                /* Bilateral state bit 8 adds one instruction only in the
                 * first fighter order. */
                ++body_instructions;
            }
            if (!countdown_path && mode_bit6 &&
                return_address == UINT32_C(0x000164c4) &&
                (r7 & (UINT32_C(1) << 8u)) != 0u &&
                (r8 & (UINT32_C(1) << 8u)) != 0u) {
                /* The swapped bilateral mode-bit-6 call rejoins one
                 * instruction earlier than the generic fallback. */
                --body_instructions;
            }
            {
                const uint32_t state8 = UINT32_C(1) << 8u;
                const uint32_t state8_bit4 =
                    state8 | (UINT32_C(1) << 4u);
                const bool asymmetric_bilateral_bit4 =
                    (r7 == state8 && r8 == state8_bit4) ||
                    (r7 == state8_bit4 && r8 == state8);

                if (asymmetric_bilateral_bit4) {
                    if (return_address == UINT32_C(0x000164b0)) {
                        if (!countdown_path && !mode_bit6) {
                            --body_instructions;
                        } else {
                            body_instructions += UINT32_C(4);
                        }
                    } else if (return_address == UINT32_C(0x000164c4)) {
                        if (!countdown_path && mode_bit6) {
                            body_instructions -= UINT32_C(5);
                        } else if (countdown_path) {
                            body_instructions += UINT32_C(3);
                        }
                    }
                }
            }
            {
                const uint32_t state8_bit1 =
                    (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
                const uint32_t state8_bit4 =
                    (UINT32_C(1) << 8u) | (UINT32_C(1) << 4u);
                const bool cross_forward =
                    r7 == state8_bit1 && r8 == state8_bit4;
                const bool cross_reverse =
                    r7 == state8_bit4 && r8 == state8_bit1;

                if (cross_forward) {
                    if (return_address == UINT32_C(0x000164b0)) {
                        if (countdown_path) {
                            body_instructions += UINT32_C(4);
                        } else {
                            body_instructions -= UINT32_C(7);
                        }
                    } else if (return_address == UINT32_C(0x000164c4)) {
                        if (countdown_path) {
                            body_instructions += UINT32_C(3);
                        } else {
                            body_instructions -= mode_bit6
                                ? UINT32_C(10) : UINT32_C(11);
                        }
                    }
                } else if (cross_reverse) {
                    if (return_address == UINT32_C(0x000164b0)) {
                        body_instructions += countdown_path
                            ? UINT32_C(15) : UINT32_C(1);
                    } else if (return_address == UINT32_C(0x000164c4)) {
                        if (countdown_path) {
                            body_instructions += UINT32_C(14);
                        } else {
                            body_instructions -= mode_bit6
                                ? UINT32_C(2) : UINT32_C(3);
                        }
                    }
                }
            }
            {
                const uint32_t state8_bit1_bit4 =
                    (UINT32_C(1) << 8u) |
                    (UINT32_C(1) << 1u) |
                    (UINT32_C(1) << 4u);
                if (r7 == state8_bit1_bit4 &&
                    r8 == state8_bit1_bit4) {
                    if (return_address == UINT32_C(0x000164b0)) {
                        body_instructions += countdown_path
                            ? UINT32_C(19) : UINT32_C(5);
                    } else if (return_address == UINT32_C(0x000164c4)) {
                        if (countdown_path) {
                            body_instructions += UINT32_C(18);
                        } else {
                            body_instructions += mode_bit6
                                ? UINT32_C(2) : UINT32_C(1);
                        }
                    }
                }
            }
        }
    }
    if (status == VF2_OK) {
        const uint32_t state8_bit2_bit4 =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 2u) |
            (UINT32_C(1) << 4u);
        if (r7 == state8_bit2_bit4 && r8 == state8_bit2_bit4) {
            if (return_address == UINT32_C(0x000164b0)) {
                body_instructions = countdown_path
                    ? body_instructions + UINT32_C(8)
                    : body_instructions - UINT32_C(6);
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    body_instructions += UINT32_C(7);
                } else {
                    body_instructions -= mode_bit6
                        ? UINT32_C(9) : UINT32_C(10);
                }
            }
        }
    }
    if (status == VF2_OK) {
        const uint32_t state8 = UINT32_C(1) << 8u;
        const uint32_t state8_bit2 =
            state8 | (UINT32_C(1) << 2u);
        const bool bilateral_bit2_accounting =
            (r7 == state8_bit2 && r8 == state8_bit2) ||
            (r7 == state8 && r8 == state8_bit2) ||
            (r8 == state8 && r7 == state8_bit2);
        if (bilateral_bit2_accounting) {
            if (return_address == UINT32_C(0x000164b0)) {
                if (!countdown_path) {
                    body_instructions -= UINT32_C(2);
                }
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    --body_instructions;
                } else {
                    body_instructions -= mode_bit6
                        ? UINT32_C(5) : UINT32_C(6);
                }
            }
        }
    }
    if (status == VF2_OK) {
        const uint32_t state8_bit4 =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 4u);
        const uint32_t state8_bit1_bit4 =
            state8_bit4 | (UINT32_C(1) << 1u);
        const bool class5_forward =
            r7 == state8_bit4 && r8 == state8_bit1_bit4;
        const bool class5_reverse =
            r8 == state8_bit4 && r7 == state8_bit1_bit4;
        if (class5_forward) {
            if (return_address == UINT32_C(0x000164b0)) {
                body_instructions += countdown_path
                    ? UINT32_C(19) : UINT32_C(5);
            } else if (return_address == UINT32_C(0x000164c4)) {
                body_instructions += countdown_path
                    ? UINT32_C(18)
                    : (mode_bit6 ? UINT32_C(2) : UINT32_C(1));
            }
        } else if (class5_reverse) {
            if (return_address == UINT32_C(0x000164b0)) {
                if (countdown_path) {
                    body_instructions += UINT32_C(8);
                } else {
                    body_instructions -= UINT32_C(6);
                }
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    body_instructions += UINT32_C(7);
                } else {
                    body_instructions -= mode_bit6
                        ? UINT32_C(9) : UINT32_C(10);
                }
            }
        }
    }
    if (status == VF2_OK) {
        const uint32_t state8_bit1 =
            (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
        const uint32_t state8_bit1_bit4 =
            state8_bit1 | (UINT32_C(1) << 4u);
        const bool class6_forward =
            r7 == state8_bit1 && r8 == state8_bit1_bit4;
        const bool class6_reverse =
            r8 == state8_bit1 && r7 == state8_bit1_bit4;
        if (class6_forward) {
            if (return_address == UINT32_C(0x000164b0)) {
                body_instructions += countdown_path
                    ? UINT32_C(15) : UINT32_C(4);
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    body_instructions += UINT32_C(14);
                } else if (mode_bit6) {
                    ++body_instructions;
                }
            }
        } else if (class6_reverse) {
            if (return_address == UINT32_C(0x000164b0)) {
                body_instructions += countdown_path
                    ? UINT32_C(15) : UINT32_C(1);
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    body_instructions += UINT32_C(14);
                } else {
                    body_instructions -= mode_bit6
                        ? UINT32_C(2) : UINT32_C(3);
                }
            }
        }
    }
    if (status == VF2_OK) {
        const uint32_t state8 = UINT32_C(1) << 8u;
        const uint32_t state8_bit1 = state8 | (UINT32_C(1) << 1u);
        const uint32_t state8_bit4 = state8 | (UINT32_C(1) << 4u);
        const bool baseline_or_single_bit1 =
            (r7 == state8 && r8 == state8) ||
            (r7 == state8 && r8 == state8_bit1) ||
            (r8 == state8 && r7 == state8_bit1);

        if (baseline_or_single_bit1) {
            if (return_address == UINT32_C(0x000164b0)) {
                if (!countdown_path) {
                    if (mode_bit6) {
                        body_instructions += UINT32_C(4);
                    } else {
                        --body_instructions;
                    }
                }
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    --body_instructions;
                } else if (mode_bit6) {
                    body_instructions -= UINT32_C(5);
                }
            }
        } else if (r7 == state8_bit1 && r8 == state8_bit1) {
            if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    --body_instructions;
                } else {
                    body_instructions -= mode_bit6
                        ? UINT32_C(3) : UINT32_C(4);
                }
            }
        } else if (r7 == state8_bit4 && r8 == state8_bit4) {
            if (return_address == UINT32_C(0x000164b0)) {
                if (countdown_path || mode_bit6) {
                    body_instructions += UINT32_C(8);
                } else {
                    body_instructions += UINT32_C(3);
                }
            } else if (return_address == UINT32_C(0x000164c4)) {
                if (countdown_path) {
                    body_instructions += UINT32_C(7);
                } else if (mode_bit6) {
                    body_instructions -= UINT32_C(9);
                } else {
                    body_instructions += UINT32_C(4);
                }
            }
        }
    }
    if (status == VF2_OK && !shared_bit1_path) {
        /* 0x189d0 BBS 1 skips this entire threshold block.  Otherwise
         * 0x189ec BBC 6 selects the normal 0x1b7ec threshold; bit 6 set
         * loads the alternate 0x1b7f0 threshold before rejoining. */
        status = vf2_model2a_read_u32(
            machine,
            mode_bit6 ? UINT32_C(0x0001b7f0) : UINT32_C(0x0001b7ec),
            &r3
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, fighter0 + UINT32_C(0x000005f4), &r13
            );
        }
        if (status == VF2_OK && (int32_t)r13 >= (int32_t)r3) {
            const uint32_t state8_bit1 =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u);
            const uint32_t state8_bit4 =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 4u);
            const bool both_bit4 = r7 == state8_bit4 && r8 == state8_bit4;
            const bool cross_bit1_bit4 =
                (r7 == state8_bit1 && r8 == state8_bit4) ||
                (r7 == state8_bit4 && r8 == state8_bit1);
            const uint32_t state8_bit1_bit4 =
                state8_bit1 | (UINT32_C(1) << 4u);
            const bool both_bit1_bit4 =
                r7 == state8_bit1_bit4 && r8 == state8_bit1_bit4;
            const uint32_t state8_bit2_bit4 =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 2u) |
                (UINT32_C(1) << 4u);
            const bool both_bit2_bit4 =
                r7 == state8_bit2_bit4 && r8 == state8_bit2_bit4;
            const uint32_t state8_bit2 =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 2u);
            const bool both_bit2 =
                r7 == state8_bit2 && r8 == state8_bit2;
            const bool asym_bit2 =
                (r7 == (UINT32_C(1) << 8u) && r8 == state8_bit2) ||
                (r8 == (UINT32_C(1) << 8u) && r7 == state8_bit2);
            const bool class5_110_112 =
                (r7 == state8_bit4 && r8 == state8_bit1_bit4) ||
                (r8 == state8_bit4 && r7 == state8_bit1_bit4);
            const bool class6_102_112 =
                (r7 == state8_bit1 && r8 == state8_bit1_bit4) ||
                (r8 == state8_bit1 && r7 == state8_bit1_bit4);
            if (!both_bit4 && !cross_bit1_bit4 && !both_bit1_bit4 &&
                !both_bit2_bit4 && !both_bit2 && !asym_bit2 &&
                !class5_110_112 && !class6_102_112) {
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &r13);
    }
    if (status == VF2_OK) {
        r12 ^= r13;
        r12 <<= 15u;
        r12 ^= r7;
        r12 ^= r8;
        if ((r12 & (UINT32_C(1) << 21u)) != 0u) {
            /* 0x18a1c..0x18a20: BBC 21 falls through to SETBIT 4.
             * Controlled forward/reverse fighter probes show exactly one
             * extra instruction per affected 0x18644 invocation. */
            r10 |= UINT32_C(1) << 4u;
            ++body_instructions;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter0 + UINT32_C(0x000005b8), r10
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter1 + UINT32_C(0x0000019f), &byte_value
        );
        if (status == VF2_OK && byte_value == UINT8_C(22)) {
            uint16_t progress = 0u;
            uint16_t target = 0u;
            status = hybrid_read_u16(
                machine, fighter0 + UINT32_C(0x000001aa), &progress
            );
            if (status == VF2_OK) {
                status = hybrid_read_u16(
                    machine, fighter0 + UINT32_C(0x0000080a), &target
                );
            }
            if (status == VF2_OK &&
                (uint32_t)progress != (uint32_t)target - UINT32_C(1)) {
                /* 0x18a30..0x18a3c: the mismatch path returns immediately. */
                tail_instruction_delta += UINT32_C(4);
            } else if (status == VF2_OK) {
                /* Equal progress executes the r9 threshold pair. */
                tail_instruction_delta += UINT32_C(6);
                if ((int32_t)r9 <= (int32_t)UINT32_C(0x3fb33333)) {
                    uint32_t helper_instructions = 0u;
                    status = hybrid_execute_game_info_type22_equal(
                        machine, cpu, fighter0, fighter1, &helper_instructions
                    );
                    if (status == VF2_OK) {
                        /* Account for the 0x18a4c CALL itself; the helper
                         * count includes 0x18bd4, 0x1ab34 and 0x18b58. */
                        tail_instruction_delta += UINT32_C(1) +
                            helper_instructions;
                    }
                }
            }
        }
    }
    if (status == VF2_OK) {
        cpu->ip = UINT32_C(0x00018a50);
        cpu->executed_instructions += body_instructions + tail_instruction_delta;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    return status;
}

/* Translate the observed 0x181c0 -> 0x184ec conditional corridor. Controlled
 * probes now cover the g0 == 0, 1, 2 and 3 directions; the small
 * 0x18e08/0x18e00 helpers are translated through their shared command-port
 * body; later fighter branches remain explicit ROM subroutine boundaries. */
static vf2_status hybrid_execute_game_info_18144_conditional_body(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t fighter,
    uint32_t flags,
    uint32_t *body_instructions
)
{
    const uint32_t port = cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                          cpu->registers[VF2_I960_G0_REGISTER + 12u];
    uint32_t r3 = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r6 = 0u;
    uint32_t r7 = 0u;
    uint32_t r8 = 0u;
    uint32_t r9 = 0u;
    uint32_t r10 = 0u;
    uint32_t r11 = 0u;
    uint32_t r12 = 0u;
    uint32_t r13 = 0u;
    uint32_t r14 = 0u;
    uint32_t r15 = 0u;
    uint32_t g0 = 0u;
    uint32_t g1 = 0u;
    uint32_t g2 = 0u;
    uint32_t g3 = 0u;
    uint32_t global = 0u;
    uint8_t state = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || body_instructions == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *body_instructions = 0u;

    /* 0x181c0..0x181f0: command-port setup. */
    status = vf2_model2a_write_u32(machine, port, UINT32_C(0x00800101));
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x1b003636));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter + UINT32_C(4), &state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, (uint32_t)state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(12));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x06800d0d));
    }

    /* 0x18204: fa helper with g0=0x1b79c, g1=0x50e000. */
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x0001b79c);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x0050e000);
        cpu->ip = UINT32_C(0x00018204);
        status = hybrid_execute_game_info_18e_helper(
            machine, cpu, UINT32_C(0x00018e08), UINT32_C(0x00018208)
        );
        g0 = cpu->registers[VF2_I960_G0_REGISTER];
        r11 = cpu->registers[11];
    }

    /* 0x1820c..0x18230 and 0x1823c: first 0x18e00 call. */
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x1b003636));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter + UINT32_C(4), &state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, (uint32_t)state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0xb4));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x06800d0d));
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x0001b7b4);
        cpu->ip = UINT32_C(0x0001823c);
        status = hybrid_execute_game_info_18e_helper(
            machine, cpu, UINT32_C(0x00018e00), UINT32_C(0x00018240)
        );
        g2 = cpu->registers[VF2_I960_G0_REGISTER];
    }

    /* 0x18244..0x18274: second 0x18e00 call. */
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x1b003636));
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter + UINT32_C(4), &state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, (uint32_t)state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x90));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x06800d0d));
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x0001b7b4);
        cpu->ip = UINT32_C(0x00018274);
        status = hybrid_execute_game_info_18e_helper(
            machine, cpu, UINT32_C(0x00018e00), UINT32_C(0x00018278)
        );
        g3 = cpu->registers[VF2_I960_G0_REGISTER];
    }

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x01000202));
    }

    /* 0x18288..0x182c4: bit-14 updates the state flags before the triple
     * loads. The controlled bit-14 probe takes this path; bit-16/bit-6 skip
     * it and arrive directly at 0x182c8. */
    if (status == VF2_OK && (flags & (UINT32_C(1) << 14u)) != 0u) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050e004), &r4
        );
        r5 = UINT32_C(0x3ecccccd);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, fighter + UINT32_C(0x804), &r15
            );
        }
        if (status == VF2_OK) {
            if ((r15 & (UINT32_C(1) << 11u)) == 0u) {
                r5 |= UINT32_C(0x80000000);
            }
            if (hybrid_float_from_bits(r4) <= hybrid_float_from_bits(r5)) {
                flags |= UINT32_C(1) << 11u;
            } else {
                flags &= ~(UINT32_C(1) << 11u);
            }
            r3 = flags;
            status = vf2_model2a_write_u32(
                machine, fighter + UINT32_C(0x1a4), flags
            );
        }
    }

    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, fighter + UINT32_C(0x2a8), &r4, &r5, &r6
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, fighter + UINT32_C(0x284), &r8, &r9, &r10
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, fighter + UINT32_C(0x1f4), &r12, &r13, &r14
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, UINT32_C(0x15802b2b));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r4);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r6);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r10);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r7);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x66c), r7
        );
    }

    r3 = 0u;
    status = status == VF2_OK
        ? vf2_model2a_read_u32(machine, UINT32_C(0x0050a0c4), &global)
        : status;
    if (status == VF2_OK) {
        r3 = (r5 - r13) & ~UINT32_C(0x80000000);
        if ((int32_t)r3 < (int32_t)global) {
            g0 |= UINT32_C(1);
        }
        r3 = (r9 - r13) & ~UINT32_C(0x80000000);
        if ((int32_t)r3 < (int32_t)global) {
            g0 |= UINT32_C(2);
        }
    }

    if (status == VF2_OK && g0 == 0u) {
        /* 0x18344 -> 0x1843c: controlled zero-result direction. The
         * selected probe takes 0x18368, writes state 1, and forms the g2/g3
         * delta before rejoining at 0x184ec. */
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a0cc), &r15
        );
        r3 = (r9 - r5) & ~UINT32_C(0x80000000);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, fighter + UINT32_C(0x00000670), r3
            );
        }
        if (status == VF2_OK && (int32_t)r3 >= (int32_t)r15) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x0050a0c0), &r15
            );
            if (status == VF2_OK && (int32_t)r7 > (int32_t)r15) {
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
        if (status == VF2_OK) {
            state = UINT8_C(1);
            status = vf2_model2a_write(
                machine, fighter + UINT32_C(0x0000060d),
                &state, sizeof(state)
            );
        }
        if (status == VF2_OK) {
            r3 = (g2 - g3) << 16u;
            r3 >>= 17u;
            r11 = g2 + r3;
            r11 += r7;
            status = hybrid_write_u16(
                machine, fighter + UINT32_C(0x00000616), (uint16_t)r11
            );
        }
        cpu->registers[3] = r3;
        cpu->registers[4] = r4;
        cpu->registers[5] = r5;
        cpu->registers[6] = r6;
        cpu->registers[7] = r7;
        cpu->registers[8] = r8;
        cpu->registers[9] = r9;
        cpu->registers[10] = r10;
        cpu->registers[11] = r11;
        cpu->registers[12] = r12;
        cpu->registers[13] = r13;
        cpu->registers[14] = r14;
        cpu->registers[15] = r15;
        cpu->registers[VF2_I960_G0_REGISTER] = g0;
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = g1;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = g2;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = g3;
        *body_instructions = UINT32_C(76);
        return status;
    }
    if (status == VF2_OK && g0 != UINT32_C(1) &&
        g0 != UINT32_C(2) && g0 != UINT32_C(3)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* 0x18338..0x1848c: g0==1 selects the command sequence directly;
     * g0==3 first combines and halves the two command vectors. */
    if (g0 == UINT32_C(3)) {
        r8 += r4;
        r10 += r6;
        r13 = hybrid_float_to_bits(
            hybrid_float_from_bits(r8) * 0.5f
        );
        r10 = hybrid_float_to_bits(
            hybrid_float_from_bits(r10) * 0.5f
        );
    }
    r15 = UINT32_C(0x17802f2f);
    status = vf2_model2a_write_u32(machine, port, r15);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r14);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r10);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r8);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, port, r12);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, port, &r7);
    }
    if (status == VF2_OK) {
        state = UINT8_C(2);
        status = vf2_model2a_write(
            machine, fighter + UINT32_C(0x60d), &state, sizeof(state)
        );
    }

    /* 0x18498..0x184e8: derive the command-state bit and update bit 11 for
     * the bit-14/bit-16 entry states. */
    r11 = 0u;
    status = status == VF2_OK
        ? vf2_model2a_read_u32(machine, UINT32_C(0x0050e004), &r3)
        : status;
    if (status == VF2_OK) {
        r11 = (r3 & UINT32_C(0x80000000)) != 0u
            ? UINT32_C(0x8000) : 0u;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u32_triple(
            machine, UINT32_C(0x0050e00c), &r4, &r5, &r6
        );
    }
    if (status == VF2_OK) {
        r8 = hybrid_float_to_bits(
            hybrid_float_from_bits(r12) - hybrid_float_from_bits(r8)
        );
        r4 = hybrid_float_to_bits(
            hybrid_float_from_bits(r4) * hybrid_float_from_bits(r8)
        );
        r10 = hybrid_float_to_bits(
            hybrid_float_from_bits(r14) - hybrid_float_from_bits(r10)
        );
        r6 = hybrid_float_to_bits(
            hybrid_float_from_bits(r6) * hybrid_float_from_bits(r10)
        );
        r4 += r6;
        if ((r4 & UINT32_C(0x80000000)) != 0u) {
            r11 ^= UINT32_C(0x8000);
        }
    }
    /* The controlled 0x181c0 probes all take 0x184cc's notbit path; the
     * command-state bit is therefore clear when 0x184dc checks it. */
    if (status == VF2_OK) {
        r11 = 0u;
    }
    if (status == VF2_OK &&
        (flags & ((UINT32_C(1) << 14u) |
                  (UINT32_C(1) << 16u))) != 0u) {
        flags = (flags & ~(UINT32_C(1) << 11u)) |
                ((r11 & UINT32_C(0x8000)) != 0u
                    ? (UINT32_C(1) << 11u) : 0u);
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x1a4), flags
        );
    }
    if (status == VF2_OK) {
        r11 += r7;
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x616), (uint16_t)r11
        );
    }

    cpu->registers[3] = r3;
    cpu->registers[4] = r4;
    cpu->registers[5] = r5;
    cpu->registers[6] = r6;
    cpu->registers[7] = r7;
    cpu->registers[8] = r8;
    cpu->registers[9] = r9;
    cpu->registers[10] = r10;
    cpu->registers[11] = r11;
    cpu->registers[12] = r12;
    cpu->registers[13] = r13;
    cpu->registers[14] = r14;
    cpu->registers[15] = r15;
    cpu->registers[VF2_I960_G0_REGISTER] = g0;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = g1;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = g2;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = g3;
    if (g0 == UINT32_C(1)) {
        *body_instructions = UINT32_C(96);
    } else if (g0 == UINT32_C(2)) {
        *body_instructions = UINT32_C(94);
    } else if ((flags & (UINT32_C(1) << 14u)) != 0u) {
        *body_instructions = UINT32_C(97);
    } else if ((flags & (UINT32_C(1) << 16u)) != 0u) {
        *body_instructions = UINT32_C(100);
    } else {
        *body_instructions = UINT32_C(90);
    }
    return status;
}

/* Recover the observed 0x18144 prefixes. The ordinary entry takes
 * 0x18144 -> 0x18188 -> 0x184ec and scans sixteen fighter records before the
 * nested 0x18538 -> 0x17b68 helper call. Controlled bit-14/16/6 entries use
 * the native conditional body below; the observed state-4/bit-15 and
 * non-state-4 bit-15 entries now share the native prefix, while unobserved
 * flag combinations remain bounded ROM corridors. */
static vf2_status hybrid_execute_game_info_18144_prefix(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    const uint32_t fighter = cpu->registers[VF2_I960_G0_REGISTER + 7u];
    const uint32_t nested_target = UINT32_C(0x00017b68);
    const uint32_t nested_return = UINT32_C(0x0001853c);
    uint32_t prefix_instructions = UINT32_C(118);
    uint8_t state_byte = 0u;
    uint16_t short_value = 0u;
    uint32_t flags = 0u;
    uint32_t r3 = 0u;
    uint32_t r4 = 0u;
    uint32_t r5 = 0u;
    uint32_t r6 = 0u;
    uint32_t r11 = 0u;
    uint32_t r15_value = 0u;
    uint32_t table = 0u;
    uint8_t zero_state = 0u;
    bool state4_fast_path = false;
    bool state4_bit15_path = false;
    uint32_t state4_prefix_instructions = 0u;
    uint32_t conditional_body_instructions = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != return_address ||
        (return_address != UINT32_C(0x0001647c) &&
         return_address != UINT32_C(0x00016494))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00018144), return_address
    );
    if (status != VF2_OK) {
        return status;
    }

    status = hybrid_read_u8(machine, fighter + UINT32_C(0x00000a00),
                            &state_byte);
    if (status == VF2_OK) {
        cpu->registers[14] = state_byte;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001a4), &flags
        );
    }
    if (status == VF2_OK) {
        cpu->registers[5] = flags;
        state4_fast_path = state_byte == 4u &&
                           (flags & (UINT32_C(1) << 15u)) == 0u;
        state4_bit15_path = state_byte == 4u &&
                            (flags & (UINT32_C(1) << 15u)) != 0u;
        if (!state4_bit15_path &&
            (flags & (UINT32_C(1) << 15u)) != 0u) {
            /* The non-state-4 bit-15 entry shares one instruction with the
             * compact native prefix accounting at the 0x10dcc boundary. */
            prefix_instructions = UINT32_C(117);
        }
        if (state4_fast_path) {
            status = vf2_model2a_write(
                machine, fighter + UINT32_C(0x00000a00),
                &zero_state, sizeof(zero_state)
            );
            state4_prefix_instructions = UINT32_C(3);
        } else if (state4_bit15_path) {
            /* 0x1814c..0x18184: state 4 with bit 15 set clears the state,
             * clears bit 15 in the state flags and refreshes the countdown
             * byte before rejoining the common 0x18188 path. */
            status = vf2_model2a_write(
                machine, fighter + UINT32_C(0x00000a00),
                &zero_state, sizeof(zero_state)
            );
            flags &= ~(UINT32_C(1) << 15u);
            if (status == VF2_OK) {
                status = hybrid_read_u8(
                    machine, UINT32_C(0x0050a0ba), &state_byte
                );
            }
            if (status == VF2_OK) {
                uint32_t control = 0u;
                uint8_t countdown = state_byte;

                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500068), &control
                );
                if (status == VF2_OK &&
                    (control & (UINT32_C(1) << 20u)) != 0u) {
                    countdown = (uint8_t)(countdown << 1u);
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x0050a0b6),
                        &countdown, sizeof(countdown)
                    );
                }
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, fighter + UINT32_C(0x000001a4), flags
                );
                cpu->registers[5] = flags;
            }
            /* The native accounting for this compact replacement is four
             * instructions below the literal ROM prelude at this boundary. */
            state4_prefix_instructions = UINT32_C(10);
        }
    }

    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x00000614), &short_value
        );
        r3 = short_value;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x00000624), &short_value
        );
        r4 = short_value;
    }
    if (status == VF2_OK) {
        r5 = r3 | r4;
        cpu->registers[5] = r5;
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x0000061c), &short_value
        );
        r6 = short_value;
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x0000061c), (uint16_t)r5
        );
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x0000060e), (uint16_t)r6
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x00000618), &short_value
        );
        r4 = short_value;
        r3 = r5 & ~r4;
        cpu->registers[3] = r3;
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x00000628), (uint16_t)r3
        );
    }
    if (status == VF2_OK &&
        (flags & ((UINT32_C(1) << 14u) |
                  (UINT32_C(1) << 16u) |
                  (UINT32_C(1) << 6u))) != 0u) {
        if ((flags & ((UINT32_C(1) << 14u) |
                      (UINT32_C(1) << 16u))) == 0u) {
            status = hybrid_read_u16(
                machine, fighter + UINT32_C(0x00000026), &short_value
            );
            r11 = (uint32_t)(int32_t)(int16_t)short_value;
            if (status == VF2_OK) {
                status = hybrid_write_u16(
                    machine, fighter + UINT32_C(0x00000616),
                    (uint16_t)r11
                );
            }
        }
        if (status == VF2_OK) {
            status = hybrid_execute_game_info_18144_conditional_body(
                machine, cpu, fighter, flags,
                &conditional_body_instructions
            );
        }
        if (status == VF2_OK) {
            cpu->ip = UINT32_C(0x000184ec);
        }
        goto hybrid_game_info_18144_post_184ec;
    }
    if (status == VF2_OK) {
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x00000026), &short_value
        );
        r11 = (uint32_t)(int32_t)(int16_t)short_value;
        cpu->registers[11] = r11;
    }
    if (status == VF2_OK) {
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x00000616), (uint16_t)r11
        );
    }
hybrid_game_info_18144_post_184ec:
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter, &r4);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001a4), &r3
        );
    }
    if (status == VF2_OK) {
        r3 = (r3 >> 15u) ^ r4;
        hybrid_set_compare_result(
            cpu,
            (r3 & (UINT32_C(1) << 6u)) != 0u
                ? VF2_I960_COMPARE_EQUAL
                : VF2_I960_COMPARE_NONE
        );
        r4 = (cpu->arithmetic_control & UINT32_C(2)) != 0u
            ? r4 | (UINT32_C(1) << 10u)
            : r4 & ~(UINT32_C(1) << 10u);
        cpu->registers[3] = r3;
        cpu->registers[4] = r4;
        status = vf2_model2a_write_u32(machine, fighter, r4);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x000001f8), &table
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table, &r4);
    }
    r6 = 15u;
    r5 = table;
    while (status == VF2_OK && r6 != 0u) {
        float left = hybrid_float_from_bits(r4);
        float right = 0.0f;

        r5 += UINT32_C(12);
        status = vf2_model2a_read_u32(machine, r5, &r3);
        if (status != VF2_OK) {
            break;
        }
        right = hybrid_float_from_bits(r3);
        if (left < right) {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_LESS);
        } else if (left > right) {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_GREATER);
            r4 = r3;
        } else {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        }
        --r6;
        if (r6 == 0u) {
            hybrid_set_compare_result(cpu, VF2_I960_COMPARE_EQUAL);
        } else {
            hybrid_set_compare_result(
                cpu,
                1u < r6 ? VF2_I960_COMPARE_LESS :
                           (1u == r6 ? VF2_I960_COMPARE_EQUAL :
                                       VF2_I960_COMPARE_GREATER)
            );
        }
    }
    if (status == VF2_OK) {
        cpu->registers[4] = r4;
        cpu->registers[5] = r5;
        cpu->registers[6] = r6;
        status = hybrid_write_u16(
            machine, fighter + UINT32_C(0x00000674), (uint16_t)r4
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->ip = UINT32_C(0x00018538);
    status = vf2_i960_cpu_enter_procedure(
        cpu, nested_target, nested_return
    );
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    status = vf2_model2a_read_u32(machine, fighter, &r15_value);
    if (status == VF2_OK) {
        cpu->registers[15] = r15_value;
        if ((r15_value & (UINT32_C(1) << 7u)) != 0u) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        /* 0x17b68: ld, bbc, ret. */
        cpu->ip = UINT32_C(0x000180b8);
        cpu->executed_instructions += UINT64_C(2);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    if (status == VF2_OK) {
        /* 0x1853c: ldis 0x5b4(g7), g0. */
        status = hybrid_read_u16(
            machine, fighter + UINT32_C(0x000005b4), &short_value
        );
        if (status == VF2_OK) {
            cpu->registers[VF2_I960_G0_REGISTER] =
                (uint32_t)(int32_t)(int16_t)short_value;
            ++cpu->executed_instructions;
        }
    }
    if (status == VF2_OK) {
        cpu->ip = UINT32_C(0x00018544);
        status = hybrid_execute_game_info_18d44(
            machine, cpu, UINT32_C(0x00018544)
        );
    }
    if (status == VF2_OK) {
        /* 0x18544: st g1, 0x5f8(g7). */
        status = vf2_model2a_write_u32(
            machine, fighter + UINT32_C(0x000005f8),
            cpu->registers[VF2_I960_G0_REGISTER + 1u]
        );
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
    }
    if (status == VF2_OK) {
        /* 0x18548: notbit 15, g0, g0. */
        cpu->registers[VF2_I960_G0_REGISTER] ^= UINT32_C(1) << 15u;
        ++cpu->executed_instructions;
    }
    if (status == VF2_OK) {
        cpu->ip = UINT32_C(0x00018550);
        status = hybrid_execute_game_info_18d44(
            machine, cpu, UINT32_C(0x00018550)
        );
    }
    if (status == VF2_OK) {
        status = hybrid_execute_game_info_18144_suffix(machine, cpu);
    }
    if (status == VF2_OK) {
        /* The dispatcher CALL at 0x16478 was replaced by this native entry. */
        cpu->executed_instructions += prefix_instructions + 1u +
            state4_prefix_instructions +
            conditional_body_instructions;
    }
    return status;
}

static vf2_status hybrid_execute_game_info_bit31_native(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_task_report *report
)
{
    const uint32_t entry_address = UINT32_C(0x0001645c);
    const uint32_t first_child = UINT32_C(0x00018144);
    const uint32_t second_child = UINT32_C(0x00018644);
    const uint32_t first_return = UINT32_C(0x0001647c);
    const uint32_t second_return = UINT32_C(0x00016494);
    const uint32_t third_return = UINT32_C(0x000164b0);
    const uint32_t task_return = UINT32_C(0x00010dcc);
    const uint32_t fighter0_slot = UINT32_C(0x00500804);
    const uint32_t fighter1_slot = UINT32_C(0x00500808);
    const uint32_t runtime_flags_address = UINT32_C(0x00508000);
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    uint32_t fighter0_state_flags = 0u;
    uint32_t fighter1_state_flags = 0u;
    uint8_t fighter0_state = 0u;
    uint8_t fighter1_state = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t shared_fighter_threshold = 0u;
    uint32_t combined_flags = 0u;
    uint8_t countdown = 0u;
    uint8_t zero = 0u;
    const uint32_t conditional_state_mask =
        (UINT32_C(1) << 15u) | (UINT32_C(1) << 14u) |
        (UINT32_C(1) << 16u) | (UINT32_C(1) << 6u);
    bool conditional_fighter_path = false;
    bool native_bit14_fighter_path = false;
    bool native_bit16_fighter_path = false;
    bool native_bit15_fighter_path = false;
    bool native_bit6_fighter_path = false;
    bool native_state4_bit15_fighter_path = false;
    bool native_18644_fighter_path = false;
    uint64_t native_instructions = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != entry_address || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, fighter0_slot, &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1_slot, &fighter1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter0 + UINT32_C(0x000001a4), &fighter0_state_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter1 + UINT32_C(0x000001a4), &fighter1_state_flags
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter0 + UINT32_C(0x00000a00), &fighter0_state
        );
    }
    if (status == VF2_OK) {
        status = hybrid_read_u8(
            machine, fighter1 + UINT32_C(0x00000a00), &fighter1_state
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, runtime_flags_address, &runtime_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a028), &shared_fighter_threshold
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    conditional_fighter_path =
        fighter0_state == 4u || fighter1_state == 4u ||
        ((fighter0_state_flags | fighter1_state_flags) &
         conditional_state_mask) != 0u;
    native_bit14_fighter_path =
        fighter0_state != 4u && fighter1_state != 4u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 14u)) != 0u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         ((UINT32_C(1) << 15u) | (UINT32_C(1) << 16u) |
          (UINT32_C(1) << 6u))) == 0u &&
        (int32_t)shared_fighter_threshold >= 0;
    native_bit16_fighter_path =
        fighter0_state != 4u && fighter1_state != 4u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 16u)) != 0u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         ((UINT32_C(1) << 15u) | (UINT32_C(1) << 14u) |
         (UINT32_C(1) << 6u))) == 0u &&
        (int32_t)shared_fighter_threshold >= 0;
    native_bit15_fighter_path =
        fighter0_state != 4u && fighter1_state != 4u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 15u)) != 0u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         ((UINT32_C(1) << 14u) | (UINT32_C(1) << 16u) |
         (UINT32_C(1) << 6u))) == 0u &&
        (int32_t)shared_fighter_threshold >= 0;
    native_bit6_fighter_path =
        fighter0_state != 4u && fighter1_state != 4u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 6u)) != 0u &&
        ((fighter0_state_flags | fighter1_state_flags) &
         ((UINT32_C(1) << 14u) | (UINT32_C(1) << 15u) |
          (UINT32_C(1) << 16u))) == 0u &&
        (int32_t)shared_fighter_threshold >= 0;
    native_state4_bit15_fighter_path =
        (fighter0_state == 4u || fighter1_state == 4u) &&
        ((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 15u)) != 0u;
    native_18644_fighter_path =
        native_bit14_fighter_path || native_bit16_fighter_path ||
        native_bit15_fighter_path || native_bit6_fighter_path;
    /* 0x1645c..0x16470: four loads; the register values are observable at
     * each child boundary, so preserve the ROM aliases explicitly. */
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    native_instructions += UINT64_C(4);
    cpu->registers[7] = fighter0_flags;
    cpu->registers[8] = fighter1_flags;

    if ((fighter0_flags & UINT32_C(0x80000000)) != 0u) {
        cpu->ip = first_return;
        status = hybrid_execute_game_info_child(
            machine, cpu, first_child, first_return
        );
        native_instructions += UINT64_C(1); /* bbc */
        if (status != VF2_OK) {
            return status;
        }
    } else {
        native_instructions += UINT64_C(1); /* bbc */
    }

    /* The second pointer pair is loaded in the opposite order by the ROM. */
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter1;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter0;
    native_instructions += UINT64_C(2);
    native_instructions += UINT64_C(1); /* second bit-31 test */
    if ((fighter1_flags & UINT32_C(0x80000000)) != 0u) {
        cpu->ip = second_return;
        status = hybrid_execute_game_info_child(
            machine, cpu, first_child, second_return
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    combined_flags = fighter0_flags & fighter1_flags;
    cpu->registers[3] = combined_flags;
    native_instructions += UINT64_C(2); /* and + bbc */
    if (status == VF2_OK && native_state4_bit15_fighter_path) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
        );
        if (status == VF2_OK) {
            native_state4_bit15_fighter_path = countdown != 0u;
            native_18644_fighter_path =
                native_18644_fighter_path ||
                native_state4_bit15_fighter_path;
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((combined_flags & UINT32_C(0x80000000)) != 0u) {
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
        native_instructions += UINT64_C(2);
        cpu->ip = third_return;
        status = native_18644_fighter_path
            ? hybrid_execute_game_info_child(
                machine, cpu, second_child, third_return
            )
            : conditional_fighter_path
                ? hybrid_execute_game_info_child_rom(
                    machine, cpu, second_child, third_return
                )
                : hybrid_execute_game_info_child(
                    machine, cpu, second_child, third_return
                );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = fighter1;
        cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter0;
        native_instructions += UINT64_C(2);
        cpu->ip = UINT32_C(0x000164c4);
        status = native_18644_fighter_path
            ? hybrid_execute_game_info_child(
                machine, cpu, second_child, UINT32_C(0x000164c4)
            )
            : conditional_fighter_path
                ? hybrid_execute_game_info_child_rom(
                    machine, cpu, second_child, UINT32_C(0x000164c4)
                )
                : hybrid_execute_game_info_child(
                    machine, cpu, second_child, UINT32_C(0x000164c4)
                );
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[15] = runtime_flags;
    native_instructions += UINT64_C(2); /* runtime load + bit-5 branch */
    if ((runtime_flags & (UINT32_C(1) << 5u)) == 0u) {
        status = vf2_model2a_write(
            machine, fighter1 + UINT32_C(0x1200), &zero, sizeof(zero)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, fighter0 + UINT32_C(0x1200), &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
            );
        }
        if (status == VF2_OK) {
            native_instructions += UINT64_C(6); /* mov/stib, mov/stib, ldob/cmpobe */
            /* 0x164f0 CMPobe 0,r3 leaves its comparison observable at the
             * task RET: zero is EQUAL; every nonzero uint8 countdown is LESS
             * when comparing literal 0 against r3. */
            hybrid_set_compare_result(
                cpu, countdown == 0u ? VF2_I960_COMPARE_EQUAL
                                     : VF2_I960_COMPARE_LESS
            );
            if (countdown != 0u) {
                --countdown;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
                );
                native_instructions += UINT64_C(2); /* subo/stob */
            }
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    if (cpu->ip != UINT32_C(0x000164c4) &&
        cpu->ip != UINT32_C(0x000164c8) &&
        cpu->ip != UINT32_C(0x000164cc)) {
        /* The child helper returns to 0x164c4 only on the shared-fighter
         * branch; otherwise the dispatcher is already at the runtime load. */
        cpu->ip = UINT32_C(0x000164c4);
    }
    if (conditional_fighter_path) {
        /* The conditional ROM prefix reaches the dispatcher RET at 0x16500;
         * the observed native corridor returns directly from 0x164c4. */
        native_instructions += UINT64_C(1);
    }
    if (native_state4_bit15_fighter_path) {
        native_instructions += UINT64_C(1);
    }
    if (!native_bit15_fighter_path &&
        !native_state4_bit15_fighter_path &&
        ((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 15u)) != 0u) {
        /* When bit 15 is combined with bit 6, 14 or 16, the isolated native
         * bit-15 corridor is not selected and the ROM-backed dispatcher path
         * executes one additional branch before the common RET. */
        native_instructions += UINT64_C(1);
    }
    if (((fighter0_state_flags | fighter1_state_flags) &
         (UINT32_C(1) << 8u)) != 0u) {
        native_instructions += UINT64_C(1);
    }
    native_instructions += UINT64_C(1); /* task RET */
    cpu->executed_instructions += native_instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != task_return) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    memset(report, 0, sizeof(*report));
    report->entry_point = entry_address;
    report->continuation = task_return;
    return VF2_OK;
}

static vf2_status hybrid_camera_fast_gate_supported(
    const vf2_model2a *machine
)
{
    uint8_t input_index = 0u;
    uint8_t control_flags = 0u;
    uint16_t input_flags = 0u;
    vf2_status status = VF2_OK;

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &input_index, sizeof(input_index)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x0006eea0) + ((uint32_t)input_index << 8u),
            &input_flags, sizeof(input_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050009c),
            &control_flags, sizeof(control_flags)
        );
    }
    if (status == VF2_OK &&
        (((input_flags & (UINT16_C(1) << 3u)) != 0u) ||
         ((control_flags & UINT8_C(1)) == 0u))) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    return status;
}

static vf2_status hybrid_camera_apply_memory(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t instruction_pointer,
    vf2_hybrid_block_report *report
)
{
    vf2_hybrid_block_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = instruction_pointer;
    local_report.registry_address = registry_address;

    switch (instruction_pointer) {
    case VF2_CAMERA_INITIALIZE_ENTRY: {
        vf2_recovered_camera_init_report recovered;
        memset(&recovered, 0, sizeof(recovered));
        status = vf2_recovered_task_camera_initialize(
            machine, registry_address, &recovered
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_HYBRID_BLOCK_CAMERA_INITIALIZE;
            local_report.exit_address = recovered.continuation;
            local_report.task_bytes_written = recovered.task_bytes_written;
            local_report.global_bytes_written = recovered.global_bytes_written;
            local_report.recovered_instruction_count = UINT64_C(2586);
            local_report.recovered_procedure_calls = UINT64_C(2);
            local_report.recovered_procedure_returns = UINT64_C(2);
        }
        break;
    }
    case VF2_CAMERA_UPDATE_ENTRY: {
        vf2_recovered_camera_update_report recovered;
        memset(&recovered, 0, sizeof(recovered));
        status = vf2_recovered_task_camera_first_update(
            machine, registry_address, &recovered
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_HYBRID_BLOCK_CAMERA_UPDATE;
            local_report.exit_address = recovered.stop_address;
            local_report.task_bytes_written = recovered.task_bytes_written;
            local_report.global_bytes_written = recovered.global_bytes_written;
            if (recovered.helpers_recovered == 7u) {
                local_report.recovered_instruction_count = UINT64_C(621);
                local_report.recovered_procedure_calls = UINT64_C(7);
                local_report.recovered_procedure_returns = UINT64_C(7);
            } else if (recovered.helpers_recovered == 6u) {
                local_report.recovered_instruction_count = UINT64_C(165);
                local_report.recovered_procedure_calls = UINT64_C(6);
                local_report.recovered_procedure_returns = UINT64_C(6);
            } else if (recovered.helpers_recovered == 4u) {
                local_report.recovered_instruction_count = UINT64_C(107);
                local_report.recovered_procedure_calls = UINT64_C(4);
                local_report.recovered_procedure_returns = UINT64_C(4);
            } else {
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
        break;
    }
    case VF2_CAMERA_GATE_ENTRY: {
        vf2_recovered_camera_gate_report recovered;
        memset(&recovered, 0, sizeof(recovered));
        status = vf2_recovered_task_camera_post_update_gate(
            machine, registry_address, &recovered
        );
        if (status == VF2_OK) {
            local_report.kind = VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE;
            local_report.exit_address = recovered.stop_address;
            local_report.task_bytes_written = recovered.task_bytes_written;
            local_report.viewport_entries_written =
                recovered.viewport_entries_written;
            local_report.viewport_executed = recovered.viewport_executed;
            local_report.fast_exit = recovered.fast_exit;
            if (recovered.fast_exit != 0 &&
                recovered.viewport_executed == 0 &&
                recovered.stop_address == VF2_CAMERA_GATE_FAST_EXIT) {
                local_report.recovered_instruction_count = UINT64_C(6);
            }
        }
        break;
    }
    default:
        status = VF2_ERROR_UNSUPPORTED;
        break;
    }

    if (status == VF2_OK && report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_hybrid_camera_apply(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t instruction_pointer,
    vf2_hybrid_block_report *report
)
{
    return hybrid_camera_apply_memory(
        machine, registry_address, instruction_pointer, report
    );
}

static vf2_status hybrid_camera_apply_cpu_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_hybrid_block_report *report
)
{
    uint32_t value = 0u;
    uint8_t input_index = 0u;
    uint8_t control_flags = 0u;
    uint8_t range_flags = 0u;
    uint16_t input_flags = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    switch (report->kind) {
    case VF2_HYBRID_BLOCK_CAMERA_INITIALIZE:
        if (report->entry_address != VF2_CAMERA_INITIALIZE_ENTRY ||
            report->exit_address != VF2_CAMERA_INITIALIZE_EXIT) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cpu->registers[2] = UINT32_C(0x0001d44c);
        cpu->registers[15] = VF2_CAMERA_INITIALIZE_EXIT;
        /* g7 is the live fighter-profile cursor. The initializer leaves it on
         * fighter 0, one 0x2000-byte profile block below the entry value. */
        cpu->registers[23] -= UINT32_C(0x00002000);
        break;

    case VF2_HYBRID_BLOCK_CAMERA_UPDATE:
        if (report->entry_address != VF2_CAMERA_UPDATE_ENTRY ||
            report->exit_address != VF2_CAMERA_UPDATE_EXIT) {
            return VF2_ERROR_UNSUPPORTED;
        }
        cpu->registers[2] = UINT32_C(0x0001d654);
        cpu->registers[13] = UINT32_C(0x3d4ccccd);
        cpu->registers[14] = UINT32_C(0x3d4ccccd);
        status = vf2_model2a_read_u32(machine, report->registry_address, &value);
        if (status == VF2_OK) {
            cpu->registers[15] = value;
            status = vf2_model2a_read_u32(
                machine, report->registry_address + UINT32_C(0x1c), &value
            );
        }
        if (status == VF2_OK) {
            cpu->registers[17] = value;
            status = vf2_model2a_read_u32(
                machine, report->registry_address + UINT32_C(0x20), &value
            );
        }
        if (status == VF2_OK) {
            cpu->registers[18] = value;
            status = vf2_model2a_read(
                machine, report->registry_address + UINT32_C(0xfa),
                &range_flags, sizeof(range_flags)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[16] = range_flags;
        if (report->recovered_procedure_calls == UINT64_C(7)) {
            uint32_t fighter1 = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 4u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 5u] =
                report->registry_address + UINT32_C(0x1bc);
            cpu->registers[VF2_I960_G0_REGISTER + 6u] =
                report->registry_address + UINT32_C(0x1e4);
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500808), &fighter1
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
        } else if (report->recovered_procedure_calls == UINT64_C(6)) {
            cpu->registers[VF2_I960_G0_REGISTER + 4u] = UINT32_MAX;
            cpu->registers[VF2_I960_G0_REGISTER + 5u] =
                report->registry_address + UINT32_C(0x1e4);
        }
        /* The recurring update finishes after selecting fighter 1, restoring
         * the cursor to the adjacent profile block. */
        cpu->registers[23] += UINT32_C(0x00002000);
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        break;

    case VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE:
        if (report->entry_address != VF2_CAMERA_GATE_ENTRY ||
            report->exit_address != VF2_CAMERA_GATE_FAST_EXIT ||
            report->fast_exit == 0 || report->viewport_executed != 0) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050009c),
            &control_flags, sizeof(control_flags)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x00500064),
                &input_index, sizeof(input_index)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine,
                UINT32_C(0x0006eea0) + ((uint32_t)input_index << 8u),
                &input_flags, sizeof(input_flags)
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = control_flags;
        cpu->registers[15] = input_flags;
        break;

    case VF2_HYBRID_BLOCK_NONE:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    if (report->recovered_procedure_calls != 0u) {
        const uint32_t depth_delta =
            report->kind == VF2_HYBRID_BLOCK_CAMERA_UPDATE &&
            (report->recovered_procedure_calls == UINT64_C(6) ||
             report->recovered_procedure_calls == UINT64_C(7))
                ? UINT32_C(2)
                : UINT32_C(1);
        if (cpu->maximum_local_frame_depth <
            cpu->local_frame_depth + depth_delta) {
            cpu->maximum_local_frame_depth =
                cpu->local_frame_depth + depth_delta;
        }
    }
    cpu->ip = report->exit_address;
    cpu->executed_instructions += report->recovered_instruction_count;
    cpu->procedure_calls += report->recovered_procedure_calls;
    cpu->procedure_returns += report->recovered_procedure_returns;
    return VF2_OK;
}

vf2_status vf2_hybrid_camera_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_block_report *report
)
{
    vf2_hybrid_block_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->registers[29] != registry_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    if (cpu->ip == VF2_CAMERA_GATE_ENTRY) {
        status = hybrid_camera_fast_gate_supported(machine);
    }
    if (status == VF2_OK) {
        status = hybrid_camera_apply_memory(
            machine, registry_address, cpu->ip, &local_report
        );
    }
    if (status == VF2_OK) {
        status = hybrid_camera_apply_cpu_poststate(machine, cpu, &local_report);
    }
    if (status == VF2_OK) {
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}


#define VF2_TASK_GAME_INFO_ENTRY UINT32_C(0x0001645c)
#define VF2_TASK_SCHEDULER_RETURN UINT32_C(0x00010dcc)

#define VF2_SCHEDULER_TASK_COUNT_ADDRESS UINT32_C(0x00011d94)
#define VF2_SCHEDULER_TASK_NAME_TABLE UINT32_C(0x00011dd8)
#define VF2_SCHEDULER_TASK_NAME_STRIDE UINT32_C(0x40)
#define VF2_SCHEDULER_TASK_NAME_SIZE 12u
#define VF2_SCHEDULER_TASK_NAME_TEXT_SIZE 11u
#define VF2_SCHEDULER_CURRENT_INDEX UINT32_C(0x00500038)
#define VF2_SCHEDULER_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_SCHEDULER_INPUT_POINTER UINT32_C(0x00500814)
#define VF2_SCHEDULER_NAME_BUFFER UINT32_C(0x0050e000)
#define VF2_SCHEDULER_NAME_CURSOR UINT32_C(0x0050e003)
#define VF2_SCHEDULER_NAME_FORMAT UINT32_C(0x0100045c)
#define VF2_SCHEDULER_TILE_NAME_CHARS 8u
#define VF2_SCHEDULER_TILE_NAME_BYTES 18u
#define VF2_SCHEDULER_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_SCHEDULER_SCRATCH_STRIDE UINT32_C(0x20)
#define VF2_SCHEDULER_TIMER_MASK UINT32_C(0x000fffff)

vf2_status vf2_hybrid_first_dispatch_scheduler_advance(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t current_task_index,
    size_t next_task_index,
    uint32_t current_registry_address,
    uint32_t next_registry_address,
    uint32_t next_entry_address,
    vf2_hybrid_scheduler_transition_report *report
)
{
    vf2_hybrid_scheduler_transition_report local_report;
    uint8_t task_name[VF2_SCHEDULER_TASK_NAME_SIZE];
    uint8_t tile_name[VF2_SCHEDULER_TILE_NAME_BYTES];
    uint8_t input_flags = 0u;
    uint32_t input_pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t scratch0 = 0u;
    uint32_t scratch1 = 0u;
    uint32_t scratch2 = 0u;
    uint32_t scratch3 = 0u;
    uint32_t current_scratch = 0u;
    uint32_t next_scratch = 0u;
    uint64_t scanned = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        current_task_index >= next_task_index || next_task_index >= 32u ||
        cpu->ip != VF2_TASK_SCHEDULER_RETURN ||
        cpu->local_frame_depth == 0u ||
        cpu->registers[29] != current_registry_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(task_name, 0, sizeof(task_name));
    memset(tile_name, 0, sizeof(tile_name));
    current_scratch = VF2_SCHEDULER_SCRATCH_BASE +
        (uint32_t)current_task_index * VF2_SCHEDULER_SCRATCH_STRIDE;
    next_scratch = VF2_SCHEDULER_SCRATCH_BASE +
        (uint32_t)next_task_index * VF2_SCHEDULER_SCRATCH_STRIDE;
    scanned = (uint64_t)(next_task_index - current_task_index);

    status = vf2_model2a_read_u32(
        machine, VF2_SCHEDULER_TASK_COUNT_ADDRESS, &task_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_INPUT_POINTER, &input_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, input_pointer + UINT32_C(0xde),
            &input_flags, sizeof(input_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), &timer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(8), &timer2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry_address + UINT32_C(0x38), &threshold
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, current_scratch, &scratch0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(4), &scratch1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(8), &scratch2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(12), &scratch3
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_SCHEDULER_TASK_NAME_TABLE +
                (uint32_t)next_task_index * VF2_SCHEDULER_TASK_NAME_STRIDE,
            task_name, VF2_SCHEDULER_TASK_NAME_TEXT_SIZE
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* This bounded recovery accepts the naturally observed first-dispatch
     * path: 29 descriptors, timing enabled, diagnostic helper enabled, and
     * both timer samples still at the reload value (zero elapsed ticks). */
    if (task_count != 29u || next_task_index >= task_count ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (runtime_flags & (UINT32_C(1) << 9u)) != 0u ||
        (input_flags & (UINT8_C(1) << 2u)) != 0u ||
        (timer1 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        (timer2 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        threshold != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch2;
    status = vf2_model2a_write_u32(
        machine, current_scratch + UINT32_C(8), scratch2
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, current_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SCHEDULER_CURRENT_INDEX,
            (uint32_t)next_task_index
        );
    }
    task_name[VF2_SCHEDULER_TASK_NAME_TEXT_SIZE] = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_BUFFER,
            task_name, sizeof(task_name)
        );
    }
    if (status == VF2_OK) {
        size_t character_index = 0u;

        /* The diagnostic formatter drops the "fa_" prefix, truncates the
         * visible name to eight characters, pads it with spaces, and stores
         * each glyph as a little-endian 0x80xx tile word. The ninth word is
         * the unstyled trailing space already present in the status field. */
        for (character_index = 0u;
             character_index < VF2_SCHEDULER_TILE_NAME_CHARS;
             ++character_index) {
            const uint8_t source =
                task_name[3u + character_index] != 0u
                    ? task_name[3u + character_index]
                    : UINT8_C(0x20);
            tile_name[character_index * 2u] = source;
            tile_name[character_index * 2u + 1u] = UINT8_C(0x80);
        }
        tile_name[16] = UINT8_C(0x20);
        tile_name[17] = UINT8_C(0x00);
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_FORMAT,
            tile_name, sizeof(tile_name)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4),
            VF2_SCHEDULER_TIMER_MASK
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    memset(&cpu->registers[2], 0, 14u * sizeof(cpu->registers[0]));
    cpu->registers[3] = task_count;
    cpu->registers[4] = next_entry_address;
    cpu->registers[5] = scratch1;
    cpu->registers[6] = scratch2;
    cpu->registers[7] = scratch3;
    cpu->registers[8] = VF2_SCHEDULER_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = next_scratch;
    cpu->registers[11] = (uint32_t)next_task_index;
    cpu->registers[13] = VF2_SCHEDULER_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] =
        scanned > UINT64_C(1) ? timer2 : threshold;
    cpu->registers[16] = VF2_SCHEDULER_NAME_CURSOR;
    cpu->registers[25] = VF2_SCHEDULER_NAME_FORMAT;
    cpu->registers[29] = next_registry_address;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;

    if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
    }
    cpu->executed_instructions += scanned * UINT64_C(104) + UINT64_C(12);
    cpu->procedure_calls += scanned * UINT64_C(2);
    cpu->procedure_returns += scanned * UINT64_C(2);
    status = vf2_i960_cpu_enter_procedure(
        cpu, next_entry_address, VF2_TASK_SCHEDULER_RETURN
    );
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.current_task_index = current_task_index;
    local_report.next_task_index = next_task_index;
    local_report.descriptors_scanned = (size_t)scanned;
    local_report.current_registry_address = current_registry_address;
    local_report.next_registry_address = next_registry_address;
    local_report.next_entry_address = next_entry_address;
    local_report.current_scratch_address = current_scratch;
    local_report.next_scratch_address = next_scratch;
    local_report.recovered_instruction_count =
        scanned * UINT64_C(104) + UINT64_C(13);
    local_report.recovered_procedure_calls = scanned * UINT64_C(2) + UINT64_C(1);
    local_report.recovered_procedure_returns = scanned * UINT64_C(2);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    (void)scratch0;
    return VF2_OK;
}


#define VF2_SECOND_SCHEDULER_CALL_SITE UINT32_C(0x0000a010)
#define VF2_SECOND_SCHEDULER_ENTRY UINT32_C(0x00010d54)
#define VF2_SECOND_SCHEDULER_TASK_RETURN UINT32_C(0x00010dcc)
#define VF2_SECOND_SCHEDULER_REGISTRY_BASE UINT32_C(0x00510000)
#define VF2_SECOND_SCHEDULER_GEOMETRY_STATUS UINT32_C(0x00800070)
#define VF2_SECOND_SCHEDULER_GEOMETRY_COMMAND UINT32_C(0x00804000)

static int hybrid_second_scheduler_task_supported(uint32_t entry_address)
{
    switch (entry_address) {
    case VF2_TASK_GAME_INFO_ENTRY:
    case VF2_PLAYER_TASK_ENTRY:
    case VF2_PLAYER_TASK_WRAPPER_ENTRY:
    case VF2_TASK_CAMERA_ENTRY:
    case VF2_TASK_USER_ENTRY:
    case VF2_TASK_SOUND_ENTRY:
    case VF2_TASK_KILL_OSAGE_ENTRY:
    case VF2_TASK_OSAGE_ENTRY:
        return 1;
    default:
        return 0;
    }
}

vf2_status vf2_hybrid_second_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
)
{
    vf2_hybrid_second_scheduler_report local_report;
    uint32_t ready_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t registry = VF2_SECOND_SCHEDULER_REGISTRY_BASE;
    uint32_t scratch = VF2_SCHEDULER_SCRATCH_BASE;
    uint32_t selected_entry = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        (cpu->ip != VF2_SECOND_SCHEDULER_CALL_SITE &&
         cpu->ip != VF2_SECOND_SCHEDULER_ENTRY) ||
        (cpu->ip == VF2_SECOND_SCHEDULER_CALL_SITE &&
         cpu->local_frame_depth > 1u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &ready_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_TASK_COUNT_ADDRESS, &task_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), &timer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(8), &timer2
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((ready_flags & (UINT32_C(1) << 16u)) != 0u ||
        task_count != 29u ||
        (runtime_flags & (UINT32_C(1) << 9u)) == 0u ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        (timer2 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (cpu->ip == VF2_SECOND_SCHEDULER_CALL_SITE) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_SECOND_SCHEDULER_ENTRY,
            VF2_SECOND_SCHEDULER_CALL_SITE + UINT32_C(4)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    /* The two 0x7b18 helpers clear the geometry status word and publish
     * command values 3 then 1. Only the final command remains visible, but
     * both calls and both returns are reflected in the architectural counts. */
    status = vf2_model2a_write_u32(
        machine, VF2_SECOND_SCHEDULER_GEOMETRY_STATUS, 0u
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SECOND_SCHEDULER_GEOMETRY_COMMAND, 3u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SECOND_SCHEDULER_GEOMETRY_STATUS, 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SECOND_SCHEDULER_GEOMETRY_COMMAND, 1u
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    for (index = 0u; index < task_count; ++index) {
        uint32_t flags = 0u;
        uint32_t stack_size = 0u;
        uint32_t elapsed = 0u;

        status = vf2_model2a_write_u32(
            machine, VF2_SCHEDULER_CURRENT_INDEX, (uint32_t)index
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, VF2_TIMER_BASE + UINT32_C(4),
                VF2_SCHEDULER_TIMER_MASK
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, registry, &flags);
        }
        if (status != VF2_OK) {
            return status;
        }
        if ((flags & UINT32_C(0x80000000)) != 0u) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(0x0c), &selected_entry
            );
            if (status != VF2_OK) {
                return status;
            }
            break;
        }

        elapsed = VF2_SCHEDULER_TIMER_MASK -
            (timer2 & VF2_SCHEDULER_TIMER_MASK);
        status = vf2_model2a_write_u32(
            machine, scratch + UINT32_C(0x10), elapsed
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(8), &stack_size
            );
        }
        if (status != VF2_OK || stack_size == 0u ||
            (stack_size & UINT32_C(0x1f)) != 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        registry += stack_size;
        scratch += VF2_SCHEDULER_SCRATCH_STRIDE;
    }

    if (index >= task_count) {
        /* The ROM returns normally when the full registry scan finds no
         * runnable descriptor. The next frame will re-enter the scheduler;
         * there is no task frame to reconstruct in this case. */
        cpu->registers[0] &= ~UINT32_C(7);
        cpu->executed_instructions += UINT64_C(220);
        cpu->procedure_calls += UINT64_C(2);
        cpu->procedure_returns += UINT64_C(2);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK) {
            return status;
        }
        ++cpu->executed_instructions;
        local_report.descriptors_scanned = task_count;
        local_report.inactive_descriptors_scanned = task_count;
        local_report.selected_task_index = task_count;
        local_report.registry_start = VF2_SECOND_SCHEDULER_REGISTRY_BASE;
        local_report.selected_registry_address = registry;
        local_report.selected_entry_address = 0u;
        local_report.scheduler_entry_address = VF2_SECOND_SCHEDULER_ENTRY;
        local_report.recovered_instruction_count = UINT64_C(221);
        local_report.recovered_procedure_calls = UINT64_C(2);
        local_report.recovered_procedure_returns = UINT64_C(3);
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
        return VF2_OK;
    }
    if (!hybrid_second_scheduler_task_supported(selected_entry)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Reconstruct the scheduler frame at the observed callx boundary. The
     * task entry itself receives a fresh local frame, while this state remains
     * cached for the task's later RET to 0x10dcc. */
    memset(&cpu->registers[2], 0, 14u * sizeof(cpu->registers[0]));
    cpu->registers[0] = UINT32_C(0x005ff500);
    cpu->registers[2] = UINT32_C(0x00010d64);
    cpu->registers[3] = task_count;
    cpu->registers[4] = selected_entry;
    cpu->registers[8] = VF2_SCHEDULER_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = scratch;
    cpu->registers[11] = (uint32_t)index;
    cpu->registers[13] = VF2_SCHEDULER_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] = timer2;
    cpu->registers[16] = 1u;
    cpu->registers[29] = registry;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;

    cpu->procedure_calls += UINT64_C(2);
    cpu->procedure_returns += UINT64_C(2);
    if (cpu->maximum_local_frame_depth < 2u) {
        cpu->maximum_local_frame_depth = 2u;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, selected_entry, VF2_SECOND_SCHEDULER_TASK_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(235);

    local_report.descriptors_scanned = index + 1u;
    local_report.inactive_descriptors_scanned = index;
    local_report.selected_task_index = index;
    local_report.registry_start = VF2_SECOND_SCHEDULER_REGISTRY_BASE;
    local_report.selected_registry_address = registry;
    local_report.selected_entry_address = selected_entry;
    local_report.scheduler_entry_address = VF2_SECOND_SCHEDULER_ENTRY;
    local_report.recovered_instruction_count = UINT64_C(235);
    local_report.recovered_procedure_calls = UINT64_C(4);
    local_report.recovered_procedure_returns = UINT64_C(2);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

static vf2_status hybrid_complete_procedure(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t body_instructions,
    uint64_t nested_calls,
    uint64_t nested_returns
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (nested_calls != 0u &&
        cpu->maximum_local_frame_depth < cpu->local_frame_depth + 1u) {
        cpu->maximum_local_frame_depth = cpu->local_frame_depth + 1u;
    }
    cpu->executed_instructions += body_instructions;
    cpu->procedure_calls += nested_calls;
    cpu->procedure_returns += nested_returns;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    return status;
}

static vf2_status hybrid_execute_camera_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    vf2_hybrid_task_report local_report;
    vf2_hybrid_block_report block;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t block_index = 0u;
    vf2_status status = VF2_OK;

    memset(&local_report, 0, sizeof(local_report));
    local_report.kind = VF2_HYBRID_TASK_CAMERA;
    local_report.entry_address = cpu->ip;
    local_report.registry_address = registry_address;
    start_instructions = cpu->executed_instructions;
    start_calls = cpu->procedure_calls;
    start_returns = cpu->procedure_returns;

    for (block_index = 0u; status == VF2_OK && block_index < 3u; ++block_index) {
        memset(&block, 0, sizeof(block));
        status = vf2_hybrid_camera_execute(
            machine, cpu, registry_address, &block
        );
        if (status == VF2_OK) {
            local_report.task_bytes_written += block.task_bytes_written;
            local_report.global_bytes_written += block.global_bytes_written;
            ++local_report.camera_blocks_executed;
        }
    }
    if (status == VF2_OK && cpu->ip != VF2_CAMERA_GATE_FAST_EXIT) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = hybrid_complete_procedure(machine, cpu, 0u, 0u, 0u);
    }
    if (status == VF2_OK && cpu->ip != VF2_TASK_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        local_report.exit_address = cpu->ip;
        local_report.recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        local_report.recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        local_report.recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}

vf2_status vf2_hybrid_first_dispatch_task_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t registry_address,
    vf2_hybrid_task_report *report
)
{
    vf2_hybrid_task_report local_report;
    vf2_recovered_task_report task_report;
    vf2_recovered_kill_osage_report kill_report;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    uint64_t body_instructions = 0u;
    uint64_t nested_calls = 0u;
    uint64_t nested_returns = 0u;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter = 0u;
    uint8_t instance = 0u;
    int interpreted_task = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->registers[29] != registry_address ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip == VF2_TASK_CAMERA_ENTRY) {
        return hybrid_execute_camera_task(
            machine, cpu, registry_address, report
        );
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(&task_report, 0, sizeof(task_report));
    memset(&kill_report, 0, sizeof(kill_report));
    local_report.entry_address = cpu->ip;
    local_report.registry_address = registry_address;
    start_instructions = cpu->executed_instructions;
    start_calls = cpu->procedure_calls;
    start_returns = cpu->procedure_returns;

    switch (cpu->ip) {
    case VF2_TASK_GAME_INFO_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_GAME_INFO;
        status = hybrid_game_info_interpreter_needed(
            machine, &interpreted_task
        );
        if (status == VF2_OK && interpreted_task) {
            status = hybrid_execute_game_info_bit31_native(
                machine, cpu, &task_report
            );
            if (status == VF2_OK) {
                task_report.registry_address = registry_address;
            }
        } else if (status == VF2_OK) {
            status = vf2_recovered_task_game_info_first_dispatch(
                machine, registry_address, &task_report
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500804), &fighter0
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00500808), &fighter1
                );
            }
            if (status == VF2_OK) {
                cpu->registers[23] = fighter1;
                cpu->registers[24] = fighter0;
                body_instructions = UINT64_C(18);
            }
        }
        break;

    case VF2_PLAYER_TASK_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_PLAYER;
        status = hybrid_execute_player_prefix(machine, cpu);
        if (status == VF2_ERROR_INVALID_ARGUMENT) {
            interpreted_task = 1;
            status = hybrid_execute_interpreted_task(
                machine, cpu, registry_address, VF2_PLAYER_TASK_ENTRY,
                &task_report
            );
        } else if (status == VF2_OK) {
            status = hybrid_execute_player_19ef8(machine, cpu);
            if (status == VF2_ERROR_UNSUPPORTED) {
                interpreted_task = 1;
                status = hybrid_execute_interpreted_task(
                    machine, cpu, registry_address, UINT32_C(0x00014288),
                    &task_report
                );
            } else if (status == VF2_OK) {
                status = hybrid_execute_player_1428c(machine, cpu);
                if (status == VF2_ERROR_UNSUPPORTED) {
                    interpreted_task = 1;
                    status = hybrid_execute_interpreted_task(
                        machine, cpu, registry_address, UINT32_C(0x0001428c),
                        &task_report
                    );
                } else if (status == VF2_OK) {
                    status = hybrid_execute_player_142c0(machine, cpu);
                    if (status == VF2_ERROR_UNSUPPORTED) {
                        interpreted_task = 1;
                        status = hybrid_execute_interpreted_task(
                            machine, cpu, registry_address,
                            UINT32_C(0x000142c0), &task_report
                        );
                    } else if (status == VF2_OK) {
                        status = hybrid_execute_player_14310(machine, cpu);
                        if (status == VF2_ERROR_UNSUPPORTED) {
                            interpreted_task = 1;
                            status = hybrid_execute_interpreted_task(
                                machine, cpu, registry_address,
                                UINT32_C(0x00014310), &task_report
                            );
                        } else if (status == VF2_OK) {
                            status = hybrid_execute_player_143e4_prefix(
                                machine, cpu
                            );
                            if (status == VF2_ERROR_UNSUPPORTED) {
                                interpreted_task = 1;
                                status = hybrid_execute_interpreted_task(
                                    machine, cpu, registry_address,
                                    UINT32_C(0x000143e4), &task_report
                                );
                            } else if (status == VF2_OK) {
                                status = hybrid_execute_player_1ab74_prefix(
                                    machine, cpu
                                );
                                if (status == VF2_ERROR_UNSUPPORTED) {
                                    interpreted_task = 1;
                                    status = hybrid_execute_interpreted_task(
                                        machine, cpu, registry_address,
                                        UINT32_C(0x000143fc), &task_report
                                    );
                                } else if (status == VF2_OK) {
                                    status = hybrid_execute_player_27ce0_prefix(
                                        machine, cpu
                                    );
                                    if (status == VF2_ERROR_UNSUPPORTED) {
                                        interpreted_task = 1;
                                        status = hybrid_execute_interpreted_task(
                                            machine, cpu, registry_address,
                                            UINT32_C(0x0001abf4), &task_report
                                        );
                                    } else if (status == VF2_OK) {
                                        interpreted_task = 1;
                                        status = hybrid_execute_player_27d00_call(
                                            machine, cpu
                                        );
                                        if (status == VF2_ERROR_UNSUPPORTED) {
                                            interpreted_task = 1;
                                            status = hybrid_execute_interpreted_task(
                                                machine, cpu, registry_address,
                                                UINT32_C(0x00027d00), &task_report
                                            );
                                        } else if (status == VF2_OK) {
                                            status = hybrid_execute_player_28184_prefix(
                                                machine, cpu
                                            );
                                            if (status == VF2_ERROR_UNSUPPORTED) {
                                                interpreted_task = 1;
                                                status = hybrid_execute_interpreted_task(
                                                    machine, cpu, registry_address,
                                                    UINT32_C(0x00028184), &task_report
                                                );
                                            } else if (status == VF2_OK) {
                                                status = hybrid_execute_player_28268_call(
                                                    machine, cpu
                                                );
                                                if (status == VF2_ERROR_UNSUPPORTED) {
                                                    interpreted_task = 1;
                                                    status = hybrid_execute_interpreted_task(
                                                        machine, cpu, registry_address,
                                                        UINT32_C(0x00028268), &task_report
                                                    );
                                                } else if (status == VF2_OK) {
                                                    status = hybrid_execute_player_28780(
                                                        machine, cpu
                                                    );
                                                    if (status == VF2_ERROR_UNSUPPORTED) {
                                                        interpreted_task = 1;
                                                        status = hybrid_execute_interpreted_task(
                                                            machine, cpu, registry_address,
                                                            UINT32_C(0x00028780), &task_report
                                                        );
                                                    } else if (status == VF2_OK) {
                                                        status = hybrid_execute_player_2826c_to_27d90(
                                                            machine, cpu
                                                        );
                                                        if (status == VF2_ERROR_UNSUPPORTED) {
                                                            interpreted_task = 1;
                                                            status = hybrid_execute_interpreted_task(
                                                                machine, cpu, registry_address,
                                                                UINT32_C(0x0002826c), &task_report
                                                            );
                                                        } else if (status == VF2_OK) {
                                                            status = hybrid_execute_player_27d90_call(
                                                                machine, cpu
                                                            );
                                                            if (status == VF2_ERROR_UNSUPPORTED) {
                                                                interpreted_task = 1;
                                                                status = hybrid_execute_interpreted_task(
                                                                    machine, cpu, registry_address,
                                                                    UINT32_C(0x00027d90), &task_report
                                                                );
                                                            } else if (status == VF2_OK) {
                                                                status = hybrid_execute_player_2901c_first_to_27dcc(
                                                                    machine, cpu
                                                                );
                                                                if (status == VF2_ERROR_UNSUPPORTED) {
                                                                    interpreted_task = 1;
                                                                    status = hybrid_execute_interpreted_task(
                                                                        machine, cpu, registry_address,
                                                                        UINT32_C(0x0002901c), &task_report
                                                                    );
                                                                } else if (status == VF2_OK) {
                                                                    status = hybrid_execute_player_repeated_call(
                                                                        cpu, UINT32_C(0x00027dcc),
                                                                        UINT32_C(0x0002901c),
                                                                        UINT32_C(0x00027dd0)
                                                                    );
                                                                    if (status == VF2_ERROR_UNSUPPORTED) {
                                                                        interpreted_task = 1;
                                                                        status = hybrid_execute_interpreted_task(
                                                                            machine, cpu, registry_address,
                                                                            UINT32_C(0x00027dcc), &task_report
                                                                        );
                                                                    } else if (status == VF2_OK) {
                                                                        status = hybrid_execute_player_2901c_second_to_27dd0(
                                                                            machine, cpu
                                                                        );
                                                                        if (status == VF2_OK) {
                                                                            status = hybrid_execute_player_27dd0_to_27fa0(
                                                                                machine, cpu
                                                                            );
                                                                        }
                                                                        if (status == VF2_ERROR_UNSUPPORTED) {
                                                                            interpreted_task = 1;
                                                                            status = hybrid_execute_interpreted_task(
                                                                                machine, cpu, registry_address,
                                                                                UINT32_C(0x00027dcc), &task_report
                                                                            );
                                                                        } else if (status == VF2_OK) {
                                                                            status = hybrid_execute_player_repeated_call(
                                                                                cpu, UINT32_C(0x00027fa0),
                                                                                UINT32_C(0x0002901c),
                                                                                UINT32_C(0x00027fa4)
                                                                            );
                                                                            if (status == VF2_ERROR_UNSUPPORTED) {
                                                                                interpreted_task = 1;
                                                                                status = hybrid_execute_interpreted_task(
                                                                                    machine, cpu, registry_address,
                                                                                    UINT32_C(0x00027fa0), &task_report
                                                                                );
                                                                            } else if (status == VF2_OK) {
                                                                                status = hybrid_execute_player_2901c_third_to_27fa4(
                                                                                    machine, cpu
                                                                                );
                                                                                if (status == VF2_OK) {
                                                                                    status = hybrid_execute_player_27fa4_to_28174(
                                                                                        machine, cpu
                                                                                    );
                                                                                }
                                                                                if (status == VF2_ERROR_UNSUPPORTED) {
                                                                                    interpreted_task = 1;
                                                                                    status = hybrid_execute_interpreted_task(
                                                                                        machine, cpu, registry_address,
                                                                                        UINT32_C(0x00027fa0), &task_report
                                                                                    );
                                                                                } else if (status == VF2_OK) {
                                                                                    status = hybrid_execute_player_repeated_call(
                                                                                        cpu, UINT32_C(0x00028174),
                                                                                        UINT32_C(0x00029414),
                                                                                        UINT32_C(0x00028178)
                                                                                    );
                                                                                    if (status == VF2_ERROR_UNSUPPORTED) {
                                                                                        interpreted_task = 1;
                                                                                        status = hybrid_execute_interpreted_task(
                                                                                            machine, cpu, registry_address,
                                                                                            UINT32_C(0x00028174), &task_report
                                                                                        );
                                                                                    } else if (status == VF2_OK) {
                                                                                        status = hybrid_execute_player_29414_zero_path(
                                                                                            machine, cpu
                                                                                        );
                                                                                        if (status == VF2_ERROR_UNSUPPORTED) {
                                                                                            interpreted_task = 1;
                                                                                            status = hybrid_execute_interpreted_task(
                                                                                                machine, cpu, registry_address,
                                                                                                UINT32_C(0x00029414), &task_report
                                                                                            );
                                                                                        } else if (status == VF2_OK) {
                                                                                            status = hybrid_execute_player_post_29414(
                                                                                                machine, cpu,
                                                                                                registry_address, &task_report
                                                                                            );
                                                                                        }
                                                                                        if (status == VF2_ERROR_UNSUPPORTED) {
                                                                                            interpreted_task = 1;
                                                                                            status = hybrid_execute_interpreted_task(
                                                                                                machine, cpu, registry_address,
                                                                                                UINT32_C(0x00029414), &task_report
                                                                                            );
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        break;

    case VF2_TASK_USER_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_USER;
        status = vf2_recovered_task_user_execute(
            machine, registry_address, &task_report
        );
        break;

    case VF2_TASK_SOUND_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_SOUND;
        status = vf2_recovered_task_sound_initialize(
            machine, registry_address, &task_report
        );
        if (status == VF2_OK) {
            body_instructions = UINT64_C(14);
        }
        break;

    case VF2_TASK_KILL_OSAGE_ENTRY:
        local_report.kind = VF2_HYBRID_TASK_KILL_OSAGE;
        status = vf2_recovered_task_kill_osage_execute(
            machine, registry_address, &kill_report
        );
        if (status == VF2_OK) {
            cpu->registers[22] = kill_report.elapsed_ticks;
            cpu->registers[23] = kill_report.second_registry_address;
            body_instructions = UINT64_C(35);
            nested_calls = UINT64_C(2);
            nested_returns = UINT64_C(2);
            local_report.task_bytes_written =
                kill_report.flag_words_written * sizeof(uint32_t);
            local_report.global_bytes_written =
                kill_report.records_marked_for_kill * sizeof(uint32_t);
        }
        break;

    case VF2_TASK_OSAGE_ENTRY:
        status = vf2_model2a_read(
            machine, registry_address + UINT32_C(4),
            &instance, sizeof(instance)
        );
        if (status == VF2_OK && instance > 1u) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            local_report.kind = instance == 0u
                ? VF2_HYBRID_TASK_OSAGE0 : VF2_HYBRID_TASK_OSAGE1;
            status = vf2_recovered_task_osage_first_dispatch(
                machine, registry_address, &task_report
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry_address + UINT32_C(0x40), &fighter
            );
        }
        if (status == VF2_OK) {
            cpu->registers[23] = fighter;
            body_instructions = instance == 0u
                ? UINT64_C(18) : UINT64_C(17);
        }
        break;

    default:
        status = VF2_ERROR_UNSUPPORTED;
        break;
    }

    if (status == VF2_OK && local_report.kind != VF2_HYBRID_TASK_KILL_OSAGE) {
        local_report.task_bytes_written = task_report.bytes_written;
        local_report.global_bytes_written = task_report.global_bytes_written;
    }
    if (status == VF2_OK && !interpreted_task) {
        status = hybrid_complete_procedure(
            machine, cpu, body_instructions, nested_calls, nested_returns
        );
    }
    if (status == VF2_OK && cpu->ip != VF2_TASK_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        local_report.exit_address = cpu->ip;
        local_report.recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        local_report.recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        local_report.recovered_procedure_returns =
            cpu->procedure_returns - start_returns;
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}

const char *vf2_hybrid_task_kind_name(vf2_hybrid_task_kind kind)
{
    switch (kind) {
    case VF2_HYBRID_TASK_GAME_INFO:
        return "fa_game_info";
    case VF2_HYBRID_TASK_CAMERA:
        return "fa_camera";
    case VF2_HYBRID_TASK_USER:
        return "fa_user";
    case VF2_HYBRID_TASK_SOUND:
        return "fa_sound";
    case VF2_HYBRID_TASK_KILL_OSAGE:
        return "fa_kill_osage";
    case VF2_HYBRID_TASK_OSAGE0:
        return "fa_osage0";
    case VF2_HYBRID_TASK_OSAGE1:
        return "fa_osage1";
    case VF2_HYBRID_TASK_PLAYER:
        return "fa_player";
    case VF2_HYBRID_TASK_NONE:
    default:
        return "none";
    }
}

const char *vf2_hybrid_block_kind_name(vf2_hybrid_block_kind kind)
{
    switch (kind) {
    case VF2_HYBRID_BLOCK_CAMERA_INITIALIZE:
        return "camera-initialize";
    case VF2_HYBRID_BLOCK_CAMERA_UPDATE:
        return "camera-update";
    case VF2_HYBRID_BLOCK_CAMERA_POST_UPDATE:
        return "camera-post-update";
    case VF2_HYBRID_BLOCK_NONE:
    default:
        return "none";
    }
}

vf2_status vf2_hybrid_first_dispatch_scheduler_finish(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t current_task_index,
    uint32_t current_registry_address,
    vf2_hybrid_scheduler_finish_report *report
)
{
    static const uint16_t final_tiles[18] = {
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8020),
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8020),
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8020),
        UINT16_C(0x8020), UINT16_C(0x8020), UINT16_C(0x8045),
        UINT16_C(0x8058), UINT16_C(0x8041), UINT16_C(0x8044),
        UINT16_C(0x0020), UINT16_C(0x0000), UINT16_C(0x0000)
    };
    enum {
        LAST_TASK_INDEX = 27u,
        INACTIVE_TASK_INDEX = 28u
    };
    const uint32_t inactive_registry = UINT32_C(0x00516400);
    const uint32_t end_registry = UINT32_C(0x00516480);
    const uint32_t current_scratch =
        VF2_SCHEDULER_SCRATCH_BASE + LAST_TASK_INDEX * VF2_SCHEDULER_SCRATCH_STRIDE;
    const uint32_t inactive_scratch =
        VF2_SCHEDULER_SCRATCH_BASE + INACTIVE_TASK_INDEX * VF2_SCHEDULER_SCRATCH_STRIDE;
    uint8_t task_name[VF2_SCHEDULER_TASK_NAME_SIZE];
    uint8_t tile_bytes[sizeof(final_tiles)];
    uint8_t input_flags = 0u;
    uint32_t input_pointer = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t task_count = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t inactive_flags = 0u;
    uint32_t scratch_count = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;
    vf2_hybrid_scheduler_finish_report local_report;

    if (machine == NULL || cpu == NULL ||
        current_task_index != LAST_TASK_INDEX ||
        current_registry_address != UINT32_C(0x00516180) ||
        cpu->ip != VF2_TASK_SCHEDULER_RETURN ||
        cpu->local_frame_depth != 1u ||
        cpu->registers[29] != current_registry_address) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(task_name, 0, sizeof(task_name));
    status = vf2_model2a_read_u32(
        machine, VF2_SCHEDULER_TASK_COUNT_ADDRESS, &task_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_RUNTIME_FLAGS, &runtime_flags
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_SCHEDULER_INPUT_POINTER, &input_pointer
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, input_pointer + UINT32_C(0xde),
            &input_flags, sizeof(input_flags)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), &timer1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(8), &timer2
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_registry_address + UINT32_C(0x38), &threshold
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, inactive_registry, &inactive_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, current_scratch + UINT32_C(8), &scratch_count
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            VF2_SCHEDULER_TASK_NAME_TABLE +
                INACTIVE_TASK_INDEX * VF2_SCHEDULER_TASK_NAME_STRIDE,
            task_name, VF2_SCHEDULER_TASK_NAME_TEXT_SIZE
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if (task_count != 29u ||
        (runtime_flags & ((UINT32_C(1) << 5u) | (UINT32_C(1) << 9u))) != 0u ||
        (input_flags & (UINT8_C(1) << 2u)) != 0u ||
        (timer1 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        (timer2 & VF2_SCHEDULER_TIMER_MASK) != VF2_SCHEDULER_TIMER_MASK ||
        threshold != 0u ||
        (inactive_flags & UINT32_C(0x80000000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch_count;
    status = vf2_model2a_write_u32(
        machine, current_scratch + UINT32_C(8), scratch_count
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, current_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, inactive_scratch + UINT32_C(0x10), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_SCHEDULER_CURRENT_INDEX, INACTIVE_TASK_INDEX
        );
    }
    task_name[VF2_SCHEDULER_TASK_NAME_TEXT_SIZE] = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_BUFFER, task_name, sizeof(task_name)
        );
    }
    for (index = 0u; index < sizeof(final_tiles) / sizeof(final_tiles[0]); ++index) {
        tile_bytes[index * 2u] = (uint8_t)final_tiles[index];
        tile_bytes[index * 2u + 1u] = (uint8_t)(final_tiles[index] >> 8u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, VF2_SCHEDULER_NAME_FORMAT,
            tile_bytes, sizeof(tile_bytes)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TIMER_BASE + UINT32_C(4), VF2_SCHEDULER_TIMER_MASK
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[16] = UINT32_C(0x00444158);
    cpu->registers[25] = UINT32_C(0x010004dc);
    cpu->registers[29] = end_registry;
    cpu->executed_instructions += UINT64_C(280);
    cpu->procedure_calls += UINT64_C(4);
    cpu->procedure_returns += UINT64_C(4);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    if (cpu->ip != UINT32_C(0x0000a014)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    local_report.current_task_index = LAST_TASK_INDEX;
    local_report.inactive_descriptors_scanned = 1u;
    local_report.final_task_index = INACTIVE_TASK_INDEX;
    local_report.current_registry_address = current_registry_address;
    local_report.inactive_registry_address = inactive_registry;
    local_report.end_registry_address = end_registry;
    local_report.continuation_address = cpu->ip;
    local_report.recovered_instruction_count = UINT64_C(281);
    local_report.recovered_procedure_calls = UINT64_C(4);
    local_report.recovered_procedure_returns = UINT64_C(5);
    local_report.cpu_poststate_applied = 1;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
