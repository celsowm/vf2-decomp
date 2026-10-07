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

# Say out loud which sanitizer path is in effect. VF2_ENABLE_SANITIZERS used to
# apply its flags only to non-MSVC compilers, so on the canonical Windows
# checkout the documented sanitizer gate instrumented nothing while still
# reporting a pass. If the option is requested, the build must state whether it
# actually got a sanitizer.
if(VF2_ENABLE_SANITIZERS)
  if(MSVC)
    message(STATUS "VF2_ENABLE_SANITIZERS=ON: MSVC AddressSanitizer (/fsanitize=address)")
  else()
    message(STATUS "VF2_ENABLE_SANITIZERS=ON: GCC/Clang -fsanitize=address,undefined")
  endif()
else()
  message(STATUS "VF2_ENABLE_SANITIZERS=OFF: no sanitizer instrumentation")
endif()

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
    src/recovered/hybrid_game_info_poststate.c
    src/recovered/player_i960_bridge.c
    src/recovered/player_i960_bridge_tail.c
    src/recovered/frame_wait.c
    src/recovered/native_runtime.c
    src/recovered/native_runtime_object_step.c
    src/recovered/native_runtime_condition.c
    src/recovered/native_runtime_state.c
    src/recovered/object_service.c
    src/recovered/polygon_object_submit.c
    src/recovered/pol_test_path_a.c
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

# Which stepper vf2_i960_run drives. See
# decomp/i960/notes/executor_step_macro_asymmetry_v0733f.md.
#
# ON (default, unchanged): executor.c is compiled with vf2_i960_step textually
# substituted to vf2_i960_step_legacy, so vf2_i960_run -- and therefore
# vf2probe and vf2cycles -- never applies arch_fix_direct_compare, while every
# hand-stepping caller of vf2_i960_step gets the arch stepper that does.
#
# OFF: the macro is not applied, so vf2_i960_run uses the same arch stepper as
# everyone else. This exists so the blast radius can be MEASURED in a separate
# tree (configure with -DVF2_LEGACY_STEP_IN_RUN=OFF) before anyone decides
# whether the default should change. Do not flip the default on the strength of
# a green run: the variant must be classified move by move first.
option(VF2_LEGACY_STEP_IN_RUN
    "Compile vf2_i960_run against the legacy stepper (default ON)" ON)

if(VF2_LEGACY_STEP_IN_RUN)
    set_source_files_properties(
        src/i960/executor.c
        PROPERTIES COMPILE_DEFINITIONS "vf2_i960_step=vf2_i960_step_legacy"
    )
    message(STATUS "VF2_LEGACY_STEP_IN_RUN=ON: vf2_i960_run uses vf2_i960_step_legacy (arch_fix_direct_compare is NOT applied on that path)")
else()
    message(STATUS "VF2_LEGACY_STEP_IN_RUN=OFF: vf2_i960_run uses the arch stepper vf2_i960_step - VARIANT BUILD, oracle semantics differ from the default tree")
endif()

set_source_files_properties(
    src/recovered/native_runtime.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_native_runtime_step_impl=vf2_native_runtime_step_base_impl"
)

set_source_files_properties(
    src/recovered/texture_bridge.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_hybrid_post_frame_bridge_execute=vf2_hybrid_post_frame_bridge_execute_impl"
)

set_source_files_properties(
    src/recovered/hybrid.c
    PROPERTIES COMPILE_DEFINITIONS "vf2_i960_run=vf2_hybrid_i960_run_tail;vf2_hybrid_first_dispatch_task_execute=vf2_hybrid_first_dispatch_task_execute_base"
)

target_include_directories(vf2_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

vf2_set_project_warnings(vf2_core)

if(NOT WIN32 OR MINGW)
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

add_executable(make_orchestrator_snap tools/make_orchestrator_snap.c)
target_link_libraries(make_orchestrator_snap PRIVATE vf2_core)
vf2_set_project_warnings(make_orchestrator_snap)

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
    add_test(NAME vf2_orchestrator_limits COMMAND vf2_orchestrator_limits_tests)

    add_executable(vf2_tgp_tests tests/hardware/test_tgp.c)
    target_link_libraries(vf2_tgp_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_tgp_tests)
    add_test(NAME vf2_tgp COMMAND vf2_tgp_tests)

    add_executable(vf2_scsp_tests tests/hardware/test_scsp.c)
    target_link_libraries(vf2_scsp_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_scsp_tests)
    add_test(NAME vf2_scsp COMMAND vf2_scsp_tests)

    add_executable(vf2_sound_board_tests tests/hardware/test_sound_board.c)
    target_link_libraries(vf2_sound_board_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_sound_board_tests)
    add_test(NAME vf2_sound_board COMMAND vf2_sound_board_tests)

    add_executable(vf2_geometry_tests tests/hardware/test_geometry.c)
    target_link_libraries(vf2_geometry_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_geometry_tests)
    add_test(NAME vf2_geometry COMMAND vf2_geometry_tests)

    add_executable(vf2_model2a_observer_tests tests/hardware/test_model2a_observer.c)
    target_link_libraries(vf2_model2a_observer_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_model2a_observer_tests)
    add_test(NAME vf2_model2a_observer COMMAND vf2_model2a_observer_tests)

    add_executable(vf2_platform_tests tests/platform/test_platform.c)
    target_link_libraries(vf2_platform_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_platform_tests)
    add_test(NAME vf2_platform COMMAND vf2_platform_tests)

    add_executable(vf2_game_tests tests/game/test_game.c)
    target_link_libraries(vf2_game_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_game_tests)
    add_test(NAME vf2_game COMMAND vf2_game_tests)

    add_executable(vf2_m68k_tests tests/m68k/test_decoder.c)
    target_link_libraries(vf2_m68k_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_tests)
    add_test(NAME vf2_m68k COMMAND vf2_m68k_tests)

    add_executable(vf2_orchestrator_scan_tests tests/analysis/test_orchestrator_scan.c)
    target_link_libraries(vf2_orchestrator_scan_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_scan_tests)
    add_test(NAME vf2_orchestrator_scan COMMAND vf2_orchestrator_scan_tests)

    add_executable(vf2_orchestrator_gates_tests tests/analysis/test_orchestrator_gates.c)
    target_link_libraries(vf2_orchestrator_gates_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_gates_tests)
    add_test(NAME vf2_orchestrator_gates COMMAND vf2_orchestrator_gates_tests)

    add_library(vf2_test_owned_main_rom OBJECT tests/support/owned_main_rom.c)
    target_include_directories(vf2_test_owned_main_rom PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    vf2_set_project_warnings(vf2_test_owned_main_rom)

    add_executable(vf2_orchestrator_bridge_tests
        tests/analysis/test_orchestrator_bridge.c
        $<TARGET_OBJECTS:vf2_test_owned_main_rom>
    )
    if(MSVC)
        target_compile_options(vf2_orchestrator_bridge_tests PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/support/owned_main_rom.h")
    else()
        target_compile_options(vf2_orchestrator_bridge_tests PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/owned_main_rom.h")
    endif()
    target_link_libraries(vf2_orchestrator_bridge_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_bridge_tests)
    add_test(NAME vf2_orchestrator_bridge COMMAND vf2_orchestrator_bridge_tests)

    add_executable(vf2_native_runtime_tests tests/recovered/test_native_runtime.c)
    target_link_libraries(vf2_native_runtime_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_runtime_tests)
    add_test(NAME vf2_native_runtime COMMAND vf2_native_runtime_tests)
    if(EXISTS "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_270d4_five_slot_pin
                 COMMAND vf2_native_runtime_tests "${VF2_ROM_DIR}")
    endif()

    add_executable(vf2_native_runtime_state_tests tests/recovered/test_native_runtime_state.c)
    target_link_libraries(vf2_native_runtime_state_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_runtime_state_tests)
    add_test(NAME vf2_native_runtime_state COMMAND vf2_native_runtime_state_tests)

    add_executable(vf2_endurance_regression_tests tests/recovered/test_endurance_regressions.c)
    target_link_libraries(vf2_endurance_regression_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_endurance_regression_tests)
    add_test(NAME vf2_endurance_regressions COMMAND vf2_endurance_regression_tests)

    add_executable(vf2_native_differential_tests tests/recovered/test_native_differential.c)
    target_link_libraries(vf2_native_differential_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_differential_tests)
    add_test(NAME vf2_native_differential COMMAND vf2_native_differential_tests)

    add_executable(vf2_player_28178_stream_tests tests/recovered/test_player_28178_stream.c)
    target_link_libraries(vf2_player_28178_stream_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_28178_stream_tests)
    add_test(NAME vf2_player_28178_stream COMMAND vf2_player_28178_stream_tests)

    add_executable(vf2_player_planar_rotation_tests tests/recovered/test_player_planar_rotation.c)
    target_link_libraries(vf2_player_planar_rotation_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_planar_rotation_tests)
    add_test(NAME vf2_player_planar_rotation COMMAND vf2_player_planar_rotation_tests)

    add_executable(vf2_post_boot_input_profile_tests tests/recovered/test_post_boot_input_profiles.c)
    target_link_libraries(vf2_post_boot_input_profile_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_post_boot_input_profile_tests)

    add_executable(vf2_phase17_zero_tests tests/recovered/test_phase17_zero.c)
    target_link_libraries(vf2_phase17_zero_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_phase17_zero_tests)

    add_executable(vf2_native_scheduler_tests tests/recovered/test_native_scheduler.c)
    target_link_libraries(vf2_native_scheduler_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_scheduler_tests)
    add_test(NAME vf2_native_scheduler COMMAND vf2_native_scheduler_tests)

    add_executable(vf2_texture_status_tail_tests tests/recovered/test_texture_status_tail.c)
    target_link_libraries(vf2_texture_status_tail_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_texture_status_tail_tests)
    add_test(NAME vf2_texture_status_tail COMMAND vf2_texture_status_tail_tests)

    add_executable(vf2_texture_counter_update_tests tests/recovered/test_texture_counter_update.c)
    target_link_libraries(vf2_texture_counter_update_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_texture_counter_update_tests)
    add_test(NAME vf2_texture_counter_update COMMAND vf2_texture_counter_update_tests)

    add_executable(vf2_texture_upload_dispatch_tests tests/recovered/test_texture_upload_dispatch.c)
    target_link_libraries(vf2_texture_upload_dispatch_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_texture_upload_dispatch_tests)
    add_test(NAME vf2_texture_upload_dispatch COMMAND vf2_texture_upload_dispatch_tests)

    add_executable(vf2_fighter_candidate_tests tests/recovered/test_fighter_candidate.c)
    target_link_libraries(vf2_fighter_candidate_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_fighter_candidate_tests)
    add_test(NAME vf2_fighter_candidate COMMAND vf2_fighter_candidate_tests)

    add_executable(vf2_object_handler_tests tests/recovered/test_object_handlers.c)
    target_link_libraries(vf2_object_handler_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_object_handler_tests)

    add_executable(vf2_polygon_object_submit_tests tests/recovered/test_polygon_object_submit.c)
    target_link_libraries(vf2_polygon_object_submit_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_polygon_object_submit_tests)
    add_test(NAME vf2_polygon_object_submit COMMAND vf2_polygon_object_submit_tests)

    add_executable(vf2_pol_test_path_a_tests tests/recovered/test_pol_test_path_a.c)
    target_link_libraries(vf2_pol_test_path_a_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_pol_test_path_a_tests)
    add_test(NAME vf2_pol_test_path_a COMMAND vf2_pol_test_path_a_tests)

    add_executable(vf2_coli_225cc_live_tests tests/recovered/test_coli_225cc_live.c)
    target_link_libraries(vf2_coli_225cc_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_225cc_live_tests)
    add_test(NAME vf2_coli_225cc_live COMMAND vf2_coli_225cc_live_tests)

    add_executable(vf2_coli_22404_live_tests tests/recovered/test_coli_22404_live.c)
    target_link_libraries(vf2_coli_22404_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_22404_live_tests)
    add_test(NAME vf2_coli_22404_live COMMAND vf2_coli_22404_live_tests)

    add_executable(vf2_coli_midbody_tail_live_tests tests/recovered/test_coli_midbody_tail_live.c)
    target_link_libraries(vf2_coli_midbody_tail_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_midbody_tail_live_tests)
    add_test(NAME vf2_coli_midbody_tail_live COMMAND vf2_coli_midbody_tail_live_tests)

    add_executable(vf2_coli_238a4_live_tests tests/recovered/test_coli_238a4_live.c)
    target_link_libraries(vf2_coli_238a4_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_238a4_live_tests)
    add_test(NAME vf2_coli_238a4_live COMMAND vf2_coli_238a4_live_tests)

    # The 0x2396c cluster builder. test_coli_2396c_poly_cluster in
    # test_native_runtime.c is a self-consistency test: identity permutation,
    # cluster zeroed, reference never run. This one is the reference-vs-native
    # differential on the live permutation and the live 4th-word window.
    add_executable(vf2_coli_2396c_live_tests tests/recovered/test_coli_2396c_live.c)
    target_link_libraries(vf2_coli_2396c_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_2396c_live_tests)
    add_test(NAME vf2_coli_2396c_live COMMAND vf2_coli_2396c_live_tests)

    # The 0x23524 shell, whose g3/g4 publication was an open question in
    # hybrid.c. Admitted warm leg must FULL MATCH; the g6-bit0 leg must fail
    # closed, and the test proves that refusal is load-bearing rather than
    # merely cautious.
    add_executable(vf2_coli_23524_live_tests tests/recovered/test_coli_23524_live.c)
    target_link_libraries(vf2_coli_23524_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_23524_live_tests)
    add_test(NAME vf2_coli_23524_live COMMAND vf2_coli_23524_live_tests)

    add_executable(vf2_coli_whole_task_live_tests tests/recovered/test_coli_whole_task_live.c)
    target_link_libraries(vf2_coli_whole_task_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_coli_whole_task_live_tests)
    add_test(NAME vf2_coli_whole_task_live COMMAND vf2_coli_whole_task_live_tests)

    add_executable(vf2_player_4505_live_tests tests/recovered/test_player_4505_live.c)
    target_link_libraries(vf2_player_4505_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_4505_live_tests)
    add_test(NAME vf2_player_4505_live COMMAND vf2_player_4505_live_tests)

    add_executable(vf2_player_29598_tests tests/recovered/test_player_29598_native.c)
    target_link_libraries(vf2_player_29598_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_29598_tests)
    add_test(NAME vf2_player_29598 COMMAND vf2_player_29598_tests)

    add_executable(vf2_player_439ac_tests tests/recovered/test_player_439ac_native.c)
    target_link_libraries(vf2_player_439ac_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_439ac_tests)
    add_test(NAME vf2_player_439ac COMMAND vf2_player_439ac_tests)

    add_executable(vf2_player_43888_tests tests/recovered/test_player_43888_native.c)
    target_link_libraries(vf2_player_43888_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_43888_tests)
    add_test(NAME vf2_player_43888 COMMAND vf2_player_43888_tests)

    add_executable(vf2_player_142c0_live_tests tests/recovered/test_player_142c0_live.c)
    target_link_libraries(vf2_player_142c0_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_142c0_live_tests)
    add_test(NAME vf2_player_142c0_live COMMAND vf2_player_142c0_live_tests)

    add_executable(vf2_player_selector1_live_tests tests/recovered/test_player_selector1_live.c)
    target_link_libraries(vf2_player_selector1_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_selector1_live_tests)

    add_executable(vf2_player_1442c_live_tests tests/recovered/test_player_1442c_live.c)
    target_link_libraries(vf2_player_1442c_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1442c_live_tests)
    add_test(NAME vf2_player_1442c_live COMMAND vf2_player_1442c_live_tests)

    add_executable(vf2_player_28918_live_tests tests/recovered/test_player_28918_live.c)
    target_link_libraries(vf2_player_28918_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_28918_live_tests)
    add_test(NAME vf2_player_28918_live COMMAND vf2_player_28918_live_tests)
    add_executable(vf2_player_1442c_state16_live_tests tests/recovered/test_player_1442c_state16_live.c)
    target_link_libraries(vf2_player_1442c_state16_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1442c_state16_live_tests)
    add_test(NAME vf2_player_1442c_state16_live COMMAND vf2_player_1442c_state16_live_tests)

    add_executable(vf2_player_1453c_live_tests tests/recovered/test_player_1453c_live.c)
    target_link_libraries(vf2_player_1453c_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1453c_live_tests)
    add_test(NAME vf2_player_1453c_live COMMAND vf2_player_1453c_live_tests)

    add_executable(vf2_player_14640_state27_live_tests tests/recovered/test_player_14640_state27_live.c)
    target_link_libraries(vf2_player_14640_state27_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_state27_live_tests)
    add_test(NAME vf2_player_14640_state27_live COMMAND vf2_player_14640_state27_live_tests)

    add_executable(vf2_player_14640_state28_live_tests tests/recovered/test_player_14640_state28_live.c)
    target_link_libraries(vf2_player_14640_state28_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_state28_live_tests)
    add_test(NAME vf2_player_14640_state28_live COMMAND vf2_player_14640_state28_live_tests)

    add_executable(vf2_player_14640_bit4set_live_tests tests/recovered/test_player_14640_bit4set_live.c)
    target_link_libraries(vf2_player_14640_bit4set_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_bit4set_live_tests)
    add_test(NAME vf2_player_14640_bit4set_live COMMAND vf2_player_14640_bit4set_live_tests)

    add_executable(vf2_player_14640_escape_live_tests tests/recovered/test_player_14640_escape_live.c)
    target_link_libraries(vf2_player_14640_escape_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_escape_live_tests)
    add_test(NAME vf2_player_14640_escape_live COMMAND vf2_player_14640_escape_live_tests)

    add_executable(vf2_player_14640_compare_live_tests tests/recovered/test_player_14640_compare_live.c)
    target_link_libraries(vf2_player_14640_compare_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_compare_live_tests)
    add_test(NAME vf2_player_14640_compare_live COMMAND vf2_player_14640_compare_live_tests)

    add_executable(vf2_player_14640_compare_escape_live_tests tests/recovered/test_player_14640_compare_escape_live.c)
    target_link_libraries(vf2_player_14640_compare_escape_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_compare_escape_live_tests)
    add_test(NAME vf2_player_14640_compare_escape_live COMMAND vf2_player_14640_compare_escape_live_tests)

    add_executable(vf2_player_14640_state13_live_tests tests/recovered/test_player_14640_state13_live.c)
    target_link_libraries(vf2_player_14640_state13_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_state13_live_tests)
    add_test(NAME vf2_player_14640_state13_live COMMAND vf2_player_14640_state13_live_tests)

    add_executable(vf2_player_14640_compare_less_live_tests tests/recovered/test_player_14640_compare_less_live.c)
    target_link_libraries(vf2_player_14640_compare_less_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_compare_less_live_tests)
    add_test(NAME vf2_player_14640_compare_less_live COMMAND vf2_player_14640_compare_less_live_tests)

    add_executable(vf2_player_14640_compare_tail_live_tests tests/recovered/test_player_14640_compare_tail_live.c)
    target_link_libraries(vf2_player_14640_compare_tail_live_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_14640_compare_tail_live_tests)
    add_test(NAME vf2_player_14640_compare_tail_live COMMAND vf2_player_14640_compare_tail_live_tests)

    if(EXISTS "${VF2_ROM_DIR}")
        add_test(NAME vf2_phase17_zero_differential COMMAND vf2_phase17_zero_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_post_boot_input_profiles_differential COMMAND vf2_post_boot_input_profile_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_object_handlers_differential COMMAND vf2_object_handler_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_polygon_object_submit_differential COMMAND vf2_polygon_object_submit_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_pol_test_path_a_differential COMMAND vf2_pol_test_path_a_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_225cc_live_differential COMMAND vf2_coli_225cc_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_22404_live_differential COMMAND vf2_coli_22404_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_midbody_tail_live_differential COMMAND vf2_coli_midbody_tail_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_238a4_live_differential COMMAND vf2_coli_238a4_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_2396c_live_differential COMMAND vf2_coli_2396c_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_23524_live_differential COMMAND vf2_coli_23524_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_coli_whole_task_live_differential COMMAND vf2_coli_whole_task_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_4505_live_differential COMMAND vf2_player_4505_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_4505_mask_family_differential COMMAND vf2_player_4505_live_tests "${VF2_ROM_DIR}" "--mask-family")
        add_test(NAME vf2_player_142c0_live_differential COMMAND vf2_player_142c0_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_selector1_live_differential COMMAND vf2_player_selector1_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_1442c_live_differential COMMAND vf2_player_1442c_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_28918_live_differential COMMAND vf2_player_28918_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_1442c_state16_live_differential COMMAND vf2_player_1442c_state16_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_1453c_live_differential COMMAND vf2_player_1453c_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_state27_live_differential COMMAND vf2_player_14640_state27_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_state28_live_differential COMMAND vf2_player_14640_state28_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_bit4set_live_differential COMMAND vf2_player_14640_bit4set_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_escape_live_differential COMMAND vf2_player_14640_escape_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_compare_live_differential COMMAND vf2_player_14640_compare_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_compare_escape_live_differential COMMAND vf2_player_14640_compare_escape_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_state13_live_differential COMMAND vf2_player_14640_state13_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_compare_less_live_differential COMMAND vf2_player_14640_compare_less_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_player_14640_compare_tail_live_differential COMMAND vf2_player_14640_compare_tail_live_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_boot_differential COMMAND vf2i960 compare-boot "${VF2_ROM_DIR}")
        add_test(NAME vf2_initialization_differential COMMAND vf2i960 compare-init "${VF2_ROM_DIR}")
        add_test(NAME vf2_task_registry_differential COMMAND vf2i960 compare-task-registry "${VF2_ROM_DIR}")
        add_test(NAME vf2_task_catalog COMMAND vf2i960 tasks "${VF2_ROM_DIR}")
        add_test(NAME vf2_runtime_checkpoint COMMAND vf2i960 runtime-checkpoint "${VF2_ROM_DIR}")
        add_test(NAME vf2_runtime_events COMMAND vf2i960 scheduler-pass "${VF2_ROM_DIR}")
        add_test(NAME vf2_timer_irq_differential COMMAND vf2i960 compare-timer-irq "${VF2_ROM_DIR}")
        add_test(NAME vf2_scheduler_dispatch COMMAND vf2i960 scheduler-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_task_recoveries_differential COMMAND vf2i960 compare-task-recoveries "${VF2_ROM_DIR}")
        add_test(NAME vf2_task_profile COMMAND vf2i960 task-profile "${VF2_ROM_DIR}")
        add_test(NAME vf2_first_dispatch_recoveries COMMAND vf2i960 compare-first-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_camera_classifier_differential COMMAND vf2i960 compare-camera-classifier "${VF2_ROM_DIR}")
        add_test(NAME vf2_camera_viewport_differential COMMAND vf2i960 compare-camera-viewport "${VF2_ROM_DIR}")
        add_test(NAME vf2_hybrid_first_dispatch COMMAND vf2i960 hybrid-first-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_first_dispatch COMMAND vf2i960 native-first-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_second_dispatch COMMAND vf2i960 native-second-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_third_dispatch COMMAND vf2i960 native-third-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_fourth_dispatch COMMAND vf2i960 native-fourth-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_fifth_dispatch COMMAND vf2i960 native-fifth-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_sixth_dispatch COMMAND vf2i960 native-sixth-dispatch "${VF2_ROM_DIR}")
        add_test(NAME vf2_native_eleventh_dispatch COMMAND vf2i960 native-nth-dispatch "${VF2_ROM_DIR}" 11)
        add_test(NAME vf2_native_twelfth_dispatch COMMAND vf2i960 native-nth-dispatch "${VF2_ROM_DIR}" 12)
        add_test(NAME vf2_texture_bridge_differential COMMAND vf2i960 compare-texture-bridge "${VF2_ROM_DIR}")
        add_test(NAME vf2_post_frame_bridge_differential COMMAND vf2i960 compare-post-frame-bridge "${VF2_ROM_DIR}")
        add_test(NAME vf2_geometry_boundary_differential COMMAND vf2i960 compare-geometry-boundary "${VF2_ROM_DIR}")
        add_test(NAME vf2_second_scheduler_entry_differential COMMAND vf2i960 compare-second-scheduler-entry "${VF2_ROM_DIR}")
        add_test(NAME vf2_game_geometry_helpers_differential COMMAND vf2i960 compare-game-geometry-helpers "${VF2_ROM_DIR}")
        add_test(NAME vf2_third_sweep_observation COMMAND vf2i960 observe-third-sweep "${VF2_ROM_DIR}")
        set_tests_properties(
            vf2_runtime_checkpoint vf2_runtime_events vf2_timer_irq_differential
            vf2_scheduler_dispatch vf2_task_recoveries_differential vf2_task_profile
            vf2_first_dispatch_recoveries vf2_camera_classifier_differential
            vf2_camera_viewport_differential vf2_hybrid_first_dispatch
            vf2_native_first_dispatch vf2_native_second_dispatch
            vf2_native_third_dispatch vf2_native_fourth_dispatch
            vf2_native_fifth_dispatch vf2_native_sixth_dispatch
            vf2_native_eleventh_dispatch
            vf2_native_twelfth_dispatch
            vf2_texture_bridge_differential vf2_post_frame_bridge_differential
            vf2_geometry_boundary_differential vf2_second_scheduler_entry_differential
            vf2_game_geometry_helpers_differential vf2_third_sweep_observation
            PROPERTIES TIMEOUT 180
        )
    endif()

    # Python factory tooling tests (v0729a-e). They guard the analysis
    # layer's contracts (frontier.py v2, taint.py, infer_structs.py,
    # infer_rules.py). They are analysis-only and have no C dependency;
    # running them through ctest keeps the factory in lock-step with the C
    # gate. Python is required; on hosts without it, the tests are
    # skipped with a clear message rather than failing the suite.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME vf2_python_factory_frontier
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_frontier.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_taint
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_taint.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_taint_e2e
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_taint_e2e.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_infer_structs
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_infer_structs.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_infer_rules
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_infer_rules.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_check_scenario
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_check_scenario.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_sweep_state
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_sweep_state.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_explore_state
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_explore_state.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_minimize_case
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_minimize_case.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_trace_case
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_trace_case.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_z3_branch
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_z3_branch.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_chain
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_factory_chain.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_p1_real_trace_demo
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_p1_real_trace_demo.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_factory_block_coverage
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_block_coverage.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_f4_individual_release
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_f4_individual_release.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_phase_2_5_refused_audit
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_phase_2_5_refused_audit.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(
            vf2_python_factory_frontier
            vf2_python_factory_taint
            vf2_python_factory_taint_e2e
            vf2_python_factory_infer_structs
            vf2_python_factory_infer_rules
            vf2_python_factory_check_scenario
            vf2_python_factory_sweep_state
            vf2_python_factory_explore_state
            vf2_python_factory_minimize_case
            vf2_python_factory_trace_case
            vf2_python_factory_z3_branch
            vf2_python_factory_chain
            vf2_python_factory_p1_real_trace_demo
            vf2_python_factory_block_coverage
            vf2_f4_individual_release
            vf2_phase_2_5_refused_audit
            PROPERTIES TIMEOUT 900
        )
    else()
        message(STATUS
            "Python3 not found; skipping vf2_python_factory_* ctest wiring")
    endif()
endif()
