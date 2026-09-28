#include "vf2/model2a.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

typedef struct observer_state {
    size_t count;
    vf2_model2a_memory_access_kind kind;
    uint32_t address;
    size_t size;
    uint8_t bytes[8];
} observer_state;

static void observe_memory(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    observer_state *state = (observer_state *)context;
    size_t copy_size = 0u;
    if (access == NULL || state == NULL) {
        return;
    }
    ++state->count;
    state->kind = access->kind;
    state->address = access->address;
    state->size = access->size;
    memset(state->bytes, 0, sizeof(state->bytes));
    copy_size = access->size < sizeof(state->bytes)
        ? access->size
        : sizeof(state->bytes);
    memcpy(state->bytes, access->data, copy_size);
}

int main(void)
{
    vf2_model2a machine;
    observer_state observed;
    uint32_t value = 0u;
    uint32_t enable = 0u;
    uint8_t byte = UINT8_C(0x5a);
    size_t before = 0u;

    memset(&machine, 0, sizeof(machine));
    memset(&observed, 0, sizeof(observed));
    EXPECT_TRUE(vf2_model2a_initialize(&machine));
    EXPECT_TRUE(vf2_model2a_set_memory_observer(
        &machine, observe_memory, &observed
    ) == VF2_OK);

    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_WORK_RAM_BASE + UINT32_C(0x20), UINT32_C(0x11223344)
    ) == VF2_OK);
    EXPECT_TRUE(observed.count == 1u);
    EXPECT_TRUE(observed.kind == VF2_MODEL2A_MEMORY_WRITE);
    EXPECT_TRUE(observed.address == VF2_WORK_RAM_BASE + UINT32_C(0x20));
    EXPECT_TRUE(observed.size == sizeof(uint32_t));
    EXPECT_TRUE(observed.bytes[0] == UINT8_C(0x44));
    EXPECT_TRUE(observed.bytes[3] == UINT8_C(0x11));

    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_WORK_RAM_BASE + UINT32_C(0x20), &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x11223344));
    EXPECT_TRUE(observed.count == 2u);
    EXPECT_TRUE(observed.kind == VF2_MODEL2A_MEMORY_READ);

    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_VIDEO_CONTROL_BASE + UINT32_C(4), &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(1));
    EXPECT_TRUE(observed.count == 3u);
    EXPECT_TRUE(observed.address == VF2_VIDEO_CONTROL_BASE + UINT32_C(4));

    EXPECT_TRUE(vf2_model2a_write(
        &machine, VF2_IO_CONTROL_BASE + UINT32_C(0x40), &byte, sizeof(byte)
    ) == VF2_OK);
    EXPECT_TRUE(observed.count == 4u);
    EXPECT_TRUE(observed.kind == VF2_MODEL2A_MEMORY_WRITE);
    EXPECT_TRUE(observed.bytes[0] == byte);

    before = observed.count;
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, UINT32_C(0x13000000), &value
    ) == VF2_ERROR_OUT_OF_BOUNDS);
    EXPECT_TRUE(observed.count == before);

    EXPECT_TRUE(vf2_model2a_set_memory_observer(&machine, NULL, NULL) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_WORK_RAM_BASE + UINT32_C(0x24), UINT32_C(0xaabbccdd)
    ) == VF2_OK);
    EXPECT_TRUE(observed.count == before);

    /* Explicit board reset uses the MAME Model 2 reset seed without changing
     * initialize()'s zeroed test/oracle construction contract. */
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_BUFFER_RAM_BASE, UINT32_C(0xaabbccdd)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_reset(&machine) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_BUFFER_RAM_BASE, &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x07800f0f));
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_BUFFER_RAM_BASE + UINT32_C(0x1fffc), &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0x07800f0f));
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_BUFFER_RAM_BASE + UINT32_C(0x20000), &value
    ) == VF2_OK);
    EXPECT_TRUE(value == 0u);
    EXPECT_TRUE(vf2_model2a_get_frame_number(&machine, &value) == VF2_OK);
    EXPECT_TRUE(value == 0u);

    /* Model 2 timers are one-shot 25 MHz down-counters, not ordinary RAM. */
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_TIMER_BASE, &value
    ) == VF2_OK);
    EXPECT_TRUE(value == VF2_MODEL2A_TIMER_RELOAD);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_TIMER_BASE, UINT32_C(10)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_set_interrupt_enable(
        &machine, UINT32_C(1) << 2u
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_advance_cycles(&machine, 4u) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_TIMER_BASE, &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(6));
    EXPECT_TRUE(vf2_model2a_advance_cycles(&machine, 6u) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(6));
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_TIMER_BASE, &value
    ) == VF2_OK);
    EXPECT_TRUE(value == VF2_MODEL2A_TIMER_RELOAD);
    EXPECT_TRUE(vf2_model2a_get_interrupt_state(
        &machine, &value, &enable
    ) == VF2_OK);
    EXPECT_TRUE((value & (UINT32_C(1) << 2u)) != 0u);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_INTERRUPT_CONTROL_BASE, UINT32_C(1) << 2u
    ) == VF2_OK);

    /* Video status follows the MAME Model 2A frame-phase convention. */
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_VIDEO_CONTROL_BASE + VF2_MODEL2A_VIDEO_STATUS_OFFSET,
        UINT32_C(3)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_advance_frame(&machine) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_advance_frame(&machine) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_COPRO_CONTROL_BASE, UINT32_C(4)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_advance_frame(&machine) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_VIDEO_CONTROL_BASE + VF2_MODEL2A_VIDEO_STATUS_OFFSET,
        &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(7));

    /* Upload mode accepts and accounts for TGP program words while the
     * measured guest read boundary remains the hardware's all-ones value. */
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_VIDEO_CONTROL_BASE +
            VF2_MODEL2A_VIDEO_GEOMETRY_CONTROL_OFFSET,
        UINT32_C(0x80000000)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_write_u32(
        &machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
        UINT32_C(0x12345678)
    ) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read_u32(
        &machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000), &value
    ) == VF2_OK);
    EXPECT_TRUE(value == UINT32_C(0xffffffff));

    vf2_model2a_shutdown(&machine);
    return failures == 0 ? 0 : 1;
}
