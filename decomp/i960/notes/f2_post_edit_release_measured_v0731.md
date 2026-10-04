# v0731 F2: rows 2-4 post-edit release frames — measured, still fail-closed

Frontier item **F2** from `v0730_first_action_runbook.md`: "rows 2-4
post-edit release frames (post-edit states with credit index != 2, derived
credits, preset != 0)". The bodies are now **measured**; they are **not
recovered**. Nothing is admitted.

## What the post-edit release frame is

The frame *after* a value-row edit. The edit frame renders the pre-edit
values; the deferred value update lands on the following frame. Latch shape
(admitted for row 1 as `{input parked, previous 0x0f000004, released 4,
nav 0}`):

```text
0x00500700 = 0x0f000000   input parked
0x00500704 = 0x0          nav none
0x00500708 = 0x4          TEST released
0x0050070c = 0x0f000004   previous = the TEST press
```

## Reproducing it (6 probe steps per row)

`base` at the `0xa6c0` boundary is **`0x599000`**, not the `0x59a3d0`
visible in the walk snapshots. Read `0x50016c` at the boundary you are
actually using; patching the wrong `base` silently measures nothing (this
already cost one false result on the a5 = 5 leg).

```sh
# 1. from an idle state at the row, cross the frame IRQ to the cluster entry
vf2probe --snapshot <idle>.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x00009ff8 --output-snapshot f2-<tag>-cl.vf2snap
# 2. patch the TEST latch AT 0x9ff8 (re-latched every frame otherwise)
vf2probe --snapshot f2-<tag>-cl.vf2snap --max-steps 1 --until 0x00009ff8 \
         --set-u32 0x00500700=0x0f000004 --set-u32 0x00500704=0x4 \
         --set-u32 0x00500708=0x0 --set-u32 0x0050070c=0x0f000000 \
         --output-snapshot f2-<tag>-edit.vf2snap
# 3. the edit frame, then onward to the next wait
vf2probe --snapshot f2-<tag>-edit.vf2snap --until 0x0000a010 \
         --output-snapshot f2-<tag>-editend.vf2snap
vf2probe --snapshot f2-<tag>-editend.vf2snap --until 0x00010fa0 \
         --output-snapshot f2-<tag>-wait2.vf2snap
# 4. cross the IRQ again, then patch the RELEASE latch
vf2probe --snapshot f2-<tag>-wait2.vf2snap --raise-irq 0x1 --enter-interrupt 12=1 \
         --until 0x00009ff8 --output-snapshot f2-<tag>-relcl.vf2snap
vf2probe --snapshot f2-<tag>-relcl.vf2snap --max-steps 1 --until 0x00009ff8 \
         --set-u32 0x00500700=0x0f000000 --set-u32 0x00500704=0x0 \
         --set-u32 0x00500708=0x4 --set-u32 0x0050070c=0x0f000004 \
         --output-snapshot f2-<tag>-rel.vf2snap
# 5. the reference body
vf2probe --snapshot f2-<tag>-rel.vf2snap --until 0x0000a010 \
         --output-snapshot f2-<tag>-ref.vf2snap
```

Starting idles that exist: `out/f1-a2-idle` is at **a5 = 3** and
`out/f1-a3-idle` is at **a5 = 4** (the `f1-aN-*` names are off by one against
`a5`; decode `0x5000a5` with explicit byte shifts, not a raw hex eyeball).
There is **no idle at a5 = 2** — see below.

## Measured results

All from the `0x9ff8` entry to the `0xa010` boundary. The idle render for
contrast is 4420 (232 prefix + 4188 body, 35 calls).

| row (`a5`) | screen label | edit frame | release chain | release body | credits after | preset after |
|---|---|---|---|---|---|---|
| 3 | `COIN/CREDIT SETTING     #  1` | 4637 | **4421** / 41 calls | **4189** | **`[2,2,2,3,3,1]`** | 0 |
| 4 | the `#  1` selector | 4635 | **4418** | **4186** | `[2,2,2,2,2,2]` | **1** |
| 2 | `CREDIT TO 1P START` | — | — | — | — | — |

Both measured rows are **fail-closed on the native side**:
`unsupported operation at 0x0000a6c0 ... entry=0x00009ff8`.

## The two distinct deferred behaviours

This is the part the runbook compressed into "credit index != 2, derived
credits, preset != 0", now separated:

- **Row 3 is a derived-credit update.** `[2,2,2,2,2,2]` becomes
  `[2,2,2,3,3,1]`. Incrementing the `COIN/CREDIT SETTING #1` value does not
  touch one field: indices 3 and 4 go to 3 and index 5 goes to 1. Any
  recovery needs this as a *rule*, and it needs more than one sample before
  it can be trusted — a single `+1` on a single row does not distinguish a
  derivation from a coincidence.
- **Row 4 is a `preset` update.** Credits are untouched; `preset`
  (`base + 0x3324`) becomes `1`. That is the `preset != 0` gate condition,
  and it is why the existing `preset != 0u` refusal fires here.

The bodies are also **per-row**, not one shared body: 4189 at row 3 versus
4186 at row 4, against 4188 for the idle render. Do not fold these into a
single counted rule without more samples.

## Row 2 is NOT measured

The only a5 = 2 artifact is `out/f1-test-row2`, and its `ip` is `0x10d54` —
past the frame wait, so `--raise-irq --until 0x9ff8` never reached the
cluster entry and both of the first two steps burned the 4,000,000 step cap
(`halt_reason: maximum steps`). The reference run that followed reported
4422 instructions, but from a state that was not the intended boundary.

**Do not use the 4422 figure.** To measure row 2 properly, reach it from a
real idle: `out/f1-a2-idle` (a5 = 3) with an UP tap
(`{input 0x0f002000, previous 0x0f000000, released 0, nav 0x2000,
a5 3}` — already admitted) lands on a5 = 2, then run the six steps above.

## What a recovery still needs

1. The **value-driven digit render**. `runs[]` hardcodes `"2"` at screen
   (6,40), (7,40), (8,40) and (9,40). With `credits != 2` those cells must
   render the live value, and the derived values mean several cells change
   from one edit.
2. The **derived-credits rule** for row 3, sampled at more than one value and
   more than one row before it is written down as a rule.
3. The **`preset` update** for row 4, including what a second edit does to a
   nonzero `preset`.
4. Per-row bodies at 4189 / 4186 (and row 2's, once measured), each proven
   against its own chained differential.

Until then these legs stay fail-closed. Nothing in this note admits a tuple.
