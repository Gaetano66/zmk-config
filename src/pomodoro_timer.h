#pragma once

#include <stdbool.h>
#include <stdint.h>

enum pomodoro_phase { POMODORO_READY, POMODORO_FOCUS, POMODORO_BREAK };

struct pomodoro_timer {
    enum pomodoro_phase phase;
    bool running;
    int64_t deadline_ms;
    unsigned phase_ms;
    enum pomodoro_phase selected;
    unsigned focus_minutes;
    unsigned break_minutes;
};

enum { POMODORO_MINUTE_MS = 60 * 1000 };

static inline unsigned pomodoro_minutes(const struct pomodoro_timer *timer, enum pomodoro_phase phase) {
    if (phase == POMODORO_BREAK) {
        return timer->break_minutes ? timer->break_minutes : 5;
    }
    return timer->focus_minutes ? timer->focus_minutes : 25;
}

static inline void pomodoro_select(struct pomodoro_timer *timer) {
    timer->selected = timer->selected == POMODORO_BREAK ? POMODORO_FOCUS : POMODORO_BREAK;
}

static inline void pomodoro_adjust(struct pomodoro_timer *timer, int direction) {
    bool is_break = timer->selected == POMODORO_BREAK;
    unsigned *setting = is_break ? &timer->break_minutes : &timer->focus_minutes;
    int minutes = (int)pomodoro_minutes(timer, timer->selected) + direction * (is_break ? 1 : 5);
    int minimum = is_break ? 1 : 5;
    int maximum = is_break ? 30 : 90;
    *setting = minutes < minimum ? minimum : minutes > maximum ? maximum : minutes;
}

static inline void pomodoro_restart(struct pomodoro_timer *timer, int64_t now) {
    timer->phase = POMODORO_FOCUS;
    timer->running = true;
    timer->phase_ms = pomodoro_minutes(timer, POMODORO_FOCUS) * POMODORO_MINUTE_MS;
    timer->deadline_ms = now + timer->phase_ms;
}

static inline void pomodoro_stop(struct pomodoro_timer *timer) {
    timer->phase = POMODORO_READY;
    timer->running = false;
    timer->deadline_ms = 0;
    timer->phase_ms = 0;
}

static inline int64_t pomodoro_remaining_ms(const struct pomodoro_timer *timer, int64_t now) {
    int64_t remaining = timer->running ? timer->deadline_ms - now : 0;
    return remaining > 0 ? remaining : 0;
}

/* Anchor the break to the original deadline, not to the UI work's arrival time. */
static inline bool pomodoro_update(struct pomodoro_timer *timer, int64_t now) {
    if (!timer->running || now < timer->deadline_ms) {
        return false;
    }
    if (timer->phase == POMODORO_FOCUS) {
        timer->phase = POMODORO_BREAK;
        timer->phase_ms = pomodoro_minutes(timer, POMODORO_BREAK) * POMODORO_MINUTE_MS;
        timer->deadline_ms += timer->phase_ms;
    }
    if (now >= timer->deadline_ms) {
        pomodoro_stop(timer);
    }
    return true;
}

static inline void pomodoro_press(struct pomodoro_timer *timer, int64_t now) {
    if (timer->running) {
        pomodoro_stop(timer);
    } else {
        pomodoro_restart(timer, now);
    }
}
