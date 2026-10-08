/* Invoke the real ABI table used by vox::DriverAndroid, with a host backend. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jni_audio_mock.h"
#include "../src/jni.c"

static void *env;
static uintptr_t *table;
void port_log(const char *fmt,...) {(void)fmt;}
void fatal_error(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);
    fprintf(stderr,"\nmock created=%d destroyed=%d closed=%d writes=%d\n",
        jni_audio_mock.created,jni_audio_mock.destroyed,jni_audio_mock.closed,jni_audio_mock.writes);
    exit(42);
}
int port_map_path(const char *in,char *out,size_t size){(void)in;(void)out;(void)size;return -1;}
static Obj *create_v(Obj *cls,Method *m,...) {
    va_list ap;va_start(ap,m);
    Obj *result=((Obj *(*)(void*,Obj*,Method*,va_list))table[29])(env,cls,m,ap);
    va_end(ap);return result;
}
static int static_int_v(Obj *cls,Method *m,...) {
    va_list ap;va_start(ap,m);
    int result=((int (*)(void*,Obj*,Method*,va_list))table[130])(env,cls,m,ap);
    va_end(ap);return result;
}
static int int_v(Obj *o,Obj *cls,Method *m,...) {
    va_list ap;va_start(ap,m);
    int result=((int (*)(void*,Obj*,Obj*,Method*,va_list))table[80])(env,o,cls,m,ap);
    va_end(ap);return result;
}
static void void_v(Obj *o,Obj *cls,Method *m,...) {
    va_list ap;va_start(ap,m);
    ((void (*)(void*,Obj*,Obj*,Method*,va_list))table[92])(env,o,cls,m,ap);
    va_end(ap);
}
static void *worker_frame(void *unused) {
    (void)unused;
    int (*push)(void*,int)=(void*)table[19];Obj *(*pop)(void*,Obj*)=(void*)table[20];
    Obj *(*find)(void*,const char*)=(void*)table[6];
    Method *(*method)(void*,Obj*,const char*,const char*)=(void*)table[33];
    assert(push(env,2)==0);Obj *cls=find(env,"android/media/AudioTrack");
    Method *ctor=method(env,cls,"<init>","(IIIIII)V");
    Obj *track=create_v(cls,ctor,3,44100,12,2,4096,1);
    assert(pop(env,track)==track && track->refs==1);
    /* pthread exit invokes the same cleanup without touching the main frame. */
    return NULL;
}

int main(int argc,char **argv) {
    assert(argc==2);jni_bridge_init();env=jni_environment();table=*(uintptr_t**)env;
    uintptr_t *vm=*(uintptr_t**)jni_vm();
    int (*get_env)(void*,void**,int)=(void*)vm[6];
    int versions[]={0x10001,0x10002,0x10004,0x10006};
    for(size_t i=0;i<sizeof(versions)/sizeof(versions[0]);i++){
        void *actual=NULL;assert(get_env(jni_vm(),&actual,versions[i])==0);
        assert(actual==jni_environment());
    }
    void *actual=env;assert(get_env(jni_vm(),&actual,0x10008)==-3 && actual==NULL);
    assert(get_env(jni_vm(),NULL,0x10002)==-1);
    assert(get_env(jni_vm(),NULL,0x10008)==-1);
    /* Reproduce the original _InitAT request, then use its returned table. */
    env=NULL;assert(get_env(jni_vm(),&env,0x10002)==0 && env!=NULL);
    table=*(uintptr_t**)env;
    int (*push)(void*,int)=(void*)table[19];Obj *(*pop)(void*,Obj*)=(void*)table[20];
    Obj *(*find)(void*,const char*)=(void*)table[6];
    Method *(*method)(void*,Obj*,const char*,const char*)=(void*)table[33];
    Method *(*static_method)(void*,Obj*,const char*,const char*)=(void*)table[113];
    Obj *(*global)(void*,Obj*)=(void*)table[21];void (*drop_global)(void*,Obj*)=(void*)table[22];
    void (*drop_local)(void*,Obj*)=(void*)table[23];Obj *(*new_local)(void*,Obj*)=(void*)table[25];
    Obj *(*bytes)(void*,int)=(void*)table[176];Obj *(*shorts)(void*,int)=(void*)table[178];
    void *(*critical)(void*,Obj*,unsigned char*)=(void*)table[222];
    void (*unpin)(void*,Obj*,void*,int)=(void*)table[223];
    Obj *cls=find(env,"android/media/AudioTrack");
    Method *ctor=method(env,cls,"<init>","(IIIIII)V");
    Method *query=static_method(env,cls,"getMinBufferSize","(III)I");
    Method *play=method(env,cls,"play","()V"),*pause=method(env,cls,"pause","()V");
    Method *stop=method(env,cls,"stop","()V"),*release=method(env,cls,"release","()V");
    Method *write=method(env,cls,"write","([BII)I");
    assert(static_int_v(cls,query,44100,12,2)==4096);
    assert(jni_audio_mock.query[0]==44100 && jni_audio_mock.query[1]==12 && jni_audio_mock.query[2]==2);
    assert(static_int_v(cls,query,48000,12,2)==SF_AUDIO_BAD_VALUE);
    if(!strcmp(argv[1],"vm-versions")){assert(((int (*)(void*))vm[5])(jni_vm())==0);return 0;}
    assert(push(env,2)==0);
    jni_audio_mock.platform_error=(int)0x80260007u;
    if(!strcmp(argv[1],"create-failure"))jni_audio_mock.fail_create=1;
    if(!strcmp(argv[1],"unknown-constructor"))ctor=method(env,cls,"<init>","(IIIIIII)V");
    Obj *track=create_v(cls,ctor,3,44100,12,2,4096,1,0);
    int expected[]={3,44100,12,2,4096,1};assert(!memcmp(jni_audio_mock.ctor,expected,sizeof(expected)));
    assert(track->kind==AUDIO_TRACK && track->refs==1);
    Obj *pcm=bytes(env,4104);
    unsigned char copied=1;unsigned char *data=critical(env,pcm,&copied);
    assert(copied==0 && data==(unsigned char*)pcm->data && pcm->critical_pins==1 && pcm->refs==2);
    for(int i=0;i<4104;i++)data[i]=(unsigned char)(i*17);
    unpin(env,pcm,data,0);assert(pcm->critical_pins==0 && pcm->refs==1);
    /* Both additional modes are ignored for this direct, uncopied storage. */
    data=critical(env,pcm,NULL);data[0]=77;unpin(env,pcm,data,2);assert(pcm->data[0]==77);
    data=critical(env,pcm,NULL);void *nested=critical(env,pcm,NULL);
    assert(data==nested && pcm->critical_pins==2);unpin(env,pcm,nested,1);unpin(env,pcm,data,0);
    assert(pcm->critical_pins==0 && pcm->refs==1);
    if(!strcmp(argv[1],"critical-pointer")){data=critical(env,pcm,NULL);unpin(env,pcm,data+1,0);}
    if(!strcmp(argv[1],"critical-mode")){data=critical(env,pcm,NULL);unpin(env,pcm,data,3);}
    if(!strcmp(argv[1],"critical-unacquired"))unpin(env,pcm,pcm->data,0);
    void_v(track,cls,play);assert(jni_audio_mock.plays==1);
    if(!strcmp(argv[1],"write-failure"))jni_audio_mock.fail_write=1;
    if(!strcmp(argv[1],"short-write"))jni_audio_mock.short_write=1;
    if(!strcmp(argv[1],"unknown-method"))write=method(env,cls,"write","([SII)I");
    if(!strcmp(argv[1],"wrong-class"))cls=find(env,"android/media/UnsupportedTrack");
    assert(int_v(track,cls,write,pcm,7,4096)==4096);
    assert(jni_audio_mock.writes==1 && jni_audio_mock.written_bytes==4096);
    assert(!memcmp(jni_audio_mock.written,pcm->data+7,4096));
    int invalid[][2]={{-1,4096},{0,-1},{9,4096},{0,4105},{INT_MAX,1},{1,INT_MAX}};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)
        assert(int_v(track,cls,write,pcm,invalid[i][0],invalid[i][1])==SF_AUDIO_BAD_VALUE);
    assert(int_v(track,cls,write,NULL,0,4096)==SF_AUDIO_BAD_VALUE);
    Obj *wrong_array=shorts(env,4096);assert(int_v(track,cls,write,wrong_array,0,4096)==SF_AUDIO_BAD_VALUE);
    drop_local(env,wrong_array);assert(jni_audio_mock.writes==1);
    assert(int_v(track,cls,write,pcm,4104,0)==0);
    void_v(track,cls,pause);assert(int_v(track,cls,write,pcm,0,4096)==0);
    void_v(track,cls,play);jni_audio_mock.paused_resume=1;
    assert(int_v(track,cls,write,pcm,0,4096)==0);jni_audio_mock.paused_resume=0;
    void_v(track,cls,stop);assert(jni_audio_mock.stops==1);
    if(!strcmp(argv[1],"release-failure"))jni_audio_mock.fail_release=1;
    void_v(track,cls,release);void_v(track,cls,release);
    assert(jni_audio_mock.closed==1 && jni_audio_mock.destroyed==0);
    assert(pop(env,NULL)==NULL);assert(jni_audio_mock.destroyed==1);

    /* A global ref survives PopLocalFrame; release closes independently. */
    assert(push(env,1)==0);track=create_v(cls,ctor,3,44100,12,2,4096,1);
    Obj *keep=global(env,track);assert(track->refs==2);assert(pop(env,NULL)==NULL);
    assert(keep->refs==1 && jni_audio_mock.destroyed==1);
    void_v(keep,cls,release);drop_global(env,keep);assert(jni_audio_mock.destroyed==2);

    /* Promote the return ref through nested frames, then delete all aliases. */
    assert(push(env,1)==0);assert(push(env,1)==0);track=create_v(cls,ctor,3,44100,12,2,4096,1);
    Obj *result=pop(env,track);assert(result==track && track->refs==1);
    Obj *alias=new_local(env,result);assert(track->refs==2);drop_local(env,alias);
    result=pop(env,result);assert(result==track && track->refs==1);
    drop_local(env,result);assert(jni_audio_mock.destroyed==3 && jni_audio_mock.closed==3);

    /* VM detach releases remaining base-frame objects of this native thread. */
    track=create_v(cls,ctor,3,44100,12,2,4096,1);(void)track;
    assert(((int (*)(void*))vm[5])(jni_vm())==0);
    assert(jni_audio_mock.destroyed==4 && jni_audio_mock.closed==4);
    track=create_v(cls,ctor,3,44100,12,2,4096,1);
    pthread_t worker;assert(pthread_create(&worker,NULL,worker_frame,NULL)==0);
    assert(pthread_join(worker,NULL)==0);
    assert(jni_audio_mock.destroyed==5 && jni_audio_mock.closed==5);
    assert(track->refs==1 && sf_audio_track_state(track->audio)==SF_AUDIO_STATE_INITIALIZED);
    drop_local(env,track);assert(jni_audio_mock.destroyed==6 && jni_audio_mock.closed==6);
    assert(((int (*)(void*))vm[5])(jni_vm())==0);
    return 0;
}
