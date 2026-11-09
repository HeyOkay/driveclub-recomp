/* Optional PRIVATE V3 geometry/clock diagnostic. Include after runtime_hud_full_tape.inc.
 * V2 remains unchanged. First1128bytes retain its geometry layout.
 * BB_HUD_GEOMETRY_TAPE=D:\... enables a separate bounded read-only-source tape.
 * Does not change full HUD v2 schema or any live state, inputs, RNG or clock. */

/* Clock snapshots bracket this geometry callback, which itself is AFTER the
 * four selected native HUD vectors were loaded. No getter calls or clock writes.
 * A native218!=21C phase is retained; it is not an invalid record. */
typedef struct {
    uint64_t root,service,clock;
    int64_t raw20,raw58,raw60;
    int64_t qpc_before,qpc_after,qpc_frequency;
    uint64_t tick_ms;
    uint32_t frame218,frame21c,flags,reserved;
} HudGeometryClockSnapshot;
_Static_assert(sizeof(HudGeometryClockSnapshot)==96,"HUD geometry clock snapshot layout");
static void hud_geometry_clock_capture(HudGeometryClockSnapshot *snapshot){
    uint32_t mxcsr;__asm__ volatile("stmxcsr %0":"=m"(mxcsr));
    memset(snapshot,0,sizeof *snapshot);
    LARGE_INTEGER before={0},after={0},frequency={0};
    BOOL q0=QueryPerformanceCounter(&before);
    snapshot->tick_ms=GetTickCount64();
    unsigned char clock_bytes[0x240];
    if(camera_pose_read((uintptr_t)image+0x1911100,&snapshot->root,8)&&
       camera_pose_read(snapshot->root+0x970,&snapshot->service,8)&&
       camera_pose_read(snapshot->service+0x1e0,&snapshot->clock,8)&&
       camera_pose_read(snapshot->clock,clock_bytes,sizeof clock_bytes)){
        memcpy(&snapshot->raw20,clock_bytes+0x20,8);
        memcpy(&snapshot->raw58,clock_bytes+0x58,8);
        memcpy(&snapshot->raw60,clock_bytes+0x60,8);
        memcpy(&snapshot->frame218,clock_bytes+0x218,4);
        memcpy(&snapshot->frame21c,clock_bytes+0x21c,4);
        snapshot->flags|=1;
    }
    BOOL q1=QueryPerformanceCounter(&after),qf=QueryPerformanceFrequency(&frequency);
    snapshot->qpc_before=before.QuadPart;snapshot->qpc_after=after.QuadPart;
    snapshot->qpc_frequency=frequency.QuadPart;
    if(q0&&q1&&qf&&frequency.QuadPart>0)snapshot->flags|=2;
    __asm__ volatile("ldmxcsr %0"::"m"(mxcsr));
}

typedef struct {
    uint64_t self,subject,observation,target,state,wall_us;
    uint32_t frame,thread;float dt;uint32_t kind,flags,mxcsr;
    unsigned char state_bytes[0x280],target_bytes[0x180],descriptor_bytes[0x20];
    HudGeometryClockSnapshot clock_before,clock_after;
} HudGeometryRecord;
_Static_assert(sizeof(HudGeometryRecord)==1320,"HUD geometry record layout");
static INIT_ONCE hud_geometry_once=INIT_ONCE_STATIC_INIT;
static SRWLOCK hud_geometry_lock=SRWLOCK_INIT;
static HudTapeHeader *hud_geometry_header;
static HudGeometryRecord *hud_geometry_records;
static BOOL CALLBACK hud_geometry_init(PINIT_ONCE once,PVOID param,PVOID*context){
    (void)once;(void)param;(void)context;wchar_t path[4096];DWORD len=GetEnvironmentVariableW(L"BB_HUD_GEOMETRY_TAPE",path,4096);
    if(!len||len>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&path[2]=='\\'))return TRUE;
    const DWORD capacity=262144;uint64_t size=sizeof(HudTapeHeader)+(uint64_t)capacity*sizeof(HudGeometryRecord);
    HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return TRUE;
    HANDLE map=CreateFileMappingW(file,NULL,PAGE_READWRITE,(DWORD)(size>>32),(DWORD)size,NULL);CloseHandle(file);if(!map)return TRUE;
    void*mem=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,(SIZE_T)size);CloseHandle(map);if(!mem)return TRUE;
    hud_geometry_header=mem;memset(mem,0,sizeof(HudTapeHeader));memcpy(hud_geometry_header->magic,"HUDGEO03",8);
    hud_geometry_header->version=3;hud_geometry_header->record_size=sizeof(HudGeometryRecord);hud_geometry_header->capacity=capacity;
    hud_geometry_records=(HudGeometryRecord*)(hud_geometry_header+1);
    printf("HudGeometryTape: enabled record=%zu capacity=%lu (diagnostic source reads only)\n",sizeof(HudGeometryRecord),(unsigned long)capacity);
    return TRUE;
}
static void hud_geometry_capture(void){
    HudTapeScope*scope=hud_tape_current;if(!scope||!scope->active||!(scope->record.flags&4))return;
    HudGeometryClockSnapshot clock_before;hud_geometry_clock_capture(&clock_before);
    InitOnceExecuteOnce(&hud_geometry_once,hud_geometry_init,NULL,NULL);if(!hud_geometry_header)return;
    const HudTapeRecord*source=&scope->record;HudGeometryRecord record;memset(&record,0,sizeof record);
    record.clock_before=clock_before;
    /* The common first72bytes are copied by named fields, not type-punning. */
    record.self=source->self;record.subject=source->subject;record.observation=source->observation;
    record.target=source->target;record.state=source->state;record.wall_us=source->wall_us;
    record.frame=source->frame;record.thread=source->thread;record.dt=source->dt;record.kind=source->kind;record.mxcsr=source->reserved;
    if(camera_pose_read(source->state,record.state_bytes,sizeof record.state_bytes))record.flags|=1;
    if(camera_pose_read(source->target,record.target_bytes,sizeof record.target_bytes))record.flags|=2;
    if(camera_pose_read(source->observation,record.descriptor_bytes,sizeof record.descriptor_bytes))record.flags|=4;
    /* Retain all attempts, but mark an authoritative matching input snapshot
     * explicitly. Concurrent geometry changes must not masquerade as this HUD
     * call's selected values. Flags15 is complete and source-vector-consistent. */
    if(record.flags==7){
        uintptr_t pointers[3];unsigned kind;memcpy(pointers,record.descriptor_bytes,sizeof pointers);
        if(pointers[1]==source->target&&pointers[2]==source->state&&camera_pose_read(pointers[0],&kind,4)&&kind==source->kind&&
           !memcmp(record.state_bytes+0xb0,source->vectors[0],16)&&!memcmp(record.target_bytes+0x140,source->vectors[1],16)&&
           !memcmp(record.state_bytes+0x90,source->vectors[2],16)&&!memcmp(record.state_bytes+0x110,source->vectors[3],16))record.flags|=8;
    }
    hud_geometry_clock_capture(&record.clock_after);
    AcquireSRWLockExclusive(&hud_geometry_lock);unsigned at=hud_geometry_header->count;
    if(at<hud_geometry_header->capacity){hud_geometry_records[at]=record;MemoryBarrier();hud_geometry_header->count=at+1;}
    else ++hud_geometry_header->dropped;
    ReleaseSRWLockExclusive(&hud_geometry_lock);
}
