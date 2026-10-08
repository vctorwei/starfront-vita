/* The observed ALicenseCheck field lookup, including immutable value refs. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "../src/jni.c"

void port_log(const char *fmt,...) {(void)fmt;}
void fatal_error(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);
    fputc('\n',stderr);exit(42);
}
int port_map_path(const char *in,char *out,size_t size){(void)in;(void)out;(void)size;return -1;}

int main(int argc,char **argv) {
    assert(argc==2);jni_bridge_init();void *env=jni_environment();uintptr_t *table=*(uintptr_t**)env;
    Obj *(*find)(void*,const char*)=(void*)table[6];
    Field *(*field_id)(void*,Obj*,const char*,const char*)=(void*)table[144];
    Obj *(*read_field)(void*,Obj*,Field*)=(void*)table[145];
    void (*drop)(void*,Obj*)=(void*)table[23];
    int (*push)(void*,int)=(void*)table[19];Obj *(*pop)(void*,Obj*)=(void*)table[20];
    const char *(*utf)(void*,Obj*,unsigned char*)=(void*)table[169];
    void (*utf_release)(void*,Obj*,const char*)=(void*)table[170];
    const char *owner="android/content/Context",*name="TELEPHONY_SERVICE",*sig="Ljava/lang/String;";
    if(!strcmp(argv[1],"unknown-owner"))owner="android/media/AudioTrack";
    if(!strcmp(argv[1],"unknown-name"))name="SOME_SERVICE";
    if(!strcmp(argv[1],"unknown-signature"))sig="I";
    Obj *cls=find(env,owner);unsigned before=cls->refs;
    Field *f=field_id(env,cls,name,sig);assert(f==field_id(env,cls,name,sig));
    assert(cls->refs==before+1 && f->value->refs==1);
    if(!strcmp(argv[1],"wrong-receiver")){Obj *other=find(env,"android/app/Activity");read_field(env,other,f);}
    if(!strcmp(argv[1],"invalid-field-id"))read_field(env,cls,NULL);
    if(!strcmp(argv[1],"wrong-read-slot"))((int (*)(void*,Obj*,Field*))table[150])(env,cls,f);
    assert(push(env,2)==0);Obj *a=read_field(env,cls,f),*b=read_field(env,cls,f);
    assert(a==b && a->kind==STRING && a->length==5 && a->refs==3);
    unsigned char copy=0;const char *characters=utf(env,a,&copy);
    assert(copy==1 && !strcmp(characters,"phone"));
    assert(pop(env,NULL)==NULL && f->value->refs==1);
    assert(!strcmp(characters,"phone"));utf_release(env,a,characters);
    Obj *c=read_field(env,cls,f);assert(c==f->value && c->refs==2);drop(env,c);
    assert(f->value->refs==1);
    drop(env,cls);return 0;
}
