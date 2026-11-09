/* Local, optional, read-only physics publication observer. Not a runtime fix.
 * Include after camera_pose_read/camera_rate_hook are declared. Installation
 * requires BB_PHYSICS_SOURCE_TAPE=D:\...; absent/invalid path installs no hooks.
 * No additional native queries, simulation calls, clock or source writes. */
typedef struct {
    char magic[8]; uint32_t version,record_size,capacity,count,dropped,reserved;
    uint64_t next_call; unsigned char padding[24];
} PhysicsSourceTapeHeader;
typedef struct {
    uint64_t call_id,self,raw,state,carstate,caller,clock_before,clock_after;
    uint64_t wall_before_us,wall_after_us;
    uint32_t kind,thread,flags,frame_before,frame_after,mxcsr;
    float dt; int32_t count;
    unsigned char owner_prefix[0x20],orientation[0x40];
    unsigned char clock_before_bytes[0x240],clock_after_bytes[0x240];
    unsigned char state_bytes[0x280],carstate_bytes[0x180];
} PhysicsSourceTapeRecord;
_Static_assert(sizeof(PhysicsSourceTapeHeader)==64,"physics source tape header");
_Static_assert(sizeof(PhysicsSourceTapeRecord)==2384,"physics source tape record");
static PhysicsSourceTapeHeader *physics_source_header;
static PhysicsSourceTapeRecord *physics_source_records;
static SRWLOCK physics_source_lock=SRWLOCK_INIT;
static volatile LONG64 physics_source_call_id;
typedef uintptr_t (ABI *PhysicsSourcePublish)(void *,float);
typedef uintptr_t (ABI *PhysicsSourceSubsteps)(void *,int,void *,float);
static PhysicsSourcePublish physics_source_publish_original;
static PhysicsSourceSubsteps physics_source_substeps_original;
static uint64_t physics_source_wall_us(void){
    LARGE_INTEGER counter,frequency; QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart ? (uint64_t)((counter.QuadPart/(uint64_t)frequency.QuadPart)*1000000+
        (counter.QuadPart%(uint64_t)frequency.QuadPart)*1000000/(uint64_t)frequency.QuadPart):0;
}
static int physics_source_clock(uint64_t *clock,unsigned char *bytes,uint32_t *frame){
    uintptr_t a=0;
    if(!camera_pose_read((uintptr_t)image+0x1911100,&a,8)||
       !camera_pose_read(a+0x968,&a,8)||!camera_pose_read(a+0x1e0,&a,8)||
       !camera_pose_read(a,bytes,0x240))return 0;
    *clock=a;memcpy(frame,bytes+0x21c,4);return 1;
}
static void physics_source_begin(PhysicsSourceTapeRecord *r,unsigned kind,void *self,float dt,int count,void *orientation,uintptr_t caller){
    memset(r,0,sizeof *r);r->call_id=(uint64_t)InterlockedIncrement64(&physics_source_call_id);
    r->kind=kind;r->thread=GetCurrentThreadId();r->self=(uintptr_t)self;r->caller=caller;
    r->dt=dt;r->count=count;__asm__ volatile("stmxcsr %0":"=m"(r->mxcsr));
    if(physics_source_clock(&r->clock_before,r->clock_before_bytes,&r->frame_before))r->flags|=1;
    if(kind==1){
        if(camera_pose_read((uintptr_t)self,r->owner_prefix,sizeof r->owner_prefix)){
            memcpy(&r->raw,r->owner_prefix+8,8);r->flags|=2;
        }
    }else{
        r->raw=(uintptr_t)self;
        if(orientation&&camera_pose_read((uintptr_t)orientation,r->orientation,sizeof r->orientation))r->flags|=4;
    }
    if(r->raw){r->state=r->raw+0x2ad0;r->carstate=r->raw+0x2eb0;}
    r->wall_before_us=physics_source_wall_us();
    /* Every observer's temporary FP environment is restored before native code. */
    __asm__ volatile("ldmxcsr %0"::"m"(r->mxcsr));
}
static void physics_source_end(PhysicsSourceTapeRecord *r){
    uint32_t after_mxcsr;__asm__ volatile("stmxcsr %0":"=m"(after_mxcsr));
    r->wall_after_us=physics_source_wall_us();
    if(physics_source_clock(&r->clock_after,r->clock_after_bytes,&r->frame_after))r->flags|=8;
    if(r->state&&camera_pose_read(r->state,r->state_bytes,sizeof r->state_bytes))r->flags|=16;
    if(r->carstate&&camera_pose_read(r->carstate,r->carstate_bytes,sizeof r->carstate_bytes))r->flags|=32;
    AcquireSRWLockExclusive(&physics_source_lock);
    uint32_t at=physics_source_header->count;
    if(at<physics_source_header->capacity){physics_source_records[at]=*r;MemoryBarrier();physics_source_header->count=at+1;}
    else ++physics_source_header->dropped;
    physics_source_header->next_call=(uint64_t)physics_source_call_id;
    ReleaseSRWLockExclusive(&physics_source_lock);
    __asm__ volatile("ldmxcsr %0"::"m"(after_mxcsr));
}
static ABI uintptr_t physics_source_publish_trace(void *self,float dt){
    PhysicsSourceTapeRecord r;uintptr_t caller=(uintptr_t)__builtin_return_address(0)-(uintptr_t)image;
    physics_source_begin(&r,1,self,dt,0,NULL,caller);
    uintptr_t result=physics_source_publish_original(self,dt);
    physics_source_end(&r);return result;
}
static ABI uintptr_t physics_source_substeps_trace(void *raw,int count,void *orientation,float dt){
    PhysicsSourceTapeRecord r;uintptr_t caller=(uintptr_t)__builtin_return_address(0)-(uintptr_t)image;
    physics_source_begin(&r,2,raw,dt,count,orientation,caller);
    uintptr_t result=physics_source_substeps_original(raw,count,orientation,dt);
    physics_source_end(&r);return result;
}
static __attribute__((unused)) void physics_source_tape_install(void){
    if(physics_source_header)return;
    wchar_t path[4096];DWORD len=GetEnvironmentVariableW(L"BB_PHYSICS_SOURCE_TAPE",path,4096);
    if(!len||len>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&path[2]=='\\'))return;
    static const unsigned char publication[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xec,0x38,0xc5,0xfa,0x11,0x45,0xbc};
    static const unsigned char substeps[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x78};
    if(runtime_image_size<0x7cb150+sizeof publication||
       memcmp(image+0x7cb150,publication,sizeof publication)||memcmp(image+0x7c7dd0,substeps,sizeof substeps)){
        puts("PhysicsSourceTape: unsupported native signatures; no hooks installed");return;
    }
    const uint32_t capacity=65536;uint64_t size=sizeof(PhysicsSourceTapeHeader)+(uint64_t)capacity*sizeof(PhysicsSourceTapeRecord);
    HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    HANDLE map=CreateFileMappingW(file,NULL,PAGE_READWRITE,(DWORD)(size>>32),(DWORD)size,NULL);CloseHandle(file);if(!map)return;
    void *mem=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,(SIZE_T)size);CloseHandle(map);if(!mem)return;
    physics_source_header=mem;memset(mem,0,sizeof(PhysicsSourceTapeHeader));memcpy(physics_source_header->magic,"PHYSRC01",8);
    physics_source_header->version=1;physics_source_header->record_size=sizeof(PhysicsSourceTapeRecord);physics_source_header->capacity=capacity;
    physics_source_records=(PhysicsSourceTapeRecord *)(physics_source_header+1);
    physics_source_publish_original=(PhysicsSourcePublish)camera_rate_hook(0x7cb150,publication,sizeof publication,(uintptr_t)physics_source_publish_trace);
    physics_source_substeps_original=(PhysicsSourceSubsteps)camera_rate_hook(0x7c7dd0,substeps,sizeof substeps,(uintptr_t)physics_source_substeps_trace);
    printf("PhysicsSourceTape: diagnostic-only native publication=%s substeps=%s record=%zu capacity=%u\n",
        physics_source_publish_original?"on":"unavailable",physics_source_substeps_original?"on":"unavailable",sizeof(PhysicsSourceTapeRecord),capacity);
}
