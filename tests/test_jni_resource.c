/* Exercise the actual JNI dispatch, array length, region, and local release. */
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
int port_map_path(const char *in,char *out,size_t size) {
    (void)in;(void)out;(void)size;return -1;
}

int main(int argc,char **argv) {
    assert(argc==3);
    jni_bridge_init();
    const char *owner=!strcmp(argv[2],"wrong-owner") ?
        "com/gameloft/android/ANMP/GloftSFHP/ML/GameRenderer" :
        "com/gameloft/android/ANMP/GloftSFHP/ML/Game";
    Obj *cls=jni_class(owner);
    Method *m=method_id(jni_environment(),cls,"db",
        !strcmp(argv[2],"wrong-signature") ? "()I" : "()[B");
    /* Match CallStaticObjectMethodV used by ALicenseCheck::LoadConfig. */
    Obj *array=(Obj*)call(jni_environment(),cls,m);
    assert(array->kind==ARRAY && array->stride==1);
    assert(array_length(jni_environment(),array)==262144);
    unsigned char *actual=malloc(262144),*expected=malloc(262144);
    assert(actual && expected);
    get_region(jni_environment(),array,0,262144,actual);
    FILE *f=fopen(argv[1],"rb");assert(f);
    assert(fread(expected,1,262144,f)==262144);assert(fgetc(f)==EOF);
    assert(fclose(f)==0);
    assert(memcmp(actual,expected,262144)==0);
    /* Local deletion must release the object after the native copy. */
    assert(array->refs==1);
    local_delete(jni_environment(),array);
    local_delete(jni_environment(),cls);
    free(actual);free(expected);
    return 0;
}
