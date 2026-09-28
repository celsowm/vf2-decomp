#ifndef VF2_GAME_H
#define VF2_GAME_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/native_runtime.h"
#include "vf2/platform.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"
#include "vf2/tgp.h"

typedef struct vf2_game {
    unsigned frame_number;
    int initialized;
    vf2_platform *platform;
    vf2_tgp *tgp;
    vf2_sound_board *sound;
    /* Native runtime objects are borrowed, never freed by vf2_game. */
    vf2_model2a *native_machine;
    vf2_i960_cpu *native_cpu;
    vf2_native_runtime_state *native_runtime;
    vf2_native_runtime_run_report native_report;
    uint32_t *native_copro_words;
    size_t native_copro_word_count;
    size_t native_copro_word_capacity;
    int native_copro_capture_enabled;
    uint32_t native_geometry_start;
    uint32_t native_geometry_end;
    size_t native_geometry_word_count;
    vf2_tgp_geometry_stream_report native_geometry_report;
    uint32_t native_geometry_preview[16];
    size_t native_geometry_preview_count;
    size_t native_geometry_class_counts[32];
    uint32_t input;
    int input_set;
} vf2_game;

vf2_status vf2_game_initialize(vf2_game *game);

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
);

vf2_status vf2_game_set_input(vf2_game *game, uint32_t input);
vf2_status vf2_game_attach_native_runtime(
    vf2_game *game,
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_native_runtime_state *runtime
);

/* Validation-only frame runner.  This boundary may execute the recovered
 * i960 runtime; vf2_game_update never calls it. */
vf2_status vf2_game_run_native_frame(
    vf2_game *game,
    size_t max_blocks,
    vf2_native_runtime_run_report *report
);
vf2_status vf2_game_attach_audio(
    vf2_game *game,
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    const uint8_t *sample_rom,
    size_t sample_rom_size
);
vf2_status vf2_game_begin_frame(vf2_game *game, uint32_t color);
vf2_status vf2_game_submit_geometry(
    vf2_game *game,
    const uint32_t *words,
    size_t word_count,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
);
vf2_status vf2_game_end_frame(vf2_game *game);
vf2_status vf2_game_read_pixels(
    const vf2_game *game,
    uint32_t *pixels,
    size_t pixel_count
);
vf2_status vf2_game_render_audio(
    vf2_game *game,
    int16_t *left,
    int16_t *right,
    size_t frames
);

vf2_status vf2_game_update(vf2_game *game);
void vf2_game_shutdown(vf2_game *game);

#endif
