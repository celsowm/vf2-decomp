#define VF2_GAME_DISP_COUNTER_ENTRY UINT32_C(0x0002cae0)
#define VF2_GAME_DISP_COUNTER_RETURN UINT32_C(0x0002b22c)
#define VF2_GAME_DISP_COUNTER_VALUE UINT32_C(0x00500028)
#define VF2_GAME_DISP_RUNTIME_FLAGS UINT32_C(0x00500068)
#define VF2_GAME_DISP_RUNTIME_BASE_FLAGS UINT32_C(0x80004400)
#define VF2_GAME_DISP_RUNTIME_HALF_BIT UINT32_C(0x00100000)
#define VF2_GAME_DISP_COUNTER_MAX UINT16_C(0x18ff)
#define VF2_GAME_DISP_GLYPH_HELPER UINT32_C(0x00008fe0)
#define VF2_GAME_DISP_INTEGER_GLYPHS UINT32_C(0x02a67d84)
#define VF2_GAME_DISP_FRACTION_GLYPHS UINT32_C(0x02a67e0c)
#define VF2_GAME_DISP_INTEGER_TENS UINT32_C(0x010001b8)
#define VF2_GAME_DISP_INTEGER_UNITS UINT32_C(0x010001bc)
#define VF2_GAME_DISP_FRACTION_TENS UINT32_C(0x01000240)
#define VF2_GAME_DISP_FRACTION_UNITS UINT32_C(0x01000242)
#define VF2_GAME_DISP_COUNTER_PARENT_INSTRUCTIONS UINT64_C(37)

static int16_t game_disp_sign_extend_u16(uint16_t value)
{
    return (int16_t)value;
}

static vf2_status game_disp_read_i16(
    vf2_model2a *machine,
    uint32_t address,
    int16_t *value
)
{
    uint16_t raw = 0u;
    vf2_status status = game_disp_leaf_read_u16(machine, address, &raw);

    if (status == VF2_OK && value != NULL) {
        *value = game_disp_sign_extend_u16(raw);
    } else if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return status;
}

static vf2_status game_disp_tile_glyph_helper(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t expected_table,
    uint32_t expected_width,
    uint32_t expected_height
)
{
    const uint32_t table = cpu->registers[VF2_I960_G0_REGISTER];
    const uint32_t destination_start = cpu->registers[VF2_I960_G0_REGISTER + 9u];
    const uint32_t glyph = cpu->registers[VF2_I960_G0_REGISTER + 4u];
    const uint32_t page = cpu->registers[VF2_I960_G0_REGISTER + 5u];
    int16_t base = 0;
    int16_t marker = 0;
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t stride = 0u;
    uint32_t source = 0u;
    uint32_t destination = destination_start;
    uint32_t row = 0u;
    uint32_t column = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL || table != expected_table ||
        page != 0u || glyph > UINT32_C(9)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = game_disp_read_i16(machine, table, &base);
    if (status == VF2_OK) {
        status = game_disp_read_i16(machine, table + UINT32_C(2), &marker);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(4), &width);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(8), &height);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, table + UINT32_C(12), &stride);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (base != INT16_MIN || marker != INT16_C(1) ||
        width != expected_width || height != expected_height ||
        stride != UINT32_C(10)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    cpu->registers[VF2_I960_G0_REGISTER + 6u] =
        (uint32_t)(int32_t)base;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = width;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = height;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = stride;

    source = table + UINT32_C(16) +
        UINT32_C(2) * width * (glyph + height * stride * page);
    for (row = 0u; row < height; ++row) {
        uint32_t row_source = source;
        uint32_t row_destination = destination;

        for (column = 0u; column < width; ++column) {
            int16_t cell = 0;
            uint16_t output = 0u;

            status = game_disp_read_i16(machine, row_source, &cell);
            if (status != VF2_OK) {
                return status;
            }
            output = (uint16_t)((int32_t)cell + (int32_t)base);
            status = game_disp_leaf_write_u16(
                machine, row_destination, output
            );
            if (status != VF2_OK) {
                return status;
            }
            row_source += UINT32_C(2);
            row_destination += UINT32_C(2);
        }
        source += UINT32_C(2) * width * stride;
        destination += UINT32_C(128);
    }

    cpu->registers[VF2_I960_G0_REGISTER] = source;
    cpu->registers[VF2_I960_G0_REGISTER + 9u] = destination_start;
    cpu->executed_instructions +=
        UINT64_C(26) + (uint64_t)height *
            (UINT64_C(10) + UINT64_C(7) * (uint64_t)width);
    set_runtime_equal_condition(cpu);
    return vf2_i960_cpu_return_procedure(cpu, machine);
}
