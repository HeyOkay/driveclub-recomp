/* PRIVATE READ-ONLY DIAGNOSTIC. Original640900 and7CAFD0 exactly once.
 * Absent explicit D-only BB_SDK_REBASE_TRACE installs no hooks or file.
 * Captures real SDK dispatch/mean/raw histories; no added native getter call. */
typedef uintptr_t(ABI*SbrOriginal)(void);
static __attribute__((used)) SbrOriginal sbr_setter_original,sbr_outer_original;
typedef struct {uint64_t qpc_us,clock,physics_clock,vptr,setter88;int64_t time,dt,physics_time,physics_dt;uint32_t counter218,frame21c,physics218,physics21c,mxcsr,flags;float sdk_linear[4],sdk_angular[4];unsigned char raw[0x280];} SbrSnapshot;
typedef struct {uint64_t sequence,self,owner,raw,sdk,caller,parent_sequence;uint32_t kind,thread,handle,reconcile;float dt,subdt;uint32_t count,depth;float argument[4];uint64_t reserved;SbrSnapshot before,after;} SbrRecord;
typedef struct {char magic[8];uint32_t version,record_size,capacity,count,dropped,failed;uint64_t sequence,qpc_frequency,overflow_depth,image_base;} SbrHeader;
static void lfpo_sbr_notify(const SbrRecord*r,unsigned phase);
_Static_assert(sizeof(SbrSnapshot)==768,"SDK rebase snapshot768");
_Static_assert(sizeof(SbrRecord)==1648,"SDK rebase record1648");
_Static_assert(sizeof(SbrHeader)==64,"SDK rebase header64");
static HANDLE sbr_file=INVALID_HANDLE_VALUE;static SRWLOCK sbr_file_lock=SRWLOCK_INIT;
static SbrHeader sbr_header;static volatile LONG64 sbr_sequence;
static int sbr_enabled;static _Thread_local unsigned sbr_depth;static _Thread_local SbrRecord sbr_stack[8];
static uint64_t sbr_qpc(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return(uint64_t)(t.QuadPart/f.QuadPart)*1000000u+(uint64_t)(t.QuadPart%f.QuadPart)*1000000u/(uint64_t)f.QuadPart;}
static int sbr_clock(size_t offset,uint64_t*clock,int64_t*time,int64_t*dt,uint32_t*a,uint32_t*b){uintptr_t p=0;if(!camera_pose_read(runtime_image_base+0x1911100,&p,8)||!camera_pose_read(p+offset,&p,8)||!camera_pose_read(p+0x1e0,&p,8)||!camera_pose_read(p+0x58,time,8)||!camera_pose_read(p+0x60,dt,8)||!camera_pose_read(p+0x218,a,4)||!camera_pose_read(p+0x21c,b,4))return 0;*clock=p;return 1;}
static void sbr_snapshot(SbrRecord*r,SbrSnapshot*s,const unsigned char*context){
 s->qpc_us=sbr_qpc();memcpy(&s->mxcsr,context+24,4);
 if(r->raw&&camera_pose_read(r->raw+0x2ad0,s->raw,sizeof s->raw))s->flags|=1;
 if(r->sdk&&camera_pose_read(r->sdk+0x150,&s->vptr,8)&&camera_pose_read(s->vptr+0x88,&s->setter88,8)&&camera_pose_read(r->sdk+0x230,s->sdk_linear,32))s->flags|=2;
 if(sbr_clock(0x970,&s->clock,&s->time,&s->dt,&s->counter218,&s->frame21c))s->flags|=4;
 if(sbr_clock(0x968,&s->physics_clock,&s->physics_time,&s->physics_dt,&s->physics218,&s->physics21c))s->flags|=8;
 uintptr_t raw=0,sdk=0;if(camera_pose_read(r->owner+8,&raw,8)&&camera_pose_read(r->owner+0x10,&sdk,8)&&raw==r->raw&&sdk==r->sdk)s->flags|=16;
}
static int sbr_header_write(void){LARGE_INTEGER p;p.QuadPart=0;DWORD got=0;return SetFilePointerEx(sbr_file,p,NULL,FILE_BEGIN)&&WriteFile(sbr_file,&sbr_header,sizeof sbr_header,&got,NULL)&&got==sizeof sbr_header;}
static void sbr_emit(const SbrRecord*r){
 AcquireSRWLockExclusive(&sbr_file_lock);
 if(sbr_header.count>=sbr_header.capacity){++sbr_header.dropped;sbr_header_write();ReleaseSRWLockExclusive(&sbr_file_lock);return;}
 LARGE_INTEGER p;p.QuadPart=64+(LONGLONG)sbr_header.count*sizeof(SbrRecord);DWORD got=0;
 if(SetFilePointerEx(sbr_file,p,NULL,FILE_BEGIN)&&WriteFile(sbr_file,r,sizeof*r,&got,NULL)&&got==sizeof*r)++sbr_header.count;else++sbr_header.failed;
 if(r->sequence>sbr_header.sequence)sbr_header.sequence=r->sequence;sbr_header_write();ReleaseSRWLockExclusive(&sbr_file_lock);
}
/* Context offsets are asserted by the independent CPU ABI fixture. Snapshot
 * does not call an original native getter or synthesize any physical value. */
static __attribute__((used,noinline)) ABI void sbr_enter(const unsigned char*context,unsigned kind){
#ifdef SDK_REBASE_TEST_CLOBBER
 __asm__ volatile("vzeroall":::"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15");uint32_t testcsr=0x7f80;__asm__ volatile("ldmxcsr %0"::"m"(testcsr):"memory");
#endif
 ++sbr_depth;if(!sbr_enabled)return;
 if(sbr_depth>8){AcquireSRWLockExclusive(&sbr_file_lock);++sbr_header.overflow_depth;sbr_header_write();ReleaseSRWLockExclusive(&sbr_file_lock);return;}
 SbrRecord*r=sbr_stack+sbr_depth-1;memset(r,0,sizeof*r);r->sequence=(uint64_t)InterlockedIncrement64(&sbr_sequence);r->kind=kind;r->thread=GetCurrentThreadId();r->depth=sbr_depth;
 if(sbr_depth>1)r->parent_sequence=sbr_stack[sbr_depth-2].sequence;
 memcpy(&r->self,context+1112,8);memcpy(&r->caller,context+1176,8);if(r->caller>=runtime_image_base)r->caller-=runtime_image_base;
 if(kind==1){uintptr_t argument=0;memcpy(&r->handle,context+1120,4);memcpy(&argument,context+1136,8);camera_pose_read(argument,r->argument,16);
  int32_t index=(int32_t)(r->handle+UINT32_C(0xff0f3f01));camera_pose_read(r->self+(int64_t)index*0xb8+0x38,&r->owner,8);
 }else{r->owner=r->self;memcpy(&r->reconcile,context+1120,4);memcpy(&r->dt,context+512,4);}
 if(r->owner){camera_pose_read(r->owner+8,&r->raw,8);camera_pose_read(r->owner+0x10,&r->sdk,8);}
 sbr_snapshot(r,&r->before,context);
 lfpo_sbr_notify(r,0);
}
static __attribute__((used,noinline)) ABI void sbr_leave(const unsigned char*context){
#ifdef SDK_REBASE_TEST_CLOBBER
 __asm__ volatile("vzeroall":::"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15");uint32_t testcsr=0x7f80;__asm__ volatile("ldmxcsr %0"::"m"(testcsr):"memory");
#endif
 if(!sbr_depth)return;
 if(sbr_enabled&&sbr_depth<=8){SbrRecord*r=sbr_stack+sbr_depth-1;sbr_snapshot(r,&r->after,context);lfpo_sbr_notify(r,1);if(r->kind==2){camera_pose_read(r->owner+0x18,&r->count,4);camera_pose_read(r->owner+0x1c,&r->subdt,4);}sbr_emit(r);}
 --sbr_depth;
}
/* Preserve exact pre-native and post-native all GPR/YMM/RFLAGS/x87/MXCSR.
 * These two functions have only register arguments. Extra8bytes only align the
 * original CALL stack; no caller stack arguments exist or are moved. */
#define SBR_SAVE "pushfq\n pushq %rax\n pushq %rbx\n pushq %rcx\n pushq %rdx\n pushq %rbp\n pushq %rsi\n pushq %rdi\n pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %r12\n pushq %r13\n pushq %r14\n pushq %r15\n subq $1048,%rsp\n fxsave64 0(%rsp)\n" \
 "vmovdqu %ymm0,512(%rsp)\n vmovdqu %ymm1,544(%rsp)\n vmovdqu %ymm2,576(%rsp)\n vmovdqu %ymm3,608(%rsp)\n vmovdqu %ymm4,640(%rsp)\n vmovdqu %ymm5,672(%rsp)\n vmovdqu %ymm6,704(%rsp)\n vmovdqu %ymm7,736(%rsp)\n vmovdqu %ymm8,768(%rsp)\n vmovdqu %ymm9,800(%rsp)\n vmovdqu %ymm10,832(%rsp)\n vmovdqu %ymm11,864(%rsp)\n vmovdqu %ymm12,896(%rsp)\n vmovdqu %ymm13,928(%rsp)\n vmovdqu %ymm14,960(%rsp)\n vmovdqu %ymm15,992(%rsp)\n"
#define SBR_RESTORE "fxrstor64 0(%rsp)\n vmovdqu 512(%rsp),%ymm0\n vmovdqu 544(%rsp),%ymm1\n vmovdqu 576(%rsp),%ymm2\n vmovdqu 608(%rsp),%ymm3\n vmovdqu 640(%rsp),%ymm4\n vmovdqu 672(%rsp),%ymm5\n vmovdqu 704(%rsp),%ymm6\n vmovdqu 736(%rsp),%ymm7\n vmovdqu 768(%rsp),%ymm8\n vmovdqu 800(%rsp),%ymm9\n vmovdqu 832(%rsp),%ymm10\n vmovdqu 864(%rsp),%ymm11\n vmovdqu 896(%rsp),%ymm12\n vmovdqu 928(%rsp),%ymm13\n vmovdqu 960(%rsp),%ymm14\n vmovdqu 992(%rsp),%ymm15\n addq $1048,%rsp\n popq %r15\n popq %r14\n popq %r13\n popq %r12\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n popq %rdi\n popq %rsi\n popq %rbp\n popq %rdx\n popq %rcx\n popq %rbx\n popq %rax\n popfq\n"
static __attribute__((naked,used)) ABI uintptr_t sbr_setter(void){__asm__ volatile(SBR_SAVE "movq %rsp,%rdi\n movl $1,%esi\n callq sbr_enter\n" SBR_RESTORE "leaq -8(%rsp),%rsp\n callq *sbr_setter_original(%rip)\n leaq 8(%rsp),%rsp\n" SBR_SAVE "movq %rsp,%rdi\n callq sbr_leave\n" SBR_RESTORE "retq\n");}
static __attribute__((naked,used)) ABI uintptr_t sbr_outer(void){__asm__ volatile(SBR_SAVE "movq %rsp,%rdi\n movl $2,%esi\n callq sbr_enter\n" SBR_RESTORE "leaq -8(%rsp),%rsp\n callq *sbr_outer_original(%rip)\n leaq 8(%rsp),%rsp\n" SBR_SAVE "movq %rsp,%rdi\n callq sbr_leave\n" SBR_RESTORE "retq\n");}
#undef SBR_SAVE
#undef SBR_RESTORE
static void sbr_jump(unsigned char*p,uintptr_t target){static const unsigned char op[]={0xff,0x25,0,0,0,0};memcpy(p,op,6);memcpy(p+6,&target,8);}
static void sdk_rebase_observer_install(void){
 wchar_t path[4096];DWORD n=GetEnvironmentVariableW(L"BB_SDK_REBASE_TRACE",path,4096);const char*oracle=camera_fix_getenv("BB_CAMERA_ACCURACY_ORACLE");
 if(n<4||n>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&(path[2]=='\\'||path[2]=='/'))||!oracle||strcmp(oracle,"1"))return;
 if(sbr_enabled)return;
 static const unsigned char setter[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x38};
 static const unsigned char outer[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xec,0x38,0x41,0x89,0xf6};
 if(runtime_image_size<0x7cafd0+sizeof outer||memcmp(image+0x640900,setter,sizeof setter)||memcmp(image+0x7cafd0,outer,sizeof outer)){puts("SDKRebaseTrace: original signatures unavailable; no hooks/file");return;}
 unsigned char*code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!code)return;
 memcpy(code,setter,sizeof setter);sbr_jump(code+sizeof setter,runtime_image_base+0x640900+sizeof setter);memcpy(code+64,outer,sizeof outer);sbr_jump(code+64+sizeof outer,runtime_image_base+0x7cafd0+sizeof outer);
 DWORD old;if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return;}FlushInstructionCache(GetCurrentProcess(),code,4096);
 sbr_file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(sbr_file==INVALID_HANDLE_VALUE){VirtualFree(code,0,MEM_RELEASE);return;}
 memset(&sbr_header,0,sizeof sbr_header);memcpy(sbr_header.magic,"SDKREB01",8);sbr_header.version=1;sbr_header.record_size=sizeof(SbrRecord);sbr_header.capacity=262144;sbr_header.image_base=runtime_image_base;LARGE_INTEGER f;QueryPerformanceFrequency(&f);sbr_header.qpc_frequency=(uint64_t)f.QuadPart;
 if(!sbr_header_write()){CloseHandle(sbr_file);sbr_file=INVALID_HANDLE_VALUE;VirtualFree(code,0,MEM_RELEASE);return;}
 sbr_setter_original=(SbrOriginal)code;sbr_outer_original=(SbrOriginal)(code+64);sbr_enabled=1;
 sbr_jump(image+0x640900,(uintptr_t)sbr_setter);memset(image+0x640900+14,0x90,sizeof setter-14);sbr_jump(image+0x7cafd0,(uintptr_t)sbr_outer);memset(image+0x7cafd0+14,0x90,sizeof outer-14);
 FlushInstructionCache(GetCurrentProcess(),image+0x640900,sizeof setter);FlushInstructionCache(GetCurrentProcess(),image+0x7cafd0,sizeof outer);
 puts("SDKRebaseTrace: READ-ONLY640900/7CAFD0 before+after, actualSDKvptr/88 and raw10/40/B0/mean; boundedD-only, no native queries or physical writes");
}
