#include "vf2/recovered.h"

#include <string.h>

enum {
    VF2_BOOT_CLEAR_WORK_0_OFFSET = 0x00000000u,
    VF2_BOOT_CLEAR_WORK_0_WORDS = 0x000273f8u,
    VF2_BOOT_CLEAR_WORK_1_OFFSET = 0x0009d000u,
    VF2_BOOT_CLEAR_WORK_1_WORDS = 0x00018c00u,
    VF2_BOOT_CLEAR_BUFFER_OFFSET = 0x00000000u,
    VF2_BOOT_CLEAR_BUFFER_WORDS = 0x00008000u,
    VF2_BOOT_CPU_CONTROL_SOURCE = 0x00003880u,
    VF2_BOOT_INTERRUPT_SOURCE = 0x00003000u,
    VF2_BOOT_INTERRUPT_DESTINATION = 0x005ff410u,
    VF2_BOOT_INTERRUPT_COPY_BYTES = 0x000000b0u,
    VF2_BOOT_INTERRUPT_TABLE_SOURCE = 0x00003a80u,
    VF2_BOOT_INTERRUPT_TABLE_COPY_BYTES = 0x00000410u,
    VF2_BOOT_INTERRUPT_STACK = 0x005ff000u,
    VF2_BOOT_IAC_PACKET = 0x00003860u,
    VF2_BOOT_NEXT_INSTRUCTION = 0x000001b0u,
    VF2_BOOT_QUAD_COPY_ENTRY = 0x00000b58u,
    VF2_BOOT_QUAD_COPY_FIRST_LIMIT = 0x00000404u,
    VF2_BOOT_QUAD_COPY_FIRST_RETURN = 0x00000170u,
    VF2_BOOT_QUAD_COPY_SECOND_LIMIT = 0x000000b0u,
    VF2_BOOT_QUAD_COPY_SECOND_RETURN = 0x0000018cu
};

static int boot_quad_copy_context_observed(const vf2_i960_cpu *cpu)
{
    const uint32_t limit = cpu->registers[16];
    const uint32_t source = cpu->registers[17];
    const uint32_t destination = cpu->registers[18];
    const uint32_t offset = cpu->registers[20];
    const uint32_t return_address = cpu->registers[30];

    if (offset != 0u) {
        return 0;
    }
    if (limit == VF2_BOOT_QUAD_COPY_FIRST_LIMIT &&
        source == VF2_BOOT_INTERRUPT_TABLE_SOURCE &&
        destination == VF2_BOOT_INTERRUPT_STACK &&
        return_address == VF2_BOOT_QUAD_COPY_FIRST_RETURN) {
        return 1;
    }
    return limit == VF2_BOOT_QUAD_COPY_SECOND_LIMIT &&
           source == VF2_BOOT_INTERRUPT_SOURCE &&
           destination == VF2_BOOT_INTERRUPT_DESTINATION &&
           return_address == VF2_BOOT_QUAD_COPY_SECOND_RETURN;
}

vf2_status vf2_recovered_iac_reinitialize_copy_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t offset = 0u;
    size_t index = 0u;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_BOOT_QUAD_COPY_ENTRY ||
        !boot_quad_copy_context_observed(cpu)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (;;) {
        offset = cpu->registers[20];
        for (index = 0u; index < 4u; ++index) {
            const uint32_t word_offset = offset + (uint32_t)(index * 4u);
            vf2_status status = vf2_model2a_read_u32(
                machine,
                cpu->registers[17] + word_offset,
                &cpu->registers[24u + index]
            );
            if (status != VF2_OK) {
                return status;
            }
            status = vf2_model2a_write_u32(
                machine,
                cpu->registers[18] + word_offset,
                cpu->registers[24u + index]
            );
            if (status != VF2_OK) {
                return status;
            }
        }

        cpu->registers[20] += UINT32_C(16);
        cpu->executed_instructions += UINT64_C(4);
        if ((int32_t)cpu->registers[16] <= (int32_t)cpu->registers[20]) {
            break;
        }
    }

    cpu->ip = cpu->registers[30];
    ++cpu->executed_instructions;
    return VF2_OK;
}

static vf2_status copy_cpu_control_table(
    vf2_model2a *machine,
    size_t *bytes_copied
)
{
    uint32_t source = VF2_BOOT_CPU_CONTROL_SOURCE;
    uint32_t destination = VF2_CPU_CONTROL_BASE;
    size_t copied = 0u;
    for (;;) {
        uint32_t value = 0u;
        vf2_status status = vf2_model2a_read_u32(machine, source, &value);
        if (status != VF2_OK) {
            return status;
        }
        if (value == UINT32_C(0xffffffff)) {
            break;
        }
        status = vf2_model2a_write_u32(machine, destination, value);
        if (status != VF2_OK) {
            return status;
        }
        source += 4u;
        destination += 4u;
        copied += 4u;
    }
    *bytes_copied = copied;
    return VF2_OK;
}
static vf2_status copy_machine_bytes(
    vf2_model2a *machine,
    uint32_t destination,
    uint32_t source,
    size_t size
)
{
    uint8_t buffer[16];
    size_t offset = 0u;
    if ((size & 15u) != 0u) {
        return VF2_ERROR_BAD_SIZE;
    }
    for (offset = 0u; offset < size; offset += sizeof(buffer)) {
        vf2_status status = vf2_model2a_read(
            machine,
            source + (uint32_t)offset,
            buffer,
            sizeof(buffer)
        );
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_model2a_write(
            machine,
            destination + (uint32_t)offset,
            buffer,
            sizeof(buffer)
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static vf2_status recover_final_cpu_state(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    size_t index = 0u;
    uint32_t last_quad[4];
    if (cpu == NULL) {
        return VF2_OK;
    }
    const int cold_start =
        cpu->prcb == UINT32_C(0x00003000) && !cpu->reinitialized;

    /* Stage 1 does not reset the i960 register file. Preserve registers that
     * the ROM never writes so this recovery is valid both for power-on boot
     * and for the phase-17 non-returning soft-reset branch. A cold reset has
     * only sp/fp initialized, so preserving untouched registers is also the
     * exact cold-boot behavior. */
    cpu->registers[3] = 0x80000000u;
    cpu->registers[4] = 0x00980000u;
    cpu->registers[14] = 0x00920000u;
    cpu->registers[16] = 0x000000b0u;
    cpu->registers[17] = 0x00003000u;
    cpu->registers[18] = 0x005ff410u;
    cpu->registers[19] = 0xffffffffu;
    cpu->registers[20] = 0x000000b0u;
    cpu->registers[21] = 0xff000010u;
    cpu->registers[22] = 0x00003860u;
    for (index = 0u; index < 4u; ++index) {
        vf2_status status = vf2_model2a_read_u32(
            machine,
            VF2_BOOT_INTERRUPT_SOURCE + 0xa0u + (uint32_t)(index * 4u),
            &last_quad[index]
        );
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[24u + index] = last_quad[index];
    }
    cpu->registers[28] = VF2_BOOT_INTERRUPT_STACK;
    cpu->registers[30] = 0x0000018cu;
    cpu->ip = VF2_BOOT_NEXT_INSTRUCTION;
    if (cold_start) {
        cpu->sat = 0u;
        cpu->prcb = VF2_BOOT_INTERRUPT_DESTINATION;
        cpu->arithmetic_control = UINT32_C(0x00000002);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        cpu->reinitialized = true;
    }
    return VF2_OK;
}

vf2_status vf2_recovered_boot_stage1_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_boot_stage1_report *report
)
{
    vf2_status status = VF2_OK;

    if (machine == NULL || report == NULL || machine->main_rom == NULL ||
        machine->work_ram == NULL || machine->buffer_ram == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(report, 0, sizeof(*report));
    report->cpu_control_source = VF2_BOOT_CPU_CONTROL_SOURCE;
    report->cpu_control_destination = VF2_CPU_CONTROL_BASE;
    report->interrupt_table_source = VF2_BOOT_INTERRUPT_TABLE_SOURCE;
    report->interrupt_stack = VF2_BOOT_INTERRUPT_STACK;
    report->replacement_prcb = VF2_BOOT_INTERRUPT_DESTINATION;
    report->iac_packet = VF2_BOOT_IAC_PACKET;
    report->next_instruction = VF2_BOOT_NEXT_INSTRUCTION;

    status = copy_cpu_control_table(
        machine,
        &report->cpu_control_bytes_copied
    );
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_model2a_write_u32(machine, VF2_VIDEO_CONTROL_BASE, 0x80000000u);
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_recovered_memory_clear_u32(
        machine->work_ram,
        machine->work_ram_size,
        VF2_BOOT_CLEAR_WORK_0_OFFSET,
        VF2_BOOT_CLEAR_WORK_0_WORDS
    );
    if (status != VF2_OK) {
        return status;
    }
    report->work_ram_words_cleared += VF2_BOOT_CLEAR_WORK_0_WORDS;

    status = vf2_recovered_memory_clear_u32(
        machine->work_ram,
        machine->work_ram_size,
        VF2_BOOT_CLEAR_WORK_1_OFFSET,
        VF2_BOOT_CLEAR_WORK_1_WORDS
    );
    if (status != VF2_OK) {
        return status;
    }
    report->work_ram_words_cleared += VF2_BOOT_CLEAR_WORK_1_WORDS;

    status = vf2_recovered_memory_clear_u32(
        machine->buffer_ram,
        machine->buffer_ram_size,
        VF2_BOOT_CLEAR_BUFFER_OFFSET,
        VF2_BOOT_CLEAR_BUFFER_WORDS
    );
    if (status != VF2_OK) {
        return status;
    }
    report->buffer_ram_words_cleared = VF2_BOOT_CLEAR_BUFFER_WORDS;

    status = copy_machine_bytes(
        machine,
        VF2_BOOT_INTERRUPT_STACK,
        VF2_BOOT_INTERRUPT_TABLE_SOURCE,
        VF2_BOOT_INTERRUPT_TABLE_COPY_BYTES
    );
    if (status != VF2_OK) {
        return status;
    }
    status = copy_machine_bytes(
        machine,
        VF2_BOOT_INTERRUPT_DESTINATION,
        VF2_BOOT_INTERRUPT_SOURCE,
        VF2_BOOT_INTERRUPT_COPY_BYTES
    );
    if (status != VF2_OK) {
        return status;
    }
    report->interrupt_state_bytes_copied =
        VF2_BOOT_INTERRUPT_TABLE_COPY_BYTES + VF2_BOOT_INTERRUPT_COPY_BYTES;

    status = vf2_model2a_write_u32(
        machine,
        VF2_BOOT_INTERRUPT_DESTINATION + 0x14u,
        VF2_BOOT_INTERRUPT_STACK
    );
    if (status != VF2_OK) {
        return status;
    }

    return recover_final_cpu_state(machine, cpu);
}

vf2_status vf2_recovered_boot_stage1(
    vf2_model2a *machine,
    vf2_recovered_boot_stage1_report *report
)
{
    return vf2_recovered_boot_stage1_execute(machine, NULL, report);
}
