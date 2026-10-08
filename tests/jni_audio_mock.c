/* Host-only backend double: validates JNI inputs and reference ownership.
 * Real sceAudioOut output is covered by the separate audio backend tests. */
#include "jni_audio_mock.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct SfAudioTrack { int state, play_state, released, error; uint32_t frames; };
JniAudioMock jni_audio_mock;

int sf_audio_track_min_buffer(int rate,int mask,int encoding) {
    jni_audio_mock.query[0]=rate;jni_audio_mock.query[1]=mask;jni_audio_mock.query[2]=encoding;
    return rate==44100 && mask==12 && encoding==2 ? SF_AUDIO_BUFFER_BYTES : SF_AUDIO_BAD_VALUE;
}
int sf_audio_track_create(int stream,int rate,int mask,int encoding,int bytes,int mode,
                         SfAudioTrack **out,int *platform_error) {
    int args[]={stream,rate,mask,encoding,bytes,mode};memcpy(jni_audio_mock.ctor,args,sizeof(args));
    *out=NULL;*platform_error=0;
    if(stream!=3 || rate!=44100 || mask!=12 || encoding!=2 || bytes!=4096 || mode!=1)return SF_AUDIO_BAD_VALUE;
    if(jni_audio_mock.fail_create){*platform_error=jni_audio_mock.platform_error;return SF_AUDIO_ERROR;}
    SfAudioTrack *t=calloc(1,sizeof(*t));assert(t);
    t->state=1;t->play_state=1;*out=t;jni_audio_mock.created++;return 0;
}
int sf_audio_track_release(SfAudioTrack *t) {
    jni_audio_mock.release_calls++;
    if(t->released)return 0;
    if(jni_audio_mock.fail_release){t->error=jni_audio_mock.platform_error;t->state=0;return SF_AUDIO_ERROR;}
    t->released=1;t->state=0;t->play_state=1;jni_audio_mock.closed++;return 0;
}
int sf_audio_track_destroy(SfAudioTrack **t) {
    if(!*t)return 0;
    int r=sf_audio_track_release(*t);if(r<0)return r;
    free(*t);*t=NULL;jni_audio_mock.destroyed++;return 0;
}
int sf_audio_track_play(SfAudioTrack *t){if(t->state!=1)return -3;t->play_state=3;jni_audio_mock.plays++;return 0;}
int sf_audio_track_pause(SfAudioTrack *t){if(t->state!=1)return -3;t->play_state=2;jni_audio_mock.pauses++;return 0;}
int sf_audio_track_stop(SfAudioTrack *t){if(t->state!=1)return -3;t->play_state=1;t->frames=0;jni_audio_mock.stops++;return 0;}
int sf_audio_track_flush(SfAudioTrack *t){return t->state==1 && t->play_state!=3 ? 0 : -3;}
int sf_audio_track_set_stereo_volume(SfAudioTrack *t,float left,float right){return t->state==1 && left>=0 && left<=1 && right>=0 && right<=1 ? 0 : -2;}
int sf_audio_track_write(SfAudioTrack *t,const void *pcm,size_t bytes) {
    jni_audio_mock.writes++;
    if(jni_audio_mock.fail_write){t->error=jni_audio_mock.platform_error;t->state=0;return -1;}
    if(t->state!=1)return -3;
    if(t->play_state!=3 || !bytes)return 0;
    if(jni_audio_mock.paused_resume)return 0; /* pause occurred during write; resume already completed */
    if(bytes%4 || bytes<256)return -2;
    if(jni_audio_mock.short_write)return (int)bytes-256;
    assert(bytes<=sizeof(jni_audio_mock.written));
    memcpy(jni_audio_mock.written,pcm,bytes);jni_audio_mock.written_bytes=bytes;
    t->frames+=(uint32_t)(bytes/4);return (int)bytes;
}
int sf_audio_track_state(SfAudioTrack *t){return t?t->state:0;}
int sf_audio_track_play_state(SfAudioTrack *t){return t?t->play_state:1;}
int sf_audio_track_playback_head(SfAudioTrack *t,uint32_t *frames){*frames=t->frames;return t->state==1?0:-3;}
int sf_audio_track_last_platform_error(SfAudioTrack *t){return t?t->error:0;}
