#include "port.h"
#include "fd_bridge.h"
#include <vitasdk.h>
#include <vitaGL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef GL_SHININESS
#define GL_SHININESS 0x1601
#endif
#ifndef GL_SPOT_DIRECTION
#define GL_SPOT_DIRECTION 0x1204
#endif

/* Android 32-bit Bionic FILE has an 84-byte public prefix. Keep its inline
   getc counters at zero so __srget handles reads; never pass it to newlib. */
typedef struct { unsigned char bionic[84]; FILE *host; } BFile;
unsigned char sf_stdio[3*84];
static FILE *host_file(void *p) {
    if (p==sf_stdio) return stdin;
    if (p==sf_stdio+84) return stdout;
    if (p==sf_stdio+168) return stderr;
    if (!p) fatal_error("NULL Android FILE pointer");
    return ((BFile *)p)->host;
}
static int track_file(FILE *f, const char *mode) {
    int flags = strchr(mode, '+') ? 2 : (*mode == 'r' ? 0 : 1);
    if (*mode == 'a') flags |= SF_O_APPEND;
    struct stat st;
    int fd = fileno(f);
    return port_fd_track(fd, flags, fstat(fd, &st) == 0 && S_ISREG(st.st_mode));
}
static void *wrap_file(FILE *f, const char *mode) {
    if (!f) return NULL;
    BFile *b=calloc(1,sizeof(*b));
    if (!b) { fclose(f); errno=ENOMEM; return NULL; }
    if (track_file(f, mode) < 0) { int saved=errno; fclose(f); free(b); errno=saved; return NULL; }
    b->host=f; return b;
}
void *sf_fopen(const char *name,const char *mode) {
    char path[1024]; if (port_map_path(name,path,sizeof(path))) return NULL;
    FILE *f=fopen(path,mode);
    /* The supplied data has the complete 1024-width sprite variant, while
       this APK requests sprites_high_res at Vita's 960-width display. Keep
       driver/input dimensions intact and use that variant only if the exact
       requested archive is absent. Its UI geometry still needs validation. */
    const char *base=strrchr(path,'/');
    if(!f && errno==ENOENT && mode[0]=='r' && !strchr(mode,'+') && base &&
       !strncmp(path,GAME_DIR "/",strlen(GAME_DIR)+1) &&
       !strcmp(base+1,"sprites_high_res.gla")) {
        f=fopen(GAME_DIR "/sprites_1024.gla",mode);
        if(f)port_log("Sprite variant fallback: sprites_high_res.gla -> sprites_1024.gla; layout under test");
    }
    port_log("fopen %s [%s]: %s",path,mode,f?"OK":strerror(errno));
    return wrap_file(f,mode);
}
int sf_fclose(void *p) { FILE *f=host_file(p);port_fd_forget(fileno(f));int r=fclose(f); if(p!=sf_stdio && p!=sf_stdio+84 && p!=sf_stdio+168)free(p);return r; }
size_t sf_fread(void *b,size_t s,size_t n,void *p) {return fread(b,s,n,host_file(p));}
size_t sf_fwrite(const void *b,size_t s,size_t n,void *p) {return fwrite(b,s,n,host_file(p));}
int sf_fseek(void *p,long o,int w) {return fseek(host_file(p),o,w);}
long sf_ftell(void *p) {return ftell(host_file(p));}
int sf_fflush(void *p) {return fflush(p?host_file(p):NULL);}
int sf_fgetpos(void *p,int *o) {long pos=ftell(host_file(p));if(pos<0)return -1;*o=pos;return 0;}
int sf_fsetpos(void *p,const int *o) {return fseek(host_file(p),*o,SEEK_SET);}
int sf_getc(void *p) {return fgetc(host_file(p));}
int sf_putc(int c,void *p) {return fputc(c,host_file(p));}
int sf_ungetc(int c,void *p) {return ungetc(c,host_file(p));}
char *sf_fgets(char *s,int n,void *p) {return fgets(s,n,host_file(p));}
int sf_fputs(const char *s,void *p) {return fputs(s,host_file(p));}
int sf_setvbuf(void *p,char *b,int mode,size_t size) {return setvbuf(host_file(p),b,mode,size);}
void *sf_tmpfile(void) {return sf_fopen(SAVE_DIR "/temporary.bin","w+b");}
void *sf_freopen(const char *n,const char *m,void *p) {
    char path[1024];if(port_map_path(n,path,sizeof(path)))return NULL;
    FILE *old=host_file(p);port_fd_forget(fileno(old));
    FILE *f=freopen(path,m,old);if(!f)return NULL;
    if(p!=sf_stdio && p!=sf_stdio+84 && p!=sf_stdio+168)((BFile*)p)->host=f;
    if(track_file(f,m)<0)fatal_error("Cannot track reopened file descriptor");
    return p;
}
int sf_fprintf(void *p,const char *fmt,...) {va_list ap;va_start(ap,fmt);int r=vfprintf(host_file(p),fmt,ap);va_end(ap);return r;}
int sf_fscanf(void *p,const char *fmt,...) {va_list ap;va_start(ap,fmt);int r=vfscanf(host_file(p),fmt,ap);va_end(ap);return r;}
int sf_printf(const char *fmt,...) {char b[1024];va_list ap;va_start(ap,fmt);int r=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);port_log("game: %s",b);return r;}
int sf_puts(const char *s) {port_log("game: %s",s);return 0;}
int sf_android_log_print(int priority,const char *tag,const char *fmt,...) {char b[1024];va_list ap;va_start(ap,fmt);int r=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);port_log("android[%d/%s]: %s",priority,tag?tag:"",b);return r;}
int sf_android_log_write(int priority,const char *tag,const char *s) {return sf_android_log_print(priority,tag,"%s",s);}
void sf_abort(void) {fatal_error("Game called abort()");}
void sf_stack_fail(void) {fatal_error("Game stack check failed");}
void sf_exit(int code) {fatal_error("Game requested exit(%d)",code);}
uintptr_t sf_stack_guard=0x71c42a83;
void *sf_dso_handle=&sf_dso_handle;

int sf_remove(const char *n){char p[1024];return port_map_path(n,p,sizeof(p))?-1:remove(p);}
int sf_rename(const char *a,const char *b){char p[1024],q[1024];return port_map_path(a,p,sizeof(p))||port_map_path(b,q,sizeof(q))?-1:rename(p,q);}
int sf_chdir(const char *n){char p[1024];return port_map_path(n,p,sizeof(p))?-1:chdir(p);}
int sf_socket(int a,int b,int c){port_log("Network unavailable in test build (socket)");errno=ENETUNREACH;return -1;}
int sf_gethostname(char *name,size_t length) {
    if(!name || !length){errno=EINVAL;return -1;}
    /* Both native GetHostName helpers ignore errors and then copy the buffer.
       The SDK queries the DHCP hostname; keep failure safe for those callers. */
    name[0]=0;
    int result=gethostname(name,length),saved_errno=errno;
    if(result<0)name[0]=0;
    port_log("gethostname: %s",result<0?"unavailable":"OK");
    errno=saved_errno;
    return result;
}
/* AOSP android-2.3.7_r1 libc/include/sys/stat.h: ARM EABI stat64 layout. */
typedef struct {
    uint64_t dev; uint8_t pad0[4]; uint32_t ino32,mode,nlink,uid,gid;
    uint64_t rdev; uint8_t pad3[4]; int64_t size;
    uint32_t blksize; uint64_t blocks;
    uint32_t atime,atime_ns,mtime,mtime_ns,ctime,ctime_ns; uint64_t ino;
} AndroidStat;
_Static_assert(sizeof(AndroidStat)==104,"Android stat ABI size");
_Static_assert(offsetof(AndroidStat,size)==48,"Android stat size offset");
int sf_fstat(int fd,AndroidStat *out) {
    struct stat in;if(fstat(fd,&in)<0)return -1;
    memset(out,0,sizeof(*out));out->dev=in.st_dev;out->ino32=out->ino=in.st_ino;
    out->mode=in.st_mode;out->nlink=in.st_nlink;out->uid=in.st_uid;out->gid=in.st_gid;
    out->rdev=in.st_rdev;out->size=in.st_size;out->blksize=in.st_blksize;out->blocks=in.st_blocks;
    out->atime=in.st_atime;out->mtime=in.st_mtime;out->ctime=in.st_ctime;return 0;
}
long sf_sysconf(int name){if(name==39)return 4096;if(name==97||name==96)return 3;fatal_error("Unimplemented Android sysconf(%d)",name);}
char *sf_getenv(const char *name){if(!strcmp(name,"HOME"))return SAVE_DIR;if(!strcmp(name,"EXTERNAL_STORAGE"))return "ux0:data";return NULL;}

/* pthread_mutex_t is a 32-bit inline state in Bionic and a pointer in Vita
   pthreads. Translate static initializers under a lock before publishing. */
static pthread_mutex_t mutex_init_lock=PTHREAD_MUTEX_INITIALIZER;
static int ensure_mutex(uint32_t *p) {
    int r=0;pthread_mutex_lock(&mutex_init_lock);
    if (*p<=0xffff) {
        pthread_mutexattr_t a;pthread_mutexattr_init(&a);
        pthread_mutexattr_settype(&a,(*p&0x4000)?PTHREAD_MUTEX_RECURSIVE:((*p&0x8000)?PTHREAD_MUTEX_ERRORCHECK:PTHREAD_MUTEX_NORMAL));
        pthread_mutex_t m;r=pthread_mutex_init(&m,&a);pthread_mutexattr_destroy(&a);
        if(!r)*p=(uint32_t)m;
    }
    pthread_mutex_unlock(&mutex_init_lock);return r;
}
int sf_mutex_init(uint32_t *p,const int *attr){*p=attr?((*attr&3)==1?0x4000:((*attr&3)==2?0x8000:0)):0;return ensure_mutex(p);}
int sf_mutex_lock(uint32_t *p){int r=ensure_mutex(p);return r?r:pthread_mutex_lock((pthread_mutex_t*)p);}
int sf_mutex_trylock(uint32_t *p){int r=ensure_mutex(p);return r?r:pthread_mutex_trylock((pthread_mutex_t*)p);}
int sf_mutex_unlock(uint32_t *p){int r=ensure_mutex(p);return r?r:pthread_mutex_unlock((pthread_mutex_t*)p);}
int sf_mutex_destroy(uint32_t *p){if(*p<=0xffff)return 0;int r=pthread_mutex_destroy((pthread_mutex_t*)p);if(!r)*p=0;return r;}
int sf_pthread_create(pthread_t *id,const void *attr,void *(*entry)(void*),void *arg){if(attr)fatal_error("Android pthread attributes need translation");return pthread_create(id,NULL,entry,arg);}

static unsigned char ctype_data[257];
static short lower_data[257],upper_data[257];
const unsigned char *sf_ctype=ctype_data;
const short *sf_lower=lower_data;
const short *sf_upper=upper_data;
void bridges_init(void) {
    port_fd_init();
    lower_data[0]=upper_data[0]=-1;
    for(int i=0;i<256;i++) {
        ctype_data[i+1]=(isupper(i)?1:0)|(islower(i)?2:0)|(isdigit(i)?4:0)|(isspace(i)?8:0)|(ispunct(i)?16:0)|(iscntrl(i)?32:0)|(isxdigit(i)?64:0)|(i==' '?128:0);
        lower_data[i+1]=tolower(i);upper_data[i+1]=toupper(i);
    }
}
void sf_glMaterialf(GLenum face,GLenum pname,GLfloat value){if(pname!=GL_SHININESS)fatal_error("glMaterialf pname=0x%x",pname);glMaterialfv(face,pname,&value);}
void sf_glLightf(GLenum light,GLenum pname,GLfloat value){if(pname==GL_AMBIENT||pname==GL_DIFFUSE||pname==GL_SPECULAR||pname==GL_POSITION||pname==GL_SPOT_DIRECTION)fatal_error("glLightf vector parameter");glLightfv(light,pname,&value);}
void sf_glSampleCoverage(GLfloat value,GLboolean invert){if(value!=1.0f||invert)fatal_error("Multisample coverage unsupported");}

/* Keep the compiler's real status. Failed shaders otherwise get requested
   repeatedly by the native effect cache, hiding the first useful error. */
static void log_shader_failure(GLuint shader) {
    char message[768]={0};GLsizei length=0;GLint type=0;
    glGetShaderiv(shader,GL_SHADER_TYPE,&type);
    glGetShaderInfoLog(shader,sizeof(message),&length,message);
    port_log("Shader %u type=0x%x: %s",shader,type,message[0]?message:"compiler returned no diagnostic");
}
void sf_glCompileShader(GLuint shader) {
    glCompileShader(shader);
    GLint compiled=GL_FALSE;glGetShaderiv(shader,GL_COMPILE_STATUS,&compiled);
    if(!compiled) {
        GLenum error=glGetError();
        log_shader_failure(shader);
        fatal_error("Shader compilation failed: %u, GL error=0x%x, caller=0x%08x",
                    shader,error,(unsigned)(uintptr_t)__builtin_return_address(0));
    }
}
void sf_glLinkProgram(GLuint program) {
    glLinkProgram(program);
    GLint linked=GL_FALSE;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked) {
        GLenum error=glGetError();GLuint shaders[2]={0};GLsizei count=0;
        glGetAttachedShaders(program,2,&count,shaders);
        for(GLsizei i=0;i<count;i++)log_shader_failure(shaders[i]);
        fatal_error("Shader program link failed: %u, GL error=0x%x, caller=0x%08x",
                    program,error,(unsigned)(uintptr_t)__builtin_return_address(0));
    }
    static unsigned linked_count;
    linked_count++;
    if(linked_count<=3 || linked_count%25==0)
        port_log("Shader programs linked: %u",linked_count);
}
