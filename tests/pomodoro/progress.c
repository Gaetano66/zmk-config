#include <assert.h>
#include <stddef.h>

#include "../../src/pomodoro_progress.h"

static void timer_progress(void) {
    struct pomodoro_timer timer = {0};
    assert(pomodoro_progress(&timer, 0) == POMO_RGB_INACTIVE);
    pomodoro_restart(&timer, 100);
    assert(pomodoro_progress(&timer, 100) == 0);
    assert(pomodoro_progress(&timer, 750100) == 50);
    pomodoro_press(&timer, 750100);
    assert(pomodoro_progress(&timer, 900000) == 50);
    pomodoro_adjust(&timer, 1);
    assert(pomodoro_progress(&timer, 900000) == 50);
    pomodoro_press(&timer, 900000);
    assert(pomodoro_progress(&timer, 1650000) == 100);
    pomodoro_update(&timer, 1650000);
    assert(pomodoro_progress(&timer, 1650000) == 0);
    assert(pomodoro_progress(&timer, 1800000) == 50);
    pomodoro_update(&timer, 1950000);
    assert(pomodoro_progress(&timer, 1950000) == POMO_RGB_INACTIVE);
    pomodoro_restart(&timer, 2000000);
    assert(pomodoro_progress(&timer, 2900000) == 50);
}

static void physical_fill(void) {
    for (size_t led = 0; led < POMO_RGB_LEDS; ++led) {
        for (unsigned pct = 0; pct <= 100; ++pct) {
            unsigned left = pomodoro_rgb_gain(false, led, pct);
            unsigned right = pomodoro_rgb_gain(true, led, pct);
            if (pct <= 50) { assert(right == 0); }
            if (pct >= 50) { assert(left == 255); }
            if (pct == 0) { assert(left == 0); }
            if (pct == 100) { assert(right == 255); }
            if (pct > 0) {
                assert(left >= pomodoro_rgb_gain(false, led, pct - 1));
                assert(right >= pomodoro_rgb_gain(true, led, pct - 1));
            }
        }
        assert(pomodoro_rgb_gain(false, led, POMO_RGB_INACTIVE) == 255);
        assert(pomodoro_rgb_gain(true, led, POMO_RGB_INACTIVE) == 255);
    }
    assert(pomodoro_rgb_gain(false, 24, 5) > pomodoro_rgb_gain(false, 6, 5));
    assert(pomodoro_rgb_gain(true, 6, 55) > pomodoro_rgb_gain(true, 24, 55));
    assert(pomodoro_rgb_gain(false, 2, 18) > pomodoro_rgb_gain(false, 0, 18));
    assert(pomodoro_rgb_gain(true, 0, 68) > pomodoro_rgb_gain(true, 2, 68));
}

int main(void) {
    timer_progress();
    physical_fill();
    assert(pomodoro_rgb_peer_progress(73, 4999) == 73);
    assert(pomodoro_rgb_peer_progress(73, POMO_RGB_PEER_TIMEOUT_MS) == POMO_RGB_INACTIVE);
    assert(pomodoro_rgb_peer_progress(102, 0) == POMO_RGB_INACTIVE);
}
