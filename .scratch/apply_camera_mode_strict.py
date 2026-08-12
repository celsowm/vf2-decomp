from pathlib import Path

src_path = Path("src/recovered/texture_bridge_match.c")
src = src_path.read_text()

anchor = "static vf2_status phase17_zero_render_index4_entry(\n"
helpers = r'''static vf2_status phase17_zero_render_index4_selector(
    vf2_model2a *machine,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint8_t selector = 0u;
    uint32_t source = 0u;
    vf2_status status = fill_tile_plane_spaces(
        machine, UINT32_C(0x01000298), UINT32_C(52), UINT32_C(1)
    );

    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, UINT32_C(0x00508040), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100029c), UINT32_C(0x000570e4)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x000570f8), UINT32_C(0x0100028a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read(
            machine, UINT32_C(0x00508040), &selector, sizeof(selector)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010002aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010002aa);
    }
    return status;
}

static vf2_status phase17_zero_render_index4_control(
    vf2_model2a *machine,
    uint32_t control,
    uint8_t selector,
    uint32_t *final_g0,
    uint32_t *final_g9
)
{
    int32_t value = 0;
    uint32_t source = 0u;
    vf2_status status = vf2_model2a_write(
        machine, control + UINT32_C(0x40), &selector, sizeof(selector)
    );

    if (status == VF2_OK) {
        status = fill_tile_plane_spaces(
            machine, UINT32_C(0x01000398), UINT32_C(52), UINT32_C(1)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_read_s8(
            machine, control + UINT32_C(0x40), &value
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_render_decimal_inline(
            machine, value, UINT32_C(0x0100039c), UINT32_C(0x0005714c)
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, UINT32_C(0x00057160), UINT32_C(0x0100038a)
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine,
            UINT32_C(0x00058d78) + (uint32_t)selector * UINT32_C(4),
            &source
        );
    }
    if (status == VF2_OK) {
        status = phase17_zero_copy_text(
            machine, source, UINT32_C(0x010003aa)
        );
    }
    if (status == VF2_OK) {
        *final_g0 = source;
        *final_g9 = UINT32_C(0x010003aa);
    }
    return status;
}

'''
assert src.count(anchor) == 1
src = src.replace(anchor, helpers + anchor, 1)

old = '''        if (menu_index == UINT8_C(4)) {
            if (effective_input_flags != 0u ||
                effective_previous_flags != 0u || navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
        } else {
'''
new = '''        if (menu_index == UINT8_C(4)) {
            uint8_t selector = 0u;
            uint32_t final_g0 = 0u;
            uint32_t final_g9 = 0u;
            uint64_t active_nested_calls = UINT64_C(2);
            uint32_t active_depth_delta = UINT32_C(2);
            const uint32_t supported_input =
                (UINT32_C(1) << 12u) | (UINT32_C(1) << 13u);
            const uint32_t supported_navigation =
                (UINT32_C(1) << 8u) | (UINT32_C(1) << 12u) |
                (UINT32_C(1) << 13u);

            if ((effective_input_flags & ~supported_input) != 0u ||
                effective_previous_flags != effective_input_flags ||
                (navigation_flags & ~supported_navigation) != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags != 0u) {
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 12u)) != 0u) {
                selector = (uint8_t)(selector + UINT8_C(1));
                if (selector >= UINT8_C(17)) {
                    selector = 0u;
                }
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(555) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            } else if (status == VF2_OK &&
                       (navigation_flags & (UINT32_C(1) << 13u)) != 0u) {
                selector = selector == 0u
                    ? UINT8_C(16)
                    : (uint8_t)(selector - UINT8_C(1));
                status = vf2_model2a_write(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(500) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            } else if (status == VF2_OK &&
                       (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                status = phase17_zero_render_index4_control(
                    machine, control, selector, &final_g0, &final_g9
                );
                expected_instructions =
                    UINT64_C(471) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
            }
            if (status != VF2_OK) {
                return status;
            }
            if (navigation_flags != 0u) {
                cpu->registers[VF2_I960_G0_REGISTER] = final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = final_g9;
                set_equal_condition(cpu);
            }
            account_nested_procedure(
                cpu, active_nested_calls, active_nested_calls
            );
            if (cpu->maximum_local_frame_depth <
                cpu->local_frame_depth + active_depth_delta) {
                cpu->maximum_local_frame_depth =
                    cpu->local_frame_depth + active_depth_delta;
            }
        } else {
'''
assert src.count(old) == 1
src = src.replace(old, new, 1)

old = '''        account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
        if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
            cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
        }
'''
new = '''        if (menu_index != UINT8_C(4)) {
            account_nested_procedure(cpu, UINT64_C(2), UINT64_C(2));
            if (cpu->maximum_local_frame_depth < cpu->local_frame_depth + 2u) {
                cpu->maximum_local_frame_depth = cpu->local_frame_depth + 2u;
            }
        }
'''
assert src.count(old) == 1
src = src.replace(old, new, 1)
src_path.write_text(src)

test_path = Path("tests/recovered/test_phase17_zero.c")
test = test_path.read_text()
needle = '''        IDLE_CASE("index4-camera-mode", 4, 0, 0, 0, 0, 37, 2, 3, 3),
'''
addition = '''        IDLE_CASE("index4-camera-mode", 4, 0, 0, 0, 0, 37, 2, 3, 3),
        IDLE_CASE("index4-copy-selector", 4, 0, (1u << 8u),
                  0, 0, 471, 7, 8, 4),
        IDLE_CASE("index4-selector-next", 4, 0, (1u << 12u),
                  0, 0, 555, 7, 8, 4),
        IDLE_CASE("index4-selector-prev", 4, 0, (1u << 13u),
                  0, 0, 500, 7, 8, 4),
        IDLE_CASE("index4-copy-next-priority", 4, 0,
                  (1u << 8u) | (1u << 12u), 0, 0, 555, 7, 8, 4),
        IDLE_CASE("index4-copy-prev-priority", 4, 0,
                  (1u << 8u) | (1u << 13u), 0, 0, 500, 7, 8, 4),
        IDLE_CASE("index4-next-prev-priority", 4, 0,
                  (1u << 12u) | (1u << 13u), 0, 0, 555, 7, 8, 4),
        IDLE_CASE("index4-all-priority", 4, 0,
                  (1u << 8u) | (1u << 12u) | (1u << 13u),
                  0, 0, 555, 7, 8, 4),
        IDLE_CASE("index4-input12-noop", 4, (1u << 12u), 0,
                  0, 0, 37, 2, 3, 3),
        IDLE_CASE("index4-input13-noop", 4, (1u << 13u), 0,
                  0, 0, 37, 2, 3, 3),
'''
assert test.count(needle) == 1
test = test.replace(needle, addition, 1)
test_path.write_text(test)
