/* Observe ACTUAL HUD transform builder output, not reconstructed spring input.
 * 478CB0 writes both matrices returned by native478F80/478F90. Original builder
 * is invoked once with original self. No getter, RNG or layout writes added.
 * Matrices are HUD-level transforms; this is not a GPU vertex-buffer capture. */
typedef uintptr_t(ABI*HudDisplayBuilder)(void*);
static HudDisplayBuilder hud_display_original;
static ABI uintptr_t hud_display_builder(void*self){
 uintptr_t result=hud_display_original(self);uint32_t csr;__asm__ volatile("stmxcsr %0":"=m"(csr));
 float data[24]={0};unsigned ok=camera_pose_read((uintptr_t)self+0x90,data,64);
 const unsigned offsets[]={0x1c,0x48,0x74};for(unsigned i=0;i<3;++i)ok=ok&&camera_pose_read((uintptr_t)self+offsets[i],data+16+i,4);data[19]=(float)ok;
 camera_accuracy_log(15,self,0,0x478cb0,0,data,24);
 memset(data,0,sizeof data);ok=camera_pose_read((uintptr_t)self+0xd0,data,64);data[19]=(float)ok;camera_accuracy_log(16,self,0,0x478cb0,0,data,24);
 __asm__ volatile("ldmxcsr %0"::"m"(csr):"memory");return result;
}
static int hud_display_trace_install(void){
 const char*requested=getenv("BB_HUD_DISPLAY_TRACE");if(!requested||strcmp(requested,"1"))return 0;
 static const unsigned char entry[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x81,0xec,0xd0,0,0,0};
 hud_display_original=(HudDisplayBuilder)camera_rate_hook(0x478cb0,entry,sizeof entry,(uintptr_t)hud_display_builder);
 if(hud_display_original)puts("HudDisplayTrace: read-only actual478CB0 output matrices90/D0, no extra builder/getter calls");return hud_display_original!=NULL;
}
