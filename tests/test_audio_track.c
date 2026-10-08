#include "audio_track.h"

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psp2/audioout.h>

static int opens, releases, outputs, drains, configs, current_frames, port_live;
static int open_error, release_error, config_error, volume_error, rest_error;
static int fail_output_call, fail_drain_call;
static int output_returns_frames;
static int last_volume[2];
static unsigned char captured[SF_AUDIO_BUFFER_BYTES * 8];
static size_t captured_bytes;
static pthread_mutex_t mock_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mock_cond = PTHREAD_COND_INITIALIZER;
static int block_output, output_entered, allow_output;
static const void *pending_pcm;
static unsigned char pending_copy[SF_AUDIO_BUFFER_BYTES];
static size_t pending_bytes;
static int remaining_frames;

/* Model deferred consumption, not an immediate memcpy-only audio device.
 * The previous buffer must remain valid until the next blocking submission. */
static void finish_pending(void) {
    if (pending_pcm) assert(!memcmp(pending_pcm, pending_copy, pending_bytes));
    pending_pcm = NULL;
    pending_bytes = 0;
    remaining_frames = 0;
}

int sceAudioOutOpenPort(int type, int len, int freq, int mode) {
    assert(type == SCE_AUDIO_OUT_PORT_TYPE_BGM);
    assert(len == 1024 && freq == 44100 && mode == SCE_AUDIO_OUT_MODE_STEREO);
    ++opens;
    if (open_error) return open_error;
    assert(!port_live);
    port_live = 1;
    current_frames = len;
    return 27;
}

int sceAudioOutReleasePort(int port) {
    assert(port == 27 && port_live);
    ++releases;
    if (release_error) return release_error;
    finish_pending();
    port_live = 0;
    return 0;
}

int sceAudioOutSetConfig(int port, int len, int freq, int mode) {
    assert(port == 27 && port_live);
    assert(len >= 64 && len <= 1024 && len % 64 == 0);
    assert(freq == 44100 && mode == SCE_AUDIO_OUT_MODE_STEREO);
    ++configs;
    if (config_error) return config_error;
    current_frames = len;
    return 0;
}

int sceAudioOutSetVolume(int port, int flags, const int *volumes) {
    assert(port == 27 && port_live && flags == 3);
    if (volume_error) return volume_error;
    last_volume[0] = volumes[0];
    last_volume[1] = volumes[1];
    return 0;
}

int sceAudioOutOutput(int port, const void *pcm) {
    assert(port == 27 && port_live);
    if (pcm) {
        assert(((uintptr_t)pcm & 63) == 0);
        assert(pcm != pending_pcm);
        if (pending_pcm) assert(!memcmp(pending_pcm, pending_copy, pending_bytes));
        ++outputs;
        if (outputs == fail_output_call) return -101;
        pthread_mutex_lock(&mock_mutex);
        if (block_output) {
            output_entered = 1;
            pthread_cond_broadcast(&mock_cond);
            while (!allow_output) pthread_cond_wait(&mock_cond, &mock_mutex);
        }
        pthread_mutex_unlock(&mock_mutex);
        finish_pending();
        size_t bytes = (size_t)current_frames * 4;
        assert(captured_bytes + bytes <= sizeof(captured));
        memcpy(captured + captured_bytes, pcm, bytes);
        captured_bytes += bytes;
        pending_pcm = pcm;
        pending_bytes = bytes;
        memcpy(pending_copy, pcm, bytes);
        remaining_frames = current_frames;
        return output_returns_frames ? current_frames : 0;
    }
    ++drains;
    if (drains == fail_drain_call) return -102;
    finish_pending();
    return 0;
}

int sceAudioOutGetRestSample(int port) {
    assert(port == 27 && port_live);
    return rest_error ? rest_error : remaining_frames;
}

static void reset_mock(void) {
    assert(!port_live);
    opens = releases = outputs = drains = configs = current_frames = 0;
    open_error = release_error = config_error = volume_error = rest_error = 0;
    fail_output_call = fail_drain_call = 0;
    output_returns_frames = 0;
    captured_bytes = 0;
    block_output = output_entered = allow_output = 0;
    assert(!pending_pcm && !remaining_frames);
    last_volume[0] = last_volume[1] = -1;
}

static SfAudioTrack *create(void) {
    SfAudioTrack *track = NULL;
    int native_error = 99;
    assert(sf_audio_track_create(3, 44100, 12, 2, 4096, 1, &track, &native_error) == 0);
    assert(track && native_error == 0);
    assert(sf_audio_track_state(track) == SF_AUDIO_STATE_INITIALIZED);
    assert(sf_audio_track_play_state(track) == SF_AUDIO_PLAYSTATE_STOPPED);
    return track;
}

static uint32_t head(SfAudioTrack *track) {
    uint32_t frames = 99;
    assert(sf_audio_track_playback_head(track, &frames) == 0);
    return frames;
}

static unsigned char samples[4096 * 3 + 5];
static atomic_int writer_done;
static SfAudioTrack *writer_track;
static int writer_result;

static void *writer(void *unused) {
    (void)unused;
    writer_result = sf_audio_track_write(writer_track, samples + 1, 4096);
    atomic_store(&writer_done, 1);
    return NULL;
}

int main(void) {
    for (size_t i = 0; i < sizeof(samples); ++i) samples[i] = (unsigned char)(i * 37 + 13);
    assert(sf_audio_track_min_buffer(44100, 12, 2) == 4096);
    assert(sf_audio_track_min_buffer(48000, 12, 2) == SF_AUDIO_BAD_VALUE);
    assert(sf_audio_track_min_buffer(44100, 4, 2) == SF_AUDIO_BAD_VALUE);
    assert(sf_audio_track_min_buffer(44100, 12, 3) == SF_AUDIO_BAD_VALUE);
    SfAudioTrack *track = NULL;
    int native_error = 99;
    assert(sf_audio_track_create(2, 44100, 12, 2, 4096, 1, &track, &native_error) == -2);
    assert(sf_audio_track_create(3, 44100, 12, 2, 4096, 0, &track, &native_error) == -2);
    assert(sf_audio_track_create(3, 44100, 12, 2, 2048, 1, &track, &native_error) == -2);
    assert(!track && !opens && native_error == 0);

    open_error = -103;
    assert(sf_audio_track_create(3, 44100, 12, 2, 4096, 1, &track, &native_error) == -1);
    assert(!track && native_error == -103 && !port_live && !releases);
    reset_mock();

    track = create();
    assert(sf_audio_track_write(track, samples, 4096) == 0 && !outputs);
    assert(sf_audio_track_play(track) == 0);
    assert(sf_audio_track_play_state(track) == SF_AUDIO_PLAYSTATE_PLAYING);
    assert(sf_audio_track_flush(track) == SF_AUDIO_INVALID_OPERATION);
    assert(sf_audio_track_write(track, NULL, 4) == SF_AUDIO_BAD_VALUE);
    assert(sf_audio_track_write(track, samples, 7) == SF_AUDIO_BAD_VALUE);
    assert(sf_audio_track_write(track, samples, 4) == SF_AUDIO_BAD_VALUE);
    assert(!outputs && !head(track));
    assert(sf_audio_track_write(track, samples + 1, 4096 * 2 + 64 * 4) == 4096 * 2 + 64 * 4);
    assert(outputs == 3 && drains == 1 && configs == 1);
    assert(captured_bytes == 4096 * 2 + 64 * 4);
    assert(!memcmp(captured, samples + 1, captured_bytes));
    assert(head(track) == 2048); /* Last 64 frames are still queued. */
    assert(sf_audio_track_write(track, samples, 256 + 4) == 256);
    assert(captured_bytes == 4096 * 2 + 64 * 4 + 256);
    assert(!memcmp(captured + captured_bytes - 256, samples, 256));
    assert(sf_audio_track_set_stereo_volume(track, 0.25f, 0.75f) == 0);
    assert(last_volume[0] == 8192 && last_volume[1] == 24576);
    assert(sf_audio_track_set_stereo_volume(track, NAN, 1) == -2);
    assert(sf_audio_track_set_stereo_volume(track, -0.1f, 1) == -2);
    assert(sf_audio_track_pause(track) == 0);
    assert(sf_audio_track_play_state(track) == SF_AUDIO_PLAYSTATE_PAUSED);
    uint32_t before = head(track);
    int before_outputs = outputs;
    assert(sf_audio_track_write(track, samples, 4096) == 0);
    assert(head(track) == before && outputs == before_outputs);
    assert(sf_audio_track_flush(track) == 0 && !head(track));
    assert(sf_audio_track_play(track) == 0);
    assert(sf_audio_track_write(track, samples, 4096) == 4096);
    assert(current_frames == 1024);
    assert(sf_audio_track_stop(track) == 0 && !head(track));
    assert(sf_audio_track_release(track) == 0 && !port_live);
    assert(sf_audio_track_state(track) == 0);
    assert(sf_audio_track_play(track) == -3);
    assert(sf_audio_track_write(track, samples, 4096) == -3);
    assert(sf_audio_track_release(track) == 0 && releases == 1);
    assert(sf_audio_track_destroy(&track) == 0 && !track && releases == 1);
    reset_mock();

    /* VitaSDK documents zero success; Vita3K returns the submitted frame count.
     * Both successful ABI results must preserve the byte count returned to JNI. */
    track = create();
    assert(sf_audio_track_play(track) == 0);
    output_returns_frames = 1;
    assert(sf_audio_track_write(track, samples, 4096) == 4096);
    assert(outputs == 1 && drains == 0 && head(track) == 0);
    remaining_frames = 512;
    assert(head(track) == 512 && drains == 0);
    finish_pending();
    assert(head(track) == 1024 && drains == 0);
    assert(captured_bytes == 4096 && !memcmp(captured, samples, 4096));
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    /* A failed second output reports only the first accepted block. */
    track = create();
    assert(sf_audio_track_play(track) == 0);
    fail_output_call = 2;
    assert(sf_audio_track_write(track, samples, 8192) == 4096);
    assert(captured_bytes == 4096 && !memcmp(captured, samples, 4096));
    assert(sf_audio_track_state(track) == 0);
    assert(sf_audio_track_last_platform_error(track) == -101);
    uint32_t frames = 0;
    assert(sf_audio_track_playback_head(track, &frames) == -3);
    assert(sf_audio_track_write(track, samples, 4096) == -3);
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    track = create();
    assert(sf_audio_track_play(track) == 0);
    assert(sf_audio_track_write(track, samples, 4096) == 4096 && drains == 0);
    fail_drain_call = 1;
    assert(sf_audio_track_write(track, samples, 256) == -1); /* Reconfigure drain fails. */
    assert(sf_audio_track_last_platform_error(track) == -102);
    frames = 99;
    assert(sf_audio_track_playback_head(track, &frames) == -3 && frames == 0);
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    track = create();
    rest_error = -107;
    assert(sf_audio_track_playback_head(track, &frames) == SF_AUDIO_ERROR);
    assert(sf_audio_track_last_platform_error(track) == -107);
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    track = create();
    assert(sf_audio_track_play(track) == 0);
    config_error = -104;
    assert(sf_audio_track_write(track, samples, 256) == -1 && !outputs);
    assert(sf_audio_track_last_platform_error(track) == -104);
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    track = create();
    volume_error = -105;
    assert(sf_audio_track_set_stereo_volume(track, 1, 1) == -1);
    assert(sf_audio_track_last_platform_error(track) == -105);
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    /* A failed port close retains the owning handle so release can be retried. */
    track = create();
    release_error = -106;
    assert(sf_audio_track_destroy(&track) == -1 && track && port_live);
    assert(sf_audio_track_last_platform_error(track) == -106);
    release_error = 0;
    assert(sf_audio_track_destroy(&track) == 0 && !track && !port_live);
    reset_mock();

    /* Caller memory may change immediately, but in-flight audio cannot.
     * Three writes also exercise reuse of the first owned buffer. No silence,
     * drain calls, reconfiguration, duplication or dropped PCM may be inserted. */
    track = create();
    assert(sf_audio_track_play(track) == 0);
    unsigned char caller[4096];
    for (int i = 0; i < 3; ++i) {
        memcpy(caller, samples + i * 4096, 4096);
        assert(sf_audio_track_write(track, caller, sizeof(caller)) == sizeof(caller));
        memset(caller, 0, sizeof(caller));
    }
    assert(outputs == 3 && drains == 0 && configs == 0);
    assert(captured_bytes == 3 * 4096 && !memcmp(captured, samples, captured_bytes));
    assert(head(track) == 2048);
    assert(sf_audio_track_pause(track) == 0 && head(track) == 3072);
    assert(sf_audio_track_destroy(&track) == 0);
    reset_mock();

    /* Backpressure comes from a blocking submission, without a per-block drain. */
    track = create();
    assert(sf_audio_track_play(track) == 0);
    assert(sf_audio_track_write(track, samples + 1, 4096) == 4096);
    writer_track = track;
    block_output = 1;
    atomic_store(&writer_done, 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, writer, NULL) == 0);
    pthread_mutex_lock(&mock_mutex);
    while (!output_entered) pthread_cond_wait(&mock_cond, &mock_mutex);
    assert(!atomic_load(&writer_done));
    assert(captured_bytes == 4096 && !memcmp(captured, samples + 1, 4096));
    allow_output = 1;
    pthread_cond_broadcast(&mock_cond);
    pthread_mutex_unlock(&mock_mutex);
    assert(pthread_join(thread, NULL) == 0);
    assert(atomic_load(&writer_done) && writer_result == 4096 && head(track) == 1024);
    assert(drains == 0);
    block_output = 0;
    assert(sf_audio_track_destroy(&track) == 0);
    puts("AudioTrack PCM output, backpressure, state and failure tests passed");
    return 0;
}
