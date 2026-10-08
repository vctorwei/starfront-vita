/* Real JNI table calls from ALicenseCheck's original missing-device-ID path. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "../src/jni.c"

static int unavailable_logs,tracking_logs,reorientation_logs;
void port_log(const char *fmt,...) {
    if(strstr(fmt,"getDeviceId: unavailable"))unavailable_logs++;
    if(strstr(fmt,"Game.d:"))tracking_logs++;
    if(!strcmp(fmt,"Platform UI: %s")){
        va_list ap;va_start(ap,fmt);const char *name=va_arg(ap,const char*);va_end(ap);
        if(!strcmp(name,"SetReorientation"))reorientation_logs++;
    }
}
void fatal_error(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);
    fputc('\n',stderr);exit(42);
}
int port_map_path(const char *in,char *out,size_t size){(void)in;(void)out;(void)size;return -1;}

static Obj *invoke_v(uintptr_t *table,int slot,void *env,Obj *o,Method *m,...) {
    va_list ap;va_start(ap,m);
    Obj *r=((Obj *(*)(void*,Obj*,Method*,va_list))table[slot])(env,o,m,ap);
    va_end(ap);return r;
}
static void invoke_void_v(uintptr_t *table,void *env,Obj *o,Method *m,...) {
    va_list ap;va_start(ap,m);
    ((void (*)(void*,Obj*,Method*,va_list))table[142])(env,o,m,ap);va_end(ap);
}

int main(int argc,char **argv) {
    assert(argc==2);jni_bridge_init();void *env=jni_environment();uintptr_t *table=*(uintptr_t**)env;
    Obj *(*find)(void*,const char*)=(void*)table[6];
    Method *(*method)(void*,Obj*,const char*,const char*)=(void*)table[33];
    Method *(*static_method)(void*,Obj*,const char*,const char*)=(void*)table[113];
    Obj *(*class_of)(void*,Obj*)=(void*)table[31];
    Obj *(*call_static)(void*,Obj*,Method*,...)=(void*)table[114];
    Obj *(*call_object)(void*,Obj*,Method*,...)=(void*)table[34];
    void (*drop)(void*,Obj*)=(void*)table[23];
    int (*push)(void*,int)=(void*)table[19];Obj *(*pop)(void*,Obj*)=(void*)table[20];
    Obj *(*global)(void*,Obj*)=(void*)table[21];void (*global_drop)(void*,Obj*)=(void*)table[22];
    Field *(*field_id)(void*,Obj*,const char*,const char*)=(void*)table[144];
    Obj *(*read_field)(void*,Obj*,Field*)=(void*)table[145];
    const char *(*utf)(void*,Obj*,unsigned char*)=(void*)table[169];
    Obj *game=find(env,SF_GAME_CLASS);
    const char *da_sig=!strcmp(argv[1],"wrong-da-signature")?"()Ljava/lang/Object;":"()Landroid/app/Activity;";
    Obj *da_owner=game;
    if(!strcmp(argv[1],"wrong-da-owner"))da_owner=find(env,"com/example/Game");
    Method *da=static_method(env,da_owner,"da",da_sig);
    if(!strcmp(argv[1],"wrong-static-receiver"))call_static(env,NULL,da);
    assert(push(env,8)==0);
    Obj *activity=invoke_v(table,115,env,da_owner,da);
    assert(activity && activity->kind==ACTIVITY && activity->refs==2);
    Obj *second=call_static(env,game,da);assert(second==activity && activity->refs==3);
    Obj *runtime_class=class_of(env,activity);assert(runtime_class==game);
    Obj *context=find(env,"android/content/Context");
    Field *phone_field=field_id(env,context,"TELEPHONY_SERVICE","Ljava/lang/String;");
    Obj *phone=read_field(env,context,phone_field);
    const char *service_sig=!strcmp(argv[1],"wrong-service-signature")?
        "(Ljava/lang/String;)Ljava/lang/String;":"(Ljava/lang/String;)Ljava/lang/Object;";
    Method *service=method(env,runtime_class,"getSystemService",service_sig);
    if(!strcmp(argv[1],"unknown-service"))phone=((Obj *(*)(void*,const char*))table[167])(env,"wifi");
    if(!strcmp(argv[1],"wrong-activity-receiver"))call_object(env,game,service,phone);
    Obj *manager=invoke_v(table,35,env,activity,service,phone);
    assert(manager && manager->kind==TELEPHONY_MANAGER && manager->refs==2);
    Obj *manager_class=class_of(env,manager);assert(!strcmp(manager_class->data,SF_TELEPHONY_CLASS));
    const char *id_sig=!strcmp(argv[1],"wrong-id-signature")?"()Ljava/lang/Object;":"()Ljava/lang/String;";
    Obj *id_owner=manager_class;
    if(!strcmp(argv[1],"wrong-id-owner"))id_owner=game;
    Method *get_id=method(env,id_owner,"getDeviceId",id_sig);
    if(!strcmp(argv[1],"wrong-id-receiver"))call_object(env,activity,get_id);
    Obj *id=invoke_v(table,35,env,manager,get_id);
    assert(id==NULL && unavailable_logs==1);
    unsigned char copy=0x7f;assert(utf(env,id,&copy)==NULL && copy==0x7f);
    if(!strcmp(argv[1],"invalid-utf-object"))utf(env,manager,NULL);
    /* The original getIMEI cleared its destination before requesting UTF;
       a NULL result selects the branch that leaves every byte cleared. */
    unsigned char imei[255];memset(imei,0,sizeof(imei));
    const char *chars=utf(env,id,NULL);if(chars)memcpy(imei,chars,strlen(chars));
    for(size_t i=0;i<sizeof(imei);i++)assert(imei[i]==0);
    drop(env,phone);assert(phone_field->value->refs==1);
    assert(pop(env,activity)==activity);
    assert(activity->refs==2 && manager->refs==1);
    Obj *retained=global(env,activity);drop(env,activity);assert(retained->refs==2);
    assert(push(env,4)==0);
    assert(call_static(env,game,da)==retained && retained->refs==3);
    assert(pop(env,NULL)==NULL && retained->refs==2);
    global_drop(env,retained);assert(application_activity->refs==1);
    const char *d_sig=!strcmp(argv[1],"wrong-d-signature")?"()I":"()V";
    Method *tracking=static_method(env,game,"d",d_sig);
    if(!strcmp(argv[1],"wrong-d-receiver"))invoke_void_v(table,env,application_activity,tracking);
    invoke_void_v(table,env,game,tracking);
    for(int i=0;i<1000;i++)((void (*)(void*,Obj*,Method*))table[141])(env,game,tracking);
    assert(tracking_logs==1001 && telephony_manager->refs==1);
    Method *orientation=static_method(env,game,"SetReorientation","(I)V");
    assert(orientation==static_method(env,game,"SetReorientation","(I)V"));
    invoke_void_v(table,env,game,orientation,0);
    for(int i=0;i<1000;i++)((void (*)(void*,Obj*,Method*,...))table[141])(env,game,orientation,i);
    assert(reorientation_logs==1);
    if(!strcmp(argv[1],"wrong-dc-signature")){
        Method *dc=static_method(env,game,"dc","()I");call_static(env,game,dc);
    }
    drop(env,game);return 0;
}
