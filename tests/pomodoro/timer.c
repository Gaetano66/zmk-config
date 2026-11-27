#include <assert.h>
#include "../../src/pomodoro_timer.h"

static void focus_and_break(void) {
    struct pomodoro_timer t = {0};
    assert(t.phase == POMODORO_READY && !t.running);
    assert(!pomodoro_press(&t, 100));
    assert(t.phase == POMODORO_FOCUS && t.running);
    assert(pomodoro_remaining_ms(&t, 100) == 25 * 60 * 1000);
    int64_t focus_due = 100 + 25 * 60 * 1000;
    assert(!pomodoro_update(&t, focus_due - 1));
    assert(pomodoro_update(&t, focus_due));
    assert(t.phase == POMODORO_BREAK && t.running);
    assert(pomodoro_remaining_ms(&t, focus_due) == 5 * 60 * 1000);
    assert(!pomodoro_update(&t, focus_due));
    assert(pomodoro_update(&t, focus_due + 5 * 60 * 1000));
    assert(t.phase == POMODORO_READY && !t.running);
    assert(!pomodoro_update(&t, focus_due + 5 * 60 * 1000 + 1));
}

static void pause_and_resume(void) {
    struct pomodoro_timer t = {0};
    pomodoro_press(&t, 0);
    pomodoro_press(&t, 1234);
    assert(t.phase == POMODORO_FOCUS && !t.running);
    assert(pomodoro_remaining_ms(&t, 9000000) == 25 * 60 * 1000 - 1234);
    assert(!pomodoro_update(&t, 9000000));
    pomodoro_press(&t, 9000000);
    assert(t.phase == POMODORO_FOCUS && t.running);
    assert(pomodoro_remaining_ms(&t, 9000001) == 25 * 60 * 1000 - 1235);
    assert(pomodoro_update(&t, 9000000 + 25 * 60 * 1000 - 1234));
    pomodoro_press(&t, 9000000 + 25 * 60 * 1000 - 1234 + 4321);
    assert(t.phase == POMODORO_BREAK && !t.running);
    assert(pomodoro_remaining_ms(&t, 99999999) == 5 * 60 * 1000 - 4321);
}

static void late_updates_use_absolute_deadlines(void) {
    struct pomodoro_timer t = {0};
    pomodoro_press(&t, 0);
    assert(pomodoro_update(&t, 25 * 60 * 1000 + 325));
    assert(pomodoro_remaining_ms(&t, 25 * 60 * 1000 + 325) == 5 * 60 * 1000 - 325);
    assert(pomodoro_update(&t, 31 * 60 * 1000));
    assert(t.phase == POMODORO_READY && !t.running);
    pomodoro_press(&t, 32 * 60 * 1000);
    assert(pomodoro_update(&t, 63 * 60 * 1000));
    assert(t.phase == POMODORO_READY && !t.running);
}

static void blink_is_two_brief_blackout_pulses(void) {
    assert(!pomodoro_rgb_dim_at(-1));
    assert(pomodoro_rgb_dim_at(0) && pomodoro_rgb_dim_at(179));
    assert(!pomodoro_rgb_dim_at(180) && !pomodoro_rgb_dim_at(359));
    assert(pomodoro_rgb_dim_at(360) && pomodoro_rgb_dim_at(539));
    assert(!pomodoro_rgb_dim_at(540) && !pomodoro_rgb_dim_at(900000));
}

static void configurable_restart(void) {
    struct pomodoro_timer t = {0};
    pomodoro_adjust(&t, 1);
    assert(pomodoro_minutes(&t, POMODORO_FOCUS) == 30);
    pomodoro_select(&t);
    pomodoro_adjust(&t, -1);
    assert(t.selected == POMODORO_BREAK && pomodoro_minutes(&t, POMODORO_BREAK) == 4);
    pomodoro_restart(&t, 100);
    assert(t.phase == POMODORO_FOCUS && t.running && t.deadline_ms == 1800100);
    assert(pomodoro_update(&t, 1800100));
    assert(t.phase == POMODORO_BREAK && t.deadline_ms == 2040100);
    pomodoro_restart(&t, 1900000);
    assert(t.phase == POMODORO_FOCUS && t.deadline_ms == 3700000);
    pomodoro_press(&t, 2000000);
    pomodoro_restart(&t, 2100000);
    assert(t.running && t.deadline_ms == 3900000);
    assert(pomodoro_update(&t, 5000000) && t.phase == POMODORO_READY);
    assert(pomodoro_minutes(&t, POMODORO_FOCUS) == 30 && pomodoro_minutes(&t, POMODORO_BREAK) == 4);
    for (int i = 0; i < 100; ++i) { pomodoro_adjust(&t, -1); }
    assert(pomodoro_minutes(&t, POMODORO_BREAK) == 1);
    pomodoro_select(&t);
    for (int i = 0; i < 100; ++i) { pomodoro_adjust(&t, 1); }
    assert(pomodoro_minutes(&t, POMODORO_FOCUS) == 90);
}

int main(void) {
    configurable_restart();
    focus_and_break();
    pause_and_resume();
    late_updates_use_absolute_deadlines();
    blink_is_two_brief_blackout_pulses();
}
