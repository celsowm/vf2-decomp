# TGP recovery

Virtua Fighter 2 Version 2.1 runs on Model 2A and uploads program words to the
TGP. This subtree will track:

- uploaded microcode;
- FIFO command formats;
- data-memory banks;
- matrix, lighting, clipping and geometry operations;
- C replacements validated against traces.

No TGP microcode is shipped in this repository.

## Recovered hardware boundary

`vf2_tgp` models the evidence-backed host boundary without embedding the
uploaded TGP microcode interpreter. The current boundary includes:

- the eight-word host-to-TGP and TGP-to-host FIFOs;
- the 0x1000-word uploaded-program window;
- the 0x400000 bank into Model 2 buffer RAM and the 0x800000 bank into
  copro data;
- the masked polygon-ROM word window used by object-data commands;
- the table-backed sin/cos, atan, inverse and inverse-square-root services;

- the function-port packet tag formed from the host word offset;
- the TGP FIFO command table (`commands.csv`, `docs/TGP_COMMANDS.md`): the
  `Fn_*` name, argument count and reply count of each opcode, measured on the
  game's own uploaded program and matched to Sega's PS2 port; and
- a bounded geometry-stream framing scanner for documented command classes,
  including object/direct/polygon data counts and end markers.
- a stateful software geometry executor for direct data, polygon-ROM/RAM object
  data, matrix/translation/focus state, link reuse and depth-tested triangle
  submission.

The lookup-table implementation uses the local 0x40000-byte TGP table region
and little-endian 32-bit entries. The scalar transformations are independently
implemented from the documented Model 2 I/O behavior; no MAME source is copied.

The polygon FIFO packet decoder, lighting/clipping fidelity and TGP microcode
execution remain unrecovered. A deterministic software reference path now
covers column-major matrix projection, viewport mapping, clipped triangle
submission and front-to-back depth testing through `vf2_platform`; it is
deliberately separate from the unknown hardware packet decoder. The bounded
scanner and executor cover the known geometry-stream command framing and
reject unsupported DSP/debug commands. The adjacent Model 2 geometry-ring
compatibility API is available as
`vf2_geometry`; it covers only the observed packed command and four-entry
frame-commit boundary. Unsupported DSP/debug commands are rejected rather than
assigned guessed lengths.

`vf2_game_attach_graphics` and the paired frame APIs provide the game-facing
software lifecycle. They intentionally stop at geometry submission; the
unrecovered match and fighter state machines are not synthesized here.
