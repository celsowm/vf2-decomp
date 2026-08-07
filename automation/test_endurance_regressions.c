#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void enter_parent(vf2_i960_cpu *cpu, uint32_t target)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu,
            target,
            UINT32_C(0x00001004)
        ) == VF2_OK
    );
}

static void set_greater(vf2_i960_cpu *cpu)
{
    cpu->arithmetic_control = UINT32_C(0x3f001001);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
}

static void test_inactive_palette_upload_preserves_condition(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00546000),
            0u
        ) == VF2_OK
    );
    enter_parent(&cpu, UINT32_C(0x00002de4));
    set_greater(&cpu);

    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine,
            &cpu,
            &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD);
    CHECK(report.entry_address == UINT32_C(0x00002de4));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001001));

    vf2_model2a_shutdown(&machine);
}

static void test_frame_timer_zero_modulo_counter_path(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_bridge_report report;
    uint32_t stored_minimum = UINT32_C(0xffffffff);
    uint8_t zero = 0u;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00f00008),
            UINT32_C(0x000fffff)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00f0000c),
            UINT32_C(0x0007a120)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x00500000),
            &zero,
            sizeof(zero)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00500020),
            UINT32_C(32)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            &machine,
            UINT32_C(0x00500160),
            UINT32_C(0x12345678)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine,
            UINT32_C(0x0050006d),
            &zero,
            sizeof(zero)
        ) == VF2_OK
    );

    enter_parent(&cpu, UINT32_C(0x00010f08));
    set_greater(&cpu);
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine,
            &cpu,
            &report
        ) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX);
    CHECK(report.entry_address == UINT32_C(0x00010f08));
    CHECK(report.exit_address == UINT32_C(0x00010f90));
    CHECK(report.recovered_instruction_count == UINT64_C(21));
    CHECK(cpu.ip == UINT32_C(0x00010f90));
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001001));
    CHECK(
        vf2_model2a_read_u32(
            &machine,
            UINT32_C(0x00500160),
            &stored_minimum
        ) == VF2_OK
    );
    CHECK(stored_minimum == cpu.registers[5]);

    vf2_model2a_shutdown(&machine);
}

int main(void)
{
    test_inactive_palette_upload_preserves_condition();
    test_frame_timer_zero_modulo_counter_path();

    if (failures != 0) {
        fprintf(stderr, "%d endurance regression test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("endurance regression tests passed");
    return EXIT_SUCCESS;
}
