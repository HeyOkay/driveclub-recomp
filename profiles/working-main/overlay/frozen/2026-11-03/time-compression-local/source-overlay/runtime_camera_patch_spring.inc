/* PRIVATE opt-in two-chase-spring native30 candidate. Original authored settings,
 * native substeps/clamps, raw world velocity, rigid SO(3) orientation resampling,
 * then original projection/sign gain at each canonical time. No strength blend.
 * BB_CAMERA_REFERENCE_SPRINGS=1 opts in. Default and unsupported targets use
 * native passthrough. Type1/direct point-velocity targets only. Native observation
 * anchors before first force; absent that, raw history warms without prehistory.
 * Piecewise interpolation is
 * an explicit input reconstruction assumption, not proof of unobserved impacts.
 * Does not modify physics or double-adapt the successful CE3540 noise step.
 */
#include "runtime_camera_spring_reference_core.inc"
#include "shared-phase-clock-runtime.inc"
#include "runtime_camera_spring_raw_input.inc"
static _Thread_local void *camera_dynamics_self;
typedef float (ABI *CameraReferenceForce)(void *,float,float);
typedef float (ABI *CameraDynamicsSpring)(void *,float);
typedef void (ABI *CameraReferenceConstructor)(void *,void *);
typedef void (ABI *CameraReferenceReset)(void *,float);
static CameraReferenceForce camera_reference_force_original;
static CameraDynamicsSpring camera_dynamics_spring_original;
static CameraReferenceConstructor camera_reference_constructor_original;
static CameraReferenceReset camera_reference_reset_original;
typedef void (ABI *CameraReferenceDestructor)(void*);
static CameraReferenceDestructor camera_reference_delete_original,camera_reference_destruct_original;
static int camera_reference_spring_enabled,camera_reference_trace_enabled;
static CRSStore camera_reference_store;
static SRWLOCK camera_reference_store_lock=SRWLOCK_INIT;
static uint64_t camera_reference_revisions[CRS_STATE_CAPACITY];
static uint64_t camera_reference_generations[CRS_STATE_CAPACITY];
static unsigned camera_reference_busy[CRS_STATE_CAPACITY];
static uint64_t camera_reference_next_generation;
typedef struct {uintptr_t owner;unsigned depth;} CameraReferenceLifetime;
static CameraReferenceLifetime camera_reference_lifetimes[CRS_STATE_CAPACITY];
static unsigned camera_reference_untracked_lifetimes;
typedef struct {
    void *self,*camera;float dt,force,x,v,post_v;uint32_t frame;
    unsigned char before_force[0x2c];CRRHistory raw;unsigned axis;int raw_ready,ready;
} CameraReferenceInput;
static _Thread_local CameraReferenceInput camera_reference_input;
#include "runtime_camera_spring_tape.inc"
#include "runtime_camera_helmet_reference.inc"
enum {CRS_NATIVE=0,CRS_APPLIED=1,CRS_UNSUPPORTED=2,CRS_INPUT_MISMATCH=3,
      CRS_EXHAUSTED=4,CRS_COLLISION=5,CRS_STALE=6,CRS_UNREADABLE=7};

static void camera_reference_invalidate_locked(uintptr_t owner) {
    for(unsigned i=0;i<CRS_STATE_CAPACITY;++i)if(camera_reference_store.slots[i].owner==owner) {
        ++camera_reference_revisions[i];camera_reference_generations[i]=0;camera_reference_busy[i]=0;
    }
    crs_invalidate_owner(&camera_reference_store,owner);
}
static int camera_reference_lifetime_busy_locked(uintptr_t owner){
    if(camera_reference_untracked_lifetimes)return 1;
    for(unsigned i=0;i<CRS_STATE_CAPACITY;++i)if(camera_reference_lifetimes[i].owner==owner)return 1;
    return 0;
}
static void camera_reference_lifetime_begin(uintptr_t owner){
    AcquireSRWLockExclusive(&camera_reference_store_lock);camera_reference_invalidate_locked(owner);
    CameraReferenceLifetime*slot=NULL;for(unsigned i=0;i<CRS_STATE_CAPACITY;++i)if(camera_reference_lifetimes[i].owner==owner){slot=camera_reference_lifetimes+i;break;}
    if(!slot)for(unsigned i=0;i<CRS_STATE_CAPACITY;++i)if(!camera_reference_lifetimes[i].owner){slot=camera_reference_lifetimes+i;break;}
    if(slot){slot->owner=owner;++slot->depth;}else ++camera_reference_untracked_lifetimes;
    ReleaseSRWLockExclusive(&camera_reference_store_lock);crr_invalidate(owner);
}
static void camera_reference_lifetime_end(uintptr_t owner){
    AcquireSRWLockExclusive(&camera_reference_store_lock);camera_reference_invalidate_locked(owner);
    int found=0;for(unsigned i=0;i<CRS_STATE_CAPACITY;++i)if(camera_reference_lifetimes[i].owner==owner){found=1;if(!--camera_reference_lifetimes[i].depth)memset(camera_reference_lifetimes+i,0,sizeof camera_reference_lifetimes[i]);break;}
    if(!found&&camera_reference_untracked_lifetimes)--camera_reference_untracked_lifetimes;
    ReleaseSRWLockExclusive(&camera_reference_store_lock);crr_invalidate(owner);
}
static void camera_reference_invalidate(uintptr_t owner) {
    AcquireSRWLockExclusive(&camera_reference_store_lock);
    camera_reference_invalidate_locked(owner);
    ReleaseSRWLockExclusive(&camera_reference_store_lock);
    crr_invalidate(owner);
}
static __attribute__((unused)) void camera_reference_target_discontinuity(uintptr_t target) {
    /* Called by the target-history observer on native reset/rebind/frame gap.
       CCD600 clears its reset byte internally; reading it later at force time
       cannot detect that lifecycle. No lock is held across nested invalidation. */
    uintptr_t owners[CRR_CAPACITY];unsigned count=0;
    AcquireSRWLockShared(&crr_lock);
    for(unsigned i=0;i<CRR_CAPACITY;++i)if(crr_histories[i].owner&&crr_histories[i].target==target)owners[count++]=crr_histories[i].owner;
    ReleaseSRWLockShared(&crr_lock);
    for(unsigned i=0;i<count;++i)camera_reference_invalidate(owners[i]);
    AcquireSRWLockExclusive(&crr_lock);crr_forget_native_observation(target);ReleaseSRWLockExclusive(&crr_lock);
}
static ABI void camera_reference_constructor(void *self,void *configuration) {
    /* Invalidate both before/after native lifetime transitions. No native callback
       executes under the store lock. The caller does not use constructor RAX. */
    camera_reference_lifetime_begin((uintptr_t)self);
    camera_reference_constructor_original(self,configuration);
    camera_reference_lifetime_end((uintptr_t)self);
}
static ABI void camera_reference_reset(void *self,float dt) {
    camera_reference_lifetime_begin((uintptr_t)self);
    camera_reference_reset_original(self,dt);
    camera_reference_lifetime_end((uintptr_t)self);
}
static int camera_reference_force_site(uintptr_t caller,void *self) {
    uintptr_t camera=(uintptr_t)camera_dynamics_self;
    return camera && ((caller==0xd1c3dd && (uintptr_t)self==camera+0x538) ||
                      (caller==0xd1c42b && (uintptr_t)self==camera+0x50c));
}
static int camera_reference_spring_site(uintptr_t caller,void *self) {
    uintptr_t camera=(uintptr_t)camera_dynamics_self;
    return camera && ((caller==0xd1c3ed && (uintptr_t)self==camera+0x538) ||
                      (caller==0xd1c43b && (uintptr_t)self==camera+0x50c));
}
static float camera_reference_force_at(void *self,float force,float dt,uintptr_t caller) {
    int helmet_axis=chr_site(self,caller,1);
    if(helmet_axis>=0)return camera_helmet_force_at(self,force,dt,(unsigned)helmet_axis);
    CameraReferenceInput input={0};camera_reference_input.ready=0;
    if((camera_reference_spring_enabled||camera_reference_trace_enabled) && camera_reference_force_site(caller,self)) {
        input.self=self;input.camera=camera_dynamics_self;input.dt=dt;input.force=force;
        input.ready=camera_pose_read((uintptr_t)self,input.before_force,sizeof input.before_force) &&
                    camera_pose_read((uintptr_t)input.camera+0xe8,&input.frame,sizeof input.frame);
        if(input.ready){
            input.x=crs_float(input.before_force,0x14);input.v=crs_float(input.before_force,0x10);
            input.axis=caller==0xd1c3dd?0:1;
            input.raw_ready=crr_capture(input.camera,input.frame,dt,input.axis,force,&input.raw);
        }
    }
    float result=camera_reference_force_original(self,force,dt);
    if(input.ready)input.ready=camera_pose_read((uintptr_t)self+0x10,&input.post_v,4);
    camera_reference_input=input;
    return result;
}
static ABI float camera_reference_force(void *self,float force,float dt) {
    return camera_reference_force_at(self,force,dt,(uintptr_t)__builtin_return_address(0)-runtime_image_base);
}

/* Work on a local state snapshot. Native force/spring callbacks are scalar,
 * inactive-impulse operations on a local aligned record, outside every lock.
 * A lifecycle transition or concurrent same-object update invalidates commit. */
static int camera_reference_compute(void *self,const CameraReferenceInput *input,
 const unsigned char *before,CRSState native_after,float dt,CRSOutput *out,uint64_t *generation) {
    if(!crs_supported(before)) {camera_reference_invalidate((uintptr_t)input->camera);return CRS_UNSUPPORTED;}
    /* Native30 (and slower) remains byte-for-byte native. A fresh raw window is
       required before high-rate adaptation; no invented prehistory. */
    if(!input->raw_ready || (!input->raw.origin_known&&(input->raw.count<2||input->raw.time-input->raw.points[0].t<(double)crs_T))) {
        AcquireSRWLockExclusive(&camera_reference_store_lock);
        camera_reference_invalidate_locked((uintptr_t)input->camera);
        ReleaseSRWLockExclusive(&camera_reference_store_lock);return CRS_NATIVE;
    }
    CRSAdapter working;uint64_t revision,gen;unsigned index;
    AcquireSRWLockExclusive(&camera_reference_store_lock);
    if(camera_reference_lifetime_busy_locked((uintptr_t)input->camera)){ReleaseSRWLockExclusive(&camera_reference_store_lock);return CRS_NATIVE;}
    CRSAdapter *state=crs_lookup(&camera_reference_store,(uintptr_t)input->camera,(uintptr_t)self);
    if(!state){ReleaseSRWLockExclusive(&camera_reference_store_lock);return CRS_EXHAUSTED;}
    /* A native-rate epoch starts natively. A Q-sized step occurring inside an
       already active VRR epoch must retain its existing fractional phase. */
    if(dt==crs_T&&!state->initialized){ReleaseSRWLockExclusive(&camera_reference_store_lock);return CRS_NATIVE;}
    index=(unsigned)(state-camera_reference_store.slots);
    if(camera_reference_busy[index]) {
        state->initialized=0;camera_reference_busy[index]=0;++camera_reference_revisions[index];
        ReleaseSRWLockExclusive(&camera_reference_store_lock);return CRS_COLLISION;
    }
    if(!camera_reference_generations[index])camera_reference_generations[index]=++camera_reference_next_generation;
    gen=camera_reference_generations[index];revision=++camera_reference_revisions[index];
    camera_reference_busy[index]=1;working=*state;
    ReleaseSRWLockExclusive(&camera_reference_store_lock);
    CRRForceContext provider={&input->raw,input->axis,input->force};
    HCSClock clock=hcs_read_clock(input->frame,dt);if(clock.time_us!=input->raw.global_time_us||clock.clock!=input->raw.global_clock)clock.valid=0;
    int ok=crs_render_global(&working,gen,input->frame,before,(CRSState){input->x,input->v},dt,input->force,
                     (CRSOps){camera_reference_force_original,camera_dynamics_spring_original},out,0,
                     crr_force_global,&provider,input->raw.epoch,&clock);
    AcquireSRWLockExclusive(&camera_reference_store_lock);
    state=&camera_reference_store.slots[index];
    if(camera_reference_revisions[index]!=revision || !camera_reference_busy[index] ||
       state->owner!=(uintptr_t)input->camera || state->spring!=(uintptr_t)self) {
        ReleaseSRWLockExclusive(&camera_reference_store_lock);return CRS_STALE;
    }
    camera_reference_busy[index]=0;++camera_reference_revisions[index];
    CRSState current;uint32_t frame=0;
    if(!camera_pose_read((uintptr_t)self+0x10,&current.v,4) ||
       !camera_pose_read((uintptr_t)self+0x14,&current.x,4) ||
       !camera_pose_read((uintptr_t)input->camera+0xe8,&frame,sizeof frame) ||
       frame!=input->frame || !crs_same(current,native_after))ok=0;
    uintptr_t begin=0,target=0,binding=0;
    if(!camera_pose_read((uintptr_t)input->camera+0x148,&begin,8)||!camera_pose_read(begin,&target,8)||
       target!=input->raw.target||!camera_pose_read(target+0x28,&binding,8)||binding!=input->raw.binding)ok=0;
    if(ok) {
        *state=working;*generation=gen;
        memcpy((unsigned char *)self+0x10,&out->display.stored.v,4);
        memcpy((unsigned char *)self+0x14,&out->display.stored.x,4);
    } else state->initialized=0;
    ReleaseSRWLockExclusive(&camera_reference_store_lock);
    return ok?CRS_APPLIED:CRS_UNSUPPORTED;
}

static SRWLOCK camera_reference_trace_lock=SRWLOCK_INIT;
static FILE *camera_reference_trace_file;
static unsigned camera_reference_trace_rows;
static size_t camera_reference_trace_bytes;
static int camera_reference_trace_finished;
static wchar_t camera_reference_trace_path[4096];
static int camera_reference_trace_gate(void) {
    static int initialized,started;static wchar_t gate[4096];
    if(!initialized){DWORD n=GetEnvironmentVariableW(L"BB_CAMERA_TRACE_START",gate,4096);initialized=1;if(!n)started=1;else if(n>=4096)gate[0]=0;}
    if(!started&&gate[0]){DWORD a=GetFileAttributesW(gate);if(a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY))started=1;}
    return started;
}
static void camera_reference_trace_after(void *self,uintptr_t caller,float dt,
 const unsigned char *before,const CameraReferenceInput *input,CRSState native_after,
 float native_return,int status,const CRSOutput *out,uint64_t generation) {
    if(!camera_reference_trace_enabled)return;
    AcquireSRWLockExclusive(&camera_reference_trace_lock);
    if(camera_reference_trace_finished||!camera_reference_trace_gate()) {
        ReleaseSRWLockExclusive(&camera_reference_trace_lock);return;
    }
    if(!camera_reference_trace_file) {
        camera_reference_trace_file=_wfopen(camera_reference_trace_path,L"wb");
        if(!camera_reference_trace_file){camera_reference_trace_finished=1;ReleaseSRWLockExclusive(&camera_reference_trace_lock);return;}
        static char buffer[65536];setvbuf(camera_reference_trace_file,buffer,_IOFBF,sizeof buffer);
        const char *header="row,wall_ms,thread,owner,spring,frame,caller,status,adapter_enabled,dt,force_dt,raw_force,before_force_x,before_force_v,post_force_v,native_x,native_v,native_return,display_x,display_v,display_return,canonical_x,canonical_v,canonical_ticks,time,partial,reset,duplicate,generation,frequency,damping,limit,max_steps,stiffness,max_step,s20,s24,s28,global_time_us,global_canonical_us,global_primed\n";
        camera_reference_trace_bytes=fwrite(header,1,strlen(header),camera_reference_trace_file);
    }
    /* Fixed limits apply only to explicitly enabled diagnostic writes. */
    if(camera_reference_trace_rows>=150000 || camera_reference_trace_bytes>=64u*1024u*1024u) {
        fclose(camera_reference_trace_file);camera_reference_trace_file=NULL;camera_reference_trace_finished=1;
        ReleaseSRWLockExclusive(&camera_reference_trace_lock);return;
    }
    CRSOutput fallback={0};fallback.display=(CRSSample){native_after,native_return};
    fallback.canonical=(CRSState){NAN,NAN};if(!out)out=&fallback;
    char row[4096];
    int n=snprintf(row,sizeof row,
      "%u,%llu,%lu,0x%llx,0x%llx,%u,0x%llx,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%u,%.17g,%d,%d,%d,%llu,%.9g,%.9g,%.9g,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%lld,%lld,%u\n",
      ++camera_reference_trace_rows,(unsigned long long)GetTickCount64(),(unsigned long)GetCurrentThreadId(),
      (unsigned long long)(uintptr_t)input->camera,(unsigned long long)(uintptr_t)self,input->frame,
      (unsigned long long)caller,status,camera_reference_spring_enabled,(double)dt,(double)input->dt,
      (double)input->force,(double)input->x,(double)input->v,(double)input->post_v,
      (double)native_after.x,(double)native_after.v,(double)native_return,
      (double)out->display.stored.x,(double)out->display.stored.v,(double)out->display.returned,
      (double)out->canonical.x,(double)out->canonical.v,out->ticks,out->time,out->partial,out->reset,out->duplicate,
      (unsigned long long)generation,(double)crs_float(before,0),(double)crs_float(before,4),(double)crs_float(before,8),
      crs_uint(before,0xc),(double)crs_float(before,0x18),(double)crs_float(before,0x1c),
      (double)crs_float(before,0x20),(double)crs_float(before,0x24),(double)crs_float(before,0x28),(long long)out->global_time_us,(long long)out->global_canonical_us,out->global_primed);
    if(n>0&&(size_t)n<sizeof row) {
        if(camera_reference_trace_bytes+(size_t)n>64u*1024u*1024u) {
            fclose(camera_reference_trace_file);camera_reference_trace_file=NULL;camera_reference_trace_finished=1;
        } else {
            size_t written=fwrite(row,1,(size_t)n,camera_reference_trace_file);camera_reference_trace_bytes+=written;
            if(written!=(size_t)n){fclose(camera_reference_trace_file);camera_reference_trace_file=NULL;camera_reference_trace_finished=1;}
            else if((camera_reference_trace_rows%64)==0)fflush(camera_reference_trace_file);
        }
    }
    ReleaseSRWLockExclusive(&camera_reference_trace_lock);
}
static float camera_dynamics_spring_at(void *self,float dt,uintptr_t caller) {
    int helmet_axis=chr_site(self,caller,0);
    if(helmet_axis>=0)return camera_helmet_step_at(self,dt,(unsigned)helmet_axis);
    if(!camera_dynamics_self || !camera_reference_spring_site(caller,self))return camera_dynamics_spring_original(self,dt);
    unsigned char before[0x2c];int valid=camera_pose_read((uintptr_t)self,before,sizeof before);
    CameraReferenceInput input=camera_reference_input;camera_reference_input.ready=0;
    /* Observe after the actual native call; this dt is already normalized by the
       original chase method. Never substitute camera_rate_dt from outer TLS. */
    float result=camera_dynamics_spring_original(self,dt);CRSState after={NAN,NAN};
    valid=valid&&camera_pose_read((uintptr_t)self+0x10,&after.v,4)&&camera_pose_read((uintptr_t)self+0x14,&after.x,4);
    if(valid&&input.ready){unsigned char tape_after[44];if(camera_pose_read((uintptr_t)self,tape_after,44))
        camera_spring_tape_record(1,self,input.camera,input.frame,input.axis,input.force,dt,result,input.before_force,before,tape_after,input.raw.target,input.raw.binding);}
    uint32_t frame=0;int matches=input.ready&&input.self==self&&input.camera==camera_dynamics_self&&
      !memcmp(&input.dt,&dt,4)&&valid&&input.x==crs_float(before,0x14)&&input.post_v==crs_float(before,0x10)&&
      crs_settings_same(input.before_force,before)&&camera_pose_read((uintptr_t)input.camera+0xe8,&frame,sizeof frame)&&frame==input.frame;
    CRSOutput output;uint64_t generation=0;int status=valid?CRS_NATIVE:CRS_UNREADABLE;
    if(camera_reference_spring_enabled) {
        if(matches)status=camera_reference_compute(self,&input,before,after,dt,&output,&generation);
        else {camera_reference_invalidate((uintptr_t)camera_dynamics_self);status=CRS_INPUT_MISMATCH;}
    }
    if(valid)camera_reference_trace_after(self,caller,dt,before,&input,after,result,status,status==CRS_APPLIED?&output:NULL,generation);
    return status==CRS_APPLIED?output.display.returned:result;
}
static ABI float camera_dynamics_spring(void *self,float dt) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-runtime_image_base;
    float audit[24]={0};camera_pose_read((uintptr_t)self,audit,44);
    float final=camera_dynamics_spring_at(self,dt,caller);camera_pose_read((uintptr_t)self,audit+11,44);audit[22]=audit[23]=final;
    camera_accuracy_log(4,self,(uintptr_t)camera_dynamics_self,caller,dt,audit,24);return final;
}
static void *camera_dynamics_select(void *self) {
    uintptr_t type=0,update=0;
    if(!camera_pose_read((uintptr_t)self,&type,8)||!camera_pose_read(type+0x18,&update,8)||update!=runtime_image_base+0xd1b7c0)return NULL;
    return self;
}
static ABI void camera_reference_delete(void*self){camera_reference_lifetime_begin((uintptr_t)self);camera_reference_delete_original(self);camera_reference_lifetime_end((uintptr_t)self);}
static ABI void camera_reference_destruct(void*self){camera_reference_lifetime_begin((uintptr_t)self);camera_reference_destruct_original(self);camera_reference_lifetime_end((uintptr_t)self);}
static void*camera_reference_destructor_hook(size_t offset,const unsigned char*expected,uintptr_t target){
    /* These entries have only10 non-RIP bytes. Relocate their following LEA to
       MOVABS, then jump with R11 so the computed RAX vtable address is retained. */
    unsigned char*code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!code)return NULL;
    memcpy(code,expected,10);code[10]=0x48;code[11]=0xb8;uintptr_t table=runtime_image_base+0x1654ee0;memcpy(code+12,&table,8);
    code[20]=0x49;code[21]=0xbb;uintptr_t resume=runtime_image_base+offset+17;memcpy(code+22,&resume,8);memcpy(code+30,"\x41\xff\xe3",3);
    DWORD old;if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return NULL;}
    FlushInstructionCache(GetCurrentProcess(),code,33);
    memcpy(image+offset,"\x48\xb8",2);memcpy(image+offset+2,&target,8);memcpy(image+offset+10,"\xff\xe0",2);memset(image+offset+12,0x90,5);
    FlushInstructionCache(GetCurrentProcess(),image+offset,17);return code;
}
static void camera_dynamics_install(void) {
    const char *enabled=camera_fix_getenv("BB_CAMERA_REFERENCE_SPRINGS");int requested=enabled&&atoi(enabled)!=0;
    DWORD n=GetEnvironmentVariableW(L"BB_CAMERA_SPRING_TRACE",camera_reference_trace_path,4096);
    camera_reference_trace_enabled=n>3&&n<4096&&
       (camera_reference_trace_path[0]==L'D'||camera_reference_trace_path[0]==L'd')&&
       camera_reference_trace_path[1]==L':'&&(camera_reference_trace_path[2]==L'\\'||camera_reference_trace_path[2]==L'/');
    if(!requested&&!camera_reference_trace_enabled)return;
    static const unsigned char step[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50};
    static const unsigned char force[]={0xc5,0xfa,0x59,0xc1,0xc5,0xfa,0x58,0x47,0x10,0xc5,0xfa,0x11,0x47,0x10};
    static const unsigned char ctor[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x49,0x89,0xf6};
    static const unsigned char reset[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x40};
    static const unsigned char deleting[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x89,0xfb,0x48,0x8d,0x05,0x5f,0x98,0x93,0x00};
    static const unsigned char destructing[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x89,0xfb,0x48,0x8d,0x05,0xaf,0x97,0x93,0x00};
    if(runtime_image_size<0xd21050+sizeof reset || memcmp(image+0x6ce5b0,step,sizeof step)||
       memcmp(image+0x6ce740,force,sizeof force)||memcmp(image+0xd1aca0,ctor,sizeof ctor)||
       memcmp(image+0xd21050,reset,sizeof reset)||memcmp(image+0xd1b670,deleting,sizeof deleting)||
       memcmp(image+0xd1b720,destructing,sizeof destructing)) {puts("CameraReference: unsupported native chase/spring lifecycle signatures; native passthrough");return;}
    camera_reference_constructor_original=(CameraReferenceConstructor)camera_rate_hook(0xd1aca0,ctor,sizeof ctor,(uintptr_t)camera_reference_constructor);
    camera_reference_reset_original=(CameraReferenceReset)camera_rate_hook(0xd21050,reset,sizeof reset,(uintptr_t)camera_reference_reset);
    camera_reference_delete_original=(CameraReferenceDestructor)camera_reference_destructor_hook(0xd1b670,deleting,(uintptr_t)camera_reference_delete);
    camera_reference_destruct_original=(CameraReferenceDestructor)camera_reference_destructor_hook(0xd1b720,destructing,(uintptr_t)camera_reference_destruct);
    camera_dynamics_spring_original=(CameraDynamicsSpring)camera_rate_hook(0x6ce5b0,step,sizeof step,(uintptr_t)camera_dynamics_spring);
    if(camera_dynamics_spring_original)camera_reference_force_original=(CameraReferenceForce)camera_rate_hook(0x6ce740,force,sizeof force,(uintptr_t)camera_reference_force);
    camera_reference_spring_enabled=requested&&camera_reference_constructor_original&&camera_reference_reset_original&&camera_reference_delete_original&&camera_reference_destruct_original&&camera_dynamics_spring_original&&camera_reference_force_original;
    camera_helmet_reference_install(requested&&camera_dynamics_spring_original&&camera_reference_force_original);
    puts(camera_reference_spring_enabled?"CameraReference: opt-in two-chase-spring native30 candidate (raw velocity/orientation reconstruction)":"CameraReference: native spring passthrough; reference candidate off/unavailable");
}
