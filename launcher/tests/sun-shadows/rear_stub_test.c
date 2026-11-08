#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SV __attribute__((sysv_abi))
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d: %s case%s\n",__LINE__,#c,testname);exit(2);}}while(0)
typedef struct Desc { void** vt; uint32_t refs,pad; uint32_t w,h,fmt; } Desc;
static const char* testname="init";
static int creates,constructs,dtors,native_expected,no_retain,alignchecks;
static uint64_t expected[12];
static Desc *made[3];
static void* factory=(void*)(uintptr_t)0x12345678;
static void alignment(void* frame){CHECK(((uintptr_t)frame&15)==0);alignchecks++;}
SV static void destructor(Desc*d){alignment(__builtin_frame_address(0));CHECK(d->refs==0);dtors++;}
static void* vtable[2]={0,(void*)destructor};
SV static void create(void* fac,Desc** out,uint64_t w,uint64_t h,uint64_t depth,uint64_t zero,uint64_t fmt,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){
 alignment(__builtin_frame_address(0));CHECK(fac==factory);CHECK(!*out);CHECK(depth==1&&zero==0);CHECK(a==0&&b==1&&c==0&&d==6&&e==0&&f==0);CHECK(creates<3);
 Desc*x=calloc(1,sizeof(*x));CHECK(x);x->vt=vtable;x->refs=1;x->w=w;x->h=h;x->fmt=fmt;*out=x;made[creates++]=x;
}
SV static void construct(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,Desc* primary,Desc* colour,Desc* depth,uint64_t i,uint64_t j,uint64_t k,uint64_t l){
 alignment(__builtin_frame_address(0));CHECK(a==expected[0]&&b==expected[1]&&c==expected[2]&&d==expected[3]&&e==expected[4]);CHECK(i==expected[8]&&j==expected[9]&&k==expected[10]&&l==expected[11]);constructs++;
 if(native_expected){CHECK(creates==3);CHECK(primary==made[0]&&colour==made[1]&&depth==made[2]);CHECK(primary->w==2048&&primary->h==4096&&primary->fmt==7);CHECK(colour->w==512&&colour->h==512&&colour->fmt==37);CHECK(depth->w==512&&depth->h==512&&depth->fmt==7);for(int n=0;n<3;n++){CHECK(made[n]->refs==1);if(!no_retain)made[n]->refs++;}}
 else{CHECK(creates==0);CHECK((uint64_t)primary==expected[5]&&(uint64_t)colour==expected[6]&&(uint64_t)depth==expected[7]);}
}
SV extern void run_harness(void*,void*,uint64_t*,uint64_t*);
extern void harness_resume(void);
static void jump(void*p,void*target){unsigned char code[12]={0x48,0xb8};memcpy(code+2,&target,8);code[10]=0xff;code[11]=0xe0;memcpy(p,code,sizeof(code));}
int main(int argc,char**argv){
 CHECK(argc==2);SYSTEM_INFO si;GetSystemInfo(&si);CHECK(si.dwPageSize==4096);
 unsigned char*img=VirtualAlloc(NULL,0x2750000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);CHECK(img);
 FILE*f=fopen(argv[1],"rb");CHECK(f);size_t len=fread(img+0x1551b00,1,1280,f);CHECK(len==714);CHECK(fgetc(f)==EOF);fclose(f);
 unsigned char hook[5]={0xe9}; int32_t displacement=0x1551b00-(0x5b92d6+5);memcpy(hook+1,&displacement,4);memcpy(img+0x5b92d6,hook,5);
 jump(img+0x739f30,(void*)create);jump(img+0x5ede70,(void*)construct);jump(img+0x5b92db,(void*)harness_resume);memcpy(img+0x2749f50,&factory,8);
 uint64_t frames[6]={0};frames[0]=(uintptr_t)&frames[2];frames[2]=(uintptr_t)&frames[4];
 unsigned char cfg[0x548]={0},desc[0x100]={0};void*handle=cfg;void* dp=desc;memcpy(cfg+0x530,&dp,8);
 uint64_t args[12]={0x11111111,0x22222222,0x33333333,0x44444444,0x55555555,0x66666666,0x77777777,0x88888888,0x99999999,0xaaaaaaaa,0xbbbbbbbb,(uintptr_t)&handle};
 const struct {const char*name;int kind,w,h;} tests[]={
 {"native384",0,384,136},{"native256",0,256,128},{"native1152",0,1152,408},{"native768",0,768,384},{"release_zero",9,256,128},
 {"wrong_return1",1,384,136},{"wrong_return2",2,384,136},{"wrong_return3",3,384,136},{"null_handle",4,384,136},{"null_config",5,384,136},{"null_description",6,384,136},
 {"384_wrong_height",7,384,135},{"256_wrong_height",7,256,127},{"1152_wrong_height",7,1152,407},{"768_wrong_height",7,768,383},{"other_width",7,1920,1080}};
 for(unsigned t=0;t<sizeof(tests)/sizeof(tests[0]);t++){
  testname=tests[t].name;int kind=tests[t].kind;creates=constructs=dtors=alignchecks=0;no_retain=(kind==9);native_expected=(kind==0||kind==9);memset(made,0,sizeof(made));
  frames[1]=(uintptr_t)img+0x5b95c3;frames[3]=(uintptr_t)img+0x5c720f;frames[5]=(uintptr_t)img+0xfbd4d9;if(kind>=1&&kind<=3)frames[2*kind-1]++;
  uint32_t wh[2]={tests[t].w,tests[t].h};memcpy(desc+0x2c,wh,8);handle=kind==5?NULL:cfg;dp=kind==6?NULL:desc;memcpy(cfg+0x530,&dp,8);args[11]=kind==4?0:(uintptr_t)&handle;memcpy(expected,args,sizeof(args));
  uint64_t out[8]={0};run_harness(img+0x5b92d6,frames,args,out);
  CHECK(constructs==1);CHECK(out[0]==0xb0b0b0b0&&out[1]==0xc0c0c0c0&&out[2]==0xd0d0d0d0&&out[3]==0xe0e0e0e0&&out[4]==0xf0f0f0f0&&out[5]==(uintptr_t)frames);CHECK((out[6]&15)==0);CHECK((out[7]&(1<<10))==0);
  if(native_expected){CHECK(creates==3);CHECK(dtors==(no_retain?3:0));for(int n=0;n<3;n++){CHECK(made[n]->refs==(no_retain?0:1));free(made[n]);}CHECK(alignchecks==(no_retain?7:4));}else{CHECK(creates==0&&dtors==0&&alignchecks==1);}
  printf("PASS %s constructors=%d creates=%d local_releases=3_if_native dtors=%d flagsDF=0 alignment=16 callee_saved=all\n",testname,constructs,creates,dtors);
 }
 VirtualFree(img,0,MEM_RELEASE);puts("PASS all16cases actual714byteassembledstub CPUonly");return 0;
}
