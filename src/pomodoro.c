#define DT_DRV_COMPAT zmk_behavior_corne_pomodoro
#include <drivers/behavior.h>
#include <zephyr/kernel.h>

#include "pomodoro.h"
#include "../dts/pomodoro_commands.h"
#include "pomodoro_progress.h"

enum { POMO_RGB_SYNC = 5 };  /* Internal transport command, never a keymap action. */
static struct k_spinlock progress_lock;
static unsigned progress_value = POMO_RGB_INACTIVE;
static int64_t progress_seen;

static void set_rgb_progress(unsigned value, int64_t now) {
    k_spinlock_key_t key = k_spin_lock(&progress_lock);
    progress_value = value;
    progress_seen = now;
    k_spin_unlock(&progress_lock, key);
}

unsigned corne_pomodoro_rgb_progress(void) {
    k_spinlock_key_t key = k_spin_lock(&progress_lock);
    unsigned value = progress_value;
    int64_t seen = progress_seen;
    k_spin_unlock(&progress_lock, key);
    return IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
               ? value
               : pomodoro_rgb_peer_progress(value, k_uptime_get() - seen);
}


#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && IS_ENABLED(CONFIG_ZMK_DISPLAY)
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <lvgl.h>
#include <zmk/activity.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/split/central.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE)
#include <zmk/split/bluetooth/service.h>
BUILD_ASSERT(sizeof(DEVICE_DT_NAME(DT_INST(0, DT_DRV_COMPAT))) <= ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN);
#endif

#include "pomodoro_timer.h"
#include "oled_portrait.h"

static struct pomodoro_timer timer;
static int64_t show_settings_until;
K_MUTEX_DEFINE(timer_lock);
static struct k_spinlock blink_lock;
static int64_t blink_started_ms = -1000;

/* The central owns the timer. Absolute snapshots also recover from dropped BLE writes. */
static void publish_rgb_progress(unsigned value, int64_t now) {
    static unsigned sent_value = POMO_RGB_INACTIVE;
    static int64_t sent_at;
    set_rgb_progress(value, now);
    if (sent_value == value && (value == POMO_RGB_INACTIVE ||
                                now - sent_at < POMO_RGB_HEARTBEAT_MS)) {
        return;
    }
    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_INST(0, DT_DRV_COMPAT)),
        .param1 = POMO_RGB_SYNC,
        .param2 = value,
    };
    struct zmk_behavior_binding_event event = {.timestamp = now};
    for (uint8_t i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT; ++i) {
        zmk_split_central_invoke_behavior(i, &binding, event, true);
    }
    sent_value = value;
    sent_at = now;
}

static void start_blink(int64_t now) {
    k_spinlock_key_t key = k_spin_lock(&blink_lock);
    blink_started_ms = now;
    k_spin_unlock(&blink_lock, key);
}

bool corne_pomodoro_dim_rgb(void) {
    k_spinlock_key_t key = k_spin_lock(&blink_lock);
    int64_t started = blink_started_ms;
    k_spin_unlock(&blink_lock, key);
    return pomodoro_rgb_dim_at(k_uptime_get() - started);
}

/* LVGL and the OLED are accessed only on ZMK's display work queue. */
static lv_obj_t *panel;
static lv_obj_t *label;
static bool unblanked_for_timer;
static const struct device *const oled = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static bool create_panel(void) {
    lv_obj_t *status = lv_scr_act();
    panel = lv_obj_create(lv_layer_top());
    if (!panel) {
        return false;
    }
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_obj_get_style_bg_color(status, LV_PART_MAIN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    label = lv_label_create(panel);
    if (!label) {
        lv_obj_del(panel);
        panel = NULL;
        return false;
    }
    lv_obj_set_style_text_color(label, lv_obj_get_style_text_color(status, LV_PART_MAIN), LV_PART_MAIN);
    corne_oled_portrait_label(label, 0);
    return true;
}

static void refresh_oled(bool running, enum pomodoro_phase phase, unsigned seconds,
                        bool show_settings, enum pomodoro_phase selected, unsigned minutes) {
    if (!zmk_display_is_initialized()) {
        return;
    }
    bool idle = IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE) &&
                zmk_activity_get_state() != ZMK_ACTIVITY_ACTIVE;
    if (!running && !show_settings) {
        if (panel) {
            lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        }
        if (idle && unblanked_for_timer) {
            display_blanking_on(oled);
        }
        unblanked_for_timer = false;
        return;
    }
    if (!panel && !create_panel()) {
        return;
    }
    char text[32];
    if (show_settings) {
        snprintk(text, sizeof(text), "%s\n%u min", selected == POMODORO_BREAK ? "Pause" : "Fokus",
                 minutes);
    } else {
        snprintk(text, sizeof(text), "%s\n%02u:%02u", phase == POMODORO_FOCUS ? "Fokus" : "Pause",
                 seconds / 60, seconds % 60);
    }
    lv_label_set_text(label, text);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    if (idle) {
        /* ZMK stops its LVGL tick while idle. Refresh the changed text ourselves. */
        lv_refr_now(NULL);
        /* An already queued ZMK idle job can reblank the panel after a previous refresh. */
        unblanked_for_timer |= display_blanking_off(oled) == 0;
    } else {
        unblanked_for_timer = false;
    }
}

static void refresh(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(pomodoro_work, refresh);

static void refresh(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&timer_lock, K_FOREVER);
    int64_t now = k_uptime_get();
    if (pomodoro_update(&timer, now)) {
        start_blink(now);
    }
    bool running = timer.running;
    enum pomodoro_phase phase = timer.phase;
    unsigned remaining_ms = (unsigned)pomodoro_remaining_ms(&timer, now);
    unsigned seconds = (remaining_ms + 999) / 1000;
    unsigned progress = pomodoro_progress(&timer, now);
    bool show_settings = now < show_settings_until;
    enum pomodoro_phase selected = timer.selected;
    unsigned minutes = pomodoro_minutes(&timer, selected);
    unsigned next_ms = 0;
    if (running) {
        next_ms = remaining_ms % 1000;
        next_ms = next_ms ? next_ms : 1000;
    }
    if (show_settings) {
        unsigned settings_ms = show_settings_until - now;
        next_ms = next_ms ? MIN(next_ms, settings_ms) : settings_ms;
    }
    if (!running && progress != POMO_RGB_INACTIVE) {
        /* Keep the frozen progress bar on a paused timer, including after reconnect. */
        next_ms = next_ms ? MIN(next_ms, POMO_RGB_HEARTBEAT_MS) : POMO_RGB_HEARTBEAT_MS;
    }
    if (next_ms) {
        k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_work, K_MSEC(next_ms));
    }
    k_mutex_unlock(&timer_lock);
    publish_rgb_progress(progress, now);
    refresh_oled(running, phase, seconds, show_settings, selected, minutes);
}

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE)
static int activity_changed(const zmk_event_t *event) {
    ARG_UNUSED(event);
    /* Let ZMK apply its own idle blanking first, then restore the running overlay. */
    k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_work, K_MSEC(20));
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(pomodoro_display, activity_changed);
ZMK_SUBSCRIPTION(pomodoro_display, zmk_activity_state_changed);
#endif
#endif

static int press(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 == POMO_RGB_SYNC) {
        if (!IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && binding->param2 <= POMO_RGB_INACTIVE) {
            set_rgb_progress(binding->param2, k_uptime_get());
        }
        return ZMK_BEHAVIOR_OPAQUE;
    }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && IS_ENABLED(CONFIG_ZMK_DISPLAY)
    k_mutex_lock(&timer_lock, K_FOREVER);
    int64_t now = k_uptime_get();
    switch (binding->param1) {
    case POMO_TOGGLE:
        if (pomodoro_press(&timer, now)) {
            start_blink(now);
        }
        show_settings_until = 0;
        break;
    case POMO_RESTART:
        pomodoro_restart(&timer, now);
        show_settings_until = 0;
        break;
    case POMO_SELECT:
    case POMO_MINUS:
    case POMO_PLUS:
        /* Finalize any due transition before applying settings to future phases. */
        if (pomodoro_update(&timer, now)) {
            start_blink(now);
        }
        if (binding->param1 == POMO_SELECT) {
            pomodoro_select(&timer);
        } else {
            pomodoro_adjust(&timer, binding->param1 == POMO_PLUS ? 1 : -1);
        }
        show_settings_until = now + 2000;
        break;
    default:
        break;
    }
    k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_work, K_NO_WAIT);
    k_mutex_unlock(&timer_lock);
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int release(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = press,
    .binding_released = release,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &api);
