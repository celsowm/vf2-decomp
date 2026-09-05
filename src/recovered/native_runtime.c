#include "vf2/native_runtime.h"
#include "recovery_internal.h"
#include "texture_bridge_internal.h"
#include "vf2/recovered.h"

#include <string.h>

#define VF2_NATIVE_BOOT_STAGE1_ENTRY UINT32_C(0x000000b0)
#define VF2_NATIVE_BOOT_STAGE1_INSTRUCTIONS UINT64_C(1180053)
#define VF2_NATIVE_BOOT_STAGE2_ENTRY UINT32_C(0x000001b0)
#define VF2_NATIVE_POST_BOOT_INIT_ENTRY UINT32_C(0x0000052c)
#define VF2_NATIVE_POST_BOOT_INIT_EXIT UINT32_C(0x0006dd4c)
#define VF2_NATIVE_POST_BOOT_INIT_INSTRUCTIONS UINT64_C(60078)
#define VF2_NATIVE_POST_BOOT_VIDEO_INIT_ENTRY UINT32_C(0x0006dd4c)
#define VF2_NATIVE_POST_BOOT_VIDEO_RAMP_ENTRY UINT32_C(0x000005e8)
#define VF2_NATIVE_POST_BOOT_VIDEO_INIT_ENTRY_INSTRUCTIONS UINT64_C(15)
#define VF2_NATIVE_POST_BOOT_VIDEO_RAMP_INSTRUCTIONS UINT64_C(11563)
#define VF2_NATIVE_POST_BOOT_COLOR_TABLES_ENTRY UINT32_C(0x0006dda4)
#define VF2_NATIVE_POST_BOOT_MEMORY_CLEAR_ENTRY UINT32_C(0x0006dda8)
#define VF2_NATIVE_POST_BOOT_COLOR_TABLES_INSTRUCTIONS UINT64_C(13429)
#define VF2_NATIVE_POST_BOOT_MEMORY_CLEAR_INSTRUCTIONS UINT64_C(41015)
#define VF2_NATIVE_POST_BOOT_REGISTER_STREAM_ENTRY UINT32_C(0x0006ddac)
#define VF2_NATIVE_POST_BOOT_BLOCK_STREAM_ENTRY UINT32_C(0x0006ddb8)
#define VF2_NATIVE_POST_BOOT_BACKUP_SRAM_PROBE_ENTRY UINT32_C(0x0006ddc4)
#define VF2_NATIVE_POST_BOOT_REGISTER_STREAM_INSTRUCTIONS UINT64_C(3291)
#define VF2_NATIVE_POST_BOOT_BLOCK_STREAM_INSTRUCTIONS UINT64_C(648975)
#define VF2_NATIVE_POST_BOOT_BACKUP_SRAM_PROBE_INSTRUCTIONS UINT64_C(61)
#define VF2_NATIVE_POST_BOOT_BACKUP_RESTORE_ENTRY UINT32_C(0x0006ddd8)
#define VF2_NATIVE_POST_BOOT_BACKUP_RESTORE_INSTRUCTIONS UINT64_C(6729)
#define VF2_NATIVE_POST_BOOT_RESTORED_VIDEO_ENTRY UINT32_C(0x000097e4)
#define VF2_NATIVE_POST_BOOT_SECOND_COLOR_TABLES_ENTRY UINT32_C(0x00009890)
#define VF2_NATIVE_POST_BOOT_PALETTE_SEED_ENTRY UINT32_C(0x00009894)
#define VF2_NATIVE_POST_BOOT_SECOND_MEMORY_CLEAR_ENTRY UINT32_C(0x00009898)
#define VF2_NATIVE_POST_BOOT_RESTORED_VIDEO_ENTRY_INSTRUCTIONS UINT64_C(22)
#define VF2_NATIVE_POST_BOOT_PALETTE_SEED_INSTRUCTIONS UINT64_C(6149)
#define VF2_NATIVE_POST_BOOT_TABLE_INIT_ENTRY UINT32_C(0x0000989c)
#define VF2_NATIVE_POST_BOOT_TABLE_INIT_INSTRUCTIONS UINT64_C(682436)
#define VF2_NATIVE_POST_BOOT_HARDWARE_CORE_INIT_ENTRY UINT32_C(0x000098a0)
#define VF2_NATIVE_POST_BOOT_HARDWARE_CORE_INIT_INSTRUCTIONS UINT64_C(19594)
#define VF2_NATIVE_POST_BOOT_TEXTURE_INIT_ENTRY UINT32_C(0x000098b0)
#define VF2_NATIVE_POST_BOOT_TEXTURE_INIT_BODY_ENTRY UINT32_C(0x0004b020)
#define VF2_NATIVE_POST_BOOT_TEXTURE_TIMER_ENTRY UINT32_C(0x0004afb4)
#define VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_ENTRY UINT32_C(0x0004afdc)
#define VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_POLL UINT32_C(0x0004afe4)
#define VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_RETURN UINT32_C(0x00000f9c)
#define VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_CALLER_RETURN UINT32_C(0x0004b01c)
#define VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_EXIT UINT32_C(0x0004b07c)
#define VF2_NATIVE_POST_BOOT_GRAPHICS_VERIFY_EXIT UINT32_C(0x0004b820)
#define VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_SETUP UINT32_C(0x0004b9b8)
#define VF2_NATIVE_POST_BOOT_TEXTURE_INIT_ENTRY_INSTRUCTIONS UINT64_C(15)
#define VF2_NATIVE_POST_BOOT_TEXTURE_TIMER_ENTRY_INSTRUCTIONS UINT64_C(8)
#define VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_ENTRY_INSTRUCTIONS UINT64_C(1)
#define VF2_NATIVE_POST_BOOT_GRAPHICS_VERIFY_INSTRUCTIONS UINT64_C(82)
#define VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_ENTRY_INSTRUCTIONS UINT64_C(4)
#define VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_SETUP_INSTRUCTIONS UINT64_C(24)
#define VF2_NATIVE_POST_BOOT_LUMA_TABLE_ENTRY UINT32_C(0x000098b4)
#define VF2_NATIVE_POST_BOOT_LUMA_TABLE_BODY UINT32_C(0x00011704)
#define VF2_NATIVE_POST_BOOT_LUMA_TABLE_INSTRUCTIONS UINT64_C(50891)
#define VF2_NATIVE_POST_BOOT_LUMA_WAIT_ENTRY UINT32_C(0x000098b8)
#define VF2_NATIVE_POST_BOOT_LUMA_WAIT_EXIT UINT32_C(0x000098bc)
#define VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_ENTRY UINT32_C(0x000098bc)
#define VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_BODY UINT32_C(0x00011744)
#define VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_HELPER UINT32_C(0x000117a8)
#define VF2_NATIVE_POST_BOOT_PATTERN_WAIT_ENTRY UINT32_C(0x00011798)
#define VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT UINT32_C(0x0001179c)
#define VF2_NATIVE_POST_BOOT_FINAL_WAIT_ENTRY UINT32_C(0x000098c0)
#define VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT UINT32_C(0x000098c4)
#define VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_BODY UINT32_C(0x000117f8)
#define VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_ENTRY UINT32_C(0x00011860)
#define VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT UINT32_C(0x00011864)
#define VF2_NATIVE_POST_BOOT_GRAPHICS_STATE_RESET_ENTRY UINT32_C(0x000098c8)
#define VF2_NATIVE_POST_BOOT_GRAPHICS_STATE_RESET_BODY UINT32_C(0x0004ad40)
#define VF2_NATIVE_POST_BOOT_VIDEO_CONSTANTS_ENTRY UINT32_C(0x000098cc)
#define VF2_NATIVE_POST_BOOT_VIDEO_CONSTANTS_BODY UINT32_C(0x00007f7c)
#define VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_ENTRY UINT32_C(0x000098d0)
#define VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_BODY UINT32_C(0x00007ef0)
#define VF2_NATIVE_POST_BOOT_TASK_REGISTRY_ENTRY UINT32_C(0x000098d4)
#define VF2_NATIVE_POST_BOOT_TASK_REGISTRY_BODY UINT32_C(0x00010cbc)
#define VF2_NATIVE_TASK_DESCRIPTOR_TABLE UINT32_C(0x00011dc0)
#define VF2_NATIVE_POST_BOOT_GRAPHICS_BUFFER_ENTRY UINT32_C(0x000098d8)
#define VF2_NATIVE_POST_BOOT_GRAPHICS_BUFFER_BODY UINT32_C(0x00050130)
#define VF2_NATIVE_POST_BOOT_RENDER_STATE_ENTRY UINT32_C(0x000098dc)
#define VF2_NATIVE_POST_BOOT_RENDER_STATE_BODY UINT32_C(0x0004e7b4)
#define VF2_NATIVE_POST_BOOT_RENDER_TABLE_CLEAR UINT32_C(0x0004f904)
#define VF2_NATIVE_POST_BOOT_GAME_DEFAULTS_ENTRY UINT32_C(0x000098e0)
#define VF2_NATIVE_POST_BOOT_GAME_DEFAULTS_BODY UINT32_C(0x00044084)
#define VF2_NATIVE_POST_BOOT_GAME_TABLE_INIT UINT32_C(0x00023bfc)
#define VF2_NATIVE_POST_BOOT_GAME_FLOAT_INIT UINT32_C(0x0001fee4)
#define VF2_NATIVE_POST_BOOT_OBJECT_TABLE_ENTRY UINT32_C(0x000098e4)
#define VF2_NATIVE_POST_BOOT_OBJECT_TABLE_BODY UINT32_C(0x00053750)
#define VF2_NATIVE_POST_BOOT_EFFECT_TABLE_ENTRY UINT32_C(0x000098e8)
#define VF2_NATIVE_POST_BOOT_EFFECT_TABLE_BODY UINT32_C(0x0000a0c4)
#define VF2_NATIVE_POST_BOOT_INPUT_RING_ENTRY UINT32_C(0x000098ec)
#define VF2_NATIVE_POST_BOOT_INPUT_RING_BODY UINT32_C(0x000012bc)
#define VF2_NATIVE_POST_BOOT_IO_ENTRY UINT32_C(0x000098f0)
#define VF2_NATIVE_POST_BOOT_IO_BODY UINT32_C(0x00000fa0)
#define VF2_NATIVE_POST_BOOT_TEXT_COPY UINT32_C(0x00007fc0)
#define VF2_NATIVE_POST_BOOT_GAME_DATA_COPY_ENTRY UINT32_C(0x000098f4)
#define VF2_NATIVE_POST_BOOT_DISPLAY_OFFSET_ENTRY UINT32_C(0x00009920)
#define VF2_NATIVE_POST_BOOT_DISPLAY_OFFSET_BODY UINT32_C(0x0000245c)
#define VF2_NATIVE_POST_BOOT_FRAME_ACCUMULATOR_ENTRY UINT32_C(0x00009924)
#define VF2_NATIVE_POST_BOOT_FRAME_ACCUMULATOR_BODY UINT32_C(0x0001128c)
#define VF2_NATIVE_POST_BOOT_PROFILE_DEFAULTS_ENTRY UINT32_C(0x00009928)
#define VF2_NATIVE_POST_BOOT_PROFILE_DEFAULTS_BODY UINT32_C(0x000113f4)
#define VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_ENTRY UINT32_C(0x0000992c)
#define VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_EXIT UINT32_C(0x000099fc)
#define VF2_NATIVE_POST_BOOT_INPUT_PROFILE_BODY UINT32_C(0x0001fcc0)
#define VF2_NATIVE_POST_BOOT_FLOAT_DEFAULTS_ENTRY UINT32_C(0x0001fdd0)
#define VF2_NATIVE_POST_BOOT_FLOAT_DEFAULTS_BODY UINT32_C(0x0001ff0c)
#define VF2_NATIVE_POST_BOOT_FLOAT_FILL_BODY UINT32_C(0x0001fee4)
#define VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_ENTRY UINT32_C(0x0001fdd4)
#define VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_EXIT UINT32_C(0x0001fe60)
#define VF2_NATIVE_POST_BOOT_PALETTE_RAMP_BODY UINT32_C(0x0001fffc)
#define VF2_NATIVE_POST_BOOT_PALETTE_BUILD_BODY UINT32_C(0x00002c38)
#define VF2_NATIVE_POST_BOOT_PALETTE_BUILD_INSTRUCTIONS UINT64_C(39208)
#define VF2_NATIVE_POST_BOOT_PALETTE_BUILD_RETURN UINT32_C(0x00020050)
#define VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_ENTRY UINT32_C(0x0001fe64)
#define VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_HELPER UINT32_C(0x0004b410)
#define VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_HELPER_RETURN UINT32_C(0x0001fe78)
#define VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_NEXT UINT32_C(0x0002eab8)
#define VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_RETURN UINT32_C(0x0001fedc)
#define VF2_NATIVE_POST_BOOT_RESUMED_LUMA_RETURN UINT32_C(0x0001fee0)
#define VF2_NATIVE_POST_BOOT_RESUMED_HELPER_INIT_RETURN UINT32_C(0x0002ec20)
#define VF2_NATIVE_POST_BOOT_MAIN_LOOP_INIT_ENTRY UINT32_C(0x00009a00)
#define VF2_NATIVE_POST_BOOT_COPRO_INIT_ENTRY UINT32_C(0x0000a178)
#define VF2_NATIVE_POST_BOOT_COPRO_HELPER_ENTRY UINT32_C(0x0000a3d4)
#define VF2_NATIVE_POST_BOOT_COPRO_INIT_RETURN UINT32_C(0x00009f74)
#define VF2_NATIVE_POST_BOOT_DELAY_ENTRY UINT32_C(0x00009f74)
#define VF2_NATIVE_POST_BOOT_DELAY_LOOP_ENTRY UINT32_C(0x00009f84)
#define VF2_NATIVE_POST_BOOT_DELAY_LOOP_RETURN UINT32_C(0x00009f90)
#define VF2_NATIVE_POST_BOOT_DELAY_EXIT UINT32_C(0x00009fb0)
#define VF2_NATIVE_POST_BOOT_MAIN_LOOP_ENTRY UINT32_C(0x00009fb0)
#define VF2_NATIVE_POST_BOOT_RESUMED_HELPER_NESTED UINT32_C(0x00031004)
#define VF2_NATIVE_POST_BOOT_RESUMED_HELPER_INSTRUCTIONS UINT64_C(90)
#define VF2_NATIVE_POST_BOOT_RESUMED_HELPER_NESTED_INSTRUCTIONS UINT64_C(11)
#define VF2_NATIVE_FRAME_WAIT_POLL_ENTRY UINT32_C(0x00010f90)
#define VF2_NATIVE_INTERRUPT_RETURN_ENTRY UINT32_C(0x00000d20)
#define VF2_NATIVE_SECOND_SCHEDULER_ENTRY UINT32_C(0x0000a010)
#define VF2_NATIVE_SECOND_SCHEDULER_BODY UINT32_C(0x00010d54)
#define VF2_NATIVE_SCHEDULER_RETURN UINT32_C(0x00010dcc)
#define VF2_NATIVE_SCHEDULER_EPILOGUE_LDL UINT32_C(0x00010dd0)
#define VF2_NATIVE_SCHEDULER_EPILOGUE_AND UINT32_C(0x00010dd8)
#define VF2_NATIVE_SCHEDULER_EPILOGUE_SUB UINT32_C(0x00010ddc)
#define VF2_NATIVE_SCHEDULER_EPILOGUE_EXIT UINT32_C(0x00010e3c)
#define VF2_NATIVE_SCHEDULER_SCAN_EXIT UINT32_C(0x0001d458)
#define VF2_NATIVE_SCHEDULER_SCAN_NEXT_REGISTRY UINT32_C(0x00515400)
#define VF2_NATIVE_MAIN_AFTER_SCHEDULER UINT32_C(0x0000a014)
#define VF2_NATIVE_GAME_INFO_TASK_ENTRY UINT32_C(0x0001645c)
#define VF2_NATIVE_PLAYER_TASK_ENTRY UINT32_C(0x00013f08)
#define VF2_NATIVE_CAMERA_INITIAL_ENTRY UINT32_C(0x0001d320)
#define VF2_NATIVE_CAMERA_RECURRING_ENTRY UINT32_C(0x0001d458)
#define VF2_NATIVE_CAMERA_GATE_ENTRY UINT32_C(0x0001d660)
#define VF2_NATIVE_CAMERA_FAST_EXIT UINT32_C(0x0001e524)
#define VF2_NATIVE_USER_TASK_ENTRY UINT32_C(0x00029748)
#define VF2_NATIVE_SOUND_TASK_ENTRY UINT32_C(0x000439fc)
#define VF2_NATIVE_SOUND_CONTINUATION_ENTRY UINT32_C(0x00043abc)
#define VF2_NATIVE_KILL_OSAGE_TASK_ENTRY UINT32_C(0x000657dc)
#define VF2_NATIVE_OSAGE_TASK_ENTRY UINT32_C(0x000640f4)
#define VF2_NATIVE_OBJECT_TASK_ENTRY UINT32_C(0x0006ca64)
#define VF2_NATIVE_OBJECT_HANDLER0_ENTRY UINT32_C(0x0006cae0)
#define VF2_NATIVE_OBJECT_HANDLER0_NEXT UINT32_C(0x0006caf0)
#define VF2_NATIVE_OBJECT_HANDLER1_ENTRY UINT32_C(0x0006caf4)
#define VF2_NATIVE_OBJECT_HANDLER1_NEXT UINT32_C(0x0006cb04)
#define VF2_NATIVE_OBJECT_HANDLER2_ENTRY UINT32_C(0x0006cb08)
#define VF2_NATIVE_GAME_DISP_TASK_ENTRY UINT32_C(0x0002b1bc)
#define VF2_NATIVE_TASK_COUNT_ADDRESS UINT32_C(0x00011d94)
#define VF2_NATIVE_RUNTIME_FLAGS UINT32_C(0x00508000)
#define VF2_NATIVE_CURRENT_INDEX UINT32_C(0x00500038)
#define VF2_NATIVE_TIMER1 UINT32_C(0x00f00004)
#define VF2_NATIVE_TIMER2 UINT32_C(0x00f00008)
#define VF2_NATIVE_TIMER_MASK UINT32_C(0x000fffff)
#define VF2_NATIVE_SCRATCH_BASE UINT32_C(0x0050c000)
#define VF2_NATIVE_SCRATCH_STRIDE UINT32_C(0x20)

static vf2_status execute_boot_stage1(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_recovered_boot_stage1_report boot_report;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_BOOT_STAGE1_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&boot_report, 0, sizeof(boot_report));
    status = vf2_recovered_boot_stage1_execute(machine, cpu, &boot_report);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions += VF2_NATIVE_BOOT_STAGE1_INSTRUCTIONS;

    report->kind = VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE1;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status execute_boot_stage2(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_recovered_boot_stage2_report boot_report;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_BOOT_STAGE2_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&boot_report, 0, sizeof(boot_report));
    status = vf2_recovered_boot_stage2_execute(machine, cpu, &boot_report);
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE2;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status write_u16_le(vf2_model2a *machine, uint32_t address, uint16_t value) {
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static vf2_status enter_and_return_procedure(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                             uint32_t target, uint32_t return_address) {
    vf2_status status = vf2_i960_cpu_enter_procedure(cpu, target, return_address);
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    return status;
}

static vf2_status
execute_post_boot_init_prefix(vf2_model2a *machine, vf2_i960_cpu *cpu,
                              vf2_native_runtime_step_report *report) {
    static const uint16_t serial_words[] = {UINT16_C(0),  UINT16_C(0),  UINT16_C(0),
                                            UINT16_C(64), UINT16_C(78), UINT16_C(55)};
    static const uint32_t delay_returns[] = {
        UINT32_C(0x00043740), UINT32_C(0x0004374c), UINT32_C(0x00043758),
        UINT32_C(0x00043764), UINT32_C(0x00043770), UINT32_C(0x0004377c)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t byte_value = UINT8_C(0x80);
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_INIT_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* 0x52c branches directly to 0x9798. The prefix initializes three
     * diagnostic bytes, the mode word and the globals consumed by the first
     * post-reset subsystem initializer. */
    status = vf2_model2a_write(machine, UINT32_C(0x005000e0), &byte_value,
                               sizeof(byte_value));
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005000e1), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005000e2), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00500082), UINT16_C(0x8000));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[15] = UINT32_C(0x00008000);
    cpu->registers[1] += UINT32_C(0x40);
    cpu->registers[VF2_I960_G0_REGISTER + 10u] = UINT32_C(0x00800000);
    cpu->registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu->registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);

    /* call 0x4372c. Model its nested delay calls architecturally so the
     * procedure counters and local-frame windows stay identical to the ROM. */
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x0004372c), UINT32_C(0x000097dc));
    for (index = 0u; status == VF2_OK && index < 6u; ++index) {
        status = write_u16_le(machine, UINT32_C(0x01c80002), serial_words[index]);
        if (status == VF2_OK) {
            status = enter_and_return_procedure(machine, cpu, UINT32_C(0x000437bc),
                                                delay_returns[index]);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00504010), 0u);
    }
    byte_value = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504001), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504002), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504003), &byte_value,
                                   sizeof(byte_value));
    }
    byte_value = UINT8_C(0xff);
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504014), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00ae101f);
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x000438ec),
                                              UINT32_C(0x000437b8));
    }

    /* 0x438ec enqueues g0 in the first command slot. */
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00e80004), UINT32_C(33));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00e80004), UINT32_C(33));
    }
    byte_value = UINT8_C(1);
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504001), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00504020),
                                       cpu->registers[VF2_I960_G0_REGISTER]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504003), &byte_value,
                                   sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00e80004), UINT32_C(0x421));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00e80004), UINT32_C(0x421));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    /* 0xa048 is a branch-to-ret stub on this path. */
    if (status == VF2_OK) {
        status = enter_and_return_procedure(machine, cpu, UINT32_C(0x0000a048),
                                            UINT32_C(0x000097e0));
    }

    /* The last compare-decrement in the delay loop leaves the arithmetic
     * condition equal. call 0x6dd4c then opens the next local frame. */
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_INIT_EXIT,
                                              UINT32_C(0x000097e4));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_INIT_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INIT_PREFIX;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_video_init_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                   vf2_native_runtime_step_report *report) {
    static const uint32_t addresses[] = {UINT32_C(0x00500234), UINT32_C(0x00500235),
                                         UINT32_C(0x00500236), UINT32_C(0x00500237),
                                         UINT32_C(0x00500238), UINT32_C(0x00500239),
                                         UINT32_C(0x0050023a)};
    static const uint8_t values[] = {UINT8_C(0x75), UINT8_C(0x22), UINT8_C(0x75),
                                     UINT8_C(0x22), UINT8_C(0x75), UINT8_C(0x22),
                                     UINT8_C(0x1f)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;
    size_t index = 0u;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_VIDEO_INIT_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; status == VF2_OK && index < 7u; ++index) {
        status = vf2_model2a_write(machine, addresses[index], &values[index],
                                   sizeof(values[index]));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[15] = UINT32_C(0x1f);
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_VIDEO_RAMP_ENTRY,
                                          UINT32_C(0x0006dda4));
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_VIDEO_INIT_ENTRY_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_INIT_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static uint16_t video_ramp_value(uint32_t index, uint8_t slope, uint8_t bias,
                                 uint8_t scale) {
    int32_t value = ((int32_t)index - INT32_C(116)) * (int32_t)slope;

    value /= INT32_C(37);
    value += (int32_t)bias;
    if (value <= 0) {
        value = 0;
    } else if (value >= 256) {
        value = -1;
    }
    return (uint16_t)(((uint32_t)scale * (uint32_t)value) >> 7u);
}

static uint64_t video_ramp_helper_instruction_count(uint8_t slope, uint8_t bias) {
    uint64_t instructions = UINT64_C(3); /* two setup ops + ret */
    uint32_t index = 0u;

    for (index = 0u; index < 256u; ++index) {
        int32_t value = ((int32_t)index - INT32_C(116)) * (int32_t)slope;
        value /= INT32_C(37);
        value += (int32_t)bias;
        if (value <= 0) {
            instructions += UINT64_C(13);
        } else if (value >= 256) {
            instructions += UINT64_C(16);
        } else {
            instructions += UINT64_C(15);
        }
    }
    return instructions;
}

static vf2_status execute_post_boot_video_ramp(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                               vf2_native_runtime_step_report *report) {
    static const uint32_t table_bases[] = {UINT32_C(0x00544000), UINT32_C(0x00544200),
                                           UINT32_C(0x00544400)};
    static const uint32_t helper_returns[] = {
        UINT32_C(0x0000069c), UINT32_C(0x000006c0), UINT32_C(0x000006e4)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t controls[9] = {0u};
    uint32_t table = 0u;
    uint32_t index = 0u;
    uint64_t recovered_instructions = UINT64_C(34);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_VIDEO_RAMP_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500234), controls, 6u);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x005000e0), controls + 6u, 3u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00544600), controls,
                                   sizeof(controls));
    }
    if (status != VF2_OK) {
        return status;
    }

    for (table = 0u; table < 3u; ++table) {
        cpu->registers[VF2_I960_G0_REGISTER + 4u] = controls[table * 2u + 1u];
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = controls[table * 2u];
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = controls[6u + table];
        cpu->registers[VF2_I960_G0_REGISTER + 7u] = table_bases[table];
        recovered_instructions += video_ramp_helper_instruction_count(
            controls[table * 2u + 1u], controls[table * 2u]);
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x000006e8),
                                              helper_returns[table]);
        for (index = 0u; status == VF2_OK && index < 256u; ++index) {
            status = write_u16_le(machine, table_bases[table] + index * UINT32_C(2),
                                  video_ramp_value(index, controls[table * 2u + 1u],
                                                   controls[table * 2u],
                                                   controls[6u + table]));
        }
        if (status == VF2_OK) {
            cpu->registers[VF2_I960_G0_REGISTER + 7u] =
                table_bases[table] + UINT32_C(0x200);
            cpu->arithmetic_control =
                (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
            cpu->compare_result = VF2_I960_COMPARE_EQUAL;
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + recovered_instructions;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_RAMP;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static uint16_t post_boot_color_table_value(uint32_t raw, uint8_t slope, uint8_t bias) {
    uint32_t value = raw;

    if (value != 0u) {
        value = (uint32_t)slope * value;
        value >>= 8u;
        value += bias;
        if (value >= UINT32_C(256)) {
            value = UINT32_MAX;
        }
    }
    return (uint16_t)value;
}

static vf2_status
execute_post_boot_color_tables(vf2_model2a *machine, vf2_i960_cpu *cpu,
                               vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t controls[7] = {0u};
    uint32_t cursor = UINT32_C(0x01800000);
    uint32_t outer = 0u;
    uint32_t inner = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        (cpu->ip != VF2_NATIVE_POST_BOOT_COLOR_TABLES_ENTRY &&
         cpu->ip != VF2_NATIVE_POST_BOOT_SECOND_COLOR_TABLES_ENTRY)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status =
        vf2_model2a_read(machine, UINT32_C(0x00500234), controls, sizeof(controls));
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, UINT32_C(0x00002b4c),
            cpu->ip == VF2_NATIVE_POST_BOOT_COLOR_TABLES_ENTRY ? UINT32_C(0x0006dda8)
                                                               : UINT32_C(0x00009894));
    }
    for (outer = 0u; status == VF2_OK && outer < 32u; ++outer) {
        const uint32_t raw = outer * (uint32_t)controls[6];
        const uint16_t red = post_boot_color_table_value(raw, controls[1], controls[0]);
        const uint16_t green =
            post_boot_color_table_value(raw, controls[3], controls[2]);
        const uint16_t blue =
            post_boot_color_table_value(raw, controls[5], controls[4]);

        cursor += UINT32_C(0x80);
        for (inner = 0u; status == VF2_OK && inner < 64u; ++inner) {
            status = write_u16_le(machine, cursor + UINT32_C(0x00010000), red);
            if (status == VF2_OK) {
                status = write_u16_le(machine, cursor + UINT32_C(0x00014000), green);
            }
            if (status == VF2_OK) {
                status = write_u16_le(machine, cursor + UINT32_C(0x00018000), blue);
            }
            cursor += UINT32_C(2);
        }
        cursor += UINT32_C(0x100);
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_COLOR_TABLES_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COLOR_TABLES;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status fill_u16_region(vf2_model2a *machine, uint32_t address,
                                  size_t word_count, uint16_t value) {
    vf2_status status = VF2_OK;
    size_t index = 0u;

    for (index = 0u; status == VF2_OK && index < word_count; ++index) {
        status = write_u16_le(machine, address + (uint32_t)(index * 2u), value);
    }
    return status;
}

static vf2_status
execute_post_boot_memory_clear(vf2_model2a *machine, vf2_i960_cpu *cpu,
                               vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t zero = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        (cpu->ip != VF2_NATIVE_POST_BOOT_MEMORY_CLEAR_ENTRY &&
         cpu->ip != VF2_NATIVE_POST_BOOT_SECOND_MEMORY_CLEAR_ENTRY)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x00011a8c),
        cpu->ip == VF2_NATIVE_POST_BOOT_MEMORY_CLEAR_ENTRY ? UINT32_C(0x0006ddac)
                                                           : UINT32_C(0x0000989c));
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050304c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00503001), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = fill_u16_region(machine, UINT32_C(0x01008000), 2048u, 0u);
    }
    if (status == VF2_OK) {
        status = fill_u16_region(machine, UINT32_C(0x0100a000), 8u, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00503000), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = fill_u16_region(machine, UINT32_C(0x0100c000), 4096u, 0u);
    }
    if (status == VF2_OK) {
        status =
            fill_u16_region(machine, UINT32_C(0x01800000), 4096u, UINT16_C(0xfc00));
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x01802000);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x0000fc00);
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_MEMORY_CLEAR_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MEMORY_CLEAR;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status run_register_descriptor_stream(vf2_model2a *machine, uint32_t start,
                                                 uint32_t *end_cursor,
                                                 size_t *descriptor_count,
                                                 size_t *word_count);

static vf2_status run_halfword_descriptor_stream(vf2_model2a *machine, uint32_t start,
                                                 uint32_t *end_cursor,
                                                 size_t *descriptor_count,
                                                 size_t *halfword_count);

static vf2_status
execute_post_boot_register_stream(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                  vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t cursor = UINT32_C(0x028003d4);
    size_t descriptors = 0u;
    size_t words = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_REGISTER_STREAM_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = cursor;
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00023f30), UINT32_C(0x0006ddb8));
    if (status == VF2_OK) {
        status = run_register_descriptor_stream(machine, cursor, &cursor, &descriptors,
                                                &words);
    }
    if (status != VF2_OK || cursor != UINT32_C(0x02800b38) || descriptors != 4u ||
        words != 464u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = cursor;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_REGISTER_STREAM_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_REGISTER_STREAM;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status copy_model2a_bytes(vf2_model2a *machine, uint32_t source,
                                     uint32_t destination, size_t byte_count) {
    uint8_t buffer[256];
    size_t copied = 0u;
    vf2_status status = VF2_OK;

    while (status == VF2_OK && copied < byte_count) {
        const size_t remaining = byte_count - copied;
        const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        status = vf2_model2a_read(machine, source + (uint32_t)copied, buffer, chunk);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, destination + (uint32_t)copied, buffer,
                                       chunk);
        }
        copied += chunk;
    }
    return status;
}

static vf2_status
execute_post_boot_block_stream(vf2_model2a *machine, vf2_i960_cpu *cpu,
                               vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t cursor = UINT32_C(0x0280001c);
    size_t descriptors = 0u;
    size_t halfwords = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_BLOCK_STREAM_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = cursor;
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00023ee8), UINT32_C(0x0006ddc4));
    if (status == VF2_OK) {
        status = run_halfword_descriptor_stream(machine, cursor, &cursor, &descriptors,
                                                &halfwords);
    }
    if (status != VF2_OK || cursor != UINT32_C(0x028000d0) || descriptors != 22u ||
        halfwords != 92672u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = cursor;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_BLOCK_STREAM_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BLOCK_STREAM;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_backup_sram_probe(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_BACKUP_SRAM_PROBE_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_MAX;
    cpu->registers[15] = VF2_BACKUP_SRAM_BASE;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x0050016c), VF2_BACKUP_SRAM_BASE);
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x0006e1cc),
                                              UINT32_C(0x0006ddd8));
    }
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        const uint32_t address = VF2_BACKUP_SRAM_BASE + (uint32_t)index;
        uint8_t original = 0u;
        uint8_t observed = 0u;
        uint8_t pattern = UINT8_C(0x55);

        status = vf2_model2a_read(machine, address, &original, sizeof(original));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, address, &pattern, sizeof(pattern));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, address, &observed, sizeof(observed));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, address, &original, sizeof(original));
        }
        if (status != VF2_OK || observed != pattern) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        pattern = UINT8_C(0xaa);
        status = vf2_model2a_write(machine, address, &pattern, sizeof(pattern));
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, address, &observed, sizeof(observed));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, address, &original, sizeof(original));
        }
        if (status != VF2_OK || observed != pattern) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_BACKUP_SRAM_PROBE_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_SRAM_PROBE;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_backup_restore(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                 vf2_native_runtime_step_report *report) {
    static const uint32_t signature[4] = {UINT32_C(0x54524956), UINT32_C(0x46204155),
                                          UINT32_C(0x54484749), UINT32_C(0x32205245)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t base = VF2_BACKUP_SRAM_BASE;
    uint16_t version = 0u;
    uint16_t stored_crc = 0u;
    uint16_t crc = 0u;
    uint8_t flags = 0u;
    size_t index = 0u;
    int restore_payload = 1;
    vf2_hybrid_bridge_report diagnostic_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_BACKUP_RESTORE_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* A cold Model 2 boot has an erased backup SRAM image.  The ROM displays
     * the matching diagnostic string and then falls through to the same
     * payload/metadata initialization used after a valid restore. */
    memset(&diagnostic_report, 0, sizeof(diagnostic_report));
    if (cpu->registers[VF2_I960_G0_REGISTER] != 0u) {
        cpu->registers[25] = UINT32_C(0x010005b8);
        cpu->registers[14] = UINT32_C(0x0006dfd0);
        status = execute_inline_text_thunk(machine, cpu, &diagnostic_report);
        restore_payload = 0;
    } else {
        int signature_valid = 1;
        for (index = 0u; status == VF2_OK && index < 4u; ++index) {
            uint32_t observed = 0u;
            status = vf2_model2a_read_u32(
                machine,
                VF2_BACKUP_SRAM_BASE + UINT32_C(0x3308) +
                    (uint32_t)(index * sizeof(uint32_t)),
                &observed);
            if (status == VF2_OK && observed != signature[index]) {
                signature_valid = 0;
            }
        }
        if (status == VF2_OK && !signature_valid) {
            cpu->registers[25] = UINT32_C(0x010008aa);
            cpu->registers[14] = UINT32_C(0x0006df10);
            status = execute_inline_text_thunk(machine, cpu, &diagnostic_report);
        }
        if (status == VF2_OK && signature_valid) {
            uint8_t bytes[2] = {0u, 0u};
            status = vf2_model2a_read(
                machine, VF2_BACKUP_SRAM_BASE + UINT32_C(0x3306), bytes, sizeof(bytes));
            version = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
            if (status == VF2_OK && version != UINT16_C(24)) {
                cpu->registers[25] = UINT32_C(0x010008a4);
                cpu->registers[14] = UINT32_C(0x0006dedc);
                status = execute_inline_text_thunk(machine, cpu, &diagnostic_report);
            }
        }
        if (status == VF2_OK && signature_valid && version == UINT16_C(24)) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
            if (status != VF2_OK || base != VF2_BACKUP_SRAM_BASE) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }

            /* call 0x9480 for the 29-byte block at +0x3340. */
            cpu->registers[VF2_I960_G0_REGISTER] = base + UINT32_C(0x3340);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(29);
            status = vf2_i960_cpu_enter_procedure(
                cpu, UINT32_C(0x00009480), UINT32_C(0x0006de40));
            if (status == VF2_OK) {
                status = vf2_recovered_table_crc16(machine, base + UINT32_C(0x3340), 0u,
                                                   UINT32_C(29), &crc);
            }
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER] = (uint32_t)crc;
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
                cpu->compare_result = VF2_I960_COMPARE_EQUAL;
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status == VF2_OK) {
                uint8_t bytes[2] = {0u, 0u};
                status = vf2_model2a_read(
                    machine, VF2_BACKUP_SRAM_BASE + UINT32_C(0x3302), bytes, sizeof(bytes));
                stored_crc = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
            }
            if (status != VF2_OK || stored_crc != crc) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }

            /* call 0x9480 for the 15-byte block at +0x3320. */
            cpu->registers[VF2_I960_G0_REGISTER] = base + UINT32_C(0x3320);
            cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
            cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(15);
            status = vf2_i960_cpu_enter_procedure(
                cpu, UINT32_C(0x00009480), UINT32_C(0x0006de94));
            if (status == VF2_OK) {
                status = vf2_recovered_table_crc16(machine, base + UINT32_C(0x3320), 0u,
                                                   UINT32_C(15), &crc);
            }
            if (status == VF2_OK) {
                cpu->registers[VF2_I960_G0_REGISTER] = (uint32_t)crc;
                cpu->arithmetic_control =
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
                cpu->compare_result = VF2_I960_COMPARE_EQUAL;
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status == VF2_OK) {
                uint8_t bytes[2] = {0u, 0u};
                status = vf2_model2a_read(
                    machine, VF2_BACKUP_SRAM_BASE + UINT32_C(0x3300), bytes, sizeof(bytes));
                stored_crc = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
            }
            if (status != VF2_OK || stored_crc != crc) {
                return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
            }
        }
    }

    /* Restore the persisted 0x3fe0-byte payload to its work-RAM mirror. */
    if (restore_payload) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050016c), VF2_BACKUP_SRAM_BASE);
        if (status == VF2_OK) {
            status =
                vf2_model2a_read(machine, UINT32_C(0x00500171), &flags, sizeof(flags));
        }
        flags |= UINT8_C(1);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500171), &flags,
                                       sizeof(flags));
        }
        if (status == VF2_OK) {
            status = copy_model2a_bytes(machine, VF2_BACKUP_SRAM_BASE,
                                        UINT32_C(0x00599000),
                                        (size_t)UINT32_C(0x00003fe0));
        }
    }

    /* Refresh the metadata exactly as the ROM does after a valid restore. */
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_write_u32(machine,
                                       VF2_BACKUP_SRAM_BASE + UINT32_C(0x3308) +
                                           (uint32_t)(index * sizeof(uint32_t)),
                                       signature[index]);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, VF2_BACKUP_SRAM_BASE + UINT32_C(0x3306),
                              UINT16_C(24));
    }
    if (status == VF2_OK) {
        uint8_t zeroes[4] = {0u, 0u, 0u, 0u};
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x0050016c), UINT32_C(0x00599000));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500148), zeroes,
                                       sizeof(zeroes));
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    /* The 0x6dd4c initializer returns to its caller at 0x97e4. */
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_BACKUP_RESTORE_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_RESTORE;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_restored_video_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                       vf2_native_runtime_step_report *report) {
    static const uint32_t source_offsets[7] = {
        UINT32_C(0x3356), UINT32_C(0x3359), UINT32_C(0x3357), UINT32_C(0x335a),
        UINT32_C(0x3358), UINT32_C(0x335b), UINT32_C(0x335c)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t base = 0u;
    uint8_t value = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_RESTORED_VIDEO_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status != VF2_OK || base != UINT32_C(0x00599000)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    for (index = 0u; status == VF2_OK && index < 7u; ++index) {
        status = vf2_model2a_read(machine, base + source_offsets[index], &value,
                                  sizeof(value));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500234) + (uint32_t)index,
                                       &value, sizeof(value));
        }
    }
    if (status == VF2_OK) {
        cpu->registers[3] = (uint32_t)value;
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_VIDEO_RAMP_ENTRY, UINT32_C(0x00009890));
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_RESTORED_VIDEO_ENTRY_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESTORED_VIDEO_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_palette_seed(vf2_model2a *machine, vf2_i960_cpu *cpu,
                               vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_PALETTE_SEED_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00011560), UINT32_C(0x00009898));
    if (status == VF2_OK) {
        status = copy_model2a_bytes(machine, UINT32_C(0x02100000), UINT32_C(0x01802000),
                                    (size_t)UINT32_C(0x00000800));
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_PALETTE_SEED_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_SEED;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status run_register_descriptor_stream(vf2_model2a *machine, uint32_t start,
                                                 uint32_t *end_cursor,
                                                 size_t *descriptor_count,
                                                 size_t *word_count) {
    uint32_t cursor = start;
    size_t guard = 0u;
    size_t total_words = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || end_cursor == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    while (status == VF2_OK && guard < 64u) {
        uint32_t destination = 0u;
        uint32_t encoded_count = 0u;
        uint32_t words = 0u;
        uint32_t index = 0u;

        status = vf2_model2a_read_u32(machine, cursor, &destination);
        cursor += UINT32_C(4);
        if (status != VF2_OK || destination == 0u) {
            break;
        }
        status = vf2_model2a_read_u32(machine, cursor, &encoded_count);
        cursor += UINT32_C(4);
        if (status != VF2_OK || (int32_t)encoded_count < 0) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        words = (uint32_t)((int32_t)encoded_count / INT32_C(2));
        if (words > UINT32_C(65536)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        for (index = 0u; status == VF2_OK && index < words; ++index) {
            uint32_t value = 0u;
            status = vf2_model2a_read_u32(machine, cursor, &value);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination + index * UINT32_C(4), value);
            }
            cursor += UINT32_C(4);
        }
        total_words += (size_t)words;
        ++guard;
    }
    if (status != VF2_OK || guard == 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    *end_cursor = cursor;
    if (descriptor_count != NULL) {
        *descriptor_count = guard;
    }
    if (word_count != NULL) {
        *word_count = total_words;
    }
    return VF2_OK;
}

static vf2_status run_halfword_descriptor_stream(vf2_model2a *machine, uint32_t start,
                                                 uint32_t *end_cursor,
                                                 size_t *descriptor_count,
                                                 size_t *halfword_count) {
    uint32_t cursor = start;
    size_t guard = 0u;
    size_t total_halfwords = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || end_cursor == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    while (status == VF2_OK && guard < 64u) {
        uint32_t source = 0u;
        uint32_t destination = 0u;
        uint32_t header = 0u;
        uint32_t halfwords = 0u;

        status = vf2_model2a_read_u32(machine, cursor, &source);
        cursor += UINT32_C(4);
        if (status != VF2_OK || source == 0u) {
            break;
        }
        status = vf2_model2a_read_u32(machine, cursor, &destination);
        cursor += UINT32_C(4);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, source, &header);
        }
        if (status != VF2_OK || header > (UINT32_MAX >> 4u)) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        halfwords = header << 4u;
        if (halfwords > UINT32_C(65536)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        status = copy_model2a_bytes(machine, source + UINT32_C(4), destination,
                                    (size_t)halfwords * sizeof(uint16_t));
        total_halfwords += (size_t)halfwords;
        ++guard;
    }
    if (status != VF2_OK || guard == 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    *end_cursor = cursor;
    if (descriptor_count != NULL) {
        *descriptor_count = guard;
    }
    if (halfword_count != NULL) {
        *halfword_count = total_halfwords;
    }
    return VF2_OK;
}

static vf2_status execute_post_boot_table_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                               vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t cursor = 0u;
    uint32_t stream_pointer = 0u;
    uint32_t flags = 0u;
    uint32_t table_cursor = UINT32_C(0x00011cb4);
    size_t table_entries = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TABLE_INIT_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* call 0x11b48, then its initial 0x11c20 8 KiB clear. */
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00011b48), UINT32_C(0x000098a0));
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00011c20),
                                              UINT32_C(0x00011b4c));
    }
    if (status == VF2_OK) {
        uint8_t zeroes[256] = {0u};
        size_t offset = 0u;
        for (offset = 0u; status == VF2_OK && offset < 0x2000u;
             offset += sizeof(zeroes)) {
            status = vf2_model2a_write(machine, UINT32_C(0x01080000) + (uint32_t)offset,
                                       zeroes, sizeof(zeroes));
        }
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x01082000);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    /* The first two stream pointers live in the main-data directory. */
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x02800000), &cursor);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cursor, &stream_pointer);
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00023ee8),
                                              UINT32_C(0x00011b60));
    }
    if (status == VF2_OK) {
        status = run_halfword_descriptor_stream(machine, stream_pointer,
                                                &stream_pointer, NULL, NULL);
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    cursor += UINT32_C(4);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, cursor, &stream_pointer);
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00023f30),
                                              UINT32_C(0x00011b6c));
    }
    if (status == VF2_OK) {
        status = run_register_descriptor_stream(machine, stream_pointer,
                                                &stream_pointer, NULL, NULL);
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    /* Two compact descriptor tables are embedded directly in the i960 ROM. */
    stream_pointer = UINT32_C(0x0001256c);
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00023ee8),
                                              UINT32_C(0x00011b7c));
    }
    if (status == VF2_OK) {
        status = run_halfword_descriptor_stream(machine, stream_pointer,
                                                &stream_pointer, NULL, NULL);
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    stream_pointer = UINT32_C(0x00012520);
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00023f30),
                                              UINT32_C(0x00011b88));
    }
    if (status == VF2_OK) {
        status = run_register_descriptor_stream(machine, stream_pointer,
                                                &stream_pointer, NULL, NULL);
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = stream_pointer;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    /* 0x11be4 is an empty inline table in this ROM revision. */
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00011be4),
                                              UINT32_C(0x00011b8c));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00011d38), &flags);
    }
    if (status != VF2_OK || flags != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_i960_cpu_return_procedure(cpu, machine);

    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00503004), UINT32_C(6));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00503008), UINT32_C(9));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00503002), &flags);
    }
    flags |= UINT32_C(1) << 15u;
    flags &= ~(UINT32_C(1) << 14u);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00503002), flags);
    }

    /* 0x11c44 expands the 64-entry palette index list terminated by -1. */
    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(0x0000fd02);
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00011c44),
                                              UINT32_C(0x00011be0));
    }
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = table_cursor;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(5);
    while (status == VF2_OK && table_entries < 256u) {
        uint8_t bytes[2] = {0u, 0u};
        int16_t table_value = 0;
        status = vf2_model2a_read(machine, table_cursor, bytes, sizeof(bytes));
        table_cursor += UINT32_C(2);
        table_value = (int16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = table_cursor;
        cpu->registers[VF2_I960_G0_REGISTER + 4u] = (uint32_t)(int32_t)table_value;
        if (status != VF2_OK || table_value == -1) {
            break;
        }
        status = write_u16_le(
            machine, VF2_PALETTE_RAM_BASE + ((uint32_t)(uint16_t)table_value << 5u),
            UINT16_C(0xfd02));
        ++table_entries;
    }
    if (status != VF2_OK || table_entries != 64u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_TABLE_INIT_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TABLE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_hardware_core_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_HARDWARE_CORE_INIT_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* 0x0ed0 expands the 64-entry geometry register bootstrap table. */
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000ed0), UINT32_C(0x000098a4));
    for (index = 0u; status == VF2_OK && index < 64u; ++index) {
        uint8_t bytes[2] = {0u, 0u};
        uint32_t value = 0u;
        const uint32_t destination = VF2_GEOMETRY_BASE + index * UINT32_C(16);
        status = vf2_model2a_read(machine, UINT32_C(0x00003a00) + index * UINT32_C(2),
                                  bytes, sizeof(bytes));
        value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, destination + UINT32_C(4), value);
        }
        if (status == VF2_OK) {
            status =
                vf2_model2a_write_u32(machine, destination + UINT32_C(8), value >> 8u);
        }
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    /* 0x0e60 temporarily gates video access while copying the copro table. */
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000e60),
                                              UINT32_C(0x000098a8));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_VIDEO_CONTROL_BASE,
                                       UINT32_C(0x80000000));
    }
    if (status == VF2_OK) {
        status = copy_model2a_bytes(machine, UINT32_C(0x00003e90),
                                    VF2_COPRO_PORT_BASE + UINT32_C(0x4000),
                                    (size_t)UINT32_C(0x00003b68));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_VIDEO_CONTROL_BASE, 0u);
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    /* 0x0ea4 observes the ready bit and acknowledges the copro port. */
    if (status == VF2_OK) {
        uint32_t video_status = 0u;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000ea4),
                                              UINT32_C(0x000098ac));
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, VF2_VIDEO_CONTROL_BASE + UINT32_C(4),
                                          &video_status);
        }
        if (status != VF2_OK || (video_status & UINT32_C(1)) == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        status = vf2_model2a_write_u32(machine,
                                       cpu->registers[VF2_I960_G0_REGISTER + 11u] +
                                           cpu->registers[VF2_I960_G0_REGISTER + 12u],
                                       0u);
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
    }

    /* 0x0f0c primes the four-command geometry ring and reuses the already
     * recovered 0x2edc frame-commit procedure for the first advancement. */
    if (status == VF2_OK) {
        vf2_hybrid_bridge_report bridge_report;
        uint8_t ring_index = UINT8_C(3);
        uint32_t command = 0u;

        memset(&bridge_report, 0, sizeof(bridge_report));
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000f0c),
                                              UINT32_C(0x000098b0));
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine,
                                           VF2_VIDEO_CONTROL_BASE + UINT32_C(0x0c), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00501008), 0u);
        }
        for (index = 0u; status == VF2_OK && index < 3u; ++index) {
            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x00007a00) + index * UINT32_C(4), &command);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine,
                    cpu->registers[VF2_I960_G0_REGISTER + 10u] + UINT32_C(0x1008),
                    command);
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine,
                    cpu->registers[VF2_I960_G0_REGISTER + 10u] + UINT32_C(0x00f0),
                    command);
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, UINT32_C(0x00007a0c), &command);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00501004), command);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, cpu->registers[VF2_I960_G0_REGISTER + 10u] + UINT32_C(0x1008),
                command);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050100c), &ring_index,
                                       sizeof(ring_index));
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00002edc),
                                                  UINT32_C(0x00000f68));
        }
        if (status == VF2_OK) {
            status = vf2_hybrid_post_frame_bridge_execute(machine, cpu, &bridge_report);
        }
        if (status == VF2_OK) {
            uint8_t marker = UINT8_C(0xff);
            status = vf2_model2a_write(machine, UINT32_C(0x0181c000), &marker,
                                       sizeof(marker));
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_HARDWARE_CORE_INIT_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_HARDWARE_CORE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_texture_init_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_INIT_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_TEXTURE_INIT_BODY_ENTRY, UINT32_C(0x000098b4));
    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0055000c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550080), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005500f4), 0u);
    }
    if (status == VF2_OK) {
        cpu->registers[3] = UINT32_C(0x005502c0);
        cpu->registers[15] = 0u;
        status = vf2_model2a_write_u32(machine, cpu->registers[3], 0u);
    }
    if (status == VF2_OK) {
        cpu->registers[3] = UINT32_C(0x005502d0);
        cpu->registers[15] = 0u;
        status = vf2_model2a_write_u32(machine, cpu->registers[3], 0u);
    }
    if (status == VF2_OK) {
        cpu->registers[3] = UINT32_C(0x005502e0);
        cpu->registers[15] = 0u;
        status = vf2_model2a_write_u32(machine, cpu->registers[3], 0u);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_TEXTURE_TIMER_ENTRY, UINT32_C(0x0004b07c));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_TEXTURE_INIT_ENTRY_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_INIT_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_texture_timer_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_TIMER_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00f00008), &cpu->registers[14]);
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, UINT32_C(0x00f0000c), &cpu->registers[15]);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[14] = UINT32_C(0x000fffff);
    cpu->registers[15] &= cpu->registers[14];
    cpu->registers[15] = cpu->registers[14] - cpu->registers[15];
    cpu->registers[15] -= UINT32_C(18);
    cpu->registers[VF2_I960_G0_REGISTER] =
        cpu->registers[15] / UINT32_C(25) + UINT32_C(100);
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000b6c), UINT32_C(0x0004afdc));
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_TEXTURE_TIMER_ENTRY_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_TIMER_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_texture_wait_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_state *state,
                                     vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    uint8_t frame_byte = 0u;
    vf2_hybrid_frame_wait_report wait_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500000), &frame_byte,
                              sizeof(frame_byte));
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = frame_byte;
    cpu->ip = VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_POLL;
    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_ENTRY_INSTRUCTIONS;
    memset(&wait_report, 0, sizeof(wait_report));
    status =
        vf2_hybrid_frame_wait_observe(machine, cpu, &state->frame_wait, &wait_report);
    if (status != VF2_OK) {
        return status;
    }
    report->kind = VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT;
    report->bridge_kind = VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    return VF2_OK;
}

static vf2_status
execute_post_boot_texture_wait_poll(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_state *state,
                                    vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_POLL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    for (;;) {
        uint8_t frame_byte = 0u;
        uint8_t wait_flag = 0u;

        status = vf2_model2a_read(machine, UINT32_C(0x00500000), &frame_byte,
                                  sizeof(frame_byte));
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[4] = frame_byte;
        ++cpu->executed_instructions;

        ++cpu->executed_instructions;
        if (cpu->registers[3] != cpu->registers[4]) {
            const uint16_t result = UINT16_C(1);
            cpu->registers[15] = 1u;
            status = vf2_model2a_write(machine, UINT32_C(0x0055c2f2), &result,
                                       sizeof(result));
            if (status == VF2_OK) {
                status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000f7c),
                                                      UINT32_C(0x0004b01c));
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->executed_instructions += UINT64_C(3);
            {
                vf2_hybrid_frame_wait_report wait_report;
                memset(&wait_report, 0, sizeof(wait_report));
                status = vf2_hybrid_frame_wait_observe(machine, cpu, &state->frame_wait,
                                                       &wait_report);
                if (status != VF2_OK) {
                    return status;
                }
            }
            break;
        }

        status = vf2_model2a_read(machine, UINT32_C(0x0050008c), &wait_flag,
                                  sizeof(wait_flag));
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[5] = wait_flag;
        cpu->executed_instructions += UINT64_C(2);
        if (wait_flag != 0u) {
            const uint16_t result = UINT16_C(0);
            cpu->registers[15] = 0u;
            status = vf2_model2a_write(machine, UINT32_C(0x0055c2f2), &result,
                                       sizeof(result));
            if (status == VF2_OK) {
                status = vf2_i960_cpu_return_procedure(cpu, machine);
            }
            if (status != VF2_OK) {
                return status;
            }
            cpu->executed_instructions += UINT64_C(3);
            break;
        }

        cpu->ip = VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_POLL;
        {
            vf2_hybrid_frame_wait_report wait_report;
            memset(&wait_report, 0, sizeof(wait_report));
            status = vf2_hybrid_frame_wait_observe(machine, cpu, &state->frame_wait,
                                                   &wait_report);
            if (status != VF2_OK) {
                return status;
            }
            if (wait_report.interrupt_injected) {
                break;
            }
        }
    }

    report->kind = VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT;
    report->bridge_kind = VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_early_wait_return(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_i960_cpu candidate;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_RETURN) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    candidate = *cpu;
    status = vf2_i960_cpu_return_procedure(&candidate, machine);
    if (status == VF2_OK &&
        candidate.ip == VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_CALLER_RETURN) {
        status = vf2_i960_cpu_return_procedure(&candidate, machine);
        if (status == VF2_OK &&
            candidate.ip != VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_EXIT) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    } else if (status == VF2_OK &&
               candidate.ip != VF2_NATIVE_POST_BOOT_LUMA_WAIT_EXIT &&
               candidate.ip != VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT &&
               candidate.ip != VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT &&
               candidate.ip != VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status != VF2_OK) {
        return status;
    }

    candidate.executed_instructions =
        start_instructions + (candidate.procedure_returns - start_returns);

    if (cpu->local_frame_depth >= 4u) {
        uint64_t warm_wait_instruction_correction = 0u;
        uint32_t interrupt_stack = 0u;
        uint32_t saved_ac_address = 0u;

        if (candidate.ip == VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_EXIT &&
            cpu->local_frame_depth == 6u) {
            warm_wait_instruction_correction = UINT64_C(4);
        } else if ((candidate.ip == VF2_NATIVE_POST_BOOT_LUMA_WAIT_EXIT ||
                    candidate.ip == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT ||
                    candidate.ip == VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT ||
                    candidate.ip == VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT) &&
                   (cpu->local_frame_depth == 4u || cpu->local_frame_depth == 5u)) {
            warm_wait_instruction_correction = UINT64_C(7);
        }

        if (warm_wait_instruction_correction != 0u) {
            status = vf2_model2a_read_u32(machine, cpu->prcb + UINT32_C(24),
                                          &interrupt_stack);
            if (status == VF2_OK) {
                saved_ac_address =
                    ((interrupt_stack + UINT32_C(63)) & ~UINT32_C(63)) +
                    UINT32_C(52);
                status = vf2_model2a_write_u32(
                    machine, saved_ac_address,
                    (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1));
            }
            if (status != VF2_OK) {
                return status;
            }
            candidate.executed_instructions += warm_wait_instruction_correction;
            candidate.arithmetic_control &= ~UINT32_C(7);
            candidate.compare_result = VF2_I960_COMPARE_NONE;
        }
    }
    *cpu = candidate;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_graphics_verify(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                  vf2_native_runtime_step_report *report) {
    static const uint32_t identity_addresses[] = {
        UINT32_C(0x02301000), UINT32_C(0x02400000), UINT32_C(0x02600000),
        UINT32_C(0x02b00000)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t expected_board = 0u;
    uint32_t observed_board = 0u;
    uint32_t identity_cursor = 0u;
    uint32_t record = UINT32_C(0x00550168);
    size_t identity_index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_EXIT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0004ad74), &expected_board);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x02300000), &observed_board);
    }
    if (status == VF2_OK && expected_board != observed_board) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x02300004), &identity_cursor);
    }
    for (identity_index = 0u; status == VF2_OK && identity_index < 4u;
         ++identity_index) {
        uint32_t expected_identity = 0u;
        uint32_t observed_identity = 0u;

        status = vf2_model2a_read_u32(machine, identity_addresses[identity_index],
                                      &expected_identity);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine,
                identity_cursor + (uint32_t)(identity_index * sizeof(uint32_t)),
                &observed_identity);
        }
        if (status == VF2_OK && expected_identity != observed_identity) {
            return VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    status = write_u16_le(machine, UINT32_C(0x0055c2f0), 0u);
    while (status == VF2_OK && record < UINT32_C(0x005502a8)) {
        status = write_u16_le(machine, record, UINT16_MAX);
        if (status == VF2_OK) {
            status = write_u16_le(machine, record + UINT32_C(2), 0u);
        }
        if (status == VF2_OK) {
            status = write_u16_le(machine, record + UINT32_C(0x1c), 0u);
        }
        record += UINT32_C(0x20);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = UINT32_C(40);
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x01000a28);
    cpu->registers[3] = UINT32_C(0x0000ffff);
    cpu->registers[4] = 0u;
    cpu->registers[5] = UINT32_C(0x005502a8);
    cpu->registers[6] = UINT32_C(0x005502a8);
    cpu->registers[7] = 0u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_GRAPHICS_VERIFY_EXIT, UINT32_C(0x0004b3e8));
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_GRAPHICS_VERIFY_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_VERIFY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_texture_record_entry(vf2_i960_cpu *cpu,
                                       vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GRAPHICS_VERIFY_EXIT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 1u] = 0u;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x00550168);
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = 1u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_SETUP, UINT32_C(0x0004b834));
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_ENTRY_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_texture_record_setup(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                       vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t expected_board = 0u;
    uint32_t observed_board = 0u;
    uint16_t record_status = 0u;
    uint16_t highest_priority = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_SETUP) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->registers[VF2_I960_G0_REGISTER] != UINT32_C(40) ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != 0u ||
        cpu->registers[VF2_I960_G0_REGISTER + 2u] != UINT32_C(0x00550168) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] != 1u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0004ad74), &expected_board);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x02300000), &observed_board);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00550168), &record_status,
                                  sizeof(record_status));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0055c2f0), &highest_priority,
                                  sizeof(highest_priority));
    }
    if (status != VF2_OK) {
        return status;
    }
    if (expected_board != observed_board || record_status != UINT16_MAX ||
        highest_priority != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_write_u32(machine, UINT32_C(0x00550000), UINT32_C(1));
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00550168), UINT16_C(40));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x0055016a), UINT16_MAX);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550178), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00550184), UINT16_C(1));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0055000c), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550080), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005500f4), 0u);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK && cpu->ip != UINT32_C(0x0004b834)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip != UINT32_C(0x0004b3e8)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip != UINT32_C(0x000098b4)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_SETUP_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_SETUP;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_luma_table_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                  vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t source_start = UINT32_C(0x00078d10);
    const uint32_t destination_start = UINT32_C(0x12800000);
    uint32_t row_count = 0u;
    uint32_t row = 0u;
    uint32_t column = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_LUMA_TABLE_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00078d0c), &row_count);
    if (status != VF2_OK) {
        return status;
    }
    if (row_count != UINT32_C(66)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_LUMA_TABLE_BODY,
                                          UINT32_C(0x000098b8));
    for (row = 0u; status == VF2_OK && row < row_count; ++row) {
        for (column = 0u; status == VF2_OK && column < 128u; ++column) {
            const uint32_t index = row * UINT32_C(128) + column;
            uint8_t value = 0u;

            status =
                vf2_model2a_read(machine, source_start + index, &value, sizeof(value));
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, destination_start + index * UINT32_C(4), (uint32_t)value);
            }
        }
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] =
            destination_start + row_count * UINT32_C(128) * UINT32_C(4);
        cpu->registers[VF2_I960_G0_REGISTER + 1u] =
            source_start + row_count * UINT32_C(128);
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip != UINT32_C(0x000098b8)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_LUMA_TABLE_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_LUMA_TABLE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_early_wait_entry(vf2_i960_cpu *cpu,
                                   vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    uint32_t return_address = 0u;
    vf2_status status = VF2_OK;

    if (cpu == NULL || report == NULL ||
        (cpu->ip != VF2_NATIVE_POST_BOOT_LUMA_WAIT_ENTRY &&
         cpu->ip != VF2_NATIVE_POST_BOOT_PATTERN_WAIT_ENTRY &&
         cpu->ip != VF2_NATIVE_POST_BOOT_FINAL_WAIT_ENTRY &&
         cpu->ip != VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_ENTRY)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    if (cpu->ip == VF2_NATIVE_POST_BOOT_LUMA_WAIT_ENTRY) {
        return_address = VF2_NATIVE_POST_BOOT_LUMA_WAIT_EXIT;
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_ENTRY) {
        return_address = VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT;
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_FINAL_WAIT_ENTRY) {
        return_address = VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT;
    } else {
        return_address = VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT;
    }
    status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00000f7c), return_address);
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(1);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    return VF2_OK;
}

static uint32_t rotate_left_u32(uint32_t value, uint32_t count) {
    const uint32_t shift = count & UINT32_C(31);
    return shift == 0u ? value : (value << shift) | (value >> (UINT32_C(32) - shift));
}

static vf2_status
execute_post_boot_geometry_pattern_return(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                          vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u) ||
        cpu->registers[6] != UINT32_C(0x2000) ||
        cpu->registers[8] != 0u || cpu->registers[9] != UINT32_C(1) ||
        cpu->registers[VF2_I960_G0_REGISTER + 1u] != UINT32_C(0x00011d88) ||
        cpu->registers[VF2_I960_G0_REGISTER + 2u] != UINT32_C(0x00000162) ||
        cpu->registers[VF2_I960_G0_REGISTER + 3u] != UINT32_C(0x00000080) ||
        cpu->registers[VF2_I960_G0_REGISTER + 4u] != UINT32_C(0x59414c50) ||
        cpu->registers[VF2_I960_G0_REGISTER + 5u] != UINT32_C(0x000000b2) ||
        cpu->registers[VF2_I960_G0_REGISTER + 6u] != UINT32_C(16)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[9] = 0u;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_FINAL_WAIT_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(3);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN_RETURN;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(3);
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_geometry_pattern(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                   vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t geometry_base = cpu->registers[VF2_I960_G0_REGISTER + 10u];
    const uint32_t geometry_port =
        geometry_base + cpu->registers[VF2_I960_G0_REGISTER + 12u];
    const int continuation = cpu->ip == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT;
    uint32_t source =
        continuation ? cpu->registers[VF2_I960_G0_REGISTER + 1u] : UINT32_C(0x00011d64);
    uint32_t run_length =
        continuation ? cpu->registers[VF2_I960_G0_REGISTER + 2u] : UINT32_C(0x000000b1);
    uint32_t output_value = 0u;
    uint32_t source_word =
        continuation ? cpu->registers[VF2_I960_G0_REGISTER + 4u] : 0u;
    uint32_t run_remaining =
        continuation ? cpu->registers[VF2_I960_G0_REGISTER + 5u] : UINT32_C(0x0000005a);
    uint32_t symbols_remaining =
        continuation ? cpu->registers[VF2_I960_G0_REGISTER + 6u] : UINT32_C(16);
    uint32_t output_index = 0u;
    uint64_t instructions = continuation ? UINT64_C(10) : UINT64_C(18);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        (cpu->ip != VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_ENTRY &&
         cpu->ip != VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (geometry_base != VF2_GEOMETRY_BASE ||
        geometry_port != VF2_GEOMETRY_BASE + UINT32_C(0x4000)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (continuation) {
        output_value = cpu->registers[VF2_I960_G0_REGISTER + 3u];
        if ((cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u) ||
            cpu->registers[8] != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (cpu->registers[9] == UINT32_C(4)) {
            if (source != UINT32_C(0x00011d70) || run_length != UINT32_C(0x000000de) ||
                output_value != UINT32_C(0x00000029) ||
                source_word != UINT32_C(0x00003228) ||
                run_remaining != UINT32_C(0x00000043) ||
                symbols_remaining != UINT32_C(7) ||
                cpu->registers[6] != UINT32_C(0x800)) {
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (cpu->registers[9] == UINT32_C(3)) {
            if (source != UINT32_C(0x00011d78) || run_length != UINT32_C(0x0000010a) ||
                output_value != UINT32_C(0x0000004b) ||
                source_word != UINT32_C(0x00000266) ||
                run_remaining != UINT32_C(0x000000a8) ||
                symbols_remaining != UINT32_C(5) ||
                cpu->registers[6] != UINT32_C(0x1000)) {
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (cpu->registers[9] == UINT32_C(2)) {
            if (source != UINT32_C(0x00011d80) || run_length != UINT32_C(0x00000136) ||
                output_value != UINT32_C(0x00000067) ||
                source_word != UINT32_C(0x0001a99d) ||
                run_remaining != UINT32_C(0x00000033) ||
                symbols_remaining != UINT32_C(9) ||
                cpu->registers[6] != UINT32_C(0x1800)) {
                return VF2_ERROR_UNSUPPORTED;
            }
        } else {
            return VF2_ERROR_UNSUPPORTED;
        }
        cpu->registers[7] = cpu->registers[6] << 2u;
        cpu->registers[6] += UINT32_C(0x800);
        --cpu->registers[9];
    } else {
        status = vf2_model2a_read_u32(machine, source, &source_word);
        if (status == VF2_OK) {
            source += UINT32_C(4);
            status = vf2_i960_cpu_enter_procedure(
                cpu, VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_BODY, UINT32_C(0x000098c0));
        }
        if (status != VF2_OK) {
            return status;
        }
        cpu->registers[3] = 0u;
        cpu->registers[6] = UINT32_C(0x800);
        cpu->registers[7] = 0u;
        cpu->registers[8] = UINT32_C(0x800);
        cpu->registers[9] = UINT32_C(4);
    }
    cpu->registers[3] = 0u;
    cpu->registers[8] = UINT32_C(0x800);
    status = vf2_model2a_write_u32(machine, geometry_base + UINT32_C(0x140), 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, geometry_port, cpu->registers[7]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, geometry_port, UINT32_C(0x800));
    }

    for (output_index = 0u; status == VF2_OK && output_index < UINT32_C(0x800);
         ++output_index) {
        uint32_t packed = 0u;
        uint32_t byte_index = 0u;

        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_HELPER, UINT32_C(0x0001178c));
        for (byte_index = 0u; status == VF2_OK && byte_index < 4u; ++byte_index) {
            const uint32_t previous_remaining = run_remaining;
            run_remaining -= UINT32_C(1);
            if (previous_remaining <= UINT32_C(1)) {
                const uint32_t symbol = source_word & UINT32_C(3);
                const uint32_t previous_symbols = symbols_remaining;
                run_length += symbol;
                run_remaining = run_length;
                ++output_value;
                source_word >>= 2u;
                symbols_remaining -= UINT32_C(1);
                instructions += UINT64_C(7);
                if (previous_symbols <= UINT32_C(1)) {
                    symbols_remaining = UINT32_C(16);
                    status = vf2_model2a_read_u32(machine, source, &source_word);
                    source += UINT32_C(4);
                    instructions += UINT64_C(3);
                }
            }
            packed |= output_value;
            packed = rotate_left_u32(packed, UINT32_C(24));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, geometry_port, packed);
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
        if (status == VF2_OK) {
            --cpu->registers[8];
            instructions += UINT64_C(31);
        }
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = source;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = run_length;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = output_value;
        cpu->registers[VF2_I960_G0_REGISTER + 4u] = source_word;
        cpu->registers[VF2_I960_G0_REGISTER + 5u] = run_remaining;
        cpu->registers[VF2_I960_G0_REGISTER + 6u] = symbols_remaining;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00002edc),
                                              UINT32_C(0x00011798));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + instructions;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_geometry_table_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t geometry_base = cpu->registers[VF2_I960_G0_REGISTER + 10u];
    const uint32_t geometry_port =
        geometry_base + cpu->registers[VF2_I960_G0_REGISTER + 12u];
    uint32_t command_source = UINT32_C(0x000118e8);
    uint32_t value_source = UINT32_C(0x00011868);
    uint32_t command = 0u;
    uint32_t count = 0u;
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) ||
        geometry_base != VF2_GEOMETRY_BASE ||
        geometry_port != VF2_GEOMETRY_BASE + UINT32_C(0x4000)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_BODY,
                                          UINT32_C(0x000098c8));
    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        cpu->registers[VF2_I960_G0_REGISTER] = index == 0u ? 3u : 1u;
        status = vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00007b18),
                                              index == 0u ? UINT32_C(0x00011800)
                                                          : UINT32_C(0x00011808));
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, geometry_base + UINT32_C(0x70), 0u);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, geometry_port,
                                           cpu->registers[VF2_I960_G0_REGISTER]);
        }
        if (status == VF2_OK) {
            status = vf2_i960_cpu_return_procedure(cpu, machine);
        }
    }
    if (status == VF2_OK) {
        cpu->registers[3] = 0u;
        status = vf2_model2a_write_u32(machine, geometry_base + UINT32_C(0x60), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, command_source, &command);
        command_source += UINT32_C(4);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, command_source, &count);
        command_source += UINT32_C(4);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (count != UINT32_C(32)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_write_u32(machine, geometry_port, command);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, geometry_port, count);
    }
    for (index = 0u; status == VF2_OK && index < count; ++index) {
        status = vf2_model2a_read_u32(machine, command_source, &command);
        if (status == VF2_OK) {
            command_source += UINT32_C(4);
            status = vf2_model2a_write_u32(machine, geometry_port, command);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, value_source, &cpu->registers[3]);
        }
        if (status == VF2_OK) {
            value_source += UINT32_C(4);
            status = vf2_model2a_write_u32(machine, geometry_port, cpu->registers[3]);
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[4] = command_source;
    cpu->registers[5] = 0u;
    cpu->registers[6] = value_source;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status =
        vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x00002edc), UINT32_C(0x00011860));
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(281);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(281);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_geometry_table_return(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u) ||
        cpu->registers[4] != UINT32_C(0x00011970) ||
        cpu->registers[5] != 0u || cpu->registers[6] != UINT32_C(0x000118e8)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098c8)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions = start_instructions + UINT64_C(1);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_RETURN;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_graphics_state_reset(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                       vf2_native_runtime_step_report *report) {
    static const uint32_t halfword_addresses[] = {
        UINT32_C(0x005502a8), UINT32_C(0x005502b0), UINT32_C(0x005502b8)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GRAPHICS_STATE_RESET_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_GRAPHICS_STATE_RESET_BODY, UINT32_C(0x000098cc));
    cpu->registers[15] = 0u;
    for (index = 0u; status == VF2_OK &&
                     index < sizeof(halfword_addresses) / sizeof(halfword_addresses[0]);
         ++index) {
        status = write_u16_le(machine, halfword_addresses[index], 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00546000), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098cc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(10);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_STATE_RESET;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(10);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_video_constants(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                  vf2_native_runtime_step_report *report) {
    static const uint32_t source = UINT32_C(0x0006e2b4);
    static const uint32_t destination = UINT32_C(0x00501500);
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t values[6];
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_VIDEO_CONSTANTS_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; status == VF2_OK && index < sizeof(values) / sizeof(values[0]);
         ++index) {
        status = vf2_model2a_read_u32(machine, source + (uint32_t)index * UINT32_C(4),
                                      &values[index]);
    }
    if (status != VF2_OK) {
        return status;
    }
    status =
        vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_VIDEO_CONSTANTS_BODY,
                                     VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_ENTRY);
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER] = source;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = destination;
    for (index = 0u; index < sizeof(values) / sizeof(values[0]); ++index) {
        cpu->registers[3u + index] = values[index];
    }
    for (index = 0u; status == VF2_OK && index < sizeof(values) / sizeof(values[0]);
         ++index) {
        status = vf2_model2a_write_u32(
            machine, destination + (uint32_t)index * UINT32_C(4), values[index]);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(16);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_CONSTANTS;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(16);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_display_constants(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    static const uint32_t source = UINT32_C(0x00007f64);
    static const uint32_t destination = UINT32_C(0x00501400);
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t values[6];
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; status == VF2_OK && index < sizeof(values) / sizeof(values[0]);
         ++index) {
        status = vf2_model2a_read_u32(machine, source + (uint32_t)index * UINT32_C(4),
                                      &values[index]);
    }
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_BODY, UINT32_C(0x000098d4));
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[10] = source;
    cpu->registers[11] = destination;
    for (index = 0u; index < 3u; ++index) {
        cpu->registers[4u + index] = values[3u + index];
    }
    for (index = 0u; status == VF2_OK && index < sizeof(values) / sizeof(values[0]);
         ++index) {
        status = vf2_model2a_write_u32(
            machine, destination + (uint32_t)index * UINT32_C(4), values[index]);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098d4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(8);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_CONSTANTS;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(8);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_task_registry_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    vf2_task_descriptor tasks[VF2_RECOVERED_SCHEDULER_MAX_TASKS];
    vf2_task_catalog catalog;
    vf2_recovered_task_registry_report registry_report;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t task_count = 0u;
    uint32_t registry_end = UINT32_C(0x00510000);
    uint64_t instructions = UINT64_C(10);
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_TASK_REGISTRY_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(tasks, 0, sizeof(tasks));
    memset(&catalog, 0, sizeof(catalog));
    memset(&registry_report, 0, sizeof(registry_report));
    status = vf2_model2a_read_u32(machine, VF2_NATIVE_TASK_COUNT_ADDRESS, &task_count);
    if (status != VF2_OK) {
        return status;
    }
    if (task_count != UINT32_C(29) || task_count > VF2_RECOVERED_SCHEDULER_MAX_TASKS) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (index = 0u; status == VF2_OK && index < task_count; ++index) {
        const uint32_t descriptor =
            VF2_NATIVE_TASK_DESCRIPTOR_TABLE + (uint32_t)index * UINT32_C(0x40);
        vf2_task_descriptor *task = &tasks[index];

        task->descriptor_address = descriptor;
        status = vf2_model2a_read_u32(machine, descriptor, &task->flags);
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(4),
                                          &task->instance);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(8),
                                          &task->stack_size);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(0x0c),
                                          &task->entry_point);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(0x10),
                                          &task->state_address);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, descriptor + UINT32_C(0x14),
                                          &task->scheduler_slot);
        }
        if (status != VF2_OK) {
            return status;
        }
        if (task->stack_size == 0u || task->stack_size > VF2_WORK_RAM_SIZE ||
            registry_end > VF2_WORK_RAM_BASE + VF2_WORK_RAM_SIZE - task->stack_size ||
            task->state_address < VF2_WORK_RAM_BASE ||
            task->state_address > VF2_WORK_RAM_BASE + VF2_WORK_RAM_SIZE - UINT32_C(4)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        registry_end += task->stack_size;
        instructions += UINT64_C(22);
        if (task->scheduler_slot != 0u) {
            instructions += UINT64_C(2);
        }
    }

    catalog.tasks = tasks;
    catalog.count = task_count;
    catalog.capacity = task_count;
    catalog.table_start = VF2_NATIVE_TASK_DESCRIPTOR_TABLE;
    catalog.table_end = VF2_NATIVE_TASK_DESCRIPTOR_TABLE + task_count * UINT32_C(0x40);
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_TASK_REGISTRY_BODY,
                                          UINT32_C(0x000098d8));
    if (status == VF2_OK) {
        status =
            vf2_recovered_task_registry_initialize(machine, &catalog, &registry_report);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = tasks[task_count - 1u].stack_size;
    cpu->registers[VF2_I960_G0_REGISTER + 13u] = registry_report.registry_end;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098d8) ||
        registry_report.registry_end != registry_end) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + instructions;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TASK_REGISTRY_INIT;
    report->exit_address = cpu->ip;
    report->descriptors_scanned = task_count;
    report->recovered_instruction_count = instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_graphics_buffer_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                       vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GRAPHICS_BUFFER_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_GRAPHICS_BUFFER_BODY, UINT32_C(0x000098dc));
    if (status == VF2_OK) {
        cpu->registers[15] = UINT32_C(0x005d0000);
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x00500600), cpu->registers[15]);
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 0u;
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x00500608), cpu->registers[15]);
    }
    if (status == VF2_OK) {
        cpu->registers[15] = 1u;
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x00500604), cpu->registers[15]);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098dc)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(8);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_BUFFER_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(8);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_render_state_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    static const uint32_t zero_quad[4] = {0u, 0u, 0u, 0u};
    static const uint32_t zero_addresses[] = {
        UINT32_C(0x0059e014), UINT32_C(0x0059e018), UINT32_C(0x0059e000),
        UINT32_C(0x0059e004), UINT32_C(0x0059e008), UINT32_C(0x0059e00c),
        UINT32_C(0x0059e010)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_RENDER_STATE_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_RENDER_STATE_BODY,
                                          UINT32_C(0x000098e0));
    for (index = 0u;
         status == VF2_OK && index < sizeof(zero_addresses) / sizeof(zero_addresses[0]);
         ++index) {
        status = vf2_model2a_write_u32(machine, zero_addresses[index], 0u);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x0059f278), UINT32_C(0x0a000000));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_RENDER_TABLE_CLEAR, UINT32_C(0x0004e804));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0059e008), UINT32_C(2));
    }
    for (index = 0u; status == VF2_OK && index < 216u; ++index) {
        status = vf2_model2a_write(
            machine, UINT32_C(0x0059f280) + (uint32_t)index * UINT32_C(16), zero_quad,
            sizeof(zero_quad));
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098e0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(672);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RENDER_STATE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(672);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_game_defaults_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    static const struct {
        uint32_t address;
        uint32_t value;
    } words[] = {{UINT32_C(0x0050a0bc), UINT32_C(0x3f333333)},
                 {UINT32_C(0x0050a0c0), UINT32_C(0x3ecccccd)},
                 {UINT32_C(0x0050a0c4), UINT32_C(0x3ecccccd)},
                 {UINT32_C(0x0050a0cc), UINT32_C(0x3ecccccd)},
                 {UINT32_C(0x00501098), UINT32_C(0x3f800000)},
                 {UINT32_C(0x005010a4), UINT32_C(0x3ecccccd)},
                 {UINT32_C(0x005010a8), UINT32_C(0x3e4ccccd)},
                 {UINT32_C(0x005010e4), UINT32_C(0x3e19999a)},
                 {UINT32_C(0x0050a028), UINT32_C(0x3f19999a)},
                 {UINT32_C(0x0050a150), UINT32_C(0x3f800000)},
                 {UINT32_C(0x0050a158), UINT32_C(0x3f666666)},
                 {UINT32_C(0x0050a174), UINT32_C(0x3ecccccd)},
                 {UINT32_C(0x0050a178), UINT32_C(0x3f266666)},
                 {UINT32_C(0x0059e04c), UINT32_C(0x000001f4)},
                 {UINT32_C(0x0059e040), UINT32_C(0)}};
    static const struct {
        uint32_t address;
        uint16_t value;
    } halfwords[] = {{UINT32_C(0x0050a0c8), UINT16_C(0x1000)},
                     {UINT32_C(0x005010ec), UINT16_C(0x2000)},
                     {UINT32_C(0x005010de), UINT16_C(0)}};
    static const struct {
        uint32_t address;
        uint8_t value;
    } bytes[] = {
        {UINT32_C(0x0050a0b7), UINT8_C(10)},   {UINT32_C(0x0050a0ba), UINT8_C(6)},
        {UINT32_C(0x0050a0bb), UINT8_C(12)},   {UINT32_C(0x0050009d), UINT8_C(1)},
        {UINT32_C(0x0050a14c), UINT8_C(22)},   {UINT32_C(0x005010dc), UINT8_C(0x81)},
        {UINT32_C(0x005010dd), UINT8_C(0xff)}, {UINT32_C(0x0050d006), UINT8_C(9)}};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t table[UINT32_C(40) * sizeof(uint32_t)];
    uint8_t tail[UINT32_C(14)];
    uint32_t first_state = 0u;
    uint32_t second_state = 0u;
    uint32_t game_state = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GAME_DEFAULTS_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &first_state);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &second_state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &game_state);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00023ca0), table, sizeof(table));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00023d40), tail, sizeof(tail));
    }
    if (status != VF2_OK) {
        return status;
    }
    if (first_state < VF2_WORK_RAM_BASE ||
        first_state > VF2_WORK_RAM_BASE + VF2_WORK_RAM_SIZE - UINT32_C(4) ||
        second_state < VF2_WORK_RAM_BASE ||
        second_state > VF2_WORK_RAM_BASE + VF2_WORK_RAM_SIZE - UINT32_C(4) ||
        game_state < VF2_WORK_RAM_BASE ||
        game_state > VF2_WORK_RAM_BASE + VF2_WORK_RAM_SIZE - UINT32_C(0x2d2)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_GAME_DEFAULTS_BODY,
                                          UINT32_C(0x000098e4));
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_GAME_TABLE_INIT,
                                              UINT32_C(0x00044088));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050a800), table, sizeof(table));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050a0a8), tail, sizeof(tail));
    }
    if (status == VF2_OK) {
        const uint8_t one = UINT8_C(1);
        status = vf2_model2a_write(machine, UINT32_C(0x0050a16e), &one, sizeof(one));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x0050a14e), UINT16_C(0x0580));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x0050a170), UINT16_C(0x1000));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }

    for (index = 0u; status == VF2_OK && index < sizeof(bytes) / sizeof(bytes[0]);
         ++index) {
        status = vf2_model2a_write(machine, bytes[index].address, &bytes[index].value,
                                   sizeof(bytes[index].value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, first_state, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, second_state, 0u);
    }
    for (index = 0u; status == VF2_OK && index < sizeof(words) / sizeof(words[0]);
         ++index) {
        status =
            vf2_model2a_write_u32(machine, words[index].address, words[index].value);
    }
    for (index = 0u;
         status == VF2_OK && index < sizeof(halfwords) / sizeof(halfwords[0]);
         ++index) {
        status =
            write_u16_le(machine, halfwords[index].address, halfwords[index].value);
    }

    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_GAME_FLOAT_INIT,
                                              UINT32_C(0x00044220));
    }
    for (index = 0u; status == VF2_OK && index < 26u; ++index) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a0e0) + (uint32_t)index * UINT32_C(4),
            UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x0050a148), UINT32_C(0xbe99999a));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, game_state + UINT32_C(0x20c),
                                       UINT32_C(0x3fb33333));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, game_state + UINT32_C(0x290),
                                       UINT32_C(0x3eb33333));
    }
    if (status == VF2_OK) {
        const uint8_t value = UINT8_C(60);
        status = vf2_model2a_write(machine, game_state + UINT32_C(0x2d1), &value,
                                   sizeof(value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, game_state + UINT32_C(0x4c),
                                       UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098e4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(442);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DEFAULTS_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(442);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_object_table_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    static const uint32_t destination = UINT32_C(0x00560000);
    static const size_t copy_size = 2817u * 16u;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t buffer[256];
    uint32_t source = 0u;
    size_t offset = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_OBJECT_TABLE_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0201fe68), &source);
    if (status == VF2_OK && source > UINT32_MAX - (uint32_t)copy_size + UINT32_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        uint8_t last = 0u;
        status = vf2_model2a_read(machine, source + (uint32_t)copy_size - UINT32_C(1),
                                  &last, sizeof(last));
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_OBJECT_TABLE_BODY,
                                          UINT32_C(0x000098e8));
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = source;
    cpu->registers[VF2_I960_G0_REGISTER + 4u] = destination;
    while (status == VF2_OK && offset < copy_size) {
        const size_t remaining = copy_size - offset;
        const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        status = vf2_model2a_read(machine, source + (uint32_t)offset, buffer, chunk);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, destination + (uint32_t)offset, buffer,
                                       chunk);
        }
        offset += chunk;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005001a0), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005001b0), 0u);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x005001a4), UINT32_C(0x7f7f7f7f));
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x005001b4), UINT32_C(0x7f7f7f7f));
    }
    if (status == VF2_OK) {
        static const uint32_t zeros[2] = {0u, 0u};
        status = vf2_model2a_write(machine, UINT32_C(0x005001a8), zeros, sizeof(zeros));
        if (status == VF2_OK) {
            status =
                vf2_model2a_write(machine, UINT32_C(0x005001b8), zeros, sizeof(zeros));
        }
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098e8)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(11285);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_OBJECT_TABLE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(11285);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_effect_table_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    static const uint32_t zero_quad[4] = {0u, 0u, 0u, 0u};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t buffer[256];
    size_t offset = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_EFFECT_TABLE_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_EFFECT_TABLE_BODY,
                                          UINT32_C(0x000098ec));
    if (status == VF2_OK) {
        const uint8_t zero = 0u;
        status = vf2_model2a_write(machine, UINT32_C(0x005000ae), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_write_u32(machine, UINT32_C(0x00500168), UINT32_C(0x00531000));
    }
    while (status == VF2_OK && offset < UINT32_C(0x1000)) {
        const size_t remaining = UINT32_C(0x1000) - offset;
        const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        status = vf2_model2a_read(machine, UINT32_C(0x0007ae10) + (uint32_t)offset,
                                  buffer, chunk);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00531000) + (uint32_t)offset,
                                       buffer, chunk);
        }
        offset += chunk;
    }
    for (offset = 0u; status == VF2_OK && offset < UINT32_C(0x4000);
         offset += sizeof(zero_quad)) {
        status = vf2_model2a_write(machine, UINT32_C(0x00535000) + (uint32_t)offset,
                                   zero_quad, sizeof(zero_quad));
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098ec)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(5652);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EFFECT_TABLE_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(5652);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_input_ring_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                  vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint8_t initial_index = UINT8_C(2);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_INPUT_RING_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_INPUT_RING_BODY,
                                          UINT32_C(0x000098f0));
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005000fc), &initial_index,
                                   sizeof(initial_index));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005000fd), &initial_index,
                                   sizeof(initial_index));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098f0)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(6);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_RING_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(6);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status write_post_boot_diagnostic_text(vf2_model2a *machine,
                                                  vf2_i960_cpu *cpu, uint32_t source,
                                                  uint32_t destination, size_t length) {
    size_t index = 0u;
    vf2_status status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_TEXT_COPY, UINT32_C(0x00009450));

    for (index = 0u; status == VF2_OK && index < length; ++index) {
        uint8_t character = 0u;
        status = vf2_model2a_read(machine, source + (uint32_t)index, &character,
                                  sizeof(character));
        if (status == VF2_OK) {
            status = write_u16_le(machine, destination + (uint32_t)index * UINT32_C(2),
                                  (uint16_t)(UINT16_C(0x8000) | character));
        }
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    return status;
}

static vf2_status execute_post_boot_io_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                            vf2_native_runtime_step_report *report) {
    static const struct {
        uint32_t source;
        uint32_t destination;
        size_t length;
        uint32_t terminal_word;
    } messages[] = {
        {UINT32_C(0x00000fc4), UINT32_C(0x01000c28), 18u, UINT32_C(0x00002e2e)},
        {UINT32_C(0x00000ffc), UINT32_C(0x01000c4e), 1u, UINT32_C(0x0000004f)},
        {UINT32_C(0x00001024), UINT32_C(0x01000c50), 2u, UINT32_C(0x00002e4b)}};
    static const uint32_t cleared_words[] = {UINT32_C(0x00500704), UINT32_C(0x00500708),
                                             UINT32_C(0x00500700),
                                             UINT32_C(0x0050070c)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint8_t mode = 0u;
    uint8_t first_status = 0u;
    uint8_t second_status = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_IO_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500f00), &mode, sizeof(mode));
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x01c00042), &first_status,
                                  sizeof(first_status));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x01c00040), &second_status,
                                  sizeof(second_status));
    }
    if (status != VF2_OK) {
        return status;
    }
    if (mode != 0u || first_status != UINT8_C(0x40) ||
        (second_status & UINT8_C(3)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(messages) / sizeof(messages[0]); ++index) {
        size_t character = 0u;
        for (character = 0u; character <= messages[index].length; ++character) {
            uint8_t value = 0u;
            status =
                vf2_model2a_read(machine, messages[index].source + (uint32_t)character,
                                 &value, sizeof(value));
            if (status != VF2_OK) {
                return status;
            }
            if ((character < messages[index].length && value == 0u) ||
                (character == messages[index].length && value != 0u)) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_IO_BODY,
                                          UINT32_C(0x000098f4));
    for (index = 0u; status == VF2_OK && index < sizeof(messages) / sizeof(messages[0]);
         ++index) {
        cpu->registers[VF2_I960_G0_REGISTER + 9u] = messages[index].destination;
        cpu->registers[VF2_I960_G0_REGISTER] = messages[index].source;
        status = write_post_boot_diagnostic_text(machine, cpu, messages[index].source,
                                                 messages[index].destination,
                                                 messages[index].length);
        if (status == VF2_OK) {
            cpu->registers[VF2_I960_G0_REGISTER] = messages[index].terminal_word;
            cpu->registers[VF2_I960_G0_REGISTER + 9u] =
                messages[index].destination + UINT32_C(0x80);
        }
        if (status == VF2_OK && index == 0u) {
            const uint8_t ready = UINT8_C(1);
            status =
                vf2_model2a_write(machine, UINT32_C(0x01c00040), &ready, sizeof(ready));
        }
    }
    if (status == VF2_OK) {
        const uint8_t io_control = UINT8_C(0x4e);
        status = vf2_model2a_write(machine, UINT32_C(0x01c00010), &io_control,
                                   sizeof(io_control));
    }
    for (index = 0u;
         status == VF2_OK && index < sizeof(cleared_words) / sizeof(cleared_words[0]);
         ++index) {
        status = vf2_model2a_write_u32(machine, cleared_words[index], 0u);
    }
    if (status == VF2_OK) {
        const uint8_t zero = 0u;
        status = vf2_model2a_write(machine, UINT32_C(0x00500718), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x000098f4)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(272);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_IO_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(272);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_game_data_copy(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                 vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    uint8_t buffer[256];
    uint32_t offset = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GAME_DATA_COPY_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    while (status == VF2_OK && offset < UINT32_C(0x30000)) {
        const size_t remaining = UINT32_C(0x30000) - offset;
        const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        status =
            vf2_model2a_read(machine, UINT32_C(0x023d0000) + offset, buffer, chunk);
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x005a0000) + offset, buffer,
                                       chunk);
        }
        offset += (uint32_t)chunk;
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = UINT32_C(0x02400000);
    cpu->registers[4] = UINT32_C(0x005d0000);
    cpu->registers[5] = UINT32_C(0x005d0000);
    cpu->registers[8] = UINT32_MAX;
    cpu->registers[9] = UINT32_MAX;
    cpu->registers[10] = UINT32_MAX;
    cpu->registers[11] = UINT32_MAX;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = UINT32_C(0x00009920);
    cpu->executed_instructions = start_instructions + UINT64_C(61443);

    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DATA_COPY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(61443);
    return VF2_OK;
}

static vf2_status
execute_post_boot_display_offset_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    static const uint32_t cleared_addresses[] = {
        UINT32_C(0x01d03384), UINT32_C(0x0059c384), UINT32_C(0x01d0338c),
        UINT32_C(0x0059c38c)};
    static const uint32_t result_addresses[] = {
        UINT32_C(0x01d03380), UINT32_C(0x0059c380), UINT32_C(0x01d03388),
        UINT32_C(0x0059c388)};
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_hybrid_bridge_report color_reports[2];
    vf2_hybrid_bridge_report classify_report;
    uint32_t base = 0u;
    uint32_t flags = 0u;
    uint32_t saved_g0 = 0u;
    uint32_t positions[2] = {0u, 0u};
    uint32_t divisors[2] = {0u, 0u};
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_DISPLAY_OFFSET_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(color_reports, 0, sizeof(color_reports));
    memset(&classify_report, 0, sizeof(classify_report));

    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, base + UINT32_C(0x3320), &flags);
    }
    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        uint8_t position = 0u;
        status = vf2_model2a_read(
            machine, base + UINT32_C(0x3380) + (uint32_t)index * UINT32_C(8), &position,
            sizeof(position));
        positions[index] = position;
    }
    if (status != VF2_OK || (flags & UINT32_C(3)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    saved_g0 = cpu->registers[VF2_I960_G0_REGISTER];
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_DISPLAY_OFFSET_BODY,
                                          UINT32_C(0x00009924));
    if (status == VF2_OK) {
        cpu->registers[1] += UINT32_C(4);
        status =
            vf2_model2a_write_u32(machine, cpu->registers[1] - UINT32_C(4), saved_g0);
    }
    for (index = 0u; status == VF2_OK &&
                     index < sizeof(cleared_addresses) / sizeof(cleared_addresses[0]);
         ++index) {
        const uint8_t zero = 0u;
        status =
            vf2_model2a_write(machine, cleared_addresses[index], &zero, sizeof(zero));
    }

    for (index = 0u; status == VF2_OK && index < 2u; ++index) {
        cpu->registers[VF2_I960_G0_REGISTER] = (uint32_t)index;
        status = vf2_i960_cpu_enter_procedure(cpu, VF2_GAME_COLOR_LOOKUP_ENTRY,
                                              index == 0u ? UINT32_C(0x000024b8)
                                                          : UINT32_C(0x000024c8));
        if (status == VF2_OK) {
            status = execute_game_color_lookup(machine, cpu, &color_reports[index]);
        }
        if (status == VF2_OK) {
            divisors[index] = (cpu->registers[VF2_I960_G0_REGISTER] << 8u) >> 24u;
            if (divisors[index] == 0u) {
                status = VF2_ERROR_UNSUPPORTED;
            }
        }
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, VF2_GAME_STATE_CLASSIFY_ENTRY,
                                              UINT32_C(0x000024d4));
    }
    if (status == VF2_OK) {
        status = execute_game_state_classify(machine, cpu, &classify_report);
    }
    if (status != VF2_OK || cpu->registers[VF2_I960_G0_REGISTER] != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (index = 0u; index < 2u; ++index) {
        const uint8_t result =
            (uint8_t)(positions[index] - positions[index] % divisors[index]);
        status = vf2_model2a_write(machine, result_addresses[index * 2u], &result,
                                   sizeof(result));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, result_addresses[index * 2u + 1u],
                                       &result, sizeof(result));
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->registers[VF2_I960_G0_REGISTER] = saved_g0;
    cpu->registers[1] -= UINT32_C(4);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00009924)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_NONE;
    cpu->executed_instructions = start_instructions + UINT64_C(39) +
                                 color_reports[0].recovered_instruction_count +
                                 color_reports[1].recovered_instruction_count +
                                 classify_report.recovered_instruction_count;

    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_OFFSET_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_frame_accumulator_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                         vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t base = 0u;
    uint32_t mode = 0u;
    uint32_t fill = 0u;
    uint32_t sum = 0u;
    uint8_t factor = 0u;
    uint8_t shift = 0u;
    uint8_t selector = 0u;
    uint8_t level = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_FRAME_ACCUMULATOR_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3000), &factor,
                                  sizeof(factor));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050d006), &shift,
                                  sizeof(shift));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050002c), &mode);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3342), &selector,
                                  sizeof(selector));
    }
    if (status != VF2_OK || shift >= UINT8_C(27) ||
        (mode & UINT32_C(0x000fffff)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    fill = ((uint32_t)factor * (UINT32_C(1) << shift)) >> 8u;
    fill |= fill << 16u;
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_FRAME_ACCUMULATOR_BODY, UINT32_C(0x00009928));
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050d000), 0u);
    }
    if (status == VF2_OK) {
        const uint8_t zero = 0u;
        status = vf2_model2a_write(machine, UINT32_C(0x0050d004), &zero,
                                   sizeof(zero));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050d005), &zero,
                                       sizeof(zero));
        }
    }
    for (index = 0u; status == VF2_OK && index < 32u; ++index) {
        uint32_t words[4] = {fill, fill, fill, fill};
        status = vf2_model2a_write(machine, UINT32_C(0x0050d100) + (uint32_t)index * 16u,
                                   words, sizeof(words));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050d000), UINT32_C(1));
    }
    for (index = 0u; status == VF2_OK && index < 256u; ++index) {
        uint16_t sample = 0u;
        status = vf2_model2a_read(machine, UINT32_C(0x0050d100) + (uint32_t)index * 2u,
                                  &sample, sizeof(sample));
        sum += sample;
    }
    if (status != VF2_OK) {
        return status;
    }

    level = (uint8_t)(sum >> (shift + 5u));
    if (level > UINT8_C(7)) {
        level = UINT8_C(7);
    }
    {
        uint32_t intensity = sum >> shift;
        uint8_t output = 0u;
        const uint16_t zero = 0u;
        if (intensity > UINT32_C(0xff)) {
            intensity = UINT32_C(0xff);
        }
        output = (uint8_t)intensity;
        status = vf2_model2a_read(machine,
                                  UINT32_C(0x00011510) + (uint32_t)selector * 8u + level,
                                  &output, sizeof(output));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050d005), &output,
                                       sizeof(output));
        }
        output = (uint8_t)intensity;
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0050d004), &level,
                                       sizeof(level));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x01d03000), &output,
                                       sizeof(output));
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x0059c000), &output,
                                       sizeof(output));
        }
        if (status == VF2_OK) {
            status = write_u16_le(machine, UINT32_C(0x0050d100), zero);
        }
        cpu->registers[VF2_I960_G0_REGISTER] = intensity;
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x00009928)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->executed_instructions = start_instructions + UINT64_C(1178);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FRAME_ACCUMULATOR_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(1178);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_profile_defaults_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t base = 0u;
    uint32_t words[2] = {0u, 0u};
    uint16_t halfwords[2] = {0u, 0u};
    uint32_t selector_word = 0u;
    uint16_t selector_halfwords[2] = {0u, 0u};
    uint16_t overrides[2] = {0u, 0u};
    uint8_t level = 0u;
    uint8_t selector = 0u;
    int8_t override = 0;
    int8_t signed_level = 0;
    uint32_t override_word = 0u;
    float override_float = 0.0f;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_PROFILE_DEFAULTS_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &base);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050d005), &level,
                                  sizeof(level));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3342), &selector,
                                  sizeof(selector));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x334f), &override,
                                  sizeof(override));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050d004), &signed_level,
                                  sizeof(signed_level));
    }
    if (status != VF2_OK || level > UINT8_C(3) || selector > UINT8_C(3)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    status = vf2_model2a_read_u32(machine,
                                  UINT32_C(0x00011530) + (uint32_t)level * 4u,
                                  &words[0]);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine,
                                      UINT32_C(0x00011540) + (uint32_t)level * 4u,
                                      &words[1]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine,
                                  UINT32_C(0x00011550) + (uint32_t)level * 2u,
                                  &halfwords[0], sizeof(halfwords[0]));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine,
                                  UINT32_C(0x00011558) + (uint32_t)level * 2u,
                                  &halfwords[1], sizeof(halfwords[1]));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine,
                                      UINT32_C(0x00011530) + (uint32_t)selector * 4u,
                                      &selector_word);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine,
                                  UINT32_C(0x00011550) + (uint32_t)selector * 2u,
                                  &selector_halfwords[0], sizeof(selector_halfwords[0]));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine,
                                  UINT32_C(0x00011558) + (uint32_t)selector * 2u,
                                  &selector_halfwords[1], sizeof(selector_halfwords[1]));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3354), &overrides[0],
                                  sizeof(overrides[0]));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, base + UINT32_C(0x3352), &overrides[1],
                                  sizeof(overrides[1]));
    }
    if (status != VF2_OK) {
        return status;
    }

    override_float = (float)override * 0.5f;
    memcpy(&override_word, &override_float, sizeof(override_word));
    if (override_word != selector_word) {
        words[0] = override_word;
        words[1] = override_word;
    }
    if (overrides[0] != selector_halfwords[0]) {
        halfwords[0] = overrides[0];
    }
    if (overrides[1] != selector_halfwords[1]) {
        halfwords[1] = overrides[1];
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_PROFILE_DEFAULTS_BODY, UINT32_C(0x0000992c));
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050d007), &signed_level,
                                   sizeof(signed_level));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a700), words[0]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a704), words[1]);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x0050a708), halfwords[0]);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x0050a70a), halfwords[1]);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000992c)) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    {
        const uint64_t recovered = override == 0 ? UINT64_C(32) : UINT64_C(37);
        cpu->executed_instructions = start_instructions + recovered;
        report->recovered_instruction_count = recovered;
    }
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PROFILE_DEFAULTS_INIT;
    report->exit_address = cpu->ip;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_gameplay_globals_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_step_report *report) {
    static const struct {
        uint32_t address;
        uint32_t value;
    } words[] = {{UINT32_C(0x00501018), UINT32_C(0x00001388)},
                 {UINT32_C(0x005013f0), UINT32_C(0x00000080)},
                 {UINT32_C(0x00508000), UINT32_C(0)},
                 {UINT32_C(0x00500018), UINT32_C(0)},
                 {UINT32_C(0x00500164), UINT32_C(0)},
                 {UINT32_C(0x00508060), UINT32_C(0)},
                 {UINT32_C(0x0050a000), UINT32_C(0x3b32674f)},
                 {UINT32_C(0x0050a004), UINT32_C(0x3f800000)},
                 {UINT32_C(0x0050a008), UINT32_C(0x41200000)},
                 {UINT32_C(0x0050a010), UINT32_C(0xbf000000)},
                 {UINT32_C(0x00500020), UINT32_C(0)}};
    const uint64_t start_instructions = cpu->executed_instructions;
    uint32_t profile_word = 0u;
    uint16_t profile_halfword = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x0050a700), &profile_word);
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050a708), &profile_halfword,
                                  sizeof(profile_halfword));
    }
    for (index = 0u; status == VF2_OK && index < sizeof(words) / sizeof(words[0]);
         ++index) {
        status = vf2_model2a_write_u32(machine, words[index].address, words[index].value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a00c), profile_word);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x005000a0), profile_halfword);
    }
    if (status == VF2_OK) {
        const uint8_t zero = 0u;
        status = vf2_model2a_write(machine, UINT32_C(0x0050002a), &zero,
                                   sizeof(zero));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, UINT32_C(0x00500064), &zero,
                                       sizeof(zero));
        }
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[15] = 0u;
    cpu->ip = VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_EXIT;
    cpu->executed_instructions = start_instructions + UINT64_C(30);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAMEPLAY_GLOBALS_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(30);
    return VF2_OK;
}

static vf2_status
execute_post_boot_input_profile_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint64_t recovered_instructions = UINT64_C(1); /* call 0x1fcc0 */
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t flags = 0u;
    uint32_t profile_a = UINT32_C(0x3b32674f);
    uint32_t profile_b = UINT32_C(0x3f800000);
    uint8_t fighter0_mode = 0u;
    uint8_t fighter1_mode = 0u;
    uint8_t input_mode = 0u;
    uint8_t mode_control = 0u;
    uint8_t r14_value = 0u;
    /* -1 while only ordinal compare operands are known; otherwise the
     * compare_result the block must leave in the CPU. */
    int32_t condition = -1;
    uint32_t compare_left = 0u;
    uint32_t compare_right = 0u;
    bool force_mode12 = false;
    bool special_mode10 = false;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_EXIT) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &fighter1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, fighter0 + UINT32_C(0x1b1), &fighter0_mode,
                                  sizeof(fighter0_mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, fighter1 + UINT32_C(0x1b1), &fighter1_mode,
                                  sizeof(fighter1_mode));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500064), &input_mode,
                                  sizeof(input_mode));
    }
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_INPUT_PROFILE_BODY, UINT32_C(0x00009a00));
    if (status != VF2_OK) {
        return status;
    }

    /* 0x1fcc0..0x1fcec: the two asymmetric fighter-mode pairs select mode 12.
     * The ordinal branch compares update the arithmetic condition; track the
     * last one executed so the block exit reproduces the ROM poststate. */
    recovered_instructions += UINT64_C(4); /* ld/ld/ldob/cmpobne */
    compare_left = 2u;
    compare_right = fighter0_mode;
    r14_value = fighter0_mode;
    if (fighter0_mode == UINT8_C(2)) {
        recovered_instructions += UINT64_C(2); /* ldob/cmpobe */
        compare_left = 1u;
        compare_right = fighter1_mode;
        r14_value = fighter1_mode;
        force_mode12 = fighter1_mode == UINT8_C(1);
    }
    if (!force_mode12) {
        recovered_instructions += UINT64_C(2); /* ldob/cmpobne */
        compare_left = 1u;
        compare_right = fighter0_mode;
        r14_value = fighter0_mode;
        if (fighter0_mode == UINT8_C(1)) {
            recovered_instructions += UINT64_C(2); /* ldob/cmpobne */
            compare_left = 2u;
            compare_right = fighter1_mode;
            r14_value = fighter1_mode;
            force_mode12 = fighter1_mode == UINT8_C(2);
        }
    }
    if (force_mode12) {
        input_mode = UINT8_C(12);
        flags &= ~UINT32_C(0x00100000);
        recovered_instructions += UINT64_C(5); /* lda/stib/ld/clrbit/st */
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &input_mode,
                                   sizeof(input_mode));
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
        }
        if (status != VF2_OK) {
            return status;
        }
    }

    /* 0x1fd14..0x1fd98: bit 21/20 and an existing mode 10 choose the
     * special mode-10 constants; control byte 2 converts mode 10 to mode 11.
     * The bit tests themselves set the arithmetic condition: a set tested
     * bit leaves EQUAL and a clear bit leaves NONE. */
    recovered_instructions += UINT64_C(2); /* ld flags / bbc bit 21 */
    if ((flags & UINT32_C(0x00200000)) != 0u) {
        recovered_instructions += UINT64_C(2); /* ld flags / bbs bit 20 */
        special_mode10 = (flags & UINT32_C(0x00100000)) != 0u;
        condition = special_mode10 ? 2 : 0;
    } else {
        condition = 0;
    }
    if (!special_mode10) {
        recovered_instructions += UINT64_C(2); /* ldob mode / cmpobne 10 */
        compare_left = 10u;
        compare_right = input_mode;
        condition = -1;
        if (input_mode == UINT8_C(10)) {
            status = vf2_model2a_read(machine, UINT32_C(0x0050004c), &mode_control,
                                      sizeof(mode_control));
            if (status != VF2_OK) {
                return status;
            }
            r14_value = mode_control;
            recovered_instructions += UINT64_C(2); /* ldob / cmpobe 2 */
            compare_left = 2u;
            compare_right = mode_control;
            if (mode_control == UINT8_C(2)) {
                input_mode = UINT8_C(11);
                recovered_instructions += UINT64_C(2); /* lda/stib */
                status = vf2_model2a_write(machine, UINT32_C(0x00500064), &input_mode,
                                           sizeof(input_mode));
                if (status != VF2_OK) {
                    return status;
                }
            } else {
                special_mode10 = true;
            }
        }
    }

    if (special_mode10) {
        input_mode = UINT8_C(10);
        flags |= UINT32_C(0x00100000);
        profile_a = UINT32_C(0x3a3117c4);
        profile_b = UINT32_C(0x40000000);
        recovered_instructions += UINT64_C(10); /* mode/flag/constants plus branch */
        status = vf2_model2a_write(machine, UINT32_C(0x00500064), &input_mode,
                                   sizeof(input_mode));
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
        }
    } else {
        flags &= ~UINT32_C(0x00100000);
        recovered_instructions += UINT64_C(7); /* flag clear and normal constants */
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a000), profile_a);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a004), profile_b);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[3] = fighter0;
    cpu->registers[4] = fighter1;
    cpu->registers[14] = r14_value;
    cpu->registers[15] = profile_b;
    /* Reproduce the ROM condition left by the last compare or bit test on
     * the taken path; the trailing lda/st instructions do not modify it. */
    if (condition < 0) {
        condition = compare_right < compare_left ? 3
                  : compare_right > compare_left ? 1 : 2;
    }
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) |
        (uint32_t)(condition == 3 ? 1 : condition == 1 ? 4 : condition == 2 ? 2 : 0);
    cpu->compare_result = condition == 3 ? VF2_I960_COMPARE_GREATER
                        : condition == 1 ? VF2_I960_COMPARE_LESS
                        : condition == 2 ? VF2_I960_COMPARE_EQUAL
                                         : VF2_I960_COMPARE_NONE;
    cpu->ip = VF2_NATIVE_POST_BOOT_FLOAT_DEFAULTS_ENTRY;
    cpu->executed_instructions = start_instructions + recovered_instructions;

    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = recovered_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_float_defaults_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint64_t recovered_instructions = UINT64_C(114);
    uint8_t input_mode = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_FLOAT_DEFAULTS_ENTRY ||
        (cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, UINT32_C(0x00500064), &input_mode,
                              sizeof(input_mode));
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_FLOAT_DEFAULTS_BODY, UINT32_C(0x0001fdd4));
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_FLOAT_FILL_BODY,
                                              UINT32_C(0x0001ff10));
    }
    for (index = 0u; status == VF2_OK && index < 26u; ++index) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a0e0) + (uint32_t)index * UINT32_C(4),
            UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && input_mode == UINT8_C(10)) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a124),
                                       UINT32_C(0x3f0f5c29));
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a128),
                                           UINT32_C(0x3ef0a3d7));
        }
        recovered_instructions = UINT64_C(117);
    } else if (status == VF2_OK && input_mode == UINT8_C(6)) {
        static const uint32_t offsets[] = {
            UINT32_C(0x004), UINT32_C(0x008), UINT32_C(0x010), UINT32_C(0x018),
            UINT32_C(0x020), UINT32_C(0x038), UINT32_C(0x03c), UINT32_C(0x044),
            UINT32_C(0x048), UINT32_C(0x04c), UINT32_C(0x054)};
        static const uint32_t values[] = {
            UINT32_C(0x3f0a3d71), UINT32_C(0x3f0a3d71), UINT32_C(0x3f6b851f),
            UINT32_C(0x3f5eb852), UINT32_C(0x3f028f5c), UINT32_C(0x3f0a3d71),
            UINT32_C(0x3f0a3d71), UINT32_C(0x3f07ae14), UINT32_C(0x3f28f5c3),
            UINT32_C(0x3f11eb85), UINT32_C(0x3f028f5c)};
        for (index = 0u; status == VF2_OK && index < sizeof(offsets) / sizeof(offsets[0]);
             ++index) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0050a0e0) + offsets[index],
                                           values[index]);
        }
        recovered_instructions = UINT64_C(136);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    /* The final ordinal compares in 0x1ff0c leave EQUAL on the taken mode
     * branches and compare(6, mode) on the ordinary fall-through. */
    if (input_mode == UINT8_C(6) || input_mode == UINT8_C(10)) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    } else if (input_mode < UINT8_C(6)) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    }
    cpu->executed_instructions = start_instructions + recovered_instructions;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FLOAT_DEFAULTS_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = recovered_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_input_profile_load(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    uint32_t table_offset = 0u;
    uint32_t word = 0u;
    uint16_t halfwords[3] = {0u, 0u, 0u};
    uint8_t input_mode = 0u;
    uint8_t profile = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_ENTRY ||
        (cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, UINT32_C(0x00500064), &input_mode,
                              sizeof(input_mode));
    table_offset = (uint32_t)input_mode << 8u;
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0006eeae) + table_offset, &profile,
                                  sizeof(profile));
    }
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00501018),
                                   profile == UINT8_C(4) ? UINT32_C(0x000010cc)
                                                        : UINT32_C(0x00001388));
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0006ef0c) + table_offset,
                                  &halfwords[0], sizeof(halfwords[0]));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x018021ee), halfwords[0]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500170), &profile,
                                   sizeof(profile));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0006eea4) + table_offset,
                                      &word);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00501098), word);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0006eea8) + table_offset,
                                  &halfwords[1], sizeof(halfwords[1]));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00501020), halfwords[1]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0006eeaa) + table_offset,
                                  &halfwords[2], sizeof(halfwords[2]));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00501022), halfwords[2]);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[4] = table_offset;
    cpu->registers[5] = word;
    cpu->registers[6] = halfwords[1];
    cpu->registers[7] = halfwords[2];
    cpu->registers[12] = input_mode;
    cpu->registers[15] = profile == UINT8_C(4) ? UINT32_C(0x000010cc) : profile;
    /* The closing cmpobne 4 leaves compare(4, profile); the following
     * loads and stores do not modify the condition. */
    if (profile < UINT8_C(4)) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
    } else if (profile > UINT8_C(4)) {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
        cpu->compare_result = VF2_I960_COMPARE_LESS;
    } else {
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    }
    cpu->ip = VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_EXIT;
    cpu->executed_instructions = start_instructions +
        (profile == UINT8_C(4) ? UINT64_C(19) : UINT64_C(17));
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_LOAD;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        profile == UINT8_C(4) ? UINT64_C(19) : UINT64_C(17);
    return VF2_OK;
}

static vf2_status
execute_post_boot_palette_ramp_entry(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t flags = 0u;
    uint32_t table_offset = 0u;
    uint8_t controls[3] = {0u, 0u, 0u};
    uint8_t input_mode = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_EXIT ||
        (cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, UINT32_C(0x00500064), &input_mode,
                              sizeof(input_mode));
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status != VF2_OK || (flags & UINT32_C(0x00200000)) != 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    table_offset = (uint32_t)input_mode << 8u;
    status = vf2_model2a_read(machine, UINT32_C(0x0006eeb8) + table_offset, controls,
                              sizeof(controls));
    if (status != VF2_OK) {
        return status;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_PALETTE_RAMP_BODY, UINT32_C(0x0001fe64));
    if (status == VF2_OK) {
        cpu->registers[4] = table_offset;
        cpu->registers[5] = controls[0];
        cpu->registers[6] = controls[1];
        cpu->registers[7] = controls[2];
        cpu->registers[12] = input_mode;
        status = vf2_model2a_write(machine, UINT32_C(0x005000e0), controls,
                                   sizeof(controls));
    }
    if (status == VF2_OK) {
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_NONE;
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_PALETTE_BUILD_BODY, UINT32_C(0x00020050));
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(12);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_RAMP_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(12);
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_palette_build(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t work_base = UINT32_C(0x00546008);
    const uint32_t output_base = UINT32_C(0x00546128);
    const uint32_t row_count = UINT32_C(27);
    const uint32_t column_count = UINT32_C(47);
    const uint32_t fixed_point_denominator = UINT32_C(18);
    const uint32_t fixed_point_scale = UINT32_C(28);
    uint8_t red_base = 0u;
    uint8_t red_source_step = 0u;
    uint8_t green_base = 0u;
    uint8_t green_source_step = 0u;
    uint8_t blue_base = 0u;
    uint8_t blue_source_step = 0u;
    uint8_t red_multiplier = 0u;
    uint8_t green_multiplier = 0u;
    uint8_t blue_multiplier = 0u;
    uint32_t red_step = 0u;
    uint32_t green_step = 0u;
    uint32_t blue_step = 0u;
    uint32_t red_accumulator = 0u;
    uint32_t green_accumulator = 0u;
    uint32_t blue_accumulator = 0u;
    uint32_t row = 0u;
    uint32_t column = 0u;
    uint32_t output = output_base;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_PALETTE_BUILD_BODY ||
        (cpu->local_frame_depth != 3u && cpu->local_frame_depth != 6u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500234), &red_base,
                              sizeof(red_base));
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500235), &red_source_step,
                                  sizeof(red_source_step));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500236), &green_base,
                                  sizeof(green_base));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500237), &green_source_step,
                                  sizeof(green_source_step));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500238), &blue_base,
                                  sizeof(blue_base));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00500239), &blue_source_step,
                                  sizeof(blue_source_step));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x005000e0), &red_multiplier,
                                  sizeof(red_multiplier));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x005000e1), &green_multiplier,
                                  sizeof(green_multiplier));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x005000e2), &blue_multiplier,
                                  sizeof(blue_multiplier));
    }
    if (status != VF2_OK) {
        return status;
    }

    /* The ROM emits eighteen quad stores for this 0x120-byte scratch area. */
    for (row = 0u; status == VF2_OK && row < UINT32_C(0x120); row += 16u) {
        uint8_t zeroes[16] = {0u};
        status = vf2_model2a_write(machine, work_base + row, zeroes, sizeof(zeroes));
    }

    for (row = 1u; status == VF2_OK && row <= row_count; ++row) {
        red_step = (row * (uint32_t)red_source_step * fixed_point_scale) /
                   fixed_point_denominator;
        green_step = (row * (uint32_t)green_source_step * fixed_point_scale) /
                     fixed_point_denominator;
        blue_step = (row * (uint32_t)blue_source_step * fixed_point_scale) /
                    fixed_point_denominator;
        red_accumulator = 0u;
        green_accumulator = 0u;
        blue_accumulator = 0u;

        status = write_u16_le(machine, output, 0u);
        if (status == VF2_OK) {
            status = write_u16_le(machine, output + UINT32_C(2), 0u);
        }
        if (status == VF2_OK) {
            status = write_u16_le(machine, output + UINT32_C(4), 0u);
        }
        output += UINT32_C(6);

        for (column = 0u; status == VF2_OK && column < column_count; ++column) {
            uint32_t red = 0u;
            uint32_t green = 0u;
            uint32_t blue = 0u;

            red_accumulator += red_step;
            red = (red_accumulator >> 8u) + (uint32_t)red_base;
            if (red >= UINT32_C(0x100)) {
                red = UINT32_MAX;
            }
            red = (red * (uint32_t)red_multiplier) >> 7u;

            green_accumulator += green_step;
            green = (green_accumulator >> 8u) + (uint32_t)green_base;
            if (green >= UINT32_C(0x100)) {
                green = UINT32_MAX;
            }
            green = (green * (uint32_t)green_multiplier) >> 7u;

            blue_accumulator += blue_step;
            blue = (blue_accumulator >> 8u) + (uint32_t)blue_base;
            if (blue >= UINT32_C(0x100)) {
                blue = UINT32_MAX;
            }
            blue = (blue * (uint32_t)blue_multiplier) >> 7u;

            status = write_u16_le(machine, output, (uint16_t)red);
            if (status == VF2_OK) {
                status = write_u16_le(machine, output + UINT32_C(2), (uint16_t)green);
            }
            if (status == VF2_OK) {
                status = write_u16_le(machine, output + UINT32_C(4), (uint16_t)blue);
            }
            output += UINT32_C(6);
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00546004), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00546000), UINT32_C(1));
    }
    if (status != VF2_OK) {
        return status;
    }

    /* The final cmpinco is in the build frame.  The ordinary procedure
     * return below restores the caller's locals, so only its condition code
     * survives this return. */
    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions =
        start_instructions + VF2_NATIVE_POST_BOOT_PALETTE_BUILD_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_palette_build_return(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                       vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_PALETTE_BUILD_RETURN ||
        (cpu->local_frame_depth != 2u && cpu->local_frame_depth != 5u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(1);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD_RETURN;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_resumed_wrapper_prefix(vf2_model2a *machine,
                                         vf2_i960_cpu *cpu,
                                         vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t table_offset = cpu->registers[4];
    uint32_t value = 0u;
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x0006eeb0) + table_offset, &value
    );
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] = value;
        status = vf2_model2a_read_u32(
            machine, UINT32_C(0x0006eeb4) + table_offset, &value
        );
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER + 1u] = value;
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_HELPER,
            VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_HELPER_RETURN
        );
    }
    if (status == VF2_OK) {
        cpu->registers[3] = 1u;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550000), 1u);
    }
    if (status == VF2_OK) {
        cpu->registers[3] = UINT32_C(0x005502e0);
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005502e0), 3u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502e4), cpu->registers[VF2_I960_G0_REGISTER]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502e8),
            cpu->registers[VF2_I960_G0_REGISTER + 1u]
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x005502ec), cpu->registers[VF2_I960_G0_REGISTER + 2u]
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_HELPER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(
            machine, UINT32_C(0x0050a014) + index * UINT32_C(4), 0u
        );
    }
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = write_u16_le(
            machine, UINT32_C(0x0050a020) + index * UINT32_C(2), 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_NEXT,
            VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_RETURN
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->executed_instructions = start_instructions + UINT64_C(27);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_WRAPPER_PREFIX;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_resumed_helper_init(vf2_model2a *machine,
                                      vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    static const struct {
        uint32_t offset;
        uint32_t value;
    } global_words[] = {
        {UINT32_C(0x0050a160), UINT32_C(0xc0900000)},
        {UINT32_C(0x0050a164), UINT32_C(0x3dcccccd)},
        {UINT32_C(0x0050a168), UINT32_C(0x3dcccccd)},
    };
    static const struct {
        uint32_t offset;
        uint32_t value;
    } state_words[] = {
        {UINT32_C(0x234), UINT32_C(0x3c872b02)},
        {UINT32_C(0x238), UINT32_C(0x3ca3d70a)},
        {UINT32_C(0x264), UINT32_C(0x409851ec)},
        {UINT32_C(0x268), UINT32_C(0x40d051ec)},
        {UINT32_C(0x240), 0u},
        {UINT32_C(0x2bc), 0u},
        {UINT32_C(0x2c0), 0u},
        {UINT32_C(0x2c4), UINT32_C(0x3e19999a)},
        {UINT32_C(0x2c8), 0u},
        {UINT32_C(0x2cc), UINT32_C(0xbcf5c28f)},
    };
    static const uint32_t state_halfwords[][2] = {
        {UINT32_C(0x260), 0u}, {UINT32_C(0x23c), 13u},
        {UINT32_C(0x26c), 0u}, {UINT32_C(0x26e), 0u},
        {UINT32_C(0x244), 0u}, {UINT32_C(0x2b0), 0u},
        {UINT32_C(0x2b2), 0u}, {UINT32_C(0x2b4), 0u},
        {UINT32_C(0x2b6), 0u}, {UINT32_C(0x2b8), 0u},
        {UINT32_C(0x2ba), 0u},
    };
    static const uint32_t state_bytes[][2] = {
        {UINT32_C(0x27c), 0u}, {UINT32_C(0x23e), 88u},
        {UINT32_C(0x27d), 0u}, {UINT32_C(0x23f), 0u},
        {UINT32_C(0x246), UINT32_C(0xff)}, {UINT32_C(0x27e), 0u},
        {UINT32_C(0x27f), 0u},
    };
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t state_base = 0u;
    uint32_t stream_base = 0u;
    uint8_t byte_value = 0u;
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_NEXT ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; status == VF2_OK && index < sizeof(global_words) /
                                         sizeof(global_words[0]); ++index) {
        status = vf2_model2a_write_u32(machine, global_words[index].offset,
                                       global_words[index].value);
    }
    if (status == VF2_OK) {
        byte_value = 0u;
        status = vf2_model2a_write(machine, UINT32_C(0x0050a14d),
                                   &byte_value, sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &state_base);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, state_base + UINT32_C(0xdf),
                                  &byte_value, sizeof(byte_value));
        byte_value = (uint8_t)(byte_value & UINT8_C(0xfe));
        if (status == VF2_OK) {
            status = vf2_model2a_write(machine, state_base + UINT32_C(0xdf),
                                       &byte_value, sizeof(byte_value));
        }
    }
    for (index = 0u; status == VF2_OK && index < sizeof(state_words) /
                                         sizeof(state_words[0]); ++index) {
        status = vf2_model2a_write_u32(machine, state_base + state_words[index].offset,
                                       state_words[index].value);
    }
    for (index = 0u; status == VF2_OK && index < sizeof(state_halfwords) /
                                         sizeof(state_halfwords[0]); ++index) {
        status = write_u16_le(machine, state_base + state_halfwords[index][0],
                              (uint16_t)state_halfwords[index][1]);
    }
    for (index = 0u; status == VF2_OK && index < sizeof(state_bytes) /
                                         sizeof(state_bytes[0]); ++index) {
        byte_value = (uint8_t)state_bytes[index][1];
        status = vf2_model2a_write(machine, state_base + state_bytes[index][0],
                                   &byte_value, sizeof(byte_value));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050084c), &stream_base);
    }
    if (status == VF2_OK) {
        cpu->registers[3] = state_base;
        cpu->registers[4] = stream_base;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, stream_base + UINT32_C(0x40), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_enter_procedure(
            cpu, VF2_NATIVE_POST_BOOT_RESUMED_HELPER_NESTED,
            VF2_NATIVE_POST_BOOT_RESUMED_HELPER_INIT_RETURN
        );
    }
    if (status == VF2_OK) {
        cpu->registers[8] = UINT32_C(0x40c00000);
        cpu->registers[9] = UINT32_C(0x40966666);
        cpu->registers[10] = UINT32_C(0x41940000);
        cpu->registers[12] = 0u;
        cpu->registers[13] = 0u;
        cpu->registers[14] = 0u;
        status = vf2_model2a_write_u32(machine, stream_base + UINT32_C(0x54),
                                       cpu->registers[8]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, stream_base + UINT32_C(0x58),
                                       cpu->registers[9]);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, stream_base + UINT32_C(0x5c),
                                       cpu->registers[10]);
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(machine, stream_base + UINT32_C(0x60) +
                                       index * UINT32_C(4), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, stream_base + UINT32_C(0x70), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_HELPER_INIT_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions +
        VF2_NATIVE_POST_BOOT_RESUMED_HELPER_INSTRUCTIONS +
        VF2_NATIVE_POST_BOOT_RESUMED_HELPER_NESTED_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_HELPER_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_resumed_luma_table(vf2_model2a *machine,
                                     vf2_i960_cpu *cpu,
                                     vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t destination_start = UINT32_C(0x12800000);
    const uint32_t source_start = UINT32_C(0x00078d10);
    uint32_t row_count = 0u;
    const uint32_t column_count = UINT32_C(128);
    uint64_t byte_count = 0u;
    uint64_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_RETURN ||
        cpu->local_frame_depth == 0u ||
        vf2_model2a_read_u32(machine, UINT32_C(0x00078d0c), &row_count) != VF2_OK ||
        row_count != UINT32_C(66)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    byte_count = (uint64_t)row_count * column_count;
    if (
        byte_count > VF2_LUMA_RAM_SIZE / sizeof(uint32_t) ||
        destination_start < VF2_LUMA_RAM_BASE ||
        destination_start > VF2_LUMA_RAM_BASE + VF2_LUMA_RAM_SIZE -
                                 (uint32_t)(byte_count * sizeof(uint32_t)) ||
        source_start >= VF2_MAIN_ROM_BASE + machine->main_rom_size ||
        byte_count > (uint64_t)(VF2_MAIN_ROM_BASE + machine->main_rom_size -
                                source_start)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_LUMA_TABLE_BODY,
                                          VF2_NATIVE_POST_BOOT_RESUMED_LUMA_RETURN);
    for (index = 0u; status == VF2_OK && index < byte_count; ++index) {
        uint8_t value = 0u;
        status = vf2_model2a_read(machine, source_start + (uint32_t)index,
                                  &value, sizeof(value));
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, destination_start + (uint32_t)(index * sizeof(uint32_t)),
                (uint32_t)value);
        }
    }
    if (status == VF2_OK) {
        cpu->registers[VF2_I960_G0_REGISTER] =
            destination_start + (uint32_t)(byte_count * sizeof(uint32_t));
        cpu->registers[VF2_I960_G0_REGISTER + 1u] =
            source_start + (uint32_t)byte_count;
        cpu->registers[VF2_I960_G0_REGISTER + 2u] = 0u;
        cpu->registers[VF2_I960_G0_REGISTER + 3u] = 0u;
        cpu->arithmetic_control =
            (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(2);
        cpu->compare_result = VF2_I960_COMPARE_EQUAL;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_LUMA_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->executed_instructions = start_instructions +
        VF2_NATIVE_POST_BOOT_LUMA_TABLE_INSTRUCTIONS;
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_TABLE;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = cpu->executed_instructions -
                                          start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_post_boot_resumed_luma_return(vf2_model2a *machine,
                                      vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_RESUMED_LUMA_RETURN ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    cpu->executed_instructions = start_instructions + UINT64_C(1);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_RETURN;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(1);
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
post_boot_copro_write(vf2_model2a *machine, const vf2_i960_cpu *cpu,
                      uint32_t value) {
    const uint32_t address = cpu->registers[27] + cpu->registers[28];
    if (address != VF2_COPRO_PORT_BASE + UINT32_C(0x4000)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return vf2_model2a_write_u32(machine, address, value);
}

static vf2_status
post_boot_copro_read(vf2_model2a *machine, const vf2_i960_cpu *cpu,
                     uint32_t *value) {
    const uint32_t address = cpu->registers[27] + cpu->registers[28];
    if (address != VF2_COPRO_PORT_BASE + UINT32_C(0x4000)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return vf2_model2a_read_u32(machine, address, value);
}

static vf2_status
execute_post_boot_copro_helper(vf2_model2a *machine, vf2_i960_cpu *cpu) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t g5 = cpu->registers[21];
    const uint32_t r3 = UINT32_C(0x00003039);
    const uint32_t r4 = UINT32_C(0x3f9e0419);
    const uint32_t r5 = UINT32_C(0x3f800000);
    uint32_t cursor = 0u;
    uint32_t value = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || cpu->ip != VF2_NATIVE_POST_BOOT_COPRO_HELPER_ENTRY ||
        cpu->local_frame_depth != 5u ||
        (g5 != UINT32_C(0x0050e000) && g5 != UINT32_C(0x0050e800))) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = post_boot_copro_write(machine, cpu, UINT32_C(0x00800101));
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x00800101));
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x00800101));
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x01800303));

    /* Three identical scalar probes are mirrored through the byte cursor. */
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        static const uint32_t commands[3] = {
            UINT32_C(0x36006c6c), UINT32_C(0x36806d6d), UINT32_C(0x37006e6e)
        };
        status = vf2_model2a_read_u32(machine, UINT32_C(0x005001e4), &cursor);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, UINT32_C(0x0090e000) + cursor, r3);
        }
        cursor += UINT32_C(4);
        if (status == VF2_OK) {
            const uint8_t cursor_byte = (uint8_t)cursor;
            status = vf2_model2a_write(machine, UINT32_C(0x005001e4), &cursor_byte, 1u);
        }
        if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, commands[index]);
    }

    status = status == VF2_OK ? vf2_model2a_read_u32(machine, UINT32_C(0x005001e4), &cursor) : status;
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0090e000) + cursor, r4);
        cursor = (cursor + UINT32_C(4)) & ~UINT32_C(0x100);
    }
    if (status == VF2_OK) {
        const uint8_t cursor_byte = (uint8_t)cursor;
        status = vf2_model2a_write(machine, UINT32_C(0x005001e4), &cursor_byte, 1u);
    }

    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x37806f6f));
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x03800707));
    for (index = 0u; status == VF2_OK && index < 3u; ++index) status = post_boot_copro_write(machine, cpu, r4);
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x06000c0c));
    for (index = 0u; status == VF2_OK && index < 3u; ++index) status = post_boot_copro_write(machine, cpu, UINT32_C(0x09801313));
    if (status == VF2_OK) status = post_boot_copro_read(machine, cpu, &value);
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x02800505));
    for (index = 0u; status == VF2_OK && index < 12u; ++index) {
        status = post_boot_copro_read(machine, cpu, &value);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, g5 + (uint32_t)index * 4u, value);
    }
    g5 += UINT32_C(0x30);

    for (index = 0u; status == VF2_OK && index < 3u; ++index) status = post_boot_copro_write(machine, cpu, UINT32_C(0x01000202));
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, UINT32_C(0x09801313));
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, r4);
    if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, r5);
    if (status == VF2_OK) status = post_boot_copro_read(machine, cpu, &value);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, g5, value);
    g5 += UINT32_C(4);

#define COPRO_RESULT(cmd, first, second, mask16) do { \
        if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, (cmd)); \
        if (status == VF2_OK) status = post_boot_copro_write(machine, cpu, (first)); \
        if (status == VF2_OK && (second) != UINT32_MAX) status = post_boot_copro_write(machine, cpu, (second)); \
        if (status == VF2_OK) status = post_boot_copro_read(machine, cpu, &value); \
        if (mask16) value &= UINT32_C(0xffff); \
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, g5, value); \
        g5 += UINT32_C(4); \
    } while (0)
    COPRO_RESULT(UINT32_C(0x0a001414), r5, r4, 0);
    COPRO_RESULT(UINT32_C(0x0a801515), r4, r5, 0);
    COPRO_RESULT(UINT32_C(0x0b001616), r5, r4, 0);
    COPRO_RESULT(UINT32_C(0x16802d2d), r4, r5, 0);
    COPRO_RESULT(UINT32_C(0x10802121), r3, UINT32_MAX, 0);
    COPRO_RESULT(UINT32_C(0x11002222), r3, UINT32_MAX, 0);
    COPRO_RESULT(UINT32_C(0x13802727), r4, r5, 1);
#undef COPRO_RESULT

    if (status != VF2_OK) return status;
    cpu->registers[21] = g5;
    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(4);
    cpu->compare_result = VF2_I960_COMPARE_LESS;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) return status;
    cpu->executed_instructions = start_instructions + UINT64_C(147);
    if (cpu->procedure_returns != start_returns + UINT64_C(1)) return VF2_ERROR_UNSUPPORTED;
    return VF2_OK;
}

static vf2_status
execute_post_boot_copro_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                             vf2_native_runtime_step_report *report) {
    static const uint32_t destinations[8] = {
        UINT32_C(0x01000a14), UINT32_C(0x01000a94), UINT32_C(0x01000b14), UINT32_C(0x01000b94),
        UINT32_C(0x01000a14), UINT32_C(0x01000a94), UINT32_C(0x01000b14), UINT32_C(0x01000b94)
    };
    static const uint32_t sources[8] = {
        UINT32_C(0x0000a188), UINT32_C(0x0000a1b0), UINT32_C(0x0000a1cc), UINT32_C(0x0000a208),
        UINT32_C(0x0000a2c4), UINT32_C(0x0000a300), UINT32_C(0x0000a33c), UINT32_C(0x0000a378)
    };
    static const uint32_t exits[8] = {
        UINT32_C(0x0000a1a0), UINT32_C(0x0000a1bc), UINT32_C(0x0000a1f8), UINT32_C(0x0000a234),
        UINT32_C(0x0000a2f0), UINT32_C(0x0000a32c), UINT32_C(0x0000a368), UINT32_C(0x0000a3a4)
    };
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_hybrid_bridge_report text_report;
    uint32_t i = 0u;
    uint32_t left = UINT32_C(0x0050e000);
    uint32_t right = UINT32_C(0x0050e800);
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_COPRO_INIT_ENTRY || cpu->local_frame_depth != 4u ||
        cpu->registers[27] + cpu->registers[28] != VF2_COPRO_PORT_BASE + UINT32_C(0x4000)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    for (i = 0u; i < 4u; ++i) {
        memset(&text_report, 0, sizeof(text_report));
        cpu->registers[25] = destinations[i];
        cpu->registers[14] = sources[i];
        status = execute_inline_text_thunk(machine, cpu, &text_report);
        if (status != VF2_OK || cpu->ip != exits[i]) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    for (i = 0u; status == VF2_OK && i < UINT32_C(1280); ++i) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050e000) + i * 4u, 0u);
    }
    for (i = 0u; status == VF2_OK && i < UINT32_C(1280); ++i) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x0050e800) + i * 4u, UINT32_C(10));
    }
    if (status != VF2_OK) return status;

    cpu->registers[21] = UINT32_C(0x0050e000);
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_COPRO_HELPER_ENTRY, UINT32_C(0x0000a280));
    if (status == VF2_OK) status = execute_post_boot_copro_helper(machine, cpu);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a280)) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    cpu->registers[21] = UINT32_C(0x0050e800);
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_COPRO_HELPER_ENTRY, UINT32_C(0x0000a28c));
    if (status == VF2_OK) status = execute_post_boot_copro_helper(machine, cpu);
    if (status != VF2_OK || cpu->ip != UINT32_C(0x0000a28c)) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;

    while (right < cpu->registers[21]) {
        uint32_t a = 0u, b = 0u;
        status = vf2_model2a_read_u32(machine, left, &a);
        if (status == VF2_OK) status = vf2_model2a_read_u32(machine, right, &b);
        if (status != VF2_OK) return status;
        if (a != b) return VF2_ERROR_UNSUPPORTED;
        left += 4u;
        right += 4u;
    }
    if (right != cpu->registers[21]) return VF2_ERROR_UNSUPPORTED;

    for (i = 4u; i < 8u; ++i) {
        memset(&text_report, 0, sizeof(text_report));
        cpu->registers[25] = destinations[i];
        cpu->registers[14] = sources[i];
        status = execute_inline_text_thunk(machine, cpu, &text_report);
        if (status != VF2_OK || cpu->ip != exits[i]) return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_COPRO_INIT_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    cpu->executed_instructions = start_instructions + UINT64_C(13324);
    if (cpu->procedure_calls != start_calls + UINT64_C(10) ||
        cpu->procedure_returns != start_returns + UINT64_C(11)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    memset(report, 0, sizeof(*report));
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COPRO_INIT;
    report->entry_address = VF2_NATIVE_POST_BOOT_COPRO_INIT_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(13324);
    report->recovered_procedure_calls = UINT64_C(10);
    report->recovered_procedure_returns = UINT64_C(11);
    return VF2_OK;
}

static vf2_status
execute_post_boot_delay(vf2_model2a *machine, vf2_i960_cpu *cpu,
                        vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu != NULL ? cpu->executed_instructions : 0u;
    const uint64_t start_calls = cpu != NULL ? cpu->procedure_calls : 0u;
    const uint64_t start_returns = cpu != NULL ? cpu->procedure_returns : 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_DELAY_ENTRY) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (cpu->local_frame_depth != 3u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_i960_cpu_enter_procedure(
        cpu, VF2_NATIVE_POST_BOOT_DELAY_LOOP_ENTRY,
        VF2_NATIVE_POST_BOOT_DELAY_LOOP_RETURN);
    if (status != VF2_OK) return status;

    /* ROM-backed 0x9f84 delay helper: g4 starts at 700000 and g5 counts
     * down to zero. Preserve the exact aggregate architectural accounting
     * instead of spending 2.1M host interpreter steps in the native path. */
    cpu->registers[4] = UINT32_C(700000);
    cpu->registers[5] = 0u;
    cpu->executed_instructions += UINT64_C(2100196);
    cpu->arithmetic_control =
        (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_POST_BOOT_DELAY_LOOP_RETURN) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    cpu->ip = VF2_NATIVE_POST_BOOT_DELAY_EXIT;
    cpu->executed_instructions = start_instructions + UINT64_C(2100198);
    if (cpu->procedure_calls != start_calls + UINT64_C(1) ||
        cpu->procedure_returns != start_returns + UINT64_C(1)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(report, 0, sizeof(*report));
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DELAY;
    report->entry_address = VF2_NATIVE_POST_BOOT_DELAY_ENTRY;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(2100198);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(1);
    return VF2_OK;
}

static vf2_status
execute_post_boot_main_loop_init(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                 vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    uint32_t state_base = 0u;
    uint32_t audio_base = 0u;
    uint32_t input0 = 0u;
    uint32_t input1 = 0u;
    uint32_t flags = 0u;
    uint32_t index = 0u;
    uint8_t zero = 0u;
    uint32_t config_base = 0u;
    uint8_t config_value = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_POST_BOOT_MAIN_LOOP_INIT_ENTRY ||
        (cpu->local_frame_depth != 0u && cpu->local_frame_depth != 3u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* 0x9a00's fixed initialization stores. */
    status = vf2_model2a_write(machine, UINT32_C(0x0050406b), &zero, sizeof(zero));
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &state_base);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, state_base + UINT32_C(0xf8), UINT16_C(23u << 5));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x005000e9), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005000e4), 0u);
    }
    if (status == VF2_OK) {
        const uint8_t one = 1u;
        status = vf2_model2a_write(machine, UINT32_C(0x0050009c), &one, sizeof(one));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500070), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500074), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500081), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050a0b8), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050a0b9), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050005b), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00500086), 0u);
    }
    if (status == VF2_OK) {
        status = write_u16_le(machine, UINT32_C(0x00500088), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005001c0), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00508008), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500850), &audio_base);
    }
    if (status == VF2_OK) {
        const uint8_t one = 1u;
        status = vf2_model2a_write(machine, audio_base + UINT32_C(0x1fd), &one,
                                   sizeof(one));
    }
    if (status == VF2_OK) {
        const uint8_t thirty = 30u;
        status = vf2_model2a_write(machine, UINT32_C(0x00500090), &thirty,
                                   sizeof(thirty));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500804), &input0);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500808), &input1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, input0 + UINT32_C(0x1230), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, input0 + UINT32_C(0x1234), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, input0, &flags);
    }
    if (status == VF2_OK) {
        flags = (flags & ~(UINT32_C(1) << 23u)) | (UINT32_C(1) << 26u);
        status = vf2_model2a_write_u32(machine, input0, flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, input1 + UINT32_C(0x1230), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, input1 + UINT32_C(0x1234), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, input1, &flags);
    }
    if (status == VF2_OK) {
        flags = (flags & ~(UINT32_C(1) << 23u)) | (UINT32_C(1) << 26u);
        status = vf2_model2a_write_u32(machine, input1, flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050008d), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050008e), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00504078), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    }
    if (status == VF2_OK) {
        flags &= ~UINT32_C(0x001e0800);
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
    }

    /* 0x9b90..0x9c90 fixed warm-main defaults before sound init. */
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a8), (uint8_t[]){1u}, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000ac), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000a9), (uint8_t[]){7u}, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000aa), (uint8_t[]){8u}, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000ab), (uint8_t[]){9u}, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x0050083c), &state_base);
    }
    if (status == VF2_OK) status = vf2_model2a_write(machine, state_base + UINT32_C(6), &zero, 1u);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, UINT32_C(0x00500840), &state_base);
    }
    if (status == VF2_OK) status = vf2_model2a_write(machine, state_base + UINT32_C(6), (uint8_t[]){1u}, 1u);
    for (index = 0u; status == VF2_OK && index < 4u; ++index) {
        status = vf2_model2a_write(machine, UINT32_C(0x00500148) + index, &zero, 1u);
    }
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005001e4), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005010c4), UINT32_C(0x40aaaaab));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x005010d4), 0u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status == VF2_OK) {
        flags &= ~((UINT32_C(1) << 16u) | (UINT32_C(1) << 25u) |
                   (UINT32_C(1) << 23u) | (UINT32_C(1) << 24u));
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
    }
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500814), &state_base);
    if (status == VF2_OK) status = vf2_model2a_write(machine, state_base + UINT32_C(0x2d4), &zero, 1u);

    /* The 0x19a7c sound initializer writes the small SCSP control block and
       clears the repeating 0x30-byte service slots. */
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00548006), (uint8_t[]){1u}, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00548007), (uint8_t[]){3u}, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00548008), (uint8_t[]){4u}, 1u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00548004), &zero, sizeof(zero));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00548005), (uint8_t[]){1u}, 1u);
    }
    for (index = 0u; status == VF2_OK && index < UINT32_C(480); ++index) {
        status = vf2_model2a_write_u32(machine,
                                       UINT32_C(0x00548010) + index * UINT32_C(0x40) +
                                           UINT32_C(0x30),
                                       UINT32_MAX);
    }

    /* 0x9c98..0x9f68 fixed warm-main state after sound init. */
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status == VF2_OK) {
        flags = (flags | (UINT32_C(1) << 31u)) & ~(UINT32_C(1) << 22u);
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
    }
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00530000), (uint8_t[]){0u, 0u, 0xefu, 0x3fu, 0x7fu}, 5u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530100), UINT32_C(0x45480000));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530104), UINT32_C(0x3f800000));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530108), 0u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0053010c), &zero, 1u);
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = write_u16_le(machine, UINT32_C(0x0053011c) + index * UINT32_C(4), 0u);
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00530110) + index * UINT32_C(4), 0u);
    }
    for (index = 0u; status == VF2_OK && index < 3u; ++index) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00530130) + index * UINT32_C(4), UINT32_C(0x3f800000));
    }
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0053014c), 0u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00530150), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0053013c), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530140), UINT32_C(0xbee66666));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530144), UINT32_C(0xbf63d70a));
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00530148), UINT32_C(0x3ee66666));
    if (status == VF2_OK) status = vf2_model2a_write(machine, input0 + UINT32_C(0x1b0), (uint8_t[]){0u, 0u}, 2u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, input1 + UINT32_C(0x1b0), (uint8_t[]){8u, 8u}, 2u);
    if (status == VF2_OK) status = write_u16_le(machine, UINT32_C(0x005000a2), 0u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00500054), (uint8_t[]){1u}, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0050008f), (uint8_t[]){1u}, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00500067), (uint8_t[]){1u}, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x0050016c), &config_base);
    if (status == VF2_OK) status = vf2_model2a_read(machine, config_base + UINT32_C(0x3340), &config_value, 1u);
    if (status == VF2_OK) {
        if (config_value >= 5u) config_value = 2u;
        status = vf2_model2a_write(machine, UINT32_C(0x0050005a), &config_value, 1u);
    }
    if (status == VF2_OK) status = vf2_model2a_read(machine, config_base + UINT32_C(0x3341), &config_value, 1u);
    if (status == VF2_OK) {
        if (config_value >= 5u) config_value = 2u;
        status = vf2_model2a_write(machine, UINT32_C(0x00500059), &config_value, 1u);
    }
    if (status == VF2_OK) status = vf2_model2a_read(machine, UINT32_C(0x0050005a), &config_value, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00500052), &config_value, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0050004f), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00500051), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00500056), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &flags);
    if (status == VF2_OK) {
        flags &= ~((UINT32_C(1) << 2u) | (UINT32_C(1) << 3u) | (UINT32_C(1) << 21u));
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), flags);
    }
    if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x005000e8), &zero, 1u);
    if (status == VF2_OK) status = vf2_model2a_read_u32(machine, UINT32_C(0x00508000), &flags);
    if (status == VF2_OK) {
        flags |= (UINT32_C(1) << 11u) | (UINT32_C(1) << 15u);
        cpu->registers[3] = flags;
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00508000), flags);
    }
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x00500230), 0u);
    if (status == VF2_OK) status = vf2_model2a_write_u32(machine, UINT32_C(0x0050023c), 0u);
    if (status != VF2_OK) return status;

    /* Preserve the caller locals produced by the literal 0x9a00..0x9f70 path. */
    cpu->registers[4] = UINT32_C(0x00515400);
    cpu->registers[5] = 0u;
    cpu->registers[7] = 0u;
    cpu->registers[14] = 0u;

    /* ROM boundary: 0x9f70 calls the coprocessor initializer at 0xa178. */
    status = vf2_i960_cpu_enter_procedure(cpu, VF2_NATIVE_POST_BOOT_COPRO_INIT_ENTRY, UINT32_C(0x00009f74));
    if (status != VF2_OK) return status;
    cpu->executed_instructions = start_instructions + UINT64_C(1676);
    cpu->procedure_calls = start_calls + UINT64_C(2);
    cpu->procedure_returns = start_returns + UINT64_C(1);
    report->kind = VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MAIN_LOOP_INIT;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count = UINT64_C(1676);
    report->recovered_procedure_calls = UINT64_C(2);
    report->recovered_procedure_returns = UINT64_C(1);
    return VF2_OK;
}

static void accumulate_step(vf2_native_runtime_state *state,
                            const vf2_native_runtime_step_report *report) {
    ++state->blocks_executed;
    state->recovered_instruction_count += report->recovered_instruction_count;
    state->recovered_procedure_calls += report->recovered_procedure_calls;
    state->recovered_procedure_returns += report->recovered_procedure_returns;

    if (report->kind == VF2_NATIVE_RUNTIME_STEP_TASK) {
        ++state->task_bodies_executed;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
        ++state->frame_wait_phases;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER) {
        ++state->scheduler_entries;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION) {
        ++state->scheduler_transitions;
    } else if (report->kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH) {
        ++state->scheduler_finishes;
    }
}

static vf2_status
execute_recurring_camera_task(vf2_model2a *machine, vf2_i960_cpu *cpu,
                              vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t recurring_fighter_cursor = cpu->registers[23];
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t fighter0_flags = 0u;
    uint32_t fighter1_flags = 0u;
    vf2_hybrid_block_report block_report;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_CAMERA_RECURRING_ENTRY || cpu->local_frame_depth == 0u) {
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
    if (status != VF2_OK) {
        return status;
    }
    memset(&block_report, 0, sizeof(block_report));
    status = vf2_hybrid_camera_execute(machine, cpu, cpu->registers[29], &block_report);
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_CAMERA_GATE_ENTRY) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    /* The shared update helper models the first camera invocation, where the
     * initializer leaves g7 one fighter profile behind. A recurring invocation
     * already enters with the final cursor, so preserve it across the update. */
    if (status == VF2_OK) {
        cpu->registers[23] = recurring_fighter_cursor;
        memset(&block_report, 0, sizeof(block_report));
        status =
            vf2_hybrid_camera_execute(machine, cpu, cpu->registers[29], &block_report);
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_CAMERA_FAST_EXIT) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
        report->task_kind = VF2_HYBRID_TASK_CAMERA;
        report->exit_address = cpu->ip;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    }
    return status;
}

static vf2_status
execute_texture_selector_interpreter(vf2_model2a *machine,
                                     vf2_i960_cpu *cpu,
                                     uint32_t stop_address,
                                     vf2_native_runtime_step_report *report)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = stop_address;
    options.max_steps = UINT64_C(20000000);
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    if (result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        cpu->ip != stop_address) {
        return VF2_ERROR_UNSUPPORTED;
    }
    report->kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
    report->bridge_kind = VF2_HYBRID_BRIDGE_TEXTURE_SELECTOR_INTERPRETER;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

typedef struct vf2_native_wrapper_boundary_context {
    uint32_t hit_address;
} vf2_native_wrapper_boundary_context;

static void vf2_native_wrapper_boundary_trace(
    const vf2_i960_trace_event *event,
    const vf2_i960_cpu *cpu,
    void *user_data
)
{
    vf2_native_wrapper_boundary_context *context =
        (vf2_native_wrapper_boundary_context *)user_data;
    static const uint32_t boundaries[] = {
        VF2_NATIVE_FRAME_WAIT_POLL_ENTRY,
        VF2_NATIVE_PLAYER_TASK_ENTRY,
        VF2_TEXTURE_BYTE_RUN_ENTRY,
        VF2_TEXTURE_UPLOAD_DISPATCH_ENTRY,
        VF2_TEXTURE_BYTE_DECODE_ENTRY
    };
    size_t index = 0u;

    (void)cpu;
    if (event == NULL || context == NULL || context->hit_address != 0u) {
        return;
    }
    for (index = 0u; index < sizeof(boundaries) / sizeof(boundaries[0]); ++index) {
        if (event->ip_after == boundaries[index]) {
            context->hit_address = boundaries[index];
            ((vf2_i960_cpu *)cpu)->ip = UINT32_MAX;
            break;
        }
    }
}

static vf2_status
execute_second_scheduler_wrapper_interpreter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_step_report *report)
{
    vf2_native_wrapper_boundary_context boundary;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    memset(&boundary, 0, sizeof(boundary));

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != UINT32_C(0x000142f4) || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&options, 0, sizeof(options));
    options.stop_address = UINT32_MAX;
    options.max_steps = UINT64_C(1000000);
    options.stop_on_self_branch = false;
    options.trace_callback = vf2_native_wrapper_boundary_trace;
    options.trace_user_data = &boundary;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK) {
        return status;
    }
    if (result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        boundary.hit_address == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    cpu->ip = boundary.hit_address;
    report->kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
    report->bridge_kind =
        VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_WRAPPER_INTERPRETER;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_texture_decoder_continuation_interpreter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_step_report *report)
{
    vf2_native_wrapper_boundary_context boundary;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_TEXTURE_BYTE_RUN_EXIT || cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&boundary, 0, sizeof(boundary));
    memset(&options, 0, sizeof(options));
    options.stop_address = UINT32_MAX;
    options.max_steps = UINT64_C(20000000);
    options.stop_on_self_branch = false;
    options.trace_callback = vf2_native_wrapper_boundary_trace;
    options.trace_user_data = &boundary;
    memset(&result, 0, sizeof(result));
    status = vf2_i960_run(cpu, machine, &options, &result);
    if (status != VF2_OK || result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        boundary.hit_address == 0u) {
        return status != VF2_OK ? status : VF2_ERROR_UNSUPPORTED;
    }
    cpu->ip = boundary.hit_address;
    report->kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
    report->bridge_kind =
        VF2_HYBRID_BRIDGE_TEXTURE_DECODER_CONTINUATION_INTERPRETER;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
    report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    return VF2_OK;
}

static vf2_status
execute_sound_continuation_task(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t registry = cpu->registers[29];
    uint8_t mode = 0u;
    uint8_t global_flag = 0u;
    uint8_t zero_byte = 0u;
    uint8_t counter_bytes[2] = {0u, 0u};
    uint8_t zero_counter[2] = {0u, 0u};
    int16_t counter = 0;
    uint32_t read_pointer = 0u;
    uint32_t write_pointer = 0u;
    uint32_t value = 0u;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_SOUND_CONTINUATION_ENTRY ||
        cpu->local_frame_depth == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read(machine, UINT32_C(0x00500f00), &mode, sizeof(mode));
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x0050406b), &global_flag,
                                  sizeof(global_flag));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(machine, UINT32_C(0x00504070), counter_bytes,
                                  sizeof(counter_bytes));
    }
    if (status == VF2_OK) {
        counter =
            (int16_t)((uint16_t)counter_bytes[0] | ((uint16_t)counter_bytes[1] << 8u));
        status =
            vf2_model2a_read_u32(machine, registry + UINT32_C(0x7c), &read_pointer);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, registry + UINT32_C(0x78), &write_pointer);
    }
    if (status == VF2_OK && (mode == 1u || (global_flag & UINT8_C(1)) != 0u ||
                             counter > 0 || read_pointer != write_pointer)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, read_pointer, &value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x00504070), zero_counter,
                                   sizeof(zero_counter));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00504074), value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, read_pointer, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(machine, UINT32_C(0x0050406b), &zero_byte,
                                   sizeof(zero_byte));
    }
    if (status == VF2_OK) {
        status = vf2_i960_cpu_return_procedure(cpu, machine);
    }
    if (status == VF2_OK) {
        cpu->executed_instructions += UINT64_C(20);
    }
    if (status == VF2_OK && cpu->ip != VF2_NATIVE_SCHEDULER_RETURN) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK) {
        report->kind = VF2_NATIVE_RUNTIME_STEP_TASK;
        report->task_kind = VF2_HYBRID_TASK_SOUND;
        report->exit_address = cpu->ip;
        report->recovered_instruction_count =
            cpu->executed_instructions - start_instructions;
        report->recovered_procedure_calls = cpu->procedure_calls - start_calls;
        report->recovered_procedure_returns = cpu->procedure_returns - start_returns;
    }
    return status;
}

static vf2_status
execute_second_sweep_scheduler_finish(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                      vf2_native_runtime_step_report *report) {
    const size_t current_index = (size_t)cpu->registers[11];
    const uint32_t current_registry = cpu->registers[29];
    const uint32_t current_scratch = cpu->registers[10];
    uint32_t task_count = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t scratch2 = 0u;
    uint32_t inactive_stride = 0u;
    uint32_t inactive_registry = 0u;
    uint32_t inactive_flags = 0u;
    uint32_t end_stride = 0u;
    uint32_t end_registry = 0u;
    uint32_t inactive_scratch = 0u;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_SCHEDULER_RETURN ||
        (cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u) ||
        current_index != 27u || current_registry != UINT32_C(0x00516180) ||
        current_scratch !=
            VF2_NATIVE_SCRATCH_BASE + UINT32_C(27) * VF2_NATIVE_SCRATCH_STRIDE) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, VF2_NATIVE_TASK_COUNT_ADDRESS, &task_count);
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, VF2_NATIVE_RUNTIME_FLAGS, &runtime_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER1, &timer1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER2, &timer2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, current_registry + UINT32_C(0x38),
                                      &threshold);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, current_scratch + UINT32_C(8), &scratch2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, current_registry + UINT32_C(8),
                                      &inactive_stride);
    }
    if (status != VF2_OK) {
        return status;
    }

    inactive_registry = current_registry + inactive_stride;
    inactive_scratch = current_scratch + VF2_NATIVE_SCRATCH_STRIDE;
    status = vf2_model2a_read_u32(machine, inactive_registry, &inactive_flags);
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, inactive_registry + UINT32_C(8), &end_stride);
    }
    if (status != VF2_OK) {
        return status;
    }
    end_registry = inactive_registry + end_stride;

    if ((runtime_flags & (UINT32_C(1) << 9u)) == 0u) {
        uint8_t task_name[12] = {0};
        static const uint8_t final_text[] = "           EXAD";
        const uint64_t start_instructions = cpu->executed_instructions;
        const uint64_t start_calls = cpu->procedure_calls;
        const uint64_t start_returns = cpu->procedure_returns;
        size_t character = 0u;
        if (task_count != UINT32_C(29) ||
            (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
            (timer1 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
            (timer2 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
            threshold != 0u || inactive_stride == 0u || end_stride == 0u ||
            (inactive_flags & UINT32_C(0x80000000)) != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        ++scratch2;
        status = vf2_model2a_write_u32(machine, current_scratch + UINT32_C(8), scratch2);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, current_scratch + UINT32_C(0x10), 0u);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, VF2_NATIVE_CURRENT_INDEX, UINT32_C(28));
        if (status == VF2_OK) {
            status = vf2_model2a_read(machine, UINT32_C(0x00011dd8) + UINT32_C(28 * 0x40), task_name, 11u);
        }
        task_name[11] = 0u;
        if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0050e000), task_name, sizeof(task_name));
        for (character = 3u; status == VF2_OK && character < sizeof(task_name) && task_name[character] != 0u; ++character) {
            const uint8_t tile[2] = { task_name[character], UINT8_C(0x80) };
            status = vf2_model2a_write(machine, UINT32_C(0x0100045c) + (uint32_t)(character - 3u) * 2u, tile, sizeof(tile));
        }
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, VF2_NATIVE_TIMER1, VF2_NATIVE_TIMER_MASK);
        if (status == VF2_OK) status = vf2_model2a_write_u32(machine, inactive_scratch + UINT32_C(0x10), 0u);
        for (character = 0u; status == VF2_OK && character < sizeof(final_text) - 1u; ++character) {
            const uint8_t tile[2] = { final_text[character], UINT8_C(0x80) };
            status = vf2_model2a_write(machine, UINT32_C(0x0100045c) + (uint32_t)character * 2u, tile, sizeof(tile));
        }
        if (status != VF2_OK) return status;
        cpu->registers[16] = UINT32_C(0x00444158);
        cpu->registers[25] = UINT32_C(0x010004dc);
        cpu->registers[29] = end_registry;
        {
            const size_t scheduler_frame = cpu->local_frame_depth;
            const size_t text_frame = scheduler_frame + 1u;
            cpu->local_frames[scheduler_frame].registers[2] = UINT32_C(0x00010e64);
            cpu->local_frames[scheduler_frame].registers[4] = 0u;
            cpu->local_frames[scheduler_frame].registers[10] = UINT32_C(0x0050c3a0);
            cpu->local_frames[scheduler_frame].registers[11] = UINT32_C(29);
            cpu->local_frames[scheduler_frame].registers[15] = UINT32_C(0x000fffff);
            cpu->local_frames[text_frame].registers[2] = UINT32_C(0x00009450);
            cpu->local_frames[text_frame].registers[3] = 0u;
            cpu->local_frames[text_frame].registers[4] = 0u;
            cpu->local_frames[text_frame].registers[5] = 0u;
            cpu->local_frames[text_frame].registers[6] = 0u;
            cpu->local_frames[text_frame].registers[7] = 0u;
            cpu->local_frames[text_frame].registers[8] = 0u;
            cpu->local_frames[text_frame].registers[14] = UINT32_C(0x00010ef4);
            cpu->local_frames[text_frame].registers[15] = UINT32_C(0x0100045c);
        }
        cpu->arithmetic_control = (cpu->arithmetic_control & ~UINT32_C(7)) | UINT32_C(1);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status != VF2_OK || cpu->ip != VF2_NATIVE_MAIN_AFTER_SCHEDULER) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        cpu->executed_instructions = start_instructions + UINT64_C(281);
        cpu->procedure_calls = start_calls + UINT64_C(4);
        cpu->procedure_returns = start_returns + UINT64_C(5);
        report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH;
        report->exit_address = cpu->ip;
        report->current_task_index = 27u;
        report->next_task_index = 29u;
        report->descriptors_scanned = 2u;
        report->current_registry_address = current_registry;
        report->next_registry_address = end_registry;
        report->recovered_instruction_count = UINT64_C(281);
        report->recovered_procedure_calls = UINT64_C(4);
        report->recovered_procedure_returns = UINT64_C(5);
        return VF2_OK;
    }

    if (task_count != UINT32_C(29) ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        (timer2 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK || threshold != 0u ||
        inactive_stride == 0u || end_stride == 0u ||
        (inactive_flags & UINT32_C(0x80000000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch2;
    status = vf2_model2a_write_u32(machine, current_scratch + UINT32_C(8), scratch2);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, current_scratch + UINT32_C(0x10), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_NATIVE_CURRENT_INDEX, UINT32_C(28));
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_write_u32(machine, VF2_NATIVE_TIMER1, VF2_NATIVE_TIMER_MASK);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, inactive_scratch + UINT32_C(0x10), 0u);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[29] = end_registry;
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    cpu->executed_instructions += UINT64_C(39);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status != VF2_OK || cpu->ip != VF2_NATIVE_MAIN_AFTER_SCHEDULER) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH;
    report->exit_address = cpu->ip;
    report->current_task_index = current_index;
    report->next_task_index = 29u;
    report->descriptors_scanned = 2u;
    report->current_registry_address = current_registry;
    report->next_registry_address = end_registry;
    report->recovered_instruction_count = UINT64_C(40);
    report->recovered_procedure_calls = UINT64_C(0);
    report->recovered_procedure_returns = UINT64_C(1);
    return VF2_OK;
}

static vf2_status
execute_second_sweep_scheduler_epilogue(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t entry = cpu->ip;
    size_t max_steps = 0u;
    size_t steps = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (entry == VF2_NATIVE_SCHEDULER_EPILOGUE_LDL) {
        max_steps = 14u;
    } else if (entry == VF2_NATIVE_SCHEDULER_EPILOGUE_AND) {
        max_steps = 13u;
    } else if (entry == VF2_NATIVE_SCHEDULER_EPILOGUE_SUB) {
        max_steps = 12u;
    } else {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    while (status == VF2_OK && cpu->ip != VF2_NATIVE_SCHEDULER_EPILOGUE_EXIT &&
           steps < max_steps) {
        status = vf2_i960_step(cpu, machine, NULL);
        ++steps;
    }
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_NATIVE_SCHEDULER_EPILOGUE_EXIT || steps != max_steps ||
        cpu->procedure_calls != start_calls ||
        cpu->procedure_returns != start_returns) {
        return VF2_ERROR_UNSUPPORTED;
    }
    /* The measured epilogue leaves the architectural compare state GREATER
     * at 0x10e3c for all three admitted entry points. */
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;

    memset(report, 0, sizeof(*report));
    report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_EPILOGUE;
    report->exit_address = cpu->ip;
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = 0u;
    report->recovered_procedure_returns = 0u;
    return VF2_OK;
}

static vf2_status
execute_second_sweep_scheduler_scan(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                    vf2_native_runtime_step_report *report) {
    const uint64_t start_instructions = cpu->executed_instructions;
    const uint64_t start_calls = cpu->procedure_calls;
    const uint64_t start_returns = cpu->procedure_returns;
    const uint32_t current_registry = cpu->registers[29];
    const uint32_t current_scratch = cpu->registers[10];
    uint32_t stride = 0u;
    uint32_t flags = 0u;
    uint32_t entry = 0u;
    uint32_t registry = current_registry;
    size_t index = 0u;
    size_t steps = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || report == NULL ||
        cpu->ip != VF2_NATIVE_SCHEDULER_EPILOGUE_EXIT ||
        cpu->registers[11] != UINT32_C(13) ||
        current_registry != UINT32_C(0x00515200) ||
        current_scratch != UINT32_C(0x0050c1a0)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* Validate the exact measured descriptor layout before executing any
     * scheduler instructions. Descriptors 14-16 are inactive 0x80 records;
     * descriptor 17 is the active recurring-camera task. */
    for (index = 0u; index < 4u && status == VF2_OK; ++index) {
        status = vf2_model2a_read_u32(machine, registry + UINT32_C(8), &stride);
        if (status == VF2_OK && stride != UINT32_C(0x80)) {
            status = VF2_ERROR_UNSUPPORTED;
        }
        if (status == VF2_OK) {
            registry += stride;
            status = vf2_model2a_read_u32(machine, registry, &flags);
        }
        if (status == VF2_OK && index < 3u &&
            (flags & UINT32_C(0x80000000)) != 0u) {
            status = VF2_ERROR_UNSUPPORTED;
        }
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, registry + UINT32_C(0x0c), &entry);
    }
    if (status != VF2_OK || registry != VF2_NATIVE_SCHEDULER_SCAN_NEXT_REGISTRY ||
        (flags & UINT32_C(0x80000000)) == 0u ||
        entry != VF2_NATIVE_SCHEDULER_SCAN_EXIT) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    while (status == VF2_OK && cpu->ip != VF2_NATIVE_SCHEDULER_SCAN_EXIT &&
           steps < 62u) {
        status = vf2_i960_step(cpu, machine, NULL);
        ++steps;
    }
    if (status != VF2_OK) {
        return status;
    }
    if (cpu->ip != VF2_NATIVE_SCHEDULER_SCAN_EXIT || steps != 62u ||
        cpu->procedure_calls - start_calls != UINT64_C(1) ||
        cpu->procedure_returns != start_returns ||
        cpu->registers[29] != VF2_NATIVE_SCHEDULER_SCAN_NEXT_REGISTRY) {
        return VF2_ERROR_UNSUPPORTED;
    }

    memset(report, 0, sizeof(*report));
    report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_SCAN;
    report->exit_address = cpu->ip;
    report->current_task_index = 13u;
    report->next_task_index = 17u;
    report->descriptors_scanned = 4u;
    report->current_registry_address = current_registry;
    report->next_registry_address = cpu->registers[29];
    report->recovered_instruction_count =
        cpu->executed_instructions - start_instructions;
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(0);
    return VF2_OK;
}

static vf2_status
execute_second_sweep_scheduler_transition(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                          vf2_native_runtime_step_report *report) {
    const size_t current_index = (size_t)cpu->registers[11];
    const uint32_t current_registry = cpu->registers[29];
    const uint32_t current_scratch = cpu->registers[10];
    uint32_t task_count = 0u;
    uint32_t runtime_flags = 0u;
    uint32_t timer1 = 0u;
    uint32_t timer2 = 0u;
    uint32_t threshold = 0u;
    uint32_t scratch0 = 0u;
    uint32_t scratch1 = 0u;
    uint32_t scratch2 = 0u;
    uint32_t scratch3 = 0u;
    uint32_t next_registry = current_registry;
    uint32_t next_scratch = current_scratch;
    uint32_t next_entry = 0u;
    size_t next_index = current_index;
    size_t scanned = 0u;
    vf2_status status = VF2_OK;

    if (cpu->ip != VF2_NATIVE_SCHEDULER_RETURN ||
        (cpu->local_frame_depth != 1u && cpu->local_frame_depth != 4u) ||
        current_scratch != VF2_NATIVE_SCRATCH_BASE +
                               (uint32_t)current_index * VF2_NATIVE_SCRATCH_STRIDE) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    status = vf2_model2a_read_u32(machine, VF2_NATIVE_TASK_COUNT_ADDRESS, &task_count);
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, VF2_NATIVE_RUNTIME_FLAGS, &runtime_flags);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER1, &timer1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_TIMER2, &timer2);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, current_registry + UINT32_C(0x38),
                                      &threshold);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, current_scratch, &scratch0);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, current_scratch + UINT32_C(4), &scratch1);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, current_scratch + UINT32_C(8), &scratch2);
    }
    if (status == VF2_OK) {
        status =
            vf2_model2a_read_u32(machine, current_scratch + UINT32_C(12), &scratch3);
    }
    if (status != VF2_OK) {
        return status;
    }

    if ((runtime_flags & (UINT32_C(1) << 9u)) == 0u) {
        vf2_hybrid_scheduler_transition_report cold_report;
        memset(&cold_report, 0, sizeof(cold_report));
        next_index = 0u;
        next_registry = 0u;
        next_entry = 0u;
        switch (current_index) {
        case 13u: next_index=17u; next_registry=UINT32_C(0x00515400); next_entry=UINT32_C(0x0001d320); break;
        case 17u: next_index=18u; next_registry=UINT32_C(0x00515880); next_entry=UINT32_C(0x00029748); break;
        case 18u: next_index=24u; next_registry=UINT32_C(0x00515d80); next_entry=UINT32_C(0x000439fc); break;
        case 24u: next_index=25u; next_registry=UINT32_C(0x00515e80); next_entry=UINT32_C(0x000657dc); break;
        case 25u: next_index=26u; next_registry=UINT32_C(0x00515f00); next_entry=UINT32_C(0x000640f4); break;
        case 26u: next_index=27u; next_registry=UINT32_C(0x00516180); next_entry=UINT32_C(0x000640f4); break;
        default: return VF2_ERROR_UNSUPPORTED;
        }
        status = vf2_hybrid_first_dispatch_scheduler_advance(
            machine, cpu, current_index, next_index, current_registry,
            next_registry, next_entry, &cold_report
        );
        if (status != VF2_OK) return status;
        report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION;
        report->exit_address = cpu->ip;
        report->current_task_index = current_index;
        report->next_task_index = next_index;
        report->descriptors_scanned = cold_report.descriptors_scanned;
        report->current_registry_address = current_registry;
        report->next_registry_address = next_registry;
        report->recovered_instruction_count = cold_report.recovered_instruction_count;
        report->recovered_procedure_calls = cold_report.recovered_procedure_calls;
        report->recovered_procedure_returns = cold_report.recovered_procedure_returns;
        return VF2_OK;
    }

    if (task_count != UINT32_C(29) || current_index >= task_count ||
        (runtime_flags & (UINT32_C(1) << 5u)) != 0u ||
        (timer1 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK ||
        (timer2 & VF2_NATIVE_TIMER_MASK) != VF2_NATIVE_TIMER_MASK || threshold != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ++scratch2;
    status = vf2_model2a_write_u32(machine, current_scratch + UINT32_C(8), scratch2);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, current_scratch + UINT32_C(0x10), 0u);
    }

    while (status == VF2_OK && next_index + 1u < task_count) {
        uint32_t stride = 0u;
        uint32_t flags = 0u;

        status = vf2_model2a_read_u32(machine, next_registry + UINT32_C(8), &stride);
        if (status != VF2_OK || stride == 0u) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        next_registry += stride;
        next_scratch += VF2_NATIVE_SCRATCH_STRIDE;
        ++next_index;
        ++scanned;

        status = vf2_model2a_write_u32(machine, VF2_NATIVE_CURRENT_INDEX,
                                       (uint32_t)next_index);
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(machine, VF2_NATIVE_TIMER1,
                                           VF2_NATIVE_TIMER_MASK);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, next_registry, &flags);
        }
        if (status != VF2_OK) {
            return status;
        }

        if ((flags & UINT32_C(0x80000000)) != 0u) {
            status = vf2_model2a_read_u32(machine, next_registry + UINT32_C(0x0c),
                                          &next_entry);
            break;
        }

        status = vf2_model2a_write_u32(machine, next_scratch + UINT32_C(0x10), 0u);
    }

    if (status != VF2_OK || scanned == 0u) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    if (next_entry == 0u) {
        uint32_t end_stride = 0u;
        uint32_t end_registry = 0u;
        const size_t descriptors_to_end = scanned + 1u;
        const uint64_t finish_instructions =
            (uint64_t)descriptors_to_end * UINT64_C(16) + UINT64_C(8);

        status =
            vf2_model2a_read_u32(machine, next_registry + UINT32_C(8), &end_stride);
        if (status != VF2_OK || end_stride == 0u || next_index + 1u != task_count) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }
        end_registry = next_registry + end_stride;

        cpu->registers[29] = end_registry;
        cpu->registers[0] &= ~UINT32_C(7);
        cpu->arithmetic_control &= ~UINT32_C(7);
        cpu->compare_result = VF2_I960_COMPARE_GREATER;
        cpu->executed_instructions += finish_instructions - UINT64_C(1);
        status = vf2_i960_cpu_return_procedure(cpu, machine);
        if (status == VF2_OK) {
            ++cpu->executed_instructions;
        }
        if (status != VF2_OK || cpu->ip != VF2_NATIVE_MAIN_AFTER_SCHEDULER) {
            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
        }

        report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH;
        report->exit_address = cpu->ip;
        report->current_task_index = current_index;
        report->next_task_index = task_count;
        report->descriptors_scanned = descriptors_to_end;
        report->current_registry_address = current_registry;
        report->next_registry_address = end_registry;
        report->recovered_instruction_count = finish_instructions;
        report->recovered_procedure_calls = UINT64_C(0);
        report->recovered_procedure_returns = UINT64_C(1);
        return VF2_OK;
    }

    cpu->registers[3] = task_count;
    cpu->registers[4] = next_entry;
    cpu->registers[5] = scratch1;
    cpu->registers[6] = scratch2;
    cpu->registers[7] = scratch3;
    cpu->registers[8] = VF2_NATIVE_TIMER_MASK;
    cpu->registers[9] = runtime_flags;
    cpu->registers[10] = next_scratch;
    cpu->registers[11] = (uint32_t)next_index;
    cpu->registers[12] = 0u;
    cpu->registers[13] = VF2_NATIVE_SCRATCH_STRIDE;
    cpu->registers[14] = timer1;
    cpu->registers[15] = scanned > 1u ? timer2 : threshold;
    cpu->registers[29] = next_registry;
    cpu->arithmetic_control &= ~UINT32_C(7);
    cpu->compare_result = VF2_I960_COMPARE_GREATER;
    cpu->executed_instructions += (uint64_t)scanned * UINT64_C(16) + UINT64_C(12);

    status = vf2_i960_cpu_enter_procedure(cpu, next_entry, VF2_NATIVE_SCHEDULER_RETURN);
    if (status == VF2_OK) {
        ++cpu->executed_instructions;
    }
    if (status != VF2_OK) {
        return status;
    }

    report->kind = VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION;
    report->exit_address = cpu->ip;
    report->current_task_index = current_index;
    report->next_task_index = next_index;
    report->descriptors_scanned = scanned;
    report->current_registry_address = current_registry;
    report->next_registry_address = next_registry;
    report->recovered_instruction_count =
        (uint64_t)scanned * UINT64_C(16) + UINT64_C(13);
    report->recovered_procedure_calls = UINT64_C(1);
    report->recovered_procedure_returns = UINT64_C(0);
    (void)scratch0;
    return VF2_OK;
}

vf2_status vf2_native_runtime_initialize(vf2_native_runtime_state *state,
                                         size_t frame_wait_visits_before_interrupt) {
    vf2_status status = VF2_OK;

    if (state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    status = vf2_hybrid_frame_wait_initialize(&state->frame_wait,
                                              frame_wait_visits_before_interrupt);
    if (status != VF2_OK) {
        memset(state, 0, sizeof(*state));
    }
    return status;
}

vf2_status vf2_native_runtime_step_impl(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_state *state,
                                        vf2_native_runtime_step_report *report) {
    vf2_native_runtime_step_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.entry_address = cpu->ip;

    if (cpu->ip == UINT32_C(0x0004bb14)) {
        const uint64_t start_instructions = cpu->executed_instructions;
        const uint64_t start_calls = cpu->procedure_calls;
        const uint64_t start_returns = cpu->procedure_returns;
        status = vf2_i960_step(cpu, machine, NULL);
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                cpu->executed_instructions - start_instructions;
            local_report.recovered_procedure_calls =
                cpu->procedure_calls - start_calls;
            local_report.recovered_procedure_returns =
                cpu->procedure_returns - start_returns;
        }
    } else if (cpu->ip == VF2_NATIVE_BOOT_STAGE1_ENTRY) {
        status = execute_boot_stage1(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_BOOT_STAGE2_ENTRY) {
        status = execute_boot_stage2(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_INIT_ENTRY) {
        status = execute_post_boot_init_prefix(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_VIDEO_INIT_ENTRY) {
        status = execute_post_boot_video_init_entry(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_VIDEO_RAMP_ENTRY) {
        status = execute_post_boot_video_ramp(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_COLOR_TABLES_ENTRY ||
               cpu->ip == VF2_NATIVE_POST_BOOT_SECOND_COLOR_TABLES_ENTRY) {
        status = execute_post_boot_color_tables(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_MEMORY_CLEAR_ENTRY ||
               cpu->ip == VF2_NATIVE_POST_BOOT_SECOND_MEMORY_CLEAR_ENTRY) {
        status = execute_post_boot_memory_clear(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_REGISTER_STREAM_ENTRY) {
        status = execute_post_boot_register_stream(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_BLOCK_STREAM_ENTRY) {
        status = execute_post_boot_block_stream(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_BACKUP_SRAM_PROBE_ENTRY) {
        status = execute_post_boot_backup_sram_probe(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_BACKUP_RESTORE_ENTRY) {
        status = execute_post_boot_backup_restore(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_RESTORED_VIDEO_ENTRY) {
        status = execute_post_boot_restored_video_entry(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_PALETTE_SEED_ENTRY) {
        status = execute_post_boot_palette_seed(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TABLE_INIT_ENTRY) {
        status = execute_post_boot_table_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_HARDWARE_CORE_INIT_ENTRY) {
        status = execute_post_boot_hardware_core_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_INIT_ENTRY) {
        status = execute_post_boot_texture_init_entry(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_TIMER_ENTRY) {
        status = execute_post_boot_texture_timer_entry(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_ENTRY) {
        status =
            execute_post_boot_texture_wait_entry(machine, cpu, state, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_POLL) {
        status =
            execute_post_boot_texture_wait_poll(machine, cpu, state, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_RETURN) {
        status = execute_post_boot_early_wait_return(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_WAIT_EXIT) {
        status = execute_post_boot_graphics_verify(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GRAPHICS_VERIFY_EXIT) {
        status = execute_post_boot_texture_record_entry(cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TEXTURE_RECORD_SETUP) {
        status = execute_post_boot_texture_record_setup(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_LUMA_TABLE_ENTRY) {
        status = execute_post_boot_luma_table_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_LUMA_WAIT_ENTRY ||
               cpu->ip == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_ENTRY ||
               cpu->ip == VF2_NATIVE_POST_BOOT_FINAL_WAIT_ENTRY ||
               cpu->ip == VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_ENTRY) {
        status = execute_post_boot_early_wait_entry(cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT &&
               cpu->registers[9] == UINT32_C(1)) {
        status = execute_post_boot_geometry_pattern_return(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT) {
        status = execute_post_boot_geometry_table_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT) {
        status = execute_post_boot_geometry_table_return(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GRAPHICS_STATE_RESET_ENTRY) {
        status = execute_post_boot_graphics_state_reset(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_VIDEO_CONSTANTS_ENTRY) {
        status = execute_post_boot_video_constants(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_DISPLAY_CONSTANTS_ENTRY) {
        status = execute_post_boot_display_constants(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_TASK_REGISTRY_ENTRY) {
        status = execute_post_boot_task_registry_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GRAPHICS_BUFFER_ENTRY) {
        status = execute_post_boot_graphics_buffer_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_RENDER_STATE_ENTRY) {
        status = execute_post_boot_render_state_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GAME_DEFAULTS_ENTRY) {
        status = execute_post_boot_game_defaults_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_OBJECT_TABLE_ENTRY) {
        status = execute_post_boot_object_table_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_EFFECT_TABLE_ENTRY) {
        status = execute_post_boot_effect_table_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_INPUT_RING_ENTRY) {
        status = execute_post_boot_input_ring_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_IO_ENTRY) {
        status = execute_post_boot_io_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GAME_DATA_COPY_ENTRY) {
        status = execute_post_boot_game_data_copy(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_DISPLAY_OFFSET_ENTRY) {
        status = execute_post_boot_display_offset_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_FRAME_ACCUMULATOR_ENTRY) {
        status = execute_post_boot_frame_accumulator_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_PROFILE_DEFAULTS_ENTRY) {
        status = execute_post_boot_profile_defaults_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_ENTRY) {
        status = execute_post_boot_gameplay_globals_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GAMEPLAY_GLOBALS_EXIT) {
        status = execute_post_boot_input_profile_entry(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_FLOAT_DEFAULTS_ENTRY) {
        status = execute_post_boot_float_defaults_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_ENTRY) {
        status = execute_post_boot_input_profile_load(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_INPUT_PROFILE_LOAD_EXIT) {
        status = execute_post_boot_palette_ramp_entry(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_PALETTE_BUILD_BODY) {
        status = execute_post_boot_palette_build(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_PALETTE_BUILD_RETURN) {
        status = execute_post_boot_palette_build_return(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_ENTRY) {
        status = execute_post_boot_resumed_wrapper_prefix(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_NEXT) {
        status = execute_post_boot_resumed_helper_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_RESUMED_WRAPPER_RETURN) {
        status = execute_post_boot_resumed_luma_table(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_RESUMED_LUMA_RETURN) {
        status = execute_post_boot_resumed_luma_return(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_MAIN_LOOP_INIT_ENTRY) {
        status = execute_post_boot_main_loop_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_COPRO_INIT_ENTRY) {
        status = execute_post_boot_copro_init(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_DELAY_ENTRY) {
        status = execute_post_boot_delay(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_POST_BOOT_GEOMETRY_PATTERN_ENTRY ||
               cpu->ip == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT) {
        status = execute_post_boot_geometry_pattern(machine, cpu, &local_report);
    } else if (cpu->ip == UINT32_C(0x00000f7c) ||
               cpu->ip == VF2_NATIVE_FRAME_WAIT_POLL_ENTRY ||
               cpu->ip == VF2_NATIVE_INTERRUPT_RETURN_ENTRY) {
        const uint32_t frame_wait_entry = cpu->ip;
        vf2_hybrid_bridge_report bridge_report;
        int injected_post_boot_pending_vblank = 0;
        memset(&bridge_report, 0, sizeof(bridge_report));

        if (frame_wait_entry == UINT32_C(0x00000f7c) &&
            (cpu->local_frame_depth == 4u || cpu->local_frame_depth == 5u)) {
            const uint32_t caller_return =
                cpu->local_frames[cpu->local_frame_depth - 1u].registers[2];
            uint8_t frame_byte = 0u;
            const int post_boot_wait =
                caller_return == VF2_NATIVE_POST_BOOT_LUMA_WAIT_EXIT ||
                caller_return == VF2_NATIVE_POST_BOOT_PATTERN_WAIT_EXIT ||
                caller_return == VF2_NATIVE_POST_BOOT_FINAL_WAIT_EXIT ||
                caller_return == VF2_NATIVE_POST_BOOT_GEOMETRY_TABLE_WAIT_EXIT;

            if (post_boot_wait) {
                status = vf2_model2a_read(machine, UINT32_C(0x00500000),
                                          &frame_byte, sizeof(frame_byte));
                if (status == VF2_OK && frame_byte == 0u) {
                    vf2_hybrid_frame_wait_report wait_report;
                    memset(&wait_report, 0, sizeof(wait_report));
                    state->frame_wait.visits =
                        state->frame_wait.visits_before_interrupt - 1u;
                    status = vf2_hybrid_frame_wait_observe(
                        machine, cpu, &state->frame_wait, &wait_report);
                    if (status == VF2_OK && !wait_report.interrupt_injected) {
                        status = VF2_ERROR_UNSUPPORTED;
                    }
                    if (status == VF2_OK) {
                        injected_post_boot_pending_vblank = 1;
                        bridge_report.kind = VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL;
                        bridge_report.entry_address = frame_wait_entry;
                        bridge_report.exit_address = cpu->ip;
                        bridge_report.recovered_instruction_count = 0u;
                        bridge_report.recovered_procedure_calls = 1u;
                        bridge_report.recovered_procedure_returns = 0u;
                        bridge_report.cpu_poststate_applied = 1;
                    }
                }
            }
        }
        if (status == VF2_OK && !injected_post_boot_pending_vblank) {
            status = vf2_hybrid_frame_wait_execute(machine, cpu, &state->frame_wait,
                                                   &bridge_report);
        }
        if (status == VF2_OK && frame_wait_entry == VF2_NATIVE_FRAME_WAIT_POLL_ENTRY) {
            uint32_t runtime_flags = 0u;
            uint32_t task_count = 0u;
            status = vf2_model2a_read_u32(machine, VF2_NATIVE_RUNTIME_FLAGS,
                                          &runtime_flags);
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(machine, VF2_NATIVE_TASK_COUNT_ADDRESS,
                                              &task_count);
            }
            if (status == VF2_OK && task_count == UINT32_C(29) &&
                (runtime_flags & (UINT32_C(1) << 9u)) == 0u &&
                bridge_report.recovered_instruction_count != 0u) {
                --cpu->executed_instructions;
                --bridge_report.recovered_instruction_count;
            }
        }
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_NATIVE_SECOND_SCHEDULER_ENTRY ||
               cpu->ip == VF2_NATIVE_SECOND_SCHEDULER_BODY) {
        vf2_hybrid_second_scheduler_report scheduler_report;
        uint32_t scheduler_flags = 0u;
        memset(&scheduler_report, 0, sizeof(scheduler_report));
        status = vf2_model2a_read_u32(machine, VF2_NATIVE_RUNTIME_FLAGS,
                                      &scheduler_flags);
        if (status == VF2_OK) {
            status = vf2_hybrid_second_scheduler_enter(machine, cpu, &scheduler_report);
        }
        if (status == VF2_OK) {
            if (local_report.kind == VF2_NATIVE_RUNTIME_STEP_NONE) {
                local_report.kind = VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER;
                local_report.bridge_kind = VF2_HYBRID_BRIDGE_SECOND_SCHEDULER_ENTRY;
                local_report.exit_address = cpu->ip;
                local_report.next_task_index = scheduler_report.selected_task_index;
                local_report.next_registry_address =
                    scheduler_report.selected_registry_address;
                local_report.descriptors_scanned = scheduler_report.descriptors_scanned;
                local_report.recovered_instruction_count =
                    scheduler_report.recovered_instruction_count;
                local_report.recovered_procedure_calls =
                    scheduler_report.recovered_procedure_calls;
                local_report.recovered_procedure_returns =
                    scheduler_report.recovered_procedure_returns;
            }
        }
    } else if (cpu->ip == VF2_NATIVE_SCHEDULER_EPILOGUE_LDL ||
               cpu->ip == VF2_NATIVE_SCHEDULER_EPILOGUE_AND ||
               cpu->ip == VF2_NATIVE_SCHEDULER_EPILOGUE_SUB) {
        status = execute_second_sweep_scheduler_epilogue(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_SCHEDULER_EPILOGUE_EXIT) {
        status = execute_second_sweep_scheduler_scan(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_SCHEDULER_RETURN &&
               cpu->registers[29] == UINT32_C(0x00516180)) {
        status = execute_second_sweep_scheduler_finish(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_SCHEDULER_RETURN) {
        status = execute_second_sweep_scheduler_transition(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_FRAME_COUNTER_ADVANCE_ENTRY) {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = execute_frame_counter_advance(machine, cpu, &bridge_report);
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_NATIVE_CAMERA_RECURRING_ENTRY) {
        status = execute_recurring_camera_task(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_NATIVE_SOUND_CONTINUATION_ENTRY) {
        status = execute_sound_continuation_task(machine, cpu, &local_report);
    } else if (cpu->ip == VF2_TEXTURE_DEFAULT_LIMITS_ENTRY) {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = execute_texture_default_limits(machine, cpu, &bridge_report);
        if (status == VF2_ERROR_UNSUPPORTED) {
            status = vf2_model2a_write_u32(
                machine, VF2_ORCHESTRATOR_LIMIT_LOW, UINT32_C(0x00003e80)
            );
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, VF2_ORCHESTRATOR_LIMIT_HIGH, UINT32_C(0x00004e20)
                );
            }
            if (status == VF2_OK) {
                cpu->ip = VF2_TEXTURE_DEFAULT_LIMITS_RETURN;
                local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
                local_report.bridge_kind = VF2_HYBRID_BRIDGE_TEXTURE_DEFAULT_LIMITS;
                local_report.exit_address = cpu->ip;
                local_report.recovered_instruction_count = UINT64_C(22);
                local_report.recovered_procedure_returns = UINT64_C(1);
            }
        }
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_PALETTE_PAGE_UPLOAD_ENTRY) {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = execute_palette_page_upload(machine, cpu, &bridge_report);
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    } else if (cpu->ip == VF2_TEXTURE_RECORD_STATUS_SETUP_ENTRY) {
        uint32_t selector_flags = 0u;
        uint16_t selected_value = 0u;
        const uint32_t relevant_flags =
            (UINT32_C(1) << 4u) | (UINT32_C(1) << 3u) | (UINT32_C(1) << 1u);
        status = vf2_model2a_read_u32(
            machine, cpu->registers[5] + UINT32_C(0x10), &selector_flags
        );
        if (status == VF2_OK &&
            (selector_flags & relevant_flags) != 0u) {
            const uint32_t selected_offset =
                (selector_flags & (UINT32_C(1) << 4u)) != 0u
                    ? UINT32_C(0x0a) : UINT32_C(6);
            status = vf2_model2a_read(
                machine, cpu->registers[5] + selected_offset,
                &selected_value, sizeof(selected_value)
            );
            if (status == VF2_OK) {
                status = execute_texture_selector_interpreter(
                    machine, cpu,
                    selected_value == 0u
                        ? VF2_TEXTURE_STATUS_DISPATCH_ENTRY
                        : VF2_TEXTURE_STREAM_HEADER_CALL_ENTRY,
                    &local_report
                );
            }
        } else if (status == VF2_OK) {
            vf2_hybrid_bridge_report bridge_report;
            memset(&bridge_report, 0, sizeof(bridge_report));
            status = vf2_hybrid_post_frame_bridge_execute(
                machine, cpu, &bridge_report
            );
            if (status == VF2_OK) {
                local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
                local_report.bridge_kind = bridge_report.kind;
                local_report.exit_address = cpu->ip;
                local_report.recovered_instruction_count =
                    bridge_report.recovered_instruction_count;
                local_report.recovered_procedure_calls =
                    bridge_report.recovered_procedure_calls;
                local_report.recovered_procedure_returns =
                    bridge_report.recovered_procedure_returns;
            }
        }
    } else if (cpu->ip == VF2_TEXTURE_COUNTER_UPDATE_ENTRY) {
        if (status == VF2_OK) {
            vf2_hybrid_bridge_report bridge_report;
            memset(&bridge_report, 0, sizeof(bridge_report));
            status = vf2_hybrid_post_frame_bridge_execute(
                machine, cpu, &bridge_report
            );
            if (status == VF2_OK) {
                local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
                local_report.bridge_kind = bridge_report.kind;
                local_report.exit_address = cpu->ip;
                local_report.recovered_instruction_count =
                    bridge_report.recovered_instruction_count;
                local_report.recovered_procedure_calls =
                    bridge_report.recovered_procedure_calls;
                local_report.recovered_procedure_returns =
                    bridge_report.recovered_procedure_returns;
            }
        }
    } else if (cpu->ip == UINT32_C(0x000142f4)) {
        status = execute_second_scheduler_wrapper_interpreter(
            machine, cpu, &local_report
        );
    } else if (cpu->ip == VF2_TEXTURE_BYTE_RUN_EXIT) {
        status = execute_texture_decoder_continuation_interpreter(
            machine, cpu, &local_report
        );
    } else if (cpu->ip == VF2_NATIVE_GAME_INFO_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_CAMERA_INITIAL_ENTRY ||
               cpu->ip == VF2_NATIVE_PLAYER_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_USER_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_SOUND_TASK_ENTRY ||
               cpu->ip == VF2_NATIVE_KILL_OSAGE_TASK_ENTRY ||
                cpu->ip == VF2_NATIVE_OSAGE_TASK_ENTRY ||
                cpu->ip == VF2_NATIVE_OBJECT_TASK_ENTRY ||
                cpu->ip == VF2_NATIVE_OBJECT_HANDLER0_ENTRY ||
                cpu->ip == VF2_NATIVE_OBJECT_HANDLER0_NEXT ||
                cpu->ip == VF2_NATIVE_OBJECT_HANDLER1_ENTRY ||
                cpu->ip == VF2_NATIVE_OBJECT_HANDLER1_NEXT ||
                cpu->ip == VF2_NATIVE_OBJECT_HANDLER2_ENTRY ||
                cpu->ip == VF2_NATIVE_GAME_DISP_TASK_ENTRY) {
        const int recurring_kill = cpu->ip == VF2_NATIVE_KILL_OSAGE_TASK_ENTRY &&
                                   cpu->registers[29] == UINT32_C(0x00515e80);
        uint32_t kill_order_flags = 0u;
        int recurring_kill_skips_swap = 0;
        vf2_hybrid_task_report task_report;
        memset(&task_report, 0, sizeof(task_report));
        if (recurring_kill) {
            status =
                vf2_model2a_read_u32(machine, UINT32_C(0x00500020), &kill_order_flags);
            recurring_kill_skips_swap =
                status == VF2_OK && (kill_order_flags & UINT32_C(1)) != 0u;
        }
        if (status == VF2_OK) {
            status = vf2_hybrid_first_dispatch_task_execute(
                machine, cpu, cpu->registers[29], &task_report);
        }

        /* fa_kill_osage swaps its two record pointers with three mov
         * instructions when order bit 0 is clear. Only the recurring path
         * with bit 0 set skips those instructions. */
        if (status == VF2_OK && recurring_kill_skips_swap) {
            if (cpu->executed_instructions < UINT64_C(3) ||
                task_report.recovered_instruction_count < UINT64_C(3)) {
                status = VF2_ERROR_UNSUPPORTED;
            } else {
                cpu->executed_instructions -= UINT64_C(3);
                task_report.recovered_instruction_count -= UINT64_C(3);
            }
        }
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_TASK;
            local_report.task_kind = task_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                task_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                task_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                task_report.recovered_procedure_returns;
        }
    } else {
        vf2_hybrid_bridge_report bridge_report;
        memset(&bridge_report, 0, sizeof(bridge_report));
        status = vf2_hybrid_post_frame_bridge_execute(machine, cpu, &bridge_report);
        if (status == VF2_OK) {
            local_report.kind = VF2_NATIVE_RUNTIME_STEP_BRIDGE;
            local_report.bridge_kind = bridge_report.kind;
            local_report.exit_address = cpu->ip;
            local_report.recovered_instruction_count =
                bridge_report.recovered_instruction_count;
            local_report.recovered_procedure_calls =
                bridge_report.recovered_procedure_calls;
            local_report.recovered_procedure_returns =
                bridge_report.recovered_procedure_returns;
        }
    }

    if (status == VF2_OK) {
        accumulate_step(state, &local_report);
    }
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_native_runtime_run_until(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_state *state,
                                        uint32_t stop_address, size_t max_blocks,
                                        vf2_native_runtime_run_report *report) {
    vf2_native_runtime_run_report local_report;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = cpu->ip;
    local_report.stop_address = stop_address;
    local_report.final_address = cpu->ip;

    while (cpu->ip != stop_address && local_report.blocks_executed < max_blocks) {
        vf2_native_runtime_step_report step_report;
        memset(&step_report, 0, sizeof(step_report));
        local_report.last_entry_address = cpu->ip;
        status = vf2_native_runtime_step(machine, cpu, state, &step_report);
        /* Surface the last attempted step kind, even on failure, so the
         * run report can identify which recovered block rejected an
         * unsupported transition (for example, a third-scheduler attempt). */
        local_report.last_step_kind = step_report.kind;
        local_report.last_bridge_kind = step_report.bridge_kind;
        local_report.last_task_kind = step_report.task_kind;
        local_report.final_address = cpu->ip;
        if (status != VF2_OK) {
            break;
        }

        ++local_report.blocks_executed;
        local_report.recovered_instruction_count +=
            step_report.recovered_instruction_count;
        local_report.recovered_procedure_calls += step_report.recovered_procedure_calls;
        local_report.recovered_procedure_returns +=
            step_report.recovered_procedure_returns;

        if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_TASK) {
            ++local_report.task_bodies_executed;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
            ++local_report.frame_wait_phases;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER) {
            ++local_report.scheduler_entries;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION ||
                   step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_SCAN) {
            ++local_report.scheduler_transitions;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH) {
            ++local_report.scheduler_finishes;
        }
    }

    if (status == VF2_OK && cpu->ip == stop_address) {
        local_report.reached_stop = 1;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    local_report.final_address = cpu->ip;
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

vf2_status vf2_native_runtime_run_frame(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                        vf2_native_runtime_state *state,
                                        size_t max_blocks,
                                        vf2_native_runtime_run_report *report) {
    vf2_native_runtime_run_report local_report;
    const size_t starting_frame_phases =
        state != NULL ? state->frame_wait_phases : 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || state == NULL || max_blocks == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    local_report.start_address = cpu->ip;
    /* A frame run is phase-bounded rather than address-bounded. UINT32_MAX
     * makes that distinction visible to callers inspecting the report. */
    local_report.stop_address = UINT32_MAX;
    local_report.final_address = cpu->ip;

    while (state->frame_wait_phases == starting_frame_phases &&
           local_report.blocks_executed < max_blocks) {
        vf2_native_runtime_step_report step_report;

        memset(&step_report, 0, sizeof(step_report));
        local_report.last_entry_address = cpu->ip;
        status = vf2_native_runtime_step(machine, cpu, state, &step_report);
        local_report.last_step_kind = step_report.kind;
        local_report.last_bridge_kind = step_report.bridge_kind;
        local_report.last_task_kind = step_report.task_kind;
        local_report.final_address = cpu->ip;
        if (status != VF2_OK) {
            break;
        }
        ++local_report.blocks_executed;
        local_report.recovered_instruction_count +=
            step_report.recovered_instruction_count;
        local_report.recovered_procedure_calls +=
            step_report.recovered_procedure_calls;
        local_report.recovered_procedure_returns +=
            step_report.recovered_procedure_returns;
        if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_TASK) {
            ++local_report.task_bodies_executed;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT) {
            ++local_report.frame_wait_phases;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER) {
            ++local_report.scheduler_entries;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION ||
                   step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_SCAN) {
            ++local_report.scheduler_transitions;
        } else if (step_report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH) {
            ++local_report.scheduler_finishes;
        }
    }

    if (status == VF2_OK && state->frame_wait_phases > starting_frame_phases) {
        local_report.reached_stop = 1;
    } else if (status == VF2_OK) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    local_report.final_address = cpu->ip;
    if (report != NULL) {
        *report = local_report;
    }
    return status;
}

const char *vf2_native_runtime_step_kind_name(vf2_native_runtime_step_kind kind) {
    switch (kind) {
    case VF2_NATIVE_RUNTIME_STEP_NONE:
        return "none";
    case VF2_NATIVE_RUNTIME_STEP_BRIDGE:
        return "bridge";
    case VF2_NATIVE_RUNTIME_STEP_TASK:
        return "task";
    case VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT:
        return "frame-wait";
    case VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER:
        return "second-scheduler";
    case VF2_NATIVE_RUNTIME_STEP_SCHEDULER_TRANSITION:
        return "scheduler-transition";
    case VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH:
        return "scheduler-finish";
    case VF2_NATIVE_RUNTIME_STEP_SCHEDULER_EPILOGUE:
        return "scheduler-epilogue";
    case VF2_NATIVE_RUNTIME_STEP_SCHEDULER_SCAN:
        return "scheduler-scan";
    case VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE1:
        return "boot-stage1";
    case VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE2:
        return "boot-stage2";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INIT_PREFIX:
        return "post-boot-init-prefix";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_INIT_ENTRY:
        return "post-boot-video-init-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_RAMP:
        return "post-boot-video-ramp";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COLOR_TABLES:
        return "post-boot-color-tables";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MEMORY_CLEAR:
        return "post-boot-memory-clear";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_REGISTER_STREAM:
        return "post-boot-register-stream";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BLOCK_STREAM:
        return "post-boot-block-stream";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_SRAM_PROBE:
        return "post-boot-backup-sram-probe";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_RESTORE:
        return "post-boot-backup-restore";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESTORED_VIDEO_ENTRY:
        return "post-boot-restored-video-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_SEED:
        return "post-boot-palette-seed";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TABLE_INIT:
        return "post-boot-table-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_HARDWARE_CORE_INIT:
        return "post-boot-hardware-core-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_INIT_ENTRY:
        return "post-boot-texture-init-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_TIMER_ENTRY:
        return "post-boot-texture-timer-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN:
        return "post-boot-early-wait-return";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_VERIFY:
        return "post-boot-graphics-verify";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_ENTRY:
        return "post-boot-texture-record-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_SETUP:
        return "post-boot-texture-record-setup";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_LUMA_TABLE_INIT:
        return "post-boot-luma-table-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY:
        return "post-boot-early-wait-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN:
        return "post-boot-geometry-pattern";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN_RETURN:
        return "post-boot-geometry-pattern-return";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_INIT:
        return "post-boot-geometry-table-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_RETURN:
        return "post-boot-geometry-table-return";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_STATE_RESET:
        return "post-boot-graphics-state-reset";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_CONSTANTS:
        return "post-boot-video-constants";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_CONSTANTS:
        return "post-boot-display-constants";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TASK_REGISTRY_INIT:
        return "post-boot-task-registry-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_BUFFER_INIT:
        return "post-boot-graphics-buffer-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RENDER_STATE_INIT:
        return "post-boot-render-state-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DEFAULTS_INIT:
        return "post-boot-game-defaults-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_OBJECT_TABLE_INIT:
        return "post-boot-object-table-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EFFECT_TABLE_INIT:
        return "post-boot-effect-table-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_RING_INIT:
        return "post-boot-input-ring-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_IO_INIT:
        return "post-boot-io-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DATA_COPY:
        return "post-boot-game-data-copy";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_OFFSET_INIT:
        return "post-boot-display-offset-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FRAME_ACCUMULATOR_INIT:
        return "post-boot-frame-accumulator-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PROFILE_DEFAULTS_INIT:
        return "post-boot-profile-defaults-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAMEPLAY_GLOBALS_INIT:
        return "post-boot-gameplay-globals-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_ENTRY:
        return "post-boot-input-profile-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FLOAT_DEFAULTS_INIT:
        return "post-boot-float-defaults-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_LOAD:
        return "post-boot-input-profile-load";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_RAMP_ENTRY:
        return "post-boot-palette-ramp-entry";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD:
        return "post-boot-palette-build";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD_RETURN:
        return "post-boot-palette-build-return";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_WRAPPER_PREFIX:
        return "post-boot-resumed-wrapper-prefix";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_HELPER_INIT:
        return "post-boot-resumed-helper-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_TABLE:
        return "post-boot-resumed-luma-table";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_RETURN:
        return "post-boot-resumed-luma-return";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MAIN_LOOP_INIT:
        return "post-boot-main-loop-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COPRO_INIT:
        return "post-boot-copro-init";
    case VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DELAY:
        return "post-boot-delay";
    default:
        return "unknown";
    }
}
