#include "vf2/game.h"

#include <stdlib.h>
#include <string.h>

#include "vf2/geometry.h"

static vf2_status game_require_graphics(const vf2_game *game)
{
    if (game == NULL || game->initialized == 0 ||
        game->platform == NULL || game->tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status game_require_audio(const vf2_game *game)
{
    if (game == NULL || game->initialized == 0 || game->sound == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status game_require_native(const vf2_game *game)
{
    if (game == NULL || game->initialized == 0 ||
        game->native_machine == NULL || game->native_cpu == NULL ||
        game->native_runtime == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status game_capture_copro_write(
    void *context,
    uint32_t address,
    const void *source,
    size_t size
)
{
    vf2_game *game = (vf2_game *)context;
    uint32_t value = 0u;
    uint32_t packet = 0u;
    size_t capacity = 0u;

    if (game == NULL || source == NULL || size != sizeof(uint32_t) ||
        game->native_copro_capture_enabled == 0) {
        return VF2_ERROR_UNSUPPORTED;
    }
    value = (uint32_t)((const uint8_t *)source)[0] |
            ((uint32_t)((const uint8_t *)source)[1] << 8u) |
            ((uint32_t)((const uint8_t *)source)[2] << 16u) |
            ((uint32_t)((const uint8_t *)source)[3] << 24u);
    if (address >= VF2_COPRO_PORT_BASE &&
        address < VF2_COPRO_PORT_BASE + 0x4000u) {
        const uint32_t byte_offset = address - VF2_COPRO_PORT_BASE;
        packet = (value & UINT32_C(0x800fffff)) |
                 (((byte_offset >> 2u) & UINT32_C(0xff)) << 23u);
    } else if (address >= VF2_COPRO_PORT_BASE + 0x4000u &&
               address < VF2_COPRO_PORT_BASE + VF2_COPRO_PORT_SIZE) {
        packet = value;
    } else {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (game->native_copro_word_count == game->native_copro_word_capacity) {
        if (game->native_copro_word_capacity == 0u) {
            capacity = 256u;
        } else {
            if (game->native_copro_word_capacity > SIZE_MAX / 2u) {
                return VF2_ERROR_OUT_OF_MEMORY;
            }
            capacity = game->native_copro_word_capacity * 2u;
        }
        if (capacity > SIZE_MAX / sizeof(*game->native_copro_words)) {
            return VF2_ERROR_OUT_OF_MEMORY;
        }
        {
            uint32_t *words = (uint32_t *)realloc(
                game->native_copro_words,
                capacity * sizeof(*game->native_copro_words)
            );
            if (words == NULL) {
                return VF2_ERROR_OUT_OF_MEMORY;
            }
            game->native_copro_words = words;
            game->native_copro_word_capacity = capacity;
        }
    }
    game->native_copro_words[game->native_copro_word_count++] = packet;
    /* Keep the Model 2A backing store updated as well. The recovered runtime
     * still observes a few coprocessor-port values while the full TGP FIFO
     * device is being completed; returning UNSUPPORTED asks model2a.c to
     * retain that compatibility behavior after capture. */
    return VF2_ERROR_UNSUPPORTED;
}

static vf2_status game_render_native_geometry_ring(vf2_game *game)
{
    enum { geometry_buffer_mask = 0x0001fffc };
    uint32_t start = 0u;
    uint32_t end = 0u;
    uint32_t distance = 0u;
    uint32_t offset = 0u;
    uint32_t *words = NULL;
    size_t word_count = 0u;
    size_t index = 0u;
    vf2_tgp_geometry_stream_report scan;
    vf2_status status = VF2_OK;

    if (game == NULL || game->native_machine == NULL ||
        game->platform == NULL || game->tgp == NULL) {
        return VF2_OK;
    }
    status = vf2_model2a_read_u32(
        game->native_machine,
        VF2_GEOMETRY_BASE + VF2_GEOMETRY_PREVIOUS_OFFSET,
        &start
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            game->native_machine,
            VF2_GEOMETRY_BASE + VF2_GEOMETRY_WRITE_OFFSET,
            &end
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    game->native_geometry_start = start;
    game->native_geometry_end = end;
    game->native_geometry_word_count = 0u;
    memset(&game->native_geometry_report, 0, sizeof(game->native_geometry_report));
    game->native_geometry_preview_count = 0u;
    memset(game->native_geometry_preview, 0, sizeof(game->native_geometry_preview));
    memset(game->native_geometry_class_counts, 0,
           sizeof(game->native_geometry_class_counts));
    distance = ((end & (uint32_t)geometry_buffer_mask) -
                (start & (uint32_t)geometry_buffer_mask)) &
               (uint32_t)geometry_buffer_mask;
    if (distance == 0u || (distance & (sizeof(uint32_t) - 1u)) != 0u) {
        game->native_geometry_word_count = 0u;
        return VF2_OK;
    }
    word_count = (size_t)(distance / sizeof(uint32_t));
    game->native_geometry_word_count = word_count;
    game->native_geometry_preview_count = 0u;
    memset(game->native_geometry_preview, 0, sizeof(game->native_geometry_preview));
    memset(game->native_geometry_class_counts, 0,
           sizeof(game->native_geometry_class_counts));
    words = (uint32_t *)calloc(word_count, sizeof(*words));
    if (words == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    offset = start & (uint32_t)geometry_buffer_mask;
    for (index = 0u; index < word_count; ++index) {
        status = vf2_model2a_read_u32(
            game->native_machine,
            VF2_BUFFER_RAM_BASE + offset,
            &words[index]
        );
        if (status != VF2_OK) {
            free(words);
            return status;
        }
        ++game->native_geometry_class_counts[
            (words[index] >> 23u) & UINT32_C(0x1f)
        ];
        if (game->native_geometry_preview_count <
            sizeof(game->native_geometry_preview) /
                sizeof(game->native_geometry_preview[0])) {
            game->native_geometry_preview[
                game->native_geometry_preview_count++
            ] = words[index];
        }
        offset = (offset + sizeof(uint32_t)) &
                 (uint32_t)geometry_buffer_mask;
    }
    status = vf2_tgp_scan_geometry_stream(words, word_count, &scan);
    game->native_geometry_report = scan;
    if (status == VF2_OK && scan.ended != 0) {
        status = vf2_tgp_execute_geometry_stream(
            game->tgp, words, word_count, game->platform,
            UINT32_C(0xffffffff), &scan
        );
    }
    if (status == VF2_ERROR_UNSUPPORTED ||
        status == VF2_ERROR_OUT_OF_BOUNDS) {
        /* The native ring is still broader than the recovered packet decoder.
         * Leave an unrecognized ring untouched from the game's perspective so
         * a future decoder can consume it without making frame execution fail. */
        status = VF2_OK;
    }
    free(words);
    return status;
}

static vf2_status game_render_native_geometry(vf2_game *game)
{
    vf2_tgp_geometry_stream_report report;
    vf2_status status = VF2_OK;

    if (game == NULL || game->platform == NULL || game->tgp == NULL) {
        return VF2_OK;
    }
    if (game->native_copro_word_count != 0u) {
        status = vf2_tgp_execute_geometry_stream(
            game->tgp, game->native_copro_words,
            game->native_copro_word_count, game->platform,
            UINT32_C(0xffffffff), &report
        );
        if (status != VF2_OK) {
            return status;
        }
        game->native_copro_word_count = 0u;
    }
    return game_render_native_geometry_ring(game);
}

vf2_status vf2_game_initialize(vf2_game *game)
{
    if (game == 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    game->frame_number = 0u;
    game->initialized = 1;
    game->platform = NULL;
    game->tgp = NULL;
    game->sound = NULL;
    game->native_machine = NULL;
    game->native_cpu = NULL;
    game->native_runtime = NULL;
    memset(&game->native_report, 0, sizeof(game->native_report));
    game->native_copro_words = NULL;
    game->native_copro_word_count = 0u;
    game->native_copro_word_capacity = 0u;
    game->native_copro_capture_enabled = 0;
    game->native_geometry_start = 0u;
    game->native_geometry_end = 0u;
    game->native_geometry_word_count = 0u;
    memset(&game->native_geometry_report, 0, sizeof(game->native_geometry_report));
    memset(game->native_geometry_preview, 0, sizeof(game->native_geometry_preview));
    game->native_geometry_preview_count = 0u;
    memset(game->native_geometry_class_counts, 0,
           sizeof(game->native_geometry_class_counts));
    game->input = 0u;
    game->input_set = 0;
    return VF2_OK;
}

vf2_status vf2_game_attach_audio(
    vf2_game *game,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
)
{
    vf2_sound_board *sound;

    if (game == NULL || game->initialized == 0 || game->sound != NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    sound = (vf2_sound_board *)calloc(1u, sizeof(*sound));
    if (sound == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    vf2_sound_board_initialize(
        sound, audio_rom, audio_rom_size, sample_rom, sample_rom_size
    );
    game->sound = sound;
    return VF2_OK;
}

vf2_status vf2_game_attach_graphics(
    vf2_game *game,
    uint32_t width,
    uint32_t height,
    const uint8_t *tables,
    size_t tables_size,
    const uint8_t *copro_data,
    size_t copro_data_size,
    const uint8_t *polygon_rom,
    size_t polygon_rom_size
)
{
    vf2_platform *platform = NULL;
    vf2_tgp *tgp = NULL;
    vf2_status status = VF2_OK;

    if (game == NULL || game->initialized == 0 || width == 0u ||
        height == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (game->platform != NULL || game->tgp != NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    platform = (vf2_platform *)calloc(1u, sizeof(*platform));
    tgp = (vf2_tgp *)calloc(1u, sizeof(*tgp));
    if (platform == NULL || tgp == NULL) {
        free(platform);
        free(tgp);
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    status = vf2_platform_initialize(platform, width, height);
    if (status != VF2_OK) {
        free(platform);
        free(tgp);
        return status;
    }
    status = vf2_tgp_initialize(
        tgp, tables, tables_size, copro_data, copro_data_size
    );
    if (status != VF2_OK) {
        vf2_platform_shutdown(platform);
        free(platform);
        free(tgp);
        return status;
    }
    if (polygon_rom != NULL || polygon_rom_size != 0u) {
        status = vf2_tgp_attach_polygon_rom(
            tgp, polygon_rom, polygon_rom_size
        );
        if (status != VF2_OK) {
            vf2_platform_shutdown(platform);
            free(platform);
            free(tgp);
            return status;
        }
    }
    game->platform = platform;
    game->tgp = tgp;
    if (game->native_machine != NULL) {
        status = vf2_model2a_set_copro_callbacks(
            game->native_machine, NULL, game_capture_copro_write, game
        );
        if (status != VF2_OK) {
            game->platform = NULL;
            game->tgp = NULL;
            vf2_platform_shutdown(platform);
            free(tgp);
            free(platform);
            return status;
        }
    }
    return VF2_OK;
}

vf2_status vf2_game_set_input(vf2_game *game, uint32_t input)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    status = vf2_platform_set_input(game->platform, input);
    if (status == VF2_OK) {
        game->input = input;
        game->input_set = 1;
        if (game->native_machine != NULL) {
            status = vf2_model2a_set_input(game->native_machine, input);
        }
    }
    return status;
}

vf2_status vf2_game_attach_native_runtime(
    vf2_game *game,
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *runtime
)
{
    if (game == NULL || game->initialized == 0 || machine == NULL ||
        cpu == NULL || runtime == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    game->native_machine = machine;
    game->native_cpu = cpu;
    game->native_runtime = runtime;
    memset(&game->native_report, 0, sizeof(game->native_report));
    if (game->platform != NULL) {
        vf2_status status = vf2_model2a_set_copro_callbacks(
            machine, NULL, game_capture_copro_write, game
        );
        if (status == VF2_OK && game->input_set != 0) {
            status = vf2_model2a_set_input(machine, game->input);
        }
        return status;
    }
    return VF2_OK;
}

vf2_status vf2_game_run_native_frame(
    vf2_game *game,
    size_t max_blocks,
    vf2_native_runtime_run_report *report
)
{
    vf2_status status = game_require_native(game);
    int frame_open = 0;

    if (status != VF2_OK || max_blocks == 0u) {
        return status != VF2_OK ? status : VF2_ERROR_INVALID_ARGUMENT;
    }
    if (game->input_set != 0) {
        status = vf2_model2a_set_input(game->native_machine, game->input);
        if (status != VF2_OK) {
            return status;
        }
    }
    if (game->platform != NULL || game->tgp != NULL) {
        if (game->platform == NULL || game->tgp == NULL) {
            return VF2_ERROR_INVALID_ARGUMENT;
        }
        status = vf2_platform_begin_frame(game->platform, 0u);
        if (status != VF2_OK) {
            return status;
        }
        frame_open = 1;
        game->native_copro_word_count = 0u;
        game->native_copro_capture_enabled = 1;
    }
    status = vf2_native_runtime_run_frame(
        game->native_machine,
        game->native_cpu,
        game->native_runtime,
        max_blocks,
        &game->native_report
    );
    if (report != NULL) {
        *report = game->native_report;
    }
    game->native_copro_capture_enabled = 0;
    if (status == VF2_OK && frame_open) {
        status = game_render_native_geometry(game);
    }
    if (frame_open) {
        vf2_status end_status = vf2_platform_end_frame(game->platform);
        if (status == VF2_OK) {
            status = end_status;
        }
    }
    if (status == VF2_OK) {
        ++game->frame_number;
    }
    return status;
}

vf2_status vf2_game_begin_frame(vf2_game *game, uint32_t color)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    return vf2_platform_begin_frame(game->platform, color);
}

vf2_status vf2_game_submit_geometry(
    vf2_game *game,
    const uint32_t *words,
    size_t word_count,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK || words == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_tgp_execute_geometry_stream(
        game->tgp, words, word_count, game->platform, color, report
    );
}

vf2_status vf2_game_end_frame(vf2_game *game)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    status = vf2_platform_end_frame(game->platform);
    if (status == VF2_OK) {
        ++game->frame_number;
    }
    return status;
}

vf2_status vf2_game_read_pixels(
    const vf2_game *game,
    uint32_t *pixels,
    size_t pixel_count
)
{
    vf2_status status = game_require_graphics(game);

    if (status != VF2_OK) {
        return status;
    }
    return vf2_platform_read_pixels(game->platform, pixels, pixel_count);
}

vf2_status vf2_game_render_audio(
    vf2_game *game,
    int16_t *left,
    int16_t *right,
    size_t frames
)
{
    vf2_status status = game_require_audio(game);

    if (status != VF2_OK || left == NULL || right == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return vf2_sound_board_render(game->sound, left, right, frames);
}

vf2_status vf2_game_update(vf2_game *game)
{
    vf2_sound_voice_maintenance_report maintenance;
    vf2_sound_stream_maintenance_report stream_maintenance;

    if (game == 0 || game->initialized == 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    /* Preflight the execution boundary before touching any mutable subsystem.
     * An attached oracle without a portable gameplay pipeline is unsupported
     * as a whole frame, so audio maintenance must not create a partial update
     * before that result is reported. */
    if (game->native_machine != NULL || game->native_cpu != NULL ||
        game->native_runtime != NULL) {
        return VF2_ERROR_UNSUPPORTED;
    }

    if (game->sound != NULL) {
        vf2_status status = vf2_sound_board_maintain_streams(
            game->sound, &stream_maintenance
        );
        if (status != VF2_OK) {
            return status;
        }
        status = vf2_sound_board_maintain_voices(
            game->sound, &maintenance
        );
        if (status != VF2_OK) {
            return status;
        }
    }

    /* The interpreted/recovered i960 runtime is an oracle boundary, not the
     * normal game loop.  Keep this guard explicit until the portable fighter,
     * match and submission pipeline is attached to vf2_game.  In particular,
     * do not silently turn vf2_game_update into a call to
     * vf2_game_run_native_frame: doing so would make an apparently native
     * frame execute guest instructions and would hide missing gameplay
     * recovery behind the validation path. */
    ++game->frame_number;
    return VF2_OK;
}

void vf2_game_shutdown(vf2_game *game)
{
    if (game != 0) {
        if (game->native_machine != NULL) {
            (void)vf2_model2a_set_copro_callbacks(
                game->native_machine, NULL, NULL, NULL
            );
        }
        if (game->platform != NULL) {
            vf2_platform_shutdown(game->platform);
            free(game->platform);
        }
        free(game->tgp);
        free(game->sound);
        free(game->native_copro_words);
        game->platform = NULL;
        game->tgp = NULL;
        game->sound = NULL;
        game->native_machine = NULL;
        game->native_cpu = NULL;
        game->native_runtime = NULL;
        memset(&game->native_report, 0, sizeof(game->native_report));
        game->native_copro_words = NULL;
        game->native_copro_word_count = 0u;
        game->native_copro_word_capacity = 0u;
        game->native_copro_capture_enabled = 0;
        game->native_geometry_start = 0u;
        game->native_geometry_end = 0u;
        game->native_geometry_word_count = 0u;
        memset(&game->native_geometry_report, 0, sizeof(game->native_geometry_report));
        game->native_geometry_preview_count = 0u;
        memset(game->native_geometry_preview, 0, sizeof(game->native_geometry_preview));
        memset(game->native_geometry_class_counts, 0,
               sizeof(game->native_geometry_class_counts));
        game->input = 0u;
        game->input_set = 0;
        game->initialized = 0;
    }
}
