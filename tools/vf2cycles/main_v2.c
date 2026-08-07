#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/i960/snapshot.h"
#include "vf2/native_differential.h"
#include "vf2/native_runtime.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

typedef struct vf2_cycles_options {
    const char *rom_directory;
    const char *snapshot_path;
    const char *runtime_state_path;
    const char *failure_prefix;
    size_t cycle_count;
    size_t minimum_blocks;
    size_t maximum_blocks;
} vf2_cycles_options;

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "vf2cycles v%s\n"
        "Usage: %s --rom-dir <directory> --snapshot <file> "
        "[--state <file>] [--failure-prefix <path>] "
        "[--cycles <count>] [--min-blocks <count>] "
        "[--max-blocks <count>]\n",
        VF2_VERSION_STRING,
        program
    );
}

static int parse_size(const char *text, size_t *value)
{
    char *end = NULL;
    unsigned long long parsed = 0u;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > (unsigned long long)SIZE_MAX) {
        return 0;
    }

    *value = (size_t)parsed;
    return 1;
}

static int parse_options(
    int argc,
    char **argv,
    vf2_cycles_options *options
)
{
    int index = 1;

    if (options == NULL) {
        return 0;
    }

    memset(options, 0, sizeof(*options));
    options->cycle_count = 1u;
    options->minimum_blocks = 1u;
    options->maximum_blocks = 16384u;

    while (index < argc) {
        const char *argument = argv[index];

        if (strcmp(argument, "--rom-dir") == 0 && index + 1 < argc) {
            options->rom_directory = argv[++index];
        } else if (strcmp(argument, "--snapshot") == 0 &&
                   index + 1 < argc) {
            options->snapshot_path = argv[++index];
        } else if (strcmp(argument, "--state") == 0 &&
                   index + 1 < argc) {
            options->runtime_state_path = argv[++index];
        } else if (strcmp(argument, "--failure-prefix") == 0 &&
                   index + 1 < argc) {
            options->failure_prefix = argv[++index];
        } else if (strcmp(argument, "--cycles") == 0 &&
                   index + 1 < argc) {
            if (!parse_size(argv[++index], &options->cycle_count)) {
                return 0;
            }
        } else if (strcmp(argument, "--min-blocks") == 0 &&
                   index + 1 < argc) {
            if (!parse_size(argv[++index], &options->minimum_blocks)) {
                return 0;
            }
        } else if (strcmp(argument, "--max-blocks") == 0 &&
                   index + 1 < argc) {
            if (!parse_size(argv[++index], &options->maximum_blocks)) {
                return 0;
            }
        } else {
            return 0;
        }
        ++index;
    }

    return options->rom_directory != NULL &&
           options->snapshot_path != NULL &&
           options->minimum_blocks <= options->maximum_blocks &&
           (options->cycle_count == 0u || options->minimum_blocks != 0u);
}

static char *append_suffix(const char *path, const char *suffix)
{
    const size_t path_length = path != NULL ? strlen(path) : 0u;
    const size_t suffix_length = suffix != NULL ? strlen(suffix) : 0u;
    char *result = NULL;

    if (path == NULL || suffix == NULL ||
        path_length > SIZE_MAX - suffix_length - 1u) {
        return NULL;
    }
    result = (char *)malloc(path_length + suffix_length + 1u);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, path, path_length);
    memcpy(result + path_length, suffix, suffix_length + 1u);
    return result;
}

static vf2_status initialize_machine(
    vf2_model2a *machine,
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
)
{
    vf2_status status = VF2_OK;

    memset(machine, 0, sizeof(*machine));
    if (!vf2_model2a_initialize(machine)) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    status = vf2_model2a_attach_main_rom(
        machine,
        main_rom,
        main_rom_size
    );
    if (status == VF2_OK) {
        status = vf2_model2a_attach_main_data(
            machine,
            main_data,
            main_data_size
        );
    }
    if (status != VF2_OK) {
        vf2_model2a_shutdown(machine);
    }
    return status;
}

static void print_report(
    const vf2_native_differential_cycles_report *report,
    const vf2_native_runtime_state *runtime_state
)
{
    printf("Snapshot repeated address:          0x%08x\n",
           (unsigned)report->repeated_address);
    printf("Requested/completed cycles:         %zu/%zu\n",
           report->requested_cycles, report->completed_cycles);
    printf("Compared blocks:                    %zu\n",
           report->blocks_compared);
    printf("Reference instructions:             %llu\n",
           (unsigned long long)report->reference_instructions_executed);
    printf("Recovered native instructions:      %llu\n",
           (unsigned long long)report->native_recovered_instructions);
    printf("Final reference/native address:     0x%08x/0x%08x\n",
           (unsigned)report->final_reference_address,
           (unsigned)report->final_native_address);
    printf("Scheduler entries/transitions/end:  %zu/%zu/%zu\n",
           runtime_state->scheduler_entries,
           runtime_state->scheduler_transitions,
           runtime_state->scheduler_finishes);
    printf("Frame-wait visits/threshold/IRQs:   %zu/%zu/%zu\n",
           runtime_state->frame_wait.visits,
           runtime_state->frame_wait.visits_before_interrupt,
           runtime_state->frame_wait.interrupts_injected);
    printf("Last step:                          %s\n",
           vf2_native_runtime_step_kind_name(
               report->last_cycle.last_step.kind
           ));
    printf("Last entry/exit:                    0x%08x/0x%08x\n",
           (unsigned)report->last_cycle.last_step.entry_address,
           (unsigned)report->last_cycle.last_step.exit_address);
    printf("Last bridge/task:                   %s/%s\n",
           vf2_hybrid_bridge_kind_name(
               report->last_cycle.last_step.bridge_kind
           ),
           vf2_hybrid_task_kind_name(
               report->last_cycle.last_step.task_kind
           ));

    if (!report->last_cycle.diff.equal &&
        report->last_cycle.diff.component[0] != '\0') {
        printf("Difference:                         %s offset=0x%zx "
               "expected=0x%08x actual=0x%08x bytes=%zu\n",
               report->last_cycle.diff.component,
               report->last_cycle.diff.first_offset,
               (unsigned)report->last_cycle.diff.expected_value,
               (unsigned)report->last_cycle.diff.actual_value,
               report->last_cycle.diff.differing_bytes);
    }
}

static vf2_status run_cycles_with_checkpoints(
    vf2_model2a *reference_machine,
    vf2_i960_cpu *reference_cpu,
    vf2_model2a *native_machine,
    vf2_i960_cpu *native_cpu,
    vf2_native_runtime_state *runtime_state,
    uint32_t repeated_address,
    size_t cycle_count,
    size_t minimum_blocks,
    size_t maximum_blocks,
    vf2_i960_snapshot *last_match_snapshot,
    vf2_native_runtime_state *last_match_state,
    vf2_native_differential_cycles_report *report
)
{
    vf2_native_differential_cycles_report local_report;
    vf2_status status = VF2_OK;
    size_t cycle = 0u;

    if (reference_machine == NULL || reference_cpu == NULL ||
        native_machine == NULL || native_cpu == NULL ||
        runtime_state == NULL || last_match_snapshot == NULL ||
        last_match_state == NULL || minimum_blocks > maximum_blocks ||
        (cycle_count != 0u && minimum_blocks == 0u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.repeated_address = repeated_address;
    local_report.requested_cycles = cycle_count;
    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;

    if (cycle_count == 0u) {
        status = vf2_native_differential_run_cycles(
            reference_machine,
            reference_cpu,
            native_machine,
            native_cpu,
            runtime_state,
            repeated_address,
            0u,
            0u,
            0u,
            &local_report
        );
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_capture(
                last_match_snapshot,
                native_cpu,
                native_machine
            );
            *last_match_state = *runtime_state;
        }
    }

    for (cycle = 0u;
         status == VF2_OK && cycle < cycle_count;
         ++cycle) {
        vf2_native_differential_report cycle_report;
        size_t blocks = 0u;

        memset(&cycle_report, 0, sizeof(cycle_report));
        cycle_report.start_address = native_cpu->ip;
        cycle_report.stop_address = repeated_address;
        cycle_report.minimum_blocks = minimum_blocks;
        cycle_report.diff.equal = true;

        while (status == VF2_OK &&
               (native_cpu->ip != repeated_address ||
                blocks < minimum_blocks)) {
            vf2_native_differential_step_report step_report;

            status = vf2_i960_snapshot_capture(
                last_match_snapshot,
                native_cpu,
                native_machine
            );
            if (status != VF2_OK) {
                break;
            }
            *last_match_state = *runtime_state;

            if (blocks >= maximum_blocks) {
                memset(&cycle_report.last_step, 0,
                       sizeof(cycle_report.last_step));
                cycle_report.last_step.entry_address = native_cpu->ip;
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }

            memset(&step_report, 0, sizeof(step_report));
            status = vf2_native_differential_step(
                reference_machine,
                reference_cpu,
                native_machine,
                native_cpu,
                runtime_state,
                &step_report
            );
            cycle_report.last_step = step_report.native_step;
            cycle_report.diff = step_report.diff;
            cycle_report.reference_instructions_executed +=
                step_report.reference_instructions_executed;
            cycle_report.native_recovered_instructions +=
                step_report.native_recovered_instructions;
            if (status != VF2_OK) {
                break;
            }
            ++blocks;
            ++cycle_report.blocks_compared;
        }

        cycle_report.final_reference_address = reference_cpu->ip;
        cycle_report.final_native_address = native_cpu->ip;
        if (status == VF2_OK &&
            reference_cpu->ip == repeated_address &&
            native_cpu->ip == repeated_address &&
            blocks >= minimum_blocks) {
            cycle_report.reached_stop = 1;
            ++local_report.completed_cycles;
        } else if (status == VF2_OK) {
            status = VF2_ERROR_UNSUPPORTED;
        }

        local_report.blocks_compared += cycle_report.blocks_compared;
        local_report.reference_instructions_executed +=
            cycle_report.reference_instructions_executed;
        local_report.native_recovered_instructions +=
            cycle_report.native_recovered_instructions;
        local_report.last_cycle = cycle_report;
    }

    local_report.final_reference_address = reference_cpu->ip;
    local_report.final_native_address = native_cpu->ip;
    if (status == VF2_OK &&
        local_report.completed_cycles == cycle_count) {
        local_report.completed = 1;
    }
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

static vf2_status write_failure_report(
    const char *path,
    vf2_status failure_status,
    const vf2_i960_snapshot *checkpoint,
    const vf2_native_runtime_state *runtime_state,
    const vf2_native_differential_cycles_report *report
)
{
    FILE *file = NULL;

    file = fopen(path, "w");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }
    if (fprintf(
            file,
            "status=%s\n"
            "checkpoint_ip=0x%08x\n"
            "completed_cycles=%zu\n"
            "compared_blocks=%zu\n"
            "runtime_blocks=%zu\n"
            "frame_wait_visits=%zu\n"
            "frame_wait_threshold=%zu\n"
            "frame_wait_interrupts=%zu\n"
            "last_step_kind=%s\n"
            "last_step_entry=0x%08x\n"
            "last_step_exit=0x%08x\n"
            "last_bridge=%s\n"
            "last_task=%s\n"
            "diff_component=%s\n"
            "diff_offset=0x%zx\n"
            "diff_expected=0x%08x\n"
            "diff_actual=0x%08x\n"
            "diff_bytes=%zu\n",
            vf2_status_string(failure_status),
            (unsigned)checkpoint->cpu.ip,
            report->completed_cycles,
            report->blocks_compared,
            runtime_state->blocks_executed,
            runtime_state->frame_wait.visits,
            runtime_state->frame_wait.visits_before_interrupt,
            runtime_state->frame_wait.interrupts_injected,
            vf2_native_runtime_step_kind_name(
                report->last_cycle.last_step.kind
            ),
            (unsigned)report->last_cycle.last_step.entry_address,
            (unsigned)report->last_cycle.last_step.exit_address,
            vf2_hybrid_bridge_kind_name(
                report->last_cycle.last_step.bridge_kind
            ),
            vf2_hybrid_task_kind_name(
                report->last_cycle.last_step.task_kind
            ),
            report->last_cycle.diff.component,
            report->last_cycle.diff.first_offset,
            (unsigned)report->last_cycle.diff.expected_value,
            (unsigned)report->last_cycle.diff.actual_value,
            report->last_cycle.diff.differing_bytes
        ) < 0) {
        (void)fclose(file);
        return VF2_ERROR_IO;
    }
    if (fclose(file) != 0) {
        return VF2_ERROR_IO;
    }
    return VF2_OK;
}

static vf2_status save_failure_checkpoint(
    const char *prefix,
    vf2_status failure_status,
    const vf2_i960_snapshot *checkpoint,
    const vf2_native_runtime_state *runtime_state,
    const vf2_native_differential_cycles_report *report
)
{
    char *snapshot_path = append_suffix(prefix, ".vf2snap");
    char *state_path = append_suffix(prefix, ".runtime");
    char *report_path = append_suffix(prefix, ".txt");
    vf2_status status = VF2_OK;

    if (snapshot_path == NULL || state_path == NULL || report_path == NULL) {
        status = VF2_ERROR_OUT_OF_MEMORY;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_write_file(checkpoint, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_state_write_file(runtime_state, state_path);
    }
    if (status == VF2_OK) {
        status = write_failure_report(
            report_path,
            failure_status,
            checkpoint,
            runtime_state,
            report
        );
    }

    if (status == VF2_OK) {
        printf("Failure checkpoint snapshot:         %s\n", snapshot_path);
        printf("Failure checkpoint runtime:          %s\n", state_path);
        printf("Failure checkpoint report:           %s\n", report_path);
    } else {
        if (snapshot_path != NULL) {
            (void)remove(snapshot_path);
        }
        if (state_path != NULL) {
            (void)remove(state_path);
        }
        if (report_path != NULL) {
            (void)remove(report_path);
        }
    }

    free(report_path);
    free(state_path);
    free(snapshot_path);
    return status;
}

int main(int argc, char **argv)
{
    vf2_cycles_options options;
    vf2_verify_summary verify_summary;
    vf2_i960_snapshot snapshot;
    vf2_i960_snapshot last_match_snapshot;
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_native_runtime_state runtime_state;
    vf2_native_runtime_state last_match_state;
    vf2_native_differential_cycles_report report;
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;
    uint8_t *main_data = NULL;
    size_t main_data_size = 0u;
    char *derived_state_path = NULL;
    const char *runtime_state_path = NULL;
    int reference_initialized = 0;
    int native_initialized = 0;
    vf2_status status = VF2_OK;
    vf2_status run_status = VF2_OK;

    memset(&verify_summary, 0, sizeof(verify_summary));
    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&reference_cpu, 0, sizeof(reference_cpu));
    memset(&native_cpu, 0, sizeof(native_cpu));
    memset(&runtime_state, 0, sizeof(runtime_state));
    memset(&last_match_state, 0, sizeof(last_match_state));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&snapshot);
    vf2_i960_snapshot_init(&last_match_snapshot);

    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    runtime_state_path = options.runtime_state_path;
    if (runtime_state_path == NULL) {
        derived_state_path = append_suffix(options.snapshot_path, ".runtime");
        if (derived_state_path == NULL) {
            status = VF2_ERROR_OUT_OF_MEMORY;
        } else {
            runtime_state_path = derived_state_path;
        }
    }

    if (status == VF2_OK) {
        status = vf2_romset_verify(
            options.rom_directory,
            NULL,
            &verify_summary
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            options.rom_directory,
            VF2_REGION_MAINCPU,
            &main_rom,
            &main_rom_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            options.rom_directory,
            VF2_REGION_MAIN_DATA,
            &main_data,
            &main_data_size
        );
    }
    if (status == VF2_OK) {
        status = initialize_machine(
            &reference_machine,
            main_rom,
            main_rom_size,
            main_data,
            main_data_size
        );
        reference_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = initialize_machine(
            &native_machine,
            main_rom,
            main_rom_size,
            main_data,
            main_data_size
        );
        native_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(
            &snapshot,
            options.snapshot_path
        );
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_state_read_file(
            &runtime_state,
            runtime_state_path
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &snapshot,
            &reference_cpu,
            &reference_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &snapshot,
            &native_cpu,
            &native_machine
        );
    }
    if (status == VF2_OK) {
        run_status = run_cycles_with_checkpoints(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &runtime_state,
            snapshot.cpu.ip,
            options.cycle_count,
            options.minimum_blocks,
            options.maximum_blocks,
            &last_match_snapshot,
            &last_match_state,
            &report
        );
        status = run_status;
    }

    print_report(&report, &runtime_state);
    if (run_status == VF2_OK && status == VF2_OK) {
        printf("Endurance result:                    MATCH\n");
    } else if (status != VF2_OK) {
        fprintf(
            stderr,
            "Endurance stopped: %s\n",
            vf2_status_string(status)
        );
    }

    if (run_status != VF2_OK && options.failure_prefix != NULL &&
        last_match_snapshot.cpu.ip != 0u) {
        const vf2_status checkpoint_status = save_failure_checkpoint(
            options.failure_prefix,
            run_status,
            &last_match_snapshot,
            &last_match_state,
            &report
        );
        if (checkpoint_status != VF2_OK) {
            fprintf(
                stderr,
                "Could not write failure checkpoint: %s\n",
                vf2_status_string(checkpoint_status)
            );
            status = checkpoint_status;
        }
    }

    free(derived_state_path);
    vf2_i960_snapshot_destroy(&last_match_snapshot);
    vf2_i960_snapshot_destroy(&snapshot);
    if (native_initialized) {
        vf2_model2a_shutdown(&native_machine);
    }
    if (reference_initialized) {
        vf2_model2a_shutdown(&reference_machine);
    }
    free(main_data);
    free(main_rom);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
