#include "vf2/i960/executor.h"

#include <string.h>

#include "vf2/i960/decoder.h"

vf2_status vf2_i960_step_legacy(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    vf2_i960_trace_event *event
);

static vf2_status arch_operand_value(
    const vf2_i960_cpu *cpu,
    const vf2_i960_operand *operand,
    uint32_t *value
)
{
    if (cpu == NULL || operand == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    switch (operand->kind) {
    case VF2_I960_OPERAND_REGISTER:
    case VF2_I960_OPERAND_FP_REGISTER:
    case VF2_I960_OPERAND_SPECIAL_REGISTER:
        if (operand->value.reg >= VF2_I960_REGISTER_COUNT) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *value = cpu->registers[operand->value.reg];
        return VF2_OK;
    case VF2_I960_OPERAND_LITERAL:
        *value = (uint32_t)operand->value.literal;
        return VF2_OK;
    case VF2_I960_OPERAND_ADDRESS:
        *value = operand->value.address;
        return VF2_OK;
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
}

static void arch_set_compare(
    vf2_i960_cpu *cpu,
    vf2_i960_compare_result result
)
{
    uint32_t bits = 0u;

    if (result == VF2_I960_COMPARE_LESS) {
        bits = UINT32_C(4);
    } else if (result == VF2_I960_COMPARE_EQUAL) {
        bits = UINT32_C(2);
    } else if (result == VF2_I960_COMPARE_GREATER) {
        bits = UINT32_C(1);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

static vf2_i960_compare_result arch_compare(
    uint32_t left,
    uint32_t right,
    int is_signed
)
{
    if (is_signed != 0) {
        const int32_t signed_left = (int32_t)left;
        const int32_t signed_right = (int32_t)right;
        if (signed_left < signed_right) {
            return VF2_I960_COMPARE_LESS;
        }
        if (signed_left > signed_right) {
            return VF2_I960_COMPARE_GREATER;
        }
        return VF2_I960_COMPARE_EQUAL;
    }
    if (left < right) {
        return VF2_I960_COMPARE_LESS;
    }
    if (left > right) {
        return VF2_I960_COMPARE_GREATER;
    }
    return VF2_I960_COMPARE_EQUAL;
}

static int arch_compare_branch_matches(
    const char *mnemonic,
    vf2_i960_compare_result result
)
{
    const char *suffix = mnemonic + 4;

    if (strcmp(suffix, "bg") == 0) {
        return result == VF2_I960_COMPARE_GREATER;
    }
    if (strcmp(suffix, "be") == 0) {
        return result == VF2_I960_COMPARE_EQUAL;
    }
    if (strcmp(suffix, "bge") == 0) {
        return result == VF2_I960_COMPARE_GREATER ||
               result == VF2_I960_COMPARE_EQUAL;
    }
    if (strcmp(suffix, "bl") == 0) {
        return result == VF2_I960_COMPARE_LESS;
    }
    if (strcmp(suffix, "bne") == 0) {
        return result != VF2_I960_COMPARE_EQUAL;
    }
    if (strcmp(suffix, "ble") == 0) {
        return result == VF2_I960_COMPARE_LESS ||
               result == VF2_I960_COMPARE_EQUAL;
    }
    if (strcmp(suffix, "bo") == 0) {
        return 1;
    }
    if (strcmp(suffix, "bno") == 0) {
        return 0;
    }
    return 0;
}

static void arch_set_ip(
    vf2_i960_cpu *cpu,
    vf2_i960_trace_event *event,
    const vf2_i960_instruction *instruction,
    uint32_t ip_before,
    int branch
)
{
    cpu->ip = branch != 0
        ? instruction->target
        : ip_before + (uint32_t)instruction->size;
    if (event != NULL) {
        event->ip_after = cpu->ip;
    }
}

static vf2_status arch_fix_direct_compare(
    vf2_i960_cpu *cpu,
    vf2_i960_trace_event *event,
    const vf2_i960_instruction *instruction,
    uint32_t ip_before,
    uint32_t first,
    uint32_t second
)
{
    const char *mnemonic = instruction->mnemonic;

    if (strcmp(mnemonic, "bbs") == 0 || strcmp(mnemonic, "bbc") == 0) {
        const int set =
            (second & (UINT32_C(1) << (first & UINT32_C(31)))) != 0u;
        arch_set_compare(
            cpu, set != 0 ? VF2_I960_COMPARE_EQUAL : VF2_I960_COMPARE_NONE
        );
        arch_set_ip(
            cpu, event, instruction, ip_before,
            strcmp(mnemonic, "bbs") == 0 ? set : !set
        );
        return VF2_OK;
    }
    if (strncmp(mnemonic, "cmpob", 5u) == 0 ||
        strncmp(mnemonic, "cmpib", 5u) == 0) {
        const int is_signed = strncmp(mnemonic, "cmpib", 5u) == 0;
        const vf2_i960_compare_result result =
            arch_compare(first, second, is_signed);
        arch_set_compare(cpu, result);
        arch_set_ip(
            cpu, event, instruction, ip_before,
            arch_compare_branch_matches(mnemonic, result)
        );
    }
    return VF2_OK;
}

static vf2_status arch_execute_literal_multi_move(
    vf2_i960_cpu *cpu,
    vf2_i960_trace_event *event,
    const vf2_i960_instruction *instruction,
    uint32_t ip_before
)
{
    const char *mnemonic = instruction->mnemonic;
    const size_t count = strcmp(mnemonic, "movl") == 0 ? 2u :
                         (strcmp(mnemonic, "movt") == 0 ? 3u : 4u);
    const size_t alignment = count == 2u ? 2u : 4u;
    uint8_t destination = 0u;
    size_t index = 0u;

    if (instruction->operands[0].kind != VF2_I960_OPERAND_LITERAL ||
        instruction->operands[1].kind != VF2_I960_OPERAND_REGISTER) {
        return VF2_ERROR_UNSUPPORTED;
    }
    destination = instruction->operands[1].value.reg;
    if ((size_t)destination + count > VF2_I960_REGISTER_COUNT) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (((size_t)destination % alignment) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[destination] =
        (uint32_t)instruction->operands[0].value.literal;
    for (index = 1u; index < count; ++index) {
        cpu->registers[destination + index] = 0u;
    }
    cpu->ip = ip_before + (uint32_t)instruction->size;
    ++cpu->executed_instructions;
    if (event != NULL) {
        memset(event, 0, sizeof(*event));
        event->step = cpu->executed_instructions;
        event->ip_before = ip_before;
        event->ip_after = cpu->ip;
        event->instruction = *instruction;
    }
    return VF2_OK;
}

vf2_status vf2_i960_step(
    vf2_i960_cpu *cpu,
    vf2_model2a *machine,
    vf2_i960_trace_event *event
)
{
    vf2_i960_instruction instruction;
    const uint32_t ip_before = cpu != NULL ? cpu->ip : 0u;
    uint32_t first = 0u;
    uint32_t second = 0u;
    int direct_compare = 0;
    int literal_multi_move = 0;
    vf2_status decode_status = VF2_OK;
    vf2_status status = VF2_OK;

    if (cpu == NULL || machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&instruction, 0, sizeof(instruction));
    decode_status = vf2_i960_decode(
        machine->main_rom, machine->main_rom_size, ip_before, &instruction
    );
    if (decode_status == VF2_OK) {
        const char *mnemonic = instruction.mnemonic;
        direct_compare =
            strcmp(mnemonic, "bbs") == 0 || strcmp(mnemonic, "bbc") == 0 ||
            strncmp(mnemonic, "cmpob", 5u) == 0 ||
            strncmp(mnemonic, "cmpib", 5u) == 0;
        literal_multi_move =
            (strcmp(mnemonic, "movl") == 0 ||
             strcmp(mnemonic, "movt") == 0 ||
             strcmp(mnemonic, "movq") == 0) &&
            instruction.operands[0].kind == VF2_I960_OPERAND_LITERAL;
        if (direct_compare != 0) {
            decode_status = arch_operand_value(
                cpu, &instruction.operands[0], &first
            );
            if (decode_status == VF2_OK) {
                decode_status = arch_operand_value(
                    cpu, &instruction.operands[1], &second
                );
            }
        }
    }

    if (decode_status != VF2_OK) {
        return decode_status;
    }
    if (literal_multi_move != 0) {
        return arch_execute_literal_multi_move(
            cpu, event, &instruction, ip_before
        );
    }

    status = vf2_i960_step_legacy(cpu, machine, event);
    if (status != VF2_OK) {
        return status;
    }
    if (direct_compare != 0) {
        return arch_fix_direct_compare(
            cpu, event, &instruction, ip_before, first, second
        );
    }
    if (strcmp(instruction.mnemonic, "bo") == 0 ||
        strcmp(instruction.mnemonic, "bno") == 0) {
        const int ordered = (cpu->arithmetic_control & UINT32_C(7)) != 0u;
        arch_set_ip(
            cpu, event, &instruction, ip_before,
            strcmp(instruction.mnemonic, "bo") == 0 ? ordered : !ordered
        );
    }
    return VF2_OK;
}
