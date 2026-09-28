# v0392 — explicit Model 2A board reset

The BSD-3-Clause MAME Model 2 reference resets device-visible state and seeds
the first `0x20000` bytes of shared geometry buffer RAM with `0x07800f0f` in
`model2_state::machine_reset()`.

The portable model now exposes this as `vf2_model2a_reset`.  `initialize()`
remains the zeroed construction path used by the i960 oracle and existing
fixtures; callers that model a board reset can opt into the measured reset
state explicitly.  Attached ROM and host input are preserved.  The reset
operation clears interrupt/video/copro control, reloads and stops the four
25 MHz timers, resets geometry cursors and frame state, and applies the
MAME-cross-checked buffer seed.

Reference: `third_party/mame-model2-ref/model2.cpp`,
`model2_state::machine_reset()`.
