#pragma once
#include "../src/drm_preferences.h"

typedef struct {
    int prepared, state_queries, serial_assignments, destroyed;
    int fail_prepare, fail_state, fail_serial;
    int loaded_existing;
    SfDrmError error;
    unsigned char serial[11];
} JniDrmMock;
extern JniDrmMock jni_drm_mock;
