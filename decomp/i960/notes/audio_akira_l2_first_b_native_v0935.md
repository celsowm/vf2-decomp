# v0935 — first original Akira L2 B-class packet, an instruction-level native primitive

Date: 2026-10-25. Committed to master. The **whole music L2 tick remains unsupported** in normal dispatch.

## Original ROM basis

The owner-owned original sound 68000 ROM was inspected in CPU-visible (LOAD16_WORD_SWAP)
order. Akira music selector AD 10 04 now installs descriptor 0: status 88, timer 1,
next byte stream 0x615773, second music-record field 0x609C3E.

The first 3 bytes at that stream root select the B-class applier at
original ROM PC 0x1FFC, which first writes command/payload to the four-byte
command-ring entry, then clears **the top bit of the third byte**
at 0x2040 before writing the third-byte payload at 0x211C. If that
bit was set, it **sets bit 0 in descriptor status byte 0** via
the 0x2048 arm. The command's fourth sound-ring byte is untouched.

The original 0x211E..0x2128 path advances the 12-bit four-byte
ring write cursor and pending count. The A3 stream pointer was
postincremented three times, from 0x615773 to 0x615776, but
the worker descriptor pointer +4 is unchanged at stop-PC 0x212C.
The important distinction is that A3 is **temporary register
state**, not a committed stream cursor. Subsequent worker
control-flow may consume many more events in the same L2 tick.

## Native implementation

`vf2_sound_board_probe_akira_l2_first_b` is an explicitly-called,
ROM-bound developer diagnostic. It handles **only** the
0x615773 first music-root command shape (BE 50 FE). It:
- verifies descriptor 0 status 88, timer 1, bounded ROM source,
  four-byte ring alignment and pending FIFO capacity **before mutation**;
- executes the first original decrement and B-class packet emission;
- produces ring `BE 50 7E` with its pre-existing fourth byte intact;
- updates stream descriptor byte0 to 89, byte1 to BE, timer to 0;
- updates ring cursor by +4 mod 0x1000 and count +1;
- reports ephemeral A3=0x615776 without committing it to descriptor +4.

This primitive does **not** run from
`vf2_sound_board_maintain_streams`: allowing only one packet
from a music record and declaring the entire L2 IRQ complete would
change original behavior. Normal full music worker paths
still fail-closed transactionally.

The public ROM-free C tests cover the packet, count,
cursor, descriptor mutation, fourth-byte preservation,
non-reentrancy, invalid cursor/count and source mutation
refusal. No Sega code, music stream bytes or PCM are committed.

An owner-only ROM-backed differential starts the original
68000 at PC 0x1F7C on this same descriptor and stops
**before** PC 0x212C. It compares complete RAM and modeled
SCSP state, A3 register against the native report and
source+3 and the original descriptor. It is not part of
the ordinary public ROM-free CI, and still needs execution
with the user's original ROM set.

## Next exact frontier

Continue the original 68000 at 0x212C after this packet,
trace whether descriptor bit0 selects the next worker instruction,
and reconstruct the **whole bounded music stream** until the
original returns, including multi-packet ring emissions and
pointer state. Only then wire a completed tick into native
`vf2_sound_board_maintain_streams`, followed by the SCSP
and real interrupt scheduling separately. No fresh Akira WAV
or audio-fidelity claim is made here.
