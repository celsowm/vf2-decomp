/* ROM-backed differential fixture for the fa_coli 0x22404 contact query
 * on the live first-contact shape (v0359).
 *
 * Witness (measured, current build): the first 0x22404 call of the
 * coli-live-midbody-g01 trace (trace idx 29..106):
 *   78 steps 0x22404 -> ret 0x225b0 (body 77 + ret), g0 = 1.
 * Reproduce the entry from the midbody park:
 *   vf2probe --snapshot out/coli-midbody-22210.vf2snap \
 *     --set-ip 0x22210 \
 *     --set-reg g7=0x510980 --set-reg g8=0x512980 \
 *     --set-reg g13=0x514940 \
 *     --set-u32 0x510b24=0x100 \
 *     --set-u8  0x5111a0=0x01 \
 *     --set-u32 0x5149cc=0xffff \
 *     --until 0x22404            -> first call entry
 *
 * Live entry shape (measured at 0x22404, 0 steps):
 *   slot (g7+0x4) = 0, old snap (g13+0x8c) = 0xffff (stale),
 *   snap (g7+0x1a8) = 0, flags (g7+0x1a4) = 0x100 (bit 8),
 *   pending (g13+0x90) = 0, thr (g7+0x1aa / g7+0x808) = 0/0,
 *   field_5b8 = 0, index (g7+0x820) = 1, table (g13+0x4c) = 0x221e8,
 *   exclude (g8+0x6dc) = 0, delta (g8+0x26) = 0, coords = 0.
 *   The stale slot falls through cmpobe to bal 0x225bc (pending
 *   slot-clear, +5 vs the equal path) and rejoins at 0x22434.
 *
 * The work-RAM seed below is the exact initial-read set of the
 * reference body trace: every address the body reads before writing
 * it. All other RAM is zero. ROM (0x000232e3) and main_data
 * (0x02007ace) addresses are served by the attached real images and
 * are listed here only for provenance:
 *   0x000232E3=0x00, 0x02007ACE=0x00000008.
 * The 0x0090fb80 FIFO triple lives in buffer RAM (base-model stub,
 * identical on both sides). Registers/frames follow the existing
 * unit-test convention (fresh procedure frame); the differential
 * compares native against reference on that identical construction,
 * so any frame approximation affects both sides equally and a
 * divergence still fails closed.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define COLI_22404_ENTRY UINT32_C(0x00022404)
#define COLI_22404_RETURN UINT32_C(0x0002222c)
#define COLI_22404_FIGHTER0 UINT32_C(0x00510980)
#define COLI_22404_FIGHTER1 UINT32_C(0x00512980)
#define COLI_22404_REGISTRY UINT32_C(0x00514940)

static int failures = 0;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            fprintf(                                                          \
                stderr,                                                       \
                "FAILED %s:%d: %s\n",                                         \
                __FILE__, __LINE__, #expression                               \
            );                                                                \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

typedef struct coli_seed {
    uint32_t address;
    uint32_t value;
    uint32_t size;
} coli_seed;

/* Exact initial-read set of the reference body trace (see header). */
static const coli_seed COLI_22404_SEEDS[] = {
    { 0x00510984u, 0x00000000u, 1u },
    { 0x00510B24u, 0x00000100u, 4u },
    { 0x00510B28u, 0x00000000u, 2u },
    { 0x00510B2Au, 0x00000000u, 2u },
    { 0x00510F38u, 0x00000000u, 4u },
    { 0x00511188u, 0x00000000u, 2u },
    { 0x005111A0u, 0x00000001u, 1u },
    { 0x00512998u, 0x00000000u, 4u },
    { 0x0051299Cu, 0x00000000u, 4u },
    { 0x005129A0u, 0x00000000u, 4u },
    { 0x005129A6u, 0x00000000u, 2u },
    { 0x0051305Cu, 0x00000000u, 2u },
    { 0x0051498Cu, 0x000221E8u, 4u },
    { 0x005149CCu, 0x0000FFFFu, 2u },
    { 0x005149D0u, 0x00000000u, 2u },
};

static vf2_status write_seed_bytes(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t value,
    uint32_t size
)
{
    uint8_t bytes[4];
    size_t index = 0u;

    if (size == 0u || size > sizeof(bytes)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < size; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8u));
    }
    return vf2_model2a_write(machine, address, bytes, (size_t)size);
}

static vf2_status seed_live_entry(vf2_model2a *machine)
{
    size_t index = 0u;

    for (index = 0u;
         index < sizeof(COLI_22404_SEEDS) / sizeof(COLI_22404_SEEDS[0]);
         ++index) {
        const vf2_status status = write_seed_bytes(
            machine,
            COLI_22404_SEEDS[index].address,
            COLI_22404_SEEDS[index].value,
            COLI_22404_SEEDS[index].size
        );

        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static void setup_live_cpu(vf2_i960_cpu *cpu)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, COLI_22404_ENTRY);
    cpu->registers[0] = UINT32_C(0x005FF700);
    cpu->registers[1] = UINT32_C(0x005FF780);
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = COLI_22404_FIGHTER0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = COLI_22404_FIGHTER1;
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu->registers[VF2_I960_G0_REGISTER + 13u] = COLI_22404_REGISTRY;
    cpu->registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005FF740);
}

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(
        vf2_hybrid_coli_contact_query_execute(NULL, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_hybrid_coli_contact_query_execute(&machine, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    /* Wrong entry IP and zero frame depth both refuse. */
    CHECK(
        vf2_hybrid_coli_contact_query_execute(&machine, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    uint64_t reference_instructions = 0u;
    uint64_t native_instructions = 0u;
    uint32_t steps = 0u;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&diff, 0, sizeof(diff));

    CHECK(vf2_model2a_initialize(&reference_machine));
    CHECK(vf2_model2a_initialize(&native_machine));
    if (reference_machine.work_ram == NULL ||
        native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_rom(&reference_machine, main_rom,
                                    main_rom_size) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_rom(&native_machine, main_rom,
                                    main_rom_size) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_data(&reference_machine, main_data,
                                     main_data_size) == VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_data(&native_machine, main_data,
                                     main_data_size) == VF2_OK
    );
    CHECK(seed_live_entry(&reference_machine) == VF2_OK);
    CHECK(seed_live_entry(&native_machine) == VF2_OK);

    setup_live_cpu(&reference_cpu);
    setup_live_cpu(&native_cpu);
    CHECK(
        vf2_i960_cpu_enter_procedure(&reference_cpu, COLI_22404_ENTRY,
                                     COLI_22404_RETURN) == VF2_OK
    );
    CHECK(
        vf2_i960_cpu_enter_procedure(&native_cpu, COLI_22404_ENTRY,
                                     COLI_22404_RETURN) == VF2_OK
    );

    /* Reference: step the real body to its return. */
    reference_instructions = reference_cpu.executed_instructions;
    while (reference_cpu.ip != COLI_22404_RETURN && steps < 1024u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == COLI_22404_RETURN);
    CHECK(steps == 78u);
    CHECK(reference_cpu.registers[VF2_I960_G0_REGISTER] == 1u);
    reference_instructions =
        reference_cpu.executed_instructions - reference_instructions;

    /* Native: recovered contact query. */
    native_instructions = native_cpu.executed_instructions;
    native_status =
        vf2_hybrid_coli_contact_query_execute(&native_machine, &native_cpu);
    native_instructions =
        native_cpu.executed_instructions - native_instructions;
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == COLI_22404_RETURN);

    /* Instruction-count lockstep on the live shape. */
    CHECK(native_instructions == reference_instructions);

    /* Full live-state equality (registers, CC/AC, frames, memory). */
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "coli-22404-live ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

static void run_rom_differential(const char *rom_directory)
{
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;

    CHECK(
        vf2_romset_build_region(
            rom_directory, VF2_REGION_MAINCPU, &main_rom, &main_rom_size
        ) == VF2_OK
    );
    CHECK(
        vf2_romset_build_region(
            rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size
        ) == VF2_OK
    );
    if (main_rom == NULL || main_data == NULL) {
        free(main_rom);
        free(main_data);
        ++failures;
        return;
    }

    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(stderr, "%d coli-22404-live unit test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("coli-22404-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d coli-22404-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("coli-22404-live differential tests passed");
    return EXIT_SUCCESS;
}
