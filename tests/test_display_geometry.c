#include "display_geometry.h"
#include "port.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#if SF_NATIVE_W == 1024 && SF_NATIVE_H == 600
enum { FIT_X = 16, FIT_W = 928 };
#elif SF_NATIVE_W == 1024 && SF_NATIVE_H == 640
enum { FIT_X = 45, FIT_W = 870 };
#else
#error "This regression covers the 1024x600 and 1024x640 native configurations"
#endif
static GLint actual_viewport[4] = {0, 0, SCREEN_W, SCREEN_H};
static GLint actual_scissor[4] = {0, 0, SCREEN_W, SCREEN_H};
static GLint last_viewport[4], last_scissor[4];
static unsigned viewport_calls, scissor_calls, bind_calls, query_calls;
static GLenum last_bind_target, last_query;
static GLuint last_bind_id;
static GLint *last_query_pointer;
static unsigned sdk_error;

void port_log(const char *format, ...) { (void)format; }

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    GLint values[4] = {x, y, width, height};
    ++viewport_calls;
    memcpy(last_viewport, values, sizeof(values));
    if (width < 0 || height < 0) sdk_error = 0x501;
    else memcpy(actual_viewport, values, sizeof(values));
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    GLint values[4] = {x, y, width, height};
    ++scissor_calls;
    memcpy(last_scissor, values, sizeof(values));
    if (width < 0 || height < 0) sdk_error = 0x501;
    else memcpy(actual_scissor, values, sizeof(values));
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    ++bind_calls;
    last_bind_target = target;
    last_bind_id = framebuffer;
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER)
        sdk_error = 0x500;
}

void glGetIntegerv(GLenum pname, GLint *params) {
    ++query_calls;
    last_query = pname;
    last_query_pointer = params;
    if (!params) return;
    if (pname == GL_VIEWPORT) memcpy(params, actual_viewport, sizeof(actual_viewport));
    else if (pname == GL_SCISSOR_BOX) memcpy(params, actual_scissor, sizeof(actual_scissor));
    else params[0] = 4096;
}

static void equal_rectangle(const GLint actual[4], GLint x, GLint y, GLint width, GLint height) {
    assert(actual[0] == x && actual[1] == y && actual[2] == width && actual[3] == height);
}

static void logical_query(GLenum pname, GLint x, GLint y, GLint width, GLint height) {
    GLint result[6] = {-777, -777, -777, -777, -777, -777};
    unsigned before = query_calls;
    sf_glGetIntegerv(pname, result + 1);
    assert(query_calls == before && result[0] == -777 && result[5] == -777);
    equal_rectangle(result + 1, x, y, width, height);
}

int main(void) {
    /* Before bootstrap, display-test and ordinary API semantics are literal. */
    sf_glViewport(3, 4, 501, 257);
    assert(viewport_calls == 1);
    equal_rectangle(last_viewport, 3, 4, 501, 257);
    sf_glScissor(5, 6, 40, 41);
    equal_rectangle(last_scissor, 5, 6, 40, 41);
    GLint before_config[4];
    sf_glGetIntegerv(GL_VIEWPORT, before_config);
    assert(query_calls == 1);
    equal_rectangle(before_config, 3, 4, 501, 257);
    int touch_x = -1, touch_y = -1;
    assert(sf_display_touch_to_native(100, 200, &touch_x, &touch_y));
    assert(touch_x == 100 && touch_y == 200);

    sf_display_geometry_init();
    assert(viewport_calls == 2 && scissor_calls == 1 && bind_calls == 0);
    equal_rectangle(last_viewport, FIT_X, 0, FIT_W, SCREEN_H);
    logical_query(GL_VIEWPORT, 0, 0, SF_NATIVE_W, SF_NATIVE_H);
    logical_query(GL_SCISSOR_BOX, 0, 0, SF_NATIVE_W, SF_NATIVE_H);

    sf_glViewport(0, 0, SF_NATIVE_W, SF_NATIVE_H);
    assert(viewport_calls == 3);
    equal_rectangle(last_viewport, FIT_X, 0, FIT_W, SCREEN_H);
    sf_glScissor(0, 0, SF_NATIVE_W, SF_NATIVE_H);
    assert(scissor_calls == 2);
    equal_rectangle(last_scissor, FIT_X, 0, FIT_W, SCREEN_H);

    /* Edges, rather than independently rounded widths, prevent seams. */
    sf_glViewport(1, 1, SF_NATIVE_W / 2 - 1, SF_NATIVE_H / 2 - 1);
    equal_rectangle(last_viewport, FIT_X, 0, FIT_W / 2, SCREEN_H / 2);
    logical_query(GL_VIEWPORT, 1, 1, SF_NATIVE_W / 2 - 1, SF_NATIVE_H / 2 - 1);
    sf_glViewport(SF_NATIVE_W / 2, 0, SF_NATIVE_W / 2, SF_NATIVE_H);
    equal_rectangle(last_viewport, FIT_X + FIT_W / 2, 0, FIT_W / 2, SCREEN_H);
    sf_glScissor(SF_NATIVE_W - 1, SF_NATIVE_H - 1, 1, 1);
    equal_rectangle(last_scissor, FIT_X + FIT_W - 1, SCREEN_H - 1, 1, 1);
    logical_query(GL_SCISSOR_BOX, SF_NATIVE_W - 1, SF_NATIVE_H - 1, 1, 1);
    sf_glViewport(-1, -1, 2, 2);
    equal_rectangle(last_viewport, FIT_X - 1, -1, 1, 1);
    sf_glScissor(101, 203, 0, 0);
    assert(last_scissor[2] == 0 && last_scissor[3] == 0);

    unsigned view_before = viewport_calls, scissor_before = scissor_calls;
    sf_glBindFramebuffer(GL_FRAMEBUFFER, 123);
    assert(bind_calls == 1 && last_bind_target == GL_FRAMEBUFFER && last_bind_id == 123);
    assert(viewport_calls == view_before && scissor_calls == scissor_before);
    sf_glViewport(4, 5, 256, 128);
    equal_rectangle(last_viewport, 4, 5, 256, 128);
    logical_query(GL_VIEWPORT, 4, 5, 256, 128);
    sf_glScissor(7, 8, 42, 43);
    equal_rectangle(last_scissor, 7, 8, 42, 43);
    logical_query(GL_SCISSOR_BOX, 7, 8, 42, 43);
    /* Read-FBO changes must not change draw-coordinate mapping. */
    sf_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    sf_glViewport(4, 5, 256, 128);
    equal_rectangle(last_viewport, 4, 5, 256, 128);
    sf_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    logical_query(GL_VIEWPORT, 4, 5, 256, 128); /* Context state persists across binds. */
    sf_glViewport(0, 0, SF_NATIVE_W, SF_NATIVE_H);
    equal_rectangle(last_viewport, FIT_X, 0, FIT_W, SCREEN_H);
    sf_glBindFramebuffer(0xdead, 456);
    assert(sdk_error == 0x500);
    sf_glViewport(0, 0, SF_NATIVE_W, SF_NATIVE_H);
    equal_rectangle(last_viewport, FIT_X, 0, FIT_W, SCREEN_H);

    /* Preserve errors and previous logical state on invalid dimensions. */
    unsigned calls_before = viewport_calls;
    sf_glViewport(2, 3, -1, 5);
    assert(viewport_calls == calls_before + 1 && sdk_error == 0x501);
    equal_rectangle(last_viewport, 2, 3, -1, 5);
    logical_query(GL_VIEWPORT, 0, 0, SF_NATIVE_W, SF_NATIVE_H);
    sf_glScissor(2, 3, 4, -1);
    equal_rectangle(last_scissor, 2, 3, 4, -1);
    logical_query(GL_SCISSOR_BOX, 7, 8, 42, 43);
    /* Wide intermediates avoid x+width signed overflow. */
    sf_glViewport(INT_MAX, INT_MIN, INT_MAX, 0);
    assert(last_viewport[2] >= 0 && last_viewport[3] == 0);
    logical_query(GL_VIEWPORT, INT_MAX, INT_MIN, INT_MAX, 0);

    unsigned queries_before = query_calls;
    GLint capability[2] = {-1, -123};
    sf_glGetIntegerv(GL_MAX_TEXTURE_SIZE, capability);
    assert(query_calls == queries_before + 1 && last_query == GL_MAX_TEXTURE_SIZE);
    assert(last_query_pointer == capability && capability[0] == 4096 && capability[1] == -123);
    sf_glGetIntegerv(GL_VIEWPORT, NULL);
    assert(query_calls == queries_before + 2 && last_query == GL_VIEWPORT && !last_query_pointer);

    assert(sf_display_touch_to_native(FIT_X, 0, &touch_x, &touch_y));
    assert(touch_x == 0 && touch_y == 0);
    assert(sf_display_touch_to_native(FIT_X + FIT_W - 1, SCREEN_H - 1, &touch_x, &touch_y));
    assert(touch_x == SF_NATIVE_W - 1 && touch_y == SF_NATIVE_H - 1);
    assert(sf_display_touch_to_native(FIT_X + FIT_W / 2, SCREEN_H / 2, &touch_x, &touch_y));
    assert(touch_x == SF_NATIVE_W / 2 && touch_y == SF_NATIVE_H / 2);
    assert(!sf_display_touch_to_native(FIT_X - 1, -1, &touch_x, &touch_y));
    assert(touch_x == 0 && touch_y == 0);
    assert(!sf_display_touch_to_native(FIT_X + FIT_W, SCREEN_H, &touch_x, &touch_y));
    assert(touch_x == SF_NATIVE_W - 1 && touch_y == SF_NATIVE_H - 1);
    touch_x = -123;
    assert(!sf_display_touch_to_native(FIT_X, 0, &touch_x, NULL) && touch_x == -123);
    assert(sdk_error == 0x501); /* No bridge secretly consumes GL errors. */
    printf("Display geometry forwarding/state/touch tests passed: native %dx%d -> %dx%d +%d\n",
           SF_NATIVE_W, SF_NATIVE_H, FIT_W, SCREEN_H, FIT_X);
    return 0;
}
