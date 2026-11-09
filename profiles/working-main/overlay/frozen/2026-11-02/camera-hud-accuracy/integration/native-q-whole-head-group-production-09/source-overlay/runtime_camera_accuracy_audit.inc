/* Local diagnostic only. Memory-mapped output stays on the configured D: path.
 * Observes original hooks; never changes native input, output, timing or RNG. */
typedef struct {
    unsigned char magic[8];uint32_t version,record_size,capacity,count,overflow,reserved;
} CameraAccuracyHeader;
typedef struct {
    uint64_t wall_us,self,peer,caller;
    uint32_t frame,thread,event;float dt;
    float data[24];
} CameraAccuracyRecord;
static INIT_ONCE camera_accuracy_once=INIT_ONCE_STATIC_INIT;
static SRWLOCK camera_accuracy_lock=SRWLOCK_INIT;
static CameraAccuracyHeader *camera_accuracy_header;
static CameraAccuracyRecord *camera_accuracy_records;
static wchar_t camera_accuracy_start_path[4096];
static volatile LONG camera_accuracy_started;
static int camera_accuracy_oracle;
static volatile LONG64 camera_accuracy_provider;
static LARGE_INTEGER camera_accuracy_qpc_origin,camera_accuracy_qpc_frequency;
static uint64_t camera_accuracy_boot_origin_us;
static uint64_t camera_accuracy_time_us(void){
    LARGE_INTEGER now;
    if(camera_accuracy_qpc_frequency.QuadPart>0&&QueryPerformanceCounter(&now)){
        uint64_t delta=(uint64_t)(now.QuadPart-camera_accuracy_qpc_origin.QuadPart);
        uint64_t frequency=(uint64_t)camera_accuracy_qpc_frequency.QuadPart;
        return camera_accuracy_boot_origin_us+(delta/frequency)*1000000u+((delta%frequency)*1000000u)/frequency;
    }
    return GetTickCount64()*1000u;
}
uintptr_t camera_accuracy_replay_provider(void){return(uintptr_t)InterlockedCompareExchange64(&camera_accuracy_provider,0,0);}
static BOOL CALLBACK camera_accuracy_init(PINIT_ONCE once,PVOID param,PVOID *context){
    (void)once;(void)param;(void)context;
    wchar_t path[4096];DWORD len=GetEnvironmentVariableW(L"BB_CAMERA_ACCURACY_AUDIT",path,4096);
    if(!len||len>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&path[2]=='\\'))return TRUE;
    const DWORD capacity=4194304;
    const uint64_t size=sizeof(CameraAccuracyHeader)+(uint64_t)capacity*sizeof(CameraAccuracyRecord);
    HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return TRUE;
    HANDLE map=CreateFileMappingW(file,NULL,PAGE_READWRITE,(DWORD)(size>>32),(DWORD)size,NULL);
    CloseHandle(file);if(!map)return TRUE;
    void *memory=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,(SIZE_T)size);CloseHandle(map);if(!memory)return TRUE;
    camera_accuracy_header=memory;memset(memory,0,sizeof(CameraAccuracyHeader));
    memcpy(camera_accuracy_header->magic,"CAMAUD01",8);camera_accuracy_header->version=1;
    /* Version2 retains boot-microsecond units but uses QPC deltas. TickCount64
       is commonly quantized to15.625ms: dropping its duplicate timestamps
       invalidates VRR motion-speed estimates. Do not infer speed from v1. */
    if(QueryPerformanceFrequency(&camera_accuracy_qpc_frequency)&&
       QueryPerformanceCounter(&camera_accuracy_qpc_origin)&&camera_accuracy_qpc_frequency.QuadPart>0){
        camera_accuracy_boot_origin_us=GetTickCount64()*1000u;
        camera_accuracy_header->version=2;
    }else camera_accuracy_qpc_frequency.QuadPart=0;
    camera_accuracy_header->record_size=sizeof(CameraAccuracyRecord);camera_accuracy_header->capacity=capacity;
    camera_accuracy_records=(CameraAccuracyRecord*)(camera_accuracy_header+1);
    DWORD gate=GetEnvironmentVariableW(L"BB_CAMERA_TRACE_START",camera_accuracy_start_path,4096);
    if(!gate||gate>=4096){camera_accuracy_start_path[0]=0;camera_accuracy_started=1;}
    printf("CameraAccuracyAudit: local observer enabled, record_size=%zu capacity=%lu\n",sizeof(CameraAccuracyRecord),(unsigned long)capacity);
    return TRUE;
}
static int camera_accuracy_trace_active(void){InitOnceExecuteOnce(&camera_accuracy_once,camera_accuracy_init,NULL,NULL);return camera_accuracy_header!=NULL;}
static unsigned camera_accuracy_frame(void){
    uintptr_t a=0;unsigned frame=~0u;
    if(camera_pose_read((uintptr_t)image+0x1911100,&a,8)&&camera_pose_read(a+0x968,&a,8)&&
       camera_pose_read(a+0x1e0,&a,8))camera_pose_read(a+0x21c,&frame,4);
    return frame;
}
static void camera_accuracy_log(unsigned event,void*self,uintptr_t peer,uintptr_t caller,float dt,const float*data,unsigned n){
    if(event==4&&caller==0x7bbaa4)return; /* unrelated menu spring; keep trace budget for camera/HUD */
    InitOnceExecuteOnce(&camera_accuracy_once,camera_accuracy_init,NULL,NULL);
    if(!camera_accuracy_header)return;
    if(!camera_accuracy_started){
        if(GetFileAttributesW(camera_accuracy_start_path)==INVALID_FILE_ATTRIBUTES)return;
        InterlockedExchange(&camera_accuracy_started,1);
    }
    CameraAccuracyRecord rec={0};rec.wall_us=camera_accuracy_time_us();
    rec.self=(uintptr_t)self;rec.peer=peer;rec.caller=caller;
    rec.thread=GetCurrentThreadId();rec.frame=camera_accuracy_frame();rec.event=event;rec.dt=dt;
    if(n>24)n=24;if(data)memcpy(rec.data,data,n*sizeof(float));
    AcquireSRWLockExclusive(&camera_accuracy_lock);
    unsigned at=camera_accuracy_header->count;
    if(at<camera_accuracy_header->capacity){camera_accuracy_records[at]=rec;MemoryBarrier();camera_accuracy_header->count=at+1;}
    else ++camera_accuracy_header->overflow;
    ReleaseSRWLockExclusive(&camera_accuracy_lock);
    if(event==6){
        uintptr_t clock=0;int64_t native[2]={0};unsigned native_frame=~0u;
        if(camera_pose_read((uintptr_t)image+0x1911100,&clock,8)&&camera_pose_read(clock+0x970,&clock,8)&&
           camera_pose_read(clock+0x1e0,&clock,8)&&camera_pose_read(clock+0x58,native,16)&&
           camera_pose_read(clock+0x21c,&native_frame,4)){
            float timing[11]={0};memcpy(timing,native,16);memcpy(timing+4,&native_frame,4);
            /* Exact mapping to PHYSRC/HUDGEO absolute QPC timestamps. Never
               infer an inter-tape clock offset from native frame labels. */
            memcpy(timing+5,&camera_accuracy_qpc_origin.QuadPart,8);
            memcpy(timing+7,&camera_accuracy_qpc_frequency.QuadPart,8);
            memcpy(timing+9,&camera_accuracy_boot_origin_us,8);
            camera_accuracy_log(23,self,clock,caller,dt,timing,11);
        }
    }
}
