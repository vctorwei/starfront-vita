#include "graphics_trace.h"
#include "display_geometry.h"
#include "port.h"
#include <string.h>

#ifdef STARFRONT_GL_TRACE
#include <zlib.h>

/* Each wrapper forwards exactly once, including glGetError. Logging never asks
 * GL for extra state. Only bounded names, uniform inputs, and the native client
 * quad already consumed by a real GL call are inspected. Counters use 32-bit relaxed atomics,
 * which ARMv7 supports without an extra libatomic backend. */
typedef struct {
    unsigned get_error, nonzero_errors, clear_color, clear, viewport;
    unsigned framebuffer, program, draw_arrays, draw_elements;
    unsigned compressed_texture, texture;
    unsigned pixel_store, active_texture, bind_texture;
    unsigned uniform_location, attribute_location, uniform_1i, uniform_1iv;
    unsigned matrix_4, attribute_pointer;
    unsigned bind_buffer, enable, disable, color_mask, scissor, cull_face, front_face;
    unsigned enable_attribute, disable_attribute, client_draw_1, client_draw_4;
} GraphicsCounters;

static GraphicsCounters counters;
/* Requested state from intercepted calls; this is not an extra GL query. */
static unsigned requested_program;
static unsigned requested_texture_unit = GL_TEXTURE0;
static unsigned requested_array_buffer, requested_element_buffer, requested_attributes;
static unsigned requested_cull_enabled;
static unsigned requested_cull_mode = GL_BACK;
static unsigned requested_front_face = GL_CCW;

typedef struct {
    unsigned size, type, normalized, stride, buffer;
    uintptr_t pointer;
} RequestedAttribute;
static RequestedAttribute requested_attribute[3];
enum { FIRST_CALLS = 12, FIRST_ERRORS = 24 };

static unsigned increment(unsigned *counter) {
    return __atomic_add_fetch(counter, 1U, __ATOMIC_RELAXED);
}

static unsigned snapshot(const unsigned *counter) {
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

/* Capture the guest LR in each wrapper, not in a helper. */
#define GUEST_LR ((unsigned)(uintptr_t)__builtin_return_address(0))

static RequestedAttribute attribute_snapshot(unsigned index) {
    const RequestedAttribute *source = &requested_attribute[index];
    RequestedAttribute result = {
        snapshot(&source->size), snapshot(&source->type), snapshot(&source->normalized),
        snapshot(&source->stride), snapshot(&source->buffer),
        __atomic_load_n(&source->pointer, __ATOMIC_RELAXED)
    };
    return result;
}

/* This diagnostic reads only the native client-array layout seen in this run,
 * after the SDK draw has consumed it. Buffer offsets, other layouts, and low
 * addresses are skipped. These bounds are not a general memory validity API. */
static int client_range(uintptr_t pointer, unsigned bytes) {
    return pointer >= 4096 && pointer <= UINTPTR_MAX - bytes;
}

static void dump_client_draw(GLenum mode, GLsizei count, GLenum type,
                             const void *indices, unsigned program) {
    if ((program != 1 && program != 4) || mode != GL_TRIANGLES ||
        count < 6 || type != GL_UNSIGNED_SHORT ||
        snapshot(&requested_array_buffer) || snapshot(&requested_element_buffer))
        return;
    unsigned *counter = program == 1 ? &counters.client_draw_1 : &counters.client_draw_4;
    if (snapshot(counter) >= 4)
        return;
    unsigned required_attributes = program == 1 ? 3U : 7U;
    if ((snapshot(&requested_attributes) & required_attributes) != required_attributes)
        return;
    RequestedAttribute position = attribute_snapshot(0);
    RequestedAttribute color = attribute_snapshot(program == 1 ? 1 : 2);
    RequestedAttribute uv = attribute_snapshot(1);
    unsigned stride = program == 1 ? 16U : 24U;
    if (position.buffer || color.buffer || position.size != 3 ||
        position.type != GL_FLOAT || position.normalized || position.stride != stride ||
        color.size != 4 || color.type != GL_UNSIGNED_BYTE || color.normalized != 1 ||
        color.stride != stride || color.pointer != position.pointer + (program == 1 ? 12U : 20U) ||
        !client_range(position.pointer, 4U * stride) || !client_range((uintptr_t)indices, 12))
        return;
    if (program == 4 && (uv.buffer || uv.size != 2 || uv.type != GL_FLOAT ||
        uv.normalized || uv.stride != stride || uv.pointer != position.pointer + 12U))
        return;
    unsigned sample = increment(counter);
    if (sample > 4)
        return;
    uint16_t index[6];
    memcpy(index, indices, sizeof(index));
    port_log("GL trace client draw #%u program=%u: indices=%u,%u,%u,%u,%u,%u stride=%u",
             sample, program, (unsigned)index[0], (unsigned)index[1], (unsigned)index[2],
             (unsigned)index[3], (unsigned)index[4], (unsigned)index[5], stride);
    /* Keep this sample restricted to its first quad, never follow an index. */
    unsigned used_vertices = 0;
    for (unsigned i = 0; i < 6; i++) {
        if (index[i] > 3) {
            port_log("GL trace client draw #%u program=%u: vertex dump skipped; indices extend beyond first quad",
                     sample, program);
            return;
        }
        used_vertices |= 1U << index[i];
    }
    if (used_vertices != 15) {
        port_log("GL trace client draw #%u program=%u: vertex dump skipped; SDK indices do not consume all four vertices",
                 sample, program);
        return;
    }
    for (unsigned i = 0; i < 4; i++) {
        float xyz[3], texcoord[2] = {0, 0};
        unsigned char rgba[4];
        memcpy(xyz, (const void *)(position.pointer + i * stride), sizeof(xyz));
        memcpy(rgba, (const void *)(color.pointer + i * stride), sizeof(rgba));
        if (program == 4)
            memcpy(texcoord, (const void *)(uv.pointer + i * stride), sizeof(texcoord));
        if (program == 4)
            port_log("GL trace client draw #%u program=%u vertex%u: xyz=%.3f,%.3f,%.3f uv=%.5f,%.5f rgba=%u,%u,%u,%u",
                     sample, program, i, (double)xyz[0], (double)xyz[1], (double)xyz[2],
                     (double)texcoord[0], (double)texcoord[1], (unsigned)rgba[0],
                     (unsigned)rgba[1], (unsigned)rgba[2], (unsigned)rgba[3]);
        else
            port_log("GL trace client draw #%u program=%u vertex%u: xyz=%.3f,%.3f,%.3f rgba=%u,%u,%u,%u",
                     sample, program, i, (double)xyz[0], (double)xyz[1], (double)xyz[2],
                     (unsigned)rgba[0], (unsigned)rgba[1], (unsigned)rgba[2], (unsigned)rgba[3]);
    }
}

GLenum sf_trace_glGetError(void) {
    unsigned caller = GUEST_LR;
    GLenum result = glGetError();
    increment(&counters.get_error);
    if (result != GL_NO_ERROR) {
        unsigned index = increment(&counters.nonzero_errors);
        if (index <= FIRST_ERRORS)
            port_log("GL trace error #%u: 0x%x guest LR=0x%08x", index,
                     (unsigned)result, caller);
    }
    return result;
}

void sf_trace_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    unsigned caller = GUEST_LR;
    glClearColor(red, green, blue, alpha);
    unsigned index = increment(&counters.clear_color);
    if (index <= FIRST_CALLS)
        port_log("GL trace ClearColor #%u: %.3f %.3f %.3f %.3f guest LR=0x%08x",
                 index, (double)red, (double)green, (double)blue, (double)alpha, caller);
}

void sf_trace_glClear(GLbitfield mask) {
    unsigned caller = GUEST_LR;
    glClear(mask);
    unsigned index = increment(&counters.clear);
    if (index <= FIRST_CALLS)
        port_log("GL trace Clear #%u: mask=0x%x guest LR=0x%08x", index,
                 (unsigned)mask, caller);
}

void sf_trace_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    unsigned caller = GUEST_LR;
    sf_glViewport(x, y, width, height);
    unsigned index = increment(&counters.viewport);
    if (index <= FIRST_CALLS)
        port_log("GL trace Viewport #%u: %d %d %d %d guest LR=0x%08x", index,
                 (int)x, (int)y, (int)width, (int)height, caller);
}

void sf_trace_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    unsigned caller = GUEST_LR;
    sf_glBindFramebuffer(target, framebuffer);
    unsigned index = increment(&counters.framebuffer);
    if (index <= FIRST_CALLS)
        port_log("GL trace BindFramebuffer #%u: target=0x%x id=%u guest LR=0x%08x",
                 index, (unsigned)target, (unsigned)framebuffer, caller);
}

void sf_trace_glUseProgram(GLuint program) {
    unsigned caller = GUEST_LR;
    glUseProgram(program);
    __atomic_store_n(&requested_program, (unsigned)program, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.program);
    if (index <= FIRST_CALLS)
        port_log("GL trace UseProgram #%u: id=%u guest LR=0x%08x", index,
                 (unsigned)program, caller);
}

void sf_trace_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    unsigned caller = GUEST_LR;
    glDrawArrays(mode, first, count);
    unsigned index = increment(&counters.draw_arrays);
    if (index <= FIRST_CALLS)
        port_log("GL trace DrawArrays #%u: mode=0x%x first=%d count=%d requestedProgram=%u requestedCull=%u/0x%x requestedFront=0x%x guest LR=0x%08x",
                 index, (unsigned)mode, (int)first, (int)count,
                 snapshot(&requested_program), snapshot(&requested_cull_enabled),
                 snapshot(&requested_cull_mode), snapshot(&requested_front_face), caller);
}

void sf_trace_glDrawElements(GLenum mode, GLsizei count, GLenum type,
                            const GLvoid *indices) {
    unsigned caller = GUEST_LR;
    glDrawElements(mode, count, type, indices);
    unsigned program = snapshot(&requested_program);
    unsigned index = increment(&counters.draw_elements);
    if (index <= FIRST_CALLS)
        port_log("GL trace DrawElements #%u: mode=0x%x count=%d type=0x%x indices=%p requestedProgram=%u requestedAttribMask=0x%x requestedBuffers=%u/%u requestedCull=%u/0x%x requestedFront=0x%x guest LR=0x%08x",
                 index, (unsigned)mode, (int)count, (unsigned)type,
                 (const void *)indices, program, snapshot(&requested_attributes),
                 snapshot(&requested_array_buffer), snapshot(&requested_element_buffer),
                 snapshot(&requested_cull_enabled), snapshot(&requested_cull_mode),
                 snapshot(&requested_front_face), caller);
    dump_client_draw(mode, count, type, indices, program);
}

void sf_trace_glCompressedTexImage2D(GLenum target, GLint level,
                                   GLenum internal_format, GLsizei width,
                                   GLsizei height, GLint border,
                                   GLsizei image_size, const void *data) {
    unsigned caller = GUEST_LR;
    unsigned index = increment(&counters.compressed_texture);
    unsigned data_crc = 0;
    /* Sample the bytes the original SDK call is about to consume. The first
     * uploads in this native path are client PVRTC payloads, at least 32 bytes.
     * These checks limit this diagnostic; they are not a memory-validity API. */
    int crc_sample = index <= FIRST_CALLS && data && image_size >= 32 &&
                     image_size <= 8 * 1024 * 1024 &&
                     client_range((uintptr_t)data, (unsigned)image_size);
    if (crc_sample)
        data_crc = (unsigned)crc32(crc32(0L, Z_NULL, 0), data, (uInt)image_size);
    glCompressedTexImage2D(target, level, internal_format, width, height,
                           border, image_size, data);
    if (index <= FIRST_CALLS) {
        port_log("GL trace CompressedTexImage2D #%u: target=0x%x level=%d internal=0x%x %dx%d border=%d bytes=%d data=%p guest LR=0x%08x",
                 index, (unsigned)target, (int)level, (unsigned)internal_format,
                 (int)width, (int)height, (int)border, (int)image_size, data, caller);
        if (crc_sample)
            port_log("GL trace CompressedTexImage2D #%u: pre-upload data CRC32=0x%08x bytes=%d",
                     index, data_crc, (int)image_size);
        else
            port_log("GL trace CompressedTexImage2D #%u: data CRC32 skipped; outside bounded client-payload sample",
                     index);
    }
}

void sf_trace_glTexImage2D(GLenum target, GLint level, GLint internal_format,
                         GLsizei width, GLsizei height, GLint border,
                         GLenum format, GLenum type, const GLvoid *data) {
    unsigned caller = GUEST_LR;
    glTexImage2D(target, level, internal_format, width, height, border, format, type, data);
    unsigned index = increment(&counters.texture);
    if (index <= FIRST_CALLS)
        port_log("GL trace TexImage2D #%u: target=0x%x level=%d internal=0x%x %dx%d border=%d format=0x%x type=0x%x data=%p guest LR=0x%08x",
                 index, (unsigned)target, (int)level, (unsigned)internal_format,
                 (int)width, (int)height, (int)border, (unsigned)format,
                 (unsigned)type, (const void *)data, caller);
}

void sf_trace_glPixelStorei(GLenum pname, GLint param) {
    unsigned caller = GUEST_LR;
    glPixelStorei(pname, param);
    unsigned index = increment(&counters.pixel_store);
    if (index <= FIRST_CALLS)
        port_log("GL trace PixelStorei #%u: pname=0x%x param=%d guest LR=0x%08x",
                 index, (unsigned)pname, (int)param, caller);
}

void sf_trace_glActiveTexture(GLenum texture) {
    unsigned caller = GUEST_LR;
    glActiveTexture(texture);
    __atomic_store_n(&requested_texture_unit, (unsigned)texture, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.active_texture);
    if (index <= 24)
        port_log("GL trace ActiveTexture #%u: unit=0x%x guest LR=0x%08x",
                 index, (unsigned)texture, caller);
}

void sf_trace_glBindTexture(GLenum target, GLuint texture) {
    unsigned caller = GUEST_LR;
    glBindTexture(target, texture);
    unsigned index = increment(&counters.bind_texture);
    if (index <= 24)
        port_log("GL trace BindTexture #%u: target=0x%x id=%u requestedUnit=0x%x guest LR=0x%08x",
                 index, (unsigned)target, (unsigned)texture,
                 snapshot(&requested_texture_unit), caller);
}

GLint sf_trace_glGetUniformLocation(GLuint program, const GLchar *name) {
    unsigned caller = GUEST_LR;
    GLint result = glGetUniformLocation(program, name);
    unsigned index = increment(&counters.uniform_location);
    if (index <= 128)
        port_log("GL trace GetUniformLocation #%u: program=%u name=%.96s location=%d/0x%x guest LR=0x%08x",
                 index, (unsigned)program, name ? name : "(null)",
                 (int)result, (unsigned)result, caller);
    return result;
}

GLint sf_trace_glGetAttribLocation(GLuint program, const GLchar *name) {
    unsigned caller = GUEST_LR;
    GLint result = glGetAttribLocation(program, name);
    unsigned index = increment(&counters.attribute_location);
    if (index <= 64)
        port_log("GL trace GetAttribLocation #%u: program=%u name=%.96s location=%d guest LR=0x%08x",
                 index, (unsigned)program, name ? name : "(null)", (int)result, caller);
    return result;
}

void sf_trace_glUniform1i(GLint location, GLint value) {
    unsigned caller = GUEST_LR;
    glUniform1i(location, value);
    unsigned index = increment(&counters.uniform_1i);
    if (index <= 48)
        port_log("GL trace Uniform1i #%u: requestedProgram=%u location=%d/0x%x value=%d guest LR=0x%08x",
                 index, snapshot(&requested_program), (int)location,
                 (unsigned)location, (int)value, caller);
}

void sf_trace_glUniform1iv(GLint location, GLsizei count, const GLint *value) {
    unsigned caller = GUEST_LR;
    glUniform1iv(location, count, value);
    unsigned index = increment(&counters.uniform_1iv);
    if (index <= 48) {
        port_log("GL trace Uniform1iv #%u: requestedProgram=%u location=%d/0x%x count=%d data=%p guest LR=0x%08x",
                 index, snapshot(&requested_program), (int)location,
                 (unsigned)location, (int)count, (const void *)value, caller);
        /* VitaGL ignores locations -1 and 0 without consuming value. */
        if (location != -1 && location != 0 && count > 0 && value)
            port_log("GL trace Uniform1iv #%u first value: %d", index, (int)value[0]);
    }
}

void sf_trace_glUniformMatrix4fv(GLint location, GLsizei count,
                               GLboolean transpose, const GLfloat *value) {
    unsigned caller = GUEST_LR;
    glUniformMatrix4fv(location, count, transpose, value);
    unsigned index = increment(&counters.matrix_4);
    if (index <= 32) {
        port_log("GL trace UniformMatrix4fv #%u: requestedProgram=%u location=%d/0x%x count=%d transpose=%u data=%p guest LR=0x%08x",
                 index, snapshot(&requested_program), (int)location,
                 (unsigned)location, (int)count, (unsigned)transpose,
                 (const void *)value, caller);
        if (location != -1 && location != 0 && count > 0 && value)
            port_log("GL trace UniformMatrix4fv #%u first matrix: %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f",
                     index, (double)value[0], (double)value[1], (double)value[2], (double)value[3],
                     (double)value[4], (double)value[5], (double)value[6], (double)value[7],
                     (double)value[8], (double)value[9], (double)value[10], (double)value[11],
                     (double)value[12], (double)value[13], (double)value[14], (double)value[15]);
    }
}

void sf_trace_glVertexAttribPointer(GLuint attribute, GLint size, GLenum type,
                                  GLboolean normalized, GLsizei stride,
                                  const void *pointer) {
    unsigned caller = GUEST_LR;
    glVertexAttribPointer(attribute, size, type, normalized, stride, pointer);
    if (attribute < 3) {
        RequestedAttribute *layout = &requested_attribute[attribute];
        __atomic_store_n(&layout->size, (unsigned)size, __ATOMIC_RELAXED);
        __atomic_store_n(&layout->type, (unsigned)type, __ATOMIC_RELAXED);
        __atomic_store_n(&layout->normalized, (unsigned)normalized, __ATOMIC_RELAXED);
        __atomic_store_n(&layout->stride, (unsigned)stride, __ATOMIC_RELAXED);
        __atomic_store_n(&layout->buffer, snapshot(&requested_array_buffer), __ATOMIC_RELAXED);
        __atomic_store_n(&layout->pointer, (uintptr_t)pointer, __ATOMIC_RELAXED);
    }
    unsigned index = increment(&counters.attribute_pointer);
    if (index <= 48)
        port_log("GL trace VertexAttribPointer #%u: requestedProgram=%u attribute=%u size=%d type=0x%x normalized=%u stride=%d data=%p guest LR=0x%08x",
                 index, snapshot(&requested_program), (unsigned)attribute, (int)size,
                 (unsigned)type, (unsigned)normalized, (int)stride, pointer, caller);
}

void sf_trace_glBindBuffer(GLenum target, GLuint buffer) {
    unsigned caller = GUEST_LR;
    glBindBuffer(target, buffer);
    if (target == GL_ARRAY_BUFFER)
        __atomic_store_n(&requested_array_buffer, (unsigned)buffer, __ATOMIC_RELAXED);
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        __atomic_store_n(&requested_element_buffer, (unsigned)buffer, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.bind_buffer);
    if (index <= 24)
        port_log("GL trace BindBuffer #%u: target=0x%x id=%u guest LR=0x%08x",
                 index, (unsigned)target, (unsigned)buffer, caller);
}

void sf_trace_glEnable(GLenum capability) {
    unsigned caller = GUEST_LR;
    glEnable(capability);
    if (capability == GL_CULL_FACE)
        __atomic_store_n(&requested_cull_enabled, 1U, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.enable);
    if (index <= 48)
        port_log("GL trace Enable #%u: cap=0x%x guest LR=0x%08x", index, (unsigned)capability, caller);
}

void sf_trace_glDisable(GLenum capability) {
    unsigned caller = GUEST_LR;
    glDisable(capability);
    if (capability == GL_CULL_FACE)
        __atomic_store_n(&requested_cull_enabled, 0U, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.disable);
    if (index <= 48)
        port_log("GL trace Disable #%u: cap=0x%x guest LR=0x%08x", index, (unsigned)capability, caller);
}

void sf_trace_glCullFace(GLenum mode) {
    unsigned caller = GUEST_LR;
    glCullFace(mode);
    __atomic_store_n(&requested_cull_mode, (unsigned)mode, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.cull_face);
    if (index <= 24)
        port_log("GL trace CullFace #%u: mode=0x%x guest LR=0x%08x",
                 index, (unsigned)mode, caller);
}

void sf_trace_glFrontFace(GLenum mode) {
    unsigned caller = GUEST_LR;
    glFrontFace(mode);
    __atomic_store_n(&requested_front_face, (unsigned)mode, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.front_face);
    if (index <= 24)
        port_log("GL trace FrontFace #%u: mode=0x%x guest LR=0x%08x",
                 index, (unsigned)mode, caller);
}

void sf_trace_glColorMask(GLboolean red, GLboolean green,
                         GLboolean blue, GLboolean alpha) {
    unsigned caller = GUEST_LR;
    glColorMask(red, green, blue, alpha);
    unsigned index = increment(&counters.color_mask);
    if (index <= 24)
        port_log("GL trace ColorMask #%u: %u,%u,%u,%u guest LR=0x%08x", index,
                 (unsigned)red, (unsigned)green, (unsigned)blue, (unsigned)alpha, caller);
}

void sf_trace_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    unsigned caller = GUEST_LR;
    sf_glScissor(x, y, width, height);
    unsigned index = increment(&counters.scissor);
    if (index <= FIRST_CALLS)
        port_log("GL trace Scissor #%u: %d %d %d %d guest LR=0x%08x", index,
                 (int)x, (int)y, (int)width, (int)height, caller);
}

void sf_trace_glEnableVertexAttribArray(GLuint attribute) {
    unsigned caller = GUEST_LR;
    glEnableVertexAttribArray(attribute);
    if (attribute < 32)
        __atomic_fetch_or(&requested_attributes, 1U << attribute, __ATOMIC_RELAXED);
    unsigned index = increment(&counters.enable_attribute);
    if (index <= 32)
        port_log("GL trace EnableVertexAttribArray #%u: index=%u requestedMask=0x%x guest LR=0x%08x",
                 index, (unsigned)attribute, snapshot(&requested_attributes), caller);
}

void sf_trace_glDisableVertexAttribArray(GLuint attribute) {
    unsigned caller = GUEST_LR;
    glDisableVertexAttribArray(attribute);
    if (attribute < 32)
        __atomic_fetch_and(&requested_attributes, ~(1U << attribute), __ATOMIC_RELAXED);
    unsigned index = increment(&counters.disable_attribute);
    if (index <= 32)
        port_log("GL trace DisableVertexAttribArray #%u: index=%u requestedMask=0x%x guest LR=0x%08x",
                 index, (unsigned)attribute, snapshot(&requested_attributes), caller);
}

void sf_graphics_trace_summary(void) {
    port_log("GL trace totals: GetError=%u nonzero=%u ClearColor=%u Clear=%u Viewport=%u BindFramebuffer=%u UseProgram=%u DrawArrays=%u DrawElements=%u CompressedTexImage2D=%u TexImage2D=%u",
             snapshot(&counters.get_error), snapshot(&counters.nonzero_errors),
             snapshot(&counters.clear_color), snapshot(&counters.clear),
             snapshot(&counters.viewport), snapshot(&counters.framebuffer),
             snapshot(&counters.program), snapshot(&counters.draw_arrays),
             snapshot(&counters.draw_elements), snapshot(&counters.compressed_texture),
             snapshot(&counters.texture));
    port_log("GL trace setup totals: PixelStorei=%u ActiveTexture=%u BindTexture=%u GetUniformLocation=%u GetAttribLocation=%u Uniform1i=%u Uniform1iv=%u UniformMatrix4fv=%u VertexAttribPointer=%u",
             snapshot(&counters.pixel_store), snapshot(&counters.active_texture),
             snapshot(&counters.bind_texture), snapshot(&counters.uniform_location),
             snapshot(&counters.attribute_location), snapshot(&counters.uniform_1i),
             snapshot(&counters.uniform_1iv), snapshot(&counters.matrix_4),
             snapshot(&counters.attribute_pointer));
    port_log("GL trace state totals: BindBuffer=%u Enable=%u Disable=%u ColorMask=%u Scissor=%u EnableAttrib=%u DisableAttrib=%u ClientDraw1=%u ClientDraw4=%u CullFace=%u FrontFace=%u",
             snapshot(&counters.bind_buffer), snapshot(&counters.enable), snapshot(&counters.disable),
             snapshot(&counters.color_mask), snapshot(&counters.scissor),
             snapshot(&counters.enable_attribute), snapshot(&counters.disable_attribute),
             snapshot(&counters.client_draw_1), snapshot(&counters.client_draw_4),
             snapshot(&counters.cull_face), snapshot(&counters.front_face));
}

#else

void sf_graphics_trace_summary(void) {
}

#endif
