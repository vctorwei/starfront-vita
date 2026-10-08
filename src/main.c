#include <vitasdk.h>
#include <vitaGL.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"
#include "so_util.h"
#include "data_manifest.h"
#include "display_geometry.h"
#include "emulator_touch.h"
#ifdef STARFRONT_DISPLAY_TEST
#include "display_test.h"
#endif
#ifdef STARFRONT_GL_TRACE
#include "graphics_trace.h"
#include "native_state_table.h"
#endif

unsigned int _newlib_heap_size_user=200*1024*1024;
/* vita-elf-create writes this symbol's address into the process parameters.
   LoadConfig uses a 256 KiB local buffer, overflowing Vita's default 256 KiB
   main stack before its next call (00.02 core: PC 0x984d0404). */
#define GAME_MAIN_STACK_SIZE (2*1024*1024)
int sceUserMainThreadStackSize=GAME_MAIN_STACK_SIZE;
extern so_default_dynlib port_imports[];
extern const int port_imports_size;
static so_module game;
#ifdef STARFRONT_GL_TRACE
static void trace_native_state(int force) {
    /* Verified original ELF singleton and gxStateStack layout. No getter is
       called: Application::GetInstance would construct one when absent. */
    uintptr_t app = *(const uint32_t *)(game.text_base + 0x57a5b4);
    if (app < 0x80000000U || app >= 0xa0000000U || (app & 3)) {
        port_log("Native state: application=%p unavailable", (void *)app);
        return;
    }
    uintptr_t app_vptr = *(const uint32_t *)app;
    if (app_vptr != game.text_base + 0x54c1d0) {
        port_log("Native state: application=%p unexpected vptr=%p", (void *)app, (void *)app_vptr);
        return;
    }
    int index = *(const int *)(app + 56);
    if (index < 0 || index > 11) {
        port_log("Native state: application=%p stack index=%d", (void *)app, index);
        return;
    }
    uintptr_t state = *(const uint32_t *)(app + 4 + index * 4);
    if (state < 0x80000000U || state >= 0xa0000000U || (state & 3)) {
        port_log("Native state: index=%d state=%p unavailable", index, (void *)state);
        return;
    }
    uintptr_t vptr = *(const uint32_t *)state;
    static uintptr_t previous_state, previous_vptr;
    static int previous_index = -2;
    if (!force && state == previous_state && vptr == previous_vptr && index == previous_index) return;
    previous_state = state;
    previous_vptr = vptr;
    previous_index = index;
    const char *name = "unknown";
    for (unsigned i=0; i<sizeof(native_state_types)/sizeof(native_state_types[0]); ++i)
        if (vptr == game.text_base + native_state_types[i].vptr) name = native_state_types[i].name;
    port_log("Native state: index=%d state=%p vptr=%p class=%s", index, (void *)state, (void *)vptr, name);
}
#endif
#define JAVA_PREFIX "Java_com_gameloft_android_ANMP_GloftSFHP_ML_"
static void *entry(const char *name) {
    char full[256];snprintf(full,sizeof(full),JAVA_PREFIX "%s",name);
    uintptr_t p=so_symbol(&game,full);if(!p)fatal_error("Missing game entry: %s",name);return (void*)p;
}
static int exists(const char *path){SceIoStat s;return sceIoGetstat(path,&s)>=0;}
static void verify_data(void) {
    SceIoStat s;
    if(sceIoGetstat(SO_PATH,&s)<0)fatal_error("Missing libstarfront.so\nPrepare your own APK/data with prepare_game.py.\nCopy its output to ux0:data/starfront");
    if(s.st_size!=EXPECTED_SO_SIZE)fatal_error("Wrong game library size: %llu",(unsigned long long)s.st_size);
    SceUID fd=sceIoOpen(SO_PATH,SCE_O_RDONLY,0);if(fd<0)fatal_error("Cannot open game library: 0x%x",fd);
    unsigned char *buf=malloc(64*1024);if(!buf)fatal_error("Cannot allocate checksum buffer");
    uLong crc=crc32(0,NULL,0);int n;unsigned total=0;
    while((n=sceIoRead(fd,buf,64*1024))>0){crc=crc32(crc,buf,n);total+=n;}
    sceIoClose(fd);free(buf);
    if(n<0||total!=EXPECTED_SO_SIZE||crc!=EXPECTED_SO_CRC)fatal_error("Library checksum mismatch. Recopy libstarfront.so");
    int missing=0;
    const char *missing_paths[3]={NULL,NULL,NULL};
    for(unsigned i=0;i<sizeof(expected_files)/sizeof(expected_files[0]);i++) {
        char path[1024];snprintf(path,sizeof(path),DATA_DIR "/%s",expected_files[i].path);
        if(sceIoGetstat(path,&s)<0 || s.st_size!=expected_files[i].size){
            port_log("Missing/wrong size: %s",expected_files[i].path);
            if(missing<3)missing_paths[missing]=expected_files[i].path;
            missing++;
        }
    }
    if(missing)fatal_error("%d resource files missing or incomplete.\nBase: " DATA_DIR "/\n%s\n%s\n%s%s",
        missing,missing_paths[0],missing_paths[1]?missing_paths[1]:"",
        missing_paths[2]?missing_paths[2]:"",missing>3?"\n(First 3 shown)":"");
}
typedef void (*TouchFn)(void*,void*,int,int,int,int);
#ifndef STARFRONT_AUTOSTART
typedef struct {int active,id,x,y;} Finger;
static void touch_poll(TouchFn touch,void *env,void *cls) {
    static Finger fingers[2];SceTouchData t;memset(&t,0,sizeof(t));
    sceTouchPeek(SCE_TOUCH_PORT_FRONT,&t,1);
    for(int s=0;s<2;s++)if(fingers[s].active){int found=0;for(unsigned i=0;i<t.reportNum;i++)if(t.report[i].id==fingers[s].id)found=1;if(!found){touch(env,cls,0,fingers[s].x,fingers[s].y,s);fingers[s].active=0;}}
    for(unsigned i=0;i<t.reportNum;i++) {
        int slot=-1;for(int s=0;s<2;s++)if(fingers[s].active&&fingers[s].id==t.report[i].id)slot=s;
        int action=2;
        if(slot<0){for(int s=0;s<2;s++)if(!fingers[s].active){slot=s;break;}action=1;}
        if(slot<0)continue;
        Finger *f=fingers+slot;int x,y;
        int inside=sf_display_touch_to_native(t.report[i].x*SCREEN_W/1920,
                                              t.report[i].y*SCREEN_H/1088,&x,&y);
        if(action==1&&!inside)continue;
        if(action==1||x!=f->x||y!=f->y)touch(env,cls,action,x,y,slot);
        *f=(Finger){1,t.report[i].id,x,y};
    }
}
#endif
int main(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceAppUtilInit(&(SceAppUtilInitParam){0},&(SceAppUtilBootParam){0});
    port_init_log();
#ifdef STARFRONT_DISPLAY_TEST
    return sf_display_test();
#endif
    port_log("Starfront PS Vita - DEVELOPMENT TEST 00.06");
    port_log("This build is not confirmed playable.");
    SceUID main_thread=sceKernelGetThreadId();
    SceKernelThreadInfo thread_info={.size=sizeof(thread_info)};
    int thread_result=sceKernelGetThreadInfo(main_thread,&thread_info);
    if(thread_result<0)fatal_error("Cannot inspect main thread stack: 0x%08x",thread_result);
    port_log("Main thread stack: %d KiB; free: %d bytes",thread_info.stackSize/1024,sceKernelGetThreadStackFreeSize(main_thread));
    if(thread_info.stackSize<GAME_MAIN_STACK_SIZE)
        fatal_error("Main thread stack too small: %d bytes (expected %d)",thread_info.stackSize,GAME_MAIN_STACK_SIZE);
    port_log("[1/8] Checking plugins");
    int unk[2]={0};int ku=_vshKernelSearchModuleByName("kubridge",unk);
    port_log("kubridge module: 0x%08x",ku);
    if(ku<0)fatal_error("kubridge.skprx is not loaded. Reboot after installing it.");
    if(!exists("ur0:data/libshacccg.suprx")&&!exists("ur0:data/external/libshacccg.suprx"))fatal_error("libshacccg.suprx not found in ur0:data or ur0:data/external");
    port_log("[2/8] Checking library CRC and resource file sizes");verify_data();
#ifdef STARFRONT_AUTOSTART
    port_log("Preflight passed. Emulator test build: automatic startup.");
#else
    port_log("Preflight passed. Starting game.");
#endif
    scePowerSetArmClockFrequency(444);scePowerSetBusClockFrequency(222);scePowerSetGpuClockFrequency(222);
    port_log("[3/8] Loading ARMv7 library at 0x98000000");
    int r=so_load(&game,SO_PATH,0x98000000);if(r<0)fatal_error("so_load failed: 0x%08x",r);
    port_log("ELF text=0x%08x data=0x%08x",(unsigned)game.text_base,(unsigned)game.data_base);
    bridges_init();jni_bridge_init();
    so_relocate(&game);r=so_resolve(&game,port_imports,port_imports_size,1);if(r)fatal_error("%d imports unresolved",-r);
    so_flush_caches(&game);
    port_log("[4/8] Initializing vitaGL 960x544");
    port_screen(0);
    /* vitaGL returns whether it fell back to a smaller display, not success. */
    int fallback=vglInitExtended(0,SCREEN_W,SCREEN_H,64*1024*1024,SCE_GXM_MULTISAMPLE_NONE);
    port_log("vitaGL initialized, resolution fallback=%d",fallback);
    if(fallback)fatal_error("Unexpected display resolution fallback");
    sf_display_geometry_init();
    /* The public GL_SHADER_COMPILER query is a capability constant. Check
       the compiler's actual initialization result before native shaders. */
    int compiler=shark_init(NULL);
    if(compiler<0)compiler=shark_init("ur0:data/external/libshacccg.suprx");
    if(compiler<0)fatal_error("Runtime shader compiler init failed: 0x%08x",(unsigned)compiler);
    port_log("Runtime shader compiler available; GLSL translator linked");
    port_log("[5/8] Running %d game constructors",game.num_init_array);so_initialize(&game);
    port_stop_if_failed();
    void *env=jni_environment();void *game_class=jni_class("com/gameloft/android/ANMP/GloftSFHP/ML/Game");
    int (*onload)(void*,void*)=(void*)so_symbol(&game,"JNI_OnLoad");
    if(!onload)fatal_error("JNI_OnLoad missing");
    port_log("[6/8] JNI_OnLoad");int jver=onload(jni_vm(),NULL);port_log("JNI version: 0x%x",jver);
    if(jver!=0x10004&&jver!=0x10006)fatal_error("Unsupported JNI version: 0x%x",jver);
    void (*init)(void*,void*)=entry("Game_nativeInit");
    void (*renderer)(void*,void*,int,int,int)=entry("GameRenderer_nativeInit");
    void (*resize)(void*,void*,int,int)=entry("GameRenderer_nativeResize");
    void (*media)(void*,void*,int)=entry("GLMediaPlayer_nativeInit");
    void (*render)(void*,void*)=entry("GameRenderer_nativeRender");
    void (*done)(void*,void*)=entry("GameRenderer_nativeDone");
    void (*key_down)(void*,void*,int)=entry("Game_nativeOnKeyDown");
    void (*key_up)(void*,void*,int)=entry("Game_nativeOnKeyUp");
    TouchFn touch=entry("GameGLSurfaceView_nativeOnTouch");
    port_log("[7/8] Game.nativeInit");init(env,game_class);
    port_stop_if_failed();
    void *renderer_class=jni_class("com/gameloft/android/ANMP/GloftSFHP/ML/GameRenderer");
    port_log("GameRenderer.nativeInit(manufacturer=4,%d,%d)",SF_NATIVE_W,SF_NATIVE_H);
    renderer(env,renderer_class,4,SF_NATIVE_W,SF_NATIVE_H);
    port_stop_if_failed();
    port_log("GLMediaPlayer.nativeInit(0)");media(env,jni_class("com/gameloft/android/ANMP/GloftSFHP/ML/GLMediaPlayer"),0);
    port_stop_if_failed();
    resize(env,renderer_class,SF_NATIVE_W,SF_NATIVE_H);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    void *touch_class=jni_class("com/gameloft/android/ANMP/GloftSFHP/ML/GameGLSurfaceView");
    port_log("[8/8] Render loop. Touchscreen; START=Back; SELECT+START=exit.");
    unsigned frame=0,old_buttons=0;
    for(;;) {
        port_stop_if_failed();
        SceCtrlData pad;sceCtrlPeekBufferPositive(0,&pad,1);
        if((pad.buttons&(SCE_CTRL_START|SCE_CTRL_SELECT))==(SCE_CTRL_START|SCE_CTRL_SELECT))break;
        if((pad.buttons&SCE_CTRL_START)&&!(old_buttons&SCE_CTRL_START))key_down(env,game_class,4);
        if(!(pad.buttons&SCE_CTRL_START)&&(old_buttons&SCE_CTRL_START))key_up(env,game_class,4);
        old_buttons=pad.buttons;
#ifdef STARFRONT_AUTOSTART
        sf_emulator_touch_poll(touch,env,touch_class);
#else
        touch_poll(touch,env,touch_class);
#endif
        render(env,renderer_class);vglSwapBuffers(GL_FALSE);
#ifdef STARFRONT_GL_TRACE
        trace_native_state(frame < 10 || frame % 300 == 0);
#endif
        if(frame<10 || frame%300==0) {
            port_log("Frame %u completed",frame);
#ifdef STARFRONT_GL_TRACE
            sf_graphics_trace_summary();
#endif
#ifdef STARFRONT_AUTOSTART
            int flush=port_flush_emulator_log();
            if(flush<0)fatal_error("Cannot flush emulator test log: 0x%08x",(unsigned)flush);
#endif
        }
        frame++;
    }
    port_log("Exit requested by controller");done(env,renderer_class);glFinish();sceKernelExitProcess(0);return 0;
}
