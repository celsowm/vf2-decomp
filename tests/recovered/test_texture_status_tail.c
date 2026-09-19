#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"

#define STATUS_TAIL_ENTRY UINT32_C(0x0004d25c)
#define STATUS_SELECTOR UINT32_C(0x0050002b)
#define SPECIAL_SOURCE UINT32_C(0x0004d28c)
#define COMMON_SOURCE UINT32_C(0x0004d2ac)
#define SPECIAL_DESTINATION UINT32_C(0x010040e2)
#define COMMON_DESTINATION UINT32_C(0x010000e2)
#define RETURN_SENTINEL UINT32_C(0x00012340)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void seed_inline_text(
    uint8_t *rom,
    uint32_t source,
    const char text[13]
)
{
    memcpy(rom + source, text, 12u);
    memset(rom + source + 12u, 0, 4u);
}

static void check_rendered_text(
    const vf2_model2a *machine,
    uint32_t destination,
    const char text[13]
)
{
    uint8_t rendered[24];
    size_t index = 0u;

    memset(rendered, 0, sizeof(rendered));
    CHECK(
        vf2_model2a_read(
            machine, destination, rendered, sizeof(rendered)
        ) == VF2_OK
    );
    for (index = 0u; index < 12u; ++index) {
        CHECK(rendered[index * 2u] == (uint8_t)text[index]);
        CHECK(rendered[index * 2u + 1u] == UINT8_C(0x80));
    }
}

static vf2_hybrid_bridge_report run_selector_case(uint8_t selector)
{
    static const char special_text[13] = "SPECIAL-LINE";
    static const char common_text[13] = "COMMON-LINE!";
    vf2_hybrid_bridge_report report;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *rom = NULL;

    memset(&report, 0, sizeof(report));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));

    rom = (uint8_t *)calloc(VF2_MAIN_ROM_SIZE, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        return report;
    }
    seed_inline_text(rom, SPECIAL_SOURCE, special_text);
    seed_inline_text(rom, COMMON_SOURCE, common_text);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL || machine.tile_ram == NULL) {
        free(rom);
        return report;
    }
    CHECK(
        vf2_model2a_attach_main_rom(
            &machine, rom, VF2_MAIN_ROM_SIZE
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, STATUS_SELECTOR, &selector, sizeof(selector)
        ) == VF2_OK
    );

    vf2_i960_cpu_reset(&cpu, 0u, 0u, STATUS_TAIL_ENTRY);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu, STATUS_TAIL_ENTRY, RETURN_SENTINEL
        ) == VF2_OK
    );
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(
            &machine, &cpu, &report
        ) == VF2_OK
    );

    CHECK(cpu.ip == RETURN_SENTINEL);
    CHECK(cpu.local_frame_depth == 0u);
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL);
    CHECK(report.entry_address == STATUS_TAIL_ENTRY);
    CHECK(report.exit_address == RETURN_SENTINEL);
    CHECK(report.iterations == UINT64_C(2));
    CHECK(report.rows == UINT64_C(24));
    CHECK(report.bytes_written == 48u);
    CHECK(report.recovered_procedure_calls == UINT64_C(2));
    CHECK(report.recovered_procedure_returns == UINT64_C(3));
    CHECK(report.cpu_poststate_applied != 0);

    check_rendered_text(&machine, SPECIAL_DESTINATION, special_text);
    check_rendered_text(&machine, COMMON_DESTINATION, common_text);

    vf2_model2a_shutdown(&machine);
    free(rom);
    return report;
}


static void run_common_only_spaces(void)
{
    /* Oracle attract park: mode 0x03 blits only the common destination
     * from ROM spaces at 0x4d2ac (measured 156 steps to 0x4d2bc). */
    static const char spaces[13] = "            ";
    vf2_hybrid_bridge_report report;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *rom = NULL;
    uint8_t rendered[24];

    memset(&report, 0, sizeof(report));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    memset(rendered, 0, sizeof(rendered));

    rom = (uint8_t *)calloc(VF2_MAIN_ROM_SIZE, 1u);
    CHECK(rom != NULL);
    if (rom == NULL) {
        return;
    }
    seed_inline_text(rom, SPECIAL_SOURCE, spaces);
    seed_inline_text(rom, COMMON_SOURCE, spaces);

    CHECK(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL || machine.tile_ram == NULL) {
        free(rom);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_rom(&machine, rom, VF2_MAIN_ROM_SIZE) == VF2_OK
    );
    CHECK(
        vf2_model2a_write(
            &machine, STATUS_SELECTOR, &(uint8_t){3}, 1u
        ) == VF2_OK
    );
    vf2_i960_cpu_reset(&cpu, 0u, 0u, STATUS_TAIL_ENTRY);
    CHECK(
        vf2_i960_cpu_enter_procedure(
            &cpu, STATUS_TAIL_ENTRY, RETURN_SENTINEL
        ) == VF2_OK
    );
    CHECK(
        vf2_hybrid_post_frame_bridge_execute(&machine, &cpu, &report) == VF2_OK
    );
    CHECK(report.kind == VF2_HYBRID_BRIDGE_TEXTURE_STATUS_TAIL);
    CHECK(report.entry_address == STATUS_TAIL_ENTRY);
    CHECK(report.exit_address == RETURN_SENTINEL);
    CHECK(report.bytes_written >= 24u);
    CHECK(cpu.ip == RETURN_SENTINEL);
    CHECK(
        vf2_model2a_read(
            &machine, COMMON_DESTINATION, rendered, sizeof(rendered)
        ) == VF2_OK
    );
    CHECK(rendered[0] == UINT8_C(0x20));
    CHECK(rendered[1] == UINT8_C(0x80));
    CHECK(
        vf2_model2a_read(
            &machine, SPECIAL_DESTINATION, rendered, sizeof(rendered)
        ) == VF2_OK
    );
    /* Special plane must stay untouched on the common-only path. */
    CHECK(rendered[0] != UINT8_C(0x20) || rendered[1] != UINT8_C(0x80));

    vf2_model2a_shutdown(&machine);
    free(rom);
}


int main(void)
{
    const vf2_hybrid_bridge_report selector12 = run_selector_case(UINT8_C(12));
    const vf2_hybrid_bridge_report selector13 = run_selector_case(UINT8_C(13));

    CHECK(selector12.recovered_instruction_count != 0u);
    CHECK(
        selector13.recovered_instruction_count ==
        selector12.recovered_instruction_count + UINT64_C(2)
    );
    CHECK(
        selector13.recovered_procedure_calls ==
        selector12.recovered_procedure_calls
    );
    CHECK(
        selector13.recovered_procedure_returns ==
        selector12.recovered_procedure_returns
    );
    run_common_only_spaces();

    if (failures != 0) {
        fprintf(stderr, "%d texture-status-tail test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("texture-status-tail tests passed");
    return EXIT_SUCCESS;
}
