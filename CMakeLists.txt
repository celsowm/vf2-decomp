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
option(VF2_BUILD_HOST "Build vf2_host, the SDL3 window/input/audio shell" OFF)
option(VF2_BUILD_VIEWER "Build vf2_model_viewer, the SDL3 extracted-model viewer" OFF)
option(VF2_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(VF2_ENABLE_LIFTED "Generate and compile the build-time lifted i960->C bridge (docs/VF2LIFT.md); scaffolding, opt-in" OFF)
option(VF2_VERIFY_AD21_BA "Verify owner-ROM AD21 BA payload-table dispatch and preserved RAM mutations" OFF)
option(VF2_VERIFY_AD21_B_ALL "Require owner-ROM AD21 parity for all twelve B9..BE packets" OFF)
option(VF2_VERIFY_AD21_B_SEQUENTIAL "Require owner-ROM AD21 full twelve B-packet stateful drain parity" OFF)
option(VF2_VERIFY_FRAME342_CHAIN "Require owner-ROM 9E/Cx/AD21/L2/B stateful chain and PCM parity" OFF)
option(VF2_VERIFY_AKIRA_MUSIC "Require original-ROM Akira SOUND TEST stream to KEYON and nonzero software PCM" OFF)
option(VF2_VERIFY_AKIRA_WALK "Require original-ROM Akira 32-descriptor PCM page-refill walker regression" OFF)
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
    src/app/session.c
    src/game/game.c
    src/hardware/geometry.c
    src/host/host_common.c
    src/hardware/model2a.c
    src/hardware/model2a_observer.c
    src/hardware/scsp.c
    src/hardware/sound_board.c
    src/hardware/tgp.c
    src/m68k/decoder.c
    src/m68k/stepper.c
    src/i960/decoder.c
    src/i960/executor.c
    src/i960/executor_arch.c
    src/i960/formatter.c
    src/i960/instruction.c
    src/i960/snapshot.c
    src/lift/runtime.c
    src/platform/platform_null.c
    src/platform/platform_software.c
    src/platform/tile.c
    src/platform/model_decode.c
    src/platform/texture_sheet.c
    src/platform/texture_link.c
    src/platform/geo_display.c
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

# Build-time i960->C recompiler (plan phase 4, Option B; docs/VF2LIFT.md).
# The tool always builds; the generated C is opt-in and never committed.
add_executable(vf2lift tools/vf2lift/main.c)
target_link_libraries(vf2lift PRIVATE vf2_core)
vf2_set_project_warnings(vf2lift)

include(GNUInstallDirs)
install(TARGETS vf2 vf2rom vf2i960 vf2cycles vf2recover vf2probe vf2m68k vf2lift
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# --- Lifted i960->C bridge (opt-in, scaffolding; docs/VF2LIFT.md) ------------
# When enabled, vf2lift regenerates one C function per selected guest function
# from the user's ROM into the build directory at build time. The generated C
# is never committed. The lift set is driven by the committed Phase 3 coverage
# backlog (the functions the attract session actually reached) plus any extra
# addresses listed in VF2_LIFT_FUNCTIONS.
set(VF2_LIFT_FUNCTIONS
    0x00010cbc  # task_registry_initialize
    0x00000b58  # iac_reinitialize_copy
    0x00010d54  # scheduler_dispatch
    CACHE STRING "Extra guest entry addresses vf2lift emits when VF2_ENABLE_LIFTED=ON")
set(VF2_LIFT_BACKLOG "${CMAKE_SOURCE_DIR}/decomp/i960/coverage_attract_backlog.csv"
    CACHE FILEPATH "Coverage backlog CSV that selects the lifted function set")

if(VF2_ENABLE_LIFTED)
  if(EXISTS "${VF2_ROM_DIR}")
    set(VF2_LIFT_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
    set(VF2_LIFT_GEN_C "${VF2_LIFT_GEN_DIR}/vf2_lifted.c")
    set(VF2_LIFT_GEN_H "${VF2_LIFT_GEN_DIR}/vf2_lifted.h")
    add_custom_command(
      OUTPUT "${VF2_LIFT_GEN_C}" "${VF2_LIFT_GEN_H}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VF2_LIFT_GEN_DIR}"
      COMMAND "$<TARGET_FILE:vf2lift>" "${VF2_ROM_DIR}"
              "${VF2_LIFT_GEN_C}" "${VF2_LIFT_GEN_H}"
              --backlog "${VF2_LIFT_BACKLOG}" ${VF2_LIFT_FUNCTIONS}
      DEPENDS vf2lift "${VF2_LIFT_BACKLOG}"
      COMMENT "vf2lift: generating lifted i960->C bridge into ${VF2_LIFT_GEN_DIR}"
      VERBATIM
    )
    add_library(vf2_lifted STATIC "${VF2_LIFT_GEN_C}")
    target_include_directories(vf2_lifted PUBLIC "${VF2_LIFT_GEN_DIR}")
    target_link_libraries(vf2_lifted PUBLIC vf2_core)
    vf2_set_project_warnings(vf2_lifted)

    # Cold boot snapshot for the session-scope lifted check (ROM-derived, never
    # committed). Generated once by the recovered runtime to its sixth-dispatch
    # checkpoint, the same snapshot scripts/coverage_report uses.
    set(VF2_LIFT_COLD_SNAP "${VF2_LIFT_GEN_DIR}/cold.vf2snap")
    add_custom_command(
      OUTPUT "${VF2_LIFT_COLD_SNAP}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VF2_LIFT_GEN_DIR}"
      COMMAND "$<TARGET_FILE:vf2i960>" native-sixth-dispatch
              "${VF2_ROM_DIR}" "${VF2_LIFT_COLD_SNAP}"
      DEPENDS vf2i960
      COMMENT "vf2lift: generating cold boot snapshot for the session check"
      VERBATIM
    )
    add_custom_target(vf2_lift_cold_snapshot DEPENDS "${VF2_LIFT_COLD_SNAP}")
    message(STATUS "VF2_ENABLE_LIFTED=ON: lifted bridge will be generated from ${VF2_ROM_DIR}")
  else()
    message(WARNING "VF2_ENABLE_LIFTED=ON but VF2_ROM_DIR (${VF2_ROM_DIR}) not found; "
                    "lifted bridge not generated")
  endif()
endif()

if(VF2_BUILD_HOST OR VF2_BUILD_VIEWER)
  # SDL3 is only ever linked by the host shell and the model viewer.  Prefer a
  # system or vcpkg package; otherwise fetch the pinned release and link static.
  find_package(SDL3 3.2.0 CONFIG QUIET)
  if(NOT SDL3_FOUND)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.2.8
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
  endif()
endif()

if(VF2_BUILD_HOST)
  add_executable(vf2_host src/host/host_sdl.c)
  target_link_libraries(vf2_host PRIVATE vf2_core SDL3::SDL3)
  vf2_set_project_warnings(vf2_host)
  install(TARGETS vf2_host RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  if(VF2_BUILD_TESTS)
    # ROM-free: real SDL event queue, dummy video and audio drivers.
    add_test(NAME vf2_host_selftest COMMAND vf2_host --selftest --frames 30)
    set_tests_properties(vf2_host_selftest PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
      TIMEOUT 60)
  endif()
endif()

if(VF2_BUILD_VIEWER)
  # The viewer renders textured meshes with SDL_GPU (SDL3's GPU API) and SPIR-V
  # shaders. SPIR-V is produced at build time from the HLSL sources and embedded
  # via bin2c so the viewer has no runtime shader-file dependency. The HLSL->
  # SPIR-V compiler is glslangValidator (the Windows SDK's dxc here is DXIL-only;
  # glslang produces the exact SDL_GPU descriptor sets: vertex UBO=set 1,
  # fragment texture/sampler=set 2). glslc (shaderc) also works if preferred.
  find_program(VF2_HLSL2SPV NAMES glslangValidator glslc)
  if(NOT VF2_HLSL2SPV)
    message(FATAL_ERROR
      "VF2_BUILD_VIEWER=ON needs a HLSL->SPIR-V compiler. Install glslang "
      "(pacman -S mingw-w64-ucrt-x86_64-glslang) or shaderc, or set VF2_HLSL2SPV.")
  endif()
  set(VF2_VIEWER_GEN "${CMAKE_BINARY_DIR}/generated/vf2_model_viewer")
  file(MAKE_DIRECTORY "${VF2_VIEWER_GEN}")
  set(VF2_VIEWER_SHADER_HEADERS "")
  foreach(stage vert frag)
    if(stage STREQUAL "vert")
      set(sym "model_vert_spv")
    else()
      set(sym "model_frag_spv")
    endif()
    set(src "${CMAKE_SOURCE_DIR}/tools/vf2_model_viewer/shaders/model.${stage}.hlsl")
    set(spv "${VF2_VIEWER_GEN}/model.${stage}.spv")
    set(hdr "${VF2_VIEWER_GEN}/model_${stage}_spv.h")
    add_custom_command(
      OUTPUT "${spv}"
      COMMAND "${VF2_HLSL2SPV}" -e main -S ${stage} -D
              --target-env vulkan1.0 -o "${spv}" "${src}"
      DEPENDS "${src}"
      COMMENT "glslang: ${stage} HLSL -> SPIR-V"
      VERBATIM)
    add_custom_command(
      OUTPUT "${hdr}"
      COMMAND "${CMAKE_COMMAND}" -DIN=${spv} -DOUT=${hdr} -DNAME=${sym}
              -P "${CMAKE_SOURCE_DIR}/tools/vf2_model_viewer/bin2c.cmake"
      DEPENDS "${spv}"
      COMMENT "embed ${stage} SPIR-V -> ${sym}"
      VERBATIM)
    list(APPEND VF2_VIEWER_SHADER_HEADERS "${hdr}")
  endforeach()

  add_executable(vf2_model_viewer tools/vf2_model_viewer/main.c
    ${VF2_VIEWER_SHADER_HEADERS})
  target_include_directories(vf2_model_viewer PRIVATE "${VF2_VIEWER_GEN}")
  target_link_libraries(vf2_model_viewer PRIVATE vf2_core SDL3::SDL3)
  if(NOT WIN32)
    target_link_libraries(vf2_model_viewer PRIVATE m)
  endif()
  vf2_set_project_warnings(vf2_model_viewer)
  install(TARGETS vf2_model_viewer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  if(VF2_BUILD_TESTS)
    # ROM-free: loads the OBJ, inits SDL, probes for a GPU device, exits 0
    # (headless CI has no GPU, which is not a failure).
    add_test(NAME vf2_model_viewer_selftest COMMAND vf2_model_viewer --selftest)
    set_tests_properties(vf2_model_viewer_selftest PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 60)
  endif()
endif()

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

    add_executable(vf2_model_decode_tests tests/platform/test_model_decode.c)
    target_link_libraries(vf2_model_decode_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_model_decode_tests)
    add_test(NAME vf2_model_decode COMMAND vf2_model_decode_tests)

    add_executable(vf2_texture_sheet_tests tests/platform/test_texture_sheet.c)
    target_link_libraries(vf2_texture_sheet_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_texture_sheet_tests)
    add_test(NAME vf2_texture_sheet COMMAND vf2_texture_sheet_tests)

    add_executable(vf2_texture_link_tests tests/platform/test_texture_link.c)
    target_link_libraries(vf2_texture_link_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_texture_link_tests)
    add_test(NAME vf2_texture_link COMMAND vf2_texture_link_tests)

    add_executable(vf2_geo_display_tests tests/platform/test_geo_display.c)
    target_link_libraries(vf2_geo_display_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_geo_display_tests)
    add_test(NAME vf2_geo_display COMMAND vf2_geo_display_tests)

    add_executable(vf2_tile_tests tests/platform/test_tile.c)
    target_link_libraries(vf2_tile_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_tile_tests)
    add_test(NAME vf2_tile COMMAND vf2_tile_tests)
    if(EXISTS "${VF2_ROM_DIR}")
        add_test(NAME vf2_tile_font COMMAND vf2_tile_tests "${VF2_ROM_DIR}")
        # Cold snapshot for the end-to-end session tile-layer test (TEST MENU),
        # generated on demand from the ROM (never committed).
        set(VF2_TILE_COLD_SNAP "${CMAKE_BINARY_DIR}/generated/cold_tiles.vf2snap")
        add_custom_command(
            OUTPUT "${VF2_TILE_COLD_SNAP}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/generated"
            COMMAND "$<TARGET_FILE:vf2i960>" native-sixth-dispatch
                    "${VF2_ROM_DIR}" "${VF2_TILE_COLD_SNAP}"
            DEPENDS vf2i960
            COMMENT "tile: generating cold snapshot for the session tile-layer test"
            VERBATIM)
        add_custom_target(vf2_tile_cold_snapshot DEPENDS "${VF2_TILE_COLD_SNAP}")
        add_dependencies(vf2_tile_tests vf2_tile_cold_snapshot)
        add_test(NAME vf2_tile_session
                 COMMAND vf2_tile_tests "${VF2_ROM_DIR}" "${VF2_TILE_COLD_SNAP}")
    endif()

    add_executable(vf2_host_tests tests/host/test_host.c)
    target_link_libraries(vf2_host_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_host_tests)
    add_test(NAME vf2_host COMMAND vf2_host_tests)

    # Independent frame-exact original i960 sound-ring producer replay.
    # --selftest is ROM-free; the production CLI requires the owner's
    # pre-event snapshot and never creates synthetic game commands.
    add_executable(vf2_audio_writer_replay tests/host/audio_writer_replay.c)
    target_link_libraries(vf2_audio_writer_replay PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_audio_writer_replay)
    add_test(NAME vf2_audio_writer_replay_rom_free
             COMMAND vf2_audio_writer_replay --selftest)

    # Owner-ROM differential: original vector-15 sound ISR versus
    # the established shadow sound drainer, from the SAME authentic
    # pre-event checkpoint and controller input. ROM-free selftest
    # is mandatory in CI; the owner invokes the real case manually.
    add_executable(vf2_audio_isr_replay tests/host/test_audio_isr_replay.c)
    target_link_libraries(vf2_audio_isr_replay PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_audio_isr_replay)
    add_test(NAME vf2_audio_isr_replay_rom_free
             COMMAND vf2_audio_isr_replay --selftest)

    # Optional PRIVATE owner-ROM evidence: do not check snapshots or
    # ROMs into Git. The filename passed here must be the genuine
    # pre-frame-342 checkpoint captured from the original i960.
    set(VF2_AUDIO_ISR_FRAME342_SNAPSHOT "" CACHE FILEPATH
        "Private original VF2.1 i960 pre-frame-342 snapshot for vector-15 differential")
    if(VF2_AUDIO_ISR_FRAME342_SNAPSHOT AND
       EXISTS "${VF2_AUDIO_ISR_FRAME342_SNAPSHOT}" AND
       VF2_ROM_DIR AND EXISTS "${VF2_ROM_DIR}")
        add_test(NAME vf2_audio_isr_frame342_owner_rom
                 COMMAND vf2_audio_isr_replay
                    "${VF2_ROM_DIR}"
                    "${VF2_AUDIO_ISR_FRAME342_SNAPSHOT}"
                    0x20 0x43924 0x00504024 0x009e0a7f)
        set_tests_properties(vf2_audio_isr_frame342_owner_rom
                             PROPERTIES TIMEOUT 900)
    endif()

    # Oracle-frame UART hunt driver (v0825 8d): steps real frames through
    # the interpreter oracle with a passive bus observer on the Model 2A
    # serial port (0x009c0000). Needs the gitignored sixth-fresh snapshot;
    # skips (exit 77) where it is absent, per the p1-demo precedent.
    add_executable(vf2_session_uart_tests tests/host/test_session_uart.c)
    target_link_libraries(vf2_session_uart_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_session_uart_tests)
    if(EXISTS "${VF2_ROM_DIR}")
        add_test(NAME vf2_session_uart_probe
            COMMAND vf2_session_uart_tests
                "${VF2_ROM_DIR}"
                "${CMAKE_SOURCE_DIR}/out/sixth-fresh.vf2snap")
        set_tests_properties(vf2_session_uart_probe PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 900)
        # Session->host audio plumbing (v0825 8e): attach + render
        # determinism + ring round-trip + WAV artifact. Same snapshot
        # rule as the UART probe.
        add_executable(vf2_session_audio_tests tests/host/test_session_audio.c)
        target_link_libraries(vf2_session_audio_tests PRIVATE vf2_core)
        vf2_set_project_warnings(vf2_session_audio_tests)
        add_test(NAME vf2_session_audio_plumbing
            COMMAND vf2_session_audio_tests
                "${VF2_ROM_DIR}"
                "${CMAKE_SOURCE_DIR}/out/sixth-fresh.vf2snap")
        set_tests_properties(vf2_session_audio_plumbing PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 900)
        # T3 pump leg: same binary with the live-queue snapshot as
        # argv[3] (park-sixth-a6c0 holds 3 filed entries). Skips
        # where the snapshot is absent, same rule as above.
        add_test(NAME vf2_session_audio_pump
            COMMAND vf2_session_audio_tests
                "${VF2_ROM_DIR}"
                "${CMAKE_SOURCE_DIR}/out/sixth-fresh.vf2snap"
                "${CMAKE_SOURCE_DIR}/out/park-sixth-a6c0.vf2snap")
        set_tests_properties(vf2_session_audio_pump PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 900)
        # E1r pump cursor accounting: argv[4] = a probe-staged queue
        # (OK b0-class entry + refused 90 entry) on top of the park
        # snapshot; skips where the staged snapshot is absent (it is
        # a local out/ artifact, same rule as the other legs).
        add_test(NAME vf2_session_audio_pump_staged
            COMMAND vf2_session_audio_tests
                "${VF2_ROM_DIR}"
                "${CMAKE_SOURCE_DIR}/out/sixth-fresh.vf2snap"
                "${CMAKE_SOURCE_DIR}/out/park-sixth-a6c0.vf2snap"
                "${CMAKE_SOURCE_DIR}/out/park-staged-2entries.vf2snap")
        set_tests_properties(vf2_session_audio_pump_staged PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 900)
        # T4 queue hunt: oracle-frame filer/flusher arrivals from
        # gameplay (sixth-fresh). Hits reported, not gated; the
        # 8d snapshot rule applies.
        add_executable(vf2_session_queue_hunt_tests tests/host/test_session_queue_hunt.c)
        target_link_libraries(vf2_session_queue_hunt_tests PRIVATE vf2_core)
        vf2_set_project_warnings(vf2_session_queue_hunt_tests)
        add_test(NAME vf2_session_queue_hunt
            COMMAND vf2_session_queue_hunt_tests
                "${VF2_ROM_DIR}"
                "${CMAKE_SOURCE_DIR}/out/sixth-fresh.vf2snap")
        set_tests_properties(vf2_session_queue_hunt PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 900)
        # Sound-drain interrupt (vector 15) live service (v0853): the
        # real ROM ISR drains the live park queue to the UART slot
        # (0x01c80000, the address the drainer's own lda stages); the
        # pump's shadow-CPU C drain is the differential control.
        # Needs the gitignored park + sixth-fresh snapshots; skips
        # (exit 77) where absent, same rule as the legs above.
        add_executable(vf2_session_sound_isr_tests tests/host/test_session_sound_isr.c)
        target_link_libraries(vf2_session_sound_isr_tests PRIVATE vf2_core)
        vf2_set_project_warnings(vf2_session_sound_isr_tests)
        add_test(NAME vf2_session_sound_isr
            COMMAND vf2_session_sound_isr_tests
                "${VF2_ROM_DIR}"
                "${CMAKE_SOURCE_DIR}/out/park-sixth-a6c0.vf2snap"
                "${CMAKE_SOURCE_DIR}/out/sixth-fresh.vf2snap")
        set_tests_properties(vf2_session_sound_isr PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 900)
    endif()

    add_executable(vf2_game_tests tests/game/test_game.c)
    target_link_libraries(vf2_game_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_game_tests)
    add_test(NAME vf2_game COMMAND vf2_game_tests)

    add_executable(vf2_m68k_tests tests/m68k/test_decoder.c)
    target_link_libraries(vf2_m68k_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_tests)
    add_test(NAME vf2_m68k COMMAND vf2_m68k_tests)

    add_executable(vf2_m68k_step_tests tests/m68k/test_stepper.c)
    target_link_libraries(vf2_m68k_step_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_step_tests)
    add_test(NAME vf2_m68k_step COMMAND vf2_m68k_step_tests)

    add_executable(vf2_m68k_pcrel_base_tests tests/m68k/test_pcrel_base.c)
    target_link_libraries(vf2_m68k_pcrel_base_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_pcrel_base_tests)
    add_test(NAME vf2_m68k_pcrel_base COMMAND vf2_m68k_pcrel_base_tests)

    add_executable(vf2_m68k_triangle_tests tests/m68k/test_triangle.c)
    target_link_libraries(vf2_m68k_triangle_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_triangle_tests)

    add_executable(vf2_m68k_loopb_probe_tests tests/m68k/test_loopb_probe.c)
    target_link_libraries(vf2_m68k_loopb_probe_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_loopb_probe_tests)

    add_executable(vf2_m68k_midi_tests tests/m68k/test_midi.c)
    target_link_libraries(vf2_m68k_midi_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_midi_tests)

    add_executable(vf2_m68k_dispatch_tests tests/m68k/test_dispatch.c)
    target_link_libraries(vf2_m68k_dispatch_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_dispatch_tests)

    add_executable(vf2_m68k_cdispatch_tests tests/m68k/test_cdispatch.c)
    target_link_libraries(vf2_m68k_cdispatch_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_cdispatch_tests)

    # The original-68000 Akira post-14 observer itself is testable
    # WITHOUT original ROMs. Owner-only continuation is a separate
    # explicit command and never executes in public CI.
    add_test(NAME vf2_m68k_akira_post14_observer_rom_free
             COMMAND vf2_m68k_cdispatch_tests
                     --selftest-akira-continuation)

    # ROM-backed L3 comparison for real 0xAD23xx/0xAD24xx
    # selections, independent from the i960 gameplay trigger.
    add_executable(vf2_m68k_l3_selection_tests tests/m68k/test_l3_real_rom_selections.c)
    target_link_libraries(vf2_m68k_l3_selection_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_l3_selection_tests)

    # Isolated diagnostic of five authentic frame-342 L3 commands:
    # native C dispatch + original Motorola 68000 checkpoints.
    # Requires the owner's original audio ROM and samples to execute.
    add_executable(vf2_m68k_frame342_dispatch_tests
                   tests/m68k/test_frame342_dispatch.c)
    target_link_libraries(vf2_m68k_frame342_dispatch_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_frame342_dispatch_tests)

    # ROM-free exact AD21 00 branch regression: valid header first
    # vacant descriptor versus neighboring selector and bad header.
    add_executable(vf2_m68k_ad21_frame342_rom_free
                   tests/m68k/test_ad21_frame342.c)
    target_link_libraries(vf2_m68k_ad21_frame342_rom_free PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_ad21_frame342_rom_free)
    add_test(NAME vf2_m68k_ad21_frame342_rom_free
             COMMAND vf2_m68k_ad21_frame342_rom_free)

    # ROM-free next-frame AD21 B/CF stream; original owner ROM
    # runs the stronger 68000-vs-C 12-packet differential probe.
    add_executable(vf2_m68k_ad21_l2_rom_free
                   tests/m68k/test_ad21_l2_frame342.c)
    target_link_libraries(vf2_m68k_ad21_l2_rom_free PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_ad21_l2_rom_free)
    add_test(NAME vf2_m68k_ad21_l2_rom_free
             COMMAND vf2_m68k_ad21_l2_rom_free)

    # Original Motorola 68000 0xCB2..0xD72: 16 MOVEM triplets,
    # 48 native instructions / 512 bytes per sample page.
    add_executable(vf2_m68k_engine_copy_page_tests
                   tests/m68k/test_engine_copy_page.c)
    target_link_libraries(vf2_m68k_engine_copy_page_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_engine_copy_page_tests)
    add_test(NAME vf2_m68k_engine_copy_page_rom_free
             COMMAND vf2_m68k_engine_copy_page_tests)

    # ROM-free, directly executable native validation of the
    # original 0x1f04 descriptor-clear/write free-slot contract.
    add_executable(vf2_m68k_a0_descriptor_tests tests/m68k/test_a0_descriptor_native.c)
    vf2_set_project_warnings(vf2_m68k_a0_descriptor_tests)
    add_test(NAME vf2_m68k_a0_descriptor_native COMMAND vf2_m68k_a0_descriptor_tests)

    add_executable(vf2_m68k_l2_ad_tests tests/m68k/test_l2_ad_native.c)
    vf2_set_project_warnings(vf2_m68k_l2_ad_tests)
    add_test(NAME vf2_m68k_l2_ad_native COMMAND vf2_m68k_l2_ad_tests)

    add_executable(vf2_m68k_l2_ad_integration_tests
                   tests/m68k/test_l2_ad_integration.c)
    target_link_libraries(vf2_m68k_l2_ad_integration_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_l2_ad_integration_tests)
    add_test(NAME vf2_m68k_l2_ad_integration
             COMMAND vf2_m68k_l2_ad_integration_tests)

    # Verify the second packet produced by the AD23/AD24 worker:
    # original 90 HIGH data lookup, Loop-B field writes and hold.
    add_executable(vf2_m68k_90_ad_followup_tests
                   tests/m68k/test_90_ad_followup.c)
    target_link_libraries(vf2_m68k_90_ad_followup_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_90_ad_followup_tests)
    add_test(NAME vf2_m68k_90_ad_followup
             COMMAND vf2_m68k_90_ad_followup_tests)

    # Independent original 68000 CPU execution to the 0xfac
    # free-voice transition and first instruction of 0xa8c.
    add_executable(vf2_m68k_90_to_voice_tests
                   tests/m68k/test_90_to_voice.c)
    target_link_libraries(vf2_m68k_90_to_voice_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_90_to_voice_tests)

    add_executable(vf2_m68k_snapshot_tests tests/m68k/test_snapshot.c)
    target_link_libraries(vf2_m68k_snapshot_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_m68k_snapshot_tests)

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

    add_executable(vf2_player_cf04_tests tests/recovered/test_player_cf04_native.c)
    target_link_libraries(vf2_player_cf04_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_cf04_tests)
    add_test(NAME vf2_player_cf04 COMMAND vf2_player_cf04_tests)

    # 0x43800 sound-queue UART drainer (v0825 8f): synthetic unit
    # legs always run; the ROM-backed C-vs-stepper differential
    # needs a ROM dir and is wired only where one exists.
    add_executable(vf2_sound_43800_tests tests/recovered/test_sound_43800_native.c)
    target_link_libraries(vf2_sound_43800_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_sound_43800_tests)
    if(EXISTS "${VF2_ROM_DIR}")
        add_test(NAME vf2_sound_43800_drainer
            COMMAND vf2_sound_43800_tests "${VF2_ROM_DIR}")
    else()
        add_test(NAME vf2_sound_43800_drainer COMMAND vf2_sound_43800_tests)
    endif()

    add_executable(vf2_player_1fee4_tests tests/recovered/test_player_1fee4_native.c)
    target_link_libraries(vf2_player_1fee4_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1fee4_tests)
    add_test(NAME vf2_player_1fee4 COMMAND vf2_player_1fee4_tests)

    add_executable(vf2_player_1ff0c_tests tests/recovered/test_player_1ff0c_native.c)
    target_link_libraries(vf2_player_1ff0c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1ff0c_tests)
    add_test(NAME vf2_player_1ff0c COMMAND vf2_player_1ff0c_tests)

    add_executable(vf2_player_1fffc_tests tests/recovered/test_player_1fffc_native.c)
    target_link_libraries(vf2_player_1fffc_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1fffc_tests)
    add_test(NAME vf2_player_1fffc COMMAND vf2_player_1fffc_tests)

    add_executable(vf2_player_4b410_tests tests/recovered/test_player_4b410_native.c)
    target_link_libraries(vf2_player_4b410_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_4b410_tests)
    add_test(NAME vf2_player_4b410 COMMAND vf2_player_4b410_tests)

    add_executable(vf2_player_11704_tests tests/recovered/test_player_11704_native.c)
    target_link_libraries(vf2_player_11704_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_11704_tests)
    add_test(NAME vf2_player_11704 COMMAND vf2_player_11704_tests)

    add_executable(vf2_player_2eab8_tests tests/recovered/test_player_2eab8_native.c)
    target_link_libraries(vf2_player_2eab8_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_2eab8_tests)
    add_test(NAME vf2_player_2eab8 COMMAND vf2_player_2eab8_tests)

    add_executable(vf2_player_2c38_tests tests/recovered/test_player_2c38_native.c)
    target_link_libraries(vf2_player_2c38_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_2c38_tests)
    add_test(NAME vf2_player_2c38 COMMAND vf2_player_2c38_tests)

    add_executable(vf2_player_1fcc0_tests tests/recovered/test_player_1fcc0_native.c)
    target_link_libraries(vf2_player_1fcc0_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_1fcc0_tests)
    add_test(NAME vf2_player_1fcc0 COMMAND vf2_player_1fcc0_tests)

    add_executable(vf2_subo_saturation_2d40_tests tests/recovered/test_subo_saturation_2d40_native.c)
    target_link_libraries(vf2_subo_saturation_2d40_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_subo_saturation_2d40_tests)
    add_test(NAME vf2_subo_saturation_2d40 COMMAND vf2_subo_saturation_2d40_tests)

    add_executable(vf2_player_323fc_tests tests/recovered/test_player_323fc_native.c)
    target_link_libraries(vf2_player_323fc_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_323fc_tests)
    add_test(NAME vf2_player_323fc COMMAND vf2_player_323fc_tests)

    add_executable(vf2_player_32284_tests tests/recovered/test_player_32284_native.c)
    target_link_libraries(vf2_player_32284_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_32284_tests)
    add_test(NAME vf2_player_32284 COMMAND vf2_player_32284_tests)

    add_executable(vf2_player_4421c_tests tests/recovered/test_player_4421c_native.c)
    target_link_libraries(vf2_player_4421c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_4421c_tests)
    add_test(NAME vf2_player_4421c COMMAND vf2_player_4421c_tests)

    add_executable(vf2_player_7ef0_tests tests/recovered/test_player_7ef0_native.c)
    target_link_libraries(vf2_player_7ef0_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_7ef0_tests)
    add_test(NAME vf2_player_7ef0 COMMAND vf2_player_7ef0_tests)

    add_executable(vf2_player_a154_tests tests/recovered/test_player_a154_native.c)
    target_link_libraries(vf2_player_a154_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_a154_tests)
    add_test(NAME vf2_player_a154 COMMAND vf2_player_a154_tests)

    add_executable(vf2_player_4ad40_tests tests/recovered/test_player_4ad40_native.c)
    target_link_libraries(vf2_player_4ad40_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_player_4ad40_tests)
    add_test(NAME vf2_player_4ad40 COMMAND vf2_player_4ad40_tests)

    add_executable(vf2_model2a_register_switch_6dccc_tests tests/recovered/test_model2a_register_switch_6dccc_native.c)
    target_link_libraries(vf2_model2a_register_switch_6dccc_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_model2a_register_switch_6dccc_tests)
    add_test(NAME vf2_model2a_register_switch_6dccc COMMAND vf2_model2a_register_switch_6dccc_tests)

    add_executable(vf2_kill_osage_evaluate_record_tests tests/recovered/test_kill_osage_evaluate_record_native.c)
    target_link_libraries(vf2_kill_osage_evaluate_record_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_kill_osage_evaluate_record_tests)
    add_test(NAME vf2_kill_osage_evaluate_record COMMAND vf2_kill_osage_evaluate_record_tests)

    add_executable(vf2_task_object_cluster_tests tests/recovered/test_task_object_cluster_native.c)
    target_link_libraries(vf2_task_object_cluster_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_task_object_cluster_tests)
    add_test(NAME vf2_task_object_cluster COMMAND vf2_task_object_cluster_tests)

    add_executable(vf2_slot_increment_helper_6d10c_tests tests/recovered/test_slot_increment_helper_6d10c_native.c)
    target_link_libraries(vf2_slot_increment_helper_6d10c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_slot_increment_helper_6d10c_tests)
    add_test(NAME vf2_slot_increment_helper_6d10c COMMAND vf2_slot_increment_helper_6d10c_tests)

    add_executable(vf2_sibling_slot_increment_helper_6d1b4_tests tests/recovered/test_sibling_slot_increment_helper_6d1b4_native.c)
    target_link_libraries(vf2_sibling_slot_increment_helper_6d1b4_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_sibling_slot_increment_helper_6d1b4_tests)
    add_test(NAME vf2_sibling_slot_increment_helper_6d1b4 COMMAND vf2_sibling_slot_increment_helper_6d1b4_tests)

    add_executable(vf2_crc16_step_hash_9480_tests tests/recovered/test_crc16_step_hash_9480_native.c)
    target_link_libraries(vf2_crc16_step_hash_9480_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_crc16_step_hash_9480_tests)
    add_test(NAME vf2_crc16_step_hash_9480 COMMAND vf2_crc16_step_hash_9480_tests)

    add_executable(vf2_block_hash_apply_5ff7c_tests tests/recovered/test_block_hash_apply_5ff7c_native.c)
    target_link_libraries(vf2_block_hash_apply_5ff7c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_block_hash_apply_5ff7c_tests)
    add_test(NAME vf2_block_hash_apply_5ff7c COMMAND vf2_block_hash_apply_5ff7c_tests)

    add_executable(vf2_frame_end_tick_6d0a0_tests tests/recovered/test_frame_end_tick_6d0a0_native.c)
    target_link_libraries(vf2_frame_end_tick_6d0a0_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_frame_end_tick_6d0a0_tests)
    add_test(NAME vf2_frame_end_tick_6d0a0 COMMAND vf2_frame_end_tick_6d0a0_tests)

    add_executable(vf2_poly_record_clear_60330_tests tests/recovered/test_poly_record_clear_60330_native.c)
    target_link_libraries(vf2_poly_record_clear_60330_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_poly_record_clear_60330_tests)
    add_test(NAME vf2_poly_record_clear_60330 COMMAND vf2_poly_record_clear_60330_tests)

    add_executable(vf2_timer_fuse_94d0_tests tests/recovered/test_timer_fuse_94d0_native.c)
    target_link_libraries(vf2_timer_fuse_94d0_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_timer_fuse_94d0_tests)
    add_test(NAME vf2_timer_fuse_94d0 COMMAND vf2_timer_fuse_94d0_tests)

    add_executable(vf2_poly_record_load_5427c_tests tests/recovered/test_poly_record_load_5427c_native.c)
    target_link_libraries(vf2_poly_record_load_5427c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_poly_record_load_5427c_tests)
    add_test(NAME vf2_poly_record_load_5427c COMMAND vf2_poly_record_load_5427c_tests)

    add_executable(vf2_slot3_table_lookup_1b780_tests tests/recovered/test_slot3_table_lookup_1b780_native.c)
    target_link_libraries(vf2_slot3_table_lookup_1b780_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_slot3_table_lookup_1b780_tests)
    add_test(NAME vf2_slot3_table_lookup_1b780 COMMAND vf2_slot3_table_lookup_1b780_tests)

    add_executable(vf2_color_table_snapshot_5a460_tests tests/recovered/test_color_table_snapshot_5a460_native.c)
    target_link_libraries(vf2_color_table_snapshot_5a460_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_color_table_snapshot_5a460_tests)
    add_test(NAME vf2_color_table_snapshot_5a460 COMMAND vf2_color_table_snapshot_5a460_tests)

    add_executable(vf2_frame_end_tick_33f0_6cf00_tests tests/recovered/test_frame_end_tick_33f0_6cf00_native.c)
    target_link_libraries(vf2_frame_end_tick_33f0_6cf00_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_frame_end_tick_33f0_6cf00_tests)
    add_test(NAME vf2_frame_end_tick_33f0_6cf00 COMMAND vf2_frame_end_tick_33f0_6cf00_tests)

    add_executable(vf2_display_profile_load_53750_tests tests/recovered/test_display_profile_load_53750_native.c)
    target_link_libraries(vf2_display_profile_load_53750_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_display_profile_load_53750_tests)
    add_test(NAME vf2_display_profile_load_53750 COMMAND vf2_display_profile_load_53750_tests)

    add_executable(vf2_assignment_init_6010c_tests tests/recovered/test_assignment_init_6010c_native.c)
    target_link_libraries(vf2_assignment_init_6010c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_assignment_init_6010c_tests)
    add_test(NAME vf2_assignment_init_6010c COMMAND vf2_assignment_init_6010c_tests)

    add_executable(vf2_record_rebuild_dispatcher_6de3c_tests tests/recovered/test_record_rebuild_dispatcher_6de3c_native.c)
    target_link_libraries(vf2_record_rebuild_dispatcher_6de3c_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_record_rebuild_dispatcher_6de3c_tests)
    add_test(NAME vf2_record_rebuild_dispatcher_6de3c COMMAND vf2_record_rebuild_dispatcher_6de3c_tests)

    add_executable(vf2_rob_kage_disp_outside_61b88_tests tests/recovered/test_rob_kage_disp_outside_61b88_native.c)
    target_link_libraries(vf2_rob_kage_disp_outside_61b88_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_rob_kage_disp_outside_61b88_tests)
    add_test(NAME vf2_rob_kage_disp_outside_61b88 COMMAND vf2_rob_kage_disp_outside_61b88_tests)

    add_executable(vf2_reject_sample_53884_tests tests/recovered/test_reject_sample_53884_live.c)
    target_link_libraries(vf2_reject_sample_53884_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_reject_sample_53884_tests)
    add_test(NAME vf2_reject_sample_53884 COMMAND vf2_reject_sample_53884_tests)

    add_executable(vf2_set_mmode_7b18_tests tests/recovered/test_set_mmode_7b18_live.c)
    target_link_libraries(vf2_set_mmode_7b18_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_set_mmode_7b18_tests)
    add_test(NAME vf2_set_mmode_7b18 COMMAND vf2_set_mmode_7b18_tests)

    add_executable(vf2_set_obj_7c60_tests tests/recovered/test_set_obj_7c60_live.c)
    target_link_libraries(vf2_set_obj_7c60_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_set_obj_7c60_tests)
    add_test(NAME vf2_set_obj_7c60 COMMAND vf2_set_obj_7c60_tests)

    add_executable(vf2_callee_hook_counters_tests tests/recovered/test_callee_hook_counters.c)
    target_link_libraries(vf2_callee_hook_counters_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_callee_hook_counters_tests)
    add_test(NAME vf2_callee_hook_counters COMMAND vf2_callee_hook_counters_tests)

    add_executable(vf2_callee_hook_fires_native_tests tests/recovered/test_callee_hook_fires_native.c)
    target_link_libraries(vf2_callee_hook_fires_native_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_callee_hook_fires_native_tests)
    add_test(NAME vf2_callee_hook_fires_native COMMAND vf2_callee_hook_fires_native_tests)

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
        add_test(NAME vf2_m68k_triangle_differential COMMAND vf2_m68k_triangle_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_loopb_probe COMMAND vf2_m68k_loopb_probe_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_midi_probe COMMAND vf2_m68k_midi_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_dispatch_probe COMMAND vf2_m68k_dispatch_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_cdispatch_probe COMMAND vf2_m68k_cdispatch_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_l3_real_selections
                 COMMAND vf2_m68k_l3_selection_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_frame342_dispatch_owner_rom
                 COMMAND vf2_m68k_frame342_dispatch_tests "${VF2_ROM_DIR}")
        # Original owner-ROM AD21 stream, one 471-instruction M68000
        # L2 tick versus native C, 12 B/CF packets, full RAM/SCSP
        # register equality. The ROM stays local; public CI uses
        # the separate strict ROM-free shape/rollback regression.
        add_test(NAME vf2_m68k_frame342_ad21_l2_owner_rom
                 COMMAND vf2_m68k_frame342_dispatch_tests
                         "${VF2_ROM_DIR}" --probe-ad21-l2)
        set_tests_properties(vf2_m68k_frame342_ad21_l2_owner_rom
                             PROPERTIES TIMEOUT 900)
        # ROM-backed strict AD21 BA gates. B0 indexes the table by
        # ring byte 1: BA 2A -> $194a; BA 07 -> $16ba, not uploader
        # $1ac0 (selected only by ring byte 1 = $3a).
        # Both measured owner-ROM dispatches preserve staging/PCM
        # and match C memory deltas and SCSP registers exactly.
        # Opt-in because public CI intentionally contains no game ROMs.
        if(VF2_VERIFY_AD21_BA)
            add_test(NAME vf2_m68k_frame342_ad21_ba_strict_owner_rom
                     COMMAND vf2_m68k_frame342_dispatch_tests
                             "${VF2_ROM_DIR}" --verify-ad21-ba)
            set_tests_properties(vf2_m68k_frame342_ad21_ba_strict_owner_rom
                                 PROPERTIES TIMEOUT 900)
        endif()
        # Strict post-AD21 B9..BE 12-packet MC68000 vs native-C
        # delta parity. Verified with the private VF2.1 owner ROM;
        # never run in public CI without the original ROM set.
        if(VF2_VERIFY_AD21_B_ALL)
            add_test(NAME vf2_m68k_frame342_ad21_b_all_strict_owner_rom
                     COMMAND vf2_m68k_frame342_dispatch_tests
                             "${VF2_ROM_DIR}" --verify-ad21-b-all)
            set_tests_properties(vf2_m68k_frame342_ad21_b_all_strict_owner_rom
                                 PROPERTIES TIMEOUT 900)
        endif()
        # Full AD21 stateful sequential drain. Never restore sound
        # RAM or SCSP between packets; the private owner-ROM 68000
        # restarts at its dispatcher entry once per queue request.
        if(VF2_VERIFY_AD21_B_SEQUENTIAL)
            add_test(NAME vf2_m68k_frame342_ad21_b_sequential_owner_rom
                     COMMAND vf2_m68k_frame342_dispatch_tests
                             "${VF2_ROM_DIR}" --verify-ad21-b-sequential)
            set_tests_properties(vf2_m68k_frame342_ad21_b_sequential_owner_rom
                                 PROPERTIES TIMEOUT 900)
        endif()
        # Experimental full frame342 UART 9E -> three Cx -> AD21
        # -> original L2 -> 12 B driver dispatch callbacks. Includes
        # modeled PCM matching AFTER all B packets. Owner-ROM only.
        if(VF2_VERIFY_FRAME342_CHAIN)
            add_test(NAME vf2_m68k_frame342_stateful_chain_owner_rom
                     COMMAND vf2_m68k_frame342_dispatch_tests
                             "${VF2_ROM_DIR}" --verify-frame342-chain)
            set_tests_properties(vf2_m68k_frame342_stateful_chain_owner_rom
                                 PROPERTIES TIMEOUT 900)
        endif()
        # Akira SOUND TEST #11 AD 10 04: original 68000-only
        # low-header record and stream worker, 32 drained note packets,
        # 8 keyed SCSP voices and nonzero PCM model. This is not
        # recovered native-C sequencer parity, nor full music.
        if(VF2_VERIFY_AKIRA_MUSIC)
            add_test(NAME vf2_m68k_akira_music_owner_rom
                     COMMAND vf2_m68k_l3_selection_tests
                             "${VF2_ROM_DIR}" --verify-akira-music)
            set_tests_properties(vf2_m68k_akira_music_owner_rom
                                 PROPERTIES TIMEOUT 900)
        endif()
        # A one-cycle owner-ROM differential through the actual
        # 0x2242 main-loop stream walker. Verifies 32 descriptor
        # visits, genuine MOVEM page copies, and pointer updates;
        # no synthetic PCM prefill or copyrighted ROM assets in CI.
        if(VF2_VERIFY_AKIRA_WALK)
            add_test(NAME vf2_m68k_akira_original_walker_owner_rom
                     COMMAND vf2_m68k_l3_selection_tests
                             "${VF2_ROM_DIR}" --verify-akira-walk)
            set_tests_properties(vf2_m68k_akira_original_walker_owner_rom
                                 PROPERTIES TIMEOUT 900)
        endif()
        # v0898: exact fifteen original i960 vector-15 bytes from
        # owner-ROM headless frame 342 (input 0x20). Cx file after
        # TWO bytes and ignore F8; no invented 3-byte boundaries.
        add_test(NAME vf2_m68k_original_frame342_uart_stream
                 COMMAND vf2_m68k_l3_selection_tests
                         "${VF2_ROM_DIR}" --uart-stream
                         9e0a7fcd74f8ca7ef8cb7ff8ad2100)
        add_test(NAME vf2_m68k_original_cx_short_command
                 COMMAND vf2_m68k_l3_selection_tests
                         "${VF2_ROM_DIR}" --l3-packet cd74f8)
        add_test(NAME vf2_m68k_engine_copy_page_differential
                 COMMAND vf2_m68k_engine_copy_page_tests "${VF2_ROM_DIR}")
        # v0884: ROM-owned 18x11 sound selections -> 98 isolated L3
        # C-vs-original-68000 packet differentials. Never run in ROM-free CI.
        add_test(NAME vf2_m68k_l3_sound_matrix_differential
                 COMMAND vf2_m68k_l3_selection_tests
                         "${VF2_ROM_DIR}" --matrix-l3)
        add_test(NAME vf2_m68k_90_to_voice
                 COMMAND vf2_m68k_90_to_voice_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_m68k_snapshot_differential COMMAND vf2_m68k_snapshot_tests "${VF2_ROM_DIR}")
        # (v0823: the Loop-B WILL_FAIL is removed -- the probe is
        # re-pinned under the +2 base and green. The +4-era comment
        # that lived here is retired to audio_v3a_loopb_v0823.md.)
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
        add_test(NAME vf2_reject_sample_53884_differential COMMAND vf2_reject_sample_53884_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_set_mmode_7b18_differential COMMAND vf2_set_mmode_7b18_tests "${VF2_ROM_DIR}")
        add_test(NAME vf2_set_obj_7c60_differential COMMAND vf2_set_obj_7c60_tests "${VF2_ROM_DIR}")
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
        # Parallel-safety (v0825): native-nth-dispatch 11/12 share the
        # fixed CWD temp pair .vf2i960-native-nth-base6.vf2snap[.runtime]
        # (tools/vf2i960/main.c) -- concurrent runs tear each other's
        # checkpoint (observed: twelfth "I/O error" under -j12, green
        # serially). Everything else in the suite is parallel-safe
        # (stable-input reads, tempdirs, distinct files), so only these
        # two serialize.
        set_tests_properties(
            vf2_native_eleventh_dispatch
            vf2_native_twelfth_dispatch
            PROPERTIES RUN_SERIAL TRUE
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
            NAME vf2_python_audio_sound_test_catalog
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_audio_sound_test_catalog.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_audio_natural_sound_gate
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_audio_natural_sound_gate.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_audio_frame_evidence
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_audio_frame_evidence_report.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_audio_l3_packet_replay
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_audio_l3_packet_replay.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_test(
            NAME vf2_python_audio_rom_matrix_l3
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tools/python/test_audio_rom_matrix_l3.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
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
        # Skips (exit 77) when the gitignored ROM-backed trace corpus is absent.
        set_tests_properties(vf2_python_factory_p1_real_trace_demo
                             PROPERTIES SKIP_RETURN_CODE 77)
    else()
        message(STATUS
            "Python3 not found; skipping vf2_python_factory_* ctest wiring")
    endif()

    # Per-function + session-scope differential for the lifted bridge
    # (opt-in; docs/VF2LIFT.md).
    if(VF2_ENABLE_LIFTED AND TARGET vf2_lifted)
        add_executable(vf2_lift_differential_tests tests/lift/test_lifted_differential.c)
        target_link_libraries(vf2_lift_differential_tests PRIVATE vf2_lifted vf2_core)
        vf2_set_project_warnings(vf2_lift_differential_tests)
        add_test(NAME vf2_lift_differential_unit COMMAND vf2_lift_differential_tests)
        if(EXISTS "${VF2_ROM_DIR}")
            # The cold snapshot is built on demand; make the test depend on it
            # so it exists before the (3-arg) session check runs.
            add_dependencies(vf2_lift_differential_tests vf2_lift_cold_snapshot)
            add_test(NAME vf2_lift_differential
                     COMMAND vf2_lift_differential_tests
                             "${VF2_ROM_DIR}" "${VF2_LIFT_COLD_SNAP}")
        endif()
    endif()
endif()
