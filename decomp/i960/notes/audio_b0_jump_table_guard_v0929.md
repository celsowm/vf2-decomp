# v0929 — eliminate ambiguous B0 prescaler selector using the actual ROM jump table

Date: 2026-10-25. Direct commit to master.

## Critical cross-check of v0928

An audit of the historical table dumps exposed a contradictory
claim:

- `audio_b0_table_surface_v0788.md` says selector **0x12**
  targets `0x18A8`.
- The later independently enumerated tables in
  `audio_drain_path_correction_v0794.md` and
  `audio_b0_table_map_v0799.md` say **0x1A** (26 decimal)
  targets `0x18A8`, while `0x12` should be default RTS.

v0928 had trusted the first assertion without checking the
attached ROM and could therefore incorrectly configure the
prescaler for `BE 12 xx`. The previous ROM-dependent test
entered 0x18A8 *directly*: it would have compared the body but
not authenticated the dispatch selector.

## New rule: sound ROM is authoritative

For disputed live selectors `0x10`, `0x12`, `0x1A`
and `0x50`, native C now decodes an actual
`JMP (d16,PC)` entry from the attached CPU-visible
sound ROM at `0x13DE + 4*selector`. Target is extension-word
address `entry+2 + signed16(displacement)`, **not**
the opcode address plus 4. Malformed, absent, out-of-range
or unexpected jump targets refuse without touching
the prescaler or consuming the FIFO entry.

- Actual `0x18A8` target: run recovered 2-byte
  prescaler rate/countdown stores.
- Actual `0x13DC` target: ROM default RTS, no
  prescaler effects.
- Actual `0x181C`/`0x1C0A` targets: pending real
  handlers, hold FIFO rather than inventing effects.

The ROM-free fixture pins the later table map:
`BE 1A 08 -> 0x18A8 -> rate/countdown 8`,
`BE 10 04 -> 0x181C`, `BE 12 xx -> 0x13DC`,
`BE 50 7E -> 0x1C0A`. This fixture is a *synthetic
index test*, NOT proof that private ROM has the
same table. It also mutates the source opcode and
checks the no-side-effect refusal path.

The owner-only differential now checks the original
ROM JMP words and absolute targets for both disputed
indices before executing the four real `0x18A8`
instructions and comparing their descriptor effects
with native C for input arguments `00`, `08`, `FF`.
It will fail on an original-ROM disagreement; it
has not been executed here without private assets.

No game ROM, PCM, WAV, or copyrighted code copied
from a proprietary image is checked in.

## Critical outstanding frontier

`0x181C` is **not yet translated**.
The original B0/10 `BE 10 xx` volume/shared-duration
rescaling may touch `0x251F` and ten descriptors;
historical C from 2026-09 incorrectly scaled the
individual incoming descriptor instead of a global
factor. Do not resurrect that model blindly or
consider audio equivalent merely because the queue
emits `BE 10 04`. A ROM-backed instruction/side-effect
differential is needed before native admission.

To run owner tests:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/private/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
