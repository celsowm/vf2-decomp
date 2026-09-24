/* ROM-backed differential fixture for the fa_coli 0x225cc long body on
 * the live midbody witness shape (v0350/v0381).
 *
 * Witness (measured, current build):
 *   vf2probe --snapshot out/coli-midbody-22210.vf2snap \
 *     --set-ip 0x22210 \
 *     --set-reg g7=0x510980 --set-reg g8=0x512980 \
 *     --set-reg g13=0x514940 \
 *     --set-u32 0x510b24=0x100 \
 *     --set-u8  0x5111a0=0x01 \
 *     --set-u32 0x5149cc=0xffff \
 *     --until 0x225cc            -> 130 steps to 0x225cc
 *     --until 0x10dcc            -> 371 steps, 0x225cc x1
 *   (Redundant on the park, verified droppable: --set-ip, g7, g8,
 *   0x5149cc. Essential: 0x510b24 bit 8, 0x5111a0 byte. g13 only
 *   shortens the path 130 -> 125.)
 *
 * Live entry shape (measured at 0x225cc, 0 steps):
 *   g8+0x1a4 = 0, g7+0x1a4 = 0x100 (bit 8), g7+0x821 = 0,
 *   g8+0x821 = 0, g8+0x19f = 0 (!= 22: no 0x18bd4 shortcut),
 *   g8+0x5b8 = 0, g7+0x822 = 0 (r11 miss shape: be taken at
 *   0x22624, scanbit pack skipped), g7+0x844 = 0.
 *   Body: 240 steps 0x225cc -> 0x22294, 4 nested calls
 *   (0x230d4 x1, 0x23238 x2, 0x1ab34 x1).
 *
 * The work-RAM seed below is the exact initial-read set of the
 * reference memory trace (100 bytes): every address the body reads
 * before writing it. All other RAM is zero. Registers/frames follow
 * the existing unit-test convention (fresh procedure frame); the
 * differential compares native against reference on that identical
 * construction, so any frame approximation affects both sides equally
 * and a divergence still fails closed.
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

#define COLI_LIVE_ENTRY UINT32_C(0x000225cc)
#define COLI_LIVE_RETURN UINT32_C(0x00022294)
#define COLI_LIVE_FIGHTER0 UINT32_C(0x00510980)
#define COLI_LIVE_FIGHTER1 UINT32_C(0x00512980)

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

/* Exact initial-read set of the reference body trace (see header).
 * ROM (0x000xxxxx) and main_data (0x020xxxxx) addresses are served by
 * the attached real images and are listed here only for provenance:
 *   0x0001B7F9=0x0e, 0x00023270=0x3b23d70a,
 *   0x0200E5FC=0x0200ee55, 0x0200EE5D=0x03, 0x0200EE6B=0x08,
 *   0x0201CCFC=0x0201cd74, 0x0201CDFC=0x000004ac. */
static const coli_seed COLI_LIVE_SEEDS[] = {
    { 0x00500028u, 0x00000000u, 2u },
    { 0x00500068u, 0x80004400u, 4u },
    { 0x0050016Cu, 0x00599000u, 4u },
    { 0x00508000u, 0x00008A00u, 4u },
    { 0x0050A0B4u, 0x00003600u, 2u },
    { 0x0050A16Eu, 0x00000001u, 1u },
    { 0x0050A800u, 0x3F800000u, 4u },
    { 0x0050A808u, 0x3F800000u, 4u },
    { 0x0050A818u, 0x00002000u, 2u },
    { 0x00510980u, 0x04000000u, 4u },
    { 0x005109A6u, 0x00000000u, 2u },
    { 0x00510B24u, 0x00000100u, 4u },
    { 0x00510B30u, 0x00000000u, 1u },
    { 0x005111A1u, 0x00000000u, 2u },
    { 0x005111A8u, 0x00000000u, 4u },
    { 0x005111C3u, 0x00000000u, 1u },
    { 0x00511BA4u, 0x00000000u, 2u },
    { 0x00511BB4u, 0x00000000u, 4u },
    { 0x00512980u, 0x04000000u, 4u },
    { 0x00512B1Fu, 0x00000000u, 1u },
    { 0x00512B24u, 0x00000000u, 4u },
    { 0x00512B2Cu, 0x00000000u, 2u },
    { 0x00512F34u, 0x00000000u, 2u },
    { 0x00512F58u, 0x00000000u, 4u },
    { 0x00513054u, 0x000021E8u, 2u },
    { 0x00513058u, 0x00000000u, 2u },
    { 0x00513080u, 0x00000000u, 4u },
    { 0x00513184u, 0x00000000u, 4u },
    { 0x0059C351u, 0x00000000u, 1u },
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
         index < sizeof(COLI_LIVE_SEEDS) / sizeof(COLI_LIVE_SEEDS[0]);
         ++index) {
        const vf2_status status = write_seed_bytes(
            machine,
            COLI_LIVE_SEEDS[index].address,
            COLI_LIVE_SEEDS[index].value,
            COLI_LIVE_SEEDS[index].size
        );

        if (status != VF2_OK) {
            return status;
        }
    }
    return VF2_OK;
}

static void setup_live_cpu(vf2_i960_cpu *cpu)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, COLI_LIVE_ENTRY);
    cpu->registers[0] = UINT32_C(0x005FF700);
    cpu->registers[1] = UINT32_C(0x005FF780);
    cpu->registers[18] = UINT32_C(0x00000644);
    cpu->registers[VF2_I960_G0_REGISTER + 7u] = COLI_LIVE_FIGHTER0;
    cpu->registers[VF2_I960_G0_REGISTER + 8u] = COLI_LIVE_FIGHTER1;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000550);
    cpu->registers[VF2_I960_G0_REGISTER + 10u] = UINT32_C(0x00800000);
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu->registers[VF2_I960_G0_REGISTER + 13u] = UINT32_C(0x00514940);
    cpu->registers[VF2_I960_G0_REGISTER + 14u] = UINT32_C(0x00022428);
    cpu->registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005FF740);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(0x04);
}

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(
        vf2_hybrid_coli_225cc_execute(NULL, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_hybrid_coli_225cc_execute(&machine, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    /* Wrong entry IP and zero frame depth both refuse. */
    CHECK(
        vf2_hybrid_coli_225cc_execute(&machine, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    int bit16_scan4
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result run_result;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    uint64_t reference_instructions = 0u;
    uint64_t native_instructions = 0u;
    uint32_t steps = 0u;
    uint32_t g7_flags = UINT32_C(0x00000100);
    uint32_t g8_flags = UINT32_C(0x00010000);

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&options, 0, sizeof(options));
    memset(&run_result, 0, sizeof(run_result));
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
    if (bit16_scan4 != 0) {
        if (bit16_scan4 == 2) {
            g7_flags = UINT32_C(0x00400100);
        } else if (bit16_scan4 == 3) {
            /* v0453: bit-22 set with the seed's unrelated low bit clear. */
            g7_flags = UINT32_C(0x00400000);
        } else if (bit16_scan4 == 4) {
            /* v0453: bit-22 plus low bit 0. */
            g7_flags = UINT32_C(0x00400001);
        } else if (bit16_scan4 == 5) {
            /* v0453: bit-22 plus bits 8 and 16. */
            g7_flags = UINT32_C(0x00410100);
        } else if (bit16_scan4 == 6) {
            /* v0453: bit-22 plus bit 23. */
            g7_flags = UINT32_C(0x00c00100);
        } else if (bit16_scan4 == 7) {
            /* v0454: bit-22 plus bit 4, bit 12 clear. */
            g7_flags = UINT32_C(0x00400010);
        } else if (bit16_scan4 == 8) {
            /* v0454: bit-22 plus bit 12, bit 4 clear. */
            g7_flags = UINT32_C(0x00401000);
        } else if (bit16_scan4 == 9) {
            /* v0454: bit-22 plus bits 4 and 12. */
            g7_flags = UINT32_C(0x00401010);
        } else if (bit16_scan4 == 10) {
            /* v0455: g8 bit 16 plus bit 11 selects r3=42. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00010800);
        } else if (bit16_scan4 == 11) {
            /* v0455: g8 bit 16 plus bit 12. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00011000);
        } else if (bit16_scan4 == 12) {
            /* v0455: g8 bits 11 and 12 plus bit 16. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00011800);
        } else if (bit16_scan4 == 13) {
            /* v0456: g8 bits 15 and 16 skip the bbc-16 edge. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018000);
        } else if (bit16_scan4 == 14) {
            /* v0457: bit 11 remains visible after the bit-15 skip. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018800);
        } else if (bit16_scan4 == 15) {
            /* v0457: bit 12 remains visible after the bit-15 skip. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019000);
        } else if (bit16_scan4 == 16) {
            /* v0457: bits 11 and 12 remain visible after the skip. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019800);
        } else if (bit16_scan4 == 17) {
            /* v0457: low bit remains visible after the bit-15 skip. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018001);
        } else if (bit16_scan4 == 18) {
            /* v0457: bit 4 remains visible after the bit-15 skip. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018010);
        }
        CHECK(vf2_model2a_write_u32(
                  &reference_machine, COLI_LIVE_FIGHTER0 + UINT32_C(0x1a4),
                  g7_flags) == VF2_OK);
        CHECK(vf2_model2a_write_u32(
                  &native_machine, COLI_LIVE_FIGHTER0 + UINT32_C(0x1a4),
                  g7_flags) == VF2_OK);
        CHECK(vf2_model2a_write_u32(
                  &reference_machine, COLI_LIVE_FIGHTER1 + UINT32_C(0x1a4),
                  g8_flags) == VF2_OK);
        CHECK(vf2_model2a_write_u32(
                  &native_machine, COLI_LIVE_FIGHTER1 + UINT32_C(0x1a4),
                  g8_flags) == VF2_OK);
        CHECK(vf2_model2a_write(
                  &reference_machine, COLI_LIVE_FIGHTER0 + UINT32_C(0x821),
                  (const uint8_t *)"\x04", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(
                  &native_machine, COLI_LIVE_FIGHTER0 + UINT32_C(0x821),
                  (const uint8_t *)"\x04", 1u) == VF2_OK);
    }

    setup_live_cpu(&reference_cpu);
    setup_live_cpu(&native_cpu);
    CHECK(
        vf2_i960_cpu_enter_procedure(&reference_cpu, COLI_LIVE_ENTRY,
                                     COLI_LIVE_RETURN) == VF2_OK
    );
    CHECK(
        vf2_i960_cpu_enter_procedure(&native_cpu, COLI_LIVE_ENTRY,
                                     COLI_LIVE_RETURN) == VF2_OK
    );

    /* Reference: step the real body to its return. */
    reference_instructions = reference_cpu.executed_instructions;
    while (reference_cpu.ip != COLI_LIVE_RETURN && steps < 1024u) {
        reference_status =
            vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == COLI_LIVE_RETURN);
    reference_instructions =
        reference_cpu.executed_instructions - reference_instructions;

    /* Native: recovered body. */
    native_instructions = native_cpu.executed_instructions;
    native_status =
        vf2_hybrid_coli_225cc_execute(&native_machine, &native_cpu);
    native_instructions =
        native_cpu.executed_instructions - native_instructions;
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == COLI_LIVE_RETURN);

    /* Instruction-count lockstep on the live shape. */
    CHECK(native_instructions == reference_instructions);

    /* Full live-state equality (registers, CC/AC, frames, memory). */
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "coli-225cc-live ref=%d native=%d compare=%d "
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

    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 0);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 1);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 2);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 3);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 4);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 5);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 6);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 7);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 8);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 9);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 10);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 11);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 12);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 13);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 14);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 15);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 16);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 17);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 18);

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(stderr, "%d coli-225cc-live unit test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("coli-225cc-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d coli-225cc-live differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("coli-225cc-live differential tests passed");
    return EXIT_SUCCESS;
}
