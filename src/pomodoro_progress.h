#pragma once

#include <stddef.h>
#include "pomodoro_timer.h"

enum { POMO_RGB_INACTIVE = 101, POMO_RGB_BREAK = 102, POMO_RGB_LEDS = 27,
       POMO_RGB_HEARTBEAT_MS = 2000, POMO_RGB_PEER_TIMEOUT_MS = 6000 };

static inline unsigned pomodoro_progress(const struct pomodoro_timer *timer, int64_t now) {
    if (timer->phase == POMODORO_READY || !timer->phase_ms) {
        return POMO_RGB_INACTIVE;
    }
    if (timer->phase == POMODORO_BREAK) {
        return POMO_RGB_BREAK;
    }
    int64_t remaining = pomodoro_remaining_ms(timer, now);
    if (remaining >= timer->phase_ms) {
        return 0;
    }
    return ((timer->phase_ms - remaining) * 100 + timer->phase_ms / 2) / timer->phase_ms;
}

static inline unsigned pomodoro_rgb_peer_progress(unsigned received, int64_t age_ms) {
    return received <= POMO_RGB_BREAK && age_ms < POMO_RGB_PEER_TIMEOUT_MS
               ? received : POMO_RGB_INACTIVE;
}

/* Local chain index -> distance from the left half's outer edge. Underglow
 * first (the vendor JSON swaps axes for these six points), then switch LEDs.
 * Source: PandaKBLab/QMK-firmware-for-keyboards, corne(crkbd)/keyboard.json,
 * revision 887274498cfcbe5d531be4a9f612e5984780e8a2. Right half is mirrored. */
enum { POMO_HALF_WIDTH = 96 };
static inline unsigned pomodoro_led_x(bool right, size_t led) {
    static const uint8_t left_x[POMO_RGB_LEDS] = {
        85, 50, 16, 16, 50, 85, 95, 85, 85, 85, 68, 68, 68, 80,
        60, 50, 50, 50, 33, 33, 33, 16, 16, 16, 0, 0, 0,
    };
    return right ? 2 * POMO_HALF_WIDTH - 1 - left_x[led] : left_x[led];
}

static inline unsigned pomodoro_rgb_gain(bool right, size_t led, unsigned pct) {
    enum { EDGE_WIDTH = 16, MAX_X = POMO_HALF_WIDTH - 1 };
    if (pct > 100 || led >= POMO_RGB_LEDS) {
        return 255;
    }
    unsigned x = pomodoro_led_x(right, led) % POMO_HALF_WIDTH;
    unsigned start = ((right ? POMO_HALF_WIDTH : 0) + x * (POMO_HALF_WIDTH - EDGE_WIDTH) / MAX_X) * 100;
    unsigned filled = pct * 2 * POMO_HALF_WIDTH;
    if (filled <= start) {
        return 0;
    }
    unsigned covered = filled - start;
    return covered >= EDGE_WIDTH * 100 ? 255 : covered * 255 / (EDGE_WIDTH * 100);
}

struct pomodoro_color { uint8_t r, g, b; };

/* A moving rainbow with a soft brightness wave; even its trough stays lit. */
static inline struct pomodoro_color pomodoro_break_color(bool right, size_t led,
                                                         uint32_t elapsed_ms, unsigned brightness) {
    unsigned x = pomodoro_led_x(right, led);
    unsigned hue = (x * 8 + elapsed_ms / 8) % 1536;
    unsigned wave = (x * 3 + elapsed_ms / 12) % 256;
    if (wave > 127) { wave = 255 - wave; }
    uint8_t peak = brightness * (128 + wave) / 255;
    uint8_t rise = peak * (hue % 256) / 255;
    uint8_t fall = peak - rise;
    switch (hue / 256) {
    case 0: return (struct pomodoro_color){peak, rise, 0};
    case 1: return (struct pomodoro_color){fall, peak, 0};
    case 2: return (struct pomodoro_color){0, peak, rise};
    case 3: return (struct pomodoro_color){0, fall, peak};
    case 4: return (struct pomodoro_color){rise, 0, peak};
    default: return (struct pomodoro_color){peak, 0, fall};
    }
}
