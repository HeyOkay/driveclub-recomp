#ifndef CAMERA_SPRING_TAPE_INCLUDED
#define CAMERA_SPRING_TAPE_INCLUDED
/* Diagnostic only, full startup/activation tape. No race-start gate: first native
 * force/step must be recorded too. Explicit BB_CAMERA_SPRING_TAPE=D:\... opts in. */
typedef struct{unsigned char magic[8];uint32_t version,record_size,capacity,count,overflow,reserved;} CSTHeader;
typedef struct{
 uint64_t sequence,wall_ms,owner,spring,target,binding;
 uint32_t event,axis,frame,raw_valid;
 float dt,force,returned,reserved;
 unsigned char before_force[44],before_step[44],after_step[44];
 float velocity[4],previous_velocity[4],basis[16],gains[4],acceleration[4];
 uint32_t target_frame,kind,mxcsr;
} CSTRecord;
_Static_assert(sizeof(CSTRecord)==352,"Spring tape record ABI");
static INIT_ONCE cst_once=INIT_ONCE_STATIC_INIT;static SRWLOCK cst_lock=SRWLOCK_INIT;
static CSTHeader*cst_header;static CSTRecord*cst_records;
static BOOL CALLBACK cst_init(PINIT_ONCE once,PVOID parameter,PVOID*context){
 (void)once;(void)parameter;(void)context;wchar_t path[4096];DWORD n=GetEnvironmentVariableW(L"BB_CAMERA_SPRING_TAPE",path,4096);
 if(n<4||n>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')))return TRUE;
 const DWORD capacity=65536;uint64_t size=sizeof(CSTHeader)+(uint64_t)capacity*sizeof(CSTRecord);
 HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return TRUE;
 HANDLE map=CreateFileMappingW(file,NULL,PAGE_READWRITE,(DWORD)(size>>32),(DWORD)size,NULL);CloseHandle(file);if(!map)return TRUE;
 void*memory=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,(SIZE_T)size);CloseHandle(map);if(!memory)return TRUE;
 cst_header=memory;memset(memory,0,sizeof *cst_header);memcpy(cst_header->magic,"CSPRNG01",8);cst_header->version=1;cst_header->record_size=sizeof(CSTRecord);cst_header->capacity=capacity;cst_records=(CSTRecord*)(cst_header+1);
 puts("CameraSpringTape: full native input/state startup tape enabled (D: diagnostic only)");return TRUE;
}
static void camera_spring_tape_record(unsigned event,void*self,void*owner,unsigned frame,unsigned axis,
 float force,float dt,float returned,const void*before_force,const void*before_step,const void*after_step,uintptr_t target,uintptr_t binding){
 InitOnceExecuteOnce(&cst_once,cst_init,NULL,NULL);if(!cst_header)return;
 CSTRecord r={0};r.wall_ms=GetTickCount64();r.owner=(uintptr_t)owner;r.spring=(uintptr_t)self;r.target=target;r.binding=binding;r.event=event;r.axis=axis;r.frame=frame;r.dt=dt;r.force=force;r.returned=returned;
 /* Denormal/rounding controls are part of the native arithmetic contract.
    Explicit former tail-padding word; version1/352byte record stays readable. */
 __asm__ volatile("stmxcsr %0":"=m"(r.mxcsr));
 memcpy(r.before_force,before_force,44);memcpy(r.before_step,before_step,44);memcpy(r.after_step,after_step,44);
 if(event==1&&target){
  #ifdef CRR_HAS_NATIVE_OBSERVATION
  r.reserved=(float)crr_native_observation_flags(target,frame);
  #endif
  r.raw_valid=camera_pose_read(target+0x120,r.velocity,16)&&camera_pose_read(target+0x200,r.previous_velocity,16)&&camera_pose_read(target+0xd0,r.basis,64)&&
    camera_pose_read((uintptr_t)owner+0x4fc,r.gains,8)&&camera_pose_read((uintptr_t)owner+0x4ec,r.gains+2,8)&&
    camera_pose_read(target+0x140,r.acceleration,16)&&camera_pose_read(target+0x44,&r.target_frame,4)&&camera_pose_read(target+8,&r.kind,4);
 }
 AcquireSRWLockExclusive(&cst_lock);unsigned at=cst_header->count;if(at<cst_header->capacity){r.sequence=at;cst_records[at]=r;MemoryBarrier();cst_header->count=at+1;}else ++cst_header->overflow;ReleaseSRWLockExclusive(&cst_lock);
}
#endif
