#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"

int vf2_test_i960_snapshot(void)
{
    static const char path[] = "vf2-test-snapshot.bin";
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_i960_snapshot first;
    vf2_i960_snapshot second;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;

    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&first);
    vf2_i960_snapshot_init(&second);
    if (!vf2_model2a_initialize(&machine)) {
        return 1;
    }
    memset(machine.work_ram, 0x12, machine.work_ram_size);
    memset(machine.buffer_ram, 0x34, machine.buffer_ram_size);
    memset(machine.video_control, 0x56, machine.video_control_size);
    memset(machine.cpu_control, 0x78, machine.cpu_control_size);
    memset(machine.interrupt_control, 0x79, machine.interrupt_control_size);
    memset(machine.timers, 0x7a, machine.timers_size);
    memset(machine.tile_ram, 0x7a, machine.tile_ram_size);
    memset(machine.palette_ram, 0x7b, machine.palette_ram_size);
    memset(machine.io_control, 0x7c, machine.io_control_size);
    memset(machine.backup_sram, 0x7d, machine.backup_sram_size);
    memset(machine.copro_control, 0x7d, machine.copro_control_size);
    memset(machine.color_translation, 0x7e, machine.color_translation_size);
    memset(machine.texture_ram0, 0x81, machine.texture_ram0_size);
    memset(machine.texture_ram1, 0x82, machine.texture_ram1_size);
    memset(machine.luma_ram, 0x7f, machine.luma_ram_size);
    memset(machine.system_control, 0x9a, machine.system_control_size);
    machine.geometry_write_start = UINT32_C(0x1234);
    machine.geometry_read_start = UINT32_C(0x5678);
    machine.geometry_control = UINT32_C(0x80000005);
    machine.geometry_program_count = UINT32_C(42);
    vf2_i960_cpu_reset(&cpu, 0x10u, 0x20u, 0x30u);
    cpu.registers[16] = 0x12345678u;
    cpu.process_control = 0x11111111u;
    cpu.arithmetic_control = 0x22222222u;
    cpu.interrupt_control = 0x33333333u;
    cpu.executed_instructions = 99u;
    cpu.procedure_calls = 7u;
    cpu.procedure_returns = 5u;
    cpu.interrupt_entries = 3u;
    cpu.interrupt_returns = 2u;
    cpu.local_frame_depth = 2u;
    cpu.maximum_local_frame_depth = 4u;
    cpu.local_frames[0].registers[2] = 0x11112222u;
    cpu.local_frames[1].registers[3] = 0x33334444u;

    status = vf2_i960_snapshot_capture(&first, &cpu, &machine);
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_write_file(&first, path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&second, path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_compare(&first, &second, &diff);
    }
    if (status != VF2_OK || !diff.equal || second.cpu.interrupt_entries != 3u ||
        second.cpu.interrupt_returns != 2u ||
        second.texture_ram0_size != machine.texture_ram0_size ||
        second.texture_ram1_size != machine.texture_ram1_size ||
        second.texture_ram0[0x600u] != UINT8_C(0x81) ||
        second.texture_ram1[0x600u] != UINT8_C(0x82) ||
        second.geometry_write_start != UINT32_C(0x1234) ||
        second.geometry_read_start != UINT32_C(0x5678) ||
        second.geometry_control != UINT32_C(0x80000005) ||
        second.geometry_program_count != UINT32_C(42)) {
        (void)remove(path);
        vf2_i960_snapshot_destroy(&first);
        vf2_i960_snapshot_destroy(&second);
        vf2_model2a_shutdown(&machine);
        return 2;
    }
    second.work_ram[7] ^= 1u;
    status = vf2_i960_snapshot_compare(&first, &second, &diff);
    if (status != VF2_OK || diff.equal || strcmp(diff.component, "work-ram") != 0 ||
        diff.first_offset != 7u) {
        (void)remove(path);
        vf2_i960_snapshot_destroy(&first);
        vf2_i960_snapshot_destroy(&second);
        vf2_model2a_shutdown(&machine);
        return 3;
    }

    {
        vf2_model2a live_machine;
        vf2_i960_cpu live_cpu;
        uint8_t *const reused_work_ram = first.work_ram;

        memset(&live_machine, 0, sizeof(live_machine));
        memset(&live_cpu, 0, sizeof(live_cpu));
        if (!vf2_model2a_initialize(&live_machine)) {
            (void)remove(path);
            vf2_i960_snapshot_destroy(&first);
            vf2_i960_snapshot_destroy(&second);
            vf2_model2a_shutdown(&machine);
            return 4;
        }
        status = vf2_i960_snapshot_restore(&first, &live_cpu, &live_machine);
        if (status == VF2_OK) {
            status = vf2_i960_compare_live_state(
                &cpu,
                &machine,
                &live_cpu,
                &live_machine,
                &diff
            );
        }
        if (status != VF2_OK || !diff.equal) {
            vf2_model2a_shutdown(&live_machine);
            (void)remove(path);
            vf2_i960_snapshot_destroy(&first);
            vf2_i960_snapshot_destroy(&second);
            vf2_model2a_shutdown(&machine);
            return 5;
        }
        live_machine.geometry_write_start ^= UINT32_C(4);
        status = vf2_i960_compare_live_state(
            &cpu, &machine, &live_cpu, &live_machine, &diff
        );
        if (status != VF2_OK || diff.equal ||
            strcmp(diff.component, "model2-state") != 0 ||
            diff.first_offset != 0u) {
            vf2_model2a_shutdown(&live_machine);
            (void)remove(path);
            vf2_i960_snapshot_destroy(&first);
            vf2_i960_snapshot_destroy(&second);
            vf2_model2a_shutdown(&machine);
            return 6;
        }
        live_machine.geometry_write_start ^= UINT32_C(4);
        live_machine.work_ram[7] ^= 1u;
        status = vf2_i960_compare_live_state(
            &cpu,
            &machine,
            &live_cpu,
            &live_machine,
            &diff
        );
        if (status != VF2_OK || diff.equal ||
            strcmp(diff.component, "work-ram") != 0 ||
            diff.first_offset != 7u) {
            vf2_model2a_shutdown(&live_machine);
            (void)remove(path);
            vf2_i960_snapshot_destroy(&first);
            vf2_i960_snapshot_destroy(&second);
            vf2_model2a_shutdown(&machine);
            return 7;
        }
        status = vf2_i960_snapshot_capture(&first, &cpu, &machine);
        if (status != VF2_OK || first.work_ram != reused_work_ram) {
            vf2_model2a_shutdown(&live_machine);
            (void)remove(path);
            vf2_i960_snapshot_destroy(&first);
            vf2_i960_snapshot_destroy(&second);
            vf2_model2a_shutdown(&machine);
            return 8;
        }
        vf2_model2a_shutdown(&live_machine);
    }

    /* Difference-run enumeration: two separated runs inside one region, so
     * maximal-run splitting and the byte total are observable. This uses its
     * own machine and snapshot pair: `first`/`second` share region storage
     * after the reuse check above and must not be written to here. */
    {
        vf2_model2a run_machine;
        vf2_i960_snapshot runs_first;
        vf2_i960_snapshot runs_second;
        vf2_i960_snapshot_run runs[4];
        vf2_i960_cpu run_cpu;
        size_t run_count = 0u;
        size_t run_bytes = 0u;
        size_t index = 0u;
        const uint8_t *region = NULL;
        size_t region_size = 0u;
        memset(runs, 0, sizeof(runs));
        vf2_i960_snapshot_init(&runs_first);
        vf2_i960_snapshot_init(&runs_second);
        if (!vf2_model2a_initialize(&run_machine)) return 9;
        memset(run_machine.work_ram, 0x12, run_machine.work_ram_size);
        memset(run_machine.tile_ram, 0x34, run_machine.tile_ram_size);
        vf2_i960_cpu_reset(&run_cpu, 0u, 0u, 0u);
        status = vf2_i960_snapshot_capture(&runs_first, &run_cpu, &run_machine);
        if (status == VF2_OK) {
            status = vf2_i960_snapshot_capture(
                &runs_second, &run_cpu, &run_machine
            );
        }
        if (status == VF2_OK) {
            runs_second.work_ram[10] = 0x99u;
            runs_second.work_ram[11] = 0x98u;
            runs_second.work_ram[40] = 0x77u;
        }
        status = vf2_i960_snapshot_diff_runs(
            &runs_first, &runs_second, runs, 4u, &run_count, &run_bytes
        );
        if (status != VF2_OK || run_count != 2u || run_bytes != 3u) {
            vf2_model2a_shutdown(&run_machine);
            vf2_i960_snapshot_destroy(&runs_second);
            vf2_i960_snapshot_destroy(&runs_first);
            return 10;
        }
        if (strcmp(runs[0].name, "work-ram") != 0 || runs[0].offset != 10u ||
            runs[0].length != 2u || runs[1].offset != 40u ||
            runs[1].length != 1u) {
            vf2_model2a_shutdown(&run_machine);
            vf2_i960_snapshot_destroy(&runs_second);
            vf2_i960_snapshot_destroy(&runs_first);
            return 11;
        }
        /* A truncated capacity must still report the true run count and byte
         * total so the caller can detect that it received only some runs. */
        {
            size_t truncated_count = 0u;
            size_t truncated_bytes = 0u;
            vf2_i960_snapshot_run one[1];
            memset(one, 0, sizeof(one));
            status = vf2_i960_snapshot_diff_runs(
                &runs_first, &runs_second, one, 1u, &truncated_count,
                &truncated_bytes
            );
            if (status != VF2_OK || truncated_count != 2u ||
                truncated_bytes != 3u || one[0].offset != 10u) {
                vf2_model2a_shutdown(&run_machine);
                vf2_i960_snapshot_destroy(&runs_second);
                vf2_i960_snapshot_destroy(&runs_first);
                return 12;
            }
        }
        /* Counting only: no storage, still exact. */
        {
            size_t count_only = 0u;
            size_t count_only_bytes = 0u;
            status = vf2_i960_snapshot_diff_runs(
                &runs_first, &runs_second, NULL, 0u, &count_only,
                &count_only_bytes
            );
            if (status != VF2_OK || count_only != 2u ||
                count_only_bytes != 3u) {
                vf2_model2a_shutdown(&run_machine);
                vf2_i960_snapshot_destroy(&runs_second);
                vf2_i960_snapshot_destroy(&runs_first);
                return 13;
            }
        }
        region = vf2_i960_snapshot_region_data(
            &runs_first, "work-ram", &region_size
        );
        if (region == NULL || region != runs_first.work_ram ||
            region_size != runs_first.work_ram_size) {
            vf2_model2a_shutdown(&run_machine);
            vf2_i960_snapshot_destroy(&runs_second);
            vf2_i960_snapshot_destroy(&runs_first);
            return 14;
        }
        if (vf2_i960_snapshot_region_data(
                &runs_first, "no-such-region", NULL) != NULL) {
            vf2_model2a_shutdown(&run_machine);
            vf2_i960_snapshot_destroy(&runs_second);
            vf2_i960_snapshot_destroy(&runs_first);
            return 15;
        }
        /* An identical pair must report zero runs and zero bytes, and must
         * not touch the caller's storage. */
        for (index = 0u; index < 4u; ++index) {
            runs[index].offset = 12345u;
            runs[index].length = 6789u;
        }
        status = vf2_i960_snapshot_diff_runs(
            &runs_first, &runs_first, runs, 4u, &run_count, &run_bytes
        );
        if (status != VF2_OK || run_count != 0u || run_bytes != 0u ||
            runs[0].offset != 12345u) {
            vf2_model2a_shutdown(&run_machine);
            vf2_i960_snapshot_destroy(&runs_second);
            vf2_i960_snapshot_destroy(&runs_first);
            return 16;
        }
        vf2_model2a_shutdown(&run_machine);
        vf2_i960_snapshot_destroy(&runs_second);
        vf2_i960_snapshot_destroy(&runs_first);
    }

    (void)remove(path);
    vf2_i960_snapshot_destroy(&first);
    vf2_i960_snapshot_destroy(&second);
    vf2_model2a_shutdown(&machine);
    return 0;
}
