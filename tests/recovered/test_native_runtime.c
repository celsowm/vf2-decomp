#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/native_runtime.h"

static int failures = 0;

#define CHECK(expression)                                                              \
    do {                                                                               \
        if (!(expression)) {                                                           \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expression);    \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

static void enter_parent(vf2_i960_cpu *cpu, uint32_t target) {
    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(cpu, target, UINT32_C(0x00001004)) == VF2_OK);
}

static void test_initialize_and_names(void) {
    vf2_native_runtime_state state;

    memset(&state, 0xff, sizeof(state));
    CHECK(vf2_native_runtime_initialize(NULL, 4u) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_native_runtime_initialize(&state, 0u) == VF2_ERROR_INVALID_ARGUMENT);
    CHECK(state.blocks_executed == 0u);
    CHECK(state.frame_wait.visits_before_interrupt == 0u);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    CHECK(state.frame_wait.visits_before_interrupt == 4u);
    CHECK(state.blocks_executed == 0u);
    CHECK(strcmp(vf2_native_runtime_step_kind_name(
                     VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER),
                 "second-scheduler") == 0);
    CHECK(strcmp(vf2_native_runtime_step_kind_name(VF2_NATIVE_RUNTIME_STEP_TASK),
                 "task") == 0);
    CHECK(strcmp(vf2_native_runtime_step_kind_name(VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE1),
                 "boot-stage1") == 0);
    CHECK(strcmp(vf2_native_runtime_step_kind_name(VF2_NATIVE_RUNTIME_STEP_BOOT_STAGE2),
                 "boot-stage2") == 0);
    CHECK(strcmp(vf2_native_runtime_step_kind_name(
                     VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INIT_PREFIX),
                 "post-boot-init-prefix") == 0);
    {
        static const vf2_native_runtime_step_kind kinds[] = {
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_INIT_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_RAMP,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COLOR_TABLES,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MEMORY_CLEAR,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_REGISTER_STREAM,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BLOCK_STREAM,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_SRAM_PROBE,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_RESTORE,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESTORED_VIDEO_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_SEED,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TABLE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_HARDWARE_CORE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_INIT_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_TIMER_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_VERIFY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_SETUP,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_LUMA_TABLE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN_RETURN,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_RETURN,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_STATE_RESET,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_CONSTANTS,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_CONSTANTS,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TASK_REGISTRY_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_BUFFER_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RENDER_STATE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DEFAULTS_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_OBJECT_TABLE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EFFECT_TABLE_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_RING_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_IO_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DATA_COPY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_OFFSET_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FRAME_ACCUMULATOR_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PROFILE_DEFAULTS_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAMEPLAY_GLOBALS_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FLOAT_DEFAULTS_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_LOAD,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_RAMP_ENTRY,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD_RETURN,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_WRAPPER_PREFIX,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_HELPER_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_MAIN_LOOP_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_COPRO_INIT,
            VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DELAY};
        static const char *const names[] = {"post-boot-video-init-entry",
                                            "post-boot-video-ramp",
                                            "post-boot-color-tables",
                                            "post-boot-memory-clear",
                                            "post-boot-register-stream",
                                            "post-boot-block-stream",
                                            "post-boot-backup-sram-probe",
                                            "post-boot-backup-restore",
                                            "post-boot-restored-video-entry",
                                            "post-boot-palette-seed",
                                            "post-boot-table-init",
                                            "post-boot-hardware-core-init",
                                            "post-boot-texture-init-entry",
                                            "post-boot-texture-timer-entry",
                                            "post-boot-early-wait-return",
                                            "post-boot-graphics-verify",
                                            "post-boot-texture-record-entry",
                                            "post-boot-texture-record-setup",
                                            "post-boot-luma-table-init",
                                            "post-boot-early-wait-entry",
                                            "post-boot-geometry-pattern",
                                            "post-boot-geometry-pattern-return",
                                            "post-boot-geometry-table-init",
                                            "post-boot-geometry-table-return",
                                            "post-boot-graphics-state-reset",
                                            "post-boot-video-constants",
                                            "post-boot-display-constants",
                                            "post-boot-task-registry-init",
                                            "post-boot-graphics-buffer-init",
                                            "post-boot-render-state-init",
                                            "post-boot-game-defaults-init",
                                            "post-boot-object-table-init",
                                            "post-boot-effect-table-init",
                                            "post-boot-input-ring-init",
                                            "post-boot-io-init",
                                            "post-boot-game-data-copy",
                                            "post-boot-display-offset-init",
                                            "post-boot-frame-accumulator-init",
                                            "post-boot-profile-defaults-init",
                                            "post-boot-gameplay-globals-init",
                                            "post-boot-input-profile-entry",
                                            "post-boot-float-defaults-init",
                                            "post-boot-input-profile-load",
                                            "post-boot-palette-ramp-entry",
                                            "post-boot-palette-build",
                                            "post-boot-palette-build-return",
                                            "post-boot-resumed-wrapper-prefix",
                                            "post-boot-resumed-helper-init",
                                            "post-boot-main-loop-init",
                                            "post-boot-copro-init",
                                            "post-boot-delay"};
        size_t index = 0u;
        for (index = 0u; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
            CHECK(strcmp(vf2_native_runtime_step_kind_name(kinds[index]),
                         names[index]) == 0);
        }
    }
}


static void test_post_boot_delay(void) {
    uint8_t *rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint64_t start_instructions = UINT64_C(1234);

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&state, 0, sizeof(state));
    memset(&report, 0, sizeof(report));
    CHECK(rom != NULL);
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        vf2_model2a_shutdown(&machine);
        return;
    }
    memcpy(rom + 0x00009f84u, "01234567890123456789", 20u);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(
              &cpu, UINT32_C(0x00002000), UINT32_C(0x00001004)) == VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(
              &cpu, UINT32_C(0x00003000), UINT32_C(0x00002004)) == VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(
              &cpu, UINT32_C(0x00009f74), UINT32_C(0x00003004)) == VF2_OK);
    cpu.executed_instructions = start_instructions;
    cpu.procedure_calls = UINT64_C(40);
    cpu.procedure_returns = UINT64_C(20);

    CHECK(cpu.local_frame_depth == 3u);
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DELAY);
    CHECK(report.entry_address == UINT32_C(0x00009f74));
    CHECK(report.exit_address == UINT32_C(0x00009fb0));
    CHECK(report.recovered_instruction_count == UINT64_C(2100198));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00009fb0));
    CHECK(cpu.local_frame_depth == 3u);
    CHECK(cpu.executed_instructions == start_instructions + UINT64_C(2100198));
    CHECK(cpu.procedure_calls == UINT64_C(41));
    CHECK(cpu.procedure_returns == UINT64_C(21));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_post_boot_texture_init_prefix(void) {
    static const uint32_t cleared_words[] = {
        UINT32_C(0x0055000c), UINT32_C(0x00550080), UINT32_C(0x005500f4),
        UINT32_C(0x005502c0), UINT32_C(0x005502d0), UINT32_C(0x005502e0)};
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    size_t index = 0u;

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    for (index = 0u; index < sizeof(cleared_words) / sizeof(cleared_words[0]);
         ++index) {
        CHECK(vf2_model2a_write_u32(&machine, cleared_words[index],
                                    UINT32_C(0xdeadbeef)) == VF2_OK);
    }
    enter_parent(&cpu, UINT32_C(0x000098b0));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_INIT_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x000098b0));
       CHECK(report.exit_address == UINT32_C(0x0004afb4));
       CHECK(report.recovered_instruction_count == UINT64_C(15));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(cpu.local_frame_depth == 3u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    for (index = 0u; index < sizeof(cleared_words) / sizeof(cleared_words[0]);
         ++index) {
        uint32_t value = UINT32_MAX;
        CHECK(vf2_model2a_read_u32(&machine, cleared_words[index], &value) == VF2_OK);
        CHECK(value == 0u);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_TIMER_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x0004afb4));
    CHECK(report.exit_address == UINT32_C(0x00000b6c));
    CHECK(report.recovered_instruction_count == UINT64_C(8));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 4u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(171798791));
    CHECK(cpu.local_frames[3].registers[14] == UINT32_C(0x000fffff));
    CHECK(cpu.local_frames[3].registers[15] == UINT32_C(0xffffffee));
    CHECK(state.blocks_executed == 2u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_TIMER_WAIT_UPDATE);
    CHECK(report.entry_address == UINT32_C(0x00000b6c));
    CHECK(report.exit_address == UINT32_C(0x0004afdc));
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 3u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_FRAME_WAIT_POLL);
    CHECK(report.entry_address == UINT32_C(0x0004afdc));
    CHECK(report.exit_address == UINT32_C(0x0004afe4));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[3] == 0u);
    CHECK(state.frame_wait.visits == 1u);
    CHECK(state.blocks_executed == 4u);

    vf2_model2a_shutdown(&machine);
}

static void test_post_boot_texture_wait_poll(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint16_t result = 0u;
    uint8_t frame_byte = 0u;

    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    CHECK(rom != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff410) + UINT32_C(20),
                                UINT32_C(0x005ff000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff410) + UINT32_C(24),
                                UINT32_C(0x005ff500)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff000) + UINT32_C(52),
                                UINT32_C(0x00000d20)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, UINT32_C(0x005ff410), UINT32_C(0x1000));
    cpu.registers[1] = UINT32_C(0x00501000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000098b0), UINT32_C(0x1004)) ==
          VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0004b020),
                                       UINT32_C(0x000098b4)) == VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0004afb4),
                                       UINT32_C(0x0004b07c)) == VF2_OK);
    cpu.ip = UINT32_C(0x0004afdc);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x0004afdc));
    CHECK(report.exit_address == UINT32_C(0x0004afe4));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(state.frame_wait.visits == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x0004afe4));
    CHECK(report.exit_address == UINT32_C(0x00000d20));
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(cpu.local_frame_depth == 4u);
    CHECK(state.frame_wait.interrupts_injected == 1u);

    frame_byte = 1u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_byte,
                            sizeof(frame_byte)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x00000d20));
    CHECK(report.exit_address == UINT32_C(0x0004afe4));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 3u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x0004afe4));
    CHECK(report.exit_address == UINT32_C(0x00000f7c));
    CHECK(report.recovered_instruction_count == UINT64_C(5));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 4u);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0055c2f2), &result, sizeof(result)) ==
          VF2_OK);
    CHECK(result == UINT16_C(1));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x00000f7c));
    CHECK(report.exit_address == UINT32_C(0x00000d20));
    CHECK(report.recovered_instruction_count == UINT64_C(4));

    frame_byte = 2u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_byte,
                            sizeof(frame_byte)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.entry_address == UINT32_C(0x00000d20));
    CHECK(report.exit_address == UINT32_C(0x00000f7c));
    CHECK(report.recovered_instruction_count == UINT64_C(1));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x00000f7c));
    CHECK(report.exit_address == UINT32_C(0x00002ec4));
    CHECK(report.recovered_instruction_count == UINT64_C(6));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));
    CHECK(report.recovered_instruction_count == UINT64_C(4));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.entry_address == UINT32_C(0x00000f9c));
    CHECK(report.exit_address == UINT32_C(0x0004b07c));
    CHECK(report.recovered_instruction_count == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.local_frame_depth == 2u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void write_u32_bytes(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
    data[offset + 2u] = (uint8_t)(value >> 16u);
    data[offset + 3u] = (uint8_t)(value >> 24u);
}

static uint16_t read_test_u16(const vf2_model2a *machine, uint32_t address) {
    uint8_t bytes[2] = {0u, 0u};
    CHECK(vf2_model2a_read(machine, address, bytes, sizeof(bytes)) == VF2_OK);
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8u);
}

static uint32_t read_test_u32(const vf2_model2a *machine, uint32_t address) {
    uint8_t bytes[4] = {0u, 0u, 0u, 0u};
    CHECK(vf2_model2a_read(machine, address, bytes, sizeof(bytes)) == VF2_OK);
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

static void test_post_boot_graphics_verify(void) {
    static const uint32_t records[] = {UINT32_C(0x00550168), UINT32_C(0x00550188),
                                       UINT32_C(0x005501a8), UINT32_C(0x005501c8),
                                       UINT32_C(0x005501e8), UINT32_C(0x00550208),
                                       UINT32_C(0x00550228), UINT32_C(0x00550248),
                                       UINT32_C(0x00550268), UINT32_C(0x00550288)};
    uint8_t *rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    uint8_t *main_data = (uint8_t *)calloc(1u, UINT32_C(0x00b00004));
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    size_t index = 0u;
    const uint8_t frame_ready = UINT8_C(2);

    CHECK(rom != NULL);
    CHECK(main_data != NULL);
    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(main_data);
        free(rom);
        return;
    }

    write_u32_bytes(rom, 0x0004ad74u, UINT32_C(0x11111111));
    for (index = 0u; index < 6u; ++index) {
        write_u32_bytes(rom, 0x0006e2b4u + index * 4u,
                        UINT32_C(0x11110000) + (uint32_t)index);
        write_u32_bytes(rom, 0x00007f64u + index * 4u,
                        UINT32_C(0x22220000) + (uint32_t)index);
    }
    write_u32_bytes(rom, 0x00011d94u, UINT32_C(29));
    for (index = 0u; index < 29u; ++index) {
        const size_t descriptor = 0x00011dc0u + index * 0x40u;
        write_u32_bytes(rom, descriptor, UINT32_C(0x80000000));
        write_u32_bytes(rom, descriptor + 4u, (uint32_t)index);
        write_u32_bytes(rom, descriptor + 8u, UINT32_C(0x80));
        write_u32_bytes(rom, descriptor + 0x0cu,
                        UINT32_C(0x00020000) + (uint32_t)index * UINT32_C(4));
        write_u32_bytes(rom, descriptor + 0x10u,
                        UINT32_C(0x00500800) + (uint32_t)index * UINT32_C(4));
        write_u32_bytes(rom, descriptor + 0x14u, 0u);
    }
    for (index = 0u; index < 40u; ++index) {
        write_u32_bytes(rom, 0x00023ca0u + index * 4u,
                        UINT32_C(0x3f000000) + (uint32_t)index);
    }
    for (index = 0u; index < 14u; ++index) {
        rom[0x00023d40u + index] = (uint8_t)(0x80u + index);
    }
    for (index = 0u; index < 0x1000u; ++index) {
        rom[0x0007ae10u + index] = (uint8_t)(index * 5u + 3u);
    }
    write_u32_bytes(main_data, 0x0001fe68u, UINT32_C(0x02410000));
    for (index = 0u; index < 2817u * 16u; ++index) {
        main_data[0x00410000u + index] = (uint8_t)(index * 13u + 7u);
    }
    for (index = 0u; index < 0x30000u; ++index) {
        main_data[0x003d0000u + index] = (uint8_t)(index * 11u + 5u);
    }
    write_u32_bytes(main_data, 0x00300000u, UINT32_C(0x11111111));
    write_u32_bytes(main_data, 0x00300004u, UINT32_C(0x02302000));
    write_u32_bytes(main_data, 0x00301000u, UINT32_C(0x22222222));
    write_u32_bytes(main_data, 0x00400000u, UINT32_C(0x33333333));
    write_u32_bytes(main_data, 0x00600000u, UINT32_C(0x44444444));
    write_u32_bytes(main_data, 0x00b00000u, UINT32_C(0x55555555));
    write_u32_bytes(main_data, 0x00302000u, UINT32_C(0x22222222));
    write_u32_bytes(main_data, 0x00302004u, UINT32_C(0x33333333));
    write_u32_bytes(main_data, 0x00302008u, UINT32_C(0x44444444));
    write_u32_bytes(main_data, 0x0030200cu, UINT32_C(0x55555555));
    write_u32_bytes(rom, 0x00078d0cu, UINT32_C(66));
    for (index = 0u; index < 66u * 128u; ++index) {
        rom[0x00078d10u + index] = (uint8_t)(index * 3u + 1u);
    }
    write_u32_bytes(rom, 0x00011d64u, UINT32_C(0x22492492));
    write_u32_bytes(rom, 0x00011d68u, UINT32_C(0x58926162));
    write_u32_bytes(rom, 0x00011d6cu, UINT32_C(0xc8a23189));
    write_u32_bytes(rom, 0x00011d70u, UINT32_C(0x8a5658c8));
    write_u32_bytes(rom, 0x00011d74u, UINT32_C(0x99968c99));
    write_u32_bytes(rom, 0x00011d78u, UINT32_C(0xcd699a35));
    write_u32_bytes(rom, 0x00011d7cu, UINT32_C(0x6a676699));
    write_u32_bytes(rom, 0x00011d80u, UINT32_C(0x6a76a9aa));
    write_u32_bytes(rom, 0x00011d84u, UINT32_C(0x59414c50));
    memcpy(rom + 0x00000fc4u, "I/O Initialize ...", 19u);
    memcpy(rom + 0x00000ffcu, "O", 2u);
    memcpy(rom + 0x00001024u, "K.", 3u);
    write_u32_bytes(rom, 0x000118e8u, 0u);
    write_u32_bytes(rom, 0x000118ecu, UINT32_C(32));
    for (index = 0u; index < 32u; ++index) {
        write_u32_bytes(rom, 0x000118f0u + index * 4u,
                        UINT32_C(0x00100000) + (uint32_t)index);
        write_u32_bytes(rom, 0x00011868u + index * 4u,
                        UINT32_C(0x3f000000) + (uint32_t)index);
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data, UINT32_C(0x00b00004)) ==
          VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00001000));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0004b020),
                                       UINT32_C(0x000098b4)) == VF2_OK);
    cpu.ip = UINT32_C(0x0004b07c);
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    cpu.compare_result = VF2_I960_COMPARE_LESS;
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_VERIFY);
    CHECK(report.entry_address == UINT32_C(0x0004b07c));
    CHECK(report.exit_address == UINT32_C(0x0004b820));
    CHECK(report.recovered_instruction_count == UINT64_C(82));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(40));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01000a28));
    CHECK(cpu.local_frames[1].registers[3] == UINT32_C(0x0000ffff));
    CHECK(cpu.local_frames[1].registers[5] == UINT32_C(0x005502a8));
    CHECK(cpu.local_frames[1].registers[6] == UINT32_C(0x005502a8));
    CHECK(cpu.local_frames[1].registers[7] == 0u);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001002));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x0004b820));
    CHECK(report.exit_address == UINT32_C(0x0004b9b8));
    CHECK(report.recovered_instruction_count == UINT64_C(4));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 3u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(0x00550168));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == 1u);

    {
        const uint16_t priority = UINT16_C(1);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x0055c2f0), &priority,
                                sizeof(priority)) == VF2_OK);
    }
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(cpu.ip == UINT32_C(0x0004b9b8));
    {
        uint16_t unchanged = 0u;
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x00550168), &unchanged,
                               sizeof(unchanged)) == VF2_OK);
        CHECK(unchanged == UINT16_MAX);
    }
    {
        const uint16_t priority = 0u;
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x0055c2f0), &priority,
                                sizeof(priority)) == VF2_OK);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TEXTURE_RECORD_SETUP);
    CHECK(report.entry_address == UINT32_C(0x0004b9b8));
    CHECK(report.exit_address == UINT32_C(0x000098b4));
    CHECK(report.recovered_instruction_count == UINT64_C(24));
    CHECK(report.recovered_procedure_calls == 0u);
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.arithmetic_control == UINT32_C(0x3f001001));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    for (index = 0u; index < sizeof(records) / sizeof(records[0]); ++index) {
        uint8_t values[4] = {0u, 0u, 0u, 0u};
        uint16_t tail = UINT16_MAX;
        CHECK(vf2_model2a_read(&machine, records[index], values, sizeof(values)) ==
              VF2_OK);
        CHECK(values[0] == (index == 0u ? UINT8_C(40) : UINT8_C(0xff)));
        CHECK(values[1] == (index == 0u ? 0u : UINT8_C(0xff)));
        CHECK(values[2] == (index == 0u ? UINT8_C(0xff) : 0u));
        CHECK(values[3] == (index == 0u ? UINT8_C(0xff) : 0u));
        CHECK(vf2_model2a_read(&machine, records[index] + UINT32_C(0x1c), &tail,
                               sizeof(tail)) == VF2_OK);
        CHECK(tail == (index == 0u ? UINT16_C(1) : 0u));
    }
    {
        uint32_t record_argument = UINT32_MAX;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00550178), &record_argument) ==
              VF2_OK);
        CHECK(record_argument == 0u);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_LUMA_TABLE_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098b4));
    CHECK(report.exit_address == UINT32_C(0x000098b8));
    CHECK(report.recovered_instruction_count == UINT64_C(50891));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x12808400));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x0007ae10));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 66u * 128u; index += 257u) {
        uint32_t value = UINT32_MAX;
        CHECK(vf2_model2a_read_u32(&machine,
                                   UINT32_C(0x12800000) + (uint32_t)index * 4u,
                                   &value) == VF2_OK);
        CHECK(value == (uint32_t)(uint8_t)(index * 3u + 1u));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x000098b8));
    CHECK(report.exit_address == UINT32_C(0x00000f7c));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 1u);

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x00000f7c));
    CHECK(report.exit_address == UINT32_C(0x00002ec4));
    CHECK(report.recovered_instruction_count == UINT64_C(6));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 2u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.entry_address == UINT32_C(0x00002ec4));
    CHECK(report.exit_address == UINT32_C(0x00000f9c));
    CHECK(report.recovered_instruction_count == UINT64_C(4));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.entry_address == UINT32_C(0x00000f9c));
    CHECK(report.exit_address == UINT32_C(0x000098bc));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);

    cpu.registers[VF2_I960_G0_REGISTER + 10u] = VF2_GEOMETRY_BASE;
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x4000);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN);
    CHECK(report.entry_address == UINT32_C(0x000098bc));
    CHECK(report.exit_address == UINT32_C(0x00002edc));
    CHECK(report.recovered_instruction_count == UINT64_C(63799));
    CHECK(report.recovered_procedure_calls == UINT64_C(2050));
    CHECK(report.recovered_procedure_returns == UINT64_C(2048));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x00011d70));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(0x000000de));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0x00000029));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x00003228));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 5u] == UINT32_C(0x00000043));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 6u] == UINT32_C(7));
    CHECK(cpu.local_frames[1].registers[6] == UINT32_C(0x800));
    CHECK(cpu.local_frames[1].registers[7] == 0u);
    CHECK(cpu.local_frames[1].registers[8] == 0u);
    CHECK(cpu.local_frames[1].registers[9] == UINT32_C(4));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    {
        uint32_t pattern = 0u;
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
                                   &pattern) == VF2_OK);
        CHECK(pattern == UINT32_C(0xffffffff));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT);
    CHECK(report.entry_address == UINT32_C(0x00002edc));
    CHECK(report.exit_address == UINT32_C(0x00011798));
    CHECK(report.recovered_instruction_count == UINT64_C(20));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x00011798));
    CHECK(report.exit_address == UINT32_C(0x00000f7c));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 2u);

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.entry_address == UINT32_C(0x00000f7c));
    CHECK(report.exit_address == UINT32_C(0x00002ec4));
    CHECK(report.recovered_instruction_count == UINT64_C(6));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.entry_address == UINT32_C(0x00000f9c));
    CHECK(report.exit_address == UINT32_C(0x0001179c));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN);
    CHECK(report.entry_address == UINT32_C(0x0001179c));
    CHECK(report.exit_address == UINT32_C(0x00002edc));
    CHECK(report.recovered_instruction_count == UINT64_C(63742));
    CHECK(report.recovered_procedure_calls == UINT64_C(2049));
    CHECK(report.recovered_procedure_returns == UINT64_C(2048));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x00011d78));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(0x0000010a));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0x0000004b));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x00000266));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 5u] == UINT32_C(0x000000a8));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 6u] == UINT32_C(5));
    CHECK(cpu.local_frames[1].registers[6] == UINT32_C(0x1000));
    CHECK(cpu.local_frames[1].registers[7] == UINT32_C(0x2000));
    CHECK(cpu.local_frames[1].registers[8] == 0u);
    CHECK(cpu.local_frames[1].registers[9] == UINT32_C(3));
    {
        uint32_t pattern = 0u;
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
                                   &pattern) == VF2_OK);
        CHECK(pattern == UINT32_C(0xffffffff));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT);
    CHECK(report.entry_address == UINT32_C(0x00002edc));
    CHECK(report.exit_address == UINT32_C(0x00011798));
    CHECK(report.recovered_instruction_count == UINT64_C(20));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.exit_address == UINT32_C(0x00000f7c));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.exit_address == UINT32_C(0x00002ec4));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.exit_address == UINT32_C(0x0001179c));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN);
    CHECK(report.entry_address == UINT32_C(0x0001179c));
    CHECK(report.exit_address == UINT32_C(0x00002edc));
    CHECK(report.recovered_instruction_count == UINT64_C(63700));
    CHECK(report.recovered_procedure_calls == UINT64_C(2049));
    CHECK(report.recovered_procedure_returns == UINT64_C(2048));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x00011d80));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(0x00000136));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0x00000067));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x0001a99d));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 5u] == UINT32_C(0x00000033));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 6u] == UINT32_C(9));
    CHECK(cpu.local_frames[1].registers[6] == UINT32_C(0x1800));
    CHECK(cpu.local_frames[1].registers[7] == UINT32_C(0x4000));
    CHECK(cpu.local_frames[1].registers[8] == 0u);
    CHECK(cpu.local_frames[1].registers[9] == UINT32_C(2));
    {
        uint32_t pattern = 0u;
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
                                   &pattern) == VF2_OK);
        CHECK(pattern == UINT32_C(0xffffffff));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT);
    CHECK(report.entry_address == UINT32_C(0x00002edc));
    CHECK(report.exit_address == UINT32_C(0x00011798));
    CHECK(report.recovered_instruction_count == UINT64_C(20));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.exit_address == UINT32_C(0x00000f7c));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.exit_address == UINT32_C(0x00002ec4));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.exit_address == UINT32_C(0x0001179c));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN);
    CHECK(report.entry_address == UINT32_C(0x0001179c));
    CHECK(report.exit_address == UINT32_C(0x00002edc));
    CHECK(report.recovered_instruction_count == UINT64_C(63679));
    CHECK(report.recovered_procedure_calls == UINT64_C(2049));
    CHECK(report.recovered_procedure_returns == UINT64_C(2048));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x00011d88));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == UINT32_C(0x00000162));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0x00000080));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x59414c50));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 5u] == UINT32_C(0x000000b2));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 6u] == UINT32_C(16));
    CHECK(cpu.local_frames[1].registers[6] == UINT32_C(0x2000));
    CHECK(cpu.local_frames[1].registers[7] == UINT32_C(0x6000));
    CHECK(cpu.local_frames[1].registers[8] == 0u);
    CHECK(cpu.local_frames[1].registers[9] == UINT32_C(1));
    {
        uint32_t pattern = 0u;
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
                                   &pattern) == VF2_OK);
        CHECK(pattern == UINT32_C(0xffffffff));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT);
    CHECK(report.entry_address == UINT32_C(0x00002edc));
    CHECK(report.exit_address == UINT32_C(0x00011798));
    CHECK(report.recovered_instruction_count == UINT64_C(20));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.exit_address == UINT32_C(0x00000f7c));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.exit_address == UINT32_C(0x00002ec4));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.exit_address == UINT32_C(0x0001179c));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_PATTERN_RETURN);
    CHECK(report.entry_address == UINT32_C(0x0001179c));
    CHECK(report.exit_address == UINT32_C(0x000098c0));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[9] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x000098c0));
    CHECK(report.exit_address == UINT32_C(0x00000f7c));
    CHECK(report.recovered_instruction_count == UINT64_C(1));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.exit_address == UINT32_C(0x00002ec4));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.exit_address == UINT32_C(0x000098c4));
    CHECK(cpu.local_frame_depth == 0u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098c4));
    CHECK(report.exit_address == UINT32_C(0x00002edc));
    CHECK(report.recovered_instruction_count == UINT64_C(281));
    CHECK(report.recovered_procedure_calls == UINT64_C(4));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(1));
    CHECK(cpu.local_frames[1].registers[3] == UINT32_C(0x3f00001f));
    CHECK(cpu.local_frames[1].registers[4] == UINT32_C(0x00011970));
    CHECK(cpu.local_frames[1].registers[5] == 0u);
    CHECK(cpu.local_frames[1].registers[6] == UINT32_C(0x000118e8));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x60),
                                   &value) == VF2_OK);
        CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x70),
                                   &value) == VF2_OK);
        CHECK(value == 0u);
        CHECK(vf2_model2a_read_u32(&machine, VF2_GEOMETRY_BASE + UINT32_C(0x4000),
                                   &value) == VF2_OK);
        CHECK(value == UINT32_C(0xffffffff));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_GEOMETRY_FRAME_COMMIT);
    CHECK(report.entry_address == UINT32_C(0x00002edc));
    CHECK(report.exit_address == UINT32_C(0x00011860));
    CHECK(report.recovered_instruction_count == UINT64_C(20));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x00011860));
    CHECK(report.exit_address == UINT32_C(0x00000f7c));

    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &frame_ready,
                            sizeof(frame_ready)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(report.exit_address == UINT32_C(0x00002ec4));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.bridge_kind == VF2_HYBRID_BRIDGE_VIDEO_STATUS_LATCH);
    CHECK(report.exit_address == UINT32_C(0x00000f9c));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EARLY_WAIT_RETURN);
    CHECK(report.exit_address == UINT32_C(0x00011864));
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GEOMETRY_TABLE_RETURN);
    CHECK(report.entry_address == UINT32_C(0x00011864));
    CHECK(report.exit_address == UINT32_C(0x000098c8));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);

    {
        const uint16_t nonzero = UINT16_MAX;
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502a8), &nonzero,
                                sizeof(nonzero)) == VF2_OK);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502b0), &nonzero,
                                sizeof(nonzero)) == VF2_OK);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x005502b8), &nonzero,
                                sizeof(nonzero)) == VF2_OK);
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00546000), UINT32_MAX) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_STATE_RESET);
    CHECK(report.entry_address == UINT32_C(0x000098c8));
    CHECK(report.exit_address == UINT32_C(0x000098cc));
    CHECK(report.recovered_instruction_count == UINT64_C(10));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    {
        static const uint32_t addresses[] = {UINT32_C(0x005502a8), UINT32_C(0x005502b0),
                                             UINT32_C(0x005502b8),
                                             UINT32_C(0x00546000)};
        size_t address_index = 0u;
        for (address_index = 0u;
             address_index < sizeof(addresses) / sizeof(addresses[0]);
             ++address_index) {
            uint16_t value = UINT16_MAX;
            CHECK(vf2_model2a_read(&machine, addresses[address_index], &value,
                                   sizeof(value)) == VF2_OK);
            CHECK(value == 0u);
        }
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_CONSTANTS);
    CHECK(report.entry_address == UINT32_C(0x000098cc));
    CHECK(report.exit_address == UINT32_C(0x000098d0));
    CHECK(report.recovered_instruction_count == UINT64_C(16));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x0006e2b4));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x00501500));
    for (index = 0u; index < 6u; ++index) {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine,
                                   UINT32_C(0x00501500) + (uint32_t)index * 4u,
                                   &value) == VF2_OK);
        CHECK(value == UINT32_C(0x11110000) + (uint32_t)index);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_CONSTANTS);
    CHECK(report.entry_address == UINT32_C(0x000098d0));
    CHECK(report.exit_address == UINT32_C(0x000098d4));
    CHECK(report.recovered_instruction_count == UINT64_C(8));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    for (index = 0u; index < 6u; ++index) {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine,
                                   UINT32_C(0x00501400) + (uint32_t)index * 4u,
                                   &value) == VF2_OK);
        CHECK(value == UINT32_C(0x22220000) + (uint32_t)index);
    }

    memset(machine.work_ram + 0x0c000u, 0xa5, 29u * 0x20u);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_TASK_REGISTRY_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098d4));
    CHECK(report.exit_address == UINT32_C(0x000098d8));
    CHECK(report.descriptors_scanned == 29u);
    CHECK(report.recovered_instruction_count == UINT64_C(648));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x80));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 13u] == UINT32_C(0x00510e80));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 29u; ++index) {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine,
                                   UINT32_C(0x00500800) + (uint32_t)index * 4u,
                                   &value) == VF2_OK);
        CHECK(value == UINT32_C(0x00510000) + (uint32_t)index * 0x80u);
    }
    for (index = 0u; index < 29u * 0x20u; ++index) {
        CHECK(machine.work_ram[0x0c000u + index] == 0u);
    }

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500600), UINT32_MAX) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500604), UINT32_MAX) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500608), UINT32_MAX) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GRAPHICS_BUFFER_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098d8));
    CHECK(report.exit_address == UINT32_C(0x000098dc));
    CHECK(report.recovered_instruction_count == UINT64_C(8));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    {
        static const uint32_t addresses[] = {UINT32_C(0x00500600), UINT32_C(0x00500604),
                                             UINT32_C(0x00500608)};
        static const uint32_t expected[] = {UINT32_C(0x005d0000), UINT32_C(1),
                                            UINT32_C(0)};
        for (index = 0u; index < 3u; ++index) {
            uint32_t value = UINT32_MAX;
            CHECK(vf2_model2a_read_u32(&machine, addresses[index], &value) == VF2_OK);
            CHECK(value == expected[index]);
        }
    }

    memset(machine.work_ram + 0x09e000u, 0xa5, 0x2000u);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RENDER_STATE_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098dc));
    CHECK(report.exit_address == UINT32_C(0x000098e0));
    CHECK(report.recovered_instruction_count == UINT64_C(672));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059e008), &value) == VF2_OK);
        CHECK(value == UINT32_C(2));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059f278), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x0a000000));
    }
    for (index = 0u; index < 216u * 16u; ++index) {
        CHECK(machine.work_ram[0x09f280u + index] == 0u);
    }

    memset(machine.work_ram + 0x0a000u, 0xa5, 0x1000u);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DEFAULTS_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098e0));
    CHECK(report.exit_address == UINT32_C(0x000098e4));
    CHECK(report.recovered_instruction_count == UINT64_C(442));
    CHECK(report.recovered_procedure_calls == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 40u; ++index) {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine,
                                   UINT32_C(0x0050a800) + (uint32_t)index * 4u,
                                   &value) == VF2_OK);
        CHECK(value == UINT32_C(0x3f000000) + (uint32_t)index);
    }
    for (index = 0u; index < 26u; ++index) {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine,
                                   UINT32_C(0x0050a0e0) + (uint32_t)index * 4u,
                                   &value) == VF2_OK);
        CHECK(value == UINT32_C(0x3f800000));
    }
    {
        uint32_t game_state = 0u;
        uint32_t value = 0u;
        uint8_t byte = 0u;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500814), &game_state) ==
              VF2_OK);
        CHECK(vf2_model2a_read_u32(&machine, game_state + UINT32_C(0x20c), &value) ==
              VF2_OK);
        CHECK(value == UINT32_C(0x3fb33333));
        CHECK(vf2_model2a_read(&machine, game_state + UINT32_C(0x2d1), &byte,
                               sizeof(byte)) == VF2_OK);
        CHECK(byte == UINT8_C(60));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_OBJECT_TABLE_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098e4));
    CHECK(report.exit_address == UINT32_C(0x000098e8));
    CHECK(report.recovered_instruction_count == UINT64_C(11283));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0x02410000));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == UINT32_C(0x00560000));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 2817u * 16u; index += 257u) {
        uint8_t value = 0u;
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x00560000) + (uint32_t)index, &value,
                               sizeof(value)) == VF2_OK);
        CHECK(value == (uint8_t)(index * 13u + 7u));
    }
    {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005001a4), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x7f7f7f7f));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005001b4), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x7f7f7f7f));
    }

    memset(machine.work_ram + 0x035000u, 0xa5, 0x4000u);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_EFFECT_TABLE_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098e8));
    CHECK(report.exit_address == UINT32_C(0x000098ec));
    CHECK(report.recovered_instruction_count == UINT64_C(5652));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 0x1000u; index += 127u) {
        CHECK(machine.work_ram[0x031000u + index] == (uint8_t)(index * 5u + 3u));
    }
    for (index = 0u; index < 0x4000u; ++index) {
        CHECK(machine.work_ram[0x035000u + index] == 0u);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_RING_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098ec));
    CHECK(report.exit_address == UINT32_C(0x000098f0));
    CHECK(report.recovered_instruction_count == UINT64_C(6));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(machine.work_ram[0xfcu] == UINT8_C(2));
    CHECK(machine.work_ram[0xfdu] == UINT8_C(2));

    memset(machine.work_ram + 0x700u, 0xa5, 0x1cu);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_IO_INIT);
    CHECK(report.entry_address == UINT32_C(0x000098f0));
    CHECK(report.exit_address == UINT32_C(0x000098f4));
    CHECK(report.recovered_instruction_count == UINT64_C(268));
    CHECK(report.recovered_procedure_calls == UINT64_C(4));
    CHECK(report.recovered_procedure_returns == UINT64_C(4));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x00002e4b));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01000cd0));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(1));
    for (index = 0u; index < 0x10u; ++index) {
        CHECK(machine.work_ram[0x700u + index] == 0u);
    }
    for (index = 0x10u; index < 0x18u; ++index) {
        CHECK(machine.work_ram[0x700u + index] == UINT8_C(0xa5));
    }
    CHECK(machine.work_ram[0x718u] == 0u);
    CHECK(machine.work_ram[0x719u] == UINT8_C(0xa5));
    {
        static const char text[] = "I/O Initialize ...";
        uint8_t encoded[2] = {0u, 0u};
        for (index = 0u; index < sizeof(text) - 1u; ++index) {
            CHECK(vf2_model2a_read(&machine,
                                   UINT32_C(0x01000c28) + (uint32_t)index * 2u, encoded,
                                   sizeof(encoded)) == VF2_OK);
            CHECK(encoded[0] == (uint8_t)text[index]);
            CHECK(encoded[1] == UINT8_C(0x80));
        }
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x01000c4e), encoded,
                               sizeof(encoded)) == VF2_OK);
        CHECK(encoded[0] == (uint8_t)'O' && encoded[1] == UINT8_C(0x80));
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x01000c50), encoded,
                               sizeof(encoded)) == VF2_OK);
        CHECK(encoded[0] == (uint8_t)'K' && encoded[1] == UINT8_C(0x80));
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x01000c52), encoded,
                               sizeof(encoded)) == VF2_OK);
        CHECK(encoded[0] == (uint8_t)'.' && encoded[1] == UINT8_C(0x80));
    }

    memset(machine.work_ram + 0xa0000u, 0xa5, 0x30000u);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAME_DATA_COPY);
    CHECK(report.entry_address == UINT32_C(0x000098f4));
    CHECK(report.exit_address == UINT32_C(0x00009920));
    CHECK(report.recovered_instruction_count == UINT64_C(61443));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(0));
    CHECK(cpu.registers[3] == UINT32_C(0x02400000));
    CHECK(cpu.registers[4] == UINT32_C(0x005d0000));
    CHECK(cpu.registers[5] == UINT32_C(0x005d0000));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 0x30000u; index += 997u) {
        CHECK(machine.work_ram[0xa0000u + index] == (uint8_t)(index * 11u + 5u));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_DISPLAY_OFFSET_INIT);
    CHECK(report.entry_address == UINT32_C(0x00009920));
    CHECK(report.exit_address == UINT32_C(0x00009924));
    CHECK(report.recovered_instruction_count == UINT64_C(126));
    CHECK(report.recovered_procedure_calls == UINT64_C(6));
    CHECK(report.recovered_procedure_returns == UINT64_C(6));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x00002e4b));
    {
        static const uint32_t addresses[] = {
            UINT32_C(0x01d03380), UINT32_C(0x0059c380), UINT32_C(0x01d03384),
            UINT32_C(0x0059c384), UINT32_C(0x01d03388), UINT32_C(0x0059c388),
            UINT32_C(0x01d0338c), UINT32_C(0x0059c38c)};
        for (index = 0u; index < sizeof(addresses) / sizeof(addresses[0]); ++index) {
            uint8_t value = UINT8_MAX;
            CHECK(vf2_model2a_read(&machine, addresses[index], &value, sizeof(value)) ==
                  VF2_OK);
            CHECK(value == 0u);
        }
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FRAME_ACCUMULATOR_INIT);
    CHECK(report.entry_address == UINT32_C(0x00009924));
    CHECK(report.exit_address == UINT32_C(0x00009928));
    CHECK(report.recovered_instruction_count == UINT64_C(1178));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    {
        uint32_t counter = UINT32_MAX;
        uint8_t value = UINT8_MAX;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050d000), &counter) ==
              VF2_OK);
        CHECK(counter == UINT32_C(1));
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050d004), &value,
                               sizeof(value)) == VF2_OK);
        CHECK(value == 0u);
        CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050d005), &value,
                               sizeof(value)) == VF2_OK);
        CHECK(value == 0u);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PROFILE_DEFAULTS_INIT);
    CHECK(report.entry_address == UINT32_C(0x00009928));
    CHECK(report.exit_address == UINT32_C(0x0000992c));
    CHECK(report.recovered_instruction_count == UINT64_C(32));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    for (index = 0u; index < 12u; ++index) {
        CHECK(machine.work_ram[0x0a700u + index] == 0u);
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_GAMEPLAY_GLOBALS_INIT);
    CHECK(report.entry_address == UINT32_C(0x0000992c));
    CHECK(report.exit_address == UINT32_C(0x000099fc));
    CHECK(report.recovered_instruction_count == UINT64_C(30));
    CHECK(report.recovered_procedure_calls == 0u);
    CHECK(report.recovered_procedure_returns == 0u);
    CHECK(cpu.local_frame_depth == 0u);
    {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00501018), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x00001388));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x005013f0), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x00000080));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050a000), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x3b32674f));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050a004), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x3f800000));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050a008), &value) == VF2_OK);
        CHECK(value == UINT32_C(0x41200000));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0050a010), &value) == VF2_OK);
        CHECK(value == UINT32_C(0xbf000000));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x000099fc));
    CHECK(report.exit_address == UINT32_C(0x0001fdd0));
    CHECK(report.recovered_instruction_count == UINT64_C(18));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == 0u);
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_FLOAT_DEFAULTS_INIT);
    CHECK(report.entry_address == UINT32_C(0x0001fdd0));
    CHECK(report.exit_address == UINT32_C(0x0001fdd4));
    CHECK(report.recovered_instruction_count == UINT64_C(114));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.local_frame_depth == 1u);
    for (index = 0u; index < 26u; ++index) {
        uint32_t value = 0u;
        CHECK(vf2_model2a_read_u32(
                  &machine, UINT32_C(0x0050a0e0) + (uint32_t)index * 4u,
                  &value) == VF2_OK);
        CHECK(value == UINT32_C(0x3f800000));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INPUT_PROFILE_LOAD);
    CHECK(report.entry_address == UINT32_C(0x0001fdd4));
    CHECK(report.exit_address == UINT32_C(0x0001fe60));
    CHECK(report.recovered_instruction_count == UINT64_C(17));
    CHECK(report.recovered_procedure_calls == 0u);
    CHECK(report.recovered_procedure_returns == 0u);
    CHECK(cpu.local_frame_depth == 1u);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_RAMP_ENTRY);
    CHECK(report.entry_address == UINT32_C(0x0001fe60));
    CHECK(report.exit_address == UINT32_C(0x00002c38));
    CHECK(report.recovered_instruction_count == UINT64_C(12));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == 0u);
    CHECK(cpu.local_frame_depth == 3u);

    {
        static const uint8_t palette_inputs[] = {UINT8_C(3), UINT8_C(18),
                                                 UINT8_C(5), UINT8_C(18),
                                                 UINT8_C(7), UINT8_C(18)};
        static const uint8_t palette_multipliers[] = {UINT8_C(128), UINT8_C(128),
                                                      UINT8_C(128)};
        uint32_t page_value = 0u;
        memset(machine.work_ram + UINT32_C(0x46008), 0xa5, UINT32_C(0x120));
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500234), palette_inputs,
                                sizeof(palette_inputs)) == VF2_OK);
        CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000e0), palette_multipliers,
                                sizeof(palette_multipliers)) == VF2_OK);

        memset(&report, 0, sizeof(report));
        CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
        CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD);
        CHECK(report.entry_address == UINT32_C(0x00002c38));
        CHECK(report.exit_address == UINT32_C(0x00020050));
        CHECK(report.recovered_instruction_count == UINT64_C(39208));
        CHECK(report.recovered_procedure_calls == 0u);
        CHECK(report.recovered_procedure_returns == UINT64_C(1));
        CHECK(cpu.local_frame_depth == 2u);
        CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
        CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(2));
        CHECK(read_test_u16(&machine, UINT32_C(0x00546128)) == 0u);
        CHECK(read_test_u16(&machine, UINT32_C(0x0054612e)) == UINT16_C(3));
        CHECK(read_test_u16(&machine, UINT32_C(0x00546130)) == UINT16_C(5));
        CHECK(read_test_u16(&machine, UINT32_C(0x00546132)) == UINT16_C(7));
        CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00546000), &page_value) == VF2_OK);
        CHECK(page_value == UINT32_C(1));
    }

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_PALETTE_BUILD_RETURN);
    CHECK(report.entry_address == UINT32_C(0x00020050));
    CHECK(report.exit_address == UINT32_C(0x0001fe64));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_calls == 0u);
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 1u);

    /* The resumed 0x1fe64 wrapper loads the next table pair, runs the small
       0x4b410 registration helper, clears its state block, and reaches the
       still-open 0x2eab8 helper with the exact call boundary intact. */
    write_u32_bytes(rom, 0x0006eeb0u, UINT32_C(0x11111111));
    write_u32_bytes(rom, 0x0006eeb4u, UINT32_C(0x22222222));
    cpu.registers[4] = 0u;
    cpu.registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(0x33333333);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind ==
          VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_WRAPPER_PREFIX);
    CHECK(report.entry_address == UINT32_C(0x0001fe64));
    CHECK(report.exit_address == UINT32_C(0x0002eab8));
    CHECK(report.recovered_instruction_count == UINT64_C(27));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 2u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x11111111));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x22222222));
    CHECK(read_test_u32(&machine, UINT32_C(0x00550000)) == UINT32_C(1));
    CHECK(read_test_u32(&machine, UINT32_C(0x005502e0)) == UINT32_C(3));
    CHECK(read_test_u32(&machine, UINT32_C(0x005502e4)) == UINT32_C(0x11111111));
    CHECK(read_test_u32(&machine, UINT32_C(0x005502e8)) == UINT32_C(0x22222222));
    CHECK(read_test_u32(&machine, UINT32_C(0x005502ec)) == UINT32_C(0x33333333));
    CHECK(read_test_u32(&machine, UINT32_C(0x0050a014)) == 0u);
    CHECK(read_test_u32(&machine, UINT32_C(0x0050a018)) == 0u);
    CHECK(read_test_u32(&machine, UINT32_C(0x0050a01c)) == 0u);
    CHECK(read_test_u16(&machine, UINT32_C(0x0050a020)) == 0u);
    CHECK(read_test_u16(&machine, UINT32_C(0x0050a026)) == 0u);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500814),
                                UINT32_C(0x0050b000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050084c),
                                UINT32_C(0x0050b100)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_HELPER_INIT);
    CHECK(report.entry_address == UINT32_C(0x0002eab8));
    CHECK(report.exit_address == UINT32_C(0x0001fedc));
    CHECK(report.recovered_instruction_count == UINT64_C(84));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(read_test_u32(&machine, UINT32_C(0x0050a160)) == UINT32_C(0xc0900000));
    CHECK(read_test_u32(&machine, UINT32_C(0x0050b000) + UINT32_C(0x234)) ==
          UINT32_C(0x3c872b02));
    CHECK(read_test_u32(&machine, UINT32_C(0x0050b100) + UINT32_C(0x40)) == 0u);
    CHECK(read_test_u32(&machine, UINT32_C(0x0050b100) + UINT32_C(0x54)) ==
          UINT32_C(0x40c00000));
    CHECK(read_test_u32(&machine, UINT32_C(0x0050b100) + UINT32_C(0x58)) ==
          UINT32_C(0x40966666));
    CHECK(read_test_u32(&machine, UINT32_C(0x0050b100) + UINT32_C(0x5c)) ==
          UINT32_C(0x41940000));

    /* 0x1fedc calls the ROM's second luma-table copier using the live table
       pointers.  Keep this separate from the earlier 0x98b4 entry so the
       resumed call/return boundary is covered too. */
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x12800000);
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(0x00078d10);
    cpu.registers[VF2_I960_G0_REGISTER + 2u] = UINT32_C(66);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_TABLE);
    CHECK(report.entry_address == UINT32_C(0x0001fedc));
    CHECK(report.exit_address == UINT32_C(0x0001fee0));
    CHECK(report.recovered_instruction_count == UINT64_C(50891));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x12808400));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] == UINT32_C(0x0007ae10));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] == 0u);
    CHECK(read_test_u32(&machine, UINT32_C(0x12800000)) == UINT32_C(1));
    CHECK(read_test_u32(&machine, UINT32_C(0x128083fc)) ==
          UINT32_C(254));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_RESUMED_LUMA_RETURN);
    CHECK(report.entry_address == UINT32_C(0x0001fee0));
    CHECK(report.exit_address == UINT32_C(0x00009a00));
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.local_frame_depth == 0u);

    CHECK(vf2_model2a_write_u32(&machine, records[0], UINT32_C(0x12345678)) == VF2_OK);
    write_u32_bytes(main_data, 0x00302000u, UINT32_C(0xaaaaaaaa));
    enter_parent(&cpu, UINT32_C(0x0004b07c));
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(cpu.ip == UINT32_C(0x0004b07c));
    {
        uint32_t unchanged = 0u;
        CHECK(vf2_model2a_read_u32(&machine, records[0], &unchanged) == VF2_OK);
        CHECK(unchanged == UINT32_C(0x12345678));
    }

    vf2_model2a_shutdown(&machine);
    free(main_data);
    free(rom);
}

static void run_video_ramp_fixture(const uint8_t controls[9],
                                   uint64_t expected_instructions) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint16_t last_value = 0u;
    uint8_t encoded[2] = {0u, 0u};

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500234), controls, 6u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x005000e0), controls + 6u, 3u) ==
          VF2_OK);
    enter_parent(&cpu, UINT32_C(0x000005e8));
    cpu.arithmetic_control = UINT32_C(0x3f001000);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_VIDEO_RAMP);
    CHECK(report.entry_address == UINT32_C(0x000005e8));
    CHECK(report.exit_address == UINT32_C(0x00001004));
    CHECK(report.recovered_instruction_count == expected_instructions);
    CHECK(report.recovered_procedure_calls == UINT64_C(3));
    CHECK(report.recovered_procedure_returns == UINT64_C(4));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x005445fe), encoded, 2u) == VF2_OK);
    last_value = (uint16_t)((uint16_t)encoded[0] | ((uint16_t)encoded[1] << 8u));
    CHECK(last_value != UINT16_C(0));
    vf2_model2a_shutdown(&machine);
}

static void test_post_boot_video_ramp_dynamic_counts(void) {
    static const uint8_t first_controls[9] = {
        UINT8_C(0x75), UINT8_C(0x22), UINT8_C(0x75), UINT8_C(0x22), UINT8_C(0x75),
        UINT8_C(0x22), UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80)};
    static const uint8_t restored_controls[9] = {
        UINT8_C(0x40), UINT8_C(0x25), UINT8_C(0x40), UINT8_C(0x25), UINT8_C(0x40),
        UINT8_C(0x25), UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80)};

    run_video_ramp_fixture(first_controls, UINT64_C(11563));
    run_video_ramp_fixture(restored_controls, UINT64_C(11245));
}

static void test_post_boot_init_prefix(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint32_t value = 0u;
    uint8_t bytes[3] = {0u, 0u, 0u};

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0000052c));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu.arithmetic_control = UINT32_C(0x3f001000);
    cpu.compare_result = VF2_I960_COMPARE_EQUAL;
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_INIT_PREFIX);
    CHECK(report.entry_address == UINT32_C(0x0000052c));
    CHECK(report.exit_address == UINT32_C(0x0006dd4c));
    CHECK(report.recovered_instruction_count == UINT64_C(60078));
    CHECK(report.recovered_procedure_calls == UINT64_C(10));
    CHECK(report.recovered_procedure_returns == UINT64_C(9));
    CHECK(cpu.ip == UINT32_C(0x0006dd4c));
    CHECK(cpu.local_frame_depth == 1u);
    CHECK(cpu.registers[1] == VF2_WORK_RAM_BASE + UINT32_C(0x3080));
    CHECK(cpu.registers[VF2_I960_FP_REGISTER] == VF2_WORK_RAM_BASE + UINT32_C(0x3040));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x00ae101f));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 10u] == UINT32_C(0x00800000));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 11u] == UINT32_C(0x00880000));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 12u] == UINT32_C(0x00004000));
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(2));
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK(state.blocks_executed == 1u);

    CHECK(vf2_model2a_read(&machine, UINT32_C(0x005000e0), bytes, 3u) == VF2_OK);
    CHECK(bytes[0] == UINT8_C(0x80));
    CHECK(bytes[1] == UINT8_C(0x80));
    CHECK(bytes[2] == UINT8_C(0x80));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x00500082), bytes, 2u) == VF2_OK);
    CHECK(bytes[0] == UINT8_C(0x00));
    CHECK(bytes[1] == UINT8_C(0x80));
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x01c80002), bytes, 2u) == VF2_OK);
    CHECK(bytes[0] == UINT8_C(55));
    CHECK(bytes[1] == UINT8_C(0));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00504020), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x00ae101f));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00e80004), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x421));

    vf2_model2a_shutdown(&machine);
}

static void test_zero_length_run(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x12345678));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0xff, sizeof(report));
    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state, UINT32_C(0x12345678), 0u,
                                       &report) == VF2_OK);
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_executed == 0u);
    CHECK(report.start_address == UINT32_C(0x12345678));
    CHECK(report.final_address == UINT32_C(0x12345678));
    CHECK(state.blocks_executed == 0u);
    vf2_model2a_shutdown(&machine);
}

static void test_single_bridge_run(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    uint8_t enabled = 1u;
    uint8_t mode = 0u;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500171), &enabled,
                            sizeof(enabled)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002b), &mode, sizeof(mode)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c318), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0059c31c), 0u) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0006dcb8));
    cpu.arithmetic_control = UINT32_C(0x3f001004);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state, UINT32_C(0x00001004), 1u,
                                       &report) == VF2_OK);
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_executed == 1u);
    CHECK(report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_BRIDGE);
    CHECK(report.last_bridge_kind == VF2_HYBRID_BRIDGE_SYSTEM_MEMORY_DIAGNOSTIC);
    CHECK(report.recovered_instruction_count == UINT64_C(75));
    CHECK(report.recovered_procedure_calls == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(state.blocks_executed == 1u);
    CHECK(state.recovered_instruction_count == UINT64_C(75));
    CHECK(cpu.ip == UINT32_C(0x00001004));

    vf2_model2a_shutdown(&machine);
}

static void test_second_game_info_task_run(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    const uint32_t registry = UINT32_C(0x00515200);
    const uint32_t fighter0 = UINT32_C(0x00502000);
    const uint32_t fighter1 = UINT32_C(0x00503000);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(1) << 5u) ==
          VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00010d54));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu.registers[29] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0001645c),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));

    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state, UINT32_C(0x00010dcc), 1u,
                                       &report) == VF2_OK);
    CHECK(report.reached_stop == 1);
    CHECK(report.blocks_executed == 1u);
    CHECK(report.task_bodies_executed == 1u);
    CHECK(report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.last_task_kind == VF2_HYBRID_TASK_GAME_INFO);
    CHECK(report.recovered_instruction_count == UINT64_C(19));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(state.blocks_executed == 1u);
    CHECK(state.task_bodies_executed == 1u);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.registers[23] == fighter1);
    CHECK(cpu.registers[24] == fighter0);

    vf2_model2a_shutdown(&machine);
}

static void test_game_info_bit31_native_dispatch(void) {
    uint8_t *rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    const uint32_t registry = UINT32_C(0x00515200);
    const uint32_t fighter0 = UINT32_C(0x00502000);
    const uint32_t fighter1 = UINT32_C(0x00503000);
    const uint32_t fighter0_table = UINT32_C(0x00504000);
    uint32_t index = 0u;

    CHECK(rom != NULL);
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    /* Keep the observed dispatcher, recovered fighter corridors and
     * return-only ROM continuations. The fixture isolates the bounded native
     * paths without embedding the large unobserved procedures. */
    write_u32_bytes(rom, UINT32_C(0x0001645c), UINT32_C(0x90b83000));
    write_u32_bytes(rom, UINT32_C(0x00016464), UINT32_C(0x90c03000));
    write_u32_bytes(rom, UINT32_C(0x0001646c), UINT32_C(0x903de000));
    write_u32_bytes(rom, UINT32_C(0x00016470), UINT32_C(0x90462000));
    write_u32_bytes(rom, UINT32_C(0x00016474), UINT32_C(0x30f9e008));
    write_u32_bytes(rom, UINT32_C(0x00016478), UINT32_C(0x09001ccc));
    write_u32_bytes(rom, UINT32_C(0x0001647c), UINT32_C(0x90b83000));
    write_u32_bytes(rom, UINT32_C(0x00016484), UINT32_C(0x90c03000));
    write_u32_bytes(rom, UINT32_C(0x0001648c), UINT32_C(0x30fa2008));
    write_u32_bytes(rom, UINT32_C(0x00016490), UINT32_C(0x09001cb4));
    write_u32_bytes(rom, UINT32_C(0x00016494), UINT32_C(0x581a0087));
    write_u32_bytes(rom, UINT32_C(0x00016498), UINT32_C(0x30f8e02c));
    write_u32_bytes(rom, UINT32_C(0x0001649c), UINT32_C(0x90b83000));
    write_u32_bytes(rom, UINT32_C(0x000164a4), UINT32_C(0x90c03000));
    write_u32_bytes(rom, UINT32_C(0x000164ac), UINT32_C(0x09002198));
    write_u32_bytes(rom, UINT32_C(0x000164b0), UINT32_C(0x90b83000));
    write_u32_bytes(rom, UINT32_C(0x000164b8), UINT32_C(0x90c03000));
    write_u32_bytes(rom, UINT32_C(0x000164c0), UINT32_C(0x09002184));
    write_u32_bytes(rom, UINT32_C(0x000164c4), UINT32_C(0x90783000));
    write_u32_bytes(rom, UINT32_C(0x000164cc), UINT32_C(0x372be034));
    write_u32_bytes(rom, UINT32_C(0x00016500), UINT32_C(0x0a000000));
    write_u32_bytes(rom, UINT32_C(0x00018144), UINT32_C(0x0a000000));
    write_u32_bytes(rom, UINT32_C(0x00018644), UINT32_C(0x0a000000));
    write_u32_bytes(rom, UINT32_C(0x00017b68), UINT32_C(0x907de000));
    write_u32_bytes(rom, UINT32_C(0x00017b6c), UINT32_C(0x303be54c));
    write_u32_bytes(rom, UINT32_C(0x000180b8), UINT32_C(0x0a000000));
    write_u32_bytes(rom, UINT32_C(0x0001853c), UINT32_C(0xc885e5b4));
    write_u32_bytes(rom, UINT32_C(0x00018544), UINT32_C(0x928de5f8));
    write_u32_bytes(rom, UINT32_C(0x00018548), UINT32_C(0x5884080f));
    write_u32_bytes(rom, UINT32_C(0x00018550), UINT32_C(0x0a000000));
    write_u32_bytes(rom, UINT32_C(0x00018554), UINT32_C(0x0a000000));
    write_u32_bytes(rom, UINT32_C(0x0001b7ec), UINT32_C(0x40400000));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0, UINT32_C(0x80000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, UINT32_C(0x80000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        &machine, fighter0 + UINT32_C(0x000001f8), fighter0_table
    ) == VF2_OK);
    for (index = 0u; index < 16u; ++index) {
        CHECK(vf2_model2a_write_u32(
            &machine, fighter0_table + index * UINT32_C(12), 0u
        ) == VF2_OK);
    }

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00010d54));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[29] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0001645c),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state,
                                       UINT32_C(0x00010dcc), 1u,
                                       &report) == VF2_OK);
    CHECK(report.reached_stop == 1);
    CHECK(report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.last_task_kind == VF2_HYBRID_TASK_GAME_INFO);
    CHECK(report.recovered_instruction_count == UINT64_C(678));
    CHECK(report.recovered_procedure_calls == UINT64_C(12));
    CHECK(report.recovered_procedure_returns == UINT64_C(13));
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(state.task_bodies_executed == 1u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_player_task_interpreter_bridge(void) {
    uint8_t *rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    const uint32_t registry = UINT32_C(0x00510980);

    CHECK(rom != NULL);
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    write_u32_bytes(rom, UINT32_C(0x00013f08), UINT32_C(0x0a000000));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00010d54));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu.registers[29] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00013f08),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state,
                                       UINT32_C(0x00010dcc), 1u,
                                       &report) == VF2_OK);
    CHECK(report.reached_stop == 1);
    CHECK(report.last_step_kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.last_task_kind == VF2_HYBRID_TASK_PLAYER);
    CHECK(report.recovered_instruction_count == UINT64_C(1));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(state.task_bodies_executed == 1u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_budget_and_unsupported_are_explicit(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_run_report report;
    vf2_native_runtime_step_report step_report;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0xdeadbeef));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&step_report, 0xff, sizeof(step_report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &step_report) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(state.blocks_executed == 0u);
    CHECK(step_report.entry_address == UINT32_C(0xdeadbeef));
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_NONE);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state, UINT32_C(0xcafebabe), 0u,
                                       &report) == VF2_ERROR_UNSUPPORTED);
    CHECK(report.reached_stop == 0);
    CHECK(report.blocks_executed == 0u);
    CHECK(report.final_address == UINT32_C(0xdeadbeef));
    CHECK(state.blocks_executed == 0u);

    vf2_model2a_shutdown(&machine);
}

static void test_multi_frame_run(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report step_report;
    uint8_t flag = 0u;

    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    CHECK(rom != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (machine.work_ram == NULL) {
        free(rom);
        return;
    }

    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);

    /* Point Processor Control Block and Interrupt Table and Stack Pointer */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff410) + 20u,
                                UINT32_C(0x005ff000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff410) + 24u,
                                UINT32_C(0x005ff500)) == VF2_OK);
    /* Point vector 12 to VF2_NATIVE_INTERRUPT_RETURN_ENTRY = 0x00000d20 directly */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005ff000) + 36u + 16u,
                                UINT32_C(0x00000d20)) == VF2_OK);

    /* Initialize CPU at frame wait entry */
    vf2_i960_cpu_reset(&cpu, 0u, UINT32_C(0x005ff410), UINT32_C(0x00010f90));
    cpu.registers[1] = UINT32_C(0x00501000);
    cpu.registers[31] = UINT32_C(0x00500000);

    /* Write 0 to frame counter */
    flag = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &flag, sizeof(flag)) ==
          VF2_OK);

    /* Initialize native runtime with 2 visits before interrupt */
    CHECK(vf2_native_runtime_initialize(&state, 2u) == VF2_OK);

    /* Step 1: Execute frame wait poll, visits = 1, continues */
    memset(&step_report, 0, sizeof(step_report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &step_report) == VF2_OK);
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(state.frame_wait_phases == 1u);
    CHECK(state.frame_wait.visits ==
          0u); // reset back to 0 because 2 >= 2 and it raised interrupt!
    CHECK(state.frame_wait.interrupts_injected == 1u);
    CHECK(cpu.ip ==
          UINT32_C(0x00000d20)); // Interrupted and jumped to the vector 12 handler
    CHECK(cpu.local_frame_depth == 1u);

    /* Change frame byte value at 0x500000 to exit the wait on return */
    flag = 1u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x00500000), &flag, sizeof(flag)) ==
          VF2_OK);

    /* Step 2: Execute vector 12 interrupt handler and return from interrupt */
    memset(&step_report, 0, sizeof(step_report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &step_report) == VF2_OK);
    CHECK(step_report.kind == VF2_NATIVE_RUNTIME_STEP_FRAME_WAIT);
    CHECK(state.frame_wait_phases == 2u);
    CHECK(state.frame_wait.visits == 1u);
    CHECK(cpu.ip == UINT32_C(0x00010fa4)); // Succeeded interrupt return and frame exit!
    CHECK(cpu.local_frame_depth == 0u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_repeated_scheduler_entry_dispatches_recovery(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report step_report;
    vf2_native_runtime_run_report run_report;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (machine.work_ram == NULL) {
        free(rom);
        return;
    }
    /* The recovered scheduler scan reads task_count from a low address inside
     * the main ROM window; attach a blank ROM so the read returns 0 instead of
     * VF2_ERROR_OUT_OF_BOUNDS. */
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);

    /* Stand the CPU exactly at the recovered main-loop scheduler call site
     * (0x0000a010), matching the architectural preconditions required by
     * vf2_hybrid_second_scheduler_enter for the second sweep. The state
     * already records one accepted second-sweep entry, simulating the
     * end-of-frame re-hit of the same call site that should launch a third
     * sweep. The recovered scheduler entry is now generic across sweeps --
     * reference evidence (observe-third-sweep) confirms the architectural
     * preconditions are met on every sweep -- so the runtime must dispatch
     * the actual recovery rather than short-circuiting. With task_count == 0
     * the inner enter rejects via its own preconditions. */
    vf2_i960_cpu_reset(&cpu, 0u, UINT32_C(0x005ff410), UINT32_C(0x0000a010));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    state.scheduler_entries = 1u;

    memset(&step_report, 0xff, sizeof(step_report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &step_report) ==
          VF2_ERROR_UNSUPPORTED);
    /* A cold scheduler entry is now recovered only for the measured natural
     * 29-descriptor registry. This synthetic blank-ROM fixture has no valid
     * registry, so it must fail closed instead of taking the retired 4-step
     * shortcut back to the main-loop head. */
    CHECK(state.blocks_executed == 0u);
    CHECK(state.recovered_instruction_count == 0u);
    CHECK(state.scheduler_entries == 1u);
    CHECK(cpu.ip == UINT32_C(0x0000a010));

    /* run_until must preserve the same fail-closed boundary. */
    memset(&run_report, 0xff, sizeof(run_report));
    CHECK(vf2_native_runtime_run_until(&machine, &cpu, &state, UINT32_C(0x00000000), 4u,
                                       &run_report) == VF2_ERROR_UNSUPPORTED);
    CHECK(run_report.reached_stop == 0);
    CHECK(run_report.blocks_executed == 0u);
    CHECK(run_report.final_address == UINT32_C(0x0000a010));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_scheduler_selects_later_player_entry(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    const uint32_t registry_base = UINT32_C(0x00510000);
    const uint32_t player_registry = registry_base + UINT32_C(8 * 0x80);
    uint32_t index = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    write_u32_bytes(rom, UINT32_C(0x00011d94), 29u);
    write_u32_bytes(rom, UINT32_C(0x00013f08), UINT32_C(0x0a000000));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(1) << 9u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00004), UINT32_C(0x000fffff)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), UINT32_C(0x000fffff)) ==
          VF2_OK);
    for (index = 0u; index < 8u; ++index) {
        const uint32_t registry = registry_base + index * UINT32_C(0x80);
        CHECK(vf2_model2a_write_u32(&machine, registry, 0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(8),
                                    UINT32_C(0x80)) == VF2_OK);
    }
    CHECK(vf2_model2a_write_u32(&machine, player_registry,
                                UINT32_C(0x80000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player_registry + UINT32_C(8),
                                UINT32_C(0x80)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player_registry + UINT32_C(0x0c),
                                UINT32_C(0x00013f08)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0000a010));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER);
    CHECK(report.next_task_index == 8u);
    CHECK(report.next_registry_address == player_registry);
    CHECK(report.task_kind == VF2_HYBRID_TASK_NONE);
    CHECK(cpu.ip == UINT32_C(0x00013f08));

    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.task_kind == VF2_HYBRID_TASK_PLAYER);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void seed_kill_osage_task(vf2_model2a *machine, vf2_i960_cpu *cpu,
                                 uint32_t order_flags) {
    const uint32_t osage0 = UINT32_C(0x00515f00);
    const uint32_t osage1 = UINT32_C(0x00516180);

    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500868), osage0) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x0050086c), osage1) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, UINT32_C(0x00500020), order_flags) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, VF2_TIMER_BASE + UINT32_C(0x0c),
                                UINT32_C(0x0007a120)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, osage0, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, osage0 + UINT32_C(0x0c),
                                UINT32_C(0x000640f4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, osage1, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(machine, osage1 + UINT32_C(0x0c),
                                UINT32_C(0x000640f4)) == VF2_OK);

    vf2_i960_cpu_reset(cpu, 0u, 0u, UINT32_C(0x00010d54));
    cpu->registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu->registers[29] = UINT32_C(0x00515e80);
    CHECK(vf2_i960_cpu_enter_procedure(cpu, UINT32_C(0x000657dc),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
}

static void test_scheduler_selects_coli_entry_at_index10(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    const uint32_t registry_base = UINT32_C(0x00510000);
    const uint32_t coli_registry = registry_base + UINT32_C(10 * 0x80);
    uint32_t index = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    write_u32_bytes(rom, UINT32_C(0x00011d94), 29u);
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(1) << 9u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00004), UINT32_C(0x000fffff)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), UINT32_C(0x000fffff)) ==
          VF2_OK);
    for (index = 0u; index < 10u; ++index) {
        const uint32_t registry = registry_base + index * UINT32_C(0x80);
        CHECK(vf2_model2a_write_u32(&machine, registry, 0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(8),
                                    UINT32_C(0x80)) == VF2_OK);
    }
    CHECK(vf2_model2a_write_u32(&machine, coli_registry,
                                UINT32_C(0x80000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, coli_registry + UINT32_C(8),
                                UINT32_C(0x80)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, coli_registry + UINT32_C(0x0c),
                                UINT32_C(0x000221e8)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0000a010));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_SECOND_SCHEDULER);
    CHECK(report.next_task_index == 10u);
    CHECK(report.next_registry_address == coli_registry);
    CHECK(report.recovered_instruction_count == UINT64_C(187));
    CHECK(report.recovered_procedure_calls == UINT64_C(4));
    CHECK(report.recovered_procedure_returns == UINT64_C(2));
    CHECK(cpu.ip == UINT32_C(0x000221e8));

    /* The fa_coli body is admitted only for the measured PUNCH-driven
     * warm shape (bit 5 clear, live fighters, 9214/18/19). The synthetic
     * zeroed registry has no fighter objects, so the next step must still
     * fail closed instead of entering an unmeasured body. */
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_recurring_kill_osage_order_accounting(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }

    seed_kill_osage_task(&machine, &cpu, UINT32_C(1));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.task_kind == VF2_HYBRID_TASK_KILL_OSAGE);
    CHECK(report.recovered_instruction_count == UINT64_C(33));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00010dcc));

    seed_kill_osage_task(&machine, &cpu, UINT32_C(2));
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_TASK);
    CHECK(report.task_kind == VF2_HYBRID_TASK_KILL_OSAGE);
    CHECK(report.recovered_instruction_count == UINT64_C(36));
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(cpu.ip == UINT32_C(0x00010dcc));

    vf2_model2a_shutdown(&machine);
}

static void write_le32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8u);
    bytes[offset + 2u] = (uint8_t)(value >> 16u);
    bytes[offset + 3u] = (uint8_t)(value >> 24u);
}

static void test_scheduler_finishes_after_early_last_active_task(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint32_t value = 0u;
    const uint32_t registry25 = UINT32_C(0x00515e80);
    const uint32_t registry26 = UINT32_C(0x00515f00);
    const uint32_t registry27 = UINT32_C(0x00516180);
    const uint32_t registry28 = UINT32_C(0x00516400);
    const uint32_t end_registry = UINT32_C(0x00516480);
    const uint32_t scratch25 = UINT32_C(0x0050c320);

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    write_le32(rom, UINT32_C(0x00011d94), UINT32_C(29));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);

    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), UINT32_C(1) << 9u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00004), UINT32_C(0x000fffff)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00f00008), UINT32_C(0x000fffff)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry25 + UINT32_C(0x38), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry25 + UINT32_C(8), UINT32_C(0x80)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry26, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry26 + UINT32_C(8), UINT32_C(0x280)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry27, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry27 + UINT32_C(8), UINT32_C(0x280)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry28, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry28 + UINT32_C(8), UINT32_C(0x80)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, scratch25 + UINT32_C(8), UINT32_C(2)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, scratch25 + UINT32_C(0x10), UINT32_C(9)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, scratch25 + UINT32_C(0x20) + UINT32_C(0x10),
                                UINT32_C(9)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, scratch25 + UINT32_C(0x40) + UINT32_C(0x10),
                                UINT32_C(9)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, scratch25 + UINT32_C(0x60) + UINT32_C(0x10),
                                UINT32_C(9)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0000a010));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00010dcc),
                                       UINT32_C(0x0000a014)) == VF2_OK);
    cpu.registers[10] = scratch25;
    cpu.registers[11] = UINT32_C(25);
    cpu.registers[29] = registry25;

    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_SCHEDULER_FINISH);
    CHECK(report.current_task_index == 25u);
    CHECK(report.next_task_index == 29u);
    CHECK(report.descriptors_scanned == 4u);
    CHECK(report.current_registry_address == registry25);
    CHECK(report.next_registry_address == end_registry);
    CHECK(report.recovered_instruction_count == UINT64_C(72));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x0000a014));
    CHECK(cpu.registers[29] == end_registry);
    CHECK(state.scheduler_finishes == 1u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500038), &value) == VF2_OK);
    CHECK(value == UINT32_C(28));
    CHECK(vf2_model2a_read_u32(&machine, scratch25 + UINT32_C(8), &value) == VF2_OK);
    CHECK(value == UINT32_C(3));
    CHECK(vf2_model2a_read_u32(&machine, scratch25 + UINT32_C(0x10), &value) == VF2_OK);
    CHECK(value == 0u);
    CHECK(vf2_model2a_read_u32(&machine, scratch25 + UINT32_C(0x20) + UINT32_C(0x10),
                               &value) == VF2_OK);
    CHECK(value == 0u);
    CHECK(vf2_model2a_read_u32(&machine, scratch25 + UINT32_C(0x40) + UINT32_C(0x10),
                               &value) == VF2_OK);
    CHECK(value == 0u);
    CHECK(vf2_model2a_read_u32(&machine, scratch25 + UINT32_C(0x60) + UINT32_C(0x10),
                               &value) == VF2_OK);
    CHECK(value == 0u);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

int main(void) {
    test_initialize_and_names();
    test_post_boot_delay();
    test_post_boot_texture_init_prefix();
    test_post_boot_texture_wait_poll();
    test_post_boot_graphics_verify();
    test_post_boot_video_ramp_dynamic_counts();
    test_post_boot_init_prefix();
    test_zero_length_run();
    test_single_bridge_run();
    test_second_game_info_task_run();
    test_game_info_bit31_native_dispatch();
    test_player_task_interpreter_bridge();
    test_budget_and_unsupported_are_explicit();
    test_multi_frame_run();
    test_repeated_scheduler_entry_dispatches_recovery();
    test_scheduler_selects_later_player_entry();
    test_scheduler_selects_coli_entry_at_index10();
    test_recurring_kill_osage_order_accounting();
    test_scheduler_finishes_after_early_last_active_task();

    if (failures != 0) {
        fprintf(stderr, "%d native-runtime test(s) failed\n", failures);
        return 1;
    }
    printf("native-runtime tests passed\n");
    return 0;
}
