from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

old = '''        if (effective_previous_flags != effective_input_flags ||
            navigation_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (menu_index == UINT8_C(3)) {
'''
new = '''        if (effective_previous_flags != effective_input_flags) {
            return VF2_ERROR_UNSUPPORTED;
        }
        if (menu_index == UINT8_C(3)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
'''
assert text.count(old) == 1
text = text.replace(old, new, 1)

old = '''        } else if (menu_index == UINT8_C(5)) {
            switch (effective_input_flags) {
'''
new = '''        } else if (menu_index == UINT8_C(5)) {
            if (navigation_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (effective_input_flags) {
'''
assert text.count(old) == 1
text = text.replace(old, new, 1)

old = '''        } else if (menu_index == UINT8_C(12)) {
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
        } else if (effective_input_flags != 0u) {
            return VF2_ERROR_UNSUPPORTED;
        }
'''
new = '''        } else if (menu_index == UINT8_C(9)) {
            if (effective_input_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 8u:
                control_instruction_adjustment = 8;
                break;
            case UINT32_C(1) << 9u:
                control_instruction_adjustment = 2;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
        } else if (menu_index == UINT8_C(10)) {
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(0x700):
                if (effective_input_flags != 0u) {
                    return VF2_ERROR_UNSUPPORTED;
                }
                control_instruction_adjustment = -7;
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (navigation_flags == 0u) {
                switch (effective_input_flags) {
                case 0u:
                case UINT32_C(1) << 8u:
                    break;
                case UINT32_C(1) << 9u:
                    control_instruction_adjustment = -3;
                    break;
                case UINT32_C(1) << 12u:
                case UINT32_C(1) << 13u:
                case UINT32_C(1) << 14u:
                case UINT32_C(1) << 15u:
                    control_instruction_adjustment = 5;
                    break;
                case (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u):
                    control_instruction_adjustment = 8;
                    break;
                case (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u):
                    control_instruction_adjustment = 13;
                    break;
                default:
                    return VF2_ERROR_UNSUPPORTED;
                }
            }
        } else if (menu_index == UINT8_C(12)) {
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
assert text.count(old) == 1
text = text.replace(old, new, 1)

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

            if (effective_input_flags != 0u ||
                effective_previous_flags != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
            switch (navigation_flags) {
            case 0u:
                break;
            case UINT32_C(1) << 12u:
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                selector = (uint8_t)(selector + UINT8_C(1));
                if (selector >= UINT8_C(17)) {
                    selector = 0u;
                }
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00508040), &selector,
                        sizeof(selector)
                    );
                }
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_entry(
                        machine, control, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(555) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
                break;
            case UINT32_C(1) << 13u:
                status = vf2_model2a_read(
                    machine, UINT32_C(0x00508040), &selector,
                    sizeof(selector)
                );
                selector = selector == 0u
                    ? UINT8_C(16)
                    : (uint8_t)(selector - UINT8_C(1));
                if (status == VF2_OK) {
                    status = vf2_model2a_write(
                        machine, UINT32_C(0x00508040), &selector,
                        sizeof(selector)
                    );
                }
                if (status == VF2_OK) {
                    status = phase17_zero_render_index4_entry(
                        machine, control, &final_g0, &final_g9
                    );
                }
                expected_instructions =
                    UINT64_C(500) + idle_wrapper_adjustment;
                active_nested_calls = UINT64_C(7);
                active_depth_delta = UINT32_C(3);
                break;
            default:
                return VF2_ERROR_UNSUPPORTED;
            }
            if (status != VF2_OK) {
                return status;
            }
            if (navigation_flags != 0u) {
                cpu->registers[VF2_I960_G0_REGISTER] = final_g0;
                cpu->registers[VF2_I960_G0_REGISTER + 9u] = final_g9;
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
assert text.count(old) == 1
text = text.replace(old, new, 1)

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
# This exact snippet appears in other functions too; constrain to the first occurrence after menu block.
pos = text.find('    if (menu_index == UINT8_C(4) || menu_index == UINT8_C(8) ||')
assert pos >= 0
sub = text[pos:]
assert sub.count(old) >= 1
sub = sub.replace(old, new, 1)
text = text[:pos] + sub

path.write_text(text)
