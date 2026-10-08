#pragma once

/* A checkpoint reads counters only. It never queries GL or clears an error. */
void sf_graphics_trace_summary(void);

#ifdef STARFRONT_GL_TRACE
#include <vitaGL.h>

GLenum sf_trace_glGetError(void);
void sf_trace_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void sf_trace_glClear(GLbitfield mask);
void sf_trace_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void sf_trace_glBindFramebuffer(GLenum target, GLuint framebuffer);
void sf_trace_glUseProgram(GLuint program);
void sf_trace_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void sf_trace_glDrawElements(GLenum mode, GLsizei count, GLenum type,
                            const GLvoid *indices);
void sf_trace_glCompressedTexImage2D(GLenum target, GLint level,
                                   GLenum internal_format, GLsizei width,
                                   GLsizei height, GLint border,
                                   GLsizei image_size, const void *data);
void sf_trace_glTexImage2D(GLenum target, GLint level, GLint internal_format,
                         GLsizei width, GLsizei height, GLint border,
                         GLenum format, GLenum type, const GLvoid *data);
void sf_trace_glPixelStorei(GLenum pname, GLint param);
void sf_trace_glActiveTexture(GLenum texture);
void sf_trace_glBindTexture(GLenum target, GLuint texture);
GLint sf_trace_glGetUniformLocation(GLuint program, const GLchar *name);
GLint sf_trace_glGetAttribLocation(GLuint program, const GLchar *name);
void sf_trace_glUniform1i(GLint location, GLint value);
void sf_trace_glUniform1iv(GLint location, GLsizei count, const GLint *value);
void sf_trace_glUniformMatrix4fv(GLint location, GLsizei count,
                               GLboolean transpose, const GLfloat *value);
void sf_trace_glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                                  GLboolean normalized, GLsizei stride,
                                  const void *pointer);
void sf_trace_glBindBuffer(GLenum target, GLuint buffer);
void sf_trace_glEnable(GLenum capability);
void sf_trace_glDisable(GLenum capability);
void sf_trace_glCullFace(GLenum mode);
void sf_trace_glFrontFace(GLenum mode);
void sf_trace_glColorMask(GLboolean red, GLboolean green,
                         GLboolean blue, GLboolean alpha);
void sf_trace_glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void sf_trace_glEnableVertexAttribArray(GLuint index);
void sf_trace_glDisableVertexAttribArray(GLuint index);
#endif
