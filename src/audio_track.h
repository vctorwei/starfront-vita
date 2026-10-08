#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct SfAudioTrack SfAudioTrack;

enum {
    SF_AUDIO_ERROR = -1,
    SF_AUDIO_BAD_VALUE = -2,
    SF_AUDIO_INVALID_OPERATION = -3,
    SF_AUDIO_STATE_UNINITIALIZED = 0,
    SF_AUDIO_STATE_INITIALIZED = 1,
    SF_AUDIO_PLAYSTATE_STOPPED = 1,
    SF_AUDIO_PLAYSTATE_PAUSED = 2,
    SF_AUDIO_PLAYSTATE_PLAYING = 3,
    SF_AUDIO_BUFFER_BYTES = 4096
};

/* This adapter supports the observed STREAM_MUSIC/44100/stereo/PCM16/stream
 * constructor. Its minimum and supported constructor buffer are 4096 bytes. */
int sf_audio_track_min_buffer(int sample_rate, int channel_mask, int encoding);
int sf_audio_track_create(int stream_type, int sample_rate, int channel_mask,
                          int encoding, int buffer_bytes, int mode,
                          SfAudioTrack **out, int *platform_error);

/* release closes the port but retains the handle for JNI reference lifetime.
 * destroy frees and clears the handle only after the port is closed. */
int sf_audio_track_release(SfAudioTrack *track);
int sf_audio_track_destroy(SfAudioTrack **track);
int sf_audio_track_play(SfAudioTrack *track);
int sf_audio_track_pause(SfAudioTrack *track);
int sf_audio_track_stop(SfAudioTrack *track);
int sf_audio_track_flush(SfAudioTrack *track);
int sf_audio_track_set_stereo_volume(SfAudioTrack *track, float left, float right);

/* Bytes are interleaved signed little-endian PCM16 stereo. The input address
 * may be unaligned. A nonzero size must be a multiple of four bytes.
 * Writes block on real platform output and drain; no software audio is queued.
 * Vita consumes blocks of 64 frames: an incomplete trailing block is not
 * accepted, and the return value reports the completed prefix (short write).
 * A request below 64 frames returns BAD_VALUE. Paused/stopped writes return 0.
 * On platform failure, prior completed blocks are returned as a short write;
 * the track becomes uninitialized and last_platform_error retains the SCE code.
 * The original mixer writes 1024 frames, so it does not encounter a remainder. */
int sf_audio_track_write(SfAudioTrack *track, const void *pcm, size_t bytes);
int sf_audio_track_state(SfAudioTrack *track);
int sf_audio_track_play_state(SfAudioTrack *track);
int sf_audio_track_playback_head(SfAudioTrack *track, uint32_t *frames);
int sf_audio_track_last_platform_error(SfAudioTrack *track);
