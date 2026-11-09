/* Optional read-only native physical-source observer. Requires the existing
 * SDK rebase observer for original setup identity/order; no extra native query.
 * Explicit D-only BB_LOW_FPS_PHYSICS_TRACE installs only three entryhooks;
 * publication observation reuses the existing sole HSQ originalcall. */
typedef uintptr_t(ABI*LfpoOriginal)(void);
static __attribute__((used)) LfpoOriginal lfpo_original[3];
typedef struct {uint64_t pointer;int64_t time,dt;uint32_t frame218,frame21c;} LfpoClock;
typedef struct {
 uint64_t sequence,call,parent,raw,owner,sdk,caller,qpc,raw_offset;
 uint32_t kind,phase,thread,depth,index,count,flags,mxcsr,raw_bytes,reserved;
 float dt,batch_dt,quantum_numerator,quantum_divisor;
 LfpoClock start[2],end[2];
 float sdk_linear[4],sdk_angular[4],inner_linear[4],inner_angular[4],sum_linear[4],sum_angular[4],pose[16];
 unsigned char carstate_gate[0x60];float input_basis[16];
} LfpoRecord;
typedef struct {char magic[8];uint32_t version,record_size,capacity,count,dropped,failed;uint64_t sequence,raw_bytes,raw_capacity,qpc_frequency;} LfpoHeader;
_Static_assert(sizeof(LfpoClock)==32,"LFPO clock32");
_Static_assert(sizeof(LfpoRecord)==576,"LFPO record576");
_Static_assert(sizeof(LfpoHeader)==64,"LFPO header64");
enum {LFPO_RAW_BYTES=0x3c70};
typedef struct {uint64_t call,parent,raw,owner,sdk,caller;uint32_t kind,index,count;float dt,batch_dt,basis[16];} LfpoScope;
static _Thread_local struct {LfpoScope entries[8];unsigned depth,overflow;unsigned char payload[LFPO_RAW_BYTES];} lfpo_tls;
static struct {uintptr_t owner,raw,sdk;} lfpo_owners[64];
static SRWLOCK lfpo_registry_lock=SRWLOCK_INIT,lfpo_file_lock=SRWLOCK_INIT;
static HANDLE lfpo_file=INVALID_HANDLE_VALUE,lfpo_raw_file=INVALID_HANDLE_VALUE;
static LfpoHeader lfpo_header;static volatile LONG64 lfpo_call_sequence;static int lfpo_enabled;
static void lfpo_putjump(unsigned char*p,uintptr_t t){const unsigned char j[]={0xff,0x25,0,0,0,0};memcpy(p,j,6);memcpy(p+6,&t,8);}
static int lfpo_write_at(HANDLE f,uint64_t at,const void*p,DWORD size){LARGE_INTEGER pos;pos.QuadPart=(LONGLONG)at;DWORD got=0;return SetFilePointerEx(f,pos,NULL,FILE_BEGIN)&&WriteFile(f,p,size,&got,NULL)&&got==size;}
static int lfpo_header_write(void){return lfpo_write_at(lfpo_file,0,&lfpo_header,sizeof lfpo_header);}
static int lfpo_clock(unsigned root,LfpoClock*c){uintptr_t p=0;unsigned char bytes[0x220];if(!camera_pose_read(runtime_image_base+0x1911100,&p,8)||!camera_pose_read(p+root,&p,8)||!camera_pose_read(p+0x1e0,&p,8)||!camera_pose_read(p,bytes,sizeof bytes))return 0;c->pointer=p;memcpy(&c->time,bytes+0x58,8);memcpy(&c->dt,bytes+0x60,8);memcpy(&c->frame218,bytes+0x218,4);memcpy(&c->frame21c,bytes+0x21c,4);return 1;}
static int lfpo_owner_valid(uintptr_t owner,uintptr_t raw,uintptr_t sdk){uintptr_t a=0,b=0;return owner&&camera_pose_read(owner+8,&a,8)&&camera_pose_read(owner+0x10,&b,8)&&a==raw&&b==sdk;}
static void lfpo_register(const SbrRecord*r){
 if(!r->owner||!r->raw||!r->sdk||!lfpo_owner_valid(r->owner,r->raw,r->sdk))return;
 AcquireSRWLockExclusive(&lfpo_registry_lock);unsigned free_at=64;
 for(unsigned i=0;i<64;++i){if(lfpo_owners[i].owner==r->owner||lfpo_owners[i].raw==r->raw){free_at=i;break;}if(!lfpo_owners[i].owner&&free_at==64)free_at=i;}
 if(free_at<64){lfpo_owners[free_at].owner=r->owner;lfpo_owners[free_at].raw=r->raw;lfpo_owners[free_at].sdk=r->sdk;}
 ReleaseSRWLockExclusive(&lfpo_registry_lock);
}
static void lfpo_resolve(LfpoScope*s){
 AcquireSRWLockShared(&lfpo_registry_lock);
 for(unsigned i=0;i<64;++i)if(lfpo_owners[i].raw==s->raw){s->owner=lfpo_owners[i].owner;s->sdk=lfpo_owners[i].sdk;break;}
 ReleaseSRWLockShared(&lfpo_registry_lock);
 if(!lfpo_owner_valid(s->owner,s->raw,s->sdk)){s->owner=0;s->sdk=0;}
}
static void lfpo_capture(const LfpoScope*s,unsigned phase,uint32_t csr){
 if(!lfpo_enabled)return;
 LfpoRecord r;memset(&r,0,sizeof r);r.call=s->call;r.parent=s->parent;r.raw=s->raw;r.owner=s->owner;r.sdk=s->sdk;r.caller=s->caller;r.kind=s->kind;r.phase=phase;r.depth=lfpo_tls.depth;r.index=s->index;r.count=s->count;r.dt=s->dt;r.batch_dt=s->batch_dt;r.mxcsr=csr;r.thread=GetCurrentThreadId();memcpy(r.input_basis,s->basis,64);
 LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);r.qpc=(uint64_t)qpc.QuadPart;
 if(lfpo_clock(0x968,r.start))r.flags|=1;if(lfpo_clock(0x970,r.start+1))r.flags|=2;
 if(camera_pose_read(runtime_image_base+0x12f6bd0,&r.quantum_numerator,4)&&camera_pose_read(runtime_image_base+0x155eda8,&r.quantum_divisor,4))r.flags|=4;
 if(lfpo_owner_valid(s->owner,s->raw,s->sdk)&&camera_pose_read(s->sdk+0x230,r.sdk_linear,32))r.flags|=8;
 if(s->raw&&camera_pose_read(s->raw+0x2cf0,r.inner_linear,64)&&camera_pose_read(s->raw+0x2de0,r.pose,64)&&camera_pose_read(s->raw+0x2f10,r.carstate_gate,sizeof r.carstate_gate))r.flags|=16;
 /* Full native allocation prefix is captured only at completed inner steps
  * and actual scalar-producer entry/exit. No fictional absolute innerQ. */
 if((s->kind==2||s->kind==3)&&s->raw&&camera_pose_read(s->raw,lfpo_tls.payload,LFPO_RAW_BYTES)){r.flags|=32;r.raw_bytes=LFPO_RAW_BYTES;}
 if(lfpo_clock(0x968,r.end))r.flags|=64;if(lfpo_clock(0x970,r.end+1))r.flags|=128;
 if((r.flags&195)==195&&!memcmp(r.start,r.end,sizeof r.start))r.flags|=256;
 AcquireSRWLockExclusive(&lfpo_file_lock);
 r.sequence=++lfpo_header.sequence;
 if(lfpo_header.count>=lfpo_header.capacity){++lfpo_header.dropped;lfpo_header_write();ReleaseSRWLockExclusive(&lfpo_file_lock);return;}
 if(r.raw_bytes){
  if(lfpo_header.raw_bytes+r.raw_bytes<=lfpo_header.raw_capacity){r.raw_offset=lfpo_header.raw_bytes;if(lfpo_write_at(lfpo_raw_file,r.raw_offset,lfpo_tls.payload,r.raw_bytes))lfpo_header.raw_bytes+=r.raw_bytes;else{++lfpo_header.failed;r.flags|=1024;r.raw_bytes=0;}}
  else{++lfpo_header.dropped;r.flags|=512;r.raw_bytes=0;}
 }
 if(lfpo_write_at(lfpo_file,64+(uint64_t)lfpo_header.count*sizeof r,&r,sizeof r))++lfpo_header.count;else ++lfpo_header.failed;
 lfpo_header_write();ReleaseSRWLockExclusive(&lfpo_file_lock);
}
static __attribute__((used,noinline)) ABI void lfpo_enter(const unsigned char*c,unsigned kind){
#ifdef LFPO_TEST_CLOBBER
 __asm__ volatile("fninit");
 __asm__ volatile("vzeroall":::"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15");uint32_t badcsr=0x7f80;__asm__ volatile("ldmxcsr %0"::"m"(badcsr):"memory");
#endif
 if(!lfpo_enabled)return;
 if(lfpo_tls.depth>=8){++lfpo_tls.overflow;AcquireSRWLockExclusive(&lfpo_file_lock);++lfpo_header.dropped;lfpo_header_write();ReleaseSRWLockExclusive(&lfpo_file_lock);return;}
 LfpoScope*s=lfpo_tls.entries+lfpo_tls.depth++;memset(s,0,sizeof*s);s->call=(uint64_t)InterlockedIncrement64(&lfpo_call_sequence);s->kind=kind;memcpy(&s->caller,c+1176,8);if(s->caller>=runtime_image_base)s->caller-=runtime_image_base;memcpy(&s->dt,c+512,4);
 if(kind==4){uintptr_t self=0;memcpy(&self,c+1112,8);s->caller=0;s->owner=self;camera_pose_read(self+8,&s->raw,8);camera_pose_read(self+0x10,&s->sdk,8);camera_pose_read(self+0x18,&s->count,4);camera_pose_read(self+0x1c,&s->batch_dt,4);}
 else{memcpy(&s->raw,c+1112,8);lfpo_resolve(s);}
 if(kind==1){uintptr_t basis=0;memcpy(&s->count,c+1120,4);memcpy(&basis,c+1136,8);camera_pose_read(basis,s->basis,64);s->batch_dt=s->dt;}
 for(unsigned i=lfpo_tls.depth-1;i>0;--i){LfpoScope*b=lfpo_tls.entries+i-1;if(b->kind==1&&b->raw==s->raw){s->parent=b->call;s->count=b->count;s->batch_dt=b->dt;memcpy(s->basis,b->basis,64);break;}}
 uint32_t csr=0;memcpy(&csr,c+24,4);if(kind!=2)lfpo_capture(s,0,csr);
}
static __attribute__((used,noinline)) ABI void lfpo_leave(const unsigned char*c){
#ifdef LFPO_TEST_CLOBBER
 __asm__ volatile("fninit");
 __asm__ volatile("vzeroall":::"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15");uint32_t badcsr=0x7f80;__asm__ volatile("ldmxcsr %0"::"m"(badcsr):"memory");
#endif
 if(!lfpo_enabled)return;if(lfpo_tls.overflow){--lfpo_tls.overflow;return;}if(!lfpo_tls.depth)return;
 LfpoScope*s=lfpo_tls.entries+lfpo_tls.depth-1;
 if(s->kind==2)for(unsigned i=lfpo_tls.depth-1;i>0;--i){LfpoScope*b=lfpo_tls.entries+i-1;if(b->kind==1&&b->raw==s->raw){s->index=++b->index;break;}}
 uint32_t csr=0;memcpy(&csr,c+24,4);lfpo_capture(s,1,csr);--lfpo_tls.depth;
}
static void lfpo_sbr_notify(const SbrRecord*r,unsigned phase){
 if(!lfpo_enabled||r->kind!=2)return;
 _Alignas(16)unsigned char fx[512];__asm__ volatile("fxsave64 %0":"=m"(fx)::"memory");
 lfpo_register(r);LfpoScope s;memset(&s,0,sizeof s);s.call=r->sequence;s.parent=r->parent_sequence;s.raw=r->raw;s.owner=r->owner;s.sdk=r->sdk;s.caller=r->caller;s.kind=5;s.dt=r->dt;
 camera_pose_read(r->owner+0x18,&s.count,4);camera_pose_read(r->owner+0x1c,&s.batch_dt,4);uint32_t csr;memcpy(&csr,fx+24,4);lfpo_capture(&s,phase,csr);
 __asm__ volatile("fxrstor64 %0"::"m"(fx):"memory");
}
/* The save/restore definitions below are generated verbatim from the tested
 * SDK_REBASE full-context bridge. All native originals execute exactly once. */
#define LFPO_SAVE "pushfq\n pushq %rax\n pushq %rbx\n pushq %rcx\n pushq %rdx\n pushq %rbp\n pushq %rsi\n pushq %rdi\n pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %r12\n pushq %r13\n pushq %r14\n pushq %r15\n subq $1048,%rsp\n fxsave64 0(%rsp)\n" \
 "vmovdqu %ymm0,512(%rsp)\n vmovdqu %ymm1,544(%rsp)\n vmovdqu %ymm2,576(%rsp)\n vmovdqu %ymm3,608(%rsp)\n vmovdqu %ymm4,640(%rsp)\n vmovdqu %ymm5,672(%rsp)\n vmovdqu %ymm6,704(%rsp)\n vmovdqu %ymm7,736(%rsp)\n vmovdqu %ymm8,768(%rsp)\n vmovdqu %ymm9,800(%rsp)\n vmovdqu %ymm10,832(%rsp)\n vmovdqu %ymm11,864(%rsp)\n vmovdqu %ymm12,896(%rsp)\n vmovdqu %ymm13,928(%rsp)\n vmovdqu %ymm14,960(%rsp)\n vmovdqu %ymm15,992(%rsp)\n"
#define LFPO_RESTORE "fxrstor64 0(%rsp)\n vmovdqu 512(%rsp),%ymm0\n vmovdqu 544(%rsp),%ymm1\n vmovdqu 576(%rsp),%ymm2\n vmovdqu 608(%rsp),%ymm3\n vmovdqu 640(%rsp),%ymm4\n vmovdqu 672(%rsp),%ymm5\n vmovdqu 704(%rsp),%ymm6\n vmovdqu 736(%rsp),%ymm7\n vmovdqu 768(%rsp),%ymm8\n vmovdqu 800(%rsp),%ymm9\n vmovdqu 832(%rsp),%ymm10\n vmovdqu 864(%rsp),%ymm11\n vmovdqu 896(%rsp),%ymm12\n vmovdqu 928(%rsp),%ymm13\n vmovdqu 960(%rsp),%ymm14\n vmovdqu 992(%rsp),%ymm15\n addq $1048,%rsp\n popq %r15\n popq %r14\n popq %r13\n popq %r12\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n popq %rdi\n popq %rsi\n popq %rbp\n popq %rdx\n popq %rcx\n popq %rbx\n popq %rax\n popfq\n"

static __attribute__((naked,used)) ABI uintptr_t lfpo_batch(void){__asm__ volatile(LFPO_SAVE "movq %rsp,%rdi\n movl $1,%esi\n callq lfpo_enter\n" LFPO_RESTORE "leaq -8(%rsp),%rsp\n callq *lfpo_original(%rip)\n leaq 8(%rsp),%rsp\n" LFPO_SAVE "movq %rsp,%rdi\n callq lfpo_leave\n" LFPO_RESTORE "retq\n");}
static __attribute__((naked,used)) ABI uintptr_t lfpo_inner(void){__asm__ volatile(LFPO_SAVE "movq %rsp,%rdi\n movl $2,%esi\n callq lfpo_enter\n" LFPO_RESTORE "leaq -8(%rsp),%rsp\n callq *lfpo_original+8(%rip)\n leaq 8(%rsp),%rsp\n" LFPO_SAVE "movq %rsp,%rdi\n callq lfpo_leave\n" LFPO_RESTORE "retq\n");}
static __attribute__((naked,used)) ABI uintptr_t lfpo_scalar(void){__asm__ volatile(LFPO_SAVE "movq %rsp,%rdi\n movl $3,%esi\n callq lfpo_enter\n" LFPO_RESTORE "leaq -8(%rsp),%rsp\n callq *lfpo_original+16(%rip)\n leaq 8(%rsp),%rsp\n" LFPO_SAVE "movq %rsp,%rdi\n callq lfpo_leave\n" LFPO_RESTORE "retq\n");}
/* HSQ remains SOLE owner of7CB150. These source-only callbacks bracket its
 * existing original call, before HSQ24 publication. No native call/hook added. */
static __attribute__((naked,used)) ABI void lfpo_hsq_before(void*owner,float dt){__asm__ volatile(LFPO_SAVE "movq %rsp,%rdi\n movl $4,%esi\n callq lfpo_enter\n" LFPO_RESTORE "retq\n");}
static __attribute__((naked,used)) ABI void lfpo_hsq_after(void*owner,float dt){__asm__ volatile(LFPO_SAVE "movq %rsp,%rdi\n callq lfpo_leave\n" LFPO_RESTORE "retq\n");}
#undef LFPO_SAVE
#undef LFPO_RESTORE
static void low_fps_physics_observer_install(void){
 wchar_t path[4096],rawpath[4100];DWORD n=GetEnvironmentVariableW(L"BB_LOW_FPS_PHYSICS_TRACE",path,4096);
 if(n<4||n>=4096||!((path[0]=='D'||path[0]=='d')&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')))return;
 if(lfpo_enabled)return;if(!sbr_enabled||!hsq_installed){puts("LowFPSPhysics: requires active existing SDK+HSQ observers; no new hooks/files");return;}
 wchar_t conflict[2];if(GetEnvironmentVariableW(L"BB_PHYSICS_SOURCE_TAPE",conflict,2)||GetEnvironmentVariableW(L"BB_PHYSICS_INNER_TAPE",conflict,2)){puts("LowFPSPhysics: old physics source/inner tape requested; no new hooks/files");return;}
 static const unsigned char publication[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xec,0x38,0xc5,0xfa,0x11,0x45,0xbc};
 unsigned char pubentry[12],pubgateway[18];uintptr_t pubtarget=0;
 if(!hsq_publication_original||!camera_pose_read(runtime_image_base+0x7cb150,pubentry,sizeof pubentry)||pubentry[0]!=0x48||pubentry[1]!=0xb8||pubentry[10]!=0xff||pubentry[11]!=0xe0||!camera_pose_read((uintptr_t)hsq_publication_original,pubgateway,sizeof pubgateway)||memcmp(pubgateway,publication,sizeof publication)){puts("LowFPSPhysics: active HSQ original publication chain unavailable; no hooks/files");return;}
 memcpy(&pubtarget,pubentry+2,8);if(pubtarget!=(uintptr_t)hsq_publication){puts("LowFPSPhysics: nativePUB owned by another wrapper; no hooks/files");return;}
 static const size_t sites[]={0x7c7dd0,0x7c78e0,0x7c9580};
 static const unsigned sizes[]={17,17,18};
 static const unsigned char signatures[3][18]={
  {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x78},
  {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x38},
  {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x50,0x49,0x89,0xf6,0xc5,0xfa,0x11,0x45,0xe4}};
 for(unsigned i=0;i<3;++i)if(runtime_image_size<sites[i]+sizes[i]||memcmp(image+sites[i],signatures[i],sizes[i])){puts("LowFPSPhysics: original signatures unavailable; no hooks/files");return;}
 unsigned char*code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!code)return;
 for(unsigned i=0;i<3;++i){memcpy(code+i*64,signatures[i],sizes[i]);lfpo_putjump(code+i*64+sizes[i],runtime_image_base+sites[i]+sizes[i]);}
 DWORD old;if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return;}
 wcscpy(rawpath,path);wcscat(rawpath,L".raw");lfpo_file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 lfpo_raw_file=CreateFileW(rawpath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(lfpo_file==INVALID_HANDLE_VALUE||lfpo_raw_file==INVALID_HANDLE_VALUE)goto fail;
 memset(&lfpo_header,0,sizeof lfpo_header);memcpy(lfpo_header.magic,"LFPHY001",8);lfpo_header.version=1;lfpo_header.record_size=sizeof(LfpoRecord);lfpo_header.capacity=1048576;lfpo_header.raw_capacity=UINT64_C(10)*1024*1024*1024;LARGE_INTEGER f;QueryPerformanceFrequency(&f);lfpo_header.qpc_frequency=f.QuadPart;
 if(!lfpo_header_write())goto fail;
 uintptr_t pages[3];DWORD previous[3];unsigned count=0;
 for(unsigned i=0;i<3;++i){uintptr_t page=(runtime_image_base+sites[i])&~(uintptr_t)4095;unsigned j=0;for(;j<count;++j)if(pages[j]==page)break;if(j==count)pages[count++]=page;}
 unsigned protected=0;for(;protected<count;++protected)if(!VirtualProtect((void*)pages[protected],4096,PAGE_EXECUTE_READWRITE,previous+protected))break;
 if(protected!=count){for(unsigned i=0;i<protected;++i)VirtualProtect((void*)pages[i],4096,previous[i],&old);goto fail;}
 const uintptr_t wrappers[]={(uintptr_t)lfpo_batch,(uintptr_t)lfpo_inner,(uintptr_t)lfpo_scalar};
 for(unsigned i=0;i<3;++i){lfpo_original[i]=(LfpoOriginal)(code+i*64);memset(image+sites[i],0x90,sizes[i]);lfpo_putjump(image+sites[i],wrappers[i]);}
 for(unsigned i=0;i<count;++i)VirtualProtect((void*)pages[i],4096,previous[i],&old);FlushInstructionCache(GetCurrentProcess(),code,4096);FlushInstructionCache(GetCurrentProcess(),image+0x7c7000,0x5000);
 lfpo_enabled=1;puts("LowFPSPhysics: optional read-only complete inner/scalar source capture enabled; no canonical timestamps synthesized");return;
fail:
 if(lfpo_file!=INVALID_HANDLE_VALUE)CloseHandle(lfpo_file);if(lfpo_raw_file!=INVALID_HANDLE_VALUE)CloseHandle(lfpo_raw_file);lfpo_file=lfpo_raw_file=INVALID_HANDLE_VALUE;VirtualFree(code,0,MEM_RELEASE);
}
