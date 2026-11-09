#define HEAD_LATE_PAUSED_READER_BOUND 1
#include "shared-phase-clock-runtime.inc"
/* Test integration only. Private original kernel, no authored gain edits.
 * The observer captures original transaction first; applied coupled state and
 * downstream live XMM0/XMM3 are replaced together by the after bridge. */
static void hkt_jump(unsigned char*,uintptr_t);
#include "head-kernel-native-clone.inc"
#include "optional-real-clock-runtime.inc"
#include "optional-real-head-core.inc"
#include "late-sensor-q-core.inc"
static int lsq_scope_hold(const LsqSelector*,int64_t,int*,LsqPoint*);
static int lsq_unavailable_binding(const LsqSelector*,LsqBinding*,uint64_t*);
static int lsq_velocity_pair_Q(const LsqSelector*,int64_t,LsqPoint*,LsqPoint*);
#include "head-late-q-core.inc"
#include "head-late-q-runtime.inc"

#include "head-pause-api.inc"
static HCCNativeKernel hcl_kernel;
static int hcl_enabled;
static SRWLOCK hcl_lock=SRWLOCK_INIT,hcl_kernel_lock=SRWLOCK_INIT;
typedef struct {HCLState state;HLLQState late;uint64_t generation;unsigned busy;} HCLSlot;
static HCLSlot hcl_slots[64];
static uint64_t hcl_generation;
static HKTHeader*hcl_tape_header;static HKTRecord*hcl_tape_records;
static SRWLOCK hcl_tape_lock=SRWLOCK_INIT;
static void hcl_forget(uintptr_t owner){
 hps_forget(owner);
 AcquireSRWLockExclusive(&hcl_lock);
 for(unsigned i=0;i<64;++i)if(!owner||hcl_slots[i].state.owner==owner){HCLSlot*s=hcl_slots+i;hcl_invalidate(&s->state);hllq_forget(&s->late);s->state.owner=owner;s->generation=++hcl_generation;}
 ReleaseSRWLockExclusive(&hcl_lock);
}
static int hcl_step_record(void*opaque,unsigned char actor[0x380],const HKTInputs*input,uint32_t csr,uint32_t*post){
 const HKTRecord*source=opaque;HKTRecord record=*source;record.input=*input;memcpy(record.before,actor,0x380);uint32_t after;
 AcquireSRWLockExclusive(&hcl_kernel_lock);int ok=hcc_native_step(&hcl_kernel,actor,input,csr,&after);ReleaseSRWLockExclusive(&hcl_kernel_lock);
 if(!ok)return 0;if(post)*post=after;
 if(hcl_tape_header){record.mxcsr_before=csr;record.mxcsr_after=after;memcpy(record.after,actor,0x380);
  AcquireSRWLockExclusive(&hcl_tape_lock);uint32_t at=hcl_tape_header->count;
  if(at<hcl_tape_header->capacity){record.sequence=at;hcl_tape_records[at]=record;MemoryBarrier();hcl_tape_header->count=at+1;}else ++hcl_tape_header->overflow;
  ReleaseSRWLockExclusive(&hcl_tape_lock);
 }
 return 1;
}
static int hcl_apply(const HKTRecord*r){
 if(!hcl_enabled)return 0;
 HCLSlot*slot=NULL;HCLState state;HLLQState late;uint64_t generation;
 AcquireSRWLockExclusive(&hcl_lock);
 for(unsigned i=0;i<64;++i)if(hcl_slots[i].state.owner==(uintptr_t)r->actor){slot=hcl_slots+i;break;}
 if(!slot)for(unsigned i=0;i<64;++i)if(!hcl_slots[i].state.owner){slot=hcl_slots+i;slot->state.owner=(uintptr_t)r->actor;slot->generation=++hcl_generation;break;}
 if(!slot||slot->busy){ReleaseSRWLockExclusive(&hcl_lock);return 0;}
 slot->busy=1;state=slot->state;late=slot->late;generation=slot->generation;ReleaseSRWLockExclusive(&hcl_lock);
 unsigned char coupled[64];HCSClock clock=hps_head_clock(r);int status=hllq_runtime_update(&state,&late,r,hcl_step_record,(void*)r,coupled,&clock),applied=0;
 AcquireSRWLockExclusive(&hcl_lock);
 unsigned char actual[64];int unchanged=camera_pose_read((uintptr_t)r->actor+0x1a0,actual,64)&&!memcmp(actual,r->after+0x1a0,64);
 if(slot->generation==generation&&unchanged){
  slot->state=state;slot->late=late;
  if(status==HCL_APPLIED){memcpy((void*)((uintptr_t)r->actor+0x1a0),coupled,64);applied=1;}
 }else if(slot->generation==generation){hcl_invalidate(&slot->state);hllq_forget(&slot->late);slot->state.owner=(uintptr_t)r->actor;slot->generation=++hcl_generation;}
 slot->busy=0;ReleaseSRWLockExclusive(&hcl_lock);
 float audit[24]={(float)status,(float)applied,(float)state.credit_us,(float)state.ticks,(float)state.resets,(float)generation,(float)unchanged,(float)state.valid};memcpy(audit+8,state.canonical,64);
 camera_accuracy_log(11,(void*)(uintptr_t)r->actor,(uintptr_t)r->vehicle,r->frame,r->input.dt[0],audit,24);
 float timing[24]={(float)clock.domain,(float)clock.valid,(float)status,(float)applied,(float)state.primed,(float)state.observed_logical,(float)state.adapted_logical,(float)state.valid};
 memcpy(timing+8,&clock.time_us,8);memcpy(timing+10,&state.canonical_us,8);memcpy(timing+12,&clock.dt_us,8);memcpy(timing+14,&clock.clock,8);memcpy(timing+16,&clock.frame,4);memcpy(timing+17,&state.time_us,8);memcpy(timing+19,&state.credit_us,8);
 camera_accuracy_log(35,(void*)(uintptr_t)r->actor,(uintptr_t)r->vehicle,r->frame,r->input.dt[0],timing,24);
 return applied;
}
typedef uintptr_t(ABI*HCLReset)(void*);
static HCLReset hcl_reset_original;
static ABI uintptr_t hcl_reset(void*self){htp_reset_begin((uintptr_t)self);hcl_forget((uintptr_t)self);uintptr_t result=hcl_reset_original(self);hcl_forget((uintptr_t)self);htp_reset_end();return result;}
static int hcl_install(void){
 const char*v=camera_fix_getenv("BB_CAMERA_HEAD_REFERENCE");if(!v||atoi(v)!=1)return 0;
 static const unsigned char reset[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x89,0xfb};
 if(runtime_image_size<0xb91210+sizeof reset||memcmp(image+0xb91210,reset,sizeof reset)||!hcc_native_init(&hcl_kernel,image,runtime_image_size))return 0;
 hcl_reset_original=(HCLReset)camera_rate_hook(0xb91210,reset,sizeof reset,(uintptr_t)hcl_reset);
 if(!hcl_reset_original){hcc_native_destroy(&hcl_kernel);return 0;}
 wchar_t path[4096];DWORD n=GetEnvironmentVariableW(L"BB_CAMERA_HEAD_REFERENCE_TAPE",path,4096);
 if(n>3&&n<4096&&(path[0]=='D'||path[0]=='d')&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')){
  /* Reuse the validated HKT mapping ABI for actual PRIVATE original-code calls. */
  HKTHeader*save=hkt_header;HKTRecord*records=hkt_records;
  if(hkt_mapping(path)){hcl_tape_header=hkt_header;hcl_tape_records=hkt_records;}
  hkt_header=save;hkt_records=records;
 }
 hcl_enabled=1;const char*lateflag=camera_fix_getenv("BB_CAMERA_HEAD_LATE_Q");hllq_enabled=lateflag&&!strcmp(lateflag,"1");if(hllq_enabled)puts("HeadLateQ: exact private LSQ linear endpoints; native33333 coupled64 + explicit logical ZOH; selected paused clock requires separate admission");puts("HeadReference: LIVE native33333us coupled body/head kernel; sampled velocity/SO3 inputs; native30 unchanged");return 1;
}
