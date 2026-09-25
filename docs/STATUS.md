# Status

## Current master — v0.1.3

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Validated locally; GCC/Clang CI configured |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Repeated-frame corridor | 7,404,913 instructions across 866 differential blocks |
| Continuous recovered instructions | 8,675,735 |
| Native-side interpreted instructions on accepted paths | 0 |
| Second through fourth scheduler sweeps | Completely recovered for the observed corridor |
| Fifth scheduler entry | Recovered and ROM-validated |
| Current ROM-proven native boundary | Sixth `fa_player` bootstrap, the accepted `0x14288 -> 0x1428c` / `0x19ef8` corridor, the observed `0x1428c -> 0x142c0` geometry expansion, `0x142c0 -> 0x14310` setup corridor, `0x14310 -> 0x143e4` preamble, state-neutral prefix through `0x143fc`, `0x1ab74 -> 0x1abf4` entry prefix, `0x27ce0 -> 0x27d00` entry prefix, the immediate `0x27d00 -> 0x28184`, `0x28268 -> 0x28780`, `0x27d90`/`0x27dcc`/`0x27fa0 -> 0x2901c` and `0x28174 -> 0x29414` calls, `0x28184 -> 0x28268` prefix, accepted `0x28780` geometry body and measured `0x14400 -> 0x17710`, `0x14404 -> 0x1791c`, `0x14408 -> 0x4b640`, `0x14414 -> 0x16504` and `0x14418 -> 0x180bc` chain after `fa_game_info` at `0x0001645c` |
| Fighter-state bit-31 runtime path | Native dispatcher; observed `0x18144` corridors through `0x18d44`, `0x18c64` and `0x18640`; controlled `0x181c0` bodies, observed state-4/bit-15 and non-state-4 bit-15 prefixes, shared `0x18e08`/`0x18e00` helpers and the state-4/state-8 `0x18644` flag paths are native. The state-4 ROM matrix is exact for all 192 cases across flags 6/14/15/16 at threshold `-1` as well as non-negative thresholds, with the complete negative matrix now native; the state-8 matrix is exact for 96 cases with flag bits 1/2/4 and 192 cases with bit 8 included, and its negative matrix is now also native and exact for all 192 cases across bits 1/2/4/8; high bits 21/26/29/30 are exact in isolated, bilateral and aggregate combinations, including all masks 248..255 (bit 8 plus every subset of bits 1/2/4), with 12 exact distributions per mask; the observed `fa_player` bootstrap, accepted `0x19ef8` corridor through `0x1428c`, geometry expansion through `0x142c0`, setup through `0x14310`, preamble through `0x143e4`, state-neutral prefix through `0x143fc`, `0x1ab74` entry through `0x1abf4`, `0x27ce0` entry through `0x27d00`, the `0x27d00 -> 0x28184`, `0x28268 -> 0x28780`, `0x27d90`/`0x27dcc`/`0x27fa0 -> 0x2901c` and `0x28174 -> 0x29414` calls, `0x28184 -> 0x28268` prefix, accepted `0x28780` geometry body and measured `0x14400 -> 0x17710`, `0x14404 -> 0x1791c`, `0x14408 -> 0x4b640`, `0x14414 -> 0x16504` and `0x14418 -> 0x180bc` chain are also native, while later unobserved branches remain explicit ROM-backed boundaries |
| State-8 ten-bit matrix | Bits 6/21/26/29/30/31 plus bits 1/2/4/8 are exact at threshold `-1`: 12,288 fixtures (1,024 masks × 12 distributions). The nine-bit subset without bit 6 is exact at threshold `0`; positive bit 6 is additionally exact for the complete no-bit-8 bits-1/2/4/6 submatrix (192 fixtures), eight full-high masks with bit 8 covering every subset of low bits 1/2/4 (96 more fixtures), and the measured bit-8 + bit-6 + bit-14, bit-15, bit-16, bit-14 + bit-15, bit-14 + bit-16, bit-15 + bit-16, bit-14 + bit-15 + bit-16, bit-14 + bit-21, bit-15 + bit-21, bit-16 + bit-21, bit-14 + bit-15 + bit-21, bit-14 + bit-16 + bit-21, bit-15 + bit-16 + bit-21 and bit-14 + bit-15 + bit-16 + bit-21 compositions, plus the four measured bit-14, four measured bit-15 and four measured bit-16 cross-family compositions and the four measured bit-14 + bit-15, bit-14 + bit-16, bit-15 + bit-16 and bit-14 + bit-15 + bit-16 cross-family compositions with high bits 26/29/30/31, the six measured bit-14 + bit-15 pairwise high-bit compositions from high bits 26/29/30/31, the six measured bit-14 + bit-16 pairwise high-bit compositions and six measured bit-15 + bit-16 pairwise high-bit compositions from high bits 26/29/30/31, the eight measured bit-14 + bit-16 and bit-15 + bit-16 triple-high compositions and the two measured all-four-high compositions from high bits 26/29/30/31, the four measured bit-14 + bit-16 and four measured bit-15 + bit-16 pairwise compositions combining bit 21 with high bits 26/29/30/31, the four measured bit-14 + bit-15 pairwise high-bit compositions combining bit 21 with high bits 26/29/30/31, with all six bit-14 + bit-15 extensions 21+26+29, 21+26+30, 21+26+31, 21+29+30, 21+29+31 and 21+30+31 now native, the four measured bit-14 + bit-15 triple-high compositions 26+29+30, 26+29+31, 26+30+31 and 29+30+31, the six measured bit-14 + bit-15 + bit-16 high-bit extensions 21+26, 21+29, 21+30, 21+31, 21+26+29 and 21+26+29+30+31, with all six bit-14 + bit-16 extensions 21+26+29, 21+26+30, 21+26+31, 21+29+30, 21+29+31 and 21+30+31 now native, the six measured bit-15 + bit-16 extensions 21+26+29, 21+26+30, 21+26+31, 21+29+30, 21+29+31 and 21+30+31, isolated bit-6 masks for high bits 21/26/29/30/31, all ten pairwise combinations of those five high bits, and the measured high-bit aggregates 26+29 and 21+26+29+30+31 (1,512 unique fixtures total). Other positive bit-6/high-bit compositions remain explicit unsupported boundaries. |
| `vf2i960 native-third-dispatch` | `MATCH`: 42 blocks / 55,239 instructions |
| `vf2i960 native-fourth-dispatch` | `MATCH`: 78 blocks / 58,869 instructions |
| `vf2i960 native-fifth-dispatch` | `MATCH`: 830 blocks / 7,402,741 instructions |
| `vf2i960 native-sixth-dispatch` | `MATCH`: 866 blocks / 7,404,913 instructions |
| Repeated-cycle differential API | Strict per-block + cycle-boundary probe + resumable checkpoints |
| ROM-independent / ROM-backed CTest targets | 18 / 28, 46 total |
| Model 2A common bus/device boundary | Measured timer down-counters at 25 MHz with timer IRQ requests, video frame/status latch and render-mode control; TGP program-upload accounting is retained while the guest program-window read remains the measured all-ones boundary; full TGP microcode/rasterization, tile timing and SCSP FM/DSP remain explicit open boundaries |
| 68000 audio vectors, board map, voice maintenance and command dispatcher | Voice maintenance plus bounded 0x80, zero/nonzero-stream 0x90 lookup/no-live return, both 0x11d0 voice-helper branches, the populated-table 0x90 allocator prefix, live B0 0x1/0x2 packed SCSP +0x13 writes, 0xa0 stream-descriptor initialization, 0x1f7c normal/C-D packets, B0 0x10 control, F0 wait/search, FF/F7 skips, FF/2F sentinels and ordinary/F1 pointer re-entry, 0xc0, 0xe0 and selected 0xb0 command/ring paths integrated; allocator sample-table copy, high-bit stream controls, other handlers and synthesis remain open |
| TGP scalar services and host boundary | Recovered; stateful direct/object geometry reference executor with matrix/viewport/depth submission added; native Model 2A coprocessor writes are captured into the TGP stream with flat-RAM fallback; packet decoder/microcode not recovered |
| Geometry command packing and ring commit | Recovered for observed four-entry boundary |
| SCSP CPU bus, register, sample and MIDI boundary | Recovered 0x1000-byte register window, visible sound map, deterministic PCM slot renderer and slot ADSR lifecycle; FM/DSP fidelity remains open |
| Portable framebuffer/input backend | Recovered headless software surface, deterministic P1/P2 input injection and game-facing graphics/audio frame lifecycle; window/audio device adapters remain open |
| Playable port | No |

## Development head after v0.1.3 acceptance

The differential layer now exposes `vf2_native_differential_run_cycles`, which
runs any requested number of complete lockstep cycles back to the same task
entry. Each non-zero cycle enforces a minimum block count, preventing an
immediate false success when the start and destination addresses are identical.
The aggregate report preserves completed-cycle, block and instruction totals as
well as the partial final cycle when an unsupported state or mismatch is found.

This infrastructure now backs both the strict repeated-frame endurance command
and the faster cycle-boundary probe. It does not move the strict ROM-proven
per-block boundary by itself: probe-only cycles are scouting evidence until the
corresponding interval is replayed under the strict differential contract.

## Proven scope

The native path runs continuously from the completed first scheduler sweep
through the original 1,270,822-instruction post-frame bridge, the complete
observed second, third and fourth scheduler sweeps, three repeated frame
boundaries and the fifth scheduler entry. The fifth `fa_game_info` task is
reached at `0x0001645c`, registry `0x00515200`, without native-side i960
interpretation.

The v0.1.3 differential contract is:

- 866 compared repeated-frame blocks;
- 7,404,913 reference and recovered-native instructions;
- 8,675,735 continuous recovered instructions including the historical bridge;
- three repeated scheduler entries and finishes;
- twelve repeated scheduler transitions;
- six frame-wait phases; and
- exact CPU, architectural local-frame, execution-counter, frame-event and all
  mutable Model 2 memory equality after every accepted block.

The extension beyond v0.1.2 includes the observed:

- texture status scan with nine inactive records before the live tenth record;
- non-zero texture stream dispatch and five mip levels;
- 43,648 source bytes expanded into 87,296 texture bytes;
- zero-value texture-counter completion path;
- stream-resume gate with no pending transfer; and
- mode-17 system-memory diagnostic profile, which executes one instruction more
  than adjacent mode 16.

Run the strict sixth-entry acceptance with:

```sh
vf2i960 native-sixth-dispatch /path/to/vf2
```

The older third- and fourth-dispatch commands remain independent regression
contracts.

## Validation

The focused core, game, and native-runtime targets pass locally against the
supported 36-file ROM set, and the full 49-target GCC/Ninja Release CTest run
is green, including the previously failing post-boot input-profile
differential (three recovered blocks left stale comparison state after the
reference executor learned the architectural condition effects of compare-
and bit-branch instructions; they now reproduce the measured poststates).
The full MSVC/Ninja Release CTest run is a separate, still-open
toolchain/test-harness issue: the optimized TGP executable and several
ROM-backed observation targets terminate with access violations there, while
the sanitizer TGP build passes. That issue is separate from the native
runtime corridor and must be resolved before claiming a clean full suite on
that toolchain.

Public CI cannot contain the proprietary ROM set, so GitHub Actions covers the
warning-as-error GCC/Clang builds and sanitizers while strict ROM-backed
acceptance is run locally against legally obtained files.

The reference i960 executor remains a differential oracle only. It advances by
the exact instruction count reported by each recovered block and never supplies
state to the native side. Unknown addresses and unobserved branches continue to
return `VF2_ERROR_UNSUPPORTED`.

## Roadmap position

v0.1.3 completes the extension from the fourth scheduler entry to the fifth.
The active stage remains v0.2.0 game subsystems. The immediate execution target
is the complete fifth scheduler sweep and longer repeated-frame endurance runs,
while replacing evidence-specific raw-address logic with fighter, object,
match, animation, collision and input types.

TGP polygon rendering, the complete 68000 sound command path, hardware-accurate
SCSP FM/DSP synthesis, gameplay integration and production platform adapters
remain later milestones. The core now has deterministic PCM audio and a
headless framebuffer/input surface for those integrations.

## Endurance tooling

The differential layer now has two reusable pieces for the next v0.2.0 step:

- `vf2i960 native-fifth-dispatch ROM_DIR OUTPUT.vf2snap` persists the exact
  validated fifth-`fa_game_info` boundary; and
- `vf2cycles` restores that boundary and executes a requested number of complete
  repeated-address cycles with per-block CPU and mutable-memory comparison.

The checkpoint now includes a versioned, CRC-protected runtime-state sidecar
covering frame-wait progress and all native aggregate counters. The endurance
runner performs public one-block differential steps and can persist the last
fully matched state immediately before a failure.

ROM-backed endurance now extends the strict proven corridor substantially beyond
the fifth entry. A verified 36/36 ROM set completed 10,000 additional repeated-
address cycles with exact per-block CPU, local-frame, counter, frame-event and
mutable-memory equality: 360,000 additional native blocks and 15,689,445
recovered i960 instructions. The final chained checkpoint is again
`fa_game_info` at `0x0001645c`, with 10,003 scheduler entries and 10,003
injected frame IRQs accumulated by the native runtime.

The strict comparison path now compares live CPU and mutable machine state
directly instead of materializing two temporary full snapshots for every block.
Snapshot capture reuses same-sized region buffers, and equal memory regions use
a `memcmp` fast path while differing regions still fall back to the byte scan
that reports the exact component and first differing offset.

That endurance run exposed and fixed two previously unobserved periodic paths:

- inactive palette upload at `0x00002de4` now preserves the incoming i960
  arithmetic condition because the ROM's `cmpobe` does not modify condition
  codes; and
- the frame-timer prefix at `0x00010f08` now accepts the
  `(frame_counter & 31) == 0` path, skips the previous-minimum load/comparison
  exactly like the ROM, and accounts the 21-instruction path.

`vf2cycles` now also exposes `--boundary-probe` for long-horizon scouting. The
reference executor still advances by the recovered instruction count of every
native block and frame-wait host state is checked when it changes, but complete
CPU/mutable-memory snapshots are compared only when the repeated address closes
the cycle. A failed probe restores both machines and the runtime sidecar to the
exact beginning of the failing cycle, ready for strict per-block replay.
`--output-snapshot` persists a successful boundary plus its `.runtime` sidecar,
so long probes can resume without replaying earlier cycles.

Using the verified ROM set, cycle-boundary probing reached scheduler entry and
frame IRQ 16,384 with complete cycle-end state equality. This scouting result
remains distinct from the now-published 10,000-cycle strict per-block corridor.
It does show that passive repeated-frame execution has settled into a stable
state: the observed mode remains `0x00020000`, phase state/index remain
`0xff`/`0x0b`, and gameplay flags remain zero.

Controlled phase-navigation transitions are now recovered in both directions.
Injecting gameplay bit 13 (`0x00002000`) at the exact pre-`main-final-cluster`
boundary exposes the phase-17 step-back branch in `0x00058fe0`: the reference
i960 decrements phase index `11 -> 10`, writes `0x8020` to the previous
double-indirect phase target and `0x801c` to the new target, and executes 49
frame-dispatch instructions. The `0 -> 11` wrap variant takes 50 instructions.
Injecting a forward-mask bit from `0x08001008` proves the symmetric direction:
`11 -> 0` wraps in 50 instructions and ordinary `10 -> 11` takes 49, with the
forward path taking ROM-accurate priority if bit 13 is also set. Strict replay
now matches the enclosing 281-instruction step-back and 282-instruction forward
`main-final-cluster` variants plus their complete following 36-block scheduler
cycles (2,185 and 2,186 instructions respectively). This is controlled
state-transition evidence, not a claim that passive natural execution reaches
these branches.

The phase-17 reset/display mask `0x04000104` is now recovered from the same
controlled boundary. The ROM sets phase-index bit 7, clears the auxiliary phase
byte, latches `0xff`, zeroes the current object marker, clears the 48x64 tile
plane to `0x0020`, and centers/copies the current phase label. The recovered
dispatch accounts 12,657 instructions with 5 calls / 6 returns; the complete
12,889-instruction `main-final-cluster` is strict-equal, followed by 35 more
equal recovered blocks.

The observed bit-7 continuation is now recovered for phase index `0x8b`. The ROM
clears bit 7 at `0x00059154`, reads table entry `0x0005ff00`, and indirect-
dispatches to `0x0005ef60`. Its first visit executes the recovered game-meter
path, a 15-byte CRC, the 48x64 tile clear and `EXIT TEST MODE` draw. Strict
per-block replay matches the 13,286-instruction frame dispatcher and the complete
13,518-instruction `main-final-cluster`. The subsequent positive countdown path
repeats meter+CRC and matches at 626 dispatcher / 858 cluster instructions.

A resumable cycle-boundary probe starting at counter 320 completes 319 cycles,
11,484 blocks and 701,481 reference/native instructions with equal cycle-end
state and lands at the exact `counter 1` preterminal checkpoint. Strict replay
now continues from there through the terminal `0x0005f07c` transition. The
recovered terminal reproduces the ROM's layer-bit clears, global/gameplay resets,
48x64 tile clear, `0x0006116c` reset-message write and non-returning branch to
`0x000000b0`; the complete enclosing `main-final-cluster` is strict-equal at
13,426 instructions.

That handoff is no longer treated as a cold boot. Boot stage 1 now preserves the
incoming registers/control state not touched by the ROM and strictly matches
1,180,053 instructions from `0x000000b0` to `0x000001b0`. Boot stage 2 preserves
the same warm context, accounts its ROM call/return, and strictly matches another
182,514 instructions to `0x0000052c`. From the preterminal cluster through both
boot stages, reference and recovered-native execution therefore match for
1,375,993 instructions across three native blocks, including CPU, local frames,
procedure counters and all mutable memory.

The unconditional branch at `0x0000052c` and the first `0x00009798` initialization
prefix remain strict-equal through call entry `0x0006dd4c` at 60,078 instructions.
The observed `0x0006dd4c` initializer is now recovered all the way through the
caller boundary at `0x000098b0`: 15 additional strict blocks / 1,498,968
instructions, with complete CPU, local-frame, procedure-counter and mutable-memory
equality after every block. From the phase-17 preterminal checkpoint through the
warm boot and post-boot corridor, the composed proof now covers 2,935,039
instructions without native-side interpretation.

The new corridor includes the 256-entry video ramps, color/palette construction,
two shared descriptor-stream engines, valid backup-SRAM probe/CRC/restore, the
`0x00011b48` table aggregate and the observed hardware-core setup. The register
stream consumes 4 descriptors / 464 32-bit words; the larger block stream
consumes 22 descriptors / 92,672 halfwords and is reused rather than duplicated
inside the aggregate initializer. The restored video controls alter the dynamic
ramp path, so the second pass accounts 11,245 instructions versus 11,563 on the
initial pass.

The texture/graphics continuation now crosses the caller at `0x000098b0`, clears
the six observed texture state/counter words in `0x0004b020`, and reproduces the
timer-threshold prefix in `0x0004afb4`. The existing shared timer/wait helper at
`0x00000b6c` is now composed into this corridor, including its observed positive
delta path and return to `0x0004afdc`. The initializer captures the initial frame
byte and now reproduces the asynchronous status-poll loop at `0x0004afe4`, using
the shared frame-wait event model to inject and resume the interrupt exactly at
the loop boundary. The observed frame-change exit stores status 1 and calls the
early wait helper at `0x00000f7c`. Its odd/high frame-byte polling, interrupt
resumption, zeroing exit and call to the existing `0x00002ec4` video-status latch
are now composed, followed by both returns to `0x0004b07c`. The observed
equal-identity continuation is recovered for another 82 instructions: it checks
the board ID and four graphics-data identities, clears the status halfword,
initializes ten 32-byte texture records and enters `0x0004b820`. Identity
rejections are preflighted before any record write. The four-instruction
`0x0004b820` wrapper is also recovered and enters the nested texture-record setup
at `0x0004b9b8`. Its observed 24-instruction path activates record zero with ID
40, priority 1 and no argument, clears the three texture restart words and
unwinds all three procedure frames to `0x000098b4`. The path preserves the
incoming arithmetic condition because its `cmpob*` instructions do not modify
condition codes, and unsupported priority/record states reject before writes.
The next `0x00011704` call is recovered as a 66x128 luma-table expansion. It
consumes 8,448 ROM bytes, writes the same number of zero-extended 32-bit luma
entries and accounts 50,891 instructions including its caller. Execution now
reaches `0x000098b8`, where the recovered caller enters the shared early frame
wait. Its observed zero-byte exit, video-status latch and return complete at
`0x000098bc`. The `0x00011744` initializer is a run-length geometry-pattern
expander: its first pass makes 2,048 calls to `0x000117a8`, emits 8,192 bytes
through geometry port `0x00804000`, and reaches the shared `0x00002edc` frame
commit after 63,799 instructions, 2,050 calls and 2,048 returns. A direct
ROM-backed reference/native comparison matches complete CPU and mutable memory
at that boundary. The frame commit and following early wait are composed through
`0x0001179c`. The second pass continues the six live decoder registers, emits
another 8,192 bytes in 63,742 instructions with 2,049 calls / 2,048 returns and
reaches the next `0x00002edc` frame commit with terminal word `0x4b4b4b4b`.
The third pass continues the same decoder for 63,700 instructions and reaches
the following frame commit with another 8,192 bytes and terminal word
`0x67676767`. The fourth and final pass emits the last 8,192 bytes in 63,679
instructions with 2,049 calls / 2,048 returns and reaches its frame commit with
terminal word `0x80808080`. Its frame commit and early wait now complete, the
three-instruction loop exit unwinds `0x00011744` to `0x000098c0`, and the caller's
following early wait returns at `0x000098c4`. The `0x000117f8` initializer is
now recovered through its frame commit: it calls the shared geometry mode helper
for modes 3 and 1, clears offsets `0x60` and `0x70`, then emits the 32-entry ROM
command/value table in 281 instructions. Its frame commit and early wait now
complete, followed by the return to `0x000098c8`. The `0x0004ad40` reset clears
the three graphics-state halfwords at `0x005502a8/2b0/2b8` and the word at
`0x00546000`, then returns at `0x000098cc` after 10 instructions. The call into
`0x00007f7c` now copies its six-word ROM video table to `0x00501500` in 16
instructions. The following `0x00007ef0` call copies two three-word constant
groups to `0x00501400` in 8 instructions. Both preserve their exact observed
register and procedure-accounting effects and return through `0x000098d4`. The
already memory-differentially proven task-registry initializer at `0x00010cbc`
is now composed into the runtime as a 648-instruction caller block. It reads all
29 ROM descriptors, rebuilds the registry and scratch records, preserves the
observed global-register and condition state, and returns through `0x000098d8`.
The following `0x00050130` graphics-buffer initializer stores base `0x005d0000`,
active flag 1 and offset 0 in 8 instructions, returning through `0x000098dc`.
The `0x0004e7b4` render-state initializer now clears its seven control words,
sets the `0x0a000000` limit and composes `0x0004f904` to arm bit 1 and clear all
216 sixteen-byte table records. It returns through `0x000098e0` after 672
instructions. The `0x00044084` game-default initializer is now composed with its
two bounded helpers: `0x00023bfc` copies 40 ROM words and trailing defaults,
while `0x0001fee4` fills 26 float slots with 1.0. The complete caller writes the
observed gameplay constants and task-state defaults in 442 instructions with
three calls and returns, reaching `0x000098e4`. The `0x00053750` object-table
initializer now follows its main-data pointer, copies 2,817 sixteen-byte records
to `0x00560000`, initializes the two `0x7f7f7f7f` sentinels and returns through
`0x000098e8` after 11,284 instructions. The `0x0000a0c4` effect-table initializer
now copies 4 KiB from ROM to `0x00531000`, clears 16 KiB at `0x00535000` and
returns through `0x000098ec` after 5,652 instructions. The `0x000012bc` input
ring initializer writes both indices and returns through `0x000098f0` after six
instructions. The observed mode-zero path through `0x00000fa0` now emits the
three inline I/O diagnostics, validates both ready polls, initializes the I/O
control byte, clears the five input-state fields and returns through
`0x000098f4` after 270 instructions with four calls and returns. The following
inline loop copies 192 KiB from `0x023d0000` to `0x005a0000` and reaches
`0x00009920` after 61,443 instructions. The `0x0000245c` display-offset
initializer is now composed from the existing game-color lookup and state
classifier. It clears both mirrored fractional offsets, rounds the two position
bytes down by their color-derived divisors, preserves the caller's `g0` and
returns through `0x00009924` after 126 instructions with six calls and returns.
The following `0x0001128c` frame accumulator initializes 256 samples, derives
the observed zero intensity/level and resets the selected sample, returning at
`0x00009928` after 1,178 instructions. The `0x000113f4` profile initializer then
copies the selected ROM defaults to `0x0050a700` and returns at `0x0000992c`
after 32 instructions. The next 30 inline instructions initialize gameplay
globals and constants through the call boundary at `0x000099fc`. The
`0x0001fcc0` input-profile selector is now recovered for the baseline path plus
controlled modes 6, 10, 11 and 12. The two fighter-mode orderings `2/1` and
`1/2` select mode 12 in 23 and 25 instructions; mode 10 is selected either from
its live byte or gameplay flag bits 21+20, while control byte 2 redirects the
live mode-10 path to mode 11. The nested `0x0001ff0c` helper still fills all 26
float defaults, then reproduces the mode-10 two-value override in 117 instructions
and the mode-6 eleven-value override in 136 instructions; ordinary paths take
114. The following ROM profile load now covers both its 17-instruction normal
path and the 19-instruction `profile == 4` timeout override that stores `0x10cc`,
reaching `0x0001fe60`. Seven controlled states are strict-equal at all three
block boundaries: 21/21 native/reference differential comparisons match complete
CPU, local-frame, procedure-counter and mutable-memory state. Its palette wrapper is now
composed through nested entry `0x00002c38` after another 12 instructions. The
`0x00002c38` palette body clears the `0x00546008` scratch ramp, emits 28 rows
of 32 RGB halfword entries, latches the active page, and returns after 30,467
recovered instructions, followed by its one-instruction return stub to
`0x0001fe64`. Its resumed prefix now loads the table pair, reproduces the
`0x4b410` registration helper, clears `0x0050a014`–`0x0050a026`, and reaches
`0x0002eab8`; the 90-instruction initializer and nested `0x31004` setup are
also recovered. The subsequent `0x2de4` palette-page worker is now covered
for its inactive condition-preserving return and active 28-page RGB upload.
Selector 17's `phase_state == 0` wrapper now recovers the complete
`0x00055008` control-menu entry topology: all 14 idle entries (0-13), every
neighboring forward/reverse transition and both 0/13 wraps are native. Index 0
retains the `CONTROL_TEST`/runtime-bit-9 variants and fighter/object setup;
indices 1-13 now compose their motion, command, robot/camera, material/polygon,
angle and texture diagnostics from live state and MAIN_DATA-backed decimal/hex
resources. Input bit 5 covers release, held redispatch and latched short-return
behavior on every screen, including index 13's distinct 43-instruction held
handler that clears the texture/runtime latches before returning. Ninety-eight
controlled states are strict complete-live-state ROM matches. The former missing
menu-index/entry wall is therefore closed; remaining selector-17 phase-zero work
is branch-level control combinations inside already recovered screens, alongside
other bit-7 indirect table entries and unrelated input modes.

Selector 3's phase table is now complete. The table at `0x0000aac4` holds
exactly eighteen entries, and the final two are recovered from a controlled
natural `0x0000a6c0` snapshot (task pointer `0x00515b00`, harness calibrated
against the published phase-8/phase-11 corridors): phase 16 decrements the
countdown at `[0x00500834]+0x50` and stays on a 34-instruction tick, its zero
result taking the three-instruction epilogue that advances the phase byte to 17
(37 instructions), and phase 17 clears the phase byte to zero, wrapping the
cycle, in 31 instructions. All three corridors return through the
`0xae30 -> 0xa6f4` ret chain to `0x0000a010` with 3 calls / 4 returns, and
controlled native-versus-reference runs match complete CPU condition state and
every mutable memory region exactly.
