# Third-party references

This repository does not include MAME source code or game ROMs.

The i960 instruction names, Sega Model 2 memory layout and ROM arrangement are
cross-checked against public Intel i960 documentation and the BSD-3-Clause MAME
implementation. See `docs/REFERENCES.md` for source locations.

## `third_party/stfdecomp` (Sonic The Fighters reference snapshot)

`third_party/stfdecomp/` holds an untracked reference snapshot of the
source-available Sonic The Fighters disassembly (another Sega Model 2 AM2
game on i960). It is **not open source**: its README states the code is
unlicensed / all rights reserved and viewable for research only.

Rules for this tree:

- Navigation aid only, same flow as Ghidra: STF suggests → our
  executor/oracle measures on the VF2 ROM → differential tests prove.
- Never copy STF assembly or semantics wholesale into `src/recovered/`;
  never link it into the runtime, executor or portable C17 build
  (`CMakeLists.txt` excludes `third_party/`).
- Never commit ROMs, snapshots, traces or extracted assets from it.
- Compact derived metadata (symbol/address/count comparisons) may be
  recorded under `decomp/i960/notes/`; game semantics still require
  VF2-oracle evidence per `AGENTS.md`.

See `third_party/README.md` and `decomp/i960/notes/stf_homology_map.md`.
