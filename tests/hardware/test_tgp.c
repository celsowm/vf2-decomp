#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/model2a.h"
#include "vf2/tgp.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static uint32_t float_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void test_tgp_tables_and_scalar_services(void)
{
    static uint8_t tables[VF2_TGP_TABLE_WORD_COUNT * sizeof(uint32_t)];
    vf2_tgp tgp;
    uint32_t value = 0u;

    memset(tables, 0, sizeof(tables));
    write_le32(tables + 0u * 4u, UINT32_C(0x12345678));
    write_le32(tables + 0x3fffu * 4u, UINT32_C(0x87654321));
    write_le32(tables + 0x4000u * 4u, UINT32_C(0x12345678));
    write_le32(tables + 0x8000u * 4u, UINT32_C(0x3f800000));
    write_le32(tables + 0x8001u * 4u, UINT32_C(0x3f800000));
    write_le32(tables + 0xe000u * 4u, UINT32_C(0x3f800000));

    CHECK(
        vf2_tgp_initialize(&tgp, tables, sizeof(tables), NULL, 0u) == VF2_OK
    );
    CHECK(vf2_tgp_write_sincos_base(&tgp, 0u) == VF2_OK);
    CHECK(vf2_tgp_read_sincos(&tgp, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x12345678));
    CHECK(vf2_tgp_write_sincos_base(&tgp, UINT32_C(0x4001)) == VF2_OK);
    CHECK(vf2_tgp_read_sincos(&tgp, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x87654321));
    CHECK(vf2_tgp_write_sincos_base(&tgp, UINT32_C(0x8000)) == VF2_OK);
    CHECK(vf2_tgp_read_sincos(&tgp, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x92345678));

    CHECK(vf2_tgp_write_inverse_base(&tgp, 0u) == VF2_OK);
    CHECK(vf2_tgp_read_inverse(&tgp, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x7f000000));
    CHECK(
        vf2_tgp_write_inverse_base(&tgp, UINT32_C(0x80000000)) == VF2_OK
    );
    CHECK(vf2_tgp_read_inverse(&tgp, 1u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0xff000000));

    CHECK(vf2_tgp_write_inverse_sqrt_base(&tgp, 0u) == VF2_OK);
    CHECK(vf2_tgp_read_inverse_sqrt(&tgp, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x5f000000));

    write_le32(tables + 0x4000u * 4u, UINT32_C(0x12345678));
    write_le32(tables + 0x4080u * 4u, UINT32_C(0x56780000));
    CHECK(vf2_tgp_write_atan_word(&tgp, 0u, 0u) == VF2_OK);
    CHECK(vf2_tgp_write_atan_word(&tgp, 1u, 0u) == VF2_OK);
    CHECK(vf2_tgp_write_atan_word(&tgp, 2u, 0u) == VF2_OK);
    CHECK(vf2_tgp_write_atan_word(&tgp, 3u, UINT32_C(0x3c000000)) == VF2_OK);
    CHECK(vf2_tgp_read_atan(&tgp, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x9678));
}

static void test_tgp_fifos_and_banked_memory(void)
{
    static uint8_t tables[VF2_TGP_TABLE_WORD_COUNT * sizeof(uint32_t)];
    static uint8_t copro_data[8u * sizeof(uint32_t)];
    vf2_tgp tgp;
    vf2_model2a *machine = NULL;
    uint32_t value = 0u;
    uint32_t index = 0u;
    const uint32_t expected_packet =
        (UINT32_C(0xdeadbeef) & UINT32_C(0x800fffff)) |
        ((UINT32_C(0x24) >> 2u) << 23u);

    memset(tables, 0, sizeof(tables));
    memset(copro_data, 0, sizeof(copro_data));
    write_le32(copro_data + 2u * 4u, UINT32_C(0xaabbccdd));
    CHECK(
        vf2_tgp_initialize(
            &tgp, tables, sizeof(tables), copro_data, sizeof(copro_data)
        ) == VF2_OK
    );
    CHECK(vf2_tgp_input_empty(&tgp));
    CHECK(vf2_tgp_write_function_port(&tgp, UINT32_C(0x24), UINT32_C(0xdeadbeef)) == VF2_OK);
    CHECK(!vf2_tgp_input_empty(&tgp));
    CHECK(vf2_tgp_read_input(&tgp, &value) == VF2_OK);
    CHECK(value == expected_packet);
    CHECK(vf2_tgp_read_input(&tgp, &value) == VF2_ERROR_OUT_OF_BOUNDS);

    for (index = 0u; index < VF2_TGP_FIFO_WORD_COUNT; ++index) {
        CHECK(vf2_tgp_write_output(&tgp, index + UINT32_C(0x100)) == VF2_OK);
    }
    CHECK(vf2_tgp_write_output(&tgp, 0u) == VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(vf2_tgp_read_output(&tgp, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x100));

    machine = (vf2_model2a *)calloc(1u, sizeof(*machine));
    CHECK(machine != NULL);
    if (machine == NULL) {
        return;
    }
    CHECK(vf2_model2a_initialize(machine) != 0);
    CHECK(vf2_tgp_set_bank(&tgp, UINT32_C(0x400000)) == VF2_OK);
    CHECK(
        vf2_tgp_write_banked_memory(
            &tgp, machine, 0u, UINT32_C(0xaabbccdd)
        ) == VF2_OK
    );
    CHECK(
        vf2_tgp_write_banked_memory(
            &tgp, machine, 3u, UINT32_C(0x11223344)
        ) == VF2_OK
    );
    CHECK(vf2_tgp_read_banked_memory(&tgp, machine, 3u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x11223344));
    CHECK(vf2_tgp_set_bank(&tgp, UINT32_C(0x400003)) == VF2_OK);
    CHECK(vf2_tgp_read_banked_memory(&tgp, machine, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0xaabbccdd));

    CHECK(vf2_tgp_set_bank(&tgp, UINT32_C(0x800000)) == VF2_OK);
    CHECK(vf2_tgp_read_banked_memory(&tgp, machine, 2u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0xaabbccdd));
    CHECK(vf2_tgp_upload_program_word(&tgp, 0u, UINT32_C(0xfeedface)) == VF2_OK);
    CHECK(vf2_tgp_upload_program_word(&tgp, VF2_TGP_PROGRAM_WORD_COUNT, 0u) == VF2_ERROR_OUT_OF_BOUNDS);
    vf2_model2a_shutdown(machine);
    free(machine);
}

static void test_tgp_polygon_rom_window(void)
{
    static uint8_t tables[VF2_TGP_TABLE_WORD_COUNT * sizeof(uint32_t)];
    static uint8_t polygon_rom[8u * sizeof(uint32_t)];
    vf2_tgp tgp;
    uint32_t value = 0u;

    memset(tables, 0, sizeof(tables));
    memset(polygon_rom, 0, sizeof(polygon_rom));
    polygon_rom[0] = 0x11u;
    polygon_rom[1] = 0x22u;
    polygon_rom[2] = 0x33u;
    polygon_rom[3] = 0x44u;
    CHECK(vf2_tgp_initialize(
        &tgp, tables, sizeof(tables), NULL, 0u
    ) == VF2_OK);
    CHECK(vf2_tgp_attach_polygon_rom(
        &tgp, polygon_rom, sizeof(polygon_rom)
    ) == VF2_OK);
    CHECK(vf2_tgp_read_polygon_word(&tgp, 0u, &value) == VF2_OK);
    CHECK(value == UINT32_C(0x44332211));
    CHECK(vf2_tgp_read_polygon_word(&tgp, UINT32_C(8), &value) == VF2_OK);
    CHECK(value == UINT32_C(0x44332211));
    CHECK(vf2_tgp_attach_polygon_rom(
        &tgp, polygon_rom, 3u * sizeof(uint32_t)
    ) == VF2_ERROR_INVALID_ARGUMENT);
}

static void test_tgp_reference_projection(void)
{
    vf2_platform platform;
    vf2_tgp_matrix matrix;
    const vf2_tgp_vertex vertices[3] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},
        {1.0f, -1.0f, 0.0f, 1.0f},
        {-1.0f, 1.0f, 0.0f, 1.0f}
    };
    vf2_tgp_screen_vertex projected;

    CHECK(vf2_platform_initialize(&platform, 8u, 8u) == VF2_OK);
    vf2_tgp_matrix_identity(&matrix);
    CHECK(vf2_tgp_project_vertex(
        &matrix, &vertices[0], 8u, 8u, &projected
    ) == VF2_OK);
    CHECK(projected.x == 0 && projected.y == 7);
    CHECK(vf2_platform_begin_frame(&platform, 0u) == VF2_OK);
    CHECK(vf2_tgp_render_triangle(
        &platform, &matrix, vertices, UINT32_C(0x12345678)
    ) == VF2_OK);
    CHECK(platform.pixels[0] == UINT32_C(0x12345678));
    {
        const vf2_tgp_vertex nearer[3] = {
            {-1.0f, -1.0f, -0.5f, 1.0f},
            {1.0f, -1.0f, -0.5f, 1.0f},
            {-1.0f, 1.0f, -0.5f, 1.0f}
        };
        CHECK(vf2_tgp_render_triangle(
            &platform, &matrix, nearer, UINT32_C(0xaabbccdd)
        ) == VF2_OK);
        CHECK(platform.pixels[0] == UINT32_C(0xaabbccdd));
    }
    CHECK(vf2_tgp_project_vertex(
        &matrix,
        &(vf2_tgp_vertex){0.0f, 0.0f, 0.0f, 0.0f},
        8u,
        8u,
        &projected
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(vf2_platform_end_frame(&platform) == VF2_OK);
    vf2_platform_shutdown(&platform);
}

static void test_tgp_geometry_stream_framing(void)
{
    const uint32_t stream[] = {
        UINT32_C(0x00800000),
        UINT32_C(0x00100000), UINT32_C(0x00200000),
        UINT32_C(0x00800000), UINT32_C(0x00000002),
        UINT32_C(0x01000000),
        UINT32_C(0x00300000), UINT32_C(0x00400000),
        UINT32_C(0x00000001), UINT32_C(0x00000002),
        UINT32_C(0x00000003), UINT32_C(0x00000004),
        UINT32_C(0x00000005), UINT32_C(0x00000006),
        UINT32_C(0x00000000),
        UINT32_C(0x02800000),
        UINT32_C(0x00000000), UINT32_C(0x00000002),
        UINT32_C(0x000000aa), UINT32_C(0x000000bb),
        UINT32_C(0x07800000)
    };
    vf2_tgp_geometry_stream_report report;
    size_t command_words = 0u;

    CHECK(vf2_tgp_geometry_command_words(
        stream, sizeof(stream) / sizeof(stream[0]), &command_words
    ) == VF2_OK);
    CHECK(command_words == 5u);
    CHECK(vf2_tgp_scan_geometry_stream(
        stream, sizeof(stream) / sizeof(stream[0]), &report
    ) == VF2_OK);
    CHECK(report.ended != 0);
    CHECK(report.words_consumed == sizeof(stream) / sizeof(stream[0]));
    CHECK(report.commands == 4u);
    CHECK(report.object_commands == 1u);
    CHECK(report.direct_commands == 1u);
    CHECK(report.polygon_data_commands == 1u);
    CHECK(report.polygon_data_words == 2u);
    CHECK(vf2_tgp_geometry_command_words(
        stream, 2u, &command_words
    ) == VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(vf2_tgp_geometry_command_words(
        (const uint32_t[]){UINT32_C(0x0f800000)}, 1u, &command_words
    ) == VF2_OK);
    CHECK(vf2_tgp_geometry_command_words(
        (const uint32_t[]){UINT32_C(0x03800000)}, 1u, &command_words
    ) == VF2_ERROR_OUT_OF_BOUNDS);
}

static void test_tgp_geometry_execution(void)
{
    static uint8_t tables[VF2_TGP_TABLE_WORD_COUNT * sizeof(uint32_t)];
    vf2_tgp tgp;
    vf2_platform platform;
    vf2_tgp_geometry_stream_report report;
    uint32_t direct_stream[] = {
        UINT32_C(0x01000000), UINT32_C(0), UINT32_C(0),
        0u, 0u, 0u, 0u, 0u, 0u,
        UINT32_C(2), UINT32_C(0), UINT32_C(0),
        0u, 0u, 0u, UINT32_C(0),
        UINT32_C(0x07800000)
    };
    uint32_t object_stream[2u + 3u + 11u + 5u + 1u];
    size_t offset = 0u;

    memset(tables, 0, sizeof(tables));
    CHECK(vf2_tgp_initialize(
        &tgp, tables, sizeof(tables), NULL, 0u
    ) == VF2_OK);
    CHECK(vf2_platform_initialize(&platform, 16u, 16u) == VF2_OK);
    CHECK(vf2_platform_begin_frame(&platform, 0u) == VF2_OK);
    ((uint32_t *)direct_stream)[3] = float_bits(-1.0f);
    ((uint32_t *)direct_stream)[4] = float_bits(-1.0f);
    ((uint32_t *)direct_stream)[5] = float_bits(0.0f);
    ((uint32_t *)direct_stream)[6] = float_bits(1.0f);
    ((uint32_t *)direct_stream)[7] = float_bits(-1.0f);
    ((uint32_t *)direct_stream)[8] = float_bits(0.0f);
    direct_stream[12] = float_bits(-1.0f);
    direct_stream[13] = float_bits(1.0f);
    direct_stream[14] = float_bits(0.0f);
    CHECK(vf2_tgp_execute_geometry_stream(
        &tgp,
        direct_stream,
        sizeof(direct_stream) / sizeof(direct_stream[0]),
        &platform,
        UINT32_C(0x12345678),
        &report
    ) == VF2_OK);
    CHECK(report.ended != 0);
    CHECK(report.commands == 2u);
    CHECK(report.direct_commands == 1u);
    CHECK(report.object_links == 1u);
    CHECK(report.rendered_triangles == 1u);
    CHECK(platform.pixels[0] == UINT32_C(0x12345678));
    CHECK(vf2_platform_end_frame(&platform) == VF2_OK);

    memset(object_stream, 0, sizeof(object_stream));
    object_stream[offset++] = UINT32_C(0x03800000);
    object_stream[offset++] = UINT32_C(2);
    object_stream[offset++] = UINT32_C(0x02800000);
    object_stream[offset++] = UINT32_C(0);
    object_stream[offset++] = UINT32_C(11);
    object_stream[offset++] = float_bits(-1.0f);
    object_stream[offset++] = float_bits(-1.0f);
    object_stream[offset++] = float_bits(0.0f);
    object_stream[offset++] = float_bits(1.0f);
    object_stream[offset++] = float_bits(-1.0f);
    object_stream[offset++] = float_bits(0.0f);
    object_stream[offset++] = UINT32_C(2);
    object_stream[offset++] = float_bits(-1.0f);
    object_stream[offset++] = float_bits(1.0f);
    object_stream[offset++] = float_bits(0.0f);
    object_stream[offset++] = UINT32_C(0);
    object_stream[offset++] = UINT32_C(0x00800000);
    object_stream[offset++] = UINT32_C(0);
    object_stream[offset++] = UINT32_C(0);
    object_stream[offset++] = UINT32_C(0);
    object_stream[offset++] = UINT32_C(1);
    object_stream[offset++] = UINT32_C(0x07800000);
    CHECK(offset == sizeof(object_stream) / sizeof(object_stream[0]));
    CHECK(vf2_platform_begin_frame(&platform, 0u) == VF2_OK);
    CHECK(vf2_tgp_execute_geometry_stream(
        &tgp,
        object_stream,
        sizeof(object_stream) / sizeof(object_stream[0]),
        &platform,
        UINT32_C(0xaabbccdd),
        &report
    ) == VF2_OK);
    CHECK(report.ended != 0);
    CHECK(report.object_commands == 1u);
    CHECK(report.polygon_data_commands == 1u);
    CHECK(report.object_links == 1u);
    CHECK(report.rendered_triangles == 1u);
    CHECK(platform.pixels[0] == UINT32_C(0xaabbccdd));
    CHECK(vf2_platform_end_frame(&platform) == VF2_OK);
    vf2_platform_shutdown(&platform);
}

int main(void)
{
    test_tgp_tables_and_scalar_services();
    test_tgp_fifos_and_banked_memory();
    test_tgp_polygon_rom_window();
    test_tgp_reference_projection();
    test_tgp_geometry_stream_framing();
    test_tgp_geometry_execution();
    if (failures != 0) {
        fprintf(stderr, "%d TGP test(s) failed\n", failures);
        return 1;
    }
    puts("TGP tests passed");
    return 0;
}
