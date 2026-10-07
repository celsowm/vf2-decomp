# v0755: per-step hook — boundary investigation, NOT yet wired

**Status: design note, no code change.** This documents the
per-step hook design that would activate the 10 already-recovered
sub-callees (v0741/v0745/v0746/v0747/v0749/v0750/v0751/v0752/
v0753/v0754) in the running dispatcher chain, plus a discovered
compatibility issue with v0741 path D's partial-simulation
pattern.

`git diff --stat src/`: empty.
`git diff --stat decomp/i960/functions.csv`: empty.

## The capability the project needs

10 functions are recovered in C and unit-tested but **not active
in the running code**. They live inside interpreted ranges
(`hybrid_execute_interpreted_until`) and would need a per-step
hook to be called when the IP lands on their entry.

The 10 functions are all sub-callees of `0x29414` (4 of them) or
`0x1fcc0` (6 of them), and they are:

| callee | parent | size | status |
|---|---|---|---|
| `0x29598` | `0x29414` | 84 B | v0741 — paths A/B/C recovered, path D refused |
| `0x439ac` | `0x29414` | 80 B | v0745 — 4 paths |
| `0x43888` | `0x29414` | 200 B | v0746 — 6 paths + count-full negative control |
| `0xcf04` | `0x29414` | 184 B | v0747 — 2 paths + 1 refused sub-call |
| `0x1fee4` | `0x1fcc0` | 36 B | v0749 — trivial 26-iter init |
| `0x1ff0c` | `0x1fcc0` | 240 B | v0750 — 3 paths |
| `0x1fffc` | `0x1fcc0` | 88 B | v0751 — 2 paths + 1 refused sub-call |
| `0x4b410` | `0x1fcc0` | 60 B | v0752 — 5 writes |
| `0x11704` | `0x1fcc0` | 64 B | v0753 — 1 path, nested loop |
| `0x2eab8` | `0x1fcc0` | 364 B | v0754 — 1 path, inlined sub-call to `0x31004` |

All 10 are unit-tested PASS in ctest entries #38–#47. They are
all "stand-alone execute hooks": IP-in, side-effects, IP-out, no
caller-specific state needed beyond what they read from work RAM.

## The per-step hook design (v0744 candidate A, refined)

The hook lives in `hybrid_execute_interpreted_until` (line 436).
The existing 0x16504 special case at line 451 is a precedent for
a per-step loop; the new design generalizes that pattern.

```c
typedef vf2_status (*vf2_hybrid_callee_hook)(
    vf2_model2a *machine, vf2_i960_cpu *cpu);

typedef struct vf2_hybrid_callee_hook_entry {
    uint32_t entry_ip;
    vf2_hybrid_callee_hook hook;
} vf2_hybrid_callee_hook_entry;

static const vf2_hybrid_callee_hook_entry g_callee_hooks[] = {
    { UINT32_C(0x00029598), vf2_hybrid_player_29598_execute },
    { UINT32_C(0x000439ac), vf2_hybrid_player_439ac_execute },
    /* ... 8 more entries ... */
};
```

The per-step loop in `hybrid_execute_interpreted_until` (replacing
the existing `vf2_i960_run` path for ranges with at least one
registered hook):

```c
while (status == VF2_OK && cpu->ip != stop_address && steps < N) {
    /* Step one instruction. */
    status = vf2_i960_step(cpu, machine, NULL);
    if (status != VF2_OK) break;
    ++steps;
    /* Check if a recovered callee wants to take over. */
    const vf2_hybrid_callee_hook_entry *hook = find_hook(cpu->ip);
    if (hook != NULL) {
        status = hook->hook(machine, cpu);
        /* VF2_OK: function fully handled the call; continue.
         * VF2_ERROR_UNSUPPORTED: function refused; continue
         *     (we're already past the entry, the loop will
         *     not re-fire because cpu->ip != entry_ip).
         * Other: abort. */
    }
}
```

The "per-step loop replaces `vf2_i960_run`" change is the
invasive part. It changes the generic dispatcher for ANY range
that has at least one registered hook. For ranges without hooks,
the existing `vf2_i960_run` path is preserved.

The existing 0x16504 special case (line 451) is preserved
because it has tighter count checks that the per-step loop
doesn't reproduce. The 3 special cases at lines 362–408 in
`hybrid_execute_interpreted_task` are also preserved.

## A compatibility issue: v0741 path D's partial simulation

The v0741 recovery (0x29598, ctest #38) has 4 paths. Paths A,
B, C are fully recovered: they set `cpu->ip` to `0x295e8` (the
ret), advance `executed_instructions`, and return `VF2_OK`. Path
D is currently a "partial simulation + refuse":

```c
/* Path D: g1 == 15, setbit + 3 sub-calls.
 * Sub-calls (0xcf04, 0x439ac, 0x43888) are not yet recovered;
 * refuse this path so the dispatcher falls back to
 * hybrid_execute_interpreted_task. */
{
    uint32_t r15 = 0u;
    status = vf2_model2a_read_u32(machine, UINT32_C(0x00500068), &r15);
    if (status != VF2_OK) return status;
    r15 |= UINT32_C(1) << 20u;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x00500068), r15);
    if (status != VF2_OK) return status;
    cpu->registers[VF2_I960_G0_REGISTER + 0u] = UINT32_C(0x00ad231f);
    cpu->ip = UINT32_C(0x000295e8);
    cpu->executed_instructions += UINT64_C(8);
}
return VF2_ERROR_UNSUPPORTED;
```

This **conflicts with the per-step hook design**. If path D
refuses, the per-step loop sees `cpu->ip == 0x295e8` (not the
entry) and continues — but the 3 sub-calls and the `mov 0, g1`
between the entry and the ret have been **skipped entirely**.

The original i960 path D body is:

```
0x295bc  ld       0x00500068, r15     ; load
0x295c4  setbit   20, r15, r15        ; set bit 20
0x295c8  st       r15, 0x00500068     ; store back
0x295d0  call     0x0000cf04          ; sub-call 1 (recovered v0747)
0x295d4  lda      0x00ad231f, g0      ; load address
0x295dc  call     0x000439ac          ; sub-call 2 (recovered v0745)
0x295e0  call     0x00043888          ; sub-call 3 (recovered v0746)
0x295e4  mov      0, g1               ; g1 = 0
0x295e8  ret                          ; the function's ret
```

The v0741 partial simulation handles the `setbit` (storing bit
20 set in 0x500068) and the `lda 0x00ad231f, g0` (cpu->g0 =
0x00ad231f), but skips the 3 sub-calls and the `mov 0, g1`.
Setting `cpu->ip = 0x295e8` causes the per-step loop to continue
from the ret, missing the sub-calls and the g1 reset.

## The two ways to resolve this

**Option A: Refactor v0741 path D to refuse cleanly (no partial
simulation).** The function would just return `VF2_ERROR_UNSUPPORTED`
with `cpu->ip` unchanged. The per-step loop would step one
instruction forward, re-check the hook table, and continue
interpretation. The v0741 unit test would need to be updated to
not expect the setbit/g0 effect on path D. This is the cleanest
fit for the per-step design but is a backward-incompatible
change to a previously-merged recovery.

**Option B: Expand v0741 path D to fully handle the path by
calling the 3 recovered sub-callees (v0745/v0746/v0747) directly.**
The path D recovery would push a frame, call each sub-callee, pop
the frame, and set `cpu->ip = 0x295e4` (so the `mov 0, g1`
executes via interpretation, or just inline it). This is more
work but keeps the partial-simulation pattern. It also requires
frame-push/pop plumbing inside the recovered function, which
isn't yet a pattern.

A third option: leave the v0741 path D as-is, and don't wire
0x29598 into the per-step table at all. Paths A/B/C of 0x29598
would still need to be called somehow. We could add a "first-IP
check" pattern: in `hybrid_execute_interpreted_until`, if
`cpu->ip == 0x29598` and paths A/B/C are the active paths
(determined by some pre-check), call the function. But this
requires the dispatcher to know which path is active, which it
doesn't.

## Recommendation

Option A is the cleanest. Refactor v0741 path D to refuse cleanly,
update the v0741 unit test, then implement the per-step hook and
wire 0x29598. This is a 2-3 commit sequence (v0755 retract path D,
v0756 implement the hook, v0757 wire 0x29598 and verify).

Alternatively, the per-step hook can be implemented without
wiring 0x29598. Wire 0x1fee4/v0749, 0x1ff0c/v0750, 0x1fffc/v0751,
0x4b410/v0752, 0x11704/v0753, 0x2eab8/v0754 first (these are
all "fully-handle" functions with no partial-simulation issues
because they inline or refuse cleanly). Then 0x439ac/v0745 and
0x43888/v0746 (fully-handle, all paths). Then 0xcf04/v0747 (has
a refused sub-call to `0x1fcc0`, but the refused sub-call sets
cpu->ip to the post-call IP cleanly, so the per-step loop
would continue correctly). Then finally 0x29598/v0741 after the
path D retraction.

## The deferred 0x2c38

`0x2c38` (color_table_rebuild, 432 B, 11 blocks, complex with
non-standard `cmpinco` instructions) is the last un-recovered
sub-callee of `0x1fcc0`. Investigation during v0755 surfaced a
disassembly ambiguity: the disasm at 0x2d40 shows `subo 1, 0, g1`
but the binary encoding `0x59881901` has `M1=1, S1=0, S2=1` which
the project's decoder may not be interpreting consistently. This
is consistent with the `cmpinco` complexity — the function is a
2D color-table rebuild with a saturated-arithmetic inner loop
and a multi-word multiply/divide pre-stage. It's tractable as a
C recovery but needs the disasm-vs-encoding question resolved
first. **Defer until a focused disasm audit resolves the
`subo 1, 0, g1` ambiguity.**

## Validated

- All 10 new recoveries PASS in ctest entries #38–#47 (v0741–v0754).
- All 42 player tests PASS (excluding the 3 long-running
  `vf2_player_4505_*` differential tests).
- The 0x2eab8/v0754 inlining of 0x31004 is mechanically correct
  (5 distinct writes to r4-base from the inlined sub-call, all
  in non-overlapping addresses from the outer function's writes).
- The per-step hook design is consistent with the existing 0x16504
  per-step special case at `hybrid.c:451`.

## What the next slice should pick up

- **v0755a (refactor)**: Retract v0741 path D's partial simulation.
  Update ctest #38 to not expect the setbit/g0 effect on path D.
  Update v0741 note to document the retraction. 1 commit, low
  risk.
- **v0755b (infrastructure)**: Implement the per-step hook in
  `hybrid_execute_interpreted_until` and add a small
  `g_callee_hooks` table. Add 6 "clean" entries (v0749–v0754).
  Verify all existing tests still pass. 1 commit, medium risk.
- **v0755c (verify)**: Wire 0x29598 and 0x439ac/0x43888/0xcf04
  (v0741, v0745, v0746, v0747) and run the existing
  0x29414..0x164c4 differential test to confirm the dispatcher
  still FULL MATCHES the reference. 1 commit, medium risk.
- **v0755d (deferred)**: Recover 0x2c38 once the disasm
  ambiguity is resolved.
