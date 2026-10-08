#include <assert.h>
#include "../src/jni.c"

static const char *record_path;
void port_log(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vfprintf(stdout,fmt,ap);va_end(ap);fputc('\n',stdout);
}
void fatal_error(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);exit(42);
}
int port_map_path(const char *input,char *output,size_t size) {
    assert(!strcmp(input,"/sdcard/gameloft/games/GloftSFHP/androidTrophy.dat"));
    int n=snprintf(output,size,"%s",record_path);return n<0 || (size_t)n>=size?-1:0;
}
static void invoke_v(void *env,Obj *cls,Method *method,...) {
    va_list ap;va_start(ap,method);
    ((void (*)(void*,Obj*,Method*,va_list))(*(uintptr_t**)env)[142])(env,cls,method,ap);
    va_end(ap);
}
int main(int argc,char **argv) {
    assert(argc==5);record_path=argv[1];int id=atoi(argv[2]),slot=atoi(argv[3]);
    jni_bridge_init();void *env=jni_environment();uintptr_t *table=*(uintptr_t**)env;
    Obj *cls=((Obj *(*)(void*,const char*))table[6])(env,
        !strcmp(argv[4],"owner")?"com/example/Game":SF_GAME_CLASS);
    Method *method=((Method *(*)(void*,Obj*,const char*,const char*))table[113])(
        env,cls,"notifyTrophy",!strcmp(argv[4],"signature")?"(I)I":"(I)V");
    if(!strcmp(argv[4],"receiver"))cls=NULL;
    if(slot==141)((void (*)(void*,Obj*,Method*,...))table[slot])(env,cls,method,id);
    else if(slot==142)invoke_v(env,cls,method,id);
    else {assert(slot==143);Value args[1]={{.i=id}};
        ((void (*)(void*,Obj*,Method*,const Value*))table[slot])(env,cls,method,args);}
    return 0;
}
