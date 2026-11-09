/* Own native-input-frozen continuation. This DOES NOT make unavailable LSQ
 * data OK, retrieve a new Q, use raw V as canonical V, or cross a head reset.
 * Requires the existing actual45 reader plus a known unchanged binding tuple.
 * Scalar/tuple provenance remains unavailable LSQ; event46 marker2 identifies
 * the retained OWN last-valid canonical source used in this separate domain. */
static int hllq_zero4(const float*x){for(unsigned j=0;j<4;++j)if(x[j]!=0)return 0;return 1;}
static int hllq_try_frozen_pause(HCLState*s,HLLQState*l,const HKTRecord*r,HCLStep step,void*ctx,unsigned char output[64],const HCSClock*selected,const HCSClock*logical,const HLLQAccess*access,const LsqSelector*selector,int source_status,int*result){
 if(source_status!=LSQ_UNAVAILABLE||!access->binding||!selected->valid||selected->domain!=1||logical->dt_us!=0||!l->active||!l->canonical_valid||l->status!=HLLQ_ZOH||!s->valid||!s->adapted_logical||!l->epoch||s->owner!=(uintptr_t)r->actor||s->vehicle!=(uintptr_t)r->vehicle||s->clock!=selected->clock||logical->clock!=selected->clock||r->frame!=s->frame+1u||selected->frame!=r->frame||logical->frame!=r->frame||memcmp(s->display,r->before+0x1a0,64))return 0;
 if(memcmp(r->input.current_linear,s->linear,16)||memcmp(r->input.current_angular,s->angular,16)||memcmp(r->input.matrix,s->matrix,64)||!hllq_zero4(r->input.delta_linear)||!hllq_zero4(r->input.delta_angular))return 0;
 int first=l->frozen_native_pause!=2;
 if(first){if(l->policy!=0||s->domain!=0||l->source_status!=LSQ_OK||s->time_us!=logical->time_us||l->logical_time!=logical->time_us||selected->time_us-selected->dt_us!=logical->time_us)return 0;}
 else if(l->policy!=1||s->domain!=1||s->time_us+selected->dt_us!=selected->time_us||l->pause_logical_time!=logical->time_us||l->logical_time!=logical->time_us)return 0;
 LsqBinding known={0};uint64_t token=0;if(!access->binding(access->context,selector,&known,&token)||!token||!lsq_binding_equal(&known,&l->binding)||(!first&&l->frozen_binding_token!=token))return 0;
 LsqPoint held={0};held.binding=l->binding;held.epoch=l->epoch;held.version=l->version;held.q=l->last_source_q;
 memcpy(held.vector[LSQ_LATE_VELOCITY],s->q_linear,16);
 /* Paused continuation consumes the last OWN private canonical V. The actual
  * frozen native values above are identity/zero-input evidence ONLY. */
 *result=hllq_paused_update(s,l,r,step,ctx,output,selected,logical,&held);
 LsqBinding after={0};uint64_t after_token=0;
 if(!access->binding(access->context,selector,&after,&after_token)||after_token!=token||!lsq_binding_equal(&after,&known)){
  hcl_invalidate(s);hllq_forget(l);l->active=1;l->policy=2;l->status=HLLQ_SOURCE_UNAVAILABLE;l->source_status=source_status;*result=HCL_NATIVE;return 1;
 }
 l->source_status=source_status;l->frozen_native_pause=2;l->frozen_binding_token=token;return 1;
}
