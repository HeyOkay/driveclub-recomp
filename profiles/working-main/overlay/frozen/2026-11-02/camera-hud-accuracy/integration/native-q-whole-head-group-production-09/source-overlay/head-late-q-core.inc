/* OPTIONAL HCL late-source experiment. Existing head/source/native30 path is
 * unchanged when no private Replay scope exists. Logical Replay canonical
 * steps consume exact LSQ postFB endpoints; local64 display is sample-and-hold.
 * No partial zero-delta force transaction, prediction, gain or display delay.
 * Actor/world composition outside local1A0..1DF remains original every render.
 */
#define HLLQ_OBSERVATIONS 64
typedef struct{int64_t time;uint32_t frame;float angular[4],matrix[16];} HLLQObservation;
typedef struct{
 unsigned active,canonical_valid,history_count,initial_partial,policy,frozen_native_pause;
 int status,source_status;LsqBinding binding;uint64_t epoch,version,frozen_binding_token;
 int64_t seed_time,last_source_q,logical_time;uint64_t steps,holds,missing,resets,duplicates;
 int64_t pause_logical_time;float pause_velocity[4];
 HLLQObservation history[HLLQ_OBSERVATIONS];
 unsigned checkpoint_valid;int64_t checkpoint_q,checkpoint_duration;
 uint32_t checkpoint_mxcsr;_Alignas(16)unsigned char checkpoint_actor[0x380];
 HKTInputs checkpoint_input;float checkpoint_previous_velocity[4],checkpoint_current_velocity[4];
} HLLQState;
typedef struct{
 void*context;
 int(*hold)(void*,const LsqSelector*,int64_t,int*,LsqPoint*);
 int(*pair)(void*,const LsqSelector*,int64_t,LsqPoint*,LsqPoint*);
 int(*binding)(void*,const LsqSelector*,LsqBinding*,uint64_t*);
} HLLQAccess;
enum{HLLQ_ZOH=1,HLLQ_PREFIX_WAIT=-1,HLLQ_BAD_INPUT=-2,HLLQ_SOURCE_UNAVAILABLE=-3,HLLQ_NO_ANGULAR_BASIS_BRACKET=-4,HLLQ_BACKEND_FAILED=-5,HLLQ_REAL_NOT_ADMITTED=-6,HLLQ_BAD_CLOCK=-7};
static void hllq_forget(HLLQState*s){uint64_t resets=s->resets+1;memset(s,0,sizeof*s);s->resets=resets;}
static int hllq_basis_valid(const float*m){float q[4];return hcl_finite(m,16)&&crr_rigid_quaternion(m,q);}
static int hllq_observe(HLLQState*s,int64_t T,uint32_t frame,const float*angular,const float*matrix){
 if(!hcl_finite(angular,4)||!hllq_basis_valid(matrix))return 0;
 HLLQObservation o={0};o.time=T;o.frame=frame;memcpy(o.angular,angular,16);memcpy(o.matrix,matrix,64);
 if(s->history_count){HLLQObservation*last=s->history+s->history_count-1;if(T<last->time)return 0;if(T==last->time){*last=o;return 1;}}
 if(s->history_count==HLLQ_OBSERVATIONS){memmove(s->history,s->history+1,(HLLQ_OBSERVATIONS-1)*sizeof o);--s->history_count;}s->history[s->history_count++]=o;return 1;
}
static int hllq_sample(const HLLQState*s,int64_t T,float angular[4],float matrix[16]){
 if(!s->history_count||T<s->history[0].time||T>s->history[s->history_count-1].time)return 0;
 for(unsigned i=0;i<s->history_count;++i){const HLLQObservation*b=s->history+i;if(T==b->time){memcpy(angular,b->angular,16);memcpy(matrix,b->matrix,64);return 1;}if(i&&s->history[i-1].time<T&&T<b->time){const HLLQObservation*a=b-1;double u=(double)(T-a->time)/(double)(b->time-a->time);for(unsigned j=0;j<4;++j)angular[j]=(float)((double)a->angular[j]+((double)b->angular[j]-a->angular[j])*u);hcl_basis(a->matrix,b->matrix,u,matrix);return 1;}}
 return 0;
}
static void hllq_seed(HCLState*s,HLLQState*l,const HKTRecord*r,const HCSClock*clock){
 hcl_invalidate(s);hllq_forget(l);l->active=1;l->seed_time=clock->time_us-clock->dt_us;l->logical_time=clock->time_us;
 memcpy(s->canonical,r->before+0x1a0,64);memcpy(s->display,s->canonical,64);s->canonical_us=l->seed_time;s->adapted=s->adapted_logical=s->observed_logical=1;
 /* Native cached angular history/body matrix belong to the original pre-call
  * transaction. A bad/uninitialized cached matrix is NOT fabricated/repaired. */
 hllq_observe(l,l->seed_time,r->frame-1,(const float*)(r->before+0x140),(const float*)(r->before+0x2d0));
 hllq_observe(l,clock->time_us,r->frame,r->input.current_angular,r->input.matrix);
}
static int hllq_hold_output(HCLState*s,HLLQState*l,const HKTRecord*r,const HCSClock*clock,unsigned char output[64],int reason){
 memcpy(output,s->canonical,64);++l->holds;if(reason<0)++l->missing;l->status=reason;s->credit_us=clock->time_us-s->canonical_us;hcl_remember(s,r,output,clock);return HCL_APPLIED;
}
static void hllq_set_linear(HKTInputs*input,const LsqPoint*previous,const LsqPoint*current,int64_t duration){
 memcpy(input->current_linear,current->vector[LSQ_LATE_VELOCITY],16);
 for(unsigned j=0;j<4;++j){volatile float delta=current->vector[LSQ_LATE_VELOCITY][j]-previous->vector[LSQ_LATE_VELOCITY][j];
  /* Only initial off-grid seed can be partial. Its explicitly chosen model is
   * constant interval acceleration between two KNOWN canonical endpoints. */
  input->delta_linear[j]=duration==33333?delta:(float)((double)delta*(double)duration/33333.0);
 }
}
static int hllq_paused_update(HCLState*s,HLLQState*l,const HKTRecord*r,HCLStep step,void*context,unsigned char output[64],const HCSClock*selected,const HCSClock*logical,const LsqPoint*held){
 int continuous=l->active&&l->policy==1&&s->valid&&s->owner==(uintptr_t)r->actor&&s->vehicle==(uintptr_t)r->vehicle&&s->clock==selected->clock&&s->domain==1&&s->time_us+selected->dt_us==selected->time_us&&r->frame==s->frame+1u&&!memcmp(s->display,r->before+0x1a0,64);
 if(continuous&&(l->pause_logical_time!=logical->time_us||l->epoch!=held->epoch||!lsq_binding_equal(&l->binding,&held->binding)||memcmp(l->pause_velocity,held->vector[LSQ_LATE_VELOCITY],16))){hcl_invalidate(s);hllq_forget(l);l->active=1;l->status=HLLQ_SOURCE_UNAVAILABLE;l->policy=2;return HCL_NATIVE;}
 if(!continuous){hllq_seed(s,l,r,selected);l->policy=1;l->binding=held->binding;l->epoch=held->epoch;l->pause_logical_time=logical->time_us;memcpy(l->pause_velocity,held->vector[LSQ_LATE_VELOCITY],16);memcpy(s->q_linear,l->pause_velocity,16);memcpy(s->q_angular,r->before+0x140,16);l->canonical_valid=1;}
 l->logical_time=logical->time_us;l->last_source_q=held->q;l->version=held->version;l->source_status=LSQ_OK;
 if(!hllq_observe(l,selected->time_us,r->frame,r->input.current_angular,r->input.matrix))return hllq_hold_output(s,l,r,selected,output,HLLQ_BAD_INPUT);
 unsigned count=0;
 while((s->canonical_us/33333+1)*33333<=selected->time_us){
  int64_t endpoint=(s->canonical_us/33333+1)*33333,duration=endpoint-s->canonical_us;if(++count>8)return hllq_hold_output(s,l,r,selected,output,HLLQ_BAD_CLOCK);
  HKTInputs input=r->input;input.dt[0]=(float)duration*1e-6f;volatile float inv=1.f/input.dt[0];for(unsigned j=0;j<4;++j){input.broadcast_dt[j]=input.dt[0];input.inverse[j]=inv;input.current_linear[j]=l->pause_velocity[j];input.delta_linear[j]=0;}
  if(!hllq_sample(l,endpoint,input.current_angular,input.matrix))return hllq_hold_output(s,l,r,selected,output,HLLQ_NO_ANGULAR_BASIS_BRACKET);
  for(unsigned j=0;j<4;++j){volatile float d=input.current_angular[j]-s->q_angular[j];input.delta_angular[j]=d;}
  _Alignas(16)unsigned char actor[0x380];memcpy(actor,r->before,sizeof actor);memcpy(actor+0x1a0,s->canonical,64);if(!step(context,actor,&input,r->mxcsr_before,NULL)||!hcl_finite(actor+0x1a0,16))return hllq_hold_output(s,l,r,selected,output,HLLQ_BACKEND_FAILED);
  memcpy(s->canonical,actor+0x1a0,64);memcpy(s->q_angular,input.current_angular,16);s->canonical_us=endpoint;s->primed=1;s->ticks++;l->steps++;l->initial_partial=duration!=33333;
 }
 memcpy(output,s->canonical,64);s->credit_us=selected->time_us-s->canonical_us;
 if(s->credit_us){ /* Here held logical V means TRUE zero external linear
    * acceleration. Native internal spring/restoring/damping still evolves. */
  HKTInputs input=r->input;input.dt[0]=(float)s->credit_us*1e-6f;volatile float inv=1.f/input.dt[0];for(unsigned j=0;j<4;++j){input.broadcast_dt[j]=input.dt[0];input.inverse[j]=inv;input.current_linear[j]=l->pause_velocity[j];input.delta_linear[j]=0;volatile float d=input.current_angular[j]-s->q_angular[j];input.delta_angular[j]=d;}
  _Alignas(16)unsigned char actor[0x380];memcpy(actor,r->before,sizeof actor);memcpy(actor+0x1a0,s->canonical,64);if(!step(context,actor,&input,r->mxcsr_before,NULL)||!hcl_finite(actor+0x1a0,16))return hllq_hold_output(s,l,r,selected,output,HLLQ_BACKEND_FAILED);memcpy(output,actor+0x1a0,64);
 }
 l->status=HLLQ_ZOH;hcl_remember(s,r,output,selected);return HCL_APPLIED;
}
#include "frozen-pause-core.inc"
static int hllq_update(HCLState*state,HLLQState*late,const HKTRecord*r,HCLStep step,void*context,unsigned char output[64],const HCSClock*selected,const HCSClock*logical,const HLLQAccess*access,unsigned paused_admitted){
 if(!logical||!logical->valid||!access||!access->hold||!access->pair){if(late->active){memcpy(output,state->canonical,64);memcpy(state->display,output,64);late->status=HLLQ_BAD_CLOCK;++late->holds;++late->missing;return HCL_APPLIED;}return HCL_UNSUPPORTED;}
 LsqSelector selector={0};selector.vehicle=(uintptr_t)r->vehicle;selector.clock=logical->clock;LsqPoint held;memset(&held,0,sizeof held);int known=0;int source_status=access->hold(access->context,&selector,logical->time_us,&known,&held);
 if(!known){if(late->active){hcl_invalidate(state);hllq_forget(late);}return hcl_update(state,r,step,context,output,selected);}
 /* A paused-born/unproven selected-clock owner has no legal continuation
  * origin. Preserve its ALREADY EXECUTED native transaction, not a fabricated
  * fixed-time spring or frozen substitute. This explicitly unsupported route
  * is distinct from ordinary logical missing-Q data. LF's bound45 reader can
  * admit the observed paused continuation in a later optional component. */
 if(logical->dt_us==0&&(!selected||!selected->valid||(selected->domain&&!paused_admitted))){hcl_invalidate(state);hllq_forget(late);late->active=1;late->status=HLLQ_REAL_NOT_ADMITTED;late->source_status=source_status;late->logical_time=logical->time_us;late->last_source_q=held.q;late->policy=2;return HCL_NATIVE;}
 int64_t us=0;
 if(!hcl_finite(r->before+0x1a0,16))return HCL_UNSUPPORTED;
 if(!selected||!selected->valid||(r->flags&(HKT_READ_COMPLETE|HKT_FRAME_VALID))!=(HKT_READ_COMPLETE|HKT_FRAME_VALID)||!crs_native_us(r->input.dt[0],&us)||us!=selected->dt_us||us<=0||us>8*33333||r->frame!=selected->frame||logical->clock!=selected->clock||logical->frame!=selected->frame){if(!late->active)hllq_seed(state,late,r,logical);late->source_status=source_status;return hllq_hold_output(state,late,r,logical,output,HLLQ_BAD_CLOCK);}
 if(selected->domain){ /* Only LF's bound45/68 continuation may enter here. */
  int frozen_result=HCL_NATIVE;if(paused_admitted&&hllq_try_frozen_pause(state,late,r,step,context,output,selected,logical,access,&selector,source_status,&frozen_result))return frozen_result;
  if(paused_admitted&&logical->dt_us==0&&source_status==LSQ_OK)return hllq_paused_update(state,late,r,step,context,output,selected,logical,&held);
  hcl_invalidate(state);hllq_forget(late);late->active=1;late->source_status=source_status;late->status=HLLQ_REAL_NOT_ADMITTED;late->last_source_q=held.q;late->logical_time=logical->time_us;late->policy=2;return HCL_NATIVE;
 }
 if(selected->time_us!=logical->time_us||selected->dt_us!=logical->dt_us)return HCL_UNSUPPORTED;
 int continuous=late->active&&state->valid&&state->owner==(uintptr_t)r->actor&&state->vehicle==(uintptr_t)r->vehicle&&state->clock==selected->clock&&state->domain==0&&state->time_us+us==selected->time_us&&r->frame==state->frame+1u&&!memcmp(state->display,r->before+0x1a0,64);
 if(!continuous)hllq_seed(state,late,r,selected);
 late->source_status=source_status;late->logical_time=logical->time_us;
 if(!hllq_observe(late,selected->time_us,r->frame,r->input.current_angular,r->input.matrix))return hllq_hold_output(state,late,r,selected,output,HLLQ_BAD_INPUT);
 if(source_status!=LSQ_OK)return hllq_hold_output(state,late,r,selected,output,HLLQ_SOURCE_UNAVAILABLE);
 if(late->epoch!=held.epoch||!lsq_binding_equal(&late->binding,&held.binding)){
  hllq_seed(state,late,r,selected);late->binding=held.binding;late->epoch=held.epoch;
 }
 late->source_status=source_status;late->last_source_q=held.q;late->version=held.version;
 if(!late->canonical_valid){
  LsqPoint previous,current;int status=access->pair(access->context,&selector,held.q,&previous,&current);
  if(status!=LSQ_OK||current.epoch!=late->epoch||held.q<=late->seed_time)return hllq_hold_output(state,late,r,selected,output,HLLQ_PREFIX_WAIT);
  int64_t start=late->seed_time>held.q-33333?late->seed_time:held.q-33333;state->canonical_us=start;late->canonical_valid=1;
  float angular[4],matrix[16];if(!hllq_sample(late,start,angular,matrix)){late->canonical_valid=0;return hllq_hold_output(state,late,r,selected,output,HLLQ_NO_ANGULAR_BASIS_BRACKET);}memcpy(state->q_angular,angular,16);
 }
 unsigned steps=0;
 while(state->canonical_us<held.q){
  int64_t endpoint=(state->canonical_us/33333+1)*33333,duration=endpoint-state->canonical_us;
  if(++steps>8)return hllq_hold_output(state,late,r,selected,output,HLLQ_SOURCE_UNAVAILABLE);
  LsqPoint previous,current;int status=access->pair(access->context,&selector,endpoint,&previous,&current);
  if(status!=LSQ_OK||current.epoch!=late->epoch||!lsq_binding_equal(&current.binding,&late->binding))return hllq_hold_output(state,late,r,selected,output,HLLQ_SOURCE_UNAVAILABLE);
  HKTInputs input=r->input;input.dt[0]=(float)duration*1e-6f;volatile float inverse=1.f/input.dt[0];for(unsigned j=0;j<4;++j){input.broadcast_dt[j]=input.dt[0];input.inverse[j]=inverse;}
  if(!hllq_sample(late,endpoint,input.current_angular,input.matrix))return hllq_hold_output(state,late,r,selected,output,HLLQ_NO_ANGULAR_BASIS_BRACKET);
  for(unsigned j=0;j<4;++j){volatile float delta=input.current_angular[j]-state->q_angular[j];input.delta_angular[j]=delta;}hllq_set_linear(&input,&previous,&current,duration);
  _Alignas(16)unsigned char actor[0x380];memcpy(actor,r->before,sizeof actor);memcpy(actor+0x1a0,state->canonical,64);
  memcpy(late->checkpoint_actor,actor,sizeof actor);late->checkpoint_input=input;late->checkpoint_mxcsr=r->mxcsr_before;late->checkpoint_q=endpoint;late->checkpoint_duration=duration;memcpy(late->checkpoint_previous_velocity,previous.vector[LSQ_LATE_VELOCITY],16);memcpy(late->checkpoint_current_velocity,current.vector[LSQ_LATE_VELOCITY],16);
  if(!step(context,actor,&input,r->mxcsr_before,NULL)||!hcl_finite(actor+0x1a0,16))return hllq_hold_output(state,late,r,selected,output,HLLQ_BACKEND_FAILED);
  late->checkpoint_valid=1;late->initial_partial=duration!=33333;late->steps++;state->ticks++;state->canonical_us=endpoint;state->primed=1;memcpy(state->canonical,actor+0x1a0,64);memcpy(state->q_linear,input.current_linear,16);memcpy(state->q_angular,input.current_angular,16);
 }
 /* Same-Q authoritative replacement is a recomputation from the saved PRE-Q
  * checkpoint, not another tick. Field-only LSQ attaches need no recomputation. */
 if(late->checkpoint_valid&&late->checkpoint_q==held.q&&memcmp(late->checkpoint_current_velocity,held.vector[LSQ_LATE_VELOCITY],16)){
  LsqPoint previous,current;if(access->pair(access->context,&selector,held.q,&previous,&current)!=LSQ_OK||current.epoch!=late->epoch)return hllq_hold_output(state,late,r,selected,output,HLLQ_SOURCE_UNAVAILABLE);
  HKTInputs input=late->checkpoint_input;hllq_set_linear(&input,&previous,&current,late->checkpoint_duration);_Alignas(16)unsigned char actor[0x380];memcpy(actor,late->checkpoint_actor,sizeof actor);
  if(!step(context,actor,&input,late->checkpoint_mxcsr,NULL)||!hcl_finite(actor+0x1a0,16))return hllq_hold_output(state,late,r,selected,output,HLLQ_BACKEND_FAILED);
  late->checkpoint_input=input;memcpy(late->checkpoint_current_velocity,current.vector[LSQ_LATE_VELOCITY],16);memcpy(state->canonical,actor+0x1a0,64);memcpy(state->q_linear,input.current_linear,16);++late->duplicates;
 }
 return hllq_hold_output(state,late,r,selected,output,HLLQ_ZOH);
}
