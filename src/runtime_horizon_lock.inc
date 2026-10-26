/* Lock to horizon (BB_LOCK_HORIZON=1, the launcher's Driving tab): the in-car (driver's head) camera keeps the horizon
   level while the car rolls and pitches, as Assetto Corsa's option does. DriveClub 1.28, Windows x86-64 only.

   In the driver's head camera update (guest +0xb916d0, `this` in r15), at +0xb92420, +0x270..+0x2a0 hold the camera's
   world matrix the renderer draws from (just written by +0xb93510) and +0x2d0..+0x300 the car body's: rows of four
   floats, the x, y and z axis and the position, in metres; the axes point left, up and forward; world up is +y. Nothing
   later in the function writes +0x270 again. The camera's turn relative to the car (the head's look into corners, its
   sway, the seat's view down the road) is kept and put on the car levelled: its heading only, without roll and pitch.
   The position stays. The game's camera shake is added after this function and stays too. */
#ifdef _WIN32
#include <math.h>
static __attribute__((used)) uintptr_t horizon_lock_resume;
static void horizon_normalize(float v[3]) {
    const float n=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (n>1e-6f) { v[0]/=n; v[1]/=n; v[2]/=n; }
}
static __attribute__((used,noinline)) ABI void horizon_lock_compute(float *camera) {
    float *cam=camera+0x270/4;
    const float *car=camera+0x2d0/4;
    float row[3][3];   /* the car's axes, unit length */
    for (int i=0;i<3;++i) { for (int k=0;k<3;++k) row[i][k]=car[i*4+k]; horizon_normalize(row[i]); }
    /* The car levelled: its forward axis on the ground plane, up straight up, left from those two with the car's own
       handedness. A car pointing (nearly) straight up or down has no heading: left as it is. */
    float level[3][3]={{0,0,0},{0,1,0},{row[2][0],0,row[2][2]}};
    if (level[2][0]*level[2][0]+level[2][2]*level[2][2]<0.01f) return;
    horizon_normalize(level[2]);
    const float hand=(row[1][1]*row[2][2]-row[1][2]*row[2][1])*row[0][0]+(row[1][2]*row[2][0]-row[1][0]*row[2][2])*row[0][1]+
                     (row[1][0]*row[2][1]-row[1][1]*row[2][0])*row[0][2]<0.0f ? -1.0f : 1.0f;
    level[0][0]=hand*(level[1][1]*level[2][2]-level[1][2]*level[2][1]);
    level[0][1]=hand*(level[1][2]*level[2][0]-level[1][0]*level[2][2]);
    level[0][2]=hand*(level[1][0]*level[2][1]-level[1][1]*level[2][0]);
    /* the camera's rows in the car's axes, then on the levelled car */
    float rel[3][3], out[3][3];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) rel[i][j]=cam[i*4]*row[j][0]+cam[i*4+1]*row[j][1]+cam[i*4+2]*row[j][2];
    for (int i=0;i<3;++i) for (int k=0;k<3;++k) out[i][k]=rel[i][0]*level[0][k]+rel[i][1]*level[1][k]+rel[i][2]*level[2][k];
    for (int i=0;i<3;++i) for (int k=0;k<3;++k) cam[i*4+k]=out[i][k];
    static int logged;
    if (!logged) {
        logged=1;
        printf("Runtime: lock to horizon: first head camera, car up axis %.3f %.3f %.3f (world up is +y)\n",row[1][0],row[1][1],row[1][2]);
    }
}
/* Every register the game may still use and the flags are kept; then the two instructions the jump replaced. */
static __attribute__((naked,used)) ABI void horizon_lock_bridge(void) {
    __asm__ volatile(
        "pushfq\n"
        "pushq %rax\n"
        "pushq %rcx\n"
        "pushq %rdx\n"
        "pushq %rsi\n"
        "pushq %rdi\n"
        "pushq %r8\n"
        "pushq %r9\n"
        "pushq %r10\n"
        "pushq %r11\n"
        "subq $256, %rsp\n"
        "vmovdqu %xmm0, 0(%rsp)\n"
        "vmovdqu %xmm1, 16(%rsp)\n"
        "vmovdqu %xmm2, 32(%rsp)\n"
        "vmovdqu %xmm3, 48(%rsp)\n"
        "vmovdqu %xmm4, 64(%rsp)\n"
        "vmovdqu %xmm5, 80(%rsp)\n"
        "vmovdqu %xmm6, 96(%rsp)\n"
        "vmovdqu %xmm7, 112(%rsp)\n"
        "vmovdqu %xmm8, 128(%rsp)\n"
        "vmovdqu %xmm9, 144(%rsp)\n"
        "vmovdqu %xmm10, 160(%rsp)\n"
        "vmovdqu %xmm11, 176(%rsp)\n"
        "vmovdqu %xmm12, 192(%rsp)\n"
        "vmovdqu %xmm13, 208(%rsp)\n"
        "vmovdqu %xmm14, 224(%rsp)\n"
        "vmovdqu %xmm15, 240(%rsp)\n"
        "movq %r15, %rdi\n"
        "callq horizon_lock_compute\n"
        "vmovdqu 0(%rsp), %xmm0\n"
        "vmovdqu 16(%rsp), %xmm1\n"
        "vmovdqu 32(%rsp), %xmm2\n"
        "vmovdqu 48(%rsp), %xmm3\n"
        "vmovdqu 64(%rsp), %xmm4\n"
        "vmovdqu 80(%rsp), %xmm5\n"
        "vmovdqu 96(%rsp), %xmm6\n"
        "vmovdqu 112(%rsp), %xmm7\n"
        "vmovdqu 128(%rsp), %xmm8\n"
        "vmovdqu 144(%rsp), %xmm9\n"
        "vmovdqu 160(%rsp), %xmm10\n"
        "vmovdqu 176(%rsp), %xmm11\n"
        "vmovdqu 192(%rsp), %xmm12\n"
        "vmovdqu 208(%rsp), %xmm13\n"
        "vmovdqu 224(%rsp), %xmm14\n"
        "vmovdqu 240(%rsp), %xmm15\n"
        "addq $256, %rsp\n"
        "popq %r11\n"
        "popq %r10\n"
        "popq %r9\n"
        "popq %r8\n"
        "popq %rdi\n"
        "popq %rsi\n"
        "popq %rdx\n"
        "popq %rcx\n"
        "popq %rax\n"
        "popfq\n"
        "vmovdqa 0x310(%r15), %xmm0\n"
        "vpshufd $0, %xmm0, %xmm1\n"
        "jmpq *horizon_lock_resume(%rip)\n"
    );
}
static void horizon_lock_install(void) {
    const char *enabled=getenv("BB_LOCK_HORIZON");
    if (!enabled || strcmp(enabled,"1")) return;
    static const unsigned char expected[14]={0xc4,0xc1,0x79,0x6f,0x87,0x10,0x03,0x00,0x00,   /* vmovdqa xmm0,[r15+0x310] */
                                             0xc5,0xf9,0x70,0xc8,0x00};                     /* vpshufd xmm1,xmm0,0 */
    if (runtime_image_size<0xb92420+sizeof expected || memcmp(image+0xb92420,expected,sizeof expected)) {
        puts("Runtime: lock to horizon does not fit this game version; off");
        return;
    }
    /* jmp [rip] with the bridge's address after it: the 14 bytes of the two instructions, no register used */
    horizon_lock_resume=(uintptr_t)image+0xb92420+sizeof expected;
    const uintptr_t target=(uintptr_t)horizon_lock_bridge;
    static const unsigned char jump[6]={0xff,0x25,0,0,0,0};
    memcpy(image+0xb92420,jump,6); memcpy(image+0xb92426,&target,8);
    FlushInstructionCache(GetCurrentProcess(),image+0xb92420,sizeof expected);
    puts("Runtime: lock to horizon on (the in-car camera keeps the horizon level)");
}
#else
static void horizon_lock_install(void) {}
#endif
