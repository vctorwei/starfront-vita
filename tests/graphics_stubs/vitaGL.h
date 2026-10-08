#pragma once
#include <stdint.h>
typedef uint32_t GLenum;
typedef float GLfloat;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_VIEWPORT 0x0BA2
#define GL_SCISSOR_BOX 0x0C10
#define GL_FRAMEBUFFER 0x8D40
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_MAX_TEXTURE_SIZE 0x0D33
void glBlendFunc(GLenum source, GLenum destination);
void glBlendFuncSeparate(GLenum source_rgb, GLenum destination_rgb,
                          GLenum source_alpha, GLenum destination_alpha);
void glGetFloatv(GLenum pname, GLfloat *params);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void glBindFramebuffer(GLenum target, GLuint framebuffer);
void glGetIntegerv(GLenum pname, GLint *params);
