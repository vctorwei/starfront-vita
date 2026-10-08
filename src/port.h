#pragma once
#include <stddef.h>
#include <stdint.h>
#define DATA_DIR "ux0:data/starfront"
#define GAME_DIR DATA_DIR "/GloftSFHP"
#define SAVE_DIR DATA_DIR "/save"
#define SO_PATH DATA_DIR "/libstarfront.so"
#define SCREEN_W 960
#define SCREEN_H 544
#ifndef STARFRONT_FILE_LOG
#define STARFRONT_FILE_LOG 0
#endif
void port_log(const char *fmt, ...) __attribute__((format(printf,1,2)));
void fatal_error(const char *fmt, ...) __attribute__((noreturn,format(printf,1,2)));
void port_screen(int enabled);
void port_init_log(void);
#ifdef STARFRONT_AUTOSTART
int port_flush_emulator_log(void);
#endif
void port_stop_if_failed(void);
int port_map_path(const char *input, char *output, size_t size);
void bridges_init(void);
void *jni_environment(void);
void *jni_vm(void);
void *jni_class(const char *name);
void jni_bridge_init(void);
