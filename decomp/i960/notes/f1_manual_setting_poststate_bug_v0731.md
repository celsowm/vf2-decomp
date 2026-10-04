# F1: MANUAL SETTING entry — a real poststate bug in recovered C (v0731)

This session built a working strict differential for mid-menu frames, then
used it to find a **real defect in already-recovered code**. The a5 = 5
MANUAL SETTING entry block is unreachable in the tree (its latch tuple was
absent), so the defect was latent; admitting the tuple exposes it.

Outcome: the block's **shape is exactly right** and its **poststate is
wrong**. The tuple is deliberately left un-admitted so the path stays
fail-closed until the poststate is corrected.

## 1. The differential recipe (validated)

This is the reusable part. It works for any selector-17 menu frame and was
confirmed against a leg the project already proves.

The router entry for a whole frame is
`VF2_MAIN_FINAL_CLUSTER_ENTRY = 0x00009ff8`
(`src/recovered/texture_bridge_internal.h:96` region / used at
`texture_bridge_match.c:22686`-`22706`). The cluster chains
`shadow_verify (0x530)` -> `buffer_gate (0x110b0)` ->
`geometry_command_setup (0x2f5c)` -> `scratch_clear (0xa154)` ->
`dispatch_tick (0xa6c0)`, each with an `enter_procedure`, and exits at
`0x0000a010`. `0x0000a6c0` and `0x00010b5c` are **not** router entries:
`VF2_FRAME_DISPATCH_TICK_ENTRY` is only a *reported* address inside the
selector-0/2 handlers, and the `target != 0x10b5c` check at
`texture_bridge_match.c:21244` is *inside* the phase dispatcher.

Recipe, from any wait-state snapshot at a given `phase_a5`:

```sh
# 1. settle to a wait state with the current latches
build/Debug/vf2probe.exe --rom-dir roms/vf2 --snapshot <walk.vf2snap> \
  --max-steps 4000000 --until 0x00010f98 \
  --set-u32 0x00500700=0x0f000000 --set-u32 0x00500704=0x0 \
  --set-u32 0x00500708=0x0 --set-u32 0x0050070c=0x0f000000 \
  --output-snapshot out/wait.vf2snap

# 2. inject the frame IRQ and stop on the cluster entry
build/Debug/vf2probe.exe --rom-dir roms/vf2 --snapshot out/wait.vf2snap \
  --max-steps 4000000 --raise-irq 0x1 --enter-interrupt 12=1 \
  --until 0x00009ff8 --output-snapshot out/cl.vf2snap

# 3. patch the latch AT the cluster entry (mutations apply before execution,
#    and --until 0x9ff8 halts in 0 instructions, so the patch survives)
build/Debug/vf2probe.exe --rom-dir roms/vf2 --snapshot out/cl.vf2snap \
  --max-steps 1 --until 0x00009ff8 \
  --set-u32 0x00500700=0x0f000004 --set-u32 0x00500704=0x00000004 \
  --set-u32 0x00500708=0x0 --set-u32 0x0050070c=0x0f000000 \
  --output-snapshot out/entry.vf2snap

# 4. run both sides to 0xa010 and compare
build/Debug/vf2i960.exe native-resume roms/vf2 out/entry.vf2snap 1 0 0x0000a010 out/native.vf2snap
build/Debug/vf2probe.exe  --rom-dir roms/vf2 --snapshot out/entry.vf2snap \
  --max-steps 4000000 --until 0x0000a010 --output-snapshot out/ref.vf2snap
build/Debug/vf2i960.exe compare-snapshots out/native.vf2snap out/ref.vf2snap
```

Two traps:

- The latch must be patched at the **cluster entry**, not before it. The
  game's own input code re-latches `0x500700`/`0x500704` every frame, so a
  patch applied at a wait state is overwritten before `0x9ff8` (measured:
  patched `0x0f000004` came back as `0x0f000000`).
- `vf2probe --input <mask>` drives the host input but does **not** reproduce
  the natural walk latches: `--input 0x4` latched `0x0f001004` / nav
  `0x8000`, not `0x0f000004` / nav `0x4`. Use memory patches at `0x9ff8`.

## 2. Control: the recipe is sound

Run on the already-proven `a5 = 4` TEST value edit:

```text
native-resume ... 1 0 0x0000a010 out/native-a4.vf2snap
  Native resume: blocks=1 instructions=4635 entry=0x00009ff8 exit=0x0000a010
    calls=43 returns=43
reference vf2probe --until 0x0000a010  ->  run_instructions 4635
compare-snapshots out/native-a4.vf2snap out/ref-a4.vf2snap
  Snapshots match.
```

Instruction count, call count and the whole snapshot all agree. So
`compare-snapshots` on a native run versus a `vf2probe` run **is** a valid
strict comparison for these frames.

## 3. The defect

Same recipe on the `a5 = 5` MANUAL SETTING entry, with the candidate tuple
temporarily added to `natural_latches[]`:

```text
native-resume ... 1 0 0x0000a010 out/native-a5.vf2snap
  Native resume: blocks=1 instructions=14295 entry=0x00009ff8 exit=0x0000a010
    calls=42 returns=42
reference vf2probe --until 0x0000a010  ->  run_instructions 14295
compare-snapshots out/native-a5.vf2snap out/ref-a5.vf2snap
  Snapshots differ in registers at offset 0xe: expected=0x1 actual=0x1d
  (550 differences)
```

What is correct:

- **Instruction count is exact**: 14295 native vs 14295 reference, which is
  the recovered `14063` body plus the 232-step cluster prefix.
- **Call count is exact**: 42 native vs a reference delta of 42, which is
  the recovered `36` body calls plus 5 prefix calls and 1 block return.
- The cluster entry, the exit at `0xa010`, and the whole dispatch chain run
  natively without reaching any other guard.

What is wrong: the synthesized **poststate**. 550 register-section
differences, the first at offset `0xe` (`expected = 0x1`,
`actual = 0x1d`). The suspect is the register block in the a5 = 5 branch,
`texture_bridge_match.c:8240`-`8274`, which hard-codes `r14 = 1`,
`r16 = 62`, `r25 = 0x01001580`, `arithmetic_control = 2` /
`compare_result = EQUAL`, and the rest of the flat `g1..g31` set. Those
pins were never differentially proven for this leg — the branch was
unreachable.

## 4. Disposition

The candidate tuple

```c
{UINT32_C(0x0f000004), UINT32_C(0x0f000000), 0u, UINT32_C(0x4), UINT8_C(5), 0u, 0u}
```

is **not** in the table. A comment at the a5 = 4 TEST row records why. The
gate reported a mismatch, so per AGENTS.md rule 1 an unproven admission
must not be added and rule 5 forbids weakening validation to make a
recovery pass.

To finish: dump the reference poststate at `0xa010` field by field
(`vf2recover`, or a field-level snapshot diff — `compare-snapshots` only
prints a summary), correct the pins in the a5 = 5 branch, and re-run the
recipe until `compare-snapshots` reports a match. The instruction and call
accounting already agree, so only the poststate needs work.

## 5. Validation

- `cmake --build build --config Debug` clean.
- After the revert: `vf2_phase17_zero_differential`,
  `vf2_native_sixth_dispatch`, `vf2_texture_bridge_differential`,
  `vf2_native_differential`, `vf2_python_factory_chain` — 5/5 passed.
- Full 117/117 ctest was green earlier in this session; the only source
  change since is the 8-line comment.

## 6. Anti-traps

- Do not treat "the block ran natively and the instruction count matched" as
  proof. The poststate was wrong by 550 fields.
- Do not patch the latch before the cluster entry; the input code rewrites
  it. Patch at `0x9ff8`.
- Do not use `--input` to synthesise the natural walk latches; it produces
  a different encoding.
- Do not use `0x0000a6c0` or `0x00010b5c` as resume points. The router
  entry is `0x00009ff8`.
- Always run the control (`a5 = 4`) before believing a `compare-snapshots`
  result.
