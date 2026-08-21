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

    vf2_model2a_shutdown(&machine);
    return failures == 0 ? 0 : 1;
}
