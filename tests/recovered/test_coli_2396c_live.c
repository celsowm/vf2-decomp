/* ROM-backed live differential for the 0x2396c warm poly-cluster builder
 * (v0732r).
 *
 * Why this test exists
 * --------------------
 * The only committed test for this block, test_coli_2396c_poly_cluster in
 * tests/recovered/test_native_runtime.c, is a self-consistency test. It
 *
 *   - synthesises an IDENTITY index table (rom[0x2394cu + i] = i), so the
 *     real 30-byte permutation at 0x2394c is never exercised;
 *   - never runs the reference executor, so it asserts the native result
 *     against hand-written constants rather than against the oracle; and
 *   - zeroes the cluster, so the slot's 4th word always reads as zero.
 *
 * That last point is not hypothetical. It is exactly the fixture condition
 * that made v0732p conclude "the padding word is zero". The 4th word is a
 * real load from the ROM window at 0x020078b4 - the guest loads r3 =
 * 0x020078a8 at 0x2396c and reads r3 + 0x0c + slot*16 - and in live state all
 * 30 are distinct non-zero floats. See
 * decomp/i960/notes/p2_producer_contract_v0732q.md.
 *
 * So a body that was right on the identity case and wrong on the permuted
 * case, or that hard-coded the 4th word to zero, would pass today.
 *
 * What this test does
 * -------------------
 * Starts from the parked whole-task state at the measured call site with the
 * real ROM attached, so the live permutation and the live 4th-word window are
 * both in play, then compares the reference executor against
 * vf2_hybrid_coli_2396c_execute with vf2_i960_compare_live_state.
 *
 * Measured witness (vf2probe, this build, real ROM):
 *   out/coli-parked-221e8.vf2snap -> 0x2396c       :   90 instructions
 *   0x2396c -> 0x23590, g7 = 0x510980             : 3023 instructions,
 *                                                   +3 calls, +4 returns,
 *                                                   frame depth 7 -> 6
 *   0x2396c -> 0x23590, g7 = 0x512980 (mirrored)  : identical counts
 *
 * The mirrored leg differs from the first in exactly one variable, g7, so it
 * is a true control on the admitted leg rather than a second arbitrary case.
 * Both legs must FULL MATCH, not just the one being admitted.
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

#define CLUSTER_ENTRY UINT32_C(0x0002396c)
#define CLUSTER_RETURN UINT32_C(0x00023590)
#define FIGHTER0 UINT32_C(0x00510980)
#define FIGHTER1 UINT32_C(0x00512980)
#define REGISTRY UINT32_C(0x00514980)
#define CLUSTER_BASE UINT32_C(0x00000d00)
#define CLUSTER_SLOTS 30u
#define CLUSTER_WORDS (CLUSTER_SLOTS * 4u)
#define INDEX_TABLE UINT32_C(0x0002394c)
#define EXTRA_WINDOW UINT32_C(0x020078b4)
/* Measured on this build; see the witness above. */
#define MEASURED_INSN UINT64_C(3023)
#define MEASURED_CALLS UINT64_C(3)
#define MEASURED_RETS UINT64_C(4)

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

static vf2_status apply_f0(vf2_model2a *m)
{
    /* Same state the sibling 0x238a4 live differential uses, so the two
     * fixtures are directly comparable. */
    if (vf2_model2a_write_u32(m, UINT32_C(0x00510b24),
                              UINT32_C(0x00000100)) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (vf2_model2a_write(m, UINT32_C(0x005111a0),
                          (const uint8_t *)"\x01", 1u) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    if (vf2_model2a_write_u32(m, UINT32_C(0x005149cc),
                              UINT32_C(0x0000ffff)) != VF2_OK) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return VF2_OK;
}

/* Guard the premise of the test itself. If the real permutation ever became
 * the identity, or the 4th-word column ever read as all zeros, this test would
 * silently stop covering the two things it exists to cover - which is exactly
 * how v0732p went wrong. */
static int check_fixture_premises(const uint8_t *rom, vf2_model2a *m)
{
    size_t i;
    unsigned non_identity = 0u;
    unsigned nonzero_extra = 0u;

    for (i = 0u; i < CLUSTER_SLOTS; ++i) {
        uint8_t index = rom[INDEX_TABLE + i];
        uint32_t extra = 0u;
        if (index != (uint8_t)i) {
            ++non_identity;
        }
        if (vf2_model2a_read_u32(m, EXTRA_WINDOW + (uint32_t)i * 16u,
                                 &extra) != VF2_OK) {
            fprintf(stderr, "FAILED premise: extra window unreadable at %u\n",
                    (unsigned)i);
            return 0;
        }
        if (extra != 0u) {
            ++nonzero_extra;
        }
    }
    if (non_identity == 0u) {
        fprintf(stderr,
                "FAILED premise: the 30-byte permutation at 0x%08x is the "
                "identity, so this test would not exercise it\n",
                (unsigned)INDEX_TABLE);
        return 0;
    }
    if (nonzero_extra == 0u) {
        fprintf(stderr,
                "FAILED premise: the 4th-word window at 0x%08x reads as all "
                "zeros, which is the v0732p fixture condition\n",
                (unsigned)EXTRA_WINDOW);
        return 0;
    }
    fprintf(stderr,
            "fixture premise: %u/30 non-identity permutation bytes, "
            "%u/30 non-zero 4th words\n",
            non_identity, nonzero_extra);
    return 1;
}

static int test_one(const uint8_t *rom, size_t rom_sz, const uint8_t *data,
                    size_t data_sz, uint32_t fighter)
{
    vf2_model2a ref_m;
    vf2_model2a nat_m;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu nat_cpu;
    vf2_i960_snapshot snap;
    vf2_i960_snapshot_diff diff;
    uint64_t ref_start_ins;
    uint64_t ref_start_calls;
    uint64_t ref_start_rets;
    uint64_t nat_start_ins;
    uint64_t ref_ins;
    uint64_t nat_ins;
    size_t steps;
    unsigned mismatches = 0u;
    int result = 0;

    memset(&ref_m, 0, sizeof(ref_m));
    memset(&nat_m, 0, sizeof(nat_m));
    memset(&ref_cpu, 0, sizeof(ref_cpu));
    memset(&nat_cpu, 0, sizeof(nat_cpu));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snap);

    /* vf2_model2a_initialize returns int (non-zero == success), not a
     * vf2_status. Comparing it against VF2_OK (which is 0) compiles cleanly
     * and is exactly backwards. */
    CHECK(vf2_model2a_initialize(&ref_m) != 0);
    CHECK(vf2_model2a_initialize(&nat_m) != 0);
    if (ref_m.work_ram == NULL || nat_m.work_ram == NULL) {
        vf2_model2a_shutdown(&ref_m);
        vf2_model2a_shutdown(&nat_m);
        return 1;
    }
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rom_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rom_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, data_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, data_sz) == VF2_OK);

    if (vf2_i960_snapshot_read_file(
            &snap, "D:/ia/vf2-decomp/out/coli-parked-221e8.vf2snap") != VF2_OK &&
        vf2_i960_snapshot_read_file(
            &snap, "out/coli-parked-221e8.vf2snap") != VF2_OK) {
        fprintf(stderr, "FAILED: out/coli-parked-221e8.vf2snap not found\n");
        vf2_i960_snapshot_destroy(&snap);
        vf2_model2a_shutdown(&ref_m);
        vf2_model2a_shutdown(&nat_m);
        return 0;
    }
    CHECK(vf2_i960_snapshot_restore(&snap, &ref_cpu, &ref_m) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap, &nat_cpu, &nat_m) == VF2_OK);
    /* Restore replaces the attached regions, so re-attach the real ROM. */
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rom_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rom_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, data_sz) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, data_sz) == VF2_OK);
    CHECK(apply_f0(&ref_m) == VF2_OK);
    CHECK(apply_f0(&nat_m) == VF2_OK);

    if (!check_fixture_premises(rom, &ref_m)) {
        goto cleanup;
    }

    for (steps = 0u; steps < 20000u; ++steps) {
        if (ref_cpu.ip == CLUSTER_ENTRY) {
            break;
        }
        CHECK(vf2_i960_step(&ref_cpu, &ref_m, NULL) == VF2_OK);
    }
    CHECK(ref_cpu.ip == CLUSTER_ENTRY);
    for (steps = 0u; steps < 20000u; ++steps) {
        if (nat_cpu.ip == CLUSTER_ENTRY) {
            break;
        }
        CHECK(vf2_i960_step(&nat_cpu, &nat_m, NULL) == VF2_OK);
    }
    CHECK(nat_cpu.ip == CLUSTER_ENTRY);

    /* The single variable under test: which fighter g7 points at. This must be
     * set AT the entry. Setting it before the approach is useless - the caller
     * reloads g7 on the way in, which is why a pre-entry --set-reg g7 in
     * vf2probe silently has no effect here. */
    ref_cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter;
    nat_cpu.registers[VF2_I960_G0_REGISTER + 7u] = fighter;

    if (ref_cpu.registers[VF2_I960_G0_REGISTER + 7u] != fighter ||
        nat_cpu.registers[VF2_I960_G0_REGISTER + 7u] != fighter) {
        fprintf(stderr, "FAILED: could not pin g7 to 0x%08x at the entry\n",
                fighter);
        goto cleanup;
    }

    ref_start_ins = ref_cpu.executed_instructions;
    ref_start_calls = ref_cpu.procedure_calls;
    ref_start_rets = ref_cpu.procedure_returns;
    nat_start_ins = nat_cpu.executed_instructions;
    /* Run to the return address, NOT to the first change in
     * procedure_returns: the three inlined 0x23878 remaps are real calls, so
     * the first return belongs to a callee, not to this procedure. */
    for (steps = 0u; steps < 20000u; ++steps) {
        if (ref_cpu.ip == CLUSTER_RETURN) {
            break;
        }
        CHECK(vf2_i960_step(&ref_cpu, &ref_m, NULL) == VF2_OK);
    }
    CHECK(ref_cpu.ip == CLUSTER_RETURN);
    ref_ins = ref_cpu.executed_instructions - ref_start_ins;

    CHECK(vf2_hybrid_coli_2396c_execute(&nat_m, &nat_cpu) == VF2_OK);
    nat_ins = nat_cpu.executed_instructions - nat_start_ins;

    /* Report the counts and the state in the SAME run. Early-returning on the
     * count first hides whether the poststate also differs, and both were
     * needed to localise a real defect. */
    if (ref_ins != MEASURED_INSN) {
        fprintf(stderr, "MISMATCH reference ran %llu instructions, measured %llu\n",
                (unsigned long long)ref_ins,
                (unsigned long long)MEASURED_INSN);
        ++mismatches;
    }
    if (nat_ins != ref_ins) {
        fprintf(stderr, "MISMATCH native %llu != reference %llu instructions\n",
                (unsigned long long)nat_ins, (unsigned long long)ref_ins);
        ++mismatches;
    }
    if (ref_cpu.procedure_calls - ref_start_calls != MEASURED_CALLS) {
        fprintf(stderr, "MISMATCH reference made %llu calls, measured %llu\n",
                (unsigned long long)(ref_cpu.procedure_calls - ref_start_calls),
                (unsigned long long)MEASURED_CALLS);
        ++mismatches;
    }
    if (ref_cpu.procedure_returns - ref_start_rets != MEASURED_RETS) {
        fprintf(stderr, "MISMATCH reference made %llu returns, measured %llu\n",
                (unsigned long long)(ref_cpu.procedure_returns - ref_start_rets),
                (unsigned long long)MEASURED_RETS);
        ++mismatches;
    }

    CHECK(vf2_i960_compare_live_state(&ref_cpu, &ref_m, &nat_cpu, &nat_m,
                                      &diff) == VF2_OK);
    if (!diff.equal) {
        fprintf(stderr,
                "MISMATCH live state: component=%s bytes=%u first_offset=0x%zx "
                "expected=0x%08x actual=0x%08x\n",
                diff.component, (unsigned)diff.differing_bytes,
                diff.first_offset, diff.expected_value, diff.actual_value);
        ++mismatches;
    }

    /* Name the block explicitly, so a failure points at the cluster rather
     * than at a generic state diff. */
    {
        size_t word;
        for (word = 0u; word < CLUSTER_WORDS; ++word) {
            uint32_t want = 0u;
            uint32_t got = 0u;
            if (vf2_model2a_read_u32(&ref_m, fighter + CLUSTER_BASE +
                                                  (uint32_t)word * 4u,
                                     &want) != VF2_OK ||
                vf2_model2a_read_u32(&nat_m, fighter + CLUSTER_BASE +
                                                  (uint32_t)word * 4u,
                                     &got) != VF2_OK) {
                fprintf(stderr, "FAILED: cluster word %u unreadable\n",
                        (unsigned)word);
                goto cleanup;
            }
            if (want != got) {
                fprintf(stderr,
                        "MISMATCH cluster word %u (+0x%03x): reference 0x%08x != "
                        "native 0x%08x\n",
                        (unsigned)word, (unsigned)(word * 4u), want, got);
                ++mismatches;
            }
        }
    }
    for (unsigned slot = 0u; slot < CLUSTER_SLOTS; ++slot) {
        uint32_t want = 0u;
        uint32_t got = 0u;
        if (vf2_model2a_read_u32(&ref_m, EXTRA_WINDOW + slot * 16u, &want) !=
                VF2_OK ||
            vf2_model2a_read_u32(&nat_m, fighter + CLUSTER_BASE + slot * 16u +
                                          12u, &got) != VF2_OK) {
            fprintf(stderr, "FAILED: 4th word unreadable at slot %u\n", slot);
            goto cleanup;
        }
        if (want != got) {
            fprintf(stderr,
                    "MISMATCH slot %u 4th word: window 0x%08x != cluster 0x%08x\n",
                    slot, want, got);
            ++mismatches;
        }
    }

    if (mismatches != 0u) {
        fprintf(stderr, "fighter 0x%08x: %u mismatch(es)\n", fighter,
                mismatches);
        goto cleanup;
    }
    fprintf(stderr,
            "fighter 0x%08x: FULL MATCH (%llu insn, %llu calls, %llu returns, "
            "all %u cluster words, all 30 4th words)\n",
            fighter, (unsigned long long)ref_ins,
            (unsigned long long)(ref_cpu.procedure_calls - ref_start_calls),
            (unsigned long long)(ref_cpu.procedure_returns - ref_start_rets),
            (unsigned)CLUSTER_WORDS);
    result = 1;

cleanup:
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
    return result;
}

static void run_rom(const char *dir)
{
    uint8_t *rom = NULL;
    uint8_t *data = NULL;
    size_t rom_sz = 0u;
    size_t data_sz = 0u;

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
    if (rom_sz <= INDEX_TABLE + CLUSTER_SLOTS) {
        fprintf(stderr, "FAILED: main CPU region too small for the index table\n");
        ++failures;
        free(rom);
        free(data);
        return;
    }
    /* The measured call site, then the mirrored fighter as the control. */
    if (!test_one(rom, rom_sz, data, data_sz, FIGHTER0)) {
        ++failures;
    }
    if (!test_one(rom, rom_sz, data, data_sz, FIGHTER1)) {
        ++failures;
    }
    free(rom);
    free(data);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        puts("coli-2396c-live ROM-independent passed");
        return 0;
    }
    run_rom(argv[1]);
    if (failures) {
        fprintf(stderr, "%d coli-2396c-live failed\n", failures);
        return 1;
    }
    puts("coli-2396c-live differential tests passed");
    return 0;
}
