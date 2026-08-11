#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

typedef struct scalar_copro {
    uint32_t words[3];
    size_t count;
    uint32_t result;
    int ready;
    int bad_command;
} scalar_copro;

static float float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static vf2_status copro_write(
    void *context, uint32_t address, const void *source, size_t size)
{
    scalar_copro *copro = context;
    uint32_t value = 0u;
    float left = 0.0f;
    float right = 0.0f;

    if (copro == NULL || source == NULL || size != sizeof(value) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memcpy(&value, source, sizeof(value));
    if (copro->count >= 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    copro->words[copro->count++] = value;
    if (copro->count != 3u) {
        return VF2_OK;
    }

    left = float_from_bits(copro->words[1]);
    right = float_from_bits(copro->words[2]);
    switch (copro->words[0]) {
    case UINT32_C(0x09801313):
        copro->result = float_to_bits(left + right);
        copro->ready = 1;
        return VF2_OK;
    case UINT32_C(0x0a001414):
        copro->result = float_to_bits(left - right);
        copro->ready = 1;
        return VF2_OK;
    default:
        copro->bad_command = 1;
        return VF2_ERROR_UNSUPPORTED;
    }
}

static vf2_status copro_read(
    void *context, uint32_t address, void *destination, size_t size)
{
    scalar_copro *copro = context;

    if (copro == NULL || destination == NULL || size != sizeof(uint32_t) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || !copro->ready) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memcpy(destination, &copro->result, sizeof(copro->result));
    copro->count = 0u;
    copro->ready = 0;
    return VF2_OK;
}

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status initialize_state(
    vf2_model2a *machine,
    uint8_t menu_index,
    uint32_t input_flags)
{
    const uint32_t player0 = UINT32_C(0x00510000);
    const uint32_t player1 = UINT32_C(0x00512000);
    const uint32_t control = UINT32_C(0x00514000);
    const uint32_t descriptor = UINT32_C(0x00516000);
    const uint32_t associated0 = UINT32_C(0x00518200);
    const uint32_t associated1 = UINT32_C(0x00518300);
    uint16_t fighter_control = UINT16_C(0x1234);
    vf2_status status = VF2_OK;

    status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000), 0u);
    if (status == VF2_OK) status = write_u8(machine, UINT32_C(0x0050002a), UINT8_C(17));
    if (status == VF2_OK) status = write_u8(machine, UINT32_C(0x005000a6), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500804), player0);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500808), player1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500814), control);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050081c), descriptor);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, descriptor + UINT32_C(0x0c), UINT32_C(0x0001b9ac));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500700), input_flags);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500704), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050070c), input_flags);
    if (status == VF2_OK) status = write_u8(machine, UINT32_C(0x00508008), menu_index);
    if (status == VF2_OK) status = write_u8(machine, UINT32_C(0x00500085), UINT8_C(0x40));
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a0), &fighter_control, sizeof(fighter_control));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, player0, UINT32_MAX);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, player1, UINT32_MAX);
    if (status == VF2_OK) status = write_u8(machine, player0 + UINT32_C(0x1200), UINT8_C(0x5a));
    if (status == VF2_OK) status = write_u8(machine, player1 + UINT32_C(0x1200), UINT8_C(0xa5));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500860), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050083c), UINT32_C(0x00518000));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00518000), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500840), UINT32_C(0x00518100));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00518100), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500868), associated0);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050086c), associated1);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated0, 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, associated1, 0u);
    if (status == VF2_OK) status = write_u8(machine, UINT32_C(0x0050a0b8), 0u);
    if (status == VF2_OK) status = write_u8(machine, UINT32_C(0x0050a0b9), 0u);
    return status;
}

static vf2_status enter_frame(vf2_i960_cpu *cpu)
{
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = UINT32_C(0x00503000);
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00884000);
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = 0u;
    return vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0000a6c0), UINT32_C(0x00001004));
}

static void run_probe(
    const uint8_t *main_rom, size_t main_rom_size,
    const uint8_t *main_data, size_t main_data_size,
    uint8_t menu_index, uint32_t input_flags)
{
    vf2_model2a ref_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_hybrid_bridge_report report;
    vf2_i960_snapshot_diff diff;
    scalar_copro ref_copro;
    scalar_copro native_copro;
    vf2_status rs;
    vf2_status ns;
    vf2_status cs;

    memset(&ref_machine, 0, sizeof(ref_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    memset(&report, 0, sizeof(report));
    memset(&diff, 0, sizeof(diff));
    memset(&ref_copro, 0, sizeof(ref_copro));
    memset(&native_copro, 0, sizeof(native_copro));

    if (!vf2_model2a_initialize(&ref_machine) ||
        !vf2_model2a_initialize(&native_machine)) {
        fprintf(stderr, "init failed\n");
        exit(EXIT_FAILURE);
    }
    (void)vf2_model2a_attach_main_rom(&ref_machine, main_rom, main_rom_size);
    (void)vf2_model2a_attach_main_rom(&native_machine, main_rom, main_rom_size);
    (void)vf2_model2a_attach_main_data(&ref_machine, main_data, main_data_size);
    (void)vf2_model2a_attach_main_data(&native_machine, main_data, main_data_size);
    (void)vf2_model2a_set_copro_callbacks(&ref_machine, copro_read, copro_write, &ref_copro);
    (void)vf2_model2a_set_copro_callbacks(&native_machine, copro_read, copro_write, &native_copro);
    (void)initialize_state(&ref_machine, menu_index, input_flags);
    (void)initialize_state(&native_machine, menu_index, input_flags);
    (void)enter_frame(&ref_cpu);
    (void)enter_frame(&native_cpu);

    options.stop_address = UINT32_C(0x00001004);
    options.max_steps = UINT64_C(200000);
    rs = vf2_i960_run(&ref_cpu, &ref_machine, &options, &result);
    ns = vf2_hybrid_post_frame_bridge_execute(&native_machine, &native_cpu, &report);
    cs = vf2_i960_compare_live_state(
        &ref_cpu, &ref_machine, &native_cpu, &native_machine, &diff);

    printf("index=%u input=%08x ref=%d halt=%d ins=%llu calls=%llu ret=%llu depth=%u badcopro=%d native=%d nins=%llu ncalls=%llu nret=%llu ndepth=%u compare=%d equal=%d diff=%s off=%zu exp=%08x got=%08x g11=%08x/%08x g12=%08x/%08x\n",
        (unsigned)menu_index, (unsigned)input_flags,
        (int)rs, (int)result.halt_reason,
        (unsigned long long)ref_cpu.executed_instructions,
        (unsigned long long)ref_cpu.procedure_calls,
        (unsigned long long)ref_cpu.procedure_returns,
        (unsigned)ref_cpu.maximum_local_frame_depth,
        ref_copro.bad_command,
        (int)ns,
        (unsigned long long)report.recovered_instruction_count,
        (unsigned long long)report.recovered_procedure_calls,
        (unsigned long long)report.recovered_procedure_returns,
        (unsigned)native_cpu.maximum_local_frame_depth,
        (int)cs, diff.equal, diff.component, diff.first_offset,
        (unsigned)diff.expected_value, (unsigned)diff.actual_value,
        (unsigned)ref_cpu.registers[VF2_I960_G0_REGISTER + 11u],
        (unsigned)native_cpu.registers[VF2_I960_G0_REGISTER + 11u],
        (unsigned)ref_cpu.registers[VF2_I960_G0_REGISTER + 12u],
        (unsigned)native_cpu.registers[VF2_I960_G0_REGISTER + 12u]);

    vf2_model2a_shutdown(&ref_machine);
    vf2_model2a_shutdown(&native_machine);
}

int main(int argc, char **argv)
{
    static const uint8_t screens[] = {3u, 5u, 12u};
    static const uint32_t inputs[] = {
        UINT32_C(0), UINT32_C(1) << 8u, UINT32_C(1) << 9u,
        UINT32_C(1) << 14u, UINT32_C(1) << 15u
    };
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    size_t screen = 0u;
    size_t input = 0u;

    if (argc != 2) {
        fprintf(stderr, "usage: %s ROM_DIR\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                &main_rom, &main_rom_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                &main_data, &main_data_size) != VF2_OK) {
        fprintf(stderr, "ROM load failed\n");
        free(main_data);
        free(main_rom);
        return EXIT_FAILURE;
    }

    for (screen = 0u; screen < sizeof(screens) / sizeof(screens[0]); ++screen) {
        for (input = 0u; input < sizeof(inputs) / sizeof(inputs[0]); ++input) {
            run_probe(main_rom, main_rom_size, main_data, main_data_size,
                      screens[screen], inputs[input]);
        }
    }

    free(main_data);
    free(main_rom);
    return EXIT_SUCCESS;
}
