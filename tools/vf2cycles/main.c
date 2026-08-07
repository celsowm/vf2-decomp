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
    size_t cycle_count;
    size_t minimum_blocks;
    size_t maximum_blocks;
    size_t frame_wait_visits;
} vf2_cycles_options;

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "vf2cycles v%s\n"
        "Usage: %s --rom-dir <directory> --snapshot <file> "
        "[--cycles <count>] [--min-blocks <count>] "
        "[--max-blocks <count>] [--frame-wait-visits <count>]\n",
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
    options->frame_wait_visits = 4u;

    while (index < argc) {
        const char *argument = argv[index];

        if (strcmp(argument, "--rom-dir") == 0 && index + 1 < argc) {
            options->rom_directory = argv[++index];
        } else if (strcmp(argument, "--snapshot") == 0 &&
                   index + 1 < argc) {
            options->snapshot_path = argv[++index];
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
        } else if (strcmp(argument, "--frame-wait-visits") == 0 &&
                   index + 1 < argc) {
            if (!parse_size(argv[++index], &options->frame_wait_visits)) {
                return 0;
            }
        } else {
            return 0;
        }
        ++index;
    }

    return options->rom_directory != NULL &&
           options->snapshot_path != NULL &&
           options->frame_wait_visits != 0u &&
           options->minimum_blocks <= options->maximum_blocks &&
           (options->cycle_count == 0u || options->minimum_blocks != 0u);
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
    printf("Frame-wait phases:                  %zu\n",
           runtime_state->frame_wait_phases);
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

int main(int argc, char **argv)
{
    vf2_cycles_options options;
    vf2_verify_summary verify_summary;
    vf2_i960_snapshot snapshot;
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_native_runtime_state runtime_state;
    vf2_native_differential_cycles_report report;
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;
    uint8_t *main_data = NULL;
    size_t main_data_size = 0u;
    int reference_initialized = 0;
    int native_initialized = 0;
    vf2_status status = VF2_OK;

    memset(&verify_summary, 0, sizeof(verify_summary));
    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&reference_cpu, 0, sizeof(reference_cpu));
    memset(&native_cpu, 0, sizeof(native_cpu));
    memset(&runtime_state, 0, sizeof(runtime_state));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&snapshot);

    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    status = vf2_romset_verify(
        options.rom_directory,
        NULL,
        &verify_summary
    );
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
        status = vf2_native_runtime_initialize(
            &runtime_state,
            options.frame_wait_visits
        );
    }
    if (status == VF2_OK) {
        status = vf2_native_differential_run_cycles(
            &reference_machine,
            &reference_cpu,
            &native_machine,
            &native_cpu,
            &runtime_state,
            snapshot.cpu.ip,
            options.cycle_count,
            options.minimum_blocks,
            options.maximum_blocks,
            &report
        );
    }

    print_report(&report, &runtime_state);
    if (status == VF2_OK) {
        printf("Endurance result:                    MATCH\n");
    } else {
        fprintf(
            stderr,
            "Endurance stopped: %s\n",
            vf2_status_string(status)
        );
    }

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
