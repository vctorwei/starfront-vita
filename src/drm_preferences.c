/* The Gingerbread SHA1PRNG expansion arithmetic below follows Apache Harmony's
 * AOSP implementation. Those source files and their Apache License 2.0 notices
 * are preserved unchanged in tests/reference/drm-gingerbread. The compatibility
 * portion is available under that license: http://www.apache.org/licenses/LICENSE-2.0
 * It is provided on an AS IS basis, without warranties or conditions. */
#define _POSIX_C_SOURCE 200809L
#include "drm_preferences.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef SF_DRM_TEST_IO
#include "drm_io_mock.h"
#endif

#ifdef SF_DRM_HOST_OPENSSL
#include <openssl/evp.h>
#else
#include <polarssl/aes.h>
#include <polarssl/base64.h>
#endif

#define PREF_COUNT 20
#define MAX_VALUE 16384U
#define FORMAT_VERSION 1U
static const unsigned char file_magic[8] = {'S','F','G','L','P','R','E','F'};
static const char validation_url[] =
    "http://confirmation.gameloft.com/partners/android/validate_key.php?key=#KEY#&product=#PRODUCT_ID#&imei=#ID#";

struct SfDrmPreferences {
    char *values[PREF_COUNT];
    unsigned char *serial;
    size_t serial_size;
    SfDrmState state;
};

static int failure(SfDrmError *error, int code, int system_error, int crypto_error) {
    if (error) *error = (SfDrmError){code, system_error, crypto_error};
    return code;
}

static void clear_error(SfDrmError *error) {
    if (error) *error = (SfDrmError){0, 0, 0};
}

static uint32_t rotate_left(uint32_t value, unsigned shift) {
    return (value << shift) | (value >> (32U - shift));
}

/* One nextBytes(16) call for the fixed, eight-byte "SFHPnull" seed.
 * This is the Gingerbread SHA1PRNG engine's first counter expansion frame,
 * including its preserved lastWord=(seedBytes+7)>>(3-1) layout bug. It is not
 * SHA1(seed) truncation: a counter and the PRNG's own padding frame are hashed.
 * Reference: AOSP SHA1PRNG_SecureRandomImpl.engineNextBytes and SHA1Impl.
 * A longer output or a different seed needs the full stateful algorithm. */
static void gingerbread_key(unsigned char key[16]) {
    uint32_t words[80] = {0};
    uint32_t h[5] = {0x67452301U, 0xefcdab89U, 0x98badcfeU,
                     0x10325476U, 0xc3d2e1f0U};
    words[0] = 0x53464850U; /* SFHP */
    words[1] = 0x6e756c6cU; /* null */
    /* lastWord=3; counter=0 occupies words 3 and 4, word 2 stays zero. */
    words[5] = 0x80000000U;
    words[15] = (8U << 3) + 64U;
    for (unsigned i = 16; i < 80; ++i)
        words[i] = rotate_left(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (unsigned i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999U; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1U; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcU; }
        else { f = b ^ c ^ d; k = 0xca62c1d6U; }
        uint32_t next = rotate_left(a, 5) + f + e + k + words[i];
        e = d; d = c; c = rotate_left(b, 30); b = a; a = next;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    for (unsigned i = 0; i < 16; ++i)
        key[i] = (unsigned char)(h[i / 4] >> (24U - 8U * (i % 4)));
}

static int aes_blocks(const unsigned char key[16], int encrypt,
                       const unsigned char *input, unsigned char *output,
                       size_t size, SfDrmError *error) {
#ifdef SF_DRM_HOST_OPENSSL
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (!context) return failure(error, SF_DRM_NO_MEMORY, 0, 0);
    int written = 0, final_written = 0;
    int ok = EVP_CipherInit_ex(context, EVP_aes_128_ecb(), NULL, key, NULL, encrypt);
    if (ok) ok = EVP_CIPHER_CTX_set_padding(context, 0);
    if (ok) ok = EVP_CipherUpdate(context, output, &written, input, (int)size);
    if (ok) ok = EVP_CipherFinal_ex(context, output + written, &final_written);
    EVP_CIPHER_CTX_free(context);
    if (!ok || (size_t)(written + final_written) != size)
        return failure(error, SF_DRM_CRYPTO_ERROR, 0, -1);
    return SF_DRM_OK;
#else
    aes_context context;
    aes_init(&context);
    int code = encrypt ? aes_setkey_enc(&context, key, 128)
                       : aes_setkey_dec(&context, key, 128);
    for (size_t offset = 0; !code && offset < size; offset += 16)
        code = aes_crypt_ecb(&context, encrypt ? AES_ENCRYPT : AES_DECRYPT,
                             input + offset, output + offset);
    aes_free(&context);
    if (code) return failure(error, SF_DRM_CRYPTO_ERROR, 0, code);
    return SF_DRM_OK;
#endif
}

static int encrypt_value(const unsigned char *plain, size_t size,
                           char **encoded, SfDrmError *error) {
    *encoded = NULL;
    if ((!plain && size) || size > MAX_VALUE)
        return failure(error, SF_DRM_BAD_VALUE, 0, 0);
    size_t padded_size = (size / 16 + 1) * 16;
    unsigned char *padded = malloc(padded_size);
    unsigned char *cipher = malloc(padded_size);
    size_t encoded_capacity = ((padded_size + 2) / 3) * 4 + 1;
    char *result = malloc(encoded_capacity);
    if (!padded || !cipher || !result) {
        free(padded); free(cipher); free(result);
        return failure(error, SF_DRM_NO_MEMORY, 0, 0);
    }
    if (size) memcpy(padded, plain, size);
    memset(padded + size, (int)(padded_size - size), padded_size - size);
    unsigned char key[16];
    gingerbread_key(key);
    int code = aes_blocks(key, 1, padded, cipher, padded_size, error);
    if (!code) {
#ifdef SF_DRM_HOST_OPENSSL
        int length = EVP_EncodeBlock((unsigned char *)result, cipher, (int)padded_size);
        if (length < 0) code = failure(error, SF_DRM_CRYPTO_ERROR, 0, length);
        else result[length] = 0;
#else
        size_t length = encoded_capacity;
        int backend = base64_encode((unsigned char *)result, &length, cipher, padded_size);
        if (backend) code = failure(error, SF_DRM_CRYPTO_ERROR, 0, backend);
        else result[length] = 0;
#endif
    }
    memset(key, 0, sizeof(key));
    free(padded); free(cipher);
    if (code) free(result);
    else *encoded = result;
    return code;
}

static int decode_base64(const char *encoded, unsigned char **bytes,
                          size_t *size, SfDrmError *error) {
    *bytes = NULL; *size = 0;
    size_t length = strlen(encoded);
    if (!length || length > MAX_VALUE || length % 4)
        return failure(error, SF_DRM_CRYPTO_ERROR, 0, -1);
    /* Files written by this module use the original options=0 alphabet and
     * have no whitespace. Reject malformed values rather than repair them. */
    size_t padding = encoded[length-1] == '=' ? 1 : 0;
    if (length > 1 && encoded[length-2] == '=') ++padding;
    for (size_t i = 0; i < length - padding; ++i) {
        unsigned char c = (unsigned char)encoded[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '+' || c == '/'))
            return failure(error, SF_DRM_CRYPTO_ERROR, 0, -1);
    }
    size_t capacity = length / 4 * 3;
    unsigned char *result = malloc(capacity);
    if (!result) return failure(error, SF_DRM_NO_MEMORY, 0, 0);
#ifdef SF_DRM_HOST_OPENSSL
    int output_length = EVP_DecodeBlock(result, (const unsigned char *)encoded, (int)length);
    if (output_length < 0) {
        free(result);
        return failure(error, SF_DRM_CRYPTO_ERROR, 0, output_length);
    }
    *size = (size_t)output_length - padding;
#else
    int backend = base64_decode(result, &capacity, (const unsigned char *)encoded, length);
    if (backend) {
        free(result);
        return failure(error, SF_DRM_CRYPTO_ERROR, 0, backend);
    }
    *size = capacity;
#endif
    *bytes = result;
    return SF_DRM_OK;
}

static int decrypt_value(const char *encoded, unsigned char **plain,
                           size_t *size, SfDrmError *error) {
    *plain = NULL; *size = 0;
    if (!encoded) return failure(error, SF_DRM_BAD_VALUE, 0, 0);
    unsigned char *cipher;
    size_t cipher_size;
    int code = decode_base64(encoded, &cipher, &cipher_size, error);
    if (code) return code;
    if (!cipher_size || cipher_size % 16) {
        free(cipher);
        return failure(error, SF_DRM_CRYPTO_ERROR, 0, -1);
    }
    unsigned char *result = malloc(cipher_size + 1);
    if (!result) { free(cipher); return failure(error, SF_DRM_NO_MEMORY, 0, 0); }
    unsigned char key[16];
    gingerbread_key(key);
    code = aes_blocks(key, 0, cipher, result, cipher_size, error);
    memset(key, 0, sizeof(key));
    free(cipher);
    if (!code) {
        size_t padding = result[cipher_size-1];
        if (!padding || padding > 16) code = failure(error, SF_DRM_CRYPTO_ERROR, 0, -1);
        else for (size_t i = cipher_size - padding; i < cipher_size; ++i)
            if (result[i] != padding) { code = failure(error, SF_DRM_CRYPTO_ERROR, 0, -1); break; }
        if (!code) { *size = cipher_size - padding; result[*size] = 0; }
    }
    if (code) free(result);
    else *plain = result;
    return code;
}

static uint32_t java_next(uint64_t *seed, unsigned bits) {
    *seed = (*seed * UINT64_C(0x5deece66d) + 11) & UINT64_C(0xffffffffffff);
    return (uint32_t)(*seed >> (48U - bits));
}

static int32_t signed32(uint32_t value) {
    int32_t result; memcpy(&result, &value, sizeof(result)); return result;
}

static int64_t java_next_long(uint64_t *seed) {
    uint64_t upper = (uint64_t)java_next(seed, 32) << 32;
    int32_t lower = signed32(java_next(seed, 32));
    uint64_t bits = upper + (uint64_t)(int64_t)lower;
    int64_t result; memcpy(&result, &bits, sizeof(result)); return result;
}

static int constructor_seed(const SfDrmOptions *options, uint64_t *seed,
                              SfDrmError *error) {
    uint64_t value = 0;
    if (options && options->random_bytes) {
        unsigned char bytes[8];
        int code = options->random_bytes(options->random_context, bytes, sizeof(bytes));
        if (code) return failure(error, SF_DRM_RANDOM_ERROR, code, 0);
        for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)bytes[i] << (i * 8);
    } else {
        struct timeval now;
        if (gettimeofday(&now, NULL)) return failure(error, SF_DRM_IO_ERROR, errno, 0);
        value = (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_usec / 1000;
    }
    *seed = (value ^ UINT64_C(0x5deece66d)) & UINT64_C(0xffffffffffff);
    return SF_DRM_OK;
}

static int generate_fresh(SfDrmPreferences *preferences, uint64_t *seed,
                            SfDrmError *error) {
    for (unsigned i = 0; i < PREF_COUNT; ++i) {
        char plain[256];
        int length;
        if (i == 18) {
            length = snprintf(plain, sizeof(plain), "%s", validation_url);
        } else {
            int64_t first = java_next_long(seed);
            if (i == 3) {
                int32_t second = signed32(java_next(seed, 32));
                length = snprintf(plain, sizeof(plain), "%" PRId64 "#null#%" PRId32 "#1188", first, second);
            } else if (i == 11) {
                length = snprintf(plain, sizeof(plain), "%" PRId64 "#null#0", first);
            } else {
                int32_t second = signed32(java_next(seed, 32));
                int32_t third = signed32(java_next(seed, 32));
                length = snprintf(plain, sizeof(plain), "%" PRId64 "#0#%" PRId32 "#%" PRId32, first, second, third);
            }
        }
        if (length < 0 || (size_t)length >= sizeof(plain))
            return failure(error, SF_DRM_BAD_VALUE, 0, 0);
        char *encoded;
        int code = encrypt_value((const unsigned char *)plain, (size_t)length, &encoded, error);
        if (code) return code;
        free(preferences->values[i]);
        preferences->values[i] = encoded;
    }
    preferences->state.string_count = PREF_COUNT;
    return SF_DRM_OK;
}

static int key_index(const char *key) {
    if (!key || strlen(key) != 4 || memcmp(key, "gl_", 3) || key[3] < 'a' || key[3] > 't')
        return -1;
    return key[3] - 'a';
}

static int read_exact(FILE *file, void *buffer, size_t size, SfDrmError *error) {
    if (fread(buffer, 1, size, file) == size) return SF_DRM_OK;
    if (ferror(file)) return failure(error, SF_DRM_IO_ERROR, errno, 0);
    return failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
}

static int write_exact(FILE *file, const void *buffer, size_t size, SfDrmError *error) {
    if (fwrite(buffer, 1, size, file) == size) return SF_DRM_OK;
    return failure(error, SF_DRM_IO_ERROR, errno, 0);
}

static int read_u32(FILE *file, uint32_t *value, SfDrmError *error) {
    unsigned char bytes[4];
    int code = read_exact(file, bytes, sizeof(bytes), error);
    if (!code) *value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
                       (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
    return code;
}

static int write_u32(FILE *file, uint32_t value, SfDrmError *error) {
    unsigned char bytes[4];
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (unsigned char)(value >> (i * 8));
    return write_exact(file, bytes, sizeof(bytes), error);
}

static int load_file(const char *path, SfDrmPreferences *preferences,
                       int *exists, SfDrmError *error) {
    *exists = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        int saved = errno;
        if (saved == ENOENT) return SF_DRM_OK;
        return failure(error, SF_DRM_IO_ERROR, saved, 0);
    }
    *exists = 1;
    unsigned char magic[8];
    uint32_t version = 0, count = 0;
    int code = read_exact(file, magic, sizeof(magic), error);
    if (!code && memcmp(magic, file_magic, sizeof(magic)))
        code = failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
    if (!code) code = read_u32(file, &version, error);
    if (!code) code = read_u32(file, &count, error);
    if (!code && (version != FORMAT_VERSION || count > PREF_COUNT))
        code = failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
    for (uint32_t i = 0; !code && i < count; ++i) {
        uint32_t type = 0, key_size = 0, value_size = 0;
        code = read_u32(file, &type, error);
        if (!code) code = read_u32(file, &key_size, error);
        if (!code) code = read_u32(file, &value_size, error);
        if (!code && (type != 1 || key_size != 4 || value_size > MAX_VALUE))
            code = failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
        char key[5] = {0};
        if (!code) code = read_exact(file, key, 4, error);
        int index = code ? -1 : key_index(key);
        if (!code && (index < 0 || preferences->values[index]))
            code = failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
        char *value = NULL;
        if (!code) {
            value = malloc((size_t)value_size + 1);
            if (!value) code = failure(error, SF_DRM_NO_MEMORY, 0, 0);
        }
        if (!code) code = read_exact(file, value, value_size, error);
        if (!code && memchr(value, 0, value_size))
            code = failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
        if (code) free(value);
        else { value[value_size] = 0; preferences->values[index] = value; ++preferences->state.string_count; }
    }
    if (!code) {
        int extra = fgetc(file);
        if (extra != EOF) code = failure(error, SF_DRM_INVALID_FORMAT, 0, 0);
        else if (ferror(file)) code = failure(error, SF_DRM_IO_ERROR, errno, 0);
    }
    if (fclose(file) && !code) code = failure(error, SF_DRM_IO_ERROR, errno, 0);
    return code;
}

static int require_absent(const char *path, SfDrmError *error) {
    struct stat status;
    if (!stat(path, &status)) return failure(error, SF_DRM_IO_ERROR, EEXIST, 0);
    int saved = errno;
    if (saved != ENOENT) return failure(error, SF_DRM_IO_ERROR, saved, 0);
    return SF_DRM_OK;
}

static int move_to_absent(const char *source, const char *destination,
                           SfDrmError *error) {
    int code = require_absent(destination, error);
    if (!code && rename(source, destination))
        code = failure(error, SF_DRM_IO_ERROR, errno, 0);
    return code;
}

static int save_file(const char *path, const SfDrmPreferences *preferences,
                       int target_exists, SfDrmError *error) {
    SfDrmError fallback_error;
    if (!error) error = &fallback_error;
    clear_error(error);
    size_t length = strlen(path);
    if (length > SIZE_MAX - 5) return failure(error, SF_DRM_BAD_VALUE, 0, 0);
    char *temporary = malloc(length + 5);
    char *backup = malloc(length + 5);
    if (!temporary || !backup) {
        free(temporary); free(backup);
        return failure(error, SF_DRM_NO_MEMORY, 0, 0);
    }
    memcpy(temporary, path, length); memcpy(temporary + length, ".tmp", 5);
    memcpy(backup, path, length); memcpy(backup + length, ".bak", 5);
    /* An earlier failed recovery may have left the only old copy here. Never
     * replace that backup or silently create new state over it. */
    int code = require_absent(backup, error);
    if (code) { free(temporary); free(backup); return code; }
    FILE *file = fopen(temporary, "wb");
    if (!file) { int saved = errno; free(temporary); free(backup); return failure(error, SF_DRM_IO_ERROR, saved, 0); }
    code = write_exact(file, file_magic, sizeof(file_magic), error);
    if (!code) code = write_u32(file, FORMAT_VERSION, error);
    if (!code) code = write_u32(file, (uint32_t)preferences->state.string_count, error);
    for (unsigned i = 0; !code && i < PREF_COUNT; ++i) {
        if (!preferences->values[i]) continue;
        char key[5] = {'g','l','_', (char)('a' + i), 0};
        size_t value_size = strlen(preferences->values[i]);
        code = write_u32(file, 1, error);
        if (!code) code = write_u32(file, 4, error);
        if (!code) code = write_u32(file, (uint32_t)value_size, error);
        if (!code) code = write_exact(file, key, 4, error);
        if (!code) code = write_exact(file, preferences->values[i], value_size, error);
    }
    if (!code && fflush(file)) code = failure(error, SF_DRM_IO_ERROR, errno, 0);
    if (fclose(file) && !code) code = failure(error, SF_DRM_IO_ERROR, errno, 0);
    int backed_up = 0;
    if (!code && target_exists) {
        code = move_to_absent(path, backup, error);
        backed_up = !code;
    }
    if (!code) {
        SfDrmError commit_error = {0, 0, 0};
        code = move_to_absent(temporary, path, &commit_error);
        if (code) {
            /* Vita newlib rename unlinks its destination before sceIoRename.
             * Both commit and restoration therefore target absent paths.
             * A failed restoration leaves .bak intact for explicit recovery;
             * keep the first commit errno even if restoration also fails. */
            if (backed_up) {
                SfDrmError restore_error;
                (void)move_to_absent(backup, path, &restore_error);
            }
            if (error) *error = commit_error;
            errno = commit_error.system_error;
        } else if (backed_up && remove(backup)) {
            code = failure(error, SF_DRM_IO_ERROR, errno, 0);
        }
    }
    int saved_error = code ? error->system_error : 0;
    if (code) remove(temporary);
    free(temporary); free(backup);
    if (code) errno = saved_error;
    return code;
}

static int reload_state(SfDrmPreferences *preferences, SfDrmError *error) {
    if (!preferences->values[11]) return SF_DRM_OK;
    unsigned char *decoded;
    size_t size;
    SfDrmError ignored;
    int code = decrypt_value(preferences->values[11], &decoded, &size, &ignored);
    if (code == SF_DRM_NO_MEMORY) return failure(error, code, 0, 0);
    if (code) {
        preferences->state.reload_exception_caught = 1;
        return SF_DRM_OK;
    }
    /* Java String.split("#") drops trailing empty components. */
    size_t effective = size;
    while (effective && decoded[effective-1] == '#') --effective;
    unsigned components = effective || !size ? 1 : 0;
    for (size_t i = 0; i < effective; ++i) if (decoded[i] == '#') ++components;
    if (components != 3) {
        preferences->state.free_count = 2;
        preferences->state.invalid_save = 1;
    } else {
        /* decoded[1].compareTo(k=NULL) throws inside b()'s existing catch.
         * Do not convert that exception into a license status or a new count. */
        preferences->state.reload_exception_caught = 1;
    }
    free(decoded);
    return SF_DRM_OK;
}

void sf_drm_preferences_destroy(SfDrmPreferences **preferences) {
    if (!preferences || !*preferences) return;
    SfDrmPreferences *current = *preferences;
    for (unsigned i = 0; i < PREF_COUNT; ++i) free(current->values[i]);
    free(current->serial);
    free(current);
    *preferences = NULL;
}

int sf_drm_preferences_prepare(const char *path, const char *device_id,
                               const SfDrmOptions *options,
                               SfDrmPreferences **out, SfDrmError *error) {
    clear_error(error);
    if (!out) return failure(error, SF_DRM_BAD_VALUE, 0, 0);
    *out = NULL;
    if (!path || !*path || (options && !options->random_bytes && options->random_context))
        return failure(error, SF_DRM_BAD_VALUE, 0, 0);
    if (device_id) return failure(error, SF_DRM_UNSUPPORTED_PROFILE, 0, 0);
    SfDrmPreferences *preferences = calloc(1, sizeof(*preferences));
    if (!preferences) return failure(error, SF_DRM_NO_MEMORY, 0, 0);
    uint64_t seed;
    int code = constructor_seed(options, &seed, error);
    int exists = 0;
    if (!code) code = load_file(path, preferences, &exists, error);
    preferences->state.loaded_existing = exists;
    if (!code && !preferences->values[0]) {
        preferences->serial = malloc(5);
        if (!preferences->serial) code = failure(error, SF_DRM_NO_MEMORY, 0, 0);
        else { memcpy(preferences->serial, "1188", 5); preferences->serial_size = 4; }
        if (!code) code = generate_fresh(preferences, &seed, error);
        if (!code) code = save_file(path, preferences, exists, error);
    } else if (!code) code = reload_state(preferences, error);
    if (code) sf_drm_preferences_destroy(&preferences);
    else *out = preferences;
    return code;
}

int sf_drm_preferences_state(const SfDrmPreferences *preferences, SfDrmState *state) {
    if (!preferences || !state) return SF_DRM_BAD_VALUE;
    *state = preferences->state;
    return SF_DRM_OK;
}

const char *sf_drm_preferences_get_string(const SfDrmPreferences *preferences, const char *key) {
    int index = key_index(key);
    return preferences && index >= 0 ? preferences->values[index] : NULL;
}

const unsigned char *sf_drm_preferences_serial(const SfDrmPreferences *preferences, size_t *size) {
    if (size) *size = preferences ? preferences->serial_size : 0;
    return preferences ? preferences->serial : NULL;
}

static int valid_utf8(const unsigned char *bytes, size_t size) {
    for (size_t i = 0; i < size;) {
        unsigned char first = bytes[i++];
        if (first < 0x80) continue;
        unsigned continuation;
        uint32_t value, minimum;
        if (first >= 0xc2 && first <= 0xdf) { continuation = 1; value = first & 31U; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { continuation = 2; value = first & 15U; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { continuation = 3; value = first & 7U; minimum = 0x10000; }
        else return 0;
        if (continuation > size - i) return 0;
        while (continuation--) {
            unsigned char next = bytes[i++];
            if ((next & 0xc0U) != 0x80U) return 0;
            value = (value << 6) | (next & 63U);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
    }
    return 1;
}

int sf_drm_preferences_set_resource_serial(SfDrmPreferences *preferences,
                                           const unsigned char *bytes,
                                           size_t size, SfDrmError *error) {
    clear_error(error);
    if (!preferences || (!bytes && size) || size > MAX_VALUE || !valid_utf8(bytes, size))
        return failure(error, SF_DRM_BAD_VALUE, 0, 0);
    unsigned char *serial = malloc(size + 1);
    if (!serial) return failure(error, SF_DRM_NO_MEMORY, 0, 0);
    if (size) memcpy(serial, bytes, size);
    serial[size] = 0;
    free(preferences->serial); preferences->serial = serial; preferences->serial_size = size;
    return SF_DRM_OK;
}

#ifdef SF_DRM_TESTING
void sf_drm_test_gingerbread_key(unsigned char key[16]) { gingerbread_key(key); }
int sf_drm_test_encrypt(const unsigned char *plain, size_t size, char **encoded, SfDrmError *error) {
    clear_error(error); return encrypt_value(plain, size, encoded, error);
}
int sf_drm_test_decrypt(const char *encoded, unsigned char **plain, size_t *size, SfDrmError *error) {
    clear_error(error); return decrypt_value(encoded, plain, size, error);
}
#endif
