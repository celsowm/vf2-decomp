/* ROM-backed differential fixture for the fa_coli mid-body tail
 * on the live first-contact shape (v0383).
 *
 * Witness (measured, current build): the whole mid-body from
 * out/coli-midbody-22210.vf2snap + v0381 recipe
 *   371 steps 0x22210 -> ret 0x10dcc (body 370 + ret), 9 calls / 10 rets
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

#define COLI_MIDBODY_ENTRY UINT32_C(0x00022210)
#define COLI_MIDBODY_RETURN UINT32_C(0x00010dcc)

static int failures = 0;
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #e); ++failures; } } while (0)

static vf2_status apply_live_mutations(vf2_model2a *m) {
    if (vf2_model2a_write_u32(m, UINT32_C(0x00510b24), UINT32_C(0x00000100)) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    if (vf2_model2a_write(m, UINT32_C(0x005111a0), (const uint8_t *)"\x01", 1u) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    if (vf2_model2a_write_u32(m, UINT32_C(0x005149cc), UINT32_C(0x0000ffff)) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    return VF2_OK;
}

static void test_unit_invalid_arguments(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(vf2_hybrid_coli_midbody_tail_execute(NULL, &cpu) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, NULL) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_ERROR_INVALID_ARGUMENT);
}

static void run_rom_case(const uint8_t *main_rom, size_t main_rom_size, const uint8_t *main_data, size_t main_data_size) {
    vf2_model2a reference_machine = {0};
    vf2_model2a native_machine = {0};
    vf2_i960_cpu reference_cpu = {0};
    vf2_i960_cpu native_cpu = {0};
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;
    uint64_t reference_instructions = 0u;
    uint64_t native_instructions = 0u;
    uint32_t steps = 0u;
    int use_snapshot = 0;
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);
    CHECK(vf2_model2a_initialize(&reference_machine));
    CHECK(vf2_model2a_initialize(&native_machine));
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        vf2_i960_snapshot_destroy(&snap);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&reference_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&native_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&reference_machine, main_data, main_data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&native_machine, main_data, main_data_size) == VF2_OK);

    if (vf2_i960_snapshot_read_file(&snap, "D:/ia/vf2-decomp/out/coli-midbody-22210.vf2snap") == VF2_OK ||
        vf2_i960_snapshot_read_file(&snap, "out/coli-midbody-22210.vf2snap") == VF2_OK ||
        vf2_i960_snapshot_read_file(&snap, "../out/coli-midbody-22210.vf2snap") == VF2_OK) {
        if (vf2_i960_snapshot_restore(&snap, &reference_cpu, &reference_machine) == VF2_OK &&
            vf2_i960_snapshot_restore(&snap, &native_cpu, &native_machine) == VF2_OK) {
            use_snapshot = 1;
            CHECK(vf2_model2a_attach_main_rom(&reference_machine, main_rom, main_rom_size) == VF2_OK);
            CHECK(vf2_model2a_attach_main_rom(&native_machine, main_rom, main_rom_size) == VF2_OK);
            CHECK(vf2_model2a_attach_main_data(&reference_machine, main_data, main_data_size) == VF2_OK);
            CHECK(vf2_model2a_attach_main_data(&native_machine, main_data, main_data_size) == VF2_OK);
            CHECK(apply_live_mutations(&reference_machine) == VF2_OK);
            CHECK(apply_live_mutations(&native_machine) == VF2_OK);
            reference_cpu.ip = COLI_MIDBODY_ENTRY;
            native_cpu.ip = COLI_MIDBODY_ENTRY;
            reference_cpu.registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(0x00510980);
            reference_cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512980);
            reference_cpu.registers[VF2_I960_G0_REGISTER + 13u] = UINT32_C(0x00514940);
            native_cpu.registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(0x00510980);
            native_cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512980);
            native_cpu.registers[VF2_I960_G0_REGISTER + 13u] = UINT32_C(0x00514940);
        }
    }
    if (!use_snapshot) {
        fprintf(stderr, "warning: midbody snapshot not found, using zeroed fallback\n");
        vf2_i960_cpu_reset(&reference_cpu, 0u, 0u, COLI_MIDBODY_ENTRY);
        reference_cpu.registers[0] = UINT32_C(0x005FF700);
        reference_cpu.registers[1] = UINT32_C(0x005FF780);
        reference_cpu.registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(0x00510980);
        reference_cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512980);
        reference_cpu.registers[VF2_I960_G0_REGISTER + 13u] = UINT32_C(0x00514940);
        reference_cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005FF740);
        vf2_i960_cpu_reset(&native_cpu, 0u, 0u, COLI_MIDBODY_ENTRY);
        native_cpu.registers[0] = UINT32_C(0x005FF700);
        native_cpu.registers[1] = UINT32_C(0x005FF780);
        native_cpu.registers[VF2_I960_G0_REGISTER + 7u] = UINT32_C(0x00510980);
        native_cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512980);
        native_cpu.registers[VF2_I960_G0_REGISTER + 13u] = UINT32_C(0x00514940);
        native_cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005FF740);
        CHECK(apply_live_mutations(&reference_machine) == VF2_OK);
        CHECK(apply_live_mutations(&native_machine) == VF2_OK);
    }
    if (reference_cpu.local_frame_depth == 0u) {
        CHECK(vf2_i960_cpu_enter_procedure(&reference_cpu, COLI_MIDBODY_ENTRY, COLI_MIDBODY_RETURN) == VF2_OK);
    } else {
        reference_cpu.registers[VF2_I960_G0_REGISTER + 14u] = COLI_MIDBODY_RETURN;
    }
    if (native_cpu.local_frame_depth == 0u) {
        CHECK(vf2_i960_cpu_enter_procedure(&native_cpu, COLI_MIDBODY_ENTRY, COLI_MIDBODY_RETURN) == VF2_OK);
    } else {
        native_cpu.registers[VF2_I960_G0_REGISTER + 14u] = COLI_MIDBODY_RETURN;
    }

    reference_instructions = reference_cpu.executed_instructions;
    while (reference_cpu.ip != COLI_MIDBODY_RETURN && steps < 2048u) {
        reference_status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(reference_status == VF2_OK);
        ++steps;
        if (reference_status != VF2_OK) break;
    }
    CHECK(reference_cpu.ip == COLI_MIDBODY_RETURN);
    CHECK(steps == 371u);
    reference_instructions = reference_cpu.executed_instructions - reference_instructions;
    native_instructions = native_cpu.executed_instructions;
    native_status = vf2_hybrid_coli_midbody_tail_execute(&native_machine, &native_cpu);
    native_instructions = native_cpu.executed_instructions - native_instructions;
    CHECK(native_status == VF2_OK);
    CHECK(native_cpu.ip == COLI_MIDBODY_RETURN);
    if (native_instructions != reference_instructions) {
        fprintf(stderr, "instr mismatch native=%llu ref=%llu steps=%u\n", (unsigned long long)native_instructions, (unsigned long long)reference_instructions, steps);
    }
    CHECK(native_instructions == reference_instructions);
    compare_status = vf2_i960_compare_live_state(&reference_cpu, &reference_machine, &native_cpu, &native_machine, &diff);
    if (compare_status != VF2_OK || !diff.equal) {
        fprintf(stderr, "coli-midbody-live ref=%d native=%d compare=%d component=%s offset=%zu expected=0x%08x actual=0x%08x steps=%u\n",
            (int)reference_status, (int)native_status, (int)compare_status, diff.component, diff.first_offset, (unsigned)diff.expected_value, (unsigned)diff.actual_value, steps);
    }
    CHECK(compare_status == VF2_OK);
    CHECK(diff.equal);
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

static void run_rom_differential(const char *rom_directory) {
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    CHECK(vf2_romset_build_region(rom_directory, VF2_REGION_MAINCPU, &main_rom, &main_rom_size) == VF2_OK);
    CHECK(vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size) == VF2_OK);
    if (main_rom == NULL || main_data == NULL) { free(main_rom); free(main_data); ++failures; return; }
    run_rom_case(main_rom, main_rom_size, main_data, main_data_size);
    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv) {
    test_unit_invalid_arguments();
    if (argc != 2) {
        if (failures != 0) { fprintf(stderr, "%d coli-midbody-live unit test(s) failed\n", failures); return EXIT_FAILURE; }
        puts("coli-midbody-live ROM-independent tests passed");
        return EXIT_SUCCESS;
    }
    run_rom_differential(argv[1]);
    if (failures != 0) { fprintf(stderr, "%d coli-midbody-live differential test(s) failed\n", failures); return EXIT_FAILURE; }
    puts("coli-midbody-live differential tests passed");
    return EXIT_SUCCESS;
}
