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

old = '''        } else if (!entry_path) {
            status = vf2_model2a_read(machine, UINT32_C(0x0053010c), &mode, sizeof(mode));
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 9u)) != 0u) { mode=UINT8_C(1); status=vf2_model2a_write(machine,UINT32_C(0x0053010c),&mode,1); }
            if (status == VF2_OK && (input_flags & (UINT32_C(1) << 8u)) != 0u) { mode=0u; status=vf2_model2a_write(machine,UINT32_C(0x0053010c),&mode,1); }
            if (status == VF2_OK && mode == 0u) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00530110),-0.01f);
                if (status==VF2_OK && (input_flags&(UINT32_C(1)<<14u))!=0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00530110),0.01f);
                if (status==VF2_OK && (input_flags&(UINT32_C(1)<<12u))!=0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00530118),-0.01f);
                if (status==VF2_OK && (input_flags&(UINT32_C(1)<<13u))!=0u) status=phase17_zero_adjust_float(machine,UINT32_C(0x00530118),0.01f);
            } else if (status == VF2_OK && (input_flags & (UINT32_C(1) << 9u)) != 0u) {
                if ((input_flags&(UINT32_C(1)<<15u))!=0u) { status=read_u16(machine,UINT32_C(0x00530120),&short_value); if(status==VF2_OK) status=write_u16(machine,UINT32_C(0x00530120),(uint16_t)(short_value-UINT16_C(0x200))); }
                if (status==VF2_OK && (input_flags&(UINT32_C(1)<<14u))!=0u) { status=read_u16(machine,UINT32_C(0x00530120),&short_value); if(status==VF2_OK) status=write_u16(machine,UINT32_C(0x00530120),(uint16_t)(short_value+UINT16_C(0x200))); }
                if (status==VF2_OK && (input_flags&(UINT32_C(1)<<13u))!=0u) { status=read_u16(machine,UINT32_C(0x0053011c),&short_value); if(status==VF2_OK) status=write_u16(machine,UINT32_C(0x0053011c),(uint16_t)(short_value-UINT16_C(0x200))); }
                if (status==VF2_OK && (input_flags&(UINT32_C(1)<<12u))!=0u) { status=read_u16(machine,UINT32_C(0x0053011c),&short_value); if(status==VF2_OK) status=write_u16(machine,UINT32_C(0x0053011c),(uint16_t)(short_value+UINT16_C(0x200))); }
            }
        }
'''
new = '''        } else if (!entry_path) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
            );
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 9u)) != 0u) {
                mode = UINT8_C(1);
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
                );
            }
            if (status == VF2_OK &&
                (navigation_flags & (UINT32_C(1) << 8u)) != 0u) {
                mode = 0u;
                status = vf2_model2a_write(
                    machine, UINT32_C(0x0053010c), &mode, sizeof(mode)
                );
            }
            if (status == VF2_OK &&
                (input_flags & (UINT32_C(1) << 9u)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x00530120), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x00530120),
                            (uint16_t)(short_value - UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x00530120), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x00530120),
                            (uint16_t)(short_value + UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x0053011c), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x0053011c),
                            (uint16_t)(short_value - UINT16_C(0x200))
                        );
                    }
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = read_u16(
                        machine, UINT32_C(0x0053011c), &short_value
                    );
                    if (status == VF2_OK) {
                        status = write_u16(
                            machine, UINT32_C(0x0053011c),
                            (uint16_t)(short_value + UINT16_C(0x200))
                        );
                    }
                }
            } else if (status == VF2_OK &&
                       (input_flags & (UINT32_C(1) << 10u)) != 0u) {
                if ((input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530114), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530114), 0.01f
                    );
                }
            } else if (status == VF2_OK) {
                if ((input_flags & (UINT32_C(1) << 15u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530110), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 14u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530110), 0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 12u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530118), -0.01f
                    );
                }
                if (status == VF2_OK &&
                    (input_flags & (UINT32_C(1) << 13u)) != 0u) {
                    status = phase17_zero_adjust_float(
                        machine, UINT32_C(0x00530118), 0.01f
                    );
                }
            }
        }
'''
assert text.count(old) == 1
text = text.replace(old, new, 1)

path.write_text(text)
