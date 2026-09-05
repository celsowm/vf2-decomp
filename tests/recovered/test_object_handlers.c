#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define HANDLER_RETURN UINT32_C(0x00010dcc)
#define REGISTRY_BASE UINT32_C(0x00513000)
#define REGISTRY_STRIDE UINT32_C(0x200)
#define STACK_POINTER (VF2_WORK_RAM_BASE + UINT32_C(0x3000))

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__, __LINE__, #expression                     \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

typedef struct handler_case {
    const char *name;
    uint32_t entry;
    uint8_t instance;
    uint32_t expected_mem0c;
    uint64_t expected_instructions;
} handler_case;

/* Reference poststate measured with the ROM-backed executor from synthetic
 * state (vf2_object_handler_tests ROM_DIR, 2026-09-05):
 * - init stubs rewrite registry+0x0c to their ret continuation, 3 insns;
 * - bare ret continuations leave memory alone, 1 insn;
 * - condition state, global r15 and all other RAM are preserved. */
static const handler_case cases[] = {
    {"dispatcher", UINT32_C(0x0006ca64), 0u, UINT32_C(0x0006cae0), 4u},
    {"init0", UINT32_C(0x0006cae0), 0u, UINT32_C(0x0006caf0), 3u},
    {"init0-ret", UINT32_C(0x0006caf0), 0u, UINT32_C(0xdeadbeef), 1u},
    {"init1", UINT32_C(0x0006caf4), 1u, UINT32_C(0x0006cb04), 3u},
    {"init1-ret", UINT32_C(0x0006cb04), 1u, UINT32_C(0xdeadbeef), 1u},
    {"init2-ret", UINT32_C(0x0006cb08), 2u, UINT32_C(0xdeadbeef), 1u},
};

static vf2_status write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static void setup_state(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const handler_case *test_case,
    uint32_t registry
)
{
    memset(cpu, 0, sizeof(*cpu));
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00000000));
    cpu->registers[1] = STACK_POINTER;
    CHECK(
        vf2_i960_cpu_enter_procedure(
            cpu, test_case->entry, HANDLER_RETURN
        ) == VF2_OK
    );
    cpu->registers[29] = registry;
    /* Sentinels prove condition state survives. Local r15 is the callee
     * scratch register: lda targets it and ret discards the callee frame,
     * so the reference leaves the caller-frame value (zero) behind. */
    cpu->arithmetic_control |= UINT32_C(5);
    cpu->compare_result = VF2_I960_COMPARE_OVERFLOW;

    CHECK(
        vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x00), UINT32_C(0x11111111)
        ) == VF2_OK
    );
    CHECK(
        write_u8(machine, registry + UINT32_C(0x04), test_case->instance) ==
        VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x0c), UINT32_C(0xdeadbeef)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x90), UINT32_C(0x22222222)
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write_u32(
            machine, registry + UINT32_C(0x13c), UINT32_C(0x33333333)
        ) == VF2_OK
    );
}

static void check_cpu_equal(
    const vf2_i960_cpu *expected,
    const vf2_i960_cpu *actual,
    const char *name
)
{
    size_t index = 0u;

    (void)name;
    CHECK(actual->ip == expected->ip);
    CHECK(actual->local_frame_depth == expected->local_frame_depth);
    CHECK(
        actual->executed_instructions == expected->executed_instructions
    );
    CHECK(actual->procedure_calls == expected->procedure_calls);
    CHECK(actual->procedure_returns == expected->procedure_returns);
    CHECK(actual->arithmetic_control == expected->arithmetic_control);
    CHECK(actual->compare_result == expected->compare_result);
    for (index = 0u; index < 32u; ++index) {
        CHECK(actual->registers[index] == expected->registers[index]);
    }
}

static void run_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const handler_case *test_case,
    uint32_t registry
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_hybrid_task_report report;
    uint32_t mem0c = 0u;
    uint32_t steps = 0u;
    vf2_status status = VF2_OK;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    CHECK(vf2_model2a_initialize(&reference_machine) != 0);
    CHECK(vf2_model2a_initialize(&native_machine) != 0);
    if (reference_machine.work_ram == NULL ||
        native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_rom(
            &reference_machine, main_rom, main_rom_size
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_rom(&native_machine, main_rom, main_rom_size) ==
        VF2_OK
    );

    setup_state(&reference_machine, &reference_cpu, test_case, registry);
    setup_state(&native_machine, &native_cpu, test_case, registry);

    while (reference_cpu.ip != HANDLER_RETURN && steps < 16u) {
        status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == HANDLER_RETURN);
    CHECK(steps == test_case->expected_instructions);
    CHECK(
        (reference_cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(5)
    );
    CHECK(reference_cpu.compare_result == VF2_I960_COMPARE_OVERFLOW);
    CHECK(reference_cpu.registers[15] == 0u);
    CHECK(
        vf2_model2a_read_u32(
            &reference_machine, registry + UINT32_C(0x0c), &mem0c
        ) == VF2_OK
    );
    CHECK(mem0c == test_case->expected_mem0c);

    memset(&report, 0, sizeof(report));
    status = vf2_hybrid_first_dispatch_task_execute(
        &native_machine, &native_cpu, registry, &report
    );
    CHECK(status == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_TASK_OBJECT);
    CHECK(report.exit_address == HANDLER_RETURN);
    CHECK(
        report.recovered_instruction_count ==
        test_case->expected_instructions
    );
    CHECK(report.recovered_procedure_calls == 0u);
    CHECK(report.recovered_procedure_returns == 1u);

    check_cpu_equal(&reference_cpu, &native_cpu, test_case->name);
    CHECK(
        memcmp(
            reference_machine.work_ram, native_machine.work_ram,
            reference_machine.work_ram_size
        ) == 0
    );

    printf(
        "%-12s entry=0x%08x exact: %llu ins, mem0c=0x%08x\n",
        test_case->name, (unsigned)test_case->entry,
        (unsigned long long)test_case->expected_instructions,
        (unsigned)test_case->expected_mem0c
    );

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

static void test_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_task_report report;
    const uint32_t registry = REGISTRY_BASE;

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    memset(&cpu, 0, sizeof(cpu));
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0006cae0));
    cpu.registers[1] = STACK_POINTER;
    CHECK(
        vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0006cae0),
                                     HANDLER_RETURN) == VF2_OK
    );
    cpu.registers[29] = registry;

    memset(&report, 0, sizeof(report));
    CHECK(
        vf2_hybrid_first_dispatch_task_execute(
            NULL, &cpu, registry, &report
        ) == VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_hybrid_first_dispatch_task_execute(
            &machine, NULL, registry, &report
        ) == VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_hybrid_first_dispatch_task_execute(
            &machine, &cpu, registry + UINT32_C(0x10), &report
        ) == VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_hybrid_first_dispatch_task_execute(
            &machine, &cpu, registry, NULL
        ) == VF2_OK
    );

    vf2_model2a_shutdown(&machine);
}

int main(int argc, char **argv)
{
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;
    size_t index = 0u;

    test_invalid_arguments();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(
                stderr, "%d object-handler test(s) failed\n", failures
            );
            return EXIT_FAILURE;
        }
        puts("object-handler ROM-independent tests passed");
        return EXIT_SUCCESS;
    }
    CHECK(
        vf2_romset_build_region(
            argv[1], VF2_REGION_MAINCPU, &main_rom, &main_rom_size
        ) == VF2_OK
    );
    if (main_rom == NULL) {
        return EXIT_FAILURE;
    }

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        run_case(
            main_rom, main_rom_size, &cases[index],
            REGISTRY_BASE + (uint32_t)index * REGISTRY_STRIDE
        );
    }
    free(main_rom);

    if (failures != 0) {
        fprintf(
            stderr, "%d object-handler differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("object-handler differential tests passed");
    return EXIT_SUCCESS;
}
