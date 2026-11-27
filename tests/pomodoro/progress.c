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
    assert(pomodoro_progress(&timer, 900000) == POMO_RGB_INACTIVE);
    pomodoro_adjust(&timer, 1);
    assert(pomodoro_progress(&timer, 900000) == POMO_RGB_INACTIVE);
    pomodoro_press(&timer, 900000);
    assert(pomodoro_progress(&timer, 1800000) == 50);
    assert(pomodoro_update(&timer, 2700000));
    assert(pomodoro_progress(&timer, 2700000) == POMO_RGB_BREAK);
    assert(pomodoro_progress(&timer, 2999999) == POMO_RGB_BREAK);
    assert(pomodoro_update(&timer, 3000000));
    assert(pomodoro_progress(&timer, 3000000) == POMO_RGB_INACTIVE);
    pomodoro_restart(&timer, 3000000);
    assert(pomodoro_update(&timer, 4800000));
    pomodoro_press(&timer, 4800001);
    assert(pomodoro_progress(&timer, 4800001) == POMO_RGB_INACTIVE);
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

static void break_rainbow(void) {
    for (unsigned side = 0; side < 2; ++side) {
        for (size_t led = 0; led < POMO_RGB_LEDS; ++led) {
            struct pomodoro_color off = pomodoro_break_color(side, led, 800, 0);
            assert(!off.r && !off.g && !off.b);
            struct pomodoro_color start = pomodoro_break_color(side, led, 0, 120);
            struct pomodoro_color later = pomodoro_break_color(side, led, 1800, 120);
            assert(start.r || start.g || start.b);
            assert(start.r <= 120 && start.g <= 120 && start.b <= 120);
            assert(start.r != later.r || start.g != later.g || start.b != later.b);
        }
    }
}

int main(void) {
    timer_progress();
    physical_fill();
    break_rainbow();
    assert(pomodoro_rgb_peer_progress(73, 4999) == 73);
    assert(pomodoro_rgb_peer_progress(73, POMO_RGB_PEER_TIMEOUT_MS) == POMO_RGB_INACTIVE);
    assert(pomodoro_rgb_peer_progress(POMO_RGB_BREAK, 0) == POMO_RGB_BREAK);
    assert(pomodoro_rgb_peer_progress(POMO_RGB_BREAK, POMO_RGB_PEER_TIMEOUT_MS) == POMO_RGB_INACTIVE);
    assert(pomodoro_rgb_peer_progress(103, 0) == POMO_RGB_INACTIVE);
}
