#ifndef CAMERA_HEAD_LIVE_CORE_INCLUDED
#define CAMERA_HEAD_LIVE_CORE_INCLUDED
#include "shared-phase-clock-core.inc"
/* Native coupled torso/head transaction at authored33333us. Only the64-byte
 * coupled state is persistent. World composition/animation and physics still
 * run once per real render. Input between observations is an explicit sampled
 * model (linear velocity, rigid shortest-arc basis), not unobserved PS4 truth.
 * HKTInputs/HKTRecord, crs_native_us, crr quaternion helpers precede this file. */
typedef struct {
 uintptr_t owner,vehicle;uint32_t frame;unsigned valid,adapted;
 int64_t credit_us,canonical_us,time_us;uintptr_t clock;unsigned primed;uint64_t ticks,resets;
 unsigned char canonical[64],display[64];
 float linear[4],angular[4],q_linear[4],q_angular[4],matrix[16];
} HCLState;
typedef int(*HCLStep)(void*,unsigned char[0x380],const HKTInputs*,uint32_t,uint32_t*);
enum {HCL_NATIVE=0,HCL_APPLIED=1,HCL_UNSUPPORTED=-1,HCL_BACKEND_FAILED=-2};
static void hcl_invalidate(HCLState*s){uint64_t resets=s->resets+1;memset(s,0,sizeof *s);s->resets=resets;}
static int hcl_finite(const void*p,unsigned n){const unsigned char*b=p;for(unsigned i=0;i<n;++i){float x;memcpy(&x,b+4*i,4);if(!isfinite(x))return 0;}return 1;}
static void hcl_basis(const float*a,const float*b,double u,float*out){
 memcpy(out,b,64);
 /* A held native basis must remain BITWISE held. Reconstructing its quaternion
    would needlessly normalize native fast-trig roundoff at an interior Q. */
 if(u>=1||(!memcmp(a,b,12)&&!memcmp(a+4,b+4,12)&&!memcmp(a+8,b+8,12)))return;
 float qa[4],qb[4],q[4];crr_rigid_quaternion(a,qa);crr_rigid_quaternion(b,qb);crr_slerp(qa,qb,u,q);
 double x=q[0],y=q[1],z=q[2],w=q[3];
 out[0]=(float)(1-2*(y*y+z*z));out[1]=(float)(2*(x*y-z*w));out[2]=(float)(2*(x*z+y*w));
 out[4]=(float)(2*(x*y+z*w));out[5]=(float)(1-2*(x*x+z*z));out[6]=(float)(2*(y*z-x*w));
 out[8]=(float)(2*(x*z-y*w));out[9]=(float)(2*(y*z+x*w));out[10]=(float)(1-2*(x*x+y*y));
}
static void hcl_input(HKTInputs*out,const HKTRecord*r,const HCLState*s,int64_t duration_us,double u){
 *out=r->input;float dt=(float)duration_us*1e-6f;out->dt[0]=dt;
 volatile float inverse=1.f/dt;
 for(unsigned j=0;j<4;++j){
  out->broadcast_dt[j]=dt;out->inverse[j]=inverse;
  if(u>=1){out->current_linear[j]=r->input.current_linear[j];out->current_angular[j]=r->input.current_angular[j];}
  else {out->current_linear[j]=(float)((double)s->linear[j]+((double)r->input.current_linear[j]-s->linear[j])*u);out->current_angular[j]=(float)((double)s->angular[j]+((double)r->input.current_angular[j]-s->angular[j])*u);}
  volatile float dl=out->current_linear[j]-s->q_linear[j],da=out->current_angular[j]-s->q_angular[j];
  out->delta_linear[j]=dl;out->delta_angular[j]=da;
 }
 hcl_basis(s->matrix,r->input.matrix,u,out->matrix);
}
static void hcl_remember(HCLState*s,const HKTRecord*r,const unsigned char*display,const HCSClock*clock){
 s->clock=clock->clock;s->time_us=clock->time_us;
 s->owner=(uintptr_t)r->actor;s->vehicle=(uintptr_t)r->vehicle;s->frame=r->frame;s->valid=1;
 memcpy(s->linear,r->input.current_linear,16);memcpy(s->angular,r->input.current_angular,16);memcpy(s->matrix,r->input.matrix,64);memcpy(s->display,display,64);
}
static int hcl_update(HCLState*state,const HKTRecord*r,HCLStep step,void*context,unsigned char output[64],const HCSClock*clock){
 int64_t us;float q[4];
 if(!clock||!clock->valid){hcl_invalidate(state);return HCL_UNSUPPORTED;}
 if((r->flags&(HKT_READ_COMPLETE|HKT_FRAME_VALID))!=(HKT_READ_COMPLETE|HKT_FRAME_VALID)||
    !crs_native_us(r->input.dt[0],&us)||us>8*33333||us!=clock->dt_us||r->frame!=clock->frame||!hcl_finite(r->before+0x1a0,16)||
    !hcl_finite(r->input.current_linear,4)||!hcl_finite(r->input.current_angular,4)||!crr_rigid_quaternion(r->input.matrix,q)){
  hcl_invalidate(state);return HCL_UNSUPPORTED;
 }
 HCLState s=*state;
 int continuous=s.valid&&s.owner==(uintptr_t)r->actor&&s.vehicle==(uintptr_t)r->vehicle&&s.clock==clock->clock&&s.time_us+us==clock->time_us&&r->frame==s.frame+1u&&!memcmp(s.display,r->before+0x1a0,64);
 if(!continuous){hcl_invalidate(&s);memcpy(s.canonical,r->before+0x1a0,64);memcpy(s.q_linear,r->before+0x130,16);memcpy(s.q_angular,r->before+0x140,16);memcpy(s.linear,s.q_linear,16);memcpy(s.angular,s.q_angular,16);memcpy(s.matrix,r->input.matrix,64);s.canonical_us=clock->time_us-us;s.primed=s.canonical_us%33333==0;}
 /* Exact unmodified30FPS path, including resets and its native FP flags. A Q
    stall in an established VRR epoch is deliberately NOT taken through here. */
 if(!s.adapted&&us==33333&&s.credit_us==0){
  memcpy(s.canonical,r->after+0x1a0,64);memcpy(s.q_linear,r->input.current_linear,16);memcpy(s.q_angular,r->input.current_angular,16);s.canonical_us=clock->time_us;s.primed=s.canonical_us%33333==0;++s.ticks;
  hcl_remember(&s,r,r->after+0x1a0,clock);*state=s;return HCL_NATIVE;
 }
 s.adapted=1;int64_t step_us=33333-s.canonical_us%33333;
 _Alignas(16)unsigned char actor[0x380];
 while(s.canonical_us+step_us<=clock->time_us){
  int64_t endpoint=s.canonical_us+step_us,offset=endpoint-(clock->time_us-us);
  HKTInputs input;hcl_input(&input,r,&s,step_us,(double)offset/(double)us);
  memcpy(actor,r->before,sizeof actor);memcpy(actor+0x1a0,s.canonical,64);
  if(!step(context,actor,&input,r->mxcsr_before,NULL)||!hcl_finite(actor+0x1a0,16)){hcl_invalidate(state);return HCL_BACKEND_FAILED;}
  memcpy(s.canonical,actor+0x1a0,64);memcpy(s.q_linear,input.current_linear,16);memcpy(s.q_angular,input.current_angular,16);++s.ticks;s.canonical_us=endpoint;s.primed=1;step_us=33333;
 }
 s.credit_us=clock->time_us-s.canonical_us;
 memcpy(output,s.canonical,64);
 if(s.credit_us){
  HKTInputs input;hcl_input(&input,r,&s,s.credit_us,1.0);
  memcpy(actor,r->before,sizeof actor);memcpy(actor+0x1a0,s.canonical,64);
  if(!step(context,actor,&input,r->mxcsr_before,NULL)||!hcl_finite(actor+0x1a0,16)){hcl_invalidate(state);return HCL_BACKEND_FAILED;}
  memcpy(output,actor+0x1a0,64);
 }
 hcl_remember(&s,r,output,clock);*state=s;return HCL_APPLIED;
}
#endif
