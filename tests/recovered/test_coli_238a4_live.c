/* ROM-backed differential fixture for fa_coli g3-scan helper live
 * (v0384).  Warm is 5 steps (bit8 clear).  Live bit8 with the
 * parked whole-task fighter state (f0 0x510b24=0x100, second
 * fighter clear) takes the 0x1aa/0x808 compare and the 30-trip
 * setbit loop: first g3-scan 136 steps, second 5 steps.  The
 * mirrored f1 (fighter1 bit8) is symmetric: first 5, second 134.
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

#define G3SCAN_ENTRY UINT32_C(0x000238a4)

static int failures = 0;
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #e); ++failures; } } while (0)

static vf2_status apply_f0(vf2_model2a *m) {
    if (vf2_model2a_write_u32(m, UINT32_C(0x00510b24), UINT32_C(0x00000100)) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    if (vf2_model2a_write(m, UINT32_C(0x005111a0), (const uint8_t *)"\x01", 1u) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    if (vf2_model2a_write_u32(m, UINT32_C(0x005149cc), UINT32_C(0x0000ffff)) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    return VF2_OK;
}
static vf2_status apply_f1(vf2_model2a *m) {
    if (vf2_model2a_write_u32(m, UINT32_C(0x00512b24), UINT32_C(0x00000100)) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    if (vf2_model2a_write(m, UINT32_C(0x005111a0), (const uint8_t *)"\x01", 1u) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    if (vf2_model2a_write_u32(m, UINT32_C(0x005149cc), UINT32_C(0x0000ffff)) != VF2_OK) return VF2_ERROR_UNSUPPORTED;
    return VF2_OK;
}

static void test_one(const uint8_t *rom, size_t rom_sz, const uint8_t *data, size_t data_sz, int f1) {
    vf2_model2a ref_m={0}, nat_m={0};
    vf2_i960_cpu ref_cpu={0}, nat_cpu={0};
    vf2_i960_snapshot snap; vf2_i960_snapshot_init(&snap);
    CHECK(vf2_model2a_initialize(&ref_m)); CHECK(vf2_model2a_initialize(&nat_m));
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rom_sz)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rom_sz)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, data_sz)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, data_sz)==VF2_OK);
    int ok = (vf2_i960_snapshot_read_file(&snap,"D:/ia/vf2-decomp/out/coli-parked-221e8.vf2snap")==VF2_OK ||
              vf2_i960_snapshot_read_file(&snap,"out/coli-parked-221e8.vf2snap")==VF2_OK);
    CHECK(ok);
    CHECK(vf2_i960_snapshot_restore(&snap,&ref_cpu,&ref_m)==VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap,&nat_cpu,&nat_m)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rom_sz)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rom_sz)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, data_sz)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, data_sz)==VF2_OK);
    if (f1) { CHECK(apply_f1(&ref_m)==VF2_OK); CHECK(apply_f1(&nat_m)==VF2_OK); }
    else { CHECK(apply_f0(&ref_m)==VF2_OK); CHECK(apply_f0(&nat_m)==VF2_OK); }
    size_t steps=0;
    while(ref_cpu.ip != G3SCAN_ENTRY && steps<20000){
        CHECK(vf2_i960_step(&ref_cpu,&ref_m,NULL)==VF2_OK);
        steps++;
    }
    CHECK(ref_cpu.ip == G3SCAN_ENTRY);
    steps=0;
    while(nat_cpu.ip != G3SCAN_ENTRY && steps<20000){
        CHECK(vf2_i960_step(&nat_cpu,&nat_m,NULL)==VF2_OK);
        steps++;
    }
    CHECK(nat_cpu.ip == G3SCAN_ENTRY);
    {
        uint64_t ref_start_ins = ref_cpu.executed_instructions;
        uint64_t csteps=0;
        uint64_t start_rets = ref_cpu.procedure_returns;
        while(ref_cpu.procedure_returns == start_rets && csteps<500){
            CHECK(vf2_i960_step(&ref_cpu,&ref_m,NULL)==VF2_OK);
            csteps++;
        }
        uint64_t ref_ins = ref_cpu.executed_instructions - ref_start_ins;
        uint64_t nat_start_ins = nat_cpu.executed_instructions;
        vf2_status ns = vf2_hybrid_coli_238a4_execute(&nat_m, &nat_cpu);
        uint64_t nat_ins = nat_cpu.executed_instructions - nat_start_ins;
        CHECK(ns==VF2_OK);
        if (f1==0) { CHECK(ref_ins==136u); } else { CHECK(ref_ins==5u); }
        CHECK(nat_ins==ref_ins);
        vf2_i960_snapshot_diff d2; memset(&d2,0,sizeof(d2));
        CHECK(vf2_i960_compare_live_state(&ref_cpu,&ref_m,&nat_cpu,&nat_m,&d2)==VF2_OK);
        CHECK(d2.equal);
    }
    {
        size_t s=0;
        while(ref_cpu.ip != G3SCAN_ENTRY && s<5000){
            CHECK(vf2_i960_step(&ref_cpu,&ref_m,NULL)==VF2_OK);
            s++;
        }
        CHECK(ref_cpu.ip == G3SCAN_ENTRY);
        s=0;
        while(nat_cpu.ip != G3SCAN_ENTRY && s<5000){
            CHECK(vf2_i960_step(&nat_cpu,&nat_m,NULL)==VF2_OK);
            s++;
        }
        CHECK(nat_cpu.ip == G3SCAN_ENTRY);
        uint64_t ref_start_ins = ref_cpu.executed_instructions;
        uint64_t start_rets = ref_cpu.procedure_returns;
        size_t csteps=0;
        while(ref_cpu.procedure_returns == start_rets && csteps<500){
            CHECK(vf2_i960_step(&ref_cpu,&ref_m,NULL)==VF2_OK);
            csteps++;
        }
        uint64_t ref_ins = ref_cpu.executed_instructions - ref_start_ins;
        uint64_t nat_start_ins = nat_cpu.executed_instructions;
        vf2_status ns = vf2_hybrid_coli_238a4_execute(&nat_m, &nat_cpu);
        uint64_t nat_ins = nat_cpu.executed_instructions - nat_start_ins;
        CHECK(ns==VF2_OK);
        if (f1==0) { CHECK(ref_ins==5u); } else { CHECK(ref_ins==134u); }
        CHECK(nat_ins==ref_ins);
        vf2_i960_snapshot_diff d2; memset(&d2,0,sizeof(d2));
        CHECK(vf2_i960_compare_live_state(&ref_cpu,&ref_m,&nat_cpu,&nat_m,&d2)==VF2_OK);
        CHECK(d2.equal);
    }
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
}

static void run_rom(const char *dir){
    uint8_t *rom=NULL,*data=NULL; size_t rs=0,ds=0;
    CHECK(vf2_romset_build_region(dir, VF2_REGION_MAINCPU, &rom,&rs)==VF2_OK);
    CHECK(vf2_romset_build_region(dir, VF2_REGION_MAIN_DATA, &data,&ds)==VF2_OK);
    test_one(rom,rs,data,ds,0);
    test_one(rom,rs,data,ds,1);
    free(rom); free(data);
}
int main(int argc,char **argv){
    if(argc!=2){ puts("coli-238a4-live ROM-independent passed"); return 0; }
    run_rom(argv[1]);
    if(failures){ fprintf(stderr,"%d coli-238a4-live failed\n",failures); return 1; }
    puts("coli-238a4-live differential tests passed");
    return 0;
}
