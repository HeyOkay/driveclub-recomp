/* CPU-only original coupled-kernel clone. Never patches the game image.
 * Every native RIP-relative data reference is validated and relocated into a
 * private writable data page; code page becomes RX. Original branch offsets
 * remain unchanged, with no external calls. Native arithmetic bytes unchanged. */
typedef struct {uint16_t offset;uint8_t length,disp_offset,index;uint32_t native_target;} HCCReloc;
#include "head-kernel-relocations.inc"
typedef struct {unsigned char*allocation,*code;float*half,*one;} HCCNativeKernel;
static __attribute__((naked,used)) ABI void hcc_native_finish(void){
 __asm__ volatile("leaq 8(%rbp), %rsp\n" "popq %r15\n" "popq %r14\n" "popq %r13\n" "popq %r12\n" "popq %rbx\n" "popq %rbp\n" "retq\n");
}
static __attribute__((naked,used)) ABI void hcc_native_execute(void*actor,const HKTInputs*input,const void*code){
 __asm__ volatile(
  "pushq %rbp\n" "pushq %rbx\n" "pushq %r12\n" "pushq %r13\n" "pushq %r14\n" "pushq %r15\n"
  "subq $328, %rsp\n" "leaq 320(%rsp), %rbp\n" "movq %rdx, -320(%rbp)\n" "movq %rdi, %r15\n"
  "vmovups 0(%rsi), %xmm0\n"
  "vmovaps %xmm0, -240(%rbp)\n"
  "vmovups 16(%rsi), %xmm0\n"
  "vmovaps %xmm0, -64(%rbp)\n"
  "vmovups 32(%rsi), %xmm0\n"
  "vmovaps %xmm0, -80(%rbp)\n"
  "vmovups 48(%rsi), %xmm0\n"
  "vmovaps %xmm0, -272(%rbp)\n"
  "vmovups 64(%rsi), %xmm0\n"
  "vmovaps %xmm0, -288(%rbp)\n"
  "vmovups 80(%rsi), %xmm0\n"
  "vmovaps %xmm0, -304(%rbp)\n"
  "vmovups 96(%rsi), %xmm0\n"
  "vmovaps %xmm0, -256(%rbp)\n"
  "leaq 112(%rsi), %rax\n" "jmpq *-320(%rbp)\n"
 );
}
static int hcc_native_init(HCCNativeKernel*k,const unsigned char*native,size_t size){
 memset(k,0,sizeof *k);if(size<0x191b110||hkt_hash(native+0xb919a1,1108)!=UINT64_C(0x61f9209a41ef3d57))return 0;
 unsigned char*p=VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!p)return 0;
 memcpy(p,native+0xb919a1,1108);hkt_jump(p+1108,(uintptr_t)hcc_native_finish);
 for(unsigned i=0;i<sizeof(hcc_data_targets)/sizeof(*hcc_data_targets);++i){unsigned target=hcc_data_targets[i];memcpy(p+4096+i*16,native+target,16);if(target==0x191b0f0)k->half=(float*)(p+4096+i*16);if(target==0x191b100)k->one=(float*)(p+4096+i*16);}
 for(unsigned i=0;i<sizeof(hcc_relocs)/sizeof(*hcc_relocs);++i){const HCCReloc*q=hcc_relocs+i;int32_t d;memcpy(&d,native+0xb919a1+q->offset+q->disp_offset,4);
  if((int64_t)0xb919a1+q->offset+q->length+d!=q->native_target||hcc_data_targets[q->index]!=q->native_target){VirtualFree(p,0,MEM_RELEASE);memset(k,0,sizeof *k);return 0;}
  d=(int32_t)(4096+q->index*16-q->offset-q->length);memcpy(p+q->offset+q->disp_offset,&d,4);
 }
 DWORD old;if(!VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(p,0,MEM_RELEASE);memset(k,0,sizeof *k);return 0;}
 FlushInstructionCache(GetCurrentProcess(),p,1122);k->allocation=k->code=p;return k->half&&k->one;
}
static int hcc_native_step(void*context,unsigned char actor[0x380],const HKTInputs*input,uint32_t mxcsr,uint32_t*post){
 HCCNativeKernel*k=context;if(!k||!k->code||mxcsr&~0xffffu)return 0;
 _Alignas(16) unsigned char aligned_actor[0x380];_Alignas(16) HKTInputs aligned_input;
 memcpy(aligned_actor,actor,sizeof aligned_actor);memcpy(&aligned_input,input,sizeof aligned_input);
 memcpy(k->half,input->global_half,16);memcpy(k->one,input->global_one,16);
 uint32_t host;__asm__ volatile("stmxcsr %0":"=m"(host));__asm__ volatile("ldmxcsr %0"::"m"(mxcsr):"memory");
 hcc_native_execute(aligned_actor,&aligned_input,k->code);uint32_t after;__asm__ volatile("stmxcsr %0":"=m"(after));__asm__ volatile("ldmxcsr %0"::"m"(host):"memory");memcpy(actor,aligned_actor,sizeof aligned_actor);if(post)*post=after;return 1;
}
static void hcc_native_destroy(HCCNativeKernel*k){if(k->allocation)VirtualFree(k->allocation,0,MEM_RELEASE);memset(k,0,sizeof *k);}
