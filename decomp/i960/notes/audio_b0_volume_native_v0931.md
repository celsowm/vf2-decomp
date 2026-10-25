# v0931 — Original VF2.1 B0/10 routine recovered as native C

Date: 2026-10-25. Direct to master.

## New original-ROM evidence

The user-owned private `vf2(20261018-203049).zip` was accessed
locally, and its `epr-17574.30` 68000 sound program was read in
correct CPU-visible (16-bit word-swapped) order. **The ROM, samples,
music and binary disassembly were NOT committed or shared.**

The B0 jump table at 0x13DE was confirmed from the real bytes:
- selector 0x10 -> PC 0x181C;
- selector 0x12 -> PC 0x13DC (default RTS);
- selector 0x1A -> PC 0x18A8 (prescaler setup);
- selector 0x50 -> PC 0x1C0A (still unresolved).

The original 0x181C..0x18A6 routine is **140 bytes** and
contains the following nested algorithm, now reimplemented in C:

1. Byte-left-shift incoming D0 by one. If the shifted
   byte is nonzero, increment the byte by one. Store the
   result at `0x251F` as the shared duration factor.
2. For 10 command descriptors at `0x4000 + i*16`,
   multiply `desc[3]` by factor as unsigned 16-bit,
   shift the word result right eight bits, increment
   nonzero quotient by one, and store to `desc[10]`.
3. For each of 32 voice records at `0x2800 + j*16`,
   compare `voice[4]` with the descriptor channel.
   Continue only if bit 6 of `voice[2]` is set.
4. Multiply `voice[8]` by `desc[10]`. If zero,
   write FF to `SCSP[voice.slot_word + 0x0D]`;
   otherwise write `(0xFF - ((product >> 8)&0xFF)) >> 1`.
   D4.W is an unscaled slot byte-offset as in the original
   indexed SCSP addressing, NOT a slot-number multiplier.

The correct 68000 B0 drain clears D0's upper bits before
loading the argument byte, so the native 16-bit multiplication
uses the clean byte-derived factor. The C path preflights the
SCSP indexed register bounds for *all* matching voices before
the first mutation, avoiding partial updates on invalid input.

## Dispatch and verification

Native C now handles `BE 10 xx` automatically **only**
when the actual attached audio ROM has the exact measured
0x181C..0x18A7 140-byte FNV-1a 64-bit fingerprint. Unknown
variants retain a fail-closed FIFO hold; users can still opt
in to the experimental v0930 interpreter bridge separately.

Public ROM-free tests assert 10/20 descriptor boundaries,
32-voice matching and flag gating, zero/nonzero products,
SCSP offset semantics, byte-wrapped factor, and transaction
refusal for invalid signed slot offsets. No proprietary
ROM data is needed by these tests.

The owner-original `test_b0_181c_native_original` compares
**full sound RAM and SCSP model state** after executing the
real original 68000 body against native C for seven input
values 00/01/04/40/7F/80/FF and heterogeneous synthetic
descriptor and voice records. Unlike the stand-alone
ROM-free synthetic suite, this private differential is not
part of GitHub Actions; it needs to run with the owner's ROM.

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/private/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

**Limitations:** the real original-ROM differential still
needs execution; recognition of the original ROM body is a
strong instruction-identity guard, not proof that the full
driver's register history is equivalent to the C dispatch.
Other commands (notably B0/50 -> 0x1C0A), interrupt
scheduling, and SCSP DSP/PCM parity remain open.
No new audible waveform is claimed for this change.
