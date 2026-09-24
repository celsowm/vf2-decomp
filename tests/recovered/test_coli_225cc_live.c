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
        } else if (bit16_scan4 == 19) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018004);
        } else if (bit16_scan4 == 20) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018005);
        } else if (bit16_scan4 == 21) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018801);
        } else if (bit16_scan4 == 22) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018804);
        } else if (bit16_scan4 == 23) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018805);
        } else if (bit16_scan4 == 24) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019001);
        } else if (bit16_scan4 == 25) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019004);
        } else if (bit16_scan4 == 26) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019005);
        } else if (bit16_scan4 == 27) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019801);
        } else if (bit16_scan4 == 28) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019804);
        } else if (bit16_scan4 == 29) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019805);
        } else if (bit16_scan4 == 30) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018011);
        } else if (bit16_scan4 == 31) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018014);
        } else if (bit16_scan4 == 32) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018015);
        } else if (bit16_scan4 == 33) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018810);
        } else if (bit16_scan4 == 34) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018811);
        } else if (bit16_scan4 == 35) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018814);
        } else if (bit16_scan4 == 36) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018815);
        } else if (bit16_scan4 == 37) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019010);
        } else if (bit16_scan4 == 38) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019011);
        } else if (bit16_scan4 == 39) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019014);
        } else if (bit16_scan4 == 40) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019015);
        } else if (bit16_scan4 == 41) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019810);
        } else if (bit16_scan4 == 42) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019811);
        } else if (bit16_scan4 == 43) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019814);
        } else if (bit16_scan4 == 44) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00019815);
        } else if (bit16_scan4 == 45) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018002);
        } else if (bit16_scan4 == 47) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018020);
        } else if (bit16_scan4 == 48) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018040);
        } else if (bit16_scan4 == 49) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018080);
        } else if (bit16_scan4 == 50) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018200);
        } else if (bit16_scan4 == 51) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018400);
        } else if (bit16_scan4 == 52) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001c000);
        } else if (bit16_scan4 == 53) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00038000);
        } else if (bit16_scan4 == 54) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018100);
        } else if (bit16_scan4 == 55) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00018008);
        } else if (bit16_scan4 == 56) {
            /* v0463: bit-13 scan-4 branch through the existing long body. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a000);
        } else if (bit16_scan4 == 57) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a001);
        } else if (bit16_scan4 == 58) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a004);
        } else if (bit16_scan4 == 59) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a010);
        } else if (bit16_scan4 == 60) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a020);
        } else if (bit16_scan4 == 61) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a040);
        } else if (bit16_scan4 == 62) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a080);
        } else if (bit16_scan4 == 63) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a200);
        } else if (bit16_scan4 == 64) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a400);
        } else if (bit16_scan4 == 65) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a008);
        } else if (bit16_scan4 == 66) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a100);
        } else if (bit16_scan4 == 67) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a800);
        } else if (bit16_scan4 == 68) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001b000);
        } else if (bit16_scan4 == 69) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001e000);
        } else if (bit16_scan4 == 70) {
            /* v0467: bit-3/bit-8/bit-13 compact route. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a108);
        } else if (bit16_scan4 == 71) {
            /* v0467: bit-3/bit-8/bit-11/bit-13 field route. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a808);
        } else if (bit16_scan4 == 72) {
            /* v0467: bit-3/bit-8/bit-12/bit-13 field route. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001b008);
        } else if (bit16_scan4 == 73) {
            /* v0467: bit-3/bit-8/bit-13/bit-14 field route. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001e008);
        } else if (bit16_scan4 == 74) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a009);
        } else if (bit16_scan4 == 75) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a00c);
        } else if (bit16_scan4 == 76) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a018);
        } else if (bit16_scan4 == 77) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a028);
        } else if (bit16_scan4 == 78) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a048);
        } else if (bit16_scan4 == 79) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a088);
        } else if (bit16_scan4 == 80) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a208);
        } else if (bit16_scan4 == 81) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001a408);
        } else if (bit16_scan4 == 82) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00002000);
        } else if (bit16_scan4 == 83) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00002001);
        } else if (bit16_scan4 == 84) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00002010);
        } else if (bit16_scan4 == 85) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0000a000);
        } else if (bit16_scan4 == 86) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012000);
        } else if (bit16_scan4 == 87) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012001);
        } else if (bit16_scan4 == 88) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012004);
        } else if (bit16_scan4 == 89) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012008);
        } else if (bit16_scan4 == 90) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012010);
        } else if (bit16_scan4 == 91) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012020);
        } else if (bit16_scan4 == 92) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012040);
        } else if (bit16_scan4 == 93) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012080);
        } else if (bit16_scan4 == 94) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012800);
        } else if (bit16_scan4 == 95) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00013000);
        } else if (bit16_scan4 == 96) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00016000);
        } else if (bit16_scan4 == 97) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012808);
        } else if (bit16_scan4 == 98) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00013008);
        } else if (bit16_scan4 == 99) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00016008);
        } else if (bit16_scan4 == 100) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00012810);
        } else if (bit16_scan4 == 101) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00013010);
        } else if (bit16_scan4 == 102) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00016010);
        } else if (bit16_scan4 == 103) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014000);
        } else if (bit16_scan4 == 104) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014008);
        } else if (bit16_scan4 == 105) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014800);
        } else if (bit16_scan4 == 106) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00015000);
        } else if (bit16_scan4 == 107) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00015800);
        } else if (bit16_scan4 == 108) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00016800);
        } else if (bit16_scan4 == 109) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00017000);
        } else if (bit16_scan4 == 110) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00017800);
        } else if (bit16_scan4 == 111) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001d000);
        } else if (bit16_scan4 == 112) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f000);
        } else if (bit16_scan4 == 113) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014001);
        } else if (bit16_scan4 == 114) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014004);
        } else if (bit16_scan4 == 115) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014010);
        } else if (bit16_scan4 == 116) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014020);
        } else if (bit16_scan4 == 117) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014040);
        } else if (bit16_scan4 == 118) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014080);
        } else if (bit16_scan4 == 119) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014808);
        } else if (bit16_scan4 == 120) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00014810);
        } else if (bit16_scan4 == 121) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00015008);
        } else if (bit16_scan4 == 122) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00015010);
        } else if (bit16_scan4 == 123) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00015808);
        } else if (bit16_scan4 == 124) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x00015810);
        } else if (bit16_scan4 == 125) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001d008);
        } else if (bit16_scan4 == 126) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001d010);
        } else if (bit16_scan4 == 127) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f008);
        } else if (bit16_scan4 == 128) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f010);
        } else if (bit16_scan4 == 129) {
            /* v0486: measured bit-2 sibling of the high selector words. */
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f004);
        } else if (bit16_scan4 == 130) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f001);
        } else if (bit16_scan4 == 131) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f005);
        } else if (bit16_scan4 == 132) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f009);
        } else if (bit16_scan4 == 133) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f00c);
        } else if (bit16_scan4 == 134) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f011);
        } else if (bit16_scan4 == 135) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f014);
        } else if (bit16_scan4 == 136) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f015);
        } else if (bit16_scan4 == 137) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f020);
        } else if (bit16_scan4 == 138) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f002);
        } else if (bit16_scan4 == 139) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f003);
        } else if (bit16_scan4 == 140) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f006);
        } else if (bit16_scan4 == 141) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f007);
        } else if (bit16_scan4 == 142) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f00a);
        } else if (bit16_scan4 == 143) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f00b);
        } else if (bit16_scan4 == 144) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f00e);
        } else if (bit16_scan4 == 145) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f00f);
        } else if (bit16_scan4 == 146) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f012);
        } else if (bit16_scan4 == 147) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f013);
        } else if (bit16_scan4 == 148) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f016);
        } else if (bit16_scan4 == 149) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f017);
        } else if (bit16_scan4 == 150) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f018);
        } else if (bit16_scan4 == 151) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f019);
        } else if (bit16_scan4 == 152) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f01a);
        } else if (bit16_scan4 == 153) {
            g7_flags = UINT32_C(0x00400100);
            g8_flags = UINT32_C(0x0001f01b);
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

/* v0493: the measured type-22 shortcut with a type-5 walker miss at index 1.
 * The parked entry snapshot already has the live 0x225cc frame and parent
 * return chain. Reference probing with +0x19f=22 and +0x19c=1 reaches the
 * parent boundary 0x10dcc in 71 instructions, +3 calls / +5 returns. */
static void run_type22_miss_snapshot_case(
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
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    uint64_t reference_instructions = 0u;
    uint64_t native_instructions = 0u;
    uint64_t snap_instructions = 0u;
    uint64_t snap_calls = 0u;
    uint64_t snap_returns = 0u;
    uint32_t steps = 0u;
    uint32_t g8 = 0u;
    int ok = 0;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);
    CHECK(vf2_model2a_initialize(&reference_machine));
    CHECK(vf2_model2a_initialize(&native_machine));
    ok = (vf2_i960_snapshot_read_file(
              &snap,
              "D:/ia/vf2-decomp/out/coli-225cc-entry.vf2snap") == VF2_OK ||
          vf2_i960_snapshot_read_file(
              &snap, "out/coli-225cc-entry.vf2snap") == VF2_OK);
    CHECK(ok);
    if (!ok) {
        goto cleanup;
    }
    CHECK(vf2_i960_snapshot_restore(
        &snap, &reference_cpu, &reference_machine) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(
        &snap, &native_cpu, &native_machine) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(
        &reference_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(
        &reference_machine, main_data, main_data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(
        &native_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(
        &native_machine, main_data, main_data_size) == VF2_OK);

    g8 = reference_cpu.registers[VF2_I960_G0_REGISTER + 8u];
    CHECK(g8 == COLI_LIVE_FIGHTER1);
    CHECK(write_seed_bytes(
        &reference_machine, g8 + UINT32_C(0x19f), UINT32_C(22), 1u
    ) == VF2_OK);
    CHECK(write_seed_bytes(
        &native_machine, g8 + UINT32_C(0x19f), UINT32_C(22), 1u
    ) == VF2_OK);
    CHECK(write_seed_bytes(
        &reference_machine, g8 + UINT32_C(0x19c), UINT32_C(1), 2u
    ) == VF2_OK);
    CHECK(write_seed_bytes(
        &native_machine, g8 + UINT32_C(0x19c), UINT32_C(1), 2u
    ) == VF2_OK);

    snap_instructions = snap.cpu.executed_instructions;
    snap_calls = snap.cpu.procedure_calls;
    snap_returns = snap.cpu.procedure_returns;
    while (reference_cpu.ip != UINT32_C(0x00010dcc) && steps < 128u) {
        reference_status = vf2_i960_step(
            &reference_cpu, &reference_machine, NULL
        );
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == UINT32_C(0x00010dcc));
    reference_instructions =
        reference_cpu.executed_instructions - snap_instructions;
    CHECK(reference_instructions == UINT64_C(71));
    CHECK(reference_cpu.procedure_calls - snap_calls == UINT64_C(3));
    CHECK(reference_cpu.procedure_returns - snap_returns == UINT64_C(5));

    native_instructions = native_cpu.executed_instructions;
    native_status = vf2_hybrid_coli_225cc_execute(
        &native_machine, &native_cpu
    );
    native_instructions =
        native_cpu.executed_instructions - native_instructions;
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == UINT32_C(0x00010dcc));
    CHECK(native_instructions == reference_instructions);
    CHECK(native_cpu.procedure_calls - snap_calls == UINT64_C(3));
    CHECK(native_cpu.procedure_returns - snap_returns == UINT64_C(5));

    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff
    );
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(
            stderr,
            "coli-225cc-type22-miss ref=%d native=%d compare=%d "
            "component=%s offset=%zu expected=0x%08x actual=0x%08x\n",
            (int)reference_status, (int)native_status,
            (int)compare_status, diff.component, diff.first_offset,
            (unsigned)diff.expected_value, (unsigned)diff.actual_value);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);

cleanup:
    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
    vf2_i960_snapshot_destroy(&snap);
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
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 19);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 20);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 21);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 22);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 23);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 24);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 25);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 26);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 27);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 28);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 29);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 30);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 31);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 32);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 33);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 34);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 35);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 36);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 37);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 38);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 39);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 40);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 41);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 42);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 43);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 44);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 45);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 47);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 48);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 49);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 50);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 51);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 52);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 53);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 54);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 55);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 56);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 57);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 58);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 59);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 60);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 61);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 62);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 63);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 64);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 65);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 66);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 67);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 68);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 69);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 70);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 71);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 72);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 73);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 74);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 75);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 76);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 77);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 78);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 79);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 80);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 81);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 82);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 83);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 84);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 85);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 86);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 87);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 88);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 89);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 90);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 91);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 92);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 93);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 94);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 95);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 96);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 97);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 98);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 99);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 100);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 101);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 102);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 103);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 104);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 105);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 106);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 107);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 108);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 109);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 110);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 111);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 112);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 113);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 114);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 115);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 116);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 117);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 118);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 119);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 120);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 121);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 122);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 123);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 124);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 125);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 126);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 127);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 128);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 129);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 130);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 131);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 132);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 133);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 134);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 135);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 136);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 137);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 138);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 139);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 140);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 141);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 142);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 143);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 144);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 145);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 146);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 147);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 148);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 149);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 150);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 151);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 152);
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size, 153);
    run_type22_miss_snapshot_case(
        main_rom, main_rom_size, main_data, main_data_size
    );

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
