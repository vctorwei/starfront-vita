/* Test the real JNI table slots, including native deletion before release. */
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

int main(void) {
    jni_bridge_init();
    void *env=jni_environment();
    uintptr_t *table=*(uintptr_t**)env;
    Obj *(*create)(void*,const char*)=(void*)table[167];
    const char *(*acquire)(void*,Obj*,unsigned char*)=(void*)table[169];
    void (*release)(void*,Obj*,const char*)=(void*)table[170];
    void (*delete_local)(void*,Obj*)=(void*)table[23];
    /* Old Dalvik leaves isCopy untouched for a NULL jstring and returns NULL
       without an exception. This is the native getIMEI unavailable-ID path. */
    unsigned char null_copy=0x7f;
    assert(acquire(env,NULL,&null_copy)==NULL && null_copy==0x7f);
    assert(acquire(env,NULL,NULL)==NULL);
    assert(((Obj *(*)(void*))table[15])(env)==NULL);
    assert(((int (*)(void*))table[228])(env)==0);
    release(env,NULL,NULL);
    const char *expected="Savegame \xe4\xb8\xad\xe6\x96\x87 / PS Vita";
    Obj *local=create(env,expected);
    unsigned char copy_a=0,copy_b=0;
    const char *a=acquire(env,local,&copy_a);
    const char *b=acquire(env,local,&copy_b);
    assert(copy_a==1 && copy_b==1 && a!=b);
    assert(a!=local->data && b!=local->data);
    delete_local(env,local);
    /* Both buffers stay valid after the original Obj and its data are freed. */
    assert(strcmp(a,expected)==0 && strcmp(b,expected)==0);
    release(env,local,a);
    assert(strcmp(b,expected)==0);
    release(env,local,b);

    local=create(env,"");
    const char *empty=acquire(env,local,NULL);
    delete_local(env,local);
    assert(empty[0]=='\0');
    release(env,local,empty);

    /* Repeated acquisition/release must not require leaked Obj ownership. */
    for(int i=0;i<1000;i++){
        local=create(env,expected);
        a=acquire(env,local,NULL);
        release(env,local,a);
        delete_local(env,local);
    }
    return 0;
}
