#include "emulator_touch.h"

#ifdef STARFRONT_AUTOSTART
#include "display_geometry.h"
#include "port.h"
#include <psp2/touch.h>
#include <string.h>

enum { TOUCH_UP = 0, TOUCH_DOWN = 1, TOUCH_MOVE = 2 };
typedef struct { int active, id, x, y; } Finger;
static Finger fingers[2];
static int initialized, rectangle_active, suppress_front;
static unsigned rectangle_count;

static unsigned report_count(const SceTouchData *data) {
    return data->reportNum < SCE_TOUCH_MAX_REPORT ? data->reportNum : SCE_TOUCH_MAX_REPORT;
}

static void release_finger(unsigned slot, SfEmulatorTouchFn touch, void *env, void *cls) {
    Finger *finger = fingers + slot;
    if (finger->active) {
        touch(env, cls, TOUCH_UP, finger->x, finger->y, (int)slot);
        finger->active = 0;
    }
}

static void update_finger(unsigned slot, int id, int x, int y,
                          SfEmulatorTouchFn touch, void *env, void *cls) {
    Finger *finger = fingers + slot;
    if (!finger->active)
        touch(env, cls, TOUCH_DOWN, x, y, (int)slot);
    else if (finger->x != x || finger->y != y)
        touch(env, cls, TOUCH_MOVE, x, y, (int)slot);
    *finger = (Finger){1, id, x, y};
}

static int has_id(const SceTouchData *data, int id) {
    for (unsigned i = 0; i < report_count(data); ++i)
        if (data->report[i].id == id)
            return 1;
    return 0;
}

static int front_point(const SceTouchReport *report, int *x, int *y) {
    return sf_display_touch_to_native((int)report->x * SCREEN_W / 1920,
                                      (int)report->y * SCREEN_H / 1088, x, y);
}

static int rear_point(const SceTouchReport *report, int *x, int *y) {
    /* Exact desktop Vita3K touch.cpp mapping: rear y=108+fraction*781.
     * The physical screen coordinates then use the same fitted-viewport
     * inverse as genuine front touches, including bars and edge clamping. */
    return sf_display_touch_to_native((int)report->x * SCREEN_W / 1920,
                                      ((int)report->y - 108) * SCREEN_H / 781, x, y);
}

static void normal_front(const SceTouchData *front, SfEmulatorTouchFn touch,
                         void *env, void *cls) {
    for (unsigned slot = 0; slot < 2; ++slot)
        if (fingers[slot].active && !has_id(front, fingers[slot].id))
            release_finger(slot, touch, env, cls);
    for (unsigned i = 0; i < report_count(front); ++i) {
        int slot = -1, x, y;
        for (unsigned s = 0; s < 2; ++s)
            if (fingers[s].active && fingers[s].id == front->report[i].id)
                slot = (int)s;
        if (slot < 0)
            for (unsigned s = 0; s < 2; ++s)
                if (!fingers[s].active) { slot = (int)s; break; }
        if (slot < 0)
            continue;
        int inside = front_point(front->report + i, &x, &y);
        if (!fingers[slot].active && !inside)
            continue;
        update_finger((unsigned)slot, front->report[i].id, x, y, touch, env, cls);
    }
}

void sf_emulator_touch_poll(SfEmulatorTouchFn touch, void *env, void *cls) {
    if (!touch)
        return;
    if (!initialized) {
        int error = sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
        if (error < 0)
            fatal_error("Emulator rear touch sampling failed: 0x%08x", (unsigned)error);
        initialized = 1;
        port_log("Emulator input: right mouse drag fixes front finger 0 and moves finger 1; release ends both");
    }
    SceTouchData front, rear;
    memset(&front, 0, sizeof(front));
    memset(&rear, 0, sizeof(rear));
    int front_result = sceTouchPeek(SCE_TOUCH_PORT_FRONT, &front, 1);
    int rear_result = sceTouchPeek(SCE_TOUCH_PORT_BACK, &rear, 1);
    /* Failed polling is an empty sample for that panel; never reuse stale
     * reports. An independently held right gesture remains valid when only
     * the front sample fails. */
    if (front_result < 0)
        memset(&front, 0, sizeof(front));
    if (rear_result < 0)
        memset(&rear, 0, sizeof(rear));

    unsigned front_count = report_count(&front);
    int rear_present = report_count(&rear) != 0;
    int cursor_x = 0, cursor_y = 0;
    int cursor_inside = rear_present && rear_point(rear.report, &cursor_x, &cursor_y);

    /* Keep actual two-finger front input available (touchscreen, controller
     * touchpad, or Vita3K's pinch modifier) instead of replacing its points. */
    if (front_count >= 2) {
        if (rectangle_active) {
            release_finger(0, touch, env, cls);
            release_finger(1, touch, env, cls);
            rectangle_active = 0;
        }
        suppress_front = 0;
        normal_front(&front, touch, env, cls);
        return;
    }

    if (rectangle_active) {
        if (rear_present) {
            update_finger(1, -2, cursor_x, cursor_y, touch, env, cls);
            return;
        }
        if(rectangle_count<=8)
            port_log("Emulator rectangle %u released: anchor=%d,%d cursor=%d,%d",
                     rectangle_count,fingers[0].x,fingers[0].y,fingers[1].x,fingers[1].y);
        release_finger(0, touch, env, cls);
        release_finger(1, touch, env, cls);
        rectangle_active = 0;
        /* A still-held left button belongs to the gesture just ended. Wait
         * for it to lift so releasing right does not create a new tap. */
        suppress_front = front_count != 0;
        return;
    }

    if (rear_present && cursor_inside) {
        int anchor_x = cursor_x, anchor_y = cursor_y, keep_slot_zero = 0;
        int previous_slot = -1;
        for (unsigned slot = 0; slot < 2; ++slot)
            if (fingers[slot].active && has_id(&front, fingers[slot].id)) {
                previous_slot = (int)slot;
                break;
            }
        if (previous_slot >= 0) {
            /* Capture before reading this frame's front mouse position:
             * left and right share a cursor, so moving it must not move
             * the already-established first corner. */
            anchor_x = fingers[previous_slot].x;
            anchor_y = fingers[previous_slot].y;
            keep_slot_zero = previous_slot == 0;
        } else if (front_count == 1 && !suppress_front) {
            int candidate_x, candidate_y;
            if (front_point(front.report, &candidate_x, &candidate_y)) {
                anchor_x = candidate_x;
                anchor_y = candidate_y;
            }
        }
        if (!keep_slot_zero)
            release_finger(0, touch, env, cls);
        release_finger(1, touch, env, cls);
        update_finger(0, -1, anchor_x, anchor_y, touch, env, cls);
        update_finger(1, -2, cursor_x, cursor_y, touch, env, cls);
        rectangle_active = 1;
        if (++rectangle_count <= 8)
            port_log("Emulator rectangle %u: anchor=%d,%d cursor=%d,%d",
                     rectangle_count, anchor_x, anchor_y, cursor_x, cursor_y);
        return;
    }

    if (suppress_front) {
        if (front_count)
            return;
        suppress_front = 0;
    }
    normal_front(&front, touch, env, cls);
}
#endif
