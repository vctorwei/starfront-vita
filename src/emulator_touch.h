#pragma once

typedef void (*SfEmulatorTouchFn)(void *environment, void *klass, int action,
                                 int x, int y, int slot);

/* Vita3K maps its right mouse button to the rear panel. This test-only bridge
 * turns it into a two-corner front-screen gesture. Ordinary Vita builds do not
 * compile this implementation or sample the rear panel. */
#ifdef STARFRONT_AUTOSTART
void sf_emulator_touch_poll(SfEmulatorTouchFn touch, void *environment, void *klass);
#endif
