#include "vf2/i960/snapshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t snapshot_magic[8] = {'V', 'F', '2', 'S', 'N', 'A', 'P', 0};

enum { VF2_SNAPSHOT_REGION_COUNT = 18u };

typedef struct snapshot_region_ref {
    const char *name;
    uint8_t **data;
    size_t *size;
} snapshot_region_ref;

typedef struct snapshot_const_region_ref {
    const char *name;
    const uint8_t *data;
    size_t size;
} snapshot_const_region_ref;

static void writable_regions(vf2_i960_snapshot *snapshot, snapshot_region_ref regions[VF2_SNAPSHOT_REGION_COUNT])
{
    regions[0] = (snapshot_region_ref){"geometry", &snapshot->geometry, &snapshot->geometry_size};
    regions[1] = (snapshot_region_ref){"copro-port", &snapshot->copro_port, &snapshot->copro_port_size};
    regions[2] = (snapshot_region_ref){"work-ram", &snapshot->work_ram, &snapshot->work_ram_size};
    regions[3] = (snapshot_region_ref){"buffer-ram", &snapshot->buffer_ram, &snapshot->buffer_ram_size};
    regions[4] = (snapshot_region_ref){"video-control", &snapshot->video_control, &snapshot->video_control_size};
    regions[5] = (snapshot_region_ref){"cpu-control", &snapshot->cpu_control, &snapshot->cpu_control_size};
    regions[6] = (snapshot_region_ref){"interrupt-control", &snapshot->interrupt_control, &snapshot->interrupt_control_size};
    regions[7] = (snapshot_region_ref){"timers", &snapshot->timers, &snapshot->timers_size};
    regions[8] = (snapshot_region_ref){"tile-ram", &snapshot->tile_ram, &snapshot->tile_ram_size};
    regions[9] = (snapshot_region_ref){"palette-ram", &snapshot->palette_ram, &snapshot->palette_ram_size};
    regions[10] = (snapshot_region_ref){"io-control", &snapshot->io_control, &snapshot->io_control_size};
    regions[11] = (snapshot_region_ref){"backup-sram", &snapshot->backup_sram, &snapshot->backup_sram_size};
    regions[12] = (snapshot_region_ref){"copro-control", &snapshot->copro_control, &snapshot->copro_control_size};
    regions[13] = (snapshot_region_ref){"color-translation", &snapshot->color_translation, &snapshot->color_translation_size};
    regions[14] = (snapshot_region_ref){"texture-ram0", &snapshot->texture_ram0, &snapshot->texture_ram0_size};
    regions[15] = (snapshot_region_ref){"texture-ram1", &snapshot->texture_ram1, &snapshot->texture_ram1_size};
    regions[16] = (snapshot_region_ref){"luma-ram", &snapshot->luma_ram, &snapshot->luma_ram_size};
    regions[17] = (snapshot_region_ref){"system-control", &snapshot->system_control, &snapshot->system_control_size};
}

static void const_regions(const vf2_i960_snapshot *snapshot, snapshot_const_region_ref regions[VF2_SNAPSHOT_REGION_COUNT])
{
    regions[0] = (snapshot_const_region_ref){"geometry", snapshot->geometry, snapshot->geometry_size};
    regions[1] = (snapshot_const_region_ref){"copro-port", snapshot->copro_port, snapshot->copro_port_size};
    regions[2] = (snapshot_const_region_ref){"work-ram", snapshot->work_ram, snapshot->work_ram_size};
    regions[3] = (snapshot_const_region_ref){"buffer-ram", snapshot->buffer_ram, snapshot->buffer_ram_size};
    regions[4] = (snapshot_const_region_ref){"video-control", snapshot->video_control, snapshot->video_control_size};
    regions[5] = (snapshot_const_region_ref){"cpu-control", snapshot->cpu_control, snapshot->cpu_control_size};
    regions[6] = (snapshot_const_region_ref){"interrupt-control", snapshot->interrupt_control, snapshot->interrupt_control_size};
    regions[7] = (snapshot_const_region_ref){"timers", snapshot->timers, snapshot->timers_size};
    regions[8] = (snapshot_const_region_ref){"tile-ram", snapshot->tile_ram, snapshot->tile_ram_size};
    regions[9] = (snapshot_const_region_ref){"palette-ram", snapshot->palette_ram, snapshot->palette_ram_size};
    regions[10] = (snapshot_const_region_ref){"io-control", snapshot->io_control, snapshot->io_control_size};
    regions[11] = (snapshot_const_region_ref){"backup-sram", snapshot->backup_sram, snapshot->backup_sram_size};
    regions[12] = (snapshot_const_region_ref){"copro-control", snapshot->copro_control, snapshot->copro_control_size};
    regions[13] = (snapshot_const_region_ref){"color-translation", snapshot->color_translation, snapshot->color_translation_size};
    regions[14] = (snapshot_const_region_ref){"texture-ram0", snapshot->texture_ram0, snapshot->texture_ram0_size};
    regions[15] = (snapshot_const_region_ref){"texture-ram1", snapshot->texture_ram1, snapshot->texture_ram1_size};
    regions[16] = (snapshot_const_region_ref){"luma-ram", snapshot->luma_ram, snapshot->luma_ram_size};
    regions[17] = (snapshot_const_region_ref){"system-control", snapshot->system_control, snapshot->system_control_size};
}

static void machine_regions(const vf2_model2a *machine, snapshot_const_region_ref regions[VF2_SNAPSHOT_REGION_COUNT])
{
    regions[0] = (snapshot_const_region_ref){"geometry", machine->geometry, machine->geometry_size};
    regions[1] = (snapshot_const_region_ref){"copro-port", machine->copro_port, machine->copro_port_size};
    regions[2] = (snapshot_const_region_ref){"work-ram", machine->work_ram, machine->work_ram_size};
    regions[3] = (snapshot_const_region_ref){"buffer-ram", machine->buffer_ram, machine->buffer_ram_size};
    regions[4] = (snapshot_const_region_ref){"video-control", machine->video_control, machine->video_control_size};
    regions[5] = (snapshot_const_region_ref){"cpu-control", machine->cpu_control, machine->cpu_control_size};
    regions[6] = (snapshot_const_region_ref){"interrupt-control", machine->interrupt_control, machine->interrupt_control_size};
    regions[7] = (snapshot_const_region_ref){"timers", machine->timers, machine->timers_size};
    regions[8] = (snapshot_const_region_ref){"tile-ram", machine->tile_ram, machine->tile_ram_size};
    regions[9] = (snapshot_const_region_ref){"palette-ram", machine->palette_ram, machine->palette_ram_size};
    regions[10] = (snapshot_const_region_ref){"io-control", machine->io_control, machine->io_control_size};
    regions[11] = (snapshot_const_region_ref){"backup-sram", machine->backup_sram, machine->backup_sram_size};
    regions[12] = (snapshot_const_region_ref){"copro-control", machine->copro_control, machine->copro_control_size};
    regions[13] = (snapshot_const_region_ref){"color-translation", machine->color_translation, machine->color_translation_size};
    regions[14] = (snapshot_const_region_ref){"texture-ram0", machine->texture_ram0, machine->texture_ram0_size};
    regions[15] = (snapshot_const_region_ref){"texture-ram1", machine->texture_ram1, machine->texture_ram1_size};
    regions[16] = (snapshot_const_region_ref){"luma-ram", machine->luma_ram, machine->luma_ram_size};
    regions[17] = (snapshot_const_region_ref){"system-control", machine->system_control, machine->system_control_size};
}

static vf2_status copy_region(uint8_t **destination, size_t *destination_size, const uint8_t *source, size_t source_size)
{
    uint8_t *copy = NULL;
    if (destination == NULL || destination_size == NULL || (source == NULL && source_size != 0u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (*destination != NULL && *destination_size == source_size) {
        if (source_size != 0u) {
            memcpy(*destination, source, source_size);
        }
        return VF2_OK;
    }
    if (source_size != 0u) {
        copy = (uint8_t *)malloc(source_size);
        if (copy == NULL) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        memcpy(copy, source, source_size);
    }
    free(*destination);
    *destination = copy;
    *destination_size = source_size;
    return VF2_OK;
}

static int write_bytes(FILE *file, const void *data, size_t size)
{
    return size == 0u || fwrite(data, 1u, size, file) == size;
}

static int read_bytes(FILE *file, void *data, size_t size)
{
    return size == 0u || fread(data, 1u, size, file) == size;
}

static int write_u32(FILE *file, uint32_t value)
{
    uint8_t data[4];
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
    return write_bytes(file, data, sizeof(data));
}

static int write_u64(FILE *file, uint64_t value)
{
    uint8_t data[8];
    size_t index = 0u;
    for (index = 0u; index < sizeof(data); ++index) {
        data[index] = (uint8_t)(value >> (index * 8u));
    }
    return write_bytes(file, data, sizeof(data));
}

static int read_u32(FILE *file, uint32_t *value)
{
    uint8_t data[4];
    if (!read_bytes(file, data, sizeof(data))) {
        return 0;
    }
    *value = (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
             ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
    return 1;
}

static int read_u64(FILE *file, uint64_t *value)
{
    uint8_t data[8];
    size_t index = 0u;
    uint64_t result = 0u;
    if (!read_bytes(file, data, sizeof(data))) {
        return 0;
    }
    for (index = 0u; index < sizeof(data); ++index) {
        result |= (uint64_t)data[index] << (index * 8u);
    }
    *value = result;
    return 1;
}

void vf2_i960_snapshot_init(vf2_i960_snapshot *snapshot)
{
    if (snapshot != NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
    }
}

void vf2_i960_snapshot_destroy(vf2_i960_snapshot *snapshot)
{
    size_t index = 0u;
    snapshot_region_ref regions[VF2_SNAPSHOT_REGION_COUNT];
    if (snapshot == NULL) {
        return;
    }
    writable_regions(snapshot, regions);
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        free(*regions[index].data);
    }
    memset(snapshot, 0, sizeof(*snapshot));
}

vf2_status vf2_i960_snapshot_capture(vf2_i960_snapshot *snapshot, const vf2_i960_cpu *cpu, const vf2_model2a *machine)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;
    snapshot_region_ref destination[VF2_SNAPSHOT_REGION_COUNT];
    snapshot_const_region_ref source[VF2_SNAPSHOT_REGION_COUNT];
    if (snapshot == NULL || cpu == NULL || machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    snapshot->cpu = *cpu;
    snapshot->geometry_write_start = machine->geometry_write_start;
    snapshot->geometry_read_start = machine->geometry_read_start;
    snapshot->geometry_control = machine->geometry_control;
    snapshot->geometry_program_count = machine->geometry_program_count;
    writable_regions(snapshot, destination);
    machine_regions(machine, source);
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT && status == VF2_OK; ++index) {
        status = copy_region(destination[index].data, destination[index].size, source[index].data, source[index].size);
    }
    if (status != VF2_OK) {
        vf2_i960_snapshot_destroy(snapshot);
    }
    return status;
}

vf2_status vf2_i960_snapshot_restore(const vf2_i960_snapshot *snapshot, vf2_i960_cpu *cpu, vf2_model2a *machine)
{
    size_t index = 0u;
    snapshot_const_region_ref source[VF2_SNAPSHOT_REGION_COUNT];
    snapshot_const_region_ref destination[VF2_SNAPSHOT_REGION_COUNT];
    if (snapshot == NULL || cpu == NULL || machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    const_regions(snapshot, source);
    machine_regions(machine, destination);
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        if (source[index].size != destination[index].size ||
            (source[index].data == NULL && source[index].size != 0u) ||
            (destination[index].data == NULL && destination[index].size != 0u)) {
            return VF2_ERROR_BAD_SIZE;
        }
    }
    *cpu = snapshot->cpu;
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        memcpy((uint8_t *)destination[index].data, source[index].data, source[index].size);
    }
    machine->geometry_write_start = snapshot->geometry_write_start;
    machine->geometry_read_start = snapshot->geometry_read_start;
    machine->geometry_control = snapshot->geometry_control;
    machine->geometry_program_count = snapshot->geometry_program_count;
    return VF2_OK;
}

vf2_status vf2_i960_snapshot_write_file(const vf2_i960_snapshot *snapshot, const char *path)
{
    FILE *file = NULL;
    size_t index = 0u;
    int ok = 1;
    snapshot_const_region_ref regions[VF2_SNAPSHOT_REGION_COUNT];
    if (snapshot == NULL || path == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }
    const_regions(snapshot, regions);
    ok = write_bytes(file, snapshot_magic, sizeof(snapshot_magic));
    ok = ok && write_u32(file, VF2_I960_SNAPSHOT_VERSION);
    ok = ok && write_u32(file, snapshot->cpu.sat);
    ok = ok && write_u32(file, snapshot->cpu.prcb);
    ok = ok && write_u32(file, snapshot->cpu.ip);
    ok = ok && write_u32(file, snapshot->cpu.process_control);
    ok = ok && write_u32(file, snapshot->cpu.arithmetic_control);
    ok = ok && write_u32(file, snapshot->cpu.interrupt_control);
    ok = ok && write_u32(file, (uint32_t)snapshot->cpu.compare_result);
    ok = ok && write_u32(file, snapshot->cpu.reinitialized ? 1u : 0u);
    ok = ok && write_u64(file, snapshot->cpu.executed_instructions);
    ok = ok && write_u64(file, snapshot->cpu.procedure_calls);
    ok = ok && write_u64(file, snapshot->cpu.procedure_returns);
    ok = ok && write_u64(file, snapshot->cpu.interrupt_entries);
    ok = ok && write_u64(file, snapshot->cpu.interrupt_returns);
    ok = ok && write_u32(file, snapshot->cpu.local_frame_depth);
    ok = ok && write_u32(file, snapshot->cpu.maximum_local_frame_depth);
    for (index = 0u; ok && index < VF2_I960_REGISTER_COUNT; ++index) {
        ok = write_u32(file, snapshot->cpu.registers[index]);
    }
    for (index = 0u; ok && index < VF2_I960_MAX_LOCAL_FRAMES; ++index) {
        size_t reg = 0u;
        for (reg = 0u; ok && reg < VF2_I960_LOCAL_REGISTER_COUNT; ++reg) {
            ok = write_u32(file, snapshot->cpu.local_frames[index].registers[reg]);
        }
    }
    ok = ok && write_u32(file, snapshot->geometry_write_start);
    ok = ok && write_u32(file, snapshot->geometry_read_start);
    ok = ok && write_u32(file, snapshot->geometry_control);
    ok = ok && write_u32(file, snapshot->geometry_program_count);
    ok = ok && write_u32(file, VF2_SNAPSHOT_REGION_COUNT);
    for (index = 0u; ok && index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        ok = regions[index].size <= UINT32_MAX && write_u32(file, (uint32_t)regions[index].size);
    }
    for (index = 0u; ok && index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        ok = write_bytes(file, regions[index].data, regions[index].size);
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok ? VF2_OK : VF2_ERROR_IO;
}

static vf2_status allocate_read_region(FILE *file, uint8_t **data, size_t size)
{
    uint8_t *buffer = NULL;
    if (size != 0u) {
        buffer = (uint8_t *)malloc(size);
        if (buffer == NULL) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        if (!read_bytes(file, buffer, size)) {
            free(buffer);
            return VF2_ERROR_IO;
        }
    }
    *data = buffer;
    return VF2_OK;
}

vf2_status vf2_i960_snapshot_read_file(vf2_i960_snapshot *snapshot, const char *path)
{
    FILE *file = NULL;
    uint8_t magic[8];
    uint32_t version = 0u;
    uint32_t compare_result = 0u;
    uint32_t reinitialized = 0u;
    uint32_t region_count = 0u;
    uint32_t sizes[VF2_SNAPSHOT_REGION_COUNT];
    size_t index = 0u;
    vf2_status status = VF2_OK;
    snapshot_region_ref regions[VF2_SNAPSHOT_REGION_COUNT];
    if (snapshot == NULL || path == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }
    vf2_i960_snapshot_destroy(snapshot);
    if (!read_bytes(file, magic, sizeof(magic)) || memcmp(magic, snapshot_magic, sizeof(magic)) != 0 ||
        !read_u32(file, &version) || version < 5u ||
        version > VF2_I960_SNAPSHOT_VERSION ||
        !read_u32(file, &snapshot->cpu.sat) || !read_u32(file, &snapshot->cpu.prcb) ||
        !read_u32(file, &snapshot->cpu.ip) || !read_u32(file, &snapshot->cpu.process_control) ||
        !read_u32(file, &snapshot->cpu.arithmetic_control) || !read_u32(file, &snapshot->cpu.interrupt_control) ||
        !read_u32(file, &compare_result) || !read_u32(file, &reinitialized) ||
        !read_u64(file, &snapshot->cpu.executed_instructions) ||
        !read_u64(file, &snapshot->cpu.procedure_calls) ||
        !read_u64(file, &snapshot->cpu.procedure_returns) ||
        !read_u64(file, &snapshot->cpu.interrupt_entries) ||
        !read_u64(file, &snapshot->cpu.interrupt_returns) ||
        !read_u32(file, &snapshot->cpu.local_frame_depth) ||
        !read_u32(file, &snapshot->cpu.maximum_local_frame_depth) ||
        snapshot->cpu.local_frame_depth > VF2_I960_MAX_LOCAL_FRAMES ||
        snapshot->cpu.maximum_local_frame_depth > VF2_I960_MAX_LOCAL_FRAMES) {
        fclose(file);
        return VF2_ERROR_BAD_SIZE;
    }
    snapshot->cpu.compare_result = (vf2_i960_compare_result)compare_result;
    snapshot->cpu.reinitialized = reinitialized != 0u;
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (!read_u32(file, &snapshot->cpu.registers[index])) {
            fclose(file);
            vf2_i960_snapshot_destroy(snapshot);
            return VF2_ERROR_IO;
        }
    }
    for (index = 0u; index < VF2_I960_MAX_LOCAL_FRAMES; ++index) {
        size_t reg = 0u;
        for (reg = 0u; reg < VF2_I960_LOCAL_REGISTER_COUNT; ++reg) {
            if (!read_u32(file, &snapshot->cpu.local_frames[index].registers[reg])) {
                fclose(file);
                vf2_i960_snapshot_destroy(snapshot);
                return VF2_ERROR_IO;
            }
        }
    }
    if (version >= 6u &&
        (!read_u32(file, &snapshot->geometry_write_start) ||
         !read_u32(file, &snapshot->geometry_read_start) ||
         !read_u32(file, &snapshot->geometry_control) ||
         !read_u32(file, &snapshot->geometry_program_count))) {
        fclose(file);
        vf2_i960_snapshot_destroy(snapshot);
        return VF2_ERROR_IO;
    }
    if (!read_u32(file, &region_count) || region_count != VF2_SNAPSHOT_REGION_COUNT) {
        fclose(file);
        vf2_i960_snapshot_destroy(snapshot);
        return VF2_ERROR_BAD_SIZE;
    }
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        if (!read_u32(file, &sizes[index])) {
            fclose(file);
            vf2_i960_snapshot_destroy(snapshot);
            return VF2_ERROR_IO;
        }
    }
    writable_regions(snapshot, regions);
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT && status == VF2_OK; ++index) {
        *regions[index].size = sizes[index];
        status = allocate_read_region(file, regions[index].data, sizes[index]);
    }
    if (status == VF2_OK && version == 5u) {
        if (snapshot->geometry_size < UINT32_C(0x300c) ||
            snapshot->video_control_size < UINT32_C(12)) {
            status = VF2_ERROR_BAD_SIZE;
        } else {
            const uint8_t *write_start = snapshot->geometry + UINT32_C(0x1008);
            const uint8_t *read_start = snapshot->geometry + UINT32_C(0x3008);
            const uint8_t *control = snapshot->video_control + UINT32_C(8);
            snapshot->geometry_write_start =
                ((uint32_t)write_start[0] | ((uint32_t)write_start[1] << 8u) |
                 ((uint32_t)write_start[2] << 16u) | ((uint32_t)write_start[3] << 24u)) &
                UINT32_C(0x000fffff);
            snapshot->geometry_read_start =
                ((uint32_t)read_start[0] | ((uint32_t)read_start[1] << 8u) |
                 ((uint32_t)read_start[2] << 16u) | ((uint32_t)read_start[3] << 24u)) &
                UINT32_C(0x000fffff);
            snapshot->geometry_control =
                (uint32_t)control[0] | ((uint32_t)control[1] << 8u) |
                ((uint32_t)control[2] << 16u) | ((uint32_t)control[3] << 24u);
            /* v5 did not serialize the transient program-word count. */
            snapshot->geometry_program_count = 0u;
        }
    }
    if (fclose(file) != 0 && status == VF2_OK) {
        status = VF2_ERROR_IO;
    }
    if (status != VF2_OK) {
        vf2_i960_snapshot_destroy(snapshot);
    }
    return status;
}

static void compare_bytes(const char *component, const uint8_t *expected, const uint8_t *actual, size_t size, vf2_i960_snapshot_diff *diff)
{
    size_t index = 0u;
    if (size == 0u || memcmp(expected, actual, size) == 0) {
        return;
    }
    for (index = 0u; index < size; ++index) {
        if (expected[index] != actual[index]) {
            if (diff->equal) {
                diff->equal = false;
                (void)snprintf(diff->component, sizeof(diff->component), "%s", component);
                diff->first_offset = index;
                diff->expected_value = expected[index];
                diff->actual_value = actual[index];
            }
            ++diff->differing_bytes;
        }
    }
}

static vf2_status compare_memory_regions(
    const vf2_i960_snapshot *expected,
    const vf2_i960_snapshot *actual,
    vf2_i960_snapshot_diff *diff,
    int initialize_diff
)
{
    size_t index = 0u;
    snapshot_const_region_ref expected_regions[VF2_SNAPSHOT_REGION_COUNT];
    snapshot_const_region_ref actual_regions[VF2_SNAPSHOT_REGION_COUNT];
    if (expected == NULL || actual == NULL || diff == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (initialize_diff) {
        memset(diff, 0, sizeof(*diff));
        diff->equal = true;
    }
    const_regions(expected, expected_regions);
    const_regions(actual, actual_regions);
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        if (expected_regions[index].size != actual_regions[index].size) {
            if (diff->equal) {
                diff->equal = false;
                (void)snprintf(diff->component, sizeof(diff->component), "region-size");
                diff->first_offset = index;
                diff->expected_value = (uint32_t)expected_regions[index].size;
                diff->actual_value = (uint32_t)actual_regions[index].size;
            }
            ++diff->differing_bytes;
            return VF2_OK;
        }
    }
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        compare_bytes(expected_regions[index].name, expected_regions[index].data,
                      actual_regions[index].data, expected_regions[index].size, diff);
    }
    return VF2_OK;
}

vf2_status vf2_i960_compare_live_state(
    const vf2_i960_cpu *expected_cpu,
    const vf2_model2a *expected_machine,
    const vf2_i960_cpu *actual_cpu,
    const vf2_model2a *actual_machine,
    vf2_i960_snapshot_diff *diff
)
{
    snapshot_const_region_ref expected_regions[VF2_SNAPSHOT_REGION_COUNT];
    snapshot_const_region_ref actual_regions[VF2_SNAPSHOT_REGION_COUNT];
    size_t index = 0u;

    if (expected_cpu == NULL || expected_machine == NULL ||
        actual_cpu == NULL || actual_machine == NULL || diff == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(diff, 0, sizeof(*diff));
    diff->equal = true;
    if (expected_cpu->sat != actual_cpu->sat ||
        expected_cpu->prcb != actual_cpu->prcb ||
        expected_cpu->ip != actual_cpu->ip ||
        expected_cpu->process_control != actual_cpu->process_control ||
        expected_cpu->arithmetic_control != actual_cpu->arithmetic_control ||
        expected_cpu->interrupt_control != actual_cpu->interrupt_control ||
        expected_cpu->compare_result != actual_cpu->compare_result ||
        expected_cpu->reinitialized != actual_cpu->reinitialized ||
        expected_cpu->local_frame_depth != actual_cpu->local_frame_depth) {
        diff->equal = false;
        (void)snprintf(diff->component, sizeof(diff->component), "cpu-state");
        if (expected_cpu->ip != actual_cpu->ip) {
            diff->first_offset = 0u;
            diff->expected_value = expected_cpu->ip;
            diff->actual_value = actual_cpu->ip;
        } else if (expected_cpu->compare_result != actual_cpu->compare_result) {
            diff->first_offset = 1u;
            diff->expected_value = (uint32_t)expected_cpu->compare_result;
            diff->actual_value = (uint32_t)actual_cpu->compare_result;
        } else if (expected_cpu->arithmetic_control !=
                   actual_cpu->arithmetic_control) {
            diff->first_offset = 2u;
            diff->expected_value = expected_cpu->arithmetic_control;
            diff->actual_value = actual_cpu->arithmetic_control;
        } else if (expected_cpu->local_frame_depth !=
                   actual_cpu->local_frame_depth) {
            diff->first_offset = 3u;
            diff->expected_value = expected_cpu->local_frame_depth;
            diff->actual_value = actual_cpu->local_frame_depth;
        } else {
            diff->first_offset = 4u;
            diff->expected_value = expected_cpu->ip;
            diff->actual_value = actual_cpu->ip;
        }
        ++diff->differing_bytes;
    }
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (expected_cpu->registers[index] != actual_cpu->registers[index]) {
            if (diff->equal) {
                diff->equal = false;
                (void)snprintf(diff->component, sizeof(diff->component), "registers");
                diff->first_offset = index;
                diff->expected_value = expected_cpu->registers[index];
                diff->actual_value = actual_cpu->registers[index];
            }
            ++diff->differing_bytes;
        }
    }
    if (expected_cpu->local_frame_depth != actual_cpu->local_frame_depth) {
        return VF2_OK;
    }
    {
        const uint32_t expected_model2[4] = {
            expected_machine->geometry_write_start,
            expected_machine->geometry_read_start,
            expected_machine->geometry_control,
            expected_machine->geometry_program_count
        };
        const uint32_t actual_model2[4] = {
            actual_machine->geometry_write_start,
            actual_machine->geometry_read_start,
            actual_machine->geometry_control,
            actual_machine->geometry_program_count
        };
        for (index = 0u; index < 4u; ++index) {
            if (expected_model2[index] != actual_model2[index]) {
                if (diff->equal) {
                    diff->equal = false;
                    (void)snprintf(diff->component, sizeof(diff->component), "model2-state");
                    diff->first_offset = index;
                    diff->expected_value = expected_model2[index];
                    diff->actual_value = actual_model2[index];
                }
                ++diff->differing_bytes;
            }
        }
    }
    for (index = 0u; index < expected_cpu->local_frame_depth; ++index) {
        size_t reg = 0u;
        for (reg = 0u; reg < VF2_I960_LOCAL_REGISTER_COUNT; ++reg) {
            const uint32_t expected_value = expected_cpu->local_frames[index].registers[reg];
            const uint32_t actual_value = actual_cpu->local_frames[index].registers[reg];
            if (expected_value != actual_value) {
                if (diff->equal) {
                    diff->equal = false;
                    (void)snprintf(diff->component, sizeof(diff->component), "local-frames");
                    diff->first_offset = index * VF2_I960_LOCAL_REGISTER_COUNT + reg;
                    diff->expected_value = expected_value;
                    diff->actual_value = actual_value;
                }
                ++diff->differing_bytes;
            }
        }
    }

    machine_regions(expected_machine, expected_regions);
    machine_regions(actual_machine, actual_regions);
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        if (expected_regions[index].size != actual_regions[index].size) {
            if (diff->equal) {
                diff->equal = false;
                (void)snprintf(diff->component, sizeof(diff->component), "region-size");
                diff->first_offset = index;
                diff->expected_value = (uint32_t)expected_regions[index].size;
                diff->actual_value = (uint32_t)actual_regions[index].size;
            }
            ++diff->differing_bytes;
            return VF2_OK;
        }
    }
    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {
        compare_bytes(
            expected_regions[index].name,
            expected_regions[index].data,
            actual_regions[index].data,
            expected_regions[index].size,
            diff
        );
    }
    return VF2_OK;
}

vf2_status vf2_i960_snapshot_compare_memory(
    const vf2_i960_snapshot *expected,
    const vf2_i960_snapshot *actual,
    vf2_i960_snapshot_diff *diff
)
{
    return compare_memory_regions(expected, actual, diff, 1);
}

vf2_status vf2_i960_snapshot_compare(
    const vf2_i960_snapshot *expected,
    const vf2_i960_snapshot *actual,
    vf2_i960_snapshot_diff *diff
)
{
    size_t index = 0u;
    if (expected == NULL || actual == NULL || diff == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(diff, 0, sizeof(*diff));
    diff->equal = true;
    if (expected->cpu.sat != actual->cpu.sat || expected->cpu.prcb != actual->cpu.prcb ||
        expected->cpu.ip != actual->cpu.ip ||
        expected->cpu.process_control != actual->cpu.process_control ||
        expected->cpu.arithmetic_control != actual->cpu.arithmetic_control ||
        expected->cpu.interrupt_control != actual->cpu.interrupt_control ||
        expected->cpu.compare_result != actual->cpu.compare_result ||
        expected->cpu.reinitialized != actual->cpu.reinitialized ||
        expected->cpu.local_frame_depth != actual->cpu.local_frame_depth) {
        diff->equal = false;
        (void)snprintf(diff->component, sizeof(diff->component), "cpu-state");
        if (expected->cpu.ip != actual->cpu.ip) {
            diff->first_offset = 0u;
            diff->expected_value = expected->cpu.ip;
            diff->actual_value = actual->cpu.ip;
        } else if (expected->cpu.compare_result != actual->cpu.compare_result) {
            diff->first_offset = 1u;
            diff->expected_value = (uint32_t)expected->cpu.compare_result;
            diff->actual_value = (uint32_t)actual->cpu.compare_result;
        } else if (expected->cpu.arithmetic_control !=
                   actual->cpu.arithmetic_control) {
            diff->first_offset = 2u;
            diff->expected_value = expected->cpu.arithmetic_control;
            diff->actual_value = actual->cpu.arithmetic_control;
        } else {
            diff->first_offset = 4u;
            diff->expected_value = expected->cpu.ip;
            diff->actual_value = actual->cpu.ip;
        }
        ++diff->differing_bytes;
    }
    {
        const uint32_t expected_model2[4] = {
            expected->geometry_write_start,
            expected->geometry_read_start,
            expected->geometry_control,
            expected->geometry_program_count
        };
        const uint32_t actual_model2[4] = {
            actual->geometry_write_start,
            actual->geometry_read_start,
            actual->geometry_control,
            actual->geometry_program_count
        };
        for (index = 0u; index < 4u; ++index) {
            if (expected_model2[index] != actual_model2[index]) {
                if (diff->equal) {
                    diff->equal = false;
                    (void)snprintf(diff->component, sizeof(diff->component), "model2-state");
                    diff->first_offset = index;
                    diff->expected_value = expected_model2[index];
                    diff->actual_value = actual_model2[index];
                }
                ++diff->differing_bytes;
            }
        }
    }
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (expected->cpu.registers[index] != actual->cpu.registers[index]) {
            if (diff->equal) {
                diff->equal = false;
                (void)snprintf(diff->component, sizeof(diff->component), "registers");
                diff->first_offset = index;
                diff->expected_value = expected->cpu.registers[index];
                diff->actual_value = actual->cpu.registers[index];
            }
            ++diff->differing_bytes;
        }
    }
    for (index = 0u; index < expected->cpu.local_frame_depth; ++index) {
        size_t reg = 0u;
        for (reg = 0u; reg < VF2_I960_LOCAL_REGISTER_COUNT; ++reg) {
            const uint32_t expected_value = expected->cpu.local_frames[index].registers[reg];
            const uint32_t actual_value = actual->cpu.local_frames[index].registers[reg];
            if (expected_value != actual_value) {
                if (diff->equal) {
                    diff->equal = false;
                    (void)snprintf(diff->component, sizeof(diff->component), "local-frames");
                    diff->first_offset = index * VF2_I960_LOCAL_REGISTER_COUNT + reg;
                    diff->expected_value = expected_value;
                    diff->actual_value = actual_value;
                }
                ++diff->differing_bytes;
            }
        }
    }
    return compare_memory_regions(expected, actual, diff, 0);
}
