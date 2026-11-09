/* PRIVATE bounded read-only final CPU skinning-palette commit observer.
 * Model mapping is exclusively from observed GForce Player+Driver components.
 * No renderer/getter call is added. This captures CPU palette submission data,
 * not a readback of GPU vertices or the finished image. */
typedef uintptr_t(ABI*DriverPaletteCommit)(void*);
static __attribute__((used)) DriverPaletteCommit driver_palette_original;
typedef struct {uintptr_t component,entity,model,owner,vehicle;uint32_t entity_id;} DriverPaletteOwner;
static DriverPaletteOwner driver_palette_owners[64];static SRWLOCK driver_palette_owner_lock=SRWLOCK_INIT,driver_palette_file_lock=SRWLOCK_INIT;
static volatile LONG64 driver_palette_epoch=1;static volatile LONG driver_palette_sequence;
static int driver_palette_enabled;static wchar_t driver_palette_path[4096];
typedef struct {unsigned char magic[8];uint32_t version,record_size,bone_count,capacity,count,dropped,failed,reserved;uint64_t epoch,qpc_frequency,reserved64;} DriverPaletteHeader;
typedef struct {uint64_t qpc_us,epoch,binding,model,entity,owner,vehicle,state;int64_t time,clock_dt;uint32_t frame_start,frame_end,counter218_start,counter218_end,thread,flags,bone_count,sequence;float model_root[16],car_pose[16];} DriverPaletteRecord;
_Static_assert(sizeof(DriverPaletteHeader)==64,"driver palette header64");
_Static_assert(sizeof(DriverPaletteRecord)==240,"driver palette record prefix240");
static DriverPaletteHeader driver_palette_header;static HANDLE driver_palette_file=INVALID_HANDLE_VALUE;
static void driver_palette_discontinuity(void){InterlockedIncrement64(&driver_palette_epoch);}
static uint64_t driver_palette_qpc_us(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (uint64_t)(t.QuadPart/f.QuadPart)*1000000u+(uint64_t)(t.QuadPart%f.QuadPart)*1000000u/(uint64_t)f.QuadPart;}
static int driver_palette_clock(uintptr_t*clock,int64_t*time,int64_t*dt,uint32_t*a,uint32_t*b){
 uintptr_t p=0;if(!camera_pose_read(runtime_image_base+0x1911100,&p,8)||!camera_pose_read(p+0x970,&p,8)||!camera_pose_read(p+0x1e0,&p,8)||!camera_pose_read(p+0x58,time,8)||!camera_pose_read(p+0x60,dt,8)||!camera_pose_read(p+0x218,a,4)||!camera_pose_read(p+0x21c,b,4))return 0;*clock=p;return 1;
}
static void driver_palette_register(void*self){
 if(!driver_palette_enabled)return;
 DriverPaletteOwner current={0};current.component=(uintptr_t)self;char name[64]={0};
 if(!camera_pose_read((uintptr_t)self+8,&current.entity,8)||!camera_pose_read((uintptr_t)self+0x18,&current.model,8)||!current.model||!camera_pose_read((uintptr_t)self+0x20,&current.owner,8)||!camera_pose_read(current.entity+0x18,&current.entity_id,4)||!camera_pose_read(current.entity+0x2a,name,63)||memcmp(name,"Player+Driver",13))return;
 AcquireSRWLockShared(&driver_palette_owner_lock);
 for(unsigned i=0;i<64;++i){DriverPaletteOwner*p=driver_palette_owners+i;if(p->component==current.component&&p->entity==current.entity&&p->entity_id==current.entity_id&&p->model==current.model&&p->owner==current.owner&&p->vehicle){ReleaseSRWLockShared(&driver_palette_owner_lock);return;}}
 ReleaseSRWLockShared(&driver_palette_owner_lock);
 uintptr_t registry=0,manager=0,sentinel=0,node=0;
 if(!camera_pose_read(runtime_image_base+0x1911100,&registry,8)||!camera_pose_read(registry+0x970,&registry,8)||!camera_pose_read(registry+0x2d0,&manager,8)||!camera_pose_read(manager+0x18,&sentinel,8)||!camera_pose_read(sentinel,&node,8))return;
 for(unsigned i=0;i<128&&node&&node!=sentinel;++i){uintptr_t component=0,entity=0;if(!camera_pose_read(node+0x18,&component,8)||!camera_pose_read(component+8,&entity,8))return;if(entity==current.owner){if(!camera_pose_read(component+0x50,&current.vehicle,8))return;break;}if(!camera_pose_read(node,&node,8))return;}
 if(!current.vehicle)return;
 AcquireSRWLockExclusive(&driver_palette_owner_lock);DriverPaletteOwner*slot=NULL;
 for(unsigned i=0;i<64;++i)if(driver_palette_owners[i].component==current.component){slot=driver_palette_owners+i;break;}
 if(!slot)for(unsigned i=0;i<64;++i)if(!driver_palette_owners[i].component){slot=driver_palette_owners+i;break;}
 if(slot){*slot=current;driver_palette_discontinuity();}ReleaseSRWLockExclusive(&driver_palette_owner_lock);
}
static int driver_palette_header_write(void){
 LARGE_INTEGER offset;offset.QuadPart=0;DWORD got=0;return SetFilePointerEx(driver_palette_file,offset,NULL,FILE_BEGIN)&&WriteFile(driver_palette_file,&driver_palette_header,sizeof driver_palette_header,&got,NULL)&&got==sizeof driver_palette_header;
}
static void driver_palette_fail(void){AcquireSRWLockExclusive(&driver_palette_file_lock);++driver_palette_header.failed;ReleaseSRWLockExclusive(&driver_palette_file_lock);}
static __attribute__((used,noinline)) ABI void driver_palette_capture(void*binding){
#ifdef DRIVER_PALETTE_TEST_CLOBBER
 __asm__ volatile("vzeroall":::"ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15");uint32_t testcsr=0x7f80;__asm__ volatile("ldmxcsr %0"::"m"(testcsr):"memory");
#endif
 if(!driver_palette_enabled)return;
 uintptr_t model=0,instance=0,header=0,palette=0;
 if(!camera_pose_read((uintptr_t)binding,&model,8)||!camera_pose_read((uintptr_t)binding+8,&instance,8)||!instance||!camera_pose_read((uintptr_t)binding+0x10,&header,8)||!header||!camera_pose_read(header,&palette,8)||!palette)return;
 DriverPaletteOwner mapped={0};AcquireSRWLockShared(&driver_palette_owner_lock);for(unsigned i=0;i<64;++i)if(driver_palette_owners[i].model==model){mapped=driver_palette_owners[i];break;}ReleaseSRWLockShared(&driver_palette_owner_lock);if(!mapped.component)return;
 uintptr_t current_entity=0,current_model=0,current_owner=0;uint32_t current_id=0;
 if(!camera_pose_read(mapped.component+8,&current_entity,8)||!camera_pose_read(mapped.component+0x18,&current_model,8)||!camera_pose_read(mapped.component+0x20,&current_owner,8)||!camera_pose_read(current_entity+0x18,&current_id,4)||current_entity!=mapped.entity||current_model!=mapped.model||current_owner!=mapped.owner||current_id!=mapped.entity_id){driver_palette_fail();return;}
 uintptr_t wrapper=0,resource=0,physics=0,state=0,clock=0;uint16_t bones=0;unsigned char dirty=0;
 if(!camera_pose_read(model+0xd8,&wrapper,8)||!camera_pose_read(wrapper,&resource,8)||!camera_pose_read(resource+0x10,&bones,2)||!bones||bones>512||!camera_pose_read(header+0xc,&dirty,1)||dirty!=1||!camera_pose_read(mapped.vehicle+0x68,&physics,8)||!camera_pose_read(physics+0x10,&state,8)||!state){driver_palette_fail();return;}
 _Alignas(16)unsigned char bytes[sizeof(DriverPaletteRecord)+512*48];DriverPaletteRecord*record=(DriverPaletteRecord*)bytes;memset(record,0,sizeof*record);
 record->qpc_us=driver_palette_qpc_us();record->epoch=(uint64_t)InterlockedCompareExchange64(&driver_palette_epoch,0,0);record->binding=(uintptr_t)binding;record->model=model;record->entity=mapped.entity;record->owner=mapped.owner;record->vehicle=mapped.vehicle;record->state=state;record->thread=GetCurrentThreadId();record->bone_count=bones;record->sequence=(uint32_t)InterlockedIncrement(&driver_palette_sequence);
 if(!driver_palette_clock(&clock,&record->time,&record->clock_dt,&record->counter218_start,&record->frame_start)||!camera_pose_read(model+0x10,record->model_root,64)||!camera_pose_read(state+0x310,record->car_pose,64)||!camera_pose_read(palette,bytes+sizeof*record,(size_t)bones*48)){driver_palette_fail();return;}record->flags=1;
 int64_t end_time=0,end_dt=0;uintptr_t end_clock=0;float root_again[16],car_again[16];
 if(driver_palette_clock(&end_clock,&end_time,&end_dt,&record->counter218_end,&record->frame_end)&&end_clock==clock&&end_time==record->time&&record->frame_end==record->frame_start&&record->counter218_end==record->counter218_start)record->flags|=2;
 if(camera_pose_read(model+0x10,root_again,64)&&!memcmp(root_again,record->model_root,64))record->flags|=4;
 if(camera_pose_read(state+0x310,car_again,64)&&!memcmp(car_again,record->car_pose,64))record->flags|=8;
 if(record->epoch==(uint64_t)InterlockedCompareExchange64(&driver_palette_epoch,0,0))record->flags|=16;
 AcquireSRWLockExclusive(&driver_palette_file_lock);
 if(driver_palette_file==INVALID_HANDLE_VALUE){
  driver_palette_file=CreateFileW(driver_palette_path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(driver_palette_file==INVALID_HANDLE_VALUE){driver_palette_enabled=0;ReleaseSRWLockExclusive(&driver_palette_file_lock);return;}
  LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);memcpy(driver_palette_header.magic,"DRVPAL01",8);driver_palette_header.version=1;driver_palette_header.record_size=(uint32_t)(sizeof*record+(size_t)bones*48);driver_palette_header.bone_count=bones;driver_palette_header.capacity=131072;driver_palette_header.qpc_frequency=(uint64_t)frequency.QuadPart;
 }
 if(driver_palette_header.count>=driver_palette_header.capacity||bones!=driver_palette_header.bone_count){++driver_palette_header.dropped;driver_palette_header_write();ReleaseSRWLockExclusive(&driver_palette_file_lock);return;}
 LARGE_INTEGER offset;offset.QuadPart=(LONGLONG)sizeof(DriverPaletteHeader)+(LONGLONG)driver_palette_header.count*driver_palette_header.record_size;DWORD got=0;
 if(SetFilePointerEx(driver_palette_file,offset,NULL,FILE_BEGIN)&&WriteFile(driver_palette_file,bytes,driver_palette_header.record_size,&got,NULL)&&got==driver_palette_header.record_size)++driver_palette_header.count;else++driver_palette_header.failed;
 driver_palette_header.epoch=record->epoch;driver_palette_header_write();ReleaseSRWLockExclusive(&driver_palette_file_lock);
}
/* Exact post-native register preservation wrapper is appended below. */
static __attribute__((naked,used)) ABI uintptr_t driver_palette_commit(void*binding){
 __asm__ volatile(
  "subq $8,%rsp\n"
  "movq %rdi,0(%rsp)\n"
  "callq *driver_palette_original(%rip)\n"
  "pushfq\n"
  "pushq %rax\n"
  "pushq %rbx\n"
  "pushq %rcx\n"
  "pushq %rdx\n"
  "pushq %rbp\n"
  "pushq %rsi\n"
  "pushq %rdi\n"
  "pushq %r8\n"
  "pushq %r9\n"
  "pushq %r10\n"
  "pushq %r11\n"
  "pushq %r12\n"
  "pushq %r13\n"
  "pushq %r14\n"
  "pushq %r15\n"
  "subq $528,%rsp\n"
  "vmovdqu %ymm0,0(%rsp)\n"
  "vmovdqu %ymm1,32(%rsp)\n"
  "vmovdqu %ymm2,64(%rsp)\n"
  "vmovdqu %ymm3,96(%rsp)\n"
  "vmovdqu %ymm4,128(%rsp)\n"
  "vmovdqu %ymm5,160(%rsp)\n"
  "vmovdqu %ymm6,192(%rsp)\n"
  "vmovdqu %ymm7,224(%rsp)\n"
  "vmovdqu %ymm8,256(%rsp)\n"
  "vmovdqu %ymm9,288(%rsp)\n"
  "vmovdqu %ymm10,320(%rsp)\n"
  "vmovdqu %ymm11,352(%rsp)\n"
  "vmovdqu %ymm12,384(%rsp)\n"
  "vmovdqu %ymm13,416(%rsp)\n"
  "vmovdqu %ymm14,448(%rsp)\n"
  "vmovdqu %ymm15,480(%rsp)\n"
  "stmxcsr 512(%rsp)\n"
  "movq 656(%rsp),%rdi\n"
  "callq driver_palette_capture\n"
  "ldmxcsr 512(%rsp)\n"
  "vmovdqu 0(%rsp),%ymm0\n"
  "vmovdqu 32(%rsp),%ymm1\n"
  "vmovdqu 64(%rsp),%ymm2\n"
  "vmovdqu 96(%rsp),%ymm3\n"
  "vmovdqu 128(%rsp),%ymm4\n"
  "vmovdqu 160(%rsp),%ymm5\n"
  "vmovdqu 192(%rsp),%ymm6\n"
  "vmovdqu 224(%rsp),%ymm7\n"
  "vmovdqu 256(%rsp),%ymm8\n"
  "vmovdqu 288(%rsp),%ymm9\n"
  "vmovdqu 320(%rsp),%ymm10\n"
  "vmovdqu 352(%rsp),%ymm11\n"
  "vmovdqu 384(%rsp),%ymm12\n"
  "vmovdqu 416(%rsp),%ymm13\n"
  "vmovdqu 448(%rsp),%ymm14\n"
  "vmovdqu 480(%rsp),%ymm15\n"
  "addq $528,%rsp\n"
  "popq %r15\n"
  "popq %r14\n"
  "popq %r13\n"
  "popq %r12\n"
  "popq %r11\n"
  "popq %r10\n"
  "popq %r9\n"
  "popq %r8\n"
  "popq %rdi\n"
  "popq %rsi\n"
  "popq %rbp\n"
  "popq %rdx\n"
  "popq %rcx\n"
  "popq %rbx\n"
  "popq %rax\n"
  "popfq\n"
  "leaq 8(%rsp),%rsp\n"
  "retq\n"
 );
}

static void driver_palette_install(void){
 DWORD count=GetEnvironmentVariableW(L"BB_DRIVER_PALETTE_TRACE",driver_palette_path,4096);
 if(count<4||count>=4096||!((driver_palette_path[0]=='D'||driver_palette_path[0]=='d')&&driver_palette_path[1]==':'&&(driver_palette_path[2]=='\\'||driver_palette_path[2]=='/')))return;
 static const unsigned char entry[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x58};
 driver_palette_original=(DriverPaletteCommit)camera_rate_hook(0xc9e500,entry,sizeof entry,(uintptr_t)driver_palette_commit);
 if(driver_palette_original){driver_palette_enabled=1;puts("DriverPaletteTrace: actual nativeC9E500 postcommit, observedPlayer+Driver only; boundedD-only compactpalette+model/carpose, no native writes");}
}
