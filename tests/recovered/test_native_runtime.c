#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/fighter_candidate.h"
#include "vf2/i960/executor.h"
#include "vf2/native_runtime.h"
#include "vf2/rom.h"

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

static uint8_t read_test_u8(const vf2_model2a *machine, uint32_t address) {
    uint8_t value = 0u;
    CHECK(vf2_model2a_read(machine, address, &value, sizeof(value)) == VF2_OK);
    return value;
}

static uint32_t read_test_u32(const vf2_model2a *machine, uint32_t address) {
    uint8_t bytes[4] = {0u, 0u, 0u, 0u};
    CHECK(vf2_model2a_read(machine, address, bytes, sizeof(bytes)) == VF2_OK);
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

/* v0389: fail-closed gates for the ghost selector 0x4505.  The forced-g0
 * 0x4505 shape of earlier notes never occurs live (the ROM loads g0 with
 * 0x505 via `ldos (g6), g0` at 0x14280) and the reference faults at the
 * 0x27048 cvtri on every forced drive, so the corridor must refuse it.
 * The live 0x505/1622 corridor is pinned ROM-backed by
 * vf2_player_4505_live; here C admission stays gated on the live shape
 * only. */
static void test_player_19ef8_selector_4505(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_task_report report;
    const uint32_t player = UINT32_C(0x00510800);
    const uint32_t registry = UINT32_C(0x00514980);

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);

    /* F0 bit 31 set matches the player-14288-* reference fault shape. */
    CHECK(vf2_model2a_write_u32(&machine, player,
                                UINT32_C(0x80000002)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0x1a4), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c),
                                UINT32_C(0x02000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) ==
          VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00014288));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00014288),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    cpu.registers[29] = registry;
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00004505);
    memset(&report, 0, sizeof(report));
    {
        vf2_status st = vf2_hybrid_first_dispatch_task_execute(
            &machine, &cpu, registry, &report);
        /* The ghost selector must never produce the live corridor. */
        CHECK(!(st == VF2_OK &&
                report.recovered_instruction_count == UINT64_C(1622) &&
                cpu.ip == UINT32_C(0x0001428c)));
        CHECK(!(st == VF2_OK &&
                report.recovered_instruction_count == UINT64_C(1622)));
        CHECK(st != VF2_OK || report.kind == VF2_HYBRID_TASK_PLAYER);
    }

    /* F0 bit 5 set is also forbidden on the measured 0x4505 shape. */
    CHECK(vf2_model2a_write_u32(&machine, player,
                                UINT32_C(0x00000020)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00014288));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00014288),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    cpu.registers[29] = registry;
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00004505);
    memset(&report, 0, sizeof(report));
    {
        vf2_status st = vf2_hybrid_first_dispatch_task_execute(
            &machine, &cpu, registry, &report);
        CHECK(!(st == VF2_OK &&
                report.recovered_instruction_count == UINT64_C(1622) &&
                cpu.ip == UINT32_C(0x0001428c)));
    }

    /* Unadmitted selector still fails closed. */
    CHECK(vf2_model2a_write_u32(&machine, player,
                                UINT32_C(0x04000000)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00014288));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00014288),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    cpu.registers[29] = registry;
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00001234);
    memset(&report, 0, sizeof(report));
    {
        vf2_status st = vf2_hybrid_first_dispatch_task_execute(
            &machine, &cpu, registry, &report);
        CHECK(!(st == VF2_OK &&
                report.recovered_instruction_count == UINT64_C(1622)));
    }

    vf2_model2a_shutdown(&machine);
    free(rom);
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

static void test_coli_bit5_set_early_ret(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_task_report report;
    const uint32_t registry = UINT32_C(0x00514980);
    uint32_t flags = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    /* Measured warm flags with bit 5 forced set (0x8a00 | 0x20). */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008a20)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000221e8));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[29] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000221e8),
                                       UINT32_C(0x00010dcc)) == VF2_OK);

    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_first_dispatch_task_execute(&machine, &cpu, registry,
                                                 &report) == VF2_OK);
    CHECK(report.kind == VF2_HYBRID_TASK_COLI);
    CHECK(report.exit_address == UINT32_C(0x00010dcc));
    CHECK(report.recovered_instruction_count == UINT64_C(3));
    CHECK(report.recovered_procedure_calls == UINT64_C(0));
    CHECK(report.recovered_procedure_returns == UINT64_C(1));
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00508000), &flags) ==
          VF2_OK);
    CHECK(flags == UINT32_C(0x00008a20));

    /* Bit 5 clear remains the measured warm-body bridge; a synthetic
     * zeroed machine has no fighters and must still fail closed. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008a00)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000221e8));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[29] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000221e8),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_hybrid_first_dispatch_task_execute(&machine, &cpu, registry,
                                                 &report) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_coli_bitmask_22298_early_path(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    const uint32_t fighter1 = UINT32_C(0x00512800);
    const uint8_t poison[2] = {UINT8_C(0xef), UINT8_C(0xbe)};
    uint64_t start_instructions = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022298));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022298),
                                       UINT32_C(0x00022214)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_bitmask_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022214));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(7));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) == UINT16_C(0));

    /* v0351 live sibling: bit 8 set, bit 1 clear, g7 bit14 clear,
     * g7+0x61c==0, g8+0x821 not in {2,5,6} → body 13 + ret = 14. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x6dc), poison,
                            sizeof(poison)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x61c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022298));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022298),
                                       UINT32_C(0x00022214)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_bitmask_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022214));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(14));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) == UINT16_C(0));

    /* Fail-closed: g7 bit14 set (unmeasured 0x222b4 float loop). */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x6dc), poison,
                            sizeof(poison)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 14u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022298));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022298),
                                       UINT32_C(0x00022214)) == VF2_OK);
    CHECK(vf2_hybrid_coli_bitmask_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) ==
          UINT16_C(0xbeef));

    /* Fail-closed: g7+0x61c != 0 (unmeasured shorter stos-0 shape). */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x61c),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x6dc), poison,
                            sizeof(poison)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022298));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022298),
                                       UINT32_C(0x00022214)) == VF2_OK);
    CHECK(vf2_hybrid_coli_bitmask_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) ==
          UINT16_C(0xbeef));

    /* Fail-closed: g8+0x821 in {2,5,6} (unmeasured 0x22338/0x2233c).
     * Scan byte is read from g8, not g7. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x61c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x821),
                            (const uint8_t *)"\x02", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x6dc), poison,
                            sizeof(poison)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022298));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022298),
                                       UINT32_C(0x00022214)) == VF2_OK);
    CHECK(vf2_hybrid_coli_bitmask_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) ==
          UINT16_C(0xbeef));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_coli_contact_query_22404_early_path(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    const uint32_t registry = UINT32_C(0x00514b80);
    const uint8_t poison[2] = {UINT8_C(0xef), UINT8_C(0xbe)};
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    /* 0x02007aca lives in main-data (base 0x02000000). Plant table[1]=8. */
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00008000))) !=
          NULL);
    if (main_data != NULL) {
        /* Model memory is little-endian; table[1] value 8. */
        write_u32_bytes(main_data, UINT32_C(0x7ace), UINT32_C(8));
    }
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00008000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, registry + UINT32_C(0x8c), poison,
                            sizeof(poison)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0x00000005)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x00001234)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x0002222c));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(14));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 14u] == UINT32_C(0x00022428));
    CHECK(read_test_u16(&machine, registry + UINT32_C(0x8c)) ==
          UINT16_C(0x1234));
    CHECK(read_test_u16(&machine, registry + UINT32_C(0x90)) ==
          UINT16_C(0x0004));

    /* Bit 8 set with unequal snapshots fails closed after the common
     * snapshot store (the original always stores before the bit-8 check). */
    CHECK(vf2_model2a_write(&machine, registry + UINT32_C(0x8c), poison,
                            sizeof(poison)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(read_test_u16(&machine, registry + UINT32_C(0x8c)) ==
          UINT16_C(0x1234));
    CHECK(read_test_u16(&machine, registry + UINT32_C(0x90)) ==
          UINT16_C(0x0004));

    /* Bit 8 set with equal snapshots, pending clear, helper r3 = 0,
     * empty scan mask is a measured sibling (v0290): 30 insns, g0 = 0. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808),
                                UINT32_C(0x00512800)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                            UINT32_C(0x6d4),
                                UINT32_C(0xffffffff)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512800);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x0002222c));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(30));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 14u] == UINT32_C(0x0002244c));
    CHECK(read_test_u16(&machine, UINT32_C(0x00512800) + UINT32_C(0x6d4)) ==
          UINT16_C(0));

    /* v0303: bit 8 set, equal snapshots, pending clear, helper 0,
     * index 1 (ROM mask 8), dest slot 0xffff -> non-empty after andnot,
     * g0 = 1, body 72, pending bit set, FIFO triple stored. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    /* g13+0x40[bit 3] holds the dest mask fragment. */
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x40) +
                                             UINT32_C(3) * UINT32_C(4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x26),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512800);
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x0002222c));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(73));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 1u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01000550));
    CHECK(read_test_u16(&machine, registry + UINT32_C(0x90)) ==
          UINT16_C(0x0001));
    CHECK(read_test_u16(&machine, UINT32_C(0x00512800) + UINT32_C(0x6d4)) ==
          UINT16_C(0xffff));

    /* v0359: live first-contact stale slot (coli-live-midbody-g01 first
     * 0x22404: old 0xffff vs snap 0, bit 8 set, pending clear, thr 0/0,
     * 5b8 bit0 clear, index 1, table bit3, exclude 0, delta 0).
     * Falls through to bal 0x225bc (+5), rejoins exactly: body 77,
     * g0 = 1, pending bit set, result 0x21e8. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x4),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x40) +
                                             UINT32_C(3) * UINT32_C(4),
                                UINT32_C(0x000221e8)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x6d4),
                                UINT32_C(0x0000beef)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x26),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x18),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x1c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x20),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512800);
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x0002222c));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(78));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 1u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 9u] == UINT32_C(0x01000550));
    CHECK(read_test_u16(&machine, registry + UINT32_C(0x90)) ==
          UINT16_C(0x0001));
    CHECK(read_test_u16(&machine, UINT32_C(0x00512800) + UINT32_C(0x6d4)) ==
          UINT16_C(0x21e8));

    /* v0359 negative: stale slot with an empty result (index 0, mask 0)
     * is unmeasured and stays fail-closed. */
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512800);
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    /* Restore the v0303 end state for the sibling tests below (index 1,
     * table bit3 fragment), which rely on those leftovers. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x40) +
                                             UINT32_C(3) * UINT32_C(4),
                                UINT32_C(0x0000ffff)) == VF2_OK);

    /* Slot 1 g0=1 sibling is native (v0306): 15-trip scan, body 133. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x4),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8e),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512800);
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x0002222c));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(134));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 1u);
    /* Pending bit 1 set for slot 1. */
    CHECK((read_test_u16(&machine, registry + UINT32_C(0x90)) &
           UINT16_C(0x0002)) != 0u);

    /* v0308: g8+0x26 FIFO cursor path (body 79 on the one-hit scan).
     * Contact g8 is 0x512800 on this fixture. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x4),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00512800) +
                                             UINT32_C(0x26),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x005001e4),
                                UINT32_C(0x10)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022404));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = UINT32_C(0x00512800);
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022404),
                                       UINT32_C(0x0002222c)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_contact_query_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x0002222c));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(80));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 1u);
    /* -2 stored at 0x90e010; cursor byte advanced to 0x14. */
    CHECK(read_test_u32(&machine, UINT32_C(0x0090e010)) ==
          UINT32_C(0xfffffffe));
    CHECK(read_test_u8(&machine, UINT32_C(0x005001e4)) == UINT8_C(0x14));

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

static void test_coli_238a4_early_path(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000238a4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(0xffffffff);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000238a4),
                                       UINT32_C(0x000235b8)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_238a4_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x000235b8));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(5));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == 0u);

    /* Bit 8 set is an unmeasured sibling: fail closed and leave g3. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000238a4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(0xa5a5a5a5);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000238a4),
                                       UINT32_C(0x000235b8)) == VF2_OK);
    CHECK(vf2_hybrid_coli_238a4_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(0xa5a5a5a5));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0310/v0313: 0x23238 early-out and float-threshold paths. */
static void test_coli_23238_early_out(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t g8 = UINT32_C(0x00512980);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023238));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x11);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023238),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_23238_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022e40));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(3));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x11));

    /* g0 == 0x2ce, g8+0x1f8 = 0 → g0 = 0x2cf, body 12. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023238));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x000002ce);
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = g8;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023238),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_23238_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(12));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x000002cf));

    /* g8+0x1f8 = 0.75f → g0 = 0xa7, body 11. */
    {
        const uint32_t bits_075 = UINT32_C(0x3f400000); /* 0.75f */
        CHECK(vf2_model2a_write_u32(&machine, g8 + UINT32_C(0x1f8),
                                    bits_075) == VF2_OK);
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023238));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x000002ce);
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = g8;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023238),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_23238_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(11));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x000000a7));

    /* g8+0x1f8 = 0.95f → g0 unchanged 0x2ce, body 7. */
    {
        const uint32_t bits_095 = UINT32_C(0x3f733333); /* ~0.95f */
        CHECK(vf2_model2a_write_u32(&machine, g8 + UINT32_C(0x1f8),
                                    bits_095) == VF2_OK);
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023238));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x000002ce);
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = g8;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023238),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_23238_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(7));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x000002ce));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0311: 0x230d4 bit-26 compact path (body 15 on the measured shape). */
static void test_coli_230d4_bit26(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00020000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x1cc54), UINT32_C(0x00004421));
    }
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00020000)) == VF2_OK);
    /* g8 bit 26 set; halfword inputs 0 so r4 = 0x4000 (bit 15 clear). */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(1) << 26u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000230d4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000230d4),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_230d4_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022e40));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(16));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x00004421));

    /* Bit 26 clear takes the v0314 long path. Unmeasured gates fail. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000230d4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(5);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000230d4),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    CHECK(vf2_hybrid_coli_230d4_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    /* v0314 long path: g0=4, type index 0, table hit, g0=0x4ac. */
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x1ccfc),
                        UINT32_C(0x0201cd74));
        write_u32_bytes(main_data, UINT32_C(0x1cdfc), UINT32_C(0x000004ac));
    }
    CHECK(vf2_model2a_write_u32(&machine, fighter0, UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c),
                                UINT32_C(0x00599000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000230d4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(4);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000230d4),
                                       UINT32_C(0x00022e40)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_230d4_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022e40));
    {
        const uint64_t got = cpu.executed_instructions - start_instructions;
        if (got != UINT64_C(44)) {
            fprintf(stderr, "230d4 long insns=%llu calls=%llu rets=%llu\n",
                    (unsigned long long)got,
                    (unsigned long long)(cpu.procedure_calls - start_calls),
                    (unsigned long long)(cpu.procedure_returns -
                                         start_returns));
        }
    }
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(44));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(1));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(2));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == UINT32_C(0x000004ac));

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0314: 0x225cc long body through 0x230d4 long, 0x23238 ×2, 0x1ab34
 * miss and the float tail. Drive shape from coli-225cc-entry. */
static void test_coli_225cc_long(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    uint64_t start_instructions = 0u;
    uint32_t stored = 0u;
    uint32_t cursor = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00020000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x1ccfc),
                        UINT32_C(0x0201cd74));
        write_u32_bytes(main_data, UINT32_C(0x1cdfc), UINT32_C(0x000004ac));
        /* 0x1ab34 walk uses g0=0x4ac as the table index. */
        write_u32_bytes(main_data, UINT32_C(0xd34c) + UINT32_C(0x4ac) * 4u,
                        UINT32_C(0x0201acb3));
        main_data[0x1acbbu] = 0x03u;
        main_data[0x1acc9u] = 0x08u;
    }
    rom[0x1b7f9u] = 0x0eu;
    write_u32_bytes(rom, UINT32_C(0x23270), UINT32_C(0x3b23d70a));
    /* 0x439ac g0 source — non-zero so the diagnostic store path runs.
     * g7+0x820=0 selects the 0x230bc table (probe-equivalent). */
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00001234));
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00020000)) == VF2_OK);

    CHECK(vf2_model2a_write_u32(&machine, fighter0, UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008a00)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c),
                                UINT32_C(0x00599000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a800),
                                UINT32_C(0x3f800000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a808),
                                UINT32_C(0x3f800000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a818),
                                UINT32_C(0x00000020)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1ac),
                                UINT32_C(0)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    /* 248 reference steps to 0x230b8 plus the completed ret. */
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(240));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x6d8), &cursor) == VF2_OK);
    CHECK(cursor == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == (UINT32_C(0x4ac) + (UINT32_C(3) << 26u)));

    /* v0318: g7+0x1a4 bits 4+12 set → ×0.5 scale of +0x2c/+0x34. */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00011010)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x2c),
                                UINT32_C(0x40000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x34),
                                UINT32_C(0x40800000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(254));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x2c), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x3f800000)); /* 2.0f * 0.5 */
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x34), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x40000000)); /* 4.0f * 0.5 */

    /* v0318: g8+0x1a4 bit 13 set + g8+0x5b8 bit 0 set → early join. */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00002000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(242));

    /* v0318b: bit 4 set, bit 12 clear → join cascade (+2). */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010010)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(242));

    /* v0318c: bits 4+12 with bbs 15 taken → skip scale (−7). */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00011010)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x5c2),
                            (const uint8_t *)"\x00\xc0", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(247));

    /* v0319/v0320/v0321: r11b=1 → packing + diagnostic cascade (warm).
     * g7+0x820 = 0 selects the 0x230bc table (probe-equivalent). */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    /* lim at 0x50a0b4 must be >= r11 (cmpible gate). */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x000230c8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(302));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050406a), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504078), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x1234));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00e80004), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x421));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504001), &stored) == VF2_OK);
    CHECK((stored & 0xffu) == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504020), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x1234));

    /* v0320: count>=4 → 0x439ac early-out. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(291));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050406a), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(4));

    /* v0320: count=0 match at table[1] → 0x439ac early match. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(295));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050406a), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0));

    /* v0320: count=1 multi-trip, no match → store at table[1]. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050407c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(306));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050406a), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(2));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050407c), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x1234));

    /* v0320: gate&12 != 0, branch-byte bit 0 set → 0x43888 early ret. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c),
                                UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0059c351),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(285));

    /* v0320: gate&12 != 0, branch-byte bit 0 clear → fall through.
     * Only mutate the gate pair; other diagnostic state comes from
     * the warm shape. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c),
                                UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0059c351),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(305));

    /* v0320: runtime bit 20 set + g0 high-byte match → subtract + shli.
     * ROM is write-ignored via the machine API; patch the buffers.
     * g7+0x820=0 selects the 0x230bc table. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00540080)) == VF2_OK);
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x009e1234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x009e1234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(309));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504020), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x009c1234));

    /* v0326: bit 20 set, g0 high-byte non-match → cmpobne taken. */
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00012345));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00012345));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00540080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(307));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504020), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00012345)); /* g0 unchanged */

    /* Restore ROM for subsequent shapes. */
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);

    /* v0327: r11=30 → cmpoble 30 taken → 0x22e24. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x1e", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(30)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(299));

    /* v0329: g7+0x828 bit 10 set → skip to +0x1ac. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x04", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(295));

    /* v0329: g7+0x828 bit 8 set → bbc 8 nt, bit 3 clear → 0x22e24. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x01", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(291));
    /* Reset +0x828 for subsequent shapes. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);

    /* v0330: g8+0x1a4 bit 26 set → post-diag join 0x22e24 +
     * 0x230d4 compact. Needs main_data 0x1cc54 for the compact table
     * and 0x1ab34 index 0x421 (from g0=0x4421). Separate record to
     * avoid any interaction with the 0x4ac plant. */
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x1cc54), UINT32_C(0x00004421));
        write_u32_bytes(main_data,
                        UINT32_C(0xd34c) + UINT32_C(0x421) * 4u,
                        UINT32_C(0x0201b000));
        main_data[0x1b008u] = 0x03u; /* type 3 */
        main_data[0x1b016u] = 0x08u; /* type 8 (step 0x0e from type 3) */
    }
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter1 + UINT32_C(0x1a4),
              UINT32_C(0x04000000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(263));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == (UINT32_C(0x4421) + (UINT32_C(3) << 26u)));
    /* Reset g8+0x1a4 bit 26 for subsequent shapes. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);

    /* v0332: g8+0x1a4 bit 4 set. Cascade r11*3>>1 + g0=0x23d6b,
     * post-diag join 0x22e24, 0x22e48 g0=0x2ce, miss-tail 0x1c/0x10.
     * Second 0x23238 takes the long path (g0=0x2ce → g0=0x2cf). */
    if (main_data != NULL) {
        write_u32_bytes(main_data,
                        UINT32_C(0xd34c) + UINT32_C(0x2cf) * 4u,
                        UINT32_C(0x0201b100));
        main_data[0x1b108u] = 0x03u;
        main_data[0x1b116u] = 0x08u;
    }
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter1 + UINT32_C(0x1a4),
              UINT32_C(0x00000010)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    /* Miss-tail bit-4 offsets: r7+0x1c and r7+0x10. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a810),
                                UINT32_C(0x3f800000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a81c),
                                UINT32_C(0x00000020)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(301));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == (UINT32_C(0x2cf) + (UINT32_C(3) << 26u)));
    /* Reset g8+0x1a4 bit 4 for subsequent shapes. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);

    /* v0321: g7+0x1a4 bit 18 set → skip entire diagnostic arm. */
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00040000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(247));

    /* v0321: r11=20 → r4=4 offset (still < 30 cascade gate). */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x14", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(20)) == VF2_OK);
    write_u32_bytes(rom, UINT32_C(0x230cc), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230c0), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(298));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504078), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x1234));

    /* v0331: r11=40 → r4=8 offset + cmpoble 30 taken → 0x22e24. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x28", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(40)) == VF2_OK);
    write_u32_bytes(rom, UINT32_C(0x230d0), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230c4), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(300));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504078), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x1234));
    /* Restore r11 for subsequent shapes. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);

    /* v0321: g8+0x1b1 == 9 → second diagnostic pair with constants. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1b1),
                            (const uint8_t *)"\x09", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050407c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(354));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050406a), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(2));

    /* v0321: g7+0x823 = 1 table walk. main_data[0x1e880] → inner
     * table; inner[0] = g0, inner[4] = 0 terminates. */
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1b1),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    write_u32_bytes(main_data, UINT32_C(0x1e880), UINT32_C(0x0201c000));
    write_u32_bytes(main_data, UINT32_C(0x1c000), UINT32_C(0x009e1234));
    write_u32_bytes(main_data, UINT32_C(0x1c004), UINT32_C(0x009e5678));
    write_u32_bytes(main_data, UINT32_C(0x1c008), UINT32_C(0));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(353));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050406a), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(2));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x00504078), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x009e1234));
    CHECK(vf2_model2a_read_u32(
              &machine, UINT32_C(0x0050407c), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x009e5678));

    /* v0323: g8+0x1a4 bit 16, g7+0x821=0 → early counter-- at 0x230a0. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1234),
                                UINT32_C(100)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(16));
    /* Prefix counter++ then early-exit counter-- nets zero. */
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x1234), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(100));

    /* v0324: g8+0x1a4 bit 14 → cascade skip + 0x22dd4 counter++. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00004000)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008a00)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x6d9),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    /* Float tail compares g8+0x6d9 against 0x50a16e. */
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a16e),
                            (const uint8_t *)"\x02", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(289));
    {
        uint8_t d6d9_chk = 0u;
        CHECK(vf2_model2a_read(&machine, fighter1 + UINT32_C(0x6d9),
                               &d6d9_chk, 1u) == VF2_OK);
        CHECK(d6d9_chk == UINT8_C(1));
    }

    /* v0325: g7+0x828 bit 14 set → join 0x22e24. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x40", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(286));

    /* v0333: g8+0x1a4 bit 13 set + g8+0x5b8 bit 0 clear → alt tail
     * at 0x22808 → float tail. Needs 0x230d4 table slot for r3=2. */
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x1cd7c), UINT32_C(0x000000ee));
    }
    write_u32_bytes(rom, UINT32_C(0x230c8), UINT32_C(0x00001234));
    write_u32_bytes(rom, UINT32_C(0x230bc), UINT32_C(0x00001234));
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter1 + UINT32_C(0x1a4),
              UINT32_C(0x00002000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a0b4),
                                UINT32_C(2)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(223));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == (UINT32_C(0xee) + (UINT32_C(7) << 25u)));

    /* v0339: g7+0x1a4 bit 22 + g8+0x1a4 bit 11 (bit 4 clear) →
     * bbs-11-taken to 0x22d8c: mov 5, 0x230d4 g0=5 fork (r3 = 42),
     * 0x23238 early-out, 0x22d9c tail, 0x23070 skip, ret.
     * Reference full path entry→0x22294 is 149 steps
     * (out/v0339: 113 to 0x230d4 + 36 tail). */
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x1ce1c), UINT32_C(0x000000eb));
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008a00)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a800),
                                UINT32_C(0x3f800000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a808),
                                UINT32_C(0x3f800000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a818),
                                UINT32_C(0x00000020)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00400100)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00000800)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x843),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1224),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1234),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x26),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x82a),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19f),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1ac),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(140));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x0c0100eb));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x1234), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x6d8), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(1));
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x5de)) ==
          UINT16_C(0xfff2));

    /* v0340: bit-13 + bit 3 + g7+0x844 bit 30 → 0x227dc miss path.
     * Reference entry→0x22804 is 86 steps (52 prefix + 34 block);
     * native adds the frame ret = 87. Chain replicates the live
     * 3-iter type-8 miss (table[1] → 02/04/08). */
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x0d350),
                        UINT32_C(0x02014d6d));
        main_data[0x14d75u] = 0x02u;
        main_data[0x14d7cu] = 0x04u;
        main_data[0x14d8du] = 0x08u;
    }
    rom[0x1b7f8u] = 0x07u;
    rom[0x1b7fau] = 0x11u;
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00002008)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x844),
                                UINT32_C(0x40000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x848),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x843),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1224),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1234),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x26),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x82a),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19f),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1ac),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(78));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x11000000));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x1234), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(1));

    /* v0343 L1: bit-13 + bit 3 + scan 1, bit-30 clear, +0x828 = 0 →
     * 0x22778 → 0x227ac scale (r11*3/4, r9*0.75) → cascade → warm
     * join. Reference entry→0x22294 is 275 steps. Live chain at the
     * 0x4ac table slot replicated (record 0x0200ee55, 03/08). */
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x0e5fc),
                        UINT32_C(0x0200ee55));
        main_data[0x0ee5du] = 0x03u;
        main_data[0x0ee6bu] = 0x08u;
        /* v0343: bit3-addo makes r9 = 6, so the 0x230d4-leg reads
         * slot 38 (0x201ce0c), not slot 34. */
        write_u32_bytes(main_data, UINT32_C(0x01ce0c),
                        UINT32_C(0x000004ac));
    }
    rom[0x1b7f8u] = 0x07u;
    rom[0x1b7f9u] = 0x0eu;
    rom[0x1b7fau] = 0x11u;
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00002008)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x844),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x848),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x843),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1224),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1234),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x26),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x82a),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1b0),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19f),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1ac),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008a00)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a0b4),
                            (const uint8_t *)"\x36\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a16e),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500028),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(266));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x0c0004ac));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x194), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x14000001));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x5e4), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00000000));
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x5de)) ==
          UINT16_C(0xfff2));

    /* v0343 L3: same but +0x828 bit 12 → 0x22794 scale (r11>>=1,
     * r9*=0.5) → cascade at 0x22918. Reference 271 steps. */
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x10", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(262));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x0c0004ac));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x194), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x14000001));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x5e4), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00000000));
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x5de)) ==
          UINT16_C(0xfff2));

    /* v0343 L2: +0x828 bit 13 → 0x227c4 diagnostic pair (g0=0x9e167f,
     * then 1) → alt tail at 0x22848. Reference 224 steps. */
    rom[0x23282u] = 0x06u;
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x01cdac),
                        UINT32_C(0x00000104));
    }
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x20", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050002c),
                                UINT32_C(0x100)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a0b7),
                            (const uint8_t *)"\x0a", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a14c),
                            (const uint8_t *)"\x16", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0xb2),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x85c),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050406a),
                                UINT32_C(0)) == VF2_OK);
    /* v0343: prior diag-pair shapes leave their g0 in slot[1] (work
     * RAM); L2's live slot is 0 (no early match → full iteration). */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504078),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504001),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00504003),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(215));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x0e000104));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x194), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x14000002));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x5e4), &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00000000));
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x5de)) ==
          UINT16_C(0x0006));

    /* v0345-B site A full leg: bit-13-clear + bit-3-clear + scan 1
     * + board-clear runs prefix → cascade → 0x502a4#siteA →
     * continuation (0x7fc0 ×3, 0x9444, join, site-B prefix,
     * 0x502a4#siteB, 0x7fc0#4) → existing 0x22e24 tail → OK.
     * 7 guest calls + 7 nested rets; this procedure-only unit enters
     * with stand-in return 0x22240 (single pop). The live-landing
     * unit below enters return 0x22294 with a parent frame and
     * double-pops to 0x10dcc.
     * Reference entry→0x2309c-b is 882 steps; +1 completes. */
    {
        static const uint8_t site_b_inline[] = {
            0x25u, 0x64u, 0x20u, 0x64u, 0x6fu, 0x77u, 0x6eu, 0x20u,
            0x68u, 0x69u, 0x74u, 0x20u, 0x63u, 0x6fu, 0x6du, 0x62u,
            0x6fu, 0x00u
        };
        static const uint8_t shape3_src[] = {
            0x68u, 0x69u, 0x74u, 0x20u, 0x20u, 0x20u, 0x20u, 0x20u,
            0x00u
        };
        size_t k = 0u;

        for (k = 0u; k < sizeof(site_b_inline); ++k) {
            rom[UINT32_C(0x00022e0c) + k] = site_b_inline[k];
        }
        for (k = 0u; k < 19u; ++k) {
            rom[UINT32_C(0x00022978) + k] = 0x20u;
        }
        rom[UINT32_C(0x0002298a)] = 0x00u;
        for (k = 0u; k < sizeof(shape3_src); ++k) {
            rom[UINT32_C(0x00023d50) + k] = shape3_src[k];
        }
    }
    {
        static const uint8_t site_a_inline[] = {
            0x25u, 0x64u, 0x20u, 0x68u, 0x69u, 0x74u, 0x20u, 0x63u,
            0x6fu, 0x6du, 0x62u, 0x6fu, 0x00u
        };
        size_t k = 0u;

        for (k = 0u; k < sizeof(site_a_inline); ++k) {
            rom[UINT32_C(0x00022950) + k] = site_a_inline[k];
        }
    }
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00004000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x844),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x848),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x843),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1224),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1234),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x26),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x82a),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x1b0),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19f),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x1ac),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068),
                                UINT32_C(0x00440080)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x00008800)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a0b4),
                            (const uint8_t *)"\x36\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050a16e),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500028),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00503200),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00503204),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x8),
                                UINT32_C(0x00002000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x804),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    {
        uint64_t start_calls = cpu.procedure_calls;
        uint64_t start_rets = cpu.procedure_returns;

        CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
        CHECK(cpu.ip == UINT32_C(0x00022240));
        CHECK(cpu.executed_instructions - start_instructions ==
              UINT64_C(874));
        CHECK(cpu.procedure_calls - start_calls == UINT64_C(7));
        CHECK(cpu.procedure_returns - start_rets == UINT64_C(8));
    }
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x6d8), &cursor) == VF2_OK);
    /* Live-correct composite: byte0 from the cascade counter++,
     * byte1 from the site-B prefix counter2 stob (both recorded). */
    CHECK(cursor == UINT32_C(0x00000101));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00503200),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x6d6f6320));
    CHECK(vf2_model2a_read_u32(&machine, fighter1 + UINT32_C(0x6d9),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(1));
    CHECK(vf2_model2a_read_u32(&machine, fighter1 + UINT32_C(0x198),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x0c0004ac));
    CHECK(vf2_model2a_read_u32(&machine, fighter0 + UINT32_C(0x194),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x14000001));
    CHECK(vf2_model2a_read_u32(&machine, fighter1 + UINT32_C(0x700),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(2));
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x5de)) ==
          UINT16_C(0xfff2));
    /* Observer hex is LE bytes: trace "0c74dac8" = 0xC8DA740C. */
    CHECK(vf2_model2a_read_u32(&machine, fighter1 + UINT32_C(0x5e4),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00000000));
    CHECK(vf2_model2a_read_u32(&machine, fighter1 + UINT32_C(0x5e0),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00000000));
    CHECK(vf2_model2a_read_u32(&machine, fighter1 + UINT32_C(0x5e8),
                               &stored) == VF2_OK);
    CHECK(stored == UINT32_C(0x00000000));

    /* v0346 live exit landing: re-enter site-A with the measured live
     * call frame. `call 0x225cc` at 0x22290 returns to the `ret` at
     * 0x22294; a parent frame returning to 0x10dcc makes the wrapper
     * double-pop through that ret to the scheduler. Reference full
     * path is 882 + 0x230b8 ret + 0x22294 ret = 884 steps. */
    CHECK(vf2_model2a_write_u32(
              &machine, fighter0 + UINT32_C(0x1a4),
              UINT32_C(0x00010000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0x00004000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x844),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x848),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x821),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x822),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x820),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x823),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x828),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x843),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1224),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1234),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x26),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x82a),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x19f),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1ac),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x700),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5d8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00503200),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00503204),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500028),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x804),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x804),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x194),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x198),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x5de),
                            (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022294)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    {
        uint64_t start_calls = cpu.procedure_calls;
        uint64_t start_rets = cpu.procedure_returns;

        CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
        CHECK(cpu.ip == UINT32_C(0x00010dcc));
        CHECK(cpu.executed_instructions - start_instructions ==
              UINT64_C(875));
        CHECK(cpu.procedure_calls - start_calls == UINT64_C(7));
        CHECK(cpu.procedure_returns - start_rets == UINT64_C(9));
        CHECK(cpu.local_frame_depth == 0u);
    }

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0344-B: 0x502a4 digit-parse helper (both balx sites, direct unit).
 * Site A: link 0x22950, inline "d hit combo", 9 loop iters, 2-byte
 * copy, bx-out 0x22960, 140 steps. Site B: link 0x22e0c, inline
 * "d down hit", 10 iters, 7-byte copy, bx-out 0x22e20, 170 steps. */
static void test_coli_502a4(void) {
    static const uint8_t site_a_inline[] = {
        0x25u, 0x64u, 0x20u, 0x68u, 0x69u, 0x74u, 0x20u, 0x63u, 0x6fu,
        0x6du, 0x62u, 0x6fu, 0x00u
    };
    static const uint8_t site_b_inline[] = {
        0x25u, 0x64u, 0x20u, 0x64u, 0x6fu, 0x77u, 0x6eu, 0x20u, 0x68u,
        0x69u, 0x74u, 0x20u, 0x63u, 0x6fu, 0x6du, 0x62u, 0x6fu, 0x00u
    };
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t out_buffer = UINT32_C(0x00503200);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    uint32_t word = 0u;
    size_t k = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00020000))) !=
          NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00020000)) == VF2_OK);
    for (k = 0u; k < sizeof(site_a_inline); ++k) {
        rom[UINT32_C(0x00022950) + k] = site_a_inline[k];
    }
    for (k = 0u; k < sizeof(site_b_inline); ++k) {
        rom[UINT32_C(0x00022e0c) + k] = site_b_inline[k];
    }

    /* Site A drive. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000502a4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000502a4),
                                       UINT32_C(0)) == VF2_OK);
    /* Locals are zeroed by frame entry: stage r14/r15/g1 after. */
    cpu.registers[14u] = UINT32_C(0x00022950);
    cpu.registers[15u] = UINT32_C(1);
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = out_buffer;
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_502a4_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022960));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(140));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(2));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == out_buffer);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] ==
          out_buffer + UINT32_C(2));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] ==
          UINT32_C(0x00022960));
    CHECK(vf2_model2a_read_u32(&machine, out_buffer, &word) == VF2_OK);
    CHECK(word == UINT32_C(0x0000006f));

    /* Site B drive (buffer cleared first; 7-byte overwrite). */
    CHECK(vf2_model2a_write_u32(&machine, out_buffer, UINT32_C(0)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, out_buffer + UINT32_C(4),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000502a4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000502a4),
                                       UINT32_C(0)) == VF2_OK);
    cpu.registers[14u] = UINT32_C(0x00022e0c);
    cpu.registers[15u] = UINT32_C(1);
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = out_buffer;
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_502a4_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022e20));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(170));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(2));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(2));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == out_buffer);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 1u] ==
          out_buffer + UINT32_C(7));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 2u] ==
          UINT32_C(0x00022e20));
    CHECK(vf2_model2a_read_u32(&machine, out_buffer, &word) == VF2_OK);
    CHECK(word == UINT32_C(0x6d6f6320));
    CHECK(vf2_model2a_read_u32(&machine, out_buffer + UINT32_C(4),
                               &word) == VF2_OK);
    CHECK(word == UINT32_C(0x00006f62));

    /* Unmeasured sibling: classification byte 0x64 ('d') takes the
     * 0x503b4 call edge and must fail closed. */
    rom[UINT32_C(0x0002295a)] = 0x64u;
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000502a4));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000502a4),
                                       UINT32_C(0)) == VF2_OK);
    /* Locals are zeroed by frame entry: stage r14/r15/g1 after. */
    cpu.registers[14u] = UINT32_C(0x00022950);
    cpu.registers[15u] = UINT32_C(1);
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = out_buffer;
    CHECK(vf2_hybrid_coli_502a4_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0345-A: 0x7fc0 byte-expand leaf (three call sites, direct unit).
 * Shape 1: src "o\\0" (2 iters, 16 steps, 1 store). Shape 2: 19x
 * 0x20 + NUL (19 iters, 152 steps, 18 stores). Shape 3: "hit     \\0"
 * (9 iters, 72 steps, 8 stores). Plus empty-string (8 steps, no
 * store) and a 300-byte no-NUL guard control (fail-closed). */
static void test_coli_7fc0(void) {
    static const uint8_t shape3_src[] = {
        0x68u, 0x69u, 0x74u, 0x20u, 0x20u, 0x20u, 0x20u, 0x20u, 0x00u
    };
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t k = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00020000))) !=
          NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00020000)) == VF2_OK);
    for (k = 0u; k < 19u; ++k) {
        rom[UINT32_C(0x00022978) + k] = 0x20u;
    }
    rom[UINT32_C(0x0002298a)] = 0x00u;
    for (k = 0u; k < sizeof(shape3_src); ++k) {
        rom[UINT32_C(0x00023d50) + k] = shape3_src[k];
    }
    for (k = 0u; k < 300u; ++k) {
        rom[UINT32_C(0x00023000) + k] = 0x20u;
    }
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00503200),
                                UINT32_C(0x0000006f)) == VF2_OK);

    /* Shape 1: site-A post-copy bytes, table 0x010007de. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00007fc0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00503200);
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010007de);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00007fc0),
                                       UINT32_C(0x00022964)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_7fc0_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022964));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(16));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u16(&machine, UINT32_C(0x010007de)) ==
          UINT16_C(0x806f));

    /* Shape 2: 0x9444 link data, table 0x0100085e. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00007fc0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00022978);
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x0100085e);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00007fc0),
                                       UINT32_C(0x00009450)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_7fc0_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00009450));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(152));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u16(&machine, UINT32_C(0x0100085e)) ==
          UINT16_C(0x8020));
    CHECK(read_test_u16(&machine, UINT32_C(0x01000880)) ==
          UINT16_C(0x8020));

    /* Shape 3: 0x23d50 table bytes, table 0x010006e8. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00007fc0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00023d50);
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010006e8);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00007fc0),
                                       UINT32_C(0x00022bd8)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_7fc0_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022bd8));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(72));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u16(&machine, UINT32_C(0x010006e8)) ==
          UINT16_C(0x8068));
    CHECK(read_test_u16(&machine, UINT32_C(0x010006f6)) ==
          UINT16_C(0x8020));

    /* Empty string: NUL first byte, 8 steps, no store. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00503200),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00007fc0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00503200);
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010007de);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00007fc0),
                                       UINT32_C(0x00022964)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_7fc0_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(8));

    /* Guard control: 300 non-NUL bytes never terminate. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00007fc0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x00023000);
    cpu.registers[VF2_I960_G0_REGISTER + 9u] = UINT32_C(0x010007de);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00007fc0),
                                       UINT32_C(0x00022964)) == VF2_OK);
    CHECK(vf2_hybrid_coli_7fc0_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0316: 0x225cc type-22 shortcut through 0x18bd4 (first-hit type 5). */
static void test_coli_225cc_type22(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    const uint32_t index = UINT32_C(1);
    const uint32_t record0 = UINT32_C(0x0201acb0);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    uint32_t stored = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00020000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0xd34c) + index * 4u, record0);
        /* Walk starts at record0+8. Type 5 → immediate match. */
        main_data[0x1acb8u] = 0x05u;
        /* Halfword at walk+1 for the g7+0x198 pack. */
        main_data[0x1acb9u] = 0x23u;
        main_data[0x1acbau] = 0x01u;
    }
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00020000)) == VF2_OK);

    CHECK(vf2_model2a_write_u32(&machine, fighter0, UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19f),
                            (const uint8_t *)"\x16", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19c),
                            (const uint8_t *)"\x01\x00", 2u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(53));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(4));
    /* g7+0x198 = (17<<24) + halfword at record+9. */
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == ((UINT32_C(17) << 24u) + UINT32_C(0x0123)));
    /* g8+0x198 = (1<<28) + index. */
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == ((UINT32_C(1) << 28u) + index));

    /* v0317: g7 word bit 2 set → 0x18b58 FIFO delta + common tail. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0, UINT32_C(4)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x198),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x198),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(80));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(4));
    /* 0x18b58 cleared bit 2 on the g7 word. */
    CHECK(vf2_model2a_read_u32(&machine, fighter0, &stored) == VF2_OK);
    CHECK((stored & UINT32_C(4)) == 0u);

    /* v0317: index bit 15 set → notbit 15 on the packed halfword. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0, UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x19c),
                            (const uint8_t *)"\x01\x80", 2u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x198),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x198),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000225cc));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000225cc),
                                       UINT32_C(0x00022240)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_225cc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022240));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(54));
    /* halfword 0x0123 with bit 15 flipped → 0x8123. */
    CHECK(vf2_model2a_read_u32(
              &machine, fighter0 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == ((UINT32_C(17) << 24u) + UINT32_C(0x8123)));
    CHECK(vf2_model2a_read_u32(
              &machine, fighter1 + UINT32_C(0x198), &stored) == VF2_OK);
    CHECK(stored == ((UINT32_C(1) << 28u) + UINT32_C(0x8001)));

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0312: 0x1ab34 table walk — two-iteration miss and first-hit match. */
static void test_coli_1ab34_walk(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    const uint32_t index = UINT32_C(0x421);
    const uint32_t record0 = UINT32_C(0x0201acb3);

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00020000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0xd34c) + index * 4u, record0);
        /* record0+0 type 3; +8+0x0e type 8. */
        main_data[0x1acbbu] = 0x03u;
        main_data[0x1acc9u] = 0x08u;
    }
    /* size table for type 3. */
    rom[0x1b7f9u] = 0x0eu;
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00020000)) == VF2_OK);

    /* Miss: type 3 then type 8 → g0 = 0, body 16. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0001ab34));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = UINT32_C(0x12340421);
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(9);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0001ab34),
                                       UINT32_C(0x00022e94)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_1ab34_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022e94));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(17));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);

    /* Match on first type: g0 = record+8, body 6. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0001ab34));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER] = index;
    cpu.registers[VF2_I960_G0_REGISTER + 1u] = UINT32_C(3);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0001ab34),
                                       UINT32_C(0x00022e94)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_1ab34_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(7));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == record0 + UINT32_C(8));

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

static void test_coli_23878_bit_remap(void) {
    static const uint32_t table[30] = {
        1u, 1u, 2u, 9u, 3u, 3u, 4u, 4u, 5u, 6u,
        6u, 7u, 7u, 8u, 10u, 10u, 10u, 11u, 11u, 11u,
        12u, 12u, 13u, 13u, 13u, 14u, 14u, 14u, 15u, 15u
    };
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const size_t data_size = UINT32_C(0x00008000);
    uint64_t start_instructions = 0u;
    size_t index = 0u;
    uint32_t expected = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, data_size)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    for (index = 0u; index < 30u; ++index) {
        write_u32_bytes(main_data, 0x7b76u + index * 4u, table[index]);
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data, data_size) ==
          VF2_OK);

    /* Empty source: 95 instructions, g3 stays 0. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023878));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 3u] = 0u;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023878),
                                       UINT32_C(0x00023b44)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_23878_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00023b44));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(95));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == 0u);

    /* Full bits 0..29: remap every entry, 155 instructions. */
    expected = 0u;
    for (index = 0u; index < 30u; ++index) {
        expected |= (UINT32_C(1) << table[index]);
    }
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023878));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(0x3fffffff);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023878),
                                       UINT32_C(0x00023b44)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_23878_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(155));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == expected);

    /* Single bit 0 -> mapped bit 1, 97 instructions. */
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023878));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(1);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023878),
                                       UINT32_C(0x00023b44)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_coli_23878_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(97));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 3u] == UINT32_C(2));

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

static void test_coli_238f8_warm_noop(void) {
    static const uint8_t rom_table[30] = {
        1u, 1u, 2u, 3u, 3u, 4u, 4u, 5u, 6u, 6u,
        7u, 7u, 8u, 9u, 10u, 10u, 10u, 11u, 11u, 11u,
        12u, 12u, 13u, 13u, 13u, 14u, 14u, 14u, 15u, 15u
    };
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t registry = UINT32_C(0x00514b80);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    uint8_t poison[4] = {UINT8_C(0xef), UINT8_C(0xbe), UINT8_C(0xad),
                         UINT8_C(0xde)};
    size_t index = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    for (index = 0u; index < 30u; ++index) {
        rom[0x23284u + index] = rom_table[index];
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    /* Warm source is all-zero; poison the dest cluster. */
    for (index = 0u; index < 30u; ++index) {
        CHECK(vf2_model2a_write(&machine,
                                registry + UINT32_C(0x40) +
                                    (uint32_t)index * 4u,
                                poison, sizeof(poison)) == VF2_OK);
    }

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000238f8));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000238f8),
                                       UINT32_C(0x00023644)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_238f8_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00023644));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(2855));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    /* All-zero source must not touch the poisoned dest cluster. */
    for (index = 0u; index < 30u; ++index) {
        CHECK(read_test_u16(&machine,
                            registry + UINT32_C(0x40) +
                                (uint32_t)index * 4u) ==
              UINT16_C(0xbeef));
    }

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_coli_233d0_flag_builder(void) {
    static const uint8_t table[24] = {
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x3fu, 0x00u, 0x00u, 0x00u,
        0x3fu, 0x00u, 0x00u, 0x00u,
        0x6cu, 0x33u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    const uint32_t registry = UINT32_C(0x00514980);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t index = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    for (index = 0u; index < sizeof(table); ++index) {
        rom[0x232c4u + index] = table[index];
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter0 + UINT32_C(0x1a4), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter1 + UINT32_C(0x1a4), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter0 + UINT32_C(0x1a8), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter1 + UINT32_C(0x1a8), 0u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000233d0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000233d0),
                                       UINT32_C(0x000235a0)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_233d0_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x000235a0));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(44));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 6u] == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xb4)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xb8)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xc0)) ==
          UINT32_C(0x3f));
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xc4)) ==
          UINT32_C(0x3f));
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xbc)) ==
          UINT32_C(0x0002336c));
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x88)) == 0u);

    /* Magic +0x1a8 state is an unmeasured sibling: fail closed. */
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x242)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000233d0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000233d0),
                                       UINT32_C(0x000235a0)) == VF2_OK);
    CHECK(vf2_hybrid_coli_233d0_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);
    CHECK(cpu.ip == UINT32_C(0x000233d0));

    /* Bit 18 of the or-mask is an unmeasured sibling: fail closed. */
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter0 + UINT32_C(0x1a8), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter1 + UINT32_C(0x1a4),
                                UINT32_C(1) << 18u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000233d0));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000233d0),
                                       UINT32_C(0x000235a0)) == VF2_OK);
    CHECK(vf2_hybrid_coli_233d0_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_coli_2364c_fifo_delta(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    const uint32_t registry = UINT32_C(0x00514980);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t index = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    for (index = 0u; index < 3u; ++index) {
        CHECK(vf2_model2a_write_u32(
                  &machine,
                  fighter0 + UINT32_C(0x1f4) + (uint32_t)index * 4u,
                  0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(
                  &machine,
                  fighter1 + UINT32_C(0x1f4) + (uint32_t)index * 4u,
                  0u) == VF2_OK);
    }

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0002364c));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0002364c),
                                       UINT32_C(0x000236b8)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_2364c_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x000236b8));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(17));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xc8)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xcc)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xd0)) == 0u);

    /* Non-zero +0x1f4 is an unmeasured sibling: fail closed. */
    CHECK(vf2_model2a_write_u32(&machine,
                                fighter0 + UINT32_C(0x1f4),
                                UINT32_C(0x3f800000)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0002364c));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0002364c),
                                       UINT32_C(0x000236b8)) == VF2_OK);
    CHECK(vf2_hybrid_coli_2364c_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_coli_2396c_poly_cluster(void) {
    static const uint32_t remap_table[30] = {
        1u, 1u, 2u, 9u, 3u, 3u, 4u, 4u, 5u, 6u,
        6u, 7u, 7u, 8u, 10u, 10u, 10u, 11u, 11u, 11u,
        12u, 12u, 13u, 13u, 13u, 14u, 14u, 14u, 15u, 15u
    };
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const size_t data_size = UINT32_C(0x00008000);
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t registry = UINT32_C(0x00514980);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t index = 0u;
    uint32_t remap_bodies = 0u;
    uint32_t expected_remap2 = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, data_size)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    for (index = 0u; index < 30u; ++index) {
        write_u32_bytes(main_data, 0x7b76u + index * 4u, remap_table[index]);
        expected_remap2 |= (UINT32_C(1) << remap_table[index]);
    }
    /* Index table: dest_index[i] = i (identity remap into cluster). */
    for (index = 0u; index < 30u; ++index) {
        rom[0x2394cu + index] = (uint8_t)index;
    }
    /* src table slot 0 -> 0x0090fa00 (test ROM buffer is LE). */
    rom[0x23944u + 0] = 0x00u;
    rom[0x23944u + 1] = 0xfau;
    rom[0x23944u + 2] = 0x90u;
    rom[0x23944u + 3] = 0x00u;
    /* dir count = 4 (LE). */
    rom[0x23bb8u] = 0x04u;
    rom[0x23bb8u + 1] = 0x00u;
    rom[0x23bb8u + 2] = 0x00u;
    rom[0x23bb8u + 3] = 0x00u;
    /* Direction table: four triples (LE). */
    {
        static const uint32_t dirs[12] = {
            UINT32_C(0x3f800000), 0u, UINT32_C(0xc0c00000),
            UINT32_C(0xbf800000), 0u, UINT32_C(0xc0c00000),
            0u, UINT32_C(0x3f800000), UINT32_C(0xc0c00000),
            0u, UINT32_C(0xbf800000), UINT32_C(0xc0c00000),
        };
        for (index = 0u; index < 12u; ++index) {
            rom[0x23bbcu + index * 4u + 0] = (uint8_t)dirs[index];
            rom[0x23bbcu + index * 4u + 1] =
                (uint8_t)(dirs[index] >> 8);
            rom[0x23bbcu + index * 4u + 2] =
                (uint8_t)(dirs[index] >> 16);
            rom[0x23bbcu + index * 4u + 3] =
                (uint8_t)(dirs[index] >> 24);
        }
    }
    /* scale_a / scale_b in Work RAM. */
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data, data_size) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a00c),
                                UINT32_C(0x41000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a010),
                                UINT32_C(0xbf000000)) == VF2_OK);
    /* g7+0x1c float field used by +0x108 threshold. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1c), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4), 0u) ==
          VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0002396c));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0002396c),
                                       UINT32_C(0x00023590)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    /* Zero cluster means all threshold compares fall on the warm side
     * only if the thresholds admit r7=r5=0. Warm +0xfc=+0.05, so
     * r7=0 > 0.05 is false -> setbit +0x10c fires. r5=0 > -0.1 is
     * true -> skip +0x110. r7=0 > -0.45 is true -> skip +0x114.
     * flags bit2/bit23 clear -> check +0x108. r7=0 > (0+0.05) is
     * false -> setbit +0x118. Inner loop: r10 = -8 + 0 + 0 = -8,
     * 0 > -8 is true -> skip positive. Matches warm. */
    CHECK(vf2_hybrid_coli_2396c_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00023590));
    /* remap1 empty (95); remap2/3 full 30-bit (155 each). Own 2618
     * includes the final ret, so complete adds 2617 + 405 + 1 ret. */
    remap_bodies = 95u + 155u + 155u;
    CHECK(cpu.executed_instructions - start_instructions ==
          UINT64_C(2618) + remap_bodies);
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(3));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(4));
    /* +0x10c and +0x118 have bits 0..29 set. */
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x10c)) ==
          UINT32_C(0x3fffffff));
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x118)) ==
          UINT32_C(0x3fffffff));
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x110)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x114)) == 0u);
    /* remap1 empty -> +0x624 = 0; remap2/3 remapped through the table. */
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x624)) == 0u);
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x614)) ==
          (uint16_t)expected_remap2);
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x618)) ==
          (uint16_t)expected_remap2);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

static void test_coli_23524_shell(void) {
    static const uint32_t remap_table[30] = {
        1u, 1u, 2u, 9u, 3u, 3u, 4u, 4u, 5u, 6u,
        6u, 7u, 7u, 8u, 10u, 10u, 10u, 11u, 11u, 11u,
        12u, 12u, 13u, 13u, 13u, 14u, 14u, 14u, 15u, 15u
    };
    static const uint8_t nested_rom[30] = {
        1u, 1u, 2u, 3u, 3u, 4u, 4u, 5u, 6u, 6u,
        7u, 7u, 8u, 9u, 10u, 10u, 10u, 11u, 11u, 11u,
        12u, 12u, 13u, 13u, 13u, 14u, 14u, 14u, 15u, 15u
    };
    static const uint8_t flag_table[24] = {
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x3fu, 0x00u, 0x00u, 0x00u,
        0x3fu, 0x00u, 0x00u, 0x00u,
        0x6cu, 0x33u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const size_t data_size = UINT32_C(0x00008000);
    const uint32_t fighter0 = UINT32_C(0x00510980);
    const uint32_t fighter1 = UINT32_C(0x00512980);
    const uint32_t registry = UINT32_C(0x00514980);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;
    size_t index = 0u;
    uint32_t remap_bodies = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, data_size)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    for (index = 0u; index < 30u; ++index) {
        write_u32_bytes(main_data, 0x7b76u + index * 4u, remap_table[index]);
        rom[0x2394cu + index] = (uint8_t)index;
        rom[0x23284u + index] = nested_rom[index];
    }
    for (index = 0u; index < sizeof(flag_table); ++index) {
        rom[0x232c4u + index] = flag_table[index];
    }
    rom[0x23944u + 0] = 0x00u;
    rom[0x23944u + 1] = 0xfau;
    rom[0x23944u + 2] = 0x90u;
    rom[0x23944u + 3] = 0x00u;
    rom[0x23bb8u] = 0x04u;
    rom[0x23bb8u + 1] = 0x00u;
    rom[0x23bb8u + 2] = 0x00u;
    rom[0x23bb8u + 3] = 0x00u;
    {
        static const uint32_t dirs[12] = {
            UINT32_C(0x3f800000), 0u, UINT32_C(0xc0c00000),
            UINT32_C(0xbf800000), 0u, UINT32_C(0xc0c00000),
            0u, UINT32_C(0x3f800000), UINT32_C(0xc0c00000),
            0u, UINT32_C(0xbf800000), UINT32_C(0xc0c00000),
        };
        for (index = 0u; index < 12u; ++index) {
            rom[0x23bbcu + index * 4u + 0] = (uint8_t)dirs[index];
            rom[0x23bbcu + index * 4u + 1] =
                (uint8_t)(dirs[index] >> 8);
            rom[0x23bbcu + index * 4u + 2] =
                (uint8_t)(dirs[index] >> 16);
            rom[0x23bbcu + index * 4u + 3] =
                (uint8_t)(dirs[index] >> 24);
        }
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data, data_size) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a00c),
                                UINT32_C(0x41000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050a010),
                                UINT32_C(0xbf000000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) ==
          VF2_OK);
    for (index = 0u; index < 3u; ++index) {
        CHECK(vf2_model2a_write_u32(
                  &machine,
                  fighter0 + UINT32_C(0x1f4) + (uint32_t)index * 4u,
                  0u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(
                  &machine,
                  fighter1 + UINT32_C(0x1f4) + (uint32_t)index * 4u,
                  0u) == VF2_OK);
    }
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a8), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1c), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1c), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0, 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1, 0u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00023524));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00023524),
                                       UINT32_C(0x00022210)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_23524_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022210));
    /* Shell 178 + two full 0x2396c (2618+405 each) + remaining children
     * + shell ret = 9151. */
    remap_bodies = 95u + 155u + 155u;
    CHECK(cpu.executed_instructions - start_instructions ==
          UINT64_C(178) + (UINT64_C(2618) + (uint64_t)remap_bodies) * 2u +
              UINT64_C(44) + UINT64_C(10) + UINT64_C(2855) + UINT64_C(17) +
              UINT64_C(1));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(13));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(14));
    /* g13+0x40 cluster cleared. */
    for (index = 0u; index < 16u; ++index) {
        CHECK(read_test_u32(&machine,
                            registry + UINT32_C(0x40) +
                                (uint32_t)index * 4u) == 0u);
    }
    /* Flag builder row. */
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xb4)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xb8)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x88)) == 0u);
    /* Shell cluster/rolling stores. */
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x144)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0x148)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xec)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xf0)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xf4)) == 0u);
    /* 0x2364c replies. */
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xc8)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xcc)) == 0u);
    CHECK(read_test_u32(&machine, registry + UINT32_C(0xd0)) == 0u);
    /* Six-word out cluster. */
    for (index = 0u; index < 6u; ++index) {
        CHECK(read_test_u32(&machine,
                            registry + UINT32_C(0xd4) +
                                (uint32_t)index * 4u) == 0u);
    }
    /* g6 cleared, g4 from FIFO replies (0). */
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 6u] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 4u] == 0u);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

static void test_coli_midbody_tail_warm(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    const uint32_t fighter1 = UINT32_C(0x00512800);
    const uint32_t registry = UINT32_C(0x00514b80);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    /* Warm path: both fighters bit 8 clear, zero snapshots, empty pending. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x4),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x4),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(56));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(4));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(5));
    /* Both bitmask results stored 0. */
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) == UINT16_C(0));
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x6dc)) == UINT16_C(0));
    /* Contact-query g0 left at 0; g14 is the last bal link.
     * Tail leaves g7/g8 swapped (0x22230/0x22234). */
    CHECK(cpu.registers[VF2_I960_G0_REGISTER] == 0u);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == fighter1);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == fighter0);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 14u] ==
          UINT32_C(0x00022428));

    /* Bit 8 set on fighter1 is an unmeasured sibling: fail closed. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(1) << 8u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    /* Bits 8 and 1 set is a measured sibling (v0290): admit it. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) | (UINT32_C(1) << 1u)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022298));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022298),
                                       UINT32_C(0x00022214)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_bitmask_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00022214));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(8));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x6dc)) == UINT16_C(0));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_coli_midbody_contact_hit_bit3(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    const uint32_t fighter1 = UINT32_C(0x00512800);
    const uint32_t registry = UINT32_C(0x00514b80);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00008000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x7ace), UINT32_C(8));
    }
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00008000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) ==
          VF2_OK);
    /* fighter0: bits 8+1 so second 0x22298 sibling and first contact g0=1. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) |
                                    (UINT32_C(1) << 1u)) == VF2_OK);
    /* fighter1: bit 3 for compact 0x225cc; bit 8 clear for warm contact. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                UINT32_C(1) << 3u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x4),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x4),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x40) +
                                             UINT32_C(3) * UINT32_C(4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(132));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(5));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(6));
    /* Compact 0x225cc undoes the counter increment. */
    CHECK(read_test_u16(&machine, fighter0 + UINT32_C(0x1234)) ==
          UINT16_C(0));
    /* Restored fighter assignment (not the swapped warm tail). */
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == fighter0);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == fighter1);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0305: first contact warm, second contact g0=1, compact 0x225cc
 * with the no-restore assignment (g7=fighter1, g8=fighter0).
 * Measured 129 insns before the parent ret → 130 / 5 / 6. */
static void test_coli_midbody_second_contact_bit3(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    const uint32_t fighter1 = UINT32_C(0x00512800);
    const uint32_t registry = UINT32_C(0x00514b80);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00008000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x7ace), UINT32_C(8));
    }
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00008000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) ==
          VF2_OK);
    /* fighter0: bit 3 only — compact 0x225cc g8, warm first contact. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                UINT32_C(1) << 3u) == VF2_OK);
    /* fighter1: bits 8+1 — first 0x22298 body 7, second contact g0=1. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) |
                                    (UINT32_C(1) << 1u)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x4),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x4),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    /* Equal snapshots: first warm contact rewrites g13+0x8c with
     * fighter0+0x1a8, so both +0x1a8 must match for the hit sibling. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a8),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0x1234)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x40) +
                                             UINT32_C(3) * UINT32_C(4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(130));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(5));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(6));
    /* Compact 0x225cc undoes the counter on fighter1 (g7). */
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x1234)) ==
          UINT16_C(0));
    /* No-restore assignment: g7=fighter1, g8=fighter0. */
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == fighter1);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == fighter0);

    /* Both-non-zero cascade remains fail-closed. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) |
                                    (UINT32_C(1) << 1u)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) ==
          VF2_ERROR_UNSUPPORTED);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
}

/* v0306: both contacts hit (slot0 + slot1), cascade early-out at
 * 0x22258 (fighter1+0x804 bit 15), single compact 0x225cc.
 * Measured 254 insns before the parent ret → 255 / 5 / 6. */
static void test_coli_midbody_both_contact_cascade(void) {
    uint8_t *rom = NULL;
    uint8_t *main_data = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t fighter0 = UINT32_C(0x00510800);
    const uint32_t fighter1 = UINT32_C(0x00512800);
    const uint32_t registry = UINT32_C(0x00514b80);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK((main_data = (uint8_t *)calloc(1u, UINT32_C(0x00008000))) !=
          NULL);
    if (main_data != NULL) {
        write_u32_bytes(main_data, UINT32_C(0x7ace), UINT32_C(8));
    }
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || main_data == NULL || machine.work_ram == NULL) {
        free(rom);
        free(main_data);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&machine, main_data,
                                       UINT32_C(0x00008000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500804), fighter0) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500808), fighter1) ==
          VF2_OK);
    /* fighter0 slot 0, bits 8+1+3 (compact 0x225cc g8). */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) |
                                    (UINT32_C(1) << 1u) |
                                    (UINT32_C(1) << 3u)) == VF2_OK);
    /* fighter1 slot 1, bits 8+1. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) |
                                    (UINT32_C(1) << 1u)) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter0 + UINT32_C(0x4),
                            (const uint8_t *)"\x00", 1u) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, fighter1 + UINT32_C(0x4),
                            (const uint8_t *)"\x01", 1u) == VF2_OK);
    /* Equal zero snapshots at slot 0 and slot 1. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8c),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x8e),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1aa),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x808),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x5b8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x820),
                                UINT32_C(1)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x40) +
                                             UINT32_C(3) * UINT32_C(4),
                                UINT32_C(0x0000ffff)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x6dc),
                                UINT32_C(0)) == VF2_OK);
    /* Cascade early-out: fighter0+0x804 bit 15 clear, fighter1 set. */
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x804),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x804),
                                UINT32_C(1) << 15u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;

    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(255));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(5));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(6));
    /* Compact 0x225cc undoes the counter on fighter1 (g7). */
    CHECK(read_test_u16(&machine, fighter1 + UINT32_C(0x1234)) ==
          UINT16_C(0));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == fighter1);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == fighter0);
    /* Both pending bits set (slot 0 and slot 1). */
    CHECK((read_test_u16(&machine, registry + UINT32_C(0x90)) & 3u) == 3u);

    /* v0315: tie-break cascade (bit 15 clear on both, pair equal).
     * Both 0x225cc take the compact bit-3 sibling. */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x804),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a4),
                                (UINT32_C(1) << 8u) |
                                    (UINT32_C(1) << 1u) |
                                    (UINT32_C(1) << 3u)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x822),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x822),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(282));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(7));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(8));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == fighter0);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == fighter1);

    /* v0307: pair-greater cascade arm (cmpobg → 0x22290). */
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x822),
                                UINT32_C(5)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x822),
                                UINT32_C(3)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, registry + UINT32_C(0x90),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter1 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, fighter0 + UINT32_C(0x1a8),
                                UINT32_C(0)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00022210));
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    cpu.registers[1] = UINT32_C(0x005ff580);
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter0;
    cpu.registers[VF2_I960_G0_REGISTER + 8u] = fighter1;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    cpu.registers[VF2_I960_G0_REGISTER + 13u] = registry;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00022210),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_coli_midbody_tail_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(258));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(5));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(6));
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 7u] == fighter1);
    CHECK(cpu.registers[VF2_I960_G0_REGISTER + 8u] == fighter0);

    vf2_model2a_shutdown(&machine);
    free(rom);
    free(main_data);
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

static void write_test_u8(vf2_model2a *machine, uint32_t address, uint8_t value) {
    CHECK(vf2_model2a_write(machine, address, &value, sizeof(value)) == VF2_OK);
}

static void write_test_u16(vf2_model2a *machine, uint32_t address, uint16_t value) {
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) == VF2_OK);
}

static void test_player_29414_type_paths(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t player = UINT32_C(0x00510980);
    const uint32_t return_address = UINT32_C(0x00028178);
    uint64_t start_instructions = 0u;
    uint64_t start_calls = 0u;
    uint64_t start_returns = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);

    /* Type 0: measured zero path, 8 instructions / 0 calls / 1 return. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 0u);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50),
                                UINT32_C(0xdeadbeef)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    start_calls = cpu.procedure_calls;
    start_returns = cpu.procedure_returns;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(8));
    CHECK(cpu.procedure_calls - start_calls == UINT64_C(0));
    CHECK(cpu.procedure_returns - start_returns == UINT64_C(1));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) == 0u);

    /* Type 6, bit-19 clear, scale 2.0: (0.970*2 - 2) = 0xbd75c280. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 6u);
    CHECK(vf2_model2a_write_u32(&machine, player + VF2_FIGHTER_OFF_01A4, 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0x84),
                                UINT32_C(0x40000000)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(14));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbd75c280));

    /* Type 8, bit-19 clear, scale 2.0: (0.920*2 - 2) = 0xbe23d708. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 8u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(16));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbe23d708));

    /* Type 10 shares the type-6 constant set. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 10u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(13));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbd75c280));

    /* Bit-19 set, window <= 10: same float tail, measured 17 insns type 6. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 6u);
    CHECK(vf2_model2a_write_u32(&machine, player + VF2_FIGHTER_OFF_01A4,
                                UINT32_C(0x00080000)) == VF2_OK);
    write_test_u16(&machine, player + UINT32_C(0x1aa), 10u);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50),
                                0u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(17));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbd75c280));

    /* Bit-19 set, window 11..20 (path A): halfword add r13*r11 + float tail. */
    write_test_u16(&machine, player + UINT32_C(0x1aa), 11u);
    write_test_u16(&machine, player + UINT32_C(0x18a), 0u);
    write_test_u16(&machine, player + UINT32_C(0x17c), 0u);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50), 0u) ==
          VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(34));
    CHECK(read_test_u32(&machine, UINT32_C(0x00884000)) == 1u);
    CHECK(read_test_u16(&machine, player + UINT32_C(0x18a)) ==
          UINT16_C(0xff00));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x17c)) ==
          UINT16_C(0xff00));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbd75c280));

    /* Path A with board bit 5: scratch writes, no halfword update. */
    write_test_u16(&machine, player + UINT32_C(0x1aa), 15u);
    write_test_u16(&machine, player + UINT32_C(0x18a), 0u);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x20)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50), 0u) ==
          VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(26));
    CHECK(read_test_u32(&machine, UINT32_C(0x00884000)) == 5u);
    CHECK(read_test_u16(&machine, player + UINT32_C(0x18a)) == 0u);
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbd75c280));

    /* Path B: window > 20, board bit 5 set -> early ret, no +0xc50 store. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 6u);
    write_test_u16(&machine, player + UINT32_C(0x1aa), 21u);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000),
                                UINT32_C(0x20)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50),
                                UINT32_C(0xdeadbeef)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(14));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xdeadbeef));

    /* Path B: +0x614 mask missing -> early ret. */
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    write_test_u16(&machine, player + UINT32_C(0x614), 0u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(18));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xdeadbeef));

    /* Path B: window 21 with mask -> halfword add of type-6 r8, no +0xc50. */
    write_test_u16(&machine, player + UINT32_C(0x614), 0x9000u);
    write_test_u16(&machine, player + UINT32_C(0x18a), 0u);
    write_test_u16(&machine, player + UINT32_C(0x17c), 0u);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50), 0u) ==
          VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(27));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x18a)) ==
          UINT16_C(0xf800));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x17c)) ==
          UINT16_C(0xf800));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) == 0u);

    /* Type 8 path A uses r13 = -409; body is type-6 + 2. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 8u);
    write_test_u16(&machine, player + UINT32_C(0x1aa), 11u);
    write_test_u16(&machine, player + UINT32_C(0x18a), 0u);
    write_test_u16(&machine, player + UINT32_C(0x17c), 0u);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50), 0u) ==
          VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(36));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x18a)) ==
          UINT16_C(0xfe67));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbe23d708));

    /* Type 10 path A: body is type-6 - 1. */
    write_test_u8(&machine, player + UINT32_C(0x1b1), 10u);
    write_test_u16(&machine, player + UINT32_C(0x18a), 0u);
    write_test_u16(&machine, player + UINT32_C(0x17c), 0u);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xc50), 0u) ==
          VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00029414));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.registers[VF2_I960_G0_REGISTER + 11u] = UINT32_C(0x00880000);
    cpu.registers[VF2_I960_G0_REGISTER + 12u] = UINT32_C(0x00004000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x00029414),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_29414_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(33));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x18a)) ==
          UINT16_C(0xff00));
    CHECK(read_test_u32(&machine, player + UINT32_C(0xc50)) ==
          UINT32_C(0xbd75c280));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

static void test_player_180bc_flag_tail(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    const uint32_t player = UINT32_C(0x00510980);
    const uint32_t return_address = UINT32_C(0x0001441c);
    uint64_t start_instructions = 0u;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);

    /* Warm: clear +0x1a4, clear +0x810 bit7 -> clear flag bit8, +0x6d8=0. */
    CHECK(vf2_model2a_write_u32(&machine, player + VF2_FIGHTER_OFF_01A4, 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player, 0u) == VF2_OK);
    write_test_u16(&machine, player + UINT32_C(0x26), 0u);
    write_test_u16(&machine, player + UINT32_C(0x810), 0u);
    write_test_u16(&machine, player + UINT32_C(0x5b4), 0xffffu);
    write_test_u8(&machine, player + UINT32_C(0x6d8), 0xffu);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000180bc));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000180bc),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_180bc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == return_address);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(19));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x5b4)) == 0u);
    CHECK(read_test_u32(&machine, player) == 0u);
    CHECK(read_test_u8(&machine, player + UINT32_C(0x6d8)) == 0u);

    /* Bit6 set: +0x5b4 = +0x26 + +0x812 (20 body + 1 ret). */
    CHECK(vf2_model2a_write_u32(&machine, player + VF2_FIGHTER_OFF_01A4,
                                UINT32_C(0x40)) == VF2_OK);
    write_test_u16(&machine, player + UINT32_C(0x26), 0x10u);
    write_test_u16(&machine, player + UINT32_C(0x812), 0x20u);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000180bc));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000180bc),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_180bc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(21));
    CHECK(read_test_u16(&machine, player + UINT32_C(0x5b4)) == 0x30u);

    /* Bit3 set + +0x810 bit7: set flag bit8 (17 body + 1 ret). */
    CHECK(vf2_model2a_write_u32(&machine, player + VF2_FIGHTER_OFF_01A4,
                                UINT32_C(0x08)) == VF2_OK);
    write_test_u16(&machine, player + UINT32_C(0x810), 0x80u);
    CHECK(vf2_model2a_write_u32(&machine, player, 0u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000180bc));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000180bc),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_180bc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(18));
    CHECK(read_test_u32(&machine, player) == UINT32_C(0x100));

    /* +0x1a4 bit0 set: skip the +0x6d8 store (14 body + 1 ret). */
    CHECK(vf2_model2a_write_u32(&machine, player + VF2_FIGHTER_OFF_01A4,
                                UINT32_C(1)) == VF2_OK);
    write_test_u16(&machine, player + UINT32_C(0x810), 0u);
    CHECK(vf2_model2a_write_u32(&machine, player, 0u) == VF2_OK);
    write_test_u8(&machine, player + UINT32_C(0x6d8), 0xaa);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x000180bc));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x000180bc),
                                       return_address) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_180bc_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(15));
    CHECK(read_test_u8(&machine, player + UINT32_C(0x6d8)) == 0xaa);

    /* 0x1441c tail sets flag bit7 and rets. */
    CHECK(vf2_model2a_write_u32(&machine, player, 0u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0001441c));
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0001441c),
                                       UINT32_C(0x00010dcc)) == VF2_OK);
    start_instructions = cpu.executed_instructions;
    CHECK(vf2_hybrid_player_1441c_execute(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == UINT32_C(0x00010dcc));
    CHECK(cpu.executed_instructions - start_instructions == UINT64_C(4));
    CHECK(read_test_u32(&machine, player) == UINT32_C(0x80));

    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0353: cold-boot first display — backup-SRAM diagnostic tile plane. */
static void test_post_boot_backup_broken_screen(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint8_t *rom = NULL;
    size_t index = 0u;
    static const char text[] = "BACKUP RAM IS BROKEN.";

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    /* Source constant used by execute_post_boot_backup_restore on invalid
     * backup signature (r14 = 0x0006df10, dest tile = 0x010008aa). */
    memcpy(rom + 0x0006df10u, text, sizeof(text));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    enter_parent(&cpu, UINT32_C(0x0006ddd8));
    cpu.registers[VF2_I960_G0_REGISTER] = 0u;
    cpu.ip = UINT32_C(0x0006ddd8);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.kind == VF2_NATIVE_RUNTIME_STEP_POST_BOOT_BACKUP_RESTORE);
    CHECK(report.entry_address == UINT32_C(0x0006ddd8));
    CHECK(cpu.ip == UINT32_C(0x00001004));
    for (index = 0u; index < sizeof(text) - 1u; ++index) {
        uint8_t encoded[2] = {0u, 0u};
        CHECK(vf2_model2a_read(&machine,
                               UINT32_C(0x010008aa) + (uint32_t)index * 2u, encoded,
                               sizeof(encoded)) == VF2_OK);
        CHECK(encoded[0] == (uint8_t)text[index]);
        CHECK(encoded[1] == UINT8_C(0x80));
    }
    /* Invalid signature still falls through to payload init + metadata write. */
    {
        uint32_t signature = 0u;
        CHECK(vf2_model2a_read_u32(&machine, VF2_BACKUP_SRAM_BASE + UINT32_C(0x3308),
                                   &signature) == VF2_OK);
        CHECK(signature == UINT32_C(0x54524956));
    }
    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0354: measured cold/attract signature path — selector 0 -> 2 in 34 insns. */
static void test_frame_dispatch_selector0_signature_fast_path(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint8_t *rom = NULL;
    uint8_t selector = 0u;
    uint32_t value = 0u;
    size_t index = 0u;
    static const uint32_t signature[4] = {
        UINT32_C(0x52455320), UINT32_C(0x4e4c2053),
        UINT32_C(0x4e204544), UINT32_C(0x20514555)};

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    write_u32_bytes(rom, UINT32_C(0x0000a6f8), UINT32_C(0x0000a804));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    for (index = 0u; index < 4u; ++index) {
        CHECK(vf2_model2a_write_u32(
                  &machine, UINT32_C(0x0059cfe0) + (uint32_t)index * 4u,
                  signature[index]) == VF2_OK);
    }
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &selector,
                            sizeof(selector)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500800), 0u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0000a6c0));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0000a6c0),
                                       UINT32_C(0x0000a010)) == VF2_OK);
    cpu.ip = UINT32_C(0x0000a6c0);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.recovered_instruction_count == UINT64_C(34));
    CHECK(cpu.ip == UINT32_C(0x0000a010));
    selector = 0xffu;
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002a), &selector,
                           sizeof(selector)) == VF2_OK);
    CHECK(selector == UINT8_C(2));
    /* Signature words are cleared by the helper after the compare. */
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfe0), &value) == VF2_OK);
    CHECK(value == 0u);
    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0360: SEGA warning screen (selector-0 draw) when signature is absent.
 * ROM strings 0xa9a4.. include "SEGA ENTERPRISES,LTD." at 0xaaad.
 * Natural witness park-after-irq: one frame -> sel=1, 15853 insns. */
static void test_frame_dispatch_selector0_sega_warning_draw(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state state;
    vf2_native_runtime_step_report report;
    uint8_t *rom = NULL;
    uint8_t selector = 0u;
    uint32_t cfg = UINT32_C(0x00599000);
    uint32_t value = 0u;
    size_t index = 0u;

    memset(&machine, 0, sizeof(machine));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        return;
    }
    rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE);
    CHECK(rom != NULL);
    if (rom == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    write_u32_bytes(rom, UINT32_C(0x0000a6f8), UINT32_C(0x0000a804));
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c), cfg) == VF2_OK);
    CHECK(vf2_model2a_write(&machine, cfg + UINT32_C(0x3350), "\0", 1u) == VF2_OK);
    for (index = 0u; index < 4u; ++index) {
        CHECK(vf2_model2a_write_u32(
                  &machine, UINT32_C(0x0059cfe0) + (uint32_t)index * 4u, 0u) ==
              VF2_OK);
    }
    selector = 0u;
    CHECK(vf2_model2a_write(&machine, UINT32_C(0x0050002a), &selector,
                            sizeof(selector)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500800), 0u) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0000a6c0));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0000a6c0),
                                       UINT32_C(0x0000a010)) == VF2_OK);
    cpu.ip = UINT32_C(0x0000a6c0);
    CHECK(vf2_native_runtime_initialize(&state, 4u) == VF2_OK);
    memset(&report, 0, sizeof(report));
    CHECK(vf2_native_runtime_step(&machine, &cpu, &state, &report) == VF2_OK);
    CHECK(report.recovered_instruction_count == UINT64_C(15853));
    CHECK(cpu.ip == UINT32_C(0x0000a010));
    selector = 0xffu;
    CHECK(vf2_model2a_read(&machine, UINT32_C(0x0050002a), &selector,
                           sizeof(selector)) == VF2_OK);
    CHECK(selector == UINT8_C(1));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x00500024), &value) == VF2_OK);
    CHECK(value == UINT32_C(640));
    CHECK(vf2_model2a_read_u32(&machine, UINT32_C(0x0059cfe0), &value) == VF2_OK);
    CHECK(value == 0u);
    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0357: 0x270d4/0x27b5c fail-closed when record/scratch are zero. */
static void test_player_27b5c_zero_record_fail_closed(void) {
    uint8_t *rom = NULL;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_hybrid_task_report report;
    const uint32_t player = UINT32_C(0x00510980);
    const uint32_t registry = UINT32_C(0x00515200);
    vf2_status st;

    CHECK((rom = (uint8_t *)calloc(1u, VF2_MAIN_ROM_SIZE)) != NULL);
    CHECK(vf2_model2a_initialize(&machine));
    if (rom == NULL || machine.work_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) ==
          VF2_OK);
    /* Measured sixth/punch shape: bit26 set, +0x1a4=0x20, record/scratch 0. */
    CHECK(vf2_model2a_write_u32(&machine, player, UINT32_C(0x04000000)) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0x1a4),
                                UINT32_C(0x20)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0x1a0), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, player + UINT32_C(0xbd8), 0u) ==
          VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x0050016c),
                                UINT32_C(0x00599000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00500068), 0u) == VF2_OK);
    CHECK(vf2_model2a_write_u32(&machine, UINT32_C(0x00508000), 0u) == VF2_OK);

    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x0001428c));
    cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    CHECK(vf2_i960_cpu_enter_procedure(&cpu, UINT32_C(0x0001428c),
                                       UINT32_C(0x00001004)) == VF2_OK);
    cpu.registers[29] = registry;
    cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    cpu.ip = UINT32_C(0x0001428c);
    memset(&report, 0, sizeof(report));
    st = vf2_hybrid_first_dispatch_task_execute(&machine, &cpu, registry,
                                                &report);
    /* Must not admit the 9726-instruction expander on zero record/scratch. */
    CHECK(!(st == VF2_OK && report.recovered_instruction_count ==
                                 UINT64_C(9726)));
    CHECK(!(st == VF2_OK && report.recovered_instruction_count ==
                                 UINT64_C(9745)));
    vf2_model2a_shutdown(&machine);
    free(rom);
}

/* v0359 ROM pin: five 0x270d4 slots vs oracle with COBR CC (9235 insns). */
static void test_player_270d4_five_slot_rom_pin(const char *rom_dir)
{
    static const uint32_t slot_bases[5] = {
        UINT32_C(0x005201e0), UINT32_C(0x005202d0), UINT32_C(0x005203c0),
        UINT32_C(0x005204b0), UINT32_C(0x005205a0),
    };
    static const uint32_t expected[5][20] = {
        {0, 0, 0, 0, 0, 0, 0, UINT32_C(0x44b605b0), 0, 0,
         UINT32_C(0x4660382d), 0, 0, 0, 0, 0,
         UINT32_C(0x46c2616c), 0, UINT32_C(0x46ff8000), UINT32_C(0xc67fc000)},
        {0, 0, 0, 0, 0, 0, 0, UINT32_C(0x44b605b0), 0, 0,
         UINT32_C(0x469b4d83), 0, 0, 0, 0, 0,
         UINT32_C(0x46c2616c), 0, UINT32_C(0x46ff8000), UINT32_C(0xc67fc000)},
        {0, 0, 0, 0, 0, 0, 0, UINT32_C(0x44b605b0), 0, 0,
         UINT32_C(0x4660382d), 0, 0, 0, 0, 0,
         UINT32_C(0x46c46205), 0, UINT32_C(0x47008065), UINT32_C(0xc67fc000)},
        {0, 0, 0, 0, 0, 0, 0, UINT32_C(0x44b605b0), 0, 0,
         UINT32_C(0x469b4d83), 0, 0, 0, 0, 0,
         UINT32_C(0x46c46205), 0, UINT32_C(0x47008065), UINT32_C(0xc67fc000)},
        {0, 0, 0, 0, 0, 0, UINT32_C(0x46713c72), UINT32_C(0xc511f6e6),
         UINT32_C(0x460c230a), UINT32_C(0x46653963), UINT32_C(0xc387fef0),
         UINT32_C(0xc666c667), UINT32_C(0x467f4000), 0, 0, 0,
         UINT32_C(0x467f4000), 0, UINT32_C(0x46ff7fff), UINT32_C(0xc67fc000)},
    };
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    vf2_model2a ref_machine;
    vf2_model2a c_machine;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu c_cpu;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_hybrid_task_report report;
    const uint32_t player = UINT32_C(0x00510980);
    const uint32_t registry = UINT32_C(0x00515200);
    vf2_status st;
    size_t slot = 0u;
    size_t word = 0u;

    CHECK(vf2_romset_build_region(
              rom_dir, VF2_REGION_MAINCPU, &main_rom, &main_rom_size) ==
          VF2_OK);
    CHECK(vf2_romset_build_region(
              rom_dir, VF2_REGION_MAIN_DATA, &main_data, &main_data_size) ==
          VF2_OK);
    if (main_rom == NULL || main_data == NULL) {
        free(main_rom);
        free(main_data);
        return;
    }
    memset(&ref_machine, 0, sizeof(ref_machine));
    memset(&c_machine, 0, sizeof(c_machine));
    CHECK(vf2_model2a_initialize(&ref_machine));
    CHECK(vf2_model2a_initialize(&c_machine));
    CHECK(vf2_model2a_attach_main_rom(
              &ref_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(
              &ref_machine, main_data, main_data_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(
              &c_machine, main_rom, main_rom_size) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(
              &c_machine, main_data, main_data_size) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &ref_machine, player + UINT32_C(0x1a0),
              UINT32_C(0x0201c2fc)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &ref_machine, player + UINT32_C(0xbd8),
              UINT32_C(0x00520000)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &c_machine, player + UINT32_C(0x1a0),
              UINT32_C(0x0201c2fc)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
              &c_machine, player + UINT32_C(0xbd8),
              UINT32_C(0x00520000)) == VF2_OK);

    vf2_i960_cpu_reset(&ref_cpu, 0u, 0u, UINT32_C(0x000270d4));
    ref_cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    ref_cpu.registers[31] = UINT32_C(0x005ff500);
    ref_cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    ref_cpu.ip = UINT32_C(0x000270d4);
    memset(&options, 0, sizeof(options));
    options.stop_address = UINT32_C(0x0002712c);
    options.max_steps = UINT64_C(20000);
    options.stop_on_self_branch = false;
    memset(&result, 0, sizeof(result));
    st = vf2_i960_run(&ref_cpu, &ref_machine, &options, &result);
    CHECK(st == VF2_OK);
    CHECK(ref_cpu.ip == UINT32_C(0x0002712c));
    CHECK(result.executed_instructions == UINT64_C(9235));

    vf2_i960_cpu_reset(&c_cpu, 0u, 0u, UINT32_C(0x000270d4));
    c_cpu.registers[1] = VF2_WORK_RAM_BASE + UINT32_C(0x3000);
    c_cpu.registers[VF2_I960_FP_REGISTER] = UINT32_C(0x005ff500);
    CHECK(vf2_i960_cpu_enter_procedure(
              &c_cpu, UINT32_C(0x00001000), UINT32_C(0x00001004)) == VF2_OK);
    CHECK(vf2_i960_cpu_enter_procedure(
              &c_cpu, UINT32_C(0x000270d4), UINT32_C(0x0002712c)) == VF2_OK);
    c_cpu.registers[29] = registry;
    c_cpu.registers[VF2_I960_G0_REGISTER + 7u] = player;
    c_cpu.ip = UINT32_C(0x000270d4);
    memset(&report, 0, sizeof(report));
    st = vf2_hybrid_first_dispatch_task_execute(
        &c_machine, &c_cpu, registry, &report);
    CHECK(st == VF2_OK);
    CHECK(c_cpu.ip == UINT32_C(0x0002712c));
    CHECK(report.recovered_instruction_count == UINT64_C(9235));

    for (slot = 0u; slot < 5u; ++slot) {
        for (word = 0u; word < 20u; ++word) {
            uint32_t ref_word = 0u;
            uint32_t c_word = 0u;
            CHECK(vf2_model2a_read_u32(
                      &ref_machine,
                      slot_bases[slot] + (uint32_t)word * 4u,
                      &ref_word) == VF2_OK);
            CHECK(vf2_model2a_read_u32(
                      &c_machine,
                      slot_bases[slot] + (uint32_t)word * 4u,
                      &c_word) == VF2_OK);
            if (ref_word != expected[slot][word] ||
                c_word != expected[slot][word]) {
                fprintf(
                    stderr,
                    "FAILED 270d4 slot%zu word%zu exp=%08x ref=%08x c=%08x\n",
                    slot, word, (unsigned)expected[slot][word],
                    (unsigned)ref_word, (unsigned)c_word);
                ++failures;
            }
        }
    }
    CHECK(c_cpu.registers[VF2_I960_G0_REGISTER + 3u] ==
          UINT32_C(0x00520630));
    CHECK(c_cpu.registers[VF2_I960_G0_REGISTER + 5u] ==
          UINT32_C(0x0050ea98));
    CHECK(c_cpu.registers[VF2_I960_G0_REGISTER + 6u] ==
          UINT32_C(0x0050e2d0));
    vf2_model2a_shutdown(&ref_machine);
    vf2_model2a_shutdown(&c_machine);
    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv) {
    test_initialize_and_names();
    test_post_boot_backup_broken_screen();
    test_frame_dispatch_selector0_signature_fast_path();
    test_frame_dispatch_selector0_sega_warning_draw();
    test_player_27b5c_zero_record_fail_closed();
    if (argc >= 2) {
        test_player_270d4_five_slot_rom_pin(argv[1]);
    }
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
    test_player_19ef8_selector_4505();
    test_budget_and_unsupported_are_explicit();
    test_multi_frame_run();
    test_repeated_scheduler_entry_dispatches_recovery();
    test_scheduler_selects_later_player_entry();
    test_scheduler_selects_coli_entry_at_index10();
    test_coli_bit5_set_early_ret();
    test_coli_bitmask_22298_early_path();
    test_coli_contact_query_22404_early_path();
    test_coli_238a4_early_path();
    test_coli_23238_early_out();
    test_coli_230d4_bit26();
    test_coli_502a4();
    test_coli_7fc0();
    test_coli_225cc_long();
    test_coli_225cc_type22();
    test_coli_1ab34_walk();
    test_coli_23878_bit_remap();
    test_coli_238f8_warm_noop();
    test_coli_233d0_flag_builder();
    test_coli_2364c_fifo_delta();
    test_coli_2396c_poly_cluster();
    test_coli_23524_shell();
    test_coli_midbody_tail_warm();
    test_coli_midbody_contact_hit_bit3();
    test_coli_midbody_second_contact_bit3();
    test_coli_midbody_both_contact_cascade();
    test_player_29414_type_paths();
    test_player_180bc_flag_tail();
    test_recurring_kill_osage_order_accounting();
    test_scheduler_finishes_after_early_last_active_task();

    if (failures != 0) {
        fprintf(stderr, "%d native-runtime test(s) failed\n", failures);
        return 1;
    }
    printf("native-runtime tests passed\n");
    return 0;
}
