#include <errno.h>
#include <math.h>
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
#include "vf2/geo_display.h"
#include "vf2/hash.h"
#include "vf2/hybrid.h"
#include "vf2/hybrid/player.h"
#include "vf2/model2a.h"
#include "vf2/model_decode.h"
#include "vf2/native_differential.h"
#include "vf2/recovered.h"
#include "vf2/status.h"
#include "vf2/texture_link.h"
#include "vf2/texture_sheet.h"
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
        "  %s coverage <rom-directory> <input.vf2snap> <frames> <output.json> [frame:mask,...]\n"
        "  %s geometry-census <rom-directory> <input.vf2snap> <frames>\n"
        "  %s attract-irq-probe <rom-directory> <input.vf2snap> <frames> [inject]\n"
        "  %s boot-trace <rom-directory> <frames> [read-cap] [apply-io-fix]\n"
        "  %s snapshot <rom-directory> <output.vf2snap>\n"
        "  %s resume-trace <rom-directory> <input.vf2snap> [max-steps] [clear-task-index] [fighter-flags-or] [write-address] [write-value] [output.vf2snap] [stop-address] [raise-irq] [enter-vector] [enter-level]\n"
        "  %s native-resume <rom-directory> <input.vf2snap> [max-blocks] [fighter-flags-or] [stop-address] [output.vf2snap]\n"
        "  %s game-info-child <rom-directory> <input.vf2snap> <output.vf2snap> <return-address>\n"
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
        "  %s compare-snapshots <expected.vf2snap> <actual.vf2snap>\n"
        "  %s geo-walk <ram9.bin> <frame-index> <readstart-hex>\n"
        "  %s geo-scene <ram9.bin> <readstarts.txt> <rom-dir> <tex0> <tex1> <pose-frame> <out-dir>\n"
        "  %s geo-fighter <ram9.bin> <readstarts.txt> <rom-dir> <tex0> <tex1> <pose-frame> <akira|lau> <out-dir> [colour-capture-dir] [texparams.txt]\n"
        "  %s geo-stage [--from-rom] <ram9.bin> <readstarts.txt> <rom-dir> <tex0> <tex1> <pose-frame> <out-dir> [colour-capture-dir] [texparams.txt]\n"
        "  %s geo-colour <ram9.bin> <readstarts.txt> <rom-dir> <colour-capture-dir> <pose-frame> <out.png> [texparams.txt]\n"
        "  %s geo-texparams <ram9.bin> <readstarts.txt> <out-texparams.txt>\n"
        "  %s geo-project <ram9.bin> <readstarts.txt> <rom-dir> <pose-frame> <out.png> [W H xoff yoff]\n"
        "  %s export-pieces <rom-dir> <capture.txt> <tex0.bin> <tex1.bin> <out-dir>\n"
        "  %s export-textured <rom-dir> <capture.txt> <tex0.bin> <tex1.bin> <out-dir>\n"
        "  %s texture-link <rom-dir> <object-id>\n"
        "  %s decode-texture-sheet <bank.bin> <w> <h> <out.pgm>\n",
        VF2_VERSION_STRING,
        program, /* attract-irq-probe */
        program, /* boot-trace */
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
        program,
        program,
        program,
        program, /* compare-snapshots */
        program, /* geo-walk */
        program, /* geo-scene */
        program, /* geo-fighter */
        program, /* geo-stage */
        program, /* geo-colour */
        program, /* geo-texparams */
        program, /* geo-project */
        program, /* export-pieces */
        program, /* export-textured */
        program, /* texture-link */
        program  /* decode-texture-sheet */
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
        /* v0755c: per-step hook fire counts. 4 entries in the
         * g_callee_hooks table: 0=0x29598, 1=0x439ac, 2=0x43888,
         * 3=0xcf04. */
        {
            uint64_t hook_counts[4] = {0};
            uint64_t hook_total = 0;
            vf2_hybrid_get_callee_hook_counts(hook_counts, &hook_total);
            printf(
                "  hook_fires: 0x29598=%llu 0x439ac=%llu "
                "0x43888=%llu 0xcf04=%llu total=%llu\n",
                (unsigned long long)hook_counts[0],
                (unsigned long long)hook_counts[1],
                (unsigned long long)hook_counts[2],
                (unsigned long long)hook_counts[3],
                (unsigned long long)hook_total
            );
        }
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

static int command_game_info_child(
    const char *rom_directory,
    const char *input_snapshot_path,
    const char *output_snapshot_path,
    uint32_t return_address
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot snapshot;
    vf2_i960_snapshot output_snapshot;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    vf2_i960_snapshot_init(&snapshot);
    vf2_i960_snapshot_init(&output_snapshot);

    if (return_address != UINT32_C(0x000164b0) &&
        return_address != UINT32_C(0x000164c4)) {
        status = VF2_ERROR_INVALID_ARGUMENT;
    }
    if (status == VF2_OK) {
        status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &machine, image, image_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, input_snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }
    if (status == VF2_OK) {
        status = vf2_hybrid_game_info_child_execute_for_test(
            &machine, &cpu, return_address
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_capture(
            &output_snapshot, &cpu, &machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_write_file(
            &output_snapshot, output_snapshot_path
        );
    }
    if (status == VF2_OK) {
        printf(
            "Game-info child: return=0x%08x output=%s\n",
            (unsigned)return_address, output_snapshot_path
        );
    } else {
        fprintf(
            stderr,
            "Game-info child failed: %s at IP=0x%08x\n",
            vf2_status_string(status), (unsigned)cpu.ip
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
    const char *actual_path,
    int dump_registers,
    int dump_ranges
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
    if (status == VF2_OK && dump_registers && !diff.equal) {
        size_t index = 0u;
        printf("Register differences (expected = first file, actual = second):\n");
        for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
            if (expected.cpu.registers[index] != actual.cpu.registers[index]) {
                printf(
                    "  %s%-2u expected=0x%08x actual=0x%08x\n",
                    index < (size_t)VF2_I960_G0_REGISTER ? "r" : "g",
                    index < (size_t)VF2_I960_G0_REGISTER
                        ? (unsigned)index
                        : (unsigned)(index - (size_t)VF2_I960_G0_REGISTER),
                    (unsigned)expected.cpu.registers[index],
                    (unsigned)actual.cpu.registers[index]
                );
            }
        }
        printf(
            "  depth expected=%u actual=%u\n",
            (unsigned)expected.cpu.local_frame_depth,
            (unsigned)actual.cpu.local_frame_depth
        );
        printf(
            "  ip expected=0x%08x actual=0x%08x\n",
            (unsigned)expected.cpu.ip,
            (unsigned)actual.cpu.ip
        );
        printf(
            "  ac expected=0x%08x actual=0x%08x  cc expected=%u actual=%u\n",
            (unsigned)expected.cpu.arithmetic_control,
            (unsigned)actual.cpu.arithmetic_control,
            (unsigned)expected.cpu.compare_result,
            (unsigned)actual.cpu.compare_result
        );
    }
    if (status == VF2_OK && dump_ranges) {
        size_t run_count = 0u;
        size_t differing_bytes = 0u;
        status = vf2_i960_snapshot_diff_runs(
            &expected, &actual, NULL, 0u, &run_count, &differing_bytes
        );
        if (status != VF2_OK) {
            fprintf(stderr, "Difference enumeration failed: %s\n", vf2_status_string(status));
        } else if (differing_bytes == 0u) {
            printf("No memory-region differences.\n");
        } else {
            vf2_i960_snapshot_run *runs = (vf2_i960_snapshot_run *)calloc(
                run_count, sizeof(*runs)
            );
            if (runs == NULL) {
                status = VF2_ERROR_OUT_OF_MEMORY;
                fprintf(stderr, "Difference enumeration failed: %s\n", vf2_status_string(status));
            } else {
                size_t stored = 0u;
                size_t total_bytes = 0u;
                size_t run_index = 0u;
                (void)vf2_i960_snapshot_diff_runs(
                    &expected, &actual, runs, run_count, &stored, &total_bytes
                );
                printf(
                    "Memory difference runs: %zu runs, %zu bytes\n",
                    stored, total_bytes
                );
                for (run_index = 0u; run_index < stored; ++run_index) {
                    size_t expected_size = 0u;
                    const uint8_t *expected_data = vf2_i960_snapshot_region_data(
                        &expected, runs[run_index].name, &expected_size
                    );
                    const uint8_t *actual_data = vf2_i960_snapshot_region_data(
                        &actual, runs[run_index].name, NULL
                    );
                    printf(
                        "  run %zu region=%s offset=0x%06zx length=%zu\n",
                        run_index, runs[run_index].name,
                        runs[run_index].offset, runs[run_index].length
                    );
                    if (expected_data == NULL || actual_data == NULL) continue;
                    {
                        size_t word = runs[run_index].offset & ~(size_t)1u;
                        const size_t last = runs[run_index].offset + runs[run_index].length;
                        while (word < last && word + 1u < expected_size) {
                            const uint16_t expected_value = (uint16_t)(
                                (uint16_t)expected_data[word] |
                                ((uint16_t)expected_data[word + 1u] << 8)
                            );
                            const uint16_t actual_value = (uint16_t)(
                                (uint16_t)actual_data[word] |
                                ((uint16_t)actual_data[word + 1u] << 8)
                            );
                            if (expected_value != actual_value) {
                                printf(
                                    "    0x%06zx expected=0x%04x actual=0x%04x\n",
                                    word, (unsigned)expected_value, (unsigned)actual_value
                                );
                            }
                            word += 2u;
                        }
                    }
                }
                free(runs);
            }
        }
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
                "expected=0x%08x actual=0x%08x bytes=%zu "
                "ref_cc=%u nat_cc=%u ref_ac=%08x nat_ac=%08x "
                "ref_ip=%08x nat_ip=%08x ref_depth=%u nat_depth=%u\n",
                vf2_hybrid_task_kind_name(task_report->kind), diff.component,
                diff.first_offset, (unsigned)diff.expected_value,
                (unsigned)diff.actual_value, diff.differing_bytes,
                (unsigned)original_cpu.compare_result,
                (unsigned)native_cpu.compare_result,
                (unsigned)original_cpu.arithmetic_control,
                (unsigned)native_cpu.arithmetic_control,
                (unsigned)original_cpu.ip,
                (unsigned)native_cpu.ip,
                (unsigned)original_cpu.local_frame_depth,
                (unsigned)native_cpu.local_frame_depth
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
            (bridge_steps != UINT64_C(1270820) ||
             bridge_recovered_instructions != UINT64_C(1270820) ||
             bridge_interpreted_instructions != UINT64_C(0) ||
             bridge_validated_blocks != 190u ||
             bridge_memory_checkpoints != 189u ||
             bridge_recovered_calls != UINT64_C(342) ||
             bridge_recovered_returns != UINT64_C(340) ||
             bridge_block_counts[VF2_HYBRID_BRIDGE_RETURN_STUB] != 1u ||
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
             bridge_block_counts[VF2_HYBRID_BRIDGE_INTERRUPT_BUFFER_GATE] != 0u ||
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
            ? 870u
            : (native_fifth_dispatch
                ? 833u
                : (native_fourth_dispatch ? 80u : 43u));
        const uint64_t expected_instructions = native_sixth_dispatch
            ? UINT64_C(7404901)
            : (native_fifth_dispatch
                ? UINT64_C(7402732)
                : (native_fourth_dispatch
                    ? UINT64_C(58863)
                    : UINT64_C(55236)));
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

/* Phase 3 coverage: run the i960 interpreter from a start snapshot for a
 * number of frames (frame interrupt driven by the same host-side scheduler the
 * recovered runtime and vf2_host --oracle use) and record, per executed
 * program word, how many instructions started there.  The output is compact
 * JSON (address -> hit count) that tools/python/coverage_report.py maps onto
 * decomp/i960/functions.csv.  Only counts leave the tool; no ROM, snapshot or
 * trace is written to Git. */
/* An optional input script: comma-separated frame:mask pairs (mask is a
 * VF2_PLATFORM_BUTTON_* bit set, hex or decimal).  The mask in effect changes
 * when a frame index is reached, so "5:0x100,10:0,60:0x80,65:0" inserts a coin
 * at frame 5, releases it, then presses start at frame 60.  Up to 64 points. */
typedef struct {
    uint32_t frame[64];
    uint32_t mask[64];
    size_t count;
} coverage_input_script;

static int parse_input_script(const char *text, coverage_input_script *script)
{
    script->count = 0u;
    if (text == NULL || text[0] == '\0') return 1;
    while (*text != '\0' && script->count < 64u) {
        char *end = NULL;
        unsigned long f = strtoul(text, &end, 0);
        if (end == text || *end != ':') return 0;
        text = end + 1;
        unsigned long m = strtoul(text, &end, 0);
        if (end == text) return 0;
        script->frame[script->count] = (uint32_t)f;
        script->mask[script->count] = (uint32_t)m;
        ++script->count;
        text = end;
        if (*text == ',') ++text;
        else if (*text != '\0') return 0;
    }
    return 1;
}

/* Phase 6 step 1: census of the geometry command stream the i960 submits.
 *
 * A memory observer classifies every 32-bit write into the geometry ring
 * (0x00800000), the coprocessor port (0x00880000) and the geometry buffer RAM
 * (0x00900000) by command class = (word >> 23) & 0x1f, the Model 2 header
 * convention. For each class it tracks the write count, how many DISTINCT full
 * words were seen (a handful of samples kept), and how many writes matched a
 * known copro protocol immediate -- the v0375-v0378 recovery notes established
 * that many words whose class bits alias a geometry opcode are actually copro
 * arithmetic/protocol tags, not TGP stream opcodes, so the census reports the
 * split rather than overclaiming. Distinct counts separate a single repeated
 * immediate (distinct ~ 1) from a varied opcode such as object submission
 * (distinct large). ROM is never emitted: output is class tallies and a few
 * sample header words, which are safe to inspect. */
enum { GEO_CENSUS_SAMPLE_CAP = 32u };

typedef struct geo_census {
    uint64_t class_count[32];
    uint64_t class_protocol[32];
    uint32_t sample[32][GEO_CENSUS_SAMPLE_CAP];
    uint32_t sample_n[32];      /* distinct words recorded, capped at cap */
    int sample_overflow[32];    /* true once distinct exceeded the cap */
    uint64_t geo_writes;
    uint64_t copro_writes;
    uint64_t buffer_writes;
    uint64_t total;
} geo_census;

static int geo_census_is_protocol(uint32_t word)
{
    /* Known copro protocol immediates (KNOWN_PROTOCOL_WORDS in
     * tools/python/extract_fifo_transforms.py, from the measured parks). */
    static const uint32_t known[] = {
        0x00800101u, 0x01800303u, 0x03000606u, 0x1a003434u,
        0x14802929u, 0x1c803939u, 0x09801313u, 0xffffffffu,
        /* copro arithmetic tags measured on attract (matrix_ports_p2_v0377,
         * tgp_camera_state_v0376): class bits alias geometry opcodes but these
         * drive the copro FPU, not the TGP stream. */
        0x12002424u, 0x0b001616u
    };
    size_t i;
    for (i = 0u; i < sizeof(known) / sizeof(known[0]); ++i) {
        if (word == known[i]) return 1;
    }
    return 0;
}

static void geo_census_record(geo_census *c, uint32_t word)
{
    const uint32_t cls = (word >> 23) & 0x1fu;
    uint32_t i;
    ++c->class_count[cls];
    ++c->total;
    if (geo_census_is_protocol(word)) ++c->class_protocol[cls];
    for (i = 0u; i < c->sample_n[cls]; ++i) {
        if (c->sample[cls][i] == word) return; /* already recorded */
    }
    if (c->sample_n[cls] < GEO_CENSUS_SAMPLE_CAP) {
        c->sample[cls][c->sample_n[cls]++] = word;
    } else {
        c->sample_overflow[cls] = 1;
    }
}

static void geo_census_observer(
    const vf2_model2a_memory_access *access, void *context
)
{
    geo_census *c = (geo_census *)context;
    uint32_t word = 0u;
    uint32_t addr = 0u;
    int in_range = 0;

    if (access == NULL || c == NULL ||
        access->kind != VF2_MODEL2A_MEMORY_WRITE || access->size != 4u ||
        access->data == NULL) {
        return;
    }
    addr = access->address;
    if (addr >= 0x00800000u && addr < 0x00808000u) { ++c->geo_writes; in_range = 1; }
    else if (addr >= 0x00880000u && addr < 0x00888000u) { ++c->copro_writes; in_range = 1; }
    else if (addr >= 0x00900000u && addr < 0x00980000u) { ++c->buffer_writes; in_range = 1; }
    if (!in_range) return;
    memcpy(&word, access->data, sizeof(word));
    geo_census_record(c, word);
}

static int command_geometry_census(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t frames
)
{
    const uint64_t per_frame_budget = UINT64_C(50000000);
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot snapshot;
    vf2_hybrid_frame_wait_state frame_wait;
    geo_census census;
    uint32_t frame = 0u;
    uint32_t cls = 0u;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&census, 0, sizeof(census));
    vf2_i960_snapshot_init(&snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(rom_directory, &machine, image, image_size);
    }
    if (status == VF2_OK) status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    if (status == VF2_OK) status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    if (status == VF2_OK) status = vf2_hybrid_frame_wait_initialize(&frame_wait, 4u);
    if (status == VF2_OK) vf2_model2a_set_memory_observer(&machine, geo_census_observer, &census);

    while (status == VF2_OK && frame < frames) {
        uint64_t steps = 0u;
        for (;;) {
            vf2_hybrid_frame_wait_report wait_report;
            status = vf2_hybrid_frame_wait_observe(&machine, &cpu, &frame_wait, &wait_report);
            if (status != VF2_OK) break;
            if (wait_report.interrupt_injected) { ++frame; break; }
            status = vf2_i960_step(&cpu, &machine, NULL);
            if (status != VF2_OK) break;
            if (++steps >= per_frame_budget) { status = VF2_ERROR_UNSUPPORTED; break; }
        }
    }

    if (status == VF2_OK) {
        printf("geometry-census: %u frames, geo=%llu copro=%llu buffer=%llu "
               "writes (%llu classified)\n",
               (unsigned)frame, (unsigned long long)census.geo_writes,
               (unsigned long long)census.copro_writes,
               (unsigned long long)census.buffer_writes,
               (unsigned long long)census.total);
        printf("  class  count      distinct  protocol  samples\n");
        for (cls = 0u; cls < 32u; ++cls) {
            uint32_t i;
            if (census.class_count[cls] == 0u) continue;
            printf("  0x%02x   %-9llu  %s%-7u  %-8llu ", (unsigned)cls,
                   (unsigned long long)census.class_count[cls],
                   census.sample_overflow[cls] ? ">=" : "  ",
                   (unsigned)census.sample_n[cls],
                   (unsigned long long)census.class_protocol[cls]);
            for (i = 0u; i < census.sample_n[cls] && i < 4u; ++i) {
                printf("%08x ", (unsigned)census.sample[cls][i]);
            }
            printf("\n");
        }
    } else {
        fprintf(stderr, "geometry-census failed: %s\n", vf2_status_string(status));
    }

    vf2_i960_snapshot_destroy(&snapshot);
    vf2_model2a_shutdown(&machine);
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* ---- attract-irq-probe ---------------------------------------------------
 * MAME-directed experiment (decomp/i960/notes/attract_interrupt_injection_vs_mame.md).
 * On a live Model 2 (MAME, romset vf2) the i960 takes, besides vblank (vector
 * 12), two further external interrupts ~every 2.4 attract frames: vector 14
 * (a timer whose handler touches 0x50008c) and vector 15 (the sound-ring
 * drain).  Our native/oracle loop injects ONLY vector 12.  The open question
 * (relayed from the maintainer): do 0x500704 bit 2/26 and 0x5000a6 ever move,
 * and would injecting vectors 14/15 at that cadence let the attract phase
 * machine advance past the TEST/geometry gate it is stuck on?
 *
 * Part A (always): resolve the PRCB interrupt table and print the handler
 *   address for vectors 12..15, disassembling 14/15 so the gate memory they
 *   touch is visible straight from this snapshot.
 * Part B (inject != 0): run `frames` attract frames; on ~every 2.4th frame
 *   raise+enter vector 14 (timer3 request bit 5 -> matches the 0x50008c
 *   evidence) at the first frame-wait visit and enter vector 15 at the second,
 *   before vblank fires at the fourth.  Record the gate words per frame and
 *   report every change, so a baseline (inject 0) and an injected run are
 *   directly comparable.  This is scaffolding (HLE-TEMP): it never ships. */

#define VF2_IRQ_PROBE_GATE_FLAGS UINT32_C(0x00500704) /* bit 2 and bit 26 */
#define VF2_IRQ_PROBE_GATE_SEL   UINT32_C(0x0050002a) /* == 17 to proceed */
#define VF2_IRQ_PROBE_GATE_A6    UINT32_C(0x005000a6) /* == 0 to proceed */
#define VF2_IRQ_PROBE_WAIT_FLAG  UINT32_C(0x0050008c) /* vector-14 target */
#define VF2_IRQ_PROBE_TIMER3_MASK UINT32_C(0x00000020)

typedef struct {
    uint32_t flags;   /* 0x500704 */
    uint8_t sel;      /* 0x50002a */
    uint8_t a6;       /* 0x5000a6 */
    uint8_t wait;     /* 0x50008c */
} irq_probe_gate;

static vf2_status irq_probe_read_gate(vf2_model2a *machine, irq_probe_gate *g)
{
    vf2_status status = vf2_model2a_read_u32(
        machine, VF2_IRQ_PROBE_GATE_FLAGS, &g->flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_IRQ_PROBE_GATE_SEL, &g->sel, sizeof(g->sel));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_IRQ_PROBE_GATE_A6, &g->a6, sizeof(g->a6));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, VF2_IRQ_PROBE_WAIT_FLAG, &g->wait, sizeof(g->wait));
    }
    return status;
}

static void irq_probe_print_gate(const char *tag, uint32_t frame,
                                 const irq_probe_gate *g)
{
    printf("  %-7s f%-4u  0x500704=%08x [b2=%u b26=%u]  "
           "0x50002a=%02x  0x5000a6=%02x  0x50008c=%02x\n",
           tag, (unsigned)frame, (unsigned)g->flags,
           (unsigned)((g->flags >> 2) & 1u), (unsigned)((g->flags >> 26) & 1u),
           (unsigned)g->sel, (unsigned)g->a6, (unsigned)g->wait);
}

static int irq_probe_gate_eq(const irq_probe_gate *a, const irq_probe_gate *b)
{
    return a->flags == b->flags && a->sel == b->sel &&
           a->a6 == b->a6 && a->wait == b->wait;
}

static void irq_probe_dump_handler(const uint8_t *image, size_t image_size,
                                   uint32_t vector, uint32_t handler,
                                   uint32_t count)
{
    uint32_t address = handler;
    uint32_t i;
    printf("  vector %u -> handler 0x%08x%s\n", (unsigned)vector,
           (unsigned)handler,
           handler == 0u ? " (none)" :
           (handler >= image_size ? " (outside maincpu image)" : ""));
    if (handler == 0u || handler >= image_size) {
        return;
    }
    for (i = 0u; i < count; ++i) {
        vf2_i960_instruction instruction;
        char text[256];
        if (vf2_i960_decode(image, image_size, address, &instruction) != VF2_OK) {
            break;
        }
        if (vf2_i960_format_instruction(&instruction, text, sizeof(text)) != VF2_OK) {
            break;
        }
        printf("    %08x  %08x  %s\n", (unsigned)address,
               (unsigned)instruction.words[0], text);
        address += instruction.size;
    }
}

static vf2_status irq_probe_inject_vector(vf2_model2a *machine,
                                          vf2_i960_cpu *cpu,
                                          uint32_t vector, uint32_t raise_mask)
{
    vf2_status status = VF2_OK;
    if (raise_mask != 0u) {
        status = vf2_model2a_raise_interrupt(machine, raise_mask);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_interrupt(cpu, machine, vector, 1u);
    }
    return status;
}

static int command_attract_irq_probe(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t frames,
    uint32_t inject
)
{
    const uint64_t per_frame_budget = UINT64_C(50000000);
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot snapshot;
    vf2_hybrid_frame_wait_state frame_wait;
    uint32_t interrupt_table = 0u;
    uint32_t frame = 0u;
    uint32_t v = 0u;
    uint32_t accum = 0u; /* cadence: +10 per frame, inject when >= 24 (2.4) */
    uint32_t changes = 0u;
    uint32_t injected_frames = 0u;
    int cadence_frame = 0;
    irq_probe_gate prev;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&prev, 0, sizeof(prev));
    vf2_i960_snapshot_init(&snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(rom_directory, &machine, image, image_size);
    }
    if (status == VF2_OK) status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    if (status == VF2_OK) status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    if (status == VF2_OK) status = vf2_hybrid_frame_wait_initialize(&frame_wait, 4u);
    if (status != VF2_OK) {
        fprintf(stderr, "attract-irq-probe setup failed: %s\n",
                vf2_status_string(status));
        goto done;
    }

    /* Part A: resolve and show the interrupt handlers for vectors 12..15. */
    status = vf2_model2a_read_u32(&machine, cpu.prcb + 20u, &interrupt_table);
    if (status != VF2_OK) { goto done; }
    printf("attract-irq-probe: prcb=0x%08x interrupt_table=0x%08x\n",
           (unsigned)cpu.prcb, (unsigned)interrupt_table);
    for (v = 12u; v <= 15u; ++v) {
        uint32_t handler = 0u;
        status = vf2_model2a_read_u32(
            &machine, interrupt_table + 36u + (v - 8u) * 4u, &handler);
        if (status != VF2_OK) { goto done; }
        irq_probe_dump_handler(image, image_size, v, handler,
                               (v == 14u || v == 15u) ? 20u : 1u);
    }

    /* Part B: run attract frames, recording the gate words; optionally inject
     * vectors 14/15 at the ~2.4-frame cadence. */
    printf("\nrun: %u frames, inject=%u (%s)\n", (unsigned)frames,
           (unsigned)inject,
           inject ? "v12 + v14/v15 at ~1/2.4 frames" : "v12 only (baseline)");
    status = irq_probe_read_gate(&machine, &prev);
    if (status != VF2_OK) { goto done; }
    irq_probe_print_gate("start", 0u, &prev);

    while (status == VF2_OK && frame < frames) {
        uint64_t steps = 0u;
        int this_frame_injected = 0;
        accum += 10u;
        cadence_frame = inject && (accum >= 24u);
        if (cadence_frame) { accum -= 24u; }
        for (;;) {
            vf2_hybrid_frame_wait_report wait_report;
            status = vf2_hybrid_frame_wait_observe(
                &machine, &cpu, &frame_wait, &wait_report);
            if (status != VF2_OK) break;
            if (wait_report.interrupt_injected) { ++frame; break; }
            if (cadence_frame && wait_report.wait_observed) {
                if (wait_report.visit_count == 1u) {
                    status = irq_probe_inject_vector(
                        &machine, &cpu, 14u, VF2_IRQ_PROBE_TIMER3_MASK);
                    this_frame_injected = 1;
                    if (status != VF2_OK) break;
                    continue; /* re-observe: cpu now in handler, not at spin */
                }
                if (wait_report.visit_count == 2u) {
                    status = irq_probe_inject_vector(&machine, &cpu, 15u, 0u);
                    if (status != VF2_OK) break;
                    continue;
                }
            }
            status = vf2_i960_step(&cpu, &machine, NULL);
            if (status != VF2_OK) break;
            if (++steps >= per_frame_budget) { status = VF2_ERROR_UNSUPPORTED; break; }
        }
        if (status != VF2_OK) break;
        if (this_frame_injected) { ++injected_frames; }
        {
            irq_probe_gate now;
            status = irq_probe_read_gate(&machine, &now);
            if (status != VF2_OK) break;
            if (!irq_probe_gate_eq(&prev, &now)) {
                irq_probe_print_gate(this_frame_injected ? "chg*" : "chg",
                                     frame, &now);
                ++changes;
                prev = now;
            }
        }
    }

    if (status == VF2_OK) {
        irq_probe_gate now;
        status = irq_probe_read_gate(&machine, &now);
        if (status == VF2_OK) {
            irq_probe_print_gate("end", frame, &now);
            printf("\nsummary: %u frames, %u injected frames, %u gate changes; "
                   "gate proceeds when 0x500704 bit2&26 set AND 0x50002a==17 "
                   "AND 0x5000a6==0 -> %s\n",
                   (unsigned)frame, (unsigned)injected_frames, (unsigned)changes,
                   (((now.flags >> 2) & 1u) && ((now.flags >> 26) & 1u) &&
                    now.sel == 17u && now.a6 == 0u) ? "OPEN" : "still closed");
        }
    }

done:
    if (status != VF2_OK) {
        fprintf(stderr, "attract-irq-probe failed: %s\n",
                vf2_status_string(status));
    }
    vf2_i960_snapshot_destroy(&snapshot);
    vf2_model2a_shutdown(&machine);
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* ---- boot-trace ----------------------------------------------------------
 * Boot the pure i960 interpreter from reset (no snapshot, no recovered HLE of
 * the phase logic) with vblank injection, and trace the writes/reads that the
 * MAME reference pins the attract divergence to: the phase byte 0x50002a
 * (MAME: 0x00->0x01 @f84 ip 0xa970, 0x02 @f723, 0x03 @f724, never 0x11), the
 * TGP result port 0x884000 (the 0xa520-0xa6ac routine reads 64 words @~f23)
 * and the TGP start/ready handshake at 0x980000/0x980004 (@~f10). Our TGP
 * model does not produce real result words, so the real ROM routine takes a
 * different branch here -- this command shows exactly what the native reads and
 * where 0x50002a goes, to locate the branch that drives it to 0x11.
 * Diagnostic only; prints to stdout; commits nothing. */

typedef struct {
    const vf2_i960_cpu *cpu;
    const uint32_t *frame;
    uint32_t read_884_logged;
    uint32_t read_884_cap;
    uint32_t total_884;
    uint32_t read_980004_logged;
    /* 3D-scene indicators: writes into the geometry program region (0x804000+,
     * TGP model records) and the copro command FIFO (0x884000+). */
    uint32_t geo_prg_writes;
    uint32_t fifo_writes;
    uint32_t first_geo_frame;   /* frame of first prg write, or 0xffffffff */
    int bit13_seen;             /* tracks 0x508000 bit13 transitions */
    int bit13_last;
} boot_trace_ctx;

static uint32_t boot_trace_le32(const void *p)
{
    uint32_t v = 0u;
    memcpy(&v, p, sizeof(v));
    return v;
}

static void boot_trace_observer(
    const vf2_model2a_memory_access *a, void *vctx
)
{
    boot_trace_ctx *c = (boot_trace_ctx *)vctx;
    uint32_t ip = 0u;
    uint32_t f = 0u;
    if (a == NULL || a->data == NULL || c == NULL) {
        return;
    }
    ip = c->cpu != NULL ? c->cpu->ip : 0u;
    f = c->frame != NULL ? *c->frame : 0u;
    if (a->kind == VF2_MODEL2A_MEMORY_WRITE) {
        if (a->address == 0x0050002au) {
            printf("  f%-4u ip=%08x  W 0x50002a <- 0x%02x\n",
                   (unsigned)f, (unsigned)ip, (unsigned)((const uint8_t *)a->data)[0]);
        } else if (a->address == 0x00508000u && a->size == 4u) {
            const uint32_t v = boot_trace_le32(a->data);
            const int b13 = (int)((v >> 13) & 1u);
            if (!c->bit13_seen || b13 != c->bit13_last) {
                printf("  f%-4u ip=%08x  W [508000] <- 0x%08x  (bit13=%d)\n",
                       (unsigned)f, (unsigned)ip, (unsigned)v, b13);
                c->bit13_seen = 1;
                c->bit13_last = b13;
            }
        } else if (a->address >= 0x00500704u && a->address < 0x00500708u) {
            printf("  f%-4u ip=%08x  W [%08x] <- (sz%u) 0x%08x  (0x500704 gate word)",
                   (unsigned)f, (unsigned)ip, (unsigned)a->address,
                   (unsigned)a->size,
                   a->size == 4u ? (unsigned)boot_trace_le32(a->data)
                                 : (unsigned)((const uint8_t *)a->data)[0]);
            if (c->cpu != NULL) {
                printf("  [r6=%08x r7=%08x r11=%08x]",
                       (unsigned)c->cpu->registers[6], (unsigned)c->cpu->registers[7],
                       (unsigned)c->cpu->registers[11]);
            }
            printf("\n");
        } else if (a->address >= 0x005000a6u && a->address < 0x005000a8u) {
            printf("  f%-4u ip=%08x  W [%08x] <- (sz%u) 0x%02x  (0x5000a6)\n",
                   (unsigned)f, (unsigned)ip, (unsigned)a->address,
                   (unsigned)a->size, (unsigned)((const uint8_t *)a->data)[0]);
        } else if (a->address >= 0x00804000u && a->address < 0x00808000u) {
            if (c->geo_prg_writes == 0u) {
                printf("  f%-4u ip=%08x  W [%08x] <- 0x%08x  (FIRST geometry "
                       "program write -> 3D upload)\n",
                       (unsigned)f, (unsigned)ip, (unsigned)a->address,
                       a->size == 4u ? (unsigned)boot_trace_le32(a->data)
                                     : (unsigned)((const uint8_t *)a->data)[0]);
                c->first_geo_frame = f;
            }
            ++c->geo_prg_writes;
        } else if (a->address >= 0x00884000u && a->address < 0x00888000u) {
            ++c->fifo_writes;
        } else if (a->address == 0x00980000u && a->size == 4u) {
            printf("  f%-4u ip=%08x  W [980000] <- 0x%08x  (TGP start)\n",
                   (unsigned)f, (unsigned)ip, (unsigned)boot_trace_le32(a->data));
        } else if (a->address == 0x00980004u && a->size == 4u) {
            printf("  f%-4u ip=%08x  W [980004] <- 0x%08x\n",
                   (unsigned)f, (unsigned)ip, (unsigned)boot_trace_le32(a->data));
        }
    } else { /* READ */
        if (a->address >= 0x00884000u && a->address < 0x00884100u &&
            a->size == 4u) {
            ++c->total_884;
            if (c->read_884_logged < c->read_884_cap) {
                printf("  f%-4u ip=%08x  R [%08x] = 0x%08x\n",
                       (unsigned)f, (unsigned)ip, (unsigned)a->address,
                       (unsigned)boot_trace_le32(a->data));
                ++c->read_884_logged;
            }
        } else if (a->address == 0x00980004u && a->size == 4u) {
            if (c->read_980004_logged < 16u) {
                printf("  f%-4u ip=%08x  R [980004] = 0x%08x  (TGP ready)\n",
                       (unsigned)f, (unsigned)ip, (unsigned)boot_trace_le32(a->data));
                ++c->read_980004_logged;
            }
        }
    }
}

/* Read N 32-bit little-endian words from the maincpu image (for inspecting
 * jump tables and other data the disassembler cannot decode). */
static int command_read_words(
    const char *rom_directory, uint32_t address, uint32_t count
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    uint32_t i = 0u;
    vf2_status status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status != VF2_OK) {
        fprintf(stderr, "Could not load maincpu: %s\n", vf2_status_string(status));
        return EXIT_FAILURE;
    }
    for (i = 0u; i < count; ++i) {
        const uint32_t a = address + i * 4u;
        uint32_t w = 0u;
        if ((size_t)a + 4u > image_size) {
            printf("  %08x  <out of image>\n", (unsigned)a);
            continue;
        }
        w = (uint32_t)image[a] | ((uint32_t)image[a + 1u] << 8) |
            ((uint32_t)image[a + 2u] << 16) | ((uint32_t)image[a + 3u] << 24);
        printf("  %08x  %08x\n", (unsigned)a, (unsigned)w);
    }
    free(image);
    return EXIT_SUCCESS;
}

/* --- minimal grayscale-PNG writer (dependency-free) --------------------------
 * 8-bit grayscale (color type 0), zlib "stored" (uncompressed) deflate so no
 * compressor is needed. CRC via vf2_crc32; Adler-32 inline. Local output only. */
static void png_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t png_adler32(const uint8_t *d, size_t n)
{
    uint32_t a = 1u;
    uint32_t b = 0u;
    size_t i;
    for (i = 0u; i < n; ++i) {
        a = (a + d[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static int png_chunk(FILE *f, const char *type, const uint8_t *data, size_t len)
{
    uint8_t hdr[8];
    uint8_t crcbuf[4];
    uint8_t *tmp = (uint8_t *)malloc(len + 4u);
    uint32_t crc;
    if (tmp == NULL) {
        return 0;
    }
    png_be32(hdr, (uint32_t)len);
    memcpy(hdr + 4u, type, 4u);
    memcpy(tmp, type, 4u);
    if (len != 0u) {
        memcpy(tmp + 4u, data, len);
    }
    crc = vf2_crc32(tmp, len + 4u);
    free(tmp);
    png_be32(crcbuf, crc);
    if (fwrite(hdr, 1u, 8u, f) != 8u) {
        return 0;
    }
    if (len != 0u && fwrite(data, 1u, len, f) != len) {
        return 0;
    }
    return fwrite(crcbuf, 1u, 4u, f) == 4u;
}

static int png_write_gray8(
    const char *path, uint32_t w, uint32_t h, const uint8_t *px
)
{
    static const uint8_t sig[8] = { 0x89u, 0x50u, 0x4eu, 0x47u,
                                    0x0du, 0x0au, 0x1au, 0x0au };
    uint8_t ihdr[13];
    size_t raw_len = (size_t)h * (1u + (size_t)w);
    uint8_t *raw = (uint8_t *)malloc(raw_len != 0u ? raw_len : 1u);
    uint8_t *idat = NULL;
    size_t cap;
    size_t o = 0u;
    size_t pos = 0u;
    uint32_t row;
    FILE *f = NULL;
    int ok = 0;

    if (raw == NULL) {
        return 0;
    }
    for (row = 0u; row < h; ++row) {
        raw[(size_t)row * (1u + (size_t)w)] = 0u; /* filter: none */
        memcpy(raw + (size_t)row * (1u + (size_t)w) + 1u,
               px + (size_t)row * (size_t)w, w);
    }
    cap = 2u + raw_len + ((raw_len / 65535u) + 1u) * 5u + 4u;
    idat = (uint8_t *)malloc(cap);
    if (idat == NULL) {
        free(raw);
        return 0;
    }
    idat[o++] = 0x78u; /* zlib CMF */
    idat[o++] = 0x01u; /* zlib FLG */
    while (pos < raw_len) {
        size_t blk = raw_len - pos;
        uint16_t nlen;
        if (blk > 65535u) {
            blk = 65535u;
        }
        idat[o++] = (uint8_t)((pos + blk >= raw_len) ? 1u : 0u); /* BFINAL */
        idat[o++] = (uint8_t)(blk & 0xffu);
        idat[o++] = (uint8_t)((blk >> 8) & 0xffu);
        nlen = (uint16_t)(~(uint16_t)blk);
        idat[o++] = (uint8_t)(nlen & 0xffu);
        idat[o++] = (uint8_t)((nlen >> 8) & 0xffu);
        memcpy(idat + o, raw + pos, blk);
        o += blk;
        pos += blk;
    }
    png_be32(idat + o, png_adler32(raw, raw_len));
    o += 4u;

    f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        free(idat);
        return 0;
    }
    png_be32(ihdr, w);
    png_be32(ihdr + 4u, h);
    ihdr[8] = 8u;  /* bit depth */
    ihdr[9] = 0u;  /* color type: grayscale */
    ihdr[10] = 0u; /* compression */
    ihdr[11] = 0u; /* filter */
    ihdr[12] = 0u; /* interlace */
    ok = (fwrite(sig, 1u, 8u, f) == 8u) &&
         png_chunk(f, "IHDR", ihdr, sizeof(ihdr)) &&
         png_chunk(f, "IDAT", idat, o) && png_chunk(f, "IEND", NULL, 0u);
    fclose(f);
    free(raw);
    free(idat);
    return ok;
}

/* Same minimal PNG, 8-bit RGB (color type 2). `px` is w*h*3 bytes, row-major. */
static int png_write_rgb8(
    const char *path, uint32_t w, uint32_t h, const uint8_t *px
)
{
    static const uint8_t sig[8] = { 0x89u, 0x50u, 0x4eu, 0x47u,
                                    0x0du, 0x0au, 0x1au, 0x0au };
    uint8_t ihdr[13];
    size_t stride = (size_t)w * 3u;
    size_t raw_len = (size_t)h * (1u + stride);
    uint8_t *raw = (uint8_t *)malloc(raw_len != 0u ? raw_len : 1u);
    uint8_t *idat = NULL;
    size_t cap;
    size_t o = 0u;
    size_t pos = 0u;
    uint32_t row;
    FILE *f = NULL;
    int ok = 0;

    if (raw == NULL) {
        return 0;
    }
    for (row = 0u; row < h; ++row) {
        raw[(size_t)row * (1u + stride)] = 0u; /* filter: none */
        memcpy(raw + (size_t)row * (1u + stride) + 1u,
               px + (size_t)row * stride, stride);
    }
    cap = 2u + raw_len + ((raw_len / 65535u) + 1u) * 5u + 4u;
    idat = (uint8_t *)malloc(cap);
    if (idat == NULL) {
        free(raw);
        return 0;
    }
    idat[o++] = 0x78u;
    idat[o++] = 0x01u;
    while (pos < raw_len) {
        size_t blk = raw_len - pos;
        uint16_t nlen;
        if (blk > 65535u) {
            blk = 65535u;
        }
        idat[o++] = (uint8_t)((pos + blk >= raw_len) ? 1u : 0u);
        idat[o++] = (uint8_t)(blk & 0xffu);
        idat[o++] = (uint8_t)((blk >> 8) & 0xffu);
        nlen = (uint16_t)(~(uint16_t)blk);
        idat[o++] = (uint8_t)(nlen & 0xffu);
        idat[o++] = (uint8_t)((nlen >> 8) & 0xffu);
        memcpy(idat + o, raw + pos, blk);
        o += blk;
        pos += blk;
    }
    png_be32(idat + o, png_adler32(raw, raw_len));
    o += 4u;

    f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        free(idat);
        return 0;
    }
    png_be32(ihdr, w);
    png_be32(ihdr + 4u, h);
    ihdr[8] = 8u;  /* bit depth */
    ihdr[9] = 2u;  /* color type: truecolor RGB */
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = 0u;
    ok = (fwrite(sig, 1u, 8u, f) == 8u) &&
         png_chunk(f, "IHDR", ihdr, sizeof(ihdr)) &&
         png_chunk(f, "IDAT", idat, o) && png_chunk(f, "IEND", NULL, 0u);
    fclose(f);
    free(raw);
    free(idat);
    return ok;
}

/* Write an 8-bit RGBA PNG from a separate RGB plane (w*h*3) and alpha plane
 * (w*h). Used for the coloured fighter atlas so a translucent polygon's cutout
 * texels (texel 0x0f) bake transparent -- an OBJ viewer then shows the surface
 * behind (e.g. skin through the eye-detail decal), as the scene render does by
 * skipping those texels. Same stored-deflate container as png_write_rgb8. */
static int png_write_rgba8(
    const char *path, uint32_t w, uint32_t h,
    const uint8_t *rgb, const uint8_t *alpha
)
{
    static const uint8_t sig[8] = { 0x89u, 0x50u, 0x4eu, 0x47u,
                                    0x0du, 0x0au, 0x1au, 0x0au };
    uint8_t ihdr[13];
    size_t stride = (size_t)w * 4u;
    size_t raw_len = (size_t)h * (1u + stride);
    uint8_t *raw = (uint8_t *)malloc(raw_len != 0u ? raw_len : 1u);
    uint8_t *idat = NULL;
    size_t cap;
    size_t o = 0u;
    size_t pos = 0u;
    uint32_t row;
    uint32_t col;
    FILE *f = NULL;
    int ok = 0;

    if (raw == NULL) {
        return 0;
    }
    for (row = 0u; row < h; ++row) {
        uint8_t *d = raw + (size_t)row * (1u + stride);
        *d++ = 0u; /* filter: none */
        for (col = 0u; col < w; ++col) {
            size_t p = (size_t)row * (size_t)w + col;
            d[0] = rgb[p * 3u + 0u];
            d[1] = rgb[p * 3u + 1u];
            d[2] = rgb[p * 3u + 2u];
            d[3] = alpha[p];
            d += 4;
        }
    }
    cap = 2u + raw_len + ((raw_len / 65535u) + 1u) * 5u + 4u;
    idat = (uint8_t *)malloc(cap);
    if (idat == NULL) {
        free(raw);
        return 0;
    }
    idat[o++] = 0x78u;
    idat[o++] = 0x01u;
    while (pos < raw_len) {
        size_t blk = raw_len - pos;
        uint16_t nlen;
        if (blk > 65535u) {
            blk = 65535u;
        }
        idat[o++] = (uint8_t)((pos + blk >= raw_len) ? 1u : 0u);
        idat[o++] = (uint8_t)(blk & 0xffu);
        idat[o++] = (uint8_t)((blk >> 8) & 0xffu);
        nlen = (uint16_t)(~(uint16_t)blk);
        idat[o++] = (uint8_t)(nlen & 0xffu);
        idat[o++] = (uint8_t)((nlen >> 8) & 0xffu);
        memcpy(idat + o, raw + pos, blk);
        o += blk;
        pos += blk;
    }
    png_be32(idat + o, png_adler32(raw, raw_len));
    o += 4u;

    f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        free(idat);
        return 0;
    }
    png_be32(ihdr, w);
    png_be32(ihdr + 4u, h);
    ihdr[8] = 8u;  /* bit depth */
    ihdr[9] = 6u;  /* color type: truecolor + alpha (RGBA) */
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = 0u;
    ok = (fwrite(sig, 1u, 8u, f) == 8u) &&
         png_chunk(f, "IHDR", ihdr, sizeof(ihdr)) &&
         png_chunk(f, "IDAT", idat, o) && png_chunk(f, "IEND", NULL, 0u);
    fclose(f);
    free(raw);
    free(idat);
    return ok;
}

/* --- texture-link: dump an object's decoded polygon->texture link ------------
 *
 * Reads the object-table entry {w0=tpa, w1=tha, w2=oba, w3=obc} at 0x020e0004,
 * walks the polygon record's primitives, and for each one decodes the texture
 * header (sheet rectangle + flags) and the per-vertex UVs from the texture
 * region, advancing the UV and header pointers per the geometrizer rules (see
 * include/vf2/texture_link.h). ROM-only, local; not exercised by CI (the decode
 * itself is covered synthetically by tests/platform/test_texture_link.c). */
static uint32_t tl_u32(const uint8_t *b, size_t n, size_t off)
{
    if (off + 4u > n) {
        return 0u;
    }
    return (uint32_t)b[off] | ((uint32_t)b[off + 1u] << 8) |
           ((uint32_t)b[off + 2u] << 16) | ((uint32_t)b[off + 3u] << 24);
}

static uint16_t tl_u16(const uint8_t *b, size_t n, uint32_t index)
{
    size_t off = (size_t)index * 2u;
    if (off + 2u > n) {
        return 0u;
    }
    return (uint16_t)((uint16_t)b[off] | ((uint16_t)b[off + 1u] << 8));
}

static int command_texture_link(const char *rom_directory, const char *id_arg)
{
    uint8_t *main_data = NULL;
    uint8_t *poly = NULL;
    uint8_t *tex = NULL;
    size_t main_size = 0u;
    size_t poly_size = 0u;
    size_t tex_size = 0u;
    uint32_t object_id = 0u;
    uint32_t w0;
    uint32_t w1;
    uint32_t w2;
    uint32_t w3;
    size_t table_entry;
    uint32_t rec_byte;
    size_t off;
    uint32_t uv_ptr;
    uint32_t th_ptr;
    uint32_t prim;
    vf2_status st;

    if (!parse_u32(id_arg, &object_id)) {
        fprintf(stderr, "texture-link: bad object id\n");
        return EXIT_FAILURE;
    }
    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data,
                                 &main_size);
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly,
                                     &poly_size);
    }
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_TEXTURES, &tex,
                                     &tex_size);
    }
    if (st != VF2_OK) {
        fprintf(stderr, "Could not build ROM regions: %s\n",
                vf2_status_string(st));
        free(main_data);
        free(poly);
        free(tex);
        return EXIT_FAILURE;
    }

    table_entry = 0x000e0004u + (size_t)object_id * 16u;
    w0 = tl_u32(main_data, main_size, table_entry + 0u);
    w1 = tl_u32(main_data, main_size, table_entry + 4u);
    w2 = tl_u32(main_data, main_size, table_entry + 8u);
    w3 = tl_u32(main_data, main_size, table_entry + 12u);
    rec_byte = (w2 & 0x7fffffu) * 4u;
    uv_ptr = w0 & 0x7fffffu;  /* texture-point (UV) table, u16 index */
    th_ptr = w1 & 0x7fffffu;  /* texture-header table, u16 index */

    printf("object 0x%x: w0/tpa=%08x w1/tha=%08x w2/oba=%08x w3/obc=%08x\n",
           (unsigned)object_id, (unsigned)w0, (unsigned)w1, (unsigned)w2,
           (unsigned)w3);
    printf("  record @ poly byte 0x%x; primitives:\n", (unsigned)rec_byte);

    off = (size_t)rec_byte + 24u; /* skip the initial p0,p1 edge (6 floats) */
    for (prim = 0u; prim < 4096u; ++prim) {
        uint32_t attr = tl_u32(poly, poly_size, off);
        uint32_t numv;
        uint16_t thw[4];
        vf2_texture_header h;
        uint32_t v;

        off += 4u;
        if (attr == 0x00040401u || (attr & 3u) == 0u) {
            break;
        }
        numv = (attr & 1u) ? 4u : 3u;

        thw[0] = tl_u16(tex, tex_size, th_ptr + 0u);
        thw[1] = tl_u16(tex, tex_size, th_ptr + 1u);
        thw[2] = tl_u16(tex, tex_size, th_ptr + 2u);
        thw[3] = tl_u16(tex, tex_size, th_ptr + 3u);
        vf2_texture_header_decode(thw, &h);

        printf("  prim %3u attr=%08x nv=%u  sheet%u x%u y%u %ux%u "
               "wrap%u%u mir%u%u rend%u col%u  UV:",
               (unsigned)prim, (unsigned)attr, (unsigned)numv, (unsigned)h.sheet,
               (unsigned)h.tex_x, (unsigned)h.tex_y, (unsigned)h.tex_width,
               (unsigned)h.tex_height, (unsigned)h.wrap_x, (unsigned)h.wrap_y,
               (unsigned)h.mirror_x, (unsigned)h.mirror_y, (unsigned)h.renderer,
               (unsigned)h.color_base);
        for (v = 0u; v < numv; ++v) {
            uint16_t pv = tl_u16(tex, tex_size, uv_ptr + v * 2u + 0u);
            uint16_t pu = tl_u16(tex, tex_size, uv_ptr + v * 2u + 1u);
            printf(" (%.1f,%.1f)", (double)vf2_texture_uv_texels(pu),
                   (double)vf2_texture_uv_texels(pv));
        }
        printf("\n");

        uv_ptr += numv * 2u;
        th_ptr = (uint32_t)((int32_t)th_ptr +
                            vf2_texture_header_advance_words(attr));
        off += 36u; /* normal(3) + vA(3) + vB(3) */
    }

    free(main_data);
    free(poly);
    free(tex);
    return EXIT_SUCCESS;
}

/* --- export-textured: player-1 textured mesh (OBJ + MTL + PNG) ---------------
 *
 * Decodes the player-1 pieces from a fight capture into a Wavefront OBJ with
 * per-vertex UVs, an MTL with two grayscale sheet PNGs (de-swizzled logical
 * 2048x1024), using the recovered polygon->texture link (texture_link.h). The
 * UV<->vertex pairing mirrors the geometrizer: per primitive the UV table
 * yields NumVerts (pv,pu) pairs in the order [P1(n-1),P0(n-1),P0(n),P1(n)] =
 * [p1,p0,vA,vB]; the strip advances p0/p1 by (attr>>8)&3 exactly as the
 * hardware (case1: p1=P0(n); case3: p0=P1(n)). Output is local, never committed;
 * the decode itself is covered synthetically in CI (test_texture_link.c). */

typedef struct ext_tex_piece {
    uint32_t rec_byte;
    uint32_t uv_ptr;   /* w0 & 0x7fffff */
    uint32_t th_ptr;   /* w1 & 0x7fffff */
    uint32_t object_id;
} ext_tex_piece;

static int ext_tex_load_file(const char *path, uint8_t **buf, size_t *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (f == NULL) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    *buf = (uint8_t *)malloc((size_t)n != 0u ? (size_t)n : 1u);
    if (*buf == NULL) {
        fclose(f);
        return 0;
    }
    *size = fread(*buf, 1u, (size_t)n, f);
    fclose(f);
    return *size == (size_t)n;
}

/* --- shared Model2 colour pipeline (used by geo-colour and the coloured-texture
 * export) -- texel -> luma tone curve -> shade -> colorxlat per channel -> gamma -- */

static uint16_t gc_u16(const uint8_t *b, size_t n, size_t i)
{
    size_t o = i * 2u;
    return (o + 1u < n) ? (uint16_t)(b[o] | (b[o + 1u] << 8)) : 0u;
}

/* VF2's fixed monitor colour-space conversion (model2_v.cpp:2390-2394): a hard-
 * coded bias-64 / gain-(255/191) ramp applied to every channel after the colorxlat
 * lookup. It is a constant, not programmed RAM, so the native port reproduces it
 * directly (no HLE). Shadows below 64 crush to 0; the rest stretches to full range. */
static uint8_t gc_gamma(uint32_t v)
{
    if (v <= 64u) return 0u;
    v = (v - 64u) * 255u / 191u;
    return (uint8_t)(v > 255u ? 255u : v);
}

/* One texel's final RGB. 4-bit texel -> 8-bit -> polygon tone curve in luma RAM ->
 * scale by the lighting term and clamp to 6-bit shade -> colorxlat per channel (R/G/B
 * regions at word 0x0000/0x2000/0x4000) -> VF2 gamma (model2rd.ipp:336-349). r5/g5/b5
 * are the polygon colour's 5-bit channels (from palram[colorbase+0x1000]&0x7fff).
 * lumascale is the polygon lighting (0xff = full / unlit albedo). `translucent` is the
 * polygon's transparent flag (texheader[0] bit 13); texel 0x0f is the cutout ONLY for
 * a translucent polygon (MAME selects draw_scanline_tex<Translucent>, model2_v.cpp:568
 * + model2rd.ipp). For an opaque polygon 0x0f is simply the brightest intensity --
 * treating it as transparent turned flat 0x0f-filled opaque regions (Lau's gi, a lot
 * of skin) fully black. Returns 0 and writes nothing only for a translucent cutout. */
static int gc_texel_rgb(uint8_t t4, uint32_t lumabase, int r5, int g5, int b5,
                        uint32_t lumascale, int translucent, const uint8_t *luma,
                        const uint8_t *cxlat, size_t cxlat_n, uint8_t out[3])
{
    uint32_t t8, curve, shade;
    if (translucent && t4 == 0x0fu) return 0;   /* translucent cutout only */
    t8 = (uint32_t)t4 << 4;
    curve = luma[(lumabase + (t8 >> 1)) & 0x7fffu];
    shade = (curve * lumascale) / 256u;
    if (shade > 0x3fu) shade = 0x3fu;
    out[0] = gc_gamma(gc_u16(cxlat, cxlat_n, 0x0000u + (size_t)r5 * 0x100u + shade) & 0xffu);
    out[1] = gc_gamma(gc_u16(cxlat, cxlat_n, 0x2000u + (size_t)g5 * 0x100u + shade) & 0xffu);
    out[2] = gc_gamma(gc_u16(cxlat, cxlat_n, 0x4000u + (size_t)b5 * 0x100u + shade) & 0xffu);
    return 1;
}

/* Read one little-endian float from the polygon buffer. */
static float gc_rdf(const uint8_t *p, size_t n, size_t o)
{
    uint32_t u = tl_u32(p, n, o);
    float f;
    memcpy(&f, &u, 4u);
    return f;
}

/* Per-polygon lighting term (0..255), exactly as MAME geo_parse_np_s/_nn_s
 * (model2_v.cpp:1076-1097 / 1223-1253): nt = normal x matrix(3x3); dotl = nt.light;
 * dotp = nt.(matrix x firstVertex) gives front/back; lum = (dotl*dotp<0 ? 0 : |dotl|)
 * * diffuse + ambient + specular (specular only in a specular parse mode). `nv`/`va`
 * are the already-read normal and first new vertex in the polygon's local space; the
 * point transform is in float to match ext_xform. This is the shade source the pixel
 * stage needs -- NOT full light -- so dark materials (hair, shoes) come out dark. */
static uint32_t gc_poly_luma(const float nv[3], const float va[3], const float *mat,
    const float *light, uint32_t tpi,
    const uint8_t *tp_diffuse, const uint8_t *tp_ambient,
    const uint8_t *tp_sscale, const uint8_t *tp_sctrl, uint32_t geo_mode, int twosided,
    int no_spec)
{
    double nt[3], dotl, dotp, lumv, spec = 0.0;
    float ptx, pty, ptz;
    nt[0] = (double)nv[0] * mat[0] + (double)nv[1] * mat[3] + (double)nv[2] * mat[6];
    nt[1] = (double)nv[0] * mat[1] + (double)nv[1] * mat[4] + (double)nv[2] * mat[7];
    nt[2] = (double)nv[0] * mat[2] + (double)nv[1] * mat[5] + (double)nv[2] * mat[8];
    dotl = nt[0] * light[0] + nt[1] * light[1] + nt[2] * light[2];
    ptx = va[0] * mat[0] + va[1] * mat[3] + va[2] * mat[6] + mat[9];
    pty = va[0] * mat[1] + va[1] * mat[4] + va[2] * mat[7] + mat[10];
    ptz = va[0] * mat[2] + va[1] * mat[5] + va[2] * mat[8] + mat[11];
    dotp = nt[0] * (double)ptx + nt[1] * (double)pty + nt[2] * (double)ptz;
    /* MAME shadows a front face whose normal points away from the light
     * (dotl*dotp<0 -> 0). A mirrored translucent decal (the two halves of Lau's
     * front ornament share one record, mirrored) then has one half lit and the
     * other shadowed. In-game the decal is drawn translucent OVER the lit base,
     * so the shadowed half still reads lit; a baked opaque atlas would show it
     * near-black. For the static OBJ, light such a decal two-sided (|dotl|) so
     * the mirror halves match. `twosided` is passed only for the atlas bake of a
     * translucent polygon; the scene render stays MAME-exact. */
    lumv = (twosided || dotl * dotp >= 0.0) ? fabs(dotl) : 0.0;
    /* `no_spec`: the atlas bake drops the specular term for a strongly-specular
     * (chrome) material -- a view-dependent highlight cannot be baked on a frame
     * and then viewed from another angle, so the atlas carries only the matte
     * diffuse+ambient base and the MTL marks the material metallic for the viewer
     * to shade live. Materials without specular (tp_sctrl==0, i.e. every one the
     * other ten fighters use) are unaffected, so their atlases stay byte-identical. */
    if (!no_spec &&
        ((geo_mode & 3u) == 1u || (geo_mode & 3u) == 3u) && tp_sctrl[tpi] != 0u) {
        uint32_t sc = tp_sctrl[tpi];
        spec = (2.0 * dotl) * nt[2] - (double)light[2];
        if (spec < 0.0) spec = 0.0;
        if ((sc >> 1) != 0u) spec *= spec;
        if ((sc >> 2) != 0u) spec *= spec;
        if (((sc + 1u) >> 3) != 0u) spec *= spec;
        spec *= (double)tp_sscale[tpi];
    }
    lumv = lumv * (double)tp_diffuse[tpi] + (double)tp_ambient[tpi] + spec;
    if (lumv < 0.0) lumv = 0.0;
    if (lumv > 255.0) lumv = 255.0;
    return (uint32_t)lumv;
}

/* Load a geo-texparams file into the per-index diffuse/ambient/specular tables
 * (leaves the caller's defaults for any index the file omits; the two specular
 * columns are optional for forward compatibility with older files). */
static void gc_load_texparams(const char *path, uint8_t *tp_diffuse, uint8_t *tp_ambient,
                              uint8_t *tp_sscale, uint8_t *tp_sctrl)
{
    FILE *f = fopen(path, "rb");
    char line[256];
    if (f == NULL) { fprintf(stderr, "geo: cannot read texparams %s (using defaults)\n", path); return; }
    while (fgets(line, (int)sizeof(line), f) != NULL) {
        unsigned idx, d, a, ss, sc;
        int nf;
        if (line[0] == '#') continue;
        nf = sscanf(line, "%u %u %u %u %u", &idx, &d, &a, &ss, &sc);
        if (nf >= 3 && idx < 32u) {
            tp_diffuse[idx] = (uint8_t)d; tp_ambient[idx] = (uint8_t)a;
            if (nf >= 5) { tp_sscale[idx] = (uint8_t)ss; tp_sctrl[idx] = (uint8_t)sc; }
        }
    }
    fclose(f);
}

static int ext_tex_write_sheet_png(
    const char *path, const uint8_t *bank, size_t bank_size
)
{
    const uint32_t w = 2048u;
    const uint32_t h = 1024u;
    uint8_t *px = (uint8_t *)malloc((size_t)w * h);
    uint32_t y;
    int ok;
    if (px == NULL) {
        return 0;
    }
    for (y = 0u; y < h; ++y) {
        uint32_t x;
        for (x = 0u; x < w; ++x) {
            uint8_t idx = vf2_texture_sheet_texel(bank, bank_size, x, y);
            px[(size_t)y * w + x] = vf2_texture_sheet_index_to_gray(idx);
        }
    }
    ok = png_write_gray8(path, w, h, px);
    free(px);
    return ok;
}

/* Paint one piece's polygons into the per-bank coloured sheets (2048x1024 RGB each):
 * for every polygon, decode its texture header (rectangle + colour_base + luma_base +
 * sheet + renderer) and colour it through the Model2 pixel pipeline using a
 * REPRESENTATIVE per-polygon luma -- not full light -- so dark materials (hair,
 * ponytail, shoes) come out dark as in-game. When `vmat` != NULL the luma is the real
 * per-polygon lighting at this frame (gc_poly_luma: normal x matrix, dotl, diffuse/
 * ambient/specular from the 0x06 table); when NULL it falls back to full light
 * (0xff, albedo). Textured polygons (renderer bit1) colour each texel of their
 * rectangle; untextured (solid) polygons fill the rectangle with one colorxlat colour
 * at shade = luma>>2 (draw_scanline_solid) -- and a translucent solid renders nothing.
 * Mirrors ext_emit_piece's 10-word block walk; reads the normal + first vertex for the
 * lighting. Overlaps (a rectangle reused with a different material) are last-write-wins;
 * transparent texels (0x0f) on a translucent polygon are left as-is. */
static void ext_colour_sheet_piece(
    const uint8_t *poly, size_t poly_size,
    const uint8_t *tex, size_t tex_size,
    uint32_t rec_byte, uint32_t th_ptr,
    const uint8_t *const bank[2], const size_t bank_size[2],
    const uint8_t *pal, size_t pal_n,
    const uint8_t *cxlat, size_t cxlat_n, const uint8_t *luma,
    uint8_t *const csheet[2], uint8_t *const asheet[2],
    const float *vmat, const float *light,
    const uint8_t *tp_diffuse, const uint8_t *tp_ambient,
    const uint8_t *tp_sscale, const uint8_t *tp_sctrl, uint32_t geo_mode
)
{
    size_t off = (size_t)rec_byte + 24u;
    uint32_t prim;
    for (prim = 0u; prim < 65536u; ++prim) {
        uint32_t attr = tl_u32(poly, poly_size, (uint32_t)off);
        uint16_t thw[4];
        vf2_texture_header hd;
        uint32_t palc, px, py, lumascale;
        int r5, g5, b5, translucent, untextured;
        float nv[3], va[3];
        const uint8_t *bk;
        size_t bks;
        uint8_t *cs;
        uint8_t *as;
        off += 4u;
        if ((attr & 3u) == 0u) break;
        /* 10-word block: [attr][normal(3)][P0(n)(3)][P1(n)(3)] -- read normal + P0(n) */
        nv[0] = gc_rdf(poly, poly_size, off + 0u); nv[1] = gc_rdf(poly, poly_size, off + 4u); nv[2] = gc_rdf(poly, poly_size, off + 8u);
        va[0] = gc_rdf(poly, poly_size, off + 12u); va[1] = gc_rdf(poly, poly_size, off + 16u); va[2] = gc_rdf(poly, poly_size, off + 20u);
        off += 36u;
        thw[0] = tl_u16(tex, tex_size, th_ptr + 0u); thw[1] = tl_u16(tex, tex_size, th_ptr + 1u);
        thw[2] = tl_u16(tex, tex_size, th_ptr + 2u); thw[3] = tl_u16(tex, tex_size, th_ptr + 3u);
        vf2_texture_header_decode(thw, &hd);
        palc = gc_u16(pal, pal_n, (size_t)((hd.color_base + 0x1000u) & 0x1fffu)) & 0x7fffu;
        r5 = (int)(palc & 0x1fu); g5 = (int)((palc >> 5) & 0x1fu); b5 = (int)((palc >> 10) & 0x1fu);
        translucent = (int)(hd.renderer & 1u);   /* texheader[0] bit 13 */
        untextured = ((hd.renderer & 2u) == 0u);  /* texheader[0] bit 14 clear */
        lumascale = (vmat != NULL)
            ? gc_poly_luma(nv, va, vmat, light, (attr >> 18) & 0x1fu,
                           tp_diffuse, tp_ambient, tp_sscale, tp_sctrl, geo_mode,
                           translucent, 1 /* no_spec: never bake specular */)
            : 0xffu;
        bk = bank[hd.sheet & 1u]; bks = bank_size[hd.sheet & 1u];
        cs = csheet[hd.sheet & 1u]; as = asheet[hd.sheet & 1u];
        if (untextured && translucent) goto sheet_advance;   /* solid translucent: nothing */
        if (untextured) {
            uint32_t sh = lumascale >> 2; uint8_t sr, sg, sb;
            if (sh > 0x3fu) sh = 0x3fu;
            sr = gc_gamma(gc_u16(cxlat, cxlat_n, 0x0000u + (size_t)r5 * 0x100u + sh) & 0xffu);
            sg = gc_gamma(gc_u16(cxlat, cxlat_n, 0x2000u + (size_t)g5 * 0x100u + sh) & 0xffu);
            sb = gc_gamma(gc_u16(cxlat, cxlat_n, 0x4000u + (size_t)b5 * 0x100u + sh) & 0xffu);
            for (py = 0u; py < hd.tex_height; ++py) {
                uint32_t sy = hd.tex_y + py;
                if (sy >= 1024u) break;
                for (px = 0u; px < hd.tex_width; ++px) {
                    uint32_t sx = hd.tex_x + px;
                    size_t o;
                    if (sx >= 2048u) break;
                    o = (size_t)sy * 2048u + sx;
                    cs[o * 3u] = sr; cs[o * 3u + 1u] = sg; cs[o * 3u + 2u] = sb;
                    as[o] = 0xffu;   /* solid: opaque */
                }
            }
            goto sheet_advance;
        }
        for (py = 0u; py < hd.tex_height; ++py) {
            uint32_t sy = hd.tex_y + py;
            if (sy >= 1024u) break;
            for (px = 0u; px < hd.tex_width; ++px) {
                uint32_t sx = hd.tex_x + px;
                uint8_t t4, rgb[3];
                size_t o;
                if (sx >= 2048u) break;
                o = (size_t)sy * 2048u + sx;
                t4 = vf2_texture_sheet_texel(bk, bks, sx, sy);
                if (gc_texel_rgb(t4, hd.luma_base, r5, g5, b5, lumascale, translucent,
                                 luma, cxlat, cxlat_n, rgb)) {
                    cs[o * 3u] = rgb[0]; cs[o * 3u + 1u] = rgb[1]; cs[o * 3u + 2u] = rgb[2];
                    as[o] = 0xffu;   /* texel kept: opaque */
                } else {
                    as[o] = 0u;      /* translucent cutout (texel 0x0f): transparent */
                }
            }
        }
    sheet_advance:
        th_ptr = (uint32_t)((int32_t)th_ptr + vf2_texture_header_advance_words(attr));
    }
}

static int ext_read_xyz(
    const uint8_t *poly, size_t n, uint32_t byte, vf2_model_vertex *v
)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    if ((size_t)byte + 12u > n) {
        return 0;
    }
    a = tl_u32(poly, n, byte);
    b = tl_u32(poly, n, byte + 4u);
    c = tl_u32(poly, n, byte + 8u);
    memcpy(&v->x, &a, 4u);
    memcpy(&v->y, &b, 4u);
    memcpy(&v->z, &c, 4u);
    return 1;
}

/* Apply a geometriser transform matrix (MAME model2_v.cpp transform_point:
 * 3x3 rotation in m[0..8], translation in m[9..11]) to a model vertex. A NULL
 * matrix is the identity, so untransformed callers emit object-local space. */
static void ext_xform(const float *m, const vf2_model_vertex *in,
                      vf2_model_vertex *out)
{
    if (m == NULL) {
        *out = *in;
        return;
    }
    out->x = in->x * m[0] + in->y * m[3] + in->z * m[6] + m[9];
    out->y = in->x * m[1] + in->y * m[4] + in->z * m[7] + m[10];
    out->z = in->x * m[2] + in->y * m[5] + in->z * m[8] + m[11];
}

/* Emit one polygon (3 or 4 vertices + matching UVs, in lockstep so each corner's
 * v and vt index match) as a single OBJ face. `mtx` (NULL = identity) poses each
 * vertex into world/view space. The polygon is emitted as an n-gon (not
 * pre-triangulated) exactly as MAME scan-converts it (render_polygon<4>), so a
 * viewer triangulates a non-convex/non-planar quad correctly; pre-splitting on a
 * fixed diagonal put the split outside such a quad and made an inward spike
 * (Lau's fist tops, Akira's foot tops). `onrm`/`eps`: when non-NULL and non-zero,
 * shift the whole face by `eps` world units along its (object-space) normal
 * rotated into world space -- used to lift a translucent decal (dragon, ornament)
 * off the coplanar opaque base so a z-buffered viewer draws it on top, as MAME
 * renders translucent over opaque. When `onrm` is non-NULL the face winding is
 * also oriented so its geometric normal agrees with `onrm` (the model's own
 * winding is not consistently outward). Returns the next running index. */
static size_t ext_emit_poly(
    FILE *obj, size_t n,
    const vf2_model_vertex *pv[4], const float *puv[4], uint32_t numv,
    const float *mtx, const float *onrm, float eps,
    vf2_model_vertex *vacc, size_t *vacc_n, size_t vacc_cap
)
{
    vf2_model_vertex tv[4];
    uint32_t ord[4] = {0u, 1u, 2u, 3u};
    uint32_t i;
    float wn[3] = {0.0f, 0.0f, 0.0f}, wlen = 0.0f;
    if (numv < 3u || numv > 4u) return n;
    for (i = 0u; i < numv; ++i) ext_xform(mtx, pv[i], &tv[i]);
    if (onrm != NULL) {
        if (mtx != NULL) {
            wn[0] = onrm[0]*mtx[0] + onrm[1]*mtx[3] + onrm[2]*mtx[6];
            wn[1] = onrm[0]*mtx[1] + onrm[1]*mtx[4] + onrm[2]*mtx[7];
            wn[2] = onrm[0]*mtx[2] + onrm[1]*mtx[5] + onrm[2]*mtx[8];
        } else { wn[0] = onrm[0]; wn[1] = onrm[1]; wn[2] = onrm[2]; }
        wlen = (float)sqrt((double)(wn[0]*wn[0] + wn[1]*wn[1] + wn[2]*wn[2]));
        if (wlen > 1e-6f && eps != 0.0f) {
            float s = eps / wlen;
            for (i = 0u; i < numv; ++i) { tv[i].x += wn[0]*s; tv[i].y += wn[1]*s; tv[i].z += wn[2]*s; }
        }
        if (wlen > 1e-6f) {
            /* orient winding so the face normal agrees with the model normal;
             * reverse the corner order (0, n-1, .., 1) if it is flipped. */
            float e1x = tv[1].x-tv[0].x, e1y = tv[1].y-tv[0].y, e1z = tv[1].z-tv[0].z;
            float e2x = tv[2].x-tv[0].x, e2y = tv[2].y-tv[0].y, e2z = tv[2].z-tv[0].z;
            float gx = e1y*e2z - e1z*e2y, gy = e1z*e2x - e1x*e2z, gz = e1x*e2y - e1y*e2x;
            if (gx*wn[0] + gy*wn[1] + gz*wn[2] < 0.0f) {
                if (numv == 4u) { ord[1] = 3u; ord[3] = 1u; }
                else { ord[1] = 2u; ord[2] = 1u; }
            }
        }
    }
    if (obj != NULL) {
        for (i = 0u; i < numv; ++i)
            fprintf(obj, "v %.6f %.6f %.6f\n",
                    (double)tv[ord[i]].x, (double)tv[ord[i]].y, (double)tv[ord[i]].z);
        for (i = 0u; i < numv; ++i)
            fprintf(obj, "vt %.6f %.6f\n", (double)puv[ord[i]][0], (double)puv[ord[i]][1]);
        fprintf(obj, "f");
        for (i = 0u; i < numv; ++i)
            fprintf(obj, " %zu/%zu", n + i, n + i);
        fprintf(obj, "\n");
    }
    if (vacc != NULL && vacc_n != NULL && *vacc_n + numv <= vacc_cap)
        for (i = 0u; i < numv; ++i) vacc[(*vacc_n)++] = tv[i];
    return n + numv;
}

/* Decode one model record (a quad strip) into textured triangles and write
 * them to `obj`. Shared by export-textured (one combined mesh) and
 * export-pieces (one file per piece). The decode is the geometrizer-exact
 * polygon->texture link validated in texture_polygon_link_v0831.md: initial
 * edge p0/p1, then 10-word blocks [attr][normal][vA][vB], UVs in table order
 * [P1(n-1),P0(n-1),P0(n),P1(n)], texheader advance (attr>>12)&0x1f*4, strip
 * advance (attr>>8)&3. `cur_sheet` carries the last-emitted material across
 * calls (-1 forces a fresh `usemtl`). Returns the next 1-based vertex index. */
/* A texture tile that a repeat-UV polygon references; emitted as a standalone
 * repeat-wrapped image rather than baked into the atlas (see the tiled-poly path). */
typedef struct gf_tile { uint16_t sheet, x, y, w, h; } gf_tile;

/* Up (+Y): the orientation a floor tile's winding is flipped to when its model
 * normal points down. */
static const float ext_up_normal[3] = { 0.0f, 1.0f, 0.0f };

/* Link-type-0 accounting (a strip-restart polygon MAME culls, see the cull at the
 * emit guard). Reset by the caller before a model's export, read after, to confirm
 * every path (including the repeat-tile path) drops link-0 polys. emitted must be 0. */
static unsigned long ext_link0_seen = 0ul;
static unsigned long ext_link0_emitted = 0ul;
static unsigned long ext_mirror_polys = 0ul;   /* stage polys whose texheader mirror bit flipped the UVs */

static size_t ext_emit_piece(
    FILE *obj,
    const uint8_t *poly, size_t poly_size,
    const uint8_t *tex, size_t tex_size,
    uint32_t rec_byte, uint32_t uv_ptr, uint32_t th_ptr,
    size_t vbase, int *cur_sheet, size_t *tri_count,
    const float *mtx,
    vf2_model_vertex *vacc, size_t *vacc_n, size_t vacc_cap,
    const uint8_t *tp_sctrl, uint32_t geo_mode, uint32_t *used_spec,
    gf_tile *tiles, size_t *ntiles, size_t tiles_cap
)
{
    vf2_model_vertex p0;
    vf2_model_vertex p1;
    size_t off;
    uint32_t prim;
    static int dbg_init = 0;
    static uint32_t dbg_rec = 0xffffffffu;
    int do_dbg;
    /* Distinct translucent-decal rectangles, in first-seen (draw) order, so a
     * later decal layered over an earlier one (e.g. Wolf's eye iris over the
     * war-paint) is lifted further out and wins the z-test instead of fighting
     * it coplanar. Rank 0 = the first/base translucent layer. */
    uint32_t seen_rx[32];
    uint32_t seen_ry[32];
    int nseen = 0;

    if (!dbg_init) {
        const char *e = getenv("VF2_PIECE_DBG");
        if (e != NULL) dbg_rec = (uint32_t)strtoul(e, NULL, 0);
        dbg_init = 1;
    }
    do_dbg = (rec_byte == dbg_rec && obj != NULL);

    if (!ext_read_xyz(poly, poly_size, rec_byte, &p0) ||
        !ext_read_xyz(poly, poly_size, rec_byte + 12u, &p1)) {
        return vbase;
    }
    off = (size_t)rec_byte + 24u;

    for (prim = 0u; prim < 65536u; ++prim) {
        uint32_t attr = tl_u32(poly, poly_size, (uint32_t)off);
        uint32_t numv;
        vf2_model_vertex vA;
        vf2_model_vertex vB;
        uint16_t thw[4];
        vf2_texture_header hd;
        float uv[4][2];
        float pnrm[3];
        const float *dnrm; float deps;
        uint32_t vtx;
        int poly_tiled = 0;   /* this prim samples a repeated tile -> standalone image */
        int tile_rank = -1;

        off += 4u;
        if (do_dbg)
            fprintf(stderr, "    prim %u @0x%zx attr=0x%08x (quad=%u tri=%u "
                    "strip=%u)\n", prim, off - 4u, attr, (attr & 1u),
                    ((attr & 3u) == 2u), (unsigned)((attr >> 8) & 3u));
        /* MAME geo_parse_np_s/_nn_s end a piece ONLY on attr&3==0 (the link type
         * bits zero). 0x00040401 is a valid quad (attr&3==1) that MAME keeps
         * decoding; stopping on it here truncated pieces mid-mesh (e.g. Akira's
         * trousers 0x13b998, cut at prim 10). */
        if ((attr & 3u) == 0u) {
            if (do_dbg)
                fprintf(stderr, "    STOP at prim %u: attr&3==0\n", prim);
            break;
        }
        numv = (attr & 1u) ? 4u : 3u;
        {   /* unit normal (3 floats), kept to lift translucent decals */
            vf2_model_vertex nvx;
            if (ext_read_xyz(poly, poly_size, (uint32_t)off, &nvx)) {
                pnrm[0] = nvx.x; pnrm[1] = nvx.y; pnrm[2] = nvx.z;
            } else { pnrm[0] = pnrm[1] = pnrm[2] = 0.0f; }
        }
        off += 12u;
        if (!ext_read_xyz(poly, poly_size, (uint32_t)off, &vA)) { /* P0(n) */
            break;
        }
        off += 12u;
        if (numv == 4u) {
            if (!ext_read_xyz(poly, poly_size, (uint32_t)off, &vB)) { /* P1(n) */
                break;
            }
        } else {
            /* triangle: MAME skips this slot and sm2-emu (geometrizer.cpp:557)
             * sets P1(n) = P0(n) -- a degenerate 4th vertex. Reading the skipped
             * slot as a real vertex produced garbage sliver triangles. */
            vB = vA;
        }
        off += 12u; /* advance past the P1(n) slot either way (10-word stride) */

        thw[0] = tl_u16(tex, tex_size, th_ptr + 0u);
        thw[1] = tl_u16(tex, tex_size, th_ptr + 1u);
        thw[2] = tl_u16(tex, tex_size, th_ptr + 2u);
        thw[3] = tl_u16(tex, tex_size, th_ptr + 3u);
        vf2_texture_header_decode(thw, &hd);

        {
            /* Texel UVs are read per vertex, then wrapped into the tile with ONE
             * common offset per axis for the whole polygon. A per-vertex fmod
             * scrambled every polygon that straddles a tile boundary -- one vertex at
             * V = tex_height wrapped to 0 while its neighbours stayed below, breaking
             * the triangle's interpolation (the "mosaic" on Lau's back, record
             * 0x69a890). A common offset (floor(min/size)*size) keeps the triangle
             * contiguous and still brings a polygon that lives in a higher repeat
             * (all vertices past tex_width/height) back onto the tile, exactly as the
             * per-vertex wrap already did for that case. MAME wraps per pixel
             * (model2rd.ipp:156-159), which the scene render reproduces; a baked OBJ
             * cannot, so this is the contiguous equivalent. */
            float tu4[4], tv4[4];
            float wq = (float)hd.tex_width, hq = (float)hd.tex_height;
            float minu = 0.0f, minv = 0.0f, offu = 0.0f, offv = 0.0f;
            for (vtx = 0u; vtx < numv; ++vtx) {
                uint16_t pv = tl_u16(tex, tex_size, uv_ptr + vtx * 2u + 0u);
                uint16_t pu = tl_u16(tex, tex_size, uv_ptr + vtx * 2u + 1u);
                tu4[vtx] = vf2_texture_uv_texels(pu);
                tv4[vtx] = vf2_texture_uv_texels(pv);
                /* Texheader mirror bits (th0 bit 8/9): the hardware samples the tile
                 * flipped on that axis, so a stage sign stored backwards reads correctly
                 * (the arena banners "棋逢对手"/"决斗胜负"). The atlas bakes the texels
                 * un-flipped, so flip the UV within the tile to match. Only applied for
                 * stage decor (tiles != NULL); fighters pass NULL, so their OBJs stay
                 * byte-identical. */
                if (tiles != NULL && (hd.mirror_x || hd.mirror_y)) {
                    if (hd.mirror_x) tu4[vtx] = wq - tu4[vtx];
                    if (hd.mirror_y) tv4[vtx] = hq - tv4[vtx];
                    if (vtx == 0u) ++ext_mirror_polys;
                }
                if (vtx == 0u || tu4[vtx] < minu) minu = tu4[vtx];
                if (vtx == 0u || tv4[vtx] < minv) minv = tv4[vtx];
            }
            if (wq > 0.0f) offu = (float)floor((double)minu / (double)wq) * wq;
            if (hq > 0.0f) offv = (float)floor((double)minv / (double)hq) * hq;
            for (vtx = 0u; vtx < numv; ++vtx) {
                float cu = tu4[vtx] - offu, cv = tv4[vtx] - offv;
                /* Stage decor (tiles != NULL): keep a non-tiled polygon's sample inside
                 * the one baked copy of the tile. offu/offv put the minimum on [0,size),
                 * so only the maximum can reach the far edge (texel `size`, which the bake
                 * -- filling 0..size-1 -- leaves blank): clamp it half a texel short so the
                 * edge vertex reads the last baked texel, not the gap past it. A genuine
                 * repeat (max still beyond the tile here) was already sent to the tile path
                 * above and does not reach this clamp. Fighters pass NULL: unchanged. */
                if (tiles != NULL) {
                    if (wq > 0.0f && cu > wq - 0.5f) cu = wq - 0.5f;
                    if (hq > 0.0f && cv > hq - 0.5f) cv = hq - 0.5f;
                }
                uv[vtx][0] = ((float)hd.tex_x + cu) / 2048.0f;
                uv[vtx][1] = 1.0f - ((float)hd.tex_y + cv) / 1024.0f;
            }
            /* A polygon whose UV span exceeds its tile samples the texture REPEATED
             * (floor paving especially). The atlas holds one copy of the tile, so
             * the wrapped UVs run off it into blank atlas -> a grey floor. Mark it
             * so it is emitted against a standalone repeat-wrapped tile image with
             * UVs in tile units instead (tiled[]), not baked into the atlas.
             * The trigger is that the offset-aligned UV range runs PAST one tile
             * (mxu-offu > wq, mxv-offv > hq): offu/offv bring the minimum onto the tile
             * [0,size), so any polygon whose maximum still exceeds the tile wraps onto a
             * second copy the atlas does not hold -- both the obvious multi-repeat floors
             * and the polygons that merely STRADDLE a tile boundary (span < one tile but
             * sitting across the seam, e.g. recovered arena-pit floor rec59afe0/rec59b2e0
             * whose un-wrapped half baked as blank/black). A span threshold missed the
             * straddlers; this covers them. The criterion is wrapping, not opacity: a
             * polygon is tiled only when it samples past one copy of the texture, so a
             * decal that stays within its texture (the arena sign, whose UVs never exceed
             * its cell) is never tiled regardless of its span, while a translucent
             * surface that genuinely wraps (the pit floor's rend=3 straddlers) is tiled
             * so it bakes instead of running off the atlas. A tiled polygon still gets
             * its translucent decal lift (applied later, independent of the tile path). */
            {
                float mxu = tu4[0], mxv = tv4[0]; uint32_t z;
                for (z = 1u; z < numv; ++z) { if (tu4[z] > mxu) mxu = tu4[z]; if (tv4[z] > mxv) mxv = tv4[z]; }
                poly_tiled = (tiles != NULL && ntiles != NULL &&
                              ((wq > 0.0f && (mxu - offu) > wq + 2.0f) ||
                               (hq > 0.0f && (mxv - offv) > hq + 2.0f)));
                if (poly_tiled) {
                    size_t ti;
                    for (vtx = 0u; vtx < numv; ++vtx) {
                        uv[vtx][0] = (wq > 0.0f) ? tu4[vtx] / wq : 0.0f;
                        uv[vtx][1] = (hq > 0.0f) ? 1.0f - tv4[vtx] / hq : 0.0f;
                    }
                    for (ti = 0u; ti < *ntiles; ++ti)
                        if (tiles[ti].sheet == (uint16_t)hd.sheet &&
                            tiles[ti].x == hd.tex_x && tiles[ti].y == hd.tex_y) break;
                    if (ti == *ntiles && *ntiles < tiles_cap) {
                        tiles[ti].sheet = (uint16_t)hd.sheet;
                        tiles[ti].x = (uint16_t)hd.tex_x; tiles[ti].y = (uint16_t)hd.tex_y;
                        tiles[ti].w = (uint16_t)hd.tex_width; tiles[ti].h = (uint16_t)hd.tex_height;
                        ++*ntiles;
                    }
                    tile_rank = (ti < tiles_cap) ? (int)ti : -1;
                    if (tile_rank < 0) poly_tiled = 0;
                }
            }
        }
        if (numv == 3u) {
            uv[3][0] = uv[2][0];
            uv[3][1] = uv[2][1];
        }

        /* Material = texture bank (sheet0/1), except a strongly-specular (chrome)
         * material -- one whose 0x06 spec control is non-zero, which only Dural
         * uses -- gets its own `sheetN_specM` material so the MTL can mark it
         * metallic (its specular is viewer-shaded, not baked). tp_sctrl==NULL
         * (every non-colour caller) keeps the plain sheetN material, byte for byte. */
        {
            uint32_t tpi = (attr >> 18) & 0x1fu;
            int spec_active = (!poly_tiled && tp_sctrl != NULL &&
                ((geo_mode & 3u) == 1u || (geo_mode & 3u) == 3u) &&
                tp_sctrl[tpi] != 0u);
            int mtl_key = poly_tiled
                ? (int)(0x4000u | (uint32_t)tile_rank)
                : (spec_active
                   ? (int)(0x1000u | ((uint32_t)(hd.sheet & 1u) << 5) | tpi)
                   : (int)(hd.sheet & 1u));
            if (*cur_sheet != mtl_key) {
                if (obj != NULL) {
                    if (poly_tiled)
                        fprintf(obj, "usemtl tile%d\n", tile_rank);
                    else if (spec_active)
                        fprintf(obj, "usemtl sheet%u_spec%u\n",
                                (unsigned)(hd.sheet & 1u), tpi);
                    else
                        fprintf(obj, "usemtl sheet%u\n", (unsigned)(hd.sheet & 1u));
                }
                *cur_sheet = mtl_key;
            }
            if (spec_active && used_spec != NULL)
                used_spec[hd.sheet & 1u] |= (1u << tpi);
        }

        /* Always pass the polygon's unit normal so ext_emit_poly can orient each
         * face's winding to agree with it (the model's winding is not
         * consistently outward -- MAME relies on the explicit normal, so a bare
         * OBJ shows some faces inside-out, e.g. Lau's fist tops, Akira's nose and
         * foot tops). A translucent polygon (renderer bit0) is additionally a
         * decal coplanar with the opaque base, lifted outward (eps) so a
         * z-buffered viewer draws it on top (MAME renders translucent over
         * opaque); 0.003 world units ~ 0.2% of a fighter's height. Decals can
         * themselves stack (a second decal over a first, e.g. Wolf's eye iris
         * over the war-paint): in-game they are painter-ordered, but as coplanar
         * static faces they z-fight and a mirrored pair can resolve differently
         * per side (one eye shows the iris, the mirror eye drops to the war-paint
         * cut-out -> a black eye). Lift each successive translucent layer (a new
         * texheader rectangle, in draw order) a further 0.0015 out so the later
         * decal wins the z-test everywhere; 0.0015 ~ 0.1% of a fighter, far below
         * a visible gap but well clear of z-fighting. */
        dnrm = pnrm;
        /* Floor-up rule (geometry-based). A repeated-tile floor whose OBJ winding comes
         * out downward shows the view background from above in a backface-culling viewer
         * (stage-4 central slab a9bea0; stage-3 floor rec57fd08). VF2 model normals are
         * not consistently outward, so keying off the stored normal (pnrm) missed the
         * stage-3 floor, whose stored normal is near-horizontal/degenerate. Instead key
         * off the polygon's own GEOMETRIC normal: a tiled poly that is geometrically
         * near-horizontal is a floor (or ceiling), so force its orientation to +Y. Walls
         * (near-vertical geometric normal) and fighters (never tiled) are untouched; no
         * stage has a down-facing near-horizontal tiled ceiling, so this only lifts
         * floors. The geometric normal is computed in draw order (p1,p0,vA) to match
         * ext_emit_poly, in object space then rotated to world by mtx's 3x3. */
        if (poly_tiled) {
            float e1[3], e2[3], g[3], gw[3], gl, ay;
            e1[0]=p0.x-p1.x; e1[1]=p0.y-p1.y; e1[2]=p0.z-p1.z;
            e2[0]=vA.x-p1.x; e2[1]=vA.y-p1.y; e2[2]=vA.z-p1.z;
            g[0]=e1[1]*e2[2]-e1[2]*e2[1];
            g[1]=e1[2]*e2[0]-e1[0]*e2[2];
            g[2]=e1[0]*e2[1]-e1[1]*e2[0];
            if (mtx != NULL) {
                gw[0]=g[0]*mtx[0]+g[1]*mtx[3]+g[2]*mtx[6];
                gw[1]=g[0]*mtx[1]+g[1]*mtx[4]+g[2]*mtx[7];
                gw[2]=g[0]*mtx[2]+g[1]*mtx[5]+g[2]*mtx[8];
            } else { gw[0]=g[0]; gw[1]=g[1]; gw[2]=g[2]; }
            gl=(float)sqrt((double)(gw[0]*gw[0]+gw[1]*gw[1]+gw[2]*gw[2]));
            ay = gw[1]<0.0f ? -gw[1] : gw[1];
            if (gl > 1e-6f && ay > 0.9f * gl)
                dnrm = ext_up_normal;
        }
        deps = 0.0f;
        if ((hd.renderer & 1u) != 0u) {
            int layer = nseen;   /* default: new rectangle -> next rank */
            int si;
            for (si = 0; si < nseen; ++si) {
                if (seen_rx[si] == hd.tex_x && seen_ry[si] == hd.tex_y) { layer = si; break; }
            }
            if (layer == nseen && nseen < (int)(sizeof(seen_rx) / sizeof(seen_rx[0]))) {
                seen_rx[nseen] = hd.tex_x; seen_ry[nseen] = hd.tex_y; ++nseen;
            }
            if (layer > 8) layer = 8;   /* bound the cumulative lift */
            deps = -0.003f - 0.0015f * (float)layer;
        }

        /* Link type 0 is a strip restart: MAME culls it (check_culling,
         * model2_v.cpp:254) but still advances the strip (the link update at
         * :529 is outside the cull guard). Its polygon joins the stale previous
         * edge to the new points -- a spike -- and exists only to load the next
         * strip's base, so skip emitting it while still advancing below.
         * Emit the polygon as a single face in MAME's vertex order
         * v0..v3 = P1(n-1), P0(n-1), P0(n), P1(n) = p1, p0, vA, vB, with UVs in
         * the table order [P1(n-1),P0(n-1),P0(n),P1(n)] = uv[0..3]. A triangle
         * (numv==3) drops P1(n). */
        if (((attr >> 8u) & 3u) != 0u) {
            const vf2_model_vertex *pv[4]; const float *puv[4];
            pv[0] = &p1;  puv[0] = uv[0];
            pv[1] = &p0;  puv[1] = uv[1];
            pv[2] = &vA;  puv[2] = uv[2];
            pv[3] = &vB;  puv[3] = uv[3];
            vbase = ext_emit_poly(obj, vbase, pv, puv, numv,
                                  mtx, dnrm, deps, vacc, vacc_n, vacc_cap);
            *tri_count += (numv == 4u) ? 2u : 1u;
        } else {
            ++ext_link0_seen;   /* strip restart: culled here, never emitted */
        }

        uv_ptr += numv * 2u;
        th_ptr = (uint32_t)((int32_t)th_ptr +
                            vf2_texture_header_advance_words(attr));
        switch ((attr >> 8u) & 3u) {
        case 0u:
        case 2u:
            p0 = vA;
            p1 = vB;
            break;
        case 1u:
            p1 = vA;
            break;
        case 3u:
            p0 = vB;
            break;
        default:
            break;
        }
    }
    return vbase;
}

static int command_export_textured(
    const char *rom_directory,
    const char *capture_path,
    const char *tex0_path,
    const char *tex1_path,
    const char *out_dir
)
{
    uint8_t *main_data = NULL;
    uint8_t *poly = NULL;
    uint8_t *tex = NULL;
    uint8_t *bank[2] = { NULL, NULL };
    size_t main_size = 0u;
    size_t poly_size = 0u;
    size_t tex_size = 0u;
    size_t bank_size[2] = { 0u, 0u };
    ext_tex_piece *pieces = NULL;
    size_t piece_count = 0u;
    size_t piece_cap = 0u;
    FILE *cap = NULL;
    char line[256];
    char path[1024];
    FILE *obj = NULL;
    FILE *mtl = NULL;
    size_t i;
    size_t vbase = 1u; /* OBJ indices are 1-based */
    uint32_t p1_lo = 0u;
    uint32_t p1_hi = 0u;
    size_t p1_first = 0u;
    size_t p1_last = 0u;
    size_t tri_total = 0u;
    vf2_status st;

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data,
                                 &main_size);
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly,
                                     &poly_size);
    }
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_TEXTURES, &tex,
                                     &tex_size);
    }
    if (st != VF2_OK) {
        fprintf(stderr, "Could not build ROM regions: %s\n",
                vf2_status_string(st));
        free(main_data);
        free(poly);
        return EXIT_FAILURE;
    }
    if (!ext_tex_load_file(tex0_path, &bank[0], &bank_size[0]) ||
        !ext_tex_load_file(tex1_path, &bank[1], &bank_size[1])) {
        fprintf(stderr, "Could not read texture banks\n");
        goto fail;
    }
    cap = fopen(capture_path, "rb");
    if (cap == NULL) {
        fprintf(stderr, "Could not open capture %s\n", capture_path);
        goto fail;
    }

    /* Collect each on-screen object once (first frame): geo 0x00800010 value is
     * the object's w0 (tpa); resolve id + record via the object table. */
    while (fgets(line, (int)sizeof(line), cap) != NULL) {
        unsigned seq;
        char frame[8];
        char stream[16];
        unsigned addr;
        unsigned value;
        uint32_t off = 0u;
        uint32_t bnk = 0u;
        uint32_t id = 0u;
        size_t k;
        int dup = 0;
        if (sscanf(line, "%u %7s %15s %x %x", &seq, frame, stream, &addr,
                   &value) != 5) {
            continue;
        }
        if (strcmp(frame, "f1") != 0 || strcmp(stream, "geo") != 0 ||
            addr != 0x00800010u) {
            continue;
        }
        if (vf2_model_lookup_w0(main_data, main_size, value, 0x4000u, &off, &bnk,
                                &id) != VF2_OK) {
            continue;
        }
        for (k = 0u; k < piece_count; ++k) {
            if (pieces[k].object_id == id) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            continue;
        }
        if (piece_count == piece_cap) {
            size_t ncap = piece_cap == 0u ? 64u : piece_cap * 2u;
            ext_tex_piece *np =
                (ext_tex_piece *)realloc(pieces, ncap * sizeof(*np));
            if (np == NULL) {
                goto fail;
            }
            pieces = np;
            piece_cap = ncap;
        }
        pieces[piece_count].rec_byte = off;
        pieces[piece_count].uv_ptr =
            tl_u32(main_data, main_size,
                   0x000e0004u + (size_t)id * 16u + 0u) & 0x7fffffu;
        pieces[piece_count].th_ptr =
            tl_u32(main_data, main_size,
                   0x000e0004u + (size_t)id * 16u + 4u) & 0x7fffffu;
        pieces[piece_count].object_id = id;
        ++piece_count;
    }
    fclose(cap);
    cap = NULL;

    if (piece_count == 0u) {
        fprintf(stderr, "No resolvable objects in capture\n");
        goto fail;
    }

    /* Player 1 = the first record-contiguous cluster (sorted by rec_byte, split
     * on a gap > 0x8000), mirroring extract-models. */
    for (i = 0u; i + 1u < piece_count; ++i) {
        size_t j;
        for (j = i + 1u; j < piece_count; ++j) {
            if (pieces[j].rec_byte < pieces[i].rec_byte) {
                ext_tex_piece t = pieces[i];
                pieces[i] = pieces[j];
                pieces[j] = t;
            }
        }
    }
    /* Player 1 = the LARGEST record-contiguous cluster (the fighter has far more
     * pieces than stage/shadow objects); ties keep the lowest-address one, which
     * is P1 (P2 sits higher in the polygon ROM). */
    {
        size_t cl_start = 0u;
        size_t best_start = 0u;
        size_t best_count = 0u;
        for (i = 1u; i <= piece_count; ++i) {
            int boundary = (i == piece_count) ||
                           (pieces[i].rec_byte - pieces[i - 1u].rec_byte > 0x8000u);
            if (boundary) {
                size_t count = i - cl_start;
                if (count > best_count) {
                    best_count = count;
                    best_start = cl_start;
                }
                cl_start = i;
            }
        }
        p1_first = best_start;
        p1_last = best_start + best_count - 1u;
    }
    p1_lo = pieces[p1_first].rec_byte;
    p1_hi = pieces[p1_last].rec_byte;

    snprintf(path, sizeof(path), "%s/player1.obj", out_dir);
    obj = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/player1.mtl", out_dir);
    mtl = fopen(path, "wb");
    if (obj == NULL || mtl == NULL) {
        fprintf(stderr, "Could not open output OBJ/MTL in %s\n", out_dir);
        goto fail;
    }
    fprintf(mtl, "# VF2 player1 textures (grayscale luma index). Local only.\n");
    fprintf(mtl, "newmtl sheet0\nmap_Kd tex0.png\n");
    fprintf(mtl, "newmtl sheet1\nmap_Kd tex1.png\n");

    fprintf(obj, "# VF2 player1 textured mesh. Local only, never committed.\n");
    fprintf(obj, "mtllib player1.mtl\n");

    /* Two passes over pieces: emit v/vt interleaved and remember per-face the
     * sheet, then write faces grouped by material. To keep one pass simple we
     * emit faces immediately with a usemtl before each run change. */
    {
        int cur_sheet = -1;
        for (i = p1_first; i <= p1_last; ++i) {
            vbase = ext_emit_piece(obj, poly, poly_size, tex, tex_size,
                                   pieces[i].rec_byte, pieces[i].uv_ptr,
                                   pieces[i].th_ptr, vbase, &cur_sheet,
                                   &tri_total, NULL, NULL, NULL, 0u,
                                   NULL, 0u, NULL, NULL, NULL, 0u);
        }
    }
    fclose(obj);
    obj = NULL;
    fclose(mtl);
    mtl = NULL;

    snprintf(path, sizeof(path), "%s/tex0.png", out_dir);
    (void)ext_tex_write_sheet_png(path, bank[0], bank_size[0]);
    snprintf(path, sizeof(path), "%s/tex1.png", out_dir);
    (void)ext_tex_write_sheet_png(path, bank[1], bank_size[1]);

    printf("export-textured: player1 = pieces %zu..%zu (poly 0x%x..0x%x), "
           "%zu triangles -> %s/player1.obj (+ .mtl, tex0/1.png)\n",
           p1_first, p1_last, (unsigned)p1_lo, (unsigned)p1_hi, tri_total,
           out_dir);

    free(main_data);
    free(poly);
    free(tex);
    free(bank[0]);
    free(bank[1]);
    free(pieces);
    return EXIT_SUCCESS;

fail:
    if (cap != NULL) fclose(cap);
    if (obj != NULL) fclose(obj);
    if (mtl != NULL) fclose(mtl);
    free(main_data);
    free(poly);
    free(tex);
    free(bank[0]);
    free(bank[1]);
    free(pieces);
    return EXIT_FAILURE;
}

/* --- export-pieces: one textured OBJ per on-screen object, + measured metrics -
 *
 * celsowm slice (2026-10-16): separate fighters from scenery. The earlier
 * export-textured picked one record-contiguous cluster and *labelled it P1 by
 * address range* -- that was wrong (the cluster was stage geometry). This
 * command makes no such claim. It exports EVERY on-screen object as its own
 * textured OBJ (piece_<NNN>.obj) so the fighters can be identified by eye, and
 * writes pieces.csv with per-object metrics MEASURED across the 3 capture
 * frames so a classification can be made from data, not an address guess:
 *   - geo_changed : the object's geo command {tpa,tha,oba,obc} differs across
 *                   f1/f2/f3 (an animating object re-selects its mesh; scenery
 *                   never does).  PROVES motion when set.
 *   - reads_f*    : count of TGP copro (0x884000) read-backs in the object's
 *                   per-frame window -- how much per-object transform data the
 *                   capture actually carries for it.
 *   - out_delta   : max |float| change of those read-backs across frames.
 *   - region      : oba source (ROM / fast-RAM / slow-RAM).
 * See fighter_scenery_separation_v0834.md for why these alone do NOT cleanly
 * separate fighters from scenery on this particular round-start capture. */

typedef struct ep_obj {
    uint32_t tpa;
    uint32_t tha;
    uint32_t oba;
    uint32_t obc;
    float   *reads;
    size_t   nreads;
    size_t   rcap;
} ep_obj;

typedef struct ep_frame {
    ep_obj *objs;
    size_t  n;
    size_t  cap;
} ep_frame;

static int ep_frame_index(const char *frame)
{
    if (strcmp(frame, "f1") == 0) return 0;
    if (strcmp(frame, "f2") == 0) return 1;
    if (strcmp(frame, "f3") == 0) return 2;
    return -1;
}

static void ep_push_read(ep_obj *o, uint32_t raw)
{
    float f;
    memcpy(&f, &raw, 4u);
    if (o->nreads == o->rcap) {
        size_t nc = o->rcap == 0u ? 8u : o->rcap * 2u;
        float *nr = (float *)realloc(o->reads, nc * sizeof(*nr));
        if (nr == NULL) {
            return;
        }
        o->reads = nr;
        o->rcap = nc;
    }
    o->reads[o->nreads++] = f;
}

static ep_obj *ep_frame_new(ep_frame *fr)
{
    if (fr->n == fr->cap) {
        size_t nc = fr->cap == 0u ? 128u : fr->cap * 2u;
        ep_obj *no = (ep_obj *)realloc(fr->objs, nc * sizeof(*no));
        if (no == NULL) {
            return NULL;
        }
        fr->objs = no;
        fr->cap = nc;
    }
    memset(&fr->objs[fr->n], 0, sizeof(fr->objs[fr->n]));
    return &fr->objs[fr->n++];
}

/* Parse the capture into per-frame ordered object lists. An object is bounded
 * by its geo-0x800010 submission; the 4 prg words that follow are its geo
 * command; the TGP read-backs since the previous object form its window. */
static int ep_parse(const char *capture_path, ep_frame fr[3])
{
    FILE *cap = fopen(capture_path, "rb");
    char line[256];
    /* one in-progress geo command per frame: -1 = none, else words filled */
    int gc_fill[3] = { -1, -1, -1 };
    uint32_t gc[3][4];
    static float pend[3][8192];
    size_t pend_n[3] = { 0u, 0u, 0u };
    if (cap == NULL) {
        return 0;
    }
    while (fgets(line, (int)sizeof(line), cap) != NULL) {
        unsigned seq;
        char frame[8];
        char stream[16];
        unsigned addr;
        unsigned value;
        int fi;
        if (sscanf(line, "%u %7s %15s %x %x", &seq, frame, stream, &addr,
                   &value) != 5) {
            continue;
        }
        fi = ep_frame_index(frame);
        if (fi < 0) {
            continue;
        }
        if (strcmp(stream, "geo") == 0 && (addr & 0x3fffu) == 0x0010u) {
            gc_fill[fi] = 0;
            gc[fi][0] = value; /* tpa echoed here too; real words via prg */
            continue;
        }
        if (gc_fill[fi] >= 0 && strcmp(stream, "prg") == 0) {
            gc[fi][gc_fill[fi]] = value;
            gc_fill[fi] += 1;
            if (gc_fill[fi] >= 4) {
                ep_obj *o = ep_frame_new(&fr[fi]);
                size_t k;
                gc_fill[fi] = -1;
                if (o == NULL) {
                    fclose(cap);
                    return 0;
                }
                o->tpa = gc[fi][0];
                o->tha = gc[fi][1];
                o->oba = gc[fi][2];
                o->obc = gc[fi][3];
                for (k = 0u; k < pend_n[fi]; ++k) {
                    uint32_t raw;
                    memcpy(&raw, &pend[fi][k], 4u);
                    ep_push_read(o, raw);
                }
                pend_n[fi] = 0u;
            }
            continue;
        }
        if (strcmp(stream, "fifoR") == 0 && (addr & 0xffffffu) == 0x884000u) {
            if (pend_n[fi] < (sizeof(pend[fi]) / sizeof(pend[fi][0]))) {
                float f;
                memcpy(&f, &value, 4u);
                pend[fi][pend_n[fi]++] = f;
            }
        }
    }
    fclose(cap);
    return 1;
}

static int ep_geo_changed(ep_frame fr[3], size_t k)
{
    if (k >= fr[0].n || k >= fr[1].n || k >= fr[2].n) {
        return 0;
    }
    {
        const ep_obj *a = &fr[0].objs[k];
        const ep_obj *b = &fr[1].objs[k];
        const ep_obj *c = &fr[2].objs[k];
        int same = a->tpa == b->tpa && a->tpa == c->tpa &&
                   a->tha == b->tha && a->tha == c->tha &&
                   a->oba == b->oba && a->oba == c->oba &&
                   a->obc == b->obc && a->obc == c->obc;
        return same ? 0 : 1;
    }
}

static float ep_out_delta(ep_frame fr[3], size_t k)
{
    float md = 0.0f;
    size_t m;
    size_t i;
    if (k >= fr[0].n || k >= fr[1].n || k >= fr[2].n) {
        return 0.0f;
    }
    m = fr[0].objs[k].nreads;
    if (fr[1].objs[k].nreads < m) m = fr[1].objs[k].nreads;
    if (fr[2].objs[k].nreads < m) m = fr[2].objs[k].nreads;
    for (i = 0u; i < m; ++i) {
        float a = fr[0].objs[k].reads[i];
        float b = fr[1].objs[k].reads[i];
        float c = fr[2].objs[k].reads[i];
        float d;
        if (a != a || b != b || c != c) {
            continue; /* skip NaN */
        }
        d = (float)fabs((double)a - (double)b);
        if (d > md) md = d;
        d = (float)fabs((double)b - (double)c);
        if (d > md) md = d;
        d = (float)fabs((double)a - (double)c);
        if (d > md) md = d;
    }
    return md;
}

static const char *ep_region(uint32_t oba)
{
    if (oba & 0x01000000u) {
        return "fastRAM";
    }
    if (oba & 0x00800000u) {
        return "ROM";
    }
    return "slowRAM";
}

static int command_export_pieces(
    const char *rom_directory,
    const char *capture_path,
    const char *tex0_path,
    const char *tex1_path,
    const char *out_dir
)
{
    uint8_t *main_data = NULL;
    uint8_t *poly = NULL;
    uint8_t *tex = NULL;
    uint8_t *bank[2] = { NULL, NULL };
    size_t main_size = 0u;
    size_t poly_size = 0u;
    size_t tex_size = 0u;
    size_t bank_size[2] = { 0u, 0u };
    ep_frame fr[3];
    FILE *mtl = NULL;
    FILE *csv = NULL;
    char path[1024];
    size_t k;
    size_t written = 0u;
    uint32_t *seen = NULL;
    size_t seen_n = 0u;
    vf2_status st;

    memset(fr, 0, sizeof(fr));

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data,
                                 &main_size);
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly,
                                     &poly_size);
    }
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_TEXTURES, &tex,
                                     &tex_size);
    }
    if (st != VF2_OK) {
        fprintf(stderr, "Could not build ROM regions: %s\n",
                vf2_status_string(st));
        free(main_data);
        free(poly);
        return EXIT_FAILURE;
    }
    if (!ext_tex_load_file(tex0_path, &bank[0], &bank_size[0]) ||
        !ext_tex_load_file(tex1_path, &bank[1], &bank_size[1])) {
        fprintf(stderr, "Could not read texture banks\n");
        goto fail;
    }
    if (!ep_parse(capture_path, fr)) {
        fprintf(stderr, "Could not parse capture %s\n", capture_path);
        goto fail;
    }
    if (fr[0].n == 0u) {
        fprintf(stderr, "No object submissions in capture frame f1\n");
        goto fail;
    }

    snprintf(path, sizeof(path), "%s/pieces.mtl", out_dir);
    mtl = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/pieces.csv", out_dir);
    csv = fopen(path, "wb");
    if (mtl == NULL || csv == NULL) {
        fprintf(stderr, "Could not open outputs in %s\n", out_dir);
        goto fail;
    }
    fprintf(mtl, "# VF2 pieces textures (grayscale luma index). Local only.\n");
    fprintf(mtl, "newmtl sheet0\nmap_Kd tex0.png\n");
    fprintf(mtl, "newmtl sheet1\nmap_Kd tex1.png\n");
    fprintf(csv, "index,object_id,rec_byte,tpa,tha,oba,obc,region,tris,"
                 "geo_changed,reads_f1,reads_f2,reads_f3,out_delta\n");

    seen = (uint32_t *)malloc(fr[0].n * sizeof(*seen));
    if (seen == NULL) {
        goto fail;
    }

    for (k = 0u; k < fr[0].n; ++k) {
        uint32_t w0 = fr[0].objs[k].tpa;
        uint32_t off = 0u;
        uint32_t bnk = 0u;
        uint32_t id = 0u;
        uint32_t uv_ptr;
        uint32_t th_ptr;
        size_t j;
        int dup = 0;
        FILE *obj;
        int cur_sheet = -1;
        size_t tris = 0u;

        if (vf2_model_lookup_w0(main_data, main_size, w0, 0x4000u, &off, &bnk,
                                &id) != VF2_OK) {
            continue;
        }
        for (j = 0u; j < seen_n; ++j) {
            if (seen[j] == id) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            continue;
        }
        seen[seen_n++] = id;

        uv_ptr = tl_u32(main_data, main_size,
                        0x000e0004u + (size_t)id * 16u + 0u) & 0x7fffffu;
        th_ptr = tl_u32(main_data, main_size,
                        0x000e0004u + (size_t)id * 16u + 4u) & 0x7fffffu;

        snprintf(path, sizeof(path), "%s/piece_%03zu_obj%04x.obj", out_dir, k,
                 (unsigned)id);
        obj = fopen(path, "wb");
        if (obj == NULL) {
            continue;
        }
        fprintf(obj, "# VF2 piece: capture index %zu, object id 0x%x, rec_byte "
                     "0x%x. Local only, never committed.\n", k, (unsigned)id,
                (unsigned)off);
        fprintf(obj, "mtllib pieces.mtl\n");
        (void)ext_emit_piece(obj, poly, poly_size, tex, tex_size, off, uv_ptr,
                             th_ptr, 1u, &cur_sheet, &tris, NULL, NULL, NULL,
                             0u, NULL, 0u, NULL, NULL, NULL, 0u);
        fclose(obj);
        ++written;

        fprintf(csv, "%zu,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,%s,%zu,%d,%zu,%zu,%zu,"
                     "%.4f\n",
                k, (unsigned)id, (unsigned)off,
                (unsigned)fr[0].objs[k].tpa, (unsigned)fr[0].objs[k].tha,
                (unsigned)fr[0].objs[k].oba, (unsigned)fr[0].objs[k].obc,
                ep_region(fr[0].objs[k].oba), tris, ep_geo_changed(fr, k),
                fr[0].objs[k].nreads,
                (k < fr[1].n) ? fr[1].objs[k].nreads : 0u,
                (k < fr[2].n) ? fr[2].objs[k].nreads : 0u,
                (double)ep_out_delta(fr, k));
    }

    fclose(mtl);
    mtl = NULL;
    fclose(csv);
    csv = NULL;

    snprintf(path, sizeof(path), "%s/tex0.png", out_dir);
    (void)ext_tex_write_sheet_png(path, bank[0], bank_size[0]);
    snprintf(path, sizeof(path), "%s/tex1.png", out_dir);
    (void)ext_tex_write_sheet_png(path, bank[1], bank_size[1]);

    printf("export-pieces: %zu objects in capture, %zu unique pieces written "
           "-> %s/piece_*.obj (+ pieces.mtl, pieces.csv, tex0/1.png)\n",
           fr[0].n, written, out_dir);

    for (k = 0u; k < 3u; ++k) {
        size_t i;
        for (i = 0u; i < fr[k].n; ++i) {
            free(fr[k].objs[i].reads);
        }
        free(fr[k].objs);
    }
    free(seen);
    free(main_data);
    free(poly);
    free(tex);
    free(bank[0]);
    free(bank[1]);
    return EXIT_SUCCESS;

fail:
    if (mtl != NULL) fclose(mtl);
    if (csv != NULL) fclose(csv);
    for (k = 0u; k < 3u; ++k) {
        size_t i;
        for (i = 0u; i < fr[k].n; ++i) {
            free(fr[k].objs[i].reads);
        }
        free(fr[k].objs);
    }
    free(seen);
    free(main_data);
    free(poly);
    free(tex);
    free(bank[0]);
    free(bank[1]);
    return EXIT_FAILURE;
}

/* --- geo-walk: validate the Step-1 display-list walker on a RAM9 dump ---------
 *
 * Loads one frame's RAM9 display list (131072 bytes = 0x8000 u32 LE words) from
 * a local end-of-frame dump, walks it from the given read-start with
 * vf2_geo_walk, and reports the stop reason, object count, and -- as the Step-1
 * success check -- whether Akira's record cluster (0x130a44..0x1434a0, which
 * holds obj 0x19c, his face) is among the submitted objects. The dump is local
 * only, never committed; the walker itself is covered synthetically in CI. */
static int command_geo_walk(
    const char *bin_path, const char *frame_str, const char *readstart_str
)
{
    const size_t frame_bytes = 131072u;
    const size_t frame_words = frame_bytes / 4u;
    FILE *f = NULL;
    uint8_t *raw = NULL;
    uint32_t *words = NULL;
    vf2_geo_object *objs = NULL;
    size_t max_objs = 8192u;
    size_t frame;
    uint32_t readstart;
    size_t got;
    size_t n = 0u;
    size_t opc = 0u;
    vf2_geo_stop stop = VF2_GEO_STOP_BOUNDS;
    size_t i;
    size_t akira = 0u;
    int rc = EXIT_FAILURE;

    frame = (size_t)strtoul(frame_str, NULL, 0);
    readstart = (uint32_t)strtoul(readstart_str, NULL, 16);

    f = fopen(bin_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "geo-walk: cannot open %s\n", bin_path);
        return EXIT_FAILURE;
    }
    raw = (uint8_t *)malloc(frame_bytes);
    words = (uint32_t *)malloc(frame_words * sizeof(*words));
    objs = (vf2_geo_object *)malloc(max_objs * sizeof(*objs));
    if (raw == NULL || words == NULL || objs == NULL) {
        goto done;
    }
    if (fseek(f, (long)(frame * frame_bytes), SEEK_SET) != 0) {
        fprintf(stderr, "geo-walk: cannot seek to frame %zu\n", frame);
        goto done;
    }
    got = fread(raw, 1u, frame_bytes, f);
    if (got != frame_bytes) {
        fprintf(stderr, "geo-walk: short read (%zu/%zu) -- frame %zu present?\n",
                got, frame_bytes, frame);
        goto done;
    }
    for (i = 0u; i < frame_words; ++i) {
        words[i] = (uint32_t)raw[i * 4u + 0u] |
                   ((uint32_t)raw[i * 4u + 1u] << 8) |
                   ((uint32_t)raw[i * 4u + 2u] << 16) |
                   ((uint32_t)raw[i * 4u + 3u] << 24);
    }

    if (vf2_geo_walk(words, frame_words, readstart, objs, max_objs, &n, &stop,
                     &opc) != VF2_OK) {
        fprintf(stderr, "geo-walk: invalid arguments\n");
        goto done;
    }

    for (i = 0u; i < n; ++i) {
        uint32_t rec = (objs[i].oba & 0x7fffffu) * 4u;
        if (rec >= 0x130a44u && rec <= 0x1434a0u) {
            ++akira;
        }
    }

    {
        const char *why = stop == VF2_GEO_STOP_END ? "END"
                        : stop == VF2_GEO_STOP_FULL ? "FULL (out[] limit)"
                        : stop == VF2_GEO_STOP_BOUNDS ? "BOUNDS"
                        : "OPCODE_LIMIT";
        size_t base = (size_t)((readstart & 0x1ffffu) >> 2);
        printf("geo-walk: frame %zu readstart 0x%x -> stop %s, %zu objects, "
               "%zu opcodes\n", frame, (unsigned)readstart, why, n, opc);
        if (base + 3u < frame_words) {
            printf("  list header: %08x %08x %08x %08x\n", words[base],
                   words[base + 1u], words[base + 2u], words[base + 3u]);
        }
        printf("  Akira cluster 0x130a44..0x1434a0: %zu object(s)%s\n", akira,
               akira > 0u ? " present" : " ABSENT");
    }
    rc = (stop == VF2_GEO_STOP_END) ? EXIT_SUCCESS : EXIT_FAILURE;

done:
    if (f != NULL) {
        fclose(f);
    }
    free(raw);
    free(words);
    free(objs);
    return rc;
}

/* --- geo-scene: Step 2 -- per-object matrices, grouping, posed full scene ----
 *
 * Walks every frame of a RAM9 end-of-frame dump (the Step-1 walker), tracks
 * each object slot's transform matrix across all frames, and:
 *   1. classifies each slot FIXED (matrix byte-identical in every frame =
 *      scenery) or MOVING, and clusters the moving slots spatially (2-means on
 *      mean translation) into the two fighters, with an OTHER bucket for moving
 *      outliers (shadows/effects that sit near neither fighter);
 *   2. labels the fighter cluster holding Akira's anchor records
 *      (oba rec in 0x130a44..0x1434a0, obj 0x19c = his face) as fighter A;
 *   3. writes scene.obj -- all slots of one chosen frame, posed by their
 *      matrix, geometry+UV from ROM via the validated export-pieces lookup --
 *      so the object count and layout can be eyeballed against the game.
 * Local-only; the walker is covered synthetically in CI. */

#define GS_MAX_FRAMES 64u

static int gs_read_frame(FILE *f, size_t frame, size_t frame_bytes,
                         uint32_t *words, size_t frame_words)
{
    uint8_t *raw;
    size_t got;
    size_t i;
    raw = (uint8_t *)malloc(frame_bytes);
    if (raw == NULL) {
        return 0;
    }
    if (fseek(f, (long)(frame * frame_bytes), SEEK_SET) != 0 ||
        (got = fread(raw, 1u, frame_bytes, f)) != frame_bytes) {
        free(raw);
        return 0;
    }
    for (i = 0u; i < frame_words; ++i) {
        words[i] = (uint32_t)raw[i * 4u + 0u] |
                   ((uint32_t)raw[i * 4u + 1u] << 8) |
                   ((uint32_t)raw[i * 4u + 2u] << 16) |
                   ((uint32_t)raw[i * 4u + 3u] << 24);
    }
    free(raw);
    return 1;
}

/* A small axis-aligned box marker at (cx,cy,cz), used as a position stub for a
 * slot whose geometry does not resolve, so the on-screen count stays visible in
 * scene.obj. Returns the next 1-based vertex index. */
static size_t gs_emit_box(FILE *obj, size_t vbase, double cx, double cy,
                          double cz, double s)
{
    static const int face[12][3] = {
        {0,1,2},{0,2,3},{4,6,5},{4,7,6},{0,4,5},{0,5,1},
        {1,5,6},{1,6,2},{2,6,7},{2,7,3},{3,7,4},{3,4,0}
    };
    int i;
    for (i = 0; i < 8; ++i) {
        double x = cx + ((i & 1) ? s : -s);
        double y = cy + ((i & 2) ? s : -s);
        double z = cz + ((i & 4) ? s : -s);
        fprintf(obj, "v %.6f %.6f %.6f\n", x, y, z);
    }
    for (i = 0; i < 12; ++i) {
        fprintf(obj, "f %zu %zu %zu\n", vbase + (size_t)face[i][0],
                vbase + (size_t)face[i][1], vbase + (size_t)face[i][2]);
    }
    return vbase + 8u;
}

/* |det| of the matrix's 3x3 part, normalised by its column norms: ~1 for a
 * rotation (with any uniform scale), ~0 when the three axes become coplanar --
 * which is how VF2 draws a shadow, flattening a fighter's limbs onto the
 * ground plane. Columns are (m0,m1,m2),(m3,m4,m5),(m6,m7,m8) per MAME's
 * transform_point. */
static double gs_flatness(const float *m)
{
    double a0 = (double)m[0], a1 = (double)m[1], a2 = (double)m[2];
    double b0 = (double)m[3], b1 = (double)m[4], b2 = (double)m[5];
    double c0 = (double)m[6], c1 = (double)m[7], c2 = (double)m[8];
    double cx = b1 * c2 - b2 * c1;
    double cy = b2 * c0 - b0 * c2;
    double cz = b0 * c1 - b1 * c0;
    double det = a0 * cx + a1 * cy + a2 * cz;
    double na = sqrt(a0 * a0 + a1 * a1 + a2 * a2);
    double nb = sqrt(b0 * b0 + b1 * b1 + b2 * b2);
    double nc = sqrt(c0 * c0 + c1 * c1 + c2 * c2);
    double d = na * nb * nc;
    return d > 0.0 ? fabs(det) / d : 0.0;
}

/* Inverse of the as-applied 3x3 A (A[r][c]=m[3r+c], point p' = p.A + T), into
 * inv[3r+c]. Returns 0 if singular. */
static int gs_mat3_inv(const float *m, double inv[9])
{
    double a = m[0], b = m[1], c = m[2];
    double d = m[3], e = m[4], f = m[5];
    double g = m[6], h = m[7], i = m[8];
    double det = a * (e * i - f * h) - b * (d * i - f * g)
               + c * (d * h - e * g);
    double id;
    if (fabs(det) < 1e-20) return 0;
    id = 1.0 / det;
    inv[0] = (e * i - f * h) * id; inv[1] = (c * h - b * i) * id;
    inv[2] = (b * f - c * e) * id;
    inv[3] = (f * g - d * i) * id; inv[4] = (a * i - c * g) * id;
    inv[5] = (c * d - a * f) * id;
    inv[6] = (d * h - e * g) * id; inv[7] = (b * g - a * h) * id;
    inv[8] = (a * e - b * d) * id;
    return 1;
}

/* Compose M_obj then (M_dec)^-1 into out[12]: express an object posed in view
 * space (camera baked in) back in the decor's world frame (camera removed).
 * out.A = A_obj . A_dec^-1 ; out.T = (T_obj - T_dec) . A_dec^-1. */
static void gs_worldframe(const float *m_obj, const float *m_dec,
                          const double adi[9], float out[12])
{
    int r, cc, k;
    double tf[3];
    for (r = 0; r < 3; ++r) {
        for (cc = 0; cc < 3; ++cc) {
            double s = 0.0;
            for (k = 0; k < 3; ++k)
                s += (double)m_obj[3 * r + k] * adi[3 * k + cc];
            out[3 * r + cc] = (float)s;
        }
    }
    for (cc = 0; cc < 3; ++cc) {
        double s = 0.0;
        for (k = 0; k < 3; ++k)
            s += ((double)m_obj[9 + k] - (double)m_dec[9 + k]) * adi[3 * k + cc];
        tf[cc] = s;
    }
    out[9] = (float)tf[0]; out[10] = (float)tf[1]; out[11] = (float)tf[2];
}

/* Transpose the as-applied 3x3, keeping the translation, into out[12]. */
static void gs_transpose(const float *m, float *out)
{
    out[0] = m[0]; out[1] = m[3]; out[2] = m[6];
    out[3] = m[1]; out[4] = m[4]; out[5] = m[7];
    out[6] = m[2]; out[7] = m[5]; out[8] = m[8];
    out[9] = m[9]; out[10] = m[10]; out[11] = m[11];
}

#define GS_SHADOW_EPS 0.10 /* flatness below this = a flattened (shadow) matrix */

static int command_geo_scene(
    const char *bin_path, const char *txt_path, const char *rom_directory,
    const char *tex0_path, const char *tex1_path, const char *pose_str,
    const char *out_dir
)
{
    const size_t frame_bytes = 131072u;
    const size_t frame_words = frame_bytes / 4u;
    FILE *bin = NULL;
    FILE *txt = NULL;
    FILE *obj = NULL;
    FILE *mtl = NULL;
    FILE *csv = NULL;
    char line[256];
    char path[1024];
    uint32_t *words = NULL;
    vf2_geo_object *objs = NULL;
    size_t max_objs = 4096u;
    uint8_t *main_data = NULL;
    uint8_t *poly = NULL;
    uint8_t *tex = NULL;
    uint8_t *bank[2] = { NULL, NULL };
    size_t main_size = 0u;
    size_t poly_size = 0u;
    size_t tex_size = 0u;
    size_t bank_size[2] = { 0u, 0u };
    uint32_t readstart[GS_MAX_FRAMES];
    size_t count[GS_MAX_FRAMES];
    size_t nframes = 0u;
    size_t nslots = 0u;
    size_t pose;
    size_t f;
    size_t s;
    float *mat = NULL;      /* [frame*nslots + slot][12] */
    uint32_t *obaf = NULL;  /* [frame*nslots + slot] */
    uint32_t *tpaf = NULL;
    uint32_t *thaf = NULL;
    int *fixed = NULL;      /* matrix byte-identical in every frame? */
    int *grp = NULL;        /* 0 = camera-common, 1 = self-moving */
    int *anchor = NULL;     /* oba rec in Akira's ground-truth cluster */
    int *shadow = NULL;     /* pose-frame matrix is flattened (det3 ~ 0) */
    double *flat = NULL;    /* pose-frame flatness per slot */
    double *mtx = NULL;     /* mean translation, 3 per slot */
    double *motion_t = NULL;
    double *sorted = NULL;  /* motion_t sorted, for the band threshold */
    double floor_m = 0.0;
    double threshold = 0.0;
    size_t gcount[2] = { 0u, 0u };
    size_t resolved = 0u;
    size_t stub = 0u;
    size_t vbase = 1u;
    int cur_sheet = -1;
    vf2_status st;
    int rc = EXIT_FAILURE;

    pose = (size_t)strtoul(pose_str, NULL, 0);

    /* readstarts from the .txt (one "F idx frame readstart ..." line/frame) */
    txt = fopen(txt_path, "rb");
    if (txt == NULL) {
        fprintf(stderr, "geo-scene: cannot open %s\n", txt_path);
        return EXIT_FAILURE;
    }
    while (fgets(line, (int)sizeof(line), txt) != NULL) {
        unsigned idx;
        unsigned frameno;
        unsigned rs;
        if (sscanf(line, "F %u %u %x", &idx, &frameno, &rs) == 3 &&
            idx < GS_MAX_FRAMES) {
            readstart[idx] = rs;
            if ((size_t)idx + 1u > nframes) {
                nframes = (size_t)idx + 1u;
            }
        }
    }
    fclose(txt);
    txt = NULL;
    if (nframes == 0u) {
        fprintf(stderr, "geo-scene: no 'F' frame lines in %s\n", txt_path);
        return EXIT_FAILURE;
    }
    if (pose >= nframes) {
        fprintf(stderr, "geo-scene: pose frame %zu >= %zu frames\n", pose,
                nframes);
        return EXIT_FAILURE;
    }

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data,
                                 &main_size);
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly,
                                     &poly_size);
    }
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_TEXTURES, &tex,
                                     &tex_size);
    }
    if (st != VF2_OK) {
        fprintf(stderr, "geo-scene: ROM regions: %s\n", vf2_status_string(st));
        goto done;
    }
    if (!ext_tex_load_file(tex0_path, &bank[0], &bank_size[0]) ||
        !ext_tex_load_file(tex1_path, &bank[1], &bank_size[1])) {
        fprintf(stderr, "geo-scene: cannot read texture banks\n");
        goto done;
    }

    bin = fopen(bin_path, "rb");
    words = (uint32_t *)malloc(frame_words * sizeof(*words));
    objs = (vf2_geo_object *)malloc(max_objs * sizeof(*objs));
    if (bin == NULL || words == NULL || objs == NULL) {
        fprintf(stderr, "geo-scene: cannot open %s / allocate\n", bin_path);
        goto done;
    }

    /* Frame 0 fixes the slot count. */
    {
        size_t n0 = 0u;
        vf2_geo_stop stop0;
        if (!gs_read_frame(bin, 0u, frame_bytes, words, frame_words) ||
            vf2_geo_walk(words, frame_words, readstart[0], objs, max_objs, &n0,
                         &stop0, NULL) != VF2_OK) {
            fprintf(stderr, "geo-scene: frame 0 walk failed\n");
            goto done;
        }
        nslots = n0;
    }
    if (nslots == 0u) {
        fprintf(stderr, "geo-scene: frame 0 has no objects\n");
        goto done;
    }

    mat = (float *)malloc(nframes * nslots * 12u * sizeof(*mat));
    obaf = (uint32_t *)malloc(nframes * nslots * sizeof(*obaf));
    tpaf = (uint32_t *)malloc(nframes * nslots * sizeof(*tpaf));
    thaf = (uint32_t *)malloc(nframes * nslots * sizeof(*thaf));
    fixed = (int *)malloc(nslots * sizeof(*fixed));
    grp = (int *)malloc(nslots * sizeof(*grp));
    anchor = (int *)malloc(nslots * sizeof(*anchor));
    shadow = (int *)malloc(nslots * sizeof(*shadow));
    flat = (double *)malloc(nslots * sizeof(*flat));
    mtx = (double *)malloc(nslots * 3u * sizeof(*mtx));
    motion_t = (double *)malloc(nslots * sizeof(*motion_t));
    sorted = (double *)malloc(nslots * sizeof(*sorted));
    if (mat == NULL || obaf == NULL || tpaf == NULL || thaf == NULL || fixed == NULL ||
        grp == NULL || anchor == NULL || shadow == NULL || flat == NULL ||
        mtx == NULL || motion_t == NULL || sorted == NULL) {
        goto done;
    }

    for (f = 0u; f < nframes; ++f) {
        size_t n = 0u;
        vf2_geo_stop stop;
        if (!gs_read_frame(bin, f, frame_bytes, words, frame_words) ||
            vf2_geo_walk(words, frame_words, readstart[f], objs, max_objs, &n,
                         &stop, NULL) != VF2_OK) {
            fprintf(stderr, "geo-scene: frame %zu walk failed\n", f);
            goto done;
        }
        count[f] = n;
        for (s = 0u; s < nslots; ++s) {
            size_t base = (f * nslots + s) * 12u;
            size_t k;
            if (s < n) {
                for (k = 0u; k < 12u; ++k) {
                    mat[base + k] = objs[s].matrix[k];
                }
                obaf[f * nslots + s] = objs[s].oba;
                tpaf[f * nslots + s] = objs[s].tpa;
                thaf[f * nslots + s] = objs[s].tha;
            } else {
                /* Frame shorter than slot count (not seen in practice): hold
                 * the frame-0 pose so metrics stay defined. */
                for (k = 0u; k < 12u; ++k) {
                    mat[base + k] = mat[s * 12u + k];
                }
                obaf[f * nslots + s] = obaf[s];
                tpaf[f * nslots + s] = tpaf[s];
                thaf[f * nslots + s] = thaf[s];
            }
        }
    }

    /* Per-slot: FIXED iff the 12-float matrix is byte-identical in every frame;
     * mean translation; motion_t = max over frames of |T_f - Tmean|. */
    for (s = 0u; s < nslots; ++s) {
        double mx = 0.0;
        double my = 0.0;
        double mz = 0.0;
        double mt = 0.0;
        int is_fixed = 1;
        uint32_t rec;
        const float *m0 = &mat[(0u * nslots + s) * 12u];
        for (f = 0u; f < nframes; ++f) {
            const float *mf = &mat[(f * nslots + s) * 12u];
            if (memcmp(mf, m0, 12u * sizeof(float)) != 0) {
                is_fixed = 0;
            }
            mx += (double)mf[9];
            my += (double)mf[10];
            mz += (double)mf[11];
        }
        mx /= (double)nframes;
        my /= (double)nframes;
        mz /= (double)nframes;
        for (f = 0u; f < nframes; ++f) {
            const float *mf = &mat[(f * nslots + s) * 12u];
            double dx = (double)mf[9] - mx;
            double dy = (double)mf[10] - my;
            double dz = (double)mf[11] - mz;
            double d = sqrt(dx * dx + dy * dy + dz * dz);
            if (d > mt) {
                mt = d;
            }
        }
        fixed[s] = is_fixed;
        mtx[s * 3u + 0u] = mx;
        mtx[s * 3u + 1u] = my;
        mtx[s * 3u + 2u] = mz;
        motion_t[s] = mt;
        flat[s] = gs_flatness(&mat[(pose * nslots + s) * 12u]);
        shadow[s] = (flat[s] < GS_SHADOW_EPS) ? 1 : 0;
        rec = (obaf[pose * nslots + s] & 0x7fffffu) * 4u;
        anchor[s] = (rec >= 0x130a44u && rec <= 0x1434a0u) ? 1 : 0;
    }

    /* Every object's matrix varies across frames (the camera pan is composed
     * into all of them), so a static-vs-moving test does not by itself split
     * decor from fighters -- that needs the camera removed (Step 3). What the
     * raw matrices DO give, measured here, is two bands: a common "camera"
     * band shared by objects that only ride the camera, and a self-moving tail
     * of objects that also move on their own (fighter limbs this frame). The
     * threshold is the midpoint of the widest gap in the sorted motion values,
     * so it is read from the data, not assumed. */
    for (s = 0u; s < nslots; ++s) {
        sorted[s] = motion_t[s];
    }
    for (s = 0u; s + 1u < nslots; ++s) {    /* insertion sort, nslots ~ 113 */
        size_t j = s + 1u;
        for (; j > 0u && sorted[j - 1u] > sorted[j]; --j) {
            double t = sorted[j - 1u];
            sorted[j - 1u] = sorted[j];
            sorted[j] = t;
        }
    }
    floor_m = sorted[nslots / 2u];          /* median = typical camera motion */
    {
        double best_gap = -1.0;
        size_t lo = nslots / 5u;            /* ignore gaps at the extreme ends */
        size_t hi = (nslots * 4u) / 5u;
        size_t gi;
        threshold = sorted[nslots - 1u] + 1.0; /* default: nothing self-moving */
        for (gi = lo; gi + 1u <= hi && gi + 1u < nslots; ++gi) {
            double gap = sorted[gi + 1u] - sorted[gi];
            if (gap > best_gap) {
                best_gap = gap;
                threshold = 0.5 * (sorted[gi] + sorted[gi + 1u]);
            }
        }
    }
    for (s = 0u; s < nslots; ++s) {
        grp[s] = (motion_t[s] >= threshold) ? 1 : 0;
        ++gcount[grp[s]];
    }

    /* scene.obj: every slot of the pose frame, posed by its matrix. */
    snprintf(path, sizeof(path), "%s/scene.mtl", out_dir);
    mtl = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/scene_groups.csv", out_dir);
    csv = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/scene.obj", out_dir);
    obj = fopen(path, "wb");
    if (mtl == NULL || csv == NULL || obj == NULL) {
        fprintf(stderr, "geo-scene: cannot open outputs in %s\n", out_dir);
        goto done;
    }
    fprintf(mtl, "# VF2 posed scene textures (grayscale luma index). Local.\n");
    fprintf(mtl, "newmtl sheet0\nmap_Kd tex0.png\n");
    fprintf(mtl, "newmtl sheet1\nmap_Kd tex1.png\n");
    fprintf(mtl, "newmtl marker\nKd 1.0 0.0 0.0\n");
    fprintf(csv, "slot,band,shadow,flatness,fixed,anchor,motion_t,Tx,Ty,Tz,"
                 "tpa,oba,rec_byte,tha,tris\n");
    fprintf(obj, "# VF2 posed scene: pose frame %zu, %zu object slots, RAM9 "
                 "Step-2 matrices. Local only, never committed.\n", pose,
            nslots);
    fprintf(obj, "mtllib scene.mtl\n");

    for (s = 0u; s < nslots; ++s) {
        const char *gname = shadow[s] ? "shadow"
                          : (grp[s] == 1) ? "selfmoving" : "cameracommon";
        const char *atag = anchor[s] ? "_akira" : "";
        uint32_t oba = obaf[pose * nslots + s];
        uint32_t rec = (oba & 0x7fffffu) * 4u;     /* MAME geo_object_data:
                                                    * geometry = polygon_rom
                                                    * [oba & mask], byte = *4 */
        /* UV points from tpa, texture header from tha -- directly, as MAME
         * geo_object_data does (model2_v.cpp:1715-1728), not via a tpa->record
         * table lookup. */
        uint32_t uv_ptr = tpaf[pose * nslots + s] & 0x7fffffu;
        uint32_t th_ptr = thaf[pose * nslots + s] & 0x7fffffu;
        size_t tris = 0u;
        float *m = &mat[(pose * nslots + s) * 12u];

        fprintf(obj, "o slot%03zu_%s%s_oba%06x\n", s, gname, atag,
                (unsigned)(oba & 0x7fffffu));

        if ((size_t)rec + 24u <= poly_size) {
            vbase = ext_emit_piece(obj, poly, poly_size, tex, tex_size, rec,
                                   uv_ptr, th_ptr, vbase, &cur_sheet, &tris, m,
                                   NULL, NULL, 0u, NULL, 0u, NULL, NULL, NULL, 0u);
            ++resolved;
        } else {
            fprintf(obj, "usemtl marker\n");
            vbase = gs_emit_box(obj, vbase, (double)m[9], (double)m[10],
                                (double)m[11], 30.0);
            cur_sheet = -1; /* force the next piece to re-emit its sheet */
            ++stub;
        }

        fprintf(csv, "%zu,%s,%d,%.4f,%d,%d,%.4f,%.3f,%.3f,%.3f,0x%x,0x%x,0x%x,"
                     "0x%x,%zu\n",
                s, gname, shadow[s], flat[s], fixed[s], anchor[s], motion_t[s],
                mtx[s*3u], mtx[s*3u+1u], mtx[s*3u+2u],
                (unsigned)(tpaf[pose*nslots+s]),
                (unsigned)(obaf[pose*nslots+s]),
                (unsigned)((obaf[pose*nslots+s] & 0x7fffffu) * 4u),
                (unsigned)(thaf[pose*nslots+s]), tris);
    }

    snprintf(path, sizeof(path), "%s/tex0.png", out_dir);
    (void)ext_tex_write_sheet_png(path, bank[0], bank_size[0]);
    snprintf(path, sizeof(path), "%s/tex1.png", out_dir);
    (void)ext_tex_write_sheet_png(path, bank[1], bank_size[1]);

    /* Report: per-frame recount, group counts, and the measured motion split. */
    printf("geo-scene: %zu frames, slot count per frame:", nframes);
    {
        int stable = 1;
        for (f = 0u; f < nframes; ++f) {
            if (count[f] != nslots) {
                stable = 0;
            }
        }
        printf(" %zu%s\n", nslots, stable ? " (stable across all frames)"
                                           : " (NOT stable -- see below)");
        if (!stable) {
            for (f = 0u; f < nframes; ++f) {
                printf("    frame %zu: %zu objects\n", f, count[f]);
            }
        }
    }
    {
        size_t nfixed = 0u;
        size_t nshadow = 0u;
        size_t ns_cam = 0u;   /* non-shadow camera-common */
        size_t ns_self = 0u;  /* non-shadow self-moving */
        size_t nanchor = 0u;
        size_t anchor_shadow = 0u;
        size_t anchor_self = 0u;
        double anchor_max = 0.0;
        double shadow_flat_max = 0.0;
        double body_flat_min = 2.0;
        for (s = 0u; s < nslots; ++s) {
            if (fixed[s]) {
                ++nfixed;
            }
            if (shadow[s]) {
                ++nshadow;
                if (flat[s] > shadow_flat_max) {
                    shadow_flat_max = flat[s];
                }
            } else {
                if (flat[s] < body_flat_min) {
                    body_flat_min = flat[s];
                }
                if (grp[s] == 1) {
                    ++ns_self;
                } else {
                    ++ns_cam;
                }
            }
            if (anchor[s]) {
                ++nanchor;
                if (shadow[s]) {
                    ++anchor_shadow;
                } else if (grp[s] == 1) {
                    ++anchor_self;
                }
                if (motion_t[s] > anchor_max) {
                    anchor_max = motion_t[s];
                }
            }
        }
        printf("  geometry: from oba, texture header from tha, UV points from tpa "
               "-- all directly, as MAME geo_object_data (no tpa->record lookup)\n");
        printf("  matrices: %zu/%zu slots byte-identical across all frames -> "
               "the camera is in EVERY matrix,\n", nfixed, nslots);
        printf("            so a raw static/moving test cannot split decor from "
               "fighters (Step 3).\n");
        printf("  shadows (3x3 determinant ~ 0, limbs flattened to the ground): "
               "%zu/%zu slots\n", nshadow, nslots);
        printf("            flatness: shadows <= %.4f, every non-shadow slot "
               ">= %.4f (clean gap vs GS_SHADOW_EPS=%.2f)\n",
               shadow_flat_max, (body_flat_min <= 1.5) ? body_flat_min : 0.0,
               (double)GS_SHADOW_EPS);
        printf("  repartition of %zu: shadows=%zu  non-shadow camera-common=%zu "
               " non-shadow self-moving=%zu\n", nslots, nshadow, ns_cam,
               ns_self);
        printf("    (camera-common/self-moving split at motion_t %.4f, the "
               "widest gap; median %.4f)\n", threshold, floor_m);
        printf("  Akira anchor cluster 0x130a44..0x1434a0: %zu slots "
               "(%zu shadow, %zu self-moving limbs, rest ride camera; up to "
               "motion_t %.4f)\n", nanchor, anchor_shadow, anchor_self,
               anchor_max);
    }

    /* Rotation-convention test: Akira's group (anchored, shadows removed),
     * posed as-is and with the 3x3 transposed, each to its own OBJ, plus the
     * mean closest-approach between pieces (articulation gap) for each -- the
     * convention that joins the limbs has the smaller gap. */
    {
        size_t *akslot = (size_t *)malloc(nslots * sizeof(size_t));
        size_t nak = 0u;
        if (akslot != NULL) {
            for (s = 0u; s < nslots; ++s) {
                if (anchor[s] && !shadow[s]) {
                    akslot[nak++] = s;
                }
            }
        }
        if (nak >= 2u) {
            int variant;
            size_t *pbeg = (size_t *)malloc(nak * sizeof(size_t));
            size_t *pend = (size_t *)malloc(nak * sizeof(size_t));
            size_t vcap = 300000u;
            vf2_model_vertex *vbuf =
                (vf2_model_vertex *)malloc(vcap * sizeof(*vbuf));
            if (pbeg == NULL || pend == NULL || vbuf == NULL) {
                free(pbeg); free(pend); free(vbuf); free(akslot);
                goto ak_done;
            }
            for (variant = 0; variant < 2; ++variant) {
                FILE *vo;
                size_t vn = 0u;
                int vsheet = -1;
                size_t vb = 1u;
                size_t i;
                size_t j;
                double gap_sum = 0.0;
                double gap_max = 0.0;
                size_t gap_cnt = 0u;
                snprintf(path, sizeof(path), "%s/akira_%s.obj", out_dir,
                         variant == 0 ? "asis" : "transposed");
                vo = fopen(path, "wb");
                if (vo == NULL) {
                    continue;
                }
                fprintf(vo, "# VF2 Akira group, pose frame %zu, rotation %s. "
                            "Local only.\nmtllib scene.mtl\n", pose,
                        variant == 0 ? "as-is (MAME transform_point)"
                                     : "transposed 3x3");
                for (i = 0u; i < nak; ++i) {
                    size_t slot = akslot[i];
                    uint32_t oba = obaf[pose * nslots + slot];
                    uint32_t rec = (oba & 0x7fffffu) * 4u;
                    /* texture header from tha, UV points from tpa (MAME
                     * geo_object_data), not the tpa->record lookup. */
                    uint32_t uvp = tpaf[pose * nslots + slot] & 0x7fffffu;
                    uint32_t thp = thaf[pose * nslots + slot] & 0x7fffffu;
                    size_t tr = 0u;
                    float mt[12];
                    const float *m = &mat[(pose * nslots + slot) * 12u];
                    pbeg[i] = vn;
                    if ((size_t)rec + 24u <= poly_size) {
                        if (variant == 1) {
                            gs_transpose(m, mt);
                        }
                        fprintf(vo, "o akira_slot%03zu_oba%06x\n", slot,
                                (unsigned)(oba & 0x7fffffu));
                        vb = ext_emit_piece(vo, poly, poly_size, tex, tex_size,
                                            rec, uvp, thp, vb, &vsheet, &tr,
                                            variant == 0 ? m : mt,
                                            vbuf, &vn, vcap, NULL, 0u, NULL,
                                            NULL, NULL, 0u);
                    }
                    pend[i] = vn;
                }
                fclose(vo);
                /* articulation gap: for each piece, the closest distance from
                 * any of its vertices to any vertex of another piece. */
                for (i = 0u; i < nak; ++i) {
                    double best = -1.0;
                    size_t a;
                    if (pend[i] <= pbeg[i]) {
                        continue;
                    }
                    for (a = pbeg[i]; a < pend[i]; ++a) {
                        for (j = 0u; j < nak; ++j) {
                            size_t b;
                            if (j == i) {
                                continue;
                            }
                            for (b = pbeg[j]; b < pend[j]; ++b) {
                                double dx = (double)vbuf[a].x - (double)vbuf[b].x;
                                double dy = (double)vbuf[a].y - (double)vbuf[b].y;
                                double dz = (double)vbuf[a].z - (double)vbuf[b].z;
                                double d = sqrt(dx*dx + dy*dy + dz*dz);
                                if (best < 0.0 || d < best) {
                                    best = d;
                                }
                            }
                        }
                    }
                    if (best >= 0.0) {
                        gap_sum += best;
                        if (best > gap_max) {
                            gap_max = best;
                        }
                        ++gap_cnt;
                    }
                }
                printf("  Akira rotation %-10s: %zu pieces -> %s/akira_%s.obj; "
                       "joint gap mean=%.4f max=%.4f\n",
                       variant == 0 ? "as-is" : "transposed", nak,
                       out_dir, variant == 0 ? "asis" : "transposed",
                       gap_cnt ? gap_sum / (double)gap_cnt : 0.0, gap_max);
            }
            free(pbeg);
            free(pend);
            free(vbuf);
        }
        free(akslot);
    }
ak_done:
    printf("  scene.obj: %zu slots posed (%zu from ROM geometry, %zu position "
           "stubs) -> %s/scene.obj (+ scene.mtl, scene_groups.csv, tex0/1.png)"
           "\n", nslots, resolved, stub, out_dir);
    rc = EXIT_SUCCESS;

done:
    if (bin != NULL) fclose(bin);
    if (txt != NULL) fclose(txt);
    if (obj != NULL) fclose(obj);
    if (mtl != NULL) fclose(mtl);
    if (csv != NULL) fclose(csv);
    free(words);
    free(objs);
    free(main_data);
    free(poly);
    free(tex);
    free(bank[0]);
    free(bank[1]);
    free(mat);
    free(obaf);
    free(tpaf);
    free(thaf);
    free(fixed);
    free(grp);
    free(anchor);
    free(mtx);
    free(motion_t);
    free(sorted);
    return rc;
}

/* --- geo-fighter: Step 3 -- export one fighter (by screen side) in world space --
 *
 * Separates the two fighters by SCREEN POSITION, not record family: record
 * families are shared (family 0x13 holds Akira's parts AND Lau's face 0x130a44;
 * 0x00 primitives split too), so a family sort loses Lau's head/torso and mixes
 * Akira (Step 3 redo). Every non-shadow, resolvable fighter piece (the two
 * decor-matrix records 0x581d50/0x1a9b54 excluded) is projected to its on-screen
 * centroid x (pieces with no vertex on screen are dropped -- they are degenerate /
 * off-screen, not visible fighter parts); the two fighters split at the widest gap
 * in the sorted x (each side >= 5 pieces); `akira` = the left group, `lau` = the
 * right. Each piece's captured matrix has the camera
 * baked in (Step 2); we re-express it in the decor's world frame by applying the
 * inverse of the decor (shared) matrix, so the fighter stands camera-free, then
 * centre on the feet: horizontally on the bbox centre, vertical axis (longest
 * extent) shifted so the lowest point sits at 0. Geometry from oba, textured via
 * the tpa UV/texheader table (as geo-scene). Local only; never committed. */
/* Project a piece's on-screen centroid x in VF2 screen space (centre 248, focus
 * 600, m_crtc_xoffset 0 -- Step 2m/2n) from its captured view-space matrix.
 * Record families are shared between both fighters (e.g. 0x13 holds Akira's parts
 * AND Lau's face 0x130a44), so the fighters must be separated by screen position,
 * not by family (Step 3 redo). Falls back to the matrix translation if no vertex
 * lands on screen; returns -1 if even that is behind the eye. */
static double gf_screen_cx(const uint8_t *poly, size_t poly_size, uint32_t rec,
                           const float *m, vf2_model_vertex *vacc, size_t vcap)
{
    size_t vn = 0u, e, non = 0u;
    int sheet = -1;
    size_t tr = 0u;
    double sum = 0.0;
    (void)ext_emit_piece(NULL, poly, poly_size, NULL, 0u, rec, 0u, 0u, 1u,
                         &sheet, &tr, m, vacc, &vn, vcap, NULL, 0u, NULL,
                         NULL, NULL, 0u);
    for (e = 0u; e < vn; ++e) {
        double X = (double)vacc[e].x * 600.0;
        double Z = (double)vacc[e].z;
        double sx;
        if (Z < 0.0001) continue;
        sx = 248.0 + X / Z;
        if (sx >= 0.0 && sx < 496.0) { sum += sx; ++non; }
    }
    return (non > 0u) ? sum / (double)non : -1.0;
}

/* union-find root with path halving. */
static size_t gf_find(size_t *p, size_t x)
{
    while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; }
    return x;
}

static void gf_store_word(uint8_t *ram, size_t idx_word, uint32_t v)
{
    size_t b = idx_word * 4u;
    ram[b + 0u] = (uint8_t)(v & 0xffu);
    ram[b + 1u] = (uint8_t)((v >> 8) & 0xffu);
    ram[b + 2u] = (uint8_t)((v >> 16) & 0xffu);
    ram[b + 3u] = (uint8_t)((v >> 24) & 0xffu);
}

/* Replay the frame's 0x05 (geo_polygon_data) commands into the two polygon-RAM
 * buffers, exactly as MAME (model2_v.cpp:1886): address, count, then `count`
 * words copied into polygon_ram1 (address & 0x01000000) or polygon_ram0 at word
 * index address & 0x7fff. Each buffer is 0x8000 words = 0x20000 bytes (LE, the
 * same layout ext_emit_piece reads the ROM in). *wmax0/1 = highest word index
 * written, so a RAM object reading past it (not uploaded this frame) is reported. */
static void gf_fill_polyram(const uint32_t *w, size_t nw, uint32_t readstart,
                            uint8_t *ram0, uint8_t *ram1,
                            size_t *wmax0, size_t *wmax1)
{
    size_t ptr = (size_t)((readstart & 0x1ffffu) >> 2);
    size_t opc = 0u;
    *wmax0 = 0u; *wmax1 = 0u;
    while (opc++ < 0x40000u && ptr < nw) {
        uint32_t op = w[ptr++];
        uint32_t cmd;
        size_t need = 0u;
        if (op & 0x80000000u) { ptr = (size_t)((op & 0x1ffffu) >> 2); continue; }
        cmd = (op >> 23) & 0x1fu;
        switch (cmd) {
        case 0x00u: need = 0u; break;
        case 0x07u: case 0x17u: case 0x08u: case 0x18u:
        case 0x10u: case 0x16u: case 0x1eu: need = 1u; break;
        case 0x09u: case 0x19u: case 0x0du: need = 2u; break;
        case 0x0au: case 0x1au: case 0x0cu: case 0x1cu: need = 3u; break;
        case 0x03u: case 0x13u: need = 6u; break;
        case 0x0bu: case 0x1bu: need = 12u; break;
        case 0x01u: case 0x11u: need = 4u; break;
        case 0x04u: case 0x14u: case 0x05u: case 0x15u:
            if (ptr + 2u > nw) return;
            need = 2u + (size_t)w[ptr + 1u]; break;
        case 0x06u:
            if (ptr + 2u > nw) return;
            need = 2u + (size_t)w[ptr + 1u] * 2u; break;
        case 0x1du:
            if (ptr + 2u > nw) return;
            need = 2u + (size_t)w[ptr + 1u] * 3u; break;
        case 0x02u: case 0x12u: {
            size_t p = ptr + 8u;
            for (;;) {
                uint32_t attr;
                if (p >= nw) return;
                attr = w[p++];
                if ((attr & 3u) == 0u) break;
                p += 5u;
                if (attr & 1u) p += 3u;
            }
            need = p - ptr; break;
        }
        case 0x0eu:
            if (ptr + 33u > nw) return;
            need = 32u + 1u + (size_t)w[ptr + 32u] * 3u; break;
        case 0x0fu: case 0x1fu: return;
        default: return;
        }
        if (ptr + need > nw) return;
        if (cmd == 0x05u || cmd == 0x15u) {
            uint32_t address = w[ptr];
            uint32_t count = w[ptr + 1u];
            uint8_t *ram = (address & 0x01000000u) ? ram1 : ram0;
            size_t *wm = (address & 0x01000000u) ? wmax1 : wmax0;
            size_t idx = (size_t)(address & 0x7fffu);
            size_t i;
            for (i = 0u; i < count && (idx + i) < 0x8000u; ++i)
                gf_store_word(ram, idx + i, w[ptr + 2u + i]);
            if (idx + count > *wm) *wm = idx + count;
        }
        ptr += need;
    }
}

static int command_geo_fighter(
    const char *bin_path, const char *txt_path, const char *rom_directory,
    const char *tex0_path, const char *tex1_path, const char *pose_str,
    const char *which, const char *out_dir, const char *colour_dir,
    const char *texparams_path, int stage_mode, int from_rom
)
{
    const size_t frame_bytes = 131072u;
    const size_t frame_words = frame_bytes / 4u;
    const size_t sheet_rgb = 2048u * 1024u * 3u;
    FILE *bin = NULL, *txt = NULL, *obj = NULL, *mtl = NULL;
    uint8_t *pal = NULL, *cxlat = NULL, *lumacap = NULL, *luma = NULL;
    uint8_t *csheet[2] = { NULL, NULL };
    uint8_t *asheet[2] = { NULL, NULL };
    size_t pal_n = 0u, cxlat_n = 0u, luma_n = 0u;
    int colour = (colour_dir != NULL);
    int gf_lit = (colour_dir != NULL && texparams_path != NULL);
    char line[256];
    char path[1100];
    uint32_t *words = NULL;
    vf2_geo_object *objs = NULL;
    size_t max_objs = 4096u;
    uint8_t *main_data = NULL, *poly = NULL, *tex = NULL;
    uint8_t *bank[2] = { NULL, NULL };
    size_t main_size = 0u, poly_size = 0u, tex_size = 0u;
    size_t bank_size[2] = { 0u, 0u };
    uint32_t readstart[GS_MAX_FRAMES];
    size_t nframes = 0u, pose, nobjs = 0u, s;
    vf2_geo_stop stop;
    int side;                    /* 0 = left (P1), 1 = right (P2) */
    char base[64];               /* `which` without a trailing -l/-r, for filenames */
    float mdec[12];
    double adi[9];
    int have_dec = 0;
    float (*mf)[12] = NULL;       /* per-candidate world matrix (camera removed) */
    float (*cand_vmat)[12] = NULL; /* per-candidate view-space matrix (for lighting) */
    float gf_light[3] = { 0.0f, 0.0f, 1.0f };
    uint8_t gf_diff[32], gf_amb[32], gf_sscale[32], gf_sctrl[32];
    uint32_t gf_mode = 0u;
    uint32_t used_spec[2] = { 0u, 0u };  /* per sheet, bit tpi set = chrome material used */
    gf_tile gf_tiles[64]; size_t ngf_tiles = 0u;   /* repeat-UV tiles emitted as standalone images */
    int gf_tile_hasalpha[64];                      /* per tile: 1 if it has a transparent texel */
    size_t cap_objs = 0u;                /* candidate-array capacity (nobjs, + room for --from-rom) */
    int *is_recovered = NULL;            /* per candidate: 1 = added from ROM (not in the capture) */
    size_t nrec_rom = 0u;                /* count of ROM-recovered objects emitted */
    uint32_t *cand_rec = NULL, *cand_tpa = NULL, *cand_tha = NULL;
    size_t *cand_s = NULL, *pstart = NULL, *pcount = NULL;
    size_t *parent = NULL, *compsz = NULL, *sel = NULL;
    int *grp2 = NULL;
    double *cand_scx = NULL, *dmat = NULL;
    float *worldv = NULL;
    uint8_t *ram0 = NULL, *ram1 = NULL;
    const uint8_t **cand_pbuf = NULL;
    size_t *cand_psize = NULL;
    size_t wmax0 = 0u, wmax1 = 0u, nram_unwritten = 0u;
    size_t ncand = 0u, i, j, k, wvn = 0u, wvcap;
    size_t ca = 0u, cb = 0u;      /* the two largest components' roots */
    double bestT = 0.05, best2nd = -1.0, Tt;
    size_t nsel = 0u, nram = 0u;
    vf2_model_vertex *vacc = NULL;
    size_t vcap = 400000u, e;
    double mn[3] = { 1e30, 1e30, 1e30 }, mx[3] = { -1e30, -1e30, -1e30 };
    double off[3] = { 0.0, 0.0, 0.0 };
    int vax = 1;
    size_t vbase = 1u, tris_total = 0u;
    int cur_sheet = -1, sheet = -1;
    size_t tr = 0u;
    vf2_status st;
    int rc = EXIT_FAILURE;

    /* `which` names the fighter and, by screen side, which connected-component
     * group to keep: P1 is on the left, the opponent on the right. In the
     * captures so far P1 is the named character (akira, jacky, ...) and the
     * right-hand fighter is Lau, so lau -> right, anything else -> left. A
     * right-side subject would append "-r". */
    if (stage_mode) {
        /* geo-stage: no fighter, no side -- export the decor set as "stage". */
        memcpy(base, "stage", 6u); side = 0;
    } else {
        size_t wl = strlen(which);
        int suff = (wl >= 2u && which[wl - 2u] == '-' &&
                    (which[wl-1u]=='l'||which[wl-1u]=='L'||which[wl-1u]=='r'||which[wl-1u]=='R'));
        if (suff) side = (which[wl-1u]=='r'||which[wl-1u]=='R') ? 1 : 0;
        else side = (strcmp(which, "lau") == 0) ? 1 : 0;
        if (suff) wl -= 2u;
        if (wl >= sizeof(base)) wl = sizeof(base) - 1u;
        memcpy(base, which, wl); base[wl] = '\0';
    }

    pose = (size_t)strtoul(pose_str, NULL, 0);
    { size_t ti; for (ti = 0u; ti < 32u; ++ti) { gf_diff[ti] = 255u; gf_amb[ti] = 0u; gf_sscale[ti] = 0u; gf_sctrl[ti] = 0u; } }
    if (gf_lit) gc_load_texparams(texparams_path, gf_diff, gf_amb, gf_sscale, gf_sctrl);
    txt = fopen(txt_path, "rb");
    if (txt == NULL) { fprintf(stderr, "geo-fighter: cannot open %s\n", txt_path); return EXIT_FAILURE; }
    while (fgets(line, (int)sizeof(line), txt) != NULL) {
        unsigned idx, frameno, rs;
        if (sscanf(line, "F %u %u %x", &idx, &frameno, &rs) == 3 && idx < GS_MAX_FRAMES) {
            readstart[idx] = rs;
            if ((size_t)idx + 1u > nframes) nframes = (size_t)idx + 1u;
        }
    }
    fclose(txt); txt = NULL;
    if (pose >= nframes) { fprintf(stderr, "geo-fighter: pose %zu >= %zu\n", pose, nframes); return EXIT_FAILURE; }

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_size);
    if (st == VF2_OK) st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly, &poly_size);
    if (st == VF2_OK) st = vf2_romset_build_region(rom_directory, VF2_REGION_TEXTURES, &tex, &tex_size);
    if (st != VF2_OK) { fprintf(stderr, "geo-fighter: ROM regions: %s\n", vf2_status_string(st)); goto done; }
    if (!ext_tex_load_file(tex0_path, &bank[0], &bank_size[0]) ||
        !ext_tex_load_file(tex1_path, &bank[1], &bank_size[1])) {
        fprintf(stderr, "geo-fighter: cannot read texture banks\n"); goto done;
    }
    if (colour) {
        /* colour-capture dir supplies palette / colorxlat / luma RAM (same regions
         * and formats as geo-colour); build the per-bank coloured (albedo) sheets. */
        snprintf(path, sizeof(path), "%s/palette_01800000.bin", colour_dir);
        if (!ext_tex_load_file(path, &pal, &pal_n)) { fprintf(stderr, "geo-fighter: %s\n", path); goto done; }
        snprintf(path, sizeof(path), "%s/colorxlat_01810000.bin", colour_dir);
        if (!ext_tex_load_file(path, &cxlat, &cxlat_n)) { fprintf(stderr, "geo-fighter: %s\n", path); goto done; }
        snprintf(path, sizeof(path), "%s/luma_12800000.bin", colour_dir);
        if (!ext_tex_load_file(path, &lumacap, &luma_n)) { fprintf(stderr, "geo-fighter: %s\n", path); goto done; }
        luma = (uint8_t *)malloc(0x8000u);
        csheet[0] = (uint8_t *)calloc(sheet_rgb, 1u);
        csheet[1] = (uint8_t *)calloc(sheet_rgb, 1u);
        /* alpha planes: 0 (transparent) everywhere; painted texels set 0xff, a
         * translucent polygon's cutout texels (0x0f) stay transparent. */
        asheet[0] = (uint8_t *)calloc(2048u * 1024u, 1u);
        asheet[1] = (uint8_t *)calloc(2048u * 1024u, 1u);
        if (luma == NULL || csheet[0] == NULL || csheet[1] == NULL ||
            asheet[0] == NULL || asheet[1] == NULL) { fprintf(stderr, "geo-fighter: alloc colour\n"); goto done; }
        for (i = 0u; i < 0x8000u; ++i) luma[i] = (i * 4u < luma_n) ? lumacap[i * 4u] : 0u;
    }

    bin = fopen(bin_path, "rb");
    words = (uint32_t *)malloc(frame_words * sizeof(*words));
    objs = (vf2_geo_object *)malloc(max_objs * sizeof(*objs));
    vacc = (vf2_model_vertex *)malloc(vcap * sizeof(*vacc));
    ram0 = (uint8_t *)calloc(0x20000u, 1u);
    ram1 = (uint8_t *)calloc(0x20000u, 1u);
    if (bin == NULL || words == NULL || objs == NULL || vacc == NULL ||
        ram0 == NULL || ram1 == NULL) { fprintf(stderr, "geo-fighter: alloc\n"); goto done; }
    if (!gs_read_frame(bin, pose, frame_bytes, words, frame_words) ||
        vf2_geo_walk(words, frame_words, readstart[pose], objs, max_objs, &nobjs, &stop, NULL) != VF2_OK) {
        fprintf(stderr, "geo-fighter: frame %zu walk failed\n", pose); goto done;
    }
    /* replay the frame's 0x05 commands to reconstruct the polygon RAM, so the
     * RAM-sourced pieces (Akira's shin 0x000010 etc.) can be decoded like MAME. */
    gf_fill_polyram(words, frame_words, readstart[pose], ram0, ram1,
                    &wmax0, &wmax1);

    /* for coloured lighting, recover the frame's light vector (0x0a) and geo mode
     * (0x07) -- the same per-polygon luma the scene render uses. */
    if (gf_lit) {
        size_t p = (size_t)((readstart[pose] & 0x1ffffu) >> 2), oc = 0u;
        for (;;) {
            uint32_t op, cmd; size_t need = 0u;
            if (oc++ >= 0x40000u || p >= frame_words) break;
            op = words[p++];
            if (op & 0x80000000u) { p = (size_t)((op & 0x1ffffu) >> 2); continue; }
            cmd = (op >> 23) & 0x1fu;
            switch (cmd) {
            case 0x00u: need = 0u; break;
            case 0x07u: case 0x17u: case 0x08u: case 0x18u:
            case 0x10u: case 0x16u: case 0x1eu: need = 1u; break;
            case 0x09u: case 0x19u: case 0x0du: need = 2u; break;
            case 0x0au: case 0x1au: case 0x0cu: case 0x1cu: need = 3u; break;
            case 0x03u: case 0x13u: need = 6u; break;
            case 0x0bu: case 0x1bu: need = 12u; break;
            case 0x01u: case 0x11u: need = 4u; break;
            case 0x04u: case 0x14u: case 0x05u: case 0x15u:
                if (p + 2u > frame_words) goto scan_done;
                need = 2u + (size_t)words[p + 1u]; break;
            case 0x06u:
                if (p + 2u > frame_words) goto scan_done;
                need = 2u + (size_t)words[p + 1u] * 2u; break;
            case 0x1du:
                if (p + 2u > frame_words) goto scan_done;
                need = 2u + (size_t)words[p + 1u] * 3u; break;
            case 0x02u: case 0x12u: {
                size_t q = p + 8u;
                for (;;) { uint32_t a; if (q >= frame_words) goto scan_done; a = words[q++]; if ((a & 3u) == 0u) break; q += 5u; if (a & 1u) q += 3u; }
                need = q - p; break;
            }
            case 0x0eu:
                if (p + 33u > frame_words) goto scan_done;
                need = 32u + 1u + (size_t)words[p + 32u] * 3u; break;
            case 0x0fu: case 0x1fu: goto scan_done;
            default: goto scan_done;
            }
            if (p + need > frame_words) goto scan_done;
            if (cmd == 0x07u || cmd == 0x17u) gf_mode = words[p];
            else if (cmd == 0x0au || cmd == 0x1au) {
                uint32_t u; u = words[p]; memcpy(&gf_light[0], &u, 4u);
                u = words[p + 1u]; memcpy(&gf_light[1], &u, 4u);
                u = words[p + 2u]; memcpy(&gf_light[2], &u, 4u);
            }
            p += need;
        }
    scan_done: ;
    }

    /* decor (shared) matrix -> camera. Fighter mode: the stone-towers decor is the
     * ROM-record range 0x3c1fac..0x3ca730 (unchanged). Stage mode: that range is
     * specific to one arena, so instead find the matrix the MOST non-shadow objects
     * share -- the decor objects all carry the camera matrix, while each fighter limb
     * has its own, so the shared one is the camera/decor matrix for ANY stage. */
    if (stage_mode) {
        size_t best_s = nobjs, best_cnt = 0u;
        for (s = 0u; s < nobjs; ++s) {
            uint32_t rec_s = (objs[s].oba & 0x7fffffu) * 4u;
            size_t t, cnt = 0u;
            if (gs_flatness(objs[s].matrix) < GS_SHADOW_EPS) continue;   /* shadow */
            if (rec_s == 0x581d50u || rec_s == 0x1a9b54u) continue;      /* bad */
            for (t = 0u; t < nobjs; ++t) {
                int kk; double d = 0.0;
                for (kk = 0; kk < 12; ++kk)
                    d += fabs((double)objs[s].matrix[kk] - (double)objs[t].matrix[kk]);
                if (d < 1e-3) ++cnt;
            }
            if (cnt > best_cnt) { best_cnt = cnt; best_s = s; }
        }
        if (best_s != nobjs) { memcpy(mdec, objs[best_s].matrix, sizeof(mdec)); have_dec = 1; }
    } else {
        for (s = 0u; s < nobjs; ++s) {
            uint32_t rec = (objs[s].oba & 0x7fffffu) * 4u;
            if (rec >= 0x3c1facu && rec <= 0x3ca730u) {
                memcpy(mdec, objs[s].matrix, sizeof(mdec));
                have_dec = 1; break;
            }
        }
    }
    if (!have_dec || !gs_mat3_inv(mdec, adi)) {
        fprintf(stderr, "geo-fighter: no invertible decor matrix\n"); goto done;
    }

    /* --from-rom adds the frustum-culled decor (model-ID range) back as extra
     * candidates, so the arrays need room beyond the walked object count. */
    cap_objs = nobjs + (from_rom ? 4096u : 0u);
    mf = (float (*)[12])malloc(cap_objs * sizeof(*mf));
    cand_vmat = (float (*)[12])malloc(cap_objs * sizeof(*cand_vmat));
    cand_rec = (uint32_t *)malloc(cap_objs * sizeof(*cand_rec));
    cand_tpa = (uint32_t *)malloc(cap_objs * sizeof(*cand_tpa));
    cand_tha = (uint32_t *)malloc(cap_objs * sizeof(*cand_tha));
    cand_s = (size_t *)malloc(cap_objs * sizeof(*cand_s));
    pstart = (size_t *)malloc(cap_objs * sizeof(*pstart));
    pcount = (size_t *)malloc(cap_objs * sizeof(*pcount));
    cand_scx = (double *)malloc(cap_objs * sizeof(*cand_scx));
    sel = (size_t *)malloc(cap_objs * sizeof(*sel));
    cand_pbuf = (const uint8_t **)malloc(cap_objs * sizeof(*cand_pbuf));
    cand_psize = (size_t *)malloc(cap_objs * sizeof(*cand_psize));
    is_recovered = (int *)calloc(cap_objs, sizeof(*is_recovered));
    wvcap = vcap;
    worldv = (float *)malloc(wvcap * 3u * sizeof(*worldv));
    if (mf == NULL || cand_vmat == NULL || cand_rec == NULL ||
        cand_tpa == NULL || cand_tha == NULL || cand_s == NULL || pstart == NULL || pcount == NULL ||
        cand_scx == NULL || sel == NULL || worldv == NULL || is_recovered == NULL ||
        cand_pbuf == NULL || cand_psize == NULL)
        goto done;

    /* every non-shadow, resolvable fighter piece (the 2 decor-matrix records out):
     * decode its WORLD vertices (camera removed) and record its on-screen x. */
    for (s = 0u; s < nobjs; ++s) {
        uint32_t oba = objs[s].oba;
        const uint8_t *pbuf;
        size_t psize, vn = 0u;
        uint32_t rec;
        int is_rom;
        /* MAME geo_object_data source: bit24 = fast polygon RAM, bit23 = polygon
         * ROM, else slow polygon RAM. ROM reads the static ROM at (oba&0x7fffff)*4;
         * RAM reads the frame's reconstructed polygon RAM at (oba&0x7fff)*4. */
        if (oba & 0x01000000u) { pbuf = ram1; psize = 0x20000u; rec = (oba & 0x7fffu) * 4u; is_rom = 0; }
        else if (oba & 0x00800000u) { pbuf = poly; psize = poly_size; rec = (oba & 0x7fffffu) * 4u; is_rom = 1; }
        else { pbuf = ram0; psize = 0x20000u; rec = (oba & 0x7fffu) * 4u; is_rom = 0; }
        if (is_rom && (rec == 0x581d50u || rec == 0x1a9b54u)) continue;  /* bad */
        if (gs_flatness(objs[s].matrix) < GS_SHADOW_EPS) continue;       /* shadow */
        if (stage_mode) {
            /* geo-stage: KEEP the decor set = ROM objects carrying the shared camera
             * matrix (the fighters' per-limb matrices differ), the inverse of the
             * fighter filter, deduped by record. Matrix-based so it works on any
             * stage, not just the stone-towers record range. */
            size_t q; int seen = 0, kk; double d = 0.0;
            if (!is_rom) continue;
            for (kk = 0; kk < 12; ++kk)
                d += fabs((double)objs[s].matrix[kk] - (double)mdec[kk]);
            if (d >= 1e-3) continue;   /* not a shared-matrix (decor) object */
            for (q = 0u; q < ncand; ++q) if (cand_rec[q] == rec) { seen = 1; break; }
            if (seen) continue;
            goto emit_cand;
        }
        if (is_rom && rec >= 0x3c1facu && rec <= 0x3ca730u) continue;    /* decor */
        if (!is_rom) {
            /* RAM object: was its word index uploaded by a 0x05 this frame? */
            size_t widx = (size_t)(oba & 0x7fffu);
            size_t wm = (oba & 0x01000000u) ? wmax1 : wmax0;
            if (widx >= wm) { ++nram_unwritten; continue; }
        }
    emit_cand:
        if ((size_t)rec + 24u > psize) continue;
        gs_worldframe(objs[s].matrix, mdec, adi, mf[ncand]);
        memcpy(cand_vmat[ncand], objs[s].matrix, sizeof(cand_vmat[ncand]));
        (void)ext_emit_piece(NULL, pbuf, psize, NULL, 0u, rec, 0u, 0u, 1u,
                             &sheet, &tr, mf[ncand], vacc, &vn, vcap, NULL, 0u, NULL,
                             NULL, NULL, 0u);
        if (vn == 0u) continue;
        if (!is_rom) ++nram;
        pstart[ncand] = wvn;
        for (e = 0u; e < vn && wvn < wvcap; ++e) {
            worldv[wvn * 3u + 0u] = vacc[e].x;
            worldv[wvn * 3u + 1u] = vacc[e].y;
            worldv[wvn * 3u + 2u] = vacc[e].z;
            ++wvn;
        }
        pcount[ncand] = wvn - pstart[ncand];
        cand_scx[ncand] = gf_screen_cx(pbuf, psize, rec, objs[s].matrix,
                                       vacc, vcap);
        cand_rec[ncand] = rec;
        cand_tpa[ncand] = objs[s].tpa;
        cand_tha[ncand] = objs[s].tha;
        cand_s[ncand] = s;
        cand_pbuf[ncand] = pbuf;
        cand_psize[ncand] = psize;
        ++ncand;
    }
    if (ncand == 0u) {
        fprintf(stderr, "geo-%s: no %s pieces\n",
                stage_mode ? "stage" : "fighter", stage_mode ? "decor" : "fighter");
        goto done;
    }

    if (stage_mode) {
        /* --from-rom: the captured decor is a frustum-culled subset of a
         * contiguous model-ID range (note stage_model_table_020e0004). Map each
         * captured record back to its ID via the model table (MAIN_DATA +0xe0004,
         * 16-byte entries {tpa,tha,oba,obc}), take the ID range the capture spans,
         * and add every valid ID in it that the capture lacks -- the culled decor.
         * Recovered objects carry the camera (decor) matrix, so gs_worldframe maps
         * them into the same camera-removed world frame as the captured decor. */
        if (from_rom && main_data != NULL) {
            /* Reverse-map each captured record to its model ID, then group the IDs
             * into clusters (consecutive gap <= GF_IDGAP). The main decor is one
             * dense cluster; a handful of far-off captured IDs (shadows/effects
             * that share the camera matrix) form their own tiny clusters and are
             * not expanded. For every cluster we fill its [lo,hi] ID range with the
             * valid IDs the capture lacks -- the frustum-culled decor. */
            const uint32_t GF_IDGAP = 64u;
            size_t ncap = ncand, nids = 0u, ci;
            uint32_t *cap_ids = (uint32_t *)malloc((ncap + 1u) * sizeof(uint32_t));
            if (cap_ids != NULL) {
                uint32_t cid;
                for (i = 0u; i < ncap; ++i) {
                    for (cid = 0u; cid < 0x2000u; ++cid) {
                        uint32_t oba = tl_u32(main_data, main_size,
                                              0xe0004u + cid * 16u + 8u);
                        if ((oba & 0x800000u) != 0u &&
                            ((oba & 0x7fffffu) * 4u) == cand_rec[i]) {
                            cap_ids[nids++] = cid; break;
                        }
                    }
                }
                /* insertion sort (nids is small) */
                for (i = 1u; i < nids; ++i) {
                    uint32_t v = cap_ids[i]; size_t j2 = i;
                    while (j2 > 0u && cap_ids[j2 - 1u] > v) { cap_ids[j2] = cap_ids[j2 - 1u]; --j2; }
                    cap_ids[j2] = v;
                }
                ci = 0u;
                while (ci < nids && ncand < cap_objs) {
                    uint32_t lo = cap_ids[ci], hi = cap_ids[ci], id;
                    size_t cj = ci + 1u;
                    while (cj < nids && cap_ids[cj] - hi <= GF_IDGAP) { hi = cap_ids[cj]; ++cj; }
                    for (id = lo; id <= hi && ncand < cap_objs; ++id) {
                        uint32_t oba = tl_u32(main_data, main_size, 0xe0004u + id * 16u + 8u);
                        uint32_t rec, tpa, tha; size_t q, vn = 0u; int seen = 0;
                        if ((oba & 0x800000u) == 0u) continue;
                        rec = (oba & 0x7fffffu) * 4u;
                        if (rec == 0u || (size_t)rec + 24u > poly_size) continue;
                        for (q = 0u; q < ncand; ++q) if (cand_rec[q] == rec) { seen = 1; break; }
                        if (seen) continue;
                        tpa = tl_u32(main_data, main_size, 0xe0004u + id * 16u) & 0x7fffffu;
                        tha = tl_u32(main_data, main_size, 0xe0004u + id * 16u + 4u) & 0x7fffffu;
                        gs_worldframe(mdec, mdec, adi, mf[ncand]);
                        memcpy(cand_vmat[ncand], mdec, sizeof(mdec));
                        (void)ext_emit_piece(NULL, poly, poly_size, NULL, 0u, rec, 0u, 0u,
                                             1u, &sheet, &tr, mf[ncand], vacc, &vn, vcap,
                                             NULL, 0u, NULL, NULL, NULL, 0u);
                        if (vn == 0u) continue;
                        pstart[ncand] = wvn;
                        for (e = 0u; e < vn && wvn < wvcap; ++e) {
                            worldv[wvn * 3u + 0u] = vacc[e].x;
                            worldv[wvn * 3u + 1u] = vacc[e].y;
                            worldv[wvn * 3u + 2u] = vacc[e].z;
                            ++wvn;
                        }
                        pcount[ncand] = wvn - pstart[ncand];
                        cand_rec[ncand] = rec; cand_tpa[ncand] = tpa; cand_tha[ncand] = tha;
                        cand_pbuf[ncand] = poly; cand_psize[ncand] = poly_size;
                        is_recovered[ncand] = 1; ++nrec_rom;
                        ++ncand;
                    }
                    ci = cj;
                }
                free(cap_ids);
            }
        }
        /* no fighters to separate: every decor object is kept, in its own
         * camera-removed world frame (the same frame in every capture). */
        for (i = 0u; i < ncand; ++i) sel[nsel++] = i;
        if (getenv("VF2_CASE_CHECK") != NULL) {
            size_t ndet = 0u, ci;
            for (ci = 0u; ci < ncand; ++ci) {
                const float *m = cand_vmat[ci];
                double det = (double)m[0]*((double)m[4]*m[8]-(double)m[5]*m[7])
                           - (double)m[1]*((double)m[3]*m[8]-(double)m[5]*m[6])
                           + (double)m[2]*((double)m[3]*m[7]-(double)m[4]*m[6]);
                if (det < 0.0) { ++ndet;
                    fprintf(stderr, "CASE det<0 (reflection) rec=0x%06x det=%+.3f\n",
                            cand_rec[ci], det); }
            }
            fprintf(stderr, "CASES: %zu decor objects, det<0=%zu\n", ncand, ndet);
        }
        goto stage_selected;
    }

    /* pairwise minimum vertex distance between pieces (world space). */
    dmat = (double *)malloc(ncand * ncand * sizeof(*dmat));
    parent = (size_t *)malloc(ncand * sizeof(*parent));
    compsz = (size_t *)malloc(ncand * sizeof(*compsz));
    grp2 = (int *)malloc(ncand * sizeof(*grp2));
    if (dmat == NULL || parent == NULL || compsz == NULL || grp2 == NULL)
        goto done;
    for (i = 0u; i < ncand; ++i) {
        dmat[i * ncand + i] = 0.0;
        for (j = i + 1u; j < ncand; ++j) {
            double best = 1e30;
            size_t a, b;
            for (a = 0u; a < pcount[i]; ++a) {
                const float *va = &worldv[(pstart[i] + a) * 3u];
                for (b = 0u; b < pcount[j]; ++b) {
                    const float *vb = &worldv[(pstart[j] + b) * 3u];
                    double dx = (double)va[0] - vb[0];
                    double dy = (double)va[1] - vb[1];
                    double dz = (double)va[2] - vb[2];
                    double d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 < best) best = d2;
                }
            }
            dmat[i * ncand + j] = dmat[j * ncand + i] = sqrt(best);
        }
    }

    /* Connected components: two pieces of the same fighter touch at a joint
     * (small min vertex distance), while the two fighters stay separated. Sweep
     * the joint threshold and keep the one whose 2nd-largest component is biggest
     * -- both fighters whole and not merged (a too-small T fragments a fighter, a
     * too-large T fuses the two). */
    for (Tt = 0.02; Tt <= 0.6001; Tt += 0.02) {
        size_t b1 = 0u, b2 = 0u;
        for (i = 0u; i < ncand; ++i) parent[i] = i;
        for (i = 0u; i < ncand; ++i)
            for (j = i + 1u; j < ncand; ++j)
                if (dmat[i * ncand + j] <= Tt) {
                    size_t ri = gf_find(parent, i), rj = gf_find(parent, j);
                    if (ri != rj) parent[ri] = rj;
                }
        for (i = 0u; i < ncand; ++i) compsz[i] = 0u;
        for (i = 0u; i < ncand; ++i) compsz[gf_find(parent, i)]++;
        for (i = 0u; i < ncand; ++i) {
            if (compsz[i] > b1) { b2 = b1; b1 = compsz[i]; }
            else if (compsz[i] > b2) { b2 = compsz[i]; }
        }
        if ((double)b2 > best2nd) { best2nd = (double)b2; bestT = Tt; }
    }

    /* final components at bestT */
    for (i = 0u; i < ncand; ++i) parent[i] = i;
    for (i = 0u; i < ncand; ++i)
        for (j = i + 1u; j < ncand; ++j)
            if (dmat[i * ncand + j] <= bestT) {
                size_t ri = gf_find(parent, i), rj = gf_find(parent, j);
                if (ri != rj) parent[ri] = rj;
            }
    for (i = 0u; i < ncand; ++i) compsz[i] = 0u;
    for (i = 0u; i < ncand; ++i) compsz[gf_find(parent, i)]++;
    /* two largest components = the two fighters */
    {
        size_t na = 0u, nb = 0u;
        ca = cb = ncand;
        for (i = 0u; i < ncand; ++i) {
            if (compsz[i] == 0u) continue;
            if (compsz[i] > na) { cb = ca; nb = na; ca = i; na = compsz[i]; }
            else if (compsz[i] > nb) { cb = i; nb = compsz[i]; }
        }
    }
    if (ca == ncand || cb == ncand) { fprintf(stderr, "geo-fighter: <2 fighter components\n"); goto done; }
    /* Assign every piece to the nearer of the two cores. A crouched fighter's
     * lower legs can sit as far from his own torso as from the opponent (a wide
     * knee gap at f20), so they form their own small component; attaching each
     * orphan to the nearer core puts them back on the right fighter. Pieces far
     * from both cores (> GF_ATTACH_CAP) are dropped as garbage/off-frame. */
    for (i = 0u; i < ncand; ++i) {
        size_t ri = gf_find(parent, i), m, n;
        double dA, dB;
        if (ri == ca) { grp2[i] = 0; continue; }
        if (ri == cb) { grp2[i] = 1; continue; }
        /* orphan: nearest core for the WHOLE component (keep a limb's pieces
         * together -- e.g. the front foot that reaches toward the opponent). */
        dA = 1e30; dB = 1e30;
        for (m = 0u; m < ncand; ++m) {
            if (gf_find(parent, m) != ri) continue;
            for (n = 0u; n < ncand; ++n) {
                size_t rn = gf_find(parent, n);
                double d = dmat[m * ncand + n];
                if (rn == ca && d < dA) dA = d;
                else if (rn == cb && d < dB) dB = d;
            }
        }
        if (dA > 0.5 && dB > 0.5) grp2[i] = -1;           /* garbage/off-frame */
        else grp2[i] = (dA <= dB) ? 0 : 1;
    }

    /* label the two groups by mean on-screen x: the left one is akira. */
    {
        double sa = 0.0, sb = 0.0;
        size_t na = 0u, nb = 0u;
        int akira_grp, want_grp;
        for (i = 0u; i < ncand; ++i) {
            if (cand_scx[i] < 0.0) continue;
            if (grp2[i] == 0) { sa += cand_scx[i]; ++na; }
            else if (grp2[i] == 1) { sb += cand_scx[i]; ++nb; }
        }
        sa = na ? sa / (double)na : 1e9;
        sb = nb ? sb / (double)nb : 1e9;
        akira_grp = (sa <= sb) ? 0 : 1;
        want_grp = (side == 0) ? akira_grp : 1 - akira_grp;
        for (i = 0u; i < ncand; ++i)
            if (grp2[i] == want_grp) sel[nsel++] = i;
        if (getenv("VF2_CASE_CHECK") != NULL) {
            size_t ndet = 0u, nun = 0u, ci;
            for (ci = 0u; ci < ncand; ++ci) {
                const float *m = cand_vmat[ci];
                double det = (double)m[0]*((double)m[4]*m[8]-(double)m[5]*m[7])
                           - (double)m[1]*((double)m[3]*m[8]-(double)m[5]*m[6])
                           + (double)m[2]*((double)m[3]*m[7]-(double)m[4]*m[6]);
                if (det < 0.0) { ++ndet;
                    if (grp2[ci] == want_grp)
                        fprintf(stderr, "CASE det<0 (reflection) rec=0x%06x det=%+.3f\n", cand_rec[ci], det); }
                if (grp2[ci] != 0 && grp2[ci] != 1) ++nun;
            }
            fprintf(stderr, "CASES: %zu candidates, det<0=%zu, unassigned=%zu\n", ncand, ndet, nun);
        }
    }
    if (nsel == 0u) { fprintf(stderr, "geo-fighter: no %s pieces at frame %zu\n", which, pose); goto done; }

stage_selected:
    /* pass 1: world-space bbox (for feet centring; also reported for the stage) */
    for (k = 0u; k < nsel; ++k) {
        size_t idx = sel[k], a;
        for (a = 0u; a < pcount[idx]; ++a) {
            const float *v = &worldv[(pstart[idx] + a) * 3u];
            int kk;
            for (kk = 0; kk < 3; ++kk) {
                if ((double)v[kk] < mn[kk]) mn[kk] = (double)v[kk];
                if ((double)v[kk] > mx[kk]) mx[kk] = (double)v[kk];
            }
        }
    }
    /* vertical axis = longest bbox extent; feet at that axis' minimum */
    {
        double ex0 = mx[0] - mn[0], ex1 = mx[1] - mn[1], ex2 = mx[2] - mn[2];
        vax = (ex1 >= ex0 && ex1 >= ex2) ? 1 : (ex0 >= ex2 ? 0 : 2);
    }
    if (stage_mode) {
        /* keep the shared camera-removed world frame -- do NOT recentre, so the
         * same decor record lands at the same coordinates in every capture and
         * the per-capture stages merge by record. */
        off[0] = off[1] = off[2] = 0.0;
    } else {
        off[0] = 0.5 * (mn[0] + mx[0]);
        off[1] = 0.5 * (mn[1] + mx[1]);
        off[2] = 0.5 * (mn[2] + mx[2]);
        off[vax] = mn[vax];
    }

    /* pass 2: write textured OBJ in the centred world frame. The MTL is written
     * after the mesh (below) so it can declare a metallic material for each
     * strongly-specular (chrome) material the mesh actually used, collected in
     * `used_spec` (bit tpi per sheet). */
    snprintf(path, sizeof(path), "%s/%s.obj", out_dir, base);
    obj = fopen(path, "wb");
    if (obj == NULL) { fprintf(stderr, "geo-fighter: cannot write %s\n", path); goto done; }
    fprintf(obj, "# VF2 %s, pose frame %zu, camera removed (decor matrix inverse), "
                 "feet-centred. %zu pieces. Local only, never committed.\n",
            base, pose, nsel);
    fprintf(obj, "mtllib %s.mtl\n", base);
    ext_link0_seen = 0ul; ext_link0_emitted = 0ul; ext_mirror_polys = 0ul;
    for (k = 0u; k < nsel; ++k) {
        size_t idx = sel[k];
        size_t tris = 0u;
        float m2[12];
        /* texheader from tha, UV points from tpa -- directly, as MAME
         * geo_object_data does (model2_v.cpp:1715-1728), not via the tpa->record
         * table lookup (which returned the wrong texheader for objects sharing a
         * tpa, e.g. Lau's vest reading navy instead of red). */
        uint32_t uv_ptr = cand_tpa[idx] & 0x7fffffu, th_ptr = cand_tha[idx] & 0x7fffffu;
        memcpy(m2, mf[idx], sizeof(m2));
        m2[9]  = (float)((double)m2[9]  - off[0]);
        m2[10] = (float)((double)m2[10] - off[1]);
        m2[11] = (float)((double)m2[11] - off[2]);
        if (from_rom)
            fprintf(obj, "o %s_%s_rec%06x\n", base,
                    is_recovered[idx] ? "rom" : "cap", (unsigned)cand_rec[idx]);
        else
            fprintf(obj, "o %s_piece%02zu_rec%06x\n", base, k, (unsigned)cand_rec[idx]);
        vbase = ext_emit_piece(obj, cand_pbuf[idx], cand_psize[idx], tex, tex_size,
                               cand_rec[idx], uv_ptr, th_ptr, vbase, &cur_sheet,
                               &tris, m2, NULL, NULL, 0u,
                               gf_lit ? gf_sctrl : NULL, gf_mode, used_spec,
                               stage_mode ? gf_tiles : NULL,
                               stage_mode ? &ngf_tiles : NULL,
                               stage_mode ? (sizeof(gf_tiles) / sizeof(gf_tiles[0])) : 0u);
        if (colour)
            ext_colour_sheet_piece(cand_pbuf[idx], cand_psize[idx], tex, tex_size,
                                   cand_rec[idx], th_ptr,
                                   (const uint8_t *const *)bank, bank_size,
                                   pal, pal_n, cxlat, cxlat_n, luma, csheet, asheet,
                                   gf_lit ? cand_vmat[idx] : NULL, gf_light,
                                   gf_diff, gf_amb, gf_sscale, gf_sctrl, gf_mode);
        tris_total += tris;
    }
    if (stage_mode)
        fprintf(stderr, "  link-0 polys: seen=%lu emitted=%lu (must be 0); mirrored UV polys: %lu\n",
                ext_link0_seen, ext_link0_emitted, ext_mirror_polys);
    if (colour) {
        size_t t;
        snprintf(path, sizeof(path), "%s/%s_tex0.png", out_dir, base);
        (void)png_write_rgba8(path, 2048u, 1024u, csheet[0], asheet[0]);
        snprintf(path, sizeof(path), "%s/%s_tex1.png", out_dir, base);
        (void)png_write_rgba8(path, 2048u, 1024u, csheet[1], asheet[1]);
        /* repeat-UV tiles: cut each referenced tile out of the baked atlas as its
         * own image so a REPEAT-wrapped material tiles it (the polys' UVs are in
         * tile units). The tile already carries the floor's baked colour + luma. */
        for (t = 0u; t < ngf_tiles; ++t) {
            uint32_t tw = gf_tiles[t].w, th2 = gf_tiles[t].h, xx, yy;
            const uint8_t *cs = csheet[gf_tiles[t].sheet & 1u];
            const uint8_t *as = asheet[gf_tiles[t].sheet & 1u];
            uint8_t *trgb = (tw && th2) ? (uint8_t *)malloc((size_t)tw * th2 * 3u) : NULL;
            uint8_t *tal = (tw && th2) ? (uint8_t *)malloc((size_t)tw * th2) : NULL;
            gf_tile_hasalpha[t] = 0;
            if (trgb != NULL && tal != NULL) {
                for (yy = 0u; yy < th2; ++yy)
                    for (xx = 0u; xx < tw; ++xx) {
                        size_t sx = (size_t)gf_tiles[t].x + xx, sy = (size_t)gf_tiles[t].y + yy;
                        size_t dd = (size_t)yy * tw + xx, ss;
                        if (sx >= 2048u || sy >= 1024u) { trgb[dd*3u]=trgb[dd*3u+1u]=trgb[dd*3u+2u]=0u; tal[dd]=0u; gf_tile_hasalpha[t]=1; continue; }
                        ss = sy * 2048u + sx;
                        trgb[dd*3u] = cs[ss*3u]; trgb[dd*3u+1u] = cs[ss*3u+1u]; trgb[dd*3u+2u] = cs[ss*3u+2u];
                        tal[dd] = as[ss];
                        if (as[ss] < 255u) gf_tile_hasalpha[t] = 1;
                    }
                snprintf(path, sizeof(path), "%s/%s_tile%zu.png", out_dir, base, t);
                (void)png_write_rgba8(path, tw, th2, trgb, tal);
            }
            free(trgb); free(tal);
        }
    } else {
        snprintf(path, sizeof(path), "%s/tex0.png", out_dir);
        (void)ext_tex_write_sheet_png(path, bank[0], bank_size[0]);
        snprintf(path, sizeof(path), "%s/tex1.png", out_dir);
        (void)ext_tex_write_sheet_png(path, bank[1], bank_size[1]);
    }

    /* material library, written after the mesh so the chrome materials reflect
     * what was actually emitted. The base sheet0/1 lines are unchanged, so a
     * fighter that uses no chrome material (every one but Dural) gets a
     * byte-identical MTL. */
    snprintf(path, sizeof(path), "%s/%s.mtl", out_dir, base);
    mtl = fopen(path, "wb");
    if (mtl != NULL) {
        if (colour) {
            fprintf(mtl, "# VF2 %s textures (colour: palette+luma+colorxlat+gamma, "
                         "%s). Local only.\n", base,
                    gf_lit ? "representative per-polygon luma" : "full-light albedo");
            /* RGBA atlas: a translucent polygon's cutout texels (0x0f) bake with
             * alpha 0. map_d points at the same PNG so a viewer uses its alpha;
             * illum 1, d 1 = alpha-test so the surface behind a decal shows. */
            fprintf(mtl, "newmtl sheet0\nillum 1\nd 1.0\nmap_Kd %s_tex0.png\nmap_d %s_tex0.png\n", base, base);
            fprintf(mtl, "newmtl sheet1\nillum 1\nd 1.0\nmap_Kd %s_tex1.png\nmap_d %s_tex1.png\n", base, base);
            /* A strongly-specular (chrome) material: its atlas holds only the
             * matte diffuse+ambient base (the view-dependent highlight is not
             * baked), so mark it metallic for the viewer to shade live. The 0x06
             * table drives the shininess: spec_ctrl sets the exponent (1/2/4/8
             * squarings in gc_poly_luma), spec_scale the strength. Blender's OBJ
             * importer reads Pm/Pr (PBR -> Principled metallic/roughness); Ns/Ks
             * are the legacy fallback. */
            {
                int sh; uint32_t tpi;
                for (sh = 0; sh < 2; ++sh)
                    for (tpi = 0u; tpi < 32u; ++tpi) {
                        uint32_t sc;
                        double exps, metal, rough, ns;
                        if ((used_spec[sh] & (1u << tpi)) == 0u) continue;
                        sc = gf_sctrl[tpi];
                        exps = 1.0;
                        if ((sc >> 1) != 0u) exps *= 2.0;
                        if ((sc >> 2) != 0u) exps *= 2.0;
                        if (((sc + 1u) >> 3) != 0u) exps *= 2.0;   /* 1,2,4,8 */
                        metal = (double)gf_sscale[tpi] / 255.0;
                        rough = 0.5 / exps + 0.05;                 /* sharper -> smoother */
                        if (rough > 0.9) rough = 0.9;
                        ns = exps * 100.0; if (ns > 900.0) ns = 900.0;
                        fprintf(mtl, "newmtl sheet%d_spec%u\n", sh, tpi);
                        fprintf(mtl, "illum 2\nd 1.0\n");
                        fprintf(mtl, "Pm %.3f\nPr %.3f\n", metal, rough);
                        fprintf(mtl, "Ks %.3f %.3f %.3f\nNs %.1f\n", metal, metal, metal, ns);
                        fprintf(mtl, "map_Kd %s_tex%d.png\nmap_d %s_tex%d.png\n",
                                base, sh, base, sh);
                    }
            }
            /* repeat-UV tiles: standalone images, UVs in tile units -> REPEAT
             * (the OBJ importer's default wrap for UVs outside [0,1]). map_d is
             * emitted ONLY for a tile that actually has transparent texels: Blender's
             * OBJ importer feeds a map_d image's COLOUR (not its alpha) into the
             * material's dissolve, so a map_d on an opaque but dark tile (the stone
             * arena slab tile3) makes it ~30% opaque and the viewport shows through it.
             * An opaque tile gets map_Kd only and stays solid. */
            {
                size_t t;
                for (t = 0u; t < ngf_tiles; ++t) {
                    fprintf(mtl, "newmtl tile%zu\nillum 1\nd 1.0\nmap_Kd %s_tile%zu.png\n",
                            t, base, t);
                    if (gf_tile_hasalpha[t])
                        fprintf(mtl, "map_d %s_tile%zu.png\n", base, t);
                }
            }
        } else {
            fprintf(mtl, "# VF2 %s textures (grayscale luma index). Local only.\n", base);
            fprintf(mtl, "newmtl sheet0\nmap_Kd tex0.png\n");
            fprintf(mtl, "newmtl sheet1\nmap_Kd tex1.png\n");
        }
        fclose(mtl); mtl = NULL;
    }

    /* verify every map_Kd/map_d the MTL names resolves to a file that exists, so a
     * missing tile/atlas PNG fails loudly here instead of showing up magenta in a
     * viewer. */
    {
        char line2[1024]; FILE *mv;
        size_t nmiss = 0u;
        snprintf(path, sizeof(path), "%s/%s.mtl", out_dir, base);
        mv = fopen(path, "rb");
        if (mv != NULL) {
            while (fgets(line2, (int)sizeof(line2), mv) != NULL) {
                char ref[512]; FILE *tf;
                if (sscanf(line2, " map_Kd %511s", ref) == 1 ||
                    sscanf(line2, " map_d %511s", ref) == 1) {
                    snprintf(path, sizeof(path), "%s/%s", out_dir, ref);
                    tf = fopen(path, "rb");
                    if (tf == NULL) {
                        fprintf(stderr, "geo-%s: MTL references missing file %s\n",
                                stage_mode ? "stage" : "fighter", ref);
                        ++nmiss;
                    } else fclose(tf);
                }
            }
            fclose(mv);
        }
        if (nmiss > 0u) {
            fprintf(stderr, "geo-%s: %zu MTL texture reference(s) have no file\n",
                    stage_mode ? "stage" : "fighter", nmiss);
            rc = EXIT_FAILURE; goto done;
        }
    }

    printf("geo-%s: %s frame %zu -> %s/%s.obj (+ %s.mtl, tex0/1.png %s)\n",
           stage_mode ? "stage" : "fighter",
           base, pose, out_dir, base, base, colour ? "[colour]" : "[grayscale]");
    if (stage_mode && from_rom)
        printf("  %zu objects: %zu captured + %zu recovered from ROM id range, "
               "%zu triangles (o names: _cap_ = captured, _rom_ = ROM-recovered)\n",
               nsel, nsel - nrec_rom, nrec_rom, tris_total);
    else if (stage_mode)
        printf("  %zu distinct decor objects (shared-matrix stage set), %zu triangles; "
               "camera removed, world frame kept (merges by record across captures)\n",
               nsel, tris_total);
    else {
        printf("  %zu fighter pieces (%zu decoded from reconstructed polygon RAM), "
               "3D connected-components at joint threshold %.2f; selected %s (%s) = "
               "%zu pieces, %zu triangles\n",
               ncand, nram, bestT, base, side == 0 ? "left" : "right", nsel,
               tris_total);
        if (nram_unwritten > 0u)
            printf("  note: %zu RAM piece(s) read an index not uploaded this frame "
                   "(0x05 from a previous frame needed)\n", nram_unwritten);
        printf("  camera removed; vertical axis %c, feet at origin; "
               "bbox world extent (%.2f,%.2f,%.2f)\n",
               "xyz"[vax], mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]);
    }
    rc = EXIT_SUCCESS;

done:
    if (bin != NULL) fclose(bin);
    if (txt != NULL) fclose(txt);
    if (obj != NULL) fclose(obj);
    if (mtl != NULL) fclose(mtl);
    free(words); free(objs); free(vacc); free(mf); free(cand_vmat);
    free(cand_rec); free(cand_tpa); free(cand_tha); free(cand_s); free(pstart); free(pcount);
    free(cand_scx); free(sel); free(worldv); free(dmat); free(parent);
    free(compsz); free(grp2); free(ram0); free(ram1);
    free(cand_pbuf); free(cand_psize); free(is_recovered);
    free(main_data); free(poly); free(tex); free(bank[0]); free(bank[1]);
    free(pal); free(cxlat); free(lumacap); free(luma);
    free(csheet[0]); free(csheet[1]);
    free(asheet[0]); free(asheet[1]);
    return rc;
}

/* --- geo-texparams: find command 0x06 across a series of RAM9 lists --------------
 *
 * The geometry engine's per-index texture parameters (diffuse/ambient lighting, set
 * by command 0x06) PERSIST across frames -- a fight frame often carries no 0x06 at
 * all, so a single-frame colour render falls back to defaults and under-lights the
 * decor (see command_geo_colour). This tool walks every frame of a multi-frame RAM9
 * capture (e.g. one spanning stage load + round start) in readstarts order, applies
 * every 0x06 in sequence, and writes the resulting PERSISTED state -- the most recent
 * value for each of the 32 indices -- as "<index> <diffuse> <ambient>" lines. Feed
 * the file back to `geo-colour ... <texparams.txt>` to light the decor. Local only. */
static int command_geo_texparams(
    const char *bin_path, const char *txt_path, const char *out_path
)
{
    const size_t frame_bytes = 131072u, frame_words = frame_bytes / 4u;
    FILE *bin = NULL, *txt = NULL, *out = NULL;
    char line[256];
    uint32_t *w = NULL;
    uint32_t readstart[GS_MAX_FRAMES];
    uint8_t tp_diffuse[32], tp_ambient[32], tp_sscale[32], tp_sctrl[32];
    size_t nframes = 0u, f, i, total06 = 0u, total07 = 0u;
    uint32_t geo_mode = 0u;
    int seen_mode = 0;
    int rc = EXIT_FAILURE;

    for (i = 0u; i < 32u; ++i) { tp_diffuse[i] = 255u; tp_ambient[i] = 0u; tp_sscale[i] = 0u; tp_sctrl[i] = 0u; }
    txt = fopen(txt_path, "rb");
    if (txt == NULL) { fprintf(stderr, "geo-texparams: cannot open %s\n", txt_path); return EXIT_FAILURE; }
    while (fgets(line, (int)sizeof(line), txt) != NULL) {
        unsigned idx, fn, rs;
        if (sscanf(line, "F %u %u %x", &idx, &fn, &rs) == 3 && idx < GS_MAX_FRAMES) {
            readstart[idx] = rs;
            if ((size_t)idx + 1u > nframes) nframes = (size_t)idx + 1u;
        }
    }
    fclose(txt); txt = NULL;
    if (nframes == 0u) { fprintf(stderr, "geo-texparams: no frames in %s\n", txt_path); return EXIT_FAILURE; }

    bin = fopen(bin_path, "rb");
    w = (uint32_t *)malloc(frame_words * sizeof(*w));
    out = fopen(out_path, "wb");
    if (bin == NULL || w == NULL || out == NULL) { fprintf(stderr, "geo-texparams: alloc/open\n"); goto done; }
    fprintf(out, "# VF2 persisted texture parameters (command 0x06), accumulated over\n");
    fprintf(out, "# %zu frame(s). Format: <index> <diffuse> <ambient> <spec_scale> <spec_ctrl>\n", nframes);
    fprintf(out, "# (diffuse=param&0xff, ambient=(param>>8)&0xff, spec_scale=(param>>16)&0xff,\n");
    fprintf(out, "#  spec_ctrl=param>>24; coef/LOD omitted -- unused without mipmapping). Local only.\n");

    for (f = 0u; f < nframes; ++f) {
        size_t ptr, opc = 0u, n06 = 0u;
        if (!gs_read_frame(bin, f, frame_bytes, w, frame_words)) continue;
        ptr = (size_t)((readstart[f] & 0x1ffffu) >> 2);
        for (;;) {
            uint32_t op, cmd;
            size_t need = 0u;
            if (opc++ >= 0x40000u || ptr >= frame_words) break;
            op = w[ptr++];
            if (op & 0x80000000u) { ptr = (size_t)((op & 0x1ffffu) >> 2); continue; }
            cmd = (op >> 23) & 0x1fu;
            switch (cmd) {
            case 0x00u: need = 0u; break;
            case 0x07u: case 0x17u: case 0x08u: case 0x18u:
            case 0x10u: case 0x16u: case 0x1eu: need = 1u; break;
            case 0x09u: case 0x19u: case 0x0du: need = 2u; break;
            case 0x0au: case 0x1au: case 0x0cu: case 0x1cu: need = 3u; break;
            case 0x03u: case 0x13u: need = 6u; break;
            case 0x0bu: case 0x1bu: need = 12u; break;
            case 0x01u: case 0x11u: need = 4u; break;
            case 0x04u: case 0x14u: case 0x05u: case 0x15u:
                if (ptr + 2u > frame_words) goto fend;
                need = 2u + (size_t)w[ptr + 1u]; break;
            case 0x06u:
                if (ptr + 2u > frame_words) goto fend;
                need = 2u + (size_t)w[ptr + 1u] * 2u; break;
            case 0x1du:
                if (ptr + 2u > frame_words) goto fend;
                need = 2u + (size_t)w[ptr + 1u] * 3u; break;
            case 0x02u: case 0x12u: {
                size_t p = ptr + 8u;
                for (;;) { uint32_t a; if (p >= frame_words) goto fend; a = w[p++]; if ((a & 3u) == 0u) break; p += 5u; if (a & 1u) p += 3u; }
                need = p - ptr; break;
            }
            case 0x0eu: if (ptr + 33u > frame_words) goto fend; need = 32u + 1u + (size_t)w[ptr + 32u] * 3u; break;
            case 0x0fu: case 0x1fu: goto fend;
            default: goto fend;
            }
            if (ptr + need > frame_words) goto fend;
            if (cmd == 0x07u || cmd == 0x17u) { geo_mode = w[ptr]; seen_mode = 1; ++total07; }
            if (cmd == 0x06u) {
                uint32_t index = (w[ptr] >> 2) & 0x1fu, count = w[ptr + 1u], j;
                ++n06;
                for (j = 0u; j < count; ++j) {
                    uint32_t prm = w[ptr + 2u + j * 2u];   /* param word; +1 = coef (LOD) */
                    tp_diffuse[index] = (uint8_t)(prm & 0xffu);
                    tp_ambient[index] = (uint8_t)((prm >> 8) & 0xffu);
                    tp_sscale[index]  = (uint8_t)((prm >> 16) & 0xffu);
                    tp_sctrl[index]   = (uint8_t)((prm >> 24) & 0xffu);
                    index = (index + 1u) & 0x1fu;
                }
            }
            ptr += need;
        }
    fend:
        if (n06 > 0u) { fprintf(out, "# frame %zu: %zu x 0x06\n", f, n06); total06 += n06; }
    }
    if (seen_mode)
        fprintf(out, "# geo_mode (command 0x07) last seen = 0x%08x; parser (mode&3) = %u (%s)\n",
                geo_mode, geo_mode & 3u,
                (geo_mode & 3u) == 1u || (geo_mode & 3u) == 3u ? "specular" : "no-specular");
    else
        fprintf(out, "# geo_mode (command 0x07) not seen in this capture (persists from earlier; defaulting to no-specular)\n");
    for (i = 0u; i < 32u; ++i)
        fprintf(out, "%2zu %3u %3u %3u %3u\n", i, tp_diffuse[i], tp_ambient[i],
                tp_sscale[i], tp_sctrl[i]);
    printf("geo-texparams: %zu frame(s), %zu x 0x06, %zu x 0x07 -> %s\n",
           nframes, total06, total07, out_path);
    if (seen_mode)
        printf("  geo_mode = 0x%08x, parser (mode&3) = %u (%s)\n", geo_mode, geo_mode & 3u,
               (geo_mode & 3u) == 1u || (geo_mode & 3u) == 3u ? "specular" : "no-specular");
    if (total06 == 0u)
        printf("  (no 0x06 found -- capture frames spanning stage load / round start)\n");
    rc = EXIT_SUCCESS;
done:
    if (bin != NULL) fclose(bin);
    if (txt != NULL) fclose(txt);
    if (out != NULL) fclose(out);
    free(w);
    return rc;
}

/* --- geo-colour: Step colours -- faithful textured render vs the screenshot ----
 *
 * Renders one RAM9 frame in colour exactly like MAME draw_scanline_tex / sm2-emu
 * polygon.frag: per pixel, fetch the 4-bit texel, scale to 8 bits, look it up in
 * the polygon's tone curve in luma RAM (chosen by lumabase = (texheader[1]&0xff)
 * <<7), scale by the polygon's lighting term, clamp to a 6-bit shade, then index
 * the colorxlat table with that shade and each 5-bit channel of the colour entry,
 * then run each channel through VF2's fixed monitor gamma (bias 64, gain 255/191).
 *
 * The colour is INDIRECT: colorbase = (texheader[3]>>6)&0x3ff selects a palette
 * slot, and the real 5-5-5 colour is palram[colorbase + 0x1000] & 0x7fff -- the low
 * 0x1000 words are the 2D tilemap palette (reddish here), so a direct read tints the
 * whole stage red (model2rd.ipp:256-274). colorxlat channel regions are at word
 * 0x0000/0x2000/0x4000 (R/G/B). palette / colorxlat / luma come from the local
 * colour capture (0x01800000 / 0x01810000 / 0x12800000).
 *
 * Lighting is the geo_parse diffuse+ambient term (model2_v.cpp:1092-1097); specular,
 * mipmapping, bilinear and perspective-correct UV are deferred. The per-index
 * diffuse/ambient come from command 0x06, which persists across frames and is absent
 * from a single-frame capture -- see the default-init note in command_geo_colour.
 * Local only; never committed. (gc_u16 / gc_gamma / gc_texel_rgb are defined with the
 * texture writers above, shared with the coloured-texture export.) */
/* Optional single-pixel trace target (VF2_COLOUR_TRACE="x,y"): when the pixel that
 * wins the z-test is (gc_tx,gc_ty), dump the full draw_scanline_tex pipeline for it so
 * a witness polygon can be compared field by field against screen.png. */
static int gc_tx = -1, gc_ty = -1;

static void gc_fill_tri_tex(
    uint8_t *fb, double *zb, int W, int H,
    const double *X, const double *Y, const double *Z,
    const double *U, const double *V,
    const uint8_t *bank, size_t bank_size, const vf2_texture_header *hd,
    const uint8_t *luma, const uint8_t *cxlat, size_t cxlat_n,
    uint32_t lumabase, int r5, int g5, int b5, uint32_t lumascale, int untextured)
{
    /* Untextured (solid-colour) polygon: no texel, the shade is the polygon lighting
     * term >> 2 indexing colorxlat directly (MAME draw_scanline_solid, model2rd.ipp:66).
     * Precompute the one colour and fill the triangle with it. */
    uint8_t solid[3];
    if (untextured) {
        uint32_t sh = lumascale >> 2; if (sh > 0x3fu) sh = 0x3fu;
        solid[0] = gc_gamma(gc_u16(cxlat, cxlat_n, 0x0000u + (size_t)r5 * 0x100u + sh) & 0xffu);
        solid[1] = gc_gamma(gc_u16(cxlat, cxlat_n, 0x2000u + (size_t)g5 * 0x100u + sh) & 0xffu);
        solid[2] = gc_gamma(gc_u16(cxlat, cxlat_n, 0x4000u + (size_t)b5 * 0x100u + sh) & 0xffu);
    } else { solid[0] = solid[1] = solid[2] = 0u; }
    {
    int minx = (int)floor(X[0] < X[1] ? (X[0] < X[2] ? X[0] : X[2]) : (X[1] < X[2] ? X[1] : X[2]));
    int maxx = (int)ceil(X[0] > X[1] ? (X[0] > X[2] ? X[0] : X[2]) : (X[1] > X[2] ? X[1] : X[2]));
    int miny = (int)floor(Y[0] < Y[1] ? (Y[0] < Y[2] ? Y[0] : Y[2]) : (Y[1] < Y[2] ? Y[1] : Y[2]));
    int maxy = (int)ceil(Y[0] > Y[1] ? (Y[0] > Y[2] ? Y[0] : Y[2]) : (Y[1] > Y[2] ? Y[1] : Y[2]));
    double d = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2]);
    int px, py;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= W) maxx = W - 1;
    if (maxy >= H) maxy = H - 1;
    if (fabs(d) < 1e-9) return;
    for (py = miny; py <= maxy; ++py) {
        for (px = minx; px <= maxx; ++px) {
            double a = ((Y[1] - Y[2]) * (px - X[2]) + (X[2] - X[1]) * (py - Y[2])) / d;
            double b = ((Y[2] - Y[0]) * (px - X[2]) + (X[0] - X[2]) * (py - Y[2])) / d;
            double c = 1.0 - a - b;
            double z, tu, tv;
            size_t fi = (size_t)py * (size_t)W + (size_t)px;
            int lu, lv;
            uint8_t t4, rgb[3];
            if (a < -0.001 || b < -0.001 || c < -0.001) continue;
            z = a * Z[0] + b * Z[1] + c * Z[2];
            if (z >= zb[fi]) continue;
            if (untextured) {
                zb[fi] = z;
                fb[fi * 3u + 0u] = solid[0]; fb[fi * 3u + 1u] = solid[1]; fb[fi * 3u + 2u] = solid[2];
                if (px == gc_tx && py == gc_ty) {
                    uint32_t sh = lumascale >> 2; if (sh > 0x3fu) sh = 0x3fu;
                    fprintf(stderr,
                        "TRACE px(%d,%d) UNTEXTURED renderer=%u colorbase=0x%03x\n"
                        "  r5=%d g5=%d b5=%d lumascale=%u shade(luma>>2)=%u -> gamma RGB=(%u,%u,%u)\n",
                        px, py, hd->renderer, hd->color_base, r5, g5, b5, lumascale, sh,
                        solid[0], solid[1], solid[2]);
                }
                continue;
            }
            tu = a * U[0] + b * U[1] + c * U[2];
            tv = a * V[0] + b * V[1] + c * V[2];
            lu = hd->tex_width ? (int)fmod(tu, (double)hd->tex_width) : 0;
            lv = hd->tex_height ? (int)fmod(tv, (double)hd->tex_height) : 0;
            if (lu < 0) lu += (int)hd->tex_width;
            if (lv < 0) lv += (int)hd->tex_height;
            t4 = vf2_texture_sheet_texel(bank, bank_size,
                                         hd->tex_x + (uint32_t)lu,
                                         hd->tex_y + (uint32_t)lv);
            if (!gc_texel_rgb(t4, lumabase, r5, g5, b5, lumascale,
                              (int)(hd->renderer & 1u), luma, cxlat, cxlat_n, rgb)) continue;
            zb[fi] = z;
            fb[fi * 3u + 0u] = rgb[0];
            fb[fi * 3u + 1u] = rgb[1];
            fb[fi * 3u + 2u] = rgb[2];
            if (px == gc_tx && py == gc_ty) {
                uint32_t t8 = (uint32_t)t4 << 4;
                uint32_t curve = luma[(lumabase + (t8 >> 1)) & 0x7fffu];
                uint32_t shade = (curve * lumascale) / 256u; if (shade > 0x3fu) shade = 0x3fu;
                fprintf(stderr,
                    "TRACE px(%d,%d) sheet%u rect(%u,%u %ux%u) renderer=%u translucent=%u\n"
                    "  colorbase=0x%03x lumabase=0x%04x r5=%d g5=%d b5=%d lumascale=%u\n"
                    "  texel=0x%x t8=0x%02x curve(luma[0x%04x])=%u shade=%u\n"
                    "  colorxlat R=%u G=%u B=%u -> gamma RGB=(%u,%u,%u)\n",
                    px, py, hd->sheet, hd->tex_x, hd->tex_y, hd->tex_width, hd->tex_height,
                    hd->renderer, hd->renderer & 1u,
                    hd->color_base, lumabase, r5, g5, b5, lumascale,
                    t4, t8, (lumabase + (t8 >> 1)) & 0x7fffu, curve, shade,
                    gc_u16(cxlat, cxlat_n, 0x0000u + (size_t)r5 * 0x100u + shade) & 0xffu,
                    gc_u16(cxlat, cxlat_n, 0x2000u + (size_t)g5 * 0x100u + shade) & 0xffu,
                    gc_u16(cxlat, cxlat_n, 0x4000u + (size_t)b5 * 0x100u + shade) & 0xffu,
                    rgb[0], rgb[1], rgb[2]);
            }
        }
    }
    }
}

static int command_geo_colour(
    const char *bin_path, const char *txt_path, const char *rom_directory,
    const char *capture_dir, const char *pose_str, const char *out_path,
    const char *texparams_path
)
{
    const size_t frame_bytes = 131072u;
    const size_t frame_words = frame_bytes / 4u;
    const int W = 496, H = 384;
    FILE *bin = NULL, *txt = NULL;
    char line[256], path[1100];
    uint32_t *w = NULL;
    uint8_t *main_data = NULL, *poly = NULL, *tex = NULL;
    uint8_t *bank[2] = { NULL, NULL };
    uint8_t *pal = NULL, *cxlat = NULL, *lumacap = NULL, *luma = NULL;
    size_t main_size = 0u, poly_size = 0u, tex_size = 0u;
    size_t bank_size[2] = { 0u, 0u };
    size_t pal_n = 0u, cxlat_n = 0u, luma_n = 0u;
    uint8_t *fb = NULL;
    double *zb = NULL;
    uint32_t readstart[GS_MAX_FRAMES];
    size_t nframes = 0u, pose, ptr, opc = 0u, i, nspec = 0u;
    float mat[12];
    float light[3] = { 0.0f, 0.0f, 1.0f };
    uint32_t geo_mode = 0u;      /* command 0x07; (mode&3)==1 or 3 enables specular */
    uint8_t tp_diffuse[32], tp_ambient[32], tp_sscale[32], tp_sctrl[32];
    double fx = 600.0, fy = 600.0, cx = 248.0, cy = 319.0, xoff = 0.0, yoff = 130.0;
    int rc = EXIT_FAILURE;
    vf2_status st;

    pose = (size_t)strtoul(pose_str, NULL, 0);
    { const char *tr = getenv("VF2_COLOUR_TRACE"); if (tr != NULL) sscanf(tr, "%d,%d", &gc_tx, &gc_ty); }
    memset(mat, 0, sizeof(mat));
    /* Texture parameters (diffuse/ambient per index) are set by command 0x06 and
     * PERSIST in the geometry engine across frames (MAME value-inits them to 0/0 in
     * geo_init and they are only ever overwritten, never reset). This single-frame
     * capture contains no 0x06, so the real ambient term -- set in an earlier frame
     * -- is unavailable; we fall back to full diffuse / no ambient. That renders the
     * fighters faithfully (they are diffuse-lit) but under-lights decor faces angled
     * away from the light, which in-game are lifted by the persisted ambient.
     * `geo-texparams` recovers those persisted values from a multi-frame capture;
     * pass its output as texparams_path to light the decor correctly. */
    for (i = 0u; i < 32u; ++i) { tp_diffuse[i] = 255u; tp_ambient[i] = 0u; tp_sscale[i] = 0u; tp_sctrl[i] = 0u; }
    if (texparams_path != NULL)
        gc_load_texparams(texparams_path, tp_diffuse, tp_ambient, tp_sscale, tp_sctrl);

    txt = fopen(txt_path, "rb");
    if (txt == NULL) { fprintf(stderr, "geo-colour: cannot open %s\n", txt_path); return EXIT_FAILURE; }
    while (fgets(line, (int)sizeof(line), txt) != NULL) {
        unsigned idx, fn, rs;
        if (sscanf(line, "F %u %u %x", &idx, &fn, &rs) == 3 && idx < GS_MAX_FRAMES) {
            readstart[idx] = rs;
            if ((size_t)idx + 1u > nframes) nframes = (size_t)idx + 1u;
        }
    }
    fclose(txt); txt = NULL;
    if (pose >= nframes) { fprintf(stderr, "geo-colour: pose %zu >= %zu\n", pose, nframes); return EXIT_FAILURE; }

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_size);
    if (st == VF2_OK) st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly, &poly_size);
    if (st == VF2_OK) st = vf2_romset_build_region(rom_directory, VF2_REGION_TEXTURES, &tex, &tex_size);
    if (st != VF2_OK) { fprintf(stderr, "geo-colour: ROM: %s\n", vf2_status_string(st)); goto done; }

    snprintf(path, sizeof(path), "%s/tex0_12000000.bin", capture_dir);
    if (!ext_tex_load_file(path, &bank[0], &bank_size[0])) { fprintf(stderr, "geo-colour: %s\n", path); goto done; }
    snprintf(path, sizeof(path), "%s/tex1_12400000.bin", capture_dir);
    if (!ext_tex_load_file(path, &bank[1], &bank_size[1])) { fprintf(stderr, "geo-colour: %s\n", path); goto done; }
    snprintf(path, sizeof(path), "%s/palette_01800000.bin", capture_dir);
    if (!ext_tex_load_file(path, &pal, &pal_n)) { fprintf(stderr, "geo-colour: %s\n", path); goto done; }
    snprintf(path, sizeof(path), "%s/colorxlat_01810000.bin", capture_dir);
    if (!ext_tex_load_file(path, &cxlat, &cxlat_n)) { fprintf(stderr, "geo-colour: %s\n", path); goto done; }
    snprintf(path, sizeof(path), "%s/luma_12800000.bin", capture_dir);
    if (!ext_tex_load_file(path, &lumacap, &luma_n)) { fprintf(stderr, "geo-colour: %s\n", path); goto done; }
    /* luma capture is the 0x20000-byte address space, 1 byte per 32-bit word */
    luma = (uint8_t *)malloc(0x8000u);
    if (luma == NULL) goto done;
    for (i = 0u; i < 0x8000u; ++i) luma[i] = (i * 4u < luma_n) ? lumacap[i * 4u] : 0u;

    bin = fopen(bin_path, "rb");
    w = (uint32_t *)malloc(frame_words * sizeof(*w));
    fb = (uint8_t *)malloc((size_t)W * (size_t)H * 3u);
    zb = (double *)malloc((size_t)W * (size_t)H * sizeof(*zb));
    if (bin == NULL || w == NULL || fb == NULL || zb == NULL) { fprintf(stderr, "geo-colour: alloc\n"); goto done; }
    if (!gs_read_frame(bin, pose, frame_bytes, w, frame_words)) { fprintf(stderr, "geo-colour: read %zu\n", pose); goto done; }
    for (i = 0u; i < (size_t)W * (size_t)H; ++i) { fb[i * 3u] = 18u; fb[i * 3u + 1u] = 18u; fb[i * 3u + 2u] = 24u; zb[i] = 1e30; }

    ptr = (size_t)((readstart[pose] & 0x1ffffu) >> 2);
    for (;;) {
        uint32_t op, cmd;
        size_t need = 0u;
        if (opc++ >= 0x40000u || ptr >= frame_words) break;
        op = w[ptr++];
        if (op & 0x80000000u) { ptr = (size_t)((op & 0x1ffffu) >> 2); continue; }
        cmd = (op >> 23) & 0x1fu;
        switch (cmd) {
        case 0x00u: need = 0u; break;
        case 0x07u: case 0x17u: case 0x08u: case 0x18u:
        case 0x10u: case 0x16u: case 0x1eu: need = 1u; break;
        case 0x09u: case 0x19u: case 0x0du: need = 2u; break;
        case 0x0au: case 0x1au: case 0x0cu: case 0x1cu: need = 3u; break;
        case 0x03u: case 0x13u: need = 6u; break;
        case 0x0bu: case 0x1bu: need = 12u; break;
        case 0x01u: case 0x11u: need = 4u; break;
        case 0x04u: case 0x14u: case 0x05u: case 0x15u:
            if (ptr + 2u > frame_words) goto cend;
            need = 2u + (size_t)w[ptr + 1u]; break;
        case 0x06u:
            if (ptr + 2u > frame_words) goto cend;
            need = 2u + (size_t)w[ptr + 1u] * 2u; break;
        case 0x1du:
            if (ptr + 2u > frame_words) goto cend;
            need = 2u + (size_t)w[ptr + 1u] * 3u; break;
        case 0x02u: case 0x12u: {
            size_t p = ptr + 8u;
            for (;;) { uint32_t a; if (p >= frame_words) goto cend; a = w[p++]; if ((a & 3u) == 0u) break; p += 5u; if (a & 1u) p += 3u; }
            need = p - ptr; break;
        }
        case 0x0eu: if (ptr + 33u > frame_words) goto cend; need = 32u + 1u + (size_t)w[ptr + 32u] * 3u; break;
        case 0x0fu: case 0x1fu: goto cend;
        default: goto cend;
        }
        if (ptr + need > frame_words) goto cend;

        if (cmd == 0x0bu || cmd == 0x1bu) {
            size_t k; for (k = 0u; k < 12u; ++k) { uint32_t u = w[ptr + k]; memcpy(&mat[k], &u, 4u); }
        } else if (cmd == 0x0cu || cmd == 0x1cu) {
            uint32_t u; u = w[ptr]; memcpy(&mat[9], &u, 4u); u = w[ptr + 1u]; memcpy(&mat[10], &u, 4u); u = w[ptr + 2u]; memcpy(&mat[11], &u, 4u);
        } else if (cmd == 0x09u || cmd == 0x19u) {
            uint32_t u; float f; u = w[ptr]; memcpy(&f, &u, 4u); fx = (double)f; u = w[ptr + 1u]; memcpy(&f, &u, 4u); fy = (double)f;
        } else if (cmd == 0x0au || cmd == 0x1au) {
            uint32_t u; u = w[ptr]; memcpy(&light[0], &u, 4u); u = w[ptr + 1u]; memcpy(&light[1], &u, 4u); u = w[ptr + 2u]; memcpy(&light[2], &u, 4u);
        } else if (cmd == 0x07u || cmd == 0x17u) {
            geo_mode = w[ptr];   /* geo_mode (model2_v.cpp:1956); (mode&3) picks parser */
        } else if (cmd == 0x06u) {
            /* each entry is TWO words: [param][distance-coef] (geo_texture_parameters,
             * model2_v.cpp:1931-1944). diffuse/ambient/specular live in the param word. */
            uint32_t index = (w[ptr] >> 2) & 0x1fu, count = w[ptr + 1u], j;
            for (j = 0u; j < count; ++j) {
                uint32_t prm = w[ptr + 2u + j * 2u];
                tp_diffuse[index] = (uint8_t)(prm & 0xffu);
                tp_ambient[index] = (uint8_t)((prm >> 8) & 0xffu);
                tp_sscale[index]  = (uint8_t)((prm >> 16) & 0xffu);
                tp_sctrl[index]   = (uint8_t)((prm >> 24) & 0xffu);
                index = (index + 1u) & 0x1fu;
            }
        } else if (cmd == 0x03u || cmd == 0x13u) {
            uint32_t c = w[ptr + 2u]; int icx = (int)((c >> 16) & 0xfffu), icy = (int)(c & 0xfffu);
            if (icx & 0x800) icx = -(0x800 - (icx & 0x7ff));
            if (icy & 0x800) icy = -(0x800 - (icy & 0x7ff));
            cx = (double)icx; cy = (double)icy;
        } else if (cmd == 0x01u || cmd == 0x11u) {
            uint32_t oba = w[ptr + 2u], rec = (oba & 0x7fffffu) * 4u, tpa = w[ptr + 0u];
            /* MAME geo_object_data (model2_v.cpp:1715-1728): the object command is
             * {tpa, tha, oba, obc}. UV points come straight from tpa and the
             * texture header from tha -- NOT via a tpa->record table lookup (that
             * inherited indirection returns the first entry sharing a tpa, whose
             * tha can differ, giving the wrong texheader/colorbase). */
            uint32_t tha = w[ptr + 1u];
            uint32_t uv_ptr = tpa & 0x7fffffu, th_ptr = tha & 0x7fffffu;
            size_t o;
            vf2_model_vertex p0, p1;
            if ((size_t)rec + 24u > poly_size) { ptr += need; continue; }
            if (!ext_read_xyz(poly, poly_size, rec, &p0) || !ext_read_xyz(poly, poly_size, rec + 12u, &p1)) { ptr += need; continue; }
            o = (size_t)rec + 24u;
            for (;;) {
                uint32_t attr = tl_u32(poly, poly_size, (uint32_t)o);
                vf2_model_vertex nv, vA, vB;
                uint16_t thw[4]; vf2_texture_header hd; float uvt[4][2];
                double Xs[3], Ys[3], Zs[3], Us[3], Vs[3];
                double nt[3], dotl, dotp, lumv; uint32_t lumascale, lumabase, colorbase;
                int r5, g5, b5, untextured; uint32_t palc, numv, vtx, tri;
                o += 4u;
                if ((attr & 3u) == 0u) break;
                numv = (attr & 1u) ? 4u : 3u;
                (void)ext_read_xyz(poly, poly_size, (uint32_t)o, &nv);   /* normal */
                o += 12u;
                if (!ext_read_xyz(poly, poly_size, (uint32_t)o, &vA)) break;
                o += 12u;
                if (numv == 4u) { if (!ext_read_xyz(poly, poly_size, (uint32_t)o, &vB)) break; } else { vB = vA; }
                o += 12u;
                thw[0] = tl_u16(tex, tex_size, th_ptr + 0u); thw[1] = tl_u16(tex, tex_size, th_ptr + 1u);
                thw[2] = tl_u16(tex, tex_size, th_ptr + 2u); thw[3] = tl_u16(tex, tex_size, th_ptr + 3u);
                vf2_texture_header_decode(thw, &hd);
                colorbase = ((uint32_t)thw[3] >> 6) & 0x3ffu;
                lumabase = ((uint32_t)thw[1] & 0xffu) << 7;
                /* lighting: transform normal by the 3x3, diffuse+ambient term */
                nt[0] = (double)nv.x * mat[0] + (double)nv.y * mat[3] + (double)nv.z * mat[6];
                nt[1] = (double)nv.x * mat[1] + (double)nv.y * mat[4] + (double)nv.z * mat[7];
                nt[2] = (double)nv.x * mat[2] + (double)nv.y * mat[5] + (double)nv.z * mat[8];
                dotl = nt[0] * light[0] + nt[1] * light[1] + nt[2] * light[2];
                /* dotp uses THIS polygon's own new point (MAME model2_v.cpp:1080),
                 * i.e. vA (the first vertex read this iteration), not the strip base. */
                { vf2_model_vertex pt; ext_xform(mat, &vA, &pt); dotp = nt[0] * pt.x + nt[1] * pt.y + nt[2] * pt.z; }
                lumv = (dotl * dotp < 0.0) ? 0.0 : fabs(dotl);
                {
                    uint32_t tpi = (attr >> 18) & 0x1fu;
                    double spec = 0.0;
                    /* specular, only in a specular parse mode (geo_parse_np_s,
                     * model2_v.cpp:1243-1252): spec = max(2*dotl*nz - light.z, 0),
                     * squared per specular_control bit, times specular_scale. */
                    if (((geo_mode & 3u) == 1u || (geo_mode & 3u) == 3u) && tp_sctrl[tpi] != 0u) {
                        uint32_t sc = tp_sctrl[tpi];
                        spec = (2.0 * dotl) * nt[2] - (double)light[2];
                        if (spec < 0.0) spec = 0.0;
                        if ((sc >> 1) != 0u) spec *= spec;
                        if ((sc >> 2) != 0u) spec *= spec;
                        if (((sc + 1u) >> 3) != 0u) spec *= spec;
                        spec *= (double)tp_sscale[tpi];
                        if (spec > 0.0) ++nspec;
                    }
                    lumv = lumv * (double)tp_diffuse[tpi] + (double)tp_ambient[tpi] + spec;
                }
                if (lumv < 0.0) lumv = 0.0;
                if (lumv > 255.0) lumv = 255.0;
                lumascale = (uint32_t)lumv;
                /* colorbase is an INDIRECT palette index: the real 5-5-5 colour is
                 * palram[colorbase + 0x1000] & 0x7fff (model2rd.ipp:270). The low
                 * 0x1000 words are the 2D tilemap palette, not the 3D polygon one. */
                palc = gc_u16(pal, pal_n, (size_t)((colorbase + 0x1000u) & 0x1fffu)) & 0x7fffu;
                r5 = (int)(palc & 0x1fu); g5 = (int)((palc >> 5) & 0x1fu); b5 = (int)((palc >> 10) & 0x1fu);
                /* UVs (texel coords) for the ring vertices, table order
                 * [P1(n-1),P0(n-1),P0(n),P1(n)] = uvt[0..3]. */
                for (vtx = 0u; vtx < numv; ++vtx) {
                    uint16_t pvv = tl_u16(tex, tex_size, uv_ptr + vtx * 2u + 0u);
                    uint16_t puu = tl_u16(tex, tex_size, uv_ptr + vtx * 2u + 1u);
                    uvt[vtx][0] = vf2_texture_uv_texels(puu);
                    uvt[vtx][1] = vf2_texture_uv_texels(pvv);
                }
                if (numv == 3u) { uvt[3][0] = uvt[2][0]; uvt[3][1] = uvt[2][1]; }
                /* renderer (model2_v.cpp:568): bit1 textured, bit0 translucent. An
                 * untextured (solid) polygon colours from the lighting term via
                 * colorxlat (draw_scanline_solid), not a texel; a translucent SOLID
                 * renders nothing. */
                untextured = ((hd.renderer & 2u) == 0u);
                if (untextured && (hd.renderer & 1u)) goto poly_advance;
                /* link type 0 is a culled strip-restart polygon (MAME
                 * check_culling, model2_v.cpp:254): skip drawing, still advance. */
                if (((attr >> 8u) & 3u) == 0u) goto poly_advance;
                /* strip triangles: (p0,p1,vB) and, for a quad, (p0,vB,vA) */
                for (tri = 0u; tri < (numv == 4u ? 2u : 1u); ++tri) {
                    vf2_model_vertex vv[3]; double uu[3][2]; int ok = 1, e;
                    if (tri == 0u) {
                        vv[0] = p0; vv[1] = p1; vv[2] = vB;
                        uu[0][0] = uvt[1][0]; uu[0][1] = uvt[1][1];
                        uu[1][0] = uvt[0][0]; uu[1][1] = uvt[0][1];
                        uu[2][0] = uvt[3][0]; uu[2][1] = uvt[3][1];
                    } else {
                        vv[0] = p0; vv[1] = vB; vv[2] = vA;
                        uu[0][0] = uvt[1][0]; uu[0][1] = uvt[1][1];
                        uu[1][0] = uvt[3][0]; uu[1][1] = uvt[3][1];
                        uu[2][0] = uvt[2][0]; uu[2][1] = uvt[2][1];
                    }
                    for (e = 0; e < 3; ++e) {
                        vf2_model_vertex pv; double Zc;
                        ext_xform(mat, &vv[e], &pv);
                        Zc = (double)pv.z;
                        if (Zc < 0.0001) { ok = 0; break; }
                        Xs[e] = xoff + cx + ((double)pv.x * fx) / Zc;
                        Ys[e] = ((double)384.0 - cy) + yoff - ((double)pv.y * fy) / Zc;
                        Zs[e] = Zc; Us[e] = uu[e][0]; Vs[e] = uu[e][1];
                    }
                    if (!ok) continue;
                    gc_fill_tri_tex(fb, zb, W, H, Xs, Ys, Zs, Us, Vs,
                                    bank[hd.sheet & 1u], bank_size[hd.sheet & 1u], &hd,
                                    luma, cxlat, cxlat_n, lumabase, r5, g5, b5, lumascale,
                                    untextured);
                }
            poly_advance:
                th_ptr = (uint32_t)((int32_t)th_ptr + vf2_texture_header_advance_words(attr));
                uv_ptr += numv * 2u;
                switch ((attr >> 8u) & 3u) {
                case 0u: case 2u: p0 = vA; p1 = vB; break;
                case 1u: p1 = vA; break;
                case 3u: p0 = vB; break;
                default: break;
                }
            }
        }
        ptr += need;
    }
cend:
    if (!png_write_rgb8(out_path, (uint32_t)W, (uint32_t)H, fb)) { fprintf(stderr, "geo-colour: write %s\n", out_path); goto done; }
    printf("geo-colour: frame %zu -> %s (%dx%d); palette %zuB colorxlat %zuB luma %zuB\n",
           pose, out_path, W, H, pal_n, cxlat_n, luma_n);
    printf("  texparams %s; geo_mode 0x%x (parser %u, %s); %zu specular polygon-term(s)\n",
           texparams_path ? texparams_path : "(defaults 255/0)", geo_mode, geo_mode & 3u,
           (geo_mode & 3u) == 1u || (geo_mode & 3u) == 3u ? "specular" : "no-specular", nspec);
    rc = EXIT_SUCCESS;
done:
    if (bin != NULL) fclose(bin);
    if (txt != NULL) fclose(txt);
    free(w); free(fb); free(zb); free(luma);
    free(main_data); free(poly); free(tex); free(bank[0]); free(bank[1]);
    free(pal); free(cxlat); free(lumacap);
    return rc;
}

/* --- geo-project: Step 2c -- MAME-exact 2D projection of one frame to PNG -----
 *
 * Walks one RAM9 frame, tracks the current transform matrix (0x0b/0x0c), focal
 * distance (0x09) and window/vanishing-point (0x03) exactly as MAME's geo_parse
 * does, decodes each object's geometry from oba (geo_object_data), and projects
 * every triangle to screen with MAME's model2_3d_project:
 *     sx = xoff + cx + (x*focus.x)/z
 *     sy = (384 - cy) + yoff - (y*focus.y)/z
 * then fills it z-buffered into an RGB image. Objects are coloured by their
 * oba record: Akira's anchor cluster 0x130a44..0x1434a0 red, shadows (flattened
 * matrix) dark, everything else grey -- so the projection can be overlaid on a
 * MAME screenshot of the same frame (the validation criterion). Local only.
 *
 * The opcode sizing mirrors src/platform/geo_display.c (the validated walker);
 * this command needs the focal/window state the walker does not expose, so it
 * walks inline. */

static void gs_fill_tri(uint8_t *fb, double *zb, int W, int H,
                        double x0, double y0, double z0,
                        double x1, double y1, double z1,
                        double x2, double y2, double z2,
                        uint8_t r, uint8_t g, uint8_t b)
{
    double minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    double maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    double miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    double maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    double area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    int xlo;
    int xhi;
    int ylo;
    int yhi;
    int px;
    int py;
    if (area == 0.0) {
        return;
    }
    xlo = (int)floor(minx); xhi = (int)ceil(maxx);
    ylo = (int)floor(miny); yhi = (int)ceil(maxy);
    if (xlo < 0) xlo = 0;
    if (ylo < 0) ylo = 0;
    if (xhi > W - 1) xhi = W - 1;
    if (yhi > H - 1) yhi = H - 1;
    for (py = ylo; py <= yhi; ++py) {
        for (px = xlo; px <= xhi; ++px) {
            double cxp = (double)px + 0.5;
            double cyp = (double)py + 0.5;
            double w0 = ((x1 - cxp) * (y2 - cyp) - (x2 - cxp) * (y1 - cyp));
            double w1 = ((x2 - cxp) * (y0 - cyp) - (x0 - cxp) * (y2 - cyp));
            double w2 = ((x0 - cxp) * (y1 - cyp) - (x1 - cxp) * (y0 - cyp));
            double z;
            size_t idx;
            if (area > 0.0) {
                if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0) continue;
            } else {
                if (w0 > 0.0 || w1 > 0.0 || w2 > 0.0) continue;
            }
            z = (w0 * z0 + w1 * z1 + w2 * z2) / area;
            idx = (size_t)py * (size_t)W + (size_t)px;
            if (z < zb[idx]) {
                zb[idx] = z;
                fb[idx * 3u + 0u] = r;
                fb[idx * 3u + 1u] = g;
                fb[idx * 3u + 2u] = b;
            }
        }
    }
}

static int command_geo_project(
    const char *bin_path, const char *txt_path, const char *rom_directory,
    const char *pose_str, const char *out_path,
    int W, int H, double xoff, double yoff
)
{
    const size_t frame_bytes = 131072u;
    const size_t frame_words = frame_bytes / 4u;
    FILE *bin = NULL;
    FILE *txt = NULL;
    FILE *csv = NULL;
    FILE *tri = NULL;
    char line[256];
    char csvpath[1100];
    char tripath[1100];
    uint32_t *w = NULL;
    uint8_t *main_data = NULL;
    uint8_t *poly = NULL;
    size_t main_size = 0u;
    size_t poly_size = 0u;
    uint8_t *fb = NULL;
    double *zb = NULL;
    vf2_model_vertex *vb = NULL;
    vf2_model_vertex *vb2 = NULL;
    size_t vcap = 200000u;
    uint32_t readstart[GS_MAX_FRAMES];
    size_t nframes = 0u;
    size_t pose;
    size_t ptr;
    size_t opc = 0u;
    float mat[12];
    uint32_t mode = 0u;
    double fx = 1.0;
    double fy = 1.0;
    double cx = 0.0;
    double cy = 0.0;
    int have_focus = 0;
    int have_win = 0;
    size_t nobj = 0u;
    size_t ntri = 0u;
    int rc = EXIT_FAILURE;
    vf2_status st;
    size_t i;

    pose = (size_t)strtoul(pose_str, NULL, 0);
    memset(mat, 0, sizeof(mat));

    txt = fopen(txt_path, "rb");
    if (txt == NULL) {
        fprintf(stderr, "geo-project: cannot open %s\n", txt_path);
        return EXIT_FAILURE;
    }
    while (fgets(line, (int)sizeof(line), txt) != NULL) {
        unsigned idx;
        unsigned fn;
        unsigned rs;
        if (sscanf(line, "F %u %u %x", &idx, &fn, &rs) == 3 &&
            idx < GS_MAX_FRAMES) {
            readstart[idx] = rs;
            if ((size_t)idx + 1u > nframes) nframes = (size_t)idx + 1u;
        }
    }
    fclose(txt);
    txt = NULL;
    if (pose >= nframes) {
        fprintf(stderr, "geo-project: pose frame %zu >= %zu\n", pose, nframes);
        return EXIT_FAILURE;
    }

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data,
                                 &main_size);
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly,
                                     &poly_size);
    }
    if (st != VF2_OK) {
        fprintf(stderr, "geo-project: ROM regions: %s\n",
                vf2_status_string(st));
        goto done;
    }

    bin = fopen(bin_path, "rb");
    w = (uint32_t *)malloc(frame_words * sizeof(*w));
    fb = (uint8_t *)malloc((size_t)W * (size_t)H * 3u);
    zb = (double *)malloc((size_t)W * (size_t)H * sizeof(*zb));
    vb = (vf2_model_vertex *)malloc(vcap * sizeof(*vb));
    vb2 = (vf2_model_vertex *)malloc(vcap * sizeof(*vb2));
    if (bin == NULL || w == NULL || fb == NULL || zb == NULL || vb == NULL ||
        vb2 == NULL) {
        fprintf(stderr, "geo-project: open/alloc failed\n");
        goto done;
    }
    if (!gs_read_frame(bin, pose, frame_bytes, w, frame_words)) {
        fprintf(stderr, "geo-project: cannot read frame %zu\n", pose);
        goto done;
    }
    for (i = 0u; i < (size_t)W * (size_t)H; ++i) {
        fb[i * 3u + 0u] = 18u;   /* dark background */
        fb[i * 3u + 1u] = 18u;
        fb[i * 3u + 2u] = 24u;
        zb[i] = 1e30;
    }

    snprintf(csvpath, sizeof(csvpath), "%s.csv", out_path);
    csv = fopen(csvpath, "wb");
    if (csv != NULL) {
        fprintf(csv, "obj,rec_byte,group,side,mode3,cx,cy,minx,miny,maxx,"
                     "maxy,screen_h,ntri,rawext,c0n,c1n,c2n,zmin,zmax,"
                     "tx,ty,tz\n");
    }
    /* per-triangle projected dump (screen coords) so an external overlay can
     * draw each object's actual contour -- not just its bbox. */
    snprintf(tripath, sizeof(tripath), "%s.tris", out_path);
    tri = fopen(tripath, "wb");
    if (tri != NULL) {
        fprintf(tri, "obj,rec_byte,group,x0,y0,x1,y1,x2,y2\n");
    }

    ptr = (size_t)((readstart[pose] & 0x1ffffu) >> 2);
    for (;;) {
        uint32_t op;
        uint32_t cmd;
        size_t need = 0u;
        if (opc++ >= 0x40000u || ptr >= frame_words) break;
        op = w[ptr++];
        if (op & 0x80000000u) { ptr = (size_t)((op & 0x1ffffu) >> 2); continue; }
        cmd = (op >> 23) & 0x1fu;
        switch (cmd) {
        case 0x00u: need = 0u; break;
        case 0x07u: case 0x17u: case 0x08u: case 0x18u:
        case 0x10u: case 0x16u: case 0x1eu: need = 1u; break;
        case 0x09u: case 0x19u: case 0x0du: need = 2u; break;
        case 0x0au: case 0x1au: case 0x0cu: case 0x1cu: need = 3u; break;
        case 0x03u: case 0x13u: need = 6u; break;
        case 0x0bu: case 0x1bu: need = 12u; break;
        case 0x01u: case 0x11u: need = 4u; break;
        case 0x04u: case 0x14u: case 0x05u: case 0x15u:
            if (ptr + 2u > frame_words) goto walk_end;
            need = 2u + (size_t)w[ptr + 1u]; break;
        case 0x06u:
            if (ptr + 2u > frame_words) goto walk_end;
            need = 2u + (size_t)w[ptr + 1u] * 2u; break;
        case 0x1du:
            if (ptr + 2u > frame_words) goto walk_end;
            need = 2u + (size_t)w[ptr + 1u] * 3u; break;
        case 0x02u: case 0x12u: {
            size_t p = ptr + 8u;
            for (;;) {
                uint32_t attr;
                if (p >= frame_words) goto walk_end;
                attr = w[p++];
                if ((attr & 3u) == 0u) break;
                p += 5u;
                if (attr & 1u) p += 3u;
            }
            need = p - ptr; break;
        }
        case 0x0eu:
            if (ptr + 33u > frame_words) goto walk_end;
            need = 32u + 1u + (size_t)w[ptr + 32u] * 3u; break;
        case 0x0fu: case 0x1fu: goto walk_end;
        default: goto walk_end;
        }
        if (ptr + need > frame_words) goto walk_end;

        if (cmd == 0x07u || cmd == 0x17u) {
            mode = w[ptr];
        } else if (cmd == 0x0bu || cmd == 0x1bu) {
            size_t k;
            for (k = 0u; k < 12u; ++k) {
                uint32_t u = w[ptr + k];
                memcpy(&mat[k], &u, 4u);
            }
        } else if (cmd == 0x0cu || cmd == 0x1cu) {
            uint32_t u;
            u = w[ptr + 0u]; memcpy(&mat[9], &u, 4u);
            u = w[ptr + 1u]; memcpy(&mat[10], &u, 4u);
            u = w[ptr + 2u]; memcpy(&mat[11], &u, 4u);
        } else if (cmd == 0x09u || cmd == 0x19u) {
            uint32_t u;
            float f;
            u = w[ptr + 0u]; memcpy(&f, &u, 4u); fx = (double)f;
            u = w[ptr + 1u]; memcpy(&f, &u, 4u); fy = (double)f;
            have_focus = 1;
        } else if (cmd == 0x03u || cmd == 0x13u) {
            uint32_t c = w[ptr + 2u];      /* track the CURRENT window center0 */
            int icx = (int)((c >> 16) & 0xfffu);
            int icy = (int)(c & 0xfffu);
            if (icx & 0x800) icx = -(0x800 - (icx & 0x7ff));
            if (icy & 0x800) icy = -(0x800 - (icy & 0x7ff));
            cx = (double)icx;
            cy = (double)icy;
            have_win = 1;
        } else if (cmd == 0x01u || cmd == 0x11u) {
            uint32_t oba = w[ptr + 2u];
            uint32_t rec = (oba & 0x7fffffu) * 4u;
            unsigned m3 = (unsigned)(mode & 3u);
            size_t vn = 0u;
            int sheet = -1;
            size_t tr = 0u;
            uint8_t cr = 150u;
            uint8_t cg = 150u;
            uint8_t cb = 150u;
            const char *og = "decor";
            const char *side = "-";
            int is_decor = (rec >= 0x3c1facu && rec <= 0x3ca730u);
            int is_shadow = (gs_flatness(mat) < GS_SHADOW_EPS);
            if ((size_t)rec + 24u <= poly_size) {
                size_t t;
                size_t e;
                double ox0 = 1e30;
                double oy0 = 1e30;
                double ox1 = -1e30;
                double oy1 = -1e30;
                double sumx = 0.0;
                size_t non = 0u;
                size_t otri = 0u;
                double objcx;
                double zmin = 1e30;
                double zmax = -1e30;
                double rawext = 0.0;
                double c0n;
                double c1n;
                double c2n;
                size_t vn2 = 0u;
                int sheet2 = -1;
                size_t tr2 = 0u;
                double rx0 = 1e30, ry0 = 1e30, rz0 = 1e30;
                double rx1 = -1e30, ry1 = -1e30, rz1 = -1e30;
                vn = 0u;
                (void)ext_emit_piece(NULL, poly, poly_size, NULL, 0u, rec,
                                     0u, 0u, 1u, &sheet, &tr, mat, vb, &vn,
                                     vcap, NULL, 0u, NULL, NULL, NULL, 0u);
                /* raw (identity-posed) vertices, to compare the object's own
                 * vertex scale against its matrix column norms. */
                (void)ext_emit_piece(NULL, poly, poly_size, NULL, 0u, rec,
                                     0u, 0u, 1u, &sheet2, &tr2, NULL, vb2, &vn2,
                                     vcap, NULL, 0u, NULL, NULL, NULL, 0u);
                for (e = 0u; e < vn2; ++e) {
                    double rvx = (double)vb2[e].x;
                    double rvy = (double)vb2[e].y;
                    double rvz = (double)vb2[e].z;
                    if (rvx < rx0) rx0 = rvx;
                    if (rvx > rx1) rx1 = rvx;
                    if (rvy < ry0) ry0 = rvy;
                    if (rvy > ry1) ry1 = rvy;
                    if (rvz < rz0) rz0 = rvz;
                    if (rvz > rz1) rz1 = rvz;
                }
                if (vn2 > 0u) {
                    double dx = rx1 - rx0;
                    double dy = ry1 - ry0;
                    double dz = rz1 - rz0;
                    rawext = dx;
                    if (dy > rawext) rawext = dy;
                    if (dz > rawext) rawext = dz;
                }
                c0n = sqrt((double)(mat[0]*mat[0] + mat[1]*mat[1]
                                    + mat[2]*mat[2]));
                c1n = sqrt((double)(mat[3]*mat[3] + mat[4]*mat[4]
                                    + mat[5]*mat[5]));
                c2n = sqrt((double)(mat[6]*mat[6] + mat[7]*mat[7]
                                    + mat[8]*mat[8]));
                /* pass 1: robust screen centroid from on-screen vertices */
                for (e = 0u; e < vn; ++e) {
                    double X = (double)vb[e].x * fx;
                    double Y = (double)vb[e].y * fy;
                    double Z = (double)vb[e].z;
                    double sx;
                    double sy;
                    if (Z < zmin) zmin = Z;
                    if (Z > zmax) zmax = Z;
                    if (Z < 0.0001) continue;
                    sx = xoff + cx + X / Z;
                    sy = ((double)384.0 - cy) + yoff - Y / Z;
                    if (sx >= 0.0 && sx < (double)W && sy >= 0.0 &&
                        sy < (double)H) {
                        sumx += sx;
                        ++non;
                    }
                }
                objcx = (non > 0u) ? sumx / (double)non : -1.0;
                /* classify: decor grey, shadow dark, else by screen side */
                if (is_decor) {
                    cr = 150u; cg = 150u; cb = 150u; og = "decor";
                } else if (is_shadow) {
                    cr = 40u; cg = 40u; cb = 52u; og = "shadow";
                    side = (objcx >= 0.0 && objcx < (double)(W / 2))
                         ? "L" : "R";
                } else if (non > 0u) {
                    og = "fighter";
                    if (objcx < (double)(W / 2)) {
                        cr = 70u; cg = 130u; cb = 235u; side = "L"; /* Akira */
                    } else {
                        cr = 235u; cg = 110u; cb = 40u; side = "R"; /* Lau */
                    }
                } else {
                    og = "offscreen";
                }
                /* pass 2: fill */
                for (t = 0u; t + 3u <= vn; t += 3u) {
                    double px[3];
                    double py[3];
                    double pz[3];
                    int ok = 1;
                    for (e = 0u; e < 3u; ++e) {
                        double X = (double)vb[t + e].x * fx;
                        double Y = (double)vb[t + e].y * fy;
                        double Z = (double)vb[t + e].z;
                        if (Z < 0.0001) { ok = 0; break; }
                        px[e] = xoff + cx + X / Z;
                        py[e] = ((double)384.0 - cy) + yoff - Y / Z;
                        pz[e] = Z;
                    }
                    if (!ok) continue;
                    for (e = 0u; e < 3u; ++e) {
                        if (px[e] < ox0) ox0 = px[e];
                        if (px[e] > ox1) ox1 = px[e];
                        if (py[e] < oy0) oy0 = py[e];
                        if (py[e] > oy1) oy1 = py[e];
                    }
                    gs_fill_tri(fb, zb, W, H,
                                px[0], py[0], pz[0], px[1], py[1], pz[1],
                                px[2], py[2], pz[2], cr, cg, cb);
                    if (tri != NULL) {
                        fprintf(tri,
                                "%zu,0x%x,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                                nobj, (unsigned)rec, og, px[0], py[0],
                                px[1], py[1], px[2], py[2]);
                    }
                    ++ntri;
                    ++otri;
                }
                if (csv != NULL && otri > 0u) {
                    fprintf(csv, "%zu,0x%x,%s,%s,%u,%.1f,%.1f,%.1f,%.1f,%.1f,"
                                 "%.1f,%.1f,%zu,%.1f,%.4f,%.4f,%.4f,%.2f,%.2f,"
                                 "%.4f,%.4f,%.4f\n",
                            nobj, (unsigned)rec, og,
                            side, m3, objcx, 0.5 * (oy0 + oy1),
                            ox0, oy0, ox1, oy1, oy1 - oy0, otri,
                            rawext, c0n, c1n, c2n, zmin, zmax,
                            (double)mat[9], (double)mat[10], (double)mat[11]);
                }
                ++nobj;
            }
        }
        ptr += need;
    }
walk_end:

    if (!png_write_rgb8(out_path, (uint32_t)W, (uint32_t)H, fb)) {
        fprintf(stderr, "geo-project: cannot write %s\n", out_path);
        goto done;
    }
    printf("geo-project: frame %zu -> %s (%dx%d)\n", pose, out_path, W, H);
    printf("  focus=%s(%.3f,%.3f) window-center0=%s(%.0f,%.0f) xoff=%.0f "
           "yoff=%.0f\n", have_focus ? "" : "default ", fx, fy,
           have_win ? "" : "none ", cx, cy, xoff, yoff);
    printf("  %zu objects projected, %zu triangles filled "
           "(left/Akira blue, right/Lau orange, decor grey, shadow dark)\n",
           nobj, ntri);
    rc = EXIT_SUCCESS;

done:
    if (bin != NULL) fclose(bin);
    if (txt != NULL) fclose(txt);
    if (csv != NULL) fclose(csv);
    if (tri != NULL) fclose(tri);
    free(w);
    free(main_data);
    free(poly);
    free(fb);
    free(zb);
    free(vb);
    free(vb2);
    return rc;
}

/* --- decode-texture-sheet: a texture-RAM bank -> grayscale PGM ----------------
 *
 * Reads a raw texture bank (local-only, e.g. a MAME capture of texture RAM
 * 0x12000000 / 0x12400000), decodes the 4-bit luma-index texels with
 * vf2_texture_sheet_decode, and writes an 8-bit grayscale binary PGM (P5), one
 * pixel = index*17. Input and output stay local; this never touches the repo.
 * CI exercises the decoder on synthetic data (tests/platform/test_texture_sheet.c),
 * not this file-I/O command. */
static int command_decode_texture_sheet(
    const char *bank_path,
    const char *width_arg,
    const char *height_arg,
    const char *out_path
)
{
    FILE *in;
    FILE *out;
    uint8_t *bank;
    uint8_t *index;
    long bank_size;
    size_t read_count;
    size_t texel_count;
    size_t i;
    uint32_t width = 0u;
    uint32_t height = 0u;
    vf2_status status;

    if (!parse_u32(width_arg, &width) || !parse_u32(height_arg, &height)) {
        fprintf(stderr, "decode-texture-sheet: bad width/height\n");
        return EXIT_FAILURE;
    }

    in = fopen(bank_path, "rb");
    if (in == NULL) {
        fprintf(stderr, "decode-texture-sheet: cannot open %s\n", bank_path);
        return EXIT_FAILURE;
    }
    if (fseek(in, 0, SEEK_END) != 0 || (bank_size = ftell(in)) < 0 ||
        fseek(in, 0, SEEK_SET) != 0) {
        fprintf(stderr, "decode-texture-sheet: cannot size %s\n", bank_path);
        fclose(in);
        return EXIT_FAILURE;
    }
    bank = (uint8_t *)malloc((size_t)bank_size);
    if (bank == NULL) {
        fprintf(stderr, "decode-texture-sheet: out of memory\n");
        fclose(in);
        return EXIT_FAILURE;
    }
    read_count = fread(bank, 1u, (size_t)bank_size, in);
    fclose(in);
    if (read_count != (size_t)bank_size) {
        fprintf(stderr, "decode-texture-sheet: short read on %s\n", bank_path);
        free(bank);
        return EXIT_FAILURE;
    }

    texel_count = (size_t)width * (size_t)height;
    index = (uint8_t *)malloc(texel_count != 0u ? texel_count : 1u);
    if (index == NULL) {
        fprintf(stderr, "decode-texture-sheet: out of memory\n");
        free(bank);
        return EXIT_FAILURE;
    }

    status = vf2_texture_sheet_decode(bank, (size_t)bank_size, width, height,
                                      index, texel_count, NULL);
    free(bank);
    if (status != VF2_OK) {
        fprintf(stderr,
                "decode-texture-sheet: decode failed (status %d); a %ux%u "
                "sheet needs width a multiple of 8 and a bank of >= %zu bytes\n",
                (int)status, (unsigned)width, (unsigned)height,
                texel_count / 2u);
        free(index);
        return EXIT_FAILURE;
    }

    out = fopen(out_path, "wb");
    if (out == NULL) {
        fprintf(stderr, "decode-texture-sheet: cannot write %s\n", out_path);
        free(index);
        return EXIT_FAILURE;
    }
    fprintf(out, "P5\n%u %u\n255\n", (unsigned)width, (unsigned)height);
    for (i = 0u; i < texel_count; ++i) {
        index[i] = vf2_texture_sheet_index_to_gray(index[i]);
    }
    if (fwrite(index, 1u, texel_count, out) != texel_count) {
        fprintf(stderr, "decode-texture-sheet: short write on %s\n", out_path);
        fclose(out);
        free(index);
        return EXIT_FAILURE;
    }
    fclose(out);
    free(index);
    printf("decode-texture-sheet: wrote %ux%u grayscale PGM to %s\n",
           (unsigned)width, (unsigned)height, out_path);
    return EXIT_SUCCESS;
}

/* --- extract-models: decode a character's mesh from a MAME fight capture ------
 *
 * Reads the capture's geo-0x00800010 object words (= object-table w0), resolves
 * each to a polygon-ROM model record via the object table (0x020e0004), decodes it
 * with vf2_model_decode_record, clusters pieces into characters by object-id
 * proximity, and writes one Wavefront OBJ per character (playerN.obj) plus
 * scene.obj into <out-dir>. The OBJ geometry is untextured (slice 4). All inputs
 * and outputs are local; nothing is committed. */

typedef struct extract_piece {
    uint32_t sub_order;
    uint32_t object_id;
    uint32_t byte_offset;
    uint32_t bank;
} extract_piece;

typedef struct extract_cluster {
    size_t start;
    size_t end;
    uint32_t min_sub;
} extract_cluster;

static int extract_write_obj(
    const char *path, const uint8_t *poly, size_t poly_size,
    const extract_piece *pieces, size_t piece_count, size_t *total_tris_out
)
{
    FILE *f = fopen(path, "wb");
    size_t vbase = 1u; /* OBJ is 1-indexed */
    size_t total_tris = 0u;
    size_t i = 0u;
    const size_t max_tris = 200000u;
    vf2_model_triangle *tris =
        (vf2_model_triangle *)malloc(max_tris * sizeof(*tris));
    if (f == NULL || tris == NULL) {
        if (f != NULL) fclose(f);
        free(tris);
        return 0;
    }
    fprintf(f, "# VF2 extracted mesh (untextured, slice 4). Local only, never committed.\n");
    for (i = 0u; i < piece_count; ++i) {
        size_t n = 0u;
        size_t t = 0u;
        if (vf2_model_decode_record(
                poly, poly_size, pieces[i].byte_offset, 1, tris, max_tris, &n
            ) != VF2_OK) {
            continue;
        }
        fprintf(f, "o piece_%03u_id_%03x\n",
                (unsigned)pieces[i].sub_order, (unsigned)pieces[i].object_id);
        for (t = 0u; t < n; ++t) {
            fprintf(f, "v %.6f %.6f %.6f\n",
                    (double)tris[t].v[0].x, (double)tris[t].v[0].y, (double)tris[t].v[0].z);
            fprintf(f, "v %.6f %.6f %.6f\n",
                    (double)tris[t].v[1].x, (double)tris[t].v[1].y, (double)tris[t].v[1].z);
            fprintf(f, "v %.6f %.6f %.6f\n",
                    (double)tris[t].v[2].x, (double)tris[t].v[2].y, (double)tris[t].v[2].z);
        }
        for (t = 0u; t < n; ++t) {
            const size_t a = vbase + t * 3u;
            fprintf(f, "f %zu %zu %zu\n", a, a + 1u, a + 2u);
        }
        vbase += n * 3u;
        total_tris += n;
    }
    fclose(f);
    free(tris);
    if (total_tris_out != NULL) {
        *total_tris_out = total_tris;
    }
    return 1;
}

/* Write one piece's mesh to its own OBJ, centered on the origin (for per-piece
 * decode validation). Returns the triangle count, or 0 on failure/empty. */
static size_t extract_write_piece(
    const char *path, const uint8_t *poly, size_t poly_size, uint32_t byte_offset
)
{
    const size_t max_tris = 65536u;
    vf2_model_triangle *tris =
        (vf2_model_triangle *)malloc(max_tris * sizeof(*tris));
    size_t n = 0u;
    size_t t = 0u;
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    float lo[3], hi[3];
    FILE *f = NULL;
    if (tris == NULL) {
        return 0u;
    }
    if (vf2_model_decode_record(poly, poly_size, byte_offset, 1, tris, max_tris, &n)
            != VF2_OK || n == 0u) {
        free(tris);
        return 0u;
    }
    lo[0] = hi[0] = tris[0].v[0].x;
    lo[1] = hi[1] = tris[0].v[0].y;
    lo[2] = hi[2] = tris[0].v[0].z;
    for (t = 0u; t < n; ++t) {
        size_t k = 0u;
        for (k = 0u; k < 3u; ++k) {
            const float c[3] = { tris[t].v[k].x, tris[t].v[k].y, tris[t].v[k].z };
            size_t a = 0u;
            for (a = 0u; a < 3u; ++a) {
                if (c[a] < lo[a]) lo[a] = c[a];
                if (c[a] > hi[a]) hi[a] = c[a];
            }
        }
    }
    cx = (lo[0] + hi[0]) * 0.5f;
    cy = (lo[1] + hi[1]) * 0.5f;
    cz = (lo[2] + hi[2]) * 0.5f;
    f = fopen(path, "wb");
    if (f == NULL) {
        free(tris);
        return 0u;
    }
    fprintf(f, "# VF2 piece mesh, centered. Local only, never committed.\n");
    for (t = 0u; t < n; ++t) {
        size_t k = 0u;
        for (k = 0u; k < 3u; ++k) {
            fprintf(f, "v %.6f %.6f %.6f\n",
                    (double)(tris[t].v[k].x - cx),
                    (double)(tris[t].v[k].y - cy),
                    (double)(tris[t].v[k].z - cz));
        }
    }
    for (t = 0u; t < n; ++t) {
        const size_t a = 1u + t * 3u;
        fprintf(f, "f %zu %zu %zu\n", a, a + 1u, a + 2u);
    }
    fclose(f);
    free(tris);
    return n;
}

static int command_extract_models(
    const char *rom_directory, const char *capture_path, const char *out_dir
)
{
    uint8_t *main_data = NULL;
    uint8_t *poly = NULL;
    size_t main_size = 0u;
    size_t poly_size = 0u;
    FILE *cap = NULL;
    extract_piece *pieces = NULL;
    size_t piece_count = 0u;
    size_t piece_cap = 0u;
    char line[256];
    uint32_t sub = 0u;
    vf2_status st = VF2_OK;

    st = vf2_romset_build_region(rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_size);
    if (st == VF2_OK) {
        st = vf2_romset_build_region(rom_directory, VF2_REGION_POLYGONS, &poly, &poly_size);
    }
    if (st != VF2_OK) {
        fprintf(stderr, "Could not build ROM regions: %s\n", vf2_status_string(st));
        free(main_data);
        return EXIT_FAILURE;
    }
    cap = fopen(capture_path, "rb");
    if (cap == NULL) {
        fprintf(stderr, "Could not open capture %s\n", capture_path);
        free(main_data);
        free(poly);
        return EXIT_FAILURE;
    }
    /* Collect geo 0x00800010 object words from the first captured frame. */
    while (fgets(line, (int)sizeof(line), cap) != NULL) {
        char frame[16];
        char region[16];
        unsigned seq = 0u;
        unsigned addr = 0u;
        unsigned data = 0u;
        uint32_t byte_off = 0u;
        uint32_t bank = 0u;
        uint32_t id = 0u;
        if (line[0] == '#') {
            continue;
        }
        if (sscanf(line, "%u %15s %15s %x %x", &seq, frame, region, &addr, &data) != 5) {
            continue;
        }
        if (strcmp(frame, "f1") != 0 || strcmp(region, "geo") != 0 ||
            addr != 0x00800010u) {
            continue;
        }
        if (vf2_model_lookup_w0(main_data, main_size, (uint32_t)data, 0x2000u,
                                &byte_off, &bank, &id) != VF2_OK) {
            continue;
        }
        if (piece_count == piece_cap) {
            size_t ncap = piece_cap == 0u ? 128u : piece_cap * 2u;
            extract_piece *np =
                (extract_piece *)realloc(pieces, ncap * sizeof(*pieces));
            if (np == NULL) {
                break;
            }
            pieces = np;
            piece_cap = ncap;
        }
        pieces[piece_count].sub_order = sub++;
        pieces[piece_count].object_id = id;
        pieces[piece_count].byte_offset = byte_off;
        pieces[piece_count].bank = bank;
        ++piece_count;
    }
    fclose(cap);
    printf("Resolved %zu object pieces from %s\n", piece_count, capture_path);

    /* Per-piece OBJ (centered on origin) for decode validation. */
    {
        size_t pi = 0u;
        char ppath[1024];
        for (pi = 0u; pi < piece_count; ++pi) {
            size_t tris = 0u;
            snprintf(ppath, sizeof(ppath), "%s/piece_%03zu_id%03x.obj", out_dir,
                     pi, (unsigned)pieces[pi].object_id);
            tris = extract_write_piece(ppath, poly, poly_size,
                                       pieces[pi].byte_offset);
            if (tris != 0u && pi < 8u) {
                printf("  piece %zu id 0x%x: %zu tris -> %s\n", pi,
                       (unsigned)pieces[pi].object_id, tris, ppath);
            }
        }
    }

    /* A character's model records are contiguous in the polygon ROM, so cluster by
     * polygon-ROM byte offset (gap > 0x8000 = new character). A character = a
     * cluster with >= 4 pieces; players are numbered by first submission order. */
    {
        size_t idx_sorted[4096];
        size_t used = piece_count < 4096u ? piece_count : 4096u;
        size_t i = 0u;
        char path[1024];
        size_t scene_tris = 0u;
        /* qualifying clusters, recorded as [start,end) into idx_sorted + min order */
        extract_cluster clusters[256];
        size_t cluster_count = 0u;
        for (i = 0u; i < used; ++i) {
            idx_sorted[i] = i;
        }
        /* insertion sort by byte_offset (piece counts are small) */
        for (i = 1u; i < used; ++i) {
            size_t key = idx_sorted[i];
            size_t j = i;
            while (j > 0u &&
                   pieces[idx_sorted[j - 1u]].byte_offset > pieces[key].byte_offset) {
                idx_sorted[j] = idx_sorted[j - 1u];
                --j;
            }
            idx_sorted[j] = key;
        }
        i = 0u;
        while (i < used) {
            size_t start = i;
            uint32_t min_sub = pieces[idx_sorted[i]].sub_order;
            ++i;
            while (i < used &&
                   pieces[idx_sorted[i]].byte_offset <=
                       pieces[idx_sorted[i - 1u]].byte_offset + 0x8000u) {
                if (pieces[idx_sorted[i]].sub_order < min_sub) {
                    min_sub = pieces[idx_sorted[i]].sub_order;
                }
                ++i;
            }
            if (i - start >= 4u && cluster_count < 256u) {
                clusters[cluster_count].start = start;
                clusters[cluster_count].end = i;
                clusters[cluster_count].min_sub = min_sub;
                ++cluster_count;
            }
        }
        /* order characters by first submission */
        for (i = 1u; i < cluster_count; ++i) {
            size_t a = 0u;
            for (a = i; a > 0u && clusters[a - 1u].min_sub > clusters[a].min_sub; --a) {
                extract_cluster tmp = clusters[a - 1u];
                clusters[a - 1u] = clusters[a];
                clusters[a] = tmp;
            }
        }
        for (i = 0u; i < cluster_count; ++i) {
            extract_piece cluster[512];
            size_t ccount = 0u;
            size_t k = 0u;
            size_t tris = 0u;
            for (k = clusters[i].start; k < clusters[i].end && ccount < 512u; ++k) {
                cluster[ccount++] = pieces[idx_sorted[k]];
            }
            snprintf(path, sizeof(path), "%s/player%zu.obj", out_dir, i + 1u);
            if (extract_write_obj(path, poly, poly_size, cluster, ccount, &tris)) {
                printf("  player%zu: %zu pieces, poly 0x%x..0x%x -> %s (%zu tris)\n",
                       i + 1u, ccount, (unsigned)cluster[0].byte_offset,
                       (unsigned)cluster[ccount - 1u].byte_offset, path, tris);
            }
        }
        snprintf(path, sizeof(path), "%s/scene.obj", out_dir);
        if (extract_write_obj(path, poly, poly_size, pieces, piece_count, &scene_tris)) {
            printf("  scene: %zu pieces -> %s (%zu tris)\n", piece_count, path, scene_tris);
        }
    }

    free(pieces);
    free(main_data);
    free(poly);
    return EXIT_SUCCESS;
}

static int command_boot_trace(
    const char *rom_directory, uint32_t frames, uint32_t read_cap, uint32_t apply_io_fix
)
{
    const uint64_t per_frame_budget = UINT64_C(50000000);
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_frame_wait_state frame_wait;
    boot_trace_ctx ctx;
    uint32_t frame = 0u;
    int hung = 0;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&ctx, 0, sizeof(ctx));

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(rom_directory, &machine, image, image_size);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_reset_from_machine(
            &cpu, &machine, vectors.system_address_table,
            vectors.initial_prcb, vectors.start_ip);
    }
    if (status == VF2_OK) status = vf2_hybrid_frame_wait_initialize(&frame_wait, 4u);
    if (status != VF2_OK) {
        fprintf(stderr, "boot-trace setup failed: %s\n", vf2_status_string(status));
        goto done;
    }

    if (apply_io_fix) {
        /* Apply the proven IO_CONTROL fix (io_control[0x1c] = 0xff open-bus)
         * that makes the boot match MAME; see
         * decomp/i960/notes/attract_phase_50002a_divergence.md. Done here (not
         * in vf2_model2a_initialize) so the default model stays byte-compatible
         * with the recovered native runtime, whose boot bridges do not yet cover
         * the corrected (3D-active) path. */
        const uint8_t open_bus = 0xffu;
        status = vf2_model2a_write(&machine, 0x01c0001cu, &open_bus, 1u);
        if (status != VF2_OK) {
            fprintf(stderr, "boot-trace: could not apply io fix: %s\n",
                    vf2_status_string(status));
            goto done;
        }
    }

    ctx.cpu = &cpu;
    ctx.frame = &frame;
    ctx.read_884_cap = read_cap;
    ctx.first_geo_frame = 0xffffffffu;
    vf2_model2a_set_memory_observer(&machine, boot_trace_observer, &ctx);

    printf("boot-trace: pure interpreter from reset (start_ip=0x%08x), up to "
           "%u frames%s; tracing 0x50002a writes, 0x884000 reads (cap %u), "
           "0x98000x TGP handshake.\n",
           (unsigned)vectors.start_ip, (unsigned)frames,
           apply_io_fix ? " [io_control[0x1c]=0xff fix APPLIED]" : "",
           (unsigned)read_cap);

    while (status == VF2_OK && frame < frames) {
        uint64_t steps = 0u;
        for (;;) {
            vf2_hybrid_frame_wait_report wait_report;
            status = vf2_hybrid_frame_wait_observe(
                &machine, &cpu, &frame_wait, &wait_report);
            if (status != VF2_OK) break;
            if (wait_report.interrupt_injected) { ++frame; break; }
            status = vf2_i960_step(&cpu, &machine, NULL);
            if (status != VF2_OK) break;
            if (++steps >= per_frame_budget) { hung = 1; break; }
        }
        if (status != VF2_OK || hung) break;
    }

    vf2_model2a_set_memory_observer(&machine, NULL, NULL);
    if (hung) {
        printf("\nboot-trace: STUCK at frame %u, ip=0x%08x after %llu steps "
               "(interpreter from reset does not reach the attract phase loop "
               "unaided).\n",
               (unsigned)frame, (unsigned)cpu.ip,
               (unsigned long long)per_frame_budget);
    } else if (status == VF2_OK) {
        uint8_t sel = 0u, a6 = 0u;
        (void)vf2_model2a_read(&machine, 0x0050002au, &sel, 1u);
        (void)vf2_model2a_read(&machine, 0x005000a6u, &a6, 1u);
        printf("\nboot-trace: %u frames, ip=0x%08x, 0x50002a=0x%02x, "
               "0x5000a6=0x%02x, total 0x884000 reads=%u.\n",
               (unsigned)frame, (unsigned)cpu.ip, (unsigned)sel, (unsigned)a6,
               (unsigned)ctx.total_884);
        printf("  geometry-program writes (0x804000+)=%u, fifo writes "
               "(0x884000+)=%u, first geo frame=%s.\n",
               (unsigned)ctx.geo_prg_writes, (unsigned)ctx.fifo_writes,
               ctx.first_geo_frame == 0xffffffffu ? "none"
                   : "see above");
    } else {
        fprintf(stderr, "boot-trace failed at frame %u ip=0x%08x: %s\n",
                (unsigned)frame, (unsigned)cpu.ip, vf2_status_string(status));
    }

done:
    vf2_model2a_shutdown(&machine);
    free(image);
    return (status == VF2_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_coverage(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t frames,
    const char *output_path,
    const coverage_input_script *script
)
{
    const uint64_t per_frame_budget = UINT64_C(50000000);
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot snapshot;
    vf2_hybrid_frame_wait_state frame_wait;
    uint32_t *counts = NULL;
    size_t words = 0u;
    uint64_t executed = 0u, out_of_rom = 0u, distinct = 0u;
    uint32_t frame = 0u;
    FILE *out = NULL;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    vf2_i960_snapshot_init(&snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(rom_directory, &machine, image, image_size);
    }
    if (status == VF2_OK) status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    if (status == VF2_OK) status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    if (status == VF2_OK) status = vf2_hybrid_frame_wait_initialize(&frame_wait, 4u);
    if (status == VF2_OK) {
        words = image_size / 4u;
        counts = (uint32_t *)calloc(words, sizeof(*counts));
        if (counts == NULL) status = VF2_ERROR_OUT_OF_MEMORY;
    }
    while (status == VF2_OK && frame < frames) {
        uint64_t steps = 0u;
        if (script != NULL) {
            size_t i;
            uint32_t mask = 0u;
            int have = 0;
            for (i = 0u; i < script->count; ++i) {
                if (script->frame[i] <= frame) { mask = script->mask[i]; have = 1; }
            }
            if (have) {
                status = vf2_model2a_set_input(&machine, mask);
                if (status != VF2_OK) break;
            }
        }
        for (;;) {
            vf2_hybrid_frame_wait_report wait_report;
            uint32_t ip;
            status = vf2_hybrid_frame_wait_observe(
                &machine, &cpu, &frame_wait, &wait_report
            );
            if (status != VF2_OK) break;
            if (wait_report.interrupt_injected) {
                ++frame;
                break;
            }
            ip = cpu.ip;
            if (ip < image_size) {
                if (counts[ip / 4u] == 0u) ++distinct;
                ++counts[ip / 4u];
            } else {
                ++out_of_rom;
            }
            ++executed;
            status = vf2_i960_step(&cpu, &machine, NULL);
            if (status != VF2_OK) break;
            if (++steps >= per_frame_budget) { status = VF2_ERROR_UNSUPPORTED; break; }
        }
    }
    if (status == VF2_OK) {
        out = fopen(output_path, "wb");
        if (out == NULL) status = VF2_ERROR_IO;
    }
    if (status == VF2_OK) {
        int first = 1;
        size_t w;
        fprintf(out, "{\n");
        fprintf(out, "  \"frames\": %u,\n", (unsigned)frame);
        fprintf(out, "  \"executed\": %llu,\n", (unsigned long long)executed);
        fprintf(out, "  \"out_of_rom\": %llu,\n", (unsigned long long)out_of_rom);
        fprintf(out, "  \"distinct_words\": %llu,\n", (unsigned long long)distinct);
        fprintf(out, "  \"counts\": {");
        for (w = 0u; w < words; ++w) {
            if (counts[w] == 0u) continue;
            fprintf(out, "%s\n    \"0x%08x\": %u", first ? "" : ",",
                    (unsigned)(w * 4u), (unsigned)counts[w]);
            first = 0;
        }
        fprintf(out, "%s  }\n}\n", first ? "" : "\n");
        if (fclose(out) != 0) status = VF2_ERROR_IO;
    }
    if (status == VF2_OK) {
        printf("coverage: %u frames, %llu instructions, %llu distinct words "
               "(%llu out of ROM) -> %s\n",
               (unsigned)frame, (unsigned long long)executed,
               (unsigned long long)distinct, (unsigned long long)out_of_rom,
               output_path);
    } else {
        fprintf(stderr, "coverage failed: %s\n", vf2_status_string(status));
    }
    free(counts);
    vf2_i960_snapshot_destroy(&snapshot);
    vf2_model2a_shutdown(&machine);
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
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

    if (strcmp(argv[1], "coverage") == 0 && (argc == 6 || argc == 7)) {
        uint32_t frames = 0u;
        coverage_input_script script;
        if (!parse_u32(argv[4], &frames) || frames == 0u) {
            fprintf(stderr, "Invalid frame count: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        if (!parse_input_script(argc == 7 ? argv[6] : NULL, &script)) {
            fprintf(stderr, "Invalid input script: %s\n", argv[6]);
            return EXIT_FAILURE;
        }
        return command_coverage(argv[2], argv[3], frames, argv[5], &script);
    }

    if (strcmp(argv[1], "geometry-census") == 0 && argc == 5) {
        uint32_t frames = 0u;
        if (!parse_u32(argv[4], &frames) || frames == 0u) {
            fprintf(stderr, "Invalid frame count: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        return command_geometry_census(argv[2], argv[3], frames);
    }

    if (strcmp(argv[1], "attract-irq-probe") == 0 && (argc == 5 || argc == 6)) {
        uint32_t frames = 0u;
        uint32_t inject = 0u;
        if (!parse_u32(argv[4], &frames) || frames == 0u) {
            fprintf(stderr, "Invalid frame count: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        if (argc == 6 && !parse_u32(argv[5], &inject)) {
            fprintf(stderr, "Invalid inject flag: %s\n", argv[5]);
            return EXIT_FAILURE;
        }
        return command_attract_irq_probe(argv[2], argv[3], frames, inject);
    }

    if (strcmp(argv[1], "boot-trace") == 0 &&
        (argc == 4 || argc == 5 || argc == 6)) {
        uint32_t frames = 0u;
        uint32_t read_cap = 80u;
        uint32_t apply_io_fix = 0u;
        if (!parse_u32(argv[3], &frames) || frames == 0u) {
            fprintf(stderr, "Invalid frame count: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
        if (argc >= 5 && !parse_u32(argv[4], &read_cap)) {
            fprintf(stderr, "Invalid read cap: %s\n", argv[4]);
            return EXIT_FAILURE;
        }
        if (argc == 6 && !parse_u32(argv[5], &apply_io_fix)) {
            fprintf(stderr, "Invalid io-fix flag: %s\n", argv[5]);
            return EXIT_FAILURE;
        }
        return command_boot_trace(argv[2], frames, read_cap, apply_io_fix);
    }

    if (strcmp(argv[1], "read-words") == 0 && argc == 5) {
        uint32_t address = 0u;
        uint32_t count = 0u;
        if (!parse_u32(argv[3], &address) || !parse_u32(argv[4], &count) ||
            count == 0u) {
            fprintf(stderr, "Invalid address or count\n");
            return EXIT_FAILURE;
        }
        return command_read_words(argv[2], address, count);
    }

    if (strcmp(argv[1], "extract-models") == 0 && argc == 5) {
        return command_extract_models(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "decode-texture-sheet") == 0 && argc == 6) {
        return command_decode_texture_sheet(argv[2], argv[3], argv[4], argv[5]);
    }

    if (strcmp(argv[1], "texture-link") == 0 && argc == 4) {
        return command_texture_link(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "export-textured") == 0 && argc == 7) {
        return command_export_textured(argv[2], argv[3], argv[4], argv[5],
                                       argv[6]);
    }

    if (strcmp(argv[1], "export-pieces") == 0 && argc == 7) {
        return command_export_pieces(argv[2], argv[3], argv[4], argv[5],
                                     argv[6]);
    }

    if (strcmp(argv[1], "geo-walk") == 0 && argc == 5) {
        return command_geo_walk(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "geo-scene") == 0 && argc == 9) {
        return command_geo_scene(argv[2], argv[3], argv[4], argv[5], argv[6],
                                 argv[7], argv[8]);
    }

    if (strcmp(argv[1], "geo-fighter") == 0 && (argc >= 10 && argc <= 12)) {
        return command_geo_fighter(argv[2], argv[3], argv[4], argv[5], argv[6],
                                   argv[7], argv[8], argv[9],
                                   argc >= 11 ? argv[10] : NULL,
                                   argc >= 12 ? argv[11] : NULL, 0, 0);
    }

    if (strcmp(argv[1], "geo-stage") == 0) {
        int fr = (argc >= 3 && strcmp(argv[2], "--from-rom") == 0);
        int b = fr ? 3 : 2;          /* first positional arg */
        int n = argc - b;            /* positional arg count (7..9) */
        if (n >= 7 && n <= 9) {
            return command_geo_fighter(argv[b], argv[b+1], argv[b+2], argv[b+3],
                                       argv[b+4], argv[b+5], "stage", argv[b+6],
                                       n >= 8 ? argv[b+7] : NULL,
                                       n >= 9 ? argv[b+8] : NULL, 1, fr);
        }
    }

    if (strcmp(argv[1], "geo-colour") == 0 && (argc == 8 || argc == 9)) {
        return command_geo_colour(argv[2], argv[3], argv[4], argv[5], argv[6],
                                  argv[7], argc == 9 ? argv[8] : NULL);
    }

    if (strcmp(argv[1], "geo-texparams") == 0 && argc == 5) {
        return command_geo_texparams(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "geo-project") == 0 && (argc == 7 || argc == 11)) {
        int W = 496;        /* MAME visible screen, to overlay 1:1 */
        int H = 384;
        /* m_crtc_xoffset = 84 + (int16_t)hsync (model2_v.cpp:643): the 84 is a
         * MAME per-game base (comment :69), and VF2's CRTC H output cancels it,
         * so the effective x-offset is 0 -- measured by contour-fitting BOTH the
         * decor and the fighters (Step 2n): at xo=84 each sits ~84px too far
         * right; at xo=0 both register. y keeps 130 (decor dy~0 there). */
        double xo = 0.0;    /* VF2 effective m_crtc_xoffset (84 base + hsync) */
        double yo = 130.0;  /* MAME m_crtc_yoffset (130 + vsync) */
        if (argc == 11) {
            W = atoi(argv[7]);
            H = atoi(argv[8]);
            xo = atof(argv[9]);
            yo = atof(argv[10]);
        }
        if (W < 1 || H < 1 || W > 8192 || H > 8192) {
            fprintf(stderr, "geo-project: bad canvas size\n");
            return EXIT_FAILURE;
        }
        return command_geo_project(argv[2], argv[3], argv[4], argv[5], argv[6],
                                   W, H, xo, yo);
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

    if (strcmp(argv[1], "game-info-child") == 0 && argc == 6) {
        uint32_t return_address = 0u;
        if (!parse_u32(argv[5], &return_address)) {
            fprintf(stderr, "Invalid game-info child return address\n");
            return EXIT_FAILURE;
        }
        return command_game_info_child(
            argv[2], argv[3], argv[4], return_address
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

    if (strcmp(argv[1], "compare-snapshots") == 0 &&
        (argc == 4 || argc == 5 || argc == 6)) {
        int index = 4;
        int dump_registers = 0;
        int dump_ranges = 0;
        for (index = 4; index < argc; ++index) {
            if (strcmp(argv[index], "registers") == 0) dump_registers = 1;
            else if (strcmp(argv[index], "ranges") == 0) dump_ranges = 1;
            else {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        return command_compare_snapshots(
            argv[2], argv[3], dump_registers, dump_ranges
        );
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
