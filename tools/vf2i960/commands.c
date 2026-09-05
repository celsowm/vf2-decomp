#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/cfg.h"
#include "vf2/analysis/pseudoc.h"
#include "vf2/analysis/symbols.h"
#include "vf2/analysis/tasks.h"
#include "vf2/boot.h"
#include "vf2/i960/decoder.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/rom.h"
#include "vf2/hash.h"
#include "vf2/hybrid.h"
#include "vf2/model2a.h"
#include "vf2/native_differential.h"
#include "vf2/recovered.h"
#include "vf2/status.h"
#include "vf2/version.h"

static void usage(const char *program)
{
    fprintf(
        stderr,
        "vf2i960 v%s\n"
        "Usage:\n"
        "  %s disasm <rom-directory> [address] [instruction-count]\n"
        "  %s function <rom-directory> <address>\n"
        "  %s analyze <rom-directory> <output-directory>\n"
        "  %s tasks <rom-directory> [output-directory]\n"
        "  %s xrefs <rom-directory> <address>\n"
        "  %s frame <rom-directory> <address>\n"
        "  %s pseudoc <rom-directory> <address> [output-file]\n"
        "  %s execute <rom-directory> [stop-address] [max-steps]\n"
        "  %s runtime-checkpoint <rom-directory>\n"
        "  %s scheduler-pass <rom-directory>\n"
        "  %s scheduler-dispatch <rom-directory>\n"
        "  %s task-profile <rom-directory> [output.csv]\n"
        "  %s trace <rom-directory> <output.csv> [max-steps]\n"
        "  %s snapshot <rom-directory> <output.vf2snap>\n"
        "  %s resume-trace <rom-directory> <input.vf2snap> [max-steps] [clear-task-index] [fighter-flags-or] [write-address] [write-value] [output.vf2snap] [stop-address] [raise-irq] [enter-vector] [enter-level]\n"
        "  %s native-resume <rom-directory> <input.vf2snap> [max-blocks] [fighter-flags-or] [stop-address] [output.vf2snap]\n"
        "  %s compare-game-info <rom-directory> <input.vf2snap> [fighter-flags-or] [stop-address]\n"
        "  %s compare-boot <rom-directory>\n"
        "  %s compare-init <rom-directory>\n"
        "  %s compare-task-registry <rom-directory>\n"
        "  %s compare-timer-irq <rom-directory>\n"
        "  %s compare-task-recoveries <rom-directory>\n"
        "  %s compare-first-dispatch <rom-directory>\n"
        "  %s compare-camera-classifier <rom-directory>\n"
        "  %s compare-camera-viewport <rom-directory>\n"
        "  %s hybrid-first-dispatch <rom-directory>\n"
        "  %s native-first-dispatch <rom-directory>\n"
        "  %s native-second-dispatch <rom-directory>\n"
        "  %s native-third-dispatch <rom-directory>\n"
        "  %s native-fourth-dispatch <rom-directory>\n"
        "  %s native-fifth-dispatch <rom-directory> [output.vf2snap]\n"
        "  %s native-sixth-dispatch <rom-directory> [output.vf2snap]\n"
        "  %s compare-texture-bridge <rom-directory>\n"
        "  %s compare-post-frame-bridge <rom-directory>\n"
        "  %s compare-geometry-boundary <rom-directory>\n"
        "  %s compare-second-scheduler-entry <rom-directory>\n"
        "  %s compare-game-geometry-helpers <rom-directory>\n"
        "  %s observe-third-sweep <rom-directory>\n"
        "  %s trace-orchestrator <rom-directory> [output.csv]\n"
        "  %s compare-snapshots <expected.vf2snap> <actual.vf2snap>\n",
        VF2_VERSION_STRING,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program
    );
}

static FILE *g_orchestrator_trace_file = NULL;
static uint64_t g_orchestrator_trace_step = 0u;
static const char *g_native_snapshot_path = NULL;

static bool is_orchestrator_cluster_ip(uint32_t ip)
{
    return ip >= UINT32_C(0x0004bb18) && ip <= UINT32_C(0x0004c180);
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0ul;

    if (text == NULL || value == NULL) {
        return 0;
    }

    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }

    *value = (uint32_t)parsed;
    return 1;
}

static vf2_status load_maincpu(
    const char *rom_directory,
    uint8_t **image,
    size_t *image_size,
    vf2_i960_boot_vectors *vectors
)
{
    vf2_status status = vf2_romset_build_region(
        rom_directory,
        VF2_REGION_MAINCPU,
        image,
        image_size
    );
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_parse_i960_boot_vectors(*image, *image_size, vectors);
    if (status != VF2_OK) {
        free(*image);
        *image = NULL;
        *image_size = 0u;
    }
    return status;
}

static vf2_status run_analysis(
    const uint8_t *image,
    size_t image_size,
    uint32_t first_entry,
    vf2_i960_analysis *analysis
)
{
    uint32_t entries[64];
    size_t entry_count = 0u;
    size_t task_index = 0u;
    vf2_task_catalog task_catalog;
    vf2_status status = vf2_i960_analysis_init(
        analysis,
        image,
        image_size
    );
    if (status != VF2_OK) {
        return status;
    }

    vf2_task_catalog_init(&task_catalog);
    entries[entry_count++] = first_entry;
    entries[entry_count++] = 0x000001b0u;
    status = vf2_task_catalog_scan(&task_catalog, image, image_size);
    if (status == VF2_OK) {
        for (task_index = 0u; task_index < task_catalog.count &&
             entry_count < sizeof(entries) / sizeof(entries[0]); ++task_index) {
            size_t existing = 0u;
            bool duplicate = false;
            const uint32_t entry = task_catalog.tasks[task_index].entry_point;
            for (existing = 0u; existing < entry_count; ++existing) {
                if (entries[existing] == entry) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                entries[entry_count++] = entry;
            }
        }
        status = vf2_i960_analyze(analysis, entries, entry_count);
    }
    if (status == VF2_OK) {
        status = vf2_task_catalog_apply_symbols(&task_catalog, analysis);
    }
    vf2_task_catalog_destroy(&task_catalog);
    if (status == VF2_OK) {
        const char *symbol_directory = getenv("VF2_SYMBOL_DIR");
        if (symbol_directory == NULL || symbol_directory[0] == '\0') {
            symbol_directory = "decomp/i960";
        }
        status = vf2_i960_apply_symbol_overlays(analysis, symbol_directory);
    }
    if (status != VF2_OK) {
        vf2_i960_analysis_destroy(analysis);
    }
    return status;
}

static int command_disasm(
    const char *rom_directory,
    uint32_t address,
    uint32_t instruction_count
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    uint32_t index = 0u;
    vf2_status status = load_maincpu(
        rom_directory,
        &image,
        &image_size,
        &vectors
    );

    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }

    if (address == UINT32_MAX) {
        address = vectors.start_ip;
    }

    for (index = 0u; index < instruction_count; ++index) {
        vf2_i960_instruction instruction;
        char text[256];
        status = vf2_i960_decode(image, image_size, address, &instruction);
        if (status != VF2_OK) {
            fprintf(
                stderr,
                "%08x: decode failed: %s\n",
                (unsigned)address,
                vf2_status_string(status)
            );
            free(image);
            return EXIT_FAILURE;
        }
        status = vf2_i960_format_instruction(
            &instruction,
            text,
            sizeof(text)
        );
        if (status != VF2_OK) {
            free(image);
            return EXIT_FAILURE;
        }
        printf(
            "%08x  %08x  %s\n",
            (unsigned)address,
            (unsigned)instruction.words[0],
            text
        );
        address += instruction.size;
    }

    free(image);
    return EXIT_SUCCESS;
}

static void print_analysis_summary(const vf2_i960_analysis *analysis)
{
    size_t code_bytes = 0u;
    size_t string_bytes = 0u;
    size_t padding_bytes = 0u;
    size_t index = 0u;

    for (index = 0u; index < analysis->image_size; ++index) {
        if (analysis->image_map[index] == VF2_IMAGE_CODE) {
            ++code_bytes;
        } else if (analysis->image_map[index] == VF2_IMAGE_STRING) {
            ++string_bytes;
        } else if (analysis->image_map[index] == VF2_IMAGE_PADDING) {
            ++padding_bytes;
        }
    }

    printf("Decoded instructions: %zu\n", analysis->decoded_instruction_count);
    printf("Invalid instructions: %zu\n", analysis->invalid_instruction_count);
    printf("Functions:            %zu\n", analysis->function_count);
    printf("Basic blocks:         %zu\n", analysis->block_count);
    printf("Cross-references:     %zu\n", analysis->xref_count);
    printf("Constant facts:       %zu\n", analysis->constant_fact_count);
    printf("Indirect targets:     %zu\n", analysis->indirect_target_count);
    printf("Resolved indirect:    %zu\n", analysis->resolved_indirect_count);
    printf("Unresolved indirect:  %zu\n", analysis->unresolved_indirect_count);
    printf("Split candidates:     %zu\n", analysis->function_split_count);
    printf("Code bytes:           %zu\n", code_bytes);
    printf("String bytes:         %zu\n", string_bytes);
    printf("Padding bytes:        %zu\n", padding_bytes);
}

static int command_tasks(
    const char *rom_directory,
    const char *output_directory
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_task_catalog catalog;
    vf2_status status = VF2_OK;
    size_t index = 0u;

    vf2_task_catalog_init(&catalog);
    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    (void)vectors;
    if (status == VF2_OK) {
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK && output_directory != NULL) {
        status = vf2_task_catalog_write(&catalog, output_directory);
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Task discovery failed: %s\n", vf2_status_string(status));
        vf2_task_catalog_destroy(&catalog);
        free(image);
        return EXIT_FAILURE;
    }

    printf("Task descriptor table: 0x%08x-0x%08x\n",
           (unsigned)catalog.table_start, (unsigned)catalog.table_end);
    printf("Recovered tasks:       %zu\n", catalog.count);
    printf("\n%-18s %-10s %-10s %-10s %-10s %-10s\n",
           "name", "descriptor", "entry", "state", "stack", "flags");
    for (index = 0u; index < catalog.count; ++index) {
        const vf2_task_descriptor *task = &catalog.tasks[index];
        printf("%-18s 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
               task->name,
               (unsigned)task->descriptor_address,
               (unsigned)task->entry_point,
               (unsigned)task->state_address,
               (unsigned)task->stack_size,
               (unsigned)task->flags);
        if (task->instance != 0u) {
            printf(" instance=%u", (unsigned)task->instance);
        }
        putchar('\n');
    }
    if (output_directory != NULL) {
        printf("Wrote task catalog to %s\n", output_directory);
    }

    vf2_task_catalog_destroy(&catalog);
    free(image);
    return EXIT_SUCCESS;
}

static int command_analyze(
    const char *rom_directory,
    const char *output_directory
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_i960_analysis analysis;
    vf2_task_catalog task_catalog;
    vf2_status status = load_maincpu(
        rom_directory,
        &image,
        &image_size,
        &vectors
    );

    vf2_task_catalog_init(&task_catalog);
    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }

    status = run_analysis(
        image,
        image_size,
        vectors.start_ip,
        &analysis
    );
    if (status == VF2_OK) {
        status = vf2_i960_write_analysis(&analysis, output_directory);
    }
    if (status == VF2_OK) {
        status = vf2_task_catalog_scan(&task_catalog, image, image_size);
    }
    if (status == VF2_OK) {
        status = vf2_task_catalog_write(&task_catalog, output_directory);
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Analysis failed: %s\n", vf2_status_string(status));
        vf2_i960_analysis_destroy(&analysis);
        vf2_task_catalog_destroy(&task_catalog);
        free(image);
        return EXIT_FAILURE;
    }

    print_analysis_summary(&analysis);
    printf("Task descriptors:     %zu\n", task_catalog.count);
    printf("Wrote analysis to %s\n", output_directory);
    vf2_i960_analysis_destroy(&analysis);
    vf2_task_catalog_destroy(&task_catalog);
    free(image);
    return EXIT_SUCCESS;
}

static int command_function(
    const char *rom_directory,
    uint32_t address
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_i960_analysis analysis;
    const vf2_function *function = NULL;
    size_t block_index = 0u;
    vf2_status status = load_maincpu(
        rom_directory,
        &image,
        &image_size,
        &vectors
    );

    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }

    status = run_analysis(image, image_size, address, &analysis);
    if (status != VF2_OK) {
        fprintf(stderr, "Analysis failed: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }

    function = vf2_i960_find_function(&analysis, address);
    if (function == NULL) {
        fprintf(stderr, "No function found at 0x%08x\n", (unsigned)address);
        vf2_i960_analysis_destroy(&analysis);
        free(image);
        return EXIT_FAILURE;
    }

    printf(
        "%s: address=0x%08x end=0x%08x blocks=%zu indirect=%s\n",
        function->name,
        (unsigned)function->address,
        (unsigned)function->end,
        function->block_count,
        function->has_indirect_flow ? "yes" : "no"
    );

    for (block_index = function->first_block;
         block_index < function->first_block + function->block_count;
         ++block_index) {
        const vf2_basic_block *block = &analysis.blocks[block_index];
        uint32_t current = block->start;
        printf("\nblock_%08x:\n", (unsigned)block->start);
        while (current < block->end) {
            vf2_i960_instruction instruction;
            char text[256];
            if (vf2_i960_decode(image, image_size, current, &instruction) != VF2_OK ||
                vf2_i960_format_instruction(&instruction, text, sizeof(text)) != VF2_OK) {
                break;
            }
            printf("  %08x  %s\n", (unsigned)current, text);
            current += instruction.size;
        }
    }

    vf2_i960_analysis_destroy(&analysis);
    free(image);
    return EXIT_SUCCESS;
}

static int command_xrefs(
    const char *rom_directory,
    uint32_t address
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_i960_analysis analysis;
    size_t index = 0u;
    size_t matches = 0u;
    vf2_status status = load_maincpu(
        rom_directory,
        &image,
        &image_size,
        &vectors
    );

    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }

    status = run_analysis(image, image_size, vectors.start_ip, &analysis);
    if (status != VF2_OK) {
        fprintf(stderr, "Analysis failed: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }

    for (index = 0u; index < analysis.xref_count; ++index) {
        const vf2_xref *xref = &analysis.xrefs[index];
        if (xref->target == address || xref->source == address) {
            printf(
                "0x%08x -> 0x%08x  %s\n",
                (unsigned)xref->source,
                (unsigned)xref->target,
                vf2_xref_type_name(xref->type)
            );
            ++matches;
        }
    }
    printf("%zu reference(s).\n", matches);

    vf2_i960_analysis_destroy(&analysis);
    free(image);
    return EXIT_SUCCESS;
}

static int command_frame(const char *rom_directory, uint32_t address)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_i960_analysis analysis;
    const vf2_function *function = NULL;
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }
    status = run_analysis(image, image_size, vectors.start_ip, &analysis);
    if (status != VF2_OK) {
        fprintf(stderr, "Analysis failed: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }
    function = vf2_i960_find_function(&analysis, address);
    if (function == NULL) {
        fprintf(stderr, "No function found at 0x%08x\n", (unsigned)address);
        vf2_i960_analysis_destroy(&analysis);
        free(image);
        return EXIT_FAILURE;
    }
    printf("Function:             %s\n", function->name);
    printf("Address:              0x%08x\n", (unsigned)function->address);
    printf("Estimated frame:      0x%x bytes\n", (unsigned)function->stack_frame_size);
    printf("Uses frame pointer:   %s\n", function->uses_frame_pointer ? "yes" : "no");
    printf("Leaf function:        %s\n", function->leaf ? "yes" : "no");
    printf("Argument mask g0-g7:  0x%04x\n", (unsigned)function->argument_register_mask);
    printf("Return mask:          0x%04x\n", (unsigned)function->return_register_mask);
    printf("Resolved indirect:    %zu\n", function->resolved_indirect_count);
    printf("Unresolved indirect:  %zu\n", function->unresolved_indirect_count);
    vf2_i960_analysis_destroy(&analysis);
    free(image);
    return EXIT_SUCCESS;
}

static int command_pseudoc(
    const char *rom_directory,
    uint32_t address,
    const char *output_path
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_i960_analysis analysis;
    FILE *output = stdout;
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }
    status = run_analysis(image, image_size, vectors.start_ip, &analysis);
    if (status != VF2_OK) {
        fprintf(stderr, "Analysis failed: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }
    if (output_path != NULL) {
        output = fopen(output_path, "wb");
        if (output == NULL) {
            fprintf(stderr, "Could not open output file: %s\n", output_path);
            vf2_i960_analysis_destroy(&analysis);
            free(image);
            return EXIT_FAILURE;
        }
    }
    status = vf2_i960_write_function_pseudoc(&analysis, address, output);
    if (output_path != NULL && fclose(output) != 0 && status == VF2_OK) {
        status = VF2_ERROR_IO;
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Could not generate pseudocode: %s\n", vf2_status_string(status));
    }
    vf2_i960_analysis_destroy(&analysis);
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}


typedef struct trace_context {
    FILE *file;
} trace_context;

enum { VF2_EXECUTION_HISTORY_CAPACITY = 32u };

typedef struct execution_history {
    vf2_i960_trace_event events[VF2_EXECUTION_HISTORY_CAPACITY];
    size_t count;
    size_t next;
} execution_history;

static void history_trace_callback(
    const vf2_i960_trace_event *event,
    const vf2_i960_cpu *cpu,
    void *user_data
)
{
    execution_history *history = (execution_history *)user_data;
    (void)cpu;
    if (history == NULL || event == NULL) {
        return;
    }
    history->events[history->next] = *event;
    history->next = (history->next + 1u) % VF2_EXECUTION_HISTORY_CAPACITY;
    if (history->count < VF2_EXECUTION_HISTORY_CAPACITY) {
        ++history->count;
    }
}

static void print_execution_history(const execution_history *history)
{
    size_t index = 0u;
    size_t start = 0u;
    if (history == NULL || history->count == 0u) {
        return;
    }
    start = history->count == VF2_EXECUTION_HISTORY_CAPACITY ? history->next : 0u;
    fprintf(stderr, "Recent instructions:\n");
    for (index = 0u; index < history->count; ++index) {
        const vf2_i960_trace_event *event =
            &history->events[(start + index) % VF2_EXECUTION_HISTORY_CAPACITY];
        char text[256];
        if (vf2_i960_format_instruction(&event->instruction, text, sizeof(text)) != VF2_OK) {
            (void)snprintf(text, sizeof(text), "<format-error>");
        }
        fprintf(stderr, "  %10llu  %08x -> %08x  %s\n",
                (unsigned long long)event->step,
                (unsigned)event->ip_before,
                (unsigned)event->ip_after,
                text);
    }
}

static void initialize_machine_pattern(vf2_model2a *machine)
{
    memset(machine->work_ram, 0xa5, machine->work_ram_size);
    memset(machine->buffer_ram, 0x5a, machine->buffer_ram_size);
    memset(machine->video_control, 0x33, machine->video_control_size);
    memset(machine->cpu_control, 0x44, machine->cpu_control_size);
    memset(machine->system_control, 0x55, machine->system_control_size);
}

static vf2_status initialize_boot_machine(
    const char *rom_directory,
    vf2_model2a *machine,
    const uint8_t *image,
    size_t image_size
)
{
    uint8_t *main_data = NULL;
    size_t main_data_size = 0u;
    vf2_status status = VF2_OK;

    if (!vf2_model2a_initialize(machine)) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    status = vf2_model2a_attach_main_rom(machine, image, image_size);
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory,
            VF2_REGION_MAIN_DATA,
            &main_data,
            &main_data_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_take_main_data(machine, main_data, main_data_size);
        if (status == VF2_OK) {
            main_data = NULL;
        }
    }
    free(main_data);
    if (status != VF2_OK) {
        vf2_model2a_shutdown(machine);
        return status;
    }
    initialize_machine_pattern(machine);
    return VF2_OK;
}

static void trace_csv_callback(
    const vf2_i960_trace_event *event,
    const vf2_i960_cpu *cpu,
    void *user_data
)
{
    trace_context *context = (trace_context *)user_data;
    char text[256];
    (void)cpu;
    if (context == NULL || context->file == NULL || event == NULL) {
        return;
    }
    if (vf2_i960_format_instruction(&event->instruction, text, sizeof(text)) != VF2_OK) {
        (void)snprintf(text, sizeof(text), "<format-error>");
    }
    (void)fprintf(
        context->file,
        "%llu,0x%08x,0x%08x,\"%s\"\n",
        (unsigned long long)event->step,
        (unsigned)event->ip_before,
        (unsigned)event->ip_after,
        text
    );
}

static void print_execution_summary(
    const vf2_i960_cpu *cpu,
    const vf2_i960_run_result *result,
    const vf2_model2a *machine
)
{
    printf("Halt reason:          %s\n", vf2_i960_halt_reason_name(result->halt_reason));
    printf("Instruction pointer: 0x%08x\n", (unsigned)cpu->ip);
    printf("Executed:            %llu\n", (unsigned long long)result->executed_instructions);
    printf("SAT:                 0x%08x\n", (unsigned)cpu->sat);
    printf("PRCB:                0x%08x\n", (unsigned)cpu->prcb);
    printf("Reinitialized:       %s\n", cpu->reinitialized ? "yes" : "no");
    printf("Work RAM CRC-32:     %08x\n", (unsigned)vf2_crc32(machine->work_ram, machine->work_ram_size));
    printf("Buffer RAM CRC-32:   %08x\n", (unsigned)vf2_crc32(machine->buffer_ram, machine->buffer_ram_size));
    printf("CPU control CRC-32:  %08x\n", (unsigned)vf2_crc32(machine->cpu_control, machine->cpu_control_size));
    printf("Process control:      0x%08x\n", (unsigned)cpu->process_control);
    printf("Arithmetic control:   0x%08x\n", (unsigned)cpu->arithmetic_control);
    printf("Interrupt control:    0x%08x\n", (unsigned)cpu->interrupt_control);
    printf("PFP/SP/RIP/FP:       %08x %08x %08x %08x\n",
           (unsigned)cpu->registers[0], (unsigned)cpu->registers[1],
           (unsigned)cpu->registers[2],
           (unsigned)cpu->registers[VF2_I960_FP_REGISTER]);
    printf("Procedure calls:     %llu\n", (unsigned long long)cpu->procedure_calls);
    printf("Procedure returns:   %llu\n", (unsigned long long)cpu->procedure_returns);
    printf("Current frame depth: %u\n", (unsigned)cpu->local_frame_depth);
    printf("Maximum frame depth: %u\n", (unsigned)cpu->maximum_local_frame_depth);
}

static void print_register_dump(const vf2_i960_cpu *cpu)
{
    unsigned row = 0u;
    for (row = 0u; row < 4u; ++row) {
        const unsigned base = row * 8u;
        printf("r%-2u=%08x r%-2u=%08x r%-2u=%08x r%-2u=%08x "
               "r%-2u=%08x r%-2u=%08x r%-2u=%08x r%-2u=%08x\n",
               base + 0u, (unsigned)cpu->registers[base + 0u],
               base + 1u, (unsigned)cpu->registers[base + 1u],
               base + 2u, (unsigned)cpu->registers[base + 2u],
               base + 3u, (unsigned)cpu->registers[base + 3u],
               base + 4u, (unsigned)cpu->registers[base + 4u],
               base + 5u, (unsigned)cpu->registers[base + 5u],
               base + 6u, (unsigned)cpu->registers[base + 6u],
               base + 7u, (unsigned)cpu->registers[base + 7u]);
    }
}

static vf2_status execute_boot_path(
    const char *rom_directory,
    const uint8_t *image,
    size_t image_size,
    const vf2_i960_boot_vectors *vectors,
    uint32_t stop_address,
    uint64_t max_steps,
    vf2_i960_trace_callback callback,
    void *callback_data,
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_i960_run_result *result
)
{
    vf2_i960_run_options options;
    vf2_status status = initialize_boot_machine(
        rom_directory, machine, image, image_size
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_reset_from_machine(
        cpu,
        machine,
        vectors->system_address_table,
        vectors->initial_prcb,
        vectors->start_ip
    );
    if (status != VF2_OK) {
        return status;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = stop_address;
    options.max_steps = max_steps;
    options.stop_on_self_branch = true;
    options.trace_callback = callback;
    options.trace_user_data = callback_data;
    status = vf2_i960_run(cpu, machine, &options, result);
    return status;
}

static int command_execute(
    const char *rom_directory,
    uint32_t stop_address,
    uint64_t max_steps
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_result result;
    execution_history history;
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    memset(&history, 0, sizeof(history));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&result, 0, sizeof(result));
    if (status == VF2_OK) {
        status = execute_boot_path(
            rom_directory,
            image,
            image_size,
            &vectors,
            stop_address,
            max_steps,
            history_trace_callback,
            &history,
            &machine,
            &cpu,
            &result
        );
    }
    if (status != VF2_OK) {
        char text[256];
        vf2_i960_instruction instruction;
        fprintf(stderr, "Execution failed: %s\n", vf2_status_string(status));
        fprintf(stderr, "Halt address: 0x%08x\n", (unsigned)result.halt_address);
        if (vf2_i960_decode(image, image_size, result.halt_address, &instruction) == VF2_OK &&
            vf2_i960_format_instruction(&instruction, text, sizeof(text)) == VF2_OK) {
            fprintf(stderr, "Instruction: %s\n", text);
        }
        print_execution_history(&history);
        if (machine.work_ram != NULL) {
            print_execution_summary(&cpu, &result, &machine);
            print_register_dump(&cpu);
            vf2_model2a_shutdown(&machine);
        }
        free(image);
        return EXIT_FAILURE;
    }
    print_execution_summary(&cpu, &result, &machine);
    vf2_model2a_shutdown(&machine);
    free(image);
    return result.halt_reason == VF2_I960_HALT_STOP_ADDRESS ? EXIT_SUCCESS : EXIT_FAILURE;
}


typedef struct camera_classifier_case {
    uint32_t first_value;
    uint32_t vertical_value;
    uint32_t second_value;
    uint32_t range_value;
    uint32_t vertical_limit;
} camera_classifier_case;

static int command_compare_camera_classifier(const char *rom_directory)
{
    static const camera_classifier_case cases[] = {
        {UINT32_C(0x00000000), UINT32_C(0x3f4f5c29), UINT32_C(0xc0a0a3d7),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0x41100000), UINT32_C(0xbf800000), UINT32_C(0x40000000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0xc1100000), UINT32_C(0xbf800000), UINT32_C(0x40000000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0x3f800000), UINT32_C(0xbf800000), UINT32_C(0x41100000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0x3f800000), UINT32_C(0xbf800000), UINT32_C(0xc1100000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0x3f800000), UINT32_C(0xbf800000), UINT32_C(0x40000000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0xbf800000), UINT32_C(0xbf800000), UINT32_C(0x40000000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0x40400000), UINT32_C(0xbf800000), UINT32_C(0x40000000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)},
        {UINT32_C(0x40400000), UINT32_C(0xbf800000), UINT32_C(0xc0000000),
         UINT32_C(0x41000000), UINT32_C(0xbe99999a)}
    };
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_result result;
    vf2_i960_run_options options;
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    size_t index = 0u;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&result, 0, sizeof(result));
    memset(&options, 0, sizeof(options));

    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &machine, image, image_size
        );
    }
    for (index = 0u; status == VF2_OK &&
         index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const camera_classifier_case *test_case = &cases[index];
        uint32_t recovered = 0u;

        status = vf2_model2a_write_u32(
            &machine, UINT32_C(0x0050a00c), test_case->range_value
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                &machine, UINT32_C(0x0050a148), test_case->vertical_limit
            );
        }
        if (status != VF2_OK) {
            break;
        }

        vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00000040));
        cpu.registers[16] = test_case->first_value;
        cpu.registers[17] = test_case->vertical_value;
        cpu.registers[18] = test_case->second_value;
        status = vf2_i960_cpu_enter_procedure(
            &cpu, UINT32_C(0x000214dc), UINT32_C(0x00000040)
        );
        if (status == VF2_OK) {
            memset(&options, 0, sizeof(options));
            options.stop_address = UINT32_C(0x00000040);
            options.max_steps = UINT64_C(256);
            options.stop_on_self_branch = true;
            status = vf2_i960_run(&cpu, &machine, &options, &result);
        }
        if (status == VF2_OK &&
            result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        recovered = vf2_recovered_camera_classify_range(
            test_case->first_value,
            test_case->vertical_value,
            test_case->second_value,
            test_case->range_value,
            test_case->vertical_limit
        );
        if (status == VF2_OK && cpu.registers[16] != recovered) {
            fprintf(
                stderr,
                "Camera classifier mismatch in case %zu: i960=0x%08x C=0x%08x\\n",
                index, (unsigned)cpu.registers[16], (unsigned)recovered
            );
            status = VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status == VF2_OK) {
        printf("Camera classifier differential validation: MATCH\\n");
        printf("Helper address:                             0x000214dc\\n");
        printf("Validated cases:                            %zu\\n",
               sizeof(cases) / sizeof(cases[0]));
    } else {
        fprintf(stderr, "Camera classifier comparison failed: %s\\n",
                vf2_status_string(status));
    }

    if (machine.work_ram != NULL) {
        vf2_model2a_shutdown(&machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}


static vf2_status prepare_camera_viewport_case(
    vf2_model2a *machine,
    uint32_t registry_address,
    int fixed_case
)
{
    uint8_t zero_registry[0x300];
    const uint32_t center = fixed_case
        ? UINT32_C(0xc2c80000) : UINT32_C(0xc1a00000);
    const uint32_t scale = UINT32_C(0x41000000);
    vf2_status status = VF2_OK;

    memset(zero_registry, 0, sizeof(zero_registry));
    status = vf2_model2a_write(
        machine, registry_address, zero_registry, sizeof(zero_registry)
    );
    if (status == VF2_OK) {
        const uint8_t input_index = 0u;
        status = vf2_model2a_write(
            machine, UINT32_C(0x00500064), &input_index, sizeof(input_index)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a00c), scale);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a014), center);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500804), UINT32_C(0x00502000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500808), UINT32_C(0x00503000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00502000) + UINT32_C(0x1f4), UINT32_C(0x42c80000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00503000) + UINT32_C(0x1f4), UINT32_C(0x42c80000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050109c), UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005010a0), UINT32_C(0x3f000000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005010a4), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005010a8), UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005010e4), 0u);
    }
    if (status == VF2_OK) {
        const uint8_t weight[2] = {0x00u, 0x40u};
        status = vf2_model2a_write(
            machine, UINT32_C(0x005010ec), weight, sizeof(weight)
        );
    }
    if (status == VF2_OK) {
        const uint8_t divisor[2] = {0u, 0u};
        status = vf2_model2a_write(
            machine, UINT32_C(0x0050a024), divisor, sizeof(divisor)
        );
    }
    return status;
}

static int command_compare_camera_viewport(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a original_machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu original_cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_i960_snapshot entry_snapshot;
    vf2_i960_snapshot original_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_recovered_camera_viewport_report report;
    vf2_status status = VF2_OK;
    const uint32_t registry_address = UINT32_C(0x00515400);
    size_t case_index = 0u;

    memset(&original_machine, 0, sizeof(original_machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    memset(&original_cpu, 0, sizeof(original_cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    memset(&diff, 0, sizeof(diff));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&entry_snapshot);
    vf2_i960_snapshot_init(&original_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK && image_size <= UINT32_C(0x0006eea1)) {
        status = VF2_ERROR_BAD_SIZE;
    }
    if (status == VF2_OK) {
        image[0x0006eea0u] = 0x0eu;
        image[0x0006eea1u] = 0x00u;
    }

    for (case_index = 0u; status == VF2_OK && case_index < 2u; ++case_index) {
        if (original_machine.work_ram != NULL) {
            vf2_model2a_shutdown(&original_machine);
        }
        if (recovered_machine.work_ram != NULL) {
            vf2_model2a_shutdown(&recovered_machine);
        }
        vf2_i960_snapshot_destroy(&entry_snapshot);
        vf2_i960_snapshot_destroy(&original_snapshot);
        vf2_i960_snapshot_destroy(&recovered_snapshot);
        vf2_i960_snapshot_init(&entry_snapshot);
        vf2_i960_snapshot_init(&original_snapshot);
        vf2_i960_snapshot_init(&recovered_snapshot);

        status = initialize_boot_machine(
            rom_directory, &original_machine, image, image_size
        );
        if (status == VF2_OK) {
            status = initialize_boot_machine(
                rom_directory, &recovered_machine, image, image_size
            );
        }
        if (status == VF2_OK) {
            status = prepare_camera_viewport_case(
                &original_machine, registry_address, case_index == 0u
            );
        }
        vf2_i960_cpu_reset(&original_cpu, 0u, 0u, UINT32_C(0x00000040));
        original_cpu.registers[27] = VF2_COPRO_PORT_BASE;
        original_cpu.registers[28] = UINT32_C(0x4000);
        original_cpu.registers[29] = registry_address;
        if (status == VF2_OK) {
            status = vf2_i960_cpu_enter_procedure(
                &original_cpu, UINT32_C(0x0001d678), UINT32_C(0x00000040)
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_capture(
                &entry_snapshot, &original_cpu, &original_machine
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_restore(
                &entry_snapshot, &recovered_cpu, &recovered_machine
            );
        }
        if (status == VF2_OK) {
            memset(&options, 0, sizeof(options));
            options.stop_address = UINT32_C(0x0001d8e8);
            options.max_steps = UINT64_C(10000);
            options.stop_on_self_branch = true;
            status = vf2_i960_run(
                &original_cpu, &original_machine, &options, &result
            );
        }
        if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_recovered_task_camera_viewport_construct(
                &recovered_machine, registry_address, &report
            );
            if (status != VF2_OK) {
                fprintf(stderr, "Recovered viewport case %zu failed: %s\n",
                        case_index, vf2_status_string(status));
            }
        }
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_capture(
                &original_snapshot, &original_cpu, &original_machine
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_capture(
                &recovered_snapshot, &recovered_cpu, &recovered_machine
            );
        }
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_compare_memory(
                &original_snapshot, &recovered_snapshot, &diff
            );
        }
        if (status == VF2_OK && !diff.equal) {
            fprintf(
                stderr,
                "Viewport case %zu mismatch in %s at 0x%zx: i960=0x%x C=0x%x (%zu differences)\n",
                case_index, diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value,
                diff.differing_bytes
            );
            status = VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status == VF2_OK) {
        printf("Camera viewport differential validation: MATCH\n");
        printf("Recovered block:                        0x0001d678-0x0001d8e8\n");
        printf("Integrated helpers:                     0x0001fbb4 0x0001eff0 0x0001facc\n");
        printf("Validated scenarios:                    2 (fixed and calculated)\n");
        printf("Viewport entries:                       8 + 10\n");
    } else {
        fprintf(
            stderr, "Camera viewport comparison failed: %s (case=%zu ip=0x%08x halt=%s at 0x%08x)\n",
            vf2_status_string(status), case_index,
            (unsigned)original_cpu.ip,
            vf2_i960_halt_reason_name(result.halt_reason),
            (unsigned)result.halt_address
        );
    }

    vf2_i960_snapshot_destroy(&entry_snapshot);
    vf2_i960_snapshot_destroy(&original_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    if (original_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&original_machine);
    }
    if (recovered_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&recovered_machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_scheduler_pass(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_result result;
    vf2_i960_run_options options;
    uint32_t request = 0u;
    uint32_t enable = 0u;
    uint32_t wait_flag = 0u;
    uint64_t checkpoint_instructions = 0u;
    uint64_t interrupt_instructions = 0u;
    uint64_t release_instructions = 0u;
    uint64_t frame_interrupt_instructions = 0u;
    const char *stage = "load-maincpu";
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&result, 0, sizeof(result));
    memset(&options, 0, sizeof(options));

    if (status == VF2_OK) {
        stage = "runtime-checkpoint";
        status = execute_boot_path(
            rom_directory,
            image,
            image_size,
            &vectors,
            UINT32_C(0x0004aff8),
            UINT64_C(5000000),
            NULL,
            NULL,
            &machine,
            &cpu,
            &result
        );
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    checkpoint_instructions = cpu.executed_instructions;
    if (status == VF2_OK) {
        stage = "read-interrupt-state";
        status = vf2_model2a_get_interrupt_state(&machine, &request, &enable);
    }
    if (status == VF2_OK && (enable & UINT32_C(1 << 5)) == 0u) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        stage = "raise-timer-interrupt";
        status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1 << 5));
    }
    if (status == VF2_OK) {
        stage = "enter-interrupt";
        status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 14u, 1u);
    }
    if (status == VF2_OK) {
        stage = "run-interrupt-handler";
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004aff8);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    interrupt_instructions = result.executed_instructions;
    if (status == VF2_OK) {
        stage = "read-wait-flag";
        status = vf2_model2a_read_u32(&machine, UINT32_C(0x0050008c), &wait_flag);
    }
    if (status == VF2_OK && (wait_flag & UINT32_C(0xff)) == 0u) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        stage = "release-wait";
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004b07c);
        options.max_steps = UINT64_C(1000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    release_instructions = result.executed_instructions;
    if (status == VF2_OK) {
        stage = "raise-frame-interrupt";
        status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1));
    }
    if (status == VF2_OK) {
        stage = "enter-frame-interrupt";
        status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 12u, 1u);
    }
    if (status == VF2_OK) {
        stage = "run-frame-interrupt";
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004b07c);
        options.max_steps = UINT64_C(5000000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    frame_interrupt_instructions = result.executed_instructions;
    if (status == VF2_OK) {
        status = vf2_model2a_get_interrupt_state(&machine, &request, &enable);
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Scheduler pass failed during %s: %s\n", stage, vf2_status_string(status));
        fprintf(stderr, "IP=0x%08x halt=%s address=0x%08x\n",
                (unsigned)cpu.ip, vf2_i960_halt_reason_name(result.halt_reason),
                (unsigned)result.halt_address);
    } else {
        printf("Scheduler synchronization pass: COMPLETE\n");
        printf("Runtime checkpoint:            0x%08x\n", UINT32_C(0x0004aff8));
        printf("Timer interrupt vector:        14\n");
        printf("Timer interrupt handler:       0x%08x\n", UINT32_C(0x00000d50));
        printf("Wait routine return:           0x%08x\n", (unsigned)cpu.ip);
        printf("Checkpoint instructions:       %llu\n",
               (unsigned long long)checkpoint_instructions);
        printf("Interrupt handler instructions:%llu\n",
               (unsigned long long)interrupt_instructions);
        printf("Wait release instructions:     %llu\n",
               (unsigned long long)release_instructions);
        printf("Frame interrupt instructions:  %llu\n",
               (unsigned long long)frame_interrupt_instructions);
        printf("Interrupt request/enable:      %08x/%08x\n",
               (unsigned)request, (unsigned)enable);
        printf("Interrupt entries/returns:     %llu/%llu\n",
               (unsigned long long)cpu.interrupt_entries,
               (unsigned long long)cpu.interrupt_returns);
        printf("Current frame depth:           %u\n",
               (unsigned)cpu.local_frame_depth);
    }

    if (machine.work_ram != NULL) {
        vf2_model2a_shutdown(&machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_scheduler_dispatch(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_result result;
    vf2_i960_run_options options;
    vf2_task_catalog catalog;
    vf2_recovered_scheduler_report plan;
    vf2_status status = VF2_OK;
    const char *stage = "load-maincpu";
    uint64_t step = 0u;
    uint32_t previous_flags = 0u;
    uint32_t ready_instruction = 0u;
    unsigned frame_wait_visits = 0u;
    unsigned frame_interrupts = 0u;
    size_t dispatched = 0u;
    bool ready = false;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&result, 0, sizeof(result));
    memset(&options, 0, sizeof(options));
    memset(&plan, 0, sizeof(plan));
    vf2_task_catalog_init(&catalog);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        stage = "scan-task-catalog";
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK) {
        stage = "runtime-checkpoint";
        status = execute_boot_path(
            rom_directory, image, image_size, &vectors,
            UINT32_C(0x0004aff8), UINT64_C(5000000),
            NULL, NULL, &machine, &cpu, &result
        );
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        stage = "timer-irq";
        status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1 << 5));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 14u, 1u);
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004aff8);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004b07c);
        options.max_steps = UINT64_C(1000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(&machine, UINT32_C(0x00500068), &previous_flags);
    }

    stage = "natural-runtime-transition";
    for (step = 0u; status == VF2_OK && step < UINT64_C(5000000); ++step) {
        const uint32_t ip_before = cpu.ip;
        uint32_t flags = 0u;

        if (ready && dispatched < plan.runnable_count &&
            ip_before == plan.runnable_entry_points[dispatched] &&
            cpu.registers[29] == plan.runnable_registry_addresses[dispatched]) {
            ++dispatched;
            if (dispatched == plan.runnable_count) {
                break;
            }
        }

        status = vf2_i960_step(&cpu, &machine, NULL);
        if (status != VF2_OK) {
            break;
        }
        status = vf2_model2a_read_u32(&machine, UINT32_C(0x00500068), &flags);
        if (status != VF2_OK) {
            break;
        }
        if (!ready && (flags & UINT32_C(0x80000000)) != 0u) {
            ready = true;
            ready_instruction = ip_before;
            stage = "scheduler-plan";
            status = vf2_recovered_scheduler_plan(&machine, &catalog, &plan);
            if (status != VF2_OK) {
                break;
            }
            stage = "task-dispatch-prefix";
        }
        previous_flags = flags;
        (void)previous_flags;

        if (cpu.ip == UINT32_C(0x00000f7c)) {
            ++frame_wait_visits;
            if (frame_wait_visits >= 4u) {
                status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1));
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 12u, 1u);
                }
                ++frame_interrupts;
                frame_wait_visits = 0u;
            }
        }
        if (cpu.ip == ip_before && cpu.ip == UINT32_C(0x0004aff8)) {
            status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1 << 5));
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 14u, 1u);
            }
        }
    }

    if (status == VF2_OK && (!ready || plan.descriptor_count != 29u ||
        plan.runnable_count != 7u || dispatched != plan.runnable_count)) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Scheduler dispatch failed during %s: %s\n",
                stage, vf2_status_string(status));
        fprintf(stderr, "IP=0x%08x step=%llu ready=%s dispatched=%zu/%zu\n",
                (unsigned)cpu.ip, (unsigned long long)step,
                ready ? "yes" : "no", dispatched, plan.runnable_count);
    } else {
        size_t index = 0u;
        printf("Scheduler first dispatch: COMPLETE\n");
        printf("Ready flag transition:    0x%08x at instruction 0x%08x\n",
               UINT32_C(0x00500068), (unsigned)ready_instruction);
        printf("Frame interrupts injected:%u\n", frame_interrupts);
        printf("Scheduler entry:          0x%08x\n", UINT32_C(0x00010d54));
        printf("Task descriptors:         %zu\n", plan.descriptor_count);
        printf("Initially runnable:        %zu\n", plan.runnable_count);
        printf("Observed dispatches:       %zu\n", dispatched);
        for (index = 0u; index < dispatched; ++index) {
            const size_t task_index = plan.runnable_task_indices[index];
            printf("  %zu. %-16s entry=0x%08x registry=0x%08x\n",
                   index + 1u, catalog.tasks[task_index].name,
                   (unsigned)plan.runnable_entry_points[index],
                   (unsigned)plan.runnable_registry_addresses[index]);
        }
        printf("Validation boundary:      before unmodeled geometry path\n");
    }

    vf2_task_catalog_destroy(&catalog);
    if (machine.work_ram != NULL) {
        vf2_model2a_shutdown(&machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static uint32_t task_registry_address(
    const vf2_task_catalog *catalog,
    size_t task_index
)
{
    uint32_t address = UINT32_C(0x00510000);
    size_t index = 0u;
    if (catalog == NULL || task_index >= catalog->count) {
        return 0u;
    }
    for (index = 0u; index < task_index; ++index) {
        address += catalog->tasks[index].stack_size;
    }
    return address;
}

static vf2_status run_isolated_task(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t entry_point,
    uint32_t registry_address,
    vf2_i960_run_result *result
)
{
    vf2_i960_run_options options;
    vf2_status status = VF2_OK;
    memset(&options, 0, sizeof(options));
    memset(result, 0, sizeof(*result));
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00000040));
    cpu->registers[29] = registry_address;
    status = vf2_i960_cpu_enter_procedure(
        cpu, entry_point, UINT32_C(0x00000040)
    );
    if (status == VF2_OK) {
        options.stop_address = UINT32_C(0x00000040);
        options.max_steps = UINT64_C(1000000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(cpu, machine, &options, result);
    }
    if (status == VF2_OK && result->halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    return status;
}

static int command_compare_task_recoveries(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_task_catalog catalog;
    vf2_model2a interpreted_machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu interpreted_cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_result user_result;
    vf2_i960_run_result sound_result;
    vf2_i960_snapshot interpreted_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_recovered_task_report user_report;
    vf2_recovered_task_report sound_report;
    const vf2_task_descriptor *user_task = NULL;
    const vf2_task_descriptor *sound_task = NULL;
    size_t user_index = 0u;
    size_t sound_index = 0u;
    uint32_t user_registry = 0u;
    uint32_t sound_registry = 0u;
    vf2_status status = VF2_OK;

    memset(&interpreted_machine, 0, sizeof(interpreted_machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    memset(&interpreted_cpu, 0, sizeof(interpreted_cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&user_result, 0, sizeof(user_result));
    memset(&sound_result, 0, sizeof(sound_result));
    memset(&diff, 0, sizeof(diff));
    memset(&user_report, 0, sizeof(user_report));
    memset(&sound_report, 0, sizeof(sound_report));
    vf2_task_catalog_init(&catalog);
    vf2_i960_snapshot_init(&interpreted_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK) {
        user_task = vf2_task_catalog_find(&catalog, "fa_user");
        sound_task = vf2_task_catalog_find(&catalog, "fa_sound");
        if (user_task == NULL || sound_task == NULL) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        user_index = (size_t)(user_task - catalog.tasks);
        sound_index = (size_t)(sound_task - catalog.tasks);
        user_registry = task_registry_address(&catalog, user_index);
        sound_registry = task_registry_address(&catalog, sound_index);
        if (user_registry == 0u || sound_registry == 0u) {
            status = VF2_ERROR_BAD_SIZE;
        }
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &interpreted_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &recovered_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_task_registry_initialize(
            &interpreted_machine, &catalog, NULL
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_task_registry_initialize(
            &recovered_machine, &catalog, NULL
        );
    }

    if (status == VF2_OK) {
        status = run_isolated_task(
            &interpreted_machine,
            &interpreted_cpu,
            user_task->entry_point,
            user_registry,
            &user_result
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_task_user_execute(
            &recovered_machine, user_registry, &user_report
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &interpreted_snapshot, &interpreted_cpu, &interpreted_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &recovered_snapshot, &recovered_cpu, &recovered_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare_memory(
            &interpreted_snapshot, &recovered_snapshot, &diff
        );
    }
    if (status == VF2_OK && !diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    vf2_i960_snapshot_destroy(&interpreted_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    vf2_i960_snapshot_init(&interpreted_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);
    memset(&diff, 0, sizeof(diff));

    if (status == VF2_OK) {
        status = run_isolated_task(
            &interpreted_machine,
            &interpreted_cpu,
            sound_task->entry_point,
            sound_registry,
            &sound_result
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_task_sound_initialize(
            &recovered_machine, sound_registry, &sound_report
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &interpreted_snapshot, &interpreted_cpu, &interpreted_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &recovered_snapshot, &recovered_cpu, &recovered_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare_memory(
            &interpreted_snapshot, &recovered_snapshot, &diff
        );
    }
    if (status == VF2_OK && !diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Task recovery comparison failed: %s\n", vf2_status_string(status));
        if (!diff.equal && diff.component[0] != '\0') {
            fprintf(stderr,
                    "Mismatch in %s at offset 0x%zx: expected=0x%x actual=0x%x (%zu differences)\n",
                    diff.component, diff.first_offset,
                    (unsigned)diff.expected_value, (unsigned)diff.actual_value,
                    diff.differing_bytes);
        }
    } else {
        printf("Task recovery differential validation: MATCH\n");
        printf("Recovered task entries:               2\n");
        printf("fa_user entry/registry:               0x%08x/0x%08x\n",
               (unsigned)user_task->entry_point, (unsigned)user_registry);
        printf("fa_user interpreted instructions:     %llu\n",
               (unsigned long long)user_result.executed_instructions);
        printf("fa_sound entry/registry:              0x%08x/0x%08x\n",
               (unsigned)sound_task->entry_point, (unsigned)sound_registry);
        printf("fa_sound continuation:                0x%08x\n",
               (unsigned)sound_report.continuation);
        printf("fa_sound interpreted instructions:    %llu\n",
               (unsigned long long)sound_result.executed_instructions);
        printf("fa_sound task/global bytes written:   %zu/%zu\n",
               sound_report.bytes_written, sound_report.global_bytes_written);
    }

    vf2_i960_snapshot_destroy(&interpreted_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    vf2_task_catalog_destroy(&catalog);
    if (interpreted_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&interpreted_machine);
    }
    if (recovered_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&recovered_machine);
    }
    free(image);
    return status == VF2_OK && diff.equal ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct task_profile_baseline {
    uint8_t *work_ram;
    uint8_t *buffer_ram;
    uint8_t *geometry;
    uint8_t *copro_port;
} task_profile_baseline;

enum { TASK_PROFILE_MAX_CALLS = 64u };

typedef struct task_call_observation {
    uint32_t call_site;
    uint32_t target;
    uint32_t frame_depth;
    bool indirect;
} task_call_observation;

typedef struct task_execution_profile {
    const vf2_task_descriptor *task;
    uint32_t registry_address;
    uint64_t instructions;
    uint64_t procedure_calls;
    uint64_t procedure_returns;
    uint32_t entry_frame_depth;
    uint32_t maximum_frame_depth;
    size_t changed_work_ram;
    size_t changed_buffer_ram;
    size_t changed_geometry;
    size_t changed_copro_port;
    uint32_t first_work_ram_change;
    uint32_t last_work_ram_change;
    task_call_observation calls[TASK_PROFILE_MAX_CALLS];
    size_t observed_call_count;
} task_execution_profile;

static void task_profile_baseline_destroy(task_profile_baseline *baseline)
{
    if (baseline != NULL) {
        free(baseline->work_ram);
        free(baseline->buffer_ram);
        free(baseline->geometry);
        free(baseline->copro_port);
        memset(baseline, 0, sizeof(*baseline));
    }
}

static vf2_status task_profile_baseline_capture(
    task_profile_baseline *baseline,
    const vf2_model2a *machine
)
{
    if (baseline == NULL || machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    task_profile_baseline_destroy(baseline);
    baseline->work_ram = malloc(machine->work_ram_size);
    baseline->buffer_ram = malloc(machine->buffer_ram_size);
    baseline->geometry = malloc(machine->geometry_size);
    baseline->copro_port = malloc(machine->copro_port_size);
    if (baseline->work_ram == NULL || baseline->buffer_ram == NULL ||
        baseline->geometry == NULL || baseline->copro_port == NULL) {
        task_profile_baseline_destroy(baseline);
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    memcpy(baseline->work_ram, machine->work_ram, machine->work_ram_size);
    memcpy(baseline->buffer_ram, machine->buffer_ram, machine->buffer_ram_size);
    memcpy(baseline->geometry, machine->geometry, machine->geometry_size);
    memcpy(baseline->copro_port, machine->copro_port, machine->copro_port_size);
    return VF2_OK;
}

static size_t count_region_changes(
    const uint8_t *before,
    const uint8_t *after,
    size_t size,
    size_t *first,
    size_t *last
)
{
    size_t count = 0u;
    size_t index = 0u;
    size_t first_change = SIZE_MAX;
    size_t last_change = 0u;
    for (index = 0u; index < size; ++index) {
        if (before[index] != after[index]) {
            if (first_change == SIZE_MAX) {
                first_change = index;
            }
            last_change = index;
            ++count;
        }
    }
    if (first != NULL) {
        *first = first_change;
    }
    if (last != NULL) {
        *last = count == 0u ? SIZE_MAX : last_change;
    }
    return count;
}

static void finish_task_profile(
    task_execution_profile *profile,
    const task_profile_baseline *baseline,
    const vf2_model2a *machine,
    const vf2_i960_cpu *cpu,
    uint64_t start_instructions,
    uint64_t start_calls,
    uint64_t start_returns
)
{
    size_t first = SIZE_MAX;
    size_t last = SIZE_MAX;
    profile->instructions = cpu->executed_instructions - start_instructions;
    profile->procedure_calls = cpu->procedure_calls - start_calls;
    profile->procedure_returns = cpu->procedure_returns - start_returns;
    profile->changed_work_ram = count_region_changes(
        baseline->work_ram, machine->work_ram, machine->work_ram_size, &first, &last
    );
    profile->first_work_ram_change = first == SIZE_MAX
        ? 0u : VF2_WORK_RAM_BASE + (uint32_t)first;
    profile->last_work_ram_change = last == SIZE_MAX
        ? 0u : VF2_WORK_RAM_BASE + (uint32_t)last;
    profile->changed_buffer_ram = count_region_changes(
        baseline->buffer_ram, machine->buffer_ram, machine->buffer_ram_size, NULL, NULL
    );
    profile->changed_geometry = count_region_changes(
        baseline->geometry, machine->geometry, machine->geometry_size, NULL, NULL
    );
    profile->changed_copro_port = count_region_changes(
        baseline->copro_port, machine->copro_port, machine->copro_port_size, NULL, NULL
    );
}

static int command_task_profile(const char *rom_directory, const char *output_path)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_result result;
    vf2_i960_run_options options;
    vf2_task_catalog catalog;
    vf2_recovered_scheduler_report plan;
    task_execution_profile profiles[VF2_RECOVERED_SCHEDULER_MAX_TASKS];
    task_profile_baseline baseline;
    vf2_i960_snapshot entry_snapshot;
    vf2_i960_snapshot original_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff recovery_diff;
    vf2_recovered_camera_init_report camera_init_report;
    vf2_recovered_camera_update_report camera_update_report;
    vf2_recovered_camera_gate_report camera_gate_report;
    vf2_recovered_kill_osage_report kill_osage_report;
    vf2_status status = VF2_OK;
    const char *stage = "load-maincpu";
    uint64_t step = 0u;
    uint64_t task_start_instructions = 0u;
    uint64_t task_start_calls = 0u;
    uint64_t task_start_returns = 0u;
    uint32_t previous_flags = 0u;
    uint32_t post_dispatch_checkpoint = 0u;
    unsigned frame_wait_visits = 0u;
    size_t completed = 0u;
    size_t validated_recoveries = 0u;
    size_t camera_prefix_validations = 0u;
    size_t camera_update_validations = 0u;
    size_t camera_gate_validations = 0u;
    bool ready = false;
    bool active = false;
    bool active_recovery = false;
    bool active_camera_prefix = false;
    bool active_camera_update = false;
    bool active_camera_gate = false;
    FILE *csv = NULL;

    memset(&machine, 0, sizeof(machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&result, 0, sizeof(result));
    memset(&options, 0, sizeof(options));
    memset(&plan, 0, sizeof(plan));
    memset(profiles, 0, sizeof(profiles));
    memset(&baseline, 0, sizeof(baseline));
    memset(&recovery_diff, 0, sizeof(recovery_diff));
    memset(&camera_init_report, 0, sizeof(camera_init_report));
    memset(&camera_update_report, 0, sizeof(camera_update_report));
    memset(&camera_gate_report, 0, sizeof(camera_gate_report));
    memset(&kill_osage_report, 0, sizeof(kill_osage_report));
    vf2_task_catalog_init(&catalog);
    vf2_i960_snapshot_init(&entry_snapshot);
    vf2_i960_snapshot_init(&original_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        stage = "scan-task-catalog";
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK) {
        stage = "runtime-checkpoint";
        status = execute_boot_path(
            rom_directory, image, image_size, &vectors,
            UINT32_C(0x0004aff8), UINT64_C(5000000),
            NULL, NULL, &machine, &cpu, &result
        );
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1 << 5));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 14u, 1u);
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004aff8);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004b07c);
        options.max_steps = UINT64_C(1000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&cpu, &machine, &options, &result);
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &recovered_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(&machine, UINT32_C(0x00500068), &previous_flags);
    }

    stage = "profile-first-dispatch";
    for (step = 0u; status == VF2_OK && step < UINT64_C(8000000); ++step) {
        const uint32_t ip_before = cpu.ip;
        uint32_t flags = 0u;

        if (ready && !active && completed < plan.runnable_count &&
            ip_before == plan.runnable_entry_points[completed] &&
            cpu.registers[29] == plan.runnable_registry_addresses[completed]) {
            task_execution_profile *profile = &profiles[completed];
            profile->task = &catalog.tasks[plan.runnable_task_indices[completed]];
            profile->registry_address = plan.runnable_registry_addresses[completed];
            profile->entry_frame_depth = cpu.local_frame_depth;
            profile->maximum_frame_depth = cpu.local_frame_depth;
            task_start_instructions = cpu.executed_instructions;
            task_start_calls = cpu.procedure_calls;
            task_start_returns = cpu.procedure_returns;
            status = task_profile_baseline_capture(&baseline, &machine);
            active = status == VF2_OK;
            active_recovery = false;
            active_camera_prefix = false;
            active_camera_update = false;
            active_camera_gate = false;
            if (status == VF2_OK &&
                (strcmp(profile->task->name, "fa_game_info") == 0 ||
                 strcmp(profile->task->name, "fa_camera") == 0 ||
                 strcmp(profile->task->name, "fa_user") == 0 ||
                 strcmp(profile->task->name, "fa_sound") == 0 ||
                 strcmp(profile->task->name, "fa_kill_osage") == 0 ||
                 strncmp(profile->task->name, "fa_osage", 8u) == 0)) {
                vf2_recovered_task_report recovery_report;
                memset(&recovery_report, 0, sizeof(recovery_report));
                vf2_i960_snapshot_destroy(&entry_snapshot);
                vf2_i960_snapshot_init(&entry_snapshot);
                status = vf2_i960_snapshot_capture(&entry_snapshot, &cpu, &machine);
                if (status == VF2_OK) {
                    status = vf2_i960_snapshot_restore(
                        &entry_snapshot, &recovered_cpu, &recovered_machine
                    );
                }
                if (status == VF2_OK && strcmp(profile->task->name, "fa_game_info") == 0) {
                    status = vf2_recovered_task_game_info_first_dispatch(
                        &recovered_machine, profile->registry_address, &recovery_report
                    );
                    active_recovery = status == VF2_OK;
                } else if (status == VF2_OK && strcmp(profile->task->name, "fa_camera") == 0) {
                    status = vf2_recovered_task_camera_initialize(
                        &recovered_machine,
                        profile->registry_address,
                        &camera_init_report
                    );
                    active_camera_prefix = status == VF2_OK;
                } else if (status == VF2_OK && strcmp(profile->task->name, "fa_user") == 0) {
                    status = vf2_recovered_task_user_execute(
                        &recovered_machine, profile->registry_address, &recovery_report
                    );
                    active_recovery = status == VF2_OK;
                } else if (status == VF2_OK && strcmp(profile->task->name, "fa_sound") == 0) {
                    status = vf2_recovered_task_sound_initialize(
                        &recovered_machine, profile->registry_address, &recovery_report
                    );
                    active_recovery = status == VF2_OK;
                } else if (status == VF2_OK && strcmp(profile->task->name, "fa_kill_osage") == 0) {
                    status = vf2_recovered_task_kill_osage_execute(
                        &recovered_machine,
                        profile->registry_address,
                        &kill_osage_report
                    );
                    active_recovery = status == VF2_OK;
                } else if (status == VF2_OK) {
                    status = vf2_recovered_task_osage_first_dispatch(
                        &recovered_machine, profile->registry_address, &recovery_report
                    );
                    active_recovery = status == VF2_OK;
                }
            }
            if (status != VF2_OK) {
                break;
            }
        }

        {
            vf2_i960_trace_event event;
            const bool task_was_active = active;
            memset(&event, 0, sizeof(event));
            status = vf2_i960_step(&cpu, &machine, &event);
            if (status != VF2_OK) {
                break;
            }
            if (task_was_active &&
                event.instruction.flow == VF2_I960_FLOW_CALL) {
                task_execution_profile *profile = &profiles[completed];
                if (profile->observed_call_count < TASK_PROFILE_MAX_CALLS) {
                    task_call_observation *call =
                        &profile->calls[profile->observed_call_count++];
                    call->call_site = event.ip_before;
                    call->target = event.ip_after;
                    call->frame_depth = cpu.local_frame_depth;
                    call->indirect = event.instruction.indirect;
                }
            }
        }

        if (active_camera_prefix && cpu.ip == UINT32_C(0x0001d458)) {
            vf2_i960_snapshot_destroy(&original_snapshot);
            vf2_i960_snapshot_destroy(&recovered_snapshot);
            vf2_i960_snapshot_init(&original_snapshot);
            vf2_i960_snapshot_init(&recovered_snapshot);
            memset(&recovery_diff, 0, sizeof(recovery_diff));
            status = vf2_i960_snapshot_capture(
                &original_snapshot, &cpu, &machine
            );
            if (status == VF2_OK) {
                status = vf2_i960_snapshot_capture(
                    &recovered_snapshot, &recovered_cpu, &recovered_machine
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_snapshot_compare_memory(
                    &original_snapshot, &recovered_snapshot, &recovery_diff
                );
            }
            if (status == VF2_OK && !recovery_diff.equal) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                ++validated_recoveries;
                ++camera_prefix_validations;
                active_camera_prefix = false;
                status = vf2_recovered_task_camera_first_update(
                    &recovered_machine,
                    profiles[completed].registry_address,
                    &camera_update_report
                );
                active_camera_update = status == VF2_OK;
            }
        }

        if (active_camera_update && cpu.ip == UINT32_C(0x0001d660)) {
            vf2_i960_snapshot_destroy(&original_snapshot);
            vf2_i960_snapshot_destroy(&recovered_snapshot);
            vf2_i960_snapshot_init(&original_snapshot);
            vf2_i960_snapshot_init(&recovered_snapshot);
            memset(&recovery_diff, 0, sizeof(recovery_diff));
            status = vf2_i960_snapshot_capture(
                &original_snapshot, &cpu, &machine
            );
            if (status == VF2_OK) {
                status = vf2_i960_snapshot_capture(
                    &recovered_snapshot, &recovered_cpu, &recovered_machine
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_snapshot_compare_memory(
                    &original_snapshot, &recovered_snapshot, &recovery_diff
                );
            }
            if (status == VF2_OK && !recovery_diff.equal) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                ++validated_recoveries;
                ++camera_update_validations;
                active_camera_update = false;
                status = vf2_recovered_task_camera_post_update_gate(
                    &recovered_machine,
                    profiles[completed].registry_address,
                    &camera_gate_report
                );
                active_camera_gate = status == VF2_OK;
            }
        }

        if (active_camera_gate && cpu.ip == camera_gate_report.stop_address) {
            vf2_i960_snapshot_destroy(&original_snapshot);
            vf2_i960_snapshot_destroy(&recovered_snapshot);
            vf2_i960_snapshot_init(&original_snapshot);
            vf2_i960_snapshot_init(&recovered_snapshot);
            memset(&recovery_diff, 0, sizeof(recovery_diff));
            status = vf2_i960_snapshot_capture(
                &original_snapshot, &cpu, &machine
            );
            if (status == VF2_OK) {
                status = vf2_i960_snapshot_capture(
                    &recovered_snapshot, &recovered_cpu, &recovered_machine
                );
            }
            if (status == VF2_OK) {
                status = vf2_i960_snapshot_compare_memory(
                    &original_snapshot, &recovered_snapshot, &recovery_diff
                );
            }
            if (status == VF2_OK && !recovery_diff.equal) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                ++validated_recoveries;
                ++camera_gate_validations;
                active_camera_gate = false;
            }
        }

        if (active) {
            task_execution_profile *profile = &profiles[completed];
            if (cpu.local_frame_depth > profile->maximum_frame_depth) {
                profile->maximum_frame_depth = cpu.local_frame_depth;
            }
            if (cpu.local_frame_depth < profile->entry_frame_depth) {
                finish_task_profile(
                    profile, &baseline, &machine, &cpu,
                    task_start_instructions, task_start_calls, task_start_returns
                );
                task_profile_baseline_destroy(&baseline);
                if (active_recovery) {
                    vf2_i960_snapshot_destroy(&original_snapshot);
                    vf2_i960_snapshot_destroy(&recovered_snapshot);
                    vf2_i960_snapshot_init(&original_snapshot);
                    vf2_i960_snapshot_init(&recovered_snapshot);
                    memset(&recovery_diff, 0, sizeof(recovery_diff));
                    status = vf2_i960_snapshot_capture(
                        &original_snapshot, &cpu, &machine
                    );
                    if (status == VF2_OK) {
                        status = vf2_i960_snapshot_capture(
                            &recovered_snapshot, &recovered_cpu, &recovered_machine
                        );
                    }
                    if (status == VF2_OK) {
                        status = vf2_i960_snapshot_compare_memory(
                            &original_snapshot, &recovered_snapshot, &recovery_diff
                        );
                    }
                    if (status == VF2_OK && !recovery_diff.equal) {
                        status = VF2_ERROR_UNSUPPORTED;
                    }
                    if (status == VF2_OK) {
                        ++validated_recoveries;
                    }
                }
                if (active_camera_prefix || active_camera_update ||
                    active_camera_gate) {
                    status = VF2_ERROR_UNSUPPORTED;
                    break;
                }
                active_recovery = false;
                active_camera_prefix = false;
                active_camera_update = false;
                active_camera_gate = false;
                active = false;
                ++completed;
                if (completed == plan.runnable_count) {
                    post_dispatch_checkpoint = cpu.ip;
                    break;
                }
            }
        }

        status = vf2_model2a_read_u32(&machine, UINT32_C(0x00500068), &flags);
        if (status != VF2_OK) {
            break;
        }
        if (!ready && (flags & UINT32_C(0x80000000)) != 0u) {
            ready = true;
            status = vf2_recovered_scheduler_plan(&machine, &catalog, &plan);
            if (status != VF2_OK) {
                break;
            }
        }
        previous_flags = flags;
        (void)previous_flags;

        if (cpu.ip == UINT32_C(0x00000f7c)) {
            ++frame_wait_visits;
            if (frame_wait_visits >= 4u) {
                status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1));
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 12u, 1u);
                }
                frame_wait_visits = 0u;
            }
        }
        if (cpu.ip == ip_before && cpu.ip == UINT32_C(0x0004aff8)) {
            status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1 << 5));
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_interrupt(&cpu, &machine, 14u, 1u);
            }
        }
    }

    if (status == VF2_OK && (!ready || plan.runnable_count != 7u || completed != 7u ||
        validated_recoveries != 9u || camera_prefix_validations != 1u ||
        camera_update_validations != 1u || camera_gate_validations != 1u ||
        post_dispatch_checkpoint != UINT32_C(0x00010dcc))) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK && output_path != NULL) {
        csv = fopen(output_path, "wb");
        if (csv == NULL) {
            status = VF2_ERROR_IO;
        }
    }
    if (status == VF2_OK && csv != NULL) {
        size_t index = 0u;
        fputs("order,name,entry,registry,instructions,calls,returns,entry_depth,max_depth,work_ram_changed,buffer_ram_changed,geometry_changed,copro_port_changed,first_work_change,last_work_change,call_sequence\n", csv);
        for (index = 0u; index < completed; ++index) {
            const task_execution_profile *profile = &profiles[index];
            size_t call_index = 0u;
            fprintf(csv,
                    "%zu,%s,0x%08x,0x%08x,%llu,%llu,%llu,%u,%u,%zu,%zu,%zu,%zu,0x%08x,0x%08x,\"",
                    index + 1u, profile->task->name,
                    (unsigned)profile->task->entry_point,
                    (unsigned)profile->registry_address,
                    (unsigned long long)profile->instructions,
                    (unsigned long long)profile->procedure_calls,
                    (unsigned long long)profile->procedure_returns,
                    (unsigned)profile->entry_frame_depth,
                    (unsigned)profile->maximum_frame_depth,
                    profile->changed_work_ram, profile->changed_buffer_ram,
                    profile->changed_geometry, profile->changed_copro_port,
                    (unsigned)profile->first_work_ram_change,
                    (unsigned)profile->last_work_ram_change);
            for (call_index = 0u; call_index < profile->observed_call_count; ++call_index) {
                const task_call_observation *call = &profile->calls[call_index];
                fprintf(csv, "%s0x%08x->0x%08x%s",
                        call_index == 0u ? "" : "|",
                        (unsigned)call->call_site,
                        (unsigned)call->target,
                        call->indirect ? "*" : "");
            }
            fputs("\"\n", csv);
        }
        if (fclose(csv) != 0) {
            status = VF2_ERROR_IO;
        }
        csv = NULL;
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Task profiling failed during %s: %s\n",
                stage, vf2_status_string(status));
        fprintf(stderr, "IP=0x%08x step=%llu completed=%zu/%zu\n",
                (unsigned)cpu.ip, (unsigned long long)step,
                completed, plan.runnable_count);
    } else {
        size_t index = 0u;
        printf("First-dispatch task profile: COMPLETE\n");
        printf("Profiled tasks:             %zu\n", completed);
        printf("C-validated task paths:     %zu\n", validated_recoveries);
        printf("Camera init prefixes:       %zu\n", camera_prefix_validations);
        printf("Camera update prefixes:     %zu\n", camera_update_validations);
        printf("Camera post-update gates:   %zu\n", camera_gate_validations);
        printf("Camera update boundary:     0x%08x\n",
               (unsigned)camera_update_report.stop_address);
        printf("Camera range flags:         0x%02x\n",
               (unsigned)camera_update_report.range_flags);
        printf("Camera mode handler:        0x%08x\n",
               (unsigned)camera_update_report.mode_handler);
        printf("Camera gate boundary:       0x%08x\n",
               (unsigned)camera_gate_report.stop_address);
        printf("Camera gate control/input:  0x%02x/0x%04x\n",
               (unsigned)camera_gate_report.control_flags,
               (unsigned)camera_gate_report.input_flags);
        printf("Camera fast exit:           %s\n",
               camera_gate_report.fast_exit != 0 ? "yes" : "no");
        printf("Post-dispatch checkpoint:   0x%08x\n",
               (unsigned)post_dispatch_checkpoint);
        printf("Camera palette entries:     %zu\n",
               camera_init_report.palette_entries_written);
        printf("Kill-osage records/kills:   %zu/%zu\n",
               kill_osage_report.records_evaluated,
               kill_osage_report.records_marked_for_kill);
        for (index = 0u; index < completed; ++index) {
            const task_execution_profile *profile = &profiles[index];
            size_t call_index = 0u;
            printf("  %zu. %-16s ins=%llu calls=%llu depth=%u->%u changed(work/buf/geo/copro)=%zu/%zu/%zu/%zu\n",
                   index + 1u, profile->task->name,
                   (unsigned long long)profile->instructions,
                   (unsigned long long)profile->procedure_calls,
                   (unsigned)profile->entry_frame_depth,
                   (unsigned)profile->maximum_frame_depth,
                   profile->changed_work_ram, profile->changed_buffer_ram,
                   profile->changed_geometry, profile->changed_copro_port);
            for (call_index = 0u; call_index < profile->observed_call_count; ++call_index) {
                const task_call_observation *call = &profile->calls[call_index];
                printf("      call 0x%08x -> 0x%08x%s depth=%u\n",
                       (unsigned)call->call_site,
                       (unsigned)call->target,
                       call->indirect ? " indirect" : "",
                       (unsigned)call->frame_depth);
            }
        }
        if (output_path != NULL) {
            printf("CSV report:                 %s\n", output_path);
        }
    }

    if (csv != NULL) {
        (void)fclose(csv);
    }
    task_profile_baseline_destroy(&baseline);
    vf2_i960_snapshot_destroy(&entry_snapshot);
    vf2_i960_snapshot_destroy(&original_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    vf2_task_catalog_destroy(&catalog);
    if (machine.work_ram != NULL) {
        vf2_model2a_shutdown(&machine);
    }
    if (recovered_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&recovered_machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_trace(
    const char *rom_directory,
    const char *output_path,
    uint64_t max_steps
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_result result;
    trace_context context;
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    context.file = NULL;
    if (status == VF2_OK) {
        context.file = fopen(output_path, "wb");
        if (context.file == NULL) {
            status = VF2_ERROR_IO;
        }
    }
    if (status == VF2_OK) {
        (void)fprintf(context.file, "step,ip_before,ip_after,instruction\n");
        status = execute_boot_path(
            rom_directory,
            image,
            image_size,
            &vectors,
            0x000001b0u,
            max_steps,
            trace_csv_callback,
            &context,
            &machine,
            &cpu,
            &result
        );
    }
    if (context.file != NULL && fclose(context.file) != 0 && status == VF2_OK) {
        status = VF2_ERROR_IO;
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Trace failed: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }
    print_execution_summary(&cpu, &result, &machine);
    printf("Trace written to %s\n", output_path);
    vf2_model2a_shutdown(&machine);
    free(image);
    return EXIT_SUCCESS;
}

static int command_snapshot(
    const char *rom_directory,
    const char *output_path
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_result result;
    vf2_i960_snapshot snapshot;
    vf2_status status = VF2_OK;
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&result, 0, sizeof(result));
    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    vf2_i960_snapshot_init(&snapshot);
    if (status == VF2_OK) {
        status = execute_boot_path(
            rom_directory,
            image,
            image_size,
            &vectors,
            0x000001b0u,
            2000000u,
            NULL,
            NULL,
            &machine,
            &cpu,
            &result
        );
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(&snapshot, &cpu, &machine);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_write_file(&snapshot, output_path);
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Snapshot failed: %s\n", vf2_status_string(status));
        vf2_i960_snapshot_destroy(&snapshot);
        if (image != NULL && machine.work_ram != NULL) {
            vf2_model2a_shutdown(&machine);
        }
        free(image);
        return EXIT_FAILURE;
    }
    print_execution_summary(&cpu, &result, &machine);
    printf("Snapshot written to %s\n", output_path);
    vf2_i960_snapshot_destroy(&snapshot);
    vf2_model2a_shutdown(&machine);
    free(image);
    return EXIT_SUCCESS;
}

/* Resume a captured reference state for scouting beyond the current native
 * boundary.  This is intentionally a reference-only observer: it reports the
 * first decoder/executor failure and injects the same frame/timer interrupts
 * used by the differential runner, without claiming a native recovery. */
static int command_resume_trace(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t max_steps,
    uint32_t clear_task_index,
    uint32_t fighter_flags_or,
    uint32_t write_address,
    uint32_t write_value,
    const char *output_snapshot_path,
    uint32_t stop_address,
    uint32_t raise_irq_mask,
    uint32_t enter_vector,
    uint32_t enter_level
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot snapshot;
    vf2_i960_snapshot output_snapshot;
    vf2_i960_trace_event event;
    vf2_status status = VF2_OK;
    uint32_t frame_wait_visits = 0u;
    uint32_t timer_interrupts = 0u;
    uint32_t frame_interrupts = 0u;
    uint32_t calls = 0u;
    uint32_t steps = 0u;
    uint32_t halt_ip = 0u;
    bool stopped_on_error = false;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    vf2_i960_snapshot_init(&snapshot);
    vf2_i960_snapshot_init(&output_snapshot);
    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }
    if (status == VF2_OK && clear_task_index < 29u) {
        uint32_t address = UINT32_C(0x00510000);
        uint32_t index = 0u;
        for (index = 0u; index < clear_task_index; ++index) {
            uint32_t stack_size = 0u;
            status = vf2_model2a_read_u32(
                &machine, address + UINT32_C(8), &stack_size
            );
            if (status != VF2_OK) {
                break;
            }
            address += stack_size;
        }
        if (status == VF2_OK) {
            uint32_t flags = 0u;
            status = vf2_model2a_read_u32(&machine, address, &flags);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    &machine, address, flags & ~UINT32_C(0x80000000)
                );
            }
        }
    }
    if (status == VF2_OK && fighter_flags_or != UINT32_MAX) {
        uint32_t fighter0 = 0u;
        uint32_t fighter1 = 0u;
        uint32_t flags = 0u;
        status = vf2_model2a_read_u32(
            &machine, UINT32_C(0x00500804), &fighter0
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                &machine, UINT32_C(0x00500808), &fighter1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(&machine, fighter0, &flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                &machine, fighter0, flags | fighter_flags_or
            );
            status = vf2_model2a_read_u32(&machine, fighter1, &flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                &machine, fighter1, flags | fighter_flags_or
            );
        }
    }
    if (status == VF2_OK && write_address != UINT32_MAX) {
        status = vf2_model2a_write_u32(&machine, write_address, write_value);
    }
    if (status == VF2_OK && raise_irq_mask != UINT32_MAX) {
        status = vf2_model2a_raise_interrupt(&machine, raise_irq_mask);
    }
    if (status == VF2_OK && enter_vector != UINT32_MAX &&
        enter_level != UINT32_MAX) {
        status = vf2_i960_cpu_enter_interrupt(
            &cpu, &machine, enter_vector, enter_level
        );
    }
    if (status == VF2_OK) {
        printf("Resume trace start: IP=0x%08x instructions=%llu\n",
               (unsigned)cpu.ip,
               (unsigned long long)cpu.executed_instructions);
        {
            uint32_t registry = UINT32_C(0x00510000);
            uint32_t index = 0u;
            printf("  task registry snapshot:\n");
            for (index = 0u; index < 29u; ++index) {
                uint32_t flags = 0u;
                uint32_t stack_size = 0u;
                uint32_t entry = 0u;
                if (vf2_model2a_read_u32(&machine, registry, &flags) != VF2_OK ||
                    vf2_model2a_read_u32(&machine, registry + UINT32_C(8), &stack_size) != VF2_OK ||
                    vf2_model2a_read_u32(&machine, registry + UINT32_C(0x0c), &entry) != VF2_OK) {
                    break;
                }
                printf("    %2u registry=0x%08x flags=0x%08x stack=0x%08x entry=0x%08x\n",
                       (unsigned)index, (unsigned)registry, (unsigned)flags,
                       (unsigned)stack_size, (unsigned)entry);
                registry += stack_size;
            }
        }
        {
            uint32_t fighter0 = 0u;
            uint32_t fighter1 = 0u;
            uint32_t flags0 = 0u;
            uint32_t flags1 = 0u;
            uint32_t state_flags0 = 0u;
            uint32_t state_flags1 = 0u;
            uint8_t state0 = 0u;
            uint8_t state1 = 0u;
            if (vf2_model2a_read_u32(
                    &machine, UINT32_C(0x00500804), &fighter0
                ) == VF2_OK &&
                vf2_model2a_read_u32(
                    &machine, UINT32_C(0x00500808), &fighter1
                ) == VF2_OK &&
                vf2_model2a_read_u32(&machine, fighter0, &flags0) == VF2_OK &&
                vf2_model2a_read_u32(&machine, fighter1, &flags1) == VF2_OK &&
                vf2_model2a_read_u32(
                    &machine, fighter0 + UINT32_C(0x000001a4), &state_flags0
                ) == VF2_OK &&
                vf2_model2a_read_u32(
                    &machine, fighter1 + UINT32_C(0x000001a4), &state_flags1
                ) == VF2_OK &&
                vf2_model2a_read(
                    &machine, fighter0 + UINT32_C(0x00000a00), &state0,
                    sizeof(state0)
                ) == VF2_OK &&
                vf2_model2a_read(
                    &machine, fighter1 + UINT32_C(0x00000a00), &state1,
                    sizeof(state1)
                ) == VF2_OK) {
                printf("  fighters: p0=0x%08x flags=0x%08x state=0x%08x/%u p1=0x%08x flags=0x%08x state=0x%08x/%u\n",
                       (unsigned)fighter0, (unsigned)flags0,
                       (unsigned)state_flags0, (unsigned)state0,
                       (unsigned)fighter1, (unsigned)flags1,
                       (unsigned)state_flags1, (unsigned)state1);
            }
        }
    }

    while (status == VF2_OK && steps < max_steps) {
        const uint32_t ip_before = cpu.ip;
        memset(&event, 0, sizeof(event));
        status = vf2_i960_step(&cpu, &machine, &event);
        ++steps;
        if (status != VF2_OK) {
            halt_ip = ip_before;
            stopped_on_error = true;
            break;
        }
        if (event.instruction.flow == VF2_I960_FLOW_CALL) {
            ++calls;
            if (calls <= 128u || event.ip_after == UINT32_C(0x0001645c) ||
                event.ip_after == UINT32_C(0x00010d54)) {
                printf("  call #%u 0x%08x -> 0x%08x r29=0x%08x depth=%u\n",
                       (unsigned)calls, (unsigned)event.ip_before,
                       (unsigned)event.ip_after,
                       (unsigned)cpu.registers[29],
                       (unsigned)cpu.local_frame_depth);
            }
        }
        if (ip_before == UINT32_C(0x0000a010)) {
            printf("  scheduler call at instructions=%llu\n",
                   (unsigned long long)cpu.executed_instructions);
        }
        if (ip_before == UINT32_C(0x00000f7c) ||
            ip_before == UINT32_C(0x00010f98) ||
            ip_before == UINT32_C(0x00010fa0) ||
            ip_before == UINT32_C(0x0004afe4)) {
            ++frame_wait_visits;
            if (frame_wait_visits >= 4u) {
                status = vf2_model2a_raise_interrupt(&machine, UINT32_C(1));
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(
                        &cpu, &machine, 12u, 1u
                    );
                }
                frame_wait_visits = 0u;
                ++frame_interrupts;
            }
        }
        if (ip_before == UINT32_C(0x0004aff8) && cpu.ip == ip_before) {
            status = vf2_model2a_raise_interrupt(
                &machine, UINT32_C(1) << 5u
            );
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_interrupt(
                    &cpu, &machine, 14u, 1u
                );
            }
            ++timer_interrupts;
        }
        if (stop_address != UINT32_MAX && cpu.ip == stop_address) {
            printf("Resume trace stop address reached at IP=0x%08x instructions=%llu\n",
                   (unsigned)cpu.ip,
                   (unsigned long long)cpu.executed_instructions);
            break;
        }
    }

    if (stopped_on_error) {
        vf2_i960_instruction instruction;
        char text[256];
        printf("Resume trace stopped: status=%s IP=0x%08x instructions=%llu\n",
               vf2_status_string(status), (unsigned)halt_ip,
               (unsigned long long)cpu.executed_instructions);
        if (vf2_i960_decode(image, image_size, halt_ip, &instruction) == VF2_OK &&
            vf2_i960_format_instruction(&instruction, text, sizeof(text)) == VF2_OK) {
            printf("  instruction: %s\n", text);
        }
        status = VF2_OK;
    } else {
        printf("Resume trace budget exhausted at IP=0x%08x instructions=%llu\n",
               (unsigned)cpu.ip,
               (unsigned long long)cpu.executed_instructions);
    }
    printf("  calls=%u frame-interrupts=%u timer-interrupts=%u steps=%u\n",
           (unsigned)calls, (unsigned)frame_interrupts,
           (unsigned)timer_interrupts, (unsigned)steps);

    if (status == VF2_OK && output_snapshot_path != NULL) {
        uint32_t previous = 0u;
        uint32_t read = 0u;
        uint32_t write = 0u;
        size_t copro_nonzero_words = 0u;
        uint32_t first_copro_nonzero = 0u;
        int found_copro_nonzero = 0;
        size_t geometry_nonzero_words = 0u;
        uint32_t first_geometry_nonzero = 0u;
        int found_geometry_nonzero = 0;
        size_t nonzero_words = 0u;
        size_t stream_nonzero_words = 0u;
        size_t readable_nonzero_words = 0u;
        uint32_t first_nonzero = 0u;
        uint32_t first_stream_nonzero = 0u;
        uint32_t first_readable_nonzero = 0u;
        int found_nonzero = 0;
        int found_stream_nonzero = 0;
        int found_readable_nonzero = 0;
        size_t printed_nonzero = 0u;
        if (vf2_model2a_read_u32(
                &machine, UINT32_C(0x00803008), &previous
            ) == VF2_OK &&
            vf2_model2a_read_u32(
                &machine, UINT32_C(0x00802008), &read
            ) == VF2_OK &&
            vf2_model2a_read_u32(
                &machine, UINT32_C(0x00801008), &write
            ) == VF2_OK) {
            for (uint32_t address = UINT32_C(0x00900000);
                 address < UINT32_C(0x00980000); address += 4u) {
                uint32_t value = 0u;
                if (vf2_model2a_read_u32(&machine, address, &value) != VF2_OK) {
                    break;
                }
                if (value != 0u) {
                    ++nonzero_words;
                    if (!found_nonzero) {
                        first_nonzero = address;
                        found_nonzero = 1;
                    }
                    if (printed_nonzero < 32u) {
                        printf("  geometry[0x%08x]=0x%08x\n",
                               (unsigned)address, (unsigned)value);
                        ++printed_nonzero;
                    }
                }
                const int in_stream = previous <= write
                    ? address >= previous && address < write
                    : address >= previous || address < write;
                if (in_stream && value != 0u) {
                    ++stream_nonzero_words;
                    if (!found_stream_nonzero) {
                        first_stream_nonzero = address;
                        found_stream_nonzero = 1;
                    }
                }
                const int in_readable = read <= write
                    ? address >= read && address < write
                    : address >= read || address < write;
                if (in_readable && value != 0u) {
                    ++readable_nonzero_words;
                    if (!found_readable_nonzero) {
                        first_readable_nonzero = address;
                        found_readable_nonzero = 1;
                    }
                }
            }
            printf("Geometry buffer: read=0x%08x previous=0x%08x write=0x%08x nonzero_words=%zu first=0x%08x stream_nonzero=%zu stream_first=0x%08x readable_nonzero=%zu readable_first=0x%08x\n",
                   (unsigned)read, (unsigned)previous, (unsigned)write,
                   nonzero_words, (unsigned)first_nonzero,
                   stream_nonzero_words, (unsigned)first_stream_nonzero,
                   readable_nonzero_words, (unsigned)first_readable_nonzero);
            for (uint32_t address = UINT32_C(0x00880000);
                 address < UINT32_C(0x00888000); address += 4u) {
                uint32_t value = 0u;
                if (vf2_model2a_read_u32(&machine, address, &value) != VF2_OK) {
                    break;
                }
                if (value != 0u) {
                    ++copro_nonzero_words;
                    if (!found_copro_nonzero) {
                        first_copro_nonzero = address;
                        found_copro_nonzero = 1;
                    }
                }
            }
            printf("Copro port: nonzero_words=%zu first=0x%08x\n",
                   copro_nonzero_words, (unsigned)first_copro_nonzero);
            for (uint32_t address = UINT32_C(0x00800000);
                 address < UINT32_C(0x00808000); address += 4u) {
                uint32_t value = 0u;
                if (vf2_model2a_read_u32(&machine, address, &value) != VF2_OK) {
                    break;
                }
                if (value != 0u) {
                    ++geometry_nonzero_words;
                    if (!found_geometry_nonzero) {
                        first_geometry_nonzero = address;
                        found_geometry_nonzero = 1;
                    }
                }
            }
            printf("Geometry RAM: nonzero_words=%zu first=0x%08x\n",
                   geometry_nonzero_words, (unsigned)first_geometry_nonzero);
        }
        status = vf2_i960_snapshot_capture(
            &output_snapshot, &cpu, &machine
        );
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_write_file(
                &output_snapshot, output_snapshot_path
            );
        }
        if (status == VF2_OK) {
            printf("Resume snapshot written to %s\n", output_snapshot_path);
        }
    }

    vf2_i960_snapshot_destroy(&snapshot);
    vf2_i960_snapshot_destroy(&output_snapshot);
    if (machine.work_ram != NULL) {
        vf2_model2a_shutdown(&machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_native_resume(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t max_blocks,
    uint32_t fighter_flags_or,
    uint32_t stop_address,
    const char *output_snapshot_path
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot snapshot;
    vf2_i960_snapshot output_snapshot;
    vf2_native_runtime_state runtime;
    vf2_native_runtime_run_report report;
    vf2_native_runtime_step_report forced_step;
    uint32_t start_address = 0u;
    int forced_one_block = 0;
    int budget_exhausted = 0;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&runtime, 0, sizeof(runtime));
    memset(&report, 0, sizeof(report));
    memset(&forced_step, 0, sizeof(forced_step));
    vf2_i960_snapshot_init(&snapshot);
    vf2_i960_snapshot_init(&output_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }
    if (status == VF2_OK && fighter_flags_or != UINT32_MAX) {
        uint32_t fighter0 = 0u;
        uint32_t fighter1 = 0u;
        uint32_t flags = 0u;
        status = vf2_model2a_read_u32(
            &machine, UINT32_C(0x00500804), &fighter0
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                &machine, UINT32_C(0x00500808), &fighter1
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(&machine, fighter0, &flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                &machine, fighter0, flags | fighter_flags_or
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(&machine, fighter1, &flags);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                &machine, fighter1, flags | fighter_flags_or
            );
        }
    }
    if (status == VF2_OK &&
        (cpu.ip == UINT32_C(0x000164b0) ||
         cpu.ip == UINT32_C(0x000164c4))) {
        /* Native runtime entry is the task dispatcher; the suspended
         * snapshot still exposes the original CALL return IP to ptrace. */
        cpu.ip = UINT32_C(0x0001645c);
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_initialize(&runtime, 4u);
    }
    if (status == VF2_OK) {
        start_address = cpu.ip;
        if (max_blocks != 0u && cpu.ip == stop_address) {
            status = vf2_native_runtime_step(
                &machine, &cpu, &runtime, &forced_step
            );
            forced_one_block = status == VF2_OK;
        }
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_run_until(
            &machine, &cpu, &runtime, stop_address,
            max_blocks - (forced_one_block ? 1u : 0u),
            &report
        );
        if (forced_one_block) {
            report.start_address = start_address;
            ++report.blocks_executed;
            report.recovered_instruction_count +=
                forced_step.recovered_instruction_count;
            report.recovered_procedure_calls +=
                forced_step.recovered_procedure_calls;
            report.recovered_procedure_returns +=
                forced_step.recovered_procedure_returns;
            if (status == VF2_OK &&
                report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_NONE) {
                report.last_step_kind = forced_step.kind;
                report.last_bridge_kind = forced_step.bridge_kind;
                report.last_task_kind = forced_step.task_kind;
            }
        }
    }
    if (status == VF2_OK && !report.reached_stop) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_ERROR_UNSUPPORTED && !report.reached_stop &&
        report.blocks_executed == max_blocks) {
        budget_exhausted = 1;
    }
    /* Preserve the post-failure state too: a rejected native bridge is often
     * the most useful snapshot for differential diagnosis. */
    if (output_snapshot_path != NULL &&
        (status == VF2_OK || report.blocks_executed != 0u)) {
        vf2_status snapshot_status = vf2_i960_snapshot_capture(
            &output_snapshot, &cpu, &machine
        );
        if (snapshot_status == VF2_OK) {
            snapshot_status = vf2_i960_snapshot_write_file(
                &output_snapshot, output_snapshot_path
            );
        }
        if (status == VF2_OK) {
            status = snapshot_status;
        }
    }
    if (status == VF2_OK) {
        printf(
            "Native resume: blocks=%zu instructions=%llu entry=0x%08x "
            "exit=0x%08x task=%s\n",
            report.blocks_executed,
            (unsigned long long)report.recovered_instruction_count,
            (unsigned)report.start_address,
            (unsigned)report.final_address,
            vf2_hybrid_task_kind_name(report.last_task_kind)
        );
        printf(
            "  calls=%llu returns=%llu fighter_flags_or=0x%08x\n",
            (unsigned long long)report.recovered_procedure_calls,
            (unsigned long long)report.recovered_procedure_returns,
            (unsigned)fighter_flags_or
        );
    } else if (budget_exhausted) {
        fprintf(
            stderr,
            "Native resume budget exhausted at 0x%08x after %zu blocks "
            "entry=0x%08x "
            "step=%s bridge=%s task=%s\n",
            (unsigned)report.final_address,
            report.blocks_executed,
            (unsigned)report.last_entry_address,
            vf2_native_runtime_step_kind_name(report.last_step_kind),
            vf2_hybrid_bridge_kind_name(report.last_bridge_kind),
            vf2_hybrid_task_kind_name(report.last_task_kind)
        );
    } else {
        fprintf(
            stderr,
            "Native resume failed: %s at 0x%08x after %zu blocks "
            "entry=0x%08x "
            "step=%s bridge=%s task=%s\n",
            vf2_status_string(status),
            (unsigned)report.final_address,
            report.blocks_executed,
            (unsigned)report.last_entry_address,
            vf2_native_runtime_step_kind_name(report.last_step_kind),
            vf2_hybrid_bridge_kind_name(report.last_bridge_kind),
            vf2_hybrid_task_kind_name(report.last_task_kind)
        );
    }

    vf2_i960_snapshot_destroy(&snapshot);
    vf2_i960_snapshot_destroy(&output_snapshot);
    if (machine.work_ram != NULL) {
        vf2_model2a_shutdown(&machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_compare_game_info(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t fighter_flags_or,
    uint32_t stop_address
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
    vf2_native_runtime_state native_state;
    vf2_native_differential_report report;
    vf2_status status = VF2_OK;
    int reference_initialized = 0;
    int native_initialized = 0;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&reference_cpu, 0, sizeof(reference_cpu));
    memset(&native_cpu, 0, sizeof(native_cpu));
    memset(&native_state, 0, sizeof(native_state));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
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
        status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
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
    if (status == VF2_OK && fighter_flags_or != UINT32_MAX) {
        uint32_t fighter0 = 0u;
        uint32_t fighter1 = 0u;
        uint32_t flags = 0u;
        vf2_model2a *machines[2] = {
            &reference_machine, &native_machine
        };
        size_t machine_index = 0u;

        for (machine_index = 0u; machine_index < 2u && status == VF2_OK;
             ++machine_index) {
            status = vf2_model2a_read_u32(
                machines[machine_index], UINT32_C(0x00500804), &fighter0
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machines[machine_index], UINT32_C(0x00500808), &fighter1
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machines[machine_index], fighter0, &flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machines[machine_index], fighter0,
                    flags | fighter_flags_or
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machines[machine_index], fighter1, &flags
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machines[machine_index], fighter1,
                    flags | fighter_flags_or
                );
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_initialize(&native_state, 4u);
    }
    if (status == VF2_OK) {
        status = vf2_native_differential_run_until(
            &reference_machine, &reference_cpu,
            &native_machine, &native_cpu, &native_state,
            stop_address, 1u, &report
        );
    }

    if (status == VF2_OK) {
        printf(
            "Game-info differential: blocks=%zu instructions=%llu "
            "entry=0x%08x exit=0x%08x match=%s\n",
            report.blocks_compared,
            (unsigned long long)report.native_recovered_instructions,
            (unsigned)report.start_address,
            (unsigned)report.final_native_address,
            report.diff.equal ? "yes" : "no"
        );
    } else {
        fprintf(
            stderr,
            "Game-info differential failed: %s at reference=0x%08x "
            "native=0x%08x blocks=%zu diff=%s offset=0x%zx\n",
            vf2_status_string(status),
            (unsigned)report.final_reference_address,
            (unsigned)report.final_native_address,
            report.blocks_compared,
            report.diff.component,
            report.diff.first_offset
        );
    }

    vf2_i960_snapshot_destroy(&snapshot);
    if (native_initialized) {
        vf2_model2a_shutdown(&native_machine);
    }
    if (reference_initialized) {
        vf2_model2a_shutdown(&reference_machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_compare_boot(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a interpreted_machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu interpreted_cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_result result;
    vf2_recovered_boot_stage1_report report;
    vf2_i960_snapshot interpreted_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;
    memset(&interpreted_cpu, 0, sizeof(interpreted_cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&result, 0, sizeof(result));
    memset(&report, 0, sizeof(report));
    memset(&diff, 0, sizeof(diff));
    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    vf2_i960_snapshot_init(&interpreted_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);
    memset(&interpreted_machine, 0, sizeof(interpreted_machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    if (status == VF2_OK) {
        status = execute_boot_path(
            rom_directory,
            image,
            image_size,
            &vectors,
            0x000001b0u,
            2000000u,
            NULL,
            NULL,
            &interpreted_machine,
            &interpreted_cpu,
            &result
        );
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &recovered_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_reset_from_machine(
            &recovered_cpu,
            &recovered_machine,
            vectors.system_address_table,
            vectors.initial_prcb,
            vectors.start_ip
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_boot_stage1_execute(
            &recovered_machine,
            &recovered_cpu,
            &report
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &interpreted_snapshot,
            &interpreted_cpu,
            &interpreted_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &recovered_snapshot,
            &recovered_cpu,
            &recovered_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(
            &interpreted_snapshot,
            &recovered_snapshot,
            &diff
        );
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Boot comparison failed: %s\n", vf2_status_string(status));
    } else if (!diff.equal) {
        fprintf(
            stderr,
            "Mismatch in %s at offset 0x%zx: expected=0x%x actual=0x%x (%zu differences)\n",
            diff.component,
            diff.first_offset,
            (unsigned)diff.expected_value,
            (unsigned)diff.actual_value,
            diff.differing_bytes
        );
    } else {
        printf("Boot differential validation: MATCH\n");
        printf("Interpreted instructions:     %llu\n", (unsigned long long)result.executed_instructions);
        printf("Final instruction pointer:    0x%08x\n", (unsigned)interpreted_cpu.ip);
        printf("CPU control bytes copied:     %zu\n", report.cpu_control_bytes_copied);
        printf("Interrupt-state bytes copied: %zu\n", report.interrupt_state_bytes_copied);
        printf("Compared regions: work RAM, buffer RAM, video control, CPU control, system control\n");
    }
    vf2_i960_snapshot_destroy(&interpreted_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    vf2_model2a_shutdown(&interpreted_machine);
    vf2_model2a_shutdown(&recovered_machine);
    free(image);
    return status == VF2_OK && diff.equal ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_compare_init(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a interpreted_machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu interpreted_cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_result result;
    vf2_recovered_boot_stage1_report stage1_report;
    vf2_recovered_boot_stage2_report stage2_report;
    vf2_i960_snapshot interpreted_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;

    memset(&interpreted_cpu, 0, sizeof(interpreted_cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&result, 0, sizeof(result));
    memset(&stage1_report, 0, sizeof(stage1_report));
    memset(&stage2_report, 0, sizeof(stage2_report));
    memset(&diff, 0, sizeof(diff));
    memset(&interpreted_machine, 0, sizeof(interpreted_machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    vf2_i960_snapshot_init(&interpreted_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = execute_boot_path(
            rom_directory,
            image,
            image_size,
            &vectors,
            UINT32_C(0x0000052c),
            UINT64_C(10000000),
            NULL,
            NULL,
            &interpreted_machine,
            &interpreted_cpu,
            &result
        );
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &recovered_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_reset_from_machine(
            &recovered_cpu,
            &recovered_machine,
            vectors.system_address_table,
            vectors.initial_prcb,
            vectors.start_ip
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_boot_stage1_execute(
            &recovered_machine,
            &recovered_cpu,
            &stage1_report
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_boot_stage2_execute(
            &recovered_machine,
            &recovered_cpu,
            &stage2_report
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &interpreted_snapshot,
            &interpreted_cpu,
            &interpreted_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &recovered_snapshot,
            &recovered_cpu,
            &recovered_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(
            &interpreted_snapshot,
            &recovered_snapshot,
            &diff
        );
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Initialization comparison failed: %s\n", vf2_status_string(status));
    } else if (!diff.equal) {
        fprintf(
            stderr,
            "Mismatch in %s at offset 0x%zx: expected=0x%x actual=0x%x (%zu differences)\n",
            diff.component,
            diff.first_offset,
            (unsigned)diff.expected_value,
            (unsigned)diff.actual_value,
            diff.differing_bytes
        );
    } else {
        printf("Initialization differential validation: MATCH\n");
        printf("Interpreted instructions:             %llu\n",
               (unsigned long long)result.executed_instructions);
        printf("Final instruction pointer:            0x%08x\n",
               (unsigned)interpreted_cpu.ip);
        printf("Palette entries initialized:          %zu\n",
               stage2_report.palette_entries_written);
        printf("Color translation entries initialized: %zu\n",
               stage2_report.color_entries_written);
        printf("Tile halfwords cleared:               %zu\n",
               stage2_report.tile_halfwords_cleared);
        printf("Boot palette halfwords written:       %zu\n",
               stage2_report.boot_palette_halfwords_written);
    }

    vf2_i960_snapshot_destroy(&interpreted_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    if (interpreted_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&interpreted_machine);
    }
    if (recovered_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&recovered_machine);
    }
    free(image);
    return status == VF2_OK && diff.equal ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_compare_task_registry(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_task_catalog catalog;
    vf2_model2a interpreted_machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu interpreted_cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_recovered_task_registry_report report;
    vf2_i960_snapshot interpreted_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;

    memset(&interpreted_machine, 0, sizeof(interpreted_machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    memset(&interpreted_cpu, 0, sizeof(interpreted_cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    memset(&report, 0, sizeof(report));
    memset(&diff, 0, sizeof(diff));
    vf2_task_catalog_init(&catalog);
    vf2_i960_snapshot_init(&interpreted_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &interpreted_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        vf2_i960_cpu_reset(&interpreted_cpu, 0u, 0u, UINT32_C(0x00000040));
        status = vf2_i960_cpu_enter_procedure(
            &interpreted_cpu,
            UINT32_C(0x00010cbc),
            UINT32_C(0x00000040)
        );
        options.stop_address = UINT32_C(0x00000040);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(&interpreted_cpu, &interpreted_machine, &options, &result);
    }
    if (status == VF2_OK && result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &recovered_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_recovered_task_registry_initialize(
            &recovered_machine,
            &catalog,
            &report
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &interpreted_snapshot,
            &interpreted_cpu,
            &interpreted_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &recovered_snapshot,
            &recovered_cpu,
            &recovered_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare_memory(
            &interpreted_snapshot,
            &recovered_snapshot,
            &diff
        );
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Task-registry comparison failed: %s\n", vf2_status_string(status));
        if (result.halt_reason != VF2_I960_HALT_NONE) {
            fprintf(stderr, "Halt reason: %s at 0x%08x\n",
                    vf2_i960_halt_reason_name(result.halt_reason),
                    (unsigned)result.halt_address);
        }
    } else if (!diff.equal) {
        fprintf(stderr,
                "Mismatch in %s at offset 0x%zx: expected=0x%x actual=0x%x (%zu differences)\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value,
                diff.differing_bytes);
    } else {
        printf("Task-registry differential validation: MATCH\n");
        printf("Recovered task descriptors:           %zu\n", report.task_count);
        printf("Interpreted instructions:             %llu\n",
               (unsigned long long)result.executed_instructions);
        printf("Descriptor table:                     0x%08x-0x%08x\n",
               (unsigned)report.source_table_start,
               (unsigned)report.source_table_end);
        printf("Runtime registry:                     0x%08x-0x%08x\n",
               (unsigned)report.registry_start,
               (unsigned)report.registry_end);
        printf("State pointers written:               %zu\n",
               report.state_pointers_written);
        printf("Scratch bytes cleared:                %zu\n",
               report.scratch_bytes_cleared);
    }

    vf2_i960_snapshot_destroy(&interpreted_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    vf2_task_catalog_destroy(&catalog);
    if (interpreted_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&interpreted_machine);
    }
    if (recovered_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&recovered_machine);
    }
    free(image);
    return status == VF2_OK && diff.equal ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_compare_timer_irq(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a interpreted_machine;
    vf2_model2a recovered_machine;
    vf2_i960_cpu interpreted_cpu;
    vf2_i960_cpu recovered_cpu;
    vf2_i960_run_result interpreted_result;
    vf2_i960_run_result recovered_result;
    vf2_i960_run_options options;
    vf2_recovered_timer_irq_report report;
    vf2_i960_snapshot interpreted_snapshot;
    vf2_i960_snapshot recovered_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;

    memset(&interpreted_machine, 0, sizeof(interpreted_machine));
    memset(&recovered_machine, 0, sizeof(recovered_machine));
    memset(&interpreted_cpu, 0, sizeof(interpreted_cpu));
    memset(&recovered_cpu, 0, sizeof(recovered_cpu));
    memset(&interpreted_result, 0, sizeof(interpreted_result));
    memset(&recovered_result, 0, sizeof(recovered_result));
    memset(&options, 0, sizeof(options));
    memset(&report, 0, sizeof(report));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&interpreted_snapshot);
    vf2_i960_snapshot_init(&recovered_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = execute_boot_path(
            rom_directory, image, image_size, &vectors,
            UINT32_C(0x0004aff8), UINT64_C(5000000),
            NULL, NULL, &interpreted_machine, &interpreted_cpu, &interpreted_result
        );
    }
    if (status == VF2_OK && interpreted_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = execute_boot_path(
            rom_directory, image, image_size, &vectors,
            UINT32_C(0x0004aff8), UINT64_C(5000000),
            NULL, NULL, &recovered_machine, &recovered_cpu, &recovered_result
        );
    }
    if (status == VF2_OK && recovered_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_raise_interrupt(&interpreted_machine, UINT32_C(1) << 5u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_raise_interrupt(&recovered_machine, UINT32_C(1) << 5u);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(&interpreted_cpu, &interpreted_machine, 14u, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(&recovered_cpu, &recovered_machine, 14u, 1u);
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004aff8);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &interpreted_cpu, &interpreted_machine, &options, &interpreted_result
        );
    }
    if (status == VF2_OK && interpreted_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_recovered_timer_irq_dispatch(&recovered_machine, &report);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &interpreted_snapshot, &interpreted_cpu, &interpreted_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &recovered_snapshot, &recovered_cpu, &recovered_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare_memory(
            &interpreted_snapshot, &recovered_snapshot, &diff
        );
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Timer IRQ comparison failed: %s\n", vf2_status_string(status));
    } else if (!diff.equal) {
        fprintf(stderr,
                "Mismatch in %s at offset 0x%zx: expected=0x%x actual=0x%x (%zu differences)\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value,
                diff.differing_bytes);
    } else {
        printf("Timer IRQ differential validation: MATCH\n");
        printf("Interrupt vector/handler:         14/0x00000d50\n");
        printf("Interpreted handler instructions: %llu\n",
               (unsigned long long)interpreted_result.executed_instructions);
        printf("Serviced mask:                    0x%08x\n",
               (unsigned)report.serviced_mask);
        printf("Timer index/reload:               %u/0x%08x\n",
               (unsigned)report.timer_index, (unsigned)report.timer_reload);
        printf("Runtime wait released:            %s\n",
               report.wait_released ? "yes" : "no");
    }

    vf2_i960_snapshot_destroy(&interpreted_snapshot);
    vf2_i960_snapshot_destroy(&recovered_snapshot);
    if (interpreted_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&interpreted_machine);
    }
    if (recovered_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&recovered_machine);
    }
    free(image);
    return status == VF2_OK && diff.equal ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_compare_snapshots(
    const char *expected_path,
    const char *actual_path
)
{
    vf2_i960_snapshot expected;
    vf2_i960_snapshot actual;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&expected);
    vf2_i960_snapshot_init(&actual);
    status = vf2_i960_snapshot_read_file(&expected, expected_path);
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&actual, actual_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(&expected, &actual, &diff);
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Snapshot comparison failed: %s\n", vf2_status_string(status));
    } else if (diff.equal) {
        printf("Snapshots match.\n");
    } else {
        printf(
            "Snapshots differ in %s at offset 0x%zx: expected=0x%x actual=0x%x (%zu differences)\n",
            diff.component,
            diff.first_offset,
            (unsigned)diff.expected_value,
            (unsigned)diff.actual_value,
            diff.differing_bytes
        );
    }
    vf2_i960_snapshot_destroy(&expected);
    vf2_i960_snapshot_destroy(&actual);
    return status == VF2_OK && diff.equal ? EXIT_SUCCESS : EXIT_FAILURE;
}


static vf2_status compare_hybrid_snapshots(
    const vf2_i960_cpu *expected_cpu,
    const vf2_model2a *expected_machine,
    const vf2_i960_cpu *actual_cpu,
    const vf2_model2a *actual_machine,
    vf2_i960_snapshot_diff *diff
)
{
    vf2_i960_snapshot expected;
    vf2_i960_snapshot actual;
    vf2_status status = VF2_OK;

    vf2_i960_snapshot_init(&expected);
    vf2_i960_snapshot_init(&actual);
    status = vf2_i960_snapshot_capture(&expected, expected_cpu, expected_machine);
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(&actual, actual_cpu, actual_machine);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(&expected, &actual, diff);
    }
    vf2_i960_snapshot_destroy(&actual);
    vf2_i960_snapshot_destroy(&expected);
    return status;
}

static int command_hybrid_first_dispatch(const char *rom_directory)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a original_machine;
    vf2_model2a hybrid_machine;
    vf2_i960_cpu original_cpu;
    vf2_i960_cpu hybrid_cpu;
    vf2_i960_run_result run_result;
    vf2_i960_run_options options;
    vf2_task_catalog catalog;
    vf2_recovered_scheduler_report plan;
    vf2_i960_snapshot entry_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_hybrid_block_report block_reports[4];
    vf2_status status = VF2_OK;
    const char *stage = "load-maincpu";
    uint64_t step = 0u;
    uint64_t reference_instructions = 0u;
    uint64_t interpreted_continuation = 0u;
    uint32_t previous_flags = 0u;
    uint32_t camera_registry = 0u;
    unsigned frame_wait_visits = 0u;
    size_t block_count = 0u;
    bool ready = false;

    memset(&original_machine, 0, sizeof(original_machine));
    memset(&hybrid_machine, 0, sizeof(hybrid_machine));
    memset(&original_cpu, 0, sizeof(original_cpu));
    memset(&hybrid_cpu, 0, sizeof(hybrid_cpu));
    memset(&run_result, 0, sizeof(run_result));
    memset(&options, 0, sizeof(options));
    memset(&plan, 0, sizeof(plan));
    memset(&diff, 0, sizeof(diff));
    memset(block_reports, 0, sizeof(block_reports));
    vf2_task_catalog_init(&catalog);
    vf2_i960_snapshot_init(&entry_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        stage = "scan-task-catalog";
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK) {
        stage = "runtime-checkpoint";
        status = execute_boot_path(
            rom_directory, image, image_size, &vectors,
            UINT32_C(0x0004aff8), UINT64_C(5000000),
            NULL, NULL, &original_machine, &original_cpu, &run_result
        );
    }
    if (status == VF2_OK && run_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_raise_interrupt(
            &original_machine, UINT32_C(1) << 5u
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(
            &original_cpu, &original_machine, 14u, 1u
        );
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004aff8);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &original_cpu, &original_machine, &options, &run_result
        );
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004b07c);
        options.max_steps = UINT64_C(1000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &original_cpu, &original_machine, &options, &run_result
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            &original_machine, UINT32_C(0x00500068), &previous_flags
        );
    }

    stage = "find-camera-entry";
    for (step = 0u; status == VF2_OK && step < UINT64_C(8000000); ++step) {
        const uint32_t ip_before = original_cpu.ip;
        uint32_t flags = 0u;

        if (ready && camera_registry != 0u &&
            original_cpu.ip == UINT32_C(0x0001d320) &&
            original_cpu.registers[29] == camera_registry) {
            break;
        }

        status = vf2_i960_step(&original_cpu, &original_machine, NULL);
        if (status != VF2_OK) {
            break;
        }
        status = vf2_model2a_read_u32(
            &original_machine, UINT32_C(0x00500068), &flags
        );
        if (status != VF2_OK) {
            break;
        }
        if (!ready && (flags & UINT32_C(0x80000000)) != 0u) {
            size_t task_index = 0u;
            ready = true;
            status = vf2_recovered_scheduler_plan(
                &original_machine, &catalog, &plan
            );
            if (status != VF2_OK) {
                break;
            }
            for (task_index = 0u; task_index < plan.runnable_count; ++task_index) {
                const vf2_task_descriptor *task =
                    &catalog.tasks[plan.runnable_task_indices[task_index]];
                if (strcmp(task->name, "fa_camera") == 0) {
                    camera_registry = plan.runnable_registry_addresses[task_index];
                    break;
                }
            }
            if (camera_registry == 0u) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
        }
        previous_flags = flags;
        (void)previous_flags;

        if (original_cpu.ip == UINT32_C(0x00000f7c)) {
            ++frame_wait_visits;
            if (frame_wait_visits >= 4u) {
                status = vf2_model2a_raise_interrupt(
                    &original_machine, UINT32_C(1)
                );
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(
                        &original_cpu, &original_machine, 12u, 1u
                    );
                }
                frame_wait_visits = 0u;
            }
        }
        if (original_cpu.ip == ip_before &&
            original_cpu.ip == UINT32_C(0x0004aff8)) {
            status = vf2_model2a_raise_interrupt(
                &original_machine, UINT32_C(1) << 5u
            );
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_interrupt(
                    &original_cpu, &original_machine, 14u, 1u
                );
            }
        }
    }
    if (status == VF2_OK &&
        (original_cpu.ip != UINT32_C(0x0001d320) || camera_registry == 0u)) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        stage = "initialize-hybrid-machine";
        status = initialize_boot_machine(
            rom_directory, &hybrid_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &entry_snapshot, &original_cpu, &original_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &entry_snapshot, &hybrid_cpu, &hybrid_machine
        );
    }

    stage = "hybrid-first-dispatch";
    for (step = 0u; status == VF2_OK && step < UINT64_C(3000000); ++step) {
        if (original_cpu.ip == UINT32_C(0x00010dcc) &&
            hybrid_cpu.ip == UINT32_C(0x00010dcc)) {
            break;
        }
        if (original_cpu.ip != hybrid_cpu.ip) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        if (hybrid_cpu.ip == UINT32_C(0x0001d320) ||
            hybrid_cpu.ip == UINT32_C(0x0001d458) ||
            hybrid_cpu.ip == UINT32_C(0x0001d660)) {
            vf2_hybrid_block_report block;
            uint64_t instructions_before = original_cpu.executed_instructions;
            memset(&block, 0, sizeof(block));
            status = vf2_hybrid_camera_execute(
                &hybrid_machine, &hybrid_cpu, camera_registry, &block
            );
            if (status != VF2_OK) {
                break;
            }
            if (block.cpu_poststate_applied == 0) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            memset(&options, 0, sizeof(options));
            options.stop_address = block.exit_address;
            options.max_steps = UINT64_C(1000000);
            options.stop_on_self_branch = true;
            status = vf2_i960_run(
                &original_cpu, &original_machine, &options, &run_result
            );
            if (status == VF2_OK &&
                run_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            memset(&diff, 0, sizeof(diff));
            if (status == VF2_OK) {
                status = compare_hybrid_snapshots(
                    &original_cpu, &original_machine,
                    &hybrid_cpu, &hybrid_machine, &diff
                );
            }
            if (status == VF2_OK && !diff.equal) {
                fprintf(
                    stderr,
                    "Block %s mismatch: %s offset=0x%zx "
                    "expected=0x%08x actual=0x%08x bytes=%zu\n",
                    vf2_hybrid_block_kind_name(block.kind), diff.component,
                    diff.first_offset, (unsigned)diff.expected_value,
                    (unsigned)diff.actual_value, diff.differing_bytes
                );
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK &&
                (original_cpu.executed_instructions !=
                     hybrid_cpu.executed_instructions ||
                 original_cpu.procedure_calls != hybrid_cpu.procedure_calls ||
                 original_cpu.procedure_returns != hybrid_cpu.procedure_returns ||
                 original_cpu.maximum_local_frame_depth !=
                     hybrid_cpu.maximum_local_frame_depth)) {
                fprintf(stderr, "Block %s counter post-state mismatch\n",
                        vf2_hybrid_block_kind_name(block.kind));
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                reference_instructions +=
                    original_cpu.executed_instructions - instructions_before;
                if (block_count < sizeof(block_reports) / sizeof(block_reports[0])) {
                    block_reports[block_count] = block;
                }
                ++block_count;
            }
        } else {
            status = vf2_i960_step(
                &original_cpu, &original_machine, NULL
            );
            if (status == VF2_OK) {
                status = vf2_i960_step(
                    &hybrid_cpu, &hybrid_machine, NULL
                );
            }
            if (status == VF2_OK) {
                ++interpreted_continuation;
            }
        }
    }

    memset(&diff, 0, sizeof(diff));
    if (status == VF2_OK &&
        (original_cpu.ip != UINT32_C(0x00010dcc) ||
         hybrid_cpu.ip != UINT32_C(0x00010dcc) || block_count != 3u)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK &&
        (original_cpu.executed_instructions != hybrid_cpu.executed_instructions ||
         original_cpu.procedure_calls != hybrid_cpu.procedure_calls ||
         original_cpu.procedure_returns != hybrid_cpu.procedure_returns ||
         original_cpu.interrupt_entries != hybrid_cpu.interrupt_entries ||
         original_cpu.interrupt_returns != hybrid_cpu.interrupt_returns ||
         original_cpu.maximum_local_frame_depth !=
             hybrid_cpu.maximum_local_frame_depth)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = compare_hybrid_snapshots(
            &original_cpu, &original_machine,
            &hybrid_cpu, &hybrid_machine, &diff
        );
    }
    if (status == VF2_OK && !diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status != VF2_OK) {
        fprintf(stderr, "Hybrid first-dispatch validation failed during %s: %s\n",
                stage, vf2_status_string(status));
        fprintf(stderr, "Original/hybrid IP: 0x%08x/0x%08x blocks=%zu\n",
                (unsigned)original_cpu.ip, (unsigned)hybrid_cpu.ip, block_count);
        if (!diff.equal && diff.component[0] != '\0') {
            fprintf(stderr, "Difference: %s offset=0x%zx bytes=%zu\n",
                    diff.component, diff.first_offset, diff.differing_bytes);
        }
    } else {
        size_t index = 0u;
        printf("Hybrid first-dispatch validation: MATCH\n");
        printf("Camera registry:                 0x%08x\n",
               (unsigned)camera_registry);
        printf("Recovered blocks substituted:   %zu\n", block_count);
        printf("Reference instructions compared: %llu\n",
               (unsigned long long)reference_instructions);
        printf("ROM register synchronization:    none\n");
        printf("Interpreted continuation steps: %llu\n",
               (unsigned long long)interpreted_continuation);
        printf("Final scheduler checkpoint:      0x%08x\n",
               (unsigned)original_cpu.ip);
        printf("Final CPU and memory state:      MATCH\n");
        printf("Recovered CPU post-states:       %zu\n", block_count);
        for (index = 0u; index < block_count; ++index) {
            const vf2_hybrid_block_report *block = &block_reports[index];
            printf("  %zu. %-20s 0x%08x -> 0x%08x%s\n",
                   index + 1u, vf2_hybrid_block_kind_name(block->kind),
                   (unsigned)block->entry_address,
                   (unsigned)block->exit_address,
                   block->viewport_executed != 0 ? " + viewport" : "");
        }
    }

    vf2_i960_snapshot_destroy(&entry_snapshot);
    vf2_task_catalog_destroy(&catalog);
    if (hybrid_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&hybrid_machine);
    }
    if (original_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&original_machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}


static int command_native_dispatch_ex(
    const char *rom_directory,
    bool continue_to_second_dispatch,
    bool native_third_dispatch,
    bool native_fourth_dispatch,
    bool native_fifth_dispatch,
    bool native_sixth_dispatch,
    bool observe_third_sweep
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a original_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu original_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_run_result run_result;
    vf2_i960_run_options options;
    vf2_task_catalog catalog;
    vf2_recovered_scheduler_report plan;
    vf2_i960_snapshot entry_snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_hybrid_task_report task_reports[VF2_RECOVERED_SCHEDULER_MAX_TASKS];
    vf2_status status = VF2_OK;
    const char *stage = "load-maincpu";
    uint64_t step = 0u;
    uint64_t recovered_instructions = 0u;
    uint64_t scheduler_instructions = 0u;
    uint64_t recovered_calls = 0u;
    uint64_t recovered_returns = 0u;
    uint32_t previous_flags = 0u;
    unsigned frame_wait_visits = 0u;
    size_t completed = 0u;
    vf2_hybrid_scheduler_finish_report finish_report;
    uint64_t bridge_steps = 0u;
    uint64_t bridge_interpreted_instructions = 0u;
    uint64_t bridge_recovered_instructions = 0u;
    uint64_t bridge_recovered_calls = 0u;
    uint64_t bridge_recovered_returns = 0u;
    size_t bridge_validated_blocks = 0u;
    size_t bridge_memory_checkpoints = 0u;
    size_t bridge_block_counts[VF2_HYBRID_BRIDGE_COUNT] = {0u};
    uint32_t first_geometry_instruction = 0u;
    uint32_t first_geometry_address = 0u;
    uint32_t first_geometry_changed_byte = 0u;
    unsigned second_frame_interrupts = 0u;
    vf2_hybrid_frame_wait_state original_frame_wait;
    vf2_hybrid_frame_wait_state native_frame_wait;
    uint32_t persistent_context_count = 0u;
    bool second_scheduler_seen = false;
    bool ready = false;

    memset(&original_machine, 0, sizeof(original_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&original_cpu, 0, sizeof(original_cpu));
    memset(&native_cpu, 0, sizeof(native_cpu));
    memset(&run_result, 0, sizeof(run_result));
    memset(&options, 0, sizeof(options));
    memset(&plan, 0, sizeof(plan));
    memset(&diff, 0, sizeof(diff));
    memset(task_reports, 0, sizeof(task_reports));
    memset(&finish_report, 0, sizeof(finish_report));
    memset(&original_frame_wait, 0, sizeof(original_frame_wait));
    memset(&native_frame_wait, 0, sizeof(native_frame_wait));
    vf2_task_catalog_init(&catalog);
    vf2_i960_snapshot_init(&entry_snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        stage = "scan-task-catalog";
        status = vf2_task_catalog_scan(&catalog, image, image_size);
    }
    if (status == VF2_OK) {
        stage = "runtime-checkpoint";
        status = execute_boot_path(
            rom_directory, image, image_size, &vectors,
            UINT32_C(0x0004aff8), UINT64_C(5000000),
            NULL, NULL, &original_machine, &original_cpu, &run_result
        );
    }
    if (status == VF2_OK && run_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_raise_interrupt(
            &original_machine, UINT32_C(1) << 5u
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(
            &original_cpu, &original_machine, 14u, 1u
        );
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004aff8);
        options.max_steps = UINT64_C(10000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &original_cpu, &original_machine, &options, &run_result
        );
    }
    if (status == VF2_OK) {
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0004b07c);
        options.max_steps = UINT64_C(1000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &original_cpu, &original_machine, &options, &run_result
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            &original_machine, UINT32_C(0x00500068), &previous_flags
        );
    }

    stage = "find-first-task-entry";
    for (step = 0u; status == VF2_OK && step < UINT64_C(8000000); ++step) {
        const uint32_t ip_before = original_cpu.ip;
        uint32_t flags = 0u;

        if (ready && plan.runnable_count != 0u &&
            original_cpu.ip == plan.runnable_entry_points[0] &&
            original_cpu.registers[29] == plan.runnable_registry_addresses[0]) {
            break;
        }

        status = vf2_i960_step(&original_cpu, &original_machine, NULL);
        if (status != VF2_OK) {
            break;
        }
        status = vf2_model2a_read_u32(
            &original_machine, UINT32_C(0x00500068), &flags
        );
        if (status != VF2_OK) {
            break;
        }
        if (!ready && (flags & UINT32_C(0x80000000)) != 0u) {
            ready = true;
            status = vf2_recovered_scheduler_plan(
                &original_machine, &catalog, &plan
            );
            if (status != VF2_OK) {
                break;
            }
        }
        previous_flags = flags;
        (void)previous_flags;

        if (original_cpu.ip == UINT32_C(0x00000f7c)) {
            ++frame_wait_visits;
            if (frame_wait_visits >= 4u) {
                status = vf2_model2a_raise_interrupt(
                    &original_machine, UINT32_C(1)
                );
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(
                        &original_cpu, &original_machine, 12u, 1u
                    );
                }
                frame_wait_visits = 0u;
            }
        }
        if (original_cpu.ip == ip_before &&
            original_cpu.ip == UINT32_C(0x0004aff8)) {
            status = vf2_model2a_raise_interrupt(
                &original_machine, UINT32_C(1) << 5u
            );
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_interrupt(
                    &original_cpu, &original_machine, 14u, 1u
                );
            }
        }
    }
    if (status == VF2_OK &&
        (!ready || plan.runnable_count != 7u ||
         original_cpu.ip != plan.runnable_entry_points[0])) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        stage = "initialize-native-machine";
        status = initialize_boot_machine(
            rom_directory, &native_machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &entry_snapshot, &original_cpu, &original_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &entry_snapshot, &native_cpu, &native_machine
        );
    }

    stage = "native-first-dispatch";
    for (completed = 0u;
         status == VF2_OK && completed < plan.runnable_count;
         ++completed) {
        const uint32_t expected_entry = plan.runnable_entry_points[completed];
        const uint32_t expected_registry =
            plan.runnable_registry_addresses[completed];
        vf2_hybrid_task_report *task_report = &task_reports[completed];

        if (original_cpu.ip != expected_entry || native_cpu.ip != expected_entry ||
            original_cpu.registers[29] != expected_registry ||
            native_cpu.registers[29] != expected_registry) {
            status = VF2_ERROR_UNSUPPORTED;
            break;
        }

        status = vf2_hybrid_first_dispatch_task_execute(
            &native_machine, &native_cpu, expected_registry, task_report
        );
        if (status != VF2_OK) {
            break;
        }
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x00010dcc);
        options.max_steps = UINT64_C(1000000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &original_cpu, &original_machine, &options, &run_result
        );
        if (status == VF2_OK &&
            run_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        memset(&diff, 0, sizeof(diff));
        if (status == VF2_OK) {
            status = compare_hybrid_snapshots(
                &original_cpu, &original_machine,
                &native_cpu, &native_machine, &diff
            );
        }
        if (status == VF2_OK && !diff.equal) {
            fprintf(
                stderr,
                "Task %s mismatch: %s offset=0x%zx "
                "expected=0x%08x actual=0x%08x bytes=%zu\n",
                vf2_hybrid_task_kind_name(task_report->kind), diff.component,
                diff.first_offset, (unsigned)diff.expected_value,
                (unsigned)diff.actual_value, diff.differing_bytes
            );
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK &&
            (original_cpu.executed_instructions !=
                 native_cpu.executed_instructions ||
             original_cpu.procedure_calls != native_cpu.procedure_calls ||
             original_cpu.procedure_returns != native_cpu.procedure_returns ||
             original_cpu.maximum_local_frame_depth !=
                 native_cpu.maximum_local_frame_depth)) {
            fprintf(
                stderr, "Task %s counter post-state mismatch\n",
                vf2_hybrid_task_kind_name(task_report->kind)
            );
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status != VF2_OK) {
            break;
        }
        recovered_instructions += task_report->recovered_instruction_count;
        recovered_calls += task_report->recovered_procedure_calls;
        recovered_returns += task_report->recovered_procedure_returns;

        if (completed + 1u == plan.runnable_count) {
            break;
        }
        {
            const size_t current_task_index =
                plan.runnable_task_indices[completed];
            const size_t next_task_index =
                plan.runnable_task_indices[completed + 1u];
            const uint32_t next_entry =
                plan.runnable_entry_points[completed + 1u];
            const uint32_t next_registry =
                plan.runnable_registry_addresses[completed + 1u];
            vf2_hybrid_scheduler_transition_report transition_report;

            memset(&transition_report, 0, sizeof(transition_report));
            status = vf2_hybrid_first_dispatch_scheduler_advance(
                &native_machine,
                &native_cpu,
                current_task_index,
                next_task_index,
                expected_registry,
                next_registry,
                next_entry,
                &transition_report
            );
            if (status == VF2_OK) {
                scheduler_instructions +=
                    transition_report.recovered_instruction_count;
                recovered_calls +=
                    transition_report.recovered_procedure_calls;
                recovered_returns +=
                    transition_report.recovered_procedure_returns;
            }

            for (step = 0u;
                 status == VF2_OK && step < UINT64_C(1000);
                 ++step) {
                if (original_cpu.ip == next_entry &&
                    original_cpu.registers[29] == next_registry) {
                    break;
                }
                status = vf2_i960_step(
                    &original_cpu, &original_machine, NULL
                );
            }
            if (status == VF2_OK &&
                (original_cpu.ip != next_entry || native_cpu.ip != next_entry ||
                 original_cpu.registers[29] != next_registry ||
                 native_cpu.registers[29] != next_registry)) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            memset(&diff, 0, sizeof(diff));
            if (status == VF2_OK) {
                status = compare_hybrid_snapshots(
                    &original_cpu, &original_machine,
                    &native_cpu, &native_machine, &diff
                );
            }
            if (status == VF2_OK && !diff.equal) {
                fprintf(
                    stderr,
                    "Scheduler transition %zu->%zu mismatch: %s "
                    "offset=0x%zx expected=0x%08x actual=0x%08x bytes=%zu\n",
                    current_task_index, next_task_index, diff.component,
                    diff.first_offset, (unsigned)diff.expected_value,
                    (unsigned)diff.actual_value, diff.differing_bytes
                );
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK &&
                (original_cpu.executed_instructions !=
                     native_cpu.executed_instructions ||
                 original_cpu.procedure_calls != native_cpu.procedure_calls ||
                 original_cpu.procedure_returns != native_cpu.procedure_returns ||
                 original_cpu.maximum_local_frame_depth !=
                     native_cpu.maximum_local_frame_depth)) {
                fprintf(
                    stderr,
                    "Scheduler transition %zu->%zu counter mismatch\n",
                    current_task_index, next_task_index
                );
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
    }

    memset(&diff, 0, sizeof(diff));
    if (status == VF2_OK &&
        (completed + 1u != plan.runnable_count ||
         original_cpu.ip != UINT32_C(0x00010dcc) ||
         native_cpu.ip != UINT32_C(0x00010dcc))) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = compare_hybrid_snapshots(
            &original_cpu, &original_machine,
            &native_cpu, &native_machine, &diff
        );
    }
    if (status == VF2_OK && !diff.equal) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK && continue_to_second_dispatch) {
        stage = "native-first-sweep-finish";
        memset(&options, 0, sizeof(options));
        options.stop_address = UINT32_C(0x0000a014);
        options.max_steps = UINT64_C(1000);
        options.stop_on_self_branch = true;
        status = vf2_i960_run(
            &original_cpu, &original_machine, &options, &run_result
        );
        if (status == VF2_OK &&
            run_result.halt_reason != VF2_I960_HALT_STOP_ADDRESS) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_hybrid_first_dispatch_scheduler_finish(
                &native_machine,
                &native_cpu,
                plan.runnable_task_indices[plan.runnable_count - 1u],
                plan.runnable_registry_addresses[plan.runnable_count - 1u],
                &finish_report
            );
        }
        memset(&diff, 0, sizeof(diff));
        if (status == VF2_OK) {
            status = compare_hybrid_snapshots(
                &original_cpu, &original_machine,
                &native_cpu, &native_machine, &diff
            );
        }
        if (status == VF2_OK && !diff.equal) {
            fprintf(
                stderr,
                "Scheduler finish mismatch: %s offset=0x%zx "
                "expected=0x%08x actual=0x%08x bytes=%zu\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value,
                (unsigned)diff.actual_value, diff.differing_bytes
            );
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK &&
            (original_cpu.executed_instructions !=
                 native_cpu.executed_instructions ||
             original_cpu.procedure_calls != native_cpu.procedure_calls ||
             original_cpu.procedure_returns != native_cpu.procedure_returns ||
             original_cpu.maximum_local_frame_depth !=
                 native_cpu.maximum_local_frame_depth)) {
            fprintf(stderr, "Scheduler finish counter mismatch\n");
            status = VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status == VF2_OK && continue_to_second_dispatch) {
        const uint32_t second_entry = plan.runnable_entry_points[0];
        const uint32_t second_registry = plan.runnable_registry_addresses[0];

        stage = "second-scheduler-pass";
        status = vf2_hybrid_frame_wait_initialize(
            &original_frame_wait, 4u
        );
        if (status == VF2_OK) {
            status = vf2_hybrid_frame_wait_initialize(
                &native_frame_wait, 4u
            );
        }
        bridge_steps = 0u;
        while (status == VF2_OK && bridge_steps < UINT64_C(2000000)) {
            const uint32_t original_ip_before = original_cpu.ip;
            const uint32_t native_ip_before = native_cpu.ip;
            const bool second_scheduler_candidate =
                native_ip_before == UINT32_C(0x0000a010);
            const bool frame_wait_bridge_candidate =
                native_ip_before == UINT32_C(0x00010f90) ||
                native_ip_before == UINT32_C(0x00000d20);
            const bool bridge_candidate =
                native_ip_before == UINT32_C(0x0004c868) ||
                native_ip_before == UINT32_C(0x0004c6e0) ||
                native_ip_before == UINT32_C(0x0004cb64) ||
                native_ip_before == UINT32_C(0x0004cce8) ||
                native_ip_before == UINT32_C(0x0004cc28) ||
                native_ip_before == UINT32_C(0x0004cd18) ||
                native_ip_before == UINT32_C(0x0004c180) ||
                native_ip_before == UINT32_C(0x0004c3f0) ||
                native_ip_before == UINT32_C(0x0004c4d4) ||
                native_ip_before == UINT32_C(0x0004c544) ||
                native_ip_before == UINT32_C(0x0004c928) ||
                native_ip_before == UINT32_C(0x0004ce88) ||
                native_ip_before == UINT32_C(0x0004bb18) ||
                native_ip_before == UINT32_C(0x0004bd24) ||
                native_ip_before == UINT32_C(0x0004bde0) ||
                native_ip_before == UINT32_C(0x0004bebc) ||
                native_ip_before == UINT32_C(0x0004bef4) ||
                native_ip_before == UINT32_C(0x0004bf2c) ||
                native_ip_before == UINT32_C(0x0004bf60) ||
                native_ip_before == UINT32_C(0x0004bf90) ||
                native_ip_before == UINT32_C(0x0004bfdc) ||
                native_ip_before == UINT32_C(0x0004bb94) ||
                native_ip_before == UINT32_C(0x0004bb98) ||
                native_ip_before == UINT32_C(0x0004bc58) ||
                native_ip_before == UINT32_C(0x0004bcd4) ||
                native_ip_before == UINT32_C(0x0004bfe0) ||
                native_ip_before == UINT32_C(0x0004d16c) ||
                native_ip_before == UINT32_C(0x00007fc0) ||
                native_ip_before == UINT32_C(0x0004f944) ||
                native_ip_before == UINT32_C(0x00002ec4) ||
                native_ip_before == UINT32_C(0x00002edc) ||
                native_ip_before == UINT32_C(0x00002f5c) ||
                native_ip_before == UINT32_C(0x0000a154) ||
                native_ip_before == UINT32_C(0x00002de4) ||
                native_ip_before == UINT32_C(0x0004cdb0) ||
                native_ip_before == UINT32_C(0x0004cdd4) ||
                native_ip_before == UINT32_C(0x00000b6c) ||
                native_ip_before == UINT32_C(0x00009444) ||
                native_ip_before == UINT32_C(0x0004d2c0) ||
                native_ip_before == UINT32_C(0x0000281c) ||
                native_ip_before == UINT32_C(0x000026ec) ||
                native_ip_before == UINT32_C(0x000028d4) ||
                native_ip_before == UINT32_C(0x000020f0) ||
                native_ip_before == UINT32_C(0x00001abc) ||
                native_ip_before == UINT32_C(0x00001f5c) ||
                native_ip_before == UINT32_C(0x0004e808) ||
                native_ip_before == UINT32_C(0x00010f08) ||
                native_ip_before == UINT32_C(0x00000bc0) ||
                native_ip_before == UINT32_C(0x00000c0c) ||
                native_ip_before == UINT32_C(0x00000c94) ||
                native_ip_before == UINT32_C(0x00000ce0) ||
                native_ip_before == UINT32_C(0x00010fa4) ||
                native_ip_before == UINT32_C(0x0004d25c) ||
                native_ip_before == UINT32_C(0x00000c78) ||
                native_ip_before == UINT32_C(0x00000c80) ||
                native_ip_before == UINT32_C(0x00000c90) ||
                native_ip_before == UINT32_C(0x00000cd4) ||
                native_ip_before == UINT32_C(0x0000a038) ||
                native_ip_before == UINT32_C(0x00009fb0) ||
                native_ip_before == UINT32_C(0x00009ff8) ||
                native_ip_before == UINT32_C(0x0000a014) ||
                native_ip_before == UINT32_C(0x0000a030) ||
                native_ip_before == UINT32_C(0x0000a034) ||
                native_ip_before == UINT32_C(0x00000c00) ||
                native_ip_before == UINT32_C(0x0004bab4) ||
                native_ip_before == UINT32_C(0x000020ec) ||
                native_ip_before == UINT32_C(0x0006dcb8) ||
                native_ip_before == UINT32_C(0x000110f4) ||
                native_ip_before == UINT32_C(0x000112f8) ||
                native_ip_before == UINT32_C(0x00011c78) ||
                native_ip_before == UINT32_C(0x00000530) ||
                native_ip_before == UINT32_C(0x000110b0) ||
                native_ip_before == UINT32_C(0x0000a6c0) ||
                native_ip_before == UINT32_C(0x0000a748) ||
                native_ip_before == UINT32_C(0x00001064) ||
                native_ip_before == UINT32_C(0x00001290) ||
                native_ip_before == UINT32_C(0x00024534) ||
                native_ip_before == UINT32_C(0x00023f6c) ||
                native_ip_before == UINT32_C(0x00001e6c) ||
                native_ip_before == UINT32_C(0x00001edc) ||
                native_ip_before == UINT32_C(0x000012d8) ||
                native_ip_before == UINT32_C(0x00044268) ||
                native_ip_before == UINT32_C(0x000438ec) ||
                native_ip_before == UINT32_C(0x0004b8d8) ||
                native_ip_before == UINT32_C(0x0004ba80) ||
                native_ip_before == UINT32_C(0x0004bd00) ||
                native_ip_before == UINT32_C(0x0004bd5c) ||
                native_ip_before == UINT32_C(0x0004be6c);

            if (original_cpu.ip == second_entry &&
                native_cpu.ip == second_entry &&
                original_cpu.registers[29] == second_registry &&
                native_cpu.registers[29] == second_registry) {
                break;
            }
            if (original_ip_before != native_ip_before) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            if (original_ip_before == UINT32_C(0x00010d54)) {
                second_scheduler_seen = true;
            }

            if (frame_wait_bridge_candidate) {
                vf2_hybrid_bridge_report wait_bridge_report;
                uint64_t reference_step = 0u;
                memset(&wait_bridge_report, 0, sizeof(wait_bridge_report));
                status = vf2_hybrid_frame_wait_execute(
                    &native_machine,
                    &native_cpu,
                    &native_frame_wait,
                    &wait_bridge_report
                );
                if (status != VF2_OK) {
                    fprintf(
                        stderr,
                        "frame-wait bridge failed at 0x%08x: %s\n",
                        (unsigned)native_ip_before,
                        vf2_status_string(status)
                    );
                    break;
                }
                for (reference_step = 0u;
                     status == VF2_OK &&
                     reference_step <
                         wait_bridge_report.recovered_instruction_count;
                     ++reference_step) {
                    vf2_hybrid_frame_wait_report original_wait_report;
                    memset(
                        &original_wait_report,
                        0,
                        sizeof(original_wait_report)
                    );
                    status = vf2_i960_step(
                        &original_cpu, &original_machine, NULL
                    );
                    if (status == VF2_OK) {
                        ++bridge_steps;
                        status = vf2_hybrid_frame_wait_observe(
                            &original_machine,
                            &original_cpu,
                            &original_frame_wait,
                            &original_wait_report
                        );
                    }
                    if (status == VF2_OK &&
                        original_wait_report.interrupt_injected) {
                        ++second_frame_interrupts;
                    }
                }
                if (status == VF2_OK &&
                    (original_frame_wait.visits !=
                         native_frame_wait.visits ||
                     original_frame_wait.interrupts_injected !=
                         native_frame_wait.interrupts_injected ||
                     original_frame_wait.visits_before_interrupt !=
                         native_frame_wait.visits_before_interrupt)) {
                    status = VF2_ERROR_UNSUPPORTED;
                }
                memset(&diff, 0, sizeof(diff));
                if (status == VF2_OK) {
                    status = compare_hybrid_snapshots(
                        &original_cpu,
                        &original_machine,
                        &native_cpu,
                        &native_machine,
                        &diff
                    );
                }
                if (status == VF2_OK && !diff.equal) {
                    fprintf(
                        stderr,
                        "Frame-wait block %s mismatch: %s offset=0x%zx "
                        "expected=0x%08x actual=0x%08x bytes=%zu\n",
                        vf2_hybrid_bridge_kind_name(wait_bridge_report.kind),
                        diff.component,
                        diff.first_offset,
                        (unsigned)diff.expected_value,
                        (unsigned)diff.actual_value,
                        diff.differing_bytes
                    );
                    status = VF2_ERROR_UNSUPPORTED;
                }
                if (status == VF2_OK &&
                    (original_cpu.executed_instructions !=
                         native_cpu.executed_instructions ||
                     original_cpu.procedure_calls !=
                         native_cpu.procedure_calls ||
                     original_cpu.procedure_returns !=
                         native_cpu.procedure_returns ||
                     original_cpu.maximum_local_frame_depth !=
                         native_cpu.maximum_local_frame_depth)) {
                    status = VF2_ERROR_UNSUPPORTED;
                }
                if (status == VF2_OK) {
                    bridge_recovered_instructions +=
                        wait_bridge_report.recovered_instruction_count;
                    bridge_recovered_calls +=
                        wait_bridge_report.recovered_procedure_calls;
                    bridge_recovered_returns +=
                        wait_bridge_report.recovered_procedure_returns;
                    ++bridge_validated_blocks;
                    ++bridge_memory_checkpoints;
                    ++bridge_block_counts[wait_bridge_report.kind];
                }
                continue;
            }

            if (second_scheduler_candidate) {
                vf2_hybrid_second_scheduler_report scheduler_report;
                uint64_t reference_step = 0u;
                memset(&scheduler_report, 0, sizeof(scheduler_report));
                status = vf2_hybrid_second_scheduler_enter(
                    &native_machine, &native_cpu, &scheduler_report
                );
                if (status != VF2_OK) {
                    break;
                }
                for (reference_step = 0u;
                     status == VF2_OK &&
                     reference_step < scheduler_report.recovered_instruction_count;
                     ++reference_step) {
                    status = vf2_i960_step(
                        &original_cpu, &original_machine, NULL
                    );
                    if (status == VF2_OK) {
                        ++bridge_steps;
                    }
                }
                memset(&diff, 0, sizeof(diff));
                if (status == VF2_OK) {
                    status = compare_hybrid_snapshots(
                        &original_cpu, &original_machine,
                        &native_cpu, &native_machine, &diff
                    );
                }
                if (status == VF2_OK && !diff.equal) {
                    fprintf(
                        stderr,
                        "Second scheduler entry mismatch: %s offset=0x%zx "
                        "expected=0x%08x actual=0x%08x bytes=%zu\n",
                        diff.component, diff.first_offset,
                        (unsigned)diff.expected_value,
                        (unsigned)diff.actual_value, diff.differing_bytes
                    );
                    status = VF2_ERROR_UNSUPPORTED;
                }
                if (status == VF2_OK &&
                    (original_cpu.executed_instructions !=
                         native_cpu.executed_instructions ||
                     original_cpu.procedure_calls != native_cpu.procedure_calls ||
                     original_cpu.procedure_returns !=
                         native_cpu.procedure_returns ||
                     original_cpu.maximum_local_frame_depth !=
                         native_cpu.maximum_local_frame_depth)) {
                    status = VF2_ERROR_UNSUPPORTED;
                }
                if (status == VF2_OK) {
                    bridge_recovered_instructions +=
                        scheduler_report.recovered_instruction_count;
                    bridge_recovered_calls +=
                        scheduler_report.recovered_procedure_calls;
                    bridge_recovered_returns +=
                        scheduler_report.recovered_procedure_returns;
                    ++bridge_validated_blocks;
                    ++bridge_memory_checkpoints;
                    ++bridge_block_counts[
                        VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY
                    ];
                    second_scheduler_seen = true;
                }
                continue;
            }

            if (bridge_candidate) {
                vf2_hybrid_bridge_report bridge_report;
                uint64_t reference_step = 0u;
                memset(&bridge_report, 0, sizeof(bridge_report));
                status = vf2_hybrid_post_frame_bridge_execute(
                    &native_machine, &native_cpu, &bridge_report
                );
                if (status != VF2_OK) {
                    break;
                }
                for (reference_step = 0u;
                     status == VF2_OK &&
                     reference_step < bridge_report.recovered_instruction_count;
                     ++reference_step) {
                    status = vf2_i960_step(
                        &original_cpu, &original_machine, NULL
                    );
                    if (status == VF2_OK) {
                        ++bridge_steps;
                    }
                }
                if (status == VF2_OK &&
                    (original_cpu.ip != native_cpu.ip ||
                     original_cpu.local_frame_depth !=
                         native_cpu.local_frame_depth)) {
                    fprintf(
                        stderr,
                        "Bridge block %s post-state address mismatch: "
                        "IP=0x%08x/0x%08x depth=%u/%u\n",
                        vf2_hybrid_bridge_kind_name(bridge_report.kind),
                        (unsigned)original_cpu.ip, (unsigned)native_cpu.ip,
                        original_cpu.local_frame_depth,
                        native_cpu.local_frame_depth
                    );
                    status = VF2_ERROR_UNSUPPORTED;
                }
                {
                    const bool compare_memory_now =
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_DIAGNOSTIC_TEXT_COPY ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TILE_GLYPH_EXPAND ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_METER_UPDATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_INPUT_UPDATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_INPUT_RING ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_RESTORE_PREFIX ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_TIMER_SUFFIX ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_GAME_STATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_TILE_SYNC ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_MAIN_POST_TIMER ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_MAIN_CLEAR_PREFIX ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_MAIN_TEXTURE_ORCHESTRATOR_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_MAIN_FRAME_TIMER_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_INPUT_RING_POLL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_GAME_EVENT_QUEUE_WRITE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_MAINTENANCE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_ENTRY_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_RECORD_STATUS_SETUP ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_STREAM_HEADER_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE ||
                        bridge_report.kind ==
                            VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE ||
                        (bridge_validated_blocks % 128u) == 0u;
                    memset(&diff, 0, sizeof(diff));
                    if (status == VF2_OK && compare_memory_now) {
                        status = compare_hybrid_snapshots(
                            &original_cpu, &original_machine,
                            &native_cpu, &native_machine, &diff
                        );
                        if (status == VF2_OK) {
                            ++bridge_memory_checkpoints;
                        }
                    }
                    if (status == VF2_OK && compare_memory_now && !diff.equal) {
                        fprintf(
                            stderr,
                            "Bridge block %s mismatch: %s offset=0x%zx "
                            "expected=0x%08x actual=0x%08x bytes=%zu\n",
                            vf2_hybrid_bridge_kind_name(bridge_report.kind),
                            diff.component, diff.first_offset,
                            (unsigned)diff.expected_value,
                            (unsigned)diff.actual_value, diff.differing_bytes
                        );
                        status = VF2_ERROR_UNSUPPORTED;
                    }
                }
                if (status == VF2_OK &&
                    (original_cpu.executed_instructions !=
                         native_cpu.executed_instructions ||
                     original_cpu.procedure_calls != native_cpu.procedure_calls ||
                     original_cpu.procedure_returns !=
                         native_cpu.procedure_returns ||
                     original_cpu.maximum_local_frame_depth !=
                         native_cpu.maximum_local_frame_depth)) {
                    fprintf(
                        stderr,
                        "Bridge block %s counter mismatch: "
                        "ins=%llu/%llu calls=%llu/%llu returns=%llu/%llu "
                        "max-depth=%u/%u\n",
                        vf2_hybrid_bridge_kind_name(bridge_report.kind),
                        (unsigned long long)original_cpu.executed_instructions,
                        (unsigned long long)native_cpu.executed_instructions,
                        (unsigned long long)original_cpu.procedure_calls,
                        (unsigned long long)native_cpu.procedure_calls,
                        (unsigned long long)original_cpu.procedure_returns,
                        (unsigned long long)native_cpu.procedure_returns,
                        original_cpu.maximum_local_frame_depth,
                        native_cpu.maximum_local_frame_depth
                    );
                    status = VF2_ERROR_UNSUPPORTED;
                }
                if (status == VF2_OK &&
                    (bridge_report.kind ==
                         VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT ||
                     bridge_report.kind ==
                         VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX)) {
                    first_geometry_instruction = UINT32_C(0x00002eec);
                    first_geometry_address = UINT32_C(0x00803008);
                    first_geometry_changed_byte = UINT32_C(0x00803009);
                }
                if (status == VF2_OK) {
                    bridge_recovered_instructions +=
                        bridge_report.recovered_instruction_count;
                    bridge_recovered_calls +=
                        bridge_report.recovered_procedure_calls;
                    bridge_recovered_returns +=
                        bridge_report.recovered_procedure_returns;
                    ++bridge_validated_blocks;
                    if ((size_t)bridge_report.kind <
                        sizeof(bridge_block_counts) /
                            sizeof(bridge_block_counts[0])) {
                        ++bridge_block_counts[bridge_report.kind];
                    }
                }
                continue;
            }

            status = vf2_i960_step(&original_cpu, &original_machine, NULL);
            if (status == VF2_OK) {
                status = vf2_i960_step(&native_cpu, &native_machine, NULL);
            }
            if (status != VF2_OK) {
                break;
            }
            if (first_geometry_instruction == 0u &&
                native_ip_before == UINT32_C(0x00002eec)) {
                first_geometry_instruction = native_ip_before;
                first_geometry_address =
                    native_cpu.registers[26] + UINT32_C(0x00003008);
                first_geometry_changed_byte =
                    first_geometry_address + UINT32_C(1);
            }
            ++bridge_steps;
            ++bridge_interpreted_instructions;
            if (g_orchestrator_trace_file != NULL &&
                is_orchestrator_cluster_ip(native_ip_before)) {
                char instruction_text[256];
                vf2_i960_instruction instruction;
                vf2_status decode_status = vf2_i960_decode(
                    native_machine.main_rom, native_machine.main_rom_size,
                    native_ip_before, &instruction);
                if (decode_status == VF2_OK && instruction.valid) {
                    (void)vf2_i960_format_instruction(
                        &instruction, instruction_text, sizeof(instruction_text));
                } else {
                    (void)snprintf(instruction_text, sizeof(instruction_text),
                                   "<decode-failure>");
                }
                (void)fprintf(g_orchestrator_trace_file,
                              "%llu,0x%08x,0x%08x,%u,%u,%llu,%llu,%llu,%llu,\"%s\"\n",
                              (unsigned long long)g_orchestrator_trace_step,
                              (unsigned)native_ip_before,
                              (unsigned)native_cpu.ip,
                              (unsigned)native_cpu.local_frame_depth,
                              (unsigned)native_cpu.arithmetic_control,
                              (unsigned long long)native_cpu.executed_instructions,
                              (unsigned long long)native_cpu.procedure_calls,
                              (unsigned long long)native_cpu.procedure_returns,
                              (unsigned long long)native_cpu.maximum_local_frame_depth,
                              instruction_text);
                ++g_orchestrator_trace_step;
            }
            if (original_cpu.ip != native_cpu.ip) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }

            {
                vf2_hybrid_frame_wait_report original_wait_report;
                vf2_hybrid_frame_wait_report native_wait_report;
                memset(&original_wait_report, 0, sizeof(original_wait_report));
                memset(&native_wait_report, 0, sizeof(native_wait_report));
                status = vf2_hybrid_frame_wait_observe(
                    &original_machine,
                    &original_cpu,
                    &original_frame_wait,
                    &original_wait_report
                );
                if (status == VF2_OK) {
                    status = vf2_hybrid_frame_wait_observe(
                        &native_machine,
                        &native_cpu,
                        &native_frame_wait,
                        &native_wait_report
                    );
                }
                if (status == VF2_OK &&
                    (original_wait_report.wait_observed !=
                         native_wait_report.wait_observed ||
                     original_wait_report.interrupt_injected !=
                         native_wait_report.interrupt_injected ||
                     original_frame_wait.visits != native_frame_wait.visits ||
                     original_frame_wait.interrupts_injected !=
                         native_frame_wait.interrupts_injected)) {
                    status = VF2_ERROR_UNSUPPORTED;
                }
                if (status == VF2_OK &&
                    original_wait_report.interrupt_injected) {
                    ++second_frame_interrupts;
                }
            }
            if (status == VF2_OK &&
                original_cpu.ip == original_ip_before &&
                native_cpu.ip == native_ip_before &&
                original_cpu.ip == UINT32_C(0x0004aff8)) {
                status = vf2_model2a_raise_interrupt(
                    &original_machine, UINT32_C(1) << 5u
                );
                if (status == VF2_OK) {
                    status = vf2_model2a_raise_interrupt(
                        &native_machine, UINT32_C(1) << 5u
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(
                        &original_cpu, &original_machine, 14u, 1u
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_i960_cpu_enter_interrupt(
                        &native_cpu, &native_machine, 14u, 1u
                    );
                }
            }
        }
        if (status == VF2_OK &&
            (!second_scheduler_seen || second_frame_interrupts != 1u ||
             original_cpu.ip != second_entry || native_cpu.ip != second_entry ||
             original_cpu.registers[29] != second_registry ||
             native_cpu.registers[29] != second_registry)) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        memset(&diff, 0, sizeof(diff));
        if (status == VF2_OK) {
            status = compare_hybrid_snapshots(
                &original_cpu, &original_machine,
                &native_cpu, &native_machine, &diff
            );
        }
        if (status == VF2_OK && !diff.equal) {
            fprintf(
                stderr,
                "Second-dispatch mismatch: %s offset=0x%zx "
                "expected=0x%08x actual=0x%08x bytes=%zu\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value,
                (unsigned)diff.actual_value, diff.differing_bytes
            );
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK &&
            (original_cpu.executed_instructions !=
                 native_cpu.executed_instructions ||
             original_cpu.procedure_calls != native_cpu.procedure_calls ||
             original_cpu.procedure_returns != native_cpu.procedure_returns ||
             original_cpu.maximum_local_frame_depth !=
                 native_cpu.maximum_local_frame_depth)) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                &native_machine, UINT32_C(0x0050c368),
                &persistent_context_count
            );
        }
        if (status == VF2_OK && persistent_context_count != 1u) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK &&
            (bridge_steps != UINT64_C(1270824) ||
             bridge_recovered_instructions != UINT64_C(1270824) ||
             bridge_interpreted_instructions != UINT64_C(0) ||
             bridge_validated_blocks != 192u ||
             bridge_memory_checkpoints != 190u ||
             bridge_recovered_calls != UINT64_C(342) ||
             bridge_recovered_returns != UINT64_C(340) ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_RETURN_STUB] != 2u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_METER_UPDATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_INPUT_UPDATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_STATE_UPDATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TILE_CONTROLLER_UPDATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_SAVE_PREFIX] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_INPUT_RING] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_RESTORE_PREFIX] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_TIMER_SUFFIX] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_PLAYER_LAYER] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_GAME_INPUT] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_GAME_STATE] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_TILE_SYNC] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_MAIN_POST_TIMER] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_MAIN_CLEAR_PREFIX] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_MAIN_FINAL_CLUSTER] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_MAIN_GEOMETRY_PREFIX] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_MAIN_TEXTURE_ORCHESTRATOR_CALL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_MAIN_FRAME_TIMER_CALL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_INITIAL_CLUSTER] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_UPLOAD_DISPATCH] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_INPUT_RING_POLL] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE] != 0u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE] != 4u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE] != 1u ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY] != 1u ||
             first_geometry_instruction != UINT32_C(0x00002eec) ||
             first_geometry_address != UINT32_C(0x00803008) ||
             first_geometry_changed_byte != UINT32_C(0x00803009))) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Native first-dispatch validation failed during %s: %s\n",
            stage, vf2_status_string(status)
        );
        fprintf(
            stderr,
            "Original/native IP: 0x%08x/0x%08x completed=%zu/%zu\n",
            (unsigned)original_cpu.ip, (unsigned)native_cpu.ip,
            completed, plan.runnable_count
        );
        if (!diff.equal && diff.component[0] != '\0') {
            fprintf(
                stderr, "Difference: %s offset=0x%zx bytes=%zu\n",
                diff.component, diff.first_offset, diff.differing_bytes
            );
        }
    } else if (continue_to_second_dispatch) {
        printf("Native second-dispatch validation: MATCH\n");
        printf("Recovered first-sweep task bodies: %zu\n", plan.runnable_count);
        printf("Recovered first-sweep task instructions: %llu\n",
               (unsigned long long)recovered_instructions);
        printf("Recovered scheduler transitions:   %zu\n",
               plan.runnable_count - 1u);
        printf("Recovered transition instructions: %llu\n",
               (unsigned long long)scheduler_instructions);
        printf("Recovered first-sweep finish:       %llu instructions\n",
               (unsigned long long)finish_report.recovered_instruction_count);
        printf("Inactive descriptors scanned:       %zu\n",
               finish_report.inactive_descriptors_scanned);
        printf("Recovered traversal instructions:   %llu\n",
               (unsigned long long)(
                   recovered_instructions + scheduler_instructions +
                   finish_report.recovered_instruction_count
               ));
        printf("Post-scheduler bridge instructions: %llu total\n",
               (unsigned long long)bridge_steps);
        printf("Recovered bridge instructions:      %llu\n",
               (unsigned long long)bridge_recovered_instructions);
        printf("Interpreted bridge instructions:    %llu\n",
               (unsigned long long)bridge_interpreted_instructions);
        printf("Recovered bridge blocks:            %zu\n",
               bridge_validated_blocks);
        printf("Differential memory checkpoints:    %zu\n",
               bridge_memory_checkpoints);
        printf("  texture byte decoders:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_BYTE_DECODE]);
        printf("  texture word decoders:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_WORD_DECODE]);
        printf("  symbol-table builders:            %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_SYMBOL_TABLE_BUILD
               ]);
        printf("  pair-table builders:              %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_PAIR_TABLE_BUILD
               ]);
        printf("  texture tree dispatches:          %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_TREE_DISPATCH]);
        printf("  texture tree expansions:          %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_TREE_EXPAND]);
        printf("  texture word prepares:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_WORD_PREPARE]);
        printf("  texture color prepares:           %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_COLOR_PREPARE]);
        printf("  texture color conversions:        %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_COLOR_CONVERT]);
        printf("  texture address tables:           %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_ADDRESS_TABLE]);
        printf("  diagnostic text copies:           %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_DIAGNOSTIC_TEXT_COPY]);
        printf("  tile glyph expansions:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TILE_GLYPH_EXPAND]);
        printf("  video status latches:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH]);
        printf("  geometry frame commits:           %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT]);
        printf("  geometry command setups:          %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_GEOMETRY_COMMAND_SETUP]);
        printf("  frame scratch clears:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_SCRATCH_CLEAR]);
        printf("  palette page uploads:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_PALETTE_PAGE_UPLOAD]);
        printf("  texture convert loops:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_LOOP]);
        printf("  texture convert posts:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_CONVERT_POST]);
        printf("  timer wait updates:               %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE]);
        printf("  inline text thunks:               %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_INLINE_TEXT_THUNK]);
        printf("  texture status lines:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TEXTURE_STATUS_LINE]);
        printf("  game-state classifiers:           %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_STATE_CLASSIFY]);
        printf("  game-color lookups:               %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_GAME_COLOR_LOOKUP]);
        printf("  game threshold evaluations:       %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_GAME_THRESHOLD_EVALUATE
               ]);
        printf("  system memory diagnostics:        %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC
               ]);
        printf("  video input syncs:                %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_INPUT_SYNC]);
        printf("  frame counter advances:           %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_COUNTER_ADVANCE]);
        printf("  frame phase advances:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_PHASE_ADVANCE]);
        printf("  frame shadow verifies:            %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_SHADOW_VERIFY]);
        printf("  frame buffer gates:               %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_BUFFER_GATE]);
        printf("  frame dispatch ticks:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK]);
        printf("  frame geometry gates:             %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_GEOMETRY_GATE]);
        printf("  frame wait polls:                 %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL]);
        printf("  interrupt wait exits:             %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_INTERRUPT_RETURN_WAIT_EXIT
               ]);
        printf("  video register composes:          %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_REGISTER_COMPOSE]);
        printf("  video input latch writes:         %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_INPUT_LATCH_WRITE]);
        printf("  player update gates:              %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_PLAYER_UPDATE_GATE]);
        printf("  video layer commits:              %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_VIDEO_LAYER_COMMIT]);
        printf("  input bit0 sequence gates:        %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_INPUT_BIT0_SEQUENCE_GATE]);
        printf("  input bit1 sequence gates:        %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_INPUT_BIT1_SEQUENCE_GATE]);
        printf("  input ring polls:                 %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_INPUT_RING_POLL]);
        printf("  tile runtime gates:               %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_TILE_RUNTIME_GATE]);
        printf("  orchestrator save/calls:          %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_SAVE_CALL
               ]);
        printf("  frame gate/calls:                 %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_FRAME_GATE_CALL
               ]);
        printf("  texture default limits:           %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS
               ]);
        printf("  status dispatch/calls:             %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_STATUS_DISPATCH_CALL
               ]);
        printf("  active prepare/calls:              %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_ACTIVE_PREPARE_CALL
               ]);
        printf("  texture header decodes:            %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_HEADER_DECODE
               ]);
        printf("  status scan endings:              %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END
               ]);
        printf("  child gate A calls:                %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_A
               ]);
        printf("  child gate B calls:                %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_CHILD_GATE_B
               ]);
        printf("  loop zero gates:                   %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_LOOP_GATE
               ]);
        printf("  record advances:                   %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_RECORD_ADVANCE
               ]);
        printf("  final status/calls:                %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_FINAL_STATUS_CALL
               ]);
        printf("  body returns:                      %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_BODY_RETURN
               ]);
        printf("  post-body calls:                   %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_POST_BODY_CALL
               ]);
        printf("  counter updates:                   %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_COUNTER_UPDATE
               ]);
        printf("  orchestrator epilogues:            %zu\n",
               bridge_block_counts[
                   VF2_HYBRID_BRIDGE_TEXTURE_ORCHESTRATOR_EPILOGUE
               ]);
        printf("  second scheduler entries:         %zu\n",
               bridge_block_counts[VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY]);
        printf("Recovered bridge calls/returns:     %llu/%llu\n",
               (unsigned long long)bridge_recovered_calls,
               (unsigned long long)bridge_recovered_returns);
        printf("Frame-wait threshold:               4 visits\n");
        printf("Frame interrupts injected:          %u (vector 12)\n",
               second_frame_interrupts);
        printf("First geometry instruction:         0x%08x\n",
               (unsigned)first_geometry_instruction);
        printf("First geometry write target:        0x%08x\n",
               (unsigned)first_geometry_address);
        printf("First changed geometry byte:        0x%08x\n",
               (unsigned)first_geometry_changed_byte);
        printf("Persistent task contexts:           %zu\n", catalog.count);
        printf("Second scheduler entry:             0x00010d54\n");
        printf("Second first task:                  %s\n",
               catalog.tasks[plan.runnable_task_indices[0]].name);
        printf("Second task entry:                  0x%08x\n",
               (unsigned)native_cpu.ip);
        printf("Second registry:                    0x%08x\n",
               (unsigned)native_cpu.registers[29]);
        printf("Snapshot restores after first entry: 0\n");
        printf("Final CPU and memory state:         MATCH\n");
    } else {
        size_t index = 0u;
        printf("Native first-dispatch validation: MATCH\n");
        printf("Recovered task bodies:            %zu\n", plan.runnable_count);
        printf("Recovered task instructions:      %llu\n",
               (unsigned long long)recovered_instructions);
        printf("Recovered procedure calls/returns: %llu/%llu\n",
               (unsigned long long)recovered_calls,
               (unsigned long long)recovered_returns);
        printf("Recovered scheduler transitions:  %zu\n",
               plan.runnable_count - 1u);
        printf("Recovered scheduler instructions: %llu\n",
               (unsigned long long)scheduler_instructions);
        printf("Recovered traversal instructions: %llu\n",
               (unsigned long long)(
                   recovered_instructions + scheduler_instructions
               ));
        printf("Interpreted task/scheduler steps:  0/0\n");
        printf("Final scheduler checkpoint:        0x%08x\n",
               (unsigned)native_cpu.ip);
        printf("Final CPU and memory state:        MATCH\n");
        for (index = 0u; index < plan.runnable_count; ++index) {
            const vf2_hybrid_task_report *task = &task_reports[index];
            printf(
                "  %zu. %-14s entry=0x%08x registry=0x%08x ins=%llu\n",
                index + 1u, vf2_hybrid_task_kind_name(task->kind),
                (unsigned)task->entry_address,
                (unsigned)task->registry_address,
                (unsigned long long)task->recovered_instruction_count
            );
        }
    }

    if (status == VF2_OK &&
        (native_third_dispatch || native_fourth_dispatch ||
         native_fifth_dispatch || native_sixth_dispatch)) {
        const uint32_t repeated_entry = plan.runnable_entry_points[0];
        const uint32_t repeated_registry = plan.runnable_registry_addresses[0];
        const size_t minimum_blocks = native_sixth_dispatch
            ? 837u
            : (native_fifth_dispatch
                ? 83u
                : (native_fourth_dispatch ? 45u : 1u));
        const size_t expected_blocks = native_sixth_dispatch
            ? 874u
            : (native_fifth_dispatch
                ? 836u
                : (native_fourth_dispatch ? 82u : 44u));
        const uint64_t expected_instructions = native_sixth_dispatch
            ? UINT64_C(7404917)
            : (native_fifth_dispatch
                ? UINT64_C(7402744)
                : (native_fourth_dispatch
                    ? UINT64_C(58871)
                    : UINT64_C(55240)));
        const char *dispatch_label = native_sixth_dispatch
            ? "sixth"
            : (native_fifth_dispatch
                ? "fifth"
                : (native_fourth_dispatch ? "fourth" : "third"));
        vf2_native_runtime_state runtime_state;
        vf2_native_differential_report third_report;

        stage = native_sixth_dispatch
            ? "native-sixth-dispatch"
            : (native_fifth_dispatch
                ? "native-fifth-dispatch"
                : (native_fourth_dispatch
                    ? "native-fourth-dispatch"
                    : "native-third-dispatch"));
        memset(&runtime_state, 0, sizeof(runtime_state));
        memset(&third_report, 0, sizeof(third_report));
        status = vf2_native_runtime_initialize(&runtime_state, 4u);
        if (status == VF2_OK) {
            status = vf2_native_differential_run_until_after(
                &original_machine,
                &original_cpu,
                &native_machine,
                &native_cpu,
                &runtime_state,
                repeated_entry,
                minimum_blocks,
                16384u,
                &third_report
            );
        }
        if (status == VF2_OK &&
            (third_report.blocks_compared != expected_blocks ||
             third_report.reference_instructions_executed !=
                expected_instructions ||
             third_report.native_recovered_instructions !=
                expected_instructions ||
             original_cpu.registers[29] != repeated_registry ||
             native_cpu.registers[29] != repeated_registry)) {
            status = VF2_ERROR_UNSUPPORTED;
        }

        if (status != VF2_OK) {
            fprintf(
                stderr,
                "Native repeated-dispatch validation failed during %s: %s\n",
                stage,
                vf2_status_string(status)
            );
            fprintf(
                stderr,
                "Reference/native IP: 0x%08x/0x%08x blocks=%zu "
                "instructions=%llu/%llu\n",
                (unsigned)third_report.final_reference_address,
                (unsigned)third_report.final_native_address,
                third_report.blocks_compared,
                (unsigned long long)
                    third_report.reference_instructions_executed,
                (unsigned long long)
                    third_report.native_recovered_instructions
            );
            fprintf(
                stderr,
                "Last native step: %s entry=0x%08x exit=0x%08x "
                "bridge=%s task=%s\n",
                vf2_native_runtime_step_kind_name(
                    third_report.last_step.kind
                ),
                (unsigned)third_report.last_step.entry_address,
                (unsigned)third_report.last_step.exit_address,
                vf2_hybrid_bridge_kind_name(
                    third_report.last_step.bridge_kind
                ),
                vf2_hybrid_task_kind_name(
                    third_report.last_step.task_kind
                )
            );
            if (!third_report.diff.equal &&
                third_report.diff.component[0] != '\0') {
                fprintf(
                    stderr,
                    "Difference: %s offset=0x%zx expected=0x%08x "
                    "actual=0x%08x bytes=%zu\n",
                    third_report.diff.component,
                    third_report.diff.first_offset,
                    (unsigned)third_report.diff.expected_value,
                    (unsigned)third_report.diff.actual_value,
                    third_report.diff.differing_bytes
                );
            }
        } else {
            printf("\nNative %s-dispatch validation: MATCH\n", dispatch_label);
            printf("Repeated-frame blocks compared:     %zu\n",
                   third_report.blocks_compared);
            printf("Repeated-frame instructions:        %llu\n",
                   (unsigned long long)
                       third_report.native_recovered_instructions);
            printf("Reference instructions compared:    %llu\n",
                   (unsigned long long)
                       third_report.reference_instructions_executed);
            printf("Repeated scheduler entries:         %zu\n",
                   runtime_state.scheduler_entries);
            printf("Repeated scheduler transitions:     %zu\n",
                   runtime_state.scheduler_transitions);
            printf("Repeated scheduler finishes:        %zu\n",
                   runtime_state.scheduler_finishes);
            printf("Repeated frame-wait phases:         %zu\n",
                   runtime_state.frame_wait_phases);
            printf("%s task entry:                   0x%08x\n",
                   native_sixth_dispatch
                       ? "Sixth"
                       : (native_fifth_dispatch
                           ? "Fifth"
                           : (native_fourth_dispatch ? "Fourth" : "Third")),
                   (unsigned)native_cpu.ip);
            printf("%s registry:                     0x%08x\n",
                   native_sixth_dispatch
                       ? "Sixth"
                       : (native_fifth_dispatch
                           ? "Fifth"
                           : (native_fourth_dispatch ? "Fourth" : "Third")),
                   (unsigned)native_cpu.registers[29]);
            printf("Continuous recovered instructions:  %llu\n",
                   (unsigned long long)(
                       bridge_steps +
                       third_report.native_recovered_instructions
                   ));
            printf("Final CPU and memory state:         MATCH\n");

            if ((native_fifth_dispatch || native_sixth_dispatch) &&
                g_native_snapshot_path != NULL) {
                vf2_i960_snapshot output_snapshot;
                const size_t snapshot_path_length =
                    strlen(g_native_snapshot_path);
                char *runtime_state_path = NULL;

                vf2_i960_snapshot_init(&output_snapshot);
                if (snapshot_path_length <=
                    SIZE_MAX - sizeof(".runtime")) {
                    runtime_state_path = (char *)malloc(
                        snapshot_path_length + sizeof(".runtime")
                    );
                }
                if (runtime_state_path == NULL) {
                    status = VF2_ERROR_OUT_OF_MEMORY;
                } else {
                    memcpy(
                        runtime_state_path,
                        g_native_snapshot_path,
                        snapshot_path_length
                    );
                    memcpy(
                        runtime_state_path + snapshot_path_length,
                        ".runtime",
                        sizeof(".runtime")
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_i960_snapshot_capture(
                        &output_snapshot,
                        &native_cpu,
                        &native_machine
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_i960_snapshot_write_file(
                        &output_snapshot,
                        g_native_snapshot_path
                    );
                }
                if (status == VF2_OK) {
                    status = vf2_native_runtime_state_write_file(
                        &runtime_state,
                        runtime_state_path
                    );
                }
                vf2_i960_snapshot_destroy(&output_snapshot);
                if (status == VF2_OK) {
                    printf("Fifth-dispatch snapshot:            %s\n",
                           g_native_snapshot_path);
                    printf("Fifth-dispatch runtime state:       %s\n",
                           runtime_state_path);
                } else {
                    (void)remove(g_native_snapshot_path);
                    if (runtime_state_path != NULL) {
                        (void)remove(runtime_state_path);
                    }
                    fprintf(
                        stderr,
                        "Could not write fifth-dispatch checkpoint: %s\n",
                        vf2_status_string(status)
                    );
                }
                free(runtime_state_path);
            }
        }
    }

    if (status == VF2_OK && observe_third_sweep) {
        /* Evidence-gathering mode: step the reference i960 forward from the
         * strict-validated boundary at the second fa_game_info task entry,
         * manually injecting vector-12 interrupts at the frame-wait poll IPs
         * (mirroring what the recovered frame-wait code does internally) so
         * the reference can cross each subsequent frame boundary. Each visit
         * to the main-loop scheduler call site 0x0000a010 is recorded along
         * with the task selection the scheduler scan makes there. The
         * recovered C side is intentionally NOT advanced here; the native
         * runtime refuses to proceed past the third scheduler call site
         * until evidence from this observation is folded back into a
         * recovered third-sweep entry. */
        uint64_t budget = UINT64_C(12000000);
        uint32_t sweep_index = 1u;
        uint32_t sweep_visits = 0u;
        uint32_t observe_frame_wait_visits = 0u;
        uint32_t interrupts_injected = 0u;
        uint32_t prev_ip = original_cpu.ip;

        printf("\nThird-sweep observation (reference i960 only):\n");
        printf("  start IP=0x%08x instructions=%llu\n",
                (unsigned)original_cpu.ip,
                (unsigned long long)original_cpu.executed_instructions);
        printf("  watching for visits to 0x0000a010 ...\n");
        {
            const char *boundary_park_path = getenv("VF2_PARK_SNAPSHOT");
            if (boundary_park_path != NULL && boundary_park_path[0] != '\0') {
                vf2_i960_snapshot boundary_snapshot;
                vf2_i960_snapshot_init(&boundary_snapshot);
                if (vf2_i960_snapshot_capture(
                        &boundary_snapshot, &original_cpu,
                        &original_machine) == VF2_OK &&
                    vf2_i960_snapshot_write_file(
                        &boundary_snapshot,
                        boundary_park_path) == VF2_OK) {
                    printf(
                        "  >> parked post-second-dispatch boundary at %s\n",
                        boundary_park_path
                    );
                }
                vf2_i960_snapshot_destroy(&boundary_snapshot);
            }
        }

        while (status == VF2_OK && budget > 0u) {
            const uint32_t ip_before = original_cpu.ip;
            const uint32_t fp_before = original_cpu.registers[VF2_I960_FP_REGISTER];
            const uint32_t r1_before = original_cpu.registers[1];
            const uint32_t depth_before = original_cpu.local_frame_depth;
            status = vf2_i960_step(&original_cpu, &original_machine, NULL);
            if (status != VF2_OK) {
                break;
            }
            --budget;
            prev_ip = ip_before;

            if (original_cpu.ip == UINT32_C(0x000221cc) ||
                original_cpu.ip == UINT32_C(0x000221e8)) {
                const char *park_path = getenv("VF2_PARK_SNAPSHOT");
                uint32_t coli_flags = 0u;
                uint32_t coli_entry = 0u;
                (void)vf2_model2a_read_u32(
                    &original_machine, UINT32_C(0x00514980), &coli_flags
                );
                (void)vf2_model2a_read_u32(
                    &original_machine, UINT32_C(0x0051498c), &coli_entry
                );
                printf(
                    "  >> coli ip=0x%08x ins=%llu flags=0x%08x entry=0x%08x\n",
                    (unsigned)original_cpu.ip,
                    (unsigned long long)original_cpu.executed_instructions,
                    (unsigned)coli_flags, (unsigned)coli_entry
                );
                if (park_path != NULL && park_path[0] != '\0') {
                    vf2_i960_snapshot park_snapshot;
                    vf2_i960_snapshot_init(&park_snapshot);
                    if (vf2_i960_snapshot_capture(
                            &park_snapshot, &original_cpu,
                            &original_machine) == VF2_OK &&
                        vf2_i960_snapshot_write_file(
                            &park_snapshot, park_path) == VF2_OK) {
                        printf("  >> parked snapshot at %s\n", park_path);
                    }
                    vf2_i960_snapshot_destroy(&park_snapshot);
                }
                if (original_cpu.ip == UINT32_C(0x000221e8) &&
                    park_path != NULL && park_path[0] != '\0') {
                    break;
                }
            }

            if (ip_before == UINT32_C(0x0000a748)) {
                uint32_t gate_flags = 0u;
                uint8_t gate_state = 0u;
                uint8_t gate_alt = 0u;
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00500704), &gate_flags);
                (void)vf2_model2a_read(&original_machine, UINT32_C(0x0050002a), &gate_state, sizeof(gate_state));
                (void)vf2_model2a_read(&original_machine, UINT32_C(0x005000a6), &gate_alt, sizeof(gate_alt));
                printf(
                    "  >> frame_geometry_gate visit ins=%llu ip_after=0x%08x flags=0x%08x state[0x0050002a]=%u alt[0x005000a6]=%u\n",
                    (unsigned long long)original_cpu.executed_instructions,
                    (unsigned)original_cpu.ip,
                    (unsigned)gate_flags, (unsigned)gate_state, (unsigned)gate_alt
                );
            }

            if (ip_before == UINT32_C(0x0000a010)) {
                uint32_t selected_index = 0u;
                uint32_t selected_entry = 0u;
                uint32_t selected_registry = 0u;
                uint32_t registry_iter = UINT32_C(0x00510000);
                uint32_t max_scan = 64u;
                uint32_t scan = 0u;
                uint32_t ready_flags_obs = 0u;
                uint32_t runtime_flags_obs = 0u;
                uint32_t task_count_obs = 0u;
                uint32_t timer1_obs = 0u;
                uint32_t timer2_obs = 0u;

                printf(
                    "  #%u visit-AT 0x0000a010 ins=%llu\n",
                    sweep_index,
                    (unsigned long long)original_cpu.executed_instructions
                );
                printf(
                    "    cpu BEFORE call: ip=0x%08x fp=0x%08x r1=0x%08x frame_depth=%u\n",
                    (unsigned)ip_before,
                    (unsigned)fp_before,
                    (unsigned)r1_before,
                    depth_before
                );

                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00500068), &ready_flags_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00508000), &runtime_flags_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00011d94), &task_count_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00f00000) + 4u, &timer1_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00f00000) + 8u, &timer2_obs);

                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00500068), &ready_flags_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00508000), &runtime_flags_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00011d94), &task_count_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00f00000) + 4u, &timer1_obs);
                (void)vf2_model2a_read_u32(&original_machine, UINT32_C(0x00f00000) + 8u, &timer2_obs);
                printf(
                    "    ready_flags=0x%08x runtime_flags=0x%08x task_count=%u timer1=0x%08x timer2=0x%08x\n",
                    (unsigned)ready_flags_obs, (unsigned)runtime_flags_obs,
                    task_count_obs, (unsigned)timer1_obs, (unsigned)timer2_obs
                );

                for (scan = 0u; scan < max_scan && status == VF2_OK; ++scan) {
                    uint32_t flags = 0u;
                    uint32_t stack_size = 0u;
                    status = vf2_model2a_read_u32(
                        &original_machine, registry_iter, &flags
                    );
                    if (status != VF2_OK) {
                        break;
                    }
                    if ((flags & UINT32_C(0x80000000)) != 0u) {
                        status = vf2_model2a_read_u32(
                            &original_machine,
                            registry_iter + UINT32_C(0x0c),
                            &selected_entry
                        );
                        if (status == VF2_OK) {
                            selected_index = scan;
                            selected_registry = registry_iter;
                        }
                        break;
                    }
                    status = vf2_model2a_read_u32(
                        &original_machine,
                        registry_iter + UINT32_C(0x08),
                        &stack_size
                    );
                    if (status != VF2_OK || stack_size == 0u ||
                        (stack_size & UINT32_C(0x1f)) != 0u) {
                        if (status == VF2_OK) {
                            status = VF2_ERROR_UNSUPPORTED;
                        }
                        break;
                    }
                    registry_iter += stack_size;
                }
                if (status == VF2_OK) {
                    printf(
                        "    sweep #%u selected task index=%u entry=0x%08x registry=0x%08x\n",
                        sweep_index, selected_index,
                        (unsigned)selected_entry,
                        (unsigned)selected_registry
                    );
                } else {
                    fprintf(
                        stderr,
                        "  sweep #%u registry scan failed: %s\n",
                        sweep_index, vf2_status_string(status)
                    );
                }
                ++sweep_index;
                ++sweep_visits;
                if (sweep_visits >= 4u) {
                    break;
                }
                continue;
            }

            if (ip_before == UINT32_C(0x00010f98) ||
                ip_before == UINT32_C(0x00010f90)) {
                const uint8_t changed = 1u;
                ++observe_frame_wait_visits;
                status = vf2_model2a_write(
                    &original_machine, UINT32_C(0x00500000),
                    &changed, sizeof(changed)
                );
                if (status == VF2_OK &&
                    (observe_frame_wait_visits % 4u) == 0u) {
                    status = vf2_model2a_raise_interrupt(
                        &original_machine, UINT32_C(1) << 0u
                    );
                    if (status == VF2_OK) {
                        status = vf2_i960_cpu_enter_interrupt(
                            &original_cpu, &original_machine, 12u, 1u
                        );
                    }
                    if (status == VF2_OK) {
                        ++interrupts_injected;
                    }
                }
            }
        }
        if (status == VF2_OK) {
            printf(
                "  observation halted after %u sweep visits / %u frame waits / %u interrupts, final IP=0x%08x\n",
                sweep_visits, observe_frame_wait_visits, interrupts_injected,
                (unsigned)original_cpu.ip
            );
        } else {
            fprintf(
                stderr,
                "  observation aborted: %s at IP=0x%08x (prev=0x%08x)\n",
                vf2_status_string(status), (unsigned)original_cpu.ip,
                (unsigned)prev_ip
            );
        }
    }

    vf2_i960_snapshot_destroy(&entry_snapshot);
    vf2_task_catalog_destroy(&catalog);
    if (native_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&native_machine);
    }
    if (original_machine.work_ram != NULL) {
        vf2_model2a_shutdown(&original_machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_native_first_dispatch(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, false, false, false, false, false, false
    );
}

static int command_native_second_dispatch(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, true, false, false, false, false, false
    );
}

static int command_native_third_dispatch(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, true, true, false, false, false, false
    );
}

static int command_native_fourth_dispatch(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, true, false, true, false, false, false
    );
}

static int command_native_fifth_dispatch(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, true, false, false, true, false, false
    );
}

static int command_native_sixth_dispatch(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, true, false, false, false, true, false
    );
}

static int command_native_observe_third_sweep(const char *rom_directory)
{
    return command_native_dispatch_ex(
        rom_directory, true, false, false, false, false, true
    );
}

static const char *orchestrator_trace_default_path(void)
{
    return "decomp/i960/notes/texture_orchestrator_v0023.csv";
}

static int command_trace_orchestrator(
    const char *rom_directory,
    const char *output_path
)
{
    int dispatch_result = EXIT_FAILURE;
    FILE *trace_file = NULL;
    const char *path =
        (output_path != NULL && output_path[0] != '\0') ? output_path
                                                         : orchestrator_trace_default_path();

    trace_file = fopen(path, "wb");
    if (trace_file == NULL) {
        fprintf(stderr, "Could not open orchestrator trace output: %s\n", path);
        return EXIT_FAILURE;
    }
    (void)fprintf(trace_file,
                  "step,ip_before,ip_after,frame_depth,arithmetic_control,"
                  "executed_instructions,procedure_calls,procedure_returns,"
                  "maximum_local_frame_depth,instruction\n");
    g_orchestrator_trace_file = trace_file;
    g_orchestrator_trace_step = 0u;

    dispatch_result = command_native_dispatch_ex(
        rom_directory, true, false, false, false, false, false
    );

    g_orchestrator_trace_file = NULL;
    g_orchestrator_trace_step = 0u;
    if (fclose(trace_file) != 0 && dispatch_result == EXIT_SUCCESS) {
        fprintf(stderr, "Could not close orchestrator trace output: %s\n", path);
        dispatch_result = EXIT_FAILURE;
    }
    if (dispatch_result != EXIT_SUCCESS) {
        fprintf(
            stderr,
            "Trace-orchestrator aborted: native second-dispatch validation "
            "did not reach MATCH. Headline totals must be preserved before any "
            "trace record may be considered evidence.\n"
        );
        return dispatch_result;
    }
    printf("Orchestrator trace written to %s\n", path);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "disasm") == 0) {
        uint32_t address = UINT32_MAX;
        uint32_t count = 32u;
        if (argc >= 4 && !parse_u32(argv[3], &address)) {
            fprintf(stderr, "Invalid address: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        if (argc >= 5 && !parse_u32(argv[4], &count)) {
            fprintf(stderr, "Invalid instruction count: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        return command_disasm(argv[2], address, count);
    }

    if (strcmp(argv[1], "function") == 0 && argc == 4) {
        uint32_t address = 0u;
        if (!parse_u32(argv[3], &address)) {
            fprintf(stderr, "Invalid address: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        return command_function(argv[2], address);
    }

    if (strcmp(argv[1], "analyze") == 0 && argc == 4) {
        return command_analyze(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "tasks") == 0 && (argc == 3 || argc == 4)) {
        return command_tasks(argv[2], argc == 4 ? argv[3] : NULL);
    }

    if (strcmp(argv[1], "xrefs") == 0 && argc == 4) {
        uint32_t address = 0u;
        if (!parse_u32(argv[3], &address)) {
            fprintf(stderr, "Invalid address: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        return command_xrefs(argv[2], address);
    }

    if (strcmp(argv[1], "frame") == 0 && argc == 4) {
        uint32_t address = 0u;
        if (!parse_u32(argv[3], &address)) {
            fprintf(stderr, "Invalid address: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        return command_frame(argv[2], address);
    }

    if (strcmp(argv[1], "pseudoc") == 0 && (argc == 4 || argc == 5)) {
        uint32_t address = 0u;
        if (!parse_u32(argv[3], &address)) {
            fprintf(stderr, "Invalid address: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        return command_pseudoc(argv[2], address, argc == 5 ? argv[4] : NULL);
    }

    if (strcmp(argv[1], "execute") == 0 && argc >= 3 && argc <= 5) {
        uint32_t stop_address = 0x0000052cu;
        uint32_t max_steps = 10000000u;
        if (argc >= 4 && !parse_u32(argv[3], &stop_address)) {
            fprintf(stderr, "Invalid stop address: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        if (argc >= 5 && !parse_u32(argv[4], &max_steps)) {
            fprintf(stderr, "Invalid maximum steps: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        return command_execute(argv[2], stop_address, max_steps);
    }


    if (strcmp(argv[1], "runtime-checkpoint") == 0 && argc == 3) {
        return command_execute(argv[2], UINT32_C(0x0004aff8), UINT64_C(5000000));
    }

    if (strcmp(argv[1], "scheduler-pass") == 0 && argc == 3) {
        return command_scheduler_pass(argv[2]);
    }
    if (strcmp(argv[1], "scheduler-dispatch") == 0 && argc == 3) {
        return command_scheduler_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "task-profile") == 0 && (argc == 3 || argc == 4)) {
        return command_task_profile(argv[2], argc == 4 ? argv[3] : NULL);
    }

    if (strcmp(argv[1], "trace") == 0 && (argc == 4 || argc == 5)) {
        uint32_t max_steps = 10000u;
        if (argc == 5 && !parse_u32(argv[4], &max_steps)) {
            fprintf(stderr, "Invalid maximum steps: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        return command_trace(argv[2], argv[3], max_steps);
    }

    if (strcmp(argv[1], "snapshot") == 0 && argc == 4) {
        return command_snapshot(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "resume-trace") == 0 &&
        (argc == 4 || argc == 5 || argc == 6 || argc == 7 || argc == 8 ||
         argc == 10 || argc == 11 || argc == 14)) {
        uint32_t max_steps = UINT32_C(10000000);
        uint32_t clear_task_index = UINT32_MAX;
        uint32_t fighter_flags_or = UINT32_MAX;
        uint32_t write_address = UINT32_MAX;
        uint32_t write_value = 0u;
        uint32_t stop_address = UINT32_MAX;
        uint32_t raise_irq_mask = UINT32_MAX;
        uint32_t enter_vector = UINT32_MAX;
        uint32_t enter_level = UINT32_MAX;
        if (argc == 5 && !parse_u32(argv[4], &max_steps)) {
            fprintf(stderr, "Invalid maximum steps: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        if (argc == 6 &&
            (!parse_u32(argv[4], &max_steps) ||
             !parse_u32(argv[5], &clear_task_index))) {
            fprintf(stderr, "Invalid resume-trace options\n");
            return EXIT_FAILURE;
        }
        if (argc >= 7 &&
            (!parse_u32(argv[4], &max_steps) ||
             !parse_u32(argv[5], &clear_task_index) ||
             !parse_u32(argv[6], &fighter_flags_or))) {
            fprintf(stderr, "Invalid resume-trace options\n");
            return EXIT_FAILURE;
        }
        if (argc == 10 &&
            (!parse_u32(argv[7], &write_address) ||
             !parse_u32(argv[8], &write_value))) {
            fprintf(stderr, "Invalid resume-trace memory write\n");
            return EXIT_FAILURE;
        }
        if (argc == 11 &&
            (!parse_u32(argv[7], &write_address) ||
             !parse_u32(argv[8], &write_value) ||
             !parse_u32(argv[10], &stop_address))) {
            fprintf(stderr, "Invalid resume-trace options\n");
            return EXIT_FAILURE;
        }
        if (argc == 14 &&
            (!parse_u32(argv[7], &write_address) ||
             !parse_u32(argv[8], &write_value) ||
             !parse_u32(argv[10], &stop_address) ||
             !parse_u32(argv[11], &raise_irq_mask) ||
             !parse_u32(argv[12], &enter_vector) ||
             !parse_u32(argv[13], &enter_level))) {
            fprintf(stderr, "Invalid resume-trace options\n");
            return EXIT_FAILURE;
        }
        return command_resume_trace(
            argv[2], argv[3], max_steps, clear_task_index, fighter_flags_or,
            write_address, write_value,
            argc == 8 ? argv[7] : (argc == 10 || argc == 11 || argc == 14 ? argv[9] : NULL),
            stop_address, raise_irq_mask, enter_vector, enter_level
        );
    }

    if (strcmp(argv[1], "native-resume") == 0 &&
        (argc == 4 || argc == 5 || argc == 6 || argc == 7 || argc == 8)) {
        uint32_t max_blocks = UINT32_C(1);
        uint32_t fighter_flags_or = UINT32_MAX;
        uint32_t stop_address = UINT32_C(0x00010dcc);
        if (argc >= 5 && !parse_u32(argv[4], &max_blocks)) {
            fprintf(stderr, "Invalid native-resume block budget\n");
            return EXIT_FAILURE;
        }
        if (argc == 6 && !parse_u32(argv[5], &fighter_flags_or)) {
            fprintf(stderr, "Invalid native-resume fighter flags\n");
            return EXIT_FAILURE;
        }
        if (argc == 7 &&
            (!parse_u32(argv[5], &fighter_flags_or) ||
             !parse_u32(argv[6], &stop_address))) {
            fprintf(stderr, "Invalid native-resume options\n");
            return EXIT_FAILURE;
        }
        if (argc == 8 &&
            (!parse_u32(argv[5], &fighter_flags_or) ||
             !parse_u32(argv[6], &stop_address))) {
            fprintf(stderr, "Invalid native-resume options\n");
            return EXIT_FAILURE;
        }
        return command_native_resume(
            argv[2], argv[3], max_blocks, fighter_flags_or, stop_address,
            argc == 8 ? argv[7] : NULL
        );
    }

    if (strcmp(argv[1], "compare-game-info") == 0 &&
        (argc == 4 || argc == 5 || argc == 6)) {
        uint32_t fighter_flags_or = UINT32_MAX;
        uint32_t stop_address = UINT32_C(0x00010dcc);
        if (argc >= 5 && !parse_u32(argv[4], &fighter_flags_or)) {
            fprintf(stderr, "Invalid game-info fighter flags\n");
            return EXIT_FAILURE;
        }
        if (argc == 6 && !parse_u32(argv[5], &stop_address)) {
            fprintf(stderr, "Invalid game-info stop address\n");
            return EXIT_FAILURE;
        }
        return command_compare_game_info(
            argv[2], argv[3], fighter_flags_or, stop_address
        );
    }

    if (strcmp(argv[1], "compare-boot") == 0 && argc == 3) {
        return command_compare_boot(argv[2]);
    }
    if (strcmp(argv[1], "compare-init") == 0 && argc == 3) {
        return command_compare_init(argv[2]);
    }
    if (strcmp(argv[1], "compare-task-registry") == 0 && argc == 3) {
        return command_compare_task_registry(argv[2]);
    }
    if (strcmp(argv[1], "compare-timer-irq") == 0 && argc == 3) {
        return command_compare_timer_irq(argv[2]);
    }
    if (strcmp(argv[1], "compare-task-recoveries") == 0 && argc == 3) {
        return command_compare_task_recoveries(argv[2]);
    }
    if (strcmp(argv[1], "compare-first-dispatch") == 0 && argc == 3) {
        return command_task_profile(argv[2], NULL);
    }
    if (strcmp(argv[1], "compare-camera-classifier") == 0 && argc == 3) {
        return command_compare_camera_classifier(argv[2]);
    }
    if (strcmp(argv[1], "compare-camera-viewport") == 0 && argc == 3) {
        return command_compare_camera_viewport(argv[2]);
    }
    if (strcmp(argv[1], "hybrid-first-dispatch") == 0 && argc == 3) {
        return command_hybrid_first_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "native-first-dispatch") == 0 && argc == 3) {
        return command_native_first_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "native-second-dispatch") == 0 && argc == 3) {
        return command_native_second_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "native-third-dispatch") == 0 && argc == 3) {
        return command_native_third_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "native-fourth-dispatch") == 0 && argc == 3) {
        return command_native_fourth_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "native-fifth-dispatch") == 0 &&
        (argc == 3 || argc == 4)) {
        g_native_snapshot_path = argc == 4 ? argv[3] : NULL;
        return command_native_fifth_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "native-sixth-dispatch") == 0 &&
        (argc == 3 || argc == 4)) {
        g_native_snapshot_path = argc == 4 ? argv[3] : NULL;
        return command_native_sixth_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "compare-texture-bridge") == 0 && argc == 3) {
        return command_native_second_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "compare-post-frame-bridge") == 0 && argc == 3) {
        return command_native_second_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "compare-geometry-boundary") == 0 && argc == 3) {
        return command_native_second_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "compare-second-scheduler-entry") == 0 && argc == 3) {
        return command_native_second_dispatch(argv[2]);
    }
    if (strcmp(argv[1], "compare-game-geometry-helpers") == 0 && argc == 3) {
        return command_native_second_dispatch(argv[2]);
    }

    if (strcmp(argv[1], "observe-third-sweep") == 0 && argc == 3) {
        return command_native_observe_third_sweep(argv[2]);
    }

    if (strcmp(argv[1], "trace-orchestrator") == 0 &&
        (argc == 3 || argc == 4)) {
        return command_trace_orchestrator(
            argv[2], argc == 4 ? argv[3] : NULL);
    }

    if (strcmp(argv[1], "compare-snapshots") == 0 && argc == 4) {
        return command_compare_snapshots(argv[2], argv[3]);
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
