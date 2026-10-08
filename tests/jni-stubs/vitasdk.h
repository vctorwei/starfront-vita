/* Host-only declarations for testing the resource JNI contract. */
#pragma once
#include <stdint.h>
typedef int SceUID;
typedef struct { uint32_t unused; } SceIoStat;
typedef struct { char d_name[256]; } SceIoDirent;
#define SCE_O_WRONLY 1
#define SCE_O_CREAT 0x200
static inline SceUID sceIoDopen(const char *p){(void)p;return -1;}
static inline int sceIoDread(SceUID d,SceIoDirent *e){(void)d;(void)e;return 0;}
static inline int sceIoDclose(SceUID d){(void)d;return 0;}
static inline int sceIoGetstat(const char *p,SceIoStat *s){(void)p;(void)s;return -1;}
static inline SceUID sceIoOpen(const char *p,int flags,int mode){(void)p;(void)flags;(void)mode;return -1;}
static inline int sceIoClose(SceUID d){(void)d;return 0;}
static inline int sceIoRename(const char *p,const char *q){(void)p;(void)q;return -1;}
static inline int sceIoRemove(const char *p){(void)p;return -1;}
