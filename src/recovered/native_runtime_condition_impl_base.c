#include "vf2/native_runtime.h"

#include "texture_bridge_internal.h"

#define VF2_FRAME_SELECTOR UINT32_C(0x0050002a)
#define VF2_FRAME_SELECTOR_COPY UINT32_C(0x0050002b)
#define VF2_FRAME_SELECTOR_MASK UINT32_C(0x0050002c)
#define VF2_START_STATE UINT32_C(0x005000a4)
#define VF2_SELECTOR0_SIGNATURE UINT32_C(0x0059cfe0)
#define VF2_SELECTOR0_RECT_BASE UINT32_C(0x01000000)
#define VF2_SELECTOR0_COMMAND_BASE UINT32_C(0x01004000)
#define VF2_SELECTOR0_GLYPH_TABLE UINT32_C(0x02a69f02)
#define VF2_SELECTOR0_SIGNATURE_HELPER UINT32_C(0x00061198)
#define VF2_SELECTOR0_RECT_HELPER UINT32_C(0x00008ef0)
#define VF2_SELECTOR0_TEXT_HELPER UINT32_C(0x00008918)
#define VF2_SELECTOR0_BODY_ENTRY UINT32_C(0x0000a804)
#define VF2_SELECTOR0_BODY_RETURN UINT32_C(0x0000a6f4)
#define VF2_SELECTOR0_DISPATCH_RETURN UINT32_C(0x0000a010)
#define VF2_SELECTOR0_INSTRUCTIONS UINT64_C(15853)

vf2_status vf2_native_runtime_step_impl(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *state,
    vf2_native_runtime_step_report *report
);

vf2_status vf2_hybrid_bridge_apply_condition_poststate(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t entry_r3,
    uint32_t entry_r7
);

static void set_runtime_none_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_NONE;
}

static void set_runtime_equal_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
}

static void set_runtime_less_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
}

static void set_runtime_greater_condition(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
}

static vf2_status read_frame_selector(
    vf2_model2a *machine,
    uint8_t *selector
)
{
    return vf2_model2a_read(
        machine,
        VF2_FRAME_SELECTOR,
        selector,
        sizeof(*selector)
    );
}

static vf2_status selector0_signature_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    static const uint32_t expected[4] = {
        UINT32_C(0x52455320), UINT32_C(0x4e4c2053),
        UINT32_C(0x4e204544), UINT32_C(0x20514555)
    };
    uint32_t index = 0u;
    int match = 1;
    vf2_status status = VF2_OK;

    for (index = 0u; index < UINT32_C(4); ++index) {
        status = vf2_model2a_read_u32(
            machine,
            VF2_SELECTOR0_SIGNATURE + index * UINT32_C(4),
            &cpu->registers[4u + index]
        );
        if (status != VF2_OK) {
            return status;
        }
        if (cpu->registers[4u + index] != expected[index]) {
            match = 0;
        }
        cpu->registers[8u + index] = expected[index];
    }

    cpu->registers[VF2_I960_G0_REGISTER] =
        match ? UINT32_MAX : UINT32_C(0);
    for (index = 0u; index < UINT32_C(4); ++index) {
        cpu->registers[4u + index] = 0u;
        status = vf2_model2a_write_u32(
            machine,
            VF2_SELECTOR0_SIGNATURE + index * UINT32_C(4),
            UINT32_C(0)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status selector0_rect_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t row = 0u;
    vf2_status status = VF2_OK;

    cpu->registers[3] = UINT32_C(32);
    for (row = 0u; row < cpu->registers[VF2_I960_G0_REGISTER + 1u]; ++row) {
        uint32_t column = 0u;
        cpu->registers[6] = cpu->registers[VF2_I960_G0_REGISTER + 9u];
        cpu->registers[7] = cpu->registers[VF2_I960_G0_REGISTER];
        for (column = 0u;
             column < cpu->registers[VF2_I960_G0_REGISTER];
             ++column) {
            status = write_u16(
                machine,
                cpu->registers[6],
                UINT16_C(32)
            );
            if (status != VF2_OK) {
                return status;
            }
            cpu->registers[6] += UINT32_C(2);
            --cpu->registers[7];
        }
        cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(0x80);
        --cpu->registers[VF2_I960_G0_REGISTER + 1u];
        --row;
        if (cpu->registers[VF2_I960_G0_REGISTER + 1u] == 0u) {
            break;
        }
    }
    set_runtime_equal_condition(cpu);
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status selector0_text_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t *bytes_written
)
{
    vf2_status status = VF2_OK;

    cpu->registers[5] = UINT32_C(0x00008000);
    for (;;) {
        uint8_t character = 0u;
        uint16_t first = 0u;
        uint16_t second = 0u;

        status = vf2_model2a_read(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER],
            &character,
            sizeof(character)
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[4] = character;
        if (character == 0u) {
            break;
        }
        ++cpu->registers[VF2_I960_G0_REGISTER];

        status = read_u16(
            machine,
            VF2_SELECTOR0_GLYPH_TABLE + (uint32_t)character * UINT32_C(4),
            &first
        );
        if (status == VF2_OK) {
            status = read_u16(
                machine,
                VF2_SELECTOR0_GLYPH_TABLE + UINT32_C(2) +
                    (uint32_t)character * UINT32_C(4),
                &second
            );
        }
        if (status != VF2_OK) {
            return status;
        }

        cpu->registers[15] = first;
        cpu->registers[6] = (uint32_t)first + UINT32_C(0x8000);
        status = write_u16(
            machine,
            cpu->registers[VF2_I960_G0_REGISTER + 9u],
            (uint16_t)cpu->registers[6]
        );
        if (status == VF2_OK) {
            cpu->registers[15] = second;
            cpu->registers[6] = (uint32_t)second + UINT32_C(0x8000);
            status = write_u16(
                machine,
                cpu->registers[VF2_I960_G0_REGISTER + 9u] + UINT32_C(0x80),
                (uint16_t)cpu->registers[6]
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[VF2_I960_G0_REGISTER + 9u] += UINT32_C(2);
        if (bytes_written != NULL) {
            *bytes_written += 2u * sizeof(uint16_t);
        }
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status execute_selector0_body(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    size_t *bytes_written
)
{
    static const uint32_t destinations[9] = {
        UINT32_C(0x01000332), UINT32_C(0x01000618),
        UINT32_C(0x01000818), UINT32_C(0x01000a18),
        UINT32_C(0x01000c18), UINT32_C(0x01000e18),
        UINT32_C(0x01001018), UINT32_C(0x01001218),
        UINT32_C(0x01001650)
    };
    static const uint32_t sources[9] = {
        UINT32_C(0x0000a9a4), UINT32_C(0x0000a9b2),
        UINT32_C(0x0000a9d9), UINT32_C(0x0000a9fa),
        UINT32_C(0x0000aa1a), UINT32_C(0x0000aa42),
        UINT32_C(0x0000aa67), UINT32_C(0x0000aa87),
        UINT32_C(0x0000aaad)
    };
    static const uint32_t returns[9] = {
        UINT32_C(0x0000a8b0), UINT32_C(0x0000a8c4),
        UINT32_C(0x0000a8d8), UINT32_C(0x0000a8ec),
        UINT32_C(0x0000a900), UINT32_C(0x0000a914),
        UINT32_C(0x0000a928), UINT32_C(0x0000a93c),
        UINT32_C(0x0000a950)
    };
    uint32_t base = 0u;
    uint32_t fill_offset = 0u;
    uint8_t mode = 0u;
    uint8_t selector = 0u;
    size_t index = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0050016c), &base
    );

    cpu->registers[3] = base;
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine,
            base + UINT32_C(0x3350),
            &mode,
            sizeof(mode)
        );
    }
    cpu->registers[3] = mode;
    if (status != VF2_OK || mode != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_SELECTOR0_SIGNATURE_HELPER,
        UINT32_C(0x0000a81c)
    );
    if (status == VF2_OK) {
        status = selector0_signature_helper(machine, cpu);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a81c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (bytes_written != NULL) {
        *bytes_written += 4u * sizeof(uint32_t);
    }
    if (cpu->registers[VF2_I960_G0_REGISTER] == UINT32_MAX) {
        selector = UINT8_C(2);
        status = vf2_model2a_write(
            machine, VF2_FRAME_SELECTOR, &selector, sizeof(selector));
        if (status != VF2_OK) {
            return status;
        }
        if (bytes_written != NULL) {
            *bytes_written += sizeof(selector);
        }
        return vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (cpu->registers[VF2_I960_G0_REGISTER] != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[3] = UINT32_C(0x0000c007);
    cpu->registers[4] = UINT32_C(0xc007c007);
    cpu->registers[5] = UINT32_C(0xc007c007);
    cpu->registers[6] = UINT32_C(0xc007c007);
    cpu->registers[7] = UINT32_C(0xc007c007);
    cpu->registers[8] = VF2_SELECTOR0_COMMAND_BASE;
    cpu->registers[12] = UINT32_C(49);
    cpu->registers[15] = UINT32_C(0x80);
    for (fill_offset = 0u;
         fill_offset < UINT32_C(0x1880);
         fill_offset += UINT32_C(4)) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_SELECTOR0_COMMAND_BASE + fill_offset,
            UINT32_C(0xc007c007)
        );
        if (status != VF2_OK) {
            return status;
        }
        if ((fill_offset & UINT32_C(0x7f)) == UINT32_C(0x7c)) {
            cpu->registers[8] += UINT32_C(0x80);
            --cpu->registers[12];
        }
    }
    if (bytes_written != NULL) {
        *bytes_written += (size_t)UINT32_C(0x1880);
    }

    cpu->registers[VF2_I960_G0_REGISTER + 9u] = VF2_SELECTOR0_RECT_BASE;
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(62);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(48);
    status = vf2_i960_cpu_enter_procedure(
        cpu,
        VF2_SELECTOR0_RECT_HELPER,
        UINT32_C(0x0000a89c)
    );
    if (status == VF2_OK) {
        status = selector0_rect_helper(machine, cpu);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a89c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    if (bytes_written != NULL) {
        *bytes_written += (size_t)(UINT32_C(62) * UINT32_C(48) * UINT32_C(2));
    }

    for (index = 0u; index < 9u; ++index) {
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = destinations[index];
        cpu->registers[VF2_I960_G0_REGISTER] = sources[index];
        status = vf2_i960_cpu_enter_procedure(
            cpu,
            VF2_SELECTOR0_TEXT_HELPER,
            returns[index]
        );
        if (status == VF2_OK) {
            status = selector0_text_helper(machine, cpu, bytes_written);
        }
        if (status != VF2_OK || cpu->ip != returns[index]) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }

    cpu->registers[15] = UINT32_C(5) << 7u;
    status = vf2_model2a_write_u32(
        machine,
        UINT32_C(0x00500024),
        cpu->registers[15]
    );
    if (status == VF2_OK) {
        status = read_frame_selector(machine, &selector);
    }
    if (status != VF2_OK || selector != UINT8_C(0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[15] = (uint32_t)selector + UINT32_C(1);
    selector = (uint8_t)cpu->registers[15];
    status = vf2_model2a_write(
        machine,
        VF2_FRAME_SELECTOR,
        &selector,
        sizeof(selector)
    );
    if (status != VF2_OK) {
        return status;
    }
    if (bytes_written != NULL) {
        *bytes_written += sizeof(uint32_t) + sizeof(uint8_t);
    }
    return vf2_i960_cpu_return_procedure(cpu, machine);
}

static vf2_status execute_frame_dispatch_selector0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t flags = 0u;
    uint32_t target = 0u;
    const uint32_t selector_word = UINT32_C(1);
    const uint8_t selector = UINT8_C(0);
    size_t bytes_written = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00508000), &flags
    );

    cpu->registers[15] = flags;
    if (status != VF2_OK || (flags & (UINT32_C(1) << 5u)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[3] = 0u;
    cpu->registers[4] = selector_word;
    status = vf2_model2a_write(
        machine,
        VF2_FRAME_SELECTOR_COPY,
        &selector,
        sizeof(selector)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_FRAME_SELECTOR_MASK,
            selector_word
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x0000a6f8),
            &target
        );
    }
    cpu->registers[5] = target;
    if (status != VF2_OK || target != VF2_SELECTOR0_BODY_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    bytes_written += sizeof(uint8_t) + sizeof(uint32_t);

    status = vf2_i960_cpu_enter_procedure(
        cpu,
        target,
        VF2_SELECTOR0_BODY_RETURN
    );
    if (status == VF2_OK) {
        status = execute_selector0_body(machine, cpu, &bytes_written);
    }
    if (status != VF2_OK || cpu->ip != VF2_SELECTOR0_BODY_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    {
        uint8_t final_selector = 0u;
        const int signature_fast_path =
            cpu->registers[VF2_I960_G0_REGISTER] == UINT32_MAX;
        status = read_frame_selector(machine, &final_selector);
        if (status != VF2_OK ||
            (signature_fast_path && final_selector != UINT8_C(2))) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != VF2_SELECTOR0_DISPATCH_RETURN) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->executed_instructions = start_instructions +
            (signature_fast_path ? UINT64_C(34) : VF2_SELECTOR0_INSTRUCTIONS);
    }
    report->kind = VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;
    report->entry_address = VF2_FRAME_DISPATCH_TICK_ENTRY;
    report->exit_address = cpu->ip;
    report->iterations = UINT64_C(1);
    report->bytes_written = bytes_written;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static vf2_status execute_main_final_cluster_selector0(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    vf2_hybrid_bridge_report shadow = {0};
    vf2_hybrid_bridge_report buffer = {0};
    vf2_hybrid_bridge_report geometry = {0};
    vf2_hybrid_bridge_report scratch = {0};
    vf2_hybrid_bridge_report dispatch = {0};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t start_depth = cpu->local_frame_depth;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_SHADOW_VERIFY_ENTRY, UINT32_C(0x00009ffc)
    );

    if (status == VF2_OK) {
        status = execute_frame_shadow_verify(machine, cpu, &shadow);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00009ffc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00029744), UINT32_C(0x0000a000)
    );
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a000)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_BUFFER_GATE_ENTRY, UINT32_C(0x0000a004)
    );
    if (status == VF2_OK) {
        status = execute_frame_buffer_gate(machine, cpu, &buffer);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a004)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_GEOMETRY_COMMAND_SETUP_ENTRY, UINT32_C(0x0000a008)
    );
    if (status == VF2_OK) {
        status = execute_geometry_command_setup(machine, cpu, &geometry);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a008)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_SCRATCH_CLEAR_ENTRY, UINT32_C(0x0000a00c)
    );
    if (status == VF2_OK) {
        status = execute_frame_scratch_clear(machine, cpu, &scratch);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a00c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_FRAME_DISPATCH_TICK_ENTRY, VF2_SELECTOR0_DISPATCH_RETURN
    );
    if (status == VF2_OK) {
        status = execute_frame_dispatch_selector0(machine, cpu, &dispatch);
    }
    if (status != VF2_OK || cpu->ip != VF2_SELECTOR0_DISPATCH_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (start_depth == 0u) {
        cpu->registers[13] = (start_depth << 8u) | cpu->local_frame_depth;
    }
    cpu->executed_instructions += UINT64_C(7);
    report->kind = VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER;
    report->entry_address = VF2_MAIN_FINAL_CLUSTER_ENTRY;
    report->exit_address = cpu->ip;
    report->bytes_written =
        shadow.bytes_written + buffer.bytes_written + geometry.bytes_written +
        scratch.bytes_written + dispatch.bytes_written;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    report->cpu_poststate_applied = 1;
    return VF2_OK;
}

static void accumulate_custom_bridge(
    vf2_native_runtime_state *state,
    const vf2_hybrid_bridge_report *bridge,
    vf2_native_runtime_step_report *report
)
{
    ++state->blocks_executed;
    state->recovered_instruction_count += bridge->recovered_instruction_count;
    state->recovered_procedure_calls += bridge->recovered_procedure_calls;
    state->recovered_procedure_returns += bridge->recovered_procedure_returns;

    report->kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
    report->bridge_kind = bridge->kind;
    report->entry_address = bridge->entry_address;
    report->exit_address = bridge->exit_address;
    report->recovered_instruction_count = bridge->recovered_instruction_count;
    report->recovered_procedure_calls = bridge->recovered_procedure_calls;
    report->recovered_procedure_returns = bridge->recovered_procedure_returns;
}

static vf2_status apply_post_timer_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t selector_mask = 0u;
    const vf2_status status = vf2_model2a_read_u32(
        machine, VF2_FRAME_SELECTOR_MASK, &selector_mask
    );

    if (status != VF2_OK) {
        return status;
    }
    if ((selector_mask & UINT32_C(0x00030000)) != 0u) {
        set_runtime_less_condition(cpu);
    } else if ((selector_mask & UINT32_C(0x000cffc0)) == 0u) {
        set_runtime_equal_condition(cpu);
    }
    return VF2_OK;
}

static vf2_status apply_repeated_bridge_condition(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry
)
{
    uint8_t selector = 0u;
    vf2_status status = VF2_OK;

    if (entry == VF2_INTERRUPT_PLAYER_LAYER_ENTRY &&
        cpu->ip == VF2_INTERRUPT_GAME_INPUT_ENTRY) {
        status = read_frame_selector(machine, &selector);
        if (status != VF2_OK) {
            return status;
        }
        if (selector == UINT8_C(17)) {
            set_runtime_less_condition(cpu);
        } else if (selector == UINT8_C(1) || selector == UINT8_C(16)) {
            set_runtime_greater_condition(cpu);
        }
    } else if (entry == VF2_MAIN_POST_TIMER_ENTRY &&
               cpu->ip == VF2_MAIN_CLEAR_PREFIX_ENTRY) {
        status = apply_post_timer_condition(machine, cpu);
        if (status != VF2_OK) {
            return status;
        }
    } else if (entry == VF2_TEXTURE_STREAM_HEADER_CALL_ENTRY &&
               cpu->ip == VF2_TEXTURE_STREAM_RESUME_GATE_ENTRY) {
        set_runtime_greater_condition(cpu);
    } else if (entry == VF2_TEXTURE_STREAM_RESUME_GATE_ENTRY &&
               cpu->ip == VF2_TEXTURE_RECORD_ADVANCE_ENTRY) {
        set_runtime_equal_condition(cpu);
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
    vf2_native_runtime_step_report local_report = {0};
    vf2_native_runtime_step_report *effective_report =
        report != NULL ? report : &local_report;
    const uint32_t entry = cpu != NULL ? cpu->ip : 0u;
    const uint32_t entry_r3 = cpu != NULL ? cpu->registers[3] : 0u;
    const uint32_t entry_r7 = cpu != NULL ? cpu->registers[7] : 0u;
    uint8_t entry_frame_selector = UINT8_MAX;
    uint8_t entry_start_state = UINT8_MAX;
    int selector0_custom = 0;
    vf2_status status = VF2_OK;

    if (machine != NULL &&
        (entry == VF2_FRAME_DISPATCH_TICK_ENTRY ||
         entry == VF2_MAIN_FINAL_CLUSTER_ENTRY)) {
        status = read_frame_selector(machine, &entry_frame_selector);
        if (status != VF2_OK) {
            return status;
        }
    }
    if (machine != NULL && entry == VF2_MAIN_FINAL_CLUSTER_ENTRY) {
        status = vf2_model2a_read(
            machine,
            VF2_START_STATE,
            &entry_start_state,
            sizeof(entry_start_state)
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    if (entry == VF2_MAIN_FINAL_CLUSTER_ENTRY &&
        entry_frame_selector == UINT8_C(0)) {
        vf2_hybrid_bridge_report bridge = {0};
        status = execute_main_final_cluster_selector0(machine, cpu, &bridge);
        if (status == VF2_OK) {
            accumulate_custom_bridge(state, &bridge, effective_report);
            selector0_custom = 1;
        }
    } else if (entry == VF2_FRAME_DISPATCH_TICK_ENTRY &&
               entry_frame_selector == UINT8_C(0)) {
        vf2_hybrid_bridge_report bridge = {0};
        status = execute_frame_dispatch_selector0(machine, cpu, &bridge);
        if (status == VF2_OK) {
            accumulate_custom_bridge(state, &bridge, effective_report);
            selector0_custom = 1;
        }
    } else {
        status = vf2_native_runtime_step_impl(
            machine, cpu, state, effective_report
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE) {
        if (selector0_custom) {
            if (entry == VF2_MAIN_FINAL_CLUSTER_ENTRY &&
                entry_start_state == UINT8_C(0x8b)) {
                /* The Start transition's measured selector-0 cluster leaves
                 * the architectural condition code at EQUAL. Capture the
                 * entry state because the bridge may mutate this byte. */
                set_runtime_equal_condition(cpu);
            } else if (entry == VF2_MAIN_FINAL_CLUSTER_ENTRY &&
                       cpu->registers[VF2_I960_G0_REGISTER] != 0u) {
                /* 0xa81c cmpobne 0,g0 is the last condition-setting
                 * instruction on the observed selector-0 non-zero path. */
                set_runtime_less_condition(cpu);
            } else {
                set_runtime_equal_condition(cpu);
            }
        } else {
            status = vf2_hybrid_bridge_apply_condition_poststate(
                machine, cpu, entry, entry_r3, entry_r7
            );
            if (status == VF2_OK) {
                status = apply_repeated_bridge_condition(
                    machine, cpu, entry
                );
            }
            if (status != VF2_OK) {
                return status;
            }
        }
    } else if (effective_report->kind ==
                   VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION ||
               (effective_report->kind ==
                    VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH &&
                (cpu->registers[15] & (UINT32_C(1) << 9u)) != 0u) ||
               (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
                effective_report->task_kind == VF2_HYBRID_TASK_CAMERA)) {
        set_runtime_equal_condition(cpu);
    } else if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
               effective_report->task_kind == VF2_HYBRID_TASK_KILL_OSAGE) {
        set_runtime_less_condition(cpu);
    } else if (effective_report->kind == VF2_NATIVE_RUNTIME_STEP_TASK &&
               (effective_report->task_kind == VF2_HYBRID_TASK_OSAGE0 ||
                effective_report->task_kind == VF2_HYBRID_TASK_OSAGE1)) {
        set_runtime_none_condition(cpu);
    }
    return VF2_OK;
}
