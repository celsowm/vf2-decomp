#define main vf2i960_legacy_main
#include "commands.c"
#undef main

static char *native_runtime_path(const char *snapshot_path)
{
    const size_t length = snapshot_path != NULL ? strlen(snapshot_path) : 0u;
    char *path = NULL;

    if (snapshot_path == NULL || length > SIZE_MAX - sizeof(".runtime")) {
        return NULL;
    }
    path = (char *)malloc(length + sizeof(".runtime"));
    if (path == NULL) {
        return NULL;
    }
    memcpy(path, snapshot_path, length);
    memcpy(path + length, ".runtime", sizeof(".runtime"));
    return path;
}

static vf2_status write_native_checkpoint(
    const char *snapshot_path,
    const vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    const vf2_native_runtime_state *runtime_state
)
{
    vf2_i960_snapshot snapshot;
    char *runtime_path = NULL;
    vf2_status status = VF2_OK;

    if (snapshot_path == NULL || machine == NULL || cpu == NULL ||
        runtime_state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    runtime_path = native_runtime_path(snapshot_path);
    if (runtime_path == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    vf2_i960_snapshot_init(&snapshot);
    status = vf2_i960_snapshot_capture(&snapshot, cpu, machine);
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_write_file(&snapshot, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_state_write_file(runtime_state, runtime_path);
    }
    vf2_i960_snapshot_destroy(&snapshot);

    if (status != VF2_OK) {
        (void)remove(snapshot_path);
        (void)remove(runtime_path);
    }
    free(runtime_path);
    return status;
}

static void print_native_cycle_failure(
    uint32_t dispatch,
    vf2_status status,
    const vf2_native_differential_report *report
)
{
    fprintf(
        stderr,
        "Native dispatch %u validation failed: %s\n",
        (unsigned)dispatch,
        vf2_status_string(status)
    );
    fprintf(
        stderr,
        "Reference/native IP: 0x%08x/0x%08x blocks=%zu "
        "instructions=%llu/%llu\n",
        (unsigned)report->final_reference_address,
        (unsigned)report->final_native_address,
        report->blocks_compared,
        (unsigned long long)report->reference_instructions_executed,
        (unsigned long long)report->native_recovered_instructions
    );
    fprintf(
        stderr,
        "Last native step: %s entry=0x%08x exit=0x%08x "
        "bridge=%s task=%s\n",
        vf2_native_runtime_step_kind_name(report->last_step.kind),
        (unsigned)report->last_step.entry_address,
        (unsigned)report->last_step.exit_address,
        vf2_hybrid_bridge_kind_name(report->last_step.bridge_kind),
        vf2_hybrid_task_kind_name(report->last_step.task_kind)
    );
    if (!report->diff.equal && report->diff.component[0] != '\0') {
        fprintf(
            stderr,
            "Difference: %s offset=0x%zx expected=0x%08x "
            "actual=0x%08x bytes=%zu\n",
            report->diff.component,
            report->diff.first_offset,
            (unsigned)report->diff.expected_value,
            (unsigned)report->diff.actual_value,
            report->diff.differing_bytes
        );
    }
}

static void print_native_cycle_match(
    uint32_t dispatch,
    const vf2_native_differential_report *report,
    const vf2_native_runtime_state *runtime_state,
    const vf2_i960_cpu *native_cpu
)
{
    printf("\nNative dispatch %u validation: MATCH\n", (unsigned)dispatch);
    printf("Repeated-cycle blocks compared:      %zu\n", report->blocks_compared);
    printf(
        "Repeated-cycle instructions:         %llu\n",
        (unsigned long long)report->native_recovered_instructions
    );
    printf("Repeated scheduler entries:          %zu\n", runtime_state->scheduler_entries);
    printf("Repeated scheduler transitions:      %zu\n", runtime_state->scheduler_transitions);
    printf("Repeated scheduler finishes:         %zu\n", runtime_state->scheduler_finishes);
    printf("Repeated frame-wait phases:          %zu\n", runtime_state->frame_wait_phases);
    printf("Dispatch task entry:                 0x%08x\n", (unsigned)native_cpu->ip);
    printf("Dispatch registry:                   0x%08x\n", (unsigned)native_cpu->registers[29]);
    printf("Final CPU and memory state:          MATCH\n");
}

static int command_native_continue_dispatch(
    const char *rom_directory,
    const char *input_snapshot_path,
    uint32_t target_dispatch,
    const char *output_snapshot_path
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snapshot;
    vf2_native_runtime_state runtime_state;
    vf2_native_differential_report report;
    char *runtime_path = NULL;
    uint32_t current_dispatch = 0u;
    uint32_t dispatch = 0u;
    uint32_t repeated_entry = 0u;
    vf2_status status = VF2_OK;
    int reference_initialized = 0;
    int native_initialized = 0;

    if (rom_directory == NULL || input_snapshot_path == NULL ||
        target_dispatch == 0u) {
        return EXIT_FAILURE;
    }
    runtime_path = native_runtime_path(input_snapshot_path);
    if (runtime_path == NULL) {
        return EXIT_FAILURE;
    }

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&reference_cpu, 0, sizeof(reference_cpu));
    memset(&native_cpu, 0, sizeof(native_cpu));
    memset(&runtime_state, 0, sizeof(runtime_state));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    (void)vectors;
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &reference_machine, image, image_size
        );
        reference_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &native_machine, image, image_size
        );
        native_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, input_snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &snapshot, &reference_cpu, &reference_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &snapshot, &native_cpu, &native_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_state_read_file(
            &runtime_state, runtime_path
        );
    }
    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Could not restore native checkpoint: %s\n",
            vf2_status_string(status)
        );
        goto cleanup;
    }

    if (runtime_state.scheduler_entries < 8u ||
        (runtime_state.scheduler_entries & 1u) != 0u ||
        runtime_state.scheduler_entries / 2u >
            (size_t)(UINT32_MAX - UINT32_C(6))) {
        status = VF2_ERROR_UNSUPPORTED;
        goto cleanup;
    }
    current_dispatch =
        (uint32_t)(runtime_state.scheduler_entries / 2u) + UINT32_C(6);
    if (current_dispatch < UINT32_C(6) || target_dispatch < current_dispatch) {
        fprintf(
            stderr,
            "Checkpoint represents dispatch %u; target must be >= that value.\n",
            (unsigned)current_dispatch
        );
        status = VF2_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    repeated_entry = native_cpu.ip;
    if (reference_cpu.ip != repeated_entry) {
        status = VF2_ERROR_UNSUPPORTED;
        goto cleanup;
    }

    printf(
        "Native continuation checkpoint: dispatch %u at 0x%08x\n",
        (unsigned)current_dispatch,
        (unsigned)repeated_entry
    );
    for (dispatch = current_dispatch + UINT32_C(1);
         status == VF2_OK && dispatch <= target_dispatch;
         ++dispatch) {
        memset(&report, 0, sizeof(report));
        status = vf2_native_differential_run_until_after(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &runtime_state,
            repeated_entry,
            1u,
            16384u,
            &report
        );
        if (status != VF2_OK) {
            print_native_cycle_failure(dispatch, status, &report);
            break;
        }
        print_native_cycle_match(dispatch, &report, &runtime_state, &native_cpu);
    }

    if (status == VF2_OK && output_snapshot_path != NULL) {
        status = write_native_checkpoint(
            output_snapshot_path,
            &native_machine,
            &native_cpu,
            &runtime_state
        );
        if (status == VF2_OK) {
            printf(
                "Native dispatch %u snapshot:          %s\n",
                (unsigned)target_dispatch,
                output_snapshot_path
            );
        }
    }

cleanup:
    vf2_i960_snapshot_destroy(&snapshot);
    if (native_initialized) {
        vf2_model2a_shutdown(&native_machine);
    }
    if (reference_initialized) {
        vf2_model2a_shutdown(&reference_machine);
    }
    free(image);
    free(runtime_path);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_native_nth_dispatch(
    const char *rom_directory,
    uint32_t dispatch_count,
    const char *output_snapshot_path
)
{
    static const char temporary_snapshot[] = ".vf2i960-native-nth-base6.vf2snap";
    char *temporary_runtime = NULL;
    const char *saved_snapshot_path = g_native_snapshot_path;
    int result = EXIT_FAILURE;

    if (dispatch_count == 0u) {
        fprintf(stderr, "Dispatch count must be at least 1.\n");
        return EXIT_FAILURE;
    }
    if (dispatch_count <= 4u && output_snapshot_path != NULL) {
        fprintf(
            stderr,
            "Checkpoint output is available for native dispatch 5 and later.\n"
        );
        return EXIT_FAILURE;
    }
    if (dispatch_count == 1u) {
        return command_native_first_dispatch(rom_directory);
    }
    if (dispatch_count == 2u) {
        return command_native_second_dispatch(rom_directory);
    }
    if (dispatch_count == 3u) {
        return command_native_third_dispatch(rom_directory);
    }
    if (dispatch_count == 4u) {
        return command_native_fourth_dispatch(rom_directory);
    }
    if (dispatch_count == 5u) {
        g_native_snapshot_path = output_snapshot_path;
        result = command_native_fifth_dispatch(rom_directory);
        g_native_snapshot_path = saved_snapshot_path;
        return result;
    }
    if (dispatch_count == 6u) {
        g_native_snapshot_path = output_snapshot_path;
        result = command_native_sixth_dispatch(rom_directory);
        g_native_snapshot_path = saved_snapshot_path;
        return result;
    }

    temporary_runtime = native_runtime_path(temporary_snapshot);
    if (temporary_runtime == NULL) {
        return EXIT_FAILURE;
    }
    (void)remove(temporary_snapshot);
    (void)remove(temporary_runtime);

    g_native_snapshot_path = temporary_snapshot;
    result = command_native_sixth_dispatch(rom_directory);
    g_native_snapshot_path = saved_snapshot_path;
    if (result != EXIT_SUCCESS) {
        fprintf(stderr, "Could not establish the validated sixth-dispatch base.\n");
    } else {
        result = command_native_continue_dispatch(
            rom_directory,
            temporary_snapshot,
            dispatch_count,
            output_snapshot_path
        );
    }

    (void)remove(temporary_snapshot);
    (void)remove(temporary_runtime);
    free(temporary_runtime);
    g_native_snapshot_path = saved_snapshot_path;
    return result;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "native-nth-dispatch") == 0) {
        uint32_t dispatch_count = 0u;

        if ((argc != 4 && argc != 5) ||
            !parse_u32(argv[3], &dispatch_count) || dispatch_count == 0u) {
            fprintf(
                stderr,
                "Usage: %s native-nth-dispatch <rom-directory> "
                "<dispatch-count> [output.vf2snap]\n",
                argv[0]
            );
            return EXIT_FAILURE;
        }
        return command_native_nth_dispatch(
            argv[2], dispatch_count, argc == 5 ? argv[4] : NULL
        );
    }

    if (argc >= 2 && strcmp(argv[1], "native-continue-dispatch") == 0) {
        uint32_t target_dispatch = 0u;

        if ((argc != 5 && argc != 6) ||
            !parse_u32(argv[4], &target_dispatch) || target_dispatch == 0u) {
            fprintf(
                stderr,
                "Usage: %s native-continue-dispatch <rom-directory> "
                "<input.vf2snap> <target-dispatch> [output.vf2snap]\n",
                argv[0]
            );
            return EXIT_FAILURE;
        }
        return command_native_continue_dispatch(
            argv[2], argv[3], target_dispatch, argc == 6 ? argv[5] : NULL
        );
    }

    return vf2i960_legacy_main(argc, argv);
}
