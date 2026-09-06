#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define IAC_COPY_ENTRY UINT32_C(0x00000b58)
#define IAC_FIRST_LIMIT UINT32_C(0x00000404)
#define IAC_FIRST_SOURCE UINT32_C(0x00003a80)
#define IAC_FIRST_DESTINATION UINT32_C(0x005ff000)
#define IAC_FIRST_RETURN UINT32_C(0x00000170)
#define IAC_SECOND_LIMIT UINT32_C(0x000000b0)
#define IAC_SECOND_SOURCE UINT32_C(0x00003000)
#define IAC_SECOND_DESTINATION UINT32_C(0x005ff410)
#define IAC_SECOND_RETURN UINT32_C(0x0000018c)

vf2_status vf2_recovered_iac_reinitialize_copy_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr, "FAILED %s:%d: %s\n",                     \
                __FILE__, __LINE__, #expression                     \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

typedef struct iac_copy_case {
    const char *name;
    uint32_t limit;
    uint32_t source;
    uint32_t destination;
    uint32_t return_address;
    uint32_t expected_final_offset;
    uint32_t expected_condition_bits;
    vf2_i960_compare_result expected_compare;
    uint64_t expected_instructions;
} iac_copy_case;

static const iac_copy_case cases[] = {
    {
        "interrupt-table",
        IAC_FIRST_LIMIT,
        IAC_FIRST_SOURCE,
        IAC_FIRST_DESTINATION,
        IAC_FIRST_RETURN,
        UINT32_C(0x00000410),
        UINT32_C(4),
        VF2_I960_COMPARE_LESS,
        UINT64_C(261)
    },
    {
        "interrupt-state",
        IAC_SECOND_LIMIT,
        IAC_SECOND_SOURCE,
        IAC_SECOND_DESTINATION,
        IAC_SECOND_RETURN,
        UINT32_C(0x000000b0),
        UINT32_C(2),
        VF2_I960_COMPARE_EQUAL,
        UINT64_C(45)
    }
};

static void setup_cpu(vf2_i960_cpu *cpu, const iac_copy_case *test_case)
{
    memset(cpu, 0, sizeof(*cpu));
    vf2_i960_cpu_reset(cpu, 0u, 0u, 0u);
    cpu->ip = IAC_COPY_ENTRY;
    cpu->registers[16] = test_case->limit;
    cpu->registers[17] = test_case->source;
    cpu->registers[18] = test_case->destination;
    cpu->registers[20] = 0u;
    cpu->registers[30] = test_case->return_address;
    cpu->arithmetic_control = UINT32_C(0xa5a50005);
    cpu->compare_result = VF2_I960_COMPARE_OVERFLOW;
    cpu->executed_instructions = UINT64_C(17);
    cpu->procedure_calls = UINT64_C(3);
    cpu->procedure_returns = UINT64_C(2);
}

static void check_cpu_equal(
    const vf2_i960_cpu *expected,
    const vf2_i960_cpu *actual
)
{
    size_t index = 0u;

    CHECK(actual->ip == expected->ip);
    CHECK(actual->sat == expected->sat);
    CHECK(actual->prcb == expected->prcb);
    CHECK(actual->process_control == expected->process_control);
    CHECK(actual->arithmetic_control == expected->arithmetic_control);
    CHECK(actual->compare_result == expected->compare_result);
    CHECK(actual->executed_instructions == expected->executed_instructions);
    CHECK(actual->procedure_calls == expected->procedure_calls);
    CHECK(actual->procedure_returns == expected->procedure_returns);
    CHECK(actual->local_frame_depth == expected->local_frame_depth);
    CHECK(actual->maximum_local_frame_depth == expected->maximum_local_frame_depth);
    for (index = 0u; index < 32u; ++index) {
        CHECK(actual->registers[index] == expected->registers[index]);
    }
}

static void run_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const iac_copy_case *test_case
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_status status = VF2_OK;
    uint64_t start_instructions = 0u;
    uint32_t steps = 0u;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    CHECK(vf2_model2a_initialize(&reference_machine) != 0);
    CHECK(vf2_model2a_initialize(&native_machine) != 0);
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
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
        vf2_model2a_attach_main_rom(
            &native_machine, main_rom, main_rom_size
        ) == VF2_OK
    );

    setup_cpu(&reference_cpu, test_case);
    setup_cpu(&native_cpu, test_case);
    start_instructions = reference_cpu.executed_instructions;

    while (reference_cpu.ip != test_case->return_address && steps < 300u) {
        status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) {
            break;
        }
    }

    CHECK(reference_cpu.ip == test_case->return_address);
    CHECK(
        reference_cpu.executed_instructions - start_instructions ==
        test_case->expected_instructions
    );
    CHECK((uint64_t)steps == test_case->expected_instructions);
    CHECK(reference_cpu.registers[20] == test_case->expected_final_offset);
    CHECK(
        (reference_cpu.arithmetic_control & UINT32_C(7)) ==
        test_case->expected_condition_bits
    );
    CHECK(
        (reference_cpu.arithmetic_control & ~UINT32_C(7)) ==
        (UINT32_C(0xa5a50005) & ~UINT32_C(7))
    );
    CHECK(reference_cpu.compare_result == test_case->expected_compare);
    CHECK(reference_cpu.procedure_calls == UINT64_C(3));
    CHECK(reference_cpu.procedure_returns == UINT64_C(2));

    status = vf2_recovered_iac_reinitialize_copy_execute(
        &native_machine, &native_cpu
    );
    CHECK(status == VF2_OK);
    check_cpu_equal(&reference_cpu, &native_cpu);
    CHECK(
        memcmp(
            reference_machine.work_ram,
            native_machine.work_ram,
            reference_machine.work_ram_size
        ) == 0
    );

    printf(
        "%-16s exact: %llu ins, final g4=0x%08x, cc=%u\n",
        test_case->name,
        (unsigned long long)test_case->expected_instructions,
        (unsigned)test_case->expected_final_offset,
        (unsigned)test_case->expected_condition_bits
    );

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

static void test_fail_closed(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    iac_copy_case altered = cases[0];

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    setup_cpu(&cpu, &cases[0]);

    CHECK(
        vf2_recovered_iac_reinitialize_copy_execute(NULL, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_recovered_iac_reinitialize_copy_execute(&machine, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );

    cpu.ip = IAC_COPY_ENTRY + UINT32_C(4);
    CHECK(
        vf2_recovered_iac_reinitialize_copy_execute(&machine, &cpu) ==
        VF2_ERROR_UNSUPPORTED
    );

    altered.limit = IAC_FIRST_LIMIT + UINT32_C(16);
    setup_cpu(&cpu, &altered);
    CHECK(
        vf2_recovered_iac_reinitialize_copy_execute(&machine, &cpu) ==
        VF2_ERROR_UNSUPPORTED
    );

    vf2_model2a_shutdown(&machine);
}

int main(int argc, char **argv)
{
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;
    size_t index = 0u;

    test_fail_closed();
    if (argc != 2) {
        if (failures != 0) {
            fprintf(stderr, "%d IAC copy test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("IAC copy ROM-independent tests passed");
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
        run_case(main_rom, main_rom_size, &cases[index]);
    }
    free(main_rom);

    if (failures != 0) {
        fprintf(stderr, "%d IAC copy differential test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("IAC copy differential tests passed");
    return EXIT_SUCCESS;
}
