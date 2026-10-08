#pragma once
#include <limits.h>
#include "../src/audio_track.h"

typedef struct {
    int created, destroyed, closed, release_calls, writes;
    int plays, pauses, stops, ctor[6], query[3];
    int fail_create, fail_release, fail_write, short_write, paused_resume;
    int platform_error;
    size_t written_bytes;
    unsigned char written[8192];
} JniAudioMock;
extern JniAudioMock jni_audio_mock;
