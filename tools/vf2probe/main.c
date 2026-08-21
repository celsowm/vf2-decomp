#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

#define VF2_PROBE_MAX_MUTATIONS 128u
#define VF2_PROBE_MAX_READS 128u

typedef enum vf2_probe_mutation_kind {
    VF2_PROBE_MUTATION_REGISTER = 0,
    VF2_PROBE_MUTATION_U8,
    VF2_PROBE_MUTATION_U16,
    VF2_PROBE_MUTATION_U32
} vf2_probe_mutation_kind;

typedef struct vf2_probe_mutation {
    vf2_probe_mutation_kind kind;
    uint32_t target;
    uint32_t value;
} vf2_probe_mutation;

typedef struct vf2_probe_options {
    const char *rom_directory;
    const char *snapshot_path;
    const char *output_snapshot_path;
    uint32_t stop_address;
    uint64_t max_steps;
    int has_stop_address;
    int trace;
    vf2_probe_mutation mutations[VF2_PROBE_MAX_MUTATIONS];
    size_t mutation_count;
    uint32_t reads_u32[VF2_PROBE_MAX_READS];
    size_t read_u32_count;
} vf2_probe_options;

typedef struct vf2_probe_trace_context {
    int enabled;
} vf2_probe_trace_context;

static void print_usage(FILE *stream, const char *program)
{
    fprintf(
        stream,
        "vf2probe v%s\n"
        "Usage: %s --rom-dir <dir> --snapshot <file> [options]\n"
        "Options:\n"
        "  --until <address>          stop when IP reaches address\n"
        "  --max-steps <count>        instruction limit (default 100000)\n"
        "  --set-reg <reg=value>      mutate r0..r31, g0..g15 or fp\n"
        "  --set-u8 <addr=value>      mutate one byte\n"
        "  --set-u16 <addr=value>     mutate little-endian 16-bit value\n"
        "  --set-u32 <addr=value>     mutate little-endian 32-bit value\n"
        "  --read-u32 <address>       include final 32-bit memory value\n"
        "  --output-snapshot <file>   save the resulting CPU/machine state\n"
        "  --trace                    emit one JSON record per instruction\n",
        VF2_VERSION_STRING,
        program
    );
}

static int parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed = 0u;
    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

static int parse_u32(const char *text, uint32_t *value)
{
    uint64_t parsed = 0u;
    if (!parse_u64(text, &parsed) || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int parse_u32_value(const char *text, uint32_t *value)
{
    char *end = NULL;
    long long parsed = 0;
    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }
    if (text[0] != '-') {
        return parse_u32(text, value);
    }
    errno = 0;
    parsed = strtoll(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < (long long)INT32_MIN || parsed > -1) {
        return 0;
    }
    *value = (uint32_t)(int32_t)parsed;
    return 1;
}

static int parse_register(const char *text, uint32_t *index)
{
    uint32_t parsed = 0u;
    if (text == NULL || index == NULL) {
        return 0;
    }
    if (strcmp(text, "fp") == 0) {
        *index = VF2_I960_FP_REGISTER;
        return 1;
    }
    if (text[0] == 'r' && parse_u32(text + 1, &parsed) &&
        parsed < VF2_I960_REGISTER_COUNT) {
        *index = parsed;
        return 1;
    }
    if (text[0] == 'g' && parse_u32(text + 1, &parsed) && parsed < 16u) {
        *index = VF2_I960_G0_REGISTER + parsed;
        return 1;
    }
    return 0;
}

static int parse_assignment(
    const char *text,
    char *left,
    size_t left_size,
    uint32_t *value
)
{
    const char *equals = text != NULL ? strchr(text, '=') : NULL;
    size_t length = 0u;
    if (equals == NULL || equals == text || left == NULL || left_size == 0u ||
        value == NULL) {
        return 0;
    }
    length = (size_t)(equals - text);
    if (length >= left_size) {
        return 0;
    }
    memcpy(left, text, length);
    left[length] = '\0';
    return parse_u32_value(equals + 1, value);
}

static int append_mutation(
    vf2_probe_options *options,
    vf2_probe_mutation_kind kind,
    uint32_t target,
    uint32_t value
)
{
    vf2_probe_mutation *mutation = NULL;
    if (options->mutation_count >= VF2_PROBE_MAX_MUTATIONS) {
        return 0;
    }
    mutation = &options->mutations[options->mutation_count++];
    mutation->kind = kind;
    mutation->target = target;
    mutation->value = value;
    return 1;
}

static int parse_options(int argc, char **argv, vf2_probe_options *options)
{
    int index = 1;
    memset(options, 0, sizeof(*options));
    options->max_steps = UINT64_C(100000);

    while (index < argc) {
        const char *argument = argv[index];
        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0) {
            return -1;
        }
        if (strcmp(argument, "--rom-dir") == 0 && index + 1 < argc) {
            options->rom_directory = argv[++index];
        } else if (strcmp(argument, "--snapshot") == 0 && index + 1 < argc) {
            options->snapshot_path = argv[++index];
        } else if (strcmp(argument, "--output-snapshot") == 0 && index + 1 < argc) {
            options->output_snapshot_path = argv[++index];
        } else if (strcmp(argument, "--until") == 0 && index + 1 < argc) {
            if (!parse_u32(argv[++index], &options->stop_address)) {
                return 0;
            }
            options->has_stop_address = 1;
        } else if (strcmp(argument, "--max-steps") == 0 && index + 1 < argc) {
            if (!parse_u64(argv[++index], &options->max_steps)) {
                return 0;
            }
        } else if (strcmp(argument, "--trace") == 0) {
            options->trace = 1;
        } else if ((strcmp(argument, "--set-reg") == 0 ||
                    strcmp(argument, "--set-u8") == 0 ||
                    strcmp(argument, "--set-u16") == 0 ||
                    strcmp(argument, "--set-u32") == 0) &&
                   index + 1 < argc) {
            char left[64];
            uint32_t target = 0u;
            uint32_t value = 0u;
            vf2_probe_mutation_kind kind = VF2_PROBE_MUTATION_U32;
            if (!parse_assignment(argv[++index], left, sizeof(left), &value)) {
                return 0;
            }
            if (strcmp(argument, "--set-reg") == 0) {
                kind = VF2_PROBE_MUTATION_REGISTER;
                if (!parse_register(left, &target)) {
                    return 0;
                }
            } else {
                if (!parse_u32(left, &target)) {
                    return 0;
                }
                if (strcmp(argument, "--set-u8") == 0) {
                    kind = VF2_PROBE_MUTATION_U8;
                    if (value > UINT8_MAX) {
                        return 0;
                    }
                } else if (strcmp(argument, "--set-u16") == 0) {
                    kind = VF2_PROBE_MUTATION_U16;
                    if (value > UINT16_MAX) {
                        return 0;
                    }
                }
            }
            if (!append_mutation(options, kind, target, value)) {
                return 0;
            }
        } else if (strcmp(argument, "--read-u32") == 0 && index + 1 < argc) {
            if (options->read_u32_count >= VF2_PROBE_MAX_READS ||
                !parse_u32(argv[++index], &options->reads_u32[options->read_u32_count])) {
                return 0;
            }
            ++options->read_u32_count;
        } else {
            return 0;
        }
        ++index;
    }

    return options->rom_directory != NULL && options->snapshot_path != NULL;
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
    status = vf2_model2a_attach_main_rom(machine, main_rom, main_rom_size);
    if (status == VF2_OK) {
        status = vf2_model2a_attach_main_data(machine, main_data, main_data_size);
    }
    if (status != VF2_OK) {
        vf2_model2a_shutdown(machine);
    }
    return status;
}

static vf2_status apply_mutation(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    const vf2_probe_mutation *mutation
)
{
    uint8_t bytes[4];
    if (mutation->kind == VF2_PROBE_MUTATION_REGISTER) {
        cpu->registers[mutation->target] = mutation->value;
        return VF2_OK;
    }
    bytes[0] = (uint8_t)(mutation->value & 0xffu);
    bytes[1] = (uint8_t)((mutation->value >> 8u) & 0xffu);
    bytes[2] = (uint8_t)((mutation->value >> 16u) & 0xffu);
    bytes[3] = (uint8_t)((mutation->value >> 24u) & 0xffu);
    if (mutation->kind == VF2_PROBE_MUTATION_U8) {
        return vf2_model2a_write(machine, mutation->target, bytes, 1u);
    }
    if (mutation->kind == VF2_PROBE_MUTATION_U16) {
        return vf2_model2a_write(machine, mutation->target, bytes, 2u);
    }
    return vf2_model2a_write(machine, mutation->target, bytes, 4u);
}

static void trace_callback(
    const vf2_i960_trace_event *event,
    const vf2_i960_cpu *cpu,
    void *user_data
)
{
    vf2_probe_trace_context *context = (vf2_probe_trace_context *)user_data;
    if (context == NULL || !context->enabled || event == NULL || cpu == NULL) {
        return;
    }
    printf(
        "{\"type\":\"step\",\"step\":%" PRIu64
        ",\"ip_before\":%u,\"ip_after\":%u,\"size\":%u}\n",
        event->step,
        event->ip_before,
        event->ip_after,
        (unsigned)event->instruction.size
    );
}

static void print_final(
    const vf2_probe_options *options,
    const vf2_i960_cpu *cpu,
    const vf2_model2a *machine,
    const vf2_i960_run_result *run_result
)
{
    size_t index = 0u;
    printf(
        "{\"type\":\"final\",\"status\":\"%s\",\"halt_reason\":\"%s\","
        "\"ip\":%u,\"run_instructions\":%" PRIu64
        ",\"executed_instructions\":%" PRIu64
        ",\"procedure_calls\":%" PRIu64 ",\"procedure_returns\":%" PRIu64
        ",\"reads_u32\":[",
        vf2_status_string(run_result->status),
        vf2_i960_halt_reason_name(run_result->halt_reason),
        cpu->ip,
        run_result->executed_instructions,
        cpu->executed_instructions,
        cpu->procedure_calls,
        cpu->procedure_returns
    );
    for (index = 0u; index < options->read_u32_count; ++index) {
        uint32_t value = 0u;
        vf2_status status = vf2_model2a_read_u32(
            machine, options->reads_u32[index], &value
        );
        if (index != 0u) {
            putchar(',');
        }
        if (status == VF2_OK) {
            printf(
                "{\"address\":%u,\"value\":%u}",
                options->reads_u32[index],
                value
            );
        } else {
            printf(
                "{\"address\":%u,\"error\":\"%s\"}",
                options->reads_u32[index],
                vf2_status_string(status)
            );
        }
    }
    puts("]}");
}

static vf2_status write_output_snapshot(
    const char *path,
    const vf2_i960_cpu *cpu,
    const vf2_model2a *machine
)
{
    vf2_i960_snapshot output;
    vf2_status status = VF2_OK;
    if (path == NULL) {
        return VF2_OK;
    }
    vf2_i960_snapshot_init(&output);
    status = vf2_i960_snapshot_capture(&output, cpu, machine);
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_write_file(&output, path);
    }
    vf2_i960_snapshot_destroy(&output);
    return status;
}

int main(int argc, char **argv)
{
    vf2_probe_options options;
    vf2_verify_summary verify_summary;
    vf2_i960_snapshot snapshot;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_options run_options;
    vf2_i960_run_result run_result;
    vf2_probe_trace_context trace_context;
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;
    uint8_t *main_data = NULL;
    size_t main_data_size = 0u;
    size_t index = 0u;
    int machine_initialized = 0;
    int parsed = 0;
    vf2_status status = VF2_OK;

    memset(&verify_summary, 0, sizeof(verify_summary));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&run_options, 0, sizeof(run_options));
    memset(&run_result, 0, sizeof(run_result));
    memset(&trace_context, 0, sizeof(trace_context));
    vf2_i960_snapshot_init(&snapshot);

    parsed = parse_options(argc, argv, &options);
    if (parsed < 0) {
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (parsed == 0) {
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    status = vf2_romset_verify(options.rom_directory, NULL, &verify_summary);
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            options.rom_directory, VF2_REGION_MAINCPU, &main_rom, &main_rom_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            options.rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size
        );
    }
    if (status == VF2_OK) {
        status = initialize_machine(
            &machine, main_rom, main_rom_size, main_data, main_data_size
        );
        machine_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, options.snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }
    for (index = 0u; status == VF2_OK && index < options.mutation_count; ++index) {
        status = apply_mutation(&machine, &cpu, &options.mutations[index]);
    }

    if (status == VF2_OK) {
        trace_context.enabled = options.trace;
        run_options.stop_address = options.has_stop_address ? options.stop_address : UINT32_MAX;
        run_options.max_steps = options.max_steps;
        run_options.stop_on_self_branch = true;
        run_options.trace_callback = options.trace ? trace_callback : NULL;
        run_options.trace_user_data = &trace_context;
        status = vf2_i960_run(&cpu, &machine, &run_options, &run_result);
    } else {
        run_result.status = status;
        run_result.halt_reason = VF2_I960_HALT_NONE;
    }

    if (status == VF2_OK) {
        status = write_output_snapshot(options.output_snapshot_path, &cpu, &machine);
        if (status != VF2_OK) {
            run_result.status = status;
        }
    }

    print_final(&options, &cpu, &machine, &run_result);

    if (machine_initialized) {
        vf2_model2a_shutdown(&machine);
    }
    vf2_i960_snapshot_destroy(&snapshot);
    free(main_data);
    free(main_rom);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
