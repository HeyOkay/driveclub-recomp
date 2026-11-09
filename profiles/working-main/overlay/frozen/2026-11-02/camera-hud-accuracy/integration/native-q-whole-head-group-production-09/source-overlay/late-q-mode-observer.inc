/* Optional READ-ONLY change-only native mode snapshots. No classification,
 * lifetime, source, native getter, physical or response policy is changed.
 * Original38 is an alternate Replay path gate, NOT a Source-mode boolean.
 */
typedef struct{LsqBinding binding;LsqSelector selector;uint64_t pointers[8];uint32_t flags_read;int known,status;unsigned char flags[16];} LsqmSnapshot;
static int lsqm_enabled;static SRWLOCK lsqm_lock=SRWLOCK_INIT;static LsqmSnapshot lsqm_latest[32];static unsigned char lsqm_valid[32];
static void lsqm_init(void){const char*p=getenv("BB_PRIVATE_LATE_Q_MODE_OBSERVER");lsqm_enabled=lsq_enabled&&p&&!strcmp(p,"1");if(lsqm_enabled)puts("PrivateLateQModeObserver: change-only read-only38/140/provider/nativepause/SDKflag snapshots; no mode classifier or lifetime change");}
static void lsqm_observe(const void*selector,const void*binding,int64_t actualT,int known,int status){
 if(!lsqm_enabled||!selector||!binding)return;uint32_t saved;__asm__ volatile("stmxcsr %0":"=m"(saved));
 const LsqSelector*x=selector;const LsqBinding*b=binding;LsqmSnapshot z;memset(&z,0,sizeof z);z.binding=*b;z.selector=*x;z.known=known;z.status=status;
 if(camera_pose_read(b->actor+0x38,z.flags,2))z.flags_read|=1;
 if(camera_pose_read(b->actor+0xe0,z.flags+2,5))z.flags_read|=2;
 if(camera_pose_read(b->actor+0x140,z.flags+7,1))z.flags_read|=4;
 uintptr_t root=0,service=0,system=0,clock=b->clock?b->clock:x->clock;uint32_t frame=~0u;int64_t dt=0;
 if(camera_pose_read((uintptr_t)image+0x1911100,&root,8)&&camera_pose_read(root+0x970,&service,8)&&camera_pose_read(service+0x670,&system,8)){
  z.pointers[3]=system;if(camera_pose_read(system+0x14,z.flags+8,1))z.flags_read|=8;
  if(!clock)camera_pose_read(service+0x1e0,&clock,8);
 }
 if(camera_pose_read(clock+0x80,z.flags+9,1))z.flags_read|=16;
 if(camera_pose_read(b->provider+0xcc,z.flags+10,3))z.flags_read|=32;
 uintptr_t manager=0,owner=0;uint32_t handle=0;
 if(camera_pose_read(b->vehicle+0x70,&manager,8)&&camera_pose_read(b->vehicle+0x18,&handle,4)){
  int32_t index=(int32_t)(handle+UINT32_C(0xff0f3f01));if(camera_pose_read(manager+(int64_t)index*0xb8+0x38,&owner,8)&&camera_pose_read(owner+0x96c,z.flags+13,1))z.flags_read|=64;
 }
 camera_pose_read(b->actor,z.pointers,8);camera_pose_read(b->provider,z.pointers+1,8);camera_pose_read(b->vehicle,z.pointers+2,8);camera_pose_read(system,z.pointers+4,8);
 camera_pose_read(z.pointers[4]+0x90,z.pointers+5,8);camera_pose_read(z.pointers[2]+0x3a8,z.pointers+6,8);camera_pose_read(z.pointers[0]+0x40,z.pointers+7,8);
 camera_pose_read(clock+0x21c,&frame,4);camera_pose_read(clock+0x60,&dt,8);
 int emit=0;AcquireSRWLockExclusive(&lsqm_lock);unsigned slot=32,empty=32;
 for(unsigned i=0;i<32;++i){if(lsqm_valid[i]&&!memcmp(&lsqm_latest[i].selector,x,sizeof*x)){slot=i;break;}if(!lsqm_valid[i]&&empty==32)empty=i;}
 if(slot==32)slot=empty;if(slot<32&&(!lsqm_valid[slot]||memcmp(lsqm_latest+slot,&z,sizeof z))){lsqm_latest[slot]=z;lsqm_valid[slot]=1;emit=1;}ReleaseSRWLockExclusive(&lsqm_lock);
 if(emit){LsqPoint p;memset(&p,0,sizeof p);p.binding=*b;if(!known){p.binding.owner=x->owner;p.binding.vehicle=x->vehicle;p.binding.clock=clock;}p.q=INT64_MIN;p.observed_render_T=actualT;p.observed_render_dt_us=dt;p.kind=1;p.mask=z.flags_read;p.field_observed_T[0]=known;p.field_observed_T[1]=status;p.field_observed_T[2]=frame;memcpy(p.vector,z.flags,16);memcpy(p.body_basis,z.pointers,64);lsq_trace(8,status,&p);}
 __asm__ volatile("ldmxcsr %0"::"m"(saved):"memory");
}
