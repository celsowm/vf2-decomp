#include "vf2/tgp.h"

#include <string.h>

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static int is_power_of_two(size_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

static vf2_status read_table_word(
    const vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t *value
)
{
    const size_t word_count = tgp->tables_size / sizeof(uint32_t);

    if (tgp->tables == NULL || value == NULL ||
        (size_t)word_index >= word_count) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = read_le32(tgp->tables + (size_t)word_index * sizeof(uint32_t));
    return VF2_OK;
}

static vf2_status fifo_push(
    uint32_t *fifo,
    uint8_t *write_index,
    uint8_t *count,
    uint32_t value
)
{
    if (*count >= VF2_TGP_FIFO_WORD_COUNT) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    fifo[*write_index] = value;
    *write_index = (uint8_t)((*write_index + 1u) % VF2_TGP_FIFO_WORD_COUNT);
    ++*count;
    return VF2_OK;
}

static vf2_status fifo_pop(
    uint32_t *fifo,
    uint8_t *read_index,
    uint8_t *count,
    uint32_t *value
)
{
    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (*count == 0u) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = fifo[*read_index];
    *read_index = (uint8_t)((*read_index + 1u) % VF2_TGP_FIFO_WORD_COUNT);
    --*count;
    return VF2_OK;
}

vf2_status vf2_tgp_initialize(
    vf2_tgp *tgp,
    const uint8_t *tables,
    size_t tables_size,
    const uint8_t *copro_data,
    size_t copro_data_size
)
{
    if (tgp == NULL || tables == NULL ||
        tables_size < (size_t)VF2_TGP_TABLE_WORD_COUNT * sizeof(uint32_t) ||
        (tables_size % sizeof(uint32_t)) != 0u ||
        (copro_data == NULL && copro_data_size != 0u) ||
        (copro_data_size % sizeof(uint32_t)) != 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(tgp, 0, sizeof(*tgp));
    tgp->tables = tables;
    tgp->tables_size = tables_size;
    tgp->copro_data = copro_data;
    tgp->copro_data_size = copro_data_size;
    vf2_tgp_matrix_identity(&tgp->geometry_matrix);
    tgp->geometry_focus_x = 1.0f;
    tgp->geometry_focus_y = 1.0f;
    return VF2_OK;
}

void vf2_tgp_reset(vf2_tgp *tgp)
{
    const uint8_t *tables = NULL;
    size_t tables_size = 0u;
    const uint8_t *copro_data = NULL;
    size_t copro_data_size = 0u;
    const uint8_t *polygon_rom = NULL;
    size_t polygon_rom_size = 0u;

    if (tgp == NULL) {
        return;
    }
    tables = tgp->tables;
    tables_size = tgp->tables_size;
    copro_data = tgp->copro_data;
    copro_data_size = tgp->copro_data_size;
    polygon_rom = tgp->polygon_rom;
    polygon_rom_size = tgp->polygon_rom_size;
    memset(tgp, 0, sizeof(*tgp));
    tgp->tables = tables;
    tgp->tables_size = tables_size;
    tgp->copro_data = copro_data;
    tgp->copro_data_size = copro_data_size;
    tgp->polygon_rom = polygon_rom;
    tgp->polygon_rom_size = polygon_rom_size;
    vf2_tgp_matrix_identity(&tgp->geometry_matrix);
    tgp->geometry_focus_x = 1.0f;
    tgp->geometry_focus_y = 1.0f;
}

vf2_status vf2_tgp_attach_polygon_rom(
    vf2_tgp *tgp,
    const uint8_t *polygon_rom,
    size_t polygon_rom_size
)
{
    const size_t word_count = polygon_rom_size / sizeof(uint32_t);

    if (tgp == NULL || polygon_rom == NULL ||
        polygon_rom_size < sizeof(uint32_t) ||
        (polygon_rom_size % sizeof(uint32_t)) != 0u ||
        !is_power_of_two(word_count)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    tgp->polygon_rom = polygon_rom;
    tgp->polygon_rom_size = polygon_rom_size;
    return VF2_OK;
}

vf2_status vf2_tgp_read_polygon_word(
    const vf2_tgp *tgp,
    uint32_t word_address,
    uint32_t *value
)
{
    const size_t word_count = tgp == NULL
        ? 0u : tgp->polygon_rom_size / sizeof(uint32_t);
    const size_t mask = word_count == 0u ? 0u : word_count - 1u;

    if (tgp == NULL || value == NULL || tgp->polygon_rom == NULL ||
        word_count == 0u || !is_power_of_two(word_count)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = read_le32(tgp->polygon_rom +
                       (word_address & (uint32_t)mask) * sizeof(uint32_t));
    return VF2_OK;
}

vf2_status vf2_tgp_upload_program_word(
    vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t value
)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (word_index >= VF2_TGP_PROGRAM_WORD_COUNT) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    tgp->program[word_index] = value;
    return VF2_OK;
}

vf2_status vf2_tgp_set_bank(vf2_tgp *tgp, uint32_t value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    tgp->bank_register = value;
    return VF2_OK;
}

vf2_status vf2_tgp_write_function_port(
    vf2_tgp *tgp,
    uint32_t byte_offset,
    uint32_t value
)
{
    const uint32_t port_tag = ((byte_offset >> 2u) & UINT32_C(0xff)) << 23u;
    const uint32_t packet = (value & UINT32_C(0x800fffff)) | port_tag;

    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return fifo_push(
        tgp->input_fifo, &tgp->input_write, &tgp->input_count, packet
    );
}

vf2_status vf2_tgp_read_input(vf2_tgp *tgp, uint32_t *value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return fifo_pop(
        tgp->input_fifo, &tgp->input_read, &tgp->input_count, value
    );
}

vf2_status vf2_tgp_write_output(vf2_tgp *tgp, uint32_t value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return fifo_push(
        tgp->output_fifo, &tgp->output_write, &tgp->output_count, value
    );
}

vf2_status vf2_tgp_read_output(vf2_tgp *tgp, uint32_t *value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return fifo_pop(
        tgp->output_fifo, &tgp->output_read, &tgp->output_count, value
    );
}

int vf2_tgp_input_empty(const vf2_tgp *tgp)
{
    return tgp == NULL || tgp->input_count == 0u;
}

int vf2_tgp_output_empty(const vf2_tgp *tgp)
{
    return tgp == NULL || tgp->output_count == 0u;
}

void vf2_tgp_matrix_identity(vf2_tgp_matrix *matrix)
{
    if (matrix == NULL) {
        return;
    }
    memset(matrix, 0, sizeof(*matrix));
    matrix->values[0] = 1.0f;
    matrix->values[5] = 1.0f;
    matrix->values[10] = 1.0f;
    matrix->values[15] = 1.0f;
}

vf2_status vf2_tgp_project_vertex(
    const vf2_tgp_matrix *matrix,
    const vf2_tgp_vertex *vertex,
    uint32_t width,
    uint32_t height,
    vf2_tgp_screen_vertex *screen_vertex
)
{
    float clip_x = 0.0f;
    float clip_y = 0.0f;
    float clip_z = 0.0f;
    float clip_w = 0.0f;
    float normalized_x = 0.0f;
    float normalized_y = 0.0f;

    if (matrix == NULL || vertex == NULL || screen_vertex == NULL ||
        width == 0u || height == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    clip_x = matrix->values[0] * vertex->x +
             matrix->values[4] * vertex->y +
             matrix->values[8] * vertex->z +
             matrix->values[12] * vertex->w;
    clip_y = matrix->values[1] * vertex->x +
             matrix->values[5] * vertex->y +
             matrix->values[9] * vertex->z +
             matrix->values[13] * vertex->w;
    clip_z = matrix->values[2] * vertex->x +
             matrix->values[6] * vertex->y +
             matrix->values[10] * vertex->z +
             matrix->values[14] * vertex->w;
    clip_w = matrix->values[3] * vertex->x +
             matrix->values[7] * vertex->y +
             matrix->values[11] * vertex->z +
             matrix->values[15] * vertex->w;
    if (clip_w <= 0.0f) {
        return VF2_ERROR_UNSUPPORTED;
    }
    normalized_x = clip_x / clip_w;
    normalized_y = clip_y / clip_w;
    screen_vertex->x = (int32_t)(((normalized_x + 1.0f) *
                                  (float)(width - 1u) * 0.5f) + 0.5f);
    screen_vertex->y = (int32_t)(((1.0f - normalized_y) *
                                  (float)(height - 1u) * 0.5f) + 0.5f);
    screen_vertex->depth = clip_z / clip_w;
    return VF2_OK;
}

vf2_status vf2_tgp_render_triangle(
    vf2_platform *platform,
    const vf2_tgp_matrix *matrix,
    const vf2_tgp_vertex *vertices,
    uint32_t color
)
{
    vf2_tgp_screen_vertex projected[3];
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (platform == NULL || matrix == NULL || vertices == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < 3u; ++index) {
        status = vf2_tgp_project_vertex(
            matrix,
            &vertices[index],
            platform->width,
            platform->height,
            &projected[index]
        );
        if (status != VF2_OK) {
            return status;
        }
    }
    return vf2_platform_fill_triangle_depth(
        platform,
        projected[0].x,
        projected[0].y,
        projected[0].depth,
        projected[1].x,
        projected[1].y,
        projected[1].depth,
        projected[2].x,
        projected[2].y,
        projected[2].depth,
        color
    );
}

static vf2_status geometry_count_words(
    const uint32_t *words,
    size_t word_count,
    size_t *command_words
)
{
    const uint32_t opcode = (words[0] >> 23u) & UINT32_C(0x1f);
    size_t required = 1u;
    size_t count = 0u;

    switch (opcode) {
    case 0x00u:
    case 0x0fu:
    case 0x1fu:
        break;
    case 0x01u:
        required = 5u;
        break;
    case 0x02u:
        required = 9u;
        if (word_count < required) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        for (;;) {
            const uint32_t attribute = words[required++];
            if ((attribute & UINT32_C(3)) == 0u) {
                break;
            }
            if (word_count - required < 5u) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            required += 5u;
            if ((attribute & UINT32_C(1)) != 0u) {
                if (word_count - required < 3u) {
                    return VF2_ERROR_OUT_OF_BOUNDS;
                }
                required += 3u;
            }
        }
        break;
    case 0x03u:
        required = 7u;
        break;
    case 0x04u:
    case 0x05u:
    case 0x15u:
        required = 3u;
        if (word_count < required) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        count = (size_t)words[2];
        if (count > SIZE_MAX - required) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        required += count;
        break;
    case 0x06u:
        required = 3u;
        if (word_count < required) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        count = (size_t)words[2];
        if (count > (SIZE_MAX - required) / 2u) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        required += count * 2u;
        break;
    case 0x07u:
    case 0x08u:
    case 0x10u:
    case 0x16u:
    case 0x17u:
    case 0x18u:
    case 0x19u:
    case 0x1au:
    case 0x1bu:
    case 0x1cu:
        required = 2u;
        break;
    case 0x09u:
        required = 3u;
        break;
    case 0x0au:
        required = 4u;
        break;
    case 0x0bu:
        required = 13u;
        break;
    case 0x0cu:
        required = 4u;
        break;
    case 0x0du:
        required = 3u;
        break;
    case 0x14u:
        required = 3u;
        if (word_count < required) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        count = (size_t)words[2];
        if (count > SIZE_MAX - required) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        required += count;
        break;
    case 0x0eu:
    case 0x1du:
    case 0x1eu:
    default:
        return VF2_ERROR_UNSUPPORTED;
    }
    if (required > word_count) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *command_words = required;
    return VF2_OK;
}

vf2_status vf2_tgp_geometry_command_words(
    const uint32_t *words,
    size_t word_count,
    size_t *command_words
)
{
    if (words == NULL || command_words == NULL || word_count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if ((words[0] & UINT32_C(0x80000000)) != 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return geometry_count_words(words, word_count, command_words);
}

vf2_status vf2_tgp_scan_geometry_stream(
    const uint32_t *words,
    size_t word_count,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_tgp_geometry_stream_report local = {0};

    if (words == NULL || report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    while (local.words_consumed < word_count) {
        const size_t remaining = word_count - local.words_consumed;
        const uint32_t opcode = words[local.words_consumed];
        const uint32_t command_class = (opcode >> 23u) & UINT32_C(0x1f);
        size_t command_words = 0u;
        vf2_status status = vf2_tgp_geometry_command_words(
            words + local.words_consumed,
            remaining,
            &command_words
        );
        if (status != VF2_OK) {
            return status;
        }
        local.words_consumed += command_words;
        ++local.commands;
        if (command_words > local.max_command_words) {
            local.max_command_words = command_words;
        }
        if (command_class == 0x01u) {
            ++local.object_commands;
        } else if (command_class == 0x02u) {
            ++local.direct_commands;
        } else if (command_class == 0x05u || command_class == 0x15u) {
            ++local.polygon_data_commands;
            local.polygon_data_words += command_words - 3u;
        }
        if (command_class == 0x0fu || command_class == 0x1fu) {
            local.ended = 1;
            break;
        }
    }
    *report = local;
    return VF2_OK;
}

static float geometry_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

typedef struct vf2_tgp_geometry_source {
    const vf2_tgp *tgp;
    const uint32_t *ram;
    uint32_t base;
    int polygon_rom;
} vf2_tgp_geometry_source;

static vf2_status geometry_source_read(
    const vf2_tgp_geometry_source *source,
    uint32_t offset,
    uint32_t *value
)
{
    if (source == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (source->polygon_rom != 0) {
        return vf2_tgp_read_polygon_word(
            source->tgp, source->base + offset, value
        );
    }
    if (source->ram == NULL ||
        source->base >= VF2_TGP_POLYGON_RAM_WORD_COUNT ||
        offset >= VF2_TGP_POLYGON_RAM_WORD_COUNT - source->base) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = source->ram[source->base + offset];
    return VF2_OK;
}

static vf2_tgp_vertex geometry_transform_point(
    const vf2_tgp *tgp,
    vf2_tgp_vertex point
)
{
    const vf2_tgp_matrix *matrix = &tgp->geometry_matrix;
    vf2_tgp_vertex result;

    result.x = (matrix->values[0] * point.x +
                matrix->values[4] * point.y +
                matrix->values[8] * point.z +
                matrix->values[12] * point.w) * tgp->geometry_focus_x;
    result.y = (matrix->values[1] * point.x +
                matrix->values[5] * point.y +
                matrix->values[9] * point.z +
                matrix->values[13] * point.w) * tgp->geometry_focus_y;
    result.z = matrix->values[2] * point.x +
               matrix->values[6] * point.y +
               matrix->values[10] * point.z +
               matrix->values[14] * point.w;
    result.w = matrix->values[3] * point.x +
               matrix->values[7] * point.y +
               matrix->values[11] * point.z +
               matrix->values[15] * point.w;
    return result;
}

static vf2_status geometry_render_polygon(
    vf2_tgp *tgp,
    vf2_platform *platform,
    const vf2_tgp_vertex *points,
    size_t point_count,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_tgp_matrix identity;
    vf2_tgp_vertex transformed[4];
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (point_count != 3u && point_count != 4u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    vf2_tgp_matrix_identity(&identity);
    for (index = 0u; index < point_count; ++index) {
        transformed[index] = geometry_transform_point(tgp, points[index]);
    }
    status = vf2_tgp_render_triangle(
        platform, &identity, transformed, color
    );
    if (status != VF2_OK) {
        return status;
    }
    ++report->rendered_triangles;
    if (point_count == 4u) {
        vf2_tgp_vertex second[3] = {
            transformed[0], transformed[2], transformed[3]
        };
        status = vf2_tgp_render_triangle(
            platform, &identity, second, color
        );
        if (status != VF2_OK) {
            return status;
        }
        ++report->rendered_triangles;
    }
    return VF2_OK;
}

static vf2_tgp_vertex geometry_read_point_words(
    const uint32_t *words,
    size_t offset
)
{
    vf2_tgp_vertex point;

    point.x = geometry_float(words[offset]);
    point.y = geometry_float(words[offset + 1u]);
    point.z = geometry_float(words[offset + 2u]);
    point.w = 1.0f;
    return point;
}

static vf2_status geometry_execute_direct(
    vf2_tgp *tgp,
    const uint32_t *words,
    size_t command_words,
    vf2_platform *platform,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_tgp_vertex p0;
    vf2_tgp_vertex p1;
    size_t offset = 9u;

    p0 = geometry_read_point_words(words, 3u);
    p1 = geometry_read_point_words(words, 6u);
    while (offset < command_words) {
        const uint32_t attribute = words[offset++];
        vf2_tgp_vertex points[4];
        size_t point_count = 0u;
        vf2_status status = VF2_OK;

        if ((attribute & UINT32_C(3)) == 0u) {
            break;
        }
        if (offset + 5u > command_words) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        offset += 2u;
        points[0] = p0;
        points[1] = p1;
        points[2] = geometry_read_point_words(words, offset);
        offset += 3u;
        point_count = 3u;
        if ((attribute & UINT32_C(1)) != 0u) {
            if (offset + 3u > command_words) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            points[3] = geometry_read_point_words(words, offset);
            offset += 3u;
            point_count = 4u;
        }
        status = geometry_render_polygon(
            tgp, platform, points, point_count, color, report
        );
        if (status != VF2_OK) {
            return status;
        }
        ++report->object_links;
        switch ((attribute >> 8u) & UINT32_C(3)) {
        case 0u:
        case 2u:
            p0 = points[2];
            p1 = point_count == 4u ? points[3] : points[2];
            break;
        case 1u:
            p1 = points[2];
            break;
        case 3u:
            p0 = point_count == 4u ? points[3] : points[2];
            break;
        }
    }
    return VF2_OK;
}

static vf2_status geometry_execute_object(
    vf2_tgp *tgp,
    const vf2_tgp_geometry_source *source,
    uint32_t link_count,
    vf2_platform *platform,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_tgp_vertex p0;
    vf2_tgp_vertex p1;
    uint32_t offset = 0u;
    uint32_t link_limit = link_count == 0u ? UINT32_C(0xfffff) : link_count;
    vf2_status status = VF2_OK;
    uint32_t value = 0u;

    status = geometry_source_read(source, offset++, &value);
    if (status != VF2_OK) {
        return status;
    }
    p0.x = geometry_float(value);
    status = geometry_source_read(source, offset++, &value);
    if (status != VF2_OK) {
        return status;
    }
    p0.y = geometry_float(value);
    status = geometry_source_read(source, offset++, &value);
    if (status != VF2_OK) {
        return status;
    }
    p0.z = geometry_float(value);
    p0.w = 1.0f;
    status = geometry_source_read(source, offset++, &value);
    if (status != VF2_OK) {
        return status;
    }
    p1.x = geometry_float(value);
    status = geometry_source_read(source, offset++, &value);
    if (status != VF2_OK) {
        return status;
    }
    p1.y = geometry_float(value);
    status = geometry_source_read(source, offset++, &value);
    if (status != VF2_OK) {
        return status;
    }
    p1.z = geometry_float(value);
    p1.w = 1.0f;

    for (uint32_t link = 0u; link < link_limit; ++link) {
        uint32_t attribute = 0u;
        vf2_tgp_vertex points[4];
        size_t point_count = 3u;

        status = geometry_source_read(source, offset++, &attribute);
        if (status != VF2_OK) {
            return status;
        }
        if ((attribute & UINT32_C(3)) == 0u) {
            break;
        }
        if ((tgp->geometry_mode & UINT32_C(3)) < 2u) {
            if (geometry_source_read(source, offset++, &value) != VF2_OK ||
                geometry_source_read(source, offset++, &value) != VF2_OK ||
                geometry_source_read(source, offset++, &value) != VF2_OK) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
        }
        points[0] = p0;
        points[1] = p1;
        for (size_t component = 0u; component < 3u; ++component) {
            status = geometry_source_read(source, offset++, &value);
            if (status != VF2_OK) {
                return status;
            }
            if (component == 0u) {
                points[2].x = geometry_float(value);
            } else if (component == 1u) {
                points[2].y = geometry_float(value);
            } else {
                points[2].z = geometry_float(value);
            }
        }
        points[2].w = 1.0f;
        if ((attribute & UINT32_C(1)) != 0u) {
            point_count = 4u;
            for (size_t component = 0u; component < 3u; ++component) {
                status = geometry_source_read(source, offset++, &value);
                if (status != VF2_OK) {
                    return status;
                }
                if (component == 0u) {
                    points[3].x = geometry_float(value);
                } else if (component == 1u) {
                    points[3].y = geometry_float(value);
                } else {
                    points[3].z = geometry_float(value);
                }
            }
            points[3].w = 1.0f;
        }
        status = geometry_render_polygon(
            tgp, platform, points, point_count, color, report
        );
        if (status != VF2_OK) {
            return status;
        }
        ++report->object_links;
        switch ((attribute >> 8u) & UINT32_C(3)) {
        case 0u:
        case 2u:
            p0 = points[2];
            p1 = point_count == 4u ? points[3] : points[2];
            break;
        case 1u:
            p1 = points[2];
            break;
        case 3u:
            p0 = point_count == 4u ? points[3] : points[2];
            break;
        }
    }
    return VF2_OK;
}

static void geometry_write_matrix(vf2_tgp *tgp, const uint32_t *words)
{
    vf2_tgp_matrix *matrix = &tgp->geometry_matrix;

    matrix->values[0] = geometry_float(words[1]);
    matrix->values[4] = geometry_float(words[4]);
    matrix->values[8] = geometry_float(words[7]);
    matrix->values[12] = geometry_float(words[10]);
    matrix->values[1] = geometry_float(words[2]);
    matrix->values[5] = geometry_float(words[5]);
    matrix->values[9] = geometry_float(words[8]);
    matrix->values[13] = geometry_float(words[11]);
    matrix->values[2] = geometry_float(words[3]);
    matrix->values[6] = geometry_float(words[6]);
    matrix->values[10] = geometry_float(words[9]);
    matrix->values[14] = geometry_float(words[12]);
    matrix->values[3] = 0.0f;
    matrix->values[7] = 0.0f;
    matrix->values[11] = 0.0f;
    matrix->values[15] = 1.0f;
}

vf2_status vf2_tgp_execute_geometry_stream(
    vf2_tgp *tgp,
    const uint32_t *words,
    size_t word_count,
    vf2_platform *platform,
    uint32_t color,
    vf2_tgp_geometry_stream_report *report
)
{
    vf2_tgp_geometry_stream_report local = {0};

    if (tgp == NULL || words == NULL || platform == NULL ||
        report == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    while (local.words_consumed < word_count) {
        const size_t remaining = word_count - local.words_consumed;
        const uint32_t *command = words + local.words_consumed;
        const uint32_t command_class =
            (command[0] >> 23u) & UINT32_C(0x1f);
        size_t command_words = 0u;
        vf2_status status = vf2_tgp_geometry_command_words(
            command, remaining, &command_words
        );

        if (status != VF2_OK) {
            return status;
        }
        switch (command_class) {
        case 0x01u: {
            vf2_tgp_geometry_source source;
            const uint32_t object_address = command[3];

            source.tgp = tgp;
            source.polygon_rom =
                (object_address & UINT32_C(0x00800000)) != 0u;
            source.base = source.polygon_rom != 0
                ? object_address & UINT32_C(0x7fffff)
                : object_address & UINT32_C(0x7fff);
            source.ram = source.polygon_rom != 0 ? NULL :
                ((object_address & UINT32_C(0x01000000)) != 0u
                    ? tgp->polygon_ram1 : tgp->polygon_ram0);
            status = geometry_execute_object(
                tgp, &source, command[4], platform, color, &local
            );
            if (status != VF2_OK) {
                return status;
            }
            ++local.object_commands;
            break;
        }
        case 0x02u:
            status = geometry_execute_direct(
                tgp, command, command_words, platform, color, &local
            );
            if (status != VF2_OK) {
                return status;
            }
            ++local.direct_commands;
            break;
        case 0x05u:
        case 0x15u: {
            const uint32_t address = command[1];
            const uint32_t count = command[2];
            uint32_t *destination =
                (address & UINT32_C(0x01000000)) != 0u
                    ? tgp->polygon_ram1 : tgp->polygon_ram0;
            const uint32_t index = address & UINT32_C(0x7fff);

            if (count > VF2_TGP_POLYGON_RAM_WORD_COUNT - index) {
                return VF2_ERROR_OUT_OF_BOUNDS;
            }
            memcpy(destination + index, command + 3u,
                   (size_t)count * sizeof(uint32_t));
            ++local.polygon_data_commands;
            local.polygon_data_words += count;
            break;
        }
        case 0x07u:
            tgp->geometry_mode = command[1];
            break;
        case 0x09u:
            tgp->geometry_focus_x = geometry_float(command[1]);
            tgp->geometry_focus_y = geometry_float(command[2]);
            break;
        case 0x0bu:
            geometry_write_matrix(tgp, command);
            break;
        case 0x0cu:
            tgp->geometry_matrix.values[12] = geometry_float(command[1]);
            tgp->geometry_matrix.values[13] = geometry_float(command[2]);
            tgp->geometry_matrix.values[14] = geometry_float(command[3]);
            break;
        default:
            break;
        }
        local.words_consumed += command_words;
        ++local.commands;
        if (command_words > local.max_command_words) {
            local.max_command_words = command_words;
        }
        if (command_class == 0x0fu || command_class == 0x1fu) {
            local.ended = 1;
            break;
        }
    }
    *report = local;
    return VF2_OK;
}

vf2_status vf2_tgp_write_sincos_base(vf2_tgp *tgp, uint32_t value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    tgp->sincos_base = value;
    return VF2_OK;
}

vf2_status vf2_tgp_read_sincos(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
)
{
    uint32_t angle = 0u;
    uint32_t index = 0u;
    vf2_status status = VF2_OK;

    if (tgp == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    angle = tgp->sincos_base + word_offset * UINT32_C(0x4000);
    index = angle & UINT32_C(0x3fff);
    if ((angle & UINT32_C(0x4000)) != 0u) {
        const uint32_t mirrored = UINT32_C(0x4000) - index;
        index = mirrored < UINT32_C(0x3fff) ? mirrored : UINT32_C(0x3fff);
    }
    status = read_table_word(tgp, index, value);
    if (status == VF2_OK && (angle & UINT32_C(0x8000)) != 0u) {
        *value ^= UINT32_C(0x80000000);
    }
    return status;
}

vf2_status vf2_tgp_write_atan_word(
    vf2_tgp *tgp,
    uint32_t word_index,
    uint32_t value
)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (word_index >= 4u) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    tgp->atan_base[word_index] = value;
    return VF2_OK;
}

vf2_status vf2_tgp_read_atan(const vf2_tgp *tgp, uint32_t *value)
{
    uint32_t index = 0u;
    uint32_t result = 0u;
    uint8_t exponent = 0u;
    int sign0 = 0;
    int sign1 = 0;
    int ordered = 0;

    if (tgp == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    exponent = (uint8_t)(UINT32_C(0x88) - (tgp->atan_base[3] >> 23u));
    sign0 = (tgp->atan_base[0] & UINT32_C(0x80000000)) != 0u;
    sign1 = (tgp->atan_base[1] & UINT32_C(0x80000000)) != 0u;
    ordered = (tgp->atan_base[0] & UINT32_C(0x7fffffff)) <=
              (tgp->atan_base[1] & UINT32_C(0x7fffffff));
    if (exponent <= UINT8_C(0x17)) {
        index = ((tgp->atan_base[3] & UINT32_C(0x7fffff)) |
                 UINT32_C(0x800000)) >> exponent;
    }
    if (index == UINT32_C(0x4000)) {
        index = UINT32_C(0x3fff);
    }
    if (read_table_word(tgp, index | UINT32_C(0x4000), &result) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    if ((sign0 ^ sign1 ^ ordered) != 0) {
        result >>= 16u;
    }
    if (ordered) {
        result += UINT32_C(0x4000);
    }
    if ((sign0 != 0 && !ordered) || (sign1 != 0 && ordered)) {
        result += UINT32_C(0x8000);
    }
    *value = result & UINT32_C(0xffff);
    return VF2_OK;
}

vf2_status vf2_tgp_write_inverse_base(vf2_tgp *tgp, uint32_t value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    tgp->inverse_base = value;
    return VF2_OK;
}

vf2_status vf2_tgp_read_inverse(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
)
{
    uint32_t index = 0u;
    uint32_t result = 0u;
    uint8_t exponent = 0u;

    if (tgp == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (word_offset > 1u) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    index = ((tgp->inverse_base >> 9u) & UINT32_C(0x3ffe)) | word_offset;
    if (read_table_word(tgp, index | UINT32_C(0x8000), &result) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    exponent = (uint8_t)((result >> 23u) +
               (UINT32_C(0x7f) - ((tgp->inverse_base >> 23u) & UINT32_C(0xff))));
    result = (result & UINT32_C(0x007fffff)) |
             ((uint32_t)exponent << 23u);
    if ((tgp->inverse_base & UINT32_C(0x80000000)) != 0u && word_offset != 0u) {
        result |= UINT32_C(0x80000000);
    }
    *value = result;
    return VF2_OK;
}

vf2_status vf2_tgp_write_inverse_sqrt_base(vf2_tgp *tgp, uint32_t value)
{
    if (tgp == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    tgp->inverse_sqrt_base = value;
    return VF2_OK;
}

vf2_status vf2_tgp_read_inverse_sqrt(
    const vf2_tgp *tgp,
    uint32_t word_offset,
    uint32_t *value
)
{
    uint32_t index = 0u;
    uint32_t result = 0u;
    uint8_t exponent = 0u;

    if (tgp == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (word_offset > 1u) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    index = UINT32_C(0x2000) ^
            (((tgp->inverse_sqrt_base >> 10u) & UINT32_C(0x3ffe)) |
             word_offset);
    if (read_table_word(tgp, index | UINT32_C(0xc000), &result) != VF2_OK) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    exponent = (uint8_t)((result >> 23u) +
               (UINT32_C(0x3f) - ((tgp->inverse_sqrt_base >> 24u) & UINT32_C(0x7f))));
    result = (result & UINT32_C(0x807fffff)) |
             ((uint32_t)exponent << 23u);
    if (word_offset == 0u) {
        result &= UINT32_C(0x7fffffff);
    }
    *value = result;
    return VF2_OK;
}

vf2_status vf2_tgp_read_banked_memory(
    const vf2_tgp *tgp,
    const vf2_model2a *machine,
    uint32_t word_offset,
    uint32_t *value
)
{
    uint32_t address = 0u;
    uint32_t index = 0u;

    if (tgp == NULL || machine == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* The Model 2A window decodes the bank selector in bits 16..23;
     * low register bits are not part of the external address. */
    address = (tgp->bank_register & UINT32_C(0x00ff0000)) |
              (word_offset & UINT32_C(0x0000ffff));
    index = address & UINT32_C(0x7fffff);
    if ((address & UINT32_C(0x800000)) != 0u) {
        const size_t words = tgp->copro_data_size / sizeof(uint32_t);
        const size_t mask = words == 0u ? 0u : words - 1u;
        if (words == 0u || (words & mask) != 0u) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *value = read_le32(tgp->copro_data +
                           (size_t)(index & (uint32_t)mask) * sizeof(uint32_t));
        return VF2_OK;
    }
    if ((address & UINT32_C(0x400000)) != 0u) {
        return vf2_model2a_read_u32(
            machine,
            VF2_BUFFER_RAM_BASE + (index & VF2_TGP_BUFFER_WORD_MASK) * 4u,
            value
        );
    }
    *value = 0u;
    return VF2_OK;
}

vf2_status vf2_tgp_write_banked_memory(
    const vf2_tgp *tgp,
    vf2_model2a *machine,
    uint32_t word_offset,
    uint32_t value
)
{
    uint32_t address = 0u;
    uint32_t index = 0u;

    if (tgp == NULL || machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* Match the Model 2A bank window: only the high bank byte is decoded. */
    address = (tgp->bank_register & UINT32_C(0x00ff0000)) |
              (word_offset & UINT32_C(0x0000ffff));
    index = address & UINT32_C(0x7fffff);
    if ((address & UINT32_C(0x400000)) != 0u) {
        return vf2_model2a_write_u32(
            machine,
            VF2_BUFFER_RAM_BASE + (index & VF2_TGP_BUFFER_WORD_MASK) * 4u,
            value
        );
    }
    return VF2_OK;
}
