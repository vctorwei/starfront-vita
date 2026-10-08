#pragma once
#include <vitaGL.h>

/* The native driver, renderer and touch coordinates use these dimensions for
 * their entire lifetime. Physical presentation remains SCREEN_W/SCREEN_H. */
#ifndef SF_NATIVE_W
#define SF_NATIVE_W 1024
#endif
#ifndef SF_NATIVE_H
#define SF_NATIVE_H 600
#endif
#if SF_NATIVE_W <= 0 || SF_NATIVE_H <= 0
#error "Native display dimensions must be positive"
#endif

void sf_display_geometry_init(void);
/* Always writes clamped native coordinates when both outputs exist. Returns
 * whether the point is inside the fitted viewport, so new bar touches can be
 * ignored while an existing gesture still receives movement/release. */
int sf_display_touch_to_native(int physical_x, int physical_y, int *native_x, int *native_y);
void sf_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void sf_glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void sf_glBindFramebuffer(GLenum target, GLuint framebuffer);
void sf_glGetIntegerv(GLenum pname, GLint *params);
