from pathlib import Path

path = Path("tests/recovered/test_phase17_zero.c")
text = path.read_text()

old = '''        (test_case->menu_index == UINT8_C(3) ||
         test_case->menu_index == UINT8_C(5) ||
         test_case->menu_index == UINT8_C(12));
'''
new = '''        (test_case->menu_index == UINT8_C(3) ||
         test_case->menu_index == UINT8_C(5) ||
         test_case->menu_index == UINT8_C(10) ||
         test_case->menu_index == UINT8_C(12));
'''
assert text.count(old) == 1
text = text.replace(old, new, 1)

old = '''        IDLE_CASE("index9-material", 9, 0, 0, 0, 0, 1282, 17, 18, 4),
        IDLE_CASE("index10-polygon", 10, 0, 0, 0, 0, 1154, 18, 19, 4),
'''
new = '''        IDLE_CASE("index9-material", 9, 0, 0, 0, 0, 1282, 17, 18, 4),
        IDLE_CASE("index9-material-prev", 9, 0, (1u << 8u), 0, 0,
                  1290, 17, 18, 4),
        IDLE_CASE("index9-material-next", 9, 0, (1u << 9u), 0, 0,
                  1284, 17, 18, 4),
        IDLE_CASE("index10-polygon", 10, 0, 0, 0, 0, 1154, 18, 19, 4),
        IDLE_CASE("index10-input-bit8", 10, (1u << 8u), 0, 0, 0,
                  1154, 18, 19, 4),
        IDLE_CASE("index10-angle-select", 10, (1u << 9u), 0, 0, 0,
                  1151, 18, 19, 4),
        IDLE_CASE("index10-float2-dec", 10, (1u << 12u), 0, 0, 0,
                  1159, 18, 19, 4),
        IDLE_CASE("index10-float2-inc", 10, (1u << 13u), 0, 0, 0,
                  1159, 18, 19, 4),
        IDLE_CASE("index10-float1-inc", 10, (1u << 14u), 0, 0, 0,
                  1159, 18, 19, 4),
        IDLE_CASE("index10-float1-dec", 10, (1u << 15u), 0, 0, 0,
                  1159, 18, 19, 4),
        IDLE_CASE("index10-angle-inc", 10,
                  (1u << 9u) | (1u << 14u), 0, 0, 0,
                  1162, 18, 19, 4),
        IDLE_CASE("index10-angle-dec", 10,
                  (1u << 9u) | (1u << 15u), 0, 0, 0,
                  1167, 18, 19, 4),
        IDLE_CASE("index10-reset", 10, 0, UINT32_C(0x700), 0, 0,
                  1147, 18, 19, 4),
'''
assert text.count(old) == 1
text = text.replace(old, new, 1)

path.write_text(text)
