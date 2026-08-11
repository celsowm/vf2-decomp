from pathlib import Path

path = Path("tests/recovered/test_phase17_zero.c")
text = path.read_text()

oracle = r'''typedef struct phase17_scalar_copro {
    uint32_t words[3];
    size_t count;
    uint32_t result;
    int ready;
} phase17_scalar_copro;

static float phase17_float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t phase17_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static vf2_status phase17_copro_write(
    void *context,
    uint32_t address,
    const void *source,
    size_t size
)
{
    phase17_scalar_copro *copro = context;
    uint32_t value = 0u;
    float left = 0.0f;
    float right = 0.0f;

    if (copro == NULL || source == NULL || size != sizeof(value) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || copro->count >= 3u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(&value, source, sizeof(value));
    copro->words[copro->count++] = value;
    if (copro->count != 3u) {
        return VF2_OK;
    }

    left = phase17_float_from_bits(copro->words[1]);
    right = phase17_float_from_bits(copro->words[2]);
    switch (copro->words[0]) {
    case UINT32_C(0x09801313):
        copro->result = phase17_float_to_bits(left + right);
        break;
    case UINT32_C(0x0a001414):
        copro->result = phase17_float_to_bits(left - right);
        break;
    default:
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    copro->ready = 1;
    return VF2_OK;
}

static vf2_status phase17_copro_read(
    void *context,
    uint32_t address,
    void *destination,
    size_t size
)
{
    phase17_scalar_copro *copro = context;

    if (copro == NULL || destination == NULL ||
        size != sizeof(copro->result) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || !copro->ready) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(destination, &copro->result, sizeof(copro->result));
    copro->count = 0u;
    copro->ready = 0;
    return VF2_OK;
}

'''

replacements = [
    (
        "} phase17_zero_case;\n\nstatic int check_status",
        "} phase17_zero_case;\n\n" + oracle + "static int check_status",
    ),
    (
        "    vf2_i960_snapshot_diff diff;\n"
        "    vf2_status reference_status = VF2_OK;\n"
        "    vf2_status native_status = VF2_OK;\n"
        "    vf2_status compare_status = VF2_OK;\n",
        "    vf2_i960_snapshot_diff diff;\n"
        "    phase17_scalar_copro reference_copro;\n"
        "    phase17_scalar_copro native_copro;\n"
        "    vf2_status reference_status = VF2_OK;\n"
        "    vf2_status native_status = VF2_OK;\n"
        "    vf2_status compare_status = VF2_OK;\n"
        "    const int use_copro_oracle =\n"
        "        test_case->runtime_flags == 0u &&\n"
        "        test_case->navigation_flags == 0u &&\n"
        "        test_case->previous_flags == test_case->input_flags &&\n"
        "        test_case->menu_state == UINT8_C(0x40) &&\n"
        "        (test_case->menu_index == UINT8_C(3) ||\n"
        "         test_case->menu_index == UINT8_C(5) ||\n"
        "         test_case->menu_index == UINT8_C(12));\n",
    ),
    (
        "    memset(&reference_machine, 0, sizeof(reference_machine));\n"
        "    memset(&native_machine, 0, sizeof(native_machine));\n",
        "    memset(&reference_machine, 0, sizeof(reference_machine));\n"
        "    memset(&native_machine, 0, sizeof(native_machine));\n"
        "    memset(&reference_copro, 0, sizeof(reference_copro));\n"
        "    memset(&native_copro, 0, sizeof(native_copro));\n",
    ),
    (
        "    CHECK(vf2_model2a_attach_main_data(\n"
        "              &native_machine, main_data, main_data_size) == VF2_OK);\n"
        "    CHECK(check_status(initialize_phase17_zero_state(\n",
        "    CHECK(vf2_model2a_attach_main_data(\n"
        "              &native_machine, main_data, main_data_size) == VF2_OK);\n"
        "    if (use_copro_oracle) {\n"
        "        CHECK(vf2_model2a_set_copro_callbacks(\n"
        "                  &reference_machine, phase17_copro_read,\n"
        "                  phase17_copro_write, &reference_copro) == VF2_OK);\n"
        "        CHECK(vf2_model2a_set_copro_callbacks(\n"
        "                  &native_machine, phase17_copro_read,\n"
        "                  phase17_copro_write, &native_copro) == VF2_OK);\n"
        "    }\n"
        "    CHECK(check_status(initialize_phase17_zero_state(\n",
    ),
    (
        "    CHECK(enter_frame_dispatch(&reference_cpu) == VF2_OK);\n"
        "    CHECK(enter_frame_dispatch(&native_cpu) == VF2_OK);\n\n"
        "    options.stop_address",
        "    CHECK(enter_frame_dispatch(&reference_cpu) == VF2_OK);\n"
        "    CHECK(enter_frame_dispatch(&native_cpu) == VF2_OK);\n"
        "    if (use_copro_oracle) {\n"
        "        reference_cpu.registers[VF2_I960_G0_REGISTER + 11u] =\n"
        "            UINT32_C(0x00884000);\n"
        "        reference_cpu.registers[VF2_I960_G0_REGISTER + 12u] = 0u;\n"
        "        native_cpu.registers[VF2_I960_G0_REGISTER + 11u] =\n"
        "            UINT32_C(0x00884000);\n"
        "        native_cpu.registers[VF2_I960_G0_REGISTER + 12u] = 0u;\n"
        "    }\n\n"
        "    options.stop_address",
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected one source match, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)

case_blocks = [
    (
        '        IDLE_CASE("index3-robot-position", 3, 0, 0, 0, 0, 695, 12, 13, 4),\n',
        '''        IDLE_CASE("index3-robot-position", 3, 0, 0, 0, 0, 695, 12, 13, 4),
        IDLE_CASE("index3-angle-mode", 3, (1u << 9u), 0, 0, 0,
                  688, 12, 13, 4),
        IDLE_CASE("index3-second-float-dec", 3, (1u << 12u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-second-float-inc", 3, (1u << 13u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-first-float-inc", 3, (1u << 14u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-first-float-dec", 3, (1u << 15u), 0, 0, 0,
                  700, 12, 13, 4),
        IDLE_CASE("index3-angle-ignore-dec", 3,
                  (1u << 9u) | (1u << 12u), 0, 0, 0,
                  688, 12, 13, 4),
        IDLE_CASE("index3-angle-ignore-inc", 3,
                  (1u << 9u) | (1u << 13u), 0, 0, 0,
                  688, 12, 13, 4),
        IDLE_CASE("index3-angle-inc", 3,
                  (1u << 9u) | (1u << 14u), 0, 0, 0,
                  699, 12, 13, 4),
        IDLE_CASE("index3-angle-dec", 3,
                  (1u << 9u) | (1u << 15u), 0, 0, 0,
                  704, 12, 13, 4),
''',
    ),
    (
        '        IDLE_CASE("index5-camera-position", 5, 0, 0, 0, 0, 695, 13, 14, 4),\n',
        '''        IDLE_CASE("index5-camera-position", 5, 0, 0, 0, 0, 695, 13, 14, 4),
        IDLE_CASE("index5-select-y", 5, (1u << 8u), 0, 0, 0,
                  694, 13, 14, 4),
        IDLE_CASE("index5-step-tenth", 5, (1u << 9u), 0, 0, 0,
                  697, 13, 14, 4),
        IDLE_CASE("index5-second-dec", 5, (1u << 12u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-second-inc", 5, (1u << 13u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-first-inc", 5, (1u << 14u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-first-dec", 5, (1u << 15u), 0, 0, 0,
                  700, 13, 14, 4),
        IDLE_CASE("index5-y-dec", 5, (1u << 8u) | (1u << 12u), 0, 0, 0,
                  699, 13, 14, 4),
        IDLE_CASE("index5-y-inc", 5, (1u << 8u) | (1u << 13u), 0, 0, 0,
                  699, 13, 14, 4),
        IDLE_CASE("index5-tenth-second-dec", 5,
                  (1u << 9u) | (1u << 12u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index5-tenth-second-inc", 5,
                  (1u << 9u) | (1u << 13u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index5-tenth-first-inc", 5,
                  (1u << 9u) | (1u << 14u), 0, 0, 0,
                  702, 13, 14, 4),
        IDLE_CASE("index5-tenth-first-dec", 5,
                  (1u << 9u) | (1u << 15u), 0, 0, 0,
                  702, 13, 14, 4),
''',
    ),
    (
        '        IDLE_CASE("index12-camera-xang", 12, 0, 0, 0, 0, 118, 4, 5, 4),\n',
        '''        IDLE_CASE("index12-camera-xang", 12, 0, 0, 0, 0, 118, 4, 5, 4),
        IDLE_CASE("index12-camera-xang-dec", 12, (1u << 12u), 0, 0, 0,
                  119, 4, 5, 4),
        IDLE_CASE("index12-camera-xang-inc", 12, (1u << 13u), 0, 0, 0,
                  119, 4, 5, 4),
''',
    ),
]

for old, new in case_blocks:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected one case anchor, found {count}: {old!r}")
    text = text.replace(old, new, 1)

path.write_text(text)
