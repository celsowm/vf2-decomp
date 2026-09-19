#include "vf2/recovered.h"

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#include <stdint.h>

enum {
    VF2_POLY_OBJECT_ENTRY = 0x00007c60u,
    VF2_POLY_OBJECT_TABLE_BASE = 0x020e0004u,
    VF2_POLY_OBJECT_TABLE_STRIDE = 16u,
    VF2_POLY_OBJECT_GATE_LOW = 0x00501018u,
    VF2_POLY_OBJECT_GATE_HIGH = 0x0050101cu,
    VF2_POLY_OBJECT_COUNTER = 0x00501010u,
    VF2_POLY_OBJECT_ACCUM = 0x005010d0u,
    VF2_POLY_OBJECT_FIFO_CMD = 0x1a003434u,
    VF2_POLY_OBJECT_GEO_SLOT = 0x10u,
    VF2_POLY_OBJECT_CLEAR_SLOT = 0xb0u,
    VF2_POLY_OBJECT_PTR_SLOT = 0x2008u,
    VF2_POLY_OBJECT_WRITE_START_SLOT = 0x1008u,
    VF2_POLY_OBJECT_W3_HIGH_SHIFT = 16u,
    VF2_POLY_OBJECT_W3_LOW_MASK = 0x0000ffffu,
    VF2_POLY_OBJECT_G0 = 16u,
    VF2_POLY_OBJECT_G1 = 17u,
    VF2_POLY_OBJECT_G10 = 26u,
    VF2_POLY_OBJECT_G11 = 27u,
    VF2_POLY_OBJECT_G12 = 28u
};

static void poly_object_set_compare_gate(
    vf2_i960_cpu *cpu,
    uint32_t high,
    uint32_t low
)
{
    vf2_i960_compare_result result = VF2_I960_COMPARE_EQUAL;
    uint32_t bits = UINT32_C(2);

    if (high > low) {
        result = VF2_I960_COMPARE_GREATER;
        bits = UINT32_C(1);
    } else if (high < low) {
        result = VF2_I960_COMPARE_LESS;
        bits = UINT32_C(4);
    }
    cpu->compare_result = result;
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | bits;
}

vf2_status vf2_recovered_polygon_object_submit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t gate_low = 0u;
    uint32_t gate_high = 0u;
    uint32_t counter = 0u;
    uint32_t accum = 0u;
    uint32_t pointer_word = 0u;
    uint32_t fifo_echo = 0u;
    uint32_t record[4] = {0u, 0u, 0u, 0u};
    uint32_t table_address = 0u;
    uint32_t geo_base = 0u;
    uint32_t fifo_base = 0u;
    uint32_t index = 0u;
    uint32_t w3_high = 0u;
    uint32_t w3_low = 0u;
    uint32_t r11 = 0u;
    uint32_t r9 = 0u;
    uint32_t r14 = 0u;
    uint64_t executed = 0u;
    size_t word = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_POLY_OBJECT_ENTRY || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* ld gate_low / ld gate_high / cmpobg */
    status = vf2_model2a_read_u32(machine, VF2_POLY_OBJECT_GATE_LOW, &gate_low);
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_model2a_read_u32(machine, VF2_POLY_OBJECT_GATE_HIGH, &gate_high);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = gate_low;
    cpu->registers[5] = gate_high;
    poly_object_set_compare_gate(cpu, gate_high, gate_low);
    executed += UINT64_C(3);

    if (gate_high > gate_low) {
        /* Measured gate-closed ret: no submit stores. */
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK) {
            return status;
        }
        cpu->executed_instructions += executed + UINT64_C(1);
        return VF2_OK;
    }

    geo_base = cpu->registers[VF2_POLY_OBJECT_G10];
    fifo_base = cpu->registers[VF2_POLY_OBJECT_G11];
    index = cpu->registers[VF2_POLY_OBJECT_G12];

    /* mov 0, r15 ; st r15, 0xb0(g10) */
    cpu->registers[15] = 0u;
    status = vf2_model2a_write_u32(
        machine, geo_base + VF2_POLY_OBJECT_CLEAR_SLOT, 0u
    );
    if (status != VF2_OK) {
        return status;
    }
    executed += UINT64_C(2);

    /* lda 0x1a003434 ; st (g11)[g12] */
    cpu->registers[15] = VF2_POLY_OBJECT_FIFO_CMD;
    status = vf2_model2a_write_u32(
        machine, fifo_base + index, cpu->registers[15]
    );
    if (status != VF2_OK) {
        return status;
    }
    executed += UINT64_C(2);

    /* ld 0x2008(g10) ; lda +0x30 ; st 0x1008(g10) */
    status = vf2_model2a_read_u32(
        machine, geo_base + VF2_POLY_OBJECT_PTR_SLOT, &pointer_word
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = pointer_word;
    r14 = pointer_word + UINT32_C(0x30);
    cpu->registers[14] = r14;
    status = vf2_model2a_write_u32(
        machine, geo_base + VF2_POLY_OBJECT_WRITE_START_SLOT, r14
    );
    if (status != VF2_OK) {
        return status;
    }
    executed += UINT64_C(3);

    /* st r15, (g11)[g12] */
    status = vf2_model2a_write_u32(machine, fifo_base + index, pointer_word);
    if (status != VF2_OK) {
        return status;
    }
    executed += UINT64_C(1);

    /* ld (g11)[g12], r15 */
    status = vf2_model2a_read_u32(machine, fifo_base + index, &fifo_echo);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[15] = fifo_echo;
    executed += UINT64_C(1);

    /* ld/addo/st 0x501010 */
    status = vf2_model2a_read_u32(machine, VF2_POLY_OBJECT_COUNTER, &counter);
    if (status != VF2_OK) {
        return status;
    }
    counter += 1u;
    status = vf2_model2a_write_u32(machine, VF2_POLY_OBJECT_COUNTER, counter);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = counter;
    executed += UINT64_C(3);

    /* lda 0x020e0004[g0*16], g0 ; ldq (g0), r8 */
    table_address = VF2_POLY_OBJECT_TABLE_BASE +
        (cpu->registers[VF2_POLY_OBJECT_G0] * VF2_POLY_OBJECT_TABLE_STRIDE);
    cpu->registers[VF2_POLY_OBJECT_G0] = table_address;
    status = vf2_model2a_read(machine, table_address, record, sizeof(record));
    if (status != VF2_OK) {
        return status;
    }
    for (word = 0u; word < 4u; ++word) {
        cpu->registers[8 + word] = record[word];
    }
    executed += UINT64_C(2);

    /* shro 16, r11, r3 ; lda 0xffff ; and -> r11 low half */
    w3_high = record[3] >> VF2_POLY_OBJECT_W3_HIGH_SHIFT;
    w3_low = record[3] & VF2_POLY_OBJECT_W3_LOW_MASK;
    cpu->registers[3] = w3_high;
    cpu->registers[4] = VF2_POLY_OBJECT_W3_LOW_MASK;
    r11 = w3_low;
    cpu->registers[11] = r11;
    executed += UINT64_C(3);

    /* cmpobe 0, g1, 0x7cf0 — measured: last compare on the open path.
     * Operand order is literal 0 then g1 (COBR first/second). */
    r9 = cpu->registers[9];
    {
        vf2_i960_compare_result branch_compare = VF2_I960_COMPARE_EQUAL;
        uint32_t branch_bits = UINT32_C(2);
        const uint32_t g1_value = cpu->registers[VF2_POLY_OBJECT_G1];

        if (g1_value != 0u) {
            /* 0 < g1 */
            branch_compare = VF2_I960_COMPARE_LESS;
            branch_bits = UINT32_C(4);
        }
        cpu->compare_result = branch_compare;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | branch_bits;
    }
    executed += UINT64_C(1);
    if (cpu->registers[VF2_POLY_OBJECT_G1] != 0u) {
        /* lda (r9)[r3*4], r9 ; ld/addo/st 0x5010d0 */
        r9 = r9 + (w3_high * UINT32_C(4));
        cpu->registers[9] = r9;
        status = vf2_model2a_read_u32(machine, VF2_POLY_OBJECT_ACCUM, &accum);
        if (status != VF2_OK) {
            return status;
        }
        accum += r11;
        status = vf2_model2a_write_u32(machine, VF2_POLY_OBJECT_ACCUM, accum);
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[4] = accum;
        executed += UINT64_C(4);
    }

    /* ld/addo/st 0x50101c += r11_low */
    status = vf2_model2a_read_u32(machine, VF2_POLY_OBJECT_GATE_HIGH, &gate_high);
    if (status != VF2_OK) {
        return status;
    }
    gate_high += r11;
    status = vf2_model2a_write_u32(machine, VF2_POLY_OBJECT_GATE_HIGH, gate_high);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[4] = gate_high;
    executed += UINT64_C(3);

    /* subo 1, 0, r11 */
    r11 = UINT32_C(0xffffffff);
    cpu->registers[11] = r11;
    executed += UINT64_C(1);

    /* st r8, 0x10(g10) */
    status = vf2_model2a_write_u32(
        machine, geo_base + VF2_POLY_OBJECT_GEO_SLOT, cpu->registers[8]
    );
    if (status != VF2_OK) {
        return status;
    }
    executed += UINT64_C(1);

    /* stq r8, (g10)[g12] — architectural single insn, four word stores.
     * r11 is already -1, matching the measured post-ldq overwrite. */
    for (word = 0u; word < 4u; ++word) {
        status = vf2_model2a_write_u32(
            machine,
            geo_base + index + (uint32_t)(word * 4u),
            cpu->registers[8 + word]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    executed += UINT64_C(1);

    /* ret */
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += executed + UINT64_C(1);
    return VF2_OK;
}
