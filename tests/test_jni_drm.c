/* JNI dc resource data, constructor ordering, and application state lifetime. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdatomic.h>
#include "jni_drm_mock.h"
#include "../src/jni.c"

static atomic_int resource_logs;
static int constructor_logs;
static unsigned char expected[11];
void port_log(const char *fmt,...) {
    if(strstr(fmt,"Game.dc constructor:")) {
        va_list ap;va_start(ap,fmt);const char *profile=va_arg(ap,const char*);
        int caught=va_arg(ap,int),free_count=va_arg(ap,int);unsigned strings=va_arg(ap,unsigned);va_end(ap);
        assert(!strcmp(profile,jni_drm_mock.loaded_existing?"loaded":"fresh"));
        assert(caught==jni_drm_mock.loaded_existing && free_count==0 && strings==20);
        constructor_logs++;
    }
    if(strstr(fmt,"Game.dc()[B:"))atomic_fetch_add(&resource_logs,1);
}
void fatal_error(const char *fmt,...) {
    fprintf(stderr,"DRM_MOCK prepared=%d state=%d assigned=%d\n",
        jni_drm_mock.prepared,jni_drm_mock.state_queries,jni_drm_mock.serial_assignments);
    va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);
    fputc('\n',stderr);exit(42);
}
int port_map_path(const char *in,char *out,size_t size){(void)in;(void)out;(void)size;return -1;}

static Obj *invoke_v(uintptr_t *table,void *env,Obj *cls,Method *m,...) {
    va_list ap;va_start(ap,m);
    Obj *r=((Obj *(*)(void*,Obj*,Method*,va_list))table[115])(env,cls,m,ap);
    va_end(ap);return r;
}
typedef struct {uintptr_t *table;void *env;Obj *cls;Method *method;} ThreadCall;
static void *query_thread(void *data) {
    ThreadCall *a=data;
    for(int i=0;i<25;i++){
        Obj *array=invoke_v(a->table,a->env,a->cls,a->method);
        assert(array->refs==1 && array->length==11 && !memcmp(array->data,expected,11));
        ((void (*)(void*,Obj*))a->table[23])(a->env,array);
    }
    return NULL;
}

int main(int argc,char **argv) {
    assert(argc==2);const char *scenario=argv[1];
    jni_drm_mock.error=(SfDrmError){.code=SF_DRM_CRYPTO_ERROR,.system_error=13,.crypto_error=-0x1234};
    jni_drm_mock.fail_prepare=!strcmp(scenario,"fail-prepare");
    jni_drm_mock.fail_state=!strcmp(scenario,"fail-state");
    jni_drm_mock.fail_serial=!strcmp(scenario,"fail-serial");
    jni_drm_mock.loaded_existing=!strcmp(scenario,"loaded");
    jni_bridge_init();void *env=jni_environment();uintptr_t *table=*(uintptr_t**)env;
    const char *owner=!strcmp(scenario,"wrong-owner")?"com/example/Game":SF_GAME_CLASS;
    Obj *cls=((Obj *(*)(void*,const char*))table[6])(env,owner);
    const char *sig=!strcmp(scenario,"wrong-signature")?"()I":"()[B";
    Method *m=((Method *(*)(void*,Obj*,const char*,const char*))table[113])(env,cls,"dc",sig);
    Obj *receiver=!strcmp(scenario,"wrong-static-receiver")?NULL:cls;
    int (*push)(void*,int)=(void*)table[19];Obj *(*pop)(void*,Obj*)=(void*)table[20];
    Obj *(*retain)(void*,Obj*)=(void*)table[21];void (*global_drop)(void*,Obj*)=(void*)table[22];
    int (*length)(void*,Obj*)=(void*)table[171];
    void (*region)(void*,Obj*,int,int,void*)=(void*)table[200];
    void (*set)(void*,Obj*,int,int,const void*)=(void*)table[208];
    assert(push(env,1)==0);
    Obj *first=invoke_v(table,env,receiver,m);
    assert(first && first->kind==ARRAY && first->stride==1 && length(env,first)==11);
    assert(jni_drm_mock.prepared==1 && jni_drm_mock.state_queries==1 && jni_drm_mock.serial_assignments==1);
    FILE *f=fopen(SF_APK_SERIAL_PATH,"rb");assert(f);
    assert(fread(expected,1,11,f)==11 && fgetc(f)==EOF && fclose(f)==0);
    unsigned char bytes[11];region(env,first,0,11,bytes);
    assert(!memcmp(bytes,expected,11) && !memcmp(jni_drm_mock.serial,expected,11));
    retain(env,first);assert(pop(env,NULL)==NULL && first->refs==1);
    assert(push(env,1)==0);
    Obj *second=((Obj *(*)(void*,Obj*,Method*,const Value*))table[116])(env,cls,m,NULL);
    assert(second!=first && second->data!=first->data && second->refs==1);
    assert(jni_drm_mock.prepared==1 && jni_drm_mock.state_queries==1 && jni_drm_mock.serial_assignments==2);
    region(env,first,0,11,bytes);assert(!memcmp(bytes,expected,11));global_drop(env,first);
    unsigned char changed=(unsigned char)(expected[0]^0xff);set(env,second,0,1,&changed);
    assert(!memcmp(jni_drm_mock.serial,expected,11));
    assert(pop(env,NULL)==NULL && jni_drm_mock.destroyed==0);
    /* Concurrent native callers still construct only once and each receive
       their own complete local array. The persisted preferences outlive it. */
    ThreadCall call={table,env,cls,m};pthread_t threads[8];
    for(size_t i=0;i<8;i++)assert(pthread_create(&threads[i],NULL,query_thread,&call)==0);
    for(size_t i=0;i<8;i++)assert(pthread_join(threads[i],NULL)==0);
    assert(constructor_logs==1 && atomic_load(&resource_logs)==202);
    assert(jni_drm_mock.prepared==1 && jni_drm_mock.state_queries==1 && jni_drm_mock.serial_assignments==202);
    assert(jni_drm_mock.destroyed==0 && !memcmp(jni_drm_mock.serial,expected,11));
    ((void (*)(void*,Obj*))table[23])(env,cls);return 0;
}
