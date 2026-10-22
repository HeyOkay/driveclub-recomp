/* Windows regression test: clang -O2 tools/menu_repeat_test.c -o menu_repeat_test.exe */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define ABI __attribute__((sysv_abi))
static unsigned char *image;
static uintptr_t runtime_image_base;
static size_t runtime_image_size;
#include "../src/runtime_menu_repeat.inc"
static float test_dt;
static ABI float read_delta(void) { return test_dt; }
static ABI void original_poll(void *self, void *input) {
    uint32_t *fields=self;
    uint32_t old=fields[2],held=*(uint32_t *)input;
    fields[5]=old; fields[2]=held; fields[3]=held&~old; fields[4]=old&~held;
    if (!held) return;
    if (fields[3]) { fields[16]=fields[17]=7; return; }
    int interval=(int)fields[16],remaining=(int)fields[17];
    if (remaining-- <= 0) {
        interval = interval < 2 ? 1 : interval - 1;
        remaining = interval; fields[3] = held & 15;
    }
    fields[16]=interval;fields[17]=remaining;
}
static void require(int condition,const char *message) {
    if (!condition) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
int main(void) {
    runtime_image_size=0x4000000;
    image=VirtualAlloc(NULL,runtime_image_size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    require(image!=NULL,"allocation");runtime_image_base=(uintptr_t)image;
    menu_repeat_jump(image+0x79690,(uintptr_t)read_delta);
    const unsigned char prologue[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x10};
    memcpy(image+0x28e630,prologue,sizeof prologue);
    const unsigned char restore[]={0x48,0x83,0xc4,0x10,0x5b,0x41,0x5c,0x41,0x5e,0x41,0x5f,0x5d};
    memcpy(image+0x28e63f,restore,sizeof restore);
    menu_repeat_jump(image+0x28e63f+sizeof restore,(uintptr_t)original_poll);
    for(unsigned i=0;i<25;++i) { memcpy(image+0x15c1ca0+i*24,&i,4);image[0x15c1ca0+i*24+16]=i<4; }
    const unsigned char repeat[]={0x48,0xb8,0x07,0,0,0,0x07,0,0,0,0x49,0x89,0x46,0x40};
    memcpy(image+0x28e75b,repeat,sizeof repeat);
    _putenv_s("BB_MENU_NAV_REPEAT_FIX","0");menu_repeat_install();
    require(!memcmp(image+0x28e630,prologue,sizeof prologue),"explicit opt-out");
    _putenv_s("BB_MENU_NAV_REPEAT_FIX","");menu_repeat_install();
    require(image[0x28e630]==0xff,"enabled by default");
    MenuReadButtons poll=(MenuReadButtons)(image+0x28e630);
    uint32_t self[20]={0},reference[20]={0},held;
    const unsigned fps[]={30,45,60,90,120,144,240};
    double baseline[64];unsigned baseline_count=0;
    for(unsigned n=0;n<sizeof fps/sizeof fps[0];++n) {
        memset(self,0,sizeof self);test_dt=1.0f/fps[n];held=1;
        unsigned actions=0;
        for(unsigned frame=0;frame<fps[n]*2;++frame) {
            poll(self,&held);
            if (!n) {
                original_poll(reference,&held);
                require(!memcmp(self,reference,sizeof self),"30 FPS exact original behavior");
            }
            if (self[3]&1) {
                if (!n) baseline[baseline_count++]=(double)frame/fps[n];
                else {
                    require(actions<baseline_count,"repeat count bounded");
                    require(fabs((double)frame/fps[n]-baseline[actions])<=1.0/fps[n]+0.00001,"repeat timing matches 30 FPS");
                }
                ++actions;
            }
        }
        require(actions==baseline_count,"equal number of repeated actions in two seconds");
        held=0;poll(self,&held);
        require(self[4]==1 && self[3]==0,"release retained");
        held=1;actions=0;unsigned old_actions=0;
        memset(reference,0,sizeof reference);
        for(unsigned frame=0;frame<(unsigned)ceil(fps[n]*.2);++frame) {
            poll(self,&held); actions+=(self[3]&1)!=0;
            original_poll(reference,&held); old_actions+=(reference[3]&1)!=0;
        }
        require(actions==1,"200 ms press generates one action");
        held=0;poll(self,&held);held=2;poll(self,&held);
        require(self[3]==2,"new direction accepted immediately");
        printf("PASS fps=%u repeats=%u short_press_actions=1 (original=%u)\n",fps[n],baseline_count,old_actions);
    }
    /* Uneven frame pacing must use elapsed time, rather than the selected FPS cap. */
    memset(self,0,sizeof self);held=1;test_dt=1.0f/60;poll(self,&held);
    const float deltas[]={1.0f/144,1.0f/60,1.0f/30,1.0f/45,1.0f/120};
    double elapsed=0;unsigned actions=1;
    for(unsigned frame=0;elapsed<1.9;++frame) {
        test_dt=deltas[frame%5];elapsed+=test_dt;poll(self,&held);
        if(self[3]&1) {
            require(actions<baseline_count,"variable pacing action bound");
            require(elapsed+0.00001>=baseline[actions] && elapsed-baseline[actions]<=test_dt+0.00001,"variable pacing follows elapsed time");
            ++actions;
        }
    }
    puts("PASS uneven frame pacing");
    puts("All menu-repeat native gateway and timing checks passed.");return 0;
}
