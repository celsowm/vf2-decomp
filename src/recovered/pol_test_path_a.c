#include "vf2/recovered.h"

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#include <stdint.h>

/*
 * Measured recovery of fa_pol_test 0x00021a00 path A plus palette helper
 * 0x00007f24. Evidence:
 *   decomp/i960/notes/packet_format_p1_v0377.md
 *   decomp/i960/notes/pol_test_path_a_v0378.md
 *   out/attr-p1/pol_test_task_pathA_986.jsonl (gold FIFO + geo writes)
 *
 * Gate (oracle): cmpoble 2, r14, path_A  ->  path A when mode >= 2.
 * Probe task_pathA_985 (mode=3) stops at 0x21b00 with 161 instructions
 * (one less than mode=2 because the 0x986 arm's `b` is skipped), so mode=3
 * is path A. Path B (mode < 2) remains VF2_ERROR_UNSUPPORTED.
 */

enum {
    VF2_POL_TEST_ENTRY = 0x00021a00u,
    VF2_POL_TEST_PATH_A_RET = 0x00021b00u,
    VF2_POL_TEST_PATH_A_CONT = 0x00021a28u,
    VF2_POL_TEST_PALETTE_ENTRY = 0x00007f24u,
    VF2_POL_TEST_SUBMIT_ENTRY = 0x00007c60u,
    VF2_POL_TEST_MODE_BYTE = 0x00530150u,
    VF2_POL_TEST_COUNT_BYTE = 0x0053014cu,
    VF2_POL_TEST_PALETTE_SRC = 0x00501400u,
    VF2_POL_TEST_PALETTE_BIAS = 0x005013f0u,
    VF2_POL_TEST_PALETTE_SUB = 0x0000017fu,
    VF2_POL_TEST_PALETTE_WORDS = 6u,
    VF2_POL_TEST_ID_LOOP = 0x00000986u,
    VF2_POL_TEST_ID_ALT = 0x00000985u,
    VF2_POL_TEST_MODE_PATH_A_MIN = 2u,
    VF2_POL_TEST_MODE_LOOP_ALT = 3u,
    VF2_POL_TEST_G0 = 16u,
    VF2_POL_TEST_G1 = 17u,
    VF2_POL_TEST_G10 = 26u,
    VF2_POL_TEST_G11 = 27u,
    VF2_POL_TEST_G12 = 28u,
    VF2_POL_TEST_R15 = 15u,
    VF2_POL_TEST_TASK_FRAME_SLOT = 0x0cu,
    VF2_POL_TEST_PALETTE_STORE_SLOT = 0x30u
};

/* Path-A FIFO immediates (disasm + oracle). Helper 0x7c60 contributes
 * 0x1a003434 plus the geo pointer word on each submit. */
static const uint32_t VF2_POL_TEST_FIFO_PRELUDE[] = {
    0x00800101u,
    0x01800303u,
    0x03000606u,
    0x3e9eb852u,
    0x3e428f5cu,
    0x3f9eb852u
};

static const uint32_t VF2_POL_TEST_FIFO_INTERSTITIAL[] = {
    0x03000606u,
    0x00000000u,
    0xbec7ae14u,
    0x00000000u
};

static const uint32_t VF2_POL_TEST_FIFO_CLOSE = 0x01000202u;

static vf2_status pol_test_read_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return vf2_model2a_read(machine, address, value, 1u);
}

static vf2_status pol_test_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2] = {0u, 0u};
    vf2_status status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));

    if (status != VF2_OK) {
        return status;
    }
    *value = (uint16_t)((uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u));
    return VF2_OK;
}

static vf2_status pol_test_fifo_write(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t word
)
{
    const uint32_t address =
        cpu->registers[VF2_POL_TEST_G11] + cpu->registers[VF2_POL_TEST_G12];

    cpu->registers[VF2_POL_TEST_R15] = word;
    return vf2_model2a_write_u32(machine, address, word);
}

static vf2_status pol_test_call_submit(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t object_id,
    uint32_t *helper_instructions
)
{
    const uint64_t before = cpu->executed_instructions;
    vf2_status status = VF2_OK;

    /* mov 0, g1 ; lda id, g0 ; call 0x7c60 */
    cpu->registers[VF2_POL_TEST_G1] = 0u;
    cpu->registers[VF2_POL_TEST_G0] = object_id;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_POL_TEST_SUBMIT_ENTRY, VF2_POL_TEST_PATH_A_RET
    );
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_recovered_polygon_object_submit(machine, cpu);
    if (status != VF2_OK) {
        return status;
    }
    *helper_instructions = (uint32_t)(cpu->executed_instructions - before);
    return VF2_OK;
}

vf2_status vf2_recovered_polygon_palette_pack_7f24(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint32_t geo_base = 0u;
    uint32_t index = 0u;
    uint32_t bias = 0u;
    uint32_t source = 0u;
    uint32_t remaining = 0u;
    uint64_t executed = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_POL_TEST_PALETTE_ENTRY || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    geo_base = cpu->registers[VF2_POL_TEST_G10];
    index = cpu->registers[VF2_POL_TEST_G12];
    source = cpu->registers[VF2_POL_TEST_G0];

    /* st g12, 0x30(g10) */
    status = vf2_model2a_write_u32(
        machine,
        geo_base + VF2_POL_TEST_PALETTE_STORE_SLOT,
        index
    );
    if (status != VF2_OK) {
        return status;
    }
    executed += UINT64_C(1);

    /* ld 0x005013f0, r7 ; lda 0x17f, r6 ; mov 6, r8 */
    status = vf2_model2a_read_u32(machine, VF2_POL_TEST_PALETTE_BIAS, &bias);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[7] = bias;
    cpu->registers[6] = VF2_POL_TEST_PALETTE_SUB;
    remaining = VF2_POL_TEST_PALETTE_WORDS;
    cpu->registers[8] = remaining;
    executed += UINT64_C(3);

    while (remaining != 0u) {
        uint16_t half_lo = 0u;
        uint16_t half_hi = 0u;
        int32_t lo = 0;
        int32_t hi = 0;
        uint32_t r3 = 0u;
        uint32_t r4 = 0u;

        status = pol_test_read_u16(machine, source, &half_lo);
        if (status != VF2_OK) {
            return status;
        }
        status = pol_test_read_u16(machine, source + UINT32_C(2), &half_hi);
        if (status != VF2_OK) {
            return status;
        }

        /* ldis (g0), r3 ; subi r3, r6, r3  ->  r3 = 0x17f - sext(lo) */
        lo = (int32_t)(int16_t)half_lo;
        hi = (int32_t)(int16_t)half_hi;
        r3 = (uint32_t)(VF2_POL_TEST_PALETTE_SUB - lo);
        /* ldis 2(g0), r4 ; shli 16 ; addi r3 ; addi r7 */
        r4 = ((uint32_t)hi << 16u) + r3 + bias;

        cpu->registers[3] = r3;
        cpu->registers[4] = r4;

        /* st r4, (g10)[g12] — measured: all six words hit the same port. */
        status = vf2_model2a_write_u32(machine, geo_base + index, r4);
        if (status != VF2_OK) {
            return status;
        }

        source += UINT32_C(4);
        --remaining;
        cpu->registers[VF2_POL_TEST_G0] = source;
        cpu->registers[8] = remaining;

        /* cmpdeco 1, r8, r8 ; bl — last compare of the helper body. */
        if (remaining != 0u) {
            cpu->compare_result = VF2_I960_COMPARE_GREATER;
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        } else {
            cpu->compare_result = VF2_I960_COMPARE_LESS;
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        }
        executed += UINT64_C(10);
    }

    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += executed + UINT64_C(1);
    return VF2_OK;
}

vf2_status vf2_recovered_pol_test_path_a(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
)
{
    uint8_t mode = 0u;
    uint8_t count = 0u;
    uint32_t task_instructions = 0u;
    size_t word = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->ip != VF2_POL_TEST_ENTRY || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* ldob 0x00530150, r14 ; cmpoble 2, r14, path A */
    status = pol_test_read_u8(machine, VF2_POL_TEST_MODE_BYTE, &mode);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[14] = mode;
    task_instructions += UINT32_C(2);
    if (mode < VF2_POL_TEST_MODE_PATH_A_MIN) {
        /* Path B is measured ROM but not recovered. Fail closed. */
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Path A continuation store into the task frame. */
    cpu->registers[VF2_POL_TEST_R15] = VF2_POL_TEST_PATH_A_CONT;
    status = vf2_model2a_write_u32(
        machine,
        cpu->registers[29] + VF2_POL_TEST_TASK_FRAME_SLOT,
        VF2_POL_TEST_PATH_A_CONT
    );
    if (status != VF2_OK) {
        return status;
    }
    task_instructions += UINT32_C(2);

    /* lda 0x00501400, g0 ; call 0x7f24 */
    cpu->registers[VF2_POL_TEST_G0] = VF2_POL_TEST_PALETTE_SRC;
    task_instructions += UINT32_C(2);
    {
        uint32_t helper_instructions = 0u;
        const uint64_t before = cpu->executed_instructions;

        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_POL_TEST_PALETTE_ENTRY, VF2_POL_TEST_PATH_A_RET
        );
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_recovered_polygon_palette_pack_7f24(machine, cpu);
        if (status != VF2_OK) {
            return status;
        }
        helper_instructions =
            (uint32_t)(cpu->executed_instructions - before);
        (void)helper_instructions;
    }

    /* FIFO prelude immediates via st (g11)[g12]. */
    for (word = 0u; word < sizeof(VF2_POL_TEST_FIFO_PRELUDE) /
                               sizeof(VF2_POL_TEST_FIFO_PRELUDE[0]);
         ++word) {
        status = pol_test_fifo_write(
            machine, cpu, VF2_POL_TEST_FIFO_PRELUDE[word]
        );
        if (status != VF2_OK) {
            return status;
        }
        task_instructions += UINT32_C(2);
    }

    /* ldob 0x0053014c, r3 ; cmpobe 0, r3, interstitial */
    status = pol_test_read_u8(machine, VF2_POL_TEST_COUNT_BYTE, &count);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[3] = count;
    task_instructions += UINT32_C(2);

    while (count != 0u) {
        uint32_t object_id = VF2_POL_TEST_ID_LOOP;
        uint32_t helper_instructions = 0u;

        /* ldob 0x00530150, r14 ; cmpobe 3, r14, 0x985 arm */
        status = pol_test_read_u8(machine, VF2_POL_TEST_MODE_BYTE, &mode);
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[14] = mode;
        task_instructions += UINT32_C(2);

        if (mode == VF2_POL_TEST_MODE_LOOP_ALT) {
            object_id = VF2_POL_TEST_ID_ALT;
        } else {
            object_id = VF2_POL_TEST_ID_LOOP;
            task_instructions += UINT32_C(1); /* b past the 0x985 arm */
        }

        task_instructions += UINT32_C(3); /* mov/lda/call */
        status = pol_test_call_submit(
            machine, cpu, object_id, &helper_instructions
        );
        if (status != VF2_OK) {
            return status;
        }

        /* cmpdeco 1, r3, r3 ; bl */
        --count;
        cpu->registers[3] = count;
        task_instructions += UINT32_C(2);
    }

    for (word = 0u; word < sizeof(VF2_POL_TEST_FIFO_INTERSTITIAL) /
                               sizeof(VF2_POL_TEST_FIFO_INTERSTITIAL[0]);
         ++word) {
        status = pol_test_fifo_write(
            machine, cpu, VF2_POL_TEST_FIFO_INTERSTITIAL[word]
        );
        if (status != VF2_OK) {
            return status;
        }
        task_instructions += UINT32_C(2);
    }

    /* Final submit is always id 0x985. */
    {
        uint32_t helper_instructions = 0u;

        task_instructions += UINT32_C(3);
        status = pol_test_call_submit(
            machine, cpu, VF2_POL_TEST_ID_ALT, &helper_instructions
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    status = pol_test_fifo_write(machine, cpu, VF2_POL_TEST_FIFO_CLOSE);
    if (status != VF2_OK) {
        return status;
    }
    task_instructions += UINT32_C(2);

    cpu->executed_instructions += task_instructions;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += UINT64_C(1);
    return VF2_OK;
}
