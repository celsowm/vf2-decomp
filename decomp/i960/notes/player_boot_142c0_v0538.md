# Player boot shape through `0x142c0` — v0538

The measured `out/pre14288-boot.vf2snap` shape is now covered by the existing
player recovery through both the `0x14288 -> 0x1428c` corridor and the
`0x1428c -> 0x142c0` geometry head.

## Evidence

- The reference reaches `0x1428c` in 1,622 instructions with 4 calls and 4
  returns, and reaches `0x142c0` in 10,869 instructions with 10 calls and 10
  returns.
- The selector is `0x505`, `g7` is `0x00510980`, and the entry
  `player+0x1a4` state is zero.
- Unlike the parked base/natres snapshots, the boot snapshot enters with
  local-frame depth zero and `player+0xbd8 == 0`. The oracle's successful bus
  trace writes the 60-byte expansion at `0x52078c`; the recovered path uses
  the measured transient scratch base `0x520000` for this exact shape.
- The selector branch base is zero in this snapshot, while the oracle reads
  the branch byte at `0x3351` and obtains zero. The selector setup helper
  therefore admits branch-base zero only for this exact player/selector pair.
- At the geometry head the record remains `0x0201c2fc`, the five selectors are
  `0x0505/0x0039/0x00f1/0x00e7/0x00af`, and the entry player flags are
  `0x00000800`.

The ROM-backed fixture runs the reference and native machines from the same
boot snapshot and compares full CPU state, condition state, local frames,
procedure counters, and mutable Model 2A state. The fixture also retains the
base and natres controls. The transient base, depth-zero admission and
branch-base exception are all shape-gated; unmeasured zero-frame or
zero-scratch paths remain unsupported.
