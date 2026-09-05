# References

Primary technical references for the project:

- MAME Sega Model 2 driver:
  `https://github.com/mamedev/mame/blob/master/src/mame/sega/model2.cpp`
- MAME Sega Model 2 geometry engine and rasterizer:
  `https://github.com/mamedev/mame/blob/master/src/mame/sega/model2_v.cpp`
- MAME Intel i960 implementation:
  `https://github.com/mamedev/mame/tree/master/src/devices/cpu/i960`
- Intel 80960KB Programmer's Reference Manual, document 270567-001.
- Intel i960 Processor Assembler User's Guide, document 272885.
- Sega Model 2 port DLL symbol table: the Windows ports keep the original
  board-side i960 symbol table in `.rdata`. 301 Virtua Fighter 2 names are
  recovered from it by `tools/python/extract_original_symbols.py` into
  `decomp/i960/original_symbols.csv`. The DLL itself is not redistributable and
  is not part of this repository; see `docs/ORIGINAL_SYMBOLS.md`.

MAME is used as executable hardware documentation and as a differential-testing
oracle. Its source is not copied into this repository. Intel documentation is
used to verify instruction and procedure-frame semantics.
