#include "vf2/model2a.h"

#include <stdlib.h>
#include <string.h>

#include "vf2/platform.h"

typedef struct vf2_memory_view {
    const uint8_t *read_data;
    uint8_t *write_data;
    size_t size;
    uint32_t base;
} vf2_memory_view;

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static uint32_t model2a_timer_value(
    const vf2_model2a *machine,
    size_t timer
)
{
    return read_le32(machine->timers + timer * sizeof(uint32_t));
}

static void model2a_set_timer_value(
    vf2_model2a *machine,
    size_t timer,
    uint32_t value
)
{
    write_le32(machine->timers + timer * sizeof(uint32_t), value);
}

static int model2a_timer_running(
    const vf2_model2a *machine,
    size_t timer
)
{
    return machine->video_control[UINT32_C(0x20) + timer] != 0u;
}

static void model2a_set_timer_running(
    vf2_model2a *machine,
    size_t timer,
    int running
)
{
    machine->video_control[UINT32_C(0x20) + timer] =
        running != 0 ? UINT8_C(1) : UINT8_C(0);
}

static uint32_t model2a_frame_number(const vf2_model2a *machine)
{
    return read_le32(
        machine->video_control + VF2_MODEL2A_FRAME_COUNTER_OFFSET
    );
}

static void model2a_set_frame_number(
    vf2_model2a *machine,
    uint32_t frame_number
)
{
    write_le32(
        machine->video_control + VF2_MODEL2A_FRAME_COUNTER_OFFSET,
        frame_number
    );
}

static uint32_t model2a_video_status(const vf2_model2a *machine)
{
    const uint32_t frame = model2a_frame_number(machine);
    const uint32_t render_mode = read_le32(machine->copro_control);
    const uint32_t phase = (render_mode & UINT32_C(4)) != 0u
        ? (frame & UINT32_C(1)) << 2u
        : (frame & UINT32_C(2)) << 1u;
    return phase | (read_le32(
        machine->video_control + VF2_MODEL2A_VIDEO_STATUS_OFFSET
    ) & UINT32_C(3));
}

static vf2_status model2a_push_geometry_word(
    vf2_model2a *machine,
    uint32_t value
)
{
    uint32_t buffer_offset = 0u;
    size_t byte_offset = 0u;

    if (machine == NULL || machine->buffer_ram == NULL ||
        machine->geometry == NULL ||
        machine->buffer_ram_size < sizeof(uint32_t)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    buffer_offset = machine->geometry_write_start & UINT32_C(0x0001fffc);
    byte_offset = (size_t)buffer_offset;
    if (byte_offset > machine->buffer_ram_size - sizeof(uint32_t)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if ((machine->geometry_control & UINT32_C(0x80000000)) != 0u) {
        const size_t program_offset =
            (size_t)machine->geometry_program_count * sizeof(uint32_t);
        if (program_offset > UINT32_C(0x4000) - sizeof(uint32_t)) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_le32(machine->geometry + UINT32_C(0x4000) + program_offset, value);
        ++machine->geometry_program_count;
    } else {
        write_le32(machine->buffer_ram + byte_offset, value);
        machine->geometry_write_start =
            (machine->geometry_write_start + sizeof(uint32_t)) &
            UINT32_C(0x000fffff);
    }
    return VF2_OK;
}

static int range_contains(
    uint32_t base,
    size_t region_size,
    uint32_t address,
    size_t access_size
)
{
    uint64_t relative = 0u;
    if (address < base) {
        return 0;
    }
    relative = (uint64_t)address - base;
    return relative <= region_size && access_size <= region_size - (size_t)relative;
}

static int find_view(
    const vf2_model2a *machine,
    uint32_t address,
    size_t size,
    vf2_memory_view *view
)
{
    if (range_contains(VF2_MAIN_ROM_BASE, machine->main_rom_size, address, size)) {
        view->read_data = machine->main_rom;
        view->write_data = NULL;
        view->size = machine->main_rom_size;
        view->base = VF2_MAIN_ROM_BASE;
        return machine->main_rom != NULL;
    }
    {
        const size_t primary_size = machine->main_data_size < VF2_MAIN_DATA_PRIMARY_SIZE
            ? machine->main_data_size
            : VF2_MAIN_DATA_PRIMARY_SIZE;
        if (range_contains(VF2_MAIN_DATA_BASE, primary_size, address, size)) {
            view->read_data = machine->main_data;
            view->write_data = NULL;
            view->size = primary_size;
            view->base = VF2_MAIN_DATA_BASE;
            return machine->main_data != NULL;
        }
    }
    if (machine->main_data_size > VF2_MAIN_DATA_EXTRA_OFFSET) {
        const size_t remaining = machine->main_data_size - VF2_MAIN_DATA_EXTRA_OFFSET;
        const size_t extra_size = remaining < VF2_MAIN_DATA_EXTRA_SIZE
            ? remaining
            : VF2_MAIN_DATA_EXTRA_SIZE;
        if (range_contains(VF2_MAIN_DATA_EXTRA_BASE, extra_size, address, size)) {
            view->read_data = machine->main_data + VF2_MAIN_DATA_EXTRA_OFFSET;
            view->write_data = NULL;
            view->size = extra_size;
            view->base = VF2_MAIN_DATA_EXTRA_BASE;
            return machine->main_data != NULL;
        }
    }
    if (range_contains(VF2_GEOMETRY_BASE, machine->geometry_size, address, size)) {
        view->read_data = machine->geometry;
        view->write_data = machine->geometry;
        view->size = machine->geometry_size;
        view->base = VF2_GEOMETRY_BASE;
        return machine->geometry != NULL;
    }
    if (range_contains(VF2_COPRO_PORT_BASE, machine->copro_port_size, address, size)) {
        view->read_data = machine->copro_port;
        view->write_data = machine->copro_port;
        view->size = machine->copro_port_size;
        view->base = VF2_COPRO_PORT_BASE;
        return machine->copro_port != NULL;
    }
    if (range_contains(VF2_WORK_RAM_BASE, machine->work_ram_size, address, size)) {
        view->read_data = machine->work_ram;
        view->write_data = machine->work_ram;
        view->size = machine->work_ram_size;
        view->base = VF2_WORK_RAM_BASE;
        return machine->work_ram != NULL;
    }
    if (range_contains(VF2_BUFFER_RAM_BASE, machine->buffer_ram_size, address, size)) {
        view->read_data = machine->buffer_ram;
        view->write_data = machine->buffer_ram;
        view->size = machine->buffer_ram_size;
        view->base = VF2_BUFFER_RAM_BASE;
        return machine->buffer_ram != NULL;
    }
    if (range_contains(VF2_VIDEO_CONTROL_BASE, machine->video_control_size, address, size)) {
        view->read_data = machine->video_control;
        view->write_data = machine->video_control;
        view->size = machine->video_control_size;
        view->base = VF2_VIDEO_CONTROL_BASE;
        return machine->video_control != NULL;
    }
    if (range_contains(VF2_CPU_CONTROL_BASE, machine->cpu_control_size, address, size)) {
        view->read_data = machine->cpu_control;
        view->write_data = machine->cpu_control;
        view->size = machine->cpu_control_size;
        view->base = VF2_CPU_CONTROL_BASE;
        return machine->cpu_control != NULL;
    }
    if (range_contains(VF2_INTERRUPT_CONTROL_BASE, machine->interrupt_control_size, address, size)) {
        view->read_data = machine->interrupt_control;
        view->write_data = machine->interrupt_control;
        view->size = machine->interrupt_control_size;
        view->base = VF2_INTERRUPT_CONTROL_BASE;
        return machine->interrupt_control != NULL;
    }
    if (range_contains(VF2_TIMER_BASE, machine->timers_size, address, size)) {
        view->read_data = machine->timers;
        view->write_data = machine->timers;
        view->size = machine->timers_size;
        view->base = VF2_TIMER_BASE;
        return machine->timers != NULL;
    }
    if (range_contains(VF2_TILE_RAM_BASE, machine->tile_ram_size, address, size)) {
        view->read_data = machine->tile_ram;
        view->write_data = machine->tile_ram;
        view->size = machine->tile_ram_size;
        view->base = VF2_TILE_RAM_BASE;
        return machine->tile_ram != NULL;
    }
    if (range_contains(VF2_PALETTE_RAM_BASE, machine->palette_ram_size, address, size)) {
        view->read_data = machine->palette_ram;
        view->write_data = machine->palette_ram;
        view->size = machine->palette_ram_size;
        view->base = VF2_PALETTE_RAM_BASE;
        return machine->palette_ram != NULL;
    }
    if (range_contains(VF2_IO_CONTROL_BASE, machine->io_control_size, address, size)) {
        view->read_data = machine->io_control;
        view->write_data = machine->io_control;
        view->size = machine->io_control_size;
        view->base = VF2_IO_CONTROL_BASE;
        return machine->io_control != NULL;
    }
    if (range_contains(VF2_BACKUP_SRAM_BASE, machine->backup_sram_size, address, size)) {
        view->read_data = machine->backup_sram;
        view->write_data = machine->backup_sram;
        view->size = machine->backup_sram_size;
        view->base = VF2_BACKUP_SRAM_BASE;
        return machine->backup_sram != NULL;
    }
    if (range_contains(VF2_COPRO_CONTROL_BASE, machine->copro_control_size, address, size)) {
        view->read_data = machine->copro_control;
        view->write_data = machine->copro_control;
        view->size = machine->copro_control_size;
        view->base = VF2_COPRO_CONTROL_BASE;
        return machine->copro_control != NULL;
    }
    if (range_contains(VF2_COLOR_TRANSLATION_BASE, machine->color_translation_size, address, size)) {
        view->read_data = machine->color_translation;
        view->write_data = machine->color_translation;
        view->size = machine->color_translation_size;
        view->base = VF2_COLOR_TRANSLATION_BASE;
        return machine->color_translation != NULL;
    }
    if (range_contains(VF2_TEXTURE_RAM0_BASE, machine->texture_ram0_size, address, size) ||
        range_contains(VF2_TEXTURE_RAM0_BASE + VF2_TEXTURE_RAM_MIRROR,
                       machine->texture_ram0_size, address, size)) {
        view->read_data = machine->texture_ram0;
        view->write_data = machine->texture_ram0;
        view->size = machine->texture_ram0_size;
        view->base = address >= VF2_TEXTURE_RAM0_BASE + VF2_TEXTURE_RAM_MIRROR
            ? VF2_TEXTURE_RAM0_BASE + VF2_TEXTURE_RAM_MIRROR
            : VF2_TEXTURE_RAM0_BASE;
        return machine->texture_ram0 != NULL;
    }
    if (range_contains(VF2_TEXTURE_RAM1_BASE, machine->texture_ram1_size, address, size) ||
        range_contains(VF2_TEXTURE_RAM1_BASE + VF2_TEXTURE_RAM_MIRROR,
                       machine->texture_ram1_size, address, size)) {
        view->read_data = machine->texture_ram1;
        view->write_data = machine->texture_ram1;
        view->size = machine->texture_ram1_size;
        view->base = address >= VF2_TEXTURE_RAM1_BASE + VF2_TEXTURE_RAM_MIRROR
            ? VF2_TEXTURE_RAM1_BASE + VF2_TEXTURE_RAM_MIRROR
            : VF2_TEXTURE_RAM1_BASE;
        return machine->texture_ram1 != NULL;
    }
    if (range_contains(VF2_LUMA_RAM_BASE, machine->luma_ram_size, address, size)) {
        view->read_data = machine->luma_ram;
        view->write_data = machine->luma_ram;
        view->size = machine->luma_ram_size;
        view->base = VF2_LUMA_RAM_BASE;
        return machine->luma_ram != NULL;
    }
    if (range_contains(VF2_SYSTEM_CONTROL_BASE, machine->system_control_size, address, size)) {
        view->read_data = machine->system_control;
        view->write_data = machine->system_control;
        view->size = machine->system_control_size;
        view->base = VF2_SYSTEM_CONTROL_BASE;
        return machine->system_control != NULL;
    }
    return 0;
}

static uint8_t model2a_host_input_port(
    const vf2_model2a *machine,
    uint32_t port
)
{
    uint8_t value = UINT8_C(0xff);
    const uint32_t input = machine->input;

    if (port == 1u) {
        if ((input & VF2_PLATFORM_BUTTON_COIN) != 0u) {
            value &= (uint8_t)~UINT8_C(0x01);
        }
        if ((input & VF2_PLATFORM_BUTTON_TEST) != 0u) {
            value &= (uint8_t)~UINT8_C(0x04);
        }
        if ((input & VF2_PLATFORM_BUTTON_SERVICE) != 0u) {
            value &= (uint8_t)~UINT8_C(0x08);
        }
        if ((input & VF2_PLATFORM_BUTTON_START) != 0u) {
            value &= (uint8_t)~UINT8_C(0x10);
        }
    } else if (port == 2u) {
        if ((input & VF2_PLATFORM_BUTTON_PUNCH) != 0u) {
            value &= (uint8_t)~UINT8_C(0x01);
        }
        if ((input & VF2_PLATFORM_BUTTON_KICK) != 0u) {
            value &= (uint8_t)~UINT8_C(0x02);
        }
        if ((input & VF2_PLATFORM_BUTTON_GUARD) != 0u) {
            value &= (uint8_t)~UINT8_C(0x04);
        }
        if ((input & VF2_PLATFORM_BUTTON_DOWN) != 0u) {
            value &= (uint8_t)~UINT8_C(0x10);
        }
        if ((input & VF2_PLATFORM_BUTTON_UP) != 0u) {
            value &= (uint8_t)~UINT8_C(0x20);
        }
        if ((input & VF2_PLATFORM_BUTTON_RIGHT) != 0u) {
            value &= (uint8_t)~UINT8_C(0x40);
        }
        if ((input & VF2_PLATFORM_BUTTON_LEFT) != 0u) {
            value &= (uint8_t)~UINT8_C(0x80);
        }
    } else if (port == 3u) {
        if ((input & VF2_PLATFORM_BUTTON_P2_PUNCH) != 0u) {
            value &= (uint8_t)~UINT8_C(0x01);
        }
        if ((input & VF2_PLATFORM_BUTTON_P2_KICK) != 0u) {
            value &= (uint8_t)~UINT8_C(0x02);
        }
        if ((input & VF2_PLATFORM_BUTTON_P2_GUARD) != 0u) {
            value &= (uint8_t)~UINT8_C(0x04);
        }
        if ((input & VF2_PLATFORM_BUTTON_P2_DOWN) != 0u) {
            value &= (uint8_t)~UINT8_C(0x10);
        }
        if ((input & VF2_PLATFORM_BUTTON_P2_UP) != 0u) {
            value &= (uint8_t)~UINT8_C(0x20);
        }
        if ((input & VF2_PLATFORM_BUTTON_P2_RIGHT) != 0u) {
            value &= (uint8_t)~UINT8_C(0x40);
        }
        if ((input & VF2_PLATFORM_BUTTON_P2_LEFT) != 0u) {
            value &= (uint8_t)~UINT8_C(0x80);
        }
    }
    return value;
}

static vf2_status model2a_read_input_port(
    const vf2_model2a *machine,
    uint32_t address,
    void *destination,
    size_t size
)
{
    const uint32_t relative = address - VF2_IO_CONTROL_BASE;
    const uint32_t port = relative / 2u;
    uint8_t value = 0u;

    if (size != sizeof(uint8_t) || (relative & 1u) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (relative == UINT32_C(0x10)) {
        value = model2a_host_input_port(machine, 1u);
    } else if (relative == UINT32_C(0x12)) {
        value = model2a_host_input_port(machine, 2u);
    } else if (relative == UINT32_C(0x14)) {
        value = model2a_host_input_port(machine, 3u);
    } else if (port >= 1u && port <= 3u) {
        if ((machine->io_control[0x10u] & (UINT8_C(1) << port)) != 0u) {
            value = model2a_host_input_port(machine, port);
        } else {
            value = machine->io_control[relative];
        }
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    *(uint8_t *)destination = value;
    return VF2_OK;
}

int vf2_model2a_initialize(vf2_model2a *machine)
{
    if (machine == NULL) {
        return 0;
    }

    memset(machine, 0, sizeof(*machine));
    machine->geometry_size = VF2_GEOMETRY_SIZE;
    machine->copro_port_size = VF2_COPRO_PORT_SIZE;
    machine->work_ram_size = VF2_WORK_RAM_SIZE;
    machine->buffer_ram_size = VF2_BUFFER_RAM_SIZE;
    machine->video_control_size = VF2_VIDEO_CONTROL_SIZE;
    machine->cpu_control_size = VF2_CPU_CONTROL_SIZE;
    machine->interrupt_control_size = VF2_INTERRUPT_CONTROL_SIZE;
    machine->timers_size = VF2_TIMER_SIZE;
    machine->tile_ram_size = VF2_TILE_RAM_SIZE;
    machine->palette_ram_size = VF2_PALETTE_RAM_SIZE;
    machine->io_control_size = VF2_IO_CONTROL_SIZE;
    machine->backup_sram_size = VF2_BACKUP_SRAM_SIZE;
    machine->copro_control_size = VF2_COPRO_CONTROL_SIZE;
    machine->color_translation_size = VF2_COLOR_TRANSLATION_SIZE;
    machine->texture_ram0_size = VF2_TEXTURE_RAM_SIZE;
    machine->texture_ram1_size = VF2_TEXTURE_RAM_SIZE;
    machine->luma_ram_size = VF2_LUMA_RAM_SIZE;
    machine->system_control_size = VF2_SYSTEM_CONTROL_SIZE;
    machine->geometry = (uint8_t *)calloc(1u, machine->geometry_size);
    machine->copro_port = (uint8_t *)calloc(1u, machine->copro_port_size);
    machine->work_ram = (uint8_t *)calloc(1u, machine->work_ram_size);
    machine->buffer_ram = (uint8_t *)calloc(1u, machine->buffer_ram_size);
    machine->video_control = (uint8_t *)calloc(1u, machine->video_control_size);
    machine->cpu_control = (uint8_t *)calloc(1u, machine->cpu_control_size);
    machine->interrupt_control = (uint8_t *)calloc(1u, machine->interrupt_control_size);
    machine->timers = (uint8_t *)calloc(1u, machine->timers_size);
    machine->tile_ram = (uint8_t *)calloc(1u, machine->tile_ram_size);
    machine->palette_ram = (uint8_t *)calloc(1u, machine->palette_ram_size);
    machine->io_control = (uint8_t *)calloc(1u, machine->io_control_size);
    machine->backup_sram = (uint8_t *)calloc(1u, machine->backup_sram_size);
    machine->copro_control = (uint8_t *)calloc(1u, machine->copro_control_size);
    machine->color_translation = (uint8_t *)calloc(1u, machine->color_translation_size);
    machine->texture_ram0 = (uint8_t *)calloc(1u, machine->texture_ram0_size);
    machine->texture_ram1 = (uint8_t *)calloc(1u, machine->texture_ram1_size);
    machine->luma_ram = (uint8_t *)calloc(1u, machine->luma_ram_size);
    machine->system_control = (uint8_t *)calloc(1u, machine->system_control_size);

    if (machine->geometry == NULL || machine->copro_port == NULL ||
        machine->work_ram == NULL ||
        machine->buffer_ram == NULL ||
        machine->video_control == NULL || machine->cpu_control == NULL ||
        machine->interrupt_control == NULL || machine->timers == NULL ||
        machine->tile_ram == NULL ||
        machine->palette_ram == NULL ||
        machine->io_control == NULL || machine->backup_sram == NULL ||
        machine->copro_control == NULL ||
        machine->color_translation == NULL ||
        machine->texture_ram0 == NULL || machine->texture_ram1 == NULL ||
        machine->luma_ram == NULL ||
        machine->system_control == NULL) {
        vf2_model2a_shutdown(machine);
        return 0;
    }

    {
        size_t timer = 0u;
        for (timer = 0u; timer < 4u; ++timer) {
            uint8_t *value = machine->timers + timer * 4u;
            value[0] = 0xffu;
            value[1] = 0xffu;
            value[2] = 0x0fu;
            value[3] = 0x00u;
        }
    }
    /* VF2 polls the Model 2A board-status halfword at 0x01c00042 and
     * proceeds when its low byte becomes 0x40. The harness models the
     * deterministic ready state explicitly; this is an external-device
     * input, not recovered game logic. */
    machine->io_control[0x42u] = 0x40u;
    machine->io_control[0x43u] = 0x00u;
    machine->io_control[0x10u] = 0xffu;
    machine->io_control[0x02u] = 0xffu;
    machine->io_control[0x04u] = 0xffu;
    machine->io_control[0x06u] = 0xffu;
    machine->io_control[0x16u] = 0xffu;
    machine->io_control[0x18u] = 0xffu;
    machine->io_control[0x1au] = 0x0cu;
    machine->geometry_write_start = 0u;
    machine->geometry_read_start = 0u;
    machine->geometry_control = 0u;
    machine->geometry_program_count = 0u;
    model2a_set_frame_number(machine, 0u);
    return 1;
}

void vf2_model2a_shutdown(vf2_model2a *machine)
{
    if (machine != NULL) {
        free(machine->owned_main_data);
        free(machine->geometry);
        free(machine->copro_port);
        free(machine->work_ram);
        free(machine->buffer_ram);
        free(machine->video_control);
        free(machine->cpu_control);
        free(machine->interrupt_control);
        free(machine->timers);
        free(machine->tile_ram);
        free(machine->palette_ram);
        free(machine->io_control);
        free(machine->backup_sram);
        free(machine->copro_control);
        free(machine->color_translation);
        free(machine->texture_ram0);
        free(machine->texture_ram1);
        free(machine->luma_ram);
        free(machine->system_control);
        memset(machine, 0, sizeof(*machine));
    }
}

vf2_status vf2_model2a_attach_main_rom(
    vf2_model2a *machine,
    const uint8_t *main_rom,
    size_t main_rom_size
)
{
    if (machine == NULL || main_rom == NULL || main_rom_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    machine->main_rom = main_rom;
    machine->main_rom_size = main_rom_size;
    return VF2_OK;
}

vf2_status vf2_model2a_attach_main_data(
    vf2_model2a *machine,
    const uint8_t *main_data,
    size_t main_data_size
)
{
    if (machine == NULL || main_data == NULL || main_data_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    free(machine->owned_main_data);
    machine->owned_main_data = NULL;
    machine->main_data = main_data;
    machine->main_data_size = main_data_size;
    return VF2_OK;
}

vf2_status vf2_model2a_take_main_data(
    vf2_model2a *machine,
    uint8_t *main_data,
    size_t main_data_size
)
{
    vf2_status status = VF2_OK;
    if (machine == NULL || main_data == NULL || main_data_size == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_attach_main_data(machine, main_data, main_data_size);
    if (status == VF2_OK) {
        machine->owned_main_data = main_data;
    }
    return status;
}

vf2_status vf2_model2a_set_copro_callbacks(
    vf2_model2a *machine,
    vf2_model2a_copro_read_callback read_callback,
    vf2_model2a_copro_write_callback write_callback,
    void *context
)
{
    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    machine->copro_read_callback = read_callback;
    machine->copro_write_callback = write_callback;
    machine->copro_callback_context = context;
    return VF2_OK;
}

vf2_status vf2_model2a_set_input(vf2_model2a *machine, uint32_t input)
{
    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    machine->input = input;
    return VF2_OK;
}

vf2_status vf2_model2a_read(
    const vf2_model2a *machine,
    uint32_t address,
    void *destination,
    size_t size
)
{
    vf2_memory_view view;
    size_t offset = 0u;
    if (machine == NULL || destination == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* The current VF2 boundary intentionally exposes the program upload
     * window as an unreadable hardware register.  Keep this measured
     * behavior while retaining uploaded words internally for a future TGP
     * callback; an unverified read must not become ordinary RAM. */
    if (range_contains(VF2_GEOMETRY_BASE + UINT32_C(0x4000),
                       UINT32_C(0x4000), address, size)) {
        if (size != sizeof(uint32_t) ||
            (address & (sizeof(uint32_t) - 1u)) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        write_le32((uint8_t *)destination, UINT32_C(0xffffffff));
        return VF2_OK;
    }
    if (range_contains(VF2_TIMER_BASE, machine->timers_size, address, size) &&
        size == sizeof(uint32_t) && (address & (sizeof(uint32_t) - 1u)) == 0u) {
        const size_t timer = (size_t)(address - VF2_TIMER_BASE) / sizeof(uint32_t);
        if (timer >= 4u) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_le32((uint8_t *)destination, model2a_timer_value(machine, timer));
        return VF2_OK;
    }
    if (range_contains(
            VF2_IO_CONTROL_BASE, machine->io_control_size, address, size)) {
        vf2_status input_status = model2a_read_input_port(
            machine, address, destination, size
        );
        if (input_status != VF2_ERROR_UNSUPPORTED) {
            return input_status;
        }
    }
    if (range_contains(
            VF2_COPRO_PORT_BASE, machine->copro_port_size, address, size) &&
        machine->copro_read_callback != NULL) {
        vf2_status callback_status = machine->copro_read_callback(
            machine->copro_callback_context, address, destination, size
        );
        if (callback_status != VF2_ERROR_UNSUPPORTED) {
            return callback_status;
        }
    }
    if ((address == VF2_INTERRUPT_CONTROL_BASE ||
         address == VF2_INTERRUPT_CONTROL_BASE + 4u) && size == sizeof(uint32_t)) {
        const size_t register_offset = (size_t)(address - VF2_INTERRUPT_CONTROL_BASE);
        memcpy(destination, machine->interrupt_control + register_offset, size);
        return VF2_OK;
    }
    /* FIFO control: bit 0 is set while the coprocessor output FIFO is empty.
     * The portable boundary has no autonomous TGP, so its output FIFO is
     * empty until a coprocessor callback supplies a value. */
    if (address == VF2_VIDEO_CONTROL_BASE +
                    VF2_MODEL2A_VIDEO_FIFO_STATUS_OFFSET &&
        size <= sizeof(uint32_t)) {
        const uint8_t fifo_empty[4] = {1u, 0u, 0u, 0u};
        memcpy(destination, fifo_empty, size);
        return VF2_OK;
    }
    if (address == VF2_VIDEO_CONTROL_BASE +
                    VF2_MODEL2A_VIDEO_STATUS_OFFSET &&
        size == sizeof(uint32_t)) {
        if (machine->video_control == NULL || machine->copro_control == NULL) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_le32((uint8_t *)destination, model2a_video_status(machine));
        return VF2_OK;
    }
    if (address == VF2_COPRO_CONTROL_BASE && size == sizeof(uint32_t)) {
        if (machine->copro_control == NULL) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        const uint32_t value = read_le32(machine->copro_control);
        write_le32((uint8_t *)destination,
                   (value & (UINT32_C(1) | UINT32_C(4) | UINT32_C(0x4000))));
        return VF2_OK;
    }
    if (!find_view(machine, address, size, &view) || view.read_data == NULL) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    offset = (size_t)((uint64_t)address - view.base);
    memcpy(destination, view.read_data + offset, size);
    return VF2_OK;
}

vf2_status vf2_model2a_write(
    vf2_model2a *machine,
    uint32_t address,
    const void *source,
    size_t size
)
{
    vf2_memory_view view;
    size_t offset = 0u;
    if (machine == NULL || source == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (range_contains(
            VF2_GEOMETRY_BASE + UINT32_C(0x4000),
            UINT32_C(0x4000), address, size)) {
        if (size != sizeof(uint32_t) ||
            (address & (sizeof(uint32_t) - 1u)) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        return model2a_push_geometry_word(machine, read_le32(
            (const uint8_t *)source
        ));
    }
    if (range_contains(VF2_TIMER_BASE, machine->timers_size, address, size) &&
        size == sizeof(uint32_t) && (address & (sizeof(uint32_t) - 1u)) == 0u) {
        const size_t timer = (size_t)(address - VF2_TIMER_BASE) / sizeof(uint32_t);
        if (timer >= 4u) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        model2a_set_timer_value(machine, timer,
                                read_le32((const uint8_t *)source));
        model2a_set_timer_running(machine, timer, 1);
        return VF2_OK;
    }
    /* The i960 program window is ROM with writes explicitly ignored by the
     * Model 2 map. Some original routines use low addresses as disposable
     * stack spill locations during early initialization. */
    if (range_contains(VF2_MAIN_ROM_BASE, machine->main_rom_size, address, size)) {
        return machine->main_rom != NULL ? VF2_OK : VF2_ERROR_OUT_OF_BOUNDS;
    }
    if (address == VF2_VIDEO_CONTROL_BASE && size == sizeof(uint32_t)) {
        if (machine->video_control == NULL) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        const uint32_t value = read_le32((const uint8_t *)source);
        const uint32_t previous = read_le32(machine->video_control);
        /* MAME starts/stops the TGP upload when bit 31 changes.  The
         * portable model records the transition and resets the measured
         * program-word counter; actual TGP execution remains callback-owned. */
        if ((value ^ previous) & UINT32_C(0x80000000)) {
            if ((value & UINT32_C(0x80000000)) != 0u) {
                machine->geometry_program_count = 0u;
            }
        }
        write_le32(machine->video_control, value);
        return VF2_OK;
    }
    if (address == VF2_VIDEO_CONTROL_BASE +
                       VF2_MODEL2A_VIDEO_STATUS_OFFSET &&
        size == sizeof(uint32_t)) {
        if (machine->video_control == NULL) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        write_le32(machine->video_control +
                       VF2_MODEL2A_VIDEO_STATUS_OFFSET,
                   read_le32((const uint8_t *)source));
        return VF2_OK;
    }
    if (address == VF2_COPRO_CONTROL_BASE && size == sizeof(uint32_t)) {
        if (machine->copro_control == NULL) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        const uint32_t value = read_le32((const uint8_t *)source);
        write_le32(machine->copro_control, value &
                   (UINT32_C(1) | UINT32_C(4) | UINT32_C(0x4000)));
        return VF2_OK;
    }
    if (address == VF2_INTERRUPT_CONTROL_BASE && size == sizeof(uint32_t)) {
        const uint32_t current = read_le32(machine->interrupt_control);
        const uint32_t acknowledge_mask = read_le32((const uint8_t *)source);
        write_le32(machine->interrupt_control, current & acknowledge_mask);
        return VF2_OK;
    }
    if (address == VF2_INTERRUPT_CONTROL_BASE + 4u && size == sizeof(uint32_t)) {
        write_le32(machine->interrupt_control + 4u, read_le32((const uint8_t *)source));
        return VF2_OK;
    }
    if (address == VF2_VIDEO_CONTROL_BASE + UINT32_C(8) &&
        size == sizeof(uint32_t)) {
        const uint32_t value = read_le32((const uint8_t *)source);
        if ((value & UINT32_C(0x80000000)) != 0u &&
            (machine->geometry_control & UINT32_C(0x80000000)) == 0u) {
            machine->geometry_program_count = 0u;
        }
        machine->geometry_control = value;
        write_le32(machine->video_control + 8u, value);
        return VF2_OK;
    }
    /* 315-5649 mode-0 +0x10 accepts command/configuration writes while the
     * readable input-bank backing remains the externally sampled active-low
     * system byte.  Do not alias the command write into that backing byte. */
    if (address == VF2_IO_CONTROL_BASE + UINT32_C(0x10) && size == sizeof(uint8_t) &&
        *(const uint8_t *)source == UINT8_C(0x4e)) {
        return VF2_OK;
    }
    /* The Model 2A map exposes 0x01c00040-0x01c00043 as write-only no-ops.
     * VF2 writes a handshake value and then polls the board status separately;
     * retaining the write in flat RAM would create a false infinite loop. */
    if (address >= VF2_IO_CONTROL_BASE + 0x40u &&
        (uint64_t)address + size <= (uint64_t)VF2_IO_CONTROL_BASE + 0x44u) {
        return VF2_OK;
    }
    if (range_contains(
            VF2_COPRO_PORT_BASE, machine->copro_port_size, address, size) &&
        machine->copro_write_callback != NULL) {
        vf2_status callback_status = machine->copro_write_callback(
            machine->copro_callback_context, address, source, size
        );
        if (callback_status != VF2_ERROR_UNSUPPORTED) {
            return callback_status;
        }
    }
    if (!find_view(machine, address, size, &view) || view.write_data == NULL) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    offset = (size_t)((uint64_t)address - view.base);
    memcpy(view.write_data + offset, source, size);
    if (size == sizeof(uint32_t) &&
        address == VF2_GEOMETRY_BASE + UINT32_C(0x1008)) {
        machine->geometry_write_start = read_le32((const uint8_t *)source) &
                                        UINT32_C(0x000fffff);
    } else if (size == sizeof(uint32_t) &&
               address == VF2_GEOMETRY_BASE + UINT32_C(0x3008)) {
        machine->geometry_read_start = read_le32((const uint8_t *)source) &
                                       UINT32_C(0x000fffff);
    }
    return VF2_OK;
}

vf2_status vf2_model2a_read_u32(
    const vf2_model2a *machine,
    uint32_t address,
    uint32_t *value
)
{
    uint8_t data[4];
    vf2_status status = VF2_OK;
    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, data, sizeof(data));
    if (status != VF2_OK) {
        return status;
    }
    *value = (uint32_t)data[0] |
             ((uint32_t)data[1] << 8u) |
             ((uint32_t)data[2] << 16u) |
             ((uint32_t)data[3] << 24u);
    return VF2_OK;
}

vf2_status vf2_model2a_write_u32(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t value
)
{
    uint8_t data[4];
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
    return vf2_model2a_write(machine, address, data, sizeof(data));
}


vf2_status vf2_model2a_raise_interrupt(vf2_model2a *machine, uint32_t mask)
{
    uint32_t request = 0u;
    if (machine == NULL || machine->interrupt_control == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    request = read_le32(machine->interrupt_control);
    write_le32(machine->interrupt_control, request | mask);
    return VF2_OK;
}

vf2_status vf2_model2a_set_interrupt_enable(vf2_model2a *machine, uint32_t mask)
{
    if (machine == NULL || machine->interrupt_control == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    write_le32(machine->interrupt_control + 4u, mask);
    return VF2_OK;
}

vf2_status vf2_model2a_get_interrupt_state(
    const vf2_model2a *machine,
    uint32_t *request,
    uint32_t *enable
)
{
    if (machine == NULL || machine->interrupt_control == NULL ||
        request == NULL || enable == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *request = read_le32(machine->interrupt_control);
    *enable = read_le32(machine->interrupt_control + 4u);
    return VF2_OK;
}

vf2_status vf2_model2a_advance_cycles(
    vf2_model2a *machine,
    uint64_t cycles
)
{
    size_t timer = 0u;

    if (machine == NULL || machine->timers == NULL ||
        machine->video_control == NULL || machine->interrupt_control == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (timer = 0u; timer < 4u; ++timer) {
        const uint32_t value = model2a_timer_value(machine, timer);
        if (!model2a_timer_running(machine, timer)) {
            continue;
        }
        if (cycles >= (uint64_t)value) {
            const uint32_t line = UINT32_C(1) << (unsigned)(timer + 2u);
            model2a_set_timer_value(machine, timer, VF2_MODEL2A_TIMER_RELOAD);
            model2a_set_timer_running(machine, timer, 0);
            if ((read_le32(machine->interrupt_control + 4u) & line) != 0u) {
                (void)vf2_model2a_raise_interrupt(machine, line);
            }
        } else {
            model2a_set_timer_value(machine, timer,
                                    value - (uint32_t)cycles);
        }
    }
    return VF2_OK;
}

vf2_status vf2_model2a_advance_frame(vf2_model2a *machine)
{
    if (machine == NULL || machine->video_control == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    model2a_set_frame_number(machine, model2a_frame_number(machine) + 1u);
    return VF2_OK;
}

vf2_status vf2_model2a_get_frame_number(
    const vf2_model2a *machine,
    uint32_t *frame_number
)
{
    if (machine == NULL || machine->video_control == NULL ||
        frame_number == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    *frame_number = model2a_frame_number(machine);
    return VF2_OK;
}
