#include "display_geometry.h"
#include "port.h"
#include <limits.h>
#include <string.h>

/* vitaGL and this port use a single context on the rendering thread. Viewport
 * and scissor are context state, not per-FBO state. A binding switches mapping
 * for subsequent explicit calls; it does not invent extra GL state calls. */
static int configured;
static GLuint draw_framebuffer;
static GLint fitted_x, fitted_y;
static GLsizei fitted_width = SCREEN_W, fitted_height = SCREEN_H;
static GLint logical_viewport[4], logical_scissor[4];
static unsigned viewport_calls, scissor_calls, query_calls;

static int64_t floor_ratio(int64_t value, int numerator, int denominator) {
    int64_t product = value * numerator;
    int64_t result = product / denominator;
    if (product < 0 && product % denominator)
        --result;
    return result;
}

static GLint bounded_int(int64_t value) {
    return value < INT_MIN ? INT_MIN : value > INT_MAX ? INT_MAX : (GLint)value;
}

static void map_rectangle(GLint x, GLint y, GLsizei width, GLsizei height, GLint result[4]) {
    int64_t left = floor_ratio(x, fitted_width, SF_NATIVE_W);
    int64_t bottom = floor_ratio(y, fitted_height, SF_NATIVE_H);
    int64_t right = floor_ratio((int64_t)x + width, fitted_width, SF_NATIVE_W);
    int64_t top = floor_ratio((int64_t)y + height, fitted_height, SF_NATIVE_H);
    result[0] = bounded_int(left + fitted_x);
    result[1] = bounded_int(bottom + fitted_y);
    /* Transform edges together so adjacent rectangles share their boundary. */
    result[2] = bounded_int(right - left);
    result[3] = bounded_int(top - bottom);
}

void sf_display_geometry_init(void) {
    if ((int64_t)SCREEN_W * SF_NATIVE_H > (int64_t)SCREEN_H * SF_NATIVE_W) {
        fitted_height = SCREEN_H;
        fitted_width = (GLsizei)(((int64_t)SF_NATIVE_W * SCREEN_H + SF_NATIVE_H / 2) / SF_NATIVE_H);
    } else {
        fitted_width = SCREEN_W;
        fitted_height = (GLsizei)(((int64_t)SF_NATIVE_H * SCREEN_W + SF_NATIVE_W / 2) / SF_NATIVE_W);
    }
    if (fitted_width < 1) fitted_width = 1;
    if (fitted_height < 1) fitted_height = 1;
    fitted_x = (SCREEN_W - fitted_width) / 2;
    fitted_y = (SCREEN_H - fitted_height) / 2;
    logical_viewport[0] = logical_scissor[0] = 0;
    logical_viewport[1] = logical_scissor[1] = 0;
    logical_viewport[2] = logical_scissor[2] = SF_NATIVE_W;
    logical_viewport[3] = logical_scissor[3] = SF_NATIVE_H;
    draw_framebuffer = 0;
    configured = 1;
    glViewport(fitted_x, fitted_y, fitted_width, fitted_height);
    port_log("Display geometry: native %dx%d -> physical %dx%d at %d,%d in %dx%d",
             SF_NATIVE_W, SF_NATIVE_H, (int)fitted_width, (int)fitted_height,
             (int)fitted_x, (int)fitted_y, SCREEN_W, SCREEN_H);
}

void sf_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    GLint physical[4] = {x, y, width, height};
    if (configured && width >= 0 && height >= 0) {
        GLint requested[4] = {x, y, width, height};
        memcpy(logical_viewport, requested, sizeof(requested));
        if (!draw_framebuffer)
            map_rectangle(x, y, width, height, physical);
    }
    /* Invalid sizes pass through unchanged so SDK validation/error state is
     * retained. They do not become a valid zero-size mapped rectangle. */
    glViewport(physical[0], physical[1], physical[2], physical[3]);
    if (configured && ++viewport_calls <= 12)
        port_log("Display geometry Viewport: requested=%d,%d,%d,%d forwarded=%d,%d,%d,%d drawFBO=%u",
                 (int)x, (int)y, (int)width, (int)height, (int)physical[0],
                 (int)physical[1], (int)physical[2], (int)physical[3], (unsigned)draw_framebuffer);
}

void sf_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    GLint physical[4] = {x, y, width, height};
    if (configured && width >= 0 && height >= 0) {
        GLint requested[4] = {x, y, width, height};
        memcpy(logical_scissor, requested, sizeof(requested));
        if (!draw_framebuffer)
            map_rectangle(x, y, width, height, physical);
    }
    glScissor(physical[0], physical[1], physical[2], physical[3]);
    if (configured && ++scissor_calls <= 12)
        port_log("Display geometry Scissor: requested=%d,%d,%d,%d forwarded=%d,%d,%d,%d drawFBO=%u",
                 (int)x, (int)y, (int)width, (int)height, (int)physical[0],
                 (int)physical[1], (int)physical[2], (int)physical[3], (unsigned)draw_framebuffer);
}

void sf_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    glBindFramebuffer(target, framebuffer);
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        draw_framebuffer = framebuffer;
}

void sf_glGetIntegerv(GLenum pname, GLint *params) {
    if (!configured || !params || (pname != GL_VIEWPORT && pname != GL_SCISSOR_BOX)) {
        glGetIntegerv(pname, params);
        return;
    }
    memcpy(params, pname == GL_VIEWPORT ? logical_viewport : logical_scissor,
           sizeof(logical_viewport));
    if (++query_calls <= 12)
        port_log("Display geometry GetIntegerv: pname=0x%x logical=%d,%d,%d,%d",
                 (unsigned)pname, (int)params[0], (int)params[1], (int)params[2], (int)params[3]);
}

int sf_display_touch_to_native(int physical_x, int physical_y, int *native_x, int *native_y) {
    if (!native_x || !native_y)
        return 0;
    int left = configured ? fitted_x : 0;
    int bottom = configured ? fitted_y : 0;
    int width = configured ? fitted_width : SCREEN_W;
    int height = configured ? fitted_height : SCREEN_H;
    int inside = physical_x >= left && physical_x < left + width &&
                 physical_y >= bottom && physical_y < bottom + height;
    int x = physical_x < left ? 0 : physical_x >= left + width ? width - 1 : physical_x - left;
    int y = physical_y < bottom ? 0 : physical_y >= bottom + height ? height - 1 : physical_y - bottom;
    /* Sample pixel centers, including both logical edge pixels. Raw touch Y
     * is top-origin; the centered fit has the same bars in either convention. */
    *native_x = configured ? (int)(((2LL * x + 1) * SF_NATIVE_W) / (2LL * width)) : x;
    *native_y = configured ? (int)(((2LL * y + 1) * SF_NATIVE_H) / (2LL * height)) : y;
    return inside;
}
