#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

int vf2_test_i960_executor(void)
{
    uint8_t image[128];
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status = VF2_OK;

    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x8c703000u);
    write_le32(image + 4u, VF2_WORK_RAM_BASE);
    write_le32(image + 8u, 0x5c781e00u);
    write_le32(image + 12u, 0x8c683000u);
    write_le32(image + 16u, 3u);
    write_le32(image + 20u, 0x927b9000u);
    write_le32(image + 24u, 0x8c73a004u);
    write_le32(image + 28u, 0x5a6b4b01u);
    write_le32(image + 32u, 0x14fffff4u);

    if (!vf2_model2a_initialize(&machine)) {
        return 1;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 2;
    }
    memset(machine.work_ram, 0xa5, machine.work_ram_size);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    memset(&options, 0, sizeof(options));
    options.stop_address = 36u;
    options.max_steps = 64u;
    options.stop_on_self_branch = true;
    status = vf2_i960_run(&cpu, &machine, &options, &result);
    if (status != VF2_OK || result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu.ip != 36u || cpu.registers[13] != 0u ||
        cpu.registers[14] != VF2_WORK_RAM_BASE + 12u ||
        machine.work_ram[0] != 0u || machine.work_ram[4] != 0u ||
        machine.work_ram[8] != 0u || machine.work_ram[12] != 0xa5u) {
        vf2_model2a_shutdown(&machine);
        return 3;
    }
    vf2_model2a_shutdown(&machine);

    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x587bce1fu);
    write_le32(image + 4u, 0x70418085u);
    write_le32(image + 8u, 0x60016004u);
    write_le32(image + 12u, 0x65290284u);
    write_le32(image + 16u, 0x64294284u);

    if (!vf2_model2a_initialize(&machine)) {
        return 4;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 5;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[15] = UINT32_MAX;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[15] != 0x7fffffffu) {
        vf2_model2a_shutdown(&machine);
        return 6;
    }

    cpu.ip = 4u;
    cpu.registers[5] = 3u;
    cpu.registers[6] = 4u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[8] != 12u) {
        vf2_model2a_shutdown(&machine);
        return 7;
    }

    cpu.ip = 8u;
    cpu.registers[4] = 0xff000004u;
    cpu.registers[5] = VF2_WORK_RAM_BASE;
    if (vf2_model2a_write_u32(&machine, VF2_WORK_RAM_BASE, 0xaabbccddu) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 8;
    }
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.interrupt_control != 0xaabbccddu) {
        vf2_model2a_shutdown(&machine);
        return 9;
    }

    cpu.ip = 12u;
    cpu.process_control = 0x55000000u;
    cpu.registers[4] = 0x00ff0000u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.process_control != 0x55ff0000u ||
        cpu.registers[5] != 0x55000000u) {
        vf2_model2a_shutdown(&machine);
        return 10;
    }

    cpu.ip = 16u;
    cpu.arithmetic_control = 0x00aa0000u;
    cpu.registers[4] = 0xff00ffffu;
    cpu.registers[5] = 0x12003456u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.arithmetic_control != 0x12aa3456u ||
        cpu.registers[5] != 0x00aa0000u) {
        vf2_model2a_shutdown(&machine);
        return 11;
    }

    vf2_model2a_shutdown(&machine);

    /* Verify architectural call/ret register frames with two nested calls. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0x00u, 0x09000020u); /* call 0x20 */
    write_le32(image + 0x20u, 0x09000020u); /* call 0x40 */
    write_le32(image + 0x24u, 0x0a000000u); /* ret */
    write_le32(image + 0x40u, 0x0a000000u); /* ret */

    if (!vf2_model2a_initialize(&machine)) {
        return 12;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 13;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[1] = 0x00501003u;
    cpu.registers[3] = 0x12345678u;
    cpu.registers[VF2_I960_FP_REGISTER] = 0x00500000u;
    memset(&options, 0, sizeof(options));
    options.stop_address = 4u;
    options.max_steps = 16u;
    options.stop_on_self_branch = true;
    status = vf2_i960_run(&cpu, &machine, &options, &result);
    if (status != VF2_OK || result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu.ip != 4u || cpu.local_frame_depth != 0u ||
        cpu.maximum_local_frame_depth != 2u || cpu.procedure_calls != 2u ||
        cpu.procedure_returns != 2u || cpu.registers[1] != 0x00501003u ||
        cpu.registers[3] != 0x12345678u ||
        cpu.registers[VF2_I960_FP_REGISTER] != 0x00500000u) {
        vf2_model2a_shutdown(&machine);
        return 14;
    }

    vf2_model2a_shutdown(&machine);

    /* SHRDI performs signed division by a power of two, rounding toward zero. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x59210d01u); /* shrdi 1, r4, r4 */
    if (!vf2_model2a_initialize(&machine)) {
        return 15;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 16;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[4] = (uint32_t)-3;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || (int32_t)cpu.registers[4] != -1) {
        vf2_model2a_shutdown(&machine);
        return 17;
    }
    cpu.ip = 0u;
    cpu.registers[4] = 7u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[4] != 3u) {
        vf2_model2a_shutdown(&machine);
        return 18;
    }
    vf2_model2a_shutdown(&machine);

    /* EDIV returns the remainder in the destination and quotient in the next register. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x6721088au); /* ediv 10, r4, r4 */
    if (!vf2_model2a_initialize(&machine)) {
        return 19;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 20;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[4] = UINT32_C(12345);
    cpu.registers[5] = 0u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[4] != UINT32_C(5) ||
        cpu.registers[5] != UINT32_C(1234)) {
        vf2_model2a_shutdown(&machine);
        return 21;
    }
    vf2_model2a_shutdown(&machine);

    /* BALX stores the return address in its encoded local/global register. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x85703000u); /* balx 0x20, r14 */
    write_le32(image + 4u, 0x00000020u);
    write_le32(image + 0x20u, 0x84039000u); /* bx (r14) */
    if (!vf2_model2a_initialize(&machine)) {
        return 19;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 20;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    memset(&options, 0, sizeof(options));
    options.stop_address = 8u;
    options.max_steps = 4u;
    options.stop_on_self_branch = true;
    status = vf2_i960_run(&cpu, &machine, &options, &result);
    if (status != VF2_OK || result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu.ip != 8u || cpu.registers[14] != 8u) {
        vf2_model2a_shutdown(&machine);
        return 21;
    }
    vf2_model2a_shutdown(&machine);

    /* BBC/BBS test an individual bit rather than performing a compare. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, 0x3700e00cu); /* bbs 0, r3, 0x0c */
    if (!vf2_model2a_initialize(&machine)) {
        return 22;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 23;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[3] = 1u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.ip != 12u) {
        vf2_model2a_shutdown(&machine);
        return 24;
    }
    cpu.ip = 0u;
    cpu.registers[3] = 0u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.ip != 4u) {
        vf2_model2a_shutdown(&machine);
        return 25;
    }
    vf2_model2a_shutdown(&machine);

    /* Recovered C can complete an active procedure frame without decoding RET. */
    memset(image, 0xff, sizeof(image));
    if (!vf2_model2a_initialize(&machine)) {
        return 26;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 27;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[1] = UINT32_C(0x00501003);
    cpu.registers[3] = UINT32_C(0x12345678);
    cpu.registers[16] = UINT32_C(0x11111111);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x00500000);
    status = vf2_i960_cpu_enter_procedure(
        &cpu, UINT32_C(0x20), UINT32_C(0x04)
    );
    if (status != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 28;
    }
    cpu.registers[3] = UINT32_C(0xaaaaaaaa);
    cpu.registers[16] = UINT32_C(0x22222222);
    status = vf2_i960_cpu_return_procedure(&cpu, &machine);
    if (status != VF2_OK || cpu.ip != UINT32_C(0x04) ||
        cpu.local_frame_depth != 0u || cpu.procedure_calls != 1u ||
        cpu.procedure_returns != 1u ||
        cpu.registers[3] != UINT32_C(0x12345678) ||
        cpu.registers[16] != UINT32_C(0x22222222) ||
        cpu.registers[VF2_I960_FP_REGISTER] != UINT32_C(0x00500000)) {
        vf2_model2a_shutdown(&machine);
        return 29;
    }
    vf2_model2a_shutdown(&machine);

    /* Architectural reset obtains the initial stack from PRCB + 24. */
    memset(image, 0, sizeof(image));
    write_le32(image + 0x38u, UINT32_C(0x005ff500));
    if (!vf2_model2a_initialize(&machine)) {
        return 30;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 31;
    }
    status = vf2_i960_cpu_reset_from_machine(
        &cpu, &machine, 0u, UINT32_C(0x20), UINT32_C(0x40)
    );
    if (status != VF2_OK || cpu.ip != UINT32_C(0x40) ||
        cpu.registers[VF2_I960_FP_REGISTER] != UINT32_C(0x005ff500) ||
        cpu.registers[1] != UINT32_C(0x005ff540)) {
        vf2_model2a_shutdown(&machine);
        return 32;
    }

    /* Model 2A texture banks are visible through their hardware mirrors. */
    status = vf2_model2a_write_u32(
        &machine, UINT32_C(0x12600600), UINT32_C(0x89abcdef)
    );
    if (status == VF2_OK) {
        uint32_t texture_value = 0u;
        status = vf2_model2a_read_u32(
            &machine, UINT32_C(0x12400600), &texture_value
        );
        if (status != VF2_OK || texture_value != UINT32_C(0x89abcdef)) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 33;
    }
    vf2_model2a_shutdown(&machine);

    /* Conditional compare preserves equality and otherwise replaces the
     * condition code with the second signed/ordinal comparison. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, UINT32_C(0x5a03e083)); /* cmpi r3, r15 */
    write_le32(image + 4u, UINT32_C(0x5a012183)); /* concmpi r3, r4 */
    if (!vf2_model2a_initialize(&machine)) {
        return 34;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 35;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[3] = 3u;
    cpu.registers[15] = 1u;
    cpu.registers[4] = 5u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.compare_result != VF2_I960_COMPARE_GREATER) {
        vf2_model2a_shutdown(&machine);
        return 36;
    }
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.compare_result != VF2_I960_COMPARE_LESS) {
        vf2_model2a_shutdown(&machine);
        return 37;
    }
    cpu.ip = 0u;
    cpu.registers[15] = 3u;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.compare_result != VF2_I960_COMPARE_EQUAL) {
        vf2_model2a_shutdown(&machine);
        return 38;
    }
    cpu.registers[4] = UINT32_MAX;
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.compare_result != VF2_I960_COMPARE_EQUAL) {
        vf2_model2a_shutdown(&machine);
        return 39;
    }
    vf2_model2a_shutdown(&machine);

    /* cvtri converts a single-precision real to an integer using AC[31:30]. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, UINT32_C(0x6c68100d)); /* cvtri r13, r13 */
    if (!vf2_model2a_initialize(&machine)) {
        return 40;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 41;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[13] = UINT32_C(0x3fc00000); /* 1.5 */
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[13] != 2u) {
        vf2_model2a_shutdown(&machine);
        return 42;
    }
    cpu.ip = 0u;
    cpu.arithmetic_control = UINT32_C(0xc0000000);
    cpu.registers[13] = UINT32_C(0x3fc00000);
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[13] != 1u) {
        vf2_model2a_shutdown(&machine);
        return 43;
    }
    vf2_model2a_shutdown(&machine);

    /* cvtir converts a signed integer register to a single-precision real. */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, UINT32_C(0x67b01216)); /* cvtir g6, g6 */
    if (!vf2_model2a_initialize(&machine)) {
        return 44;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 45;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[22] = UINT32_C(0xfffffff6); /* -10 */
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.registers[22] != UINT32_C(0xc1200000)) {
        vf2_model2a_shutdown(&machine);
        return 46;
    }
    vf2_model2a_shutdown(&machine);

    /* movt with a literal source zeroes the two following registers
     * (fa_coli 0x236b8: movt 0, r8). */
    memset(image, 0xff, sizeof(image));
    write_le32(image + 0u, UINT32_C(0x5e401e00)); /* movt 0, r8 */
    if (!vf2_model2a_initialize(&machine)) {
        return 47;
    }
    if (vf2_model2a_attach_main_rom(&machine, image, sizeof(image)) != VF2_OK) {
        vf2_model2a_shutdown(&machine);
        return 48;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, 0u);
    cpu.registers[0] = UINT32_C(0x11111111);
    cpu.registers[1] = UINT32_C(0x22222222);
    cpu.registers[2] = UINT32_C(0x33333333);
    cpu.registers[8] = UINT32_C(0xdeadbeef);
    cpu.registers[9] = UINT32_C(0xdeadbeef);
    cpu.registers[10] = UINT32_C(0xdeadbeef);
    status = vf2_i960_step(&cpu, &machine, NULL);
    if (status != VF2_OK || cpu.ip != 4u || cpu.registers[8] != 0u ||
        cpu.registers[9] != 0u || cpu.registers[10] != 0u) {
        vf2_model2a_shutdown(&machine);
        return 49;
    }
    vf2_model2a_shutdown(&machine);
    return 0;
}
