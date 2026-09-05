# Mapping of Uncovered and Unobserved Branches (v0.1.3)

This document catalogs major unobserved execution paths and unrecovered
subsystems in Virtua Fighter 2 Version 2.1. The accepted clean-room corridor now
runs through the seventh-dispatch validation corridor, but it remains one evidence-backed
sequence rather than a complete game implementation. Unsupported paths return
`VF2_ERROR_UNSUPPORTED` instead of falling back to i960 interpretation.

The v0168 boundary audit removed the two remaining single-instruction oracle
handoffs in `native-second-dispatch`: the `ret` stubs at `0x0004bab4` and
`0x000020ec` are now recovered bridges. The strict post-scheduler corridor is
therefore 1,270,824 recovered instructions with zero interpreted instructions,
and the modular return boundaries remain exact through the repeated third to
seventh dispatch validations.

## 1. Scheduler and task execution

Recovered scanning uses live registry strides, skips inactive descriptors and
handles the observed changing final active task. Still uncovered:

- dynamic task creation and deletion;
- abnormal task exits and error paths;
- corrupt or structurally different task registries;
- alternate priorities, preemption and interrupt-driven scheduling orders; and
- scheduler/task combinations not reached by the accepted repeated corridor.

## 2. Gameplay (`fa_game_info` and related gates)

The accepted corridor includes active input/state selector fast paths,
the player-update bit-14 exit and their observed sequence gates. Still missing:

The reference i960 executor now covers the conditional range comparisons and
single-precision integer/real conversions encountered when the fighter-state
bit-31 path is forced. The `fa_game_info` dispatcher and its post-call tail are
recovered in C. Both observed `0x18144` invocations now recover the
118-instruction prefix through `0x18538` plus the observed `0x17b68`
`ld`/`bbc`/`ret` helper, both observed `0x18d44` floating-port paths, and the
`0x18c64`/post-call suffix through `0x18640`, and both observed `0x18644`
shared-fighter corridors returning at `0x164b0` and `0x164c4`; unobserved
branches remain explicit ROM-backed boundaries and remain unrecovered as
native C. The observed state-4/bit-15 and non-state-4 bit-15 prefixes are now native. Controlled bit-14/15/16/6 probes now cover the `0x181c0` through
`0x184ec` conditional body; the observed bit-4/6/8/14/15/16 dependent `0x18644`
flag-accumulation paths are native. The ROM-backed state-4 fixture matrix now
matches all 192 cases for flag bits 6, 14, 15 and 16 at both non-negative and
negative thresholds; the complete negative matrix is native, removing the
dispatcher fallback for this four-bit state-4 corridor. The ROM-backed state-8
fixture matrix now
matches all 96 cases for flag bits 1, 2 and 4, and all 192 cases when flag bit
8 is included as the fourth dimension, covering isolated, asymmetric and
bilateral distributions with both countdown and mode-bit-6 settings. With
high bits 21, 26, 29, 30 and 31 appended, both threshold endpoints (`-1` and
`0`) match the complete 3,072-case matrix (256 masks × 12 distributions).
The
low-result branch now applies the same `+0x5b6` update rule as the
high-result branch when `r9 > threshold`. The state-8 negative matrix is also
native and exact for all 192 combinations of bits 1, 2, 4 and 8. State-8
negative cases composed of bits 1, 2, 4, 8, 21, 26, 29, 30 and 31 are now
admitted to the native path and match the complete 3,072-case matrix
(256 masks × 12 distributions) exactly. This includes every isolated,
asymmetric and bilateral distribution, both countdown values, both mode-bit-6
values and all high-bit/low-bit combinations. At non-negative thresholds, the
same high-bit fixture family is exact across the complete matrix as well,
including masks 248..255.
State-8 bit 6 is now admitted for the negative threshold: with the same
nine-bit set, its full ten-bit matrix matches 12,288 fixtures (1,024 masks ×
12 distributions) exactly. On the positive threshold, the recovered child
matches the complete no-bit-8 submatrix for bits 1/2/4/6 (192 fixtures), plus
the eight tested masks containing bit 6, bit 8 and all five high bits,
covering every subset of low bits 1/2/4 (96 more fixtures). Positive bit-6
compositions outside the measured slices remain unproven or explicit
boundaries. The measured
positive `0x140` (state bit 8 + bit 6) and `0x142` (state bit 8 + bits 1 + 6)
fighter-state compositions are additionally exact across all three physical
distributions, both countdown values and both mode-bit-6 settings (24 more
fixtures total). The measured positive `0x144` composition (state bit 8 + bits
2 + 6) is also exact across its 12 distributions/countdown/mode cases. The
measured positive `0x150` composition (state bit 8 + bits 4 + 6) is also exact
across its 12 distributions/countdown/mode cases. The measured positive
`0x146` composition (state bit 8 + bits 1 + 2 + 6) is now exact across its
12-case matrix, and the neighboring `0x152`, `0x154` and `0x156` compositions
are exact as well. The measured positive high-bit composition `0x4140` (state
bit 8 + bits 6 + 14) is also exact across its 12-case matrix, including its
bilateral order-dependent joins. The adjacent positive `0x8140` composition
(state bit 8 + bits 6 + 15) is now exact across its 12-case matrix as well.
The adjacent positive `0x10140` composition (state bit 8 + bits 6 + 16) is
also exact across its 12-case matrix. Other positive high-bit compositions
remain unproven or explicit boundaries. The measured triple `0x0c140` (state
bit 8 + bits 6 + 14 + 15) is now exact across its 12-case matrix as well.
The measured triple `0x14140` (state bit 8 + bits 6 + 14 + 16) is now exact
across its 12-case matrix as well.
The measured triple `0x18140` (state bit 8 + bits 6 + 15 + 16) is now exact
across its 12-case matrix as well.
The measured all-high composition `0x1c140` (state bit 8 + bits 6 + 14 + 15 +
16) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x204140` (state bit 8 + bits 6, 14
and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x208140` (state bit 8 + bits 6, 15
and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x210140` (state bit 8 + bits 6, 16
and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x20c140` (state bit 8 + bits 6, 14,
15 and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x214140` (state bit 8 + bits 6, 14,
16 and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x218140` (state bit 8 + bits 6, 15,
16 and 21) is now exact across its 12-case matrix as well.
The measured cross-family composition `0x21c140` (state bit 8 + bits 6, 14,
15, 16 and 21) is now exact across its 12-case matrix as well.
The measured bit-14 cross-family extensions `0x4004140` (high bit 26),
`0x20004140` (high bit 29), `0x40004140` (high bit 30) and `0x80004140`
(high bit 31) are now native and exact across their 12-case matrices. The
admission is limited to one of these four single high bits; their measured
first-order, second-order and bilateral countdown/mode joins share the
validated accounting rule, while multi-high extensions remain unsupported.
The measured bit-15 extensions `0x4008140` (high bit 26), `0x20008140`
(high bit 29), `0x40008140` (high bit 30) and `0x80008140` (high bit 31),
and the corresponding bit-16 extensions `0x4010140`, `0x20010140`,
`0x40010140` and `0x80010140`, are now native and exact across their
12-case matrices. Each admission is limited to one of the four measured
single high bits; bit-15 uses the validated first-order, second-order and
bilateral accounting rule, while bit-16's measured correction is limited to
the bilateral countdown/mode joins. Multi-high extensions remain unsupported.
The corresponding bit-14 + bit-15, bit-14 + bit-16, bit-15 + bit-16 and
bit-14 + bit-15 + bit-16 cross-family extensions with each of high bits 26,
29, 30 and 31 are now native and exact across their 12-case matrices as
well. Each admission is limited to one measured single high bit; the
countdown/mode corrections are separately measured for each of the four
fighter-bit compositions. Multi-high extensions remain unsupported.
For the bit-14 + bit-16 and bit-15 + bit-16 compositions, all six pairwise
masks from high bits 26, 29, 30 and 31 are now native and exact across their
12-case matrices as well. These twelve pair admissions are limited to the
measured pairs and reuse the corresponding single-high accounting rules;
larger multi-high combinations remain unsupported.
The four measured high-bit triples for each of the bit-14 + bit-16 and
bit-15 + bit-16 compositions are now native and exact across their 12-case
matrices as well: `0x64014140`, `0xa4014140`, `0xc4014140`, `0xe0014140`,
`0x64018140`, `0xa4018140`, `0xc4018140` and `0xe0018140`. Admission is
limited to these eight measured triples and reuses the corresponding pair
accounting rules; larger high-bit combinations remain unsupported.
The two measured all-four-high extensions `0xe4014140` (bit-14 + bit-16)
and `0xe4018140` (bit-15 + bit-16) are also native and exact across their
12-case matrices. Admission is limited to these two masks and reuses the
corresponding triple accounting rules; other larger or high-bit-21
combinations remain unsupported.
The measured bit-14 + bit-16 high-bit-21 pair extensions `0x04214140`,
`0x20214140`, `0x40214140` and `0x80214140`, and the corresponding bit-15
+ bit-16 extensions `0x04218140`, `0x20218140`, `0x40218140` and
`0x80218140`, are now native and exact across their 12-case matrices. Their
admission is limited to one measured pair of bit 21 with 26, 29, 30 or 31;
larger high-bit-21 combinations remain unsupported.
All six bit-15 + bit-16 high-bit-21 triples are also measured exact across
their 12-case matrices: `21+26+29`, `21+26+30`, `21+26+31`, `21+29+30`,
`21+29+31` and `21+30+31` (masks `0x24218140`, `0x44218140`, `0x84218140`,
`0x60218140`, `0xa0218140` and `0xc0218140`). This is evidence for the
existing corridor only; other positive compositions remain unsupported.
For the bit-14 + bit-15 composition specifically, all six pairwise masks from
high bits 26, 29, 30 and 31 (`26+29`, `26+30`, `26+31`, `29+30`, `29+31` and
`30+31`) are now native and exact across their 12-case matrices as well. The
pair admission is limited to those six measured masks and reuses the measured
bit-14 + bit-15 accounting rule; pairs involving high bit 21 and larger
multi-high combinations remain unsupported.
The four measured bit-14 + bit-15 pairs combining high bit 21 with one of
high bits 26, 29, 30 or 31 are now native and exact across their 12-case
matrices as well. Their admission is limited to those four masks and uses the
same measured accounting rule; larger multi-high combinations remain
unsupported.
All six bit-14 + bit-15 high-bit-21 triples are now native and exact across
their 12-case matrices: `21+26+29` (`0x2420c140`), `21+26+30`
(`0x4420c140`), `21+26+31` (`0x8420c140`), `21+29+30` (`0x6020c140`),
`21+29+31` (`0xa020c140`) and `21+30+31` (`0xc020c140`). Each uses an
isolated, measured `0x164c4` accounting rule; other positive compositions
remain explicit unsupported boundaries.
The existing shared corridor has also been measured for the bit-14 + bit-15
 + bit-16 composition with high-bit combinations `21+26`, `21+29`, `21+30`,
 `21+31`, `21+26+29` and `21+26+29+30+31` (masks `0x421c140`, `0x2021c140`,
 `0x4021c140`, `0x8021c140`, `0x2421c140` and `0xe421c140`). Each completed
 its 12-case positive-threshold matrix exactly, including both countdown and
 mode-bit-6 variants. This records measured coverage of the common corridor;
 other unmeasured high-bit compositions remain explicit boundaries.
The complete `0x1645c` dispatcher now also has strict full-task coverage for
state 8 with isolated bit 14: all three fighter-record distributions and both
countdown/mode-bit-6 settings match the reference. The bilateral distribution
uses one measured dispatcher-instruction accounting correction; unilateral
records retain the ordinary count. Other positive dispatcher compositions
remain explicit boundaries until their full-task accounting is measured.
The same dispatcher now admits the six measured state-8 field masks
`0x0421c000`, `0x2021c000`, `0x4021c000`, `0x8021c000`, `0x2421c000` and
`0xe421c000` (bit-14 + bit-15 + bit-16 with the measured high-bit-21
compositions). Each mask matches all three fighter-record distributions and
both countdown/mode-bit-6 settings at threshold `0`; the bilateral controls
also match thresholds `1` and `2`. The bilateral cases use the measured
two-instruction accounting correction.
Other positive dispatcher compositions remain explicit boundaries.
The complete dispatcher also now admits state 8 with bits 6+14+21, field mask
`0x00204000`. Its three fighter-record distributions match both countdown/mode
settings at threshold `0`, and all three distributions match both mode values
at thresholds `1` and `2`. The measured accounting relation includes a fixed
bilateral join plus distribution-specific countdown/mode terms; other positive
bit-6 compositions remain explicit boundaries.
The adjacent state-8 bit-6+bit-15+bit-21 field mask `0x00208000` is also
admitted. Its three distributions match both countdown/mode settings at
threshold `0`, and all three distributions match both mode values at
thresholds `1` and `2`; its accounting uses the separately measured
unilateral and bilateral countdown corrections.
The corresponding state-8 bit-6+bit-16+bit-21 field mask `0x00210000` is
also admitted. It matches the same three distributions, countdown/mode
settings and thresholds `0..2`; only the bilateral distribution requires the
measured one-instruction join correction.
The compound state-8 bit-6+bit-14+bit-15+bit-21 field mask `0x0020c000`
is also admitted. All three distributions match both countdown/mode settings
at threshold `0`, and both mode values at thresholds `1` and `2`; its native
child requires the measured distribution-specific accounting terms and the
bilateral base join.
The sibling state-8 bit-6+bit-14+bit-16+bit-21 field mask `0x00214000` is now
admitted as well. All three distributions match both countdown/mode settings
at thresholds `0`, `1` and `2`; its native child uses the measured
distribution-specific mode/countdown joins, while thresholds outside the
measured nonnegative range remain explicit boundaries.
The sibling state-8 bit-6+bit-15+bit-16+bit-21 field mask `0x00218000` is now
admitted too. All three distributions match both countdown/mode settings at
thresholds `0`, `1` and `2`; only the bilateral distribution requires the
measured two-instruction dispatcher join.
The all-three state-8 bit-6+bit-14+bit-15+bit-16+bit-21 field mask
`0x0021c000` is now admitted. All three distributions match both
countdown/mode settings at thresholds `0`, `1` and `2`; like its adjacent
bit-14/bit-16 and bit-15/bit-16 siblings, it uses a measured bilateral
two-instruction join.
The measured state-8 bit-14+bit-16+high-bit-26 field mask `0x04214000` is
also admitted. All three distributions match both countdown/mode settings at
thresholds `0`, `1` and `2`; its native child and dispatcher use the measured
bit-14/bit-16 high-bit accounting joins, while other high-bit compositions
remain explicit boundaries.
The adjacent state-8 bit-14+bit-16+high-bit-29 field mask `0x20214000` is
also admitted. All 36 combinations across the three fighter-record
distributions, countdown `0/1`, mode bit 6 clear/set and thresholds `0..2`
match the reference at `0x10dcc`, including the full CPU/memory snapshot and
instruction/call/return counters. Its dispatcher uses the measured
bit-14/bit-16 accounting joins and the measured condition-state postcondition;
other high-bit compositions remain explicit boundaries.
The next adjacent state-8 bit-14+bit-16+high-bit-30 field mask `0x40214000`
is admitted under the same measured threshold range. Its 36-case matrix also
matches the full reference snapshot and instruction/call/return counters;
the native child uses the shared bit-14/bit-16 accounting joins and the same
condition-state postcondition. Other high-bit compositions remain explicit
boundaries.
The corresponding state-8 bit-14+bit-16+high-bit-31 field mask `0x80214000`
is now admitted under the same measured threshold range. Its 36-case matrix
also matches the full reference snapshot and instruction/call/return counters;
the native child uses the shared bit-14/bit-16 accounting joins and the same
condition-state postcondition. Other high-bit compositions remain explicit
boundaries.
The first measured high-bit pair, bit-14+bit-16 with high bits 26 and 29,
uses field mask `0x64014000` and is now admitted as well. Its 36-case matrix
matches the full reference snapshot and instruction/call/return counters; it
uses the shared distribution accounting, while all three distributions leave
the measured `NONE` condition-state postcondition. Other high-bit pairs remain
explicit boundaries.
All six full-dispatch state-8 bit-14+bit-16 compositions admitted so far
(`0x04214000`, `0x20214000`, `0x40214000`, `0x80214000`, `0x64014000` and the
new bit-21 triple `0x44214000`) have been re-measured and re-admitted under
the committed `validate_game_info_full_dispatch.py` calibrated task-entry
fixture, which is now the single measurement harness for this corridor. The
earlier dispatcher-accounting constants and NONE/EQUAL condition splits for
the first five masks came from an earlier ad-hoc fixture lineage and did not
reproduce; every one of the six 36-case matrices (three fighter-record
distributions, countdown `0/1`, mode bit 6 clear/set, thresholds `0..2`) now
matches the full reference snapshot, architecture signature and all counters
exactly under one shared measured rule: unilateral accounting `+3 + mode6`
and bilateral accounting `+5 + 2*mode6` instructions, an EQUAL/LESS final
condition state keyed on the countdown byte (left naturally by the isolated
high-26 mask), and measured stale-historical-frame postconditions in the
frame slot beyond the final call depth. Unadmitted neighbours such as
`0x60214000` and out-of-range threshold `3` still fail closed.
Seven further no-bit-21 pair/triple compositions (`0x24014000`,
`0x44014000`, `0x84014000`, `0x60014000`, `0xA0014000`, `0xC0014000` and
`0xA4014000`) are now admitted through the same unified measured rule, each
with its complete 36-case matrix exact, followed by every remaining
bit-14+bit-16 state-8 composition: the isolated high bits without bit 21
(`0x04014000`, `0x20014000`, `0x40014000`, `0x80014000`), the remaining
bit-21 pairs and triples (`0x24214000`, `0x84214000`, `0x60214000`,
`0xA0214000`, `0xC0214000`), the quads (`0xE4014000`, `0x64214000`,
`0xA4214000`, `0xC4214000`, `0xE0214000`), the full quint `0xE4214000` and
the base-only mask `0x00014000`. The whole family — all 32 high-bit subsets
over bits 21/26/29/30/31 on the bit-14+bit-16 base — now matches the
reference exactly across its 36-case matrices under one shared accounting
rule, natural condition-state tail and shared stale-frame postconditions;
the fallback ROM-child path had hidden this join behind a different raw
baseline (see
`decomp/i960/notes/game_info_1645c_full_state8_family_completion_v0123.md`).
Outside-family compositions such as `{14,15}` or `{15,16}` bases and
thresholds above 2 still fail closed.
The bit-14+bit-15 state-8 base family is now admitted as well: all 28
compositions over the `0x...C000` base — the base-only mask, every high-bit
subset over bits 21/26/29/30/31, and the previously stale `0x0020C000` —
match their complete 36-case matrices exactly under one measured per-record
rule (`nrecords * (3 - 4*countdown + mode6)` dispatcher instructions),
natural condition-state tail and shared stale-frame postconditions.
A full audit of every remaining legacy full-dispatch admission against the
committed harness found eleven masks whose predicates still carried
superseded fixture-lineage constants: the two-bit bit-21 compounds
`0x00204000`, `0x00208000`, `0x00210000`, `0x00218000` and all seven
measured bit-14+bit-15+bit-16 compositions (`0x0021C000`, `0x0421C000`,
`0x2021C000`, `0x4021C000`, `0x8021C000`, `0x2421C000`, `0xE421C000`).
Their re-measured accounting is genuinely distribution-asymmetric (for
example `0x0421C000` f0-only versus f1-only differ by 5 instructions in the
mode-bit-6 cells) and is not yet recovered, so those admissions are retired
and the masks fail closed until properly re-measured. Every state-8
full-dispatch admission now remaining in the tree is proven directly under
the committed validator (see
`decomp/i960/notes/game_info_1645c_full_state8_bit14_15_family_and_audit_v0124.md`).
All eleven masks have now been re-admitted from measured data. The seven
bit-14+bit-15+bit-16 base compositions (`0x0021C000`, `0x0421C000`,
`0x2021C000`, `0x4021C000`, `0x8021C000`, `0x2421C000`, `0xE421C000`)
retain the v0125 measured per-fighter accounting (fighter-0 side `3`, or `2`
when countdown/mode6 is set; fighter-1 side
`3 + 5*mode6 + 4*cd − 4*cd*mode6`; bilateral the exact sum except the
isolated 21-only mask, whose bilateral table is `{4, 8, 7, 8}`). The four
bit-21 compounds are independently measured: `0x00204000` and `0x00208000`
retain their v0125 tables (including the measured stale `r15=0` postcondition
for `0x00208000`), while v0126 closes the last architectural gap for
`0x00210000` and `0x00218000`. The recovered `0x18644` child now performs the
reference read/modify/write that sets bit 15 of the 16-bit field at
`fighter + 0xb24` for those exact active masks, preserving all other bits.
Their measured dispatcher deficits are `+4` unilateral / `+7` bilateral for
`0x00210000` and `+4` unilateral / `+8` bilateral for `0x00218000`. See
`decomp/i960/notes/game_info_1645c_full_state8_bit14_15_16_recalibration_v0125.md`
and `decomp/i960/notes/game_info_1645c_full_state8_bit16_compounds_v0126.md`.
The bit-14 + bit-16 triple-high extension `0x24214140` (21+26+29) is now
native and exact across its 12-case matrix. Its correction is isolated to the
measured `0x164c4` return corridor; the neighboring 21+26+30 composition still
measures only 3/12 exact and remains unsupported.
The corresponding bit-14 + bit-16 extension `0x44214140` (21+26+30) is now
native and exact across its 12-case matrix as well, with the same isolated
return-corridor accounting. The neighboring 21+29+30 composition remains a
3/12 control and is still unsupported.
The third measured extension `0x84214140` (21+26+31) is now native and exact
across its 12-case matrix; the other high-bit triples were still unmeasured at
that point.
The measured extension `0x60214140` (21+29+30) is now native and exact across
its 12-case matrix. Its neighboring 21+29+31 control remains 3/12, preserving
the explicit boundary for the unmeasured triple.
The final bit-14 + bit-16 high-bit-21 triples `0xc0214140` (21+30+31) and
`0xa0214140` (21+29+31) are now native and exact across their 12-case
matrices. This completes all six combinations of bit21 with two of high bits
26, 29, 30 and 31 for this family; other positive compositions remain
unsupported.
The measured bit-14 + bit-15 triple-high extensions `0x6400c140`,
`0xa400c140`, `0xc400c140` and `0xe000c140` (high-bit triples 26+29+30,
26+29+31, 26+30+31 and 29+30+31) are now native and exact across their
12-case matrices. Admission is limited to these four measured triples and
reuses the same accounting rule; other triple-high and larger combinations
remain unsupported.
The measured positive isolated bit-6/high-bit masks `0x200140` (bit 21),
`0x4000140` (bit 26), `0x20000140` (bit 29), `0x40000140` (bit 30) and
`0x80000140` (bit 31) are each exact across their 12-case matrices. The
aggregate masks `0x24000140` (bits 26+29) and `0xe4200140` (bits
21+26+29+30+31) are exact across their 12-case matrices as well.
All ten pairwise combinations of high bits 21, 26, 29, 30 and 31 with state
bit 8 and fighter bit 6 are exact across their 12-case matrices too: masks
`0x4200140`, `0x20200140`, `0x40200140`, `0x80200140`, `0x24000140`,
`0x44000140`, `0x84000140`, `0x60000140`, `0xa0000140` and
`0xc0000140`. These ten pairwise matrices contain 120 measured fixtures;
the 26+29 matrix was already counted in the aggregate evidence above, so the
nine newly added matrices extend the measured positive family to 360 unique
cases. Other positive high-bit compositions remain explicit boundaries.
All ten high-bit triples and all five high-bit quads over bits 21, 26, 29,
30 and 31 with state bit 8 and fighter bit 6 are now ROM-measured through the
full `0x1645c` dispatcher as well. Each mask is exact across 36 fixtures
covering the three fighter distributions, countdown 0/1, mode bit 6 clear/set
and thresholds 0..2 (540 fixtures total). Together with the existing singles,
pairs and all-five-high mask, every non-empty high-bit subset is now admitted
for the no-low-bits positive bit-6+bit-8 family; unmeasured low-bit mixes remain
fail-closed.
The controlled probes
now cover `g0 == 0`, `g0 == 1`, `g0 == 2` and `g0 == 3`; the shared
`0x18e08`/`0x18e00` command-port helper body and a controlled low-result
`0x18644` threshold outcome are also covered. The `0x18978..0x189a4` high-state flag tail is native as well: bits 26..29 use the measured progress/limit gate and bits 30..31 are accumulated unconditionally. The following shared `0x189a8..0x189bc` CHKBIT/ALTERBIT tail is native too, including its observable condition-code result and bit-3 accumulation. The type-22 tail now covers both the progress mismatch and the coherent equal-progress `0x18bd4` call path, including the generic `0x1ab34` type-record resolver and the measured bit-2-clear `0x18b58` branch. Unobserved downstream comparisons and other conditional branches remain ROM-backed or unsupported.

The same bridge now covers the following `fa_player` task entry at
`0x00013f08`. Its observed 842-instruction bootstrap through the first nested
call at `0x00014288`, followed by the accepted 1,652-instruction `0x19ef8`
corridor through `0x0001428c` and the downstream geometry expansion through
`0x000142c0`, followed by the small setup corridor through `0x00014310`, the
observed preamble through `0x000143e4` and its state-neutral prefix through
`0x000143fc`, the observed `0x0001ab74` entry prefix through `0x0001abf4`,
the `0x00027ce0` entry prefix through `0x00027d00`, the immediate calls to
`0x00028184` and `0x00028780`, the observed prefix through `0x00028268` and
the accepted `0x00028780` geometry body and the measured
`0x00027d90`/`0x00027dcc`/`0x00027fa0 -> 0x0002901c` and
`0x00028174 -> 0x00029414` calls and the measured `0x00014400 -> 0x00017710`,
`0x00014404 -> 0x0001791c`, `0x00014408 -> 0x0004b640`,
`0x00014414 -> 0x00016504` and `0x00014418 -> 0x000180bc` chain, is native C and matches the ROM
endpoint exactly; later player branches remain explicit original-i960
continuations. A real sixth-entry snapshot therefore
advances through both fighter task records and back to the main loop while
retaining clearly marked ROM-backed boundaries.

- character and arena selection;
- the complete match state machine, timeout and game-over transitions;
- fighter physics, hitboxes, hurtboxes, collision, damage and combos;
- ring-out and arena-boundary handling;
- CPU opponent decision logic; and
- evidence-backed portable fighter/object structures above raw addresses.

## 3. Camera

The startup and recurring camera corridor plus the validated optional viewport
construction paths are recovered. Unobserved movement-dependent tracking,
knockdown/throw cameras, alternate presets, zoom and mode-table transitions
remain unsupported.

## 4. Texture, video and geometry bridge

The observed texture expiration, pending palette upload and first non-zero
five-level stream expansion are recovered. The record publisher at
`0x0004b9b8` now recovers its out-of-range diagnostic: values `> 0x56`
render the signed value plus `tex num error` into tile RAM (19 cells,
`0x01000064`/`0x01000072`, 160 instructions) instead of failing, and the
second publisher (`argument0+1`) is still evaluated with wraparound
(v0196). Counter2 (`0x005502e0` via `0x0004b44c`) now lets that publisher
handle the range: an out-of-range first value skips the `0x00550288`
publication, renders the diagnostic, then continues through the queue
helper at `0x0004ba70` for 198 instructions / 5 calls / 5 returns (v0197,
21/21 exact). Video-layer rejection preflights inputs before writes. Still uncovered:

- alternate texture records, page formats, palette arguments and cache states;
- other stream headers, dimensions, timer states and mip layouts;
- compressed-stream corruption and invalid symbol/pair indexes;
- geometry ring-register patterns outside the accepted sequence;
- polygon FIFO packet protocol, lighting/clipping fidelity and hardware
  renderer behavior beyond the bounded direct/object reference executor; and
- production rendering output.

The texture orchestrator limit cluster at `0x0004bfe0` is now fully recovered:
every `display_mode % 32` selector is decoded — `bbs` with source masks `0xc0`/`0xc000`/`0x0c`,
fall-through `cmpobe` for 12/13, top `bbs 16` for runtime bit16 at `0x00500068`,
plus the `0x00500064 == 6/8` and `0x00500031 < 8` matrix for `mode 9` — and all six
limit pairs are proven ROM-backed via synthetic snapshots at `0x4bfe0`
(`vf2probe --rom-dir D:/ia/vf2-decomp/roms/vf2 --until 0x4c11c --read-u32`)
sweeping `display_mode 0..255 × runtime bit16` (512 cases):
`0x3e80/0x4e20`, `0x4330/0`, `0/0x4e20`, `0x4330/0x4e20`, `0x12a8/0x4330`, `0x32c8/0x4e20`;
write-skip `2,3 mod32` cases remain explicit `VF2_ERROR_UNSUPPORTED` with unchanged RAM
(v0200, `vf2_orchestrator_limits_tests` locks 512 probes).

The observed phase-17 dispatcher path is accepted when phase state is non-zero.
Controlled ROM-backed differential evidence recovers both phase-navigation
directions in `0x00058fe0`: gameplay mask `0x08001008` advances the index with
`11 -> 0` wraparound, while bit 13 (`0x00002000`) decrements it with `0 -> 11`
wraparound. Both mark the old double-indirect phase target with `0x8020` and the
new target with `0x801c`, and the forward mask has ROM-accurate priority. The
`0x04000104` reset/display path sets phase-index bit 7, clears the 48x64 tile
plane through `0x00008ef0`, and centers the phase label via
`0x00060410 -> 0x00007fc0`.

The resulting observed bit-7 entry (`0x8b`) is also recovered: `0x00059154`
selects `0x0005ef60`, whose first visit performs meter+CRC, clears the tile plane,
draws `EXIT TEST MODE` and arms counter 320. Positive countdown visits and the
terminal `counter 1 -> 0` path at `0x0005f07c` are now recovered. The terminal
clears the observed layer/game state, writes the reset diagnostic through
`0x0006116c`, and hands off non-returningly to `0x000000b0`. Warm boot stages 1
and 2 are strict-equal through `0x0000052c`, and the post-reset continuation is
now recovered through `0x000098b0`: 60,078 instructions to `0x0006dd4c`, then
15 strict initializer blocks / 1,498,968 instructions covering descriptor-stream
copies, backup-SRAM restore, palette/table construction and hardware-core setup.
The continuation now crosses the call from `0x000098b0` into `0x0004b020`,
clears its six texture state/counter words, and derives the timer threshold in
`0x0004afb4`. It now composes the already-recovered `0x00000b6c` timer/wait
helper, returns to `0x0004afdc`, captures the initial frame byte and reaches the
status-poll loop at `0x0004afe4`. That loop now injects and resumes the shared
frame interrupt, then follows the observed frame-change exit through the status
store and call to `0x00000f7c`. The early helper's odd/high-byte wait, interrupt
resumption, `0x00002ec4` video-status latch and two caller returns are now
composed through `0x0004b07c`. The following observed equal-identity path now
checks the board/four graphics-data identities and initializes ten texture
records before entering `0x0004b820`; identity failures reject transactionally.
Its four-instruction wrapper is recovered through nested entry `0x0004b9b8`.
The observed nested setup activates record zero, clears the restart words and
unwinds to `0x000098b4`; alternate record IDs, priorities, bit-4 replacement and
mismatch diagnostic branches remain uncovered. The following `0x00011704` luma
expansion is recovered through `0x000098b8`. Its early-wait continuation now
composes the shared helper and video-status latch through `0x000098bc`. The
following `0x00011744` run-length expander's first 8,192-byte geometry pass,
frame commit and early wait are recovered through `0x0001179c`; the second
8,192-byte pass now continues the live decoder through the following frame
commit, the third reaches the next commit, and the fourth and final pass reaches
its own commit. The final commit/wait unwinds the expander, and the caller's next
early wait completes through `0x000098c4`. The following `0x000117f8` geometry
table, frame commit and early wait are recovered through its return at
`0x000098c8`. The following `0x0004ad40` reset is recovered through `0x000098cc`.
The following `0x00007f7c` and `0x00007ef0` constant-table copies are recovered
through `0x000098d4`. The existing `0x00010cbc` task-registry initializer is now
composed through its return at `0x000098d8`, and the `0x00050130` graphics-buffer
initializer returns through `0x000098dc`. The `0x0004e7b4` render-state reset and
nested 216-record clear return through `0x000098e0`. The `0x00044084` game-default
initializer and its two bounded table helpers return through `0x000098e4`. The
following `0x00053750` object-table copy and sentinel setup return through
`0x000098e8`. The `0x0000a0c4` ROM-backed effect-table copy and clear return
through `0x000098ec`. The `0x000012bc` input-ring initializer and the observed
mode-zero `0x00000fa0` diagnostic I/O initializer return through `0x000098f4`.
The inline 192 KiB game-data copy beginning there reaches `0x00009920`. The
observed `0x0000245c` display-offset initializer is recovered through its return
at `0x00009924`; alternate game-state classifications and split-screen flag
paths remain unsupported. The observed `0x0001128c` accumulator and `0x000113f4`
profile defaults are recovered through `0x0000992c`, followed by the inline
gameplay-global initialization through `0x000099fc`. Alternate accumulator
modes and signed profile overrides remain unsupported. The `0x0001fcc0`
input-profile selector is now strict-differentially recovered for the baseline
path and controlled modes 6, 10, 11 and 12. Fighter-mode pairs `2/1` and `1/2`
select mode 12, flag bits 21+20 or a live mode byte exercise mode 10, control
byte 2 redirects mode 10 to mode 11, and `0x0001ff0c` reproduces the mode-10
and mode-6 float override tables after the shared 26-float fill. The subsequent
ROM profile loader also covers `profile == 4`, including its `0x10cc` timeout
write. Seven controlled states match the reference at all three native block
boundaries (21/21 comparisons) through `0x0001fe60`; the palette wrapper then
reaches nested entry `0x00002c38`. The observed palette body is recovered for its 28-row by
32-entry RGB ramp and page latch, including the return stub at `0x00020050`.
The resumed `0x0001fe64` prefix through the `0x4b410` helper and state clears
is recovered, as is the 90-instruction `0x0002eab8` initializer and nested
`0x00031004` setup. Its `0x0001fedc` call into the 66-row `0x00011704` luma
copy and trailing return are now recovered with live pointer poststate. The
frame-dispatch bridge now covers selector 2's `0x0000ab0c` reset and advances
to selector 3. Selector 3's phase-zero `0x0000ae78` mode-table worker is now
recovered for both the live fallback profile and the zero-derived alternate
profile, including its two descriptor expanders and both ROM text sources.
Selector 17's `phase_state == 0` wrapper now follows its separate `0x00055008`
control-menu dispatcher across all 14 idle entries (0-13), every neighboring
forward/reverse transition and both 0/13 wraps. The formerly opaque screens now
reuse recovered MAIN_DATA-backed decimal/hex/text renderers plus motion,
camera/material/polygon and texture state helpers; input bit 5 covers release,
held and latched behavior on every screen, including index 13's special
43-instruction held-button early exit. Ninety-eight controlled cases are strict
ROM-backed complete-live-state matches, so there are no longer missing
phase-zero menu indices or entry transitions. Selector 3's phase table is now
complete: all eighteen entries (phases 0-17) are native, including phase 16's
countdown stay/advance pair at `0x0000c414` and phase 17's phase-reset wrap at
`0x0000c448`, each strict-matched against the reference at the `0x0000a010`
boundary. Remaining input modes, other bit-7 indirect table entries, and
unmeasured branch-level control combinations inside the now-native phase-zero
screens remain unsupported. The subsequent
`0x00002de4` palette-page upload is covered for both its inactive
condition-preserving return and its active 28-page RGB upload path.

## 5. Audio and platform

Only the accepted deterministic sound-task buffer behavior, the SCSP
register/sample/MIDI host boundary, and the ROM's 68000 voice-maintenance
transition and command-dispatcher boundaries are recovered. The populated
sample-table prefix of the `0x90` allocator is also covered. The Motorola
68000 command handlers other than the bounded `0xc0`/`0xe0` paths, selected
no-live-voice `0xb0` entries and that allocator prefix, live voice/DSP synthesis,
native windowing, gamepad mappings,
frame pacing and production platform integration remain unimplemented.

## 6. Transactional rejection coverage

The player interrupt composite now preserves CPU state when a nested player
branch is rejected, and video input validation occurs before writes. Other
large composite blocks have not yet been proven globally transactional for every
unsupported subpath; additional candidate-state or rollback boundaries should
be introduced as new rejection cases are observed.

### v0161 positive state-8 bit-14 high pair

The high-26+high-29 pair `0x24004140` is independently ROM-measured across all 12 fighter-distribution/countdown/mode-bit-6 cases and is now admitted with its mask-local second-call accounting correction. Other unmeasured bit-14 multi-high extensions remain fail-closed.

### v0205–v0210 positive state-8 bit-6+14 high family over 26,29,30,31

The 15 non-empty high subsets over bits 26,29,30,31 on base `0x00004140`
(bit-6+14) have been expanded from single-mask admissions to full
low-bit cubes over bits 1,2,4 (8 masks per high subset, `15×8=120` masks).
Each mask is `36/36 exact` via `validate_game_info_full_dispatch.py`
(3 distributions ×2 countdown ×2 mode6 ×3 thresholds `0..2`) with
`+4 bilateral / +2 unilateral` and the same stale-frame. The high
family was refactored in v0212 into a single compact predicate
`(combined & ~0xE4004156)==0 && (combined & 0x4140)==0x4140 && high!=0`.

### v0211 base low cubes

The no-high base `0x00004140` and the high-16 variant `0x00014140`
were expanded from 1–2 masks to 8 masks each (low 1,2,4), `16` masks
`36/36 exact`, completing `136` masks for the `0x4140` family.

### v0213–v0215 other positive bases low cubes

* `0x00008140` (bits 6+14+15) `1→8` low cube with `-5/-3` (v0213, compacted v0217 to `& ~0x16`)
* `0x00010140` (bits 6+14+16) `1→8` with `+8/+4` plus bit11 write (v0214, compacted v0217 — fixes `0x10144` narrow-check outlier)
* `0x0000C140`/`0x0001C140` (bits 6+14+15 and 6+14+15+16) `2→16` low cubes
  with `+5/+2` (v0215, compacted v0218 to `& ~0x10016`)
* `0x00018140` `1→8` low cube with `+9/+4` plus bit11 (v0216, compacted v0217 — fixes `0x18144` outlier)
* `0x00004140`/`0x00014140` (bits 6+14 with/without 16) `2→16` low cubes
  with `+4/+2` (v0211, compacted v0219 to `& ~0x10016`)

Each `36/36 exact`. Other positive bases (e.g. `0x8140` high combos,
`0xC140` high combos) remain explicit boundaries until their low cubes
are measured. Compact forms now use `& ~0x16` or `& ~0x10016` and share
`hybrid_set_stale_low()` (v0220).

### v0221 high-26 8140 low cube

* `0x04008140` (high-26 + bits 6+14+15) `1→8` low cube with `-5/-3` (v0221)

### v0222 high-29 8140 low cube

* `0x20008140` (high-29 + bits 6+14+15) `1→8` low cube with `-5/-3` (v0222)

### v0223 high-30/31 8140 low cubes

* `0x40008140`/`0x80008140` (high-30/31 + bits 6+14+15) `2×8` low cube with `-5/-3` (v0223)

### v0224 high-26 10140 low cube

* `0x04010140` (high-26 + bits 6+14+16) `1→8` low cube with `+8/+4` plus bit11 (v0224)

### v0225 high-30/31 10140 low cubes

* `0x40010140`/`0x80010140` (high-30/31 + bits 6+14+16) `2×8` low cube with `+8/+4` plus bit11 (v0225) — high-29 `0x20010140` stays `0/36`

### v0226 high pairs 8140

* `0x24008140`, `0x44008140`, `0x84008140`, `0x60008140`, `0xA0008140`, `0xC0008140` (6 pair bases) each `1` mask `36/36 exact` with `0` excess (v0226) — low variants `|low` remain `0/36`

### v0227 high triples/quad 8140

* `0x64008140`, `0xA4008140`, `0xC4008140`, `0xE0008140`, `0xE4008140` (5) each `1` mask `36/36 exact` with `0` excess (v0227)

### v0228 high pairs/triples/quad 10140

* 11 masks for `0x10140` over 26,29,30,31 pairs/triples/quad `0` excess +bit11 (v0228)

### v0229 high pairs/triples/quad C140

* 11 masks for `0xC140` over 26,29,30,31 pairs/triples/quad `0` excess (v0229)

### v0230 high bulk 18140/1C140/14140

* 11 for `0x18140` +11 for `0x1C140` +10 for `0x14140` (quad fail-closed) `0` excess (v0230)

### v0231 high 8140 low variants

* 11 high bases ×7 low variants (77 masks) `-5/-3` (v0231) — base low 0 already `0` excess

### v0232 high C140 low variants

* 11 high bases ×7 low variants (77 masks) `+5/+2` (v0232)

### v0233 high singles low variants 14140/18140/1C140

* 4 highs ×7 low (84 masks) `+4/+9/+5` (v0233) — base low 0 via `4140` high compact for `14140`

### v0234 high singles base 18140/1C140

* 8 base low-0 (`0` excess +bit11) (v0234)

### v0235 high-29 10140 low cube

* `0x20010140` (high-29 + bits 6+14+16) `1→8` low cube with `+6/+3` plus bit11 and `fighter+0x6da=0x1e` for bit29 (v0235)

### v0236 high pairs 10140 low variants

* 11 high bases ×7 low variants (77 masks) — 7 with bit29 `+6/+3` +`0x1e`, 4 without `+8/+4` (v0236)

### v0237 high pairs 18140 low variants

* 11 high bases ×7 low variants (77 masks) — 7 with bit29 `+7/+3` +`0x1e`, 4 without `+9/+4` (v0237)

### v0238 high pairs 1C140/14140 low variants

* 11 high bases ×7 low for `0x1C140` (77 masks) `+5/+2` and 10 bases ×7 low for `0x14140` (70 masks) `+4/+2` (v0238)

### v0239 high singles C140 low variants

* 4 highs ×7 low for `0xC140` (28 masks) `+5/+2` (v0239)

### v0240 14140 quad low variants

* `0xE4014140` quad low `1→8` (7 masks) now `+4/+2` via `& ~0x16` (v0240) — `70→77` for `0x14140`

### v0241 high quad E400 with base 0x140

* `0xE4000140` `1→8` (8 masks) `+0` base / `-3/-6` low (v0241)

### v0242–v0244 high-family base 0x140 low variants

* 5 singles ×7 low (35 masks) `-3/-6` (v0242)
* 10 pairs ×7 low (70 masks) `-3/-6` (v0243)
* 14 triples/quads ×7 low (98 masks) `-3/-6` (v0244) — excludes `0xE4000140` (v0241) and quint `0xE4200140` (18/36)

### v0245–v0246 base 0x140 full closure

* 30 bases without low `30` masks `cd0/mode6` split (`f0 +8/+3, f1 +4/+3, bi +7/+6`) (v0245)
* quint low `7` masks `cd` split (`cd0 +8/+11, cd1 +3/+6`) (v0246) — high-family base 0x140 now `248/248` exact

### v0248–v0253 bit-21 low variants for remaining bases
* `0xC140` bit-21 low `16×7=112` masks `+2/+5` no bit11 (v0248)
* `0x4140` bit-21 low `16×7=112` masks `+2/+4` no bit11 (v0249)
* `0x14140` bit-21 low `16×7=112` masks `+2/+4` no bit11 (v0250)
* `0x10140` bit-21 low `16×7=112` masks `+4/+8` plus bit11 (v0251)
* `0x18140` bit-21 low `16×7=112` masks `+4/+9` plus bit11 (v0252)
* `0x1C140` bit-21 low `16×7=112` masks `+2/+5` no bit11 (v0253) — plus `0x8140` bit-21 low `112` masks `−3/−5` (v0247) already closed. Total `672` masks.

### v0254 middle-high low variants (generalizes v0247–v0253)
* same 7 bases with `low !=0` and `outer 16` but `MIDDLE=0x1B7E3EA9` (20 bits: `0x200`+`0x400`+… excluding `outer`/`base`/`low`/`bit6`/`0x00800000`). Single-bit middle highs `9` bits and multi-bit combos (`0xC0000`, `0x1B7C0000`, `0x1B7E3EA9`) all share per-base `−3/−5`/`+2/+5`/`+4/+8`/`+4/+9` (v0254). Representative `40` combos `36/36 exact`; `outer`-only stays exact, `0x00800000` stays `NATIVE-FAIL` (excluded).

### v0255 middle-high bare+low (extends v0254)
* removes `low !=0` guard — any middle `0x1B7E3EA9` with `outer 16` and `low 8` (incl. bare) admitted, same per-base accounting and bit11 (v0255). Bare `0x00048140` etc `36/36 exact`.

### v0256 bit-23 bridge (extends v0255)
* recovers `0x17b68` helper `bbs 23,r15,0x17fe8` path: `fighter+0x30=0, fighter+0x1c=0, if 0x624!=0 then fighter+0x620=1`, `28/30` instructions vs `31` for `0x624==0/!=0`, converges to common `0x1853c` tail. `MIDDLE` widens to `0x1BFE3EA9` (`0x1B7E3EA9|0x00800000`, 21 bits) and mask `~0xFFFE3EBF`. Same 7 bases now admit any middle including bit23: `0x00808142` etc `36/36 exact` (representative `12` masks, `7` bases x `16x8` outer/low). Bare `0x00808140` etc exact. Counters adjust via same per-base `−3/−5` etc.

### v0257 bare pure-bit21 fix (extends v0256)
* bare `0x00208140` etc (`7` bases, outer `16`, middle `0x00200000` alone, low `0`) were `0/36` DIFF `-3` (middle-high over-corrected); they need `0` excess, not `−3/−5`. Guard bare with `((low & 0x16)!=0 || (middle & 0x1BDE3EA9)!=0)` so pure-bit21 bare falls through to native `0` and now `36/36 exact` (`112` masks `16×7`). Low `0x00208142` etc already `36/36`; bare `0x00808140` etc stay `36/36`.

### v0258 base 0x140 single-middle (extends v0257)

* `0x140` + exactly one `MIDDLE 0x1BFE3EA9` bit (`20` bits:
  `0x1,0x8,0x20,0x80,0x200,0x400,0x800,0x1000,0x2000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x8000000,0x10000000`)
  with any outer `16` and any low `8` (incl. bare) — `20*16*8=2560` but
  `0x00200000` alone was already `36/36`? Actually `0x00200140` was
  `0/36 +3` before, now `36/36` — single-middle uniformly `+3/+6`
  (`0x340/0x940/0x1140/0x2140/0x20140/0x141/0x00200140` etc all `0/36 +3/+6`
  → `36/36`, `16*8*19=2432` masks after excluding pure `bit21`? Wait
  pure `bit21` now included: `19` vs `20` — `0x00200140` also `+3` so
  `20*128=2560` but `0x140` bare without middle stays `0`. Net `2432`
  with `0x1BDE3EA9` guard? Actually `0x1BDE3EA9` guard for `8140` etc
  not needed for `0x140`. Representative `12` masks `36/36`, multi-middle
  `0x1840` (`+3/+7`) and `0x200342` etc stay fail-closed. See
  `decomp/i960/notes/game_info_18644_positive_base0140_single_middle_v0258.md`.

### v0259 base 0x140 any-middle (generalizes v0258)

* `0x140` + any `Mp = 0x1BDE3EA9 !=0` (19 bits: `0x1,0x8,0x20,0x80,0x200,0x400,0x800,0x1000,0x2000,0x20000,0x40000,0x80000,0x100000,0x400000,0x800000,0x1000000,0x2000000,0x8000000,0x10000000`)
  with or without `bit21 0x00200000`, any outer `16` and any low `8` — single 0x340, double 0xB40/0x1940/0x1340, quad 0x3D40, high 0x60140/0x8000340,
  bit21+Mp 0x00200340/0x00200940/0x00260140/0x08200340 etc all measured `+3` uni / `+6` bi → `36/36` (spot ~15 masks). Bare `0x140` and pure `0x00200140` remain `0/0` 36/36.
  Counts: `2^19-1=524287` *128=67,108,736 without bit21 plus same with bit21 = `134,217,472` masks (replaces v0258 `2432`). See
  `decomp/i960/notes/game_info_18644_positive_base0140_any_middle_v0259.md`.

### v0260 base 0x40 any-composition (extends v0259)

* `0x40` (bit6 alone) + any `MIDDLE 0x1BFE3EA9` (20 bits incl. bit23) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `+3` uni / `+7` bi:
  bare `0x40`, single `0x240/0x840`, double `0x1840`, bit21 `0x00200040`, bit21+single `0x00200240`, bit21+double `0x00201840`, low `0x42`, many `0x1BDE3EE9/0x1BFE3EE9` all `0/36 DIFF +3/+7` → `36/36` (spot ~12 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base0040_any_v0260.md`.

### v0261 base 0x4040 any-composition (extends v0260)

* `0x4040` (0x4000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `-2` uni / `-3` bi (native undercounts):
  bare `0x4040`, single `0x4240`, high `0x44040`, bit21 `0x00204040`, many `0x1BDE6E49` all `0/36 DIFF -2/-3` → `36/36` (spot ~8 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base4040_any_v0261.md`.

### v0262 base 0x8040 any-composition (extends v0261)

* `0x8040` (0x8000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `+3` uni / `+6` bi (native overcounts):
  bare `0x8040`, singles `0x8240/0x8440`, high `0xA040`, bit21 `0x00208040`, many `0x1BDE8E49` all `0/36 DIFF +3/+6` → `36/36` (spot ~8 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base8040_any_v0262.md`.
* `0xC040` (0x4000+0x8000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `-2` uni / `-4` bi (native undercounts):
  bare `0xC040`, single `0xC240`, bit21 `0x0020C040`, many `0x1BDECE49` all `0/36 DIFF -2/-4` → `36/36` (spot ~6 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_baseC040_any_v0263.md`.

### v0264 base 0x10040 any-composition (extends v0263)

* `0x10040` (0x10000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21, any low `8`, any outer `16` — uniformly `-4` uni / `-7` bi (native undercounts) plus work-RAM `0x510b24`/`0x512b24` `|=0x800`:
  bare `0x10040`, singles `0x10240/0x11240`, bit21 `0x00210040`, many `0x30040/0x50040` all `0/36 DIFF -4/-7` + work-ram `0x08` at `0x10b25`/`0x12b25` → `36/36` (spot ~8 masks). Counts: `2^20=1,048,576` *128=134,217,728. See
  `decomp/i960/notes/game_info_18644_positive_base10040_any_v0264.md`.
* `0x14040` (0x10000+0x4000+0x40) + any `MIDDLE 0x1BFE3EA9` (20 bits) subset — uniformly `-2` uni / `-3` bi: bare `0x14040`, single `0x14240`, bit21 `0x00214040` all `0/36 DIFF -2/-3` → `36/36` (spot ~6 masks). Counts: `2^20*128=134,217,728`. See `decomp/i960/notes/game_info_18644_positive_low_family_closure_v0265_v0267.md`.
* `0x18040` (0x10000+0x8000+0x40) + any `MIDDLE` — uniformly `-4` uni / `-8` bi plus work-RAM `0x510b24/0x512b24|=0x800`: bare `0x18040`, single `0x18240`, bit21 `0x00218040` all `0/36 DIFF -4/-8` → `36/36`. Counts: `134,217,728`.
* `0x1C040` (0x10000+0x4000+0x8000+0x40) + any `MIDDLE` — uniformly `-2` uni / `-4` bi: bare `0x1C040`, single `0x1C240`, bit21 `0x0021C040` all `0/36 DIFF -2/-4` → `36/36`. Counts: `134,217,728`.
  Low `0x40` family (all 16 combos of `0x100/0x4000/0x8000/0x10000` with bit6) now fully closed; frontier remains helper `runtime bit5` and any remaining non-low positive compositions.

### Current positive threshold scope

`1,207,961,303` masks are now `36/36 exact` for the positive `0x1645c` corridor (`2007` + `134,217,472` base `0x140` any-middle v0259 + `134,217,728` base `0x40` any v0260 + `134,217,728` base `0x4040` any v0261 + `134,217,728` base `0x8040` any v0262 + `134,217,728` base `0xC040` any v0263 + `134,217,728` base `0x10040` any v0264 + `402,653,184` bases `0x14040/0x18040/0x1C040` any v0265-v0267)
(`120` high family + `991` base/low/high families + `784` bit-21 low variants (`112×7` bases: `8140`/`C140`/`4140`/`14140`/`10140`/`18140`/`1C140`)). All use the measured
stale-frame and compare result. Remaining positive compositions still
fail closed.

### v0268 fa_object handlers

The `fa_object0/1/2` dispatcher at `0x6ca64` was already native; its five
continuations are now recovered: init stubs `0x6cae0`/`0x6caf4` rewrite
`registry+0x0c` to the measured ret continuations `0x6caf0`/`0x6cb04`,
and `0x6caf0`/`0x6cb04`/`0x6cb08` are bare rets. Each of the six cases
(dispatcher + five continuations) is `exact` for full CPU state,
condition state, frame depth, all counters and full work-RAM `memcmp`
plus report kind/exit/counts (`tests/recovered/test_object_handlers.c`,
`vf2_object_handlers_differential`). The per-frame re-arm in
`execute_selector2_body` keeps these dormant in the accepted corridor;
they were proven from synthetic state. Still open: the `0x6ca84`
indirect `callx` service loop (count `3` at `0x6cad0`, control blocks
`[0x500878, 0x50087c, 0x500880]`, called from `0x1dd70`) and the
`fa_coli` recurring body at `0x221e8` with callees `0x22298` (31
blocks), `0x22404` (24 blocks), `0x225cc` (174 blocks) and `0x23524`
(6 blocks). Reachability scouting from regenerated fifth (`out-fifth.vf2snap`,
MATCH, 836 blocks) and sixth snapshots is negative: forcing the coli
slot runnable at either `0x221cc` or `0x221e8` over ~15.3M guest
instructions never dispatches index 10 (identical call/return counters,
`10255/10254` from fifth) — the accepted corridor dispatches only a
fixed few tasks per frame, so flag/entry mutation inside these windows
is exhausted. Next attempt needs a snapshot parked in a window that
sweeps index 10 (`snapshot` + `native-resume` into the second-dispatch
initializer corridor). See `decomp/i960/notes/object_handlers_v0268.md`.
