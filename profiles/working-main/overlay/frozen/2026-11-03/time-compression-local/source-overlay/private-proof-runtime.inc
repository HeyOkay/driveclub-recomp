/* Optional read-only proof of actual PRIVATE HAI operations, not more native
 * getters. Includes follow HAIManagerCall and hai_active_call definitions. */
typedef struct{unsigned char magic[8];uint32_t version,record_size,capacity,count,dropped,failed;uint64_t qpc_frequency,reserved[3];} HAIPHeader;
typedef struct{
 uint64_t sequence,qpc,call_serial,owner,target,clock,generation,reset_token;
 int64_t render_T,render_dt_us,Q,duration_us;
 uint32_t stage,phase,ok,thread,frame,mxcsr_pre,mxcsr_post,input_size;
 int32_t count_before,count_after;uint32_t applied,unchanged;
 uint32_t euler_calls,euler_csr;float euler_raw[4];
 unsigned char input[sizeof(HACInputs)],before[0x3a0],after[0x3a0];
} HAIPRecord;
_Static_assert(sizeof(HAIPHeader)==64,"HAIP header64");
_Static_assert(sizeof(HACInputs)==360&&sizeof(HSIInput)==84,"HAIP native input ABI");
_Static_assert(sizeof(HAIPRecord)==2384,"HAIP record2384");
static HAIPHeader*haip_header;static HAIPRecord*haip_records;static SRWLOCK haip_lock=SRWLOCK_INIT;
static volatile LONG64 haip_serial;
typedef struct{uint64_t serial;unsigned phase;int64_t Q,duration;} HAIPLabel;
static _Thread_local HAIPLabel haip_current;
static _Thread_local HAIPRecord*haip_euler_record;
static HSIEuler haip_euler_original;
static ABI void haip_euler(void*matrix,float*p,float*h,float*r){
 haip_euler_original(matrix,p,h,r);uint32_t native_csr=_mm_getcsr();HAIPRecord*record=haip_euler_record;
 if(record){++record->euler_calls;memcpy(record->euler_raw,p,4);memcpy(record->euler_raw+1,h,4);memcpy(record->euler_raw+2,r,4);record->euler_csr=native_csr;}_mm_setcsr(native_csr);
}
static void haip_label(unsigned phase,int64_t Q,int64_t duration){
 if(!haip_header)return;HAIManagerCall*c=hai_active_call;if(!c)return;
 haip_current=(HAIPLabel){c->proof_serial,phase,Q,duration};
}
static uint64_t haip_next_serial(void){return haip_header?(uint64_t)InterlockedIncrement64(&haip_serial):0;}
static int haip_begin(HAIPRecord*r,unsigned stage,const unsigned char*camera,const void*input,size_t size,uint32_t native_csr){
 HAIManagerCall*c=hai_active_call;if(!haip_header||!c||!c->proof_serial||haip_current.serial!=c->proof_serial||size>sizeof r->input)return 0;
 memset(r,0,sizeof*r);r->call_serial=c->proof_serial;r->owner=c->owner;r->target=c->target;r->clock=c->clock.clock;r->generation=c->generation;r->reset_token=c->reset_seen?c->reset_token:c->state.reset_token;
 r->render_T=c->clock.time_us;r->render_dt_us=c->clock.dt_us;r->Q=haip_current.Q;r->duration_us=haip_current.duration;r->phase=haip_current.phase;r->stage=stage;r->frame=c->clock.frame;r->thread=GetCurrentThreadId();r->mxcsr_pre=native_csr;r->input_size=(uint32_t)size;
 memcpy(r->input,input,size);memcpy(r->before,camera,sizeof r->before);memcpy(&r->count_before,camera+0x33c,4);return 1;
}
static void haip_append(HAIPRecord*r){
 LARGE_INTEGER q;if(QueryPerformanceCounter(&q))r->qpc=(uint64_t)q.QuadPart;
 AcquireSRWLockExclusive(&haip_lock);uint32_t n=haip_header->count;
 if(n<haip_header->capacity){r->sequence=n;haip_records[n]=*r;MemoryBarrier();haip_header->count=n+1;}else ++haip_header->dropped;
 ReleaseSRWLockExclusive(&haip_lock);
}
static int haip_copy(unsigned char camera[0x3a0],const HSIInput*in){
 if(!haip_header)return hsi_native_copy(&hai_copy_kernel,camera,in,NULL);
 uint32_t host=_mm_getcsr();HAIPRecord r;int capture=haip_begin(&r,1,camera,in,sizeof*in,in->mxcsr_mid);_mm_setcsr(host);
 HAIPRecord*previous=haip_euler_record;haip_euler_record=capture?&r:NULL;
 uint32_t post=0;int ok=hsi_native_copy(&hai_copy_kernel,camera,in,&post);uint32_t afterhost=_mm_getcsr();haip_euler_record=previous;
 if(capture){r.ok=(uint32_t)ok;r.mxcsr_post=post;memcpy(r.after,camera,sizeof r.after);memcpy(&r.count_after,camera+0x33c,4);haip_append(&r);}_mm_setcsr(afterhost);return ok;
}
static int haip_solver(unsigned char camera[0x3a0],const HACInputs*in){
 if(!haip_header)return hac_native_step(&hcal_kernel,camera,in,NULL);
 uint32_t host=_mm_getcsr();HAIPRecord r;int capture=haip_begin(&r,2,camera,in,sizeof*in,in->mxcsr_pre);_mm_setcsr(host);
 uint32_t post=0;int ok=hac_native_step(&hcal_kernel,camera,in,&post);uint32_t afterhost=_mm_getcsr();
 if(capture){r.ok=(uint32_t)ok;r.mxcsr_post=post;memcpy(r.after,camera,sizeof r.after);memcpy(&r.count_after,camera+0x33c,4);haip_append(&r);}_mm_setcsr(afterhost);return ok;
}
static void haip_commit(const HAIManagerCall*c,const HAIResult*result,const unsigned char*native,unsigned applied,unsigned unchanged){
 if(!haip_header||!c->proof_serial)return;uint32_t host=_mm_getcsr();HAIPLabel previous=haip_current;haip_current=(HAIPLabel){c->proof_serial,6,result->canonical_us,result->credit_us};HAIPRecord r;
 if(haip_begin(&r,3,native,result,sizeof*result,host)){r.applied=applied;r.unchanged=unchanged;r.mxcsr_post=host;r.ok=camera_pose_read(c->owner,r.after,sizeof r.after);if(r.ok)memcpy(&r.count_after,r.after+0x33c,4);haip_append(&r);}
 haip_current=previous;_mm_setcsr(host);
}
static void haip_init(void){
 wchar_t path[4096];DWORD n=GetEnvironmentVariableW(L"BB_HAI_PRIVATE_PROOF",path,4096);if(n<4||n>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')))return;
 const uint32_t cap=131072;uint64_t size=64+(uint64_t)cap*sizeof(HAIPRecord);HANDLE f=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return;
 HANDLE map=CreateFileMappingW(f,NULL,PAGE_READWRITE,(DWORD)(size>>32),(DWORD)size,NULL);CloseHandle(f);if(!map)return;void*p=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,(SIZE_T)size);CloseHandle(map);if(!p)return;
 haip_header=p;memset(p,0,64);memcpy(haip_header->magic,"HAIPRF01",8);haip_header->version=1;haip_header->record_size=sizeof(HAIPRecord);haip_header->capacity=cap;LARGE_INTEGER freq;if(QueryPerformanceFrequency(&freq))haip_header->qpc_frequency=(uint64_t)freq.QuadPart;haip_records=(HAIPRecord*)(haip_header+1);
 haip_euler_original=hai_copy_kernel.euler;hai_copy_kernel.euler=haip_euler;
 puts("HAIPrivateProof: read-only actual private copy/solver/Q/commit tape enabled (D: only)");
}
