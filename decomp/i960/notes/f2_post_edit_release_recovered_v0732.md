# v0732 F2: rows 2-4 post-edit release frames - recovered and proven

Frontier item **F2** from `v0730_first_action_runbook.md` ("rows 2-4
post-edit release frames") is now **recovered**. All three measured release
frames print `Snapshots match.` on both the register and memory-region gates,
with the instruction and call counters equal to the reference.

This supersedes `f2_post_edit_release_measured_v0731.md`, which recorded the
same frames as measured-but-fail-closed. The measurement text there is still
correct; only the conclusion changed.

## Result

| leg | a5 | `preset` | reference | native | registers | memory |
|---|---|---|---|---|---|---|
| `f2-r2` | 2 | 0 | 4422 ins / 41 calls | 4422 / 41 | MATCH | MATCH |
| `f2-r3` | 3 | 0 | 4421 ins / 41 calls | 4421 / 41 | MATCH | MATCH |
| `f2-r4` | 4 | 1 | 4418 ins / 41 calls | 4418 / 41 | MATCH | MATCH |

Controls re-verified unchanged by this slice:

| leg | a5 | reference | native | result |
|---|---|---|---|---|
| `f2-r3-edit` | 3 | 4637 / 44 | 4637 / 44 | MATCH |
| `f2-r4-edit` | 4 | 4635 / 43 | 4635 / 43 | MATCH |
| `f2-r4-cl` | 4 | 4425 / 41 | 4425 / 41 | MATCH |
| `f1-a5-cl-0x4` | 5 | - | - | still fails closed |

## The harness trap that had to be retracted first

`vf2i960 native-resume` is **native only**. It restores the snapshot and
calls `vf2_native_runtime_run_until`; it never invokes the reference
executor (`tools/vf2i960/commands.c:3054`). An early version of the harness
ran it twice - once labelled "reference", once labelled "native" - and
reported `Snapshots match.` for all three rows. That was a self-comparison.

Two independent signals exposed it:

1. the reference leg reported the *same* instruction count as the native leg
   on every row, including rows where a tile difference was already known;
2. the reference leg's numbers did not move when the recovered render changed.

The correct reference leg is `vf2probe --until 0xa010`, which drives the i960
reference executor over the ROM. Per-frame deltas come from running the same
snapshot twice with `--until 0x9ff8` (the entry IP, so `run_instructions`
deltas to 0) to read the start counters, then `--until 0xa010`.

Instruction counts are part of the contract, not decoration:
`tests/recovered/test_phase17_zero.c:611` asserts both
`reference_cpu.executed_instructions` and
`bridge_report.recovered_instruction_count` against the same expected value.

## What was recovered

### 1. The latch shape (`post_edit_release`)

```c
post_edit_release = (test_held_entry != 0 &&
                     released_flags == UINT32_C(4) &&
                     previous_flags == UINT32_C(0x0f000004));
```

Identified by shape, not by the values it carries, then validated against the
measured derivation rather than waved through.

### 2. The credit derivation gate

For the post-edit release the credits are no longer all 2. They are the
measured derivation from v0731j: `credits[0]` and `credits[3]` are clamped
counters in `[0, 14]`, and the two derived bytes come from the ROM tables at
`0x5bc74` / `0x5bc84`. The gate checks exactly that - counter bounds plus
table consistency - rather than relaxing to "any value".

`preset` is admitted only for the `a5 == 4` row and only for values 0 and 1,
the two that are actually measured, and only in COMMON mode. Everything
wider fails closed.

### 3. The four credit digit cells (rows 6-9)

`(6,40) <- credits[0]`, `(7,40) <- credits[2]`, `(8,40) <- credits[4]`,
`(9,40) <- credits[5]`, each written as `0x8000 | ('0' + value)`. When the
value is 1 the trailing `S` of that row's `CREDITS` label is blanked to
`0x8020`, so the label reads `CREDIT`.

The first attempt wrote the bare numeric digit and a raw `0x0020` blank.
Both were wrong and the differential said so immediately: 5 differing bytes
at `0x350`, `0x3d0`, `0x450`, `0x4d0`, `0x4e0`. Tile cells are
`0x8000 | ASCII`, the same convention as `write_phase17_index0_text`
(`texture_bridge_match.c:1538`), and a blank is `0x8020`, not `0x0020`.

### 4. The COIN/CREDIT SETTING live re-render (a5 = 4)

The `runs[]` table is the *parked* text. When the COIN/CREDIT SETTING row is
the one being released, the ROM re-renders that section from live data.
Three cells differ, and all three come from the same mechanism.

Traced in `out/f2-r4-rel.jsonl`:

- **row 11 column 45** carries the 1-based chute number. `runs[]` hardcodes
  `"1"`, which is only right at `preset == 0`. The value cell is at
  column 45 because the label is `COIN/CREDIT SETTING     #  N`.
- **rows 13 and 24** re-emit the **plural** label `"  COINS   CREDITS"`
  (17 cells from column 31) and then stamp a packed pair over it. The static
  `"1 COIN  1 CREDIT "` in `runs[]` is exactly this render over the default
  banks, which is why the parked frames never showed the difference.

The stamper is `sub_00060d30`. `ldob (r5), r6` at `0x060d30` loads a packed
byte; the high nibble is the coin count and the low nibble the credit count:

```text
0x060d70  shro 4, r6, r9      r9 = coins
0x060d74  and  15, r9, r9
0x060d78  cmpibne 1, r9, ...   if coins == 1, blank the trailing 'S'
0x060d94  stos r10
0x060db4  stos r9             stamp the coin digit at column 31
0x060db8  and  15, r6, r9      r9 = credits
0x060dbc  cmpibne 1, r9, ...   if credits == 1, blank the trailing 'S'
0x060dd8  stos r10
0x060df8  stos r9             stamp the credit digit at column 39
```

Bank 0 (row 13) uses the `0x060c5c`/`0x060c7c`/`0x060ca0`/`0x060cc0`
instantiation of the same shape, reached by the same `bx` dispatch.

The table is a per-chute slot array at `0x61550` with **stride 2**, read at
`0x61550 + 2 * chute`:

```text
0x61550 = 0x11   chute 1   1 coin, 1 credit
0x61551 = 0x00            reserved
0x61552 = 0x12   chute 2   1 coin, 2 credits
0x61553 = 0x00            reserved
0x61554 = 0x13   chute 3   1 coin, 3 credits
...
0x6155a = 0x16   chute 6
```

The first row of a bank reads the even slot; the remaining four rows read
the odd (zero) slot, hit `cmpibe 0` and take the `0x060e00` terminator, which
is why rows 15-21 and 26-32 stay blank. The `ldob` sites in the trace are
exactly that walk: `0x61552` once, then `0x61553` four times.

The singular form is the same "blank the S when the value is 1" rule already
used for rows 6-9, which is why one mechanism covers all of it.

### 5. Per-row body lengths

Measured with `vf2probe` and pinned **only** on the post-edit-release latch,
so the shared `4190` / `4193` branches keep their own proven values:

```c
} else if (status == VF2_OK && post_edit_release) {
    instructions = phase_a5 == UINT8_C(2)
        ? UINT64_C(4190)
        : (phase_a5 == UINT8_C(3) ? UINT64_C(4189) : UINT64_C(4186));
}
```

These are **pinned measurements, not a derived rule**. The per-row cause of
the 4189 / 4186 deltas has not been disassembled. The 232-instruction prefix
is unchanged; only the body length differs per row.

## Reproducing

```sh
pwsh -NoProfile -Command "& 'out/f2diff.ps1' -Tag f2-r2,f2-r3,f2-r4"
pwsh -NoProfile -Command "& 'out/f2run.ps1' -Snap f2-r3-edit"
```

`out/f2diff.ps1` reports the full contract (instruction delta, call delta,
register equality, memory-region equality) and refuses to label a leg
`COUNT-OK` unless both counters agree. `out/f2run.ps1` takes any
`0x9ff8`-boundary snapshot and reports `NATIVE REFUSED` instead of crashing
when the native side declines a leg - that is the fail-closed path, not a
crash.

Both scripts live under `out/` and are deliberately not committed; the recipe
is in this note.

## No CTest for this leg

As with F1, `tests/recovered/test_phase17_zero.c` cannot host this boundary:
its harness enters at depth 1 and the chain blocks rewrite `0x50016c`, so
`base` and the frame depth do not match what this leg needs. Every existing
case in that file has matching ac/cc/depth. A test that asserts less than the
real gate is not shippable, so no test was added rather than a weak one.
The gate is the ROM-backed differential above.
