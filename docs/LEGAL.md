# Legal and clean-room policy

This project does not distribute Sega game ROMs, extracted game assets or
copyrighted source code.

Users must supply their own legally obtained copy of the supported ROM set.
The project stores only factual metadata required to identify and arrange the
files, including filenames, sizes and cryptographic hashes.

Contributors must not:

- commit ROMs or reconstructed region images;
- paste leaked or proprietary original source code;
- submit extracted art, audio, models or textures;
- claim clean-room recovery when proprietary materials were used.

## Disclosed non-clean-room material: symbol names

`decomp/i960/original_symbols.csv` contains 301 original Sega symbol *names* for
the i960 program, recovered from the symbol table that Sega left inside a later
Model 2 port DLL. These names were not derived independently, so the naming of
those 301 addresses is **not** clean-room and is not claimed to be.

What this does and does not cover:

- committed: the `(address, name)` pairs only, which are factual metadata of the
  same class as the rest of `decomp/i960/*.csv`;
- not committed and not redistributable: the DLL itself, or any code, data or
  disassembly from it. `tools/python/extract_original_symbols.py` reads a DLL
  the user supplies, exactly as the build reads a ROM the user supplies;
- no implementation in `src/recovered/` was derived from these names. A name
  says what a routine is called, not how it behaves, and every recovered
  behavior in this repository remains backed by measured differential evidence.

### Why this is not leaked source code or game data

The distinction matters, because the two prohibitions above that sound closest
to this file — do not paste leaked or proprietary original source code, do not
commit ROMs or extracted assets — do not describe what a symbol table is.

**It was published, not leaked.** The table is in the `.rdata` section of a
retail binary Sega sells. Every copy of the product carries it, byte for byte,
and reading it needs nothing beyond the copy a user already owns. Leaked source
means material that was never published and reached the public through a breach
of confidence; this is the opposite of that in every respect. Nothing here came
from a Sega source tree, an internal build, an NDA, or a disclosure anyone was
not entitled to make.

**It is not source code.** A symbol table is a list of `(integer, identifier)`
pairs. It contains no statements, no expressions, no control flow, no types, no
structure layouts, no constants, no comments and no algorithms — nothing that
expresses *how* any routine works. It is the same class of artifact as a PDB, a
DWARF `.debug_*` section, a linker map file or a DLL export directory: the
addresses and names a debugger needs, and nothing else. Knowing that `0x18644`
is called `get_en_info` tells you what to call it. It does not tell you a single
thing the function does; that still has to be measured, and in this repository
it is.

**It is not game data.** It is not a ROM, not a reconstructed ROM region, not
art, audio, models, textures or game strings. Nothing in it is renderable or
playable, and no part of the ROM can be reconstructed from it. The 301 names
occupy the same evidence role as the addresses already in
`decomp/i960/symbols.csv` — the difference is that these ones are right.

**What it actually is: leftover debugging metadata.** Sega's Model 2 ports
emulate the original i960 board, and the port keeps the board program's symbol
table so its own diagnostics can put a name to a guest address. Nothing in the
shipped DLL references those strings; they are reachable only by walking the
table, which is why they survive as an inert island in `.rdata`. This is the
debug apparatus of the port, retained in a retail build because nobody stripped
it — the emulator-side equivalent of shipping with symbols left on. Sega has
shipped that layer repeatedly, across the 2012 PlayStation 3 and Xbox 360
Model 2 releases and the later Windows builds, and each title in the family
carries its own table. The copy read here came from a Win64 build.

**What this does not settle.** The names are still Sega's own identifiers,
chosen by Sega's programmers, and this project did not arrive at them
independently. That is precisely why this section exists: the *naming* of those
301 addresses is disclosed as non-clean-room, and none of the reasoning above is
offered as a legal conclusion. It explains why the material is debug metadata
from a shipped product rather than leaked source or extracted game content — not
that authorship stops mattering.

`docs/ORIGINAL_SYMBOLS.md` records the provenance and explains how to drop the
file if the project would rather keep naming strictly clean-room.

This repository is not affiliated with Sega. “Virtua Fighter” and related
names are trademarks of their respective owners.
