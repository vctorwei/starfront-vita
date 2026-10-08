#pragma once
#include <vitaGL.h>

/* GLES2 blend-color state is absent from vitaGL. Constant-factor rendering
 * remains unsupported and stops explicitly before changing vitaGL state. */
void sf_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void sf_glBlendFunc(GLenum source, GLenum destination);
void sf_glBlendFuncSeparate(GLenum source_rgb, GLenum destination_rgb,
                            GLenum source_alpha, GLenum destination_alpha);
void sf_glGetFloatv(GLenum pname, GLfloat *params);
