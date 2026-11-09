/* READ-ONLY actual v250 return at123CE1. Semantic name UNKNOWN.
 * No additional SDK getter and no scalar/branch/matrix/controller changes.
 *42data0=actualXMM0scalar;1..4nativecorrection;5..8nativeposerror;
 *9..12SDKmean;13..14nativeT;15..16provider;17..18cursor;
 *19..21actual nativeprojectionaxis;22MXCSRbits;23completeReadFlag.
 *owner=actor,peer=actualSDKbody,caller=123CE1,dt=frameB0. */
static __attribute__((used)) uintptr_t fbs_gateway;
static __attribute__((used,noinline)) ABI void fbs_capture(const unsigned char*context){
 uintptr_t frame=0,actor=0,body=0,axis=0,provider=0,clock=0;float dt=0,data[24]={0};int64_t T=INT64_MIN,cursor=INT64_MIN;
 memcpy(&frame,context+1104,8);memcpy(&actor,context+1048,8);memcpy(&body,context+1112,8);memcpy(&axis,context+1032,8);memcpy(data,context+512,4);memcpy(data+22,context+24,4);
 int valid=camera_pose_read(frame-0xb0,&dt,4)&&camera_pose_read(frame-0xc0,data+1,16)&&camera_pose_read(frame-0xd0,data+5,16)&&camera_pose_read(frame-0x50,data+9,16)&&camera_pose_read(axis+0x10,data+19,12)&&camera_pose_read(frame-0xe0,&provider,8);
 if(camera_pose_read((uintptr_t)image+0x1911100,&clock,8)&&camera_pose_read(clock+0x970,&clock,8)&&camera_pose_read(clock+0x1e0,&clock,8))camera_pose_read(clock+0x58,&T,8);camera_pose_read(provider+0xb0,&cursor,8);
 memcpy(data+13,&T,8);memcpy(data+15,&provider,8);memcpy(data+17,&cursor,8);data[23]=(float)valid;camera_accuracy_log(42,(void*)actor,body,0x123ce1,dt,data,24);
}
static __attribute__((naked,used)) ABI void fbs_bridge(void){__asm__ volatile(
 "pushfq\n"
 "pushq %rax\n"
 "pushq %rcx\n"
 "pushq %rdx\n"
 "pushq %rbx\n"
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
 "subq $1024,%rsp\n"
 "fxsave64 0(%rsp)\n"
 "vmovdqu %ymm0,512(%rsp)\n"
 "vmovdqu %ymm1,544(%rsp)\n"
 "vmovdqu %ymm2,576(%rsp)\n"
 "vmovdqu %ymm3,608(%rsp)\n"
 "vmovdqu %ymm4,640(%rsp)\n"
 "vmovdqu %ymm5,672(%rsp)\n"
 "vmovdqu %ymm6,704(%rsp)\n"
 "vmovdqu %ymm7,736(%rsp)\n"
 "vmovdqu %ymm8,768(%rsp)\n"
 "vmovdqu %ymm9,800(%rsp)\n"
 "vmovdqu %ymm10,832(%rsp)\n"
 "vmovdqu %ymm11,864(%rsp)\n"
 "vmovdqu %ymm12,896(%rsp)\n"
 "vmovdqu %ymm13,928(%rsp)\n"
 "vmovdqu %ymm14,960(%rsp)\n"
 "vmovdqu %ymm15,992(%rsp)\n"
 "movq %rsp,%rdi\n"
 "callq fbs_capture\n"
 "fxrstor64 0(%rsp)\n"
 "vmovdqu 512(%rsp),%ymm0\n"
 "vmovdqu 544(%rsp),%ymm1\n"
 "vmovdqu 576(%rsp),%ymm2\n"
 "vmovdqu 608(%rsp),%ymm3\n"
 "vmovdqu 640(%rsp),%ymm4\n"
 "vmovdqu 672(%rsp),%ymm5\n"
 "vmovdqu 704(%rsp),%ymm6\n"
 "vmovdqu 736(%rsp),%ymm7\n"
 "vmovdqu 768(%rsp),%ymm8\n"
 "vmovdqu 800(%rsp),%ymm9\n"
 "vmovdqu 832(%rsp),%ymm10\n"
 "vmovdqu 864(%rsp),%ymm11\n"
 "vmovdqu 896(%rsp),%ymm12\n"
 "vmovdqu 928(%rsp),%ymm13\n"
 "vmovdqu 960(%rsp),%ymm14\n"
 "vmovdqu 992(%rsp),%ymm15\n"
 "addq $1024,%rsp\n"
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
 "popq %rbx\n"
 "popq %rdx\n"
 "popq %rcx\n"
 "popq %rax\n"
 "popfq\n"
 "jmpq *fbs_gateway(%rip)\n"
);}

static int feedback_branch_scalar_install(void){
 const char*p=getenv("BB_FEEDBACK_BRANCH_SCALAR");if(!p||strcmp(p,"1"))return 0;
 static const unsigned char expected[]={0xc5,0xf0,0x57,0xc9,0xc5,0xf8,0x2e,0xc1,0x77,0x3e,0xc4,0xc1,0x78,0x28,0x46,0x10};
 if(runtime_image_size<0x123cf1||memcmp(image+0x123ce1,expected,16)){puts("FeedbackScalar42: original branch signature unavailable; no hook");return 0;}
 unsigned char*code=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!code)return 0;const unsigned char jump[6]={0xff,0x25,0,0,0,0};
 memcpy(code,expected,8);const unsigned char ja[]={0x0f,0x87,20,0,0,0};memcpy(code+8,ja,6);memcpy(code+14,expected+10,6);memcpy(code+20,jump,6);uintptr_t normal=(uintptr_t)image+0x123cf1;memcpy(code+26,&normal,8);memcpy(code+34,jump,6);uintptr_t positive=(uintptr_t)image+0x123d29;memcpy(code+40,&positive,8);fbs_gateway=(uintptr_t)code;
 uintptr_t target=(uintptr_t)fbs_bridge;memcpy(image+0x123ce1,jump,6);memcpy(image+0x123ce7,&target,8);memset(image+0x123cef,0x90,2);FlushInstructionCache(GetCurrentProcess(),code,48);FlushInstructionCache(GetCurrentProcess(),image+0x123ce1,16);puts("FeedbackScalar42: actual original SDKv250 return; branch unchanged, no extra getter");return 1;
}
