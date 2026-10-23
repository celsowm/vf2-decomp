#ifndef VF2_TGP_H
#define VF2_TGP_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/model2a.h"
#include "vf2/platform.h"
#include "vf2/status.h"

enum {
    VF2_TGP_PROGRAM_WORD_COUNT = 0x1000,
    VF2_TGP_FIFO_WORD_COUNT = 8,
    VF2_TGP_TABLE_WORD_COUNT = 0x10000,
    VF2_TGP_POLYGON_RAM_WORD_COUNT = 0x8000,
    VF2_TGP_BUFFER_WORD_MASK = 0x7fff
};

/*
 * Portable reference geometry.  Matrices use column-major storage, matching
 * the affine form used by the Model 2 camera data.  This is a software
 * renderer boundary, not a claim about the unrecovered TGP packet format.
 */
typedef struct vf2_tgp_matrix {
    float values[16];
} vf2_tgp_matrix;

typedef struct vf2_tgp_vertex {
    float x;
    float y;
    float z;
    float w;
} vf2_tgp_vertex;

typedef struct vf2_tgp_screen_vertex {
    int32_t x;
    int32_t y;
    float depth;
} vf2_tgp_screen_vertex;

typedef struct vf2_tgp {
    const uint8_t *tables;
    size_t tables_size;
    const uint8_t *copro_data;
    size_t copro_data_size;
    const uint8_t *polygon_rom;
    size_t polygon_rom_size;
    uint32_t program[VF2_TGP_PROGRAM_WORD_COUNT];
    uint32_t bank_register;
    uint32_t sincos_base;
    uint32_t atan_base[4];
    uint32_t inverse_base;
    uint32_t inverse_sqrt_base;
    uint32_t input_fifo[VF2_TGP_FIFO_WORD_COUNT];
    uint32_t output_fifo[VF2_TGP_FIFO_WORD_COUNT];
    uint8_t input_read;
    uint8_t input_write;
    uint8_t input_count;
    uint8_t output_read;
    uint8_t output_write;
    uint8_t output_count;
    uint32_t polygon_ram0[VF2_TGP_POLYGON_RAM_WORD_COUNT];
    uint32_t polygon_ram1[VF2_TGP_POLYGON_RAM_WORD_COUNT];
    vf2_tgp_matrix geometry_matrix;
    float geometry_focus_x;
    float geometry_focus_y;
    uint32_t geometry_mode;
} vf2_tgp;

typedef struct vf2_tgp_geometry_stream_report {
    size_t words_consumed;
    size_t commands;
    size_t object_commands;
    size_t direct_commands;
    size_t polygon_data_commands;
    size_t polygon_data_words;
    size_t max_command_words;
    size_t object_links;
    size_t rendered_triangles;
    int ended;
} vf2_tgp_geometry_stream_report;

vf2_status vf2_tgp_initialize(
    vf2_tgp *tgp,
    const uint8_t *tables,
    size_t tables_size,
    const uint8_t *copro_data,
    size_t copro_data_size
);
void vf2_tgp_reset(vf2_tgp *tgp);

vf2_status vf2_tgp_attach_polygon_rom(
    vf2_tgp *tgp,
    const uint8_t *polygon_rom,
    size_t polygon_rom_size
);
vf2_status vf2_tgp_read_polygon_word(
    const vf2_tgp *tgp,
    uint32_t word_address,
    uint32_t *value
);

vf2_status vf2_tgp_upload_program_word(
    vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t value
);
vf2_status vf2_tgp_set_bank(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_write_function_port(
    vf2_tgp *tgp,
    uint32_t byte_offset,
    uint32_t value
);
vf2_status vf2_tgp_read_input(vf2_tgp *tgp, uint32_t *value);
vf2_status vf2_tgp_write_output(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_output(vf2_tgp *tgp, uint32_t *value);
int vf2_tgp_input_empty(const vf2_tgp *tgp);
int vf2_tgp_output_empty(const vf2_tgp *tgp);

void vf2_tgp_matrix_identity(vf2_tgp_matrix *matrix);
vf2_status vf2_tgp_project_vertex(
    const vf2_tgp_matrix *matrix,
    const vf2_tgp_vertex *vertex,
    uint32_t width,
    uint32_t height,
    vf2_tgp_screen_vertex *screen_vertex
);
vf2_status vf2_tgp_render_triangle(
    vf2_platform *platform,
    const vf2_tgp_matrix *matrix,
    const vf2_tgp_vertex *vertices,
    uint32_t color
);
/* TGP command header decode. A command header word is self-describing: it
 * equals (op << 23) | (op << 8) | op for a 7-bit opcode op, e.g. 0x13802727
 * (op 0x27, Fn_atan), 0x1f803f3f (op 0x3f, Fn_zyx_rot), 0x05800b0b (op 0x0b,
 * Fn_mul_matrix). It is the word the uploaded TGP program's main loop reads
 * and dispatches on (TGP PC 0x35 in VF2 2.1's program), so these are TGP
 * function calls, not Model 2 geometrizer commands: op 0x01/0x02 are
 * push/pop matrix and 0x3f is a rotation, not "object", "direct" and "end"
 * (docs/TGP_COMMANDS.md). The opcode is 7 bits (word >> 23) & 0x7f; 5 bits
 * conflates 0x07/0x27/0x47/0x67. A header test alone cannot frame a stream:
 * a 0.0 argument (0x00000000) passes it as op 0x00. */
uint32_t vf2_tgp_command_opcode(uint32_t word);
int vf2_tgp_is_command_header(uint32_t word);

/* Argument words that follow a command header in the FIFO, from the measured
 * command table (docs/TGP_COMMANDS.md): every opcode VF2 2.1 sent in 3000
 * attract frames on an emulated TGP running the game's own uploaded program,
 * each a fixed count that equals the one in Sega's PS2 port's command table.
 * Returns -1 for an opcode never measured (fail closed), and for 0x70..0x7f,
 * which the program has no handler for. */
int vf2_tgp_command_payload_words(uint32_t opcode);

/* Reply words the TGP pushes for the command, from the same table; -1 when not
 * measured or data-dependent (0x4a Fn_osage: 44..190 in the measured run). */
int vf2_tgp_command_reply_words(uint32_t opcode);

/* The command's firmware name (Fn_*) from Sega's PS2 port's table, for
 * opcodes 0x00..0x6f; NULL otherwise. A name, not a semantics. */
const char *vf2_tgp_command_name(uint32_t opcode);

typedef struct vf2_tgp_command {
    uint32_t opcode;        /* 7-bit opcode of this command's header          */
    size_t header_index;    /* index of the header word in the stream         */
    size_t payload_index;   /* index of the first payload word (header + 1)    */
    size_t payload_count;   /* number of payload words                        */
    int length_verified;    /* 1 if payload_count came from the pinned fixed
                             * length, 0 if from header delimitation           */
} vf2_tgp_command;

/* Frame the command that begins at words[pos] (which must be a command header)
 * and advance. The payload is the measured argument count when the opcode has
 * one (vf2_tgp_command_payload_words != -1); otherwise every word up to the
 * next header-shaped word, which is unreliable (a 0.0 argument looks like a
 * header), so length_verified is 0. Fills *cmd and returns the index of the
 * next command's header (pos + 1 + payload_count), or 0 on error (pos out of
 * range, words[pos] is not a header, or the stream is truncated). */
size_t vf2_tgp_next_command(
    const uint32_t *words,
    size_t count,
    size_t pos,
    vf2_tgp_command *cmd
);

vf2_status vf2_tgp_geometry_command_words(
    const uint32_t *words,
    size_t word_count,
    size_t *command_words
);
vf2_status vf2_tgp_scan_geometry_stream(
    const uint32_t *words,
    size_t word_count,
    vf2_tgp_geometry_stream_report *report
);
vf2_status vf2_tgp_execute_geometry_stream(
    vf2_tgp *tgp,
    const uint32_t *words,
    size_t word_count,
    vf2_platform *platform,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
);

vf2_status vf2_tgp_write_sincos_base(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_sincos(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
);
vf2_status vf2_tgp_write_atan_word(
    vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t value
);
vf2_status vf2_tgp_read_atan(const vf2_tgp *tgp, uint32_t *value);
vf2_status vf2_tgp_write_inverse_base(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_inverse(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
);
vf2_status vf2_tgp_write_inverse_sqrt_base(vf2_tgp *tgp, uint32_t value);
vf2_status vf2_tgp_read_inverse_sqrt(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
);

vf2_status vf2_tgp_read_banked_memory(
    const vf2_tgp *tgp,
    const vf2_model2a *machine,
    uint32_t word_offset,
    uint32_t *value
);
vf2_status vf2_tgp_write_banked_memory(
    const vf2_tgp *tgp,
    vf2_model2a *machine,
    uint32_t word_offset,
    uint32_t value
);

#endif
