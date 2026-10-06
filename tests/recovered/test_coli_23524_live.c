/* ROM-backed live differential for the fa_coli poly shell at 0x23524 (v0733d).
 *
 * Why this test exists
 * --------------------
 * hybrid.c carried a standing TODO in the shell:
 *
 *     "g3 is deliberately NOT propagated to the CPU here. The shell's own
 *      contract is already proven by its differential, and whether 0x23524
 *      should publish the g3 its children left behind is a separate question
 *      that needs a shell differential to answer."
 *
 * The premise of that comment was wrong, and building this differential is
 * what proved it.
 *
 * On g3: the shell's last write to g3 inside its own extent is 0x238a4's
 * unconditional `mov 0, g3` at 0x238a8, which the admitted paths call before
 * reading g3 at 0x235b8/0x235c8. So on every admitted path g3 is 0 at the exit
 * no matter what the two 0x2396c calls left behind, and there is no children's
 * g3 left to publish. g3 is settled, and hybrid.c:31308 is right.
 *
 * On g4: the shell's own `ld (g11)[g12], g4` at 0x23600 overwrites the ~g4 with
 * a FIFO reply, so the ~g4 is discarded by the ROM, not by the C. Also settled.
 *
 * What the differential found, and a RETRACTION
 * ----------------------------------------------
 * v0733d reported three unrecovered exit fields: `g14`, `compare_result` and
 * `arithmetic_control` bit 1, and declared the last two unpinnable.
 *
 * **Two of those three were an artifact of the fixture, and are retracted.**
 * `compare_result` and `arithmetic_control` are NOT divergences on the warm leg:
 * the native publishes exactly what the reference does (EQUAL, `0x3f001002`).
 * They diverge only on the live leg.
 *
 * The cause was this file's own driver. It stepped the reference with a
 * hand-rolled `vf2_i960_step` loop; `vf2probe` and the differential tooling use
 * `vf2_i960_run`. Over the same 7 instructions from the same snapshot, in the
 * same binary, the two paths left different compare state at the shell entry
 * (NONE vs EQUAL) - which then changed the exit value on every leg. The
 * fixture now drives the reference with `vf2_i960_run`, and the bogus
 * "entry-state dependent, therefore unpinnable" claim is withdrawn.
 *
 * What survives:
 *
 *   1. `g14` is never published, and is path-dependent: 0x23648 on both
 *      admitted legs (they execute the `bal 0x23694` at 0x23644) but the
 *      untouched entry value 0x22428 on the refused leg, which branches to
 *      0x23648 at 0x235a8 and skips the `bal` entirely.
 *   2. ~~On the LIVE leg only, the native publishes cc EQUAL and
 *      `arithmetic_control` `0x3f001002` where the reference has GREATER and
 *      `0x3f001001`.~~ **CLOSED at v0734e.** The last compare-setting
 *      instruction was measured, not guessed: the vf2probe trace record now
 *      carries a per-instruction `compare`, and on both legs the final `cc`
 *      change comes from a different compare in the `bal 0x23694` body --
 *      `cmpinco` at 0x23930 (EQUAL) on warm, `cmpobne` at 0x236c8 (GREATER) on
 *      live. The live branch of the shell now publishes GREATER at
 *      hybrid.c's `cmpobl 0, r3` split, and the differential FULL MATCHes the
 *      reference on both condition fields.
 *
 *      Two earlier candidate explanations were measured and REJECTED, and are
 *      kept because the trap is easy to fall back into:
 *        - the `subr` at 0x23864 (the `+0x650` clamp). `g8 + 0x650` is 0 on
 *          BOTH legs, so it cannot produce EQUAL on one and GREATER on the
 *          other. The whole 0x23840..0x23874 tail holds cc unchanged.
 *        - `bbc` at 0x23868. Stopping either side of it gives the same state,
 *          so it does not write cc under `vf2_i960_run`.
 *      See decomp/i960/notes/coli_23524_live_cc_located_v0734d.md.
 *
 *   3. ~~`g14` (item 2.1).~~ **CLOSED at v0734f.** The shell's `bal 0x23694`
 *      at 0x23644 leaves g14 = 0x23648 on every admitted path. The native now
 *      publishes that value before `hybrid_complete_procedure`, and the
 *      refused leg's full-refusal gate (g6-bit0 at 0x23524) fires before any
 *      of the recovered tail runs, so it cannot resurrect C.
 *
 * Both phase-2 gaps closed. The differential on this boundary is FULL MATCH
 * on every admitted leg. See
 *   decomp/i960/notes/coli_23524_live_cc_recovered_v0734e.md
 *   decomp/i960/notes/coli_23524_live_g14_recovered_v0734f.md
 * for the measurements and the negative controls.
 *
 * Measured witness (vf2probe 0.1.3, this build, real ROM; entry
 * out/coli-parked-221e8.vf2snap -> 0x23524 in 7 instructions, depth 6,
 * g7 = 0x510980, g8 = 0x511900, g13 = 0x5150f0):
 *
 *   leg   recipe                              insn  cals rets  cc       g3        g4         g6  g14
 *   A     both +0x1a4 = 0                     9151   13   14   EQUAL    0x00000000 0x00000000   0  0x23648
 *   B     f0 +0x1a4=0x100, 0x5111a0=1,
 *         0x5149cc=0xffff                     9300   12   13   GREATER  0x00000000 0xffffdffc   2  0x23648
 *   C     f0 +0x1a4 bit 18                    6193   10   11   LESS     0x0000fffe 0xffffffff   1  0x22428
 *
 * Legs A and B are admitted and must otherwise match exactly. Leg C must fail
 * closed, and its refusal is load-bearing: it never calls 0x238a4, so 0x2396c's
 * g3 = 0x0000fffe and g4 = 0xffffffff survive to the exit, which is precisely
 * what the native shell would get wrong if it admitted the leg.
 *
 * Leg A also survives poisoning the entry g3 with 0xdeadbeef: identical counts,
 * g3 = 0 at the exit. So g3 = 0 is a measured outcome of this shell, not an
 * accident of the fixture's entry state.
 *
 * The boundary is 0x22210, NOT the `bx (g14)` at 0x23874 and NOT 0x23648. The
 * shell's tail is `bal 0x23694` (leaving g14 = 0x23648) ... `bx (g14)`, which
 * jumps to 0x23648 where `ret` finally pops the caller's frame. An earlier
 * draft of this file stopped at 0x23648 and reported the native shell as
 * overcounting by 1; the test was wrong, and v0386's whole-task 9151 pin is the
 * correct number.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define SHELL_ENTRY UINT32_C(0x00023524)
/* Where the shell actually leaves the CPU: the caller's return address.
 *
 * `vf2i960 function` reports end=0x2364c for this block, which is neither that
 * nor the extent. The mechanism (measured at v0734a, which corrects the
 * description v0733c gave of it): the tool's `end` is one past the highest `ret`
 * reachable through *direct* branches and fall-through, and it does NOT descend
 * into callees. The shell's 179-instruction body is entered by a call --
 *
 *     00023644  bal   0x00023694     <- the body is a callee
 *     00023648  ret                  <- the linear path stops here; end=0x2364c
 *     0002364c  ldt  0x1f4(g7), r4   <- and 0x2364c is coincidentally also a
 *                                      callee (`call 0x2364c` at 0x235a4)
 *
 * so the sweep never sees the body at 0x23694. Naming the coincidence matters:
 * it is what made the tool's answer look like a plain first-`ret` walk.
 * See decomp/i960/notes/function_extent_measured_v0734a.md. */
#define SHELL_RETURN UINT32_C(0x00022210)

#define G14_INDEX (VF2_I960_G0_REGISTER + 14u)
#define F0_FLAGS UINT32_C(0x00510b24) /* fighter0 0x510980 + 0x1a4 */
#define F1_FLAGS UINT32_C(0x00511aa4) /* fighter1 0x511900 + 0x1a4 */
#define F0_820_BYTE UINT32_C(0x005111a0)
#define REGISTRY_9CC UINT32_C(0x005149cc)
/* The two 0x2396c leftovers, per the v0732r body differential. These are what
 * SURVIVE on the refused leg, and are why refusing it is not caution. */
#define CHILD_G3 UINT32_C(0x0000fffe)
#define CHILD_G4 UINT32_C(0xffffffff)

static int failures = 0;

#define CHECK(e)                                                       \
    do {                                                               \
        if (!(e)) {                                                    \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__,  \
                    #e);                                               \
            ++failures;                                                \
            return 0;                                                  \
        }                                                              \
    } while (0)

typedef struct {
    const char *name;
    uint32_t f0_flags;
    uint32_t f1_flags;
    int set_f0_820;      /* the live-leg recipe byte */
    uint32_t registry;   /* 0x5149cc value for the live leg */
    uint64_t insn;
    uint64_t calls;
    uint64_t rets;
    uint32_t g3;
    uint32_t g4;
    uint32_t g6;
    uint32_t ref_g14;
    /* Recorded for documentation only. NOT asserted: the exit compare_result
     * is measurably entry-state dependent, so pinning it would encode a
     * property of the fixture rather than of the shell. */
    vf2_i960_compare_result ref_cc;
    int admits; /* 1: native must FULL MATCH. 0: native must fail closed. */
} leg_t;

static const leg_t LEGS[] = {
    { "A warm", 0u, 0u, 0, 0u, UINT64_C(9151), UINT64_C(13), UINT64_C(14),
      UINT32_C(0x00000000), UINT32_C(0x00000000), UINT32_C(0),
      UINT32_C(0x00023648), VF2_I960_COMPARE_EQUAL, 1 },
    { "B live f0", UINT32_C(0x00000100), 0u, 1, UINT32_C(0x0000ffff),
      UINT64_C(9300), UINT64_C(12), UINT64_C(13),
      UINT32_C(0x00000000), UINT32_C(0xffffdffc), UINT32_C(2),
      UINT32_C(0x00023648), VF2_I960_COMPARE_GREATER, 1 },
    { "C g6 bit0", UINT32_C(0x00040000), 0u, 0, 0u,
      UINT64_C(6193), UINT64_C(10), UINT64_C(11),
      UINT32_C(0x0000fffe), UINT32_C(0xffffffff), UINT32_C(1),
      UINT32_C(0x00022428), VF2_I960_COMPARE_LESS, 0 },
};

typedef struct {
    vf2_model2a m;
    vf2_i960_cpu cpu;
} side_t;

static void side_init(side_t *s, const uint8_t *rom, size_t rom_sz,
                      const uint8_t *data, size_t data_sz)
{
    memset(&s->m, 0, sizeof(s->m));
    memset(&s->cpu, 0, sizeof(s->cpu));
    /* vf2_model2a_initialize returns int (non-zero == success), not a
     * vf2_status. Comparing it against VF2_OK (which is 0) compiles cleanly
     * and is exactly backwards. */
    (void)vf2_model2a_initialize(&s->m);
    (void)vf2_model2a_attach_main_rom(&s->m, rom, rom_sz);
    (void)vf2_model2a_attach_main_data(&s->m, data, data_sz);
}

/* Restore replaces the attached regions, so re-attach the real ROM after. */
static int side_restore(side_t *s, const vf2_i960_snapshot *snap,
                        const uint8_t *rom, size_t rom_sz, const uint8_t *data,
                        size_t data_sz)
{
    if (vf2_i960_snapshot_restore(snap, &s->cpu, &s->m) != VF2_OK) {
        fprintf(stderr, "FAILED: snapshot restore\n");
        return 0;
    }
    if (vf2_model2a_attach_main_rom(&s->m, rom, rom_sz) != VF2_OK ||
        vf2_model2a_attach_main_data(&s->m, data, data_sz) != VF2_OK) {
        fprintf(stderr, "FAILED: re-attach after restore\n");
        return 0;
    }
    return 1;
}

static int side_apply(side_t *s, const leg_t *leg)
{
    static const uint8_t one = 1u;

    if (vf2_model2a_write_u32(&s->m, F0_FLAGS, leg->f0_flags) != VF2_OK ||
        vf2_model2a_write_u32(&s->m, F1_FLAGS, leg->f1_flags) != VF2_OK) {
        fprintf(stderr, "FAILED: could not set the fighter flag words\n");
        return 0;
    }
    if (leg->set_f0_820 &&
        vf2_model2a_write(&s->m, F0_820_BYTE, &one, 1u) != VF2_OK) {
        fprintf(stderr, "FAILED: could not set the live-leg recipe byte\n");
        return 0;
    }
    if (leg->registry != 0u &&
        vf2_model2a_write_u32(&s->m, REGISTRY_9CC, leg->registry) != VF2_OK) {
        fprintf(stderr, "FAILED: could not set the registry word\n");
        return 0;
    }
    return 1;
}

/* Drive the reference with vf2_i960_run, the same entry point vf2probe and the
 * differential tooling use.
 *
 * This is not a stylistic choice. A hand-rolled `vf2_i960_step` loop over the
 * same 7 instructions, from the same snapshot, in the same binary, produced a
 * DIFFERENT compare_result at the shell entry (NONE instead of EQUAL) - which
 * in turn changed the exit compare_result on every leg. See
 * decomp/i960/notes/executor_harness_cc_divergence_v0733e.md. A fixture that
 * measures compare state must measure it the way the tools do, or it is
 * measuring its own harness. */
static int side_run_to(side_t *s, uint32_t stop, unsigned long long *ran)
{
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    vf2_status status;

    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.stop_address = stop;
    options.max_steps = 400000u;
    options.stop_on_self_branch = true;
    status = vf2_i960_run(&s->cpu, &s->m, &options, &result);
    if (status != VF2_OK) {
        fprintf(stderr, "FAILED: reference run to 0x%08x: %s\n", (unsigned)stop,
                vf2_status_string(status));
        return 0;
    }
    if (s->cpu.ip != stop) {
        fprintf(stderr, "FAILED: reference stopped at 0x%08x, wanted 0x%08x\n",
                (unsigned)s->cpu.ip, (unsigned)stop);
        return 0;
    }
    *ran = (unsigned long long)result.executed_instructions;
    return 1;
}

static int side_walk_to_entry(side_t *s, unsigned *walked)
{
    unsigned long long ran = 0u;
    *walked = 0u;
    if (!side_run_to(s, SHELL_ENTRY, &ran)) {
        return 0;
    }
    *walked = (unsigned)ran;
    return 1;
}

/* Everything compare_live_state looks at, except the three fields this file
 * documents as unrecovered. Returns 1 when the remainder is equal.
 *
 * arithmetic_control is in the named set because the shell leaves bit 1 (the
 * "a compare has been executed" bit the executor keeps in lockstep with
 * compare_result) unset: the reference ends with 0x3f001002, the native with
 * 0x3f001000. */
static int equal_except_named(const vf2_i960_cpu *ref_cpu,
                              const vf2_i960_cpu *nat_cpu, unsigned *reg_diffs,
                              uint32_t *which_reg)
{
    unsigned reg;
    unsigned diffs = 0u;

    for (reg = 0u; reg < VF2_I960_REGISTER_COUNT; ++reg) {
        if (ref_cpu->registers[reg] != nat_cpu->registers[reg]) {
            ++diffs;
            *which_reg = reg;
        }
    }
    *reg_diffs = diffs;
    return (ref_cpu->sat == nat_cpu->sat) && (ref_cpu->prcb == nat_cpu->prcb) &&
           (ref_cpu->ip == nat_cpu->ip) &&
           (ref_cpu->process_control == nat_cpu->process_control) &&
           (ref_cpu->interrupt_control == nat_cpu->interrupt_control) &&
           (ref_cpu->reinitialized == nat_cpu->reinitialized) &&
           (ref_cpu->local_frame_depth == nat_cpu->local_frame_depth);
}

static int test_leg(const uint8_t *rom, size_t rom_sz, const uint8_t *data,
                    size_t data_sz, const vf2_i960_snapshot *snap,
                    const leg_t *leg)
{
    side_t ref;
    side_t nat;
    uint64_t ref_start_ins;
    uint64_t ref_start_calls;
    uint64_t ref_start_rets;
    uint64_t nat_start_ins;
    uint64_t ref_ins;
    uint64_t nat_ins;
    vf2_status nat_status;
    vf2_i960_snapshot_diff diff;
    unsigned walked = 0u;
    unsigned long long ran = 0u;
    int ref_cpu_at_entry_cc = 0;
    uint32_t ref_cpu_at_entry_arith = 0u;
    unsigned mismatches = 0u;
    int result = 0;

    memset(&diff, 0, sizeof(diff));
    side_init(&ref, rom, rom_sz, data, data_sz);
    side_init(&nat, rom, rom_sz, data, data_sz);

    /* Order matters: the recipe is applied AT the shell entry, after the walk.
     * Applying it before the walk lets those 7 instructions clobber it, which
     * is a silent divergence from any vf2probe run that mutates at the entry.
     * Both sides get the same treatment. */
    if (!side_restore(&ref, snap, rom, rom_sz, data, data_sz) ||
        !side_restore(&nat, snap, rom, rom_sz, data, data_sz) ||
        !side_walk_to_entry(&ref, &walked) || !side_walk_to_entry(&nat, &walked) ||
        !side_apply(&ref, leg) || !side_apply(&nat, leg)) {
        goto cleanup;
    }

    ref_start_ins = ref.cpu.executed_instructions;
    ref_start_calls = ref.cpu.procedure_calls;
    ref_start_rets = ref.cpu.procedure_returns;
    nat_start_ins = nat.cpu.executed_instructions;

    ref_start_ins = ref.cpu.executed_instructions;
    ref_start_calls = ref.cpu.procedure_calls;
    ref_start_rets = ref.cpu.procedure_returns;
    nat_start_ins = nat.cpu.executed_instructions;

    /* The compare state the shell INHERITS. The exit compare_result is not a
     * function of this shell alone, so the premise is recorded rather than
     * assumed - a fixture change that alters it must be visible here. */
    ref_cpu_at_entry_cc = (int)ref.cpu.compare_result;
    ref_cpu_at_entry_arith = ref.cpu.arithmetic_control;
    fprintf(stderr, "%s entry: ip=0x%08x after %u step(s) from the parked "
                    "snapshot, inherited cc=%d arith=0x%08x\n",
            leg->name, ref.cpu.ip, walked, ref_cpu_at_entry_cc,
            ref_cpu_at_entry_arith);

    if (!side_run_to(&ref, SHELL_RETURN, &ran)) {
        goto cleanup;
    }
    ref_ins = ref.cpu.executed_instructions - ref_start_ins;
    nat_status = vf2_hybrid_coli_23524_execute(&nat.m, &nat.cpu);
    nat_ins = nat.cpu.executed_instructions - nat_start_ins;

    /* Reference-side measurements first: they are the evidence, and a silent
     * drift in the corpus must show up as its own failure. */
    if (ref_ins != leg->insn) {
        fprintf(stderr, "MISMATCH %s: reference ran %llu insn, measured %llu\n",
                leg->name, (unsigned long long)ref_ins,
                (unsigned long long)leg->insn);
        ++mismatches;
    }
    if (ref.cpu.procedure_calls - ref_start_calls != leg->calls) {
        fprintf(stderr, "MISMATCH %s: reference made %llu calls, measured %llu\n",
                leg->name,
                (unsigned long long)(ref.cpu.procedure_calls - ref_start_calls),
                (unsigned long long)leg->calls);
        ++mismatches;
    }
    if (ref.cpu.procedure_returns - ref_start_rets != leg->rets) {
        fprintf(stderr, "MISMATCH %s: reference made %llu returns, measured %llu\n",
                leg->name,
                (unsigned long long)(ref.cpu.procedure_returns - ref_start_rets),
                (unsigned long long)leg->rets);
        ++mismatches;
    }
    if (ref.cpu.registers[VF2_I960_G0_REGISTER + 3u] != leg->g3) {
        fprintf(stderr, "MISMATCH %s: reference g3 0x%08x, measured 0x%08x\n",
                leg->name, ref.cpu.registers[VF2_I960_G0_REGISTER + 3u],
                leg->g3);
        ++mismatches;
    }
    if (ref.cpu.registers[VF2_I960_G0_REGISTER + 4u] != leg->g4) {
        fprintf(stderr, "MISMATCH %s: reference g4 0x%08x, measured 0x%08x\n",
                leg->name, ref.cpu.registers[VF2_I960_G0_REGISTER + 4u],
                leg->g4);
        ++mismatches;
    }
    if (ref.cpu.registers[G14_INDEX] != leg->ref_g14) {
        fprintf(stderr, "MISMATCH %s: reference g14 0x%08x, measured 0x%08x\n",
                leg->name, ref.cpu.registers[G14_INDEX], leg->ref_g14);
        ++mismatches;
    }
    /* compare_result is deliberately NOT asserted here. It is measurably
     * dependent on the compare state inherited at the entry: the refused leg
     * exits EQUAL when entered with cc NONE and LESS when entered with cc
     * EQUAL, over the identical 6193 instructions. A value that is not a
     * function of the shell alone cannot be pinned, which is the measured
     * reason this slice changes no C. The divergence itself is asserted
     * further down. */

    if (!leg->admits) {
        /* The refusal must be a refusal, and it must be justified by a
         * register difference rather than by caution. */
        if (ref.cpu.registers[VF2_I960_G0_REGISTER + 3u] != CHILD_G3 ||
            ref.cpu.registers[VF2_I960_G0_REGISTER + 4u] != CHILD_G4) {
            fprintf(stderr,
                    "FAILED premise %s: exit registers are g3=0x%08x g4=0x%08x, "
                    "expected the 0x2396c leftovers. The refusal would no "
                    "longer be load-bearing and this test would prove "
                    "nothing.\n",
                    leg->name, ref.cpu.registers[VF2_I960_G0_REGISTER + 3u],
                    ref.cpu.registers[VF2_I960_G0_REGISTER + 4u]);
            goto cleanup;
        }
        if (nat_status != VF2_ERROR_UNSUPPORTED) {
            fprintf(stderr,
                    "FAILED %s: native returned %d; it must fail closed "
                    "(g3=0x%08x / g4=0x%08x survive there)\n",
                    leg->name, (int)nat_status,
                    ref.cpu.registers[VF2_I960_G0_REGISTER + 3u],
                    ref.cpu.registers[VF2_I960_G0_REGISTER + 4u]);
            goto cleanup;
        }
        if (mismatches != 0u) {
            fprintf(stderr, "%s: %u mismatch(es)\n", leg->name, mismatches);
            goto cleanup;
        }
        fprintf(stderr,
                "%s: correctly REFUSED, and load-bearing - reference exits "
                "g3=0x%08x g4=0x%08x cc=%d g14=0x%08x after %llu insn / %llu "
                "calls\n",
                leg->name, CHILD_G3, CHILD_G4, (int)ref.cpu.compare_result,
                leg->ref_g14, (unsigned long long)ref_ins,
                (unsigned long long)leg->calls);
        result = 1;
        goto cleanup;
    }

    if (nat_status != VF2_OK) {
        fprintf(stderr, "FAILED %s: native refused the measured admitted leg\n",
                leg->name);
        goto cleanup;
    }
    if (nat_ins != ref_ins) {
        fprintf(stderr, "MISMATCH %s: native %llu != reference %llu insn\n",
                leg->name, (unsigned long long)nat_ins, (unsigned long long)ref_ins);
        ++mismatches;
    }

    if (vf2_i960_compare_live_state(&ref.cpu, &ref.m, &nat.cpu, &nat.m,
                                    &diff) != VF2_OK) {
        fprintf(stderr, "FAILED %s: live state compare\n", leg->name);
        goto cleanup;
    }

    /* Both Phases 2.1 (g14) and 2.2 (compare_result) closed at v0734f - the
    * differential is now FULL MATCH on the 0x22210 boundary for every
    * admitted leg. Anything less is a regression. */
    if (!diff.equal) {
        unsigned reg;
        fprintf(stderr,
                "MISMATCH %s: native state does not FULL MATCH the reference\n",
                leg->name);
        if (ref.cpu.sat != nat.cpu.sat) {
            fprintf(stderr, "  sat: 0x%08x vs 0x%08x\n", ref.cpu.sat, nat.cpu.sat);
        }
        if (ref.cpu.prcb != nat.cpu.prcb) {
            fprintf(stderr, "  prcb: 0x%08x vs 0x%08x\n", ref.cpu.prcb, nat.cpu.prcb);
        }
        if (ref.cpu.ip != nat.cpu.ip) {
            fprintf(stderr, "  ip: 0x%08x vs 0x%08x\n", ref.cpu.ip, nat.cpu.ip);
        }
        if (ref.cpu.process_control != nat.cpu.process_control) {
            fprintf(stderr, "  process_control: 0x%08x vs 0x%08x\n",
                    ref.cpu.process_control, nat.cpu.process_control);
        }
        if (ref.cpu.compare_result != nat.cpu.compare_result) {
            fprintf(stderr, "  compare_result: %d vs %d\n",
                    (int)ref.cpu.compare_result, (int)nat.cpu.compare_result);
        }
        if (ref.cpu.arithmetic_control != nat.cpu.arithmetic_control) {
            fprintf(stderr, "  arithmetic_control: 0x%08x vs 0x%08x\n",
                    ref.cpu.arithmetic_control, nat.cpu.arithmetic_control);
        }
        if (ref.cpu.interrupt_control != nat.cpu.interrupt_control) {
            fprintf(stderr, "  interrupt_control: 0x%08x vs 0x%08x\n",
                    ref.cpu.interrupt_control, nat.cpu.interrupt_control);
        }
        if (ref.cpu.reinitialized != nat.cpu.reinitialized) {
            fprintf(stderr, "  reinitialized: %u vs %u\n",
                    (unsigned)ref.cpu.reinitialized,
                    (unsigned)nat.cpu.reinitialized);
        }
        if (ref.cpu.local_frame_depth != nat.cpu.local_frame_depth) {
            fprintf(stderr, "  local_frame_depth: %u vs %u\n",
                    (unsigned)ref.cpu.local_frame_depth,
                    (unsigned)nat.cpu.local_frame_depth);
        }
        for (reg = 0u; reg < VF2_I960_REGISTER_COUNT; ++reg) {
            if (ref.cpu.registers[reg] != nat.cpu.registers[reg]) {
                fprintf(stderr, "  register[%u]: reference 0x%08x != native 0x%08x\n",
                        reg, ref.cpu.registers[reg], nat.cpu.registers[reg]);
            }
        }
        ++mismatches;
    }

    if (mismatches != 0u) {
        fprintf(stderr, "%s: %u mismatch(es)\n", leg->name, mismatches);
        goto cleanup;
    }
    fprintf(stderr,
            "%s: MATCH (%llu insn, %llu calls, %llu returns, g3=0x%08x "
            "g4=0x%08x); g14 0x%08x vs 0x%08x, cc %d vs %d, "
            "arithmetic_control 0x%08x vs 0x%08x\n",
            leg->name, (unsigned long long)ref_ins, (unsigned long long)leg->calls,
            (unsigned long long)leg->rets, leg->g3, leg->g4, leg->ref_g14,
            nat.cpu.registers[G14_INDEX], (int)ref.cpu.compare_result,
            (int)nat.cpu.compare_result, ref.cpu.arithmetic_control,
            nat.cpu.arithmetic_control);
    result = 1;

cleanup:
    vf2_model2a_shutdown(&ref.m);
    vf2_model2a_shutdown(&nat.m);
    return result;
}

static void run_rom(const char *dir)
{
    uint8_t *rom = NULL;
    uint8_t *data = NULL;
    size_t rom_sz = 0u;
    size_t data_sz = 0u;
    vf2_i960_snapshot snap;
    size_t i;

    if (vf2_romset_build_region(dir, VF2_REGION_MAINCPU, &rom, &rom_sz) !=
            VF2_OK ||
        vf2_romset_build_region(dir, VF2_REGION_MAIN_DATA, &data, &data_sz) !=
            VF2_OK) {
        fprintf(stderr, "FAILED: could not build the ROM regions\n");
        ++failures;
        free(rom);
        free(data);
        return;
    }
    vf2_i960_snapshot_init(&snap);
    if (vf2_i960_snapshot_read_file(
            &snap, "D:/ia/vf2-decomp/out/coli-parked-221e8.vf2snap") != VF2_OK &&
        vf2_i960_snapshot_read_file(
            &snap, "out/coli-parked-221e8.vf2snap") != VF2_OK) {
        fprintf(stderr, "FAILED: out/coli-parked-221e8.vf2snap not found\n");
        ++failures;
        vf2_i960_snapshot_destroy(&snap);
        free(rom);
        free(data);
        return;
    }
    for (i = 0u; i < sizeof(LEGS) / sizeof(LEGS[0]); ++i) {
        if (!test_leg(rom, rom_sz, data, data_sz, &snap, &LEGS[i])) {
            ++failures;
        }
    }
    vf2_i960_snapshot_destroy(&snap);
    free(rom);
    free(data);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        puts("coli-23524-live ROM-independent passed");
        return 0;
    }
    run_rom(argv[1]);
    if (failures) {
        fprintf(stderr, "%d coli-23524-live failed\n", failures);
        return 1;
    }
    puts("coli-23524-live differential tests passed");
    return 0;
}
