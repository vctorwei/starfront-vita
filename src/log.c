#include <vitasdk.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "debugScreen.h"
#include "port.h"
#ifdef STARFRONT_AUTOSTART
static int screen_enabled = 1;
#else
/* Keep PSV startup quiet; fatal_error initializes its own diagnostic screen. */
static int screen_enabled = 0;
#endif
#if STARFRONT_FILE_LOG
static SceUID log_mutex = -1;
static SceUID log_fd = -1;
#endif
static atomic_int diagnostic_failed;
void port_stop_if_failed(void) {
    /* A background callback may stop while the render thread is still active.
       Keep that thread idle too, preserving the first error screen and log. */
    while(atomic_load_explicit(&diagnostic_failed,memory_order_acquire))
        sceKernelDelayThread(100000);
}
void port_screen(int enabled) { screen_enabled = enabled; }
void port_init_log(void) {
    sceIoMkdir(DATA_DIR, 0777);
    sceIoMkdir(SAVE_DIR, 0777);
    if (screen_enabled) psvDebugScreenInit();
#if STARFRONT_FILE_LOG
    sceIoRemove(DATA_DIR "/port.previous.log");
    sceIoRename(DATA_DIR "/port.log", DATA_DIR "/port.previous.log");
    log_fd = sceIoOpen(DATA_DIR "/port.log", SCE_O_CREAT|SCE_O_WRONLY|SCE_O_TRUNC, 0666);
    log_mutex = sceKernelCreateMutex("port_log", 0, 0, NULL);
    if (log_fd < 0 && screen_enabled) psvDebugScreenPrintf("Cannot write port.log: 0x%08x\n", log_fd);
#endif
}
static void emit(const char *message) {
#if STARFRONT_FILE_LOG
    if (log_mutex >= 0) sceKernelLockMutex(log_mutex, 1, NULL);
    char line[1200];
    int n = snprintf(line, sizeof(line), "[%llu] %s\n", (unsigned long long)sceKernelGetProcessTimeWide(), message);
    if (n >= (int)sizeof(line)) n = sizeof(line)-1;
    if (log_fd >= 0) { sceIoWrite(log_fd, line, n); sceIoSyncByFd(log_fd, 0); }
#endif
    if (screen_enabled) psvDebugScreenPrintf("%s\n", message);
#if STARFRONT_FILE_LOG
    if (log_mutex >= 0) sceKernelUnlockMutex(log_mutex, 1);
#endif
}
void port_log(const char *fmt, ...) {
#if !STARFRONT_FILE_LOG
    /* With no output enabled, skip formatting as well as all file I/O. */
    if (!screen_enabled) return;
#endif
    char text[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap); va_end(ap); emit(text);
}
#ifdef STARFRONT_AUTOSTART
int port_flush_emulator_log(void) {
#if STARFRONT_FILE_LOG
    /* Vita3K's sceIoSyncByFd is currently unimplemented. Close/reopen at
       recorded frame checkpoints so its host stdio buffer reaches disk.
       This instrumentation is only present in the isolated emulator build. */
    if(log_mutex>=0)sceKernelLockMutex(log_mutex,1,NULL);
    int result=log_fd;
    if(log_fd>=0) {
        result=sceIoClose(log_fd);
        log_fd=sceIoOpen(DATA_DIR "/port.log",SCE_O_WRONLY|SCE_O_APPEND,0666);
        if(log_fd<0)result=log_fd;
    }
    if(log_mutex>=0)sceKernelUnlockMutex(log_mutex,1);
    return result;
#else
    return 0;
#endif
}
#endif
void fatal_error(const char *fmt, ...) {
    /* Concurrent callbacks must preserve the first complete diagnosis. */
    if(atomic_exchange_explicit(&diagnostic_failed,1,memory_order_acq_rel))
        for(;;)sceKernelDelayThread(100000);
    char text[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap); va_end(ap);
    /* Stop the failing thread here; re-submit the diagnostic framebuffer even
       if vitaGL was previously displaying its own buffers. */
    psvDebugScreenInit(); screen_enabled = 1;
    emit("STOPPED - Starfront experimental port"); emit(text);
#if STARFRONT_FILE_LOG
    emit("Log: ux0:data/starfront/port.log");
    emit("Photograph this screen, or copy the log with VitaShell FTP.");
#else
    emit("Photograph this screen. File logging is disabled.");
#endif
    emit("Press START to exit.");
#if STARFRONT_FILE_LOG
    if(log_mutex>=0)sceKernelLockMutex(log_mutex,1,NULL);
    if(log_fd>=0){sceIoClose(log_fd);log_fd=-1;}
    if(log_mutex>=0)sceKernelUnlockMutex(log_mutex,1);
#endif
    for (;;) {
        SceCtrlData pad; sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) sceKernelExitProcess(1);
        sceKernelDelayThread(100000);
    }
}
