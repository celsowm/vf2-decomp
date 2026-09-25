#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

static int failures=0;
#define CHECK(e) do{ if(!(e)){fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#e); ++failures;} }while(0)

static vf2_status apply_f0(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00510b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=1; s=vf2_model2a_write(m,0x005111a0,&v,1);
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffff);
}
static vf2_status apply_f1(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00512b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=1; s=vf2_model2a_write(m,0x005111a0,&v,1);
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffff);
}
static vf2_status apply_both(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00510b24, 0x00000100);
    if(s!=VF2_OK) return s;
    s=vf2_model2a_write_u32(m, 0x00512b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=1; s=vf2_model2a_write(m,0x005111a0,&v,1);
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffff);
}
static vf2_status apply_both_high(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00510b24, 0x00000100);
    if(s!=VF2_OK) return s;
    s=vf2_model2a_write_u32(m, 0x00512b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=5; s=vf2_model2a_write(m,0x005111a1,&v,1);
    if(s!=VF2_OK) return s;
    uint8_t zero[2]={0,0}; s=vf2_model2a_write(m,0x005111a2,zero,sizeof(zero));
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x00510d84, 0);
}

static void test_one(const uint8_t *rom,size_t rs,const uint8_t *data,size_t ds,int f1){
    vf2_model2a ref_m={0}, nat_m={0};
    vf2_i960_cpu ref_cpu={0}, nat_cpu={0};
    vf2_i960_snapshot snap; vf2_i960_snapshot_init(&snap);
    (void)vf2_model2a_initialize(&ref_m);
    (void)vf2_model2a_initialize(&nat_m);
    CHECK(vf2_model2a_attach_main_rom(&ref_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m,data,ds)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m,data,ds)==VF2_OK);
    int ok = (vf2_i960_snapshot_read_file(&snap,"out/coli-parked-221e8.vf2snap")==VF2_OK ||
              vf2_i960_snapshot_read_file(&snap,"D:/ia/vf2-decomp/out/coli-parked-221e8.vf2snap")==VF2_OK);
    CHECK(ok);
    CHECK(vf2_i960_snapshot_restore(&snap,&ref_cpu,&ref_m)==VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap,&nat_cpu,&nat_m)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&ref_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m,data,ds)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m,data,ds)==VF2_OK);
    if(f1==3) {CHECK(apply_both_high(&ref_m)==VF2_OK); CHECK(apply_both_high(&nat_m)==VF2_OK);}
    else if(f1==2) {CHECK(apply_both(&ref_m)==VF2_OK); CHECK(apply_both(&nat_m)==VF2_OK);}
    else if(f1) {CHECK(apply_f1(&ref_m)==VF2_OK); CHECK(apply_f1(&nat_m)==VF2_OK);}
    else {CHECK(apply_f0(&ref_m)==VF2_OK); CHECK(apply_f0(&nat_m)==VF2_OK);}
    // reference whole-task stepping
    size_t steps=0;
    while(ref_cpu.ip != 0x00010dcc && steps<10000){
        CHECK(vf2_i960_step(&ref_cpu,&ref_m,NULL)==VF2_OK);
        steps++;
    }
    CHECK(ref_cpu.ip==0x00010dcc);
    uint64_t ref_ins = ref_cpu.executed_instructions - snap.cpu.executed_instructions;
    uint64_t ref_calls = ref_cpu.procedure_calls - snap.cpu.procedure_calls;
    uint64_t ref_rets = ref_cpu.procedure_returns - snap.cpu.procedure_returns;
    // nat via hybrid_first_dispatch (uses hybrid_execute_coli_body)
    uint32_t registry = nat_cpu.registers[29];
    // ensure ip is coli entry
    CHECK(nat_cpu.ip==0x000221e8);
    vf2_hybrid_task_report rep; memset(&rep,0,sizeof(rep));
    vf2_status ns = vf2_hybrid_first_dispatch_task_execute(&nat_m,&nat_cpu,registry,&rep);
    CHECK(ns==VF2_OK);
    CHECK(nat_cpu.ip==0x00010dcc);
    uint64_t nat_ins = nat_cpu.executed_instructions - snap.cpu.executed_instructions;
    uint64_t nat_calls = nat_cpu.procedure_calls - snap.cpu.procedure_calls;
    uint64_t nat_rets = nat_cpu.procedure_returns - snap.cpu.procedure_returns;
    const char *mode_str = f1==3 ? "both-high" : f1==2 ? "both" : f1 ? "f1" : "f0";
    printf("mode %s: ref %llu/%llu/%llu nat %llu/%llu/%llu\n", mode_str,
        (unsigned long long)ref_ins,(unsigned long long)ref_calls,(unsigned long long)ref_rets,
        (unsigned long long)nat_ins,(unsigned long long)nat_calls,(unsigned long long)nat_rets);
    if(f1==0){
        CHECK(ref_ins==9393); CHECK(ref_calls==17); CHECK(ref_rets==18);
    } else if(f1==1) {
        CHECK(ref_ins==9385); CHECK(ref_calls==17); CHECK(ref_rets==18);
    } else if(f1==2) {
        CHECK(ref_ins==9528); CHECK(ref_calls==18); CHECK(ref_rets==19);
    } else {
        CHECK(ref_ins==9526); CHECK(ref_calls==18); CHECK(ref_rets==19);
    }
    CHECK(nat_ins==ref_ins);
    CHECK(nat_calls==ref_calls);
    CHECK(nat_rets==ref_rets);
    vf2_i960_snapshot_diff d; memset(&d,0,sizeof(d));
    CHECK(vf2_i960_compare_live_state(&ref_cpu,&ref_m,&nat_cpu,&nat_m,&d)==VF2_OK);
    if(!d.equal){
        fprintf(stderr,"live state mismatch %s: %s off 0x%zx exp 0x%08x act 0x%08x bytes %zu\n", mode_str, d.component, d.first_offset, d.expected_value, d.actual_value, d.differing_bytes);
        ++failures;
    }
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
}

static void run_rom(const char *dir){
    uint8_t *rom=NULL,*data=NULL; size_t rs=0,ds=0;
    CHECK(vf2_romset_build_region(dir,VF2_REGION_MAINCPU,&rom,&rs)==VF2_OK);
    CHECK(vf2_romset_build_region(dir,VF2_REGION_MAIN_DATA,&data,&ds)==VF2_OK);
    test_one(rom,rs,data,ds,0);
    test_one(rom,rs,data,ds,1);
    test_one(rom,rs,data,ds,2);
    test_one(rom,rs,data,ds,3);
    free(rom); free(data);
}
int main(int argc,char **argv){
    if(argc!=2){ puts("coli-whole-task-live ROM-independent passed"); return failures?1:0; }
    run_rom(argv[1]);
    if(failures){ fprintf(stderr,"%d coli-whole-task-live failed\n",failures); return 1; }
    puts("coli-whole-task-live differential tests passed");
    return 0;
}
