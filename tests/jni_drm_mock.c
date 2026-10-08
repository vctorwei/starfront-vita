/* Only tests the JNI boundary and ordering. Real crypto/persistence has its
   own backend tests; this double never claims to validate that backend. */
#include "jni_drm_mock.h"
#include "../src/port.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct SfDrmPreferences { unsigned char serial[11]; };
JniDrmMock jni_drm_mock;

int sf_drm_preferences_prepare(const char *path,const char *device_id,
                               const SfDrmOptions *options,
                               SfDrmPreferences **out,SfDrmError *error) {
    assert(!strcmp(path,SAVE_DIR "/GLoft.preferences"));
    assert(device_id==NULL && options==NULL && out && error);
    assert(jni_drm_mock.prepared==0);
    jni_drm_mock.prepared++;*out=NULL;*error=(SfDrmError){0};
    if(jni_drm_mock.fail_prepare){*error=jni_drm_mock.error;return error->code;}
    *out=calloc(1,sizeof(**out));assert(*out);return SF_DRM_OK;
}
int sf_drm_preferences_state(const SfDrmPreferences *preferences,SfDrmState *state) {
    assert(preferences && state && jni_drm_mock.prepared==1);
    assert(jni_drm_mock.serial_assignments==0);
    jni_drm_mock.state_queries++;
    if(jni_drm_mock.fail_state)return SF_DRM_INVALID_FORMAT;
    *state=(SfDrmState){.free_count=0,.loaded_existing=jni_drm_mock.loaded_existing,
        .reload_exception_caught=jni_drm_mock.loaded_existing,.string_count=20};
    return SF_DRM_OK;
}
int sf_drm_preferences_set_resource_serial(SfDrmPreferences *preferences,
                                           const unsigned char *bytes,
                                           size_t size,SfDrmError *error) {
    assert(preferences && bytes && error && size==11);
    assert(jni_drm_mock.prepared==1 && jni_drm_mock.state_queries==1);
    jni_drm_mock.serial_assignments++;*error=(SfDrmError){0};
    if(jni_drm_mock.fail_serial){*error=jni_drm_mock.error;return error->code;}
    memcpy(preferences->serial,bytes,size);memcpy(jni_drm_mock.serial,bytes,size);
    return SF_DRM_OK;
}
void sf_drm_preferences_destroy(SfDrmPreferences **preferences) {
    if(!preferences || !*preferences)return;
    free(*preferences);*preferences=NULL;jni_drm_mock.destroyed++;
}
