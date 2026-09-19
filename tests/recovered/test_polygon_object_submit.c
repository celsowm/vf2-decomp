#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/recovered.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define POLY_ENTRY UINT32_C(0x00007c60)
#define POLY_RETURN UINT32_C(0x00010dcc)
#define POLY_TABLE_BASE UINT32_C(0x020e0004)
#define POLY_GEO_BASE UINT32_C(0x00800000)
#define POLY_FIFO_BASE UINT32_C(0x00884000)
#define POLY_GATE_LOW UINT32_C(0x00501018)
#define POLY_GATE_HIGH UINT32_C(0x0050101c)
#define POLY_COUNTER UINT32_C(0x00501010)
#define POLY_ACCUM UINT32_C(0x005010d0)
#define POLY_STACK (VF2_WORK_RAM_BASE + UINT32_C(0x3000))
#define POLY_MAIN_DATA_SIZE UINT32_C(0x00100000)

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

static void place_object_record(
    uint8_t *main_data,
    uint32_t object_id,
    uint32_t w0,
    uint32_t w1,
    uint32_t w2,
    uint32_t w3
)
{
    const uint32_t offset = (POLY_TABLE_BASE - VF2_MAIN_DATA_BASE) +
        object_id * UINT32_C(16);

    write_le32(main_data + offset + 0u, w0);
    write_le32(main_data + offset + 4u, w1);
    write_le32(main_data + offset + 8u, w2);
    write_le32(main_data + offset + 12u, w3);
}

static void setup_cpu(
    vf2_i960_cpu *cpu,
    uint32_t object_id,
    uint32_t g1,
    uint32_t g10,
    uint32_t g11,
    uint32_t g12
)
{
    memset(cpu, 0, sizeof(*cpu));
    vf2_i960_cpu_reset(cpu, 0u, 0u, 0u);
    cpu->ip = POLY_ENTRY;
    cpu->registers[1] = POLY_STACK;
    CHECK(
        vf2_i960_cpu_enter_procedure(cpu, POLY_ENTRY, POLY_RETURN) == VF2_OK
    );
    cpu->registers[16] = object_id; /* g0 */
    cpu->registers[17] = g1;        /* g1 */
    cpu->registers[26] = g10;       /* g10 */
    cpu->registers[27] = g11;       /* g11 */
    cpu->registers[28] = g12;       /* g12 */
    cpu->registers[9] = UINT32_C(0x00501200);
}

static void setup_machine_common(
    vf2_model2a *machine,
    uint32_t gate_low,
    uint32_t gate_high,
    uint32_t geo_ptr_word
)
{
    memset(machine, 0, sizeof(*machine));
    CHECK(vf2_model2a_initialize(machine) != 0);
    if (machine->work_ram == NULL) {
        return;
    }
    seed_u32(machine, POLY_GATE_LOW, gate_low);
    seed_u32(machine, POLY_GATE_HIGH, gate_high);
    seed_u32(machine, POLY_COUNTER, UINT32_C(7));
    seed_u32(machine, POLY_ACCUM, UINT32_C(3));
    seed_u32(machine, POLY_GEO_BASE + UINT32_C(0x2008), geo_ptr_word);
    seed_u32(machine, POLY_GEO_BASE + UINT32_C(0x10), UINT32_C(0xdeadbeef));
    seed_u32(machine, POLY_GEO_BASE + UINT32_C(0xb0), UINT32_C(0x11111111));
}

static uint8_t *make_fake_main_data(void)
{
    uint8_t *main_data = (uint8_t *)calloc(1u, POLY_MAIN_DATA_SIZE);

    if (main_data == NULL) {
        return NULL;
    }
    /* Unit-test records: synthetic, not ROM contents. */
    place_object_record(main_data, 0x2u,
                        UINT32_C(0x11111111), UINT32_C(0x22222222),
                        UINT32_C(0x33333333), UINT32_C(0x00020003));
    place_object_record(main_data, 0x3u,
                        UINT32_C(0xaabbccdd), UINT32_C(0x01020304),
                        UINT32_C(0x05060708), UINT32_C(0x000a000b));
    return main_data;
}

static void test_unit_gate_closed_no_writes(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *main_data = NULL;

    setup_machine_common(&machine, UINT32_C(0x1000), UINT32_C(0x2000),
                         UINT32_C(0x00000100));
    main_data = make_fake_main_data();
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_data(&machine, main_data, POLY_MAIN_DATA_SIZE) ==
        VF2_OK
    );
    setup_cpu(&cpu, 0x2u, 0u, POLY_GEO_BASE, POLY_FIFO_BASE, 0u);

    CHECK(vf2_recovered_polygon_object_submit(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == POLY_RETURN);
    CHECK(cpu.compare_result == VF2_I960_COMPARE_GREATER);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(1));
    CHECK(cpu.executed_instructions == UINT64_C(4));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    /* Locals r0-r15 are frame-restored on ret; globals survive. */
    CHECK(cpu.registers[16] == 0x2u);
    /* Fail-closed: gate closed must not submit. */
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0x10)) ==
          UINT32_C(0xdeadbeef));
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0xb0)) ==
          UINT32_C(0x11111111));
    CHECK(machine_u32(&machine, POLY_FIFO_BASE) == 0u);
    CHECK(machine_u32(&machine, POLY_COUNTER) == UINT32_C(7));
    CHECK(machine_u32(&machine, POLY_GATE_HIGH) == UINT32_C(0x2000));

    free(main_data);
    vf2_model2a_shutdown(&machine);
    puts("unit gate-closed: no submit stores");
}

static void test_unit_open_g1_zero(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *main_data = NULL;
    const uint32_t object_id = 0x2u;
    const uint32_t w0 = UINT32_C(0x11111111);
    const uint32_t w1 = UINT32_C(0x22222222);
    const uint32_t w2 = UINT32_C(0x33333333);
    const uint32_t w3 = UINT32_C(0x00020003);
    const uint32_t w3_low = w3 & UINT32_C(0x0000ffff);
    const uint32_t table_addr = POLY_TABLE_BASE + object_id * UINT32_C(16);

    setup_machine_common(&machine, UINT32_C(0x1000), 0u, UINT32_C(0x00000100));
    main_data = make_fake_main_data();
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_data(&machine, main_data, POLY_MAIN_DATA_SIZE) ==
        VF2_OK
    );
    setup_cpu(&cpu, object_id, 0u, POLY_GEO_BASE, POLY_FIFO_BASE, 0u);

    CHECK(vf2_recovered_polygon_object_submit(&machine, &cpu) == VF2_OK);
    CHECK(cpu.ip == POLY_RETURN);
    /* Final compare on open+g1=0 is cmpobe 0,g1 → EQUAL (not the gate LESS). */
    CHECK(cpu.compare_result == VF2_I960_COMPARE_EQUAL);
    CHECK((cpu.arithmetic_control & UINT32_C(7)) == UINT32_C(2));
    CHECK(cpu.executed_instructions == UINT64_C(28));
    CHECK(cpu.procedure_returns == UINT64_C(1));
    /* g0 survives ret as the table address (disasm: lda into g0). */
    CHECK(cpu.registers[16] == table_addr);
    /* Geo word0 pin pattern: g10+0x10 receives table w0. */
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0x10)) == w0);
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0xb0)) == 0u);
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0x1008)) ==
          UINT32_C(0x00000130));
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0x2008)) ==
          UINT32_C(0x00000100));
    /* stq r8,(g10)[g12] with g12=0 writes the quad at geo base.
     * r11 is -1 at store time (measured subo before stq). */
    CHECK(machine_u32(&machine, POLY_GEO_BASE + 0u) == w0);
    CHECK(machine_u32(&machine, POLY_GEO_BASE + 4u) == w1);
    CHECK(machine_u32(&machine, POLY_GEO_BASE + 8u) == w2);
    CHECK(machine_u32(&machine, POLY_GEO_BASE + 12u) == UINT32_C(0xffffffff));
    /* FIFO preamble via (g11)[g12]: last write is the geo pointer word. */
    CHECK(machine_u32(&machine, POLY_FIFO_BASE) == UINT32_C(0x00000100));
    CHECK(machine_u32(&machine, POLY_COUNTER) == UINT32_C(8));
    CHECK(machine_u32(&machine, POLY_GATE_HIGH) == w3_low);
    CHECK(machine_u32(&machine, POLY_ACCUM) == UINT32_C(3));

    free(main_data);
    vf2_model2a_shutdown(&machine);
    puts("unit open g1=0: w0 written to geo 0x10; fifo/cmd path observed");
}

static void test_unit_open_g1_nonzero(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *main_data = NULL;
    const uint32_t object_id = 0x3u;
    const uint32_t w0 = UINT32_C(0xaabbccdd);
    const uint32_t w3 = UINT32_C(0x000a000b);
    const uint32_t w3_low = w3 & UINT32_C(0x0000ffff);

    setup_machine_common(&machine, UINT32_C(0x1000), UINT32_C(0x10), 0u);
    main_data = make_fake_main_data();
    CHECK(main_data != NULL);
    if (main_data == NULL) {
        vf2_model2a_shutdown(&machine);
        return;
    }
    CHECK(
        vf2_model2a_attach_main_data(&machine, main_data, POLY_MAIN_DATA_SIZE) ==
        VF2_OK
    );
    setup_cpu(&cpu, object_id, 1u, POLY_GEO_BASE, POLY_FIFO_BASE, 0u);

    CHECK(vf2_recovered_polygon_object_submit(&machine, &cpu) == VF2_OK);
    CHECK(cpu.executed_instructions == UINT64_C(32));
    CHECK(cpu.registers[16] ==
          POLY_TABLE_BASE + object_id * UINT32_C(16));
    CHECK(machine_u32(&machine, POLY_GEO_BASE + UINT32_C(0x10)) == w0);
    CHECK(machine_u32(&machine, POLY_ACCUM) == UINT32_C(3) + w3_low);
    CHECK(machine_u32(&machine, POLY_GATE_HIGH) == UINT32_C(0x10) + w3_low);
    /* r9 is frame-restored on ret; the g1 scale is proven via 0x5010d0
     * and via the ROM-backed differential before return. */

    free(main_data);
    vf2_model2a_shutdown(&machine);
    puts("unit open g1!=0: r9 scale + 0x5010d0 accumulate observed");
}

static void test_unit_fail_closed_missing_table(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    setup_machine_common(&machine, UINT32_C(0x1000), 0u, 0u);
    setup_cpu(&cpu, 0x2u, 0u, POLY_GEO_BASE, POLY_FIFO_BASE, 0u);

    /* Gate open but main_data/table absent: must not claim success. */
    CHECK(
        vf2_recovered_polygon_object_submit(&machine, &cpu) ==
        VF2_ERROR_OUT_OF_BOUNDS
    );
    CHECK(cpu.ip != POLY_RETURN);

    vf2_model2a_shutdown(&machine);
    puts("unit fail-closed: missing object table rejected");
}

static void test_unit_invalid_arguments(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    setup_machine_common(&machine, 0u, 0u, 0u);
    setup_cpu(&cpu, 0x2u, 0u, POLY_GEO_BASE, POLY_FIFO_BASE, 0u);
    CHECK(
        vf2_recovered_polygon_object_submit(NULL, &cpu) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_recovered_polygon_object_submit(&machine, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    cpu.ip = POLY_ENTRY + UINT32_C(4);
    CHECK(
        vf2_recovered_polygon_object_submit(&machine, &cpu) ==
        VF2_ERROR_UNSUPPORTED
    );
    vf2_model2a_shutdown(&machine);
}

static void check_cpu_equal(const vf2_i960_cpu *expected, const vf2_i960_cpu *actual)
{
    size_t index = 0u;

    CHECK(actual->ip == expected->ip);
    CHECK(actual->sat == expected->sat);
    CHECK(actual->prcb == expected->prcb);
    CHECK(actual->process_control == expected->process_control);
    if (actual->arithmetic_control != expected->arithmetic_control ||
        actual->compare_result != expected->compare_result) {
        fprintf(
            stderr,
            "  ac/cmp expected 0x%08x/%u actual 0x%08x/%u\n",
            (unsigned)expected->arithmetic_control,
            (unsigned)expected->compare_result,
            (unsigned)actual->arithmetic_control,
            (unsigned)actual->compare_result
        );
    }
    CHECK(actual->arithmetic_control == expected->arithmetic_control);
    CHECK(actual->compare_result == expected->compare_result);
    CHECK(actual->executed_instructions == expected->executed_instructions);
    CHECK(actual->procedure_calls == expected->procedure_calls);
    CHECK(actual->procedure_returns == expected->procedure_returns);
    CHECK(actual->local_frame_depth == expected->local_frame_depth);
    CHECK(actual->maximum_local_frame_depth == expected->maximum_local_frame_depth);
    for (index = 0u; index < 32u; ++index) {
        if (actual->registers[index] != expected->registers[index]) {
            fprintf(
                stderr,
                "  reg[%u] expected 0x%08x actual 0x%08x\n",
                (unsigned)index,
                (unsigned)expected->registers[index],
                (unsigned)actual->registers[index]
            );
        }
        CHECK(actual->registers[index] == expected->registers[index]);
    }
}

static void check_regions_equal(
    const vf2_model2a *reference,
    const vf2_model2a *native
)
{
    CHECK(
        memcmp(
            reference->work_ram, native->work_ram, reference->work_ram_size
        ) == 0
    );
    CHECK(
        memcmp(
            reference->geometry, native->geometry, reference->geometry_size
        ) == 0
    );
    CHECK(
        memcmp(
            reference->copro_port, native->copro_port, reference->copro_port_size
        ) == 0
    );
    CHECK(
        memcmp(
            reference->buffer_ram, native->buffer_ram, reference->buffer_ram_size
        ) == 0
    );
    CHECK(reference->geometry_write_start == native->geometry_write_start);
    CHECK(reference->geometry_read_start == native->geometry_read_start);
    CHECK(reference->geometry_control == native->geometry_control);
}

typedef struct poly_diff_case {
    const char *name;
    uint32_t object_id;
    uint32_t g1;
    uint32_t gate_low;
    uint32_t gate_high;
    uint32_t g10;
    uint32_t g11;
    uint32_t g12;
    uint32_t geo_ptr_word;
    int expect_submit;
    uint32_t expect_w0; /* 0 = do not pin; otherwise measured oracle w0 */
    uint64_t expect_instructions;
} poly_diff_case;

static const poly_diff_case diff_cases[] = {
    {
        "gate-closed",
        0x148u, 0u, 0x1000u, 0x2000u,
        POLY_GEO_BASE, POLY_FIFO_BASE, 0u,
        0x100u, 0, 0u, 4u
    },
    {
        /* Oracle pin: g0=0x148 -> geo 0x800010 first write 0x000b026a. */
        "pin-0x148",
        0x148u, 0u, 0x1000u, 0u,
        POLY_GEO_BASE, POLY_FIFO_BASE, 0u,
        0x100u, 1, UINT32_C(0x000b026a), 28u
    },
    {
        /* Oracle pin: g0=0x88 -> geo 0x800010 first write 0x0008e6de. */
        "pin-0x88",
        0x88u, 0u, 0x1000u, 0u,
        POLY_GEO_BASE, POLY_FIFO_BASE, 0u,
        0x100u, 1, UINT32_C(0x0008e6de), 28u
    },
    {
        "pin-0x148-g1",
        0x148u, 1u, 0x1000u, 0u,
        POLY_GEO_BASE, POLY_FIFO_BASE, 0u,
        0x100u, 1, UINT32_C(0x000b026a), 32u
    },
    {
        /* Dual-port addressing: g11=0x880000, g12=0x4000 -> FIFO 0x884000
         * and stq at geo 0x804000 (measured dual write addresses). */
        "dual-port-0x148",
        0x148u, 0u, 0x1000u, 0u,
        POLY_GEO_BASE, UINT32_C(0x00880000), UINT32_C(0x4000),
        0x100u, 1, UINT32_C(0x000b026a), 28u
    }
};

static void run_rom_diff_case(
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size,
    const poly_diff_case *test_case
)
{
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    uint32_t geo_word = 0u;
    uint32_t steps = 0u;
    vf2_status status = VF2_OK;
    const uint32_t return_address = POLY_RETURN;

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

    seed_u32(&reference_machine, POLY_GATE_LOW, test_case->gate_low);
    seed_u32(&reference_machine, POLY_GATE_HIGH, test_case->gate_high);
    seed_u32(&reference_machine, POLY_COUNTER, UINT32_C(7));
    seed_u32(&reference_machine, POLY_ACCUM, UINT32_C(3));
    seed_u32(
        &reference_machine, POLY_GEO_BASE + UINT32_C(0x2008),
        test_case->geo_ptr_word
    );
    seed_u32(
        &reference_machine, POLY_GEO_BASE + UINT32_C(0x10), UINT32_C(0xdeadbeef)
    );
    seed_u32(
        &reference_machine, POLY_GEO_BASE + UINT32_C(0xb0), UINT32_C(0x11111111)
    );

    seed_u32(&native_machine, POLY_GATE_LOW, test_case->gate_low);
    seed_u32(&native_machine, POLY_GATE_HIGH, test_case->gate_high);
    seed_u32(&native_machine, POLY_COUNTER, UINT32_C(7));
    seed_u32(&native_machine, POLY_ACCUM, UINT32_C(3));
    seed_u32(
        &native_machine, POLY_GEO_BASE + UINT32_C(0x2008),
        test_case->geo_ptr_word
    );
    seed_u32(
        &native_machine, POLY_GEO_BASE + UINT32_C(0x10), UINT32_C(0xdeadbeef)
    );
    seed_u32(
        &native_machine, POLY_GEO_BASE + UINT32_C(0xb0), UINT32_C(0x11111111)
    );

    setup_cpu(
        &reference_cpu,
        test_case->object_id,
        test_case->g1,
        test_case->g10,
        test_case->g11,
        test_case->g12
    );
    setup_cpu(
        &native_cpu,
        test_case->object_id,
        test_case->g1,
        test_case->g10,
        test_case->g11,
        test_case->g12
    );

    while (reference_cpu.ip != return_address && steps < 64u) {
        status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        CHECK(status == VF2_OK);
        ++steps;
        if (status != VF2_OK) {
            break;
        }
    }
    CHECK(reference_cpu.ip == return_address);
    CHECK(reference_cpu.executed_instructions == test_case->expect_instructions);
    CHECK((uint64_t)steps == test_case->expect_instructions);

    status = vf2_recovered_polygon_object_submit(&native_machine, &native_cpu);
    CHECK(status == VF2_OK);
    check_cpu_equal(&reference_cpu, &native_cpu);
    check_regions_equal(&reference_machine, &native_machine);

    geo_word = machine_u32(
        &native_machine, test_case->g10 + UINT32_C(0x10)
    );
    if (test_case->expect_submit) {
        CHECK(geo_word != UINT32_C(0xdeadbeef));
        if (test_case->expect_w0 != 0u) {
            CHECK(geo_word == test_case->expect_w0);
            /* Correlate the same pin on the reference geo port. */
            CHECK(
                machine_u32(
                    &reference_machine, test_case->g10 + UINT32_C(0x10)
                ) == test_case->expect_w0
            );
        }
    } else {
        CHECK(geo_word == UINT32_C(0xdeadbeef));
    }

    printf(
        "%-16s id=0x%03x geo=0x%08x ins=%llu %s\n",
        test_case->name,
        (unsigned)test_case->object_id,
        (unsigned)geo_word,
        (unsigned long long)native_cpu.executed_instructions,
        test_case->expect_submit ? "submitted" : "gated"
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
    size_t index = 0u;

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

    for (index = 0u; index < sizeof(diff_cases) / sizeof(diff_cases[0]); ++index) {
        run_rom_diff_case(
            main_rom,
            main_rom_size,
            main_data,
            main_data_size,
            &diff_cases[index]
        );
    }

    free(main_rom);
    free(main_data);
}

int main(int argc, char **argv)
{
    test_unit_invalid_arguments();
    test_unit_gate_closed_no_writes();
    test_unit_open_g1_zero();
    test_unit_open_g1_nonzero();
    test_unit_fail_closed_missing_table();

    if (argc != 2) {
        if (failures != 0) {
            fprintf(stderr, "%d polygon-object-submit unit test(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        puts("polygon-object-submit ROM-independent tests passed");
        return EXIT_SUCCESS;
    }

    run_rom_differential(argv[1]);

    if (failures != 0) {
        fprintf(
            stderr,
            "%d polygon-object-submit differential test(s) failed\n",
            failures
        );
        return EXIT_FAILURE;
    }
    puts("polygon-object-submit differential tests passed");
    return EXIT_SUCCESS;
}
