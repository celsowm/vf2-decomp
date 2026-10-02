#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/boot.h"
#include "vf2/game.h"
#include "vf2/i960/snapshot.h"
#include "vf2/recovered.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s --rom-dir <directory> "
        "[--native-snapshot <snapshot>] [--frames <count>] "
        "[--input <mask>] [--pulse-input] [--trace-geometry]\n"
        " [--trace-state] [--failure-snapshot <file>]\n"
        " [--snapshot-at <frame> --snapshot-path <file>]\n",
        program
    );
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0u;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }
    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static void trace_frame_state(struct vf2_game *game, uint32_t frame)
{
    uint32_t dispatch_base = 0u;
    uint32_t game_mode_word = 0u;
    uint32_t game_flags_word = 0u;
    uint32_t input_flags_word = 0u;
    uint32_t task0_pointer = 0u;
    uint32_t task1_pointer = 0u;
    uint32_t f0_head = 0u;
    uint32_t f1_head = 0u;
    uint32_t ret_addr = 0u;
    uint8_t dispatch_mode = 0u;
    uint8_t dispatch_phase = 0u;

    if (game == NULL || game->native_machine == NULL) {
        return;
    }
    (void)vf2_model2a_read_u32(
        game->native_machine, UINT32_C(0x0050016c), &dispatch_base
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, UINT32_C(0x00500804), &task0_pointer
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, UINT32_C(0x00500808), &task1_pointer
    );
    (void)vf2_model2a_read(
        game->native_machine, dispatch_base + UINT32_C(0x3350),
        &dispatch_mode, sizeof(dispatch_mode)
    );
    (void)vf2_model2a_read(
        game->native_machine, dispatch_base + UINT32_C(0x3351),
        &dispatch_phase, sizeof(dispatch_phase)
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, dispatch_base + UINT32_C(0x3324),
        &game_mode_word
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, dispatch_base + UINT32_C(0x3320),
        &game_flags_word
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, UINT32_C(0x00500704), &input_flags_word
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, UINT32_C(0x00510980), &f0_head
    );
    (void)vf2_model2a_read_u32(
        game->native_machine, UINT32_C(0x00512980), &f1_head
    );
    if (game->native_cpu != NULL) {
        ret_addr = game->native_cpu->registers[2];
    }
    printf(
        "State frame %u: game_mode=%u mode=%u phase=0x%02x flags=0x%08x "
        "input=0x%08x task0=0x%08x task1=0x%08x f0h=0x%08x f1h=0x%08x "
        "base16c=0x%08x depth=%u r2=0x%08x r27=0x%08x r28=0x%08x ip=0x%08x\n",
        (unsigned)frame, (unsigned)game_mode_word,
        (unsigned)dispatch_mode, (unsigned)dispatch_phase,
        (unsigned)game_flags_word, (unsigned)input_flags_word,
        (unsigned)task0_pointer, (unsigned)task1_pointer,
        (unsigned)f0_head, (unsigned)f1_head,
        (unsigned)dispatch_base,
        (unsigned)(game->native_cpu != NULL
                       ? game->native_cpu->local_frame_depth : 0u),
        (unsigned)ret_addr,
        (unsigned)(game->native_cpu != NULL
                       ? game->native_cpu->registers[27] : 0u),
        (unsigned)(game->native_cpu != NULL
                       ? game->native_cpu->registers[28] : 0u),
        (unsigned)(game->native_cpu != NULL ? game->native_cpu->ip : 0u)
    );
}

static vf2_status run_native_session(
    const char *rom_directory,
    const char *snapshot_path,
    uint32_t frame_count,
    uint32_t input,
    int input_set,
    int trace_geometry,
    int pulse_input,
    int trace_state,
    const char *failure_snapshot_path,
    uint32_t snapshot_at_frame,
    const char *snapshot_at_path
)
{
    static const uint32_t width = 496u;
    static const uint32_t height = 384u;
    uint8_t *maincpu = NULL;
    uint8_t *main_data = NULL;
    uint8_t *tables = NULL;
    uint8_t *copro_data = NULL;
    uint8_t *polygons = NULL;
    uint8_t *audio_rom = NULL;
    uint8_t *sample_rom = NULL;
    size_t maincpu_size = 0u;
    size_t main_data_size = 0u;
    size_t tables_size = 0u;
    size_t copro_data_size = 0u;
    size_t polygons_size = 0u;
    size_t audio_size = 0u;
    size_t sample_size = 0u;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_native_runtime_state runtime;
    vf2_i960_snapshot snapshot;
    vf2_game game = {0};
    vf2_native_runtime_run_report report;
    uint32_t *pixels = NULL;
    int16_t *left = NULL;
    int16_t *right = NULL;
    size_t pixel_count = (size_t)width * (size_t)height;
    size_t audio_frames = 735u;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(&runtime, 0, sizeof(runtime));
    memset(&report, 0, sizeof(report));
    vf2_i960_snapshot_init(&snapshot);

    status = vf2_romset_build_region(
        rom_directory, VF2_REGION_MAINCPU, &maincpu, &maincpu_size
    );
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_COPRO_TGP_TABLES, &tables, &tables_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_COPRO_DATA, &copro_data, &copro_data_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_POLYGONS, &polygons, &polygons_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_AUDIOCPU, &audio_rom, &audio_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            rom_directory, VF2_REGION_SAMPLES, &sample_rom, &sample_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_initialize(&machine) != 0
            ? VF2_OK : VF2_ERROR_OUT_OF_MEMORY;
    }
    if (status == VF2_OK) {
        status = vf2_model2a_attach_main_rom(
            &machine, maincpu, maincpu_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_take_main_data(
            &machine, main_data, main_data_size
        );
        if (status == VF2_OK) {
            main_data = NULL;
        }
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_initialize(&runtime, 4u);
    }
    if (status == VF2_OK) {
        status = vf2_game_initialize(&game);
    }
    if (status == VF2_OK) {
        status = vf2_game_attach_graphics(
            &game, width, height, tables, tables_size, copro_data,
            copro_data_size, polygons, polygons_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_game_attach_audio(
            &game, audio_rom, audio_size, sample_rom, sample_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_game_attach_native_runtime(
            &game, &machine, &cpu, &runtime
        );
    }
    if (status == VF2_OK && input_set != 0) {
        status = vf2_game_set_input(&game, input);
    }
    if (status == VF2_OK) {
        pixels = (uint32_t *)calloc(pixel_count, sizeof(*pixels));
        left = (int16_t *)calloc(audio_frames, sizeof(*left));
        right = (int16_t *)calloc(audio_frames, sizeof(*right));
        if (pixels == NULL || left == NULL || right == NULL) {
            status = VF2_ERROR_OUT_OF_MEMORY;
        }
    }
    for (uint32_t frame = 0u; status == VF2_OK && frame < frame_count; ++frame) {
        if (input_set != 0 && pulse_input != 0) {
            const uint32_t held = input &
                ~(uint32_t)(VF2_PLATFORM_BUTTON_COIN | VF2_PLATFORM_BUTTON_START);
            const uint32_t edge = frame == 0u
                ? input & VF2_PLATFORM_BUTTON_COIN
                : frame == 1u ? input & VF2_PLATFORM_BUTTON_START : 0u;
            status = vf2_game_set_input(&game, held | edge);
            if (status != VF2_OK) {
                break;
            }
        }
        status = vf2_game_run_native_frame(&game, 100000u, &report);
        if (status == VF2_OK) {
            printf(
                "Native frame %u: blocks=%zu instructions=%llu ip=0x%08x\n",
                (unsigned)(frame + 1u), report.blocks_executed,
                (unsigned long long)report.recovered_instruction_count,
                (unsigned)report.final_address
            );
            if (trace_state != 0) {
                trace_frame_state(&game, frame + 1u);
            }
            if (snapshot_at_path != NULL && frame + 1u == snapshot_at_frame &&
                game.native_cpu != NULL && game.native_machine != NULL) {
                vf2_i960_snapshot checkpoint;
                vf2_i960_snapshot_init(&checkpoint);
                if (vf2_i960_snapshot_capture(
                        &checkpoint, game.native_cpu,
                        game.native_machine) == VF2_OK &&
                    vf2_i960_snapshot_write_file(
                        &checkpoint, snapshot_at_path) == VF2_OK) {
                    fprintf(
                        stderr, "Checkpoint snapshot: %s (frame %u)\n",
                        snapshot_at_path, (unsigned)(frame + 1u)
                    );
                }
                vf2_i960_snapshot_destroy(&checkpoint);
            }
        } else {
                fprintf(
                    stderr,
                    "Native frame failed: %s blocks=%zu ip=0x%08x captured=%zu "
                    "step=%d bridge=%d\n",
                    vf2_status_string(status), report.blocks_executed,
                    (unsigned)report.final_address, game.native_copro_word_count,
                    (int)report.last_step_kind, (int)report.last_bridge_kind
                );
            if (failure_snapshot_path != NULL && game.native_cpu != NULL &&
                game.native_machine != NULL) {
                vf2_i960_snapshot failed;
                vf2_i960_snapshot_init(&failed);
                if (vf2_i960_snapshot_capture(
                        &failed, game.native_cpu,
                        game.native_machine) == VF2_OK &&
                    vf2_i960_snapshot_write_file(
                        &failed, failure_snapshot_path) == VF2_OK) {
                    fprintf(
                        stderr, "Failure snapshot: %s\n",
                        failure_snapshot_path
                    );
                }
                vf2_i960_snapshot_destroy(&failed);
            }
            if (game.native_cpu != NULL) {
                uint32_t runtime_flags = 0u;
                uint32_t dispatch_selector = 0u;
                uint32_t dispatch_counter = 0u;
                uint32_t dispatch_base = 0u;
                uint32_t task0_pointer = 0u;
                uint32_t task1_pointer = 0u;
                uint32_t shadow_mismatch = UINT32_MAX;
                uint32_t game_mode_word = 0u;
                uint32_t game_flags_word = 0u;
                uint32_t input_flags_word = 0u;
                uint8_t dispatch_mode = 0u;
                uint8_t dispatch_phase = 0u;
                if (game.native_machine != NULL) {
                    (void)vf2_model2a_read_u32(
                        game.native_machine, UINT32_C(0x00508000), &runtime_flags
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, UINT32_C(0x00500024), &dispatch_counter
                    );
                    (void)vf2_model2a_read(
                        game.native_machine, UINT32_C(0x0050002a),
                        &dispatch_selector, sizeof(uint8_t)
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, UINT32_C(0x0050016c),
                        &dispatch_base
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, UINT32_C(0x00500804), &task0_pointer
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, UINT32_C(0x00500808), &task1_pointer
                    );
                    (void)vf2_model2a_read(
                        game.native_machine, dispatch_base + UINT32_C(0x3350),
                        &dispatch_mode, sizeof(dispatch_mode)
                    );
                    (void)vf2_model2a_read(
                        game.native_machine, dispatch_base + UINT32_C(0x3351),
                        &dispatch_phase, sizeof(dispatch_phase)
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, dispatch_base + UINT32_C(0x3324),
                        &game_mode_word
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, dispatch_base + UINT32_C(0x3320),
                        &game_flags_word
                    );
                    (void)vf2_model2a_read_u32(
                        game.native_machine, UINT32_C(0x00500704),
                        &input_flags_word
                    );
                    {
                        static const uint32_t shadow_addresses[] = {
                            UINT32_C(0x00500234), UINT32_C(0x00500235),
                            UINT32_C(0x00500236), UINT32_C(0x00500237),
                            UINT32_C(0x00500238), UINT32_C(0x00500239),
                            UINT32_C(0x005000e0), UINT32_C(0x005000e1),
                            UINT32_C(0x005000e2)
                        };
                        size_t shadow_index = 0u;
                        for (; shadow_index < sizeof(shadow_addresses) /
                             sizeof(shadow_addresses[0]); ++shadow_index) {
                            uint8_t live_byte = 0u;
                            uint8_t shadow_byte = 0u;
                            (void)vf2_model2a_read(
                                game.native_machine, shadow_addresses[shadow_index],
                                &live_byte, sizeof(live_byte)
                            );
                            (void)vf2_model2a_read(
                                game.native_machine, UINT32_C(0x00544600) +
                                    (uint32_t)shadow_index,
                                &shadow_byte, sizeof(shadow_byte)
                            );
                            if (live_byte != shadow_byte) {
                                shadow_mismatch = (uint32_t)shadow_index;
                                break;
                            }
                        }
                    }
                }
                fprintf(
                    stderr,
                    "  cpu: depth=%u g0=0x%08x g1=0x%08x g2=0x%08x "
                    "g3=0x%08x r1=0x%08x r14=0x%08x r15=0x%08x cmp=%d\n",
                    (unsigned)game.native_cpu->local_frame_depth,
                    (unsigned)game.native_cpu->registers[VF2_I960_G0_REGISTER],
                    (unsigned)game.native_cpu->registers[VF2_I960_G0_REGISTER + 1u],
                    (unsigned)game.native_cpu->registers[VF2_I960_G0_REGISTER + 2u],
                    (unsigned)game.native_cpu->registers[VF2_I960_G0_REGISTER + 3u],
                    (unsigned)game.native_cpu->registers[1],
                    (unsigned)game.native_cpu->registers[14],
                    (unsigned)game.native_cpu->registers[15],
                    (int)game.native_cpu->compare_result
                );
                fprintf(
                    stderr,
                    "  dispatch: flags=0x%08x selector=%u counter=%u "
                    "base=0x%08x mode=%u phase=0x%02x task0=0x%08x "
                    "task1=0x%08x shadow_mismatch=%u game_mode=%u "
                    "game_flags=0x%08x input_flags=0x%08x\n",
                    (unsigned)runtime_flags, (unsigned)dispatch_selector,
                    (unsigned)dispatch_counter, (unsigned)dispatch_base,
                    (unsigned)dispatch_mode, (unsigned)dispatch_phase,
                    (unsigned)task0_pointer, (unsigned)task1_pointer,
                    (unsigned)shadow_mismatch, (unsigned)game_mode_word,
                    (unsigned)game_flags_word, (unsigned)input_flags_word
                );
            }
        }
    }
    if (status == VF2_OK) {
        printf(
            "Native geometry: start=0x%08x end=0x%08x words=%zu commands=%zu "
            "ended=%d triangles=%zu\n",
            (unsigned)game.native_geometry_start,
            (unsigned)game.native_geometry_end,
            game.native_geometry_word_count,
            game.native_geometry_report.commands,
            game.native_geometry_report.ended,
            game.native_geometry_report.rendered_triangles
        );
        if (trace_geometry != 0) {
            printf("Native geometry preview:");
            for (size_t index = 0u;
                 index < game.native_geometry_preview_count; ++index) {
                printf(" 0x%08x", (unsigned)game.native_geometry_preview[index]);
            }
            putchar('\n');
            printf("Native geometry classes:");
            for (size_t index = 0u; index < 32u; ++index) {
                if (game.native_geometry_class_counts[index] != 0u) {
                    printf(" %zu=%zu", index,
                           game.native_geometry_class_counts[index]);
                }
            }
            putchar('\n');
        }
        status = vf2_game_read_pixels(&game, pixels, pixel_count);
    }
    if (status == VF2_OK) {
        status = vf2_game_render_audio(&game, left, right, audio_frames);
    }
    if (status == VF2_OK) {
        uint32_t checksum = 0u;
        for (size_t index = 0u; index < pixel_count; ++index) {
            checksum = (checksum * UINT32_C(16777619)) ^ pixels[index];
        }
        printf("Framebuffer checksum: 0x%08x\n", (unsigned)checksum);
        printf("Audio frames rendered: %zu\n", audio_frames);
    }

    free(pixels);
    free(left);
    free(right);
    vf2_game_shutdown(&game);
    vf2_i960_snapshot_destroy(&snapshot);
    vf2_model2a_shutdown(&machine);
    free(maincpu);
    free(main_data);
    free(tables);
    free(copro_data);
    free(polygons);
    free(audio_rom);
    free(sample_rom);
    return status;
}

int main(int argc, char **argv)
{
    const char *rom_directory = NULL;
    const char *native_snapshot = NULL;
    const char *failure_snapshot = NULL;
    const char *snapshot_at_path = NULL;
    uint32_t snapshot_at_frame = 0u;
    uint32_t native_frames = 1u;
    uint32_t native_input = 0u;
    int native_input_set = 0;
    int trace_geometry = 0;
    int pulse_input = 0;
    int trace_state = 0;
    vf2_verify_summary summary;
    uint8_t *maincpu = NULL;
    size_t maincpu_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_recovered_boot_state recovered;
    vf2_status status = VF2_OK;

    if (argc < 3 || strcmp(argv[1], "--rom-dir") != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    rom_directory = argv[2];
    for (int index = 3; index < argc; ++index) {
        if (strcmp(argv[index], "--native-snapshot") == 0 &&
            index + 1 < argc) {
            native_snapshot = argv[++index];
        } else if (strcmp(argv[index], "--frames") == 0 &&
                   index + 1 < argc &&
                   parse_u32(argv[++index], &native_frames) &&
                   native_frames != 0u) {
            /* parsed */
        } else if (strcmp(argv[index], "--input") == 0 &&
                   index + 1 < argc &&
                   parse_u32(argv[++index], &native_input)) {
            native_input_set = 1;
        } else if (strcmp(argv[index], "--trace-geometry") == 0) {
            trace_geometry = 1;
        } else if (strcmp(argv[index], "--trace-state") == 0) {
            trace_state = 1;
        } else if (strcmp(argv[index], "--failure-snapshot") == 0 &&
                   index + 1 < argc) {
            failure_snapshot = argv[++index];
        } else if (strcmp(argv[index], "--snapshot-at") == 0 &&
                   index + 1 < argc &&
                   parse_u32(argv[++index], &snapshot_at_frame)) {
            /* parsed */
        } else if (strcmp(argv[index], "--snapshot-path") == 0 &&
                   index + 1 < argc) {
            snapshot_at_path = argv[++index];
        } else if (strcmp(argv[index], "--pulse-input") == 0) {
            pulse_input = 1;
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("vf2-decomp v%s\n", VF2_VERSION_STRING);
    printf(
        "This release validates a repeated-frame recovered native runtime "
        "corridor; it is not yet a playable port.\n\n"
    );

    status = vf2_romset_verify(rom_directory, NULL, &summary);
    if (status != VF2_OK) {
        fprintf(
            stderr,
            "ROM verification failed: %s "
            "(valid=%zu missing=%zu size=%zu crc=%zu sha1=%zu)\n",
            vf2_status_string(status),
            summary.valid,
            summary.missing,
            summary.bad_size,
            summary.bad_crc32,
            summary.bad_sha1
        );
        return EXIT_FAILURE;
    }

    status = vf2_romset_build_region(
        rom_directory,
        VF2_REGION_MAINCPU,
        &maincpu,
        &maincpu_size
    );

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Could not build maincpu region: %s\n",
            vf2_status_string(status)
        );
        return EXIT_FAILURE;
    }

    status = vf2_parse_i960_boot_vectors(
        maincpu,
        maincpu_size,
        &vectors
    );

    if (status != VF2_OK) {
        fprintf(
            stderr,
            "Could not parse i960 boot vectors: %s\n",
            vf2_status_string(status)
        );
        free(maincpu);
        return EXIT_FAILURE;
    }

    vf2_recovered_boot_entry(
        &recovered,
        vectors.system_address_table,
        vectors.initial_prcb,
        vectors.start_ip
    );

    printf("ROM set: Virtua Fighter 2 Version 2.1 (Model 2A)\n");
    printf("Files:   %zu/%zu valid\n", summary.valid, summary.total);
    printf("SAT:     0x%08x\n", (unsigned)recovered.system_address_table);
    printf("PRCB:    0x%08x\n", (unsigned)recovered.initial_prcb);
    printf("Entry:   0x%08x\n", (unsigned)recovered.initial_entry);
    printf("\nCurrent milestone: sixth-dispatch native runtime plus integrated graphics/audio boundaries.\n");
    printf(
        "Next milestone: recover remaining sound handlers and evidence-backed "
        "fighter/gameplay state.\n"
    );

    if (native_snapshot != NULL) {
        status = run_native_session(
            rom_directory, native_snapshot, native_frames,
            native_input, native_input_set, trace_geometry, pulse_input,
            trace_state, failure_snapshot, snapshot_at_frame,
            snapshot_at_path
        );
        if (status != VF2_OK) {
            fprintf(stderr, "Native session failed: %s\n",
                    vf2_status_string(status));
            free(maincpu);
            return EXIT_FAILURE;
        }
    }

    free(maincpu);
    return EXIT_SUCCESS;
}
