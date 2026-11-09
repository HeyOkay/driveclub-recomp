/* No additional native getter. Logical hold clock is a separate coherent
 * direct read from the SAME selected master; it is not a selected real dt. */
static int hllq_enabled;
static int hllq_runtime_hold(void*unused,const LsqSelector*x,int64_t T,int*known,LsqPoint*out){(void)unused;return lsq_scope_hold(x,T,known,out);}
static int hllq_runtime_pair(void*unused,const LsqSelector*x,int64_t q,LsqPoint*a,LsqPoint*b){(void)unused;return lsq_velocity_pair_Q(x,q,a,b);}
static int hllq_runtime_binding(void*unused,const LsqSelector*x,LsqBinding*out,uint64_t*token){(void)unused;return lsq_unavailable_binding(x,out,token);}
static HCSClock hllq_logical_clock(const HKTRecord*r,const HCSClock*selected){
 HCSClock out={0};uintptr_t root=0,service=0,master=0;int64_t a[2],b[2];uint32_t fa=0,fb=0;
 if(!camera_pose_read(runtime_image_base+0x1911100,&root,8)||!camera_pose_read(root+0x968,&service,8)||!camera_pose_read(service+0x1e0,&master,8)||!master||(selected&&selected->valid&&selected->clock!=master)||!camera_pose_read(master+0x58,a,16)||!camera_pose_read(master+0x21c,&fa,4)||!camera_pose_read(master+0x58,b,16)||!camera_pose_read(master+0x21c,&fb,4)||memcmp(a,b,16)||fa!=fb||fa!=r->frame||a[0]<0||a[1]<0||a[0]<a[1])return out;
 return(HCSClock){master,a[0],a[1],fa,1,0};
}
static int hllq_runtime_update(HCLState*s,HLLQState*l,const HKTRecord*r,HCLStep step,void*context,unsigned char output[64],const HCSClock*clock){
 if(!hllq_enabled)return hcl_update(s,r,step,context,output,clock);
 HCSClock logical=hllq_logical_clock(r,clock);HLLQAccess access={NULL,hllq_runtime_hold,hllq_runtime_pair,hllq_runtime_binding};unsigned paused_admitted=0;
#ifdef HEAD_LATE_PAUSED_READER_BOUND
 paused_admitted=1;
#endif
 int status=hllq_update(s,l,r,step,context,output,clock,&logical,&access,paused_admitted);
 if(l->active){float d[24]={(float)l->status,(float)l->source_status,(float)l->policy,(float)l->canonical_valid,(float)l->initial_partial,(float)(clock?clock->domain:0),(float)l->steps,(float)l->duplicates};memcpy(d+8,&logical.time_us,8);memcpy(d+10,&l->last_source_q,8);memcpy(d+12,&l->epoch,8);memcpy(d+14,&l->version,8);memcpy(d+16,&s->canonical_us,8);memcpy(d+18,&l->seed_time,8);d[20]=(float)l->holds;d[21]=(float)l->missing;d[22]=(float)status;d[23]=(float)l->frozen_native_pause;camera_accuracy_log(46,(void*)(uintptr_t)r->actor,(uintptr_t)r->vehicle,r->frame,r->input.dt[0],d,24);}
 return status;
}
