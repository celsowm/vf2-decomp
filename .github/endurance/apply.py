from pathlib import Path
import sys


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(
            f"expected exactly one match in {path}, found {count}: {old[:80]!r}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def append_once(path: Path, marker: str, section: str) -> None:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        return
    if not text.endswith("\n"):
        text += "\n"
    path.write_text(text + "\n" + section.rstrip() + "\n", encoding="utf-8")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply.py <repository-root>")

    root = Path(sys.argv[1]).resolve()

    replace_once(
        root / "CMakeLists.txt",
        """add_executable(vf2i960 tools/vf2i960/main.c)
target_link_libraries(vf2i960 PRIVATE vf2_core)
vf2_set_project_warnings(vf2i960)

include(GNUInstallDirs)
install(TARGETS vf2 vf2rom vf2i960
""",
        """add_executable(vf2i960 tools/vf2i960/main.c)
target_link_libraries(vf2i960 PRIVATE vf2_core)
vf2_set_project_warnings(vf2i960)

add_executable(vf2cycles tools/vf2cycles/main.c)
target_link_libraries(vf2cycles PRIVATE vf2_core)
vf2_set_project_warnings(vf2cycles)

include(GNUInstallDirs)
install(TARGETS vf2 vf2rom vf2i960 vf2cycles
""",
    )

    main_c = root / "tools/vf2i960/main.c"
    replace_once(
        main_c,
        '        "  %s native-fifth-dispatch <rom-directory>\\n"\n',
        '        "  %s native-fifth-dispatch <rom-directory> [output.vf2snap]\\n"\n',
    )
    replace_once(
        main_c,
        """static FILE *g_orchestrator_trace_file = NULL;
static uint64_t g_orchestrator_trace_step = 0u;
""",
        """static FILE *g_orchestrator_trace_file = NULL;
static uint64_t g_orchestrator_trace_step = 0u;
static const char *g_native_snapshot_path = NULL;
""",
    )
    replace_once(
        main_c,
        """            printf("Continuous recovered instructions:  %llu\\n",
                   (unsigned long long)(
                       bridge_steps +
                       third_report.native_recovered_instructions
                   ));
            printf("Final CPU and memory state:         MATCH\\n");
        }
""",
        """            printf("Continuous recovered instructions:  %llu\\n",
                   (unsigned long long)(
                       bridge_steps +
                       third_report.native_recovered_instructions
                   ));
            printf("Final CPU and memory state:         MATCH\\n");

            if (native_fifth_dispatch && g_native_snapshot_path != NULL) {
                vf2_i960_snapshot output_snapshot;
                vf2_i960_snapshot_init(&output_snapshot);
                status = vf2_i960_snapshot_capture(
                    &output_snapshot,
                    &native_cpu,
                    &native_machine
                );
                if (status == VF2_OK) {
                    status = vf2_i960_snapshot_write_file(
                        &output_snapshot,
                        g_native_snapshot_path
                    );
                }
                vf2_i960_snapshot_destroy(&output_snapshot);
                if (status == VF2_OK) {
                    printf("Fifth-dispatch snapshot:            %s\\n",
                           g_native_snapshot_path);
                } else {
                    fprintf(stderr, "Could not write fifth-dispatch snapshot: %s\\n",
                            vf2_status_string(status));
                }
            }
        }
""",
    )
    replace_once(
        main_c,
        """    if (strcmp(argv[1], "native-fifth-dispatch") == 0 && argc == 3) {
        return command_native_fifth_dispatch(argv[2]);
    }
""",
        """    if (strcmp(argv[1], "native-fifth-dispatch") == 0 &&
        (argc == 3 || argc == 4)) {
        g_native_snapshot_path = argc == 4 ? argv[3] : NULL;
        return command_native_fifth_dispatch(argv[2]);
    }
""",
    )

    append_once(
        root / "README.md",
        "## Snapshot endurance runner",
        """## Snapshot endurance runner

The strict fifth-dispatch command can optionally persist its proven native
boundary as a versioned snapshot:

```sh
build/vf2i960 native-fifth-dispatch /path/to/vf2 fifth-dispatch.vf2snap
```

`vf2cycles` restores that exact CPU and mutable Model 2 state into independent
reference and recovered-native machines, then executes repeated scheduler
cycles in differential lockstep:

```sh
build/vf2cycles \\
  --rom-dir /path/to/vf2 \\
  --snapshot fifth-dispatch.vf2snap \\
  --cycles 10 \\
  --min-blocks 1 \\
  --max-blocks 16384
```

The command stops at the first unsupported native block, reference execution
failure or state mismatch and prints the partial cycle, last recovered step and
first differing component. A successful run never interprets instructions on
the native side.
""",
    )

    append_once(
        root / "docs/STATUS.md",
        "## Endurance tooling",
        """## Endurance tooling

The differential layer now has two reusable pieces for the next v0.2.0 step:

- `vf2i960 native-fifth-dispatch ROM_DIR OUTPUT.vf2snap` persists the exact
  validated fifth-`fa_game_info` boundary; and
- `vf2cycles` restores that boundary and executes a requested number of complete
  repeated-address cycles with per-block CPU and mutable-memory comparison.

This tooling does not extend the ROM-proven boundary by itself. Its first
expected failure identifies the concrete unsupported block that must be
recovered before a strict sixth-dispatch contract can be recorded.
""",
    )


if __name__ == "__main__":
    main()
