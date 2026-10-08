/* Minimal JNI 1.6 ABI for the entry points observed in this APK.
   Unknown methods/slots fail explicitly; this is not a general Java VM. */
#include "port.h"
#include "audio_track.h"
#include "drm_preferences.h"
#include <vitasdk.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef SF_APK_CONFIG_PATH
#define SF_APK_CONFIG_PATH DATA_DIR "/apk/igli.bin"
#endif
#define SF_APK_CONFIG_SIZE 262144u
#ifndef SF_APK_SERIAL_PATH
#define SF_APK_SERIAL_PATH DATA_DIR "/apk/serialkey.txt"
#endif
#define SF_APK_SERIAL_SIZE 11u
#define SF_GAME_CLASS "com/gameloft/android/ANMP/GloftSFHP/ML/Game"
#define SF_TELEPHONY_CLASS "android/telephony/TelephonyManager"

typedef struct Obj { unsigned refs, critical_pins, audio_writes; int kind, length, stride, audio_nonzero_logged; char *data; SfAudioTrack *audio; } Obj;
typedef struct Method { Obj *owner; char *name, *sig; unsigned reorientation_logged; } Method;
typedef struct Field { Obj *owner, *value; } Field;
typedef union { int i; unsigned char z; float f; double d; long long j; Obj *l; } Value;
enum { CLASS=1, STRING, ARRAY, AUDIO_TRACK, ACTIVITY, TELEPHONY_MANAGER };
typedef struct LocalFrame { struct LocalFrame *parent; Obj **objects; size_t count, capacity; } LocalFrame;
typedef struct ThreadRefs { LocalFrame *frame; } ThreadRefs;
static uintptr_t env_table[233],vm_table[8];
static uintptr_t *env_ptr=env_table,*vm_ptr=vm_table;
static Obj *classes[64];static unsigned nclasses;
static Method methods[256];static unsigned nmethods;
static Field telephony_service_field;
/* The original Game.g and Context's cached service manager retain these
   application-lifetime objects independently of native local references. */
static Obj *application_activity,*telephony_manager;
static SfDrmPreferences *drm_preferences;
static pthread_mutex_t drm_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t trophy_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t obj_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t refs_key;
static pthread_once_t refs_once=PTHREAD_ONCE_INIT;
static char saved_names[256][256];static int saved_count,saved_index;

/* Game.notifyTrophy updates an Android-local record, not a platform trophy.
   GLiveMain.<clinit> allocates 100 entries, each initially 127. */
#define SF_TROPHY_COUNT 100
static int trophy_read_int(FILE *file,int *value) {
    int c=fgetc(file),negative=0,digits=0;
    uint32_t magnitude=0;
    if(c==EOF)return ferror(file)?-1:0;
    if(c=='-' || c=='+'){negative=c=='-';c=fgetc(file);}
    uint32_t limit=negative?2147483648u:2147483647u;
    while(c!=EOF && c!='\r' && c!='\n') {
        if(c<'0' || c>'9' || magnitude>(limit-(unsigned)(c-'0'))/10u)return -1;
        magnitude=magnitude*10u+(unsigned)(c-'0');digits=1;c=fgetc(file);
    }
    if(!digits || ferror(file))return -1;
    if(c=='\r'){c=fgetc(file);if(c!=EOF && c!='\n')ungetc(c,file);}
    *value=negative?(int)(-(int64_t)magnitude):(int)magnitude;
    return 1;
}
static int trophy_path_absent(const char *path) {
    struct stat st;
    if(stat(path,&st)==0){errno=EEXIST;return 0;}
    return errno==ENOENT;
}
static int trophy_record(const char *path,int id) {
    int values[SF_TROPHY_COUNT],exists=0;
    for(int i=0;i<SF_TROPHY_COUNT;i++)values[i]=127;
    FILE *file=fopen(path,"rb");
    if(file) {
        exists=1;int count=0,value,status;
        while((status=trophy_read_int(file,&value))>0) {
            if(count==SF_TROPHY_COUNT){status=-1;break;}
            values[count++]=value;
        }
        int close_result=fclose(file);
        if(status<0){errno=EINVAL;return -1;}
        if(close_result)return -1;
    } else if(errno!=ENOENT)return -1;
    if(exists && values[id]==1)return 0;
    values[id]=1;
    char temporary[1032],backup[1032];
    snprintf(temporary,sizeof(temporary),"%s.tmp",path);
    snprintf(backup,sizeof(backup),"%s.bak",path);
    /* Preserve a recovery copy if a previous commit/restore was interrupted.
       Vita newlib rename removes its destination, so only move to absent paths. */
    if(!trophy_path_absent(backup))return -1;
    file=fopen(temporary,"wb");if(!file)return -1;
    int error=0;
    for(int i=0;i<SF_TROPHY_COUNT;i++)
        if(fprintf(file,"%d\n",values[i])<0){error=errno?errno:EIO;break;}
    if(fclose(file) && !error)error=errno?errno:EIO;
    int backed_up=0;
    if(!error && exists) {
        if(rename(path,backup))error=errno;
        else backed_up=1;
    }
    if(!error && (!trophy_path_absent(path) || rename(temporary,path)))error=errno;
    if(error) {
        if(backed_up && trophy_path_absent(path))rename(backup,path);
        remove(temporary);errno=error;return -1;
    }
    if(backed_up && remove(backup))return -1;
    return 0;
}
static void notify_trophy(int id) {
    /* Java's id==length path throws and is caught; it must not index past100. */
    if(id<0 || id>=SF_TROPHY_COUNT){port_log("JNI notifyTrophy ignored out-of-range id=%d",id);return;}
    char path[1024];
    if(port_map_path("/sdcard/gameloft/games/GloftSFHP/androidTrophy.dat",path,sizeof(path))) {
        port_log("JNI notifyTrophy(%d): path unavailable",id);return;
    }
    pthread_mutex_lock(&trophy_lock);
    int result=trophy_record(path,id),error=errno;
    pthread_mutex_unlock(&trophy_lock);
    if(result)port_log("JNI notifyTrophy(%d): local record unavailable (errno=%d)",id,error);
    else port_log("JNI notifyTrophy(%d): local achievement recorded",id);
    /* Original Game.notifyTrophy catches file/parse exceptions and returns. */
}

static void object_unref(Obj *o) {
    if(!o)return;
    pthread_mutex_lock(&obj_lock);int release=!--o->refs;pthread_mutex_unlock(&obj_lock);
    if(release){
        if(o->audio && sf_audio_track_destroy(&o->audio)<0)
            fatal_error("AudioTrack destroy failed: platform 0x%08x",(unsigned)sf_audio_track_last_platform_error(o->audio));
        free(o->data);free(o);
    }
}
static void refs_destroy(void *data) {
    ThreadRefs *refs=data;
    if(!refs)return;
    while(refs->frame){
        LocalFrame *f=refs->frame;refs->frame=f->parent;
        for(size_t i=0;i<f->count;i++)object_unref(f->objects[i]);
        free(f->objects);free(f);
    }
    free(refs);
}
static void refs_key_init(void){if(pthread_key_create(&refs_key,refs_destroy))fatal_error("JNI thread references unavailable");}
static LocalFrame *frame_new(LocalFrame *parent,int capacity) {
    if(capacity<0)fatal_error("Invalid JNI local frame capacity");
    LocalFrame *f=calloc(1,sizeof(*f));if(!f)fatal_error("JNI local frame allocation failed");
    f->capacity=capacity?(size_t)capacity:1;f->parent=parent;
    f->objects=calloc(f->capacity,sizeof(*f->objects));
    if(!f->objects)fatal_error("JNI local reference allocation failed");
    return f;
}
static ThreadRefs *thread_refs(void) {
    if(pthread_once(&refs_once,refs_key_init))fatal_error("JNI thread initialization failed");
    ThreadRefs *refs=pthread_getspecific(refs_key);
    if(!refs){
        refs=calloc(1,sizeof(*refs));if(!refs)fatal_error("JNI thread allocation failed");
        refs->frame=frame_new(NULL,16);
        if(pthread_setspecific(refs_key,refs))fatal_error("JNI thread references unavailable");
    }
    return refs;
}
static Obj *local_add(Obj *o) {
    if(!o)return NULL;
    LocalFrame *f=thread_refs()->frame;
    for(size_t i=0;i<f->count;i++)if(!f->objects[i]){f->objects[i]=o;return o;}
    if(f->count==f->capacity){
        if(f->capacity>SIZE_MAX/2/sizeof(*f->objects))fatal_error("JNI local reference capacity overflow");
        size_t capacity=f->capacity*2;
        Obj **objects=realloc(f->objects,capacity*sizeof(*objects));
        if(!objects)fatal_error("JNI local reference allocation failed");
        f->objects=objects;f->capacity=capacity;
    }
    f->objects[f->count++]=o;return o;
}
static Obj *object_new(int kind,int length,int stride) {
    if(length<0 || length>16*1024*1024 || stride<1 || stride>8)fatal_error("Invalid JNI allocation");
    Obj *o=calloc(1,sizeof(*o));if(!o)fatal_error("JNI object allocation failed");
    o->data=calloc((size_t)length+1,stride);if(!o->data)fatal_error("JNI data allocation failed");
    o->kind=kind;o->length=length;o->stride=stride;o->refs=1;return local_add(o);
}
static Obj *text_new(int kind,const char *s){Obj *o=object_new(kind,strlen(s),1);memcpy(o->data,s,o->length);return o;}
static Obj *ref_new(void *env,Obj *o){if(o){pthread_mutex_lock(&obj_lock);o->refs++;pthread_mutex_unlock(&obj_lock);}return o;}
static void ref_delete(void *env,Obj *o){object_unref(o);}
static Obj *local_new(void *env,Obj *o){return local_add(ref_new(env,o));}
static void local_delete(void *env,Obj *o) {
    if(!o)return;
    for(LocalFrame *f=thread_refs()->frame;f;f=f->parent)
        for(size_t i=0;i<f->count;i++)if(f->objects[i]==o){f->objects[i]=NULL;object_unref(o);return;}
    fatal_error("Invalid JNI local reference");
}
static int push_frame(void *env,int capacity){ThreadRefs *refs=thread_refs();refs->frame=frame_new(refs->frame,capacity);return 0;}
static Obj *pop_frame(void *env,Obj *result) {
    ThreadRefs *refs=thread_refs();LocalFrame *f=refs->frame;
    if(!f->parent)fatal_error("JNI PopLocalFrame without PushLocalFrame");
    ref_new(env,result);refs->frame=f->parent;
    for(size_t i=0;i<f->count;i++)object_unref(f->objects[i]);
    free(f->objects);free(f);return local_add(result);
}
static Obj *find_class(void *env,const char *name) {
    pthread_mutex_lock(&obj_lock);
    for(unsigned i=0;i<nclasses;i++)if(!strcmp(name,classes[i]->data)){classes[i]->refs++;Obj *c=classes[i];pthread_mutex_unlock(&obj_lock);return local_add(c);}
    if(nclasses==64)fatal_error("JNI class table full");
    Obj *c=text_new(CLASS,name);c->refs++;classes[nclasses++]=c;
    pthread_mutex_unlock(&obj_lock);port_log("JNI class: %s",name);return c;
}
static Obj *object_class(void *env,Obj *o){
    if(o && o->kind==CLASS)return local_new(env,o);
    if(o && o->kind==AUDIO_TRACK)return find_class(env,"android/media/AudioTrack");
    if(o && o->kind==ACTIVITY)return find_class(env,SF_GAME_CLASS);
    if(o && o->kind==TELEPHONY_MANAGER)return find_class(env,SF_TELEPHONY_CLASS);
    fatal_error("GetObjectClass on unsupported object");
}
static Method *method_id(void *env,Obj *cls,const char *name,const char *sig) {
    pthread_mutex_lock(&obj_lock);
    for(unsigned i=0;i<nmethods;i++)if(methods[i].owner==cls&&!strcmp(methods[i].name,name)&&!strcmp(methods[i].sig,sig)){pthread_mutex_unlock(&obj_lock);return methods+i;}
    if(nmethods==256 || !cls)fatal_error("Invalid JNI method registration");
    Method *m=methods+nmethods++;m->owner=cls;cls->refs++;m->name=strdup(name);m->sig=strdup(sig);
    pthread_mutex_unlock(&obj_lock);port_log("JNI method: %s.%s%s",cls->data,name,sig);return m;
}
static Field *static_field_id(void *env,Obj *cls,const char *name,const char *sig) {
    /* ALicenseCheck::getIMEI asks for this Android framework constant. It is
       only a service lookup key, not evidence that a telephony service exists. */
    if(!cls || cls->kind!=CLASS || !name || !sig ||
       strcmp(cls->data,"android/content/Context") || strcmp(name,"TELEPHONY_SERVICE") ||
       strcmp(sig,"Ljava/lang/String;"))
        fatal_error("Unimplemented JNI static field:\n%s.%s%s",
            cls && cls->kind==CLASS?cls->data:"(invalid class)",name?name:"(no field)",sig?sig:"");
    Obj *temporary=NULL;
    pthread_mutex_lock(&obj_lock);
    if(!telephony_service_field.owner){
        telephony_service_field.owner=cls;cls->refs++;
        temporary=text_new(STRING,"phone");temporary->refs++;
        telephony_service_field.value=temporary;
    }
    pthread_mutex_unlock(&obj_lock);
    if(temporary){
        local_delete(env,temporary);
        port_log("JNI static field: android/content/Context.TELEPHONY_SERVICE:Ljava/lang/String; = phone");
    }
    return &telephony_service_field;
}
static Obj *get_static_object_field(void *env,Obj *cls,Field *field) {
    if(field!=&telephony_service_field || !telephony_service_field.owner ||
       cls!=telephony_service_field.owner)
        fatal_error("Unsupported JNI static object field receiver/id");
    return local_new(env,field->value);
}
static void collect_args(Method *m,va_list ap,Value *a) {
    if(!m || !m->sig || m->sig[0]!='(')fatal_error("Invalid JNI method arguments");
    const char *s=m->sig+1;int n=0;
    while(*s && *s!=')') {
        if(n>=16)fatal_error("Too many JNI arguments");
        switch(*s) {
            case '[': while(*s=='[')s++;if(*s=='L'){while(*s&&*s!=';')s++;}a[n].l=va_arg(ap,Obj*);break;
            case 'L':while(*s&&*s!=';')s++;a[n].l=va_arg(ap,Obj*);break;
            case 'D':a[n].d=va_arg(ap,double);break;
            case 'F':a[n].f=(float)va_arg(ap,double);break;
            case 'J':a[n].j=va_arg(ap,long long);break;
            default:a[n].i=va_arg(ap,int);break;
        }n++;if(*s)s++;
    }
}
static const char *string_data(Obj *o){if(!o||o->kind!=STRING)fatal_error("Invalid JNI string");return o->data;}
static Obj *platform_object(Obj **retained,int kind) {
    pthread_mutex_lock(&obj_lock);
    if(*retained){
        Obj *o=*retained;o->refs++;pthread_mutex_unlock(&obj_lock);return local_add(o);
    }
    Obj *o=object_new(kind,0,1);o->refs++;*retained=o;
    pthread_mutex_unlock(&obj_lock);return o;
}
static int exact_method(Method *m,const char *owner,const char *name,const char *sig) {
    return m && m->owner && m->owner->kind==CLASS &&
        !strcmp(m->owner->data,owner) && !strcmp(m->name,name) && !strcmp(m->sig,sig);
}
static Obj *telephony_device_id(Obj *object) {
    if(!object || object->kind!=TELEPHONY_MANAGER || object!=telephony_manager)
        fatal_error("Invalid TelephonyManager query receiver");
    /* No Android modem/subscriber service supplies an ID on this platform. */
    return NULL;
}
static Obj *apk_config_array(void) {
    /* Game.db()[B reads this raw APK resource verbatim. The native code owns
       config decoding and license decisions; the bridge only supplies bytes. */
    FILE *f=fopen(SF_APK_CONFIG_PATH,"rb");
    if(!f)fatal_error("Cannot open APK resource:\n%s",SF_APK_CONFIG_PATH);
    if(fseek(f,0,SEEK_END)!=0){fclose(f);fatal_error("Cannot measure APK resource");}
    long size=ftell(f);
    if(size!=(long)SF_APK_CONFIG_SIZE){fclose(f);fatal_error("Invalid igli.bin size: %ld\nExpected %u",size,SF_APK_CONFIG_SIZE);}
    if(fseek(f,0,SEEK_SET)!=0){fclose(f);fatal_error("Cannot rewind APK resource");}
    Obj *a=object_new(ARRAY,(int)SF_APK_CONFIG_SIZE,1);
    size_t got=fread(a->data,1,SF_APK_CONFIG_SIZE,f);
    int failed=ferror(f),closed=fclose(f);
    if(got!=SF_APK_CONFIG_SIZE || failed || closed!=0){
        local_delete(NULL,a);
        fatal_error("Cannot read APK resource: %u/%u bytes",(unsigned)got,SF_APK_CONFIG_SIZE);
    }
    port_log("JNI Game.db()[B: %u bytes from APK res/raw/igli.bin",SF_APK_CONFIG_SIZE);
    return a;
}
static void drm_failure(const char *operation,int result,const SfDrmError *error) {
    fatal_error("DRM %s failed: result %d code %d\nSystem %d Crypto 0x%08x",
        operation,result,error->code,error->system_error,(unsigned)error->crypto_error);
}
static Obj *apk_serial_array(void) {
    /* Game.dc constructs Game.z before its resource-reading try/catch. Keep
       the original no-modem constructor state and persisted preferences;
       this callback never supplies a validation-success result. */
    pthread_mutex_lock(&drm_lock);
    SfDrmError error={0};int result;
    if(!drm_preferences) {
        result=sf_drm_preferences_prepare(SAVE_DIR "/GLoft.preferences",NULL,NULL,&drm_preferences,&error);
        if(result!=SF_DRM_OK || !drm_preferences){
            pthread_mutex_unlock(&drm_lock);drm_failure("constructor",result,&error);
        }
        SfDrmState state={0};result=sf_drm_preferences_state(drm_preferences,&state);
        if(result!=SF_DRM_OK){
            error.code=result;pthread_mutex_unlock(&drm_lock);drm_failure("constructor state",result,&error);
        }
        port_log("JNI Game.dc constructor: %s preferences, caught NULL-ID reload=%d, free count=%d, strings=%u",
            state.loaded_existing?"loaded":"fresh",state.reload_exception_caught,state.free_count,(unsigned)state.string_count);
    }
    FILE *f=fopen(SF_APK_SERIAL_PATH,"rb");
    if(!f){int e=errno;pthread_mutex_unlock(&drm_lock);fatal_error("Cannot open APK serial resource:\n%s\nSystem %d",SF_APK_SERIAL_PATH,e);}
    if(fseek(f,0,SEEK_END)!=0){int e=errno;fclose(f);pthread_mutex_unlock(&drm_lock);fatal_error("Cannot measure APK serial resource: system %d",e);}
    long size=ftell(f);
    if(size!=(long)SF_APK_SERIAL_SIZE){fclose(f);pthread_mutex_unlock(&drm_lock);fatal_error("Invalid serialkey.txt size: %ld\nExpected %u",size,SF_APK_SERIAL_SIZE);}
    if(fseek(f,0,SEEK_SET)!=0){int e=errno;fclose(f);pthread_mutex_unlock(&drm_lock);fatal_error("Cannot rewind APK serial resource: system %d",e);}
    unsigned char bytes[SF_APK_SERIAL_SIZE];
    size_t got=fread(bytes,1,sizeof(bytes),f);int failed=ferror(f),e=errno,closed=fclose(f);
    if(got!=sizeof(bytes) || failed || closed!=0){
        if(closed!=0)e=errno;
        pthread_mutex_unlock(&drm_lock);fatal_error("Cannot read APK serial resource: %u/%u bytes\nSystem %d",(unsigned)got,SF_APK_SERIAL_SIZE,e);
    }
    result=sf_drm_preferences_set_resource_serial(drm_preferences,bytes,sizeof(bytes),&error);
    if(result!=SF_DRM_OK){pthread_mutex_unlock(&drm_lock);drm_failure("resource serial assignment",result,&error);}
    pthread_mutex_unlock(&drm_lock);
    Obj *array=object_new(ARRAY,(int)sizeof(bytes),1);memcpy(array->data,bytes,sizeof(bytes));
    port_log("JNI Game.dc()[B: %u bytes from APK res/raw/serialkey.txt",SF_APK_SERIAL_SIZE);
    return array;
}
static void list_saves(const char *pattern) {
    saved_count=saved_index=0;
    const char *suffix=pattern;if(*suffix=='*')suffix++;
    SceUID d=sceIoDopen(GAME_DIR);if(d<0)return;
    SceIoDirent e;memset(&e,0,sizeof(e));
    while(sceIoDread(d,&e)>0 && saved_count<256) {
        size_t n=strlen(e.d_name),s=strlen(suffix);
        if(n<256&&n>=s&&!strcmp(e.d_name+n-s,suffix))memcpy(saved_names[saved_count++],e.d_name,n+1);
        memset(&e,0,sizeof(e));
    }sceIoDclose(d);
}
static int audio_method(Method *m,const char *name,const char *sig) {
    return m && m->owner && m->owner->kind==CLASS &&
        !strcmp(m->owner->data,"android/media/AudioTrack") &&
        !strcmp(m->name,name) && !strcmp(m->sig,sig);
}
static void audio_failure(Obj *o,const char *operation,int result) {
    fatal_error("AudioTrack %s failed: %d\nPlatform 0x%08x",operation,result,
        (unsigned)sf_audio_track_last_platform_error(o->audio));
}
static Obj *new_object_a(void *env,Obj *cls,Method *m,const Value *a) {
    if(!cls || cls->kind!=CLASS || !m || m->owner!=cls ||
       !audio_method(m,"<init>","(IIIIII)V"))
        fatal_error("Unimplemented JNI constructor:\n%s.%s%s",
            cls && cls->kind==CLASS?cls->data:"(invalid class)",m?m->name:"(no method)",m?m->sig:"");
    Obj *o=object_new(AUDIO_TRACK,0,1);int platform_error=0;
    int result=sf_audio_track_create(a[0].i,a[1].i,a[2].i,a[3].i,a[4].i,a[5].i,&o->audio,&platform_error);
    if(result<0){local_delete(env,o);fatal_error("AudioTrack constructor failed: %d\nPlatform 0x%08x",result,(unsigned)platform_error);}
    port_log("JNI AudioTrack: stream=%d rate=%d channels=%d encoding=%d buffer=%d mode=%d",
        a[0].i,a[1].i,a[2].i,a[3].i,a[4].i,a[5].i);
    return o;
}
static Obj *new_object_v(void *env,Obj *cls,Method *m,va_list ap){Value a[16]={0};collect_args(m,ap,a);return new_object_a(env,cls,m,a);}
static Obj *new_object(void *env,Obj *cls,Method *m,...){va_list ap;va_start(ap,m);Obj *o=new_object_v(env,cls,m,ap);va_end(ap);return o;}
static uintptr_t dispatch(Obj *object,Method *m,const Value *a) {
    const char *n=m->name,*owner=m->owner->data;
    if(!strcmp(owner,"android/media/AudioTrack")) {
        if(audio_method(m,"getMinBufferSize","(III)I")) {
            if(object!=m->owner)fatal_error("Invalid static AudioTrack receiver");
            return (uintptr_t)sf_audio_track_min_buffer(a[0].i,a[1].i,a[2].i);
        }
        if(!object || object->kind!=AUDIO_TRACK || !object->audio)
            fatal_error("Invalid AudioTrack receiver");
        int result;
        if(audio_method(m,"write","([BII)I")) {
            Obj *array=a[0].l;int offset=a[1].i,bytes=a[2].i;
            if(!array || array->kind!=ARRAY || array->stride!=1 || offset<0 || bytes<0 ||
               bytes>array->length || offset>array->length-bytes)
                return (uintptr_t)SF_AUDIO_BAD_VALUE;
            result=sf_audio_track_write(object->audio,array->data+offset,(size_t)bytes);
            if(result<0 || sf_audio_track_state(object->audio)!=SF_AUDIO_STATE_INITIALIZED ||
               (result>0 && result!=bytes))audio_failure(object,"write",result);
            if(result>0) {
                if(!object->audio_writes++)port_log("AudioTrack first PCM block submitted: %d bytes",result);
                if(!object->audio_nonzero_logged) {
                    for(int i=0;i<result;i++)if(array->data[offset+i]) {
                        object->audio_nonzero_logged=1;
                        port_log("AudioTrack nonzero PCM submitted; block %u",object->audio_writes);
                        break;
                    }
                }
            }
            /* The mixer unlocks before write. A concurrent pause can consume
               zero bytes, even if play() runs again before this state query. */
            if(!result && bytes)port_log("JNI AudioTrack write paused/stopped: %d bytes not consumed",bytes);
            return (uintptr_t)result;
        }
        if(audio_method(m,"play","()V"))result=sf_audio_track_play(object->audio);
        else if(audio_method(m,"pause","()V"))result=sf_audio_track_pause(object->audio);
        else if(audio_method(m,"stop","()V"))result=sf_audio_track_stop(object->audio);
        else if(audio_method(m,"release","()V"))result=sf_audio_track_release(object->audio);
        else fatal_error("Unimplemented Java callback:\n%s.%s%s",owner,n,m->sig);
        if(result<0)audio_failure(object,n,result);
        return 0;
    }
    if(exact_method(m,SF_GAME_CLASS,"da","()Landroid/app/Activity;")) {
        if(object!=m->owner)fatal_error("Invalid static Game.da receiver");
        port_log("JNI Game.da: PSV application Activity query object");
        return (uintptr_t)platform_object(&application_activity,ACTIVITY);
    }
    if(exact_method(m,SF_GAME_CLASS,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;")) {
        if(!object || object->kind!=ACTIVITY || object!=application_activity)
            fatal_error("Invalid Activity query receiver");
        const char *service=string_data(a[0].l);
        if(strcmp(service,"phone"))fatal_error("Unimplemented platform service: %s",service);
        /* Android's Context supplies a manager API object even when the
           underlying modem is absent. Queries report that absence below. */
        return (uintptr_t)platform_object(&telephony_manager,TELEPHONY_MANAGER);
    }
    if(exact_method(m,SF_TELEPHONY_CLASS,"getDeviceId","()Ljava/lang/String;")) {
        Obj *id=telephony_device_id(object);
        port_log("JNI TelephonyManager.getDeviceId: unavailable (NULL)");
        return (uintptr_t)id;
    }
    if(exact_method(m,SF_GAME_CLASS,"d","()V")) {
        if(object!=m->owner)fatal_error("Invalid static Game.d receiver");
        /* Original Tracking.init retains a NULL device ID. Its worker passes
           that NULL to String.replace in buildURL; b.run catches Exception
           and returns before constructing a URL or issuing HTTP. Preserve
           that caught-unavailable outcome, which changes no license state. */
        Obj *manager=platform_object(&telephony_manager,TELEPHONY_MANAGER);
        Obj *id=telephony_device_id(manager);local_delete(NULL,manager);
        if(id)fatal_error("Tracking device ID path is not implemented");
        port_log("JNI Game.d: launch statistics URL cannot use NULL device ID; original worker catches the error, no request sent");
        return 0;
    }
    if(exact_method(m,SF_GAME_CLASS,"dc","()[B")) {
        if(object!=m->owner)fatal_error("Invalid static Game.dc receiver");
        return (uintptr_t)apk_serial_array();
    }
    if(exact_method(m,SF_GAME_CLASS,"notifyTrophy","(I)V")) {
        if(object!=m->owner)fatal_error("Invalid static Game.notifyTrophy receiver");
        notify_trophy(a[0].i);return 0;
    }
    if(!strcmp(owner,SF_GAME_CLASS) &&
       !strcmp(n,"db") && !strcmp(m->sig,"()[B"))
        return (uintptr_t)apk_config_array();
    if(strstr(owner,"/Game")) {
        if(!strcmp(n,"GetPhoneLanguage"))return 0; /* English is present in supplied data. */
        if(!strcmp(n,"isWifiEnabled")||!strcmp(n,"isNeedGcOnFrame")||!strcmp(n,"isNeedUsedTouchZone"))return 0;
        if(!strcmp(n,"getManufacture"))return 4;
        if(!strcmp(n,"getManufacturer"))return (uintptr_t)text_new(STRING,"sony");
        if(!strcmp(n,"getPhoneModel"))return (uintptr_t)text_new(STRING,"PS Vita");
        if(!strcmp(n,"getSavedFiles")){list_saves(string_data(a[0].l));return 0;}
        if(!strcmp(n,"getSavedFilesNum"))return saved_count;
        if(!strcmp(n,"getNextSavedFile"))return (uintptr_t)text_new(STRING,saved_index<saved_count?saved_names[saved_index++]:"");
        if(!strcmp(n,"isExistedFile")) {
            char path[1024];if(port_map_path(string_data(a[0].l),path,sizeof(path)))return 0;
            SceIoStat st;int exists=sceIoGetstat(path,&st)>=0;
            if(!exists){SceUID fd=sceIoOpen(path,SCE_O_WRONLY|SCE_O_CREAT,0666);if(fd>=0)sceIoClose(fd);}return exists;
        }
        if(!strcmp(n,"RenameFile")) {
            char p[1024],q[1024];if(port_map_path(string_data(a[0].l),p,sizeof(p))||port_map_path(string_data(a[1].l),q,sizeof(q)))return 0;
            return sceIoRename(p,q)>=0;
        }
        if(!strcmp(n,"deleteMyFile")){char p[1024];if(!port_map_path(string_data(a[0].l),p,sizeof(p)))sceIoRemove(p);return 0;}
        if(!strcmp(n,"SetReorientation")){
            pthread_mutex_lock(&obj_lock);int first=!m->reorientation_logged;m->reorientation_logged=1;pthread_mutex_unlock(&obj_lock);
            if(first)port_log("Platform UI: %s",n);
            return 0;
        }
        if(!strcmp(n,"DismissSpinner")||!strcmp(n,"ShowSpinner")){port_log("Platform UI: %s",n);return 0;}
        if(!strcmp(n,"Exit"))fatal_error("Game requested Exit");
    }
    /* Audio and device services must be implemented from their actual contracts.
       Returning success here would hide missing sound and can deadlock loading. */
    fatal_error("Unimplemented Java callback:\n%s.%s%s",owner,n,m->sig);
}
static uintptr_t call_v(void *env,Obj *object,Method *m,va_list ap){Value a[16]={0};collect_args(m,ap,a);return dispatch(object,m,a);}
static uintptr_t call_a(void *env,Obj *object,Method *m,const Value *a){return dispatch(object,m,a);}
static uintptr_t call(void *env,Obj *object,Method *m,...){va_list ap;va_start(ap,m);uintptr_t r=call_v(env,object,m,ap);va_end(ap);return r;}
static int call_int_v(void *env,Obj *object,Method *m,va_list ap){return (int)call_v(env,object,m,ap);}
static int call_int_a(void *env,Obj *object,Method *m,const Value *a){return (int)dispatch(object,m,a);}
static int call_int(void *env,Obj *object,Method *m,...){va_list ap;va_start(ap,m);int r=call_int_v(env,object,m,ap);va_end(ap);return r;}
static void call_void_v(void *env,Obj *object,Method *m,va_list ap){(void)call_v(env,object,m,ap);}
static void call_void_a(void *env,Obj *object,Method *m,const Value *a){(void)dispatch(object,m,a);}
static void call_void(void *env,Obj *object,Method *m,...){va_list ap;va_start(ap,m);(void)call_v(env,object,m,ap);va_end(ap);}
static void nonvirtual_class(Obj *object,Obj *cls,Method *m) {
    if(!object || object->kind!=AUDIO_TRACK || !cls || cls->kind!=CLASS ||
       !m || m->owner!=cls || strcmp(cls->data,"android/media/AudioTrack"))
        fatal_error("Unsupported JNI nonvirtual receiver/class");
}
static int nonvirtual_int_v(void *env,Obj *object,Obj *cls,Method *m,va_list ap){nonvirtual_class(object,cls,m);return (int)call_v(env,object,m,ap);}
static int nonvirtual_int_a(void *env,Obj *object,Obj *cls,Method *m,const Value *a){nonvirtual_class(object,cls,m);return (int)dispatch(object,m,a);}
static int nonvirtual_int(void *env,Obj *object,Obj *cls,Method *m,...){va_list ap;va_start(ap,m);int r=nonvirtual_int_v(env,object,cls,m,ap);va_end(ap);return r;}
static void nonvirtual_void_v(void *env,Obj *object,Obj *cls,Method *m,va_list ap){nonvirtual_class(object,cls,m);(void)call_v(env,object,m,ap);}
static void nonvirtual_void_a(void *env,Obj *object,Obj *cls,Method *m,const Value *a){nonvirtual_class(object,cls,m);(void)dispatch(object,m,a);}
static void nonvirtual_void(void *env,Obj *object,Obj *cls,Method *m,...){va_list ap;va_start(ap,m);nonvirtual_void_v(env,object,cls,m,ap);va_end(ap);}
static Obj *new_string(void *env,const char *s){return text_new(STRING,s?s:"");}
static const char *get_string(void *env,Obj *o,unsigned char *copy){
    /* This APK's normal Dalvik JNI returned NULL quietly for a NULL jstring.
       getIMEI explicitly handles it; CheckJNI/general JNI rules differ. */
    if(!o)return NULL;
    /* Native wrappers can delete their local jstring while still holding the
       acquired UTF characters. Give each acquisition its own lifetime. */
    char *s=strdup(string_data(o));
    if(!s)fatal_error("JNI UTF character allocation failed");
    if(copy)*copy=1;
    return s;
}
static void release_string(void *env,Obj *o,const char *s){free((void*)s);}
static int string_length(void *env,Obj *o){return strlen(string_data(o));}
static int array_length(void *env,Obj *o){if(!o||o->kind!=ARRAY)fatal_error("Invalid JNI array");return o->length;}
static Obj *byte_array(void *env,int n){return object_new(ARRAY,n,1);}
static Obj *short_array(void *env,int n){return object_new(ARRAY,n,2);}
static Obj *int_array(void *env,int n){return object_new(ARRAY,n,4);}
static void *array_elements(void *env,Obj *o,unsigned char *copy){array_length(env,o);if(copy)*copy=0;return o->data;}
static void array_release(void *env,Obj *o,void *data,int mode){}
static void array_range(Obj *o,int start,int length){array_length(NULL,o);if(start<0||length<0||start>o->length-length)fatal_error("JNI array bounds");}
static void get_region(void *env,Obj *o,int start,int length,void *dst){array_range(o,start,length);memcpy(dst,o->data+start*o->stride,length*o->stride);}
static void set_region(void *env,Obj *o,int start,int length,const void *src){array_range(o,start,length);memcpy(o->data+start*o->stride,src,length*o->stride);}
static void *array_critical(void *env,Obj *o,unsigned char *copy) {
    array_length(env,o);
    pthread_mutex_lock(&obj_lock);o->refs++;o->critical_pins++;pthread_mutex_unlock(&obj_lock);
    if(copy)*copy=0;
    return o->data;
}
static void release_critical(void *env,Obj *o,void *data,int mode) {
    array_length(env,o);
    if(data!=o->data || (mode!=0 && mode!=1 && mode!=2))fatal_error("Invalid JNI critical array release");
    pthread_mutex_lock(&obj_lock);
    if(!o->critical_pins){pthread_mutex_unlock(&obj_lock);fatal_error("JNI critical array was not acquired");}
    o->critical_pins--;pthread_mutex_unlock(&obj_lock);
    /* Direct storage has no copy to discard. JNI_COMMIT/JNI_ABORT are ignored
       for an uncopied array; every matching release removes one pin. */
    object_unref(o);
}
static int get_vm(void *env,void **out){*out=&vm_ptr;return 0;}
static int vm_env(void *vm,void **out,int version) {
    if(!out)return -1; /* JNI_ERR: no output pointer to receive an environment. */
    *out=NULL;
    /* _InitAT requests JNI 1.2. The 1.6 table also supports its older ABI. */
    if(version!=0x10001 && version!=0x10002 && version!=0x10004 && version!=0x10006)return -3;
    *out=&env_ptr;return 0;
}
static int vm_attach(void *vm,void **out,void *args){*out=&env_ptr;return 0;}
static int vm_detach(void *vm){ThreadRefs *refs=thread_refs();if(pthread_setspecific(refs_key,NULL))fatal_error("JNI detach failed");refs_destroy(refs);return 0;}
static int version(void *env){return 0x10006;}
static void *no_exception(void *env){return NULL;}
static void clear_exception(void *env){}
static int exception_check(void *env){return 0;}
static int same_object(void *env,Obj *a,Obj *b){return a==b;}
static void jni_fatal(void *env,const char *message){fatal_error("JNI FatalError: %s",message);}

#include "jni_traps.inc"
void jni_bridge_init(void) {
    init_jni_traps(env_table);
    for(unsigned i=0;i<8;i++)vm_table[i]=(uintptr_t)jni_unknown_vm;
    #define SLOT(i,f) env_table[i]=(uintptr_t)(f)
    SLOT(4,version);SLOT(6,find_class);SLOT(15,no_exception);SLOT(17,clear_exception);SLOT(18,jni_fatal);
    SLOT(19,push_frame);SLOT(20,pop_frame);
    SLOT(21,ref_new);SLOT(22,ref_delete);SLOT(23,local_delete);SLOT(24,same_object);SLOT(25,local_new);
    SLOT(28,new_object);SLOT(29,new_object_v);SLOT(30,new_object_a);
    SLOT(31,object_class);SLOT(33,method_id);SLOT(113,method_id);
    int bases[]={34,37,49,114,117,129};
    for(unsigned i=0;i<sizeof(bases)/sizeof(bases[0]);i++){SLOT(bases[i],call);SLOT(bases[i]+1,call_v);SLOT(bases[i]+2,call_a);}
    SLOT(49,call_int);SLOT(50,call_int_v);SLOT(51,call_int_a);
    SLOT(129,call_int);SLOT(130,call_int_v);SLOT(131,call_int_a);
    SLOT(61,call_void);SLOT(62,call_void_v);SLOT(63,call_void_a);SLOT(141,call_void);SLOT(142,call_void_v);SLOT(143,call_void_a);
    SLOT(79,nonvirtual_int);SLOT(80,nonvirtual_int_v);SLOT(81,nonvirtual_int_a);
    SLOT(91,nonvirtual_void);SLOT(92,nonvirtual_void_v);SLOT(93,nonvirtual_void_a);
    SLOT(144,static_field_id);SLOT(145,get_static_object_field);
    SLOT(167,new_string);SLOT(168,string_length);SLOT(169,get_string);SLOT(170,release_string);
    SLOT(171,array_length);SLOT(176,byte_array);SLOT(178,short_array);SLOT(179,int_array);
    for(int i=183;i<=190;i++)SLOT(i,array_elements);
    for(int i=191;i<=198;i++)SLOT(i,array_release);
    for(int i=199;i<=206;i++)SLOT(i,get_region);
    for(int i=207;i<=214;i++)SLOT(i,set_region);
    SLOT(219,get_vm);SLOT(222,array_critical);SLOT(223,release_critical);SLOT(228,exception_check);
    vm_table[4]=(uintptr_t)vm_attach;vm_table[5]=(uintptr_t)vm_detach;vm_table[6]=(uintptr_t)vm_env;vm_table[7]=(uintptr_t)vm_attach;
    #undef SLOT
}
void *jni_environment(void){return &env_ptr;}
void *jni_vm(void){return &vm_ptr;}
void *jni_class(const char *name){return find_class(&env_ptr,name);}
