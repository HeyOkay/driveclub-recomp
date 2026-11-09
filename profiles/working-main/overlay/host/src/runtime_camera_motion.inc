#include <math.h>
/* Camera-only native30 derivative observation, lifecycle repair only.
   Native target update is never called while holding the history lock. */
typedef void (ABI *CameraTargetUpdate)(void *,unsigned,float);
static CameraTargetUpdate camera_target_update_original;
static int camera_motion_point_enabled;
typedef struct {double end;float dt,v[6];} CameraMotionSample;
typedef struct {
    void *self;uintptr_t binding;unsigned kind,frame,count,has_frame;
    uint64_t generation;double time;
    CameraMotionSample samples[128];
} CameraMotionHistory;
#define CAMERA_MOTION_HISTORY_COUNT 64
static CameraMotionHistory camera_motion_histories[CAMERA_MOTION_HISTORY_COUNT];
static SRWLOCK camera_motion_history_lock=SRWLOCK_INIT;
static uint64_t camera_motion_generation;
typedef struct {unsigned kind,frame;uintptr_t binding;unsigned char reset;} CameraMotionIdentity;
static CameraMotionIdentity camera_motion_identity(void *self){
    CameraMotionIdentity a;
    memcpy(&a.kind,(char *)self+8,4);memcpy(&a.binding,(char *)self+0x28,8);
    memcpy(&a.frame,(char *)self+0x44,4);a.reset=*((unsigned char *)self+0x40);
    return a;
}
static void camera_motion_reset_history(CameraMotionHistory *h,void *self,unsigned kind,uintptr_t binding){
    memset(h,0,sizeof *h);h->self=self;h->kind=kind;h->binding=binding;
    h->generation=++camera_motion_generation;
}
static CameraMotionHistory *camera_motion_history(void *self){
    CameraMotionHistory *empty=NULL;
    for(unsigned i=0;i<CAMERA_MOTION_HISTORY_COUNT;++i){
        if(camera_motion_histories[i].self==self)return &camera_motion_histories[i];
        if(!camera_motion_histories[i].self && !empty)empty=&camera_motion_histories[i];
    }
    /* Excess objects retain native output; never churn/erase active histories. */
    return empty;
}
/* Caller holds the history lock. This is the old derivative math, unchanged. */
static void camera_motion_filter_history(CameraMotionHistory *h,void *self,float dt){
    if(!(dt>0) || dt>0.1f){h->count=0;h->time=0;return;}
    if(h->count==128){memmove(h->samples,h->samples+1,127*sizeof h->samples[0]);--h->count;}
    h->time+=dt;
    CameraMotionSample *s=&h->samples[h->count++];s->end=h->time;s->dt=dt;
    memcpy(s->v,(char *)self+0x120,12);memcpy(s->v+3,(char *)self+0x130,12);
    const double interval=1.0/30.0;
    if(dt>=interval-0.000001)return;
    if(camera_motion_point_enabled && h->kind==1){
        double target=h->time-interval;float derivative[6];
        for(unsigned i=h->count-1;i>0;--i){
            CameraMotionSample *a=&h->samples[i-1],*b=&h->samples[i];
            if(a->end>target || b->end<=a->end)continue;
            double alpha=(target-a->end)/(b->end-a->end);
            for(unsigned axis=0;axis<6;++axis){
                derivative[axis]=(float)((s->v[axis]-(a->v[axis]+(b->v[axis]-a->v[axis])*alpha))/interval);
                if(!isfinite(derivative[axis]))return;
            }
            memcpy((char *)self+0x140,derivative,12);memcpy((char *)self+0x150,derivative+3,12);
            return;
        }
        return;
    }
    if(h->time<2*interval)return;
    double integral[2][6]={{0}},covered[2]={0};
    for(unsigned i=h->count;i>0;--i){
        CameraMotionSample *a=&h->samples[i-1];double start=a->end-a->dt;
        if(a->end<=h->time-2*interval)break;
        for(unsigned w=0;w<2;++w){
            double hi=h->time-w*interval,lo=hi-interval;
            double left=start>lo?start:lo,right=a->end<hi?a->end:hi,weight=right-left;
            if(weight<=0)continue;
            covered[w]+=weight;
            for(unsigned axis=0;axis<6;++axis)integral[w][axis]+=weight*a->v[axis];
        }
    }
    if(covered[0]<interval-0.000001 || covered[1]<interval-0.000001)return;
    for(unsigned axis=0;axis<6;++axis){
        float derivative=(float)((integral[0][axis]-integral[1][axis])/(interval*interval));
        if(!isfinite(derivative))return;
        memcpy((char *)self+(axis<3?0x140:0x150)+(axis%3)*4,&derivative,4);
    }
}
static ABI void camera_target_update(void *self,unsigned frame,float dt){
    const CameraMotionIdentity before=camera_motion_identity(self);
    camera_target_update_original(self,frame,dt);
    const CameraMotionIdentity after=camera_motion_identity(self);
    const int rebound=before.kind!=after.kind || before.binding!=after.binding;
    AcquireSRWLockExclusive(&camera_motion_history_lock);
    CameraMotionHistory *h=camera_motion_history(self);
    if(!h){ReleaseSRWLockExclusive(&camera_motion_history_lock);return;}
    /* A native duplicate frame does no update. Do not advance observation time.
       A pending reset/rebind still invalidates history for its next real frame. */
    if(before.frame==frame){
        if(h->self && (before.reset || after.reset || rebound || h->kind!=after.kind || h->binding!=after.binding))
            camera_motion_reset_history(h,self,after.kind,after.binding);
        ReleaseSRWLockExclusive(&camera_motion_history_lock);return;
    }
    /* A different native update has already advanced this object: never publish
       a stale callback's derivative over that newer frame. */
    unsigned current;memcpy(&current,(char *)self+0x44,4);
    if(after.frame!=frame || current!=frame){
        if(h->self)camera_motion_reset_history(h,self,after.kind,after.binding);
        ReleaseSRWLockExclusive(&camera_motion_history_lock);return;
    }
    const int lifecycle_reset=!h->self || before.reset || after.reset || rebound || before.frame+1!=frame ||
        h->kind!=after.kind || h->binding!=after.binding;
    if(!lifecycle_reset && h->has_frame && h->frame==frame){
        ReleaseSRWLockExclusive(&camera_motion_history_lock);return;
    }
    const int reset=lifecycle_reset || (h->has_frame && h->frame+1!=frame);
    if(reset)camera_motion_reset_history(h,self,after.kind,after.binding);
    h->frame=frame;h->has_frame=1;
    camera_motion_filter_history(h,self,dt);
    ReleaseSRWLockExclusive(&camera_motion_history_lock);
}
static void camera_motion_install(void){
    camera_motion_point_enabled=1;
    static const unsigned char entry[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x48,0x81,0xec,0x88,0,0,0};
    camera_target_update_original=(CameraTargetUpdate)camera_rate_hook(0xccd600,entry,sizeof entry,(uintptr_t)camera_target_update);
    printf("CameraMotion: shared per-target lifecycle history %s\n",camera_target_update_original?"on":"unavailable");
}
