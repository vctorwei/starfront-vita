#pragma once
#include <stdio.h>
size_t sf_drm_mock_fread(void *, size_t, size_t, FILE *);
size_t sf_drm_mock_fwrite(const void *, size_t, size_t, FILE *);
int sf_drm_mock_ferror(FILE *);
int sf_drm_mock_fflush(FILE *);
int sf_drm_mock_fclose(FILE *);
int sf_drm_mock_rename(const char *, const char *);
int sf_drm_mock_remove(const char *);
#define fread sf_drm_mock_fread
#define fwrite sf_drm_mock_fwrite
#define ferror sf_drm_mock_ferror
#define fflush sf_drm_mock_fflush
#define fclose sf_drm_mock_fclose
#define rename sf_drm_mock_rename
#define remove sf_drm_mock_remove
