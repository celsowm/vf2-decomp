#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#include <stdint.h>

#define VF2_OBJECT_SERVICE_ENTRY UINT32_C(0x0006ca84)
#define VF2_OBJECT_SERVICE_LOOP_RETURN UINT32_C(0x0006cab8)
#define VF2_OBJECT_SERVICE_COUNT UINT32_C(3)
#define VF2_OBJECT_SERVICE_SLOT0 UINT32_C(0x00500878)
#define VF2_OBJECT_SERVICE_SLOT1 UINT32_C(0x0050087c)
#define VF2_OBJECT_SERVICE_SLOT2 UINT32_C(0x00500880)
#define VF2_OBJECT_HANDLER0 UINT32_C(0x0006cae0)
#define VF2_OBJECT_HANDLER0_NEXT UINT32_C(0x0006caf0)
#define VF2_OBJECT_HANDLER1 UINT32_C(0x0006caf4)
#define VF2_OBJECT_HANDLER1_NEXT UINT32_C(0x0006cb04)
#define VF2_OBJECT_HANDLER2 UINT32_C(0x0006cb08)

static void object_service_set_compare(
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

static vf2_status object_service_execute_handler(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t registry
)
{
    uint32_t next_entry = 0u;
    uint64_t body_instructions = 0u;
    vf2_status status = VF2_OK;

    switch (entry) {
    case VF2_OBJECT_HANDLER0:
        next_entry = VF2_OBJECT_HANDLER0_NEXT;
        body_instructions = UINT64_C(2);
        break;
    case VF2_OBJECT_HANDLER1:
        next_entry = VF2_OBJECT_HANDLER1_NEXT;
        body_instructions = UINT64_C(2);
        break;
    case VF2_OBJECT_HANDLER0_NEXT:
    case VF2_OBJECT_HANDLER1_NEXT:
    case VF2_OBJECT_HANDLER2:
        body_instructions = UINT64_C(0);
        break;
    default:
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, entry, VF2_OBJECT_SERVICE_LOOP_RETURN
    );
    if (status != VF2_OK) {
        return status;
    }
    if (next_entry != 0u) {
        cpu->registers[15] = next_entry;
        status = vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x0c), next_entry
        );
    }
    if (status == VF2_OK) {
        cpu->executed_instructions += body_instructions;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    return status;
}

vf2_status vf2_recovered_object_service_loop_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    static const uint32_t slots[VF2_OBJECT_SERVICE_COUNT] = {
        VF2_OBJECT_SERVICE_SLOT0,
        VF2_OBJECT_SERVICE_SLOT1,
        VF2_OBJECT_SERVICE_SLOT2
    };
    const uint32_t saved_sp = cpu != NULL ? cpu->registers[1] : 0u;
    const uint32_t saved_g13 = cpu != NULL ? cpu->registers[29] : 0u;
    uint32_t count = VF2_OBJECT_SERVICE_COUNT;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_OBJECT_SERVICE_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    cpu->registers[1] = saved_sp + UINT32_C(4);
    status = vf2_model2a_write_u32(machine, saved_sp, saved_g13);
    if (status != VF2_OK) {
        cpu->registers[1] = saved_sp;
        return status;
    }
    cpu->registers[6] = count;
    cpu->executed_instructions += UINT64_C(3);

    while (status == VF2_OK && count != 0u) {
        uint32_t slot_value = 0u;
        uint32_t registry = 0u;
        uint32_t flags = 0u;
        uint32_t entry = 0u;
        const uint32_t index = count - UINT32_C(1);

        cpu->registers[5] = index;
        cpu->executed_instructions += UINT64_C(1);
        cpu->registers[5] = slots[index];
        status = vf2_model2a_read_u32(machine, slots[index], &slot_value);
        if (status != VF2_OK) {
            break;
        }
        registry = slot_value;
        cpu->registers[29] = registry;
        status = vf2_model2a_read_u32(machine, registry, &flags);
        if (status != VF2_OK) {
            break;
        }
        cpu->registers[14] = flags;
        cpu->executed_instructions += UINT64_C(4);

        if ((flags & UINT32_C(0x80000000)) != 0u) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(0x0c), &entry
            );
            if (status != VF2_OK) {
                break;
            }
            cpu->registers[5] = entry;
            cpu->executed_instructions += UINT64_C(2);
            status = object_service_execute_handler(
                machine, cpu, entry, registry
            );
            if (status != VF2_OK) {
                break;
            }
        }

        object_service_set_compare(
            cpu,
            count == UINT32_C(1)
                ? VF2_I960_COMPARE_EQUAL
                : VF2_I960_COMPARE_LESS
        );
        --count;
        cpu->registers[6] = count;
        cpu->executed_instructions += UINT64_C(2);
    }

    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[29] = saved_g13;
    status = vf2_model2a_read_u32(machine, saved_sp, &cpu->registers[29]);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[1] = saved_sp;
    cpu->executed_instructions += UINT64_C(2);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    return status;
}
