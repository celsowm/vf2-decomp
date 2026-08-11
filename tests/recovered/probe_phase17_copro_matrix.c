#define main phase17_copro_probe_original_main
#include "probe_phase17_copro.c"
#undef main

int main(int argc, char **argv)
{
    static const uint8_t screens[] = {3u, 5u, 12u};
    static const uint32_t inputs[] = {
        UINT32_C(0),
        UINT32_C(1) << 8u,
        UINT32_C(1) << 9u,
        UINT32_C(1) << 12u,
        UINT32_C(1) << 13u,
        UINT32_C(1) << 14u,
        UINT32_C(1) << 15u,
        (UINT32_C(1) << 9u) | (UINT32_C(1) << 14u),
        (UINT32_C(1) << 9u) | (UINT32_C(1) << 15u),
        (UINT32_C(1) << 8u) | (UINT32_C(1) << 12u),
        (UINT32_C(1) << 8u) | (UINT32_C(1) << 13u),
        (UINT32_C(1) << 9u) | (UINT32_C(1) << 12u),
        (UINT32_C(1) << 9u) | (UINT32_C(1) << 13u)
    };
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;
    size_t screen = 0u;
    size_t input = 0u;

    if (argc != 2) {
        fprintf(stderr, "usage: %s ROM_DIR\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_MAINCPU,
                                &main_rom, &main_rom_size) != VF2_OK ||
        vf2_romset_build_region(argv[1], VF2_REGION_MAIN_DATA,
                                &main_data, &main_data_size) != VF2_OK) {
        fprintf(stderr, "ROM load failed\n");
        free(main_data);
        free(main_rom);
        return EXIT_FAILURE;
    }

    for (screen = 0u; screen < sizeof(screens) / sizeof(screens[0]); ++screen) {
        for (input = 0u; input < sizeof(inputs) / sizeof(inputs[0]); ++input) {
            run_probe(main_rom, main_rom_size, main_data, main_data_size,
                      screens[screen], inputs[input]);
        }
    }

    free(main_data);
    free(main_rom);
    return EXIT_SUCCESS;
}
