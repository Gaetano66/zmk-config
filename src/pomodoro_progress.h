#pragma once

#include <stddef.h>
#include "pomodoro_timer.h"

enum { POMO_RGB_INACTIVE = 101, POMO_RGB_LEDS = 27, POMO_RGB_HEARTBEAT_MS = 4000,
       POMO_RGB_PEER_TIMEOUT_MS = 12000 };

static inline unsigned pomodoro_progress(const struct pomodoro_timer *timer, int64_t now) {
    if (timer->phase == POMODORO_READY || !timer->phase_ms) {
        return POMO_RGB_INACTIVE;
    }
    int64_t remaining = pomodoro_remaining_ms(timer, now);
    if (remaining >= timer->phase_ms) {
        return 0;
    }
    return ((timer->phase_ms - remaining) * 100 + timer->phase_ms / 2) / timer->phase_ms;
}

static inline unsigned pomodoro_rgb_peer_progress(unsigned received, int64_t age_ms) {
    return received <= 100 && age_ms < POMO_RGB_PEER_TIMEOUT_MS ? received : POMO_RGB_INACTIVE;
}

/* Local chain index -> distance from the left half's outer edge. Underglow
 * first (the vendor JSON swaps axes for these six points), then switch LEDs.
 * Source: PandaKBLab/QMK-firmware-for-keyboards, corne(crkbd)/keyboard.json,
 * revision 887274498cfcbe5d531be4a9f612e5984780e8a2. Right half is mirrored. */
static inline unsigned pomodoro_rgb_gain(bool right, size_t led, unsigned pct) {
    enum { HALF_WIDTH = 96, EDGE_WIDTH = 16, MAX_X = 95 };
    static const uint8_t left_x[POMO_RGB_LEDS] = {
        85, 50, 16, 16, 50, 85, 95, 85, 85, 85, 68, 68, 68, 80,
        60, 50, 50, 50, 33, 33, 33, 16, 16, 16, 0, 0, 0,
    };
    if (pct > 100 || led >= POMO_RGB_LEDS) {
        return 255;
    }
    unsigned x = right ? MAX_X - left_x[led] : left_x[led];
    unsigned start = ((right ? HALF_WIDTH : 0) + x * (HALF_WIDTH - EDGE_WIDTH) / MAX_X) * 100;
    unsigned filled = pct * 2 * HALF_WIDTH;
    if (filled <= start) {
        return 0;
    }
    unsigned covered = filled - start;
    return covered >= EDGE_WIDTH * 100 ? 255 : covered * 255 / (EDGE_WIDTH * 100);
}
