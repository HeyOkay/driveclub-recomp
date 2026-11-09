/* Observer43: EXACT native real20 selection atB9179B, not late HKT clock.
 * Replaces memory VCVTSI2SS by ONE MOV64 + register VCVTSI2SS of the same value;
 * original VMULSS constant, rounding and native MXCSR status are preserved.
 * Never changes the branch decision, selected duration or original result. */
static __attribute__((used))uintptr_t hds_scale,hds_resume;
static uint64_t hds_u64(const unsigned char*p){uint64_t x;memcpy(&x,p,8);return x;}
static void hds_jump(unsigned char*p,uintptr_t t){memcpy(p,"\xff\x25\0\0\0\0",6);memcpy(p+6,&t,8);}
static __attribute__((used,noinline))ABI void hds_capture(const unsigned char*c){
 uintptr_t self=hds_u64(c+1152),frame=hds_u64(c+1232),clock=hds_u64(c+1264),caller=0;int64_t selected;memcpy(&selected,c+1056,8);float converted,dt,original=0;memcpy(&converted,c+512,4);memcpy(&dt,c+544,4);unsigned char paused=0;uint32_t flags=0;
 if(camera_pose_read(frame-0xf0,&original,4))flags|=1;if(camera_pose_read(clock+0x40,&paused,1))flags|=2;if(camera_pose_read(frame+8,&caller,8)&&caller>=(uintptr_t)image&&caller<(uintptr_t)image+runtime_image_size)flags|=4;
 if(!memcmp(c+1024,c+1064,24)&&!memcmp(c+1048,c+1088,4)&&selected==(int64_t)hds_u64(c+1096))flags|=8;
 float data[24]={0};memcpy(data,&selected,8);memcpy(data+2,c+1024,8);memcpy(data+4,c+1064,8);memcpy(data+6,c+1032,8);memcpy(data+8,c+1072,8);memcpy(data+10,c+1040,8);memcpy(data+12,c+1080,8);memcpy(data+14,c+1048,4);memcpy(data+15,c+1088,4);data[16]=converted;data[17]=dt;data[18]=original;data[19]=(float)paused;memcpy(data+20,&flags,4);uint32_t thread=GetCurrentThreadId();memcpy(data+21,&thread,4);memcpy(data+22,&frame,8);
 camera_accuracy_log(43,(void*)self,clock,caller,dt,data,24);
}
#include "head-selector-bridge.inc"
static int hds_install(void){const char*e=camera_fix_getenv("BB_HEAD_CLOCK_SELECTOR");if(!e||strcmp(e,"1"))return 0;
 static const unsigned char sig[]={0xc4,0xe1,0xfa,0x2a,0x40,0x20,0xc5,0xfa,0x59,0x0d,0x5f,0xfe,0x7b,0x00};if(runtime_image_size<0xb9179b+sizeof sig||memcmp(image+0xb9179b,sig,sizeof sig))return 0;
 hds_scale=(uintptr_t)image+0x1351608;hds_resume=(uintptr_t)image+0xb917a9;DWORD old;if(!VirtualProtect(image+0xb9179b,sizeof sig,PAGE_EXECUTE_READWRITE,&old))return 0;hds_jump(image+0xb9179b,(uintptr_t)hds_bridge);DWORD unused;VirtualProtect(image+0xb9179b,sizeof sig,old,&unused);FlushInstructionCache(GetCurrentProcess(),image+0xb9179b,sizeof sig);puts("HeadClockSelector: event43 exact real20 integer + original converted dt at native selector; read-only clock, branch/result unchanged");return 1;
}
