# v0733c: two container bounds measured and corrected; 15 of 20 are still wrong

**v0733b made the coverage *ranking* sound. It did not make `functions.csv`
correct.** This slice measures what can be measured and corrects exactly two
rows, then records why the other eighteen were left alone.

## The tool available is a lower bound, not an extent oracle

`vf2i960 function <rom> <address>` walks to a `ret` and prints
`address=... end=... blocks=...`. It is the obvious candidate for repairing the
container bounds, and it is **not sufficient on its own**. Run over all 20
container rows:

```text
name                             csv_start  csv_end    measured
task_camera                      0x0001d320 0x0001eff0 0x0001eff0  agrees
camera_post_update_gate          0x0001d660 0x0001d984 0x0001ee34  agrees
frame_shadow_verify              0x00000530 0x00009ffc 0x000006e8  csv end is a container bound
texture_stream_header_call       0x0004be6c 0x0004c180 0x0004bfe0  csv end is a container bound
texture_status_dispatch_call     0x0004bd24 0x0004d2c0 0x0004bfe0  csv end is a container bound
texture_active_prepare_call      0x0004bde0 0x0004d16c 0x0004bfe0  csv end is a container bound
texture_status_scan_end          0x0004bd24 0x0004bf90 0x0004bfe0  agrees
texture_child_zero_gate_a        0x0004bebc 0x0004cb64 0x0004bfe0  csv end is a container bound
texture_child_zero_gate_b        0x0004bef4 0x0004cd18 0x0004bfe0  csv end is a container bound
texture_final_status_call        0x0004bf90 0x0004d25c 0x0004bfe0  csv end is a container bound
texture_orchestrator_save_call   0x0004bb18 0x0004bcd4 0x0004bcd4  agrees
texture_frame_gate_call          0x0004bcd4 0x0004bfe0 0x0004bfe0  agrees
video_register_compose           0x00001064 0x0000cfc0 0x00001200  csv end is a container bound
video_input_latch_write          0x00001290 0x0000cfcc 0x000012bc  csv end is a container bound
input_bit0_sequence_gate         0x00001e6c 0x0000cfd8 0x00001edc  csv end is a container bound
input_bit1_sequence_gate         0x00001edc 0x0000cfdc 0x00001f4c  csv end is a container bound
input_ring_poll                  0x000012d8 0x0000cfe0 0x00001314  csv end is a container bound
main_texture_orchestrator_call   0x0000a030 0x0004bcd4 0x0000a048  csv end is a container bound
main_frame_timer_call            0x0000a034 0x00010f90 0x0000a048  csv end is a container bound
interrupt_return_wait_exit       0x00000d20 0x00010fa4 0x00000d24  csv end is a container bound

containers where the measured end is smaller than the CSV end: 15 of 20
```

Two results are absurd and prove the tool is a **lower bound** rather than an
extent:

- `interrupt_return_wait_exit` measures **4 bytes** — yet its note says it
  "returns from the interrupt, records the resumed wait visit and exits on the
  changed frame byte", which cannot be four bytes.
- `main_texture_orchestrator_call` measures **24 bytes** — yet it is the 269 KB
  orchestrator wrapper the v0733 note singled out as the thing `is_wrapper`
  exists to suppress.

So the tool finds the first `ret` reachable by its analysis and stops. For a
routine with several exits, or a dispatcher whose prologue is reached through a
path the analysis does not follow, that is not the function's extent.

**Therefore no bulk rewrite.** Taking these 15 measurements at face value would
replace 15 wrong bounds with 15 possibly-wrong bounds and lose the ability to
tell which is which. That is a worse CSV, not a better one.

## The convention, confirmed on the two that are sound

Where the tool's answer is corroborated, `end` is **one past the final `ret`**,
i.e. the next function's start:

```text
000011f8  call     0x00001200
000011fc  ret
00001200  ld       0x00500700, r8        <- next function, not part of this one
```

and

```text
000012b0  b        0x000012b8
000012b8  ret
000012bc  mov      2, r15                <- next function
```

## The two rows corrected

Each has **two independent sources**: the `ret` boundary above, and the CSV's
own `notes` instruction count.

### `video_register_compose`

```text
was: 0x00001064,0x0000cfc0   (48 988 B)
now: 0x00001064,0x00001200   (412 B)
ret at 0x11fc; 0x1200 starts the next function.
```

`notes` says *"sixty-three instructions"*. The listing over
`0x1064..0x1200` contains **85 static instructions** with 16 basic blocks — a
static/executed count of 85/63, which is the ordinary relationship. 412 bytes
fits; 48 988 bytes is three orders of magnitude out.

### `video_input_latch_write`

```text
was: 0x00001290,0x0000cfcc   (48 444 B)
now: 0x00001290,0x000012bc   (44 B)
ret at 0x12b8; 0x12bc starts the next function.
```

`notes` says *"returns in seven instructions"*; the listing has **8 static
instructions** in 4 blocks. Same relationship. 44 bytes fits; 48 444 does not.

Both rows carry the correction and its provenance in their `notes`, so the CSV
is self-documenting rather than silently amended.

### What changed downstream

Container rows: **20 -> 18**. `video_register_compose` is now a leaf and appears
in the ranking at 412 bytes / 103 uncovered words, in sixth place:

```text
boot_stage_2                     recovered                      892    ...  0x000001b0..0x0000052c
texture_maintenance              recovered-observed-branch      704    ...  0x0004b8d8..0x0004bb98
camera_viewport_construct        recovered-control-block        624    ...  0x0001d678..0x0001d8e8
texture_header_decode            recovered-observed-branch      624    ...  0x0004c180..0x0004c3f0
color_table_rebuild              recovered-rom-anchor           428    ...  0x00002c38..0x00002de4
video_register_compose           recovered-observed-branch      412    ...  0x00001064..0x00001200
```

## Incidental find: unrecovered code between the two

Correcting the bounds exposes gaps in the table that the container bounds were
hiding:

- `0x00001200..0x00001290` — 144 bytes, starts with `ld 0x00500700, r8`
- `0x000012bc..0x000012d8` — 28 bytes, starts with `mov 2, r15`

Both sit between the corrected rows and the next CSV entry, and neither has a
row. The 144-byte one is large enough to be a real routine. **No claim is made
about it** — it is recorded as a target, not characterised.

## Standing rule this adds

**A lower bound is not a measurement of the thing you want.** `vf2i960 function`
gives the first `ret`; the CSV wants the function's extent. Where the two differ
by three orders of magnitude the tell is the file's *own* prose, and a bound is
only repaired when a second source agrees. Fifteen rows are still wrong, the
table now says which, and a repaired row carries its provenance in its `notes`.

Corollary for the tool: `vf2i960 function` should be documented as a
first-return walk, not an extent oracle. Until someone measures the multi-exit
cases properly, a reader who trusts it will shrink real functions to a few
bytes.

**The trap fired a second time the same day, on `0x23524`.** See
`coli_shell_contract_v0733d.md`: `vf2i960 function roms/vf2 0x23524` reports
`end=0x2364c`, which is `0x235a4`'s **callee**. The block's real boundary is
`0x22210` at 9151 instructions, and a test written against the tool's answer
invented a +1 defect in a recovery that was correct. The same lower bound
produced a correct CSV repair here and a fabricated bug there; the difference is
only whether a second source was consulted.

## Validated

- `test_block_coverage.py`: 17/17 (the real-CSV loader still sees 69 entries).
- `ctest -R vf2_python_factory`: 14/14.
- Full non-dominator suite: **117/117 in 255.90 s**.

## Still open

- 18 container rows, 13 of them still carrying a container bound. Each needs its
  own measured extent, not a bulk rewrite.
- The 144-byte and 28-byte gaps above.
- `coverage_ratio` is `0.00` for every ranked row — the v0729–v0732 corpus does
  not reach these addresses. The tool orders candidates; it does not certify
  them.
