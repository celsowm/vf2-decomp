#ifndef VF2_RECOVERY_INTERNAL_H
#define VF2_RECOVERY_INTERNAL_H

#include "vf2/model2a.h"

#include <stdint.h>

static inline vf2_status vf2_recovered_table_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t stride,
    uint32_t count,
    uint16_t *result
)
{
    uint32_t index = 0u;
    uint16_t crc = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || result == NULL || count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < count; ++index) {
        uint8_t raw = 0u;
        uint8_t table_bytes[2] = {0u, 0u};
        uint16_t table_value = 0u;
        const uint16_t high = (uint16_t)((uint32_t)crc << 8u);

        crc = (uint16_t)(crc >> 8u);
        status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
        if (status != VF2_OK) {
            return status;
        }
        source += UINT32_C(1) + stride;
        crc ^= (uint16_t)raw;
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x02000000) +
                (uint32_t)(crc & UINT16_C(0x00ff)) * UINT32_C(2),
            table_bytes,
            sizeof(table_bytes)
        );
        if (status != VF2_OK) {
            return status;
        }
        table_value = (uint16_t)((uint16_t)table_bytes[0] |
                                 ((uint16_t)table_bytes[1] << 8u));
        crc = (uint16_t)(table_value ^ high);
    }
    *result = crc;
    return VF2_OK;
}

#endif
