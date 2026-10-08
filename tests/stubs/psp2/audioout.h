#pragma once

enum {
    SCE_AUDIO_OUT_PORT_TYPE_BGM = 1,
    SCE_AUDIO_OUT_MODE_STEREO = 1,
    SCE_AUDIO_MIN_LEN = 64,
    SCE_AUDIO_OUT_MAX_VOL = 32768,
    SCE_AUDIO_VOLUME_FLAG_L_CH = 1,
    SCE_AUDIO_VOLUME_FLAG_R_CH = 2
};
int sceAudioOutOpenPort(int type, int len, int freq, int mode);
int sceAudioOutReleasePort(int port);
int sceAudioOutOutput(int port, const void *pcm);
int sceAudioOutSetConfig(int port, int len, int freq, int mode);
int sceAudioOutSetVolume(int port, int flags, const int *volumes);
