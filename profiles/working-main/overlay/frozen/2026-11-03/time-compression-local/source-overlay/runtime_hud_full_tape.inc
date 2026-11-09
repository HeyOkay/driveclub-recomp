static _Thread_local int hud_tape_private_input;
/* Local diagnostics only. No state/input/RNG writes to a live game.
 * BB_HUD_FULL_TAPE=D:\... opts into a bounded memory-mapped binary recording. */
#include <windows.h>
#include <stdint.h>
#include <string.h>
typedef struct {
    unsigned char magic[8];uint32_t version,record_size,capacity,count,dropped,reserved;
} HudTapeHeader;
typedef struct {
    uint64_t self,subject,observation,target,state,wall_us;
    uint32_t frame,thread;float dt;uint32_t kind,flags,reserved;
    float vectors[4][4];
    unsigned char before[0x84],after[0x84];
    uint32_t rng_entry[2],rng_math[2],rng_exit[2];
} HudTapeRecord;
_Static_assert(sizeof(HudTapeHeader)==32,"HUD tape header format");
_Static_assert(sizeof(HudTapeRecord)==424,"HUD tape record format");
typedef struct HudTapeScope {struct HudTapeScope*previous;int active;HudTapeRecord record;} HudTapeScope;
static _Thread_local HudTapeScope *hud_tape_current;
static INIT_ONCE hud_tape_once=INIT_ONCE_STATIC_INIT;
static SRWLOCK hud_tape_lock=SRWLOCK_INIT;
static HudTapeHeader *hud_tape_header;
static HudTapeRecord *hud_tape_records;
static BOOL CALLBACK hud_tape_init(PINIT_ONCE once,PVOID param,PVOID*context){
    (void)once;(void)param;(void)context;wchar_t path[4096];DWORD len=GetEnvironmentVariableW(L"BB_HUD_FULL_TAPE",path,4096);
    if(!len||len>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&path[2]=='\\'))return TRUE;
    const DWORD capacity=262144;uint64_t size=sizeof(HudTapeHeader)+(uint64_t)capacity*sizeof(HudTapeRecord);
    HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return TRUE;
    HANDLE map=CreateFileMappingW(file,NULL,PAGE_READWRITE,(DWORD)(size>>32),(DWORD)size,NULL);CloseHandle(file);if(!map)return TRUE;
    void*mem=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,(SIZE_T)size);CloseHandle(map);if(!mem)return TRUE;
    hud_tape_header=mem;memset(mem,0,sizeof(HudTapeHeader));memcpy(hud_tape_header->magic,"HUDTP001",8);
    hud_tape_header->version=2;hud_tape_header->record_size=sizeof(HudTapeRecord);hud_tape_header->capacity=capacity;
    hud_tape_records=(HudTapeRecord*)(hud_tape_header+1);
    printf("HudFullTape: enabled, record=%zu capacity=%lu (diagnostic only; no live RNG writes)\n",sizeof(HudTapeRecord),(unsigned long)capacity);
    return TRUE;
}
static unsigned hud_tape_frame(void){
    uintptr_t a=0;unsigned f=~0u;
    if(camera_pose_read((uintptr_t)image+0x1911100,&a,8)&&camera_pose_read(a+0x968,&a,8)&&camera_pose_read(a+0x1e0,&a,8))camera_pose_read(a+0x21c,&f,4);
    return f;
}
/* Flags:1 preHUD,2 entryRNG,4 all four selected vectors/kind,8 math-startRNG,
 *16 postHUD,32 exitRNG. Eligible complete records have flags63. */
static void hud_tape_begin(HudTapeScope*scope,void*self,void*subject,float dt){
    memset(scope,0,sizeof *scope);scope->previous=hud_tape_current;
    InitOnceExecuteOnce(&hud_tape_once,hud_tape_init,NULL,NULL);if(!hud_tape_header)return;
    scope->active=1;HudTapeRecord*r=&scope->record;r->self=(uintptr_t)self;r->subject=(uintptr_t)subject;
    r->dt=dt;r->wall_us=GetTickCount64()*1000;r->frame=hud_tape_frame();r->thread=GetCurrentThreadId();
    /* AVX/SSE denormal/rounding controls are part of the native arithmetic
     * contract. Store them in the formerly reserved32bit record word. */
    __asm__ volatile("stmxcsr %0":"=m"(r->reserved));
    if(camera_pose_read((uintptr_t)self+8,r->before,0x84))r->flags|=1;
    if(camera_pose_read((uintptr_t)image+0x191b2a0,r->rng_entry,8))r->flags|=2;
    hud_tape_current=scope;
}
static void hud_geometry_capture(void);
static __attribute__((unused)) void hud_tape_input_from_observation(void*observation,const float*selected_b0){
    HudTapeScope*s=hud_tape_current;if(!s||!s->active||(s->record.flags&4))return;
    HudTapeRecord*r=&s->record;uintptr_t p[3];r->observation=(uintptr_t)observation;
    if(camera_pose_read((uintptr_t)observation,p,24)){
        r->target=p[1];r->state=p[2];memcpy(r->vectors[0],selected_b0,16);
        if(camera_pose_read(p[0],&r->kind,4)&&camera_pose_read(p[1]+0x140,r->vectors[1],16)&&
           camera_pose_read(p[2]+0x90,r->vectors[2],16)&&camera_pose_read(p[2]+0x110,r->vectors[3],16))r->flags|=4;
    }
    if(!hud_tape_private_input)hud_geometry_capture();
    if(camera_pose_read((uintptr_t)image+0x191b2a0,r->rng_math,8))r->flags|=8;
}
/* Candidate integration can supply its already captured four real vectors,
 * avoiding additional real subject getters and retaining actual state identity. */
static __attribute__((unused)) void hud_tape_input_raw(uintptr_t observation,uintptr_t target,uintptr_t state,unsigned kind,const float vectors[4][4]){
    HudTapeScope*s=hud_tape_current;if(!s||!s->active||(s->record.flags&4))return;
    HudTapeRecord*r=&s->record;r->observation=observation;r->target=target;r->state=state;r->kind=kind;
    memcpy(r->vectors,vectors,sizeof r->vectors);r->flags|=4;
    if(!hud_tape_private_input)hud_geometry_capture();
    if(camera_pose_read((uintptr_t)image+0x191b2a0,r->rng_math,8))r->flags|=8;
}
static void hud_tape_end(HudTapeScope*scope){
    if(!scope->active)return;
    HudTapeRecord*r=&scope->record;
    if(camera_pose_read(r->self+8,r->after,0x84))r->flags|=16;
    if(camera_pose_read((uintptr_t)image+0x191b2a0,r->rng_exit,8))r->flags|=32;
    hud_tape_current=scope->previous;
    AcquireSRWLockExclusive(&hud_tape_lock);unsigned at=hud_tape_header->count;
    if(at<hud_tape_header->capacity){hud_tape_records[at]=*r;MemoryBarrier();hud_tape_header->count=at+1;}
    else ++hud_tape_header->dropped;
    ReleaseSRWLockExclusive(&hud_tape_lock);
}

#include "runtime_hud_geometry_tape_v3.inc"
