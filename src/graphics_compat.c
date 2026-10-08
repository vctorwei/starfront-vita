#include "graphics_compat.h"
#include "port.h"
#include <string.h>

/* These GLES2 enumerants are absent from the installed vitaGL header. */
enum {
    SF_GL_CONSTANT_COLOR = 0x8001,
    SF_GL_ONE_MINUS_CONSTANT_COLOR = 0x8002,
    SF_GL_CONSTANT_ALPHA = 0x8003,
    SF_GL_ONE_MINUS_CONSTANT_ALPHA = 0x8004,
    SF_GL_BLEND_COLOR = 0x8005,
    FIRST_CALLS = 24
};

/* The port uses one vitaGL context. As with vitaGL's other global GL state,
 * calls belong to its rendering thread and must be serialized by the caller. */
static GLfloat blend_color[4];
static GLenum requested_source_rgb = GL_ONE;
static GLenum requested_destination_rgb = GL_ZERO;
static GLenum requested_source_alpha = GL_ONE;
static GLenum requested_destination_alpha = GL_ZERO;
static unsigned color_calls, factor_calls, separate_calls, query_calls;

#define GUEST_LR ((unsigned)(uintptr_t)__builtin_return_address(0))

static GLfloat clamp_color(GLfloat value) {
    return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

static int constant_factor(GLenum factor) {
    return factor >= SF_GL_CONSTANT_COLOR && factor <= SF_GL_ONE_MINUS_CONSTANT_ALPHA;
}

void sf_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    unsigned caller = GUEST_LR;
    blend_color[0] = clamp_color(red);
    blend_color[1] = clamp_color(green);
    blend_color[2] = clamp_color(blue);
    blend_color[3] = clamp_color(alpha);
    if (++color_calls <= FIRST_CALLS)
        port_log("GL compatibility BlendColor #%u rgba=%.6f,%.6f,%.6f,%.6f requested factors rgb=0x%x/0x%x alpha=0x%x/0x%x guest LR=0x%08x",
                 color_calls, (double)blend_color[0], (double)blend_color[1],
                 (double)blend_color[2], (double)blend_color[3],
                 (unsigned)requested_source_rgb, (unsigned)requested_destination_rgb,
                 (unsigned)requested_source_alpha, (unsigned)requested_destination_alpha,
                 caller);
}

void sf_glBlendFunc(GLenum source, GLenum destination) {
    unsigned caller = GUEST_LR;
    if (constant_factor(source) || constant_factor(destination))
        fatal_error("Unsupported constant blend factor: glBlendFunc source=0x%x destination=0x%x rgba=%.6f,%.6f,%.6f,%.6f guest LR=0x%08x",
                    (unsigned)source, (unsigned)destination,
                    (double)blend_color[0], (double)blend_color[1],
                    (double)blend_color[2], (double)blend_color[3], caller);
    glBlendFunc(source, destination);
    requested_source_rgb = requested_source_alpha = source;
    requested_destination_rgb = requested_destination_alpha = destination;
    if (++factor_calls <= FIRST_CALLS)
        port_log("GL compatibility BlendFunc #%u source=0x%x destination=0x%x guest LR=0x%08x",
                 factor_calls, (unsigned)source, (unsigned)destination, caller);
}

void sf_glBlendFuncSeparate(GLenum source_rgb, GLenum destination_rgb,
                            GLenum source_alpha, GLenum destination_alpha) {
    unsigned caller = GUEST_LR;
    if (constant_factor(source_rgb) || constant_factor(destination_rgb) ||
        constant_factor(source_alpha) || constant_factor(destination_alpha))
        fatal_error("Unsupported constant blend factor: glBlendFuncSeparate rgb=0x%x/0x%x alpha=0x%x/0x%x rgba=%.6f,%.6f,%.6f,%.6f guest LR=0x%08x",
                    (unsigned)source_rgb, (unsigned)destination_rgb,
                    (unsigned)source_alpha, (unsigned)destination_alpha,
                    (double)blend_color[0], (double)blend_color[1],
                    (double)blend_color[2], (double)blend_color[3], caller);
    glBlendFuncSeparate(source_rgb, destination_rgb, source_alpha, destination_alpha);
    requested_source_rgb = source_rgb;
    requested_destination_rgb = destination_rgb;
    requested_source_alpha = source_alpha;
    requested_destination_alpha = destination_alpha;
    if (++separate_calls <= FIRST_CALLS)
        port_log("GL compatibility BlendFuncSeparate #%u rgb=0x%x/0x%x alpha=0x%x/0x%x guest LR=0x%08x",
                 separate_calls, (unsigned)source_rgb, (unsigned)destination_rgb,
                 (unsigned)source_alpha, (unsigned)destination_alpha, caller);
}

void sf_glGetFloatv(GLenum pname, GLfloat *params) {
    if (pname != SF_GL_BLEND_COLOR) {
        glGetFloatv(pname, params);
        return;
    }
    memcpy(params, blend_color, sizeof(blend_color));
    if (++query_calls <= FIRST_CALLS)
        port_log("GL compatibility GetFloatv(GL_BLEND_COLOR) #%u rgba=%.6f,%.6f,%.6f,%.6f guest LR=0x%08x",
                 query_calls, (double)blend_color[0], (double)blend_color[1],
                 (double)blend_color[2], (double)blend_color[3], GUEST_LR);
}
