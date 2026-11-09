/* READ-ONLY event40 at existing native/normalized feedback input site.
 * No SDK getter, crr validator, normalization, query or native/source writes.
 * owner=Replay actor, peer=resolved raw physics state, caller=SDK body wrapper.
 * data0..15 full native actor+60 matrix;16..17 nativeT before read;
 * 18..19 nativeT after;20..21 provider;22..23 actual provider cursor.
 * record.dt is actual original frame-B0 dt. Frame/thread are CAMAUD metadata.
 * INT64_MIN/NaN/zero body explicitly mark unavailable observations. */
static int feedback_target_basis_enabled;
static int feedback_target_basis_requested(void){const char*p=getenv("BB_FEEDBACK_TARGET_BASIS");return p&&!strcmp(p,"1");}
static uintptr_t ftb_raw_state(uintptr_t vehicle){uintptr_t manager=0,owner=0,body=0;uint32_t handle=0;
 if(!camera_pose_read(vehicle+0x70,&manager,8)||!camera_pose_read(vehicle+0x18,&handle,4))return 0;
 int32_t index=(int32_t)(handle+UINT32_C(0xff0f3f01));
 if(!camera_pose_read(manager+(int64_t)index*0xb8+0x38,&owner,8)||!camera_pose_read(owner+8,&body,8))return 0;return body+0x2ad0;
}
static __attribute__((noinline,used)) ABI void feedback_target_basis_capture(void*frame,void*actor,void*vehicle){
 float data[24];for(unsigned i=0;i<16;++i){uint32_t nan=UINT32_C(0x7fc00000);memcpy(data+i,&nan,4);}memset(data+16,0,32);
 int64_t before=INT64_MIN,after=INT64_MIN,cursor=INT64_MIN;uintptr_t clock=0,provider=0;float dt=0;
 if(camera_pose_read((uintptr_t)image+0x1911100,&clock,8)&&camera_pose_read(clock+0x970,&clock,8)&&camera_pose_read(clock+0x1e0,&clock,8))camera_pose_read(clock+0x58,&before,8);
 camera_pose_read((uintptr_t)actor+0x60,data,64);camera_pose_read((uintptr_t)frame-0xb0,&dt,4);camera_pose_read((uintptr_t)frame-0xe0,&provider,8);camera_pose_read(provider+0xb0,&cursor,8);
 uintptr_t raw=ftb_raw_state((uintptr_t)vehicle);camera_pose_read(clock+0x58,&after,8);
 memcpy(data+16,&before,8);memcpy(data+18,&after,8);memcpy(data+20,&provider,8);memcpy(data+22,&cursor,8);
 camera_accuracy_log(40,actor,raw,(uintptr_t)vehicle,dt,data,24);
}
/* Normal SysV function entry is8-mod16. Save all nativecaller state before
 * calling any C/WinAPI/logger; helper is safe also inside the old normalized
 * bridge which did not save YMM upper halves or the complete FP environment. */
static __attribute__((naked,used)) ABI void feedback_target_basis_observe(void*frame,void*actor,void*vehicle){
 __asm__ volatile(
 "pushfq\n pushq %rax\n pushq %rcx\n pushq %rdx\n pushq %rbx\n pushq %rbp\n pushq %rsi\n pushq %rdi\n pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %r12\n pushq %r13\n pushq %r14\n pushq %r15\n subq $1032,%rsp\n fxsave64 0(%rsp)\n"
 "vmovdqu %ymm0,512(%rsp)\n vmovdqu %ymm1,544(%rsp)\n vmovdqu %ymm2,576(%rsp)\n vmovdqu %ymm3,608(%rsp)\n vmovdqu %ymm4,640(%rsp)\n vmovdqu %ymm5,672(%rsp)\n vmovdqu %ymm6,704(%rsp)\n vmovdqu %ymm7,736(%rsp)\n vmovdqu %ymm8,768(%rsp)\n vmovdqu %ymm9,800(%rsp)\n vmovdqu %ymm10,832(%rsp)\n vmovdqu %ymm11,864(%rsp)\n vmovdqu %ymm12,896(%rsp)\n vmovdqu %ymm13,928(%rsp)\n vmovdqu %ymm14,960(%rsp)\n vmovdqu %ymm15,992(%rsp)\n"
 "callq feedback_target_basis_capture\n fxrstor64 0(%rsp)\n"
 "vmovdqu 512(%rsp),%ymm0\n vmovdqu 544(%rsp),%ymm1\n vmovdqu 576(%rsp),%ymm2\n vmovdqu 608(%rsp),%ymm3\n vmovdqu 640(%rsp),%ymm4\n vmovdqu 672(%rsp),%ymm5\n vmovdqu 704(%rsp),%ymm6\n vmovdqu 736(%rsp),%ymm7\n vmovdqu 768(%rsp),%ymm8\n vmovdqu 800(%rsp),%ymm9\n vmovdqu 832(%rsp),%ymm10\n vmovdqu 864(%rsp),%ymm11\n vmovdqu 896(%rsp),%ymm12\n vmovdqu 928(%rsp),%ymm13\n vmovdqu 960(%rsp),%ymm14\n vmovdqu 992(%rsp),%ymm15\n"
 "addq $1032,%rsp\n popq %r15\n popq %r14\n popq %r13\n popq %r12\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n popq %rdi\n popq %rsi\n popq %rbp\n popq %rbx\n popq %rdx\n popq %rcx\n popq %rax\n popfq\n retq\n");
}
