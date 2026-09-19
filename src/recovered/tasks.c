#include "vf2/recovered.h"
#include "vf2/fighter_candidate.h"
#include "vf2/i960/executor.h"

#include <string.h>

#define VF2_TASK_SOUND_CONTINUATION UINT32_C(0x00043abc)
#define VF2_TASK_SOUND_LOCAL_BUFFER_OFFSET UINT32_C(0x40)
#define VF2_TASK_SOUND_LOCAL_BUFFER_SIZE UINT32_C(0x30)
#define VF2_TASK_SOUND_GLOBAL_BUFFER UINT32_C(0x00504070)
#define VF2_TASK_SOUND_GLOBAL_BUFFER_SIZE UINT32_C(0x08)

static int task_registry_range_valid(uint32_t registry_address, uint32_t required_size)
{
    const uint64_t start = registry_address;
    const uint64_t end = start + required_size;
    const uint64_t work_start = VF2_WORK_RAM_BASE;
    const uint64_t work_end = work_start + VF2_WORK_RAM_SIZE;
    return start >= work_start && end <= work_end;
}

vf2_status vf2_recovered_task_user_execute(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
)
{
    vf2_recovered_task_report local_report;

    if (machine == NULL || !task_registry_range_valid(registry_address, 0x40u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_point = UINT32_C(0x00029748);
    local_report.registry_address = registry_address;
    local_report.continuation = 0u;
    local_report.bytes_written = 0u;
    local_report.global_bytes_written = 0u;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_recovered_task_sound_initialize(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
)
{
    vf2_recovered_task_report local_report;
    uint8_t zero_local[VF2_TASK_SOUND_LOCAL_BUFFER_SIZE];
    uint8_t zero_global[VF2_TASK_SOUND_GLOBAL_BUFFER_SIZE];
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !task_registry_range_valid(registry_address, UINT32_C(0x80))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    memset(&local_report, 0, sizeof(local_report));
    memset(zero_local, 0, sizeof(zero_local));
    memset(zero_global, 0, sizeof(zero_global));

    status = vf2_model2a_write_u32(
        machine,
        registry_address + UINT32_C(0x0c),
        VF2_TASK_SOUND_CONTINUATION
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            registry_address + UINT32_C(0x78),
            registry_address + VF2_TASK_SOUND_LOCAL_BUFFER_OFFSET
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            registry_address + UINT32_C(0x7c),
            registry_address + VF2_TASK_SOUND_LOCAL_BUFFER_OFFSET
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            registry_address + VF2_TASK_SOUND_LOCAL_BUFFER_OFFSET,
            zero_local,
            sizeof(zero_local)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine,
            VF2_TASK_SOUND_GLOBAL_BUFFER,
            zero_global,
            sizeof(zero_global)
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.entry_point = UINT32_C(0x000439fc);
    local_report.registry_address = registry_address;
    local_report.continuation = VF2_TASK_SOUND_CONTINUATION;
    local_report.bytes_written = 3u * sizeof(uint32_t) + sizeof(zero_local);
    local_report.global_bytes_written = sizeof(zero_global);
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_recovered_task_game_info_first_dispatch(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
)
{
    vf2_recovered_task_report local_report;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint8_t zero = 0u;
    uint8_t countdown = 0u;
    size_t bytes_written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || !task_registry_range_valid(registry_address, 0x40u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &fighter1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter0, &fighter0_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter1, &fighter1_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    /* Calls at 0x18144/0x18644 are outside this recovered first-dispatch path. */
    if ((fighter0_flags & UINT32_C(0x80000000)) != 0u ||
        (fighter1_flags & UINT32_C(0x80000000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if ((runtime_flags & UINT32_C(1 << 5)) == 0u) {
        status = vf2_model2a_write(
            machine, fighter0 + VF2_FIGHTER_OFF_1200, &zero, sizeof(zero)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_write(
                machine, fighter1 + VF2_FIGHTER_OFF_1200, &zero, sizeof(zero)
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
            );
        }
        if (status == VF2_OK && countdown != 0u) {
            --countdown;
            status = vf2_model2a_write(
                machine, UINT32_C(0x0050a0b6), &countdown, sizeof(countdown)
            );
            ++bytes_written;
        }
        if (status != VF2_OK) {
            return status;
        }
        bytes_written += 2u;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_point = UINT32_C(0x0001645c);
    local_report.registry_address = registry_address;
    local_report.bytes_written = bytes_written;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_recovered_task_osage_first_dispatch(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_task_report *report
)
{
    vf2_recovered_task_report local_report;
    uint8_t instance = 0u;
    uint16_t zero16 = 0u;
    uint32_t fighter_pointer_slot = 0u;
    uint32_t fighter = 0u;
    uint32_t fighter_flags = 0u;
    uint32_t buffer_address = 0u;
    uint32_t buffer_size = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !task_registry_range_valid(registry_address, UINT32_C(0x144))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read(
        machine, registry_address + UINT32_C(0x04), &instance, sizeof(instance)
    );
    if (status != VF2_OK || instance > 1u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    fighter_pointer_slot = instance == 0u
        ? UINT32_C(0x00500804) : UINT32_C(0x00500808);
    buffer_address = instance == 0u
        ? UINT32_C(0x0091e800) : UINT32_C(0x0091f000);
    buffer_size = instance == 0u
        ? UINT32_C(0x00007a00) : UINT32_C(0x00007c00);

    status = vf2_model2a_read_u32(machine, fighter_pointer_slot, &fighter);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter, &fighter_flags);
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((fighter_flags & UINT32_C(1 << 7)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(
        machine, registry_address + UINT32_C(0x40), fighter
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x48), buffer_address
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x4c), buffer_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(
            machine, registry_address + UINT32_C(0x06), &zero16, sizeof(zero16)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x138), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x13c), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x140), 0u
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_point = UINT32_C(0x000640f4);
    local_report.registry_address = registry_address;
    local_report.bytes_written = 26u;
    /* ROM 0x640f8: cmpobne 0, instance. CC from that compare only
     * (bbc bit7 does not write CC; bit7-set body is unrecovered). */
    local_report.last_compare_result = instance == 0u
        ? (uint32_t)VF2_I960_COMPARE_EQUAL
        : (uint32_t)VF2_I960_COMPARE_LESS;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

#define VF2_TASK_CAMERA_ENTRY UINT32_C(0x0001d320)
#define VF2_TASK_CAMERA_CONTINUATION UINT32_C(0x0001d458)
#define VF2_TASK_CAMERA_PALETTE_INDEX_TABLE UINT32_C(0x02100800)
#define VF2_TASK_CAMERA_PALETTE_SOURCE UINT32_C(0x02100000)
#define VF2_TASK_CAMERA_PALETTE_DESTINATION UINT32_C(0x01802000)
#define VF2_TASK_KILL_OSAGE_ENTRY UINT32_C(0x000657dc)
#define VF2_TASK_OSAGE_CONTINUATION UINT32_C(0x0006428c)
#define VF2_TASK_OSAGE0_POINTER UINT32_C(0x00500868)
#define VF2_TASK_OSAGE1_POINTER UINT32_C(0x0050086c)
#define VF2_TASK_KILL_OSAGE_ORDER_FLAGS UINT32_C(0x00500020)
#define VF2_TASK_KILL_OSAGE_COUNTER UINT32_C(0x00500164)
#define VF2_TASK_KILL_OSAGE_THRESHOLD UINT32_C(0x00004268)
#define VF2_TASK_KILL_OSAGE_TIMER_RELOAD UINT32_C(0x000fffff)

static vf2_status task_write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status task_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2];
    vf2_status status = VF2_OK;
    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status task_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static vf2_status camera_initialize_palette(
    vf2_model2a *machine,
    size_t *entries_written
)
{
    uint16_t count = 0u;
    uint32_t index = 1u;
    size_t written = 0u;
    vf2_status status = task_read_u16(
        machine, VF2_TASK_CAMERA_PALETTE_INDEX_TABLE, &count
    );

    while (status == VF2_OK && index <= (uint32_t)count) {
        uint16_t palette_index = 0u;
        uint16_t packed = 0u;
        uint16_t red = 0u;
        uint16_t green = 0u;
        uint16_t blue = 0u;

        status = task_read_u16(
            machine,
            VF2_TASK_CAMERA_PALETTE_INDEX_TABLE + index * 2u,
            &palette_index
        );
        if (status == VF2_OK) {
            status = task_read_u16(
                machine,
                VF2_TASK_CAMERA_PALETTE_SOURCE + (uint32_t)palette_index * 2u,
                &packed
            );
        }
        if (status != VF2_OK) {
            break;
        }

        red = (uint16_t)(((packed & UINT16_C(0x001f)) + 16u) >> 1u);
        green = (uint16_t)((((packed >> 5u) & UINT16_C(0x001f)) + 16u) >> 1u);
        blue = (uint16_t)((((packed >> 10u) & UINT16_C(0x001f)) + 16u) >> 1u);
        packed = (uint16_t)(red | (uint16_t)(green << 5u) |
                            (uint16_t)(blue << 10u));
        status = task_write_u16(
            machine,
            VF2_TASK_CAMERA_PALETTE_DESTINATION + (uint32_t)palette_index * 2u,
            packed
        );
        if (status == VF2_OK) {
            ++written;
        }
        ++index;
    }

    if (entries_written != NULL) {
        *entries_written = written;
    }
    return status;
}

vf2_status vf2_recovered_task_camera_initialize(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_init_report *report
)
{
    vf2_recovered_camera_init_report local_report;
    uint32_t flags = 0u;
    uint32_t fighter = 0u;
    uint32_t fighter_flags = 0u;
    uint16_t inherited_angle = 0u;
    size_t palette_entries = 0u;
    size_t task_bytes = 0u;
    size_t global_bytes = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !task_registry_range_valid(registry_address, UINT32_C(0x2d1))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));

#define CAMERA_WRITE_U32(offset_, value_)                                      \
    do {                                                                        \
        if (status == VF2_OK) {                                                 \
            status = vf2_model2a_write_u32(                                     \
                machine, registry_address + UINT32_C(offset_), UINT32_C(value_) \
            );                                                                  \
            if (status == VF2_OK) {                                             \
                task_bytes += sizeof(uint32_t);                                 \
            }                                                                   \
        }                                                                       \
    } while (0)
#define CAMERA_WRITE_U16(offset_, value_)                                      \
    do {                                                                       \
        if (status == VF2_OK) {                                                \
            status = task_write_u16(                                           \
                machine, registry_address + UINT32_C(offset_),                 \
                (uint16_t)UINT16_C(value_)                                     \
            );                                                                 \
            if (status == VF2_OK) {                                            \
                task_bytes += sizeof(uint16_t);                                \
            }                                                                  \
        }                                                                      \
    } while (0)
#define CAMERA_WRITE_U8(offset_, value_)                                      \
    do {                                                                      \
        if (status == VF2_OK) {                                               \
            status = task_write_u8(                                           \
                machine, registry_address + UINT32_C(offset_),                \
                (uint8_t)UINT8_C(value_)                                      \
            );                                                                \
            if (status == VF2_OK) {                                           \
                task_bytes += sizeof(uint8_t);                                \
            }                                                                 \
        }                                                                     \
    } while (0)

    CAMERA_WRITE_U32(0x18, 0xbe570a3d);
    CAMERA_WRITE_U32(0x1c, 0x3f47ae14);
    CAMERA_WRITE_U32(0x20, 0xc0d3d70a);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501084), UINT32_C(0x44160000)
        );
        if (status == VF2_OK) {
            global_bytes += sizeof(uint32_t);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x00501088), UINT32_C(0x44160000)
        );
        if (status == VF2_OK) {
            global_bytes += sizeof(uint32_t);
        }
    }
    CAMERA_WRITE_U32(0x18, 0x00000000);
    CAMERA_WRITE_U32(0x1c, 0x3f99999a);
    CAMERA_WRITE_U32(0x20, 0xc0a66666);
    CAMERA_WRITE_U16(0x24, 0x0000);
    CAMERA_WRITE_U16(0x26, 0x0000);
    CAMERA_WRITE_U16(0x28, 0x0000);
    CAMERA_WRITE_U32(0x48, 0x3f733333);
    CAMERA_WRITE_U32(0x50, 0x3ecccccd);
    CAMERA_WRITE_U32(0x54, 0xbf4ccccd);
    CAMERA_WRITE_U32(0x44, 0x3f400000);
    CAMERA_WRITE_U32(0x58, 0x3faccccd);
    CAMERA_WRITE_U32(0x64, 0x3f800000);
    CAMERA_WRITE_U16(0x1a8, 0x0100);
    CAMERA_WRITE_U16(0x1b0, 0x1800);
    CAMERA_WRITE_U16(0x1b2, 0x2800);
    CAMERA_WRITE_U32(0x1b4, 0x3f800000);
    CAMERA_WRITE_U32(0x1b8, 0x40333333);
    CAMERA_WRITE_U8(0x40, 0x01);
    CAMERA_WRITE_U8(0xde, 0x03);
    CAMERA_WRITE_U8(0xb0, 0x00);
    CAMERA_WRITE_U32(0xfc, 0x00000000);

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, registry_address, &flags);
    }
    if (status == VF2_OK) {
        flags &= ~(UINT32_C(1) << 6u);
        status = vf2_model2a_write_u32(machine, registry_address, flags);
        if (status == VF2_OK) {
            task_bytes += sizeof(uint32_t);
        }
    }
    if (status == VF2_OK) {
        status = camera_initialize_palette(machine, &palette_entries);
        if (status == VF2_OK) {
            global_bytes += palette_entries * sizeof(uint16_t);
        }
    }

    /* sub_0001f148. The observed first dispatch takes the branch where fighter
     * secondary setup is disabled. The alternate helper path remains guarded. */
    CAMERA_WRITE_U32(0x18, 0x00000000);
    CAMERA_WRITE_U32(0x1c, 0x3f4f5c29);
    CAMERA_WRITE_U32(0x20, 0xc0a0a3d7);
    if (status == VF2_OK) {
        status = task_read_u16(
            machine, registry_address + UINT32_C(0xf8), &inherited_angle
        );
    }
    if (status == VF2_OK) {
        status = task_write_u16(
            machine, registry_address + UINT32_C(0x24), inherited_angle
        );
        if (status == VF2_OK) {
            task_bytes += sizeof(uint16_t);
        }
    }
    CAMERA_WRITE_U16(0x26, 0x0000);
    CAMERA_WRITE_U32(0x178, 0x00000000);
    CAMERA_WRITE_U8(0x1aa, 0x00);
    CAMERA_WRITE_U8(0x2d0, 0x00);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter, &fighter_flags);
    }
    if (status == VF2_OK && (fighter_flags & (UINT32_C(1) << 7u)) != 0u) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            registry_address + UINT32_C(0x0c),
            VF2_TASK_CAMERA_CONTINUATION
        );
        if (status == VF2_OK) {
            task_bytes += sizeof(uint32_t);
        }
    }

#undef CAMERA_WRITE_U32
#undef CAMERA_WRITE_U16
#undef CAMERA_WRITE_U8

    if (status != VF2_OK) {
        return status;
    }
    local_report.entry_point = VF2_TASK_CAMERA_ENTRY;
    local_report.registry_address = registry_address;
    local_report.continuation = VF2_TASK_CAMERA_CONTINUATION;
    local_report.task_bytes_written = task_bytes;
    local_report.global_bytes_written = global_bytes;
    local_report.palette_entries_written = palette_entries;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}


static float task_bits_to_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t task_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

uint32_t vf2_recovered_camera_classify_range(
    uint32_t first_value,
    uint32_t vertical_value,
    uint32_t second_value,
    uint32_t range_value,
    uint32_t vertical_limit
)
{
    uint32_t result = 0u;
    const uint32_t negative_range = range_value | UINT32_C(0x80000000);

    if ((int32_t)first_value > (int32_t)range_value) {
        result |= UINT32_C(1 << 3);
    }
    if (first_value > negative_range) {
        result |= UINT32_C(1 << 4);
    }
    if ((int32_t)second_value > (int32_t)range_value) {
        result |= (UINT32_C(1) << 1u);
    }
    if (second_value > negative_range) {
        result |= (UINT32_C(1) << 2u);
    }
    if (result == 0u &&
        task_bits_to_float(vertical_value) <= task_bits_to_float(vertical_limit)) {
        const uint32_t first_abs = first_value & UINT32_C(0x7fffffff);
        const uint32_t second_abs = second_value & UINT32_C(0x7fffffff);
        uint32_t bit = 1u;
        uint32_t selected = second_value;
        if (second_abs < first_abs) {
            bit = 3u;
            selected = first_value;
        }
        if ((selected & UINT32_C(0x80000000)) != 0u) {
            ++bit;
        }
        result |= UINT32_C(1) << bit;
    }
    return result;
}

static vf2_status camera_initialize_fighter_tracking(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t fighter,
    uint32_t tracking_offset,
    size_t *bytes_written
)
{
    uint32_t fighter_mode = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t z = 0u;
    const uint32_t tracking = registry_address + tracking_offset;
    size_t written = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, fighter + VF2_FIGHTER_OFF_01A4, &fighter_mode
    );

    if (status == VF2_OK && (fighter_mode & UINT32_C(0x1f)) != 0u) {
        status = vf2_model2a_read_u32(
            machine, fighter + VF2_FIGHTER_OFF_01F4, &x
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, fighter + VF2_FIGHTER_OFF_01FC, &z
            );
        }
    } else if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x18), &x
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, fighter + UINT32_C(0x20), &z
            );
        }
    }
    if (status == VF2_OK && (fighter_mode & (UINT32_C(1) << 4u)) != 0u) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x1f8), &y
        );
    } else if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, fighter + UINT32_C(0x1c), &y
        );
        if (status == VF2_OK) {
            y = task_float_to_bits(
                task_bits_to_float(y) + task_bits_to_float(UINT32_C(0x3f666666))
            );
        }
    }

#define TRACK_U32(offset_, value_)                                           \
    do {                                                                      \
        if (status == VF2_OK) {                                               \
            status = vf2_model2a_write_u32(                                   \
                machine, tracking + UINT32_C(offset_), (uint32_t)(value_)     \
            );                                                                \
            if (status == VF2_OK) { written += sizeof(uint32_t); }            \
        }                                                                     \
    } while (0)

    TRACK_U32(0x00, 0u);
    TRACK_U32(0x04, x);
    TRACK_U32(0x1c, x);
    TRACK_U32(0x10, 0u);
    TRACK_U32(0x08, y);
    TRACK_U32(0x20, y);
    TRACK_U32(0x14, 0u);
    TRACK_U32(0x0c, z);
    TRACK_U32(0x24, z);
    TRACK_U32(0x18, 0u);

#undef TRACK_U32

    if (bytes_written != NULL) {
        *bytes_written = written;
    }
    return status;
}

static vf2_status camera_apply_measured_bit7_mode2(
    vf2_model2a *machine,
    uint32_t registry_address,
    size_t *bytes_written,
    size_t *helpers_recovered
)
{
    static const struct {
        uint32_t offset;
        uint32_t expected;
    } camera_guards[] = {
        {UINT32_C(0x18), UINT32_C(0x00000000)},
        {UINT32_C(0x1c), UINT32_C(0x3f4f5c29)},
        {UINT32_C(0x20), UINT32_C(0xc0a0a3d7)},
        {UINT32_C(0x44), UINT32_C(0x3f400000)},
        {UINT32_C(0x48), UINT32_C(0x3f733333)},
        {UINT32_C(0x4c), UINT32_C(0x3f800000)},
        {UINT32_C(0x50), UINT32_C(0x3ecccccd)},
        {UINT32_C(0x54), UINT32_C(0xbf4ccccd)},
        {UINT32_C(0x58), UINT32_C(0x3faccccd)},
        {UINT32_C(0x5c), UINT32_C(0x435f3333)},
        {UINT32_C(0x60), UINT32_C(0x432ccccd)},
        {UINT32_C(0x64), UINT32_C(0x3f800000)},
        {UINT32_C(0x1b4), UINT32_C(0x3f800000)},
        {UINT32_C(0x1b8), UINT32_C(0x40333333)},
        {UINT32_C(0x20c), UINT32_C(0x3fb33333)}
    };
    static const struct {
        uint32_t offset;
        uint32_t value;
    } writes[] = {
        {UINT32_C(0x1c), UINT32_C(0x7f800000)},
        {UINT32_C(0x20), UINT32_C(0x00000000)},
        {UINT32_C(0x190), UINT32_C(0x432ccccd)},
        {UINT32_C(0x194), UINT32_C(0x42ee147b)},
        {UINT32_C(0x198), UINT32_C(0x43666667)},
        {UINT32_C(0x19c), UINT32_C(0xc3666667)},
        {UINT32_C(0x1a0), UINT32_C(0x3ebf964d)},
        {UINT32_C(0x1bc), UINT32_C(0x80000000)},
        {UINT32_C(0x1e4), UINT32_C(0x80000000)}
    };
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t value = 0u;
    uint32_t fighter_index = 0u;
    size_t index = 0u;
    size_t written = 0u;
    vf2_status status = VF2_OK;

    for (index = 0u; status == VF2_OK &&
         index < sizeof(camera_guards) / sizeof(camera_guards[0]); ++index) {
        status = vf2_model2a_read_u32(
            machine, registry_address + camera_guards[index].offset, &value
        );
        if (status == VF2_OK && value != camera_guards[index].expected) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        uint8_t counter = 0u;
        uint8_t limit = 0u;
        status = vf2_model2a_read(
            machine, registry_address + UINT32_C(0x2d0), &counter, sizeof(counter)
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read(
                machine, registry_address + UINT32_C(0x2d1), &limit, sizeof(limit)
            );
        }
        if (status == VF2_OK && (counter != 0u || limit != UINT8_C(0x3c))) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &fighter1);
    }
    if (status == VF2_OK) {
        static const struct {
            uint32_t address;
            uint32_t expected;
        } global_guards[] = {
            {UINT32_C(0x0050a150), UINT32_C(0x3f800000)},
            {UINT32_C(0x00501084), UINT32_C(0x44160000)},
            {UINT32_C(0x00501088), UINT32_C(0x44160000)}
        };
        for (index = 0u; status == VF2_OK &&
             index < sizeof(global_guards) / sizeof(global_guards[0]); ++index) {
            status = vf2_model2a_read_u32(
                machine, global_guards[index].address, &value
            );
            if (status == VF2_OK && value != global_guards[index].expected) {
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
    }
    if (status == VF2_OK) {
        static const uint32_t tracking_expected[10] = {
            UINT32_C(0x00000000), UINT32_C(0x00000000),
            UINT32_C(0x3f666666), UINT32_C(0x00000000),
            UINT32_C(0x00000000), UINT32_C(0x00000000),
            UINT32_C(0x00000000), UINT32_C(0x00000000),
            UINT32_C(0x3f666666), UINT32_C(0x00000000)
        };
        const uint32_t tracking_offsets[2] = {
            UINT32_C(0x1bc), UINT32_C(0x1e4)
        };
        uint32_t tracking_index = 0u;
        for (tracking_index = 0u; status == VF2_OK && tracking_index < 2u;
             ++tracking_index) {
            for (index = 0u; status == VF2_OK && index < 10u; ++index) {
                status = vf2_model2a_read_u32(
                    machine,
                    registry_address + tracking_offsets[tracking_index] +
                        (uint32_t)index * UINT32_C(4),
                    &value
                );
                if (status == VF2_OK && value != tracking_expected[index]) {
                    status = VF2_ERROR_UNSUPPORTED;
                }
            }
        }
    }
    for (fighter_index = 0u; status == VF2_OK && fighter_index < 2u;
         ++fighter_index) {
        const uint32_t fighter = fighter_index == 0u ? fighter0 : fighter1;
        uint32_t flags = 0u;
        uint32_t mode = 0u;
        uint32_t x = 0u;
        uint32_t y = 0u;
        uint32_t z = 0u;
        status = vf2_model2a_read_u32(machine, fighter, &flags);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, fighter + VF2_FIGHTER_OFF_01A4, &mode);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, fighter + UINT32_C(0x18), &x);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, fighter + UINT32_C(0x1c), &y);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, fighter + UINT32_C(0x20), &z);
        }
        if (status == VF2_OK &&
            (((flags & (UINT32_C(1) << 7u)) == 0u) || mode != 0u ||
             x != 0u || y != 0u || z != 0u)) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    for (index = 0u; status == VF2_OK && index < sizeof(writes) / sizeof(writes[0]);
         ++index) {
        status = vf2_model2a_write_u32(
            machine, registry_address + writes[index].offset, writes[index].value
        );
        if (status == VF2_OK) {
            written += sizeof(uint32_t);
        }
    }
    if (status == VF2_OK) {
        status = task_write_u16(
            machine, registry_address + UINT32_C(0x1ae), UINT16_C(0x1800)
        );
        if (status == VF2_OK) {
            written += sizeof(uint16_t);
        }
    }
    if (status == VF2_OK) {
        status = task_write_u8(
            machine, registry_address + UINT32_C(0x2d0), UINT8_C(1)
        );
        if (status == VF2_OK) {
            ++written;
        }
    }
    if (bytes_written != NULL) {
        *bytes_written = written;
    }
    if (helpers_recovered != NULL) {
        *helpers_recovered = 3u;
    }
    return status;
}

static vf2_status camera_reset_first_dispatch(
    vf2_model2a *machine,
    uint32_t registry_address,
    size_t *bytes_written,
    size_t *helpers_recovered
)
{
    uint16_t inherited_angle = 0u;
    uint32_t fighter = 0u;
    uint32_t fighter_flags = 0u;
    size_t written = 0u;
    size_t tracking_bytes = 0u;
    size_t helper_count = 0u;
    vf2_status status = task_read_u16(
        machine, registry_address + UINT32_C(0xf8), &inherited_angle
    );

#define RESET_U32(offset_, value_)                                            \
    do {                                                                       \
        if (status == VF2_OK) {                                                \
            status = vf2_model2a_write_u32(                                    \
                machine, registry_address + UINT32_C(offset_), UINT32_C(value_)\
            );                                                                 \
            if (status == VF2_OK) { written += sizeof(uint32_t); }             \
        }                                                                      \
    } while (0)
#define RESET_U16(offset_, value_)                                            \
    do {                                                                       \
        if (status == VF2_OK) {                                                \
            status = task_write_u16(                                           \
                machine, registry_address + UINT32_C(offset_), (uint16_t)(value_)\
            );                                                                 \
            if (status == VF2_OK) { written += sizeof(uint16_t); }             \
        }                                                                      \
    } while (0)
#define RESET_U8(offset_, value_)                                             \
    do {                                                                       \
        if (status == VF2_OK) {                                                \
            status = task_write_u8(                                            \
                machine, registry_address + UINT32_C(offset_), (uint8_t)(value_)\
            );                                                                 \
            if (status == VF2_OK) { written += sizeof(uint8_t); }              \
        }                                                                      \
    } while (0)

    RESET_U32(0x18, 0x00000000);
    RESET_U32(0x1c, 0x3f4f5c29);
    RESET_U32(0x20, 0xc0a0a3d7);
    RESET_U16(0x24, inherited_angle);
    RESET_U16(0x26, 0x0000);
    RESET_U32(0x178, 0x00000000);
    RESET_U8(0x1aa, 0x00);
    RESET_U8(0x2d0, 0x00);

#undef RESET_U32
#undef RESET_U16
#undef RESET_U8

    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, fighter, &fighter_flags);
    }
    if (status == VF2_OK && (fighter_flags & (UINT32_C(1) << 7u)) != 0u) {
        status = camera_initialize_fighter_tracking(
            machine, registry_address, fighter, UINT32_C(0x1bc),
            &tracking_bytes
        );
        if (status == VF2_OK) {
            written += tracking_bytes;
            ++helper_count;
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00500808), &fighter
            );
        }
        if (status == VF2_OK) {
            status = camera_initialize_fighter_tracking(
                machine, registry_address, fighter, UINT32_C(0x1e4),
                &tracking_bytes
            );
        }
        if (status == VF2_OK) {
            uint8_t mode = 0u;
            written += tracking_bytes;
            ++helper_count;
            status = vf2_model2a_read(
                machine, registry_address + UINT32_C(0x40),
                &mode, sizeof(mode)
            );
            if (status == VF2_OK) {
                mode = (uint8_t)(mode + UINT8_C(1));
                status = task_write_u8(
                    machine, registry_address + UINT32_C(0x40), mode
                );
                if (status == VF2_OK) {
                    ++written;
                }
            }
        }
    }
    if (bytes_written != NULL) {
        *bytes_written = written;
    }
    if (helpers_recovered != NULL) {
        *helpers_recovered = helper_count;
    }
    return status;
}

vf2_status vf2_recovered_task_camera_first_update(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_update_report *report
)
{
    vf2_recovered_camera_update_report local_report;
    uint32_t task_flags = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t mode = 0u;
    uint32_t mode_handler = 0u;
    uint32_t first_value = 0u;
    uint32_t vertical_value = 0u;
    uint32_t second_value = 0u;
    uint32_t range_value = 0u;
    uint32_t vertical_limit = 0u;
    uint32_t fighter = 0u;
    uint32_t profile = 0u;
    uint8_t input_index = 0u;
    uint16_t input_flags = 0u;
    size_t task_bytes = 0u;
    size_t global_bytes = 0u;
    size_t reset_bytes = 0u;
    size_t reset_helpers = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !task_registry_range_valid(registry_address, UINT32_C(0x2d1))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));

    status = vf2_model2a_read_u32(machine, registry_address, &task_flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &runtime_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500064), &input_index, sizeof(input_index)
        );
    }
    if (status == VF2_OK) {
        status = task_read_u16(
            machine,
            UINT32_C(0x0006eea0) + ((uint32_t)input_index << 8u),
            &input_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((task_flags & UINT32_C(1 << 6)) != 0u ||
        (runtime_flags & UINT32_C(1 << 5)) != 0u ||
        (input_flags & UINT16_C(0x0078)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(
        machine, registry_address + UINT32_C(0x5c), UINT32_C(0x435f3333)
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, registry_address + UINT32_C(0x60), UINT32_C(0x432ccccd)
        );
    }
    if (status == VF2_OK) {
        task_bytes += 2u * sizeof(uint32_t);
        status = vf2_model2a_read(
            machine, registry_address + UINT32_C(0x40), &mode, sizeof(uint8_t)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0006e2e4) + (mode & UINT32_C(0xff)) * 4u,
            &mode_handler
        );
    }
    if (status == VF2_OK && mode_handler == UINT32_C(0x0001f148)) {
        status = camera_reset_first_dispatch(
            machine, registry_address, &reset_bytes, &reset_helpers
        );
        task_bytes += reset_bytes;
    } else if (status == VF2_OK && mode_handler == UINT32_C(0x0001f1c0)) {
        status = camera_apply_measured_bit7_mode2(
            machine, registry_address, &reset_bytes, &reset_helpers
        );
        task_bytes += reset_bytes;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, registry_address + UINT32_C(0x18), &first_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, registry_address + UINT32_C(0x1c), &vertical_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, registry_address + UINT32_C(0x20), &second_value
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_COPRO_PORT_BASE + UINT32_C(0x4000), first_value
        );
    }
    if (status == VF2_OK) {
        const float candidate = task_bits_to_float(first_value) +
            task_bits_to_float(UINT32_C(0x3d4ccccd));
        if (task_bits_to_float(vertical_value) < candidate) {
            vertical_value = task_float_to_bits(candidate);
            status = vf2_model2a_write_u32(
                machine, registry_address + UINT32_C(0x1c), vertical_value
            );
            if (status == VF2_OK) {
                task_bytes += sizeof(uint32_t);
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a00c), &range_value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a148), &vertical_limit);
    }
    if (status == VF2_OK) {
        local_report.range_flags = vf2_recovered_camera_classify_range(
            first_value, vertical_value, second_value, range_value, vertical_limit
        );
        status = task_write_u8(
            machine,
            registry_address + UINT32_C(0xfa),
            (uint8_t)local_report.range_flags
        );
        if (status == VF2_OK) {
            ++task_bytes;
        }
    }

    if (status == VF2_OK) {
        task_flags |= UINT32_C(1 << 8);
        status = vf2_model2a_write_u32(machine, registry_address, task_flags);
        if (status == VF2_OK) {
            task_bytes += sizeof(uint32_t);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500174), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500178), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050017c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500180), 0u);
    }
    if (status == VF2_OK) {
        global_bytes += 4u * sizeof(uint32_t);
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, fighter + UINT32_C(0x1b0), &mode, sizeof(uint8_t)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a0e0) + (mode & UINT32_C(0xff)) * 4u,
            &profile
        );
    }
    if (status == VF2_OK) {
        local_report.fighter0_profile = profile;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050109c), profile);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &fighter);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, fighter + UINT32_C(0x1b0), &mode, sizeof(uint8_t)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0050a0e0) + (mode & UINT32_C(0xff)) * 4u,
            &profile
        );
    }
    if (status == VF2_OK) {
        local_report.fighter1_profile = profile;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005010a0), profile);
    }
    if (status == VF2_OK) {
        status = task_write_u16(machine, UINT32_C(0x005010e8), 0u);
    }
    if (status == VF2_OK) {
        status = task_write_u16(machine, UINT32_C(0x005010ea), 0u);
    }
    if (status == VF2_OK) {
        global_bytes += 2u * sizeof(uint32_t) + 2u * sizeof(uint16_t);
        task_flags &= ~UINT32_C(1);
        status = vf2_model2a_write_u32(machine, registry_address, task_flags);
        if (status == VF2_OK) {
            task_bytes += sizeof(uint32_t);
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.start_address = UINT32_C(0x0001d458);
    local_report.stop_address = UINT32_C(0x0001d660);
    local_report.registry_address = registry_address;
    local_report.mode_handler = mode_handler;
    local_report.input_flags = input_flags;
    local_report.task_bytes_written = task_bytes;
    local_report.global_bytes_written = global_bytes;
    local_report.copro_scratch_bytes_written = sizeof(uint32_t);
    local_report.helpers_recovered = 4u + reset_helpers;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

vf2_status vf2_recovered_task_camera_post_update_gate(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_gate_report *report
)
{
    vf2_recovered_camera_gate_report local_report;
    uint8_t input_index = 0u;
    uint8_t control_flags = 0u;
    uint8_t camera_flags = 0u;
    uint8_t runtime_mode = 0u;
    uint8_t runtime_phase = 0u;
    uint8_t override_flags = 0u;
    uint16_t input_flags = 0u;
    uint32_t task_flags = 0u;
    vf2_recovered_camera_viewport_report viewport_report;
    size_t writes = 0u;
    size_t viewport_task_bytes = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !task_registry_range_valid(registry_address, UINT32_C(0x2d5))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    memset(&viewport_report, 0, sizeof(viewport_report));
    local_report.start_address = UINT32_C(0x0001d660);
    local_report.registry_address = registry_address;

    status = vf2_model2a_read(
        machine, UINT32_C(0x00500064), &input_index, sizeof(input_index)
    );
    if (status == VF2_OK) {
        status = task_read_u16(
            machine,
            UINT32_C(0x0006eea0) + ((uint32_t)input_index << 8u),
            &input_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    local_report.input_flags = input_flags;

    /* Bit 3 enters the recovered viewport construction block at
     * 0x0001d678. The observed first dispatch has this bit clear, but the
     * composed path is supported for differential validation. */
    if ((input_flags & (UINT16_C(1) << 3u)) != 0u) {
        status = vf2_recovered_task_camera_viewport_construct(
            machine, registry_address, &viewport_report
        );
        if (status != VF2_OK) {
            return status;
        }
        local_report.viewport_executed = 1;
        local_report.viewport_entries_written =
            viewport_report.first_entries_written +
            viewport_report.second_entries_written;
        local_report.viewport_task_bytes_written =
            viewport_report.task_bytes_written;
        viewport_task_bytes = viewport_report.task_bytes_written;
    }

    status = vf2_model2a_read(
        machine, UINT32_C(0x0050009c), &control_flags, sizeof(control_flags)
    );
    if (status != VF2_OK) {
        return status;
    }
    local_report.control_flags = control_flags;

    if ((control_flags & UINT8_C(1)) != 0u) {
        local_report.stop_address = UINT32_C(0x0001e524);
        local_report.task_bytes_written = viewport_task_bytes;
        local_report.fast_exit = 1;
        if (report != NULL) {
            *report = local_report;
        }
        return VF2_OK;
    }

    status = vf2_model2a_read_u32(machine, registry_address, &task_flags);
    if (status == VF2_OK) {
        local_report.initial_task_flags = task_flags;
        task_flags &= ~(UINT32_C(1) << 1u);
        if ((control_flags & (UINT8_C(1) << 1u)) == 0u) {
            task_flags |= (UINT32_C(1) << 1u);
        }
        task_flags &= ~(UINT32_C(1) << 2u);
        status = vf2_model2a_read(
            machine, registry_address + UINT32_C(0xde),
            &camera_flags, sizeof(camera_flags)
        );
    }
    if (status == VF2_OK &&
        (camera_flags & UINT8_C(1)) != 0u &&
        (control_flags & (UINT8_C(1) << 2u)) == 0u) {
        task_flags |= (UINT32_C(1) << 2u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, registry_address, task_flags);
        if (status == VF2_OK) {
            ++writes;
        }
    }

    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x0050002b), &runtime_mode, sizeof(runtime_mode)
        );
    }
    if (status == VF2_OK && (runtime_mode == 8u || runtime_mode == 9u)) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00500031), &runtime_phase, sizeof(runtime_phase)
        );
        if (status == VF2_OK && runtime_phase >= UINT8_C(0x10)) {
            status = vf2_model2a_read(
                machine, registry_address + UINT32_C(0x2d4),
                &override_flags, sizeof(override_flags)
            );
            if (status == VF2_OK && (override_flags & UINT8_C(1)) != 0u) {
                task_flags &= ~(UINT32_C(1) << 1u);
                status = vf2_model2a_write_u32(
                    machine, registry_address, task_flags
                );
                if (status == VF2_OK) {
                    ++writes;
                }
            }
            if (status == VF2_OK && (override_flags & (UINT8_C(1) << 1u)) != 0u) {
                task_flags &= ~(UINT32_C(1) << 2u);
                status = vf2_model2a_write_u32(
                    machine, registry_address, task_flags
                );
                if (status == VF2_OK) {
                    ++writes;
                }
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.stop_address = UINT32_C(0x0001d984);
    local_report.final_task_flags = task_flags;
    local_report.task_flag_writes = writes;
    local_report.task_bytes_written =
        viewport_task_bytes + writes * sizeof(uint32_t);
    local_report.fast_exit = 0;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}

static vf2_status kill_osage_evaluate_record(
    vf2_model2a *machine,
    uint32_t registry_address,
    uint32_t *elapsed_ticks,
    size_t *marked_for_kill,
    size_t *flag_words_written,
    vf2_i960_compare_result *last_compare
)
{
    uint32_t continuation = 0u;
    uint32_t flags = 0u;
    uint32_t accumulated_age = 0u;
    uint32_t kill_counter = 0u;
    vf2_status status = vf2_model2a_read_u32(
        machine, registry_address + UINT32_C(0x0c), &continuation
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, registry_address, &flags);
    }
    if (status != VF2_OK) {
        return status;
    }

    /* ROM 0x65838: mov 0,r7; cmpobne cont,0x6428c → miss; bbs 0 → miss;
     * bbc 2 → miss; ld age; cmpobl age+elapsed,0x4268 → miss; else hit.
     * Last cmpo* is cmpobne (early miss) or cmpobl (late miss/hit). */
    if (continuation != UINT32_C(0x0006428c)) {
        if (last_compare != NULL) {
            *last_compare = continuation < UINT32_C(0x0006428c)
                ? VF2_I960_COMPARE_LESS : VF2_I960_COMPARE_GREATER;
        }
        /* miss: clear bit 3; g6 += 0 (r7 still 0) */
        flags &= ~(UINT32_C(1) << 3u);
        status = vf2_model2a_write_u32(machine, registry_address, flags);
        if (status == VF2_OK) {
            ++*flag_words_written;
        }
        return status;
    }
    if (last_compare != NULL) {
        *last_compare = VF2_I960_COMPARE_EQUAL;
    }
    if ((flags & UINT32_C(1)) != 0u ||
        (flags & (UINT32_C(1) << 2u)) == 0u) {
        flags &= ~(UINT32_C(1) << 3u);
        status = vf2_model2a_write_u32(machine, registry_address, flags);
        if (status == VF2_OK) {
            ++*flag_words_written;
        }
        return status;
    }

    status = vf2_model2a_read_u32(
        machine, registry_address + UINT32_C(0x128), &accumulated_age
    );
    if (status != VF2_OK) {
        return status;
    }
    {
        const uint32_t sum = *elapsed_ticks + accumulated_age;
        const uint32_t limit = UINT32_C(0x00004268);
        if (last_compare != NULL) {
            if (sum < limit) {
                *last_compare = VF2_I960_COMPARE_LESS;
            } else if (sum == limit) {
                *last_compare = VF2_I960_COMPARE_EQUAL;
            } else {
                *last_compare = VF2_I960_COMPARE_GREATER;
            }
        }
        if (sum < limit) {
            /* miss after load: g6 += age */
            flags &= ~(UINT32_C(1) << 3u);
            status = vf2_model2a_write_u32(machine, registry_address, flags);
            if (status == VF2_OK) {
                *elapsed_ticks = sum;
                ++*flag_words_written;
            }
            return status;
        }
    }

    /* hit: set bit 3, bump kill counter. ROM hit path does not add age
     * into g6 (no addo r7,g6,g6). */
    flags |= (UINT32_C(1) << 3u);
    status = vf2_model2a_write_u32(machine, registry_address, flags);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TASK_KILL_OSAGE_COUNTER, &kill_counter
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TASK_KILL_OSAGE_COUNTER, kill_counter + 1u
        );
    }
    if (status == VF2_OK) {
        ++*marked_for_kill;
        ++*flag_words_written;
    }
    return status;
}

vf2_status vf2_recovered_task_kill_osage_execute(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_kill_osage_report *report
)
{
    vf2_recovered_kill_osage_report local_report;
    uint32_t first = 0u;
    uint32_t second = 0u;
    uint32_t order_flags = 0u;
    uint32_t timer3 = 0u;
    uint32_t elapsed = 0u;
    size_t marked = 0u;
    size_t flag_writes = 0u;
    vf2_i960_compare_result last_cc = VF2_I960_COMPARE_NONE;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !task_registry_range_valid(registry_address, UINT32_C(0x40))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));

    status = vf2_model2a_read_u32(machine, VF2_TASK_OSAGE0_POINTER, &first);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_TASK_OSAGE1_POINTER, &second);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TASK_KILL_OSAGE_ORDER_FLAGS, &order_flags
        );
    }
    if (status == VF2_OK && (order_flags & UINT32_C(1)) == 0u) {
        const uint32_t temporary = first;
        first = second;
        second = temporary;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, VF2_TIMER_BASE + UINT32_C(0x0c), &timer3
        );
    }
    if (status != VF2_OK || first == 0u || second == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    elapsed = VF2_TASK_KILL_OSAGE_TIMER_RELOAD -
              (timer3 & VF2_TASK_KILL_OSAGE_TIMER_RELOAD);
    elapsed -= UINT32_C(18);
    elapsed /= UINT32_C(25);

    status = kill_osage_evaluate_record(
        machine, first, &elapsed, &marked, &flag_writes, &last_cc
    );
    if (status == VF2_OK) {
        status = kill_osage_evaluate_record(
            machine, second, &elapsed, &marked, &flag_writes, &last_cc
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.entry_point = VF2_TASK_KILL_OSAGE_ENTRY;
    local_report.first_registry_address = first;
    local_report.second_registry_address = second;
    local_report.elapsed_ticks = elapsed;
    local_report.records_evaluated = 2u;
    local_report.records_marked_for_kill = marked;
    local_report.flag_words_written = flag_writes;
    local_report.last_compare_result = (uint32_t)last_cc;
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
