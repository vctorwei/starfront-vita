#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct SfDrmPreferences SfDrmPreferences;

enum {
    SF_DRM_OK = 0,
    SF_DRM_BAD_VALUE = -1,
    SF_DRM_NO_MEMORY = -2,
    SF_DRM_IO_ERROR = -3,
    SF_DRM_INVALID_FORMAT = -4,
    SF_DRM_CRYPTO_ERROR = -5,
    SF_DRM_UNSUPPORTED_PROFILE = -6,
    SF_DRM_RANDOM_ERROR = -7
};

typedef struct {
    int code;
    int system_error; /* The original errno, or caller random callback error. */
    int crypto_error; /* The original crypto backend error when applicable. */
} SfDrmError;

/* Optional deterministic entropy source for Java Random's obfuscation seed.
 * Fill exactly eight bytes; return zero on success, otherwise the original
 * error code. Bytes represent a little-endian 64-bit seed. With NULL options,
 * the constructor uses currentTimeMillis(), as the original Java does.
 * This does not supply or alter the fixed SHA1PRNG encryption seed. */
typedef int (*SfDrmRandomBytes)(void *context, unsigned char *bytes, size_t size);
typedef struct {
    SfDrmRandomBytes random_bytes;
    void *random_context;
} SfDrmOptions;

typedef struct {
    int free_count;
    int loaded_existing;
    int reload_exception_caught;
    int invalid_save;
    size_t string_count;
} SfDrmState;

/* Only the observed no-modem profile (device_id == NULL) is supported.
 * Uses Gingerbread Crypto-provider SHA1PRNG and AES/ECB/PKCS7Padding.
 * The private versioned file retains SharedPreferences string key/value
 * semantics. Android XML imports are explicitly unsupported.
 * First construction creates gl_a..gl_t with j=0 and l="1188". Reopening
 * preserves the original caught compareTo(NULL) exception and leaves j=0.
 * There is no authorized/successful-license return value in this API. */
int sf_drm_preferences_prepare(const char *path, const char *device_id,
                               const SfDrmOptions *options,
                               SfDrmPreferences **out, SfDrmError *error);
void sf_drm_preferences_destroy(SfDrmPreferences **preferences);
int sf_drm_preferences_state(const SfDrmPreferences *preferences,
                             SfDrmState *state);
const char *sf_drm_preferences_get_string(const SfDrmPreferences *preferences,
                                         const char *key);
const unsigned char *sf_drm_preferences_serial(
    const SfDrmPreferences *preferences, size_t *size);

/* The caller serializes operations and owns the lifetime of borrowed getters.
 * On existing-preference construction, l remains NULL until the resource
 * callback assigns it, as in the original constructor's reload branch. */

/* Mirrors GloftDRM.a() assigning l after the caller reads the APK resource.
 * The observed resource must be valid UTF-8. This assignment does not save
 * preferences or modify the constructor's already-persisted l="1188". */
int sf_drm_preferences_set_resource_serial(SfDrmPreferences *preferences,
                                           const unsigned char *bytes,
                                           size_t size, SfDrmError *error);

#ifdef SF_DRM_TESTING
/* Reference-vector access, omitted from the production public interface. */
void sf_drm_test_gingerbread_key(unsigned char key[16]);
int sf_drm_test_encrypt(const unsigned char *plain, size_t size,
                         char **encoded, SfDrmError *error);
int sf_drm_test_decrypt(const char *encoded, unsigned char **plain,
                         size_t *size, SfDrmError *error);
#endif
