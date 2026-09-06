#ifndef VF2_RECOVERY_INTERNAL_H
#define VF2_RECOVERY_INTERNAL_H

#include "vf2/hybrid.h"
#include "vf2/model2a.h"

#include <stdint.h>

/* Native runtime scheduler entry is routed through a narrow wrapper so newly
 * recovered initializer tasks can be composed without broadening the generic
 * second-scheduler task whitelist. Other translation units continue to call
 * the public hybrid scheduler directly. */
vf2_status vf2_native_second_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
);

/* The main loop's 0x0000a010 instruction is a single direct i960 CALL to
 * 0x00010d54. Keep that architectural call as its own recovered boundary only
 * for the measured natural scheduler prestate. Synthetic scheduler fixtures
 * and the warm-reboot initializer corridor retain their existing composed
 * semantics. */
static inline vf2_status vf2_native_runtime_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
)
{
    enum {
        VF2_RUNTIME_SCHEDULER_CALL_SITE = 0x0000a010u,
        VF2_RUNTIME_SCHEDULER_BODY = 0x00010d54u,
        VF2_RUNTIME_TASK_COUNT = 0x00011d94u,
        VF2_RUNTIME_TASK_REGISTRY = 0x00510000u,
        VF2_RUNTIME_READY_FLAGS = 0x00500068u,
        VF2_RUNTIME_FLAGS = 0x00508000u,
        VF2_RUNTIME_TIMER1 = 0x00f00004u,
        VF2_RUNTIME_TIMER2 = 0x00f00008u,
        VF2_RUNTIME_INITIALIZER_INDEX = 10u,
        VF2_RUNTIME_INITIALIZER_REGISTRY = 0x00514980u,
        VF2_RUNTIME_INITIALIZER_ENTRY = 0x000221ccu
    };
    const uint32_t runnable_mask = UINT32_C(0x80000000);
    uint32_t task_count = 0u;
    uint32_t ready_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t registry = VF2_RUNTIME_TASK_REGISTRY;
    uint32_t selected_entry = 0u;
    size_t selected_index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_RUNTIME_SCHEDULER_CALL_SITE) {
        return vf2_native_second_scheduler_enter(machine, cpu, report);
    }

    status = vf2_model2a_read_u32(machine, VF2_RUNTIME_TASK_COUNT, &task_count);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_RUNTIME_READY_FLAGS, &ready_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_RUNTIME_FLAGS, &runtime_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_RUNTIME_TIMER1, &timer1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_RUNTIME_TIMER2, &timer2);
    }
    if (status != VF2_OK) {
        return status;
    }

    if (task_count != UINT32_C(29) ||
        cpu->local_frame_depth != 0u ||
        cpu->registers[VF2_I960_FP_REGISTER] != UINT32_C(0x005ff500) ||
        cpu->registers[1] != UINT32_C(0x005ff580) ||
        ready_flags != UINT32_C(0x80004400) ||
        runtime_flags != UINT32_C(0x00008a00) ||
        (timer1 & UINT32_C(0x000fffff)) != UINT32_C(0x000fffff) ||
        (timer2 & UINT32_C(0x000fffff)) != UINT32_C(0x000fffff)) {
        return vf2_native_second_scheduler_enter(machine, cpu, report);
    }

    for (selected_index = 0u; selected_index < task_count; ++selected_index) {
        uint32_t flags = 0u;
        uint32_t stride = 0u;

        status = vf2_model2a_read_u32(machine, registry, &flags);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(8), &stride
            );
        }
        if (status != VF2_OK) {
            return status;
        }
        if (stride == 0u || (stride & UINT32_C(0x1f)) != 0u) {
            return VF2_ERROR_BAD_SIZE;
        }
        if ((flags & runnable_mask) != 0u) {
            status = vf2_model2a_read_u32(
                machine, registry + UINT32_C(0x0c), &selected_entry
            );
            if (status != VF2_OK) {
                return status;
            }
            break;
        }
        registry += stride;
    }

    if (selected_index == VF2_RUNTIME_INITIALIZER_INDEX &&
        registry == VF2_RUNTIME_INITIALIZER_REGISTRY &&
        selected_entry == VF2_RUNTIME_INITIALIZER_ENTRY) {
        return vf2_native_second_scheduler_enter(machine, cpu, report);
    }

    {
        vf2_hybrid_second_scheduler_report local_report = {0};
        const uint64_t start_calls = cpu->procedure_calls;

        status = vf2_i960_cpu_enter_procedure(
            cpu,
            VF2_RUNTIME_SCHEDULER_BODY,
            VF2_RUNTIME_SCHEDULER_CALL_SITE + UINT32_C(4)
        );
        if (status != VF2_OK) {
            return status;
        }
        ++cpu->executed_instructions;

        local_report.registry_start = VF2_RUNTIME_TASK_REGISTRY;
        local_report.scheduler_entry_address = VF2_RUNTIME_SCHEDULER_BODY;
        local_report.recovered_instruction_count = UINT64_C(1);
        local_report.recovered_procedure_calls =
            cpu->procedure_calls - start_calls;
        local_report.cpu_poststate_applied = 1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return VF2_OK;
}
#define vf2_hybrid_second_scheduler_enter vf2_native_runtime_scheduler_enter

/* Native runtime must also pass post-frame bridges through the public recovery
 * wrapper.  The low-level implementation intentionally omits condition and
 * selector-specific poststate reconstruction; calling it directly makes the
 * repeated runtime diverge from the standalone differential bridge. */
static inline vf2_status vf2_native_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    return vf2_hybrid_post_frame_bridge_execute(machine, cpu, report);
}
#define vf2_hybrid_post_frame_bridge_execute vf2_native_post_frame_bridge_execute

static inline vf2_status vf2_recovered_table_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t stride,
    uint32_t count,
    uint16_t *result
)
{
    uint32_t index = 0u;
    uint16_t crc = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || result == NULL || count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < count; ++index) {
        uint8_t raw = 0u;
        uint8_t table_bytes[2] = {0u, 0u};
        uint16_t table_value = 0u;
        const uint16_t high = (uint16_t)((uint32_t)crc << 8u);

        crc = (uint16_t)(crc >> 8u);
        status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
        if (status != VF2_OK) {
            return status;
        }
        source += UINT32_C(1) + stride;
        crc ^= (uint16_t)raw;
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x02000000) +
                (uint32_t)(crc & UINT16_C(0x00ff)) * UINT32_C(2),
            table_bytes,
            sizeof(table_bytes)
        );
        if (status != VF2_OK) {
            return status;
        }
        table_value = (uint16_t)((uint16_t)table_bytes[0] |
                                 ((uint16_t)table_bytes[1] << 8u));
        crc = (uint16_t)(table_value ^ high);
    }
    *result = crc;
    return VF2_OK;
}

#endif
