from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()

marker = '''static vf2_status phase17_zero_render_index4_entry(
    vf2_model2a *machine,
'''
helper = '''static vf2_status phase17_zero_render_index4_selector(
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

'''
assert text.count(marker) == 1
text = text.replace(marker, helper + marker, 1)

# Only the two staged CAMERA_MODE active calls use the partial selector renderer.
needle = '''                    status = phase17_zero_render_index4_entry(
                        machine, control, &final_g0, &final_g9
                    );
'''
assert text.count(needle) >= 2
text = text.replace(
    needle,
    '''                    status = phase17_zero_render_index4_selector(
                        machine, &final_g0, &final_g9
                    );
''',
    2,
)

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
