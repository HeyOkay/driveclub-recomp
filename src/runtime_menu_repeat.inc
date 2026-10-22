/* 1.28 menu key-repeat timing correction. BB_MENU_NAV_REPEAT_FIX=0 disables it. */
#ifdef _WIN32
typedef void (ABI *MenuReadButtons)(void *, void *);
static MenuReadButtons menu_repeat_original;
typedef struct { void *self; double credit; } MenuRepeatClock;
static uint32_t menu_repeat_mask;
static void menu_repeat_step(int *interval, int *remaining, uint32_t held, uint32_t *pressed) {
    if (*remaining > 0) { --*remaining; return; }
    *interval = *interval > 2 ? *interval - 1 : 1;
    *remaining = *interval;
    *pressed |= held & menu_repeat_mask;
}
static void menu_repeat_correct(unsigned char *self, double dt, double *credit, int old_interval, int old_remaining) {
    uint32_t held, pressed;
    memcpy(&held,self+8,4); memcpy(&pressed,self+12,4);
    uint32_t previous; memcpy(&previous,self+20,4);
    uint32_t fresh = held & ~previous;
    if (!held || fresh) { *credit=0; return; }
    /* The original poll already produced edge/release/axis data. Replace only its
       frame-counted repeat result and counters, using virtual 30 Hz ticks. */
    pressed=fresh;
    if (!(dt>=0) || dt>10) dt=0;
    *credit += dt;
    const double period=1.0/30.0;
    unsigned ticks=(unsigned)((*credit+0.000001)/period);
    *credit -= ticks*period;
    if (*credit<0) *credit=0;
    for (unsigned i=0;i<ticks;++i)
        menu_repeat_step(&old_interval,&old_remaining,held,&pressed);
    memcpy(self+12,&pressed,4);
    memcpy(self+64,&old_interval,4); memcpy(self+68,&old_remaining,4);
}
static ABI void menu_repeat_read(void *self, void *input) {
    static _Thread_local MenuRepeatClock clocks[16];
    MenuRepeatClock *clock=NULL;
    for (unsigned i=0;i<16;++i) {
        if (!clocks[i].self) clocks[i].self=self;
        if (clocks[i].self==self) { clock=&clocks[i]; break; }
    }
    int interval,remaining;
    memcpy(&interval,(unsigned char *)self+64,4); memcpy(&remaining,(unsigned char *)self+68,4);
    menu_repeat_original(self,input);
    if (!clock) return;
    typedef float (ABI *RealDelta)(void);
    float dt=((RealDelta)(runtime_image_base+0x79690))();
    menu_repeat_correct(self,dt,&clock->credit,interval,remaining);
    static unsigned reported;
    if (reported++==0) {
        printf("MenuRepeat: active; real_dt=%.6f repeat_mask=0x%x\n",dt,menu_repeat_mask);
        fflush(stdout);
    }
}
static void menu_repeat_jump(unsigned char *code, uintptr_t target) {
    const unsigned char prefix[6]={0xff,0x25,0,0,0,0};
    memcpy(code,prefix,6); memcpy(code+6,&target,8);
}
static void menu_repeat_install(void) {
    const char *enabled=getenv("BB_MENU_NAV_REPEAT_FIX");
    if (enabled && atoi(enabled)==0) return;
    const size_t offset=0x28e630;
    static const unsigned char expected[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x10};
    static const unsigned char repeat[]={0x48,0xb8,0x07,0,0,0,0x07,0,0,0,0x49,0x89,0x46,0x40};
    if (runtime_image_size<0x15c1ca0+25*24 || memcmp(image+offset,expected,sizeof expected) ||
        memcmp(image+0x28e75b,repeat,sizeof repeat)) {
        puts("MenuRepeat: unsupported signature; correction skipped"); return;
    }
    menu_repeat_mask=0;
    for (unsigned i=0;i<25;++i) {
        uint32_t bit; memcpy(&bit,image+0x15c1ca0+i*24,4);
        if (image[0x15c1ca0+i*24+16] && bit<32) menu_repeat_mask|=1u<<bit;
    }
    unsigned char *code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!code) { puts("MenuRepeat: allocation failed"); return; }
    memcpy(code,expected,sizeof expected);
    menu_repeat_jump(code+sizeof expected,(uintptr_t)image+offset+sizeof expected);
    DWORD old;
    if (!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)) {
        VirtualFree(code,0,MEM_RELEASE); puts("MenuRepeat: protection failed"); return;
    }
    FlushInstructionCache(GetCurrentProcess(),code,4096);
    menu_repeat_original=(MenuReadButtons)code;
    menu_repeat_jump(image+offset,(uintptr_t)menu_repeat_read);
    image[offset+14]=0x90;
    puts("MenuRepeat: original 30 FPS repeat timing enabled; input polling and UI cadence unchanged");
}
#else
static void menu_repeat_install(void) {}
#endif
