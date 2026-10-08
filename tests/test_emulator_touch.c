#include "emulator_touch.h"
#include "display_geometry.h"
#include "port.h"
#include <psp2/touch.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(SceTouchReport) == 16, "VitaSDK touch report ABI");
_Static_assert(offsetof(SceTouchData, report) == 16, "VitaSDK touch data ABI");
typedef struct { int action, x, y, slot; } Event;
static Event events[128];
static unsigned event_count, sampling_calls, peek_calls[2];
static SceTouchData samples[2];
static int peek_results[2] = {1, 1}, sampling_result;
static int held[2];
static void *const expected_env = (void *)0x1234;
static void *const expected_class = (void *)0x5678;

void port_log(const char *format, ...) { (void)format; }
void fatal_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(70);
}
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    (void)x; (void)y; (void)width; (void)height;
}
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    (void)x; (void)y; (void)width; (void)height;
}
void glBindFramebuffer(GLenum target, GLuint framebuffer) { (void)target; (void)framebuffer; }
void glGetIntegerv(GLenum pname, GLint *values) { (void)pname; (void)values; }

int sceTouchSetSamplingState(uint32_t port, uint32_t state) {
    assert(port == SCE_TOUCH_PORT_BACK && state == SCE_TOUCH_SAMPLING_STATE_START);
    ++sampling_calls;
    return sampling_result;
}
int sceTouchPeek(uint32_t port, SceTouchData *data, uint32_t buffers) {
    assert(port < 2 && buffers == 1);
    ++peek_calls[port];
    *data = samples[port];
    return peek_results[port];
}
static void receive(void *env, void *klass, int action, int x, int y, int slot) {
    assert(env == expected_env && klass == expected_class);
    assert(action >= 0 && action <= 2 && slot >= 0 && slot < 2);
    assert(x >= 0 && x < SF_NATIVE_W && y >= 0 && y < SF_NATIVE_H);
    assert(event_count < sizeof(events) / sizeof(events[0]));
    if (action == 1) { assert(!held[slot]); held[slot] = 1; }
    else if (action == 0) { assert(held[slot]); held[slot] = 0; }
    else assert(held[slot]);
    events[event_count++] = (Event){action, x, y, slot};
}
static void poll(void) {
    event_count = 0;
    sf_emulator_touch_poll(receive, expected_env, expected_class);
}
static void clear(void) {
    memset(samples, 0, sizeof(samples));
    peek_results[0] = peek_results[1] = 1;
    poll();
    poll();
    assert(event_count == 0 && !held[0] && !held[1]);
}
static void front(unsigned index, int id, int x, int y) {
    assert(index < SCE_TOUCH_MAX_REPORT);
    samples[0].report[index] = (SceTouchReport){.id = (uint8_t)id, .x = (int16_t)(x * 2), .y = (int16_t)(y * 2)};
    if (samples[0].reportNum <= index) samples[0].reportNum = index + 1;
}
static void right(int x, int y) {
    samples[1].reportNum = 1;
    /* Ceiling reverses the exact Vita3K floor conversion without losing a
     * physical pixel: rear y=108+fraction*781. */
    samples[1].report[0] = (SceTouchReport){.id = 91, .x = (int16_t)(x * 2),
        .y = (int16_t)(108 + (y * 781 + SCREEN_H - 1) / SCREEN_H)};
}
static void expected(unsigned index, int action, int physical_x, int physical_y, int slot) {
    assert(index < event_count);
    int x, y;
    sf_display_touch_to_native(physical_x, physical_y, &x, &y);
    Event actual = events[index];
    assert(actual.action == action && actual.x == x && actual.y == y && actual.slot == slot);
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "sampling-failure")) {
        sampling_result = (int)0x80350002u;
        poll();
        abort();
    }
    sf_emulator_touch_poll(NULL, expected_env, expected_class);
    assert(sampling_calls == 0 && peek_calls[0] == 0 && peek_calls[1] == 0);
    sf_display_geometry_init();
    clear();
    assert(sampling_calls == 1);

    /* Existing single-finger semantics and report-ID identity survive. */
    front(0, 11, 100, 100); poll();
    assert(event_count == 1); expected(0, 1, 100, 100, 0);
    front(0, 11, 120, 130); poll();
    assert(event_count == 1); expected(0, 2, 120, 130, 0);
    poll(); assert(event_count == 0);
    samples[0].reportNum = 0; poll();
    assert(event_count == 1); expected(0, 0, 120, 130, 0);

    front(0, 21, 100, 120); front(1, 22, 400, 420); poll();
    assert(event_count == 2); expected(0, 1, 100, 120, 0); expected(1, 1, 400, 420, 1);
    front(0, 22, 410, 430); front(1, 21, 110, 130); poll();
    assert(event_count == 2); expected(0, 2, 410, 430, 1); expected(1, 2, 110, 130, 0);
    right(600, 300); poll(); assert(event_count == 0); /* True front pair wins. */
    clear();

    /* Right-only gesture starts at one point, fixes finger0, then moves both
     * axes of finger1. Holding unchanged never repeats DOWN or MOVE. */
    right(200, 140); poll();
    assert(event_count == 2); expected(0, 1, 200, 140, 0); expected(1, 1, 200, 140, 1);
    right(700, 410); poll();
    assert(event_count == 1); expected(0, 2, 700, 410, 1);
    poll(); assert(event_count == 0);
    samples[1].reportNum = 0; poll();
    assert(event_count == 2); expected(0, 0, 200, 140, 0); expected(1, 0, 700, 410, 1);
    clear();

    /* Right arrives during a left drag: anchor uses the prior front point,
     * not the new shared mouse coordinate in the same poll. */
    front(0, 31, 180, 150); poll();
    front(0, 31, 650, 400); right(650, 400); poll();
    assert(event_count == 1); expected(0, 1, 650, 400, 1);
    front(0, 31, 700, 430); right(700, 430); poll();
    assert(event_count == 1); expected(0, 2, 700, 430, 1);
    samples[1].reportNum = 0; poll();
    assert(event_count == 2); expected(0, 0, 180, 150, 0); expected(1, 0, 700, 430, 1);
    poll(); assert(event_count == 0); /* Held left does not re-trigger tap. */
    clear();
    front(0, 32, 300, 200); poll();
    assert(event_count == 1); expected(0, 1, 300, 200, 0);
    clear();

    /* A remaining genuine contact in slot1 is explicitly released/moved to
     * slot0 before creating the moving slot1; no live slot gets DOWN twice. */
    front(0, 41, 180, 160); front(1, 42, 380, 360); poll();
    samples[0].reportNum = 1; front(0, 42, 380, 360); poll();
    assert(event_count == 1); expected(0, 0, 180, 160, 0);
    front(0, 42, 700, 420); right(700, 420); poll();
    assert(event_count == 3);
    expected(0, 0, 380, 360, 1); expected(1, 1, 380, 360, 0); expected(2, 1, 700, 420, 1);
    clear();

    /* Bar touches cannot begin a gesture. A live drag leaving the fitted
     * viewport clamps to its edge, including rear y889 -> physical y544. */
    right(0, 140); poll(); assert(event_count == 0);
    right(250, 140); poll(); assert(event_count == 2);
    right(0, 400); poll(); assert(event_count == 1); expected(0, 2, 0, 400, 1);
    samples[1].report[0].x = 1919; samples[1].report[0].y = 889; poll();
    assert(event_count == 1); expected(0, 2, 959, 544, 1);
    clear();

    /* A real front pair can take over a synthetic rectangle with a complete
     * UP pair first, then its independent points and IDs. */
    right(200, 140); poll();
    front(0, 51, 300, 120); front(1, 52, 620, 450); poll();
    assert(event_count == 4);
    expected(0, 0, 200, 140, 0); expected(1, 0, 200, 140, 1);
    expected(2, 1, 300, 120, 0); expected(3, 1, 620, 450, 1);
    clear();

    /* A failed poll may leave dirty output data. Its panel is treated empty
     * so it cannot leave the native pointer held forever. */
    front(0, 61, 200, 120); poll(); peek_results[0] = -1; poll();
    assert(event_count == 1); expected(0, 0, 200, 120, 0);
    clear();
    right(220, 180); poll(); right(720, 380); poll();
    peek_results[1] = -1; poll();
    assert(event_count == 2); expected(0, 0, 220, 180, 0); expected(1, 0, 720, 380, 1);
    clear();

    /* Corrupt reportNum cannot cause an out-of-array read. At most two real
     * front IDs are delivered, and extra reports cannot replace their slots. */
    for (unsigned i = 0; i < SCE_TOUCH_MAX_REPORT; ++i) front(i, (int)(70 + i), 200 + (int)i * 10, 200);
    samples[0].reportNum = 100; poll();
    assert(event_count == 2); expected(0, 1, 200, 200, 0); expected(1, 1, 210, 200, 1);
    clear();
    assert(sampling_calls == 1 && peek_calls[0] == peek_calls[1]);
    printf("Emulator right-drag/front identity/lifecycle/bounds tests passed: logical %dx%d\n", SF_NATIVE_W, SF_NATIVE_H);
    return 0;
}
