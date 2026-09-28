#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/recovered.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define POL_TEST_ENTRY UINT32_C(0x00021a00)
#define POL_TEST_PATH_A_RET UINT32_C(0x00021b00)
#define POL_TEST_PALETTE_ENTRY UINT32_C(0x00007f24)
#define POL_TEST_SUBMIT_ENTRY UINT32_C(0x00007c60)
#define POL_TEST_RETURN UINT32_C(0x00021c00)
#define POL_TEST_MODE UINT32_C(0x00530150)
#define POL_TEST_COUNT UINT32_C(0x0053014c)
#define POL_TEST_PALETTE_SRC UINT32_C(0x00501400)
#define POL_TEST_PALETTE_BIAS UINT32_C(0x005013f0)
#define POL_TEST_GEO_BASE UINT32_C(0x00800000)
#define POL_TEST_FIFO_BASE UINT32_C(0x00884000)
#define POL_TEST_GATE_LOW UINT32_C(0x00501018)
#define POL_TEST_GATE_HIGH UINT32_C(0x0050101c)
#define POL_TEST_COUNTER UINT32_C(0x00501010)
#define POL_TEST_TASK_FRAME UINT32_C(0x00504000)
#define POL_TEST_TABLE_BASE UINT32_C(0x020e0004)
#define POL_TEST_STACK (VF2_WORK_RAM_BASE + UINT32_C(0x3800))
#define POL_TEST_MAIN_DATA_SIZE UINT32_C(0x00100000)
#define POL_TEST_ID_985 UINT32_C(0x00000985)
#define POL_TEST_ID_986 UINT32_C(0x00000986)

static int failures = 0;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            fprintf(                                                          \
                stderr,                                                       \
                "FAILED %s:%d: %s\n",                                         \
                __FILE__, __LINE__, #expression                               \
            );                                                                \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

enum {
    WRITE_LOG_MAX = 64u,
    FIFO_LOG_MAX = 32u,
    GEO_LOG_MAX = 32u
};

typedef struct write_capture {
    uint32_t fifo[FIFO_LOG_MAX];
    size_t fifo_count;
    uint32_t geo_port[GEO_LOG_MAX];
    size_t geo_port_count;
} write_capture;

static void capture_observer(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    write_capture *capture = (write_capture *)context;
    uint32_t value = 0u;

    if (capture == NULL || access == NULL || access->data == NULL) {
        return;
    }
    if (access->kind != VF2_MODEL2A_MEMORY_WRITE || access->size != 4u) {
        return;
    }
    memcpy(&value, access->data, sizeof(value));
    if (access->address == POL_TEST_FIFO_BASE) {
        if (capture->fifo_count < FIFO_LOG_MAX) {
            capture->fifo[capture->fifo_count++] = value;
        }
        return;
    }
    if (access->address == POL_TEST_GEO_BASE) {
        if (capture->geo_port_count < GEO_LOG_MAX) {
            capture->geo_port[capture->geo_port_count++] = value;
        }
    }
}

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static uint32_t machine_u32(const vf2_model2a *machine, uint32_t address)
{
    uint32_t value = 0u;
    CHECK(vf2_model2a_read_u32(machine, address, &value) == VF2_OK);
    return value;
}

static void seed_u32(vf2_model2a *machine, uint32_t address, uint32_t value)
{
    CHECK(vf2_model2a_write_u32(machine, address, value) == VF2_OK);
}

static void seed_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    CHECK(vf2_model2a_write(machine, address, &value, 1u) == VF2_OK);
}

static void seed_palette_source(vf2_model2a *machine, uint32_t bias)
{
    seed_u32(machine, POL_TEST_PALETTE_BIAS, bias);
    /* Little-endian halfword pairs measured on long-29 park. */
    seed_u32(machine, POL_TEST_PALETTE_SRC + 0u, UINT32_C(0x00000180));
    seed_u32(machine, POL_TEST_PALETTE_SRC + 4u, UINT32_C(0x01f00000));
    seed_u32(machine, POL_TEST_PALETTE_SRC + 8u, UINT32_C(0x00f800c0));
    seed_u32(machine, POL_TEST_PALETTE_SRC + 12u, UINT32_C(0x00f800c0));
    seed_u32(machine, POL_TEST_PALETTE_SRC + 16u, UINT32_C(0x00f800c0));
    seed_u32(machine, POL_TEST_PALETTE_SRC + 20u, UINT32_C(0x00f800c0));
}

static void place_object_record(
    uint8_t *main_data,
    uint32_t object_id,
    uint32_t w0,
    uint32_t w1,
    uint32_t w2,
    uint32_t w3
)
{
    const uint32_t offset =
        (POL_TEST_TABLE_BASE - VF2_MAIN_DATA_BASE) + object_id * UINT32_C(16);

    write_le32(main_data + offset + 0u, w0);
    write_le32(main_data + offset + 4u, w1);
    write_le32(main_data + offset + 8u, w2);
    write_le32(main_data + offset + 12u, w3);
}

static void setup_cpu(
    vf2_i960_cpu *cpu,
    uint32_t entry,
    uint32_t return_address
)
{
    memset(cpu, 0, sizeof(*cpu));
    vf2_i960_cpu_reset(cpu, 0u, 0u, 0u);
    cpu->registers[1] = POL_TEST_STACK;
    cpu->registers[26] = POL_TEST_GEO_BASE; /* g10 */
    cpu->registers[27] = POL_TEST_FIFO_BASE; /* g11 */
    cpu->registers[28] = 0u;                 /* g12 */
    cpu->registers[29] = POL_TEST_TASK_FRAME; /* g13 */
    cpu->ip = entry;
    CHECK(vf2_i960_cpu_enter_procedure(cpu, entry, return_address) == VF2_OK);
}

static void setup_machine(
    vf2_model2a *machine,
    uint8_t mode,
    uint8_t count,
    uint32_t gate_low,
    uint32_t gate_high,
    uint32_t geo_pointer
)
{
    memset(machine, 0, sizeof(*machine));
    CHECK(vf2_model2a_initialize(machine) != 0);
    if (machine->work_ram == NULL) {
        return;
    }
    seed_u8(machine, POL_TEST_MODE, mode);
    seed_u8(machine, POL_TEST_COUNT, count);
    seed_u32(machine, POL_TEST_GATE_LOW, gate_low);
    seed_u32(machine, POL_TEST_GATE_HIGH, gate_high);
    seed_u32(machine, POL_TEST_COUNTER, 0u);
    seed_u32(machine, POL_TEST_GEO_BASE + UINT32_C(0x2008), geo_pointer);
    seed_u32(machine, POL_TEST_GEO_BASE + UINT32_C(0xb0), UINT32_C(0x11111111));
    seed_u32(machine, POL_TEST_GEO_BASE + UINT32_C(0x10), UINT32_C(0xdeadbeef));
    seed_palette_source(machine, UINT32_C(0x80));
    CHECK(vf2_model2a_set_memory_observer(machine, capture_observer, NULL) == VF2_OK);
}

static uint8_t *make_fake_main_data(void)
{
    uint8_t *main_data = (uint8_t *)calloc(1u, POL_TEST_MAIN_DATA_SIZE);

    if (main_data == NULL) {
        return NULL;
    }
    /* Unit-test records: synthetic ids, not ROM contents. */
    place_object_record(main_data, POL_TEST_ID_985,
                        UINT32_C(0x4a7d26), UINT32_C(0x158134),
                        UINT32_C(0x98a9c9), UINT32_C(0x00010002));
    place_object_record(main_data, POL_TEST_ID_986,
                        UINT32_C(0x4a7d36), UINT32_C(0x158138),
                        UINT32_C(0x98a9de), UINT32_C(0x00010002));
    return main_data;
}

static void check_fifo_sequence(
    const write_capture *capture,
    const uint32_t *expected,
    size_t expected_count
)
{
    size_t index = 0u;

    CHECK(capture->fifo_count == expected_count);
    if (capture->fifo_count != expected_count) {
        fprintf(
            stderr,
            "  fifo count actual=%zu expected=%zu\n",
            capture->fifo_count,
            expected_count
        );
        return;
    }
    for (index = 0u; index < expected_count; ++index) {
        if (capture->fifo[index] != expected[index]) {
            fprintf(
                stderr,
                "  fifo[%zu] actual=0x%08x expected=0x%08x\n",
                index,
                (unsigned)capture->fifo[index],
                (unsigned)expected[index]
            );
        }
        CHECK(capture->fifo[index] == expected[index]);
    }
}

static const uint32_t GOLD_PATH_A_COUNT1[] = {
    0x00800101u,
    0x01800303u,
    0x03000606u,
    0x3e9eb852u,
    0x3e428f5cu,
    0x3f9eb852u,
    0x1a003434u,
    0x00000000u,
    0x03000606u,
    0x00000000u,
    0xbec7ae14u,
    0x00000000u,
    0x1a003434u,
    0x00000000u,
    0x01000202u
};

static const uint32_t GOLD_PATH_A_COUNT0[] = {
    0x00800101u,
    0x01800303u,
    0x03000606u,
    0x3e9eb852u,
    0x3e428f5cu,
    0x3f9eb852u,
    0x03000606u,
    0x00000000u,
    0xbec7ae14u,
    0x00000000u,
    0x1a003434u,
    0x00000000u,
    0x01000202u
};

static const uint32_t GOLD_PALETTE[] = {
    0x0000007fu,
    0x01f001ffu,
    0x00f8013fu,
    0x00f8013fu,
    0x00f8013fu,
    0x00f8013fu
};

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    setup_machine(&machine, 2u, 0u, 0u, 0u, 0u);
    setup_cpu(&cpu, POL_TEST_ENTRY, POL_TEST_RETURN);
    CHECK(
        vf2_recovered_pol_test_path_a(NULL, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_recovered_pol_test_path_a(&machine, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    cpu.ip = POL_TEST_ENTRY + UINT32_C(4);
    CHECK(
        vf2_recovered_pol_test_path_a(&machine, &cpu) ==
        VF2_ERROR_UNSUPPORTED
    );
    vf2_model2a_shutdown(&machine);
}

static void test_unit_palette_helper(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    write_capture capture;
    size_t index = 0u;

    memset(&capture, 0, sizeof(capture));
    setup_machine(&machine, 2u, 0u, 0u, 0u, 0u);
    CHECK(vf2_model2a_set_memory_observer(&machine, capture_observer, &capture) == VF2_OK);
    setup_cpu(&cpu, POL_TEST_PALETTE_ENTRY, POL_TEST_RETURN);
    cpu.registers[16] = POL_TEST_PALETTE_SRC;

    CHECK(vf2_recovered_polygon_palette_pack_7f24(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == POL_TEST_RETURN);
    CHECK(cpu.procedure_returns == UINT64_C(1));
    CHECK(capture.geo_port_count == 6u);
    for (index = 0u; index < capture.geo_port_count && index < 6u; ++index) {
        CHECK(capture.geo_port[index] == GOLD_PALETTE[index]);
    }
    CHECK(machine_u32(&machine, POL_TEST_GEO_BASE + UINT32_C(0x30)) == 0u);
    /* Last palette word remains at the geo port address. */
    CHECK(machine_u32(&machine, POL_TEST_GEO_BASE) == GOLD_PALETTE[5]);
    CHECK(machine_u32(&machine, POL_TEST_PALETTE_BIAS) == UINT32_C(0x80));

    vf2_model2a_shutdown(&machine);
    puts("unit palette 0x7f24: six geo-port words pinned");
}

static void test_unit_path_b_unsupported(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    write_capture capture;
    uint8_t *main_data = NULL;

    memset(&capture, 0, sizeof(capture));
    setup_machine(&machine, 1u, 1u, UINT32_C(0x10000), 0u, 0u);
    CHECK(vf2_model2a_set_memory_observer(&machine, capture_observer, &capture) == VF2_OK);
    main_data = make_fake_main_data();
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_data(&machine, main_data, POL_TEST_MAIN_DATA_SIZE) ==
        VF2_OK
    );
    setup_cpu(&cpu, POL_TEST_ENTRY, POL_TEST_RETURN);

    CHECK(vf2_recovered_pol_test_path_a(&machine, &cpu) == VF2_ERROR_UNSUPPORTED);
    CHECK(cpu.ip != POL_TEST_RETURN);
    CHECK(capture.fifo_count == 0u);
    CHECK(capture.geo_port_count == 0u);
    CHECK(machine_u32(&machine, POL_TEST_GEO_BASE + UINT32_C(0x10)) == UINT32_C(0xdeadbeef));
    CHECK(machine_u32(&machine, POL_TEST_FIFO_BASE) == 0u);

    free(main_data);
    vf2_model2a_shutdown(&machine);
    puts("unit path B (mode<2): fail-closed UNSUPPORTED");
}

static void run_unit_path_a_case(
    const char *name,
    uint8_t mode,
    uint8_t count,
    uint32_t first_submit_w0,
    const uint32_t *fifo_gold,
    size_t fifo_gold_count,
    uint64_t expect_instructions,
    uint64_t expect_returns
)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    write_capture capture;
    uint8_t *main_data = NULL;
    size_t index = 0u;

    memset(&capture, 0, sizeof(capture));
    setup_machine(&machine, mode, count, UINT32_C(0x10000), 0u, 0u);
    CHECK(vf2_model2a_set_memory_observer(&machine, capture_observer, &capture) == VF2_OK);
    main_data = make_fake_main_data();
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_data(&machine, main_data, POL_TEST_MAIN_DATA_SIZE) ==
        VF2_OK
    );
    setup_cpu(&cpu, POL_TEST_ENTRY, POL_TEST_RETURN);

    CHECK(vf2_recovered_pol_test_path_a(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == POL_TEST_RETURN);
    if (cpu.procedure_returns != expect_returns) {
        fprintf(
            stderr,
            "  %s returns actual=%llu expected=%llu\n",
            name,
            (unsigned long long)cpu.procedure_returns,
            (unsigned long long)expect_returns
        );
    }
    CHECK(cpu.procedure_returns == expect_returns);
    check_fifo_sequence(&capture, fifo_gold, fifo_gold_count);

    /* Geo-port writes: six palette packs first, then stq w0 per submit. */
    CHECK(capture.geo_port_count == 6u + (size_t)count + 1u);
    for (index = 0u; index < 6u && index < capture.geo_port_count; ++index) {
        CHECK(capture.geo_port[index] == GOLD_PALETTE[index]);
    }
    if (capture.geo_port_count > 6u) {
        CHECK(capture.geo_port[6] == first_submit_w0);
    }
    /* Final geo+0x10 is always the last submit (0x985) on these cases. */
    CHECK(machine_u32(&machine, POL_TEST_GEO_BASE + UINT32_C(0x10)) ==
          UINT32_C(0x4a7d26));
    CHECK(machine_u32(&machine, POL_TEST_TASK_FRAME + UINT32_C(0x0c)) ==
          UINT32_C(0x00021a28));
    CHECK(machine_u32(&machine, POL_TEST_COUNTER) ==
          (uint32_t)count + 1u);
    if (expect_instructions != 0u) {
        if (cpu.executed_instructions != expect_instructions) {
            fprintf(
                stderr,
                "  %s insns actual=%llu expected=%llu\n",
                name,
                (unsigned long long)cpu.executed_instructions,
                (unsigned long long)expect_instructions
            );
        }
        CHECK(cpu.executed_instructions == expect_instructions);
    }

    printf(
        "%-22s mode=%u count=%u fifo=%zu geo6=0x%08x ins=%llu ret=%llu\n",
        name,
        (unsigned)mode,
        (unsigned)count,
        capture.fifo_count,
        capture.geo_port_count > 6u ? (unsigned)capture.geo_port[6] : 0u,
        (unsigned long long)cpu.executed_instructions,
        (unsigned long long)cpu.procedure_returns
    );

    free(main_data);
    vf2_model2a_shutdown(&machine);
}

static void test_unit_path_a_open(void)
{
    /* count=1 mode=2: loop submits 0x986 then final 0x985.
     * Instruction budget: 2 gate + 2 cont + (2+65) palette + 12 prelude
     * + 2 count + loop (2+1+3+28+2) + 8 interstitial + (3+28) final
     * + 2 close + 1 ret = 163.
     * Returns: palette + loop submit + final submit + path A ret. */
    run_unit_path_a_case(
        "pathA mode=2 count=1",
        2u,
        1u,
        UINT32_C(0x4a7d36),
        GOLD_PATH_A_COUNT1,
        sizeof(GOLD_PATH_A_COUNT1) / sizeof(GOLD_PATH_A_COUNT1[0]),
        UINT64_C(163),
        UINT64_C(4)
    );
    run_unit_path_a_case(
        "pathA mode=3 count=1",
        3u,
        1u,
        UINT32_C(0x4a7d26),
        GOLD_PATH_A_COUNT1,
        sizeof(GOLD_PATH_A_COUNT1) / sizeof(GOLD_PATH_A_COUNT1[0]),
        UINT64_C(162),
        UINT64_C(4)
    );
    run_unit_path_a_case(
        "pathA mode=2 count=0",
        2u,
        0u,
        UINT32_C(0x4a7d26),
        GOLD_PATH_A_COUNT0,
        sizeof(GOLD_PATH_A_COUNT0) / sizeof(GOLD_PATH_A_COUNT0[0]),
        UINT64_C(127),
        UINT64_C(3)
    );
}

static void run_rom_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    uint8_t mode,
    uint8_t count,
    const uint32_t *fifo_gold,
    size_t fifo_gold_count,
    const char *name
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    write_capture reference_capture;
    write_capture native_capture;
    uint32_t steps = 0u;
    vf2_status status = VF2_OK;

    memset(&reference_capture, 0, sizeof(reference_capture));
    memset(&native_capture, 0, sizeof(native_capture));
    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    CHECK(vf2_model2a_initialize(&reference_machine) != 0);
    CHECK(vf2_model2a_initialize(&native_machine) != 0);
    if (reference_machine.work_ram == NULL || native_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&reference_machine);
        vf2_model2a_shutdown(&native_machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_rom(&reference_machine, main_rom, main_rom_size) ==
        VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_rom(&native_machine, main_rom, main_rom_size) ==
        VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_data(&reference_machine, main_data, main_data_size) ==
        VF2_OK
    );
    CHECK(
        vf2_model2a_attach_main_data(&native_machine, main_data, main_data_size) ==
        VF2_OK
    );

    /* Identical seeds on both sides. */
    seed_u8(&reference_machine, POL_TEST_MODE, mode);
    seed_u8(&reference_machine, POL_TEST_COUNT, count);
    seed_u32(&reference_machine, POL_TEST_GATE_LOW, UINT32_C(0x10000));
    seed_u32(&reference_machine, POL_TEST_GATE_HIGH, 0u);
    seed_u32(&reference_machine, POL_TEST_COUNTER, 0u);
    seed_u32(&reference_machine, POL_TEST_GEO_BASE + UINT32_C(0x2008), 0u);
    seed_u32(&reference_machine, POL_TEST_GEO_BASE + UINT32_C(0xb0), UINT32_C(0x11111111));
    seed_u32(&reference_machine, POL_TEST_GEO_BASE + UINT32_C(0x10), UINT32_C(0xdeadbeef));
    seed_palette_source(&reference_machine, UINT32_C(0x80));

    seed_u8(&native_machine, POL_TEST_MODE, mode);
    seed_u8(&native_machine, POL_TEST_COUNT, count);
    seed_u32(&native_machine, POL_TEST_GATE_LOW, UINT32_C(0x10000));
    seed_u32(&native_machine, POL_TEST_GATE_HIGH, 0u);
    seed_u32(&native_machine, POL_TEST_COUNTER, 0u);
    seed_u32(&native_machine, POL_TEST_GEO_BASE + UINT32_C(0x2008), 0u);
    seed_u32(&native_machine, POL_TEST_GEO_BASE + UINT32_C(0xb0), UINT32_C(0x11111111));
    seed_u32(&native_machine, POL_TEST_GEO_BASE + UINT32_C(0x10), UINT32_C(0xdeadbeef));
    seed_palette_source(&native_machine, UINT32_C(0x80));

    CHECK(
        vf2_model2a_set_memory_observer(
            &reference_machine, capture_observer, &reference_capture
        ) == VF2_OK
    );
    CHECK(
        vf2_model2a_set_memory_observer(
            &native_machine, capture_observer, &native_capture
        ) == VF2_OK
    );

    setup_cpu(&reference_cpu, POL_TEST_ENTRY, POL_TEST_RETURN);
    setup_cpu(&native_cpu, POL_TEST_ENTRY, POL_TEST_RETURN);

    while (reference_cpu.ip != POL_TEST_RETURN && steps < 512u) {
        status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == POL_TEST_RETURN);
    CHECK(reference_cpu.executed_instructions >= UINT64_C(100));

    status = vf2_recovered_pol_test_path_a(&native_machine, &native_cpu);
    CHECK(status == VF2_OK);
    CHECK(native_cpu.ip == POL_TEST_RETURN);

    /* Gold FIFO pin (task requirement). */
    check_fifo_sequence(&native_capture, fifo_gold, fifo_gold_count);
    check_fifo_sequence(&reference_capture, fifo_gold, fifo_gold_count);

    /* Palette geo-port words must match the measured pack. */
    {
        size_t index = 0u;
        CHECK(native_capture.geo_port_count >= 6u);
        CHECK(reference_capture.geo_port_count >= 6u);
        for (index = 0u; index < 6u; ++index) {
            if (index < native_capture.geo_port_count) {
                CHECK(native_capture.geo_port[index] == GOLD_PALETTE[index]);
            }
            if (index < reference_capture.geo_port_count) {
                CHECK(reference_capture.geo_port[index] == GOLD_PALETTE[index]);
            }
        }
    }

    /* Side-effect pins vs real ROM object table. Final geo+0x10 is the
     * last submit (always 0x985 on these path-A fixtures). */
    CHECK(
        machine_u32(&native_machine, POL_TEST_GEO_BASE + UINT32_C(0x10)) ==
        UINT32_C(0x4a7d26)
    );
    CHECK(
        machine_u32(&reference_machine, POL_TEST_GEO_BASE + UINT32_C(0x10)) ==
        UINT32_C(0x4a7d26)
    );
    CHECK(machine_u32(&native_machine, POL_TEST_COUNTER) == (uint32_t)count + 1u);
    CHECK(machine_u32(&reference_machine, POL_TEST_COUNTER) == (uint32_t)count + 1u);
    CHECK(machine_u32(&native_machine, POL_TEST_GEO_BASE + UINT32_C(0x2008)) == 0u);
    CHECK(machine_u32(&reference_machine, POL_TEST_GEO_BASE + UINT32_C(0x2008)) == 0u);

    /* First submit id pin via geo-port stq word after the six palette packs. */
    if (native_capture.geo_port_count > 6u) {
        const uint32_t first =
            (mode == 3u) ? UINT32_C(0x4a7d26) : UINT32_C(0x4a7d36);
        if (count != 0u) {
            CHECK(native_capture.geo_port[6] == first);
        }
    }
    if (reference_capture.geo_port_count > 6u && count != 0u) {
        const uint32_t first =
            (mode == 3u) ? UINT32_C(0x4a7d26) : UINT32_C(0x4a7d36);
        CHECK(reference_capture.geo_port[6] == first);
    }

    /* Instruction counts: reference executes the ROM ret to return_address. */
    if (native_cpu.executed_instructions != reference_cpu.executed_instructions) {
        fprintf(
            stderr,
            "  %s insns native=%llu reference=%llu\n",
            name,
            (unsigned long long)native_cpu.executed_instructions,
            (unsigned long long)reference_cpu.executed_instructions
        );
    }
    if (count == 1u && mode == 2u) {
        CHECK(native_cpu.executed_instructions == UINT64_C(163));
        CHECK(reference_cpu.executed_instructions == UINT64_C(163));
    }
    if (count == 1u && mode == 3u) {
        CHECK(native_cpu.executed_instructions == UINT64_C(162));
        CHECK(reference_cpu.executed_instructions == UINT64_C(162));
    }
    if (count == 0u && mode == 2u) {
        CHECK(native_cpu.executed_instructions == UINT64_C(127));
        CHECK(reference_cpu.executed_instructions == UINT64_C(127));
    }

    printf(
        "%-28s mode=%u count=%u native_fifo=%zu ref_fifo=%zu "
        "native_ins=%llu ref_ins=%llu\n",
        name,
        (unsigned)mode,
        (unsigned)count,
        native_capture.fifo_count,
        reference_capture.fifo_count,
        (unsigned long long)native_cpu.executed_instructions,
        (unsigned long long)reference_cpu.executed_instructions
    );

    vf2_model2a_shutdown(&reference_machine);
    vf2_model2a_shutdown(&native_machine);
}

static void run_rom_differential(const char *rom_directory)
{
    uint8_t *main_rom = NULL;
    uint8_t *main_data = NULL;
    size_t main_rom_size = 0u;
    size_t main_data_size = 0u;

    CHECK(
        vf2_romset_build_region(
            rom_directory, VF2_REGION_MAINCPU, &main_rom, &main_rom_size
        ) == VF2_OK
    );
    CHECK(
        vf2_romset_build_region(
            rom_directory, VF2_REGION_MAIN_DATA, &main_data, &main_data_size
        ) == VF2_OK
    );
    if (main_rom == NULL || main_data == NULL) {
        free(main_rom);
        free(main_data);
        ++failures;
        return;
    }

    run_rom_case(
        main_rom,
        main_rom_size,
        main_data,
        main_data_size,
        2u,
        1u,
        GOLD_PATH_A_COUNT1,
        sizeof(GOLD_PATH_A_COUNT1) / sizeof(GOLD_PATH_A_COUNT1[0]),
        "rom pathA mode=2 count=1"
    );
    run_rom_case(
        main_rom,
        main_rom_size,
        main_data,
        main_data_size,
        3u,
        1u,
        GOLD_PATH_A_COUNT1,
        sizeof(GOLD_PATH_A_COUNT1) / sizeof(GOLD_PATH_A_COUNT1[0]),
        "rom pathA mode=3 count=1"
    );
    run_rom_case(
        main_rom,
        main_rom_size,
        main_data,
        main_data_size,
        2u,
        0u,
        GOLD_PATH_A_COUNT0,
        sizeof(GOLD_PATH_A_COUNT0) / sizeof(GOLD_PATH_A_COUNT0[0]),
        "rom pathA mode=2 count=0"
    );

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();
    test_unit_palette_helper();
    test_unit_path_b_unsupported();
    test_unit_path_a_open();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(stderr, "%d pol-test-path-a unit test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("pol-test-path-a ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d pol-test-path-a differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("pol-test-path-a differential tests passed");
    return EXIT_SUCCESS;
}
