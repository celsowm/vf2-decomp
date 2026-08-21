#ifndef VF2_MODEL2A_H
#define VF2_MODEL2A_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

typedef vf2_status (*vf2_model2a_copro_read_callback)(
    void *context,
    uint32_t address,
    void *destination,
    size_t size
);
typedef vf2_status (*vf2_model2a_copro_write_callback)(
    void *context,
    uint32_t address,
    const void *source,
    size_t size
);

typedef enum vf2_model2a_memory_access_kind {
    VF2_MODEL2A_MEMORY_READ = 0,
    VF2_MODEL2A_MEMORY_WRITE
} vf2_model2a_memory_access_kind;

typedef struct vf2_model2a_memory_access {
    vf2_model2a_memory_access_kind kind;
    uint32_t address;
    const void *data;
    size_t size;
} vf2_model2a_memory_access;

typedef void (*vf2_model2a_memory_observer)(
    const vf2_model2a_memory_access *access,
    void *context
);

enum {
    VF2_MAIN_ROM_BASE = 0x00000000u,
    VF2_MAIN_ROM_SIZE = 0x00200000u,
    VF2_MAIN_DATA_BASE = 0x02000000u,
    VF2_MAIN_DATA_PRIMARY_SIZE = 0x02000000u,
    VF2_MAIN_DATA_EXTRA_BASE = 0x06000000u,
    VF2_MAIN_DATA_EXTRA_OFFSET = 0x01000000u,
    VF2_MAIN_DATA_EXTRA_SIZE = 0x01000000u,
    VF2_MAIN_DATA_SIZE = 0x02400000u,
    VF2_WORK_RAM_BASE = 0x00500000u,
    VF2_WORK_RAM_SIZE = 0x00100000u,
    VF2_GEOMETRY_BASE = 0x00800000u,
    VF2_GEOMETRY_SIZE = 0x00008000u,
    VF2_COPRO_PORT_BASE = 0x00880000u,
    VF2_COPRO_PORT_SIZE = 0x00008000u,
    VF2_BUFFER_RAM_BASE = 0x00900000u,
    VF2_BUFFER_RAM_SIZE = 0x00080000u,
    VF2_VIDEO_CONTROL_BASE = 0x00980000u,
    VF2_VIDEO_CONTROL_SIZE = 0x00001000u,
    VF2_CPU_CONTROL_BASE = 0x00e00000u,
    VF2_CPU_CONTROL_SIZE = 0x00001000u,
    VF2_INTERRUPT_CONTROL_BASE = 0x00e80000u,
    VF2_INTERRUPT_CONTROL_SIZE = 0x00001000u,
    VF2_TIMER_BASE = 0x00f00000u,
    VF2_TIMER_SIZE = 0x00000010u,
    VF2_TILE_RAM_BASE = 0x01000000u,
    VF2_TILE_RAM_SIZE = 0x00100000u,
    VF2_PALETTE_RAM_BASE = 0x01800000u,
    VF2_PALETTE_RAM_SIZE = 0x00100000u,
    VF2_IO_CONTROL_BASE = 0x01c00000u,
    VF2_IO_CONTROL_SIZE = 0x00100000u,
    VF2_BACKUP_SRAM_BASE = 0x01d00000u,
    VF2_BACKUP_SRAM_SIZE = 0x00004000u,
    VF2_COPRO_CONTROL_BASE = 0x10000000u,
    VF2_COPRO_CONTROL_SIZE = 0x00200000u,
    VF2_COLOR_TRANSLATION_BASE = 0x01810000u,
    VF2_COLOR_TRANSLATION_SIZE = 0x0000c000u,
    VF2_TEXTURE_RAM0_BASE = 0x12000000u,
    VF2_TEXTURE_RAM1_BASE = 0x12400000u,
    VF2_TEXTURE_RAM_SIZE = 0x00200000u,
    VF2_TEXTURE_RAM_MIRROR = 0x00200000u,
    VF2_LUMA_RAM_BASE = 0x12800000u,
    VF2_LUMA_RAM_SIZE = 0x00020000u,
    VF2_SYSTEM_CONTROL_SIZE = 0x00000100u
};

#define VF2_SYSTEM_CONTROL_BASE UINT32_C(0xff000000)

typedef struct vf2_model2a {
    const uint8_t *main_rom;
    size_t main_rom_size;
    const uint8_t *main_data;
    size_t main_data_size;
    uint8_t *owned_main_data;
    uint8_t *geometry;
    size_t geometry_size;
    uint8_t *copro_port;
    size_t copro_port_size;
    uint8_t *work_ram;
    size_t work_ram_size;
    uint8_t *buffer_ram;
    size_t buffer_ram_size;
    uint8_t *video_control;
    size_t video_control_size;
    uint8_t *cpu_control;
    size_t cpu_control_size;
    uint8_t *interrupt_control;
    size_t interrupt_control_size;
    uint8_t *timers;
    size_t timers_size;
    uint8_t *tile_ram;
    size_t tile_ram_size;
    uint8_t *palette_ram;
    size_t palette_ram_size;
    uint8_t *io_control;
    size_t io_control_size;
    uint8_t *backup_sram;
    size_t backup_sram_size;
    uint8_t *copro_control;
    size_t copro_control_size;
    uint8_t *color_translation;
    size_t color_translation_size;
    uint8_t *texture_ram0;
    size_t texture_ram0_size;
    uint8_t *texture_ram1;
    size_t texture_ram1_size;
    uint8_t *luma_ram;
    size_t luma_ram_size;
    uint8_t *system_control;
    size_t system_control_size;
    uint32_t geometry_write_start;
    uint32_t geometry_read_start;
    uint32_t geometry_control;
    uint32_t geometry_program_count;
    vf2_model2a_copro_read_callback copro_read_callback;
    vf2_model2a_copro_write_callback copro_write_callback;
    void *copro_callback_context;
    vf2_model2a_memory_observer memory_observer;
    void *memory_observer_context;
    uint32_t input;
} vf2_model2a;

int vf2_model2a_initialize(vf2_model2a *machine);
void vf2_model2a_shutdown(vf2_model2a *machine);

vf2_status vf2_model2a_attach_main_rom(
    vf2_model2a *machine,
    const uint8_t *main_rom,
    size_t main_rom_size
);

vf2_status vf2_model2a_attach_main_data(
    vf2_model2a *machine,
    const uint8_t *main_data,
    size_t main_data_size
);

/* Transfer ownership of an allocated main-data image to the machine. */
vf2_status vf2_model2a_take_main_data(
    vf2_model2a *machine,
    uint8_t *main_data,
    size_t main_data_size
);

/* Install optional callbacks for the write-only/read-write coprocessor port.
 * A callback may return VF2_ERROR_UNSUPPORTED to retain the ordinary flat-RAM
 * behavior for an access it does not model. */
vf2_status vf2_model2a_set_copro_callbacks(
    vf2_model2a *machine,
    vf2_model2a_copro_read_callback read_callback,
    vf2_model2a_copro_write_callback write_callback,
    void *context
);

/* Observe successful public bus reads/writes without affecting their result.
 * access->data is valid only for the duration of the callback. */
vf2_status vf2_model2a_set_memory_observer(
    vf2_model2a *machine,
    vf2_model2a_memory_observer observer,
    void *context
);

/* Set the host input mask using VF2_PLATFORM_BUTTON_* bit definitions. */
vf2_status vf2_model2a_set_input(vf2_model2a *machine, uint32_t input);

vf2_status vf2_model2a_read(
    const vf2_model2a *machine,
    uint32_t address,
    void *destination,
    size_t size
);

vf2_status vf2_model2a_write(
    vf2_model2a *machine,
    uint32_t address,
    const void *source,
    size_t size
);

vf2_status vf2_model2a_read_u32(
    const vf2_model2a *machine,
    uint32_t address,
    uint32_t *value
);

vf2_status vf2_model2a_write_u32(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t value
);

/* Model 2 interrupt controller helpers. */
vf2_status vf2_model2a_raise_interrupt(vf2_model2a *machine, uint32_t mask);
vf2_status vf2_model2a_set_interrupt_enable(vf2_model2a *machine, uint32_t mask);
vf2_status vf2_model2a_get_interrupt_state(
    const vf2_model2a *machine,
    uint32_t *request,
    uint32_t *enable
);

#endif
