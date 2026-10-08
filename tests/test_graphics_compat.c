#include "graphics_compat.h"
#include "port.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { BLEND_COLOR = 0x8005 };
static unsigned blend_calls, separate_calls, float_calls, fatal_calls, log_calls;
static GLenum received_factors[4], received_pname;
static GLfloat *received_params;
static char last_fatal[512];
static jmp_buf fatal_return;

void port_log(const char *fmt, ...) {
    (void)fmt;
    ++log_calls;
}

void fatal_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_fatal, sizeof(last_fatal), fmt, args);
    va_end(args);
    ++fatal_calls;
    longjmp(fatal_return, 1);
}

void glBlendFunc(GLenum source, GLenum destination) {
    ++blend_calls;
    received_factors[0] = source;
    received_factors[1] = destination;
}

void glBlendFuncSeparate(GLenum source_rgb, GLenum destination_rgb,
                          GLenum source_alpha, GLenum destination_alpha) {
    ++separate_calls;
    received_factors[0] = source_rgb;
    received_factors[1] = destination_rgb;
    received_factors[2] = source_alpha;
    received_factors[3] = destination_alpha;
}

void glGetFloatv(GLenum pname, GLfloat *params) {
    ++float_calls;
    received_pname = pname;
    received_params = params;
    params[0] = 0.625f;
}

static void check_color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    GLfloat result[6] = {-123.0f, -123.0f, -123.0f, -123.0f, -123.0f, -123.0f};
    sf_glGetFloatv(BLEND_COLOR, result + 1);
    assert(result[0] == -123.0f && result[5] == -123.0f);
    assert(result[1] == red && result[2] == green && result[3] == blue && result[4] == alpha);
    assert(float_calls == 0);
}

static void check_unsupported(GLenum factor, unsigned position, int separate) {
    unsigned calls_before = blend_calls + separate_calls;
    unsigned fatal_before = fatal_calls;
    if (setjmp(fatal_return) == 0) {
        GLenum factors[4] = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO};
        factors[position] = factor;
        if (separate)
            sf_glBlendFuncSeparate(factors[0], factors[1], factors[2], factors[3]);
        else
            sf_glBlendFunc(factors[0], factors[1]);
        assert(!"constant blend factor returned without stopping");
    }
    assert(fatal_calls == fatal_before + 1);
    assert(blend_calls + separate_calls == calls_before);
    assert(strstr(last_fatal, "Unsupported constant blend factor"));
    assert(strstr(last_fatal, "rgba=0.250000,0.500000,0.750000,0.125000"));
}

int main(void) {
    check_color(0.0f, 0.0f, 0.0f, 0.0f);
    sf_glBlendColor(-1.0f, 0.5f, 2.0f, 0.125f);
    check_color(0.0f, 0.5f, 1.0f, 0.125f);
    sf_glBlendColor(-INFINITY, INFINITY, 0.0f, 1.0f);
    check_color(0.0f, 1.0f, 0.0f, 1.0f);
    sf_glBlendColor(0.25f, 0.5f, 0.75f, 0.125f);
    check_color(0.25f, 0.5f, 0.75f, 0.125f);

    sf_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    assert(blend_calls == 1 && separate_calls == 0);
    assert(received_factors[0] == GL_SRC_ALPHA && received_factors[1] == GL_ONE_MINUS_SRC_ALPHA);
    sf_glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    assert(separate_calls == 1 && blend_calls == 1);
    assert(received_factors[0] == GL_SRC_ALPHA && received_factors[1] == GL_ONE_MINUS_SRC_ALPHA);
    assert(received_factors[2] == GL_ONE && received_factors[3] == GL_ZERO);

    /* Unknown, nonconstant enums belong to the original implementation's GL
     * error behavior; this bridge neither filters them nor consumes errors. */
    sf_glBlendFunc(0xdead, GL_ONE);
    assert(blend_calls == 2 && received_factors[0] == 0xdead);
    for (GLenum factor = 0x8001; factor <= 0x8004; ++factor) {
        check_unsupported(factor, 0, 0);
        check_unsupported(factor, 1, 0);
        for (unsigned position = 0; position < 4; ++position)
            check_unsupported(factor, position, 1);
    }
    check_color(0.25f, 0.5f, 0.75f, 0.125f);
    sf_glBlendFunc(GL_ONE, GL_ZERO);
    assert(blend_calls == 3 && fatal_calls == 24);

    GLfloat params[2] = {-1.0f, -2.0f};
    sf_glGetFloatv(GL_COLOR_CLEAR_VALUE, params);
    assert(float_calls == 1 && received_pname == GL_COLOR_CLEAR_VALUE && received_params == params);
    assert(params[0] == 0.625f && params[1] == -2.0f);

    /* Diagnostics stop after their initial samples without changing state. */
    for (unsigned i = 0; i < 30; ++i)
        sf_glBlendColor(0.25f, 0.5f, 0.75f, 0.125f);
    unsigned logs_before = log_calls;
    sf_glBlendColor(0.25f, 0.5f, 0.75f, 0.125f);
    assert(log_calls == logs_before);
    puts("Graphics blend-state and unsupported constant-factor contract tests passed");
    return 0;
}
