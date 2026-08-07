#include "vf2/native_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hash.h"

static const uint8_t runtime_state_magic[8] = {
    'V', 'F', '2', 'N', 'R', 'T', 'S', 0
};

enum {
    VF2_NATIVE_RUNTIME_STATE_FIELD_COUNT = 12u,
    VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE = 16u,
    VF2_NATIVE_RUNTIME_STATE_PAYLOAD_SIZE =
        VF2_NATIVE_RUNTIME_STATE_FIELD_COUNT * 8u,
    VF2_NATIVE_RUNTIME_STATE_FILE_SIZE =
        VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE +
        VF2_NATIVE_RUNTIME_STATE_PAYLOAD_SIZE + 4u
};

static void encode_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8u);
    destination[2] = (uint8_t)(value >> 16u);
    destination[3] = (uint8_t)(value >> 24u);
}

static void encode_u64(uint8_t *destination, uint64_t value)
{
    size_t index = 0u;

    for (index = 0u; index < 8u; ++index) {
        destination[index] = (uint8_t)(value >> (index * 8u));
    }
}

static uint32_t decode_u32(const uint8_t *source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8u) |
        ((uint32_t)source[2] << 16u) |
        ((uint32_t)source[3] << 24u);
}

static uint64_t decode_u64(const uint8_t *source)
{
    uint64_t value = 0u;
    size_t index = 0u;

    for (index = 0u; index < 8u; ++index) {
        value |= (uint64_t)source[index] << (index * 8u);
    }
    return value;
}

static int decode_size(const uint8_t *source, size_t *value)
{
    const uint64_t decoded = decode_u64(source);

    if (decoded > (uint64_t)SIZE_MAX) {
        return 0;
    }
    *value = (size_t)decoded;
    return 1;
}

vf2_status vf2_native_runtime_state_write_file(
    const vf2_native_runtime_state *state,
    const char *path
)
{
    uint8_t bytes[VF2_NATIVE_RUNTIME_STATE_FILE_SIZE];
    uint64_t fields[VF2_NATIVE_RUNTIME_STATE_FIELD_COUNT];
    uint32_t checksum = 0u;
    FILE *file = NULL;
    size_t index = 0u;

    if (state == NULL || path == NULL || path[0] == '\0' ||
        state->frame_wait.visits_before_interrupt == 0u ||
        state->frame_wait.visits >=
            state->frame_wait.visits_before_interrupt) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    fields[0] = (uint64_t)state->frame_wait.visits;
    fields[1] = (uint64_t)state->frame_wait.visits_before_interrupt;
    fields[2] = (uint64_t)state->frame_wait.interrupts_injected;
    fields[3] = (uint64_t)state->blocks_executed;
    fields[4] = (uint64_t)state->task_bodies_executed;
    fields[5] = (uint64_t)state->frame_wait_phases;
    fields[6] = (uint64_t)state->scheduler_entries;
    fields[7] = (uint64_t)state->scheduler_transitions;
    fields[8] = (uint64_t)state->scheduler_finishes;
    fields[9] = state->recovered_instruction_count;
    fields[10] = state->recovered_procedure_calls;
    fields[11] = state->recovered_procedure_returns;

    memset(bytes, 0, sizeof(bytes));
    memcpy(bytes, runtime_state_magic, sizeof(runtime_state_magic));
    encode_u32(bytes + 8u, VF2_NATIVE_RUNTIME_STATE_VERSION);
    encode_u32(bytes + 12u, VF2_NATIVE_RUNTIME_STATE_FIELD_COUNT);
    for (index = 0u;
         index < VF2_NATIVE_RUNTIME_STATE_FIELD_COUNT;
         ++index) {
        encode_u64(bytes + VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE + index * 8u,
                   fields[index]);
    }
    checksum = vf2_crc32(
        bytes,
        VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE +
            VF2_NATIVE_RUNTIME_STATE_PAYLOAD_SIZE
    );
    encode_u32(
        bytes + VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE +
            VF2_NATIVE_RUNTIME_STATE_PAYLOAD_SIZE,
        checksum
    );

    file = fopen(path, "wb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }
    if (fwrite(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) {
        (void)fclose(file);
        return VF2_ERROR_IO;
    }
    if (fclose(file) != 0) {
        return VF2_ERROR_IO;
    }
    return VF2_OK;
}

vf2_status vf2_native_runtime_state_read_file(
    vf2_native_runtime_state *state,
    const char *path
)
{
    uint8_t bytes[VF2_NATIVE_RUNTIME_STATE_FILE_SIZE];
    vf2_native_runtime_state decoded;
    const uint8_t *payload =
        bytes + VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE;
    uint32_t expected_checksum = 0u;
    uint32_t actual_checksum = 0u;
    FILE *file = NULL;
    int trailing = 0;

    if (state == NULL || path == NULL || path[0] == '\0') {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }
    if (fread(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) {
        (void)fclose(file);
        return VF2_ERROR_BAD_SIZE;
    }
    trailing = fgetc(file);
    if (fclose(file) != 0) {
        return VF2_ERROR_IO;
    }
    if (trailing != EOF) {
        return VF2_ERROR_BAD_SIZE;
    }
    if (memcmp(bytes, runtime_state_magic, sizeof(runtime_state_magic)) != 0 ||
        decode_u32(bytes + 8u) != VF2_NATIVE_RUNTIME_STATE_VERSION ||
        decode_u32(bytes + 12u) != VF2_NATIVE_RUNTIME_STATE_FIELD_COUNT) {
        return VF2_ERROR_UNSUPPORTED;
    }

    expected_checksum = decode_u32(
        bytes + VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE +
            VF2_NATIVE_RUNTIME_STATE_PAYLOAD_SIZE
    );
    actual_checksum = vf2_crc32(
        bytes,
        VF2_NATIVE_RUNTIME_STATE_HEADER_SIZE +
            VF2_NATIVE_RUNTIME_STATE_PAYLOAD_SIZE
    );
    if (expected_checksum != actual_checksum) {
        return VF2_ERROR_BAD_CRC32;
    }

    memset(&decoded, 0, sizeof(decoded));
    if (!decode_size(payload + 0u * 8u, &decoded.frame_wait.visits) ||
        !decode_size(payload + 1u * 8u,
                     &decoded.frame_wait.visits_before_interrupt) ||
        !decode_size(payload + 2u * 8u,
                     &decoded.frame_wait.interrupts_injected) ||
        !decode_size(payload + 3u * 8u, &decoded.blocks_executed) ||
        !decode_size(payload + 4u * 8u, &decoded.task_bodies_executed) ||
        !decode_size(payload + 5u * 8u, &decoded.frame_wait_phases) ||
        !decode_size(payload + 6u * 8u, &decoded.scheduler_entries) ||
        !decode_size(payload + 7u * 8u, &decoded.scheduler_transitions) ||
        !decode_size(payload + 8u * 8u, &decoded.scheduler_finishes)) {
        return VF2_ERROR_BAD_SIZE;
    }

    decoded.recovered_instruction_count = decode_u64(payload + 9u * 8u);
    decoded.recovered_procedure_calls = decode_u64(payload + 10u * 8u);
    decoded.recovered_procedure_returns = decode_u64(payload + 11u * 8u);
    if (decoded.frame_wait.visits_before_interrupt == 0u ||
        decoded.frame_wait.visits >=
            decoded.frame_wait.visits_before_interrupt) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *state = decoded;
    return VF2_OK;
}
