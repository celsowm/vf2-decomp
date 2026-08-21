cmake_minimum_required(VERSION 3.20)

project(
    vf2_decomp
    VERSION 0.1.3
    DESCRIPTION "Clean-room, non-matching C decompilation project for Virtua Fighter 2 Version 2.1"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(VF2_BUILD_TESTS "Build unit tests" ON)
option(VF2_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(VF2_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(VF2_ROM_DIR "" CACHE PATH "Optional supported VF2 ROM directory for integration tests")
if(NOT VF2_ROM_DIR)
  set(VF2_ROM_DIR "${CMAKE_SOURCE_DIR}/roms/vf2")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VF2Warnings)

add_library(vf2_core STATIC
    src/analysis/boundaries.c
    src/analysis/cfg.c
    src/analysis/image_map.c
    src/analysis/orchestrator_gates.c
    src/analysis/orchestrator_limits.c
    src/analysis/orchestrator_scan.c
    src/analysis/pseudoc.c
    src/analysis/semantics.c
    src/analysis/symbols.c
    src/analysis/tasks.c
    src/analysis/xref.c
    src/core/boot.c
    src/core/file.c
    src/core/hash.c
    src/core/rom_layout.c
    src/core/rom_manifest.c
    src/core/status.c
    src/game/game.c
    src/hardware/geometry.c
    src/hardware/model2a.c
    src/hardware/model2a_observer.c
    src/hardware/scsp.c
    src/hardware/sound_board.c
    src/hardware/tgp.c
    src/m68k/decoder.c
    src/i960/decoder.c
    src/i960/executor.c
    src/i960/executor_arch.c
    src/i960/formatter.c
    src/i960/instruction.c
    src/i960/snapshot.c
    src/platform/platform_null.c
    src/platform/platform_software.c
    src/recovered/boot_entry.c
    src/recovered/camera_viewport.c
    src/recovered/hybrid.c
    src/recovered/player_i960_bridge.c
    src/recovered/player_i960_bridge_tail.c
    src/recovered/frame_wait.c
    src/recovered/native_runtime.c
    src/recovered/native_runtime_condition.c
    src/recovered/native_runtime_state.c
    src/recovered/native_differential.c
    src/recovered/native_differential_step.c
    src/recovered/texture_bridge.c
    src/recovered/texture_bridge_condition.c
    src/recovered/texture_bridge_texture.c
    src/recovered/texture_bridge_status.c
    src/recovered/texture_bridge_video.c
    src/recovered/texture_bridge_geometry.c
    src/recovered/texture_bridge_input.c
    src/recovered/texture_bridge_match.c
    src/recovered/boot/boot_stage1.c
    src/recovered/boot/boot_stage2.c
    src/recovered/boot/memory_init.c
    src/recovered/task_registry.c
    src/recovered/runtime_interrupts.c
    src/recovered/scheduler.c
    src/recovered/tasks.c
)

set_source_files_properties(
    src/hardware/model2a.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_model2a_read=vf2_model2a_read_impl;vf2_model2a_write=vf2_model2a_write_impl;vf2_model2a_read_u32=vf2_model2a_read_u32_impl;vf2_model2a_write_u32=vf2_model2a_write_u32_impl"
)

set_source_files_properties(
    src/i960/executor.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_i960_step=vf2_i960_step_legacy"
)

set_source_files_properties(
    src/recovered/native_runtime.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_native_runtime_step=vf2_native_runtime_step_impl"
)

set_source_files_properties(
    src/recovered/texture_bridge.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_hybrid_post_frame_bridge_execute=vf2_hybrid_post_frame_bridge_execute_impl"
)

set_source_files_properties(
    src/recovered/hybrid.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_i960_run=vf2_hybrid_i960_run_tail"
)

target_include_directories(vf2_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

vf2_set_project_warnings(vf2_core)

if(NOT MSVC)
    target_link_libraries(vf2_core PUBLIC m)
endif()

add_executable(vf2 src/app/main.c)
target_link_libraries(vf2 PRIVATE vf2_core)
vf2_set_project_warnings(vf2)

add_executable(vf2rom tools/vf2rom/main.c)
target_link_libraries(vf2rom PRIVATE vf2_core)
vf2_set_project_warnings(vf2rom)

add_executable(vf2i960 tools/vf2i960/main.c)
target_link_libraries(vf2i960 PRIVATE vf2_core)
vf2_set_project_warnings(vf2i960)

add_executable(vf2cycles tools/vf2cycles/main.c)
target_link_libraries(vf2cycles PRIVATE vf2_core)
vf2_set_project_warnings(vf2cycles)

add_executable(vf2recover tools/vf2recover/main.c)
target_link_libraries(vf2recover PRIVATE vf2_core)
vf2_set_project_warnings(vf2recover)

add_executable(vf2probe tools/vf2probe/main.c)
target_link_libraries(vf2probe PRIVATE vf2_core)
vf2_set_project_warnings(vf2probe)

add_executable(vf2m68k tools/vf2m68k/main.c)
target_link_libraries(vf2m68k PRIVATE vf2_core)
vf2_set_project_warnings(vf2m68k)

include(GNUInstallDirs)
install(TARGETS vf2 vf2rom vf2i960 vf2cycles vf2recover vf2probe vf2m68k
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(VF2_BUILD_TESTS)
    enable_testing()

    add_executable(vf2_tests
        tests/test_main.c
        tests/i960/test_decoder.c
        tests/i960/test_executor.c
        tests/i960/test_snapshot.c
        tests/i960/test_interrupts.c
        tests/analysis/test_cfg.c
        tests/analysis/test_semantics.c
        tests/analysis/test_tasks.c
    )
    target_link_libraries(vf2_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_tests)

    add_test(NAME vf2_tests COMMAND vf2_tests)
    add_test(NAME vf2recover_help COMMAND vf2recover --help)
    add_test(NAME vf2probe_help COMMAND vf2probe --help)

    add_executable(vf2_orchestrator_limits_tests
        tests/analysis/test_orchestrator_limits.c
    )
    target_link_libraries(vf2_orchestrator_limits_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_limits_tests)

    add_test(
        NAME vf2_orchestrator_limits
        COMMAND vf2_orchestrator_limits_tests
    )

    add_executable(vf2_tgp_tests
        tests/hardware/test_tgp.c
    )
    target_link_libraries(vf2_tgp_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_tgp_tests)

    add_test(NAME vf2_tgp COMMAND vf2_tgp_tests)

    add_executable(vf2_scsp_tests
        tests/hardware/test_scsp.c
    )
    target_link_libraries(vf2_scsp_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_scsp_tests)

    add_test(NAME vf2_scsp COMMAND vf2_scsp_tests)

    add_executable(vf2_sound_board_tests
        tests/hardware/test_sound_board.c
    )
    target_link_libraries(vf2_sound_board_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_sound_board_tests)
    add_test(NAME vf2_sound_board COMMAND vf2_sound_board_tests)

    add_executable(vf2_geometry_tests
        tests/hardware/test_geometry.c
    )
    target_link_libraries(vf2_geometry_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_geometry_tests)

    add_test(NAME vf2_geometry COMMAND vf2_geometry_tests)

    add_executable(vf2_model2a_observer_tests
        tests/hardware/test_model2a_observer.c
    )
    target_link_libraries(vf2_model2a_observer_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_model2a_observer_tests)

    add_test(NAME vf2_model2a_observer COMMAND vf2_model2a_observer_tests)

    add_executable(vf2_platform_tests
        tests/platform/test_platform.c
    )
    target_link_libraries(vf2_platform_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_platform_tests)

    add_test(NAME vf2_platform COMMAND vf2_platform_tests)

    add_executable(vf2_game_tests
        tests/game/test_game.c
    )
    target_link_libraries(vf2_game_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_game_tests)

    add_test(NAME vf2_game COMMAND vf2_game_tests)

    add_executable(vf2_m68k_tests
        tests/m68k/test_decoder.c
    )
    target_link_libraries(vf2_m68k_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_tests)

    add_test(NAME vf2_m68k COMMAND vf2_m68k_tests)

    add_executable(vf2_orchestrator_scan_tests
        tests/analysis/test_orchestrator_scan.c
    )
    target_link_libraries(vf2_orchestrator_scan_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_scan_tests)

    add_test(
        NAME vf2_orchestrator_scan
        COMMAND vf2_orchestrator_scan_tests
    )

    add_executable(vf2_orchestrator_gates_tests
        tests/analysis/test_orchestrator_gates.c
    )
    target_link_libraries(vf2_orchestrator_gates_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_gates_tests)

    add_test(
        NAME vf2_orchestrator_gates
        COMMAND vf2_orchestrator_gates_tests
    )

    add_library(vf2_test_owned_main_rom OBJECT
        tests/support/owned_main_rom.c
    )
    target_include_directories(vf2_test_owned_main_rom PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    vf2_set_project_warnings(vf2_test_owned_main_rom)

    add_executable(vf2_orchestrator_bridge_tests
        tests/analysis/test_orchestrator_bridge.c
        $<TARGET_OBJECTS:vf2_test_owned_main_rom>
    )
    if(MSVC)
        target_compile_options(vf2_orchestrator_bridge_tests PRIVATE
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/support/owned_main_rom.h"
        )
    else()
        target_compile_options(vf2_orchestrator_bridge_tests PRIVATE
            -include
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/owned_main_rom.h"
        )
    endif()
    target_link_libraries(vf2_orchestrator_bridge_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_bridge_tests)

    add_test(
        NAME vf2_orchestrator_bridge
        COMMAND vf2_orchestrator_bridge_tests
    )

    add_executable(vf2_native_runtime_tests
        tests/recovered/test_native_runtime.c
    )
    target_link_libraries(vf2_native_runtime_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_runtime_tests)

    add_test(
        NAME vf2_native_runtime
        COMMAND vf2_native_runtime_tests
    )

    add_executable(vf2_native_runtime_state_tests
        tests/recovered/test_native_runtime_state.c
    )
    target_link_libraries(vf2_native_runtime_state_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_runtime_state_tests)

    add_test(
        NAME vf2_native_runtime_state
        COMMAND vf2_native_runtime_state_tests
    )

    add_executable(vf2_endurance_regression_tests
        tests/recovered/test_endurance_regressions.c
    )
    target_link_libraries(vf2_endurance_regression_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_endurance_regression_tests)

    add_test(
        NAME vf2_endurance_regressions
        COMMAND vf2_endurance_regression_tests
    )

    add_executable(vf2_native_differential_tests
        tests/recovered/test_native_differential.c
    )
    target_link_libraries(vf2_native_differential_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_differential_tests)

    add_test(
        NAME vf2_native_differential
        COMMAND vf2_native_differential_tests
    )

    add_executable(vf2_player_28178_stream_tests
        tests/recovered/test_player_28178_stream.c
    )
    target_link_libraries(vf2_player_28178_stream_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_28178_stream_tests)

    add_test(
        NAME vf2_player_28178_stream
        COMMAND vf2_player_28178_stream_tests
    )

    add_executable(vf2_player_planar_rotation_tests
        tests/recovered/test_player_planar_rotation.c
    )
    target_link_libraries(vf2_player_planar_rotation_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_planar_rotation_tests)

    add_test(
        NAME vf2_player_planar_rotation
        COMMAND vf2_player_planar_rotation_tests
    )

    add_executable(vf2_post_boot_input_profile_tests
        tests/recovered/test_post_boot_input_profiles.c
    )
    target_link_libraries(vf2_post_boot_input_profile_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_post_boot_input_profile_tests)

    add_executable(vf2_phase17_zero_tests
        tests/recovered/test_phase17_zero.c
    )
    target_link_libraries(vf2_phase17_zero_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_phase17_zero_tests)

    add_executable(vf2_native_scheduler_tests
        tests/recovered/test_native_scheduler.c
    )
    target_link_libraries(vf2_native_scheduler_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_scheduler_tests)

    add_test(
        NAME vf2_native_scheduler
        COMMAND vf2_native_scheduler_tests
    )

    add_executable(vf2_texture_status_tail_tests
        tests/recovered/test_texture_status_tail.c
    )
    target_link_libraries(vf2_texture_status_tail_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_texture_status_tail_tests)

    add_test(
        NAME vf2_texture_status_tail
        COMMAND vf2_texture_status_tail_tests
    )

    if(EXISTS "${VF2_ROM_DIR}")
        add_test(
            NAME vf2_phase17_zero_differential
            COMMAND vf2_phase17_zero_tests "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_post_boot_input_profiles_differential
            COMMAND vf2_post_boot_input_profile_tests "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_boot_differential
            COMMAND vf2i960 compare-boot "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_initialization_differential
            COMMAND vf2i960 compare-init "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_registry_differential
            COMMAND vf2i960 compare-task-registry "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_catalog
            COMMAND vf2i960 tasks "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_runtime_checkpoint
            COMMAND vf2i960 runtime-checkpoint "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_runtime_events
            COMMAND vf2i960 scheduler-pass "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_timer_irq_differential
            COMMAND vf2i960 compare-timer-irq "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_scheduler_dispatch
            COMMAND vf2i960 scheduler-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_recoveries_differential
            COMMAND vf2i960 compare-task-recoveries "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_profile
            COMMAND vf2i960 task-profile "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_first_dispatch_recoveries
            COMMAND vf2i960 compare-first-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_camera_classifier_differential
            COMMAND vf2i960 compare-camera-classifier "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_camera_viewport_differential
            COMMAND vf2i960 compare-camera-viewport "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_hybrid_first_dispatch
            COMMAND vf2i960 hybrid-first-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_first_dispatch
            COMMAND vf2i960 native-first-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_second_dispatch
            COMMAND vf2i960 native-second-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_third_dispatch
            COMMAND vf2i960 native-third-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_fourth_dispatch
            COMMAND vf2i960 native-fourth-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_fifth_dispatch
            COMMAND vf2i960 native-fifth-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_sixth_dispatch
            COMMAND vf2i960 native-sixth-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_texture_bridge_differential
            COMMAND vf2i960 compare-texture-bridge "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_post_frame_bridge_differential
            COMMAND vf2i960 compare-post-frame-bridge "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_geometry_boundary_differential
            COMMAND vf2i960 compare-geometry-boundary "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_second_scheduler_entry_differential
            COMMAND vf2i960 compare-second-scheduler-entry "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_game_geometry_helpers_differential
            COMMAND vf2i960 compare-game-geometry-helpers "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_third_sweep_observation
            COMMAND vf2i960 observe-third-sweep "${VF2_ROM_DIR}"
        )
        set_tests_properties(
            vf2_runtime_checkpoint vf2_runtime_events vf2_timer_irq_differential
            vf2_scheduler_dispatch vf2_task_recoveries_differential vf2_task_profile
            vf2_first_dispatch_recoveries vf2_camera_classifier_differential
            vf2_camera_viewport_differential vf2_hybrid_first_dispatch
            vf2_native_first_dispatch vf2_native_second_dispatch
            vf2_native_third_dispatch vf2_native_fourth_dispatch
            vf2_native_fifth_dispatch vf2_native_sixth_dispatch
            vf2_texture_bridge_differential
            vf2_post_frame_bridge_differential
            vf2_geometry_boundary_differential
            vf2_second_scheduler_entry_differential
            vf2_game_geometry_helpers_differential
            vf2_third_sweep_observation
            PROPERTIES TIMEOUT 180
        )
    endif()
endif()