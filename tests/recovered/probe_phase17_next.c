#define main phase17_zero_test_main
#include "test_phase17_zero.c"
#undef main

typedef struct next_probe_case {
    const char *name;
    uint8_t menu_index;
    uint32_t runtime_flags;
    uint32_t input_flags;
    uint32_t navigation_flags;
    int use_copro;
} next_probe_case;

typedef struct next_probe_copro {
    uint32_t words[3];
    size_t count;
    uint32_t result;
    uint32_t unknown_command;
    int ready;
} next_probe_copro;

static vf2_status next_copro_write(
    void *context, uint32_t address, const void *source, size_t size)
{
    next_probe_copro *copro = context;
    uint32_t value = 0u;
    float left = 0.0f;
    float right = 0.0f;

    if (copro == NULL || source == NULL || size != sizeof(value) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || copro->count >= 3u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(&value, source, sizeof(value));
    copro->words[copro->count++] = value;
    if (copro->count != 3u) {
        return VF2_OK;
    }

    left = phase17_float_from_bits(copro->words[1]);
    right = phase17_float_from_bits(copro->words[2]);
    switch (copro->words[0]) {
    case UINT32_C(0x09801313):
        copro->result = phase17_float_to_bits(left + right);
        break;
    case UINT32_C(0x0a001414):
        copro->result = phase17_float_to_bits(left - right);
        break;
    default:
        copro->unknown_command = copro->words[0];
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    copro->ready = 1;
    return VF2_OK;
}

static vf2_status next_copro_read(
    void *context, uint32_t address, void *destination, size_t size)
{
    next_probe_copro *copro = context;

    if (copro == NULL || destination == NULL ||
        size != sizeof(copro->result) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || !copro->ready) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(destination, &copro->result, sizeof(copro->result));
    copro->count = 0u;
    copro->ready = 0;
    return VF2_OK;
}

static void next_run_probe(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    const next_probe_case *probe)
{
    phase17_zero_case test_case;
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result run_result;
    vf2_hybrid_bridge_report bridge_report;
    vf2_i960_snapshot_diff diff;
    next_probe_copro reference_copro;
    next_probe_copro native_copro;
    vf2_status reference_status = VF2_OK;
    vf2_status native_status = VF2_OK;
    vf2_status compare_status = VF2_OK;

    memset(&test_case, 0, sizeof(test_case));
    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&options, 0, sizeof(options));
    memset(&run_result, 0, sizeof(run_result));
    memset(&bridge_report, 0, sizeof(bridge_report));
    memset(&diff, 0, sizeof(diff));
    memset(&reference_copro, 0, sizeof(reference_copro));
    memset(&native_copro, 0, sizeof(native_copro));

    test_case.name = probe->name;
    test_case.menu_index = probe->menu_index;
    test_case.runtime_flags = probe->runtime_flags;
    test_case.input_flags = probe->input_flags;
    test_case.navigation_flags = probe->navigation_flags;
    test_case.previous_flags = probe->input_flags;
    test_case.menu_state = UINT8_C(0x40);

    if (!vf2_model2a_initialize(&reference_machine) ||
        !vf2_model2a_initialize(&native_machine)) {
        fprintf(stderr, "init failed: %s\n", probe->name);
        exit(EXIT_FAILURE);
    }
    (void)vf2_model2a_attach_main_rom(
        &reference_machine, main_rom, main_rom_size);
    (void)vf2_model2a_attach_main_rom(
        &native_machine, main_rom, main_rom_size);
    (void)vf2_model2a_attach_main_data(
        &reference_machine, main_data, main_data_size);
    (void)vf2_model2a_attach_main_data(
        &native_machine, main_data, main_data_size);
    if (probe->use_copro) {
        (void)vf2_model2a_set_copro_callbacks(
            &reference_machine, next_copro_read, next_copro_write,
            &reference_copro);
        (void)vf2_model2a_set_copro_callbacks(
            &native_machine, next_copro_read, next_copro_write,
            &native_copro);
    }
    (void)initialize_phase17_zero_state(&reference_machine, &test_case);
    (void)initialize_phase17_zero_state(&native_machine, &test_case);
    (void)enter_frame_dispatch(&reference_cpu);
    (void)enter_frame_dispatch(&native_cpu);
    if (probe->use_copro) {
        reference_cpu.registers[VF2_I960_G0_REGISTER + 11u] =
            UINT32_C(0x00884000);
        native_cpu.registers[VF2_I960_G0_REGISTER + 11u] =
            UINT32_C(0x00884000);
    }

    options.stop_address = UINT32_C(0x00001004);
    options.max_steps = UINT64_C(200000);
    reference_status = vf2_i960_run(
        &reference_cpu, &reference_machine, &options, &run_result);
    native_status = vf2_hybrid_post_frame_bridge_execute(
        &native_machine, &native_cpu, &bridge_report);
    compare_status = vf2_i960_compare_live_state(
        &reference_cpu, &reference_machine,
        &native_cpu, &native_machine, &diff);

    printf(
        "%s index=%u runtime=%08x input=%08x nav=%08x copro=%d "
        "ref=%d halt=%d ins=%llu calls=%llu ret=%llu depth=%u unknown=%08x "
        "native=%d nins=%llu ncalls=%llu nret=%llu ndepth=%u "
        "compare=%d equal=%d diff=%s off=%zu exp=%08x got=%08x\n",
        probe->name,
        (unsigned)probe->menu_index,
        (unsigned)probe->runtime_flags,
        (unsigned)probe->input_flags,
        (unsigned)probe->navigation_flags,
        probe->use_copro,
        (int)reference_status,
        (int)run_result.halt_reason,
        (unsigned long long)reference_cpu.executed_instructions,
        (unsigned long long)reference_cpu.procedure_calls,
        (unsigned long long)reference_cpu.procedure_returns,
        (unsigned)reference_cpu.maximum_local_frame_depth,
        (unsigned)reference_copro.unknown_command,
        (int)native_status,
        (unsigned long long)bridge_report.recovered_instruction_count,
        (unsigned long long)bridge_report.recovered_procedure_calls,
        (unsigned long long)bridge_report.recovered_procedure_returns,
        (unsigned)native_cpu.maximum_local_frame_depth,
        (int)compare_status,
        diff.equal,
        diff.component,
        diff.first_offset,
        (unsigned)diff.expected_value,
        (unsigned)diff.actual_value);

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

int main(int argc, char **argv)
{
    static const next_probe_case cases[] = {
        {"s4-idle", 4, 0, 0, 0, 0},
        {"s4-nav12", 4, 0, 0, UINT32_C(1) << 12u, 0},
        {"s4-nav13", 4, 0, 0, UINT32_C(1) << 13u, 0},
        {"s4-in12", 4, 0, UINT32_C(1) << 12u, 0, 0},
        {"s4-in13", 4, 0, UINT32_C(1) << 13u, 0, 0},

        {"s6-idle", 6, 0, 0, 0, 0},
        {"s6-in4", 6, 0, UINT32_C(1) << 4u, 0, 0},
        {"s6-in8", 6, 0, UINT32_C(1) << 8u, 0, 0},
        {"s6-in9", 6, 0, UINT32_C(1) << 9u, 0, 0},
        {"s6-in12", 6, 0, UINT32_C(1) << 12u, 0, 0},
        {"s6-in13", 6, 0, UINT32_C(1) << 13u, 0, 0},
        {"s6-in14", 6, 0, UINT32_C(1) << 14u, 0, 0},
        {"s6-in15", 6, 0, UINT32_C(1) << 15u, 0, 0},
        {"s6-nav8", 6, 0, 0, UINT32_C(1) << 8u, 0},
        {"s6-nav9", 6, 0, 0, UINT32_C(1) << 9u, 0},
        {"s6-nav12", 6, 0, 0, UINT32_C(1) << 12u, 0},
        {"s6-nav13", 6, 0, 0, UINT32_C(1) << 13u, 0},
        {"s6-runtime9", 6, UINT32_C(1) << 9u, 0, 0, 0},

        {"s9-idle", 9, 0, 0, 0, 0},
        {"s9-nav8", 9, 0, 0, UINT32_C(1) << 8u, 0},
        {"s9-nav9", 9, 0, 0, UINT32_C(1) << 9u, 0},
        {"s9-in12", 9, 0, UINT32_C(1) << 12u, 0, 0},
        {"s9-in13", 9, 0, UINT32_C(1) << 13u, 0, 0},
        {"s9-in14", 9, 0, UINT32_C(1) << 14u, 0, 0},
        {"s9-in15", 9, 0, UINT32_C(1) << 15u, 0, 0},

        {"s10-idle", 10, 0, 0, 0, 1},
        {"s10-in8", 10, 0, UINT32_C(1) << 8u, 0, 1},
        {"s10-in9", 10, 0, UINT32_C(1) << 9u, 0, 1},
        {"s10-in12", 10, 0, UINT32_C(1) << 12u, 0, 1},
        {"s10-in13", 10, 0, UINT32_C(1) << 13u, 0, 1},
        {"s10-in14", 10, 0, UINT32_C(1) << 14u, 0, 1},
        {"s10-in15", 10, 0, UINT32_C(1) << 15u, 0, 1},
        {"s10-mode-angle-dec", 10, 0,
         (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u), 0, 1},
        {"s10-mode-angle-inc", 10, 0,
         (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u), 0, 1},
        {"s10-reset", 10, 0, 0, UINT32_C(0x700), 1},

        {"s13-idle", 13, 0, 0, 0, 1},
        {"s13-nav14", 13, 0, 0, UINT32_C(1) << 14u, 1},
        {"s13-nav15", 13, 0, 0, UINT32_C(1) << 15u, 1},
        {"s13-in16", 13, 0, UINT32_C(1) << 16u, 0, 1},
        {"s13-in18", 13, 0, UINT32_C(1) << 18u, 0, 1},
        {"s13-in20", 13, 0, UINT32_C(1) << 20u, 0, 1},
        {"s13-in21", 13, 0, UINT32_C(1) << 21u, 0, 1},
        {"s13-in22", 13, 0, UINT32_C(1) << 22u, 0, 1},
        {"s13-in23", 13, 0, UINT32_C(1) << 23u, 0, 1},
    };
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    size_t index = 0u;

    if (argc != 2) {
        fprintf(stderr, "usage: %s ROM_DIR\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (vf2_romset_build_region(
            argv[1], VF2_REGION_MAINCPU, &main_rom, &main_rom_size) != VF2_OK ||
        vf2_romset_build_region(
            argv[1], VF2_REGION_MAIN_DATA, &main_data, &main_data_size) != VF2_OK) {
        fprintf(stderr, "ROM load failed\n");
        free(main_data);
        free(main_rom);
        return EXIT_FAILURE;
    }

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        next_run_probe(
            main_rom, main_rom_size, main_data, main_data_size, &cases[index]);
    }

    free(main_data);
    free(main_rom);
    return EXIT_SUCCESS;
}
