#include "audio_track.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/audioout.h>

enum { FRAME_BYTES = 4, OUTPUT_FRAMES = SF_AUDIO_BUFFER_BYTES / FRAME_BYTES,
       OUTPUT_BUFFERS = 2, BUFFER_ALIGNMENT = 64 };

struct SfAudioTrack {
    pthread_mutex_t mutex;
    int port;
    int state;
    int play_state;
    int output_frames;
    int last_platform_error;
    uint32_t submitted_frames;
    unsigned next_buffer;
    unsigned char buffer_storage[SF_AUDIO_BUFFER_BYTES * OUTPUT_BUFFERS + BUFFER_ALIGNMENT - 1];
};

static int valid_format(int rate, int mask, int encoding) {
    return rate == 44100 && mask == 12 && encoding == 2;
}

int sf_audio_track_min_buffer(int rate, int mask, int encoding) {
    return valid_format(rate, mask, encoding) ? SF_AUDIO_BUFFER_BYTES : SF_AUDIO_BAD_VALUE;
}

static int platform_failure(SfAudioTrack *track, int error) {
    track->last_platform_error = error;
    track->state = SF_AUDIO_STATE_UNINITIALIZED;
    track->play_state = SF_AUDIO_PLAYSTATE_STOPPED;
    return SF_AUDIO_ERROR;
}

int sf_audio_track_create(int stream, int rate, int mask, int encoding,
                          int buffer_bytes, int mode,
                          SfAudioTrack **out, int *platform_error) {
    if (platform_error) *platform_error = 0;
    if (!out) return SF_AUDIO_BAD_VALUE;
    *out = NULL;
    if (stream != 3 || !valid_format(rate, mask, encoding) || mode != 1 ||
        buffer_bytes != SF_AUDIO_BUFFER_BYTES) return SF_AUDIO_BAD_VALUE;
    SfAudioTrack *track = calloc(1, sizeof(*track));
    if (!track) return SF_AUDIO_ERROR;
    if (pthread_mutex_init(&track->mutex, NULL)) {
        free(track);
        return SF_AUDIO_ERROR;
    }
    track->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, OUTPUT_FRAMES,
                                      rate, SCE_AUDIO_OUT_MODE_STEREO);
    if (track->port < 0) {
        if (platform_error) *platform_error = track->port;
        pthread_mutex_destroy(&track->mutex);
        free(track);
        return SF_AUDIO_ERROR;
    }
    track->output_frames = OUTPUT_FRAMES;
    track->state = SF_AUDIO_STATE_INITIALIZED;
    track->play_state = SF_AUDIO_PLAYSTATE_STOPPED;
    *out = track;
    return 0;
}

int sf_audio_track_release(SfAudioTrack *track) {
    if (!track) return 0;
    pthread_mutex_lock(&track->mutex);
    if (track->port < 0) {
        pthread_mutex_unlock(&track->mutex);
        return 0;
    }
    int drain = sceAudioOutOutput(track->port, NULL);
    int close = sceAudioOutReleasePort(track->port);
    if (close >= 0) track->port = -1;
    track->state = SF_AUDIO_STATE_UNINITIALIZED;
    track->play_state = SF_AUDIO_PLAYSTATE_STOPPED;
    int result = 0;
    if (drain < 0) result = platform_failure(track, drain);
    if (close < 0) result = platform_failure(track, close);
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_destroy(SfAudioTrack **track) {
    if (!track) return SF_AUDIO_BAD_VALUE;
    if (!*track) return 0;
    int result = sf_audio_track_release(*track);
    if (result < 0) return result;
    pthread_mutex_destroy(&(*track)->mutex);
    free(*track);
    *track = NULL;
    return 0;
}

int sf_audio_track_play(SfAudioTrack *track) {
    if (!track) return SF_AUDIO_INVALID_OPERATION;
    pthread_mutex_lock(&track->mutex);
    int result = SF_AUDIO_INVALID_OPERATION;
    if (track->state == SF_AUDIO_STATE_INITIALIZED) {
        track->play_state = SF_AUDIO_PLAYSTATE_PLAYING;
        result = 0;
    }
    pthread_mutex_unlock(&track->mutex);
    return result;
}

static int control(SfAudioTrack *track, int state, int reset_head, int flush_only) {
    if (!track) return SF_AUDIO_INVALID_OPERATION;
    pthread_mutex_lock(&track->mutex);
    int result = SF_AUDIO_INVALID_OPERATION;
    if (track->state == SF_AUDIO_STATE_INITIALIZED) {
        if (flush_only && track->play_state == SF_AUDIO_PLAYSTATE_PLAYING) {
            result = SF_AUDIO_INVALID_OPERATION;
        } else {
            result = sceAudioOutOutput(track->port, NULL);
            if (result < 0) result = platform_failure(track, result);
            else {
                if (!flush_only) track->play_state = state;
                if (reset_head) track->submitted_frames = 0;
                result = 0;
            }
        }
    }
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_pause(SfAudioTrack *track) {
    return control(track, SF_AUDIO_PLAYSTATE_PAUSED, 0, 0);
}

int sf_audio_track_stop(SfAudioTrack *track) {
    return control(track, SF_AUDIO_PLAYSTATE_STOPPED, 1, 0);
}

int sf_audio_track_flush(SfAudioTrack *track) {
    return control(track, SF_AUDIO_PLAYSTATE_STOPPED, 1, 1);
}

int sf_audio_track_set_stereo_volume(SfAudioTrack *track, float left, float right) {
    if (!track) return SF_AUDIO_INVALID_OPERATION;
    if (!isfinite(left) || !isfinite(right) || left < 0 || left > 1 ||
        right < 0 || right > 1) return SF_AUDIO_BAD_VALUE;
    pthread_mutex_lock(&track->mutex);
    int result = SF_AUDIO_INVALID_OPERATION;
    if (track->state == SF_AUDIO_STATE_INITIALIZED) {
        int volumes[2] = {
            (int)(left * SCE_AUDIO_OUT_MAX_VOL + 0.5f),
            (int)(right * SCE_AUDIO_OUT_MAX_VOL + 0.5f)
        };
        result = sceAudioOutSetVolume(track->port,
                    SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, volumes);
        if (result < 0) result = platform_failure(track, result);
        else result = 0;
    }
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_write(SfAudioTrack *track, const void *pcm, size_t bytes) {
    if (!track) return SF_AUDIO_INVALID_OPERATION;
    if (bytes > INT_MAX || bytes % FRAME_BYTES || (!pcm && bytes)) return SF_AUDIO_BAD_VALUE;
    pthread_mutex_lock(&track->mutex);
    int result = SF_AUDIO_INVALID_OPERATION;
    if (track->state != SF_AUDIO_STATE_INITIALIZED) goto done;
    if (track->play_state != SF_AUDIO_PLAYSTATE_PLAYING || !bytes) {
        result = 0;
        goto done;
    }
    size_t frames = bytes / FRAME_BYTES;
    frames -= frames % SCE_AUDIO_MIN_LEN;
    if (!frames) {
        result = SF_AUDIO_BAD_VALUE;
        goto done;
    }
    size_t completed = 0;
    while (frames) {
        int count = frames > OUTPUT_FRAMES ? OUTPUT_FRAMES : (int)frames;
        int error;
        if (track->output_frames != count) {
            /* A format change needs an idle port. Normal 1024-frame mixer
             * writes keep a fixed configuration and never drain between blocks. */
            error = sceAudioOutOutput(track->port, NULL);
            if (error < 0) {
                result = platform_failure(track, error);
                break;
            }
            error = sceAudioOutSetConfig(track->port, count, 44100, SCE_AUDIO_OUT_MODE_STEREO);
            if (error < 0) {
                result = platform_failure(track, error);
                break;
            }
            track->output_frames = count;
        }
        uintptr_t aligned = ((uintptr_t)track->buffer_storage + BUFFER_ALIGNMENT - 1)
                            & ~(uintptr_t)(BUFFER_ALIGNMENT - 1);
        unsigned char *buffer = (unsigned char *)aligned
                                + track->next_buffer * SF_AUDIO_BUFFER_BYTES;
        memcpy(buffer, (const unsigned char *)pcm + completed, count * FRAME_BYTES);
        error = sceAudioOutOutput(track->port, buffer);
        if (error < 0) {
            result = platform_failure(track, error);
            break;
        }
        /* Like SDL's Vita backend, alternate two aligned buffers. Output
         * supplies hardware backpressure; the other buffer stays untouched
         * while it is in flight. Draining after every write starves the queue.
         * https://github.com/libsdl-org/SDL/blob/SDL2/src/audio/vita/SDL_vitaaudio.c */
        track->next_buffer = (track->next_buffer + 1) % OUTPUT_BUFFERS;
        track->submitted_frames += (uint32_t)count;
        completed += (size_t)count * FRAME_BYTES;
        frames -= (size_t)count;
        result = (int)completed;
    }
    if (completed) result = (int)completed;
done:
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_state(SfAudioTrack *track) {
    if (!track) return SF_AUDIO_STATE_UNINITIALIZED;
    pthread_mutex_lock(&track->mutex);
    int result = track->state;
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_play_state(SfAudioTrack *track) {
    if (!track) return SF_AUDIO_PLAYSTATE_STOPPED;
    pthread_mutex_lock(&track->mutex);
    int result = track->play_state;
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_playback_head(SfAudioTrack *track, uint32_t *frames) {
    if (!track || !frames) return SF_AUDIO_BAD_VALUE;
    pthread_mutex_lock(&track->mutex);
    *frames = 0;
    int result = SF_AUDIO_INVALID_OPERATION;
    if (track->state == SF_AUDIO_STATE_INITIALIZED) {
        int remaining = sceAudioOutGetRestSample(track->port);
        if (remaining < 0) result = platform_failure(track, remaining);
        else {
            /* Accepted samples may still be playing. Preserve Android's
             * unsigned 32-bit playback-head wrap without draining the device. */
            *frames = track->submitted_frames - (uint32_t)remaining;
            result = 0;
        }
    }
    pthread_mutex_unlock(&track->mutex);
    return result;
}

int sf_audio_track_last_platform_error(SfAudioTrack *track) {
    if (!track) return 0;
    pthread_mutex_lock(&track->mutex);
    int result = track->last_platform_error;
    pthread_mutex_unlock(&track->mutex);
    return result;
}
