from pathlib import Path

src_path = Path("src/recovered/texture_bridge_match.c")
src = src_path.read_text()

old = '''        if (status == VF2_OK) {
            uint8_t buffer_byte = UINT8_C(0x0c);
            uint8_t one = UINT8_C(1);
            uint8_t sixty = UINT8_C(0x60);
            status = vf2_model2a_write(machine, UINT32_C(0x005001e4), &buffer_byte, 1);
            if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x00501010), &one, 1);
            if (status == VF2_OK) status = vf2_model2a_write(machine, UINT32_C(0x0050101c), &sixty, 1);
        }
'''
new = '''        if (status == VF2_OK) {
            uint32_t buffer_index = 0u;
            uint32_t texture_x = 0u;
            uint32_t texture_y = 0u;
            uint32_t texture_z = 0u;
            uint32_t texture_z_squared = 0u;
            uint8_t one = UINT8_C(1);
            uint8_t sixty = UINT8_C(0x60);

            status = vf2_model2a_read_u32(
                machine, UINT32_C(0x005001e4), &buffer_index
            );
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00509804), &texture_x
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x00509808), &texture_y
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_read_u32(
                    machine, UINT32_C(0x0050980c), &texture_z
                );
            }
            if (status == VF2_OK) {
                const float texture_z_value =
                    phase17_zero_float_from_bits(texture_z);
                texture_z_squared = phase17_zero_float_to_bits(
                    texture_z_value * texture_z_value
                );
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index, texture_x
                );
            }
            if (status == VF2_OK) {
                buffer_index = (buffer_index + UINT32_C(4)) &
                               ~UINT32_C(0x100);
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index, texture_y
                );
            }
            if (status == VF2_OK) {
                buffer_index = (buffer_index + UINT32_C(4)) &
                               ~UINT32_C(0x100);
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x0090e000) + buffer_index,
                    texture_z_squared
                );
            }
            if (status == VF2_OK) {
                uint8_t buffer_byte = 0u;
                buffer_index += UINT32_C(4);
                buffer_byte = (uint8_t)buffer_index;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x005001e4), &buffer_byte,
                    sizeof(buffer_byte)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00501010), &one, sizeof(one)
                );
            }
            if (status == VF2_OK) {
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0050101c), &sixty, sizeof(sixty)
                );
            }
        }
'''
assert src.count(old) == 1
src = src.replace(old, new, 1)

old = '''            if (status == VF2_OK && kind == UINT16_C(1)) label_source = UINT32_C(0x0004d30c);
            if (status == VF2_OK) status = phase17_zero_copy_text(machine,label_source,UINT32_C(0x010000e2));
'''
new = '''            if (status == VF2_OK && kind == UINT16_C(1)) label_source = UINT32_C(0x0004d30c);
            if (status == VF2_OK) {
                status = vf2_model2a_write_u32(
                    machine, UINT32_C(0x00503100), texture
                );
            }
            if (status == VF2_OK) status = phase17_zero_copy_text(machine,label_source,UINT32_C(0x010000e2));
'''
assert src.count(old) == 1
src = src.replace(old, new, 1)

old = '''        } else if (menu_index == UINT8_C(12)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (effective_input_flags != 0u || navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
'''
new = '''        } else if (menu_index == UINT8_C(12)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
            case UINT32_C(1) << 13u:
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(13)) {
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 14u:
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -7;
                break;
            case UINT32_C(1) << 15u:
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = 1;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags == 0u) {
                switch (effective_input_flags) {
                case 0u:
                    break;
                case UINT32_C(1) << 16u:
                case UINT32_C(1) << 18u:
                case UINT32_C(1) << 20u:
                case UINT32_C(1) << 21u:
                case UINT32_C(1) << 22u:
                case UINT32_C(1) << 23u:
                    control_instruction_adjustment = 7;
                    break;
                default:
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        } else if (effective_input_flags != 0u || navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
'''
assert src.count(old) == 1
src = src.replace(old, new, 1)
src_path.write_text(src)

test_path = Path("tests/recovered/test_phase17_zero.c")
test = test_path.read_text()
start = test.index("typedef struct phase17_scalar_copro")
end = test.index("static int check_status", start)
protocol = r'''typedef struct phase17_copro_protocol {
    uint32_t words[4];
    size_t count;
    size_t expected_words;
    uint32_t result;
    int ready;
} phase17_copro_protocol;

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
    phase17_copro_protocol *copro = context;
    uint32_t value = 0u;

    if (copro == NULL || source == NULL || size != sizeof(value) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || copro->ready ||
        copro->count >= sizeof(copro->words) / sizeof(copro->words[0])) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(&value, source, sizeof(value));

    if (copro->count == 0u) {
        switch (value) {
        case UINT32_C(0x09801313):
        case UINT32_C(0x0a001414):
            copro->expected_words = 3u;
            break;
        case UINT32_C(0x10802121):
        case UINT32_C(0x11002222):
        case UINT32_C(0x1a003434):
            copro->expected_words = 2u;
            break;
        case UINT32_C(0x00800101):
        case UINT32_C(0x01000202):
        case UINT32_C(0x01800303):
        case UINT32_C(0x37806f6f):
            copro->expected_words = 1u;
            break;
        case UINT32_C(0x03800707):
            copro->expected_words = 4u;
            break;
        default:
            return VF2_ERROR_INVALID_ARGUMENT;
        }
    }

    copro->words[copro->count++] = value;
    if (copro->count != copro->expected_words) {
        return VF2_OK;
    }

    switch (copro->words[0]) {
    case UINT32_C(0x09801313): {
        const float left = phase17_float_from_bits(copro->words[1]);
        const float right = phase17_float_from_bits(copro->words[2]);
        copro->result = phase17_float_to_bits(left + right);
        copro->ready = 1;
        break;
    }
    case UINT32_C(0x0a001414): {
        const float left = phase17_float_from_bits(copro->words[1]);
        const float right = phase17_float_from_bits(copro->words[2]);
        copro->result = phase17_float_to_bits(left - right);
        copro->ready = 1;
        break;
    }
    case UINT32_C(0x10802121):
    case UINT32_C(0x11002222):
        if (copro->words[1] != 0u) {
            return VF2_ERROR_INVALID_ARGUMENT;
        }
        copro->result = 0u;
        copro->ready = 1;
        break;
    case UINT32_C(0x1a003434):
        copro->result = 0u;
        copro->ready = 1;
        break;
    case UINT32_C(0x00800101):
    case UINT32_C(0x01000202):
    case UINT32_C(0x01800303):
    case UINT32_C(0x37806f6f):
    case UINT32_C(0x03800707):
        copro->count = 0u;
        copro->expected_words = 0u;
        break;
    default:
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    return VF2_OK;
}

static vf2_status phase17_copro_read(
    void *context,
    uint32_t address,
    void *destination,
    size_t size
)
{
    phase17_copro_protocol *copro = context;

    if (copro == NULL || destination == NULL ||
        size != sizeof(copro->result) ||
        address < UINT32_C(0x00884000) ||
        address >= UINT32_C(0x00888000) || !copro->ready) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memcpy(destination, &copro->result, sizeof(copro->result));
    copro->count = 0u;
    copro->expected_words = 0u;
    copro->ready = 0;
    return VF2_OK;
}

'''
test = test[:start] + protocol + test[end:]
test = test.replace("phase17_scalar_copro reference_copro;", "phase17_copro_protocol reference_copro;", 1)
test = test.replace("phase17_scalar_copro native_copro;", "phase17_copro_protocol native_copro;", 1)
old = '''    const int use_copro_oracle =
        test_case->runtime_flags == 0u &&
        test_case->navigation_flags == 0u &&
        test_case->previous_flags == test_case->input_flags &&
        test_case->menu_state == UINT8_C(0x40) &&
        (test_case->menu_index == UINT8_C(3) ||
         test_case->menu_index == UINT8_C(5) ||
         test_case->menu_index == UINT8_C(10) ||
         test_case->menu_index == UINT8_C(12));
'''
new = '''    const int use_copro_oracle =
        test_case->runtime_flags == 0u &&
        test_case->previous_flags == test_case->input_flags &&
        test_case->menu_state == UINT8_C(0x40) &&
        ((test_case->navigation_flags == 0u &&
          (test_case->menu_index == UINT8_C(3) ||
           test_case->menu_index == UINT8_C(5) ||
           test_case->menu_index == UINT8_C(10) ||
           test_case->menu_index == UINT8_C(12))) ||
         test_case->menu_index == UINT8_C(13));
'''
assert test.count(old) == 1
test = test.replace(old, new, 1)
needle = '''        IDLE_CASE("index13-texture", 13, 0, 0, 0, 0, 1745, 16, 17, 5),
'''
addition = '''        IDLE_CASE("index13-texture", 13, 0, 0, 0, 0, 1745, 16, 17, 5),
        IDLE_CASE("index13-texture-next", 13, 0, (1u << 14u),
                  0, 0, 1738, 16, 17, 5),
        IDLE_CASE("index13-texture-prev", 13, 0, (1u << 15u),
                  0, 0, 1746, 16, 17, 5),
        IDLE_CASE("index13-z-inc", 13, (1u << 16u), 0,
                  0, 0, 1752, 16, 17, 5),
        IDLE_CASE("index13-z-dec", 13, (1u << 18u), 0,
                  0, 0, 1752, 16, 17, 5),
        IDLE_CASE("index13-y-dec", 13, (1u << 20u), 0,
                  0, 0, 1752, 16, 17, 5),
        IDLE_CASE("index13-y-inc", 13, (1u << 21u), 0,
                  0, 0, 1752, 16, 17, 5),
        IDLE_CASE("index13-x-inc", 13, (1u << 22u), 0,
                  0, 0, 1752, 16, 17, 5),
        IDLE_CASE("index13-x-dec", 13, (1u << 23u), 0,
                  0, 0, 1752, 16, 17, 5),
'''
assert test.count(needle) == 1
test = test.replace(needle, addition, 1)
test_path.write_text(test)
