# `fa_game_disp` initialization matrix — 2026-09-06

## Scope

This note records the measured recovery contract for the first `fa_game_disp`
activation in the VF2 v2.1 Model 2A/i960 program.

- task entry: `0x0002b1bc`
- task registry: `0x00515b00`
- registered continuation: `0x0002b1f8`
- scheduler return: `0x00010dcc`
- selector helper: `0x000026ec`
- nested selector/table helper: `0x0000281c`
- final initialization helper: `0x0002ab78`

The recovered path remains fail-closed. Inputs outside the explicitly measured
mode families continue through the interpreted task path.

## Recovered behavior

The initialization stores `0x0002b1f8` in registry slot `+0x0c`, computes the
selector bytes at `+0x59/+0x5a`, clears `+0x5b`, writes four `0xffffffff`
words at `+0x5c..+0x6b`, clears `+0x6c`, and preserves the observed `g9`
stack spill.

For mode flags without bit 1, the helper selects from the 26-entry pair table
used by `0x26ec/0x281c`. Mode id 25 is the measured zero-output fast path.
When mode flag bit 0 is set, both selectors use the first entry of the pair.

For mode flag bit 1, `0x281c` is not called. The output byte is derived directly
from the mode bytes. For selector `s` (`s=0` uses byte 3 and `s=1` uses byte 4):

```text
packed = (mode[1] << 16) | (mode[2] << 8) | mode[3 + s]
output = ((packed + 0x00010101) >> 8) & 0xff
```

Directed carry cases were included in the ROM-backed matrix rather than inferred
only from the arithmetic.

## Differential matrix

The ROM oracle corpus contains 62 cases:

- flags `0`: all mode ids `0..25` — 26 cases
- flags `1`: all mode ids `0..25` — 26 cases
- flags `2`: five directed arbitrary mode-byte tuples — 5 cases
- flags `3`: five directed arbitrary mode-byte tuples — 5 cases

Measured accounting regimes are:

| family | cases | instructions | calls | returns |
| --- | ---: | ---: | ---: | ---: |
| flags 2/3 direct path | 10 | 62 | 3 | 4 |
| mode id 25 fast path | 2 | 67 | 5 | 6 |
| flags 1 table path, mode 0..24 | 25 | 81 | 5 | 6 |
| flags 0 table path, mode 0..24 | 25 | 92 | 5 | 6 |

Commit `d0a26958` established exact RAM/device effects and all five accounting
counters for the matrix, but the stronger architectural hash exposed a stale
`local_frames[2]` difference inherited from the first narrow recovery in
`1704806d`.

Commit `f21cf823` restores the ROM-saved task frame after the architectural RET.
The official CI artifact for that commit passes **62/62 strong-exact** cases,
including RAM, device state, condition state, all five counters, current CPU
registers, and every saved local frame. The original zero-mode baseline also
matches byte-for-byte after this correction.

## Fail-closed boundary

The native initialization recovery is admitted only when the measured task-entry
shape is present: the expected task registry and scheduler frame relation, an
initial continuation of `0x0002b1bc`, zeroed task output area, supported mode
flag bits, and either a measured table-mode id or the measured direct bit-1
family. Any other context is delegated to the existing interpreted task path.

## Next frontier

The registered continuation `0x0002b1f8` is substantially larger: the baseline
execution is 2,903 instructions and 41 calls and emits geometry data. Its first
two calls are both to leaf `0x0002c5f0`. In the observed baseline each leaf is
15 instructions with no nested calls and updates one registry halfword (`+0x44`
for fighter 0, `+0x46` for fighter 1). Recovering that leaf is the next bounded
step before the larger display/geometry children.
