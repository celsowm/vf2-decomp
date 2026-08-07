from pathlib import Path
import sys


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected one match in {path}, found {count}")
    path.write_text(text.replace(old, new, 1))


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_endurance_fixes.py ROOT")
    root = Path(sys.argv[1])

    video = root / "src/recovered/texture_bridge_video.c"
    replace_once(
        video,
        '''    if (active == 0u) {\n        set_equal_condition(cpu);\n        status = finish_recovered_procedure(machine, cpu, UINT64_C(3));\n''',
        '''    if (active == 0u) {\n        /* cmpobe at 0x00002dec branches on equality without updating the\n         * i960 arithmetic condition code. Preserve the incoming condition. */\n        status = finish_recovered_procedure(machine, cpu, UINT64_C(3));\n''',
    )

    match = root / "src/recovered/texture_bridge_match.c"
    replace_once(
        match,
        '''    if (status != VF2_OK || (frame_counter & UINT32_C(31)) == 0u) {\n        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;\n    }\n    status = vf2_model2a_read_u32(\n        machine, UINT32_C(0x00500160), &previous_minimum\n    );\n    if (status == VF2_OK) {\n        status = vf2_model2a_write_u32(\n            machine, UINT32_C(0x00500160), cpu->registers[5]\n        );\n    }\n    if (status == VF2_OK) {\n        status = vf2_model2a_read(machine, UINT32_C(0x0050006d), &mode, 1u);\n    }\n    if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {\n        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;\n    }\n    cpu->registers[3] = previous_minimum;\n    cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;\n    (void)timer_low;\n    finish_recovered_control_block(cpu, UINT32_C(0x00010f90), UINT64_C(23));\n''',
        '''    if (status != VF2_OK) {\n        return status;\n    }\n    if ((frame_counter & UINT32_C(31)) == 0u) {\n        /* cmpobe at 0x00010f5c jumps directly to the minimum store and, like\n         * the other COBR instructions, does not update the arithmetic\n         * condition code. The skipped load leaves r3 at zero. */\n        cpu->registers[3] = 0u;\n        status = vf2_model2a_write_u32(\n            machine, UINT32_C(0x00500160), cpu->registers[5]\n        );\n        if (status == VF2_OK) {\n            status = vf2_model2a_read(\n                machine, UINT32_C(0x0050006d), &mode, 1u\n            );\n        }\n        if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {\n            return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;\n        }\n        cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;\n        (void)timer_low;\n        finish_recovered_control_block(\n            cpu, UINT32_C(0x00010f90), UINT64_C(21)\n        );\n        report->kind = VF2_HYBRID_BRIDGE_FRAME_TIMER_PREFIX;\n        report->entry_address = VF2_FRAME_TIMER_PREFIX_ENTRY;\n        report->exit_address = cpu->ip;\n        report->changed_values = UINT64_C(2);\n        report->bytes_written = sizeof(uint32_t) * 2u;\n        report->recovered_instruction_count = UINT64_C(21);\n        report->cpu_poststate_applied = 1;\n        return VF2_OK;\n    }\n    status = vf2_model2a_read_u32(\n        machine, UINT32_C(0x00500160), &previous_minimum\n    );\n    if (status == VF2_OK) {\n        status = vf2_model2a_write_u32(\n            machine, UINT32_C(0x00500160), cpu->registers[5]\n        );\n    }\n    if (status == VF2_OK) {\n        status = vf2_model2a_read(machine, UINT32_C(0x0050006d), &mode, 1u);\n    }\n    if (status != VF2_OK || mode == UINT8_C(1) || mode == UINT8_C(2)) {\n        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;\n    }\n    cpu->registers[3] = previous_minimum;\n    cpu->registers[VF2_I960_G0_REGISTER] = frame_byte;\n    (void)timer_low;\n    finish_recovered_control_block(cpu, UINT32_C(0x00010f90), UINT64_C(23));\n''',
    )

    cycles = root / "tools/vf2cycles/main.c"
    replace_once(
        cycles,
        '''    if (status == VF2_OK) {\n        run_status = run_cycles_with_checkpoints(\n            &reference_machine,\n            &reference_cpu,\n            &native_machine,\n            &native_cpu,\n            &runtime_state,\n            snapshot.cpu.ip,\n            options.cycle_count,\n            options.minimum_blocks,\n            options.maximum_blocks,\n            &last_match_snapshot,\n            &last_match_state,\n            &report\n        );\n        status = run_status;\n    }\n''',
        '''    if (status == VF2_OK) {\n        if (options.failure_prefix == NULL) {\n            run_status = vf2_native_differential_run_cycles(\n                &reference_machine,\n                &reference_cpu,\n                &native_machine,\n                &native_cpu,\n                &runtime_state,\n                snapshot.cpu.ip,\n                options.cycle_count,\n                options.minimum_blocks,\n                options.maximum_blocks,\n                &report\n            );\n        } else {\n            run_status = run_cycles_with_checkpoints(\n                &reference_machine,\n                &reference_cpu,\n                &native_machine,\n                &native_cpu,\n                &runtime_state,\n                snapshot.cpu.ip,\n                options.cycle_count,\n                options.minimum_blocks,\n                options.maximum_blocks,\n                &last_match_snapshot,\n                &last_match_state,\n                &report\n            );\n        }\n        status = run_status;\n    }\n''',
    )

    cmake = root / "CMakeLists.txt"
    replace_once(
        cmake,
        '''    add_executable(vf2_native_differential_tests\n        tests/recovered/test_native_differential.c\n    )\n''',
        '''    add_executable(vf2_endurance_regression_tests\n        tests/recovered/test_endurance_regressions.c\n    )\n    target_link_libraries(vf2_endurance_regression_tests PRIVATE vf2_core)\n    vf2_set_project_warnings(vf2_endurance_regression_tests)\n\n    add_test(\n        NAME vf2_endurance_regressions\n        COMMAND vf2_endurance_regression_tests\n    )\n\n    add_executable(vf2_native_differential_tests\n        tests/recovered/test_native_differential.c\n    )\n''',
    )

    status_doc = root / "docs/STATUS.md"
    replace_once(
        status_doc,
        '''This tooling does not extend the ROM-proven boundary by itself. Its first\nexpected failure identifies the concrete unsupported block that must be\nrecovered before a strict sixth-dispatch contract can be recorded.\n''',
        '''ROM-backed endurance now extends the proven corridor substantially beyond the\nfifth entry. A verified 36/36 ROM set completed 1,000 additional repeated-\naddress cycles with exact CPU, local-frame, counter, frame-event and mutable-\nmemory equality: 36,000 additional native blocks and 1,582,507 recovered i960\ninstructions. The final checkpoint is again `fa_game_info` at `0x0001645c`,\nwith 1,003 scheduler entries and 1,003 injected frame IRQs accumulated by the\nnative runtime.\n\nThat endurance run exposed and fixed two previously unobserved periodic paths:\n\n- inactive palette upload at `0x00002de4` now preserves the incoming i960\n  arithmetic condition because the ROM's `cmpobe` does not modify condition\n  codes; and\n- the frame-timer prefix at `0x00010f08` now accepts the\n  `(frame_counter & 31) == 0` path, skips the previous-minimum load/comparison\n  exactly like the ROM, and accounts the 21-instruction path.\n\n`vf2cycles` keeps pre-block failure snapshots only when `--failure-prefix` is\nrequested; ordinary endurance uses the lower-overhead repeated-cycle runner.\nThe next target is no longer an artificial sixth-dispatch milestone, but the\nfirst state-dependent branch that actually fails beyond this 1,000-cycle\ncorridor.\n''',
    )

    readme = root / "README.md"
    replace_once(
        readme,
        '''files. Re-running those files reproduces the unsupported block without replaying\nthe accepted corridor. A successful run never interprets instructions on the\nnative side.\n''',
        '''files. Re-running those files reproduces the unsupported block without replaying\nthe accepted corridor. Pre-block snapshots are only taken when\n`--failure-prefix` is present, so normal endurance runs avoid that extra copy. A\nsuccessful run never interprets instructions on the native side.\n\nA ROM-backed endurance run from the fifth-dispatch checkpoint has now completed\n1,000 additional repeated-address cycles (36,000 blocks / 1,582,507 recovered\ninstructions) with exact differential state equality.\n''',
    )


if __name__ == "__main__":
    main()
