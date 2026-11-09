/* CPU/private original linear Replay controller. NEVER calls a real SDKbody.
 * Original74-byte arithmetic + original projection/cap/teleport region execute
 * unchanged against private frame/actor and three capture-only SDK stubs.
 * Scalar is uninterpreted actual v250 return, not an asserted contact quantity.
 * No physical/raw/SDK state, sensor history, clock or renderer is modified. */
typedef struct{
 float current_position[4],current_SDK_mean_velocity[4],target_velocity[4],target_pose[16];
 float native_dt,scalar_virtual250,current_raw_velocity[4];uint32_t native_mxcsr;int raw_velocity_available;
} NlcInput;
typedef struct{
 unsigned scalar_calls,velocity_setter_calls,pose_setter_calls,normal_correction_valid;
 float native_position_error[4],native_preprojection_correction[4],native_normal_capped_correction[4];
 float native_post_velocity[4],native_teleport_pose[16];
 float post_minus_input_SDK_mean[4],post_minus_input_raw_velocity[4];
 uint32_t mxcsr_after_native;int raw_delta_available;
} NlcOutput;
typedef struct{unsigned char*private_image;size_t size;} NlcKernel;
typedef struct{uintptr_t*vtable;NlcOutput*result;unsigned char*frame;float scalar;} NlcSdk;
static ABI float nlc_scalar(void*self){NlcSdk*s=self;++s->result->scalar_calls;memcpy(s->result->native_position_error,s->frame-0xd0,16);memcpy(s->result->native_preprojection_correction,s->frame-0xc0,16);return s->scalar;}
static ABI void nlc_velocity(void*self,const float*v){NlcSdk*s=self;++s->result->velocity_setter_calls;memcpy(s->result->native_post_velocity,v,16);}
static ABI void nlc_pose(void*self,const float*m){NlcSdk*s=self;++s->result->pose_setter_calls;memcpy(s->result->native_teleport_pose,m,64);}
static uint64_t nlc_hash(const unsigned char*p,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;++i)h=(h^p[i])*UINT64_C(1099511628211);return h;}
static int nlc_init(NlcKernel*k,const unsigned char*original,size_t size){
 if(!k||!original||size<=0x12ad97c||nlc_hash(original+0x123c8b,74)!=UINT64_C(0xdcefb09f981d1858)||nlc_hash(original+0x123cd5,0x123ee3-0x123cd5)!=UINT64_C(0x1bc235377d23c741))return 0;
 static const unsigned char entry[]={0xc5,0xfa,0x10,0x15,0xdd,0x9c,0x18,0x1,0xc5,0xea,0x5e,0x95,0x50,0xff,0xff,0xff};if(memcmp(original+0x123c8b,entry,sizeof entry))return 0;
 k->private_image=VirtualAlloc(NULL,size,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!k->private_image)return 0;k->size=size;memcpy(k->private_image,original,size);k->private_image[0x123ee3]=0xc3;FlushInstructionCache(GetCurrentProcess(),k->private_image+0x123c8b,0x123ee4-0x123c8b);return 1;
}
static __attribute__((naked)) ABI void nlc_invoke(void*entry,void*frame,void*actor,void*sdk){
 __asm__ volatile("pushq %rbp\n pushq %rbx\n pushq %r12\n pushq %r14\n subq $16,%rsp\n movq %rdi,0(%rsp)\n movq %rsi,%rbp\n movq %rdx,%r12\n movq %rcx,%rbx\n leaq 0x60(%r12),%r14\n vmovaps 0x40(%r12),%xmm0\n vmovaps 0x90(%r12),%xmm1\n callq *0(%rsp)\n addq $16,%rsp\n popq %r14\n popq %r12\n popq %rbx\n popq %rbp\n retq\n");
}
static int nlc_evaluate(const NlcKernel*k,const NlcInput*in,NlcOutput*out){
 if(!k||!k->private_image||!in||!out)return 0;
 _Alignas(16)unsigned char bytes[0x400],actor[0xa0],fx[512];unsigned char*frame=bytes+0x200;memset(bytes,0,sizeof bytes);memset(actor,0,sizeof actor);memset(out,0,sizeof*out);
 memcpy(frame-0xb0,&in->native_dt,4);memcpy(frame-0xc0,in->current_position,16);memcpy(frame-0x50,in->current_SDK_mean_velocity,16);memcpy(actor+0x40,in->target_velocity,16);memcpy(actor+0x60,in->target_pose,64);
 uintptr_t vt[0x258/8]={0};vt[0x78/8]=(uintptr_t)nlc_velocity;vt[0x98/8]=(uintptr_t)nlc_pose;vt[0x250/8]=(uintptr_t)nlc_scalar;NlcSdk sdk={vt,out,frame,in->scalar_virtual250};
 __asm__ volatile("fxsave64 %0\n ldmxcsr %1":"=m"(fx):"m"(in->native_mxcsr):"memory");nlc_invoke(k->private_image+0x123c8b,frame,actor,&sdk);__asm__ volatile("stmxcsr %0":"=m"(out->mxcsr_after_native));
 if(!out->pose_setter_calls){out->normal_correction_valid=1;memcpy(out->native_normal_capped_correction,frame-0xc0,16);}
 for(unsigned j=0;j<4;++j){volatile float d=out->native_post_velocity[j]-in->current_SDK_mean_velocity[j];out->post_minus_input_SDK_mean[j]=d;if(in->raw_velocity_available){volatile float r=out->native_post_velocity[j]-in->current_raw_velocity[j];out->post_minus_input_raw_velocity[j]=r;}}
 out->raw_delta_available=in->raw_velocity_available;__asm__ volatile("fxrstor64 %0"::"m"(fx):"memory");return out->scalar_calls==1&&out->velocity_setter_calls==1&&out->pose_setter_calls<=1;
}
