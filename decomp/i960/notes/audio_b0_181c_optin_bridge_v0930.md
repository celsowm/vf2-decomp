# v0930 — bounded original 0x181C interpreter bridge (experimental opt-in)

Date: 2026-10-25. Direct to master.

## Goal

The native B0 handler cannot yet safely translate the authentic
`BE 10 xx` volume/duration code at 68000 PC `0x181C`.
The existing C remains fail-closed by default.

A new, **explicit opt-in** `allow_original_b0_181c_fallback`
allows an experimental middle step: execute the attached original
sound ROM's `0x181C` body with the project's existing MC68000
stepper, **in a disposable snapshot of sound RAM and SCSP**.
This does not import proprietary bytes, nor invent a volume curve.

- Original ROM B0 selector must resolve to target `0x181C`.
- The bridge seeds the reconstructed minimal context
  (`A5=0x100000`, `A6=0x1000`, `A7=0x7F00`, `D0=packet byte2`,
  `D2=0x10`, `A3=ring+cursor+2`); full register history is NOT
  preserved, therefore real-ROM parity is still unproven.
- Execution stops only on an actual RTS within 4,096
  instructions, and refuses unknown opcodes, failed bounds,
  unhandled hardware events, or other terminations.
- Exactly two classes of RAM effect are accepted:
  `0x251F` (shared factor), and `descriptor[+0x0A]`
  within the twenty 16-byte command descriptors.
  Internal 68000 stack scratch is ignored and never published.
- Other writes, SCSP changes or effects outside those addresses
  refuse, preserving the live board and the queued command.
- A successful permitted opcode sequence copies *only*
  the allowed modified bytes back and lets the existing
  dispatcher retire the command.

**This is an oracle-assisted transition aid, not native code
or a validated faithful emulator of 0x181C.** It is disabled
by default. It makes no promise that the real ROM completes
under this limited context or that its effects fit the
whitelist. A refusal is the safe expected outcome for an
unobserved branch.

## ROM-free regression

`test_b0_181c_transactional_interpreter()` uses a synthetic
ROM opcode sequence `MOVE.B D0,$151F(A6); RTS` to validate
success under explicit opt-in, and then tests rollback on
an unexpected RAM address, an unknown opcode after the
first write, and an infinite loop against the 4096-step
budget. The default-off behavior also remains covered.
No byte sequence is represented as original Sega code.

## Original-ROM next experiment

Run the owner-private probe already added in v0929:

```sh
./build/vf2_m68k_cdispatch_tests /private/vf2 --probe-b0-181c
```

Compare the original execution's factor changes and
descriptor +0A deltas with the above allowed write set.
If it writes additional addresses (for example an SCSP
slot), analyze the exact instructions **before** widening
the bridge or authoring native arithmetic.

Original PC disassembly for direct examination:

```sh
./build/vf2m68k disasm /private/vf2 0x181c 80 --raw
```

No private ROM, game audio samples or music recordings
are checked into this repository.
