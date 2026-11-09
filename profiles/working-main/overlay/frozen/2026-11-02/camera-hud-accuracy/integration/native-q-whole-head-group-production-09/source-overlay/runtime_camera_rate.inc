typedef void (ABI *CameraRateUpdate)(void *, unsigned, void *, float, float);
typedef float (ABI *CameraRateImpulse)(void *, float);
typedef void (ABI *CameraNoiseUpdate)(void *, float);
static CameraRateUpdate camera_rate_update_original;
static CameraRateImpulse camera_rate_impulse_original;
static CameraNoiseUpdate camera_noise_update_original;
static _Thread_local float camera_rate_dt;
static _Thread_local unsigned camera_rate_active;
static _Thread_local unsigned camera_noise_fixed_step;
#include "runtime_camera_patch_spring.inc"
typedef struct { void *self; double credit; ULONGLONG last; } CameraNoiseClock;
static unsigned camera_noise_ticks(double *credit, float dt) {
    if (!(dt>0) || dt>0.25f) return 0;
    const double period=1.0/30.0;
    *credit+=dt;
    unsigned ticks=(unsigned)((*credit+0.000001)/period);
    *credit-=ticks*period;
    if (*credit<0) *credit=0;
    return ticks;
}
static int camera_noise_caller(uintptr_t caller) {
    /* Random mode of the low-frequency effect (including its tail call),
       and six continuous high-frequency noise axes. Event impulses bypass this. */
    return caller==0xce30b8 || caller==0xce30c6 || caller==0xce3d8d ||
           caller==0xce3852 || caller==0xce387d || caller==0xce38aa ||
           caller==0xce38d7 || caller==0xce3904 || caller==0xce3931;
}
static ABI void camera_rate_update(void *self, unsigned index, void *matrix, float dt, float real_dt) {
    float previous=camera_rate_dt;
    unsigned active=camera_rate_active;
    void *previous_dynamics=camera_dynamics_self;
    camera_dynamics_self=camera_dynamics_select(self);
    camera_rate_dt=dt; camera_rate_active=1;
    camera_rate_update_original(self,index,matrix,dt,real_dt);
    if(camera_accuracy_trace_active()){
    float camera_tape[24]={0};unsigned kind=0,frame=0;
    camera_pose_read((uintptr_t)self+0x10,camera_tape,64);camera_pose_read((uintptr_t)self+0x50,camera_tape+16,24);
    camera_pose_read((uintptr_t)self+0xc,&kind,4);camera_pose_read((uintptr_t)self+0xe8,&frame,4);
    camera_tape[22]=(float)kind;camera_tape[23]=real_dt;
    camera_accuracy_log(6,self,index,frame,dt,camera_tape,24);
    }
    camera_reference_input.ready=0;
    camera_rate_dt=previous; camera_rate_active=active;
    camera_dynamics_self=previous_dynamics;
}
static ABI float camera_rate_impulse(void *self, float magnitude) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-runtime_image_base;
    if (camera_noise_fixed_step || !camera_rate_active || !camera_noise_caller(caller))
        return camera_rate_impulse_original(self,magnitude);
    static _Thread_local CameraNoiseClock clocks[256];
    CameraNoiseClock *clock=NULL;
    for (unsigned i=0;i<256;++i) {
        if (!clocks[i].self) clocks[i].self=self;
        if (clocks[i].self==self) { clock=&clocks[i]; break; }
    }
    if (!clock) return camera_rate_impulse_original(self,magnitude);
    ULONGLONG now=GetTickCount64();
    if (now-clock->last>250) clock->credit=0;
    clock->last=now;
    unsigned ticks=camera_noise_ticks(&clock->credit,camera_rate_dt);
    float velocity; memcpy(&velocity,(unsigned char *)self+0x10,4);
    for (unsigned i=0;i<ticks;++i)
        velocity=camera_rate_impulse_original(self,magnitude);
    return velocity;
}
#include "runtime_camera_noise_reference.inc"
static void *camera_rate_hook(size_t offset, const unsigned char *expected, size_t count, uintptr_t target) {
    if (runtime_image_size<offset+count || memcmp(image+offset,expected,count)) return NULL;
    unsigned char *code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!code) return NULL;
    memcpy(code,expected,count);
    /* RAX is scratch at both selected entries, and neither copied span reads it.
       The 12-byte entry jump avoids relocating the impulse's following RIP load. */
    uintptr_t resume=(uintptr_t)image+offset+count;
    const unsigned char jump[2]={0x48,0xb8}, tail[2]={0xff,0xe0};
    memcpy(code+count,jump,2); memcpy(code+count+2,&resume,8); memcpy(code+count+10,tail,2);
    DWORD old;
    if (!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)) { VirtualFree(code,0,MEM_RELEASE); return NULL; }
    FlushInstructionCache(GetCurrentProcess(),code,4096);
    memcpy(image+offset,jump,2); memcpy(image+offset+2,&target,8); memcpy(image+offset+10,tail,2);
    memset(image+offset+12,0x90,count-12);
    FlushInstructionCache(GetCurrentProcess(),image+offset,count);
    return code;
}

/* Read-only replay cursor getter. The original MOV/RET executes unchanged in
 * the trampoline; only its object identity is retained by the diagnostic. */
typedef int64_t (ABI *CameraAccuracyProviderGet)(void *);
static CameraAccuracyProviderGet camera_accuracy_provider_get_original;
static volatile LONG64 camera_accuracy_replay_controller;
typedef uintptr_t (ABI *CameraAccuracyControllerUpdate)(void *,void *,void *,void *);
static CameraAccuracyControllerUpdate camera_accuracy_controller_original;
static ABI uintptr_t camera_accuracy_controller_update(void *self,void *input,void *a,void *b){
    uintptr_t vt=0;unsigned before=~0u,after=~0u;
    int valid=camera_pose_read((uintptr_t)self,&vt,8)&&vt==runtime_image_base+0x15b33d0;
    if(valid)camera_pose_read((uintptr_t)self+0xe8,&before,4);
    uintptr_t result=camera_accuracy_controller_original(self,input,a,b);
    if(valid&&camera_pose_read((uintptr_t)self+0xe8,&after,4)&&after<4){
        InterlockedExchange64(&camera_accuracy_replay_controller,(LONG64)(uintptr_t)self);
        float data[3]={(float)before,(float)after,1};camera_accuracy_log(7,self,vt,0x10be80,0,data,3);
    }
    return result;
}
static ABI int64_t camera_accuracy_provider_get(void *self){
    int64_t cursor=camera_accuracy_provider_get_original(self);
    /* This small getter is shared by many unrelated clock classes. Accept the
     * single verified replay-translation call, whose RDI comes from rbp-E0. */
    if((uintptr_t)__builtin_return_address(0)!=runtime_image_base+0x1239cc)return cursor;
    InterlockedExchange64(&camera_accuracy_provider,(LONG64)(uintptr_t)self);
    float data[9]={0};memcpy(data,&cursor,8);
    unsigned char flags[2]={0};camera_pose_read((uintptr_t)self+0xcc,flags,2);
    data[6]=flags[0];data[7]=flags[1];
    camera_accuracy_log(5,self,(uintptr_t)self,0x1c3c00,0,data,9);
    return cursor;
}
static void camera_accuracy_provider_install(void){
    static const unsigned char expected[]={0x48,0x8b,0x87,0xb0,0,0,0,0xc3,0x0f,0x1f,0x84,0,0,0,0,0};
    camera_accuracy_provider_get_original=(CameraAccuracyProviderGet)camera_rate_hook(
        0x1c3c00,expected,sizeof expected,(uintptr_t)camera_accuracy_provider_get);
    printf("NativeOracle: read-only replay cursor observer %s\n",camera_accuracy_provider_get_original?"on":"unavailable");
    static const unsigned char controller[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50};
    camera_accuracy_controller_original=(CameraAccuracyControllerUpdate)camera_rate_hook(
        0x10be80,controller,sizeof controller,(uintptr_t)camera_accuracy_controller_update);
    printf("NativeOracle: read-only replay group observer %s\n",camera_accuracy_controller_original?"on":"unavailable");
}

#include "runtime_physics_source_tape.inc"

#include "runtime_camera_head_angular_tape.inc"

#include "runtime_camera_head_kernel_tape.inc"
