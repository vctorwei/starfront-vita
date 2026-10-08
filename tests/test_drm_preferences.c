#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "drm_preferences.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { FAIL_NONE, FAIL_READ, FAIL_WRITE, FAIL_FLUSH, FAIL_CLOSE, FAIL_RENAME };
static int io_failure, random_failure, random_calls;
static unsigned rename_calls, rename_failure_mask, destructive_destinations;
static int fail_backup_remove;
static char *fixture_plain[20], *fixture_encoded[20];
static char *extra_plain[4], *extra_encoded[4];
static char fixture_key[33];

size_t sf_drm_mock_fread(void *p, size_t size, size_t count, FILE *f) {
    if (io_failure == FAIL_READ) { errno = EIO; return 0; }
    return fread(p, size, count, f);
}
size_t sf_drm_mock_fwrite(const void *p, size_t size, size_t count, FILE *f) {
    if (io_failure == FAIL_WRITE) {
        size_t partial = count / 2;
        if (partial) assert(fwrite(p, size, partial, f) == partial);
        errno = ENOSPC; return partial;
    }
    return fwrite(p, size, count, f);
}
int sf_drm_mock_ferror(FILE *f) {
    return io_failure == FAIL_READ ? 1 : ferror(f);
}
int sf_drm_mock_fflush(FILE *f) {
    if (io_failure == FAIL_FLUSH) { errno = EIO; return EOF; }
    return fflush(f);
}
int sf_drm_mock_fclose(FILE *f) {
    int code = fclose(f);
    if (io_failure == FAIL_CLOSE) { errno = EIO; return EOF; }
    return code;
}
int sf_drm_mock_rename(const char *old_path, const char *new_path) {
    ++rename_calls;
    /* Model Vita newlib's destructive destination removal, including failure
     * after removal. Correct transactions never pass an existing destination. */
    if (!access(new_path, F_OK)) ++destructive_destinations;
    (void)remove(new_path);
    if (io_failure == FAIL_RENAME) { errno = EXDEV; return -1; }
    if (rename_calls <= 32 && (rename_failure_mask & (1U << (rename_calls - 1)))) {
        errno = rename_calls == 2 ? EXDEV : EACCES;
        return -1;
    }
    return rename(old_path, new_path);
}
int sf_drm_mock_remove(const char *path) {
    size_t length = strlen(path);
    if (fail_backup_remove && length >= 4 && !strcmp(path + length - 4, ".bak")) {
        errno = EACCES; return -1;
    }
    return remove(path);
}

static int random_bytes(void *context, unsigned char *bytes, size_t size) {
    assert(context == &random_calls && size == 8);
    ++random_calls;
    if (random_failure) return random_failure;
    uint64_t seed = UINT64_C(123456789);
    for (unsigned i = 0; i < 8; ++i) bytes[i] = (unsigned char)(seed >> (i * 8));
    return 0;
}

static void load_fixture(void) {
    FILE *file = fopen("tests/fixtures/drm-gingerbread.tsv", "rb");
    assert(file);
    char *line = NULL;
    size_t capacity = 0;
    int extras = 0, values = 0;
    while (getline(&line, &capacity, file) >= 0) {
        size_t length = strlen(line);
        if (length && line[length-1] == '\n') line[--length] = 0;
        char *first = strchr(line, '\t');
        assert(first); *first++ = 0;
        if (!strcmp(line, "KEY")) { assert(strlen(first) == 32); memcpy(fixture_key, first, 33); continue; }
        if (!strcmp(line, "SEED")) { assert(!strcmp(first, "123456789")); continue; }
        char *second = strchr(first, '\t');
        assert(second); *second++ = 0;
        if (!strcmp(line, "VECTOR")) {
            assert(extras < 4);
            extra_plain[extras] = strdup(first); extra_encoded[extras++] = strdup(second);
        } else {
            assert(strlen(line) == 4 && !memcmp(line, "gl_", 3));
            int index = line[3] - 'a'; assert(index >= 0 && index < 20 && !fixture_plain[index]);
            fixture_plain[index] = strdup(first); fixture_encoded[index] = strdup(second); ++values;
        }
    }
    assert(!ferror(file) && !fclose(file) && values == 20 && extras == 4);
    free(line);
}

static void verify_crypto(void) {
    unsigned char key[16];
    char key_hex[33];
    sf_drm_test_gingerbread_key(key);
    for (unsigned i = 0; i < 16; ++i) snprintf(key_hex + i * 2, 3, "%02x", key[i]);
    assert(!strcmp(key_hex, fixture_key));
    for (unsigned i = 0; i < 24; ++i) {
        const char *text = i < 20 ? fixture_plain[i] : extra_plain[i-20];
        const char *expected = i < 20 ? fixture_encoded[i] : extra_encoded[i-20];
        SfDrmError error;
        char *encoded = NULL;
        assert(!sf_drm_test_encrypt((const unsigned char *)text, strlen(text), &encoded, &error));
        assert(error.code == 0 && !strcmp(encoded, expected));
        unsigned char *decoded = NULL;
        size_t size = 99;
        assert(!sf_drm_test_decrypt(expected, &decoded, &size, &error));
        assert(size == strlen(text) && !memcmp(decoded, text, size));
        free(encoded); free(decoded);
    }
    const char *invalid[] = {"", "%%%%", "YQ==", "AAAA", "=AAA", "AAA=AAAA"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        SfDrmError error; unsigned char *decoded = (void *)1; size_t size = 1;
        assert(sf_drm_test_decrypt(invalid[i], &decoded, &size, &error) == SF_DRM_CRYPTO_ERROR);
        assert(!decoded && !size && error.code == SF_DRM_CRYPTO_ERROR);
    }
}

static unsigned char *read_bytes(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END)); long length = ftell(file); assert(length >= 0);
    rewind(file); unsigned char *bytes = malloc((size_t)length + 1); assert(bytes);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length && !fclose(file));
    *size = (size_t)length; return bytes;
}

static void assert_file_bytes(const char *path, const unsigned char *expected, size_t size) {
    size_t actual_size;
    unsigned char *actual = read_bytes(path, &actual_size);
    assert(actual_size == size && !memcmp(actual, expected, size));
    free(actual);
}

static void put_u32(FILE *file, uint32_t value) {
    unsigned char bytes[4]; for (unsigned i = 0; i < 4; ++i) bytes[i] = (unsigned char)(value >> (i * 8));
    assert(fwrite(bytes, 1, 4, file) == 4);
}

static void write_partial_preferences(const char *path, const char *encoded_l,
                                      int include_a, int type) {
    FILE *file = fopen(path, "wb"); assert(file);
    assert(fwrite("SFGLPREF", 1, 8, file) == 8);
    put_u32(file, 1); put_u32(file, (unsigned)(!!encoded_l + !!include_a));
    for (unsigned i = 0; i < 2; ++i) {
        const char *value = i ? encoded_l : (include_a ? fixture_encoded[0] : NULL);
        if (!value) continue;
        const char *key = i ? "gl_l" : "gl_a";
        put_u32(file, (unsigned)type); put_u32(file, 4); put_u32(file, (unsigned)strlen(value));
        assert(fwrite(key, 1, 4, file) == 4);
        assert(fwrite(value, 1, strlen(value), file) == strlen(value));
    }
    assert(!fclose(file));
}

static void check_state(SfDrmPreferences *preferences, int count, int existing,
                         int caught, int invalid, size_t strings) {
    SfDrmState state;
    assert(!sf_drm_preferences_state(preferences, &state));
    assert(state.free_count == count && state.loaded_existing == existing &&
           state.reload_exception_caught == caught && state.invalid_save == invalid &&
           state.string_count == strings);
}

int main(void) {
    load_fixture(); verify_crypto();
    char directory[] = "/private/tmp/starfront-drm-test-XXXXXX";
    assert(mkdtemp(directory));
    char path[512], temporary[520], backup[520];
    assert(snprintf(path, sizeof(path), "%s/GLoft.preferences", directory) > 0);
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    SfDrmOptions options = {random_bytes, &random_calls};
    SfDrmError error;
    SfDrmPreferences *preferences = NULL;
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    assert(random_calls == 1 && error.code == 0);
    check_state(preferences, 0, 0, 0, 0, 20);
    size_t serial_size;
    assert(!memcmp(sf_drm_preferences_serial(preferences, &serial_size), "1188", 4) && serial_size == 4);
    for (unsigned i = 0; i < 20; ++i) {
        char key[5] = {'g','l','_', (char)('a' + i),0};
        assert(!strcmp(sf_drm_preferences_get_string(preferences, key), fixture_encoded[i]));
    }
    assert(!sf_drm_preferences_get_string(preferences, "gl_z"));
    size_t before_size, after_size;
    unsigned char *before = read_bytes(path, &before_size);
    static const unsigned char resource[] = "resource-id";
    assert(!sf_drm_preferences_set_resource_serial(preferences, resource, sizeof(resource)-1, &error));
    assert(!memcmp(sf_drm_preferences_serial(preferences, &serial_size), resource, sizeof(resource)-1));
    assert(serial_size == sizeof(resource)-1);
    static const unsigned char invalid_utf8[] = {0xc0, 0xaf};
    assert(sf_drm_preferences_set_resource_serial(preferences, invalid_utf8, 2, &error) == SF_DRM_BAD_VALUE);
    assert(serial_size == sizeof(resource)-1);
    sf_drm_preferences_destroy(&preferences); assert(!preferences);
    sf_drm_preferences_destroy(&preferences);
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    assert(random_calls == 2);
    check_state(preferences, 0, 1, 1, 0, 20);
    assert(!sf_drm_preferences_serial(preferences, &serial_size) && serial_size == 0);
    unsigned char *after = read_bytes(path, &after_size);
    assert(before_size == after_size && !memcmp(before, after, before_size)); free(after);
    assert(!sf_drm_preferences_set_resource_serial(preferences, resource, sizeof(resource)-1, &error));
    sf_drm_preferences_destroy(&preferences);

    write_partial_preferences(path, NULL, 1, 1);
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    check_state(preferences, 0, 1, 0, 0, 1); sf_drm_preferences_destroy(&preferences);
    write_partial_preferences(path, "bad!", 1, 1);
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    check_state(preferences, 0, 1, 1, 0, 2); sf_drm_preferences_destroy(&preferences);
    write_partial_preferences(path, extra_encoded[2], 1, 1);
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    check_state(preferences, 2, 1, 0, 1, 2); sf_drm_preferences_destroy(&preferences);
    write_partial_preferences(path, extra_encoded[3], 1, 1);
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    check_state(preferences, 0, 1, 1, 0, 2); sf_drm_preferences_destroy(&preferences);
    /* Missing gl_a takes the original constructor's fresh branch. */
    write_partial_preferences(path, fixture_encoded[11], 0, 1);
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    check_state(preferences, 0, 1, 0, 0, 20); sf_drm_preferences_destroy(&preferences);
    assert(access(backup, F_OK) == -1 && destructive_destinations == 0);

    /* A valid existing partial file must survive replacement failures even
     * with Vita newlib unlinking every rename destination first. */
    write_partial_preferences(path, fixture_encoded[11], 0, 1);
    size_t partial_size;
    unsigned char *partial = read_bytes(path, &partial_size);
    rename_calls = 0; rename_failure_mask = 1U; /* path -> backup fails. */
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EACCES && errno == EACCES && rename_calls == 1);
    assert_file_bytes(path, partial, partial_size);
    assert(access(backup, F_OK) == -1 && access(temporary, F_OK) == -1);

    rename_calls = 0; rename_failure_mask = 2U; /* Commit fails; restore succeeds. */
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EXDEV && errno == EXDEV && rename_calls == 3);
    assert_file_bytes(path, partial, partial_size);
    assert(access(backup, F_OK) == -1 && access(temporary, F_OK) == -1);

    rename_calls = 0; rename_failure_mask = 6U; /* Commit and restore both fail. */
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EXDEV && errno == EXDEV && rename_calls == 3);
    assert(access(path, F_OK) == -1 && access(temporary, F_OK) == -1);
    assert_file_bytes(backup, partial, partial_size);
    rename_calls = 0; rename_failure_mask = 0;
    /* An interrupted transaction's backup cannot be overwritten by fresh init. */
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EEXIST && rename_calls == 0);
    assert_file_bytes(backup, partial, partial_size);
    assert(access(path, F_OK) == -1 && access(temporary, F_OK) == -1);
    write_partial_preferences(path, fixture_encoded[11], 0, 1);
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EEXIST && rename_calls == 0);
    assert_file_bytes(path, partial, partial_size);
    assert_file_bytes(backup, partial, partial_size);
    assert(!unlink(path));
    assert(!rename(backup, path)); /* Explicit recovery of the preserved copy. */
    rename_calls = 0;
    assert(!sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error));
    check_state(preferences, 0, 1, 0, 0, 20); sf_drm_preferences_destroy(&preferences);
    assert(rename_calls == 2 && access(backup, F_OK) == -1 && destructive_destinations == 0);
    write_partial_preferences(path, fixture_encoded[11], 0, 1);
    fail_backup_remove = 1;
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EACCES && errno == EACCES);
    assert_file_bytes(backup, partial, partial_size);
    assert(access(path, F_OK) == 0 && access(temporary, F_OK) == -1);
    fail_backup_remove = 0;
    assert(!unlink(backup));
    free(partial);

    FILE *file = fopen(path, "wb"); assert(file);
    assert(fwrite("<?xml version='1.0'?><map/>", 1, 26, file) == 26 && !fclose(file));
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_INVALID_FORMAT);
    assert(!preferences && error.code == SF_DRM_INVALID_FORMAT);
    write_partial_preferences(path, fixture_encoded[11], 1, 2);
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_INVALID_FORMAT);
    assert(!preferences);
    file = fopen(path, "wb"); assert(file);
    assert(fwrite(before, 1, before_size - 1, file) == before_size - 1 && !fclose(file));
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_INVALID_FORMAT);
    assert(!preferences);
    file = fopen(path, "wb"); assert(file);
    assert(fwrite(before, 1, before_size, file) == before_size && !fclose(file));
    io_failure = FAIL_READ;
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == EIO); io_failure = FAIL_NONE;
    assert(!unlink(path));

    for (int stage = FAIL_WRITE; stage <= FAIL_RENAME; ++stage) {
        io_failure = stage;
        assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
        assert(!preferences && error.code == SF_DRM_IO_ERROR);
        assert(error.system_error == (stage == FAIL_WRITE ? ENOSPC : stage == FAIL_RENAME ? EXDEV : EIO));
        assert(access(path, F_OK) == -1 && access(temporary, F_OK) == -1);
        io_failure = FAIL_NONE;
    }
    random_failure = -707;
    assert(sf_drm_preferences_prepare(path, NULL, &options, &preferences, &error) == SF_DRM_RANDOM_ERROR);
    assert(!preferences && error.system_error == -707); random_failure = 0;
    assert(sf_drm_preferences_prepare(path, "new-id", &options, &preferences, &error) == SF_DRM_UNSUPPORTED_PROFILE);
    assert(!preferences);
    char missing_path[512]; snprintf(missing_path, sizeof(missing_path), "%s/missing/file", directory);
    assert(sf_drm_preferences_prepare(missing_path, NULL, &options, &preferences, &error) == SF_DRM_IO_ERROR);
    assert(!preferences && error.system_error == ENOENT);
    /* The production default clock path also creates a real persistent file. */
    assert(!sf_drm_preferences_prepare(path, NULL, NULL, &preferences, &error));
    check_state(preferences, 0, 0, 0, 0, 20); sf_drm_preferences_destroy(&preferences);
    assert(!unlink(path) && !rmdir(directory));
    assert(destructive_destinations == 0);
    free(before);
    for (unsigned i = 0; i < 20; ++i) { free(fixture_plain[i]); free(fixture_encoded[i]); }
    for (unsigned i = 0; i < 4; ++i) { free(extra_plain[i]); free(extra_encoded[i]); }
    puts("Gingerbread Java vectors, preferences lifecycle, reload exceptions and I/O failures passed");
    return 0;
}
